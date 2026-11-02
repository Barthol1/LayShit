#pragma once
#include <atomic>

namespace layshit
{
/** Lock-free snapshot written by the audio thread, read by the GL renderer. */
struct VisualState
{
    std::atomic<float> inputLevel { 0 }, wetLevel { 0 }, duckGain { 1 }, flushPhase { 0 };
    std::atomic<int> splashCount { 0 };
    std::atomic<float> splashAmp { 0 };
    std::atomic<float> delaySeconds { 0.375f };
};
}
