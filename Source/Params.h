#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace layshit::pid
{
    // core delay
    inline constexpr const char* time        = "time";
    inline constexpr const char* sync        = "sync";
    inline constexpr const char* syncDiv     = "syncDiv";
    inline constexpr const char* feedback    = "feedback";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* pingPong    = "pingPong";
    inline constexpr const char* wobbleRate  = "wobbleRate";
    inline constexpr const char* wobbleDepth = "wobbleDepth";
    inline constexpr const char* fiberHP     = "fiberHP";
    inline constexpr const char* fiberLP     = "fiberLP";
    // clog
    inline constexpr const char* clogDrive   = "clogDrive";
    inline constexpr const char* clogCrush   = "clogCrush";
    inline constexpr const char* clogMix     = "clogMix";
    // shift
    inline constexpr const char* shiftHz     = "shiftHz";
    inline constexpr const char* shiftMix    = "shiftMix";
    // crystals + shimmer
    inline constexpr const char* crysAmount  = "crysAmount";
    inline constexpr const char* crysSize    = "crysSize";
    inline constexpr const char* crysDensity = "crysDensity";
    inline constexpr const char* crysSpray   = "crysSpray";
    inline constexpr const char* crysPitch   = "crysPitch";
    inline constexpr const char* crysReverse = "crysReverse";
    inline constexpr const char* crysFreeze  = "crysFreeze";
    inline constexpr const char* shimAmt     = "shimAmt";
    inline constexpr const char* shimInterval= "shimInterval";
    // plunge
    inline constexpr const char* plungeDiffuse = "plungeDiffuse";
    inline constexpr const char* plungeSize    = "plungeSize";
    // courtesy flush (ducker)
    inline constexpr const char* duckAmount  = "duckAmount";
    inline constexpr const char* duckRelease = "duckRelease";
    // flush + macro
    inline constexpr const char* flush       = "flush";
    inline constexpr const char* bristol     = "bristol";
}

namespace layshit
{
    inline const juce::StringArray syncDivNames { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                                  "1/4T", "1/4", "1/4D", "1/2", "1/1" };

    /** Length of each sync division in quarter-note beats. */
    inline constexpr float syncDivBeats[] { 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f,
                                            2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };

    inline const juce::StringArray shimIntervalNames { "+12", "+7", "+19" };
    inline constexpr float shimIntervalSemis[] { 12.0f, 7.0f, 19.0f };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using namespace juce;
        using P = AudioParameterFloat;
        AudioProcessorValueTreeState::ParameterLayout l;

        auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
        auto hz = AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
        auto ms = AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; });

        auto skewed = [] (float lo, float hi, float centre)
        {
            NormalisableRange<float> r (lo, hi);
            r.setSkewForCentre (centre);
            return r;
        };

        auto id = [] (const char* s) { return ParameterID { s, 1 }; };
        // on/off switches as 2-step choices: unlike AudioParameterBool they always snap, so state restores exactly
        auto onOff = [&] (const char* s, const char* name, bool def)
        { return std::make_unique<AudioParameterChoice> (id (s), name, StringArray { "Off", "On" }, def ? 1 : 0); };

        l.add (std::make_unique<P> (id (pid::time), "Time", skewed (1.0f, 4000.0f, 400.0f), 375.0f, ms));
        l.add (onOff (pid::sync, "Sync", false));
        l.add (std::make_unique<AudioParameterChoice> (id (pid::syncDiv), "Sync Division", syncDivNames, 6));
        l.add (std::make_unique<P> (id (pid::feedback), "Feedback", NormalisableRange<float> (0.0f, 1.1f), 0.45f, pct));
        l.add (std::make_unique<P> (id (pid::mix), "Mix", NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
        l.add (onOff (pid::pingPong, "Ping Pong", false));
        l.add (std::make_unique<P> (id (pid::wobbleRate), "Wobble Rate", skewed (0.05f, 8.0f, 1.0f), 0.6f,
                                    AudioParameterFloatAttributes().withStringFromValueFunction (
                                        [] (float v, int) { return String (v, 2) + " Hz"; })));
        l.add (std::make_unique<P> (id (pid::wobbleDepth), "Wobble Depth", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<P> (id (pid::fiberHP), "Low Cut", skewed (20.0f, 2000.0f, 200.0f), 20.0f, hz));
        l.add (std::make_unique<P> (id (pid::fiberLP), "High Cut", skewed (500.0f, 20000.0f, 4000.0f), 14000.0f, hz));

        l.add (std::make_unique<P> (id (pid::clogDrive), "Clog Drive", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<P> (id (pid::clogCrush), "Clog Crush", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<P> (id (pid::clogMix), "Clog Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

        NormalisableRange<float> shiftRange (-1000.0f, 1000.0f,
            [] (float lo, float hi, float n) { auto x = n * 2.0f - 1.0f; return juce::jmap (x * x * x, -1.0f, 1.0f, lo, hi); },
            [] (float lo, float hi, float v) { auto x = juce::jmap (v, lo, hi, -1.0f, 1.0f); return (std::cbrt (x) + 1.0f) * 0.5f; });
        l.add (std::make_unique<P> (id (pid::shiftHz), "Shift", shiftRange, 0.0f,
                                    AudioParameterFloatAttributes().withStringFromValueFunction (
                                        [] (float v, int) { return String (v, std::abs (v) < 10.0f ? 1 : 0) + " Hz"; })));
        l.add (std::make_unique<P> (id (pid::shiftMix), "Shift Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

        l.add (std::make_unique<P> (id (pid::crysAmount), "Crystals", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<P> (id (pid::crysSize), "Grain Size", skewed (10.0f, 500.0f, 100.0f), 120.0f, ms));
        l.add (std::make_unique<P> (id (pid::crysDensity), "Density", skewed (1.0f, 100.0f, 15.0f), 12.0f,
                                    AudioParameterFloatAttributes().withStringFromValueFunction (
                                        [] (float v, int) { return String (v, 1) + " /s"; })));
        l.add (std::make_unique<P> (id (pid::crysSpray), "Spray", skewed (0.0f, 1000.0f, 150.0f), 80.0f, ms));
        l.add (std::make_unique<P> (id (pid::crysPitch), "Grain Pitch", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                                    AudioParameterFloatAttributes().withStringFromValueFunction (
                                        [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; })));
        l.add (std::make_unique<P> (id (pid::crysReverse), "Reverse", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (onOff (pid::crysFreeze, "Freeze", false));
        l.add (std::make_unique<P> (id (pid::shimAmt), "Shimmer", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<AudioParameterChoice> (id (pid::shimInterval), "Shimmer Interval", shimIntervalNames, 0));

        l.add (std::make_unique<P> (id (pid::plungeDiffuse), "Plunge", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<P> (id (pid::plungeSize), "Plunge Size", NormalisableRange<float> (0.5f, 2.0f), 1.0f,
                                    AudioParameterFloatAttributes().withStringFromValueFunction (
                                        [] (float v, int) { return String (v, 2) + "x"; })));

        l.add (std::make_unique<P> (id (pid::duckAmount), "Courtesy", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
        l.add (std::make_unique<P> (id (pid::duckRelease), "Courtesy Release", skewed (50.0f, 2000.0f, 300.0f), 350.0f, ms));

        l.add (onOff (pid::flush, "Flush", false));
        l.add (std::make_unique<P> (id (pid::bristol), "Bristol", NormalisableRange<float> (1.0f, 7.0f), 4.0f,
                                    AudioParameterFloatAttributes().withStringFromValueFunction (
                                        [] (float v, int) { return "Type " + String (v, 1); })));
        return l;
    }
}
