#pragma once
#include "DspUtil.h"

namespace layshit
{
/** Analytic-signal (Hilbert) quadrature pair built from two chains of 2nd-order allpasses
    (Olli Niemitalo's coefficients, ~90 deg phase difference over most of the audio band). */
class HilbertPair
{
public:
    void reset() noexcept
    {
        for (auto* ch : { &a, &b })
            for (auto& s : *ch)
                s = {};
        bDelay = 0.0f;
    }

    /** Returns in-phase (i) and quadrature (q) parts of x. */
    void process (float x, float& i, float& q) noexcept
    {
        const float aOut = runChain (a, ca, x);
        q = runChain (b, cb, x);
        i = bDelay; // one-sample delay sits on the first chain
        bDelay = aOut;
    }

private:
    struct Stage { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    using Chain = Stage[4];

    static float runChain (Chain& ch, const float (&c)[4], float x) noexcept
    {
        for (int k = 0; k < 4; ++k)
        {
            auto& s = ch[k];
            const float y = c[k] * (x + s.y2) - s.x2;
            s.x2 = s.x1; s.x1 = x;
            s.y2 = s.y1; s.y1 = y;
            x = y;
        }
        return x;
    }


    static constexpr float ca[4] { 0.6923878f * 0.6923878f, 0.9360654322959f * 0.9360654322959f, 0.9882295226860f * 0.9882295226860f, 0.9987488452737f * 0.9987488452737f };
    static constexpr float cb[4] { 0.4021921162426f * 0.4021921162426f, 0.8561710882420f * 0.8561710882420f, 0.9722909545651f * 0.9722909545651f, 0.9952884791278f * 0.9952884791278f };

    Chain a, b;
    float bDelay = 0.0f;
};

/** Single-sideband frequency shifter: every pass through the feedback loop moves the
    spectrum by `hz`, giving barber-pole spirals. */
class Shift final : public ModuleSlot
{
public:
    void setParams (float shiftHz, float mix01) noexcept
    {
        hz.set (shiftHz);
        mix.set (mix01);
        idle = std::abs (shiftHz) < 0.01f && std::abs (hz.value) < 0.01f;
    }

    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        hz.prepare (sampleRate, 40.0f);
        mix.prepare (sampleRate, 30.0f);
        reset();
    }

    void reset() override
    {
        hz.reset (hz.target);
        mix.reset (mix.target);
        for (auto& h : hilbert)
            h.reset();
        phase = 0.0f;
    }

    bool isIdle() const noexcept override { return idle; }

    void processFrame (float& l, float& r) noexcept override
    {
        const float f = hz.next();
        const float mx = mix.next() * std::min (1.0f, std::abs (f) * 2.0f); // fade in around 0 Hz
        phase += twoPi * f / sr;
        if (phase > twoPi) phase -= twoPi;
        if (phase < 0.0f) phase += twoPi;
        const float c = std::cos (phase), s = std::sin (phase);

        float io[2] { l, r };
        for (int ch = 0; ch < 2; ++ch)
        {
            float i, q;
            hilbert[ch].process (io[ch], i, q);
            const float shifted = i * c + q * s;
            io[ch] = io[ch] + (shifted - io[ch]) * mx;
        }
        l = io[0];
        r = io[1];
    }

private:
    HilbertPair hilbert[2];
    Smoothed hz, mix;
    float sr = 44100.0f, phase = 0.0f;
    bool idle = true;
};
}
