#pragma once
#include "GfxMath.h"
#include <vector>
#include <cstdint>
#include <utility>

namespace layshit::gfx
{
/** Interleaved vertex: position(3) normal(3) uv(2). */
struct MeshData
{
    static constexpr int floatsPerVertex = 8;
    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    void clear() { vertices.clear(); indices.clear(); }
    int numVertices() const { return (int) vertices.size() / floatsPerVertex; }
    void addVertex (Vec3 p, Vec3 n, float u, float v)
    {
        vertices.insert (vertices.end(), { p.x, p.y, p.z, n.x, n.y, n.z, u, v });
    }
    Vec3 position (int i) const { auto* f = &vertices[(size_t) (i * floatsPerVertex)]; return { f[0], f[1], f[2] }; }

    /** Adds quads for a (rows x cols) vertex grid starting at vertex `base`. */
    void addGridIndices (int base, int rows, int cols);
    /** Recomputes smooth normals from triangle faces. */
    void computeNormals();
};

/** Toilet dimensions shared by the renderer and the UI layout. */
namespace toilet
{
    inline constexpr float floorY = -1.6f;
    inline constexpr float waterY = -0.45f;
    inline constexpr float ovalFront = 1.25f; // z scale toward the front (+z)
    inline constexpr float ovalBack = 1.08f;  // z scale toward the tank (-z)

    /** Inner bowl radius (before oval scaling) at height y. */
    float innerRadiusAt (float y);
}

MeshData makeLathe (const std::vector<std::pair<float, float>>& profile, int segments, float zFront, float zBack);
MeshData makeBowl();
MeshData makeSeat();
MeshData makeLid();
MeshData makeRoundedBox (Vec3 halfExtents, float exponent, int rings, int segments);
MeshData makeDisc (int rings, int segments);
MeshData makePlane (float halfSize);
/** Unit UV sphere (radius 1); normals equal positions. */
MeshData makeSphere (int rings, int segments);

/** Everything that shapes the poop this frame. */
struct PoopShape
{
    float time = 0;
    float bristol = 4;   // 1 lumpy .. 7 liquid
    float clog = 0;      // surface roughness
    float wobble = 0;    // jelly
    float crystals = 0;  // faceting is done in the shader; this adds spiky growth
    float scale = 1;
};

/** Builds the coil around the origin (base sits at y = 0) into `out` (reusing its storage). */
void buildPoop (const PoopShape& shape, MeshData& out);
}
