#pragma once
#include "DspUtil.h"
#include <vector>

namespace layshit
{
/** Chain of Schroeder allpasses that smears each repeat into a wash. */
class Plunge final : public ModuleSlot
{
public:
    void setParams (float diffuse01, float size) noexcept
    {
        diffuse.set (diffuse01);
        sizeScale = size;
        idle = diffuse01 < 1.0e-4f && diffuse.value < 1.0e-4f;
    }

    void prepare (double sampleRate) override
    {
        sr = (float) sampleRate;
        const int maxLen = (int) (0.040 * 2.0 * 1.1 * sampleRate) + 8;
        for (auto& ch : stages)
            for (auto& s : ch)
                s.buf.assign ((size_t) maxLen, 0.0f);
        diffuse.prepare (sampleRate, 40.0f);
        reset();
    }

    void reset() override
    {
        for (auto& ch : stages)
            for (auto& s : ch)
            {
                std::fill (s.buf.begin(), s.buf.end(), 0.0f);
                s.pos = 0;
            }
        diffuse.reset (diffuse.target);
    }

    bool isIdle() const noexcept override { return idle; }

    void processFrame (float& l, float& r) noexcept override
    {
        const float d = diffuse.next();
        const float g = 0.5f + 0.25f * d;
        float io[2] { l, r };
        for (int c = 0; c < 2; ++c)
        {
            float x = io[c];
            for (int k = 0; k < numStages; ++k)
            {
                auto& s = stages[c][k];
                const int len = std::clamp ((int) (delaysMs[k] * (c == 0 ? 1.0f : 1.07f) * sizeScale * 0.001f * sr),
                                            1, (int) s.buf.size() - 1);
                int readIdx = s.pos - len;
                if (readIdx < 0) readIdx += (int) s.buf.size();
                const float delayed = s.buf[(size_t) readIdx];
                const float v = x + g * delayed;
                s.buf[(size_t) s.pos] = v;
                s.pos = (s.pos + 1) % (int) s.buf.size();
                x = delayed - g * v;
            }
            io[c] = io[c] + (x - io[c]) * d;
        }
        l = io[0];
        r = io[1];
    }

private:
    static constexpr int numStages = 6;
    static constexpr float delaysMs[numStages] { 4.1f, 6.7f, 10.3f, 15.7f, 23.3f, 37.1f };

    struct Stage { std::vector<float> buf; int pos = 0; };
    Stage stages[2][numStages];
    Smoothed diffuse;
    float sizeScale = 1.0f, sr = 44100.0f;
    bool idle = true;
};
}
