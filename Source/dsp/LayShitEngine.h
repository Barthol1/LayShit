#pragma once
#include "DelayLine.h"
#include "Svf.h"
#include "Clog.h"
#include "Shift.h"
#include "Crystals.h"
#include "Plunge.h"
#include <array>
#include <atomic>

namespace layshit
{
enum ModuleId { modClog = 0, modShift = 1, modCrystals = 2, modPlunge = 3, numModules = 4 };
using ModuleOrder = std::array<int, numModules>;

inline ModuleOrder defaultOrder() { return { modClog, modShift, modCrystals, modPlunge }; }

inline bool isValidOrder (const ModuleOrder& o)
{
    int seen = 0;
    for (int m : o)
    {
        if (m < 0 || m >= numModules) return false;
        seen |= 1 << m;
    }
    return seen == (1 << numModules) - 1;
}

/** Everything the engine needs per block, independent of the plugin host / APVTS. */
struct EngineParams
{
    float timeMs = 375, feedback = 0.45f, mix = 0.35f;
    bool pingPong = false;
    float wobbleRate = 0.6f, wobbleDepth = 0.0f;
    float hpHz = 20.0f, lpHz = 14000.0f;
    float clogDrive = 0, clogCrush = 0, clogMix = 1;
    float shiftHz = 0, shiftMix = 1;
    Crystals::Params crystals;
    float plungeDiffuse = 0, plungeSize = 1;
    float duckAmount = 0, duckReleaseMs = 350;
    float bristol = 4.0f;
};

/** The whole LayShit signal path:
    in -> delay line -> [ordered modules] -> Fiber HP/LP -> soft limit -> feedback,
    wet ducked by the dry input (Courtesy Flush), plus the one-shot Flush. */
class LayShitEngine
{
public:
    static constexpr float maxDelayMs = 4000.0f;
    static constexpr float flushSeconds = 1.5f;

    LayShitEngine()
    {
        slots[modClog] = &clog;
        slots[modShift] = &shift;
        slots[modCrystals] = &crystals;
        slots[modPlunge] = &plunge;
    }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int maxSamples = (int) ((maxDelayMs + 20.0f) * 0.001f * sr) + (int) (flushSeconds * sr);
        for (auto& d : lines)
            d.prepare (maxSamples);
        for (auto* s : slots)
            s->prepare (sampleRate);
        delaySamples.prepare (sampleRate, 120.0f);
        feedback.prepare (sampleRate, 30.0f);
        mix.prepare (sampleRate, 30.0f);
        wobbleDepthS.prepare (sampleRate, 50.0f);
        reset();
    }

    void reset()
    {
        for (auto& d : lines) d.clear();
        for (auto* s : slots) s->reset();
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        delaySamples.reset (params.timeMs * 0.001f * sr);
        feedback.reset (params.feedback);
        mix.reset (params.mix);
        wobbleDepthS.reset (0.0f);
        fbL = fbR = 0.0f;
        duckEnv = 0.0f;
        fastEnv = slowEnv = 0.0f;
        flushing = false;
        flushPhase = 0.0f;
        readExtra = 0.0f;
        orderFade = 1.0f;
        fadeDir = 0;
    }

    void setParams (const EngineParams& p) noexcept
    {
        params = p;
        delaySamples.set (std::clamp (p.timeMs, 1.0f, maxDelayMs) * 0.001f * sr);
        feedback.set (std::clamp (p.feedback, 0.0f, 1.1f));
        mix.set (std::clamp (p.mix, 0.0f, 1.0f));
        wobbleDepthS.set (p.wobbleDepth * 0.006f * sr);

        // Bristol macro: 1 = hard lumps (grit), 4 = neutral, 7 = liquid (smear + darker)
        const float lumpy = std::clamp ((4.0f - p.bristol) / 3.0f, 0.0f, 1.0f);
        const float runny = std::clamp ((p.bristol - 4.0f) / 3.0f, 0.0f, 1.0f);

        clog.setParams (std::min (1.0f, p.clogDrive + lumpy * 0.35f), std::min (1.0f, p.clogCrush + lumpy * 0.25f),
                        lumpy > 0.0f ? std::max (p.clogMix, lumpy) : p.clogMix);
        shift.setParams (p.shiftHz, p.shiftMix);
        crystals.setParams (p.crystals);
        plunge.setParams (std::min (1.0f, p.plungeDiffuse + runny * 0.45f), p.plungeSize);

        const float lpEff = p.lpHz * (1.0f - 0.6f * runny);
        for (auto& f : hp) f.setCutoff (p.hpHz, sr);
        for (auto& f : lp) f.setCutoff (lpEff, sr);

        duckRelease = std::exp (-1.0f / (sr * std::max (1.0f, p.duckReleaseMs) * 0.001f));
        duckAttack = std::exp (-1.0f / (sr * 0.005f));
    }

