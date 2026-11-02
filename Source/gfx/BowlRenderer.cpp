#include "BowlRenderer.h"
#include "Shaders.h"
#include "Params.h"

using namespace juce::gl;

namespace layshit::gfx
{
namespace
{
constexpr float pi = 3.14159265f;

// camera shared by everything
const Vec3 eye { 0.0f, 6.3f, 2.75f };
const Vec3 target { 0.0f, -0.45f, -0.05f };
const Vec3 eyeFar { 0.0f, 11.2f, 6.0f };
const Vec3 targetFar { 0.0f, -0.25f, 0.45f };
constexpr float fovY = 0.62f;

std::unique_ptr<juce::OpenGLShaderProgram> buildProgram (juce::OpenGLContext& ctx, const char* vs, const char* fs,
                                                          std::initializer_list<const char*> attribs)
{
    auto p = std::make_unique<juce::OpenGLShaderProgram> (ctx);
    if (! p->addVertexShader (vs) || ! p->addFragmentShader (fs))
    {
        DBG ("Shader compile error: " << p->getLastError());
        jassertfalse;
        return nullptr;
    }
    GLuint idx = 0;
    for (auto* a : attribs)
        ctx.extensions.glBindAttribLocation (p->getProgramID(), idx++, a);
    if (! p->link())
    {
        DBG ("Shader link error: " << p->getLastError());
        jassertfalse;
        return nullptr;
    }
    return p;
}

GLint loc (juce::OpenGLShaderProgram& p, const char* name) { return glGetUniformLocation (p.getProgramID(), name); }
void setMat (juce::OpenGLShaderProgram& p, const char* n, const Mat4& m) { glUniformMatrix4fv (loc (p, n), 1, GL_FALSE, m.m.data()); }
void set1f (juce::OpenGLShaderProgram& p, const char* n, float v) { glUniform1f (loc (p, n), v); }
void set1i (juce::OpenGLShaderProgram& p, const char* n, int v) { glUniform1i (loc (p, n), v); }
void set2f (juce::OpenGLShaderProgram& p, const char* n, float a, float b) { glUniform2f (loc (p, n), a, b); }
void set3f (juce::OpenGLShaderProgram& p, const char* n, Vec3 v) { glUniform3f (loc (p, n), v.x, v.y, v.z); }

juce::Point<float> projectToUv (const Mat4& viewProj, Vec3 p)
{
    const auto c = viewProj.transform (p.x, p.y, p.z, 1.0f);
    const float w = std::abs (c[3]) > 1.0e-6f ? c[3] : 1.0e-6f;
    return { c[0] / w * 0.5f + 0.5f, c[1] / w * 0.5f + 0.5f };
}
}

// ------------------------------------------------------------------ GPU helpers
void BowlRenderer::GpuMesh::upload (const MeshData& m, bool dynamic)
{
    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);
    glGenBuffers (1, &vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (m.vertices.size() * sizeof (float)), m.vertices.data(),
                  dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glGenBuffers (1, &ebo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (m.indices.size() * sizeof (uint32_t)), m.indices.data(), GL_STATIC_DRAW);
    const GLsizei stride = MeshData::floatsPerVertex * sizeof (float);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 3, GL_FLOAT, GL_FALSE, stride, (void*) (3 * sizeof (float)));
    glEnableVertexAttribArray (2);
    glVertexAttribPointer (2, 2, GL_FLOAT, GL_FALSE, stride, (void*) (6 * sizeof (float)));
    glBindVertexArray (0);
    indexCount = (int) m.indices.size();
}

void BowlRenderer::GpuMesh::updateVertices (const MeshData& m)
{
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (m.vertices.size() * sizeof (float)), m.vertices.data(), GL_DYNAMIC_DRAW);
}

