#include "OldMan.h"
#include "Meshes.h"
#include <algorithm>

namespace layshit::gfx
{
namespace
{
constexpr float pi = 3.14159265f;
constexpr float walkSpeed = 1.9f;
const Vec3 frontOfToilet { 0.0f, 0.0f, 1.75f };
const Vec3 entryPoint { 4.6f, 0.0f, 2.3f };
const Vec3 exitPoint { -4.8f, 0.0f, 2.6f };

float ease (float x)
{
    x = std::clamp (x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

Vec3 lerp (Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }

float wrapAngle (float a)
{
    while (a > pi) a -= 2.0f * pi;
    while (a < -pi) a += 2.0f * pi;
    return a;
}

Vec3 rotY (Vec3 v, float a) { return { std::cos (a) * v.x + std::sin (a) * v.z, v.y, -std::sin (a) * v.x + std::cos (a) * v.z }; }

Mat4 limb (Vec3 a, Vec3 b, float r)
{
    const Vec3 d = b - a;
    const float len = std::max (1.0e-4f, d.length());
    const Vec3 dir = d * (1.0f / len);
    const Vec3 helper = std::abs (dir.y) < 0.9f ? Vec3 { 0, 1, 0 } : Vec3 { 1, 0, 0 };
    const Vec3 x = cross (helper, dir).normalised();
    const Vec3 z = cross (x, dir); // right-handed so winding / front faces stay consistent
    return Mat4::fromBasis (x * r, dir * (len * 0.5f + r * 0.55f), z * r, (a + b) * 0.5f);
}

Mat4 ellipsoid (Vec3 c, Vec3 radii, const Mat4& rot = Mat4::identity())
{
    return Mat4::translation (c) * rot * Mat4::scale (radii);
}
}

void OldMan::setState (State s)
{
    state = s;
    stateTime = 0.0f;
}

Vec3 OldMan::shadow() const
{
    const Vec3 c = (joints.pelvis + joints.ankle[0] + joints.ankle[1]) * (1.0f / 3.0f);
    return { c.x, c.z, isVisible() ? 0.75f : 0.0f };
}

void OldMan::update (float dt, float t, float drive01)
{
    now = t;
    drive = drive01;
    stateTime += dt;
    float walkAmount = 0.0f;

    auto walkTowards = [&] (Vec3 target) -> bool
    {
        const Vec3 d { target.x - root.x, 0.0f, target.z - root.z };
        const float dist = d.length();
        const float step = walkSpeed * dt;
        walkAmount = 1.0f;
        walkPhase += step * 3.4f;
        if (dist > 1.0e-4f)
        {
            const float want = std::atan2 (d.x, d.z);
            facing += wrapAngle (want - facing) * std::min (1.0f, dt * 8.0f);
        }
        if (dist <= step)
        {
            root = target;
            return true;
        }
        root += d * (step / dist);
        return false;
    };

    switch (state)
    {
        case State::absent:
            if (drive >= 0.65f)
            {
                root = entryPoint;
                facing = -pi * 0.5f;
                sit = 0.0f;
                pantsDown = false;
                setState (State::walkingIn);
            }
            break;

        case State::walkingIn:
            if (walkTowards (frontOfToilet))
                setState (State::turning);
            break;

        case State::turning:
            walkAmount = 0.35f;
            walkPhase += dt * 5.0f;
            facing += wrapAngle (0.0f - facing) * std::min (1.0f, dt * 6.0f);
            if (stateTime > 0.7f)
            {
                facing = 0.0f;
                setState (State::sittingDown);
            }
            break;

        case State::sittingDown:
            sit = ease (stateTime / 1.2f);
            pantsDown = sit > 0.25f;
            if (stateTime >= 1.2f)
            {
                sit = 1.0f;
                pushPhase = 0.0f;
                setState (State::straining);
            }
            break;

        case State::straining:
            sit = 1.0f;
            if (drive < 0.5f && stateTime > 2.5f)
                setState (State::standingUp);
            break;

        case State::standingUp:
            sit = 1.0f - ease (stateTime / 0.9f);
            pantsDown = sit > 0.5f;
            if (stateTime >= 0.9f)
            {
                sit = 0.0f;
                setState (State::walkingOut);
            }
            break;

        case State::walkingOut:
            if (walkTowards (exitPoint))
            {
                ++visits;
                setState (State::absent);
            }
            break;
    }

    // straining: pushes in cycles, harder with more drive
    const float driveNorm = std::clamp ((drive - 0.5f) / 0.5f, 0.0f, 1.0f);
    float strainTarget = 0.0f;
    if (state == State::straining)
    {
        pushPhase += dt;
        const float cycle = 3.4f - driveNorm * 0.8f;
        if (pushPhase > cycle) pushPhase -= cycle;
        const float pushEnd = cycle * 0.7f;
        const float push = pushPhase < pushEnd ? ease (pushPhase / (pushEnd * 0.85f))
                                               : 1.0f - ease ((pushPhase - pushEnd) / (cycle - pushEnd));
        strainTarget = push * (0.55f + 0.45f * driveNorm);
    }
    strain += (strainTarget - strain) * std::min (1.0f, dt * 8.0f);
    const float redTarget = isVisible() ? 0.25f + 0.75f * strain : 0.0f;
    red += (redTarget - red) * std::min (1.0f, dt * (redTarget > red ? 2.5f : 1.0f));

    camWeight += ((isVisible() ? 1.0f : 0.0f) - camWeight) * std::min (1.0f, dt * 1.4f);

    const Joints standing = standingPose (sit > 0.0f ? frontOfToilet : root, sit > 0.0f ? 0.0f : facing, walkPhase, walkAmount);
    joints = sit > 0.0f ? blend (standing, seatedPose (t), sit) : standing;
}

OldMan::Joints OldMan::standingPose (Vec3 r, float face, float phase, float w) const
{
    Joints j;
    const float fy = toilet::floorY;
    auto place = [&] (Vec3 local) { return Vec3 { r.x, fy, r.z } + rotY (local, face); };

    float swing[2], lift[2];
    for (int i = 0; i < 2; ++i)
    {
        const float s = std::sin (phase + (float) i * pi);
        swing[i] = s * 0.38f * w;
        lift[i] = std::max (0.0f, s) * 0.22f * w;
    }

    j.pelvis = place ({ 0.0f, 3.05f + std::abs (std::sin (phase)) * 0.07f * w, 0.0f });
    j.chest = place ({ 0.0f, 3.95f, 0.06f });
    j.neck = place ({ 0.0f, 4.75f, 0.05f });
    j.head = place ({ 0.0f, 5.25f, 0.12f });
    for (int i = 0; i < 2; ++i)
    {
        const float sx = i == 0 ? 1.0f : -1.0f;
        j.hip[i] = place ({ sx * 0.32f, 2.95f, 0.0f });
        j.knee[i] = place ({ sx * 0.34f, 1.7f + lift[i] * 0.5f, swing[i] * 0.55f + 0.05f });
        j.ankle[i] = place ({ sx * 0.34f, 0.24f + lift[i], swing[i] * 0.95f });
        j.toe[i] = place ({ sx * 0.34f, 0.16f + lift[i], swing[i] * 0.95f + 0.4f });
        j.shoulder[i] = place ({ sx * 0.72f, 4.5f, 0.0f });
        j.elbow[i] = place ({ sx * 0.8f, 3.7f, -swing[i] * 0.45f });
        j.hand[i] = place ({ sx * 0.78f, 3.0f, -swing[i] * 0.7f });
    }
    j.headTilt = 0.25f;
    j.facing = face;
    return j;
}

OldMan::Joints OldMan::seatedPose (float t) const
{
    Joints j;
    const float fy = toilet::floorY;
    const float s = strain;
    const Vec3 lean { 0.0f, -0.08f * s, 0.24f * s };
    const Vec3 tremble { std::sin (t * 47.0f) * 0.014f * s, std::sin (t * 39.0f) * 0.01f * s, 0.0f };

    j.pelvis = { 0.0f, 0.42f, -0.32f };
    j.chest = Vec3 { 0.0f, 1.35f, 0.0f } + lean + tremble;
    j.neck = Vec3 { 0.0f, 2.12f, 0.28f } + lean * 1.1f + tremble;
    j.head = Vec3 { 0.0f, 2.62f, 0.48f } + lean * 1.2f + tremble * 1.4f;
    for (int i = 0; i < 2; ++i)
    {
        const float sx = i == 0 ? 1.0f : -1.0f;
        j.hip[i] = { sx * 0.34f, 0.45f, -0.05f };
        j.knee[i] = { sx * 0.46f, 0.66f, 1.3f };
        j.ankle[i] = { sx * 0.42f, fy + 0.24f, 1.45f };
        j.toe[i] = { sx * 0.42f, fy + 0.16f, 1.85f };
        j.shoulder[i] = Vec3 { sx * 0.72f, 1.9f, 0.12f } + lean + tremble;
        j.elbow[i] = { sx * 0.66f, 0.98f, 1.12f };
        // fists clench and shake harder while pushing
        j.hand[i] = Vec3 { sx * (0.3f - 0.06f * s), 1.2f + 0.08f * s, 1.42f } + tremble * 2.0f;
    }
    j.headTilt = 0.62f - 0.12f * s;
    j.facing = 0.0f;
    return j;
}

OldMan::Joints OldMan::blend (const Joints& a, const Joints& b, float t)
{
    Joints j;
    j.pelvis = lerp (a.pelvis, b.pelvis, t);
    j.chest = lerp (a.chest, b.chest, t);
    j.neck = lerp (a.neck, b.neck, t);
    j.head = lerp (a.head, b.head, t);
    for (int i = 0; i < 2; ++i)
    {
        j.shoulder[i] = lerp (a.shoulder[i], b.shoulder[i], t);
        j.elbow[i] = lerp (a.elbow[i], b.elbow[i], t);
        j.hand[i] = lerp (a.hand[i], b.hand[i], t);
        j.hip[i] = lerp (a.hip[i], b.hip[i], t);
        j.knee[i] = lerp (a.knee[i], b.knee[i], t);
        j.ankle[i] = lerp (a.ankle[i], b.ankle[i], t);
        j.toe[i] = lerp (a.toe[i], b.toe[i], t);
    }
    j.headTilt = a.headTilt + (b.headTilt - a.headTilt) * t;
    j.facing = a.facing + wrapAngle (b.facing - a.facing) * t;
    return j;
}

void OldMan::buildParts (std::vector<Part>& out) const
{
    if (! isVisible())
        return;

    const Vec3 white { 1, 1, 1 };
    const Vec3 shirt { 0.94f, 0.94f, 0.9f };    // classic tank top
    const Vec3 pants { 0.36f, 0.27f, 0.2f };
    const Vec3 shoes { 0.1f, 0.085f, 0.075f };
    const Vec3 hair { 0.82f, 0.82f, 0.8f };
    const auto& j = joints;
    const Vec3 fwd = rotY ({ 0, 0, 1 }, j.facing);
    const Mat4 bodyRot = Mat4::rotationY (j.facing);

    auto add = [&] (const Mat4& m, int mat, Vec3 tint) { out.push_back ({ m, mat, tint }); };

    // legs and feet
    for (int i = 0; i < 2; ++i)
    {
        add (limb (j.hip[i], j.knee[i], 0.28f), pantsDown ? skin : cloth, pantsDown ? white : pants);
        add (limb (j.knee[i], j.ankle[i], 0.2f), pantsDown ? skin : cloth, pantsDown ? white : pants);
        add (limb (j.ankle[i] + Vec3 { 0, -0.04f, -0.08f }, j.toe[i], 0.17f), cloth, shoes);
    }
    if (pantsDown)
    {
        // trousers bunched around the ankles
        const Vec3 mid = (j.ankle[0] + j.ankle[1]) * 0.5f + Vec3 { 0, 0.16f, 0 };
        add (ellipsoid (mid, { 0.78f, 0.2f, 0.36f }, bodyRot), cloth, pants);
        for (int i = 0; i < 2; ++i)
            add (ellipsoid (j.ankle[i] + Vec3 { 0, 0.2f, 0 }, { 0.3f, 0.24f, 0.32f }, bodyRot), cloth, pants);
    }

    // hips, torso, the belly
    add (ellipsoid (j.pelvis, { 0.62f, 0.42f, 0.55f }, bodyRot), pantsDown ? skin : cloth, pantsDown ? white : pants);
    add (limb (j.pelvis + Vec3 { 0, 0.2f, 0 }, j.neck - Vec3 { 0, 0.18f, 0 }, 0.6f), cloth, shirt);
    add (ellipsoid (lerp (j.pelvis, j.chest, 0.45f) + fwd * 0.3f, { 0.62f, 0.56f, 0.52f }, bodyRot), cloth, shirt);

    // arms
    for (int i = 0; i < 2; ++i)
    {
        add (limb (j.shoulder[i], j.elbow[i], 0.18f), skin, white);
        add (limb (j.elbow[i], j.hand[i], 0.16f), skin, white);
        add (ellipsoid (j.hand[i], { 0.19f, 0.17f, 0.19f }, bodyRot), skin, white);
        add (ellipsoid (j.shoulder[i], { 0.24f, 0.22f, 0.24f }, bodyRot), skin, white);
    }

    // neck and head
    add (limb (j.neck - Vec3 { 0, 0.1f, 0 }, j.head - Vec3 { 0, 0.35f, 0 }, 0.23f), skin, white);
    const Mat4 headBase = Mat4::translation (j.head) * Mat4::rotationY (j.facing) * Mat4::rotationX (-j.headTilt);
    auto onHead = [&] (Vec3 offset, Vec3 radii, const Mat4& rot = Mat4::identity())
    {
        return headBase * Mat4::translation (offset) * rot * Mat4::scale (radii);
    };
    add (headBase * Mat4::scale ({ 0.55f, 0.6f, 0.55f }), face, white);
    add (onHead ({ 0.0f, -0.04f, 0.56f }, { 0.15f, 0.13f, 0.15f }), skin, { 1.0f, 0.9f, 0.88f });   // nose
    for (int i = 0; i < 2; ++i)
    {
        const float sx = i == 0 ? 1.0f : -1.0f;
        add (onHead ({ sx * 0.55f, 0.0f, 0.02f }, { 0.09f, 0.17f, 0.12f }), skin, white);                 // ears
        add (onHead ({ sx * 0.43f, 0.06f, -0.2f }, { 0.2f, 0.2f, 0.28f }), cloth, hair);                 // fringe
        const float furrow = sx * (0.3f + 0.3f * strain);
        add (onHead ({ sx * 0.2f, 0.25f - 0.03f * strain, 0.49f }, { 0.17f, 0.06f, 0.07f }, Mat4::rotationZ (furrow)), cloth, hair); // brows
        add (onHead ({ sx * 0.12f, -0.21f, 0.5f }, { 0.16f, 0.06f, 0.09f }, Mat4::rotationZ (-sx * 0.35f)), cloth, hair);         // moustache
    }
    add (onHead ({ 0.0f, 0.02f, -0.46f }, { 0.36f, 0.2f, 0.16f }), cloth, hair);
}
}
