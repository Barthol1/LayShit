#pragma once

namespace layshit::gfx::shaders
{
// ------------------------------------------------------------------ lit objects
inline constexpr const char* litVertex = R"(#version 150
in vec3 aPos;
in vec3 aNormal;
in vec2 aUv;
uniform mat4 uModel;
uniform mat4 uViewProj;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUv;
out vec3 vLocal;
void main()
{
    vec4 w = uModel * vec4(aPos, 1.0);
    vWorld = w.xyz;
    vLocal = aPos;
    vNormal = normalize(transpose(inverse(mat3(uModel))) * aNormal);
    vUv = aUv;
    gl_Position = uViewProj * w;
}
)";

inline constexpr const char* litFragment = R"(#version 150
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUv;
in vec3 vLocal;
out vec4 fragColor;

uniform int uMaterial;      // 0 porcelain, 1 poop, 2 floor, 3 skin, 4 cloth, 5 face
uniform float uRedness;
uniform float uStrain;
uniform vec3 uManShadow;    // x, z, strength
uniform vec3 uTint;
uniform vec3 uCamPos;
uniform float uTime;
uniform float uHoly;
uniform float uCrystals;
uniform float uClog;
uniform float uRunny;
uniform float uLumpy;
uniform float uWaterY;
uniform vec3 uPoopPos;

const vec3 lightDir = normalize(vec3(-0.45, 1.0, 0.35));
const vec3 gold = vec3(1.0, 0.78, 0.32);

float hash(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }

vec3 sky(vec3 r)
{
    // bathroom ceiling: soft panel light overhead, warm walls around
    float up = clamp(r.y, 0.0, 1.0);
    vec3 c = mix(vec3(0.33, 0.30, 0.27), vec3(0.85, 0.88, 0.92), pow(up, 0.6));
    float panel = smoothstep(0.92, 0.985, dot(r, normalize(vec3(-0.3, 1.0, 0.2))));
    return c + panel * vec3(1.6);
}

