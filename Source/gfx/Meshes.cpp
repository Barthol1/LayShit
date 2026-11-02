#include "Meshes.h"
#include <algorithm>

namespace layshit::gfx
{
namespace
{
constexpr float pi = 3.14159265f;

// Bowl profile (radius, height) from the floor, up the outside, over the rim, down into the trap.
const std::vector<std::pair<float, float>>& bowlProfile()
{
    static const std::vector<std::pair<float, float>> p {
        { 0.55f, -1.60f }, { 0.56f, -1.35f }, { 0.62f, -1.05f }, { 0.74f, -0.70f }, { 0.86f, -0.38f },
        { 0.95f, -0.15f }, { 1.00f, -0.04f }, { 1.01f, 0.00f }, { 0.99f, 0.035f }, { 0.94f, 0.05f },
        { 0.88f, 0.045f }, { 0.84f, 0.02f }, { 0.815f, -0.03f }, { 0.78f, -0.12f }, { 0.72f, -0.24f },
        { 0.64f, -0.35f }, { 0.56f, -0.45f }, { 0.47f, -0.55f }, { 0.37f, -0.66f }, { 0.27f, -0.78f },
        { 0.19f, -0.90f }, { 0.13f, -1.02f }, { 0.08f, -1.10f }, { 0.0f, -1.12f }
    };
    return p;
}

float smoothstep01 (float x)
{
    x = std::clamp (x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

float hashNoise (float x, float y, float z)
{
    // smooth-ish value noise from summed sines; cheap and deterministic
    return (std::sin (x * 12.9898f + y * 78.233f + z * 37.719f) * 0.5f
            + std::sin (x * 5.1f - z * 7.3f + y * 3.7f) * 0.3f
            + std::sin (y * 9.7f + z * 4.3f - x * 2.9f) * 0.2f);
}
}

float toilet::innerRadiusAt (float y)
{
    const auto& p = bowlProfile();
    // inner section starts after the rim lip (index 11)
    for (size_t i = 11; i + 1 < p.size(); ++i)
    {
        const auto [r0, y0] = p[i];
        const auto [r1, y1] = p[i + 1];
        if (y <= y0 && y >= y1)
        {
            const float t = (y0 - y) / std::max (1.0e-6f, y0 - y1);
            return r0 + (r1 - r0) * t;
        }
    }
    return y > 0.0f ? 0.84f : 0.0f;
}

void MeshData::addGridIndices (int base, int rows, int cols)
{
    for (int r = 0; r + 1 < rows; ++r)
        for (int c = 0; c + 1 < cols; ++c)
        {
            const uint32_t a = (uint32_t) (base + r * cols + c), b = a + 1;
            const uint32_t d = (uint32_t) (base + (r + 1) * cols + c), e = d + 1;
            indices.insert (indices.end(), { a, d, b, b, d, e });
        }
}

void MeshData::computeNormals()
{
    const int n = numVertices();
    std::vector<Vec3> acc ((size_t) n);
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        const auto ia = (int) indices[i], ib = (int) indices[i + 1], ic = (int) indices[i + 2];
        const Vec3 fn = cross (position (ib) - position (ia), position (ic) - position (ia));
        acc[(size_t) ia] += fn;
        acc[(size_t) ib] += fn;
        acc[(size_t) ic] += fn;
    }
    for (int i = 0; i < n; ++i)
    {
        const Vec3 nn = acc[(size_t) i].normalised();
        auto* f = &vertices[(size_t) (i * floatsPerVertex)];
        f[3] = nn.x; f[4] = nn.y; f[5] = nn.z;
    }
}

MeshData makeLathe (const std::vector<std::pair<float, float>>& profile, int segments, float zFront, float zBack)
{
    MeshData m;
    const int rows = (int) profile.size(), cols = segments + 1;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
        {
            const float a = 2.0f * pi * (float) c / (float) segments;
            const float ca = std::cos (a), sa = std::sin (a);
            const float zs = sa > 0.0f ? zFront : zBack;
            const auto [rad, y] = profile[(size_t) r];
            m.addVertex ({ rad * ca, y, rad * sa * zs }, { 0, 1, 0 }, (float) c / (float) segments, (float) r / (float) (rows - 1));
        }
    m.addGridIndices (0, rows, cols);
    m.computeNormals();

    // the seam column duplicates the first one: share their normals so there is no crease
    for (int r = 0; r < rows; ++r)
    {
        auto* first = &m.vertices[(size_t) ((r * cols) * MeshData::floatsPerVertex)];
        auto* last = &m.vertices[(size_t) ((r * cols + segments) * MeshData::floatsPerVertex)];
        for (int k = 3; k < 6; ++k)
        {
            const float avg = 0.5f * (first[k] + last[k]);
            first[k] = last[k] = avg;
        }
    }
    return m;
}

MeshData makeBowl() { return makeLathe (bowlProfile(), 128, toilet::ovalFront, toilet::ovalBack); }

MeshData makeSeat()
{
    // closed rounded-rectangle cross-section swept around the rim
    std::vector<std::pair<float, float>> p;
    const float rIn = 0.80f, rOut = 1.04f, y0 = 0.05f, y1 = 0.13f, rr = 0.035f;
    auto corner = [&] (float cx, float cy, float a0)
    {
        for (int i = 0; i <= 4; ++i)
        {
            const float a = a0 + (pi * 0.5f) * (float) i / 4.0f;
            p.emplace_back (cx + rr * std::cos (a), cy + rr * std::sin (a));
        }
    };
    corner (rOut - rr, y1 - rr, 0.0f);          // top-outer
    corner (rIn + rr, y1 - rr, pi * 0.5f);      // top-inner
    corner (rIn + rr, y0 + rr, pi);             // bottom-inner
    corner (rOut - rr, y0 + rr, pi * 1.5f);     // bottom-outer
    p.push_back (p.front());
    return makeLathe (p, 128, toilet::ovalFront, toilet::ovalBack);
}

MeshData makeLid()
{
    std::vector<std::pair<float, float>> p { { 0.0f, 0.0f }, { 0.96f, 0.0f }, { 1.03f, 0.025f }, { 1.04f, 0.05f },
                                             { 1.0f, 0.075f }, { 0.9f, 0.085f }, { 0.0f, 0.09f } };
    return makeLathe (p, 96, toilet::ovalFront, toilet::ovalBack);
}

MeshData makeRoundedBox (Vec3 he, float e, int rings, int segments)
{
    MeshData m;
    auto spow = [] (float v, float p) { return (v < 0 ? -1.0f : 1.0f) * std::pow (std::abs (v), p); };
    for (int r = 0; r <= rings; ++r)
    {
        const float phi = -pi * 0.5f + pi * (float) r / (float) rings;
        for (int c = 0; c <= segments; ++c)
        {
            const float th = -pi + 2.0f * pi * (float) c / (float) segments;
            const float cp = spow (std::cos (phi), e), sp = spow (std::sin (phi), e);
            const float ct = spow (std::cos (th), e), st = spow (std::sin (th), e);
            m.addVertex ({ he.x * cp * ct, he.y * sp, he.z * cp * st }, { 0, 1, 0 }, (float) c / (float) segments, (float) r / (float) rings);
        }
    }
    m.addGridIndices (0, rings + 1, segments + 1);
    m.computeNormals();
    return m;
}

MeshData makeDisc (int rings, int segments)
{
    MeshData m;
    m.addVertex ({ 0, 0, 0 }, { 0, 1, 0 }, 0.5f, 0.5f);
    for (int r = 1; r <= rings; ++r)
        for (int c = 0; c < segments; ++c)
        {
            const float rad = (float) r / (float) rings;
            const float a = 2.0f * pi * (float) c / (float) segments;
            m.addVertex ({ rad * std::cos (a), 0, rad * std::sin (a) }, { 0, 1, 0 }, rad, (float) c / (float) segments);
        }
    for (int c = 0; c < segments; ++c)
        m.indices.insert (m.indices.end(), { 0u, (uint32_t) (1 + (c + 1) % segments), (uint32_t) (1 + c) });
    for (int r = 1; r < rings; ++r)
        for (int c = 0; c < segments; ++c)
        {
            const uint32_t a = (uint32_t) (1 + (r - 1) * segments + c);
            const uint32_t b = (uint32_t) (1 + (r - 1) * segments + (c + 1) % segments);
            const uint32_t d = (uint32_t) (1 + r * segments + c);
            const uint32_t e = (uint32_t) (1 + r * segments + (c + 1) % segments);
            m.indices.insert (m.indices.end(), { a, b, d, b, e, d });
        }
    return m;
}

MeshData makePlane (float s)
{
    MeshData m;
    m.addVertex ({ -s, 0, -s }, { 0, 1, 0 }, 0, 0);
    m.addVertex ({ s, 0, -s }, { 0, 1, 0 }, 1, 0);
    m.addVertex ({ -s, 0, s }, { 0, 1, 0 }, 0, 1);
    m.addVertex ({ s, 0, s }, { 0, 1, 0 }, 1, 1);
    m.indices = { 0, 2, 1, 1, 2, 3 };
    return m;
}

MeshData makeSphere (int rings, int segments)
{
    MeshData m;
    for (int r = 0; r <= rings; ++r)
    {
        const float phi = pi * (float) r / (float) rings; // 0 = top
        for (int c = 0; c <= segments; ++c)
        {
            const float th = 2.0f * pi * (float) c / (float) segments;
            const Vec3 p { std::sin (phi) * std::cos (th), std::cos (phi), std::sin (phi) * std::sin (th) };
            m.addVertex (p, p, (float) c / (float) segments, (float) r / (float) rings);
        }
    }
    m.addGridIndices (0, rings + 1, segments + 1);
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3)
        std::swap (m.indices[i + 1], m.indices[i + 2]); // wind outward to match the normals
    return m;
}

void buildPoop (const PoopShape& s, MeshData& out)
{
    constexpr int along = 180, around = 28;
    out.vertices.clear();
    if (out.indices.empty())
        out.addGridIndices (0, along, around + 1);
    out.vertices.reserve ((size_t) (along * (around + 1) * MeshData::floatsPerVertex));

    const float lumpy = std::clamp ((4.0f - s.bristol) / 3.0f, 0.0f, 1.0f);
    const float runny = std::clamp ((s.bristol - 4.0f) / 3.0f, 0.0f, 1.0f);
    const float turns = 2.6f;

    auto centre = [&] (float t)
    {
        // soft-serve coil: wide base, each turn stacked on the last, tip rising to a point on the axis
        const float a = t * turns * 2.0f * pi;
        const float R = 0.30f * std::pow (1.0f - t, 1.25f) * (1.0f + runny * 0.45f);
        float y = 0.30f * t + 0.16f * smoothstep01 ((t - 0.72f) / 0.28f);
        y *= 1.0f - runny * 0.75f;
        return Vec3 { R * std::cos (a), y, R * std::sin (a) };
    };

    // parallel-transport frame along the path
    Vec3 prevT = (centre (0.002f) - centre (0.0f)).normalised();
    Vec3 nrm = cross (prevT, { 0, 1, 0 }).normalised();

    for (int i = 0; i < along; ++i)
    {
        const float t = (float) i / (float) (along - 1);
        const Vec3 c = centre (t);
        const Vec3 tan = (centre (std::min (1.0f, t + 0.004f)) - centre (std::max (0.0f, t - 0.004f))).normalised();
        // transport the normal
        const Vec3 axis = cross (prevT, tan);
        if (axis.length() > 1.0e-6f)
            nrm = (nrm - tan * dot (nrm, tan)).normalised();
        prevT = tan;
        const Vec3 bin = cross (tan, nrm).normalised();

        // tube radius: soft round start, pointy curled tip
        float rho = 0.12f * std::sqrt (std::min (1.0f, t * 18.0f)) * (1.0f - 0.55f * t);
        if (t > 0.8f) rho *= std::pow (std::max (0.0f, (1.0f - t) / 0.2f), 0.75f); // pointy tip

        // Bristol lumps: pinch the tube into nuggets
        const float lumpWave = std::pow (std::abs (std::sin (pi * t * 8.0f)), 0.55f);
        rho *= 1.0f + lumpy * (0.18f + 0.82f * lumpWave - 1.0f);

        for (int j = 0; j <= around; ++j)
        {
            const float a = 2.0f * pi * (float) j / (float) around;
            Vec3 dir = nrm * std::cos (a) + bin * std::sin (a);
            float r = rho;

            // segment creases, clog crust, jelly wobble
            r *= 1.0f + 0.05f * std::sin (t * 60.0f) * (1.0f - runny);
            r *= 1.0f + s.clog * 0.22f * hashNoise (c.x * 9.0f + dir.x * 3.0f, c.y * 9.0f + dir.y * 3.0f, c.z * 9.0f + dir.z * 3.0f);
            r *= 1.0f + s.wobble * 0.12f * std::sin (s.time * 7.0f + t * 14.0f + a * 2.0f);
            // crystal spikes
            if (s.crystals > 0.0f)
            {
                const float spike = std::max (0.0f, std::sin (a * 6.0f + t * 90.0f) * std::sin (t * 40.0f + a * 3.0f));
                r *= 1.0f + s.crystals * 0.45f * std::pow (spike, 4.0f);
            }

            Vec3 p = c + dir * r;
            if (runny > 0.0f)
            {
                // squash vertically, spread outward
                p.y = c.y + (p.y - c.y) * (1.0f - runny * 0.55f);
                p.x = c.x + (p.x - c.x) * (1.0f + runny * 0.35f);
                p.z = c.z + (p.z - c.z) * (1.0f + runny * 0.35f);
            }
            out.addVertex (p * s.scale, dir, t, (float) j / (float) around);
        }
    }
    out.computeNormals();
}
}