    /** Requests a new module order; applied with a short fade so there are no clicks. */
    void setOrder (const ModuleOrder& o) noexcept
    {
        if (! isValidOrder (o) || o == order) return;
        pendingOrder = o;
        fadeDir = -1;
    }
    const ModuleOrder& getOrder() const noexcept { return order; }

    void triggerFlush() noexcept
    {
        if (flushing) return;
        flushing = true;
        flushPhase = 0.0f;
        readExtra = 0.0f;
    }

    /** Process a block. Buffers may alias (in-place). */
    void process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        float inPeak = 0.0f, wetPeak = 0.0f;
        const float fastAtt = std::exp (-1.0f / (sr * 0.001f)), fastRel = std::exp (-1.0f / (sr * 0.03f));
        const float slowCoef = std::exp (-1.0f / (sr * 0.3f));
        const float fadeStep = 1.0f / (0.006f * sr);

        for (int i = 0; i < n; ++i)
        {
            const float xl = inL[i], xr = inR[i];
            const float ax = std::max (std::abs (xl), std::abs (xr));
            inPeak = std::max (inPeak, ax);

            // transient detector (for the splash visual)
            fastEnv = ax > fastEnv ? ax + (fastEnv - ax) * fastAtt : ax + (fastEnv - ax) * fastRel;
            slowEnv = ax + (slowEnv - ax) * slowCoef;
            if (splashCooldown > 0)
            {
                --splashCooldown;
                slowEnv = std::max (slowEnv, fastEnv * 0.6f); // re-baseline so sustained notes splash once
            }
            else if (fastEnv > 0.05f && fastEnv > slowEnv * 2.2f)
            {
                splashCount.fetch_add (1, std::memory_order_relaxed);
                splashAmp.store (std::min (1.0f, fastEnv), std::memory_order_relaxed);
                splashCooldown = (int) (0.12f * sr);
            }

            // Courtesy Flush envelope
            duckEnv = ax > duckEnv ? ax + (duckEnv - ax) * duckAttack : ax + (duckEnv - ax) * duckRelease;
            const float duckGain = 1.0f - params.duckAmount * std::min (1.0f, duckEnv * 3.0f);

            // Flush state
            float fbGain = 1.0f, outGain = 1.0f, inGain = 1.0f;
            if (flushing)
            {
                flushPhase += 1.0f / (flushSeconds * sr);
                const float rate = 1.0f - 0.8f * std::pow (std::min (flushPhase, 1.0f), 1.3f);
                readExtra += 1.0f - rate; // read head falls behind -> pitch dive
                fbGain = (1.0f - flushPhase) * (1.0f - flushPhase);
                outGain = std::pow (std::max (0.0f, 1.0f - flushPhase), 1.2f);
                inGain = 0.0f;
                if (flushPhase >= 1.0f)
                    finishFlush();
            }

            // order change fade
            if (fadeDir != 0)
            {
                orderFade += fadeStep * (float) fadeDir;
                if (orderFade <= 0.0f)
                {
                    orderFade = 0.0f;
                    order = pendingOrder;
                    fadeDir = 1;
                }
                else if (orderFade >= 1.0f)
                {
                    orderFade = 1.0f;
                    fadeDir = 0;
                }
            }

            // wobble
            const float wd = wobbleDepthS.next();
            float wobL = 0.0f, wobR = 0.0f;
            if (wd > 0.01f)
            {
                lfoPhase += params.wobbleRate / sr;
                if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                noiseCountdown -= params.wobbleRate / sr;
                if (noiseCountdown <= 0.0f) { noiseTarget = rng.bipolar(); noiseCountdown += 0.5f; }
                noise += (noiseTarget - noise) * (params.wobbleRate * 4.0f / sr);
                wobL = wd * (1.0f + 0.7f * std::sin (twoPi * lfoPhase) + 0.3f * noise);
                wobR = wd * (1.0f + 0.7f * std::sin (twoPi * lfoPhase + 0.4f) + 0.3f * noise);
            }

            const float d = delaySamples.next();
            const float maxRead = (float) lines[0].capacity() - 16.0f;
            float yl = lines[0].read (std::min (maxRead, d + wobL + readExtra));
            float yr = lines[1].read (std::min (maxRead, d + wobR + readExtra));

            for (int k = 0; k < numModules; ++k)
            {
                auto* s = slots[order[(size_t) k]];
                if (! s->isIdle())
                    s->processFrame (yl, yr);
            }

            float lo, hi;
            lp[0].process (yl, lo, hi); yl = lo;
            lp[1].process (yr, lo, hi); yr = lo;
            hp[0].process (yl, lo, hi); yl = hi;
            hp[1].process (yr, lo, hi); yr = hi;

            yl = softLimit (yl) * orderFade;
            yr = softLimit (yr) * orderFade;

            if (! std::isfinite (yl) || ! std::isfinite (yr))
            {
                reset();
                yl = yr = 0.0f;
            }

            const float fb = feedback.next() * fbGain;
            fbL = yl * fb;
            fbR = yr * fb;

            if (params.pingPong)
            {
                lines[0].write ((xl + xr) * 0.5f * inGain + fbR);
                lines[1].write (fbL);
            }
            else
            {
                lines[0].write (xl * inGain + fbL);
                lines[1].write (xr * inGain + fbR);
            }

            const float wl = yl * duckGain * outGain, wr = yr * duckGain * outGain;
            wetPeak = std::max (wetPeak, std::max (std::abs (wl), std::abs (wr)));
            lastDuck = duckGain;

            const float m = mix.next();
            outL[i] = xl * (1.0f - m) + wl * m;
            outR[i] = xr * (1.0f - m) + wr * m;
        }