void main()
{
    vec3 n = normalize(vNormal);
    if (! gl_FrontFacing) n = -n;
    vec3 v = normalize(uCamPos - vWorld);

    if (uMaterial == 1 && uCrystals > 0.001)
    {
        vec3 fn = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
        if (dot(fn, n) < 0.0) fn = -fn;
        n = normalize(mix(n, fn, clamp(uCrystals * 1.3, 0.0, 1.0)));
    }

    float ndl = max(dot(n, lightDir), 0.0);
    float wrap = max((dot(n, lightDir) + 0.35) / 1.35, 0.0);
    vec3 h = normalize(lightDir + v);
    float fres = pow(1.0 - max(dot(n, v), 0.0), 5.0);
    vec3 refl = reflect(-v, n);
    vec3 hemi = mix(vec3(0.18, 0.16, 0.15), vec3(0.55, 0.58, 0.62), n.y * 0.5 + 0.5);

    vec3 col;
    if (uMaterial == 0)
    {
        vec3 base = vec3(0.94, 0.94, 0.92) * uTint;
        float rad = length(vec2(vWorld.x, vWorld.z * 0.85));
        float inside = 1.0 - smoothstep(0.78, 0.86, rad);
        float ao = mix(1.0, mix(0.42, 1.0, smoothstep(-1.05, 0.05, vWorld.y)), inside * step(vWorld.y, 0.06));
        ao *= mix(0.55, 1.0, smoothstep(-1.6, -1.1, vWorld.y)); // darker near the floor
        col = base * (hemi * 0.75 + wrap * vec3(1.0, 0.97, 0.92) * 0.85) * ao;
        col += pow(max(dot(n, h), 0.0), 160.0) * 0.9;
        col += sky(refl) * mix(0.06, 0.55, fres) * ao;
        // holy light spilling up from the water
        float below = 1.0 - smoothstep(uWaterY - 0.1, 0.1, vWorld.y);
        col = mix(col, col * vec3(1.0, 0.88, 0.62), uHoly * inside * 0.55);
        col += gold * uHoly * inside * (0.04 + 0.22 * below);
    }
    else if (uMaterial == 1)
    {
        float crease = 0.5 + 0.5 * sin(vUv.x * 60.0);
        vec3 brown = mix(vec3(0.30, 0.16, 0.07), vec3(0.42, 0.24, 0.10), crease);
        brown = mix(brown, vec3(0.20, 0.11, 0.05), uClog * 0.7);
        brown = mix(brown, vec3(0.55, 0.38, 0.14), uRunny * 0.6);
        brown *= 0.85 + 0.3 * hash(floor(vWorld * 60.0)) * uClog;
        float shine = mix(40.0, 10.0, uClog);
        col = brown * (hemi * 0.6 + wrap * vec3(1.0, 0.95, 0.88));
        col += pow(max(dot(n, h), 0.0), shine) * mix(0.35, 0.08, uClog);
        col += sky(refl) * fres * 0.25;

        if (uCrystals > 0.001)
        {
            vec3 irid = 0.5 + 0.5 * cos(6.2831 * (dot(n, v) * 1.5 + vec3(0.0, 0.33, 0.67) + uTime * 0.1));
            col = mix(col, col * 0.4 + irid * 0.55 * (0.4 + ndl), uCrystals * 0.7);
            float s = hash(floor(vWorld * 90.0) + floor(uTime * 6.0));
            col += step(1.0 - 0.03 * uCrystals, s) * vec3(2.5) * uCrystals;
        }
        // holy: gilded and glowing
        col = mix(col, gold * (0.6 + 0.8 * ndl) + pow(max(dot(n, h), 0.0), 60.0) * 1.5, uHoly * 0.85);
        col += gold * uHoly * 0.12;
    }
    else if (uMaterial == 2)
    {
        // modern large-format charcoal tiles with hairline grout and a soft sheen
        vec2 tp = vWorld.xz / vec2(1.6, 0.8);
        tp.x += 0.5 * mod(floor(tp.y), 2.0);
        vec2 cell = floor(tp);
        vec2 f = fract(tp);
        vec2 gw = vec2(0.006, 0.012);
        float grout = smoothstep(0.0, gw.x, f.x) * smoothstep(0.0, gw.y, f.y) * (1.0 - smoothstep(1.0 - gw.x, 1.0, f.x)) * (1.0 - smoothstep(1.0 - gw.y, 1.0, f.y));
        float var = hash(vec3(cell, 1.0));
        float speck = hash(floor(vWorld * 140.0)) * 0.035;
        vec3 tile = vec3(0.075, 0.08, 0.088) * (0.9 + 0.2 * var) + speck;
        vec3 base = mix(vec3(0.02), tile, grout);
        // contact shadow under the toilet and the tank
        float d = length(vec2(vWorld.x, vWorld.z * 0.8 - 0.05));
        float shadow = mix(0.3, 1.0, smoothstep(0.45, 1.3, d));
        float dt = length(max(abs(vWorld.xz - vec2(0.0, -1.55)) - vec2(1.0, 0.25), 0.0));
        shadow *= mix(0.4, 1.0, smoothstep(0.0, 0.5, dt));
        float ms = length(vWorld.xz - uManShadow.xy);
        shadow *= 1.0 - uManShadow.z * 0.55 * (1.0 - smoothstep(0.3, 1.5, ms));
        col = base * (hemi * 1.1 + ndl * 0.6) * shadow;
        col += sky(refl) * 0.07 * grout * shadow;
        col += pow(max(dot(n, h), 0.0), 30.0) * 0.06 * shadow;
        // holy light reaches the floor
        col += gold * uHoly * 0.25 * exp(-d * 1.2);
    }
    else if (uMaterial == 4)
    {
        // cloth / hair: matte with soft sheen
        col = uTint * (hemi * 0.75 + wrap * vec3(1.0, 0.96, 0.9) * 0.8);
        col += sky(refl) * fres * 0.12;
    }
    else
    {
        // skin (3) and face (5): old, red, sweaty
        vec3 skinCol = vec3(0.93, 0.71, 0.58) * uTint;
        skinCol = mix(skinCol, vec3(0.93, 0.36, 0.30), uRedness * (uMaterial == 5 ? 0.7 : 0.35));
        float sweat = 0.35 + 0.65 * uRedness;
        vec3 featureCol = vec3(-1.0);

        if (uMaterial == 5)
        {
            vec3 p = normalize(vLocal);
            // red cheeks
            for (int i = 0; i < 2; ++i)
            {
                float s = i == 0 ? 1.0 : -1.0;
                vec2 q = p.xy - vec2(s * 0.5, -0.24);
                skinCol = mix(skinCol, vec3(0.88, 0.24, 0.22), exp(-dot(q, q) / 0.018) * (0.35 + 0.5 * uRedness) * step(0.0, p.z));
            }
            if (p.z > 0.25)
            {
                float ink = 0.0;
                // eyes squeezed shut: chevrons pointing at the nose, plus crow's feet
                for (int i = 0; i < 2; ++i)
                {
                    float s = i == 0 ? 1.0 : -1.0;
                    vec2 q = p.xy - vec2(s * 0.34, 0.17);
                    q.x *= s; // q.x < 0 is toward the nose
                    vec2 tip = vec2(-0.09, 0.0);
                    float h1 = clamp(dot(q - tip, vec2(0.19, 0.07)) / dot(vec2(0.19, 0.07), vec2(0.19, 0.07)), 0.0, 1.0);
                    float d1 = length(q - tip - vec2(0.19, 0.07) * h1);
                    float h2 = clamp(dot(q - tip, vec2(0.19, -0.07)) / dot(vec2(0.19, -0.07), vec2(0.19, -0.07)), 0.0, 1.0);
                    float d2 = length(q - tip - vec2(0.19, -0.07) * h2);
                    ink = max(ink, 1.0 - smoothstep(0.012, 0.024, min(d1, d2)));
                    for (int k = -1; k <= 1; ++k)
                    {
                        vec2 a = vec2(0.13, float(k) * 0.035);
                        vec2 dir = vec2(0.07, float(k) * 0.035);
                        float hh = clamp(dot(q - a, dir) / dot(dir, dir), 0.0, 1.0);
                        ink = max(ink, (1.0 - smoothstep(0.006, 0.014, length(q - a - dir * hh))) * 0.6);
                    }
                }
                // forehead wrinkles deepen while pushing
                for (int k = 0; k < 3; ++k)
                {
                    float y0 = 0.47 + 0.09 * float(k);
                    float d = abs(p.y - y0 - 0.02 * sin(p.x * 14.0 + float(k)));
                    ink = max(ink, (1.0 - smoothstep(0.006, 0.014, d)) * step(abs(p.x), 0.36 - 0.05 * float(k)) * (0.25 + 0.6 * uStrain));
                }
                // gritted-teeth grimace
                vec2 m = p.xy - vec2(0.0, -0.5);
                vec2 mh = vec2(0.24, 0.075 + 0.02 * uStrain);
                vec2 qd = abs(m) - mh + 0.04;
                float box = length(max(qd, 0.0)) + min(max(qd.x, qd.y), 0.0) - 0.04;
                if (box < 0.0)
                {
                    float gap = 1.0 - smoothstep(0.004, 0.009, abs(m.y));
                    float teeth = 1.0 - smoothstep(0.004, 0.009, abs(mod(m.x + 0.035, 0.07) - 0.035));
                    featureCol = mix(vec3(0.97, 0.95, 0.86), vec3(0.35, 0.2, 0.15), max(gap, teeth) * 0.8);
                }
                ink = max(ink, 1.0 - smoothstep(0.006, 0.016, abs(box)));
                skinCol = mix(skinCol, vec3(0.22, 0.1, 0.07), ink);
                // sweat beads
                vec3 c = floor(p * 16.0);
                float bead = step(0.82 - 0.25 * uRedness, hash(c)) * (1.0 - smoothstep(0.008, 0.013, length(fract(p * 16.0) - 0.5) / 16.0));
                sweat += bead * 3.0;
            }
            else if (p.y > 0.55)
            {
                // shiny bald top, beaded with sweat
                vec3 c = floor(p * 14.0);
                sweat += step(0.7 - 0.3 * uRedness, hash(c)) * (1.0 - smoothstep(0.01, 0.016, length(fract(p * 14.0) - 0.5) / 14.0)) * 3.0;
            }
        }

        vec3 albedo = featureCol.x >= 0.0 ? featureCol : skinCol;
        col = albedo * (hemi * 0.7 + wrap * vec3(1.0, 0.95, 0.9) * 0.85);
        col += pow(max(dot(n, h), 0.0), 70.0) * 0.55 * sweat;
        col += sky(refl) * fres * 0.25 * sweat;
    }

    fragColor = vec4(col, 1.0);
}
)";

