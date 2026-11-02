#include <juce_core/juce_core.h>
#include "dsp/LayShitEngine.h"
#include <vector>

using namespace layshit;

namespace
{
constexpr double kSr = 48000.0;

struct Run
{
    std::vector<float> l, r;
};

Run runEngine (LayShitEngine& e, const std::vector<float>& inL, const std::vector<float>& inR, int block = 256)
{
    Run out;
    out.l = inL;
    out.r = inR;
    for (size_t pos = 0; pos < inL.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, inL.size() - pos);
        e.process (out.l.data() + pos, out.r.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

EngineParams cleanParams()
{
    EngineParams p;
    p.timeMs = 100.0f;
    p.feedback = 0.0f;
    p.mix = 1.0f;
    p.hpHz = 10.0f;
    p.lpHz = 22000.0f;
    return p;
}

size_t argmaxAbs (const std::vector<float>& v, size_t from, size_t to)
{
    size_t best = from;
    for (size_t i = from; i < std::min (to, v.size()); ++i)
        if (std::abs (v[i]) > std::abs (v[best])) best = i;
    return best;
}

float peakAbs (const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX)
{
    float m = 0.0f;
    for (size_t i = from; i < std::min (to, v.size()); ++i) m = std::max (m, std::abs (v[i]));
    return m;
}

float goertzelPower (const std::vector<float>& x, size_t from, size_t n, double freq)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * freq / kSr;
    const double c = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double s = x[from + i] + c * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return (float) (s1 * s1 + s2 * s2 - c * s1 * s2);
}

bool allFinite (const Run& r)
{
    for (size_t i = 0; i < r.l.size(); ++i)
        if (! std::isfinite (r.l[i]) || ! std::isfinite (r.r[i])) return false;
    return true;
}
}

class LayShitDspTests final : public juce::UnitTest
{
public:
    LayShitDspTests() : juce::UnitTest ("LayShit DSP", "LayShit") {}

