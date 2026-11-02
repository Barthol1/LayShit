#pragma once
#include <juce_opengl/juce_opengl.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Meshes.h"
#include "VisualState.h"
#include "OldMan.h"
#include <functional>
#include <memory>
#include <mutex>

namespace layshit::gfx
{
/** Renders the 3D toilet scene (bowl, water, poop, particles, holy god-rays) with OpenGL. */
class BowlRenderer final : public juce::OpenGLRenderer
{
public:
    BowlRenderer (juce::OpenGLContext& ctx, juce::AudioProcessorValueTreeState& apvts, VisualState& vs);
    ~BowlRenderer() override;

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    /** If set, once `snapshotAtSeconds` have passed the composited 3D frame is handed over (GL thread). */
    std::function<void (juce::Image)> onSnapshot;
    float snapshotAtSeconds = -1.0f;
    bool snapshotTaken = false;

    /** Frosted-glass panels (component coordinates) blurred by the compositor. Message thread. */
    struct GlassRect { juce::Rectangle<float> bounds; float radius; };
    void setGlass (std::vector<GlassRect> rects);

    /** True while the old man is in the room (readable from any thread). */
    std::atomic<bool> occupied { false };

    /** Where the bowl centre lands on screen in component coordinates (for UI layout). */
    static constexpr float bowlCentreXRatio = 0.5f;

private:
    struct GpuMesh
    {
        unsigned int vao = 0, vbo = 0, ebo = 0;
        int indexCount = 0;
        void upload (const MeshData& m, bool dynamic);
        void updateVertices (const MeshData& m);
        void draw() const;
        void release();
    };

    struct Fbo
    {
        unsigned int fbo = 0, colour = 0, depth = 0, tex = 0;
        int w = 0, h = 0, samples = 0;
        void create (int width, int height, int msaa, bool asTexture);
        void release();
    };

    struct Ripple { float x = 0, z = 0, birth = -100, amp = 0; };
    struct Echo { float when = 0, x = 0, z = 0, amp = 0; bool live = false; };
    enum ParticleKind { mote, holySpark, droplet, sweat, steam };
    struct Particle { float x, y, z, vx, vy, vz, life, maxLife, size; float r, g, b; bool live = false; int kind = mote; };
    void spawnParticle (int kind, Vec3 pos, Vec3 vel, float maxLife, float size, Vec3 colour);

    float param (const char* id) const;
    float holyAmountFromParams() const;
    void updateSimulation (float dt, float now);
    void spawnRipple (float x, float z, float amp, float now);
    void buildParticles (float dt);
    void captureSnapshot (int w, int h);

    juce::OpenGLContext& context;
    juce::AudioProcessorValueTreeState& apvts;
    VisualState& visual;

    std::unique_ptr<juce::OpenGLShaderProgram> litProgram, waterProgram, pointProgram, compositeProgram;
    GpuMesh bowl, seat, lid, tank, tankLid, floorMesh, water, poop, sphere;
    OldMan oldMan;
    std::vector<OldMan::Part> manParts;
    float sweatTimer = 0, steamTimer = 0, poopBonus = 0;
    int visitsSeen = 0;
    unsigned int pointVao = 0, pointVbo = 0, emptyVao = 0;
    Fbo msaaFbo, sceneFbo;
    MeshData poopData;

    // simulation state (GL thread only)
    double startMs = 0, lastMs = 0;
    int frameCounter = 0;
    float poopAngle = 0, driftPhase = 0, jumpY = 0, jumpV = 0, respawn = 1, holySmoothed = 0;
    float overflow = 0, puddle = 0, flushSmoothed = 0, duckSmoothed = 1, wasFlushing = 0, swirlSmoothed = 0;
    juce::Point<float> poopXZ;
    int lastSplash = 0;
    std::array<Ripple, 16> ripples;
    std::array<Echo, 32> echoes;
    std::array<Particle, 512> particles;
    std::vector<float> pointData;
    float ambientRippleTimer = 0;
    juce::Random rng;
    juce::SpinLock glassLock;
    std::vector<GlassRect> glass;
};
}