// ------------------------------------------------------------------ water
inline constexpr const char* waterVertex = R"(#version 150
in vec3 aPos;
in vec3 aNormal;
in vec2 aUv;
uniform mat4 uViewProj;
uniform float uRadius;
uniform float uWaterY;
uniform vec2 uOval;          // front, back z-scale
uniform vec4 uRipples[16];   // x, z, age, amplitude
uniform float uTime;
uniform float uSwirl;        // signed swirl speed
uniform float uVortex;       // flush depth 0..1
uniform float uRadialFade;
out vec3 vWorld;
out vec3 vNormal;
out float vRad;

float heightAt(vec2 p)
{
    float h = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        vec4 r = uRipples[i];
        if (r.w <= 0.0) continue;
        float d = length(p - r.xy);
        float front = r.z * 0.9;
        float env = exp(-r.z * 1.6) * (1.0 - smoothstep(front - 0.25, front + 0.02, d)) * exp(-d * 1.5);
        h += r.w * env * sin(d * 34.0 - r.z * 30.0);
    }
    float ang = atan(p.y, p.x);
    float rad = length(p);
    h += 0.006 * abs(uSwirl) * sin(ang * 5.0 + rad * 14.0 - uTime * uSwirl * 2.0);
    h += 0.004 * sin(p.x * 7.0 + uTime * 0.8) * sin(p.y * 6.0 - uTime * 0.6);
    h -= uVortex * 0.55 * exp(-rad * rad * 9.0);
    h += uVortex * 0.03 * sin(ang * 3.0 + rad * 25.0 - uTime * 18.0);
    return h * uRadialFade;
}

