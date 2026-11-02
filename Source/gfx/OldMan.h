#pragma once
#include "GfxMath.h"
#include <vector>

namespace layshit::gfx
{
/** The old man who shows up when the distortion gets cranked: walks in, drops his pants,
    sits on the toilet and strains, red-faced and sweaty, until the drive comes back down. */
class OldMan
{
public:
    enum class State { absent, walkingIn, turning, sittingDown, straining, standingUp, walkingOut };

    enum Material { skin = 3, cloth = 4, face = 5 };

    struct Part
    {
        Mat4 model;
        int material;
        Vec3 tint;
    };

    /** drive01: how cranked the distortion is. */
    void update (float dt, float now, float drive01);

    /** Appends the ellipsoid parts (all drawn with the unit sphere mesh). */
    void buildParts (std::vector<Part>& out) const;

    bool isVisible() const { return state != State::absent; }
    State getState() const { return state; }
    /** 0..1, how far the camera should pull back to fit him in. */
    float cameraWeight() const { return camWeight; }
    float redness() const { return red; }
    float strainAmount() const { return strain; }
    Vec3 headPosition() const { return joints.head; }
    float headRadius() const { return 0.55f; }
    /** Floor shadow centre (x, z) and strength. */
    Vec3 shadow() const;

    /** Number of visits finished (renderer uses it to grow the poop). */
    int completedVisits() const { return visits; }

    /** Seconds since entering the current state (for tests / tuning). */
    float timeInState() const { return stateTime; }

private:
    struct Joints
    {
        Vec3 pelvis, chest, neck, head;
        Vec3 shoulder[2], elbow[2], hand[2], hip[2], knee[2], ankle[2], toe[2];
        float headTilt = 0, facing = 0;
    };

    void setState (State s);
    Joints standingPose (Vec3 root, float facing, float walkPhase, float walkAmount) const;
    Joints seatedPose (float now) const;
    static Joints blend (const Joints& a, const Joints& b, float t);

    State state = State::absent;
    float stateTime = 0;
    Vec3 root { 4.4f, 0, 2.3f };
    float facing = 0, walkPhase = 0, sit = 0, camWeight = 0;
    float strain = 0, red = 0, pushPhase = 0, drive = 0;
    float now = 0;
    bool pantsDown = false;
    int visits = 0;
    Joints joints;
};
}