        inputLevel = inPeak;
        wetLevel = wetPeak;
    }

    // ---- metrics for the visuals / tests
    float getInputLevel() const noexcept { return inputLevel; }
    float getWetLevel() const noexcept { return wetLevel; }
    float getDuckGain() const noexcept { return lastDuck; }
    float getFlushPhase() const noexcept { return flushing ? flushPhase : 0.0f; }
    bool isFlushing() const noexcept { return flushing; }
    int getActiveGrains() const noexcept { return crystals.activeGrainCount(); }

    std::atomic<int> splashCount { 0 };
    std::atomic<float> splashAmp { 0.0f };

private:
    void finishFlush() noexcept
    {
        flushing = false;
        flushPhase = 0.0f;
        readExtra = 0.0f;
        for (auto& d : lines) d.clear();
        for (auto* s : slots) s->reset();
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        fbL = fbR = 0.0f;
    }

    EngineParams params;
    float sr = 44100.0f;
    DelayLine lines[2];
    Clog clog;
    Shift shift;
    Crystals crystals;
    Plunge plunge;
    std::array<ModuleSlot*, numModules> slots {};
    ModuleOrder order = defaultOrder(), pendingOrder = defaultOrder();
    float orderFade = 1.0f;
    int fadeDir = 0;

    Svf hp[2], lp[2];
    Smoothed delaySamples, feedback, mix, wobbleDepthS;
    float fbL = 0, fbR = 0;
    float lfoPhase = 0, noise = 0, noiseTarget = 0, noiseCountdown = 0;
    FastRandom rng { 0xBADA55u };

    float duckEnv = 0, duckAttack = 0.99f, duckRelease = 0.999f, lastDuck = 1.0f;
    float fastEnv = 0, slowEnv = 0;
    int splashCooldown = 0;

    bool flushing = false;
    float flushPhase = 0.0f, readExtra = 0.0f;

    float inputLevel = 0, wetLevel = 0;
};
}
