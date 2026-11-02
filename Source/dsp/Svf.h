#pragma once
#include "DspUtil.h"

namespace layshit
{
/** Topology-preserving-transform state variable filter (Butterworth Q), LP and HP outputs. */
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1.41421356f, a1 = 0, a2 = 0, a3 = 0;

    void setCutoff (float hz, float sampleRate) noexcept
    {
        hz = std::clamp (hz, 10.0f, sampleRate * 0.49f);
        g = std::tan (3.14159265f * hz / sampleRate);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1 = ic2 = 0; }

    /** Runs one sample; writes low- and high-pass outputs. */
    void process (float x, float& lp, float& hp) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        hp = x - k * v1 - v2;
    }
};
}
