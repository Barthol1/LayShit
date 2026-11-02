#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace layshit
{
/** Mono circular buffer with 4-point Hermite fractional reads. */
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void write (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Value written `delay` samples ago (delay >= 1 means the most recent write is delay 1).
        Fractional delays are Hermite interpolated. */
    float read (float delay) const noexcept
    {
        delay = std::clamp (delay, 2.0f, (float) (mask - 4)); // >= 2 keeps the Hermite x2 tap on written data
        const float pos = (float) writePos - delay;
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i = (int) fl;

        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    int capacity() const noexcept { return mask + 1; }

private:
    float at (int i) const noexcept { return buffer[(size_t) (i & mask)]; }

    std::vector<float> buffer { 0.0f };
    int mask = 0;
    int writePos = 0;
};
}
