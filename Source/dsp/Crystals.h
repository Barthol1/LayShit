#pragma once
#include "DspUtil.h"
#include <vector>
#include <array>

namespace layshit
{
/** Dual-tap crossfading pitch shifter used for the shimmer. */
class PitchShifter
{
public:
    void prepare (double sr)
    {
        window = (float) (0.05 * sr); // 50 ms
        int size = 1;
        while (size < (int) window * 2 + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        reset();
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; phase = 0.0f; }

    float process (float x, float ratio) noexcept
    {
        buf[(size_t) w] = x;
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        float out = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            float p = phase + 0.5f * (float) k;
            p -= std::floor (p);
            const float d = 2.0f + p * window;
            out += readLin (d) * std::sin (p * 3.14159265f);
        }
        w = (w + 1) & mask;
        return out;
    }

private:
    float readLin (float d) const noexcept
    {
        const float pos = (float) w - d;
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i = (int) fl;
        return buf[(size_t) (i & mask)] * (1.0f - t) + buf[(size_t) ((i + 1) & mask)] * t;
    }
    std::vector<float> buf { 0.0f };
    int mask = 0, w = 0;
    float window = 2205.0f, phase = 0.0f;
};

/** Granular cloud over a capture buffer, followed by the shimmer pitch shifter.
    The product of grains and shimmer is what makes the toilet holy. */
class Crystals final : public ModuleSlot
{
public:
    struct Params
    {
        float amount = 0, sizeMs = 120, density = 12, sprayMs = 80, pitchSemis = 0, reverse = 0;
        bool freeze = false;
        float shimmer = 0, shimmerSemis = 12;
    };

    void setParams (const Params& p) noexcept
    {
        params = p;
        amount.set (p.amount);
        shimmer.set (p.shimmer);
        idle = p.amount < 1.0e-4f && p.shimmer < 1.0e-4f && amount.value < 1.0e-4f && shimmer.value < 1.0e-4f;
    }

    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        const int len = (int) (4.0 * sampleRate);
        int size = 1;
        while (size < len) size <<= 1;
        for (auto& c : capture)
            c.assign ((size_t) size, 0.0f);
        mask = size - 1;
        amount.prepare (sampleRate, 40.0f);
        shimmer.prepare (sampleRate, 60.0f);
        for (auto& p : shifters)
            p.prepare (sampleRate);
        reset();
    }

    void reset() override
    {
        for (auto& c : capture)
            std::fill (c.begin(), c.end(), 0.0f);
        for (auto& g : grains)
            g.active = false;
        for (auto& p : shifters)
            p.reset();
        writePos = 0;
        untilNext = 0.0f;
        amount.reset (amount.target);
        shimmer.reset (shimmer.target);
    }

    bool isIdle() const noexcept override { return idle; }

    int activeGrainCount() const noexcept
    {
        int n = 0;
        for (auto& g : grains) n += g.active ? 1 : 0;
        return n;
    }

    void processFrame (float& l, float& r) noexcept override
    {
        const float amt = amount.next();
        const float shim = shimmer.next();

        if (! params.freeze)
        {
            capture[0][(size_t) writePos] = l;
            capture[1][(size_t) writePos] = r;
            writePos = (writePos + 1) & mask;
        }

        float gl = 0.0f, gr = 0.0f;
        if (amt > 1.0e-4f)
        {
            untilNext -= 1.0f;
            if (untilNext <= 0.0f)
            {
                spawnGrain();
                const float interval = sr / std::max (1.0f, params.density);
                untilNext += interval * (0.6f + 0.8f * rng.uniform());
            }

            for (auto& g : grains)
            {
                if (! g.active) continue;
                const float w = 0.5f - 0.5f * std::cos (twoPi * g.age / g.length);
                const float sl = readLin (0, g.pos), sr_ = readLin (1, g.pos);
                gl += w * (sl * g.gainL + sr_ * (1.0f - g.gainR) * 0.3f);
                gr += w * (sr_ * g.gainR + sl * (1.0f - g.gainL) * 0.3f);
                g.pos += g.inc;
                g.age += 1.0f;
                if (g.age >= g.length) g.active = false;
            }

            const float overlap = std::max (1.0f, params.density * params.sizeMs * 0.001f);
            const float norm = 1.0f / std::sqrt (overlap);
            gl *= norm;
            gr *= norm;
            l = l + (gl - l) * amt;
            r = r + (gr - r) * amt;
        }

        if (shim > 1.0e-4f)
        {
            const float ratio = std::pow (2.0f, params.shimmerSemis / 12.0f);
            const float sl = shifters[0].process (l, ratio);
            const float sr_ = shifters[1].process (r, ratio);
            l = l * (1.0f - 0.5f * shim) + sl * shim * 0.8f;
            r = r * (1.0f - 0.5f * shim) + sr_ * shim * 0.8f;
        }
    }

private:
    struct Grain
    {
        bool active = false;
        float pos = 0, inc = 1, age = 0, length = 1, gainL = 1, gainR = 1;
    };

    void spawnGrain() noexcept
    {
        for (auto& g : grains)
        {
            if (g.active) continue;
            const float len = std::max (16.0f, params.sizeMs * 0.001f * sr);
            const float jitter = rng.uniform() * params.sprayMs * 0.001f * sr;
            const float ratio = std::pow (2.0f, params.pitchSemis / 12.0f);
            const bool rev = rng.uniform() < params.reverse;
            const float span = len * ratio; // samples consumed by the grain
            const float maxBack = (float) mask - 8.0f;

            float start;
            if (rev)
                start = (float) writePos - 2.0f - std::min (jitter, maxBack - span);
            else
                start = (float) writePos - 2.0f - std::min (span + jitter, maxBack);

            g.active = true;
            g.pos = start;
            g.inc = rev ? -ratio : ratio;
            g.age = 0.0f;
            g.length = len;
            const float pan = rng.bipolar() * 0.6f;
            g.gainL = std::sqrt (0.5f * (1.0f - pan)) * 1.4142f;
            g.gainR = std::sqrt (0.5f * (1.0f + pan)) * 1.4142f;
            return;
        }
    }

    float readLin (int ch, float pos) const noexcept
    {
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i = (int) fl;
        const auto& c = capture[(size_t) ch];
        return c[(size_t) (i & mask)] * (1.0f - t) + c[(size_t) ((i + 1) & mask)] * t;
    }

    Params params;
    std::array<std::vector<float>, 2> capture;
    std::array<Grain, 64> grains;
    PitchShifter shifters[2];
    Smoothed amount, shimmer;
    FastRandom rng { 0xC0FFEEu };
    int mask = 0, writePos = 0;
    float untilNext = 0.0f, sr = 44100.0f;
    bool idle = true;
};
}
