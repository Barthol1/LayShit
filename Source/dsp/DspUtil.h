#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace layshit
{
inline constexpr float twoPi = 6.283185307179586f;

/** Tiny deterministic RNG that is safe to call on the audio thread. */
struct FastRandom
{
    uint32_t state = 0x9E3779B9u;
    explicit FastRandom (uint32_t seed = 0x9E3779B9u) : state (seed ? seed : 1u) {}
    uint32_t next() noexcept { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0,1)
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }                   // [-1,1)
};

/** One-pole parameter smoother. */
struct Smoothed
{
    float value = 0.0f, target = 0.0f, coeff = 0.0f;
    void prepare (double sampleRate, float timeMs) noexcept
    {
        coeff = std::exp (-1.0f / (float) (sampleRate * timeMs * 0.001));
    }
    void reset (float v) noexcept { value = target = v; }
    void set (float t) noexcept { target = t; }
    float next() noexcept { value = target + (value - target) * coeff; return value; }
};

/** Linear below 0.9, tanh knee above; output magnitude never exceeds 1. */
inline float softLimit (float x) noexcept
{
    constexpr float t = 0.9f;
    const float ax = std::abs (x);
    if (ax <= t)
        return x;
    const float y = t + (1.0f - t) * std::tanh ((ax - t) / (1.0f - t));
    return x < 0.0f ? -y : y;
}

inline float smoothstep (float e0, float e1, float x) noexcept
{
    const float t = std::clamp ((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/** How holy the toilet is: emerges when both grains and shimmer are up. */
inline float holyAmount (float crystalsAmount, float shimmerAmount) noexcept
{
    return smoothstep (0.25f, 0.75f, crystalsAmount * shimmerAmount * 2.0f);
}

/** Simple DC blocker. */
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;
    void prepare (double sr) noexcept { r = 1.0f - (float) (twoPi * 20.0 / sr); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

/** Base for the reorderable feedback-loop modules. Processes one stereo frame at a time
    because they live inside a feedback loop whose delay can be shorter than a block. */
class ModuleSlot
{
public:
    virtual ~ModuleSlot() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void processFrame (float& l, float& r) noexcept = 0;
    /** True when the module currently does nothing audible (lets the loop skip it). */
    virtual bool isIdle() const noexcept { return false; }
};
}
