#pragma once
#include "DspUtil.h"

namespace layshit
{
/** Saturation + bitcrush/downsample. */
class Clog final : public ModuleSlot
{
public:
    void setParams (float drive01, float crush01, float mix01) noexcept
    {
        drive.set (drive01);
        crush.set (crush01);
        mix.set (mix01);
        idle = drive01 < 1.0e-4f && crush01 < 1.0e-4f && drive.value < 1.0e-4f && crush.value < 1.0e-4f;
    }

    void prepare (double sr) override
    {
        for (auto* s : { &drive, &crush, &mix })
            s->prepare (sr, 30.0f);
        for (auto& d : dc)
            d.prepare (sr);
        reset();
    }

    void reset() override
    {
        drive.reset (drive.target);
        crush.reset (crush.target);
        mix.reset (mix.target);
        for (auto& d : dc)
            d.reset();
        holdCounter = 0.0f;
        held[0] = held[1] = 0.0f;
    }

    bool isIdle() const noexcept override { return idle; }

    void processFrame (float& l, float& r) noexcept override
    {
        const float dr = drive.next(), cr = crush.next(), mx = mix.next();

        // asymmetric tanh: gain up to +30 dB, bias adds even harmonics
        const float gain = std::pow (10.0f, dr * 1.5f);
        const float bias = 0.25f * dr;
        const float makeup = 1.0f / (1.0f + dr * 1.8f);
        const float biasOff = std::tanh (bias);

        float in[2] { l, r };
        float out[2];
        for (int c = 0; c < 2; ++c)
            out[c] = dr > 1.0e-4f ? (std::tanh (gain * in[c] + bias) - biasOff) * makeup : in[c];

        if (cr > 1.0e-4f)
        {
            const float holdLen = 1.0f + cr * cr * 31.0f;
            holdCounter += 1.0f;
            if (holdCounter >= holdLen)
            {
                holdCounter -= holdLen;
                const float bits = 16.0f - cr * 12.0f;
                const float steps = std::pow (2.0f, bits - 1.0f);
                for (int c = 0; c < 2; ++c)
                    held[c] = std::round (out[c] * steps) / steps;
            }
            for (int c = 0; c < 2; ++c)
                out[c] = out[c] + (held[c] - out[c]) * std::min (1.0f, cr * 4.0f);
        }

        for (int c = 0; c < 2; ++c)
            out[c] = dc[c].process (out[c]);

        l = l + (out[0] - l) * mx;
        r = r + (out[1] - r) * mx;
    }

private:
    Smoothed drive, crush, mix;
    DcBlocker dc[2];
    float holdCounter = 0.0f;
    float held[2] {};
    bool idle = true;
};
}
