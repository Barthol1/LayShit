#include <juce_core/juce_core.h>
#include <cstdio>

namespace
{
struct StdoutLogger final : juce::Logger
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); }
};
}

int main()
{
    StdoutLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("LayShit");

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    juce::Logger::setCurrentLogger (nullptr);
    return failures == 0 ? 0 : 1;
}