void BowlRenderer::GpuMesh::draw() const
{
    glBindVertexArray (vao);
    glDrawElements (GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
}

void BowlRenderer::GpuMesh::release()
{
    if (vao) glDeleteVertexArrays (1, &vao);
    if (vbo) glDeleteBuffers (1, &vbo);
    if (ebo) glDeleteBuffers (1, &ebo);
    vao = vbo = ebo = 0;
}

void BowlRenderer::Fbo::create (int width, int height, int msaa, bool asTexture)
{
    release();
    w = width; h = height; samples = msaa;
    glGenFramebuffers (1, &fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    if (asTexture)
    {
        glGenTextures (1, &tex);
        glBindTexture (GL_TEXTURE_2D, tex);
        glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    }
    else
    {
        glGenRenderbuffers (1, &colour);
        glBindRenderbuffer (GL_RENDERBUFFER, colour);
        glRenderbufferStorageMultisample (GL_RENDERBUFFER, msaa, GL_RGBA8, w, h);
        glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colour);
        glGenRenderbuffers (1, &depth);
        glBindRenderbuffer (GL_RENDERBUFFER, depth);
        glRenderbufferStorageMultisample (GL_RENDERBUFFER, msaa, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    }
}

void BowlRenderer::Fbo::release()
{
    if (fbo) glDeleteFramebuffers (1, &fbo);
    if (colour) glDeleteRenderbuffers (1, &colour);
    if (depth) glDeleteRenderbuffers (1, &depth);
    if (tex) glDeleteTextures (1, &tex);
    fbo = colour = depth = tex = 0;
    w = h = 0;
}

// ------------------------------------------------------------------ renderer
BowlRenderer::BowlRenderer (juce::OpenGLContext& ctx, juce::AudioProcessorValueTreeState& s, VisualState& vs)
    : context (ctx), apvts (s), visual (vs)
{
}

BowlRenderer::~BowlRenderer() = default;

void BowlRenderer::setGlass (std::vector<GlassRect> rects)
{
    const juce::SpinLock::ScopedLockType sl (glassLock);
    glass = std::move (rects);
}

float BowlRenderer::param (const char* id) const
{
    if (auto* p = apvts.getRawParameterValue (id))
        return p->load();
    return 0.0f;
}

void BowlRenderer::newOpenGLContextCreated()
{
    litProgram = buildProgram (context, shaders::litVertex, shaders::litFragment, { "aPos", "aNormal", "aUv" });
    waterProgram = buildProgram (context, shaders::waterVertex, shaders::waterFragment, { "aPos", "aNormal", "aUv" });
    pointProgram = buildProgram (context, shaders::pointVertex, shaders::pointFragment, { "aPosSize", "aColor" });
    compositeProgram = buildProgram (context, shaders::compositeVertex, shaders::compositeFragment, {});

    bowl.upload (makeBowl(), false);
    seat.upload (makeSeat(), false);
    lid.upload (makeLid(), false);
    tank.upload (makeRoundedBox ({ 1.02f, 0.62f, 0.34f }, 0.22f, 24, 48), false);
    tankLid.upload (makeRoundedBox ({ 1.08f, 0.055f, 0.40f }, 0.18f, 12, 48), false);
    floorMesh.upload (makePlane (18.0f), false);
    sphere.upload (makeSphere (28, 40), false);
    water.upload (makeDisc (56, 128), false);

    buildPoop ({}, poopData);
    poop.upload (poopData, true);

    glGenVertexArrays (1, &pointVao);
    glBindVertexArray (pointVao);
    glGenBuffers (1, &pointVbo);
    glBindBuffer (GL_ARRAY_BUFFER, pointVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (particles.size() * 8 * sizeof (float)), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray (0);
    glVertexAttribPointer (0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof (float), nullptr);
    glEnableVertexAttribArray (1);
    glVertexAttribPointer (1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof (float), (void*) (4 * sizeof (float)));
    glBindVertexArray (0);

    glGenVertexArrays (1, &emptyVao);

    startMs = lastMs = juce::Time::getMillisecondCounterHiRes();
}

void BowlRenderer::openGLContextClosing()
{
    for (auto* m : { &bowl, &seat, &lid, &tank, &tankLid, &floorMesh, &water, &poop, &sphere })
        m->release();
    if (pointVao) glDeleteVertexArrays (1, &pointVao);
    if (pointVbo) glDeleteBuffers (1, &pointVbo);
    if (emptyVao) glDeleteVertexArrays (1, &emptyVao);
    pointVao = pointVbo = emptyVao = 0;
    msaaFbo.release();
    sceneFbo.release();
    litProgram.reset();
    waterProgram.reset();
    pointProgram.reset();
    compositeProgram.reset();
}

void BowlRenderer::spawnRipple (float x, float z, float amp, float now)
{
    // replace the oldest ripple
    auto* oldest = &ripples[0];
    for (auto& r : ripples)
        if (r.birth < oldest->birth) oldest = &r;
    *oldest = { x, z, now, amp };
}

void BowlRenderer::updateSimulation (float dt, float now)
{
    const float fb = param (pid::feedback);
    const float shiftHz = param (pid::shiftHz);
    const float flush = visual.flushPhase.load();
    const float duck = visual.duckGain.load();
    const float delaySec = std::max (0.02f, visual.delaySeconds.load());

    // flush: spin up, then respawn when done
    flushSmoothed = flush;
    if (flush > 0.0f)
        wasFlushing = 1.0f;
    else if (wasFlushing > 0.0f)
    {
        wasFlushing = 0.0f;
        respawn = 0.0f;
        spawnRipple (0.0f, 0.0f, 0.03f, now);
    }
    respawn = std::min (1.0f, respawn + dt * 1.6f);

    // Shift: rotation direction and speed
    const float spin = (shiftHz >= 0 ? 1.0f : -1.0f) * std::sqrt (std::abs (shiftHz)) * 0.12f;
    swirlSmoothed += (spin - swirlSmoothed) * std::min (1.0f, dt * 3.0f);
    poopAngle += (swirlSmoothed + flush * flush * 14.0f) * dt;

    // Feedback: drift around the bowl
    driftPhase += dt * (0.25f + fb * 0.35f);

    // Courtesy: sink while playing
    duckSmoothed += (duck - duckSmoothed) * std::min (1.0f, dt * 6.0f);

    // splash on transients + echoes of it every delay tap
    const int sc = visual.splashCount.load();
    if (sc != lastSplash)
    {
        lastSplash = sc;
        const float amp = juce::jlimit (0.2f, 1.0f, visual.splashAmp.load());
        jumpV += 0.9f * amp;
        spawnRipple (poopXZ.x, poopXZ.y, 0.055f * amp, now);
        for (int k = 1; k <= 8; ++k)
        {
            const float a = 0.045f * amp * std::pow (std::min (fb, 1.0f), (float) k);
            if (a < 0.002f) break;
            for (auto& e : echoes)
                if (! e.live)
                {
                    const float ang = rng.nextFloat() * 2.0f * pi;
                    e = { now + delaySec * (float) k, 0.25f * std::cos (ang), 0.25f * std::sin (ang), a, true };
                    break;
                }
        }
        // droplets
        const int drops = (int) (14 * amp) + 4;
        for (int k = 0; k < drops; ++k)
        {
            const float ang = rng.nextFloat() * 2.0f * pi, sp = 0.4f + rng.nextFloat() * 0.6f;
            spawnParticle (droplet, { poopXZ.x, toilet::waterY + 0.05f, poopXZ.y },
                           { std::cos (ang) * sp * 0.6f, 1.2f + rng.nextFloat() * 1.0f, std::sin (ang) * sp * 0.6f }, 0.9f, 0.05f,
                           { 0.7f, 0.85f, 0.9f });
        }
    }

    // the old man shows up when the distortion is cranked
    const float drive = param (pid::clogDrive) * juce::jlimit (0.0f, 1.0f, param (pid::clogMix) * 1.5f);
    oldMan.update (dt, now, drive);
    occupied.store (oldMan.isVisible());
    if (oldMan.completedVisits() != visitsSeen)
    {
        visitsSeen = oldMan.completedVisits();
        poopBonus = std::min (0.6f, poopBonus + 0.25f); // he left a contribution
        respawn = 0.0f;
        spawnRipple (0.0f, 0.0f, 0.05f, now);
    }
    poopBonus = std::max (0.0f, poopBonus - dt * 0.004f);

    if (oldMan.getState() == OldMan::State::straining || oldMan.getState() == OldMan::State::sittingDown)
    {
        const float strain = oldMan.strainAmount();
        const Vec3 head = oldMan.headPosition();
        const float r = oldMan.headRadius();
        sweatTimer -= dt * (3.0f + 22.0f * strain);
        while (sweatTimer <= 0.0f)
        {
            sweatTimer += 1.0f;
            const float ang = rng.nextFloat() * 2.0f * pi;
            const Vec3 dir { std::cos (ang), 0.35f + rng.nextFloat() * 0.5f, std::sin (ang) };
            spawnParticle (sweat, head + Vec3 { dir.x * r, dir.y * r * 0.8f, dir.z * r },
                           { dir.x * (0.8f + strain), 0.6f + rng.nextFloat() * 1.2f * (0.4f + strain), dir.z * (0.8f + strain) },
                           1.4f, 0.11f + rng.nextFloat() * 0.06f, { 0.8f, 0.95f, 1.0f });
        }
        steamTimer -= dt * strain * strain * 14.0f;
        while (steamTimer <= 0.0f)
        {
            steamTimer += 1.0f;
            spawnParticle (steam, head + Vec3 { (rng.nextFloat() - 0.5f) * 0.6f, r * 0.9f, (rng.nextFloat() - 0.5f) * 0.4f },
                           { (rng.nextFloat() - 0.5f) * 0.3f, 1.1f + rng.nextFloat() * 0.6f, (rng.nextFloat() - 0.5f) * 0.3f }, 1.6f,
                           0.28f + rng.nextFloat() * 0.18f, { 0.92f, 0.92f, 0.95f });
        }
    }
    for (auto& e : echoes)
        if (e.live && now >= e.when)
        {
            spawnRipple (e.x, e.z, e.amp, now);
            e.live = false;
        }

    // a gentle ambient ripple now and then so the water never looks dead
    ambientRippleTimer -= dt;
    if (ambientRippleTimer <= 0.0f)
    {
        ambientRippleTimer = 1.2f + rng.nextFloat() * 2.0f;
        spawnRipple (poopXZ.x, poopXZ.y, 0.004f + 0.008f * visual.wetLevel.load(), now);
    }

    // jump spring
    jumpV += (-jumpY * 40.0f - jumpV * 5.0f) * dt;
    jumpY += jumpV * dt;
    jumpY = juce::jlimit (-0.2f, 0.4f, jumpY);

    // overflow (feedback past ~95%)
    const float ovTarget = juce::jlimit (0.0f, 1.0f, (fb - 0.95f) / 0.13f);
    overflow += (ovTarget - overflow) * std::min (1.0f, dt * 0.8f);
    puddle = juce::jlimit (0.0f, 1.0f, puddle + (overflow > 0.6f ? dt * 0.12f : -dt * 0.05f));

    // holy
    const float holyTarget = holyAmountFromParams();
    holySmoothed += (holyTarget - holySmoothed) * std::min (1.0f, dt * 2.5f);
}

void BowlRenderer::spawnParticle (int kind, Vec3 pos, Vec3 vel, float maxLife, float size, Vec3 c)
{
    for (auto& p : particles)
        if (! p.live)
        {
            p = { pos.x, pos.y, pos.z, vel.x, vel.y, vel.z, 0.0f, maxLife, size, c.x, c.y, c.z, true, kind };
            return;
        }
}

void BowlRenderer::buildParticles (float dt)
{
    const float crystals = param (pid::crysAmount);
    const float density = param (pid::crysDensity);

    // keep a population of crystal motes and holy sparkles
    const int wantMotes = (int) (crystals * (40.0f + density * 1.2f));
    const int wantHoly = (int) (holySmoothed * 160.0f);
    int motes = 0, holy = 0;
    for (auto& p : particles)
        if (p.live)
        {
            if (p.kind == mote) ++motes;
            else if (p.kind == holySpark) ++holy;
        }

    for (; motes < wantMotes; ++motes)
    {
        const float ang = rng.nextFloat() * 2.0f * pi, rad = 0.15f + rng.nextFloat() * 0.45f;
        spawnParticle (mote, { poopXZ.x + std::cos (ang) * rad, toilet::waterY + rng.nextFloat() * 0.5f, poopXZ.y + std::sin (ang) * rad },
                       { -std::sin (ang) * 0.25f, 0.05f + rng.nextFloat() * 0.1f, std::cos (ang) * 0.25f },
                       1.5f + rng.nextFloat() * 1.5f, 0.035f + rng.nextFloat() * 0.03f, { 0.75f, 0.85f, 1.0f });
    }
    for (; holy < wantHoly; ++holy)
    {
        const float ang = rng.nextFloat() * 2.0f * pi, rad = rng.nextFloat() * 0.5f;
        spawnParticle (holySpark, { std::cos (ang) * rad, toilet::waterY + 0.05f, std::sin (ang) * rad * 1.2f },
                       { std::cos (ang) * 0.15f, 1.0f + rng.nextFloat() * 1.8f, std::sin (ang) * 0.15f },
                       1.6f + rng.nextFloat(), 0.05f + rng.nextFloat() * 0.06f, { 1.0f, 0.85f, 0.45f });
    }

    pointData.clear();
    for (auto& p : particles)
    {
        if (! p.live) continue;
        p.life += dt;
        if (p.life >= p.maxLife) { p.live = false; continue; }
        const bool falls = p.kind == droplet || p.kind == sweat;
        if (falls)
        {
            p.vy -= 6.0f * dt;
            const bool inBowl = std::abs (p.x) < 0.8f && std::abs (p.z) < 1.0f;
            if (p.vy < 0 && (p.y < toilet::floorY || (inBowl && p.y < toilet::waterY))) { p.live = false; continue; }
        }
        if (p.kind == steam)
        {
            p.vx *= 1.0f - dt; p.vz *= 1.0f - dt;
            p.size += dt * 0.25f;
        }
        p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt;
        const float fade = std::sin (pi * p.life / p.maxLife);
        float alpha = fade;
        if (p.kind == mote || p.kind == holySpark) alpha *= 0.6f + 0.4f * std::sin (p.life * 20.0f + p.x * 50.0f);
        if (p.kind == droplet) alpha *= 0.85f;
        if (p.kind == steam) alpha *= 0.35f;
        const float sz = (p.kind == mote || p.kind == holySpark) ? p.size : -p.size;
        pointData.insert (pointData.end(), { p.x, p.y, p.z, sz, p.r, p.g, p.b, alpha });
    }
}

float BowlRenderer::holyAmountFromParams() const
{
    const float c = param (pid::crysAmount), s = param (pid::shimAmt);
    const float t = juce::jlimit (0.0f, 1.0f, (c * s * 2.0f - 0.25f) / 0.5f);
    return t * t * (3.0f - 2.0f * t);
}

void BowlRenderer::renderOpenGL()
{
    if (litProgram == nullptr || waterProgram == nullptr || pointProgram == nullptr || compositeProgram == nullptr)
    {
        juce::OpenGLHelpers::clear (juce::Colours::darkslategrey);
        return;
    }

    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (nowMs - lastMs) * 0.001);
    lastMs = nowMs;
    const float now = (float) ((nowMs - startMs) * 0.001);

    updateSimulation (dt, now);

    const float scale = (float) context.getRenderingScale();
    auto* comp = context.getTargetComponent();
    if (comp == nullptr) return;
    const int w = juce::roundToInt (scale * (float) comp->getWidth());
    const int h = juce::roundToInt (scale * (float) comp->getHeight());
    if (w <= 0 || h <= 0) return;

    if (msaaFbo.w != w || msaaFbo.h != h)
    {
        msaaFbo.create (w, h, 4, false);
        sceneFbo.create (w, h, 0, true);
    }

    const Mat4 proj = Mat4::perspective (fovY, (float) w / (float) h, 0.5f, 40.0f);
    // the camera pulls back while the old man is visiting so he fits in the frame
    const float cw = oldMan.cameraWeight();
    const float cwe = cw * cw * (3.0f - 2.0f * cw);
    const Vec3 camEye = eye + (eyeFar - eye) * cwe;
    const Vec3 camTarget = target + (targetFar - target) * cwe;
    const Mat4 view = Mat4::lookAt (camEye, camTarget, { 0.0f, 1.0f, 0.0f });
    const Mat4 viewProj = proj * view;

    // ---- gather parameters
    const float fb = param (pid::feedback);
    const float bristol = param (pid::bristol);
    const float clog = std::max (param (pid::clogDrive), param (pid::clogCrush) * 0.8f);
    const float crystals = param (pid::crysAmount);
    const float wobble = param (pid::wobbleDepth);
    const float plunge = param (pid::plungeDiffuse);
    const float flush = flushSmoothed;
    const float lumpy = juce::jlimit (0.0f, 1.0f, (4.0f - bristol) / 3.0f);
    const float runny = juce::jlimit (0.0f, 1.0f, (bristol - 4.0f) / 3.0f);

    const float waterY = toilet::waterY + overflow * 0.43f;
    const float waterR = toilet::innerRadiusAt (waterY) * 1.015f;

    // poop placement
    const float driftR = 0.04f + fb * 0.16f;
    const float flushPull = 1.0f - flush;
    poopXZ = { std::cos (driftPhase) * driftR * flushPull, std::sin (driftPhase * 0.8f) * driftR * 1.2f * flushPull };
    const float bob = std::sin (now * (1.4f + fb)) * (0.01f + fb * 0.025f);
    const float sink = (1.0f - duckSmoothed) * 0.12f + flush * flush * 0.7f;
    const float popIn = respawn < 1.0f ? (1.0f - std::pow (1.0f - respawn, 3.0f) * std::cos (respawn * 9.0f)) : 1.0f;
    const float poopScale = juce::jmax (0.02f, (1.0f - flush * flush) * popIn) * (1.0f + poopBonus);
    const Vec3 poopPos { poopXZ.x, waterY - 0.07f + bob - sink + jumpY, poopXZ.y };

    PoopShape shape;
    shape.time = now;
    shape.bristol = bristol;
    shape.clog = clog;
    shape.wobble = wobble;
    shape.crystals = crystals;
    shape.scale = 1.0f;
    buildPoop (shape, poopData);

    // ---- scene pass
    glBindFramebuffer (GL_FRAMEBUFFER, msaaFbo.fbo);
    glViewport (0, 0, w, h);
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDisable (GL_CULL_FACE);
    glDisable (GL_BLEND);
    glClearColor (0.02f, 0.03f, 0.04f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto& lp = *litProgram;
    lp.use();
    setMat (lp, "uViewProj", viewProj);
    set3f (lp, "uCamPos", camEye);
    set1f (lp, "uRedness", oldMan.redness());
    set1f (lp, "uStrain", oldMan.strainAmount());
    set3f (lp, "uManShadow", oldMan.shadow());
    set1f (lp, "uTime", now);
    set1f (lp, "uHoly", holySmoothed);
    set1f (lp, "uCrystals", crystals);
    set1f (lp, "uClog", clog);
    set1f (lp, "uRunny", runny);
    set1f (lp, "uLumpy", lumpy);
    set1f (lp, "uWaterY", waterY);
    set3f (lp, "uPoopPos", poopPos);

    auto drawLit = [&] (const GpuMesh& m, const Mat4& model, int material, Vec3 tint)
    {
        setMat (lp, "uModel", model);
        set1i (lp, "uMaterial", material);
        set3f (lp, "uTint", tint);
        m.draw();
    };

    drawLit (floorMesh, Mat4::translation ({ 0, toilet::floorY, 0 }), 2, { 1, 1, 1 });
    drawLit (tank, Mat4::translation ({ 0.0f, 0.05f, -1.72f }), 0, { 1, 1, 1 });
    drawLit (tankLid, Mat4::translation ({ 0.0f, 0.70f, -1.72f }), 0, { 0.98f, 0.98f, 1.0f });
    drawLit (bowl, Mat4::identity(), 0, { 1, 1, 1 });
    drawLit (seat, Mat4::identity(), 0, { 1.0f, 0.99f, 0.97f });
    {
        // lid raised, leaning back against the tank
        const Vec3 hinge { 0.0f, 0.14f, -1.14f };
        const Mat4 m = Mat4::translation (hinge) * Mat4::rotationX (-1.72f) * Mat4::translation ({ -hinge.x, -hinge.y, -hinge.z });
        drawLit (lid, m, 0, { 1.0f, 0.99f, 0.97f });
    }

    manParts.clear();
    oldMan.buildParts (manParts);
    for (const auto& part : manParts)
        drawLit (sphere, part.model, part.material, part.tint);

    if (poopScale > 0.021f)
    {
        poop.updateVertices (poopData);
        const Mat4 model = Mat4::translation (poopPos) * Mat4::rotationY (poopAngle)
                           * Mat4::scale ({ poopScale, poopScale * (1.0f - flush * 0.5f), poopScale });
        drawLit (poop, model, 1, { 1, 1, 1 });
    }

    // ---- water (blended)
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    auto& wp = *waterProgram;
    wp.use();
    setMat (wp, "uViewProj", viewProj);
    set3f (wp, "uCamPos", camEye);
    set1f (wp, "uRadius", waterR);
    set1f (wp, "uWaterY", waterY);
    set2f (wp, "uOval", toilet::ovalFront, toilet::ovalBack);
    set1f (wp, "uTime", now);
    set1f (wp, "uSwirl", swirlSmoothed * 3.0f + plunge * 0.3f);
    set1f (wp, "uVortex", flush);
    set1f (wp, "uRadialFade", 1.0f);
    set1f (wp, "uHoly", holySmoothed);
    set3f (wp, "uPoopPos", poopPos);
    set1f (wp, "uPuddle", 0.0f);
    {
        float rip[16 * 4];
        for (size_t i = 0; i < ripples.size(); ++i)
        {
            const float age = now - ripples[i].birth;
            const bool alive = age >= 0.0f && age < 3.0f;
            rip[i * 4 + 0] = ripples[i].x;
            rip[i * 4 + 1] = ripples[i].z;
            rip[i * 4 + 2] = age;
            rip[i * 4 + 3] = alive ? ripples[i].amp : 0.0f;
        }
        glUniform4fv (loc (wp, "uRipples"), 16, rip);
    }
    water.draw();

    if (puddle > 0.001f)
    {
        set1f (wp, "uRadius", 0.6f + puddle * 1.9f);
        set1f (wp, "uWaterY", toilet::floorY + 0.004f);
        set2f (wp, "uOval", 1.3f, 1.0f);
        set1f (wp, "uRadialFade", 0.3f);
        set1f (wp, "uVortex", 0.0f);
        set1f (wp, "uHoly", 0.0f);
        set1f (wp, "uPuddle", 1.0f);
        water.draw();
    }

    // ---- particles (additive)
    buildParticles (dt);
    if (! pointData.empty())
    {
        glBlendFunc (GL_SRC_ALPHA, GL_ONE);
        glDepthMask (GL_FALSE);
        glEnable (GL_PROGRAM_POINT_SIZE);
        auto& pp = *pointProgram;
        pp.use();
        setMat (pp, "uViewProj", viewProj);
        set1f (pp, "uPixelScale", (float) h * 1.6f);
        glBindVertexArray (pointVao);
        glBindBuffer (GL_ARRAY_BUFFER, pointVbo);
        glBufferSubData (GL_ARRAY_BUFFER, 0, (GLsizeiptr) (pointData.size() * sizeof (float)), pointData.data());
        glDrawArrays (GL_POINTS, 0, (GLsizei) (pointData.size() / 8));
        glDepthMask (GL_TRUE);
    }
    glDisable (GL_BLEND);

    // ---- resolve MSAA -> texture
    glBindFramebuffer (GL_READ_FRAMEBUFFER, msaaFbo.fbo);
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, sceneFbo.fbo);
    glBlitFramebuffer (0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // ---- composite into JUCE's framebuffer
    glBindFramebuffer (GL_FRAMEBUFFER, context.getFrameBufferID());
    glViewport (0, 0, w, h);
    glDisable (GL_DEPTH_TEST);
    auto& cp = *compositeProgram;
    cp.use();
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, sceneFbo.tex);
    set1i (cp, "uScene", 0);
    set2f (cp, "uResolution", (float) w, (float) h);
    const auto lightUv = projectToUv (viewProj, { 0.0f, waterY, 0.05f });
    const auto haloUv = projectToUv (viewProj, { poopPos.x, poopPos.y + 0.55f * poopScale + 0.08f, poopPos.z });
    set2f (cp, "uLightPos", lightUv.x, lightUv.y);
    set2f (cp, "uHaloPos", haloUv.x, haloUv.y);
    set1f (cp, "uHoly", holySmoothed);
    set1f (cp, "uTime", now);
    set1f (cp, "uFlush", flush);
    set1f (cp, "uPixelScale", scale);
    {
        float rects[8 * 4] {}, radii[8] {};
        int count = 0;
        {
            const juce::SpinLock::ScopedLockType sl (glassLock);
            for (auto& gr : glass)
            {
                if (count >= 8) break;
                const auto b = gr.bounds * scale;
                rects[count * 4 + 0] = b.getX();
                rects[count * 4 + 1] = (float) h - b.getBottom();
                rects[count * 4 + 2] = b.getRight();
                rects[count * 4 + 3] = (float) h - b.getY();
                radii[count] = gr.radius * scale;
                ++count;
            }
        }
        glUniform4fv (loc (cp, "uGlass"), 8, rects);
        glUniform1fv (loc (cp, "uGlassR"), 8, radii);
        set1i (cp, "uGlassCount", count);
    }
    glBindVertexArray (emptyVao);
    glDrawArrays (GL_TRIANGLES, 0, 3);
    glBindVertexArray (0);
    glBindTexture (GL_TEXTURE_2D, 0);

    ++frameCounter;
    if (snapshotAtSeconds > 0.0f && ! snapshotTaken && now >= snapshotAtSeconds)
    {
        snapshotTaken = true;
        captureSnapshot (w, h);
    }
}

void BowlRenderer::captureSnapshot (int w, int h)
{
    std::vector<uint8_t> px ((size_t) (w * h * 4));
    glPixelStorei (GL_PACK_ALIGNMENT, 1);
    glReadPixels (0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const auto* s = &px[(size_t) (((h - 1 - y) * w + x) * 4)];
            bd.setPixelColour (x, y, juce::Colour (s[0], s[1], s[2], (uint8_t) 255));
        }
    if (onSnapshot)
        onSnapshot (img);
}
}
