#pragma once
#include <cmath>
#include <array>

namespace layshit::gfx
{
struct Vec3
{
    float x = 0, y = 0, z = 0;
    Vec3 operator+ (Vec3 o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator- (Vec3 o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator* (float s) const { return { x * s, y * s, z * s }; }
    Vec3& operator+= (Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    float length() const { return std::sqrt (x * x + y * y + z * z); }
    Vec3 normalised() const { const float l = length(); return l > 1.0e-9f ? *this * (1.0f / l) : Vec3 { 0, 1, 0 }; }
};
inline float dot (Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross (Vec3 a, Vec3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }

/** Column-major 4x4 matrix (OpenGL layout). */
struct Mat4
{
    std::array<float, 16> m {};

    static Mat4 identity()
    {
        Mat4 r;
        r.m = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        return r;
    }
    float& at (int row, int col) { return m[(size_t) (col * 4 + row)]; }
    float at (int row, int col) const { return m[(size_t) (col * 4 + row)]; }

    Mat4 operator* (const Mat4& o) const
    {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr)
            {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += at (rr, k) * o.at (k, c);
                r.at (rr, c) = s;
            }
        return r;
    }

    std::array<float, 4> transform (float x, float y, float z, float w) const
    {
        std::array<float, 4> r {};
        for (int rr = 0; rr < 4; ++rr)
            r[(size_t) rr] = at (rr, 0) * x + at (rr, 1) * y + at (rr, 2) * z + at (rr, 3) * w;
        return r;
    }

    static Mat4 perspective (float fovYRadians, float aspect, float zNear, float zFar)
    {
        Mat4 r;
        const float f = 1.0f / std::tan (fovYRadians * 0.5f);
        r.at (0, 0) = f / aspect;
        r.at (1, 1) = f;
        r.at (2, 2) = (zFar + zNear) / (zNear - zFar);
        r.at (2, 3) = 2.0f * zFar * zNear / (zNear - zFar);
        r.at (3, 2) = -1.0f;
        return r;
    }

    static Mat4 lookAt (Vec3 eye, Vec3 target, Vec3 up)
    {
        const Vec3 f = (target - eye).normalised();
        const Vec3 s = cross (f, up).normalised();
        const Vec3 u = cross (s, f);
        Mat4 r = identity();
        r.at (0, 0) = s.x; r.at (0, 1) = s.y; r.at (0, 2) = s.z;
        r.at (1, 0) = u.x; r.at (1, 1) = u.y; r.at (1, 2) = u.z;
        r.at (2, 0) = -f.x; r.at (2, 1) = -f.y; r.at (2, 2) = -f.z;
        r.at (0, 3) = -dot (s, eye);
        r.at (1, 3) = -dot (u, eye);
        r.at (2, 3) = dot (f, eye);
        return r;
    }

    static Mat4 translation (Vec3 t)
    {
        Mat4 r = identity();
        r.at (0, 3) = t.x; r.at (1, 3) = t.y; r.at (2, 3) = t.z;
        return r;
    }
    static Mat4 scale (Vec3 s)
    {
        Mat4 r = identity();
        r.at (0, 0) = s.x; r.at (1, 1) = s.y; r.at (2, 2) = s.z;
        return r;
    }
    static Mat4 rotationY (float a)
    {
        Mat4 r = identity();
        r.at (0, 0) = std::cos (a); r.at (0, 2) = std::sin (a);
        r.at (2, 0) = -std::sin (a); r.at (2, 2) = std::cos (a);
        return r;
    }
    /** Columns are the transformed x/y/z axes (length = scale) plus translation. */
    static Mat4 fromBasis (Vec3 x, Vec3 y, Vec3 z, Vec3 t)
    {
        Mat4 r = identity();
        r.at (0, 0) = x.x; r.at (1, 0) = x.y; r.at (2, 0) = x.z;
        r.at (0, 1) = y.x; r.at (1, 1) = y.y; r.at (2, 1) = y.z;
        r.at (0, 2) = z.x; r.at (1, 2) = z.y; r.at (2, 2) = z.z;
        r.at (0, 3) = t.x; r.at (1, 3) = t.y; r.at (2, 3) = t.z;
        return r;
    }
    static Mat4 rotationZ (float a)
    {
        Mat4 r = identity();
        r.at (0, 0) = std::cos (a); r.at (0, 1) = -std::sin (a);
        r.at (1, 0) = std::sin (a); r.at (1, 1) = std::cos (a);
        return r;
    }
    static Mat4 rotationX (float a)
    {
        Mat4 r = identity();
        r.at (1, 1) = std::cos (a); r.at (1, 2) = -std::sin (a);
        r.at (2, 1) = std::sin (a); r.at (2, 2) = std::cos (a);
        return r;
    }
};
}