void main()
{
    vec2 p = aPos.xz * uRadius;
    p.y *= (aPos.z > 0.0 ? uOval.x : uOval.y);
    float e = 0.01;
    float h = heightAt(p);
    float hx = heightAt(p + vec2(e, 0.0)) - h;
    float hz = heightAt(p + vec2(0.0, e)) - h;
    vNormal = normalize(vec3(-hx, e, -hz));
    vWorld = vec3(p.x, uWaterY + h, p.y);
    vRad = length(aPos.xz);
    gl_Position = uViewProj * vec4(vWorld, 1.0);
}
)";

inline constexpr const char* waterFragment = R"(#version 150
in vec3 vWorld;
in vec3 vNormal;
in float vRad;
out vec4 fragColor;
uniform vec3 uCamPos;
uniform float uHoly;
uniform float uTime;
uniform float uVortex;
uniform vec3 uPoopPos;
uniform float uPuddle;

const vec3 lightDir = normalize(vec3(-0.45, 1.0, 0.35));
const vec3 gold = vec3(1.0, 0.8, 0.35);

void main()
{
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCamPos - vWorld);
    float fres = 0.04 + 0.96 * pow(1.0 - max(dot(n, v), 0.0), 5.0);
    vec3 refl = reflect(-v, n);
    float up = clamp(refl.y, 0.0, 1.0);
    vec3 sky = mix(vec3(0.3, 0.28, 0.26), vec3(0.85, 0.9, 0.95), pow(up, 0.6));
    sky += smoothstep(0.93, 0.99, dot(refl, normalize(vec3(-0.3, 1.0, 0.2)))) * 2.0;

    // what we see through the water: porcelain getting deeper toward the trap
    // refraction: ripple slopes bend the view of the bowl underneath
    float rr = clamp(vRad + (n.x + n.z) * 0.9, 0.0, 1.2);
    vec3 under = mix(vec3(0.62, 0.80, 0.80), vec3(0.08, 0.16, 0.18), smoothstep(0.15, 0.85, 1.0 - rr));
    under = mix(under, vec3(0.02, 0.04, 0.05), smoothstep(0.75, 1.0, 1.0 - rr));
    // soft shadow of the poop
    float sd = length(vWorld.xz - uPoopPos.xz - vec2(0.06, -0.04));
    under *= mix(0.45, 1.0, smoothstep(0.08, 0.36, sd));

    vec3 h = normalize(lightDir + v);
    float spec = pow(max(dot(n, h), 0.0), 220.0) * 2.0 + pow(max(dot(n, h), 0.0), 40.0) * 0.25;
    float slopeLight = (dot(n, lightDir) - lightDir.y) * 3.0;

    vec3 col = mix(under, sky, fres) * (1.0 + slopeLight) + spec;
    float alpha = mix(0.62, 0.95, fres);

    // flush foam streaks
    float ang = atan(vWorld.z, vWorld.x);
    col += uVortex * 0.6 * smoothstep(0.6, 1.0, sin(ang * 4.0 + vRad * 20.0 - uTime * 14.0));

    // holy glow from the depths
    float core = exp(-vRad * vRad * 5.0);
    float caust = pow(0.5 + 0.5 * sin(vWorld.x * 40.0 + uTime * 2.0) * sin(vWorld.z * 37.0 - uTime * 1.7), 6.0);
    col += gold * uHoly * (0.3 * core + 0.1 + 0.35 * caust);
    alpha = mix(alpha, 0.9, uHoly * 0.4);

    if (uPuddle > 0.5)
    {
        col = sky * fres * 1.2 + spec + vec3(0.05, 0.12, 0.14) * 0.6;
        alpha = (0.35 + 0.5 * fres) * (1.0 - smoothstep(0.85, 1.0, vRad));
    }
    fragColor = vec4(col, alpha);
}
)";

