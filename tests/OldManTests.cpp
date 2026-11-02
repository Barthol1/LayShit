#include <juce_core/juce_core.h>
#include "gfx/OldMan.h"

using layshit::gfx::OldMan;

namespace
{
void run (OldMan& m, float& t, float seconds, float drive)
{
    const float dt = 1.0f / 60.0f;
    for (float s = 0; s < seconds; s += dt)
    {
        t += dt;
        m.update (dt, t, drive);
    }
}
}

class OldManTests final : public juce::UnitTest
{
public:
    OldManTests() : juce::UnitTest ("LayShit Old Man", "LayShit") {}

    void runTest() override
    {
        beginTest ("Stays away at moderate drive (hysteresis)");
        {
            OldMan m;
            float t = 0;
            run (m, t, 5.0f, 0.6f);
            expect (! m.isVisible());
        }

        beginTest ("Cranked drive: walks in, sits, strains");
        {
            OldMan m;
            float t = 0;
            run (m, t, 0.5f, 1.0f);
            expect (m.getState() == OldMan::State::walkingIn);
            run (m, t, 7.0f, 1.0f);
            expect (m.getState() == OldMan::State::straining);
            expect (m.redness() > 0.25f, "red in the face");

            std::vector<OldMan::Part> parts;
            m.buildParts (parts);
            expect (parts.size() > 20, "has a body");

            float maxStrain = 0;
            for (int i = 0; i < 300; ++i)
            {
                run (m, t, 1.0f / 60.0f, 1.0f);
                maxStrain = std::max (maxStrain, m.strainAmount());
            }
            expect (maxStrain > 0.6f, "pushes hard");
        }

        beginTest ("Drive back down: stands up, leaves, visit counted");
        {
            OldMan m;
            float t = 0;
            run (m, t, 8.0f, 1.0f);
            run (m, t, 0.2f, 0.3f);
            expect (m.getState() == OldMan::State::standingUp || m.getState() == OldMan::State::walkingOut);
            run (m, t, 6.0f, 0.3f);
            expect (! m.isVisible());
            expectEquals (m.completedVisits(), 1);
            run (m, t, 3.0f, 0.3f);
            expect (m.cameraWeight() < 0.05f, "camera returns");
        }

        beginTest ("Brief dip does not make him leave before he has sat a while");
        {
            OldMan m;
            float t = 0;
            run (m, t, 4.6f, 1.0f); // just sat down
            run (m, t, 0.5f, 0.0f);
            expect (m.getState() == OldMan::State::straining);
        }
    }
};

static OldManTests oldManTests;