    void runTest() override
    {
        beginTest ("DelayLine returns impulse at integer delay");
        {
            DelayLine d;
            d.prepare (1000);
            d.write (1.0f);
            for (int i = 0; i < 99; ++i) d.write (0.0f);
            expectWithinAbsoluteError (d.read (100.0f), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (d.read (99.0f), 0.0f, 1.0e-6f);
            const float half = d.read (100.5f);
            expect (half > 0.4f && half < 0.7f, "fractional read interpolates");
        }

        beginTest ("Engine: impulse tap at expected offset");
        {
            LayShitEngine e;
            e.prepare (kSr);
            e.setParams (cleanParams());
            e.reset();
            std::vector<float> l (12000, 0.0f), r (12000, 0.0f);
            l[0] = r[0] = 1.0f;
            auto out = runEngine (e, l, r);
            const auto peak = argmaxAbs (out.l, 100, out.l.size());
            expect (std::abs ((int) peak - 4800) <= 3, "tap at 4800 samples, got " + juce::String ((int) peak));
        }

        beginTest ("Engine: feedback decays geometrically");
        {
            LayShitEngine e;
            e.prepare (kSr);
            auto p = cleanParams();
            p.timeMs = 50.0f;
            p.feedback = 0.5f;
            e.setParams (p);
            e.reset();
            std::vector<float> l (12000, 0.0f), r (12000, 0.0f);
            // windowed 500 Hz burst (an impulse would be smeared by the loop filters each pass)
            for (int i = 0; i < 480; ++i)
                l[(size_t) i] = r[(size_t) i] = 0.5f * (0.5f - 0.5f * std::cos (twoPi * (float) i / 480.0f))
                                               * std::sin (twoPi * 500.0f * (float) i / (float) kSr);
            auto out = runEngine (e, l, r);
            const float t1 = peakAbs (out.l, 2400, 2900);
            const float t2 = peakAbs (out.l, 4800, 5300);
            const float t3 = peakAbs (out.l, 7200, 7700);
            expect (t1 > 0.1f, "first tap present");
            expectWithinAbsoluteError (t2 / t1, 0.5f, 0.08f);
            expectWithinAbsoluteError (t3 / t2, 0.5f, 0.08f);
        }

        beginTest ("Engine: silence in -> silence out with every module on");
        {
            LayShitEngine e;
            e.prepare (kSr);
            auto p = cleanParams();
            p.feedback = 0.9f;
            p.clogDrive = 1.0f; p.clogCrush = 0.5f;
            p.shiftHz = 120.0f;
            p.crystals.amount = 1.0f; p.crystals.shimmer = 1.0f;
            p.plungeDiffuse = 1.0f;
            p.wobbleDepth = 1.0f;
            e.setParams (p);
            e.reset();
            std::vector<float> l ((size_t) kSr, 0.0f), r ((size_t) kSr, 0.0f);
            auto out = runEngine (e, l, r);
            expect (peakAbs (out.l) < 1.0e-5f && peakAbs (out.r) < 1.0e-5f, "silent");
        }

        beginTest ("Engine: 110% feedback with everything on stays finite and bounded");
        {
            LayShitEngine e;
            e.prepare (kSr);
            auto p = cleanParams();
            p.timeMs = 80.0f;
            p.feedback = 1.1f;
            p.clogDrive = 0.7f; p.clogCrush = 0.3f;
            p.shiftHz = -40.0f;
            p.crystals.amount = 0.8f; p.crystals.shimmer = 0.9f; p.crystals.pitchSemis = 12.0f; p.crystals.reverse = 0.5f;
            p.plungeDiffuse = 0.8f;
            p.wobbleDepth = 0.6f;
            e.setParams (p);
            e.reset();
            juce::Random rnd (42);
            const size_t n = (size_t) (kSr * 6);
            std::vector<float> l (n, 0.0f), r (n, 0.0f);
            for (size_t i = 0; i < (size_t) kSr; ++i) { l[i] = rnd.nextFloat() * 1.6f - 0.8f; r[i] = rnd.nextFloat() * 1.6f - 0.8f; }
            auto out = runEngine (e, l, r);
            expect (allFinite (out), "finite");
            expect (peakAbs (out.l, (size_t) kSr) <= 1.0001f && peakAbs (out.r, (size_t) kSr) <= 1.0001f, "bounded by limiter");
        }

        beginTest ("Shift: +100 Hz moves a 1 kHz sine to 1.1 kHz");
        {
            Shift s;
            s.prepare (kSr);
            s.setParams (100.0f, 1.0f);
            s.reset();
            const size_t n = 48000;
            std::vector<float> outv (n);
            for (size_t i = 0; i < n; ++i)
            {
                float l = 0.5f * std::sin (twoPi * 1000.0f * (float) i / (float) kSr), r = l;
                s.processFrame (l, r);
                outv[i] = l;
            }
            const float up = goertzelPower (outv, 8000, 38400, 1100.0);
            const float down = goertzelPower (outv, 8000, 38400, 900.0);
            const float orig = goertzelPower (outv, 8000, 38400, 1000.0);
            expect (up > down * 100.0f, "upper sideband dominates lower: up=" + juce::String (up) + " down=" + juce::String (down) + " orig=" + juce::String (orig));
            expect (up > orig * 100.0f, "carrier suppressed");
        }

        beginTest ("Engine: ping-pong alternates channels");
        {
            LayShitEngine e;
            e.prepare (kSr);
            auto p = cleanParams();
            p.timeMs = 20.0f; // 960 samples
            p.feedback = 0.6f;
            p.pingPong = true;
            e.setParams (p);
            e.reset();
            std::vector<float> l (4000, 0.0f), r (4000, 0.0f);
            l[0] = 1.0f;
            auto out = runEngine (e, l, r);
            expect (peakAbs (out.l, 900, 1020) > 0.2f, "first echo left");
            expect (peakAbs (out.r, 900, 1020) < 0.01f, "first echo not right");
            expect (peakAbs (out.r, 1860, 1980) > 0.1f, "second echo right");
        }

        beginTest ("Engine: rapid order changes are safe and settle");
        {
            LayShitEngine e;
            e.prepare (kSr);
            auto p = cleanParams();
            p.feedback = 0.8f; p.clogDrive = 0.5f; p.shiftHz = 30.0f; p.crystals.amount = 0.5f; p.plungeDiffuse = 0.5f;
            e.setParams (p);
            e.reset();
            juce::Random rnd (7);
            ModuleOrder o = defaultOrder();
            for (int b = 0; b < 400; ++b)
            {
                std::vector<float> l (128), r (128);
                for (int i = 0; i < 128; ++i) { l[(size_t) i] = rnd.nextFloat() - 0.5f; r[(size_t) i] = rnd.nextFloat() - 0.5f; }
                std::swap (o[(size_t) rnd.nextInt (4)], o[(size_t) rnd.nextInt (4)]);
                e.setOrder (o);
                auto out = runEngine (e, l, r, 128);
                expect (allFinite (out), "finite");
            }
            std::vector<float> l (4800, 0.0f), r (4800, 0.0f);
            runEngine (e, l, r);
            expect (e.getOrder() == o, "final order applied");
        }

        beginTest ("Engine: flush drains the buffer");
        {
            LayShitEngine e;
            e.prepare (kSr);
            auto p = cleanParams();
            p.timeMs = 300.0f; p.feedback = 1.0f;
            e.setParams (p);
            e.reset();
            juce::Random rnd (3);
            std::vector<float> l (24000), r (24000);
            for (size_t i = 0; i < l.size(); ++i) { l[i] = rnd.nextFloat() - 0.5f; r[i] = l[i]; }
            runEngine (e, l, r);
            e.triggerFlush();
            std::vector<float> z ((size_t) (kSr * 2.5), 0.0f);
            auto out = runEngine (e, z, z);
            expect (! e.isFlushing(), "flush finished");
            expect (peakAbs (out.l, (size_t) (kSr * 1.6)) < 1.0e-5f, "silent after flush");
        }

        beginTest ("Engine: Courtesy Flush ducks while playing");
        {
            LayShitEngine e;
            e.prepare (kSr);
            auto p = cleanParams();
            p.duckAmount = 1.0f;
            e.setParams (p);
            e.reset();
            std::vector<float> l (24000), r (24000);
            for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = 0.5f * std::sin (twoPi * 220.0f * (float) i / (float) kSr);
            runEngine (e, l, r);
            expect (e.getDuckGain() < 0.05f, "wet ducked under steady input");
        }

        beginTest ("Engine: transients trigger splashes, steady tone does not");
        {
            LayShitEngine e;
            e.prepare (kSr);
            e.setParams (cleanParams());
            e.reset();
            std::vector<float> l ((size_t) kSr * 2, 0.0f), r;
            for (int hit = 0; hit < 3; ++hit)
                for (int i = 0; i < 2000; ++i)
                    l[(size_t) (hit * 24000 + 4000 + i)] = 0.8f * std::exp (-(float) i / 500.0f) * std::sin (0.05f * (float) i);
            r = l;
            runEngine (e, l, r);
            expectEquals (e.splashCount.load(), 3);

            LayShitEngine steady;
            steady.prepare (kSr);
            steady.setParams (cleanParams());
            steady.reset();
            std::vector<float> s ((size_t) kSr * 2);
            for (size_t i = 0; i < s.size(); ++i) s[i] = 0.3f * std::sin (twoPi * 220.0f * (float) i / (float) kSr);
            runEngine (steady, s, s);
            expect (steady.splashCount.load() <= 1, "steady tone only splashes at onset, got " + juce::String (steady.splashCount.load()));
        }

        beginTest ("Crystals: grains spawn and make sound");
        {
            Crystals c;
            c.prepare (kSr);
            Crystals::Params cp;
            cp.amount = 1.0f; cp.density = 40.0f; cp.sizeMs = 100.0f;
            c.setParams (cp);
            juce::Random rnd (9);
            float energy = 0.0f;
            int maxGrains = 0;
            for (int i = 0; i < 48000; ++i)
            {
                float l = rnd.nextFloat() - 0.5f, r = l;
                c.processFrame (l, r);
                energy += l * l;
                maxGrains = std::max (maxGrains, c.activeGrainCount());
            }
            expect (maxGrains >= 2, "overlapping grains");
            expect (energy > 10.0f, "audible output");
        }

        beginTest ("Holy amount emerges only with crystals AND shimmer");
        {
            expectEquals (holyAmount (0.0f, 1.0f), 0.0f);
            expectEquals (holyAmount (1.0f, 0.0f), 0.0f);
            expectEquals (holyAmount (1.0f, 1.0f), 1.0f);
            expect (holyAmount (0.3f, 0.3f) < 0.2f, "mild settings barely holy");
        }
    }
};

static LayShitDspTests layShitDspTests;