// ------------------------------------------------------------------ particles
inline constexpr const char* pointVertex = R"(#version 150
in vec4 aPosSize;
in vec4 aColor;
uniform mat4 uViewProj;
uniform float uPixelScale;
out vec4 vColor;
out float vStar;
void main()
{
    gl_Position = uViewProj * vec4(aPosSize.xyz, 1.0);
    gl_PointSize = abs(aPosSize.w) * uPixelScale / max(gl_Position.w, 0.1);
    vStar = aPosSize.w > 0.0 ? 1.0 : 0.0; // negative size = soft round drop, no star
    vColor = aColor;
}
)";

inline constexpr const char* pointFragment = R"(#version 150
in vec4 vColor;
in float vStar;
out vec4 fragColor;
void main()
{
    vec2 d = gl_PointCoord * 2.0 - 1.0;
    float r = dot(d, d);
    if (r > 1.0) discard;
    float a = exp(-r * 3.5);
    float star = max(0.0, 1.0 - abs(d.x * d.y) * 30.0) * (1.0 - r) * vStar;
    fragColor = vec4(vColor.rgb * (a + star * 0.8), vColor.a * (a + star));
}
)";

// ------------------------------------------------------------------ composite (god rays, halo, vignette)
inline constexpr const char* compositeVertex = R"(#version 150
out vec2 vUv;
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

inline constexpr const char* compositeFragment = R"(#version 150
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uScene;
uniform vec2 uResolution;
uniform vec2 uLightPos;   // bowl centre, uv
uniform vec2 uHaloPos;    // above the poop, uv
uniform float uHoly;
uniform float uTime;
uniform float uFlush;
uniform vec4 uGlass[8];     // pixel rects x0, y0, x1, y1 (GL origin bottom-left)
uniform float uGlassR[8];   // corner radius in pixels
uniform int uGlassCount;
uniform float uPixelScale;

float glassMask(vec2 px)
{
    float m = 0.0;
    for (int i = 0; i < 8; ++i)
    {
        if (i >= uGlassCount) break;
        vec4 r = uGlass[i];
        vec2 c = (r.xy + r.zw) * 0.5;
        vec2 hs = (r.zw - r.xy) * 0.5;
        vec2 q = abs(px - c) - hs + uGlassR[i];
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - uGlassR[i];
        m = max(m, 1.0 - smoothstep(-0.75, 0.75, d));
    }
    return m;
}

const vec3 gold = vec3(1.0, 0.85, 0.5);

void main()
{
    float aspect = uResolution.x / uResolution.y;
    vec2 uv = vUv;

    // flush: the whole view gets sucked around the bowl a little
    vec2 dc = uv - uLightPos;
    float dist0 = length(dc * vec2(aspect, 1.0));
    float twist = uFlush * 2.2 * exp(-dist0 * 5.0);
    float cs = cos(twist), sn = sin(twist);
    uv = uLightPos + vec2(cs * dc.x - sn * dc.y * (1.0 / aspect), (sn * dc.x * aspect + cs * dc.y));

    vec3 col = texture(uScene, uv).rgb;
    vec3 holyOut = vec3(0.0);

    if (uHoly > 0.001)
    {
        vec2 d = (vUv - uLightPos) * vec2(aspect, 1.0);
        float dist = length(d);
        float ang = atan(d.y, d.x);

        // radial blur of the bright bits toward the light (crepuscular rays)
        vec3 rays = vec3(0.0);
        vec2 stepv = (vUv - uLightPos) / 48.0;
        vec2 s = vUv;
        float decay = 1.0;
        for (int i = 0; i < 48; ++i)
        {
            s -= stepv;
            vec3 c = texture(uScene, s).rgb;
            float near = exp(-length((s - uLightPos) * vec2(aspect, 1.0)) * 6.0);
            rays += max(c - 0.9, 0.0) * decay * near;
            decay *= 0.955;
        }
        rays *= 0.35;

        // procedural beams shooting out of the bowl
        float b1 = pow(0.5 + 0.5 * sin(ang * 9.0 + sin(ang * 3.0 + uTime * 0.35) * 2.0 + uTime * 0.25), 7.0);
        float b2 = pow(0.5 + 0.5 * sin(ang * 23.0 - uTime * 0.6), 14.0);
        float b3 = pow(0.5 + 0.5 * sin(ang * 5.0 + uTime * 0.9), 4.0);
        float beams = (b1 + 0.6 * b2 + 0.4 * b3) * exp(-dist * 1.1) * smoothstep(0.14, 0.34, dist);
        float glow = exp(-dist * dist * 45.0) * 0.45;

        // halo above the poop
        vec2 hd = (vUv - uHaloPos) * vec2(aspect, 1.0);
        hd.y *= 2.6;
        float ring = exp(-pow((length(hd) - 0.06) / 0.008, 2.0));
        float ringGlow = exp(-pow((length(hd) - 0.06) / 0.03, 2.0)) * 0.4;

        vec3 holyCol = gold * (beams * 0.75 + glow) + rays * gold + vec3(1.0, 0.92, 0.6) * (ring * 1.4 + ringGlow);
        col += holyCol * uHoly;
        holyOut = holyCol * uHoly;
        col = mix(col, col * vec3(1.05, 1.0, 0.9), uHoly * 0.3);
    }

    // frosted glass panels: blurred, darkened scene behind the UI
    float gm = glassMask(gl_FragCoord.xy);
    if (gm > 0.0)
    {
        vec3 blur = vec3(0.0);
        vec2 px = 1.0 / uResolution;
        float rad = 14.0 * uPixelScale;
        for (int i = 0; i < 24; ++i)
        {
            float a = float(i) * 2.39996;
            float rr = sqrt((float(i) + 0.5) / 24.0) * rad;
            blur += texture(uScene, vUv + vec2(cos(a), sin(a)) * rr * px).rgb;
        }
        blur /= 24.0;
        vec3 frosted = blur * 0.38 + vec3(0.03, 0.034, 0.042) + holyOut * 0.25;
        col = mix(col, frosted, gm);
    }

    // vignette + gentle tone map
    float vig = 1.0 - smoothstep(0.35, 1.25, length((vUv - 0.5) * vec2(aspect * 0.9, 1.0)));
    col *= mix(0.7, 1.0, vig);
    col = col / (1.0 + col * 0.18);
    col = pow(col, vec3(0.97));
    fragColor = vec4(col, 1.0);
}
)";
}
