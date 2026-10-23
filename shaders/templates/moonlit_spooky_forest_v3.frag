#version 330 core
out vec4 FragColor;

// ============================================================================
// Moonlit Spooky Forest v3: "Hexed Graveyard"
// ----------------------------------------------------------------------------
// A fixed, composed view: a rotten old graveyard in the foreground (crooked,
// cracked, mossy tombstones and crosses, a broken wrought-iron fence, open
// dirt, dead grass). Witch-hexed trees frame both sides: corkscrew trunks
// with knots, bulges, hollows and clawing roots, and deep recursive
// branching ending in hooked twigs. They writhe and creak and lean in toward
// you. A still pond sits in the mid-ground, and the creaky windmill's warm
// window is the focal light under a churning toxic-green painterly sky.
//
// The unease comes from geometry and space, never from hue cycling: the
// whole scene slowly breathes and warps, perspective wobbles, there is
// heat-haze/melt distortion, shadows crawl, and faint faces hide in bark
// knots and clouds. Festive spooky, no gore.
//
// Camera: static framing with an optional slow pan/drift (pan amount 0 =
// fully static). There is no forward motion.
// Ray-traced look as in v2: soft shadows, mirror ponds, warm bounce light,
// AO, rim light, shadowed volumetric fog, jitter, filmic grade, painterly
// post.
// Built on templates/audio_reactive_starter.frag and SHADERS.md v2.4. No
// u_speed control (the app scales iTime). Derived from v2 (left untouched).
// ============================================================================

uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;
uniform vec4 iAudioBandsAtt;

// Palette: Primary = pumpkin orange, Secondary = night purple, Accent = lime wisps
uniform vec3 u_primaryColor = vec3(0.30, 0.95, 0.45);   // {"widget":"color", "palette":true, "label":"Toxic Green (Primary)"}
uniform vec3 u_secondaryColor = vec3(0.10, 0.22, 0.30); // {"widget":"color", "palette":true, "label":"Deep Teal/Purple (Secondary)"}
uniform vec3 u_accentColor = vec3(1.0, 0.50, 0.10);     // {"widget":"color", "palette":true, "label":"Warm Orange (Accent)"}

uniform float u_panAmount = 0.6;       // {"widget":"slider", "label":"Pan Amount (0 = static)", "min":0.0, "max":2.5, "smooth":true}
uniform float u_panSpeed = 0.5;        // {"widget":"slider", "label":"Pan Speed", "min":0.0, "max":2.0, "smooth":true}
uniform float u_hexTwist = 1.0;        // {"widget":"slider", "label":"Hex / Twist Strength", "min":0.0, "max":2.0, "smooth":true}
uniform float u_writhe = 1.0;          // {"widget":"slider", "label":"Writhe Amount", "min":0.0, "max":3.0, "smooth":true}
uniform float u_unease = 1.0;          // {"widget":"slider", "label":"Scene Warp / Unease", "min":0.0, "max":3.0, "smooth":true}
uniform float u_graveDensity = 0.75;   // {"widget":"slider", "label":"Graveyard Density", "min":0.0, "max":1.0}
uniform float u_treeDensity = 0.8;     // {"widget":"slider", "label":"Tree Density", "min":0.0, "max":1.0}
uniform int u_branchDetail = 7;        // {"widget":"slider", "label":"Branch Depth", "min":3, "max":9}
uniform float u_fogDensity = 1.0;      // {"widget":"slider", "label":"Fog Density", "min":0.0, "max":3.0, "smooth":true}
uniform float u_godRays = 1.0;         // {"widget":"slider", "label":"God Ray Strength", "min":0.0, "max":3.0, "smooth":true}
uniform float u_moonSize = 1.0;        // {"widget":"slider", "label":"Moon Size", "min":0.4, "max":2.0, "smooth":true}
uniform float u_moonGlow = 0.6;        // {"widget":"slider", "label":"Moon Glow", "min":0.0, "max":3.0, "smooth":true}
uniform float u_shadowSoftness = 0.5;  // {"widget":"slider", "label":"Shadow Softness", "min":0.0, "max":1.0, "smooth":true}
uniform float u_reflection = 1.0;      // {"widget":"slider", "label":"Reflection Strength", "min":0.0, "max":2.0, "smooth":true}
uniform float u_pondRipple = 1.0;      // {"widget":"slider", "label":"Pond Ripple", "min":0.0, "max":3.0, "smooth":true}
uniform float u_pumpkinGlow = 1.0;     // {"widget":"slider", "label":"Pumpkin Glow", "min":0.0, "max":3.0, "smooth":true}
uniform float u_batAmount = 1.0;       // {"widget":"slider", "label":"Bat Amount", "min":0.0, "max":2.0}
uniform float u_fireflyAmount = 1.0;   // {"widget":"slider", "label":"Firefly Amount", "min":0.0, "max":3.0, "smooth":true}
uniform float u_audioReact = 1.0;      // {"widget":"slider", "label":"Audio Reactivity", "min":0.0, "max":2.0, "smooth":true}
uniform float u_audioSens = 0.0;       // {"widget":"slider", "label":"Audio Sensitivity (octaves)", "min":-3.0, "max":3.0, "smooth":true}
uniform float u_beatThreshold = 0.45;  // {"widget":"slider", "label":"Beat Threshold (hit level)", "min":0.1, "max":0.9}
uniform float u_exposure = 1.0;        // {"widget":"slider", "label":"Exposure", "min":0.3, "max":3.0, "smooth":true}
uniform float u_grain = 0.5;           // {"widget":"slider", "label":"Film Grain", "min":0.0, "max":2.0, "smooth":true}
uniform int u_maxSteps = 160;          // {"widget":"slider", "label":"Max Steps", "min":48, "max":256}
uniform int u_quality = 2;             // {"widget":"slider", "label":"Quality (0 low, 1 medium, 2 high)", "min":0, "max":2}
uniform float u_cloudChurn = 1.0;      // {"widget":"slider", "label":"Cloud Churn", "min":0.0, "max":3.0, "smooth":true}
uniform float u_painterly = 0.6;       // {"widget":"slider", "label":"Painterliness", "min":0.0, "max":1.5, "smooth":true}
uniform float u_windmillSpeed = 1.0;   // {"widget":"slider", "label":"Windmill Speed", "min":0.0, "max":3.0, "smooth":true}
uniform float u_windmillPos = 1.8;     // {"widget":"slider", "label":"Windmill Offset", "min":1.0, "max":10.0}
uniform float u_windmillVis = 1.0;     // {"widget":"slider", "label":"Windmill Visibility (0 hides)", "min":0.0, "max":1.0}
uniform float u_grassAmount = 0.7;    // {"widget":"slider", "label":"Grass & Reeds", "min":0.0, "max":2.0}
uniform float u_skyFlash = 1.0;        // {"widget":"slider", "label":"Sky Flash", "min":0.0, "max":2.0, "smooth":true}
uniform int u_debugView = 0;           // {"widget":"slider", "label":"Debug View (0 off, 1 meters, 2 shadow/AO)", "min":0, "max":2}

const float PI = 3.14159265;
const float TAU = 6.2831853;
const int   STEP_CAP = 256;
const float MAX_DIST = 60.0;
const float CEIL = 8.2;               // nothing solid above this height
const float CELL = 5.0;               // one tree (and maybe a pumpkin) per cell
const float AVENUE = 3.6;             // no tree cell centre closer than this to the path
const float WATER_Y = -0.2;
const float POND_PERIOD = 46.0;       // a pond on the avenue every POND_PERIOD units
const float CAM_H = 1.6;
const float TREE_BASE = 0.12;         // trunks start below ground, so no terrain lookup

// ---------------------------------------------------------------------------
// Audio helper (SHADERS.md 3.3)
// ---------------------------------------------------------------------------
float bandLevel(float v, float lo, float hi, float sens) {
    float a = log2(lo) - sens;
    float b = log2(hi) - sens;
    return clamp((log2(max(v, 1e-6)) - a) / (b - a), 0.0, 1.0);
}
const vec2 WIN_BASS = vec2(0.01, 0.5);
const vec2 WIN_MID  = vec2(0.005, 0.5);
const vec2 WIN_TREB = vec2(0.0005, 0.2);

struct Audio {
    float bass, mids, treb, level;
    float Lb, Lm, Lt, pinned;
};

Audio getAudio() {
    Audio A;
    float react = clamp(u_audioReact, 0.0, 2.0);
    float sens = clamp(u_audioSens, -3.0, 3.0);
    A.pinned = smoothstep(0.85, 0.98, iAudioBandsAtt.x);
    A.Lb = bandLevel(iAudioBandsAtt.x, WIN_BASS.x, WIN_BASS.y, sens);
    A.Lm = bandLevel(iAudioBandsAtt.y, WIN_MID.x,  WIN_MID.y,  sens - 1.5 * A.pinned);
    A.Lt = bandLevel(iAudioBandsAtt.z, WIN_TREB.x, WIN_TREB.y, sens - 0.75 * A.pinned);
    float LtRaw = bandLevel(iAudioBands.z, WIN_TREB.x, WIN_TREB.y, sens - 0.75 * A.pinned);
    float LbEff = mix(A.Lb, A.Lm, A.pinned);
    A.bass  = LbEff * react;
    A.mids  = A.Lm * react;
    A.treb  = max(A.Lt, 0.8 * LtRaw) * react;
    A.level = (LbEff + A.Lm + A.Lt) / 3.0 * react;
    return A;
}
float beatAge(float L, float thr, float span) { return clamp((L - thr) / span, 0.0, 1.0); }

float meterBar(vec2 px, float x0, float v) {
    return step(x0, px.x) * step(px.x, x0 + 14.0) *
           step(20.0, px.y) * step(px.y, 20.0 + 160.0 * clamp(v, 0.0, 1.0));
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i), hash12(i + vec2(1, 0)), u.x),
               mix(hash12(i + vec2(0, 1)), hash12(i + vec2(1, 1)), u.x), u.y);
}
const mat2 FBM_M = mat2(1.6, 1.2, -1.2, 1.6);
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { v += a * vnoise(p); p = FBM_M * p; a *= 0.5; }
    return v;
}

mat2 rot2(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}
// tapered segment from the origin up the local +y axis
float sdSeg(vec3 p, float h, float r0, float r1) {
    float y = clamp(p.y / h, 0.0, 1.0);
    return length(p - vec3(0.0, y * h, 0.0)) - mix(r0, r1, y);
}
vec3 acesFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// Per-frame globals (set once in main)
// ---------------------------------------------------------------------------
vec3 g_moonDir, g_moonU, g_moonV;
float g_moonR, g_moonGlow, g_time, g_flare, g_flicker;
float g_ringR, g_ringA;               // pond beat ripple
float g_batX;                         // bat burst age (1 = just launched)
float g_swirl, g_mids, g_treb;
int g_levels;
vec3 g_colP, g_colS, g_colA;
float g_pumpD;                        // distance to the nearest pumpkin (set by map)
float g_bladeAng, g_cloudT, g_flash, g_bass;
vec3 g_millPos; float g_millSide;     // nearest windmill (per map call)
const float MILL_PERIOD = 90.0;
float g_writhe, g_flinch, g_unease, g_pulse, g_crawl;
const vec2 CAM_XZ = vec2(0.0, -2.0);   // nominal camera ground position (trees lean toward it)
const vec2 GOFF = vec2(2.5, 0.0);      // tree grid offset: columns at x = ±5 frame the view
const float GC = 1.9;                  // graveyard cell size
vec2 cellId(vec2 xz) { return floor((xz - GOFF) / CELL); }

// ---------------------------------------------------------------------------
// Layout: path, terrain, ponds, cells
// ---------------------------------------------------------------------------
float pathX(float z) { return 0.8 * sin(z * 0.08); }

vec2 pondCentre(float z) {
    return vec2(0.8, 20.5);                 // fixed mid-ground pond, in front of the windmill
}
float pondMask(vec2 xz) {
    vec2 c = pondCentre(xz.y);
    float e = length((xz - c) / vec2(4.6, 3.6));
    return smoothstep(1.06, 0.86, e);
}

// One windmill every MILL_PERIOD along the avenue, alternating sides,
// u_windmillPos units off the path (clamped so the blades never reach it).
vec3 millAt(float z, out float side) {
    side = 1.0;                             // one fixed focal windmill
    float off = clamp(u_windmillPos, 1.0, 10.0);
    return vec3(pathX(30.0) + off, 0.05, 30.0);
}

float terrainH(vec2 xz) {
    float h = 0.2 + 0.16 * sin(xz.x * 0.31 + 1.3) * sin(xz.y * 0.27)
                  + 0.08 * sin(xz.x * 0.73 - xz.y * 0.61)
                  + 0.035 * sin(xz.x * 1.9 + xz.y * 1.7);
    float av = smoothstep(4.5, 1.5, abs(xz.x - pathX(xz.y)));      // flatter avenue
    h = mix(h, 0.12 + 0.03 * sin(xz.y * 0.4), av * 0.8);
    // graveyard: open-dirt grave mounds in front of each stone
    vec2 gq = xz / GC;
    vec2 gid = floor(gq);
    if (abs(xz.x) < 5.5 && xz.y > 1.5 && xz.y < 12.5 && hash12(gid + 1.7) < u_graveDensity) {
        vec2 mo = (fract(gq) - vec2(0.5, 0.35)) * GC;
        h += 0.09 * smoothstep(1.0, 0.0, length(mo / vec2(0.36, 0.62)));
    }
    h = mix(h, WATER_Y - 0.35, pondMask(xz));                        // pond basin
    return h;
}

// Tree in cell id: xy = base position (xz), z = seed, w = exists
vec4 treeCell(vec2 id) {
    vec2 c = (id + 0.5) * CELL + GOFF + (hash22(id) - 0.5) * 0.3;
    float exists = step(hash12(id + 7.31), u_treeDensity);
    exists = max(exists, step(c.y, 22.0) * step(0.15, u_treeDensity));   // hero trees near the view are always there
    exists *= step(0.0, c.y);                                            // nothing behind / beside the camera
    exists *= step(AVENUE, abs(c.x - pathX(c.y)));
    exists *= step(pondMask(c), 0.02);
    float ms; vec3 mp = millAt(c.y, ms);
    exists *= step(4.8, length(c - mp.xz)) + step(u_windmillVis, 0.001);
    exists = min(exists, 1.0);
    return vec4(c, hash12(id * 1.91 + 3.7), exists);
}

// Pumpkin in cell id: xyz = centre, w = flicker * exists. Only cells beside
// the avenue get one, placed on the avenue side and >= 2 units from the path.
vec4 pumpkinCell(vec2 id) {
    vec2 c = (id + 0.5) * CELL + GOFF;
    float dp = c.x - pathX(c.y);
    float h = hash12(id * 1.37 + 11.0);
    if (abs(dp) < AVENUE || abs(dp) > 11.0 || h > 0.6 || c.y < 0.0) return vec4(0.0);
    vec2 pos = c - vec2(sign(dp) * 1.7, 0.0) + (hash22(id + 3.1) - 0.5) * vec2(0.3, 1.6);
    if (pondMask(pos) > 0.05) return vec4(0.0);
    float fl = 0.8 + 0.2 * sin(g_time * 13.0 + h * 40.0) * sin(g_time * 7.3 + h * 17.0);
    fl *= g_flicker;
    return vec4(pos.x, terrainH(pos) + 0.2, pos.y, fl * (1.0 + g_flare));
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
// branch-level rotations, precomputed (yaw pi/2 + 0.35 sin(1.7 i), tilt -0.36 - 0.14 sin(2.1 i + 1): reaching up and out).
// Yaw near 90 degrees keeps every child branch inside the half-space kept by
// the parent's abs() fold. With v2's yaw values most mid levels had no
// preimage, so they vanished (checked numerically: all 8 levels now fill).
const mat2 BR_YAW[9] = mat2[9](mat2(-0.00000, -1.00000, 1.00000, -0.00000), mat2(-0.34016, -0.94037, 0.94037, -0.34016), mat2(0.08932, -0.99600, 0.99600, 0.08932), mat2(0.31839, -0.94796, 0.94796, 0.31839), mat2(-0.17208, -0.98508, 0.98508, -0.17208), mat2(-0.27585, -0.96120, 0.96120, -0.27585), mat2(0.24251, -0.97015, 0.97015, 0.24251), mat2(0.21466, -0.97669, 0.97669, 0.21466), mat2(-0.29620, -0.95513, 0.95513, -0.29620));
const mat2 BR_TILT[9] = mat2[9](mat2(0.88801, 0.45983, -0.45983, 0.88801), mat2(0.93383, 0.35772, -0.35772, 0.93383), mat2(0.97221, 0.23412, -0.23412, 0.97221), mat2(0.88743, 0.46095, -0.46095, 0.88743), mat2(0.93467, 0.35552, -0.35552, 0.93467), mat2(0.97194, 0.23521, -0.23521, 0.97194), mat2(0.88686, 0.46203, -0.46203, 0.88686), mat2(0.93550, 0.35332, -0.35332, 0.93550), mat2(0.97167, 0.23633, -0.23633, 0.97167));
const mat2 HOOK = mat2(0.85252, 0.52269, -0.52269, 0.85252);               // claw curl on the last levels

// Witch-hexed tree.
//   * whole tree leans toward the viewer and flinches on beats
//   * slow writhing domain warp (bass), twig tremble (treble)
//   * corkscrew trunk: twist + helical offset, ridges, knots, bulges,
//     a base flare and a hollow facing the viewer
//   * 7 clawing roots that arch out and hook into the ground
//   * 4 leaders, then binary mirror-fold recursion up to 8 more levels with
//     noise-bent kinks, ending in hooked claw twigs (up to ~1000 tips)
//   * per-subtree bounding sphere: a branch's whole subtree lies within
//     4.4 * len of its base, so we stop once that sphere can't beat d
// Warps break exact distances, so the result is scaled by 0.6 (Lipschitz).
float treeSDF(vec3 p, float s, vec2 leanDir) {
    float bound = max(length(p.xz) - 2.5, max(p.y - 7.5, -p.y - 1.2));
    if (bound > 0.5) return bound;
    float hx = clamp(u_hexTwist, 0.0, 2.0);
    float yy = clamp(p.y / 7.0, 0.0, 1.0);
    p.xz -= leanDir * (0.5 * hx * yy * yy + 0.3 * g_flinch * yy);       // lean in / flinch
    float ph = g_time * 0.35 + s * 20.0;
    p.x += g_writhe * 0.13 * yy * sin(p.y * 1.3 + ph);
    p.z += g_writhe * 0.13 * yy * cos(p.y * 1.1 + ph * 1.3);

    // ---- corkscrew trunk
    float H = 2.5 + 1.1 * s;
    vec3 q = p;
    q.xz = rot2(q.y * (0.9 + 1.2 * s) * hx + s * 6.0) * q.xz;
    float bend = clamp(q.y / H, 0.0, 1.0);
    q.x -= (0.2 * sin(q.y * 1.9 + s * 5.0) + 0.1 * hx) * bend;          // helical offset in the twisted frame
    float ang = (abs(q.x) + abs(q.z) > 1e-5) ? atan(q.z, q.x) : 0.0;    // atan guard
    float ridge = 0.045 * sin(ang * 3.0 + q.y * 2.5);
    float knot = 0.08 * smoothstep(0.6, 1.0, sin(q.y * 4.3 + s * 11.0) * sin(ang * 2.0 + s * 7.0));
    float ty = clamp(q.y, -0.7, H);
    float rr = mix(0.24, 0.1, clamp(ty / H, 0.0, 1.0)) * (1.0 + 0.7 * exp(-max(ty, 0.0) * 3.0));
    float d = length(q - vec3(0.0, ty, 0.0)) - rr - ridge - knot;
    if (d < 0.25)                                                        // bark displacement up close
        d -= 0.03 * vnoise(vec2(ang * 5.0, q.y * 11.0)) + 0.012 * vnoise(vec2(ang * 13.0, q.y * 31.0));
    // hollow facing the viewer
    vec3 hp = p - vec3(leanDir.x * 0.38, 0.65 + 0.4 * s, leanDir.y * 0.38);
    d = max(d, -(length(hp * vec3(1.0, 0.55, 1.0)) - 0.17));

    // ---- clawing roots
    vec3 r = p;
    float ra = (abs(r.x) + abs(r.z) > 1e-5) ? atan(r.z, r.x) + s * 3.0 : 0.0;
    const float RS = 0.8976;                                             // 7 roots
    float rid = floor(ra / RS + 0.5);
    float rla = ra - rid * RS;
    float rl = length(r.xz);
    r = vec3(rl * cos(rla), r.y, rl * sin(rla));
    float rlen = 1.3 + 0.5 * hash12(vec2(rid, s));
    float rx = clamp(r.x, 0.15, rlen);
    float ry = 0.5 - 0.55 * rx + 0.12 * rx * rx - 0.25 * smoothstep(rlen - 0.4, rlen, rx);   // arch, then hook down
    float rz = 0.06 * sin(rx * 6.0 + rid * 2.0);
    float d2 = length(vec3(r.x - rx, r.y - ry, r.z - rz)) - 0.15 * (1.0 - 0.8 * rx / rlen);
    d = smin(d, d2, 0.22);

    // ---- branches
    if (d < (H - 0.8) - p.y) return d * 0.6;
    // ---- branches: 4 asymmetric leaders (no abs-mirror), each with its own
    // recursive fold subtree. Asymmetry + per-leader yaw/tilt break the
    // Rorschach look; the fold still gives dense recursive branching.
    float trem = 0.015 * g_treb;
    float trTw = trem * sin(g_time * 37.0 + s * 40.0);                  // twig tremble (hoisted: one sin per call)
    // crown bound: all leaders + subtrees sit inside this sphere
    if (length(p - vec3(0.0, H * 0.75 + 1.3, 0.0)) - 3.0 > d) return d * 0.6;
    for (int lead = 0; lead < 4; lead++) {
        float lh = hash12(vec2(float(lead), s * 17.0));
        vec3 qb = p;
        qb.y -= H * (0.5 + 0.5 * lh);                                  // start at different heights
        float yaw0 = float(lead) * 1.5708 + s * 4.0 + lh * 0.6;         // 4 * 90deg
        qb.xz = rot2(yaw0) * qb.xz;
        qb.xy = rot2(-0.45 - 0.35 * lh) * qb.xy;                        // lean up and out
        float len = 0.58 + 0.22 * lh, rad = 0.095;
        vec3 q = qb;
        for (int i = 0; i < 9; i++) {
            if (i >= g_levels) break;
            if (length(q) - 4.4 * len - rad > d) break;
            q.xz = BR_YAW[i] * q.xz;
            q.x = abs(q.x);
            q.xy = BR_TILT[i] * q.xy;
            if (i >= g_levels - 2) q.xy = HOOK * q.xy;
            // strong asymmetric kink: breaks Rorschach, plus writhe + tremble
            float kink = sin(q.y * 5.5 / len + s * 11.0 + float(i) * 2.1 + float(lead) * 1.7 + g_time * 0.7 * g_writhe);
            q.x += (0.1 * hx + 0.05 * g_writhe) * len * kink;
            q.z += (0.07 * hx) * len * kink * kink - 0.035 * hx * len + trTw * (float(i & 1) * 2.0 - 1.0);
            float db = sdSeg(q, len, rad, rad * 0.7);
            d = min(d, db);
            q.y -= len;
            len *= 0.76;
            rad = max(rad * 0.68, 0.006);
        }
    }
    return d * 0.6;
}

// Jack-o'-lantern in local space (y up). Ribbed squashed sphere plus stem.
float pumpkinSDF(vec3 q) {
    float a = (length(q.xz) > 1e-5) ? atan(q.z, q.x) : 0.0;        // atan guard
    float rib = 1.0 - 0.07 * sqrt(abs(sin(a * 4.0)));
    vec3 e = q / vec3(0.34 * rib, 0.26, 0.34 * rib);
    float d = (length(e) - 1.0) * 0.24;
    vec3 sq = q - vec3(0.0, 0.2, 0.0);
    sq.x -= 0.03 * sq.y * 8.0 * sq.y;                                // curly stem
    d = min(d, sdSeg(sq, 0.14, 0.045, 0.03));
    return d;
}

// Distance to the nearest cell boundary (+ small slack): limits steps so a
// ray cannot jump over a neighbouring cell's tree.
// Creaky broken windmill, local space: base at origin, blades face -z.
// Tapered leaning tower, crooked cap, hub, 8 tattered blades (some broken),
// tail vane. Returns distance; window handled in shading.
float sdBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); }
float windmillSDF(vec3 p) {
    float bound = length(p - vec3(0.0, 4.8, 0.0)) - 8.0;
    if (bound > 0.5) return bound;
    vec3 q = p;
    q.x -= 0.05 * q.y;                                   // lean
    float y = clamp(q.y / 5.6, 0.0, 1.0);
    float w = mix(1.25, 0.62, y);
    vec2 xz = rot2(0.785398 * 0.5) * q.xz;               // octagon-ish
    float oct = max(max(abs(q.x), abs(q.z)), (abs(xz.x) + abs(xz.y)) * 0.7071 + 0.0);
    float d = max(oct - w, abs(q.y - 2.8) - 2.8) * 0.85;
    // planks: subtle horizontal ridges (silhouette roughness)
    d -= 0.02 * smoothstep(0.3, 0.9, sin(q.y * 9.0));
    // crooked cap
    vec3 c = q - vec3(0.0, 5.95, -0.1);
    c.xy = rot2(0.12) * c.xy;
    float roof = max(sdBox(c, vec3(0.85, 0.45, 1.0)), dot(vec2(abs(c.x), c.y), vec2(0.6, 0.8)) - 0.25);
    d = min(d, roof);
    // tail vane
    vec3 tv = q - vec3(0.0, 6.0, 1.6);
    d = min(d, sdBox(tv, vec3(0.03, 0.05, 0.9)));
    d = min(d, sdBox(tv - vec3(0.0, 0.25, 0.75), vec3(0.03, 0.35, 0.25)));
    // hub
    vec3 h = q - vec3(0.0, 5.8, -1.05);
    d = min(d, max(length(h.xy) - 0.22, abs(h.z) - 0.25));
    // blades in the xy plane around the hub
    vec3 b = h - vec3(0.0, 0.0, -0.2);
    b.xy = rot2(g_bladeAng) * b.xy;
    float r = length(b.xy);
    float a = (r > 1e-5) ? atan(b.y, b.x) : 0.0;         // atan guard
    const float SEC = 0.785398;                          // 8 blades
    float id = floor((a + SEC * 0.5) / SEC);
    float la = a - id * SEC;
    vec2 bp = r * vec2(cos(la), sin(la));
    float hb = hash12(vec2(id, 4.1));
    float len = (hb < 0.18) ? 1.2 : mix(3.0, 4.2, hb);   // a couple snapped off
    float ww = 0.07 + 0.2 * smoothstep(0.4, 1.2, bp.x) - 0.06 * abs(sin(bp.x * 6.0 + id * 2.3));   // ragged edge
    float tip = bp.x - len - 0.25 * sin(bp.y * 20.0 + id);   // torn tips
    float blade = max(max(abs(bp.y - 0.04 * bp.x) - ww, max(tip, 0.25 - bp.x)), abs(b.z) - 0.035);
    // spar through each blade
    float spar = max(max(abs(bp.y) - 0.035, max(bp.x - len - 0.2, -bp.x)), abs(b.z + 0.05) - 0.04);
    d = min(d, min(blade * 0.8, spar));
    return d;
}

// ---- rotten graveyard: crooked, cracked, mossy headstones and crosses, one
// per GC cell (inside its cell, so a ray-cell step limit keeps tracing exact).
vec4 graveCell(vec2 id) {
    vec2 c = (id + 0.5) * GC + (hash22(id + 4.2) - 0.5) * 0.35;
    float e = step(hash12(id + 1.7), u_graveDensity) * step(abs(c.x), 5.2) * step(1.8, c.y) * step(c.y, 12.2);
    return vec4(c, hash12(id * 2.3 + 0.7), e);
}
float graveSDF(vec3 p, vec2 id) {
    vec4 g = graveCell(id);
    if (g.w < 0.5) return 1e3;
    vec3 q = p - vec3(g.x, terrainH(g.xy) - 0.08, g.y);
    float bnd = length(q - vec3(0.0, 0.5, 0.0)) - 0.9;
    if (bnd > 0.3) return bnd;
    float h = g.z;
    q.xz = rot2((h - 0.5) * 0.7) * q.xz;                               // crooked yaw
    q.xy = rot2((hash12(id + 9.1) - 0.5) * 0.5) * q.xy;                // lean sideways
    q.zy = rot2((hash12(id + 3.3) - 0.35) * 0.45) * q.zy;              // lean fore/back
    float sc = 0.8 + 0.45 * hash12(id + 6.6);
    q /= sc;
    float d;
    if (h < 0.62) {
        // headstone: slab with a rounded top, broken corner, crack
        float slab = sdBox(q - vec3(0.0, 0.3, 0.0), vec3(0.3, 0.3, 0.075));
        float top = max(length(q.xy - vec2(0.0, 0.6)) - 0.3, abs(q.z) - 0.075);
        d = min(slab, max(top, -(q.y - 0.6)) );
        d = min(slab, top);
        d = max(d, -q.y - 0.1);
        if (hash12(id + 5.5) < 0.5) d = max(d, dot(q.xy - vec2(0.18, 0.72), normalize(vec2(1.0, 0.8))));   // broken corner
        float crack = abs(q.x - 0.05 - 0.06 * sin(q.y * 14.0 + h * 30.0)) - 0.012;
        d = max(d, -max(crack, 0.75 - q.y - 0.4 * h));                      // crack from the top down
        d -= 0.006 * vnoise(q.xy * 30.0);                                    // weathering
    } else {
        // wooden/stone cross
        float v = sdBox(q - vec3(0.0, 0.5, 0.0), vec3(0.055, 0.55, 0.05));
        vec3 hq = q - vec3(0.0, 0.75, 0.0);
        hq.xy = rot2((h - 0.8) * 0.6) * hq.xy;                              // skewed arm
        d = min(v, sdBox(hq, vec3(0.28, 0.05, 0.05)));
    }
    return d * sc * 0.85;
}
// broken wrought-iron fence along the graveyard's left side and back
float fenceSDF(vec3 p) {
    vec3 q = p;
    float dx = 1e3;
    // left run: x = -5.6, z 1.5..12.5; back run: z = 12.8, x -5.6..-1.0
    float dl = max(abs(q.x + 5.6) - 0.6, max(1.5 - q.z, q.z - 12.5));
    float db = max(abs(q.z - 12.8) - 0.6, max(-5.6 - q.x, q.x + 1.0));
    if (min(dl, db) > 0.5) return min(dl, db);
    vec2 lr = (dl < db) ? vec2(q.z, q.x + 5.6) : vec2(q.x, q.z - 12.8);  // (along, across)
    float seg = floor(lr.x / 1.6);
    float hs = hash12(vec2(seg, 7.7));
    if (hs < 0.22) return max(abs(lr.y) - 0.3, 0.6);                      // missing section
    float y = p.y - 0.05;
    lr.y -= (hs - 0.5) * 0.35 * y;                                       // section leans
    float along = lr.x - 0.15 * (hs - 0.5) * y * y;
    float bz = mod(along, 0.22) - 0.11;
    float topY = 1.05 - 0.45 * step(0.75, hash12(vec2(floor(along / 0.22), seg)));   // some bars snapped short
    float bar = max(length(vec2(bz, lr.y)) - 0.014, max(y - topY, -y - 0.2));
    float spear = max(length(vec2(bz, lr.y)) - 0.04 * (1.0 - (y - topY) / 0.12), max(topY - y, y - topY - 0.12));
    float rails = max(abs(lr.y) - 0.018, min(abs(y - 0.9), abs(y - 0.18)) - 0.02);
    rails = max(rails, abs(mod(lr.x, 1.6) - 0.8) - 0.79);
    return min(min(bar, spear), rails) * 0.8;
}

// Distance along the current ray (g_rd) to where it leaves this cell, plus a
// small slack, so a step cannot jump over a neighbouring cell's tree.
// Ray-aware, so rays that skim a cell wall don't crawl and exhaust their
// steps (that made sky pixels turn into "hits" along x = const columns).
// Off (huge) for normals and AO so gradients are not distorted.
vec3 g_rd = vec3(0.0, 0.0, 1.0);
bool g_cellOn = false;
bool g_limited = false;
float g_trueD = 1e3;     // treesOnly: unclipped distance to this cell's tree   // true when the last map() step was clipped by cellLimit
float cellLimit(vec2 xz) {
    if (!g_cellOn) return 1e3;
    vec2 lc = (xz - GOFF) - CELL * floor((xz - GOFF) / CELL);
    vec2 r = g_rd.xz;
    float tx = (abs(r.x) > 1e-4) ? ((r.x > 0.0) ? (CELL - lc.x) / r.x : -lc.x / r.x) : 1e3;
    float tz = (abs(r.y) > 1e-4) ? ((r.y > 0.0) ? (CELL - lc.y) / r.y : -lc.y / r.y) : 1e3;
    float lim = max(min(tx, tz), 0.0) + 0.06;
    if (abs(xz.x) < 6.5 && xz.y > 1.0 && xz.y < 13.5) {                   // graveyard cells
        vec2 gl = xz - GC * floor(xz / GC);
        float gx = (abs(r.x) > 1e-4) ? ((r.x > 0.0) ? (GC - gl.x) / r.x : -gl.x / r.x) : 1e3;
        float gz = (abs(r.y) > 1e-4) ? ((r.y > 0.0) ? (GC - gl.y) / r.y : -gl.y / r.y) : 1e3;
        lim = min(lim, max(min(gx, gz), 0.0) + 0.03);
    }
    return lim;
}

// Whole-scene breathing / warping (unease) plus a beat warp pulse. Applied
// to every distance query; results are scaled by WARP_K (Lipschitz).
const float WARP_K = 0.85;
vec3 uneaseWarp(vec3 p) {
    float U = g_unease;
    float T = g_time;
    float hgt = smoothstep(0.0, 4.0, p.y);
    p.y += 0.07 * U * sin(p.x * 0.31 + T * 0.4) * sin(p.z * 0.23 - T * 0.33);          // ground breathes
    p.xz += U * 0.16 * hgt * vec2(sin(p.z * 0.17 + T * 0.27), cos(p.x * 0.19 + T * 0.23));
    vec2 rc = p.xz - CAM_XZ;
    float rl = length(rc) + 1e-3;
    p.xz += (rc / rl) * g_pulse * 0.18 * sin(rl * 0.45 - 1.0) * hgt;                     // beat pulse
    return p;
}

// Cheap tree stand-in for god-ray shadows: twisted trunk capsule plus a
// lumpy, holey crown shell (sin-lattice), so shafts still come through gaps.
float treesProxy(vec3 p) {
    p = uneaseWarp(p);
    p.xz += g_crawl * vec2(sin(p.y * 0.9 + g_time * 0.8), cos(p.y * 0.7 + g_time * 0.65));
    vec2 id = cellId(p.xz);
    vec4 tc = treeCell(id);
    float d = 1e3;
    if (tc.w > 0.5) {
        vec3 q = p - vec3(tc.x, TREE_BASE, tc.y);
        float H = 2.5 + 1.1 * tc.z;
        float trunk = length(q - vec3(0.0, clamp(q.y, 0.0, H), 0.0)) - 0.22;
        vec3 c = q - vec3(0.0, H * 0.75 + 1.3, 0.0);
        float holes = sin(c.x * 3.1 + tc.z * 9.0) * sin(c.y * 2.7 + g_time * 0.3 * g_writhe) * sin(c.z * 3.3);
        float crown = max(abs(length(c * vec3(1.0, 1.4, 1.0)) - 1.6) - 0.6, holes * 0.5 - 0.05);
        d = min(trunk, crown) * 0.7;
    }
    float cl = cellLimit(p.xz);
    g_limited = cl < d;
    g_trueD = d;
    return min(d, cl);
}

float treesOnly(vec3 p) {
    p = uneaseWarp(p);
    p.xz += g_crawl * vec2(sin(p.y * 0.9 + g_time * 0.8), cos(p.y * 0.7 + g_time * 0.65)); // crawling shadows
    vec2 id = cellId(p.xz);
    vec4 tc = treeCell(id);
    float d = 1e3;
    if (tc.w > 0.5) {
        vec3 q = p - vec3(tc.x, TREE_BASE, tc.y);
        d = treeSDF(q, tc.z, normalize(CAM_XZ - tc.xy)) * WARP_K;
    }
    float cl = cellLimit(p.xz);
    g_limited = cl < d;
    g_trueD = d;
    return min(d, cl);
}

// materials: 1 ground, 2 water, 3 bark, 4 pumpkin
// materials: 1 ground, 2 water, 3 bark, 4 pumpkin, 5 windmill, 6 stone, 7 iron
vec2 map(vec3 p) {
    p = uneaseWarp(p);
    vec2 res = vec2((p.y - terrainH(p.xz)) * 0.8 * WARP_K, 1.0);
    float w = p.y - WATER_Y;
    if (w < res.x) res = vec2(w, 2.0);

    vec2 id = cellId(p.xz);
    vec4 tc = treeCell(id);
    if (tc.w > 0.5) {
        vec3 q = p - vec3(tc.x, TREE_BASE, tc.y);
        float d = treeSDF(q, tc.z, normalize(CAM_XZ - tc.xy)) * WARP_K;
        if (d < res.x) res = vec2(d, 3.0);
    }
    if (u_windmillVis > 0.001) {
        vec3 mp = millAt(p.z, g_millSide);
        vec3 lp = p - mp;
        lp.xz = rot2(g_millSide * 0.35) * lp.xz;          // turn toward the avenue
        float dm = windmillSDF(lp) * WARP_K;
        if (dm < res.x) res = vec2(dm, 5.0);
    }
    if (abs(p.x) < 6.5 && p.z > 1.0 && p.z < 13.5 && p.y < 1.6) {
        float dg = graveSDF(p, floor(p.xz / GC)) * WARP_K;
        if (dg < res.x) res = vec2(dg, 6.0);
        float df = fenceSDF(p) * WARP_K;
        if (df < res.x) res = vec2(df, 7.0);
    }
    g_pumpD = 1e3;
    vec4 pk = pumpkinCell(id);
    if (pk.w > 0.0) {
        float d = pumpkinSDF(p - pk.xyz) * WARP_K;
        g_pumpD = d;
        if (d < res.x) res = vec2(d, 4.0);
    }
    float cl = cellLimit(p.xz);
    g_limited = cl < res.x;
    res.x = min(res.x, cl);
    return res;
}

vec3 calcNormal(vec3 p, float t) {
    float e = 0.0006 * clamp(t, 1.0, 20.0);
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * e).x + k.yyx * map(p + k.yyx * e).x +
                     k.yxy * map(p + k.yxy * e).x + k.xxx * map(p + k.xxx * e).x);
}

float calcAO(vec3 p, vec3 n, int taps) {
    float occ = 0.0, sca = 1.0;
    for (int i = 0; i < 5; i++) {
        if (i >= taps) break;
        float h = 0.04 + 0.14 * float(i);
        occ += (h - map(p + n * h).x) * sca;
        sca *= 0.75;
    }
    return clamp(1.0 - 1.8 * occ, 0.0, 1.0);
}

// Soft shadow toward the moon (penumbra from the closest-approach ratio).
float softShadow(vec3 ro, vec3 rd, int steps, float k, float jit) {
    float res = 1.0;
    float t = 0.03 + 0.03 * jit;
    g_rd = rd; g_cellOn = true;
    int lv0 = g_levels; g_levels = min(max(lv0 - 2, 3), 5);              // shadows: coarser crown
    for (int i = 0; i < 64; i++) {
        if (i >= steps) break;
        float h = treesOnly(ro + rd * t);      // casters: trees (terrain is gentle, pumpkins tiny)
        res = min(res, k * g_trueD / t);   // penumbra from the unclipped distance (no banding)
        if (res < 0.002) break;
        t += clamp(h, 0.02, 0.45) * (0.8 + 0.4 * jit);   // small capped steps: thin branches no longer band into stair-steps
        if (t > 25.0 || ro.y + rd.y * t > CEIL) break;
    }
    g_cellOn = false; g_levels = lv0;
    res = clamp(res, 0.0, 1.0);
    return res * res * (3.0 - 2.0 * res);
}

// ---------------------------------------------------------------------------
// Sky: purple gradient, stars, cratered moon with halo, cute bats
// ---------------------------------------------------------------------------
float batShape(vec2 p, float flap, out float eye) {
    float ax = abs(p.x);
    float body = length(vec2(p.x * 1.25, p.y + 0.03)) - 0.15;
    float head = length(p - vec2(0.0, 0.16)) - 0.11;
    float ear  = length(vec2(ax - 0.075, (p.y - 0.27) * 0.55)) - 0.045;
    float top = 0.05 + flap * 0.75 * ax - 0.1 * ax * ax + 0.06 * smoothstep(0.75, 1.0, ax);
    float bot = top - 0.36 * (1.0 - 0.7 * ax) + 0.12 * sqrt(abs(sin(ax * 9.42)));
    float wing = max(max(p.y - top, bot - p.y), max(ax - 1.0, 0.06 - ax));
    eye = length(vec2(ax - 0.045, p.y - 0.175)) - 0.022;
    return min(min(body, head), min(ear, wing));
}

// returns rgb, alpha (bat silhouettes over everything behind them)
vec4 bats(vec2 m, float px) {
    float cover = 0.0;
    float eyes = 0.0;
    int nAmb = int(clamp(u_batAmount, 0.0, 2.0) * 4.0 + 0.5);
    for (int i = 0; i < 8; i++) {
        if (i >= nAmb) break;
        float h = hash12(vec2(float(i), 3.3));
        float sp = 0.035 * (0.7 + 0.6 * h);
        float ph = fract(g_time * sp + h * 7.0);
        vec2 c = vec2(mix(-1.5, 1.5, ph) * (h > 0.5 ? 1.0 : -1.0),
                      0.02 + 0.28 * (hash12(vec2(float(i), 9.1)) - 0.4) + 0.04 * sin(g_time * 1.3 + h * 20.0));
        float sz = 0.06 + 0.04 * h;
        float flap = sin(g_time * (9.0 + 4.0 * h) + h * 30.0);
        vec2 lp = (m - c) / sz;
        if (dot(lp, lp) > 2.0) continue;
        if (h <= 0.5) lp.x = -lp.x;
        float e;
        float d = batShape(lp, flap, e) * sz;
        cover = max(cover, smoothstep(px, -px, d));
        eyes = max(eyes, smoothstep(px, -px, e * sz));
    }
    // beat burst: bats scatter outward from the moon
    if (g_batX > 0.0 && u_batAmount > 0.0) {
        float seed = floor(g_time * 0.5);
        for (int k = 0; k < 7; k++) {
            float hk = hash12(vec2(float(k), seed));
            float ang = float(k) * 0.8976 + hk * 0.6;
            float r = (1.0 - g_batX) * (0.5 + 0.5 * hk) * 1.3 + 0.05;
            vec2 c = r * vec2(cos(ang), sin(ang) * 0.7 + 0.15);
            float sz = 0.045 + 0.06 * (1.0 - g_batX) + 0.02 * hk;
            float flap = sin(g_time * 22.0 + hk * 30.0);
            vec2 lp = (m - c) / sz;
            if (dot(lp, lp) > 2.0) continue;
            float e;
            float d = batShape(lp, flap, e) * sz;
            float a = smoothstep(px, -px, d) * smoothstep(0.0, 0.15, g_batX);
            cover = max(cover, a);
            eyes = max(eyes, smoothstep(px, -px, e * sz) * a);
        }
    }
    vec3 c = vec3(0.004, 0.006, 0.005) + vec3(1.0, 0.6, 0.15) * eyes * 1.5;
    return vec4(c, cover);
}

vec3 moonSurface(vec2 mp) {
    // mp: disc coordinates, |mp| < 1
    float nz = sqrt(max(1.0 - dot(mp, mp), 0.0));
    float shade = 0.55 + 0.45 * nz;                                   // limb darkening
    float maria = 0.5 + 0.5 * sin(mp.x * 3.1 + 1.0) * sin(mp.y * 2.7 - 0.6) + 0.25 * sin(mp.x * 7.0 + mp.y * 5.0);
    float alb = mix(1.0, 0.72, smoothstep(0.55, 0.95, maria));
    for (int i = 0; i < 9; i++) {
        vec2 h = hash22(vec2(float(i), 1.7));
        vec2 c = (h - 0.5) * 1.5;
        float cr = 0.06 + 0.13 * hash12(vec2(float(i), 5.2));
        float d = length(mp - c) / cr;
        if (d < 1.15) {
            float floorD = smoothstep(1.0, 0.75, d);
            float rim = smoothstep(0.75, 1.0, d) * smoothstep(1.15, 1.0, d);
            float lit = dot(normalize(mp - c + 1e-5), vec2(-0.6, 0.8));  // rim lit from upper left
            alb *= 1.0 - 0.18 * floorD;
            alb *= 1.0 + 0.25 * rim * lit;
        }
    }
    return vec3(1.0, 0.96, 0.86) * alb * shade;
}

// Churning toxic clouds: domain-warped fbm on a sky dome plane, stretched
// horizontally for brushy streaks. Returns colour; alpha = cloud cover.
vec4 clouds(vec3 rd) {
    vec2 c = rd.xz / (max(rd.y, 0.0) + 0.18);
    c = c * vec2(0.45, 0.9) + vec2(0.0, g_cloudT * 0.6);
    float T = g_cloudT;
    vec2 q = vec2(fbm(c + vec2(0.0, T)), fbm(c + vec2(5.2, 1.3) - vec2(T * 0.7, 0.0)));
    vec2 r = vec2(fbm(c + 3.5 * q + vec2(1.7, 9.2) + 0.15 * T * g_swirl),
                  fbm(c + 3.5 * q + vec2(8.3, 2.8) - 0.12 * T * g_swirl));
    float f = fbm(c + 3.0 * r);
    float streak = vnoise(vec2(c.x * 0.6, c.y * 9.0) + r * 2.0);    // brush streaks
    f = clamp(f + 0.12 * (streak - 0.5), 0.0, 1.0);
    vec3 deep = g_colS * 0.25;
    vec3 emer = mix(g_colA, g_colS, 0.5) * 0.42;
    vec3 lime = g_colA * 0.75;
    vec3 col = mix(deep, emer, smoothstep(0.25, 0.6, f));
    col = mix(col, lime, smoothstep(0.62, 0.9, f) * (0.55 + 0.45 * r.x));
    col = mix(col, vec3(0.22, 0.05, 0.22), smoothstep(0.55, 0.85, q.y) * 0.35);   // purple hints
    col = mix(col, vec3(0.35, 0.06, 0.04), smoothstep(0.7, 0.95, r.y) * 0.25);    // red hints
    col *= (0.35 + 0.55 * length(q)) * 0.75;
    // pareidolia: a faint face hidden in the churn (two hollow eyes, a slack mouth)
    vec2 fc = vec2(atan(rd.x, rd.z) - 0.05, rd.y - 0.33) * vec2(1.0, 1.4);
    float fe = smoothstep(0.06, 0.0, length(vec2(abs(fc.x) - 0.09, fc.y)) - 0.02);
    float fm = smoothstep(0.03, 0.0, abs(length(vec2(fc.x, fc.y + 0.06)) - 0.1)) * step(fc.y, -0.09);
    f = mix(f, f * 0.55, max(fe, fm * 0.6) * 0.35 * (0.6 + 0.4 * sin(g_time * 0.3)));
    float cover = smoothstep(0.3, 0.7, f);
    return vec4(col, cover);
}

vec3 sky(vec3 rd, bool withBats, float px) {
    float up = clamp(rd.y, 0.0, 1.0);
    vec3 col = mix(g_colA * 0.05 + g_colS * 0.08, g_colS * 0.03, pow(up, 0.5));
    // stars, only visible through cloud gaps
    float az = (abs(rd.x) + abs(rd.z) > 1e-6) ? atan(rd.x, rd.z) : 0.0;  // atan guard
    vec2 sg = vec2(az * 40.0, rd.y * 80.0);
    vec2 sid = floor(sg);
    vec2 sf = fract(sg) - 0.5;
    float sh = hash12(sid);
    vec3 starC = vec3(0.0);
    if (sh < 0.1 && rd.y > 0.05) {
        vec2 o = (hash22(sid + 1.3) - 0.5) * 0.6;
        float tw = 0.6 + 0.4 * sin(g_time * (2.0 + 6.0 * sh) + sh * 60.0);
        tw = mix(tw, tw * tw * 2.5, clamp(g_treb, 0.0, 1.0));       // treble sparkle
        starC = vec3(0.8, 1.0, 0.85) * exp(-dot(sf - o, sf - o) * 90.0) * tw * (0.4 + 3.0 * sh);
    }
    // hazy moon
    float cth = dot(rd, g_moonDir);
    vec2 m = vec2(dot(rd, g_moonU), dot(rd, g_moonV));
    vec3 moonC = vec3(0.0); // hazed
    if (cth > 0.0) {
        float md = length(m) / g_moonR;
        float halo = exp(-max(md - 1.0, 0.0) * 1.6) * 0.35 + exp(-max(md - 1.0, 0.0) * 0.35) * 0.12;
        moonC += mix(vec3(0.7, 1.0, 0.75), g_colA, 0.4) * halo * g_moonGlow;
        if (md < 1.02) {
            vec3 ms = moonSurface(m / g_moonR) * vec3(0.8, 1.0, 0.75) * 1.3 * (0.7 + 0.3 * g_moonGlow);
            moonC = mix(moonC, ms, smoothstep(1.0 + px / g_moonR, 1.0 - px / g_moonR, md));
        }
    }
    vec4 cl = clouds(rd);
    moonC *= 0.6;                                               // haze
    col += starC * (1.0 - cl.a) + moonC;
    // clouds over the moon: moon glows through (lit edges), partly hidden
    float lit = 1.0 + 1.6 * exp(-max(length(m) / g_moonR - 1.0, 0.0) * 0.8) * step(0.0, cth) * g_moonGlow;
    col = mix(col, cl.rgb * lit, cl.a * 0.9);
    col += (1.0 - cl.a * 0.5) * cl.rgb * 0.12;
    // festive green "lightning" flash on beats: lights the cloud tops
    col += g_flash * g_colA * 0.9 * (0.15 + 0.85 * cl.a * cl.a) * smoothstep(-0.05, 0.3, rd.y);
    col *= smoothstep(-0.25, 0.05, rd.y) * 0.85 + 0.15;
    if (withBats && cth > 0.0) {
        vec4 bt = bats(m, px);
        col = mix(col, bt.rgb, bt.a);
    }
    return col;
}

// ---------------------------------------------------------------------------
// Lighting
// ---------------------------------------------------------------------------
vec3 moonLightCol() { return mix(vec3(0.55, 0.85, 0.65), g_colA, 0.3) * (0.3 + 0.2 * g_moonGlow) * (1.0 + 1.5 * g_flash); }

vec3 pumpkinLights(vec3 p, vec3 n) {
    vec2 id0 = cellId(p.xz);
    float acc = 0.0;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec4 pk = pumpkinCell(id0 + vec2(float(i), float(j)));
            if (pk.w <= 0.0) continue;
            vec3 d = pk.xyz + vec3(0.0, 0.05, 0.0) - p;
            float r2 = dot(d, d);
            float ndl = max(dot(n, d * inversesqrt(r2 + 1e-6)), 0.0) * 0.8 + 0.2;
            acc += ndl * pk.w / (1.0 + r2 * 2.5) * smoothstep(25.0, 6.0, r2);   // windowed: zero before the 3x3 horizon (no seams)
        }
    }
    // windmill window: a warm light at the window
    if (u_windmillVis > 0.001) {
        float ms; vec3 mp = millAt(p.z, ms);
        vec3 wp = mp + vec3(-ms * 0.6, 3.4, -1.0);
        vec3 d = wp - p;
        float r2 = dot(d, d);
        acc += (max(dot(n, d * inversesqrt(r2 + 1e-6)), 0.0) * 0.8 + 0.2) * 1.5 * g_flicker / (1.0 + r2 * 0.6) * u_windmillVis;
    }
    return mix(g_colP, vec3(1.0, 0.6, 0.2), 0.3) * acc * 3.2 * u_pumpkinGlow;
}

// Pumpkin face: carved triangle eyes and a toothy grin, facing the avenue
float pumpkinFace(vec3 q, vec2 faceDir) {
    vec2 f = vec2(dot(q.xz, vec2(faceDir.y, -faceDir.x)), q.y);      // (sideways, up)
    float front = dot(q.xz, faceDir);
    if (front < 0.12) return 0.0;
    vec2 e = vec2(abs(f.x) - 0.11, f.y - 0.05);
    float eye = max(max(-e.y - 0.0, e.y + abs(e.x) * 1.4 - 0.08), 0.0);
    float eyeM = step(e.y, 0.08 - abs(e.x) * 1.4) * step(0.0, e.y + 0.0) * step(abs(e.x), 0.06);
    float nose = step(abs(f.x), 0.03 - (f.y + 0.02) * 0.5) * step(-0.02, f.y) * step(f.y, 0.03);
    float mouthTop = -0.06 + 0.05 * f.x * f.x * 10.0;
    float teeth = 0.018 * step(0.5, fract(f.x * 14.0));
    float mouth = step(abs(f.x), 0.19) * step(f.y, mouthTop - teeth) * step(mouthTop - 0.07 + 0.08 * f.x * f.x * 10.0, f.y);
    return max(max(eyeM, nose), mouth) * smoothstep(0.12, 0.2, front) + 0.0 * eye;
}

// Full surface shade. Shadow/AO counts come from the quality preset.
vec3 shade(vec3 pos, vec3 n, vec3 rd, float mat, int shSteps, int aoTaps, float jit, out float shadowOut) {
    vec3 L = g_moonDir;
    float k = mix(40.0, 5.0, clamp(u_shadowSoftness, 0.0, 1.0));
    float sh = (shSteps > 0) ? softShadow(pos + n * 0.02, L, shSteps, k, jit) : 1.0;
    float ao = (aoTaps > 0) ? calcAO(pos, n, aoTaps) : 1.0;
    shadowOut = sh * ao;

    vec3 alb;
    float spec = 0.0;
    vec3 emis = vec3(0.0);
    if (mat < 1.5) {
        // mossy ground with fallen orange leaves
        vec2 lc = pos.xz * 3.0;
        vec2 lid = floor(lc);
        float lh = hash12(lid);
        vec2 lo = fract(lc) - 0.5 - (hash22(lid) - 0.5) * 0.5;
        lo = rot2(lh * 6.28) * lo;
        float leaf = step(lh, 0.07) * smoothstep(0.02, -0.02, length(lo * vec2(1.0, 2.2)) - 0.11);   // sparse small leaves
        alb = 0.22 * mix(vec3(0.075, 0.07, 0.085), vec3(0.06, 0.09, 0.06), 0.5 + 0.5 * sin(pos.x * 1.7 + pos.z * 1.3));
        alb = mix(alb, mix(g_colP, vec3(0.4, 0.12, 0.04), lh * 10.0) * 0.12 * (0.6 + 0.4 * sin(lo.x * 40.0)), leaf);
        float gy = smoothstep(6.5, 5.0, abs(pos.x)) * smoothstep(1.0, 2.5, pos.z) * smoothstep(13.5, 12.0, pos.z);
        alb = mix(alb, vec3(0.03, 0.022, 0.015) * (0.7 + 0.6 * vnoise(pos.xz * 6.0)), gy * 0.8);   // open dirt
        spec = 0.15;
    } else if (mat < 3.5) {
        // bark: twisted ridges
        float ridge = 0.5 + 0.5 * sin(atan(n.z, n.x + 1e-6) * 7.0 + pos.y * 3.0);
        alb = vec3(0.022, 0.024, 0.022) * (0.75 + 0.35 * ridge) * (0.7 + 0.6 * vnoise(vec2(pos.y * 9.0, atan(n.z, n.x + 1e-6) * 4.0)));
        spec = 0.08;
        // pareidolia: a faint pair of eyes and a mouth in some trunks' knots
        vec3 wp = uneaseWarp(pos);
        vec4 tc = treeCell(cellId(wp.xz));
        vec2 ld = normalize(CAM_XZ - tc.xy);
        vec3 lp = wp - vec3(tc.x, TREE_BASE, tc.y);
        vec2 sd = vec2(-ld.y, ld.x);
        float fy = 1.45 + 0.6 * tc.z;
        vec2 fp = vec2(dot(lp.xz, sd), lp.y - fy);
        if (hash12(tc.xy) < 0.55 && dot(lp.xz, ld) > 0.0) {
            float eyes = smoothstep(0.075, 0.035, length(vec2(abs(fp.x) - 0.12, fp.y * 1.6)));
            float mouth = smoothstep(0.05, 0.02, abs(length(vec2(fp.x, (fp.y + 0.12) * 1.4)) - 0.12)) * step(fp.y, -0.18);
            float face = max(eyes, mouth * 0.7);
            alb *= 1.0 - 0.85 * face;
            emis += g_colA * 0.05 * eyes * (0.6 + 0.4 * sin(g_time * 0.7 + tc.z * 20.0)) * (1.0 + 2.0 * g_flash);
        }
    } else if (mat > 6.5) {
        // wrought iron: black, a little rust, glossy
        alb = mix(vec3(0.012), vec3(0.06, 0.025, 0.012), vnoise(pos.xy * 9.0 + pos.z * 7.0) * 0.6);
        spec = 0.5;
    } else if (mat > 5.5) {
        // stone: weathered grey, moss on the up-facing parts, dark cracks
        float nz = vnoise(pos.xy * 7.0 + pos.z * 3.0) * 0.6 + vnoise(pos.xy * 23.0 - pos.z * 9.0) * 0.4;
        alb = vec3(0.05, 0.052, 0.05) * (0.6 + 0.6 * nz);
        float moss = smoothstep(0.1, 0.7, n.y + 0.5 * nz - 0.25) + 0.4 * smoothstep(0.55, 0.8, vnoise(pos.xz * 5.0 + pos.y * 4.0));
        alb = mix(alb, vec3(0.03, 0.06, 0.02), clamp(moss, 0.0, 1.0) * 0.85);
        spec = 0.05;
    } else if (mat > 4.5) {
        // windmill: weathered dark wood, one warm glowing window
        vec3 mp = millAt(pos.z, g_millSide);
        vec3 lp = pos - mp;
        lp.xz = rot2(g_millSide * 0.35) * lp.xz;
        lp.x -= 0.05 * lp.y;
        alb = vec3(0.02, 0.022, 0.02) * (0.8 + 0.4 * vnoise(lp.xy * vec2(2.0, 12.0)));
        spec = 0.03;
        vec2 wq = vec2(lp.x + 0.0, lp.y - 3.4);
        float win = step(abs(wq.x), 0.17) * step(abs(wq.y), 0.25) * step(lp.z, -0.4)
                  * (1.0 - step(abs(wq.x), 0.02)) * (1.0 - step(abs(wq.y), 0.02));          // cross mullion
        float win2 = step(abs(lp.x - 0.3), 0.1) * step(abs(lp.y - 1.4), 0.14) * step(lp.z, -0.4);
        emis = mix(vec3(1.0, 0.65, 0.25), g_colP, 0.3) * (6.0 * win + 2.0 * win2) * g_flicker * u_windmillVis;
        alb *= 1.0 - max(win, win2);
    } else {
        // pumpkin: orange skin, green-brown stem, glowing carved face
        vec3 wpos = uneaseWarp(pos);
        vec2 id = cellId(wpos.xz);
        vec4 pk = pumpkinCell(id);
        vec3 q = wpos - pk.xyz;
        float dp = (id.x + 0.5) * CELL + GOFF.x - pathX((id.y + 0.5) * CELL);
        vec2 faceDir = normalize(vec2(-sign(dp), -1.3));
        bool stem = q.y > 0.19;
        alb = stem ? vec3(0.12, 0.13, 0.04) : g_colP * 0.75;
        spec = 0.35;
        float face = pumpkinFace(q, faceDir);
        vec3 fire = mix(vec3(1.0, 0.55, 0.12), g_colP, 0.4) * 8.0 * pk.w * u_pumpkinGlow;
        emis = fire * face;
        // subsurface: the candle shines through the skin a little
        emis += g_colP * 0.25 * pk.w * u_pumpkinGlow * (1.0 - face) * (stem ? 0.0 : 1.0);
        alb *= 1.0 - face;
    }

    vec3 col = vec3(0.0);
    float dif = max(dot(n, L), 0.0);
    col += alb * moonLightCol() * dif * sh;
    col += alb * (g_colA * 0.12 + g_colS * 0.15) * (0.6 + 0.4 * n.y) * ao * (1.0 + 3.0 * g_flash);   // green sky ambient
    col += alb * vec3(0.02, 0.03, 0.025) * clamp(0.5 - 0.5 * n.y, 0.0, 1.0) * ao;        // ground bounce
    col += alb * pumpkinLights(pos, n) * ao;                                              // warm GI
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 4.0);
    float back = pow(max(dot(rd, L), 0.0), 3.0);
    col += moonLightCol() * 0.5 * fres * (0.06 + 0.5 * back) * (0.35 + 0.65 * ao) * (mat > 2.5 ? 1.0 : 0.4);  // rim
    vec3 hv = normalize(L - rd);
    col += moonLightCol() * spec * pow(max(dot(n, hv), 0.0), 48.0) * sh * 0.6;
    col += emis;
    return col;
}

// Pond surface: gentle ripples plus the beat ring around the pond centre
float waterH(vec2 xz) {
    float a = u_pondRipple;
    float h = a * (0.004 * sin(xz.x * 3.1 + g_time * 1.3) * sin(xz.y * 2.7 - g_time * 1.1)
                 + 0.003 * sin(xz.x * 5.3 - xz.y * 4.1 + g_time * 2.0));
    vec2 c = pondCentre(xz.y);
    float x = length(xz - c) - g_ringR;
    h += g_ringA * exp(-x * x / 0.3) * sin(x * 13.0);
    return h;
}
vec3 waterNormal(vec2 xz) {
    const float e = 0.02;
    float h0 = waterH(xz);
    float hx = waterH(xz + vec2(e, 0.0));
    float hz = waterH(xz + vec2(0.0, e));
    return normalize(vec3(-(hx - h0) / e, 1.0, -(hz - h0) / e));
}

// ---------------------------------------------------------------------------
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    // painterly: warp the sample position along a smooth flow field, giving
    // brushy wobbled edges (a cheap single-pass stand-in for a smudge filter)
    float paint = clamp(u_painterly, 0.0, 1.5);
    vec2 flowP = uv * 7.0;
    vec2 flow = vec2(vnoise(flowP) - 0.5, vnoise(flowP + 17.3) - 0.5);
    float brushN = vnoise(rot2(flow.x * 3.0) * (uv * vec2(260.0, 32.0)));
    uv += paint * (flow * 0.006 + (brushN - 0.5) * 0.0025 * vec2(1.0, 0.3));
    // heat haze shimmer near the horizon + slow downward melt
    float Ue = clamp(u_unease, 0.0, 3.0);
    float hz = exp(-abs(uv.y + 0.02) * 9.0);
    uv += Ue * (0.0022 * hz * vec2(sin(uv.y * 90.0 + iTime * 2.3), 0.5 * cos(uv.x * 70.0 + iTime * 1.9))
              + vec2(0.0, -0.006 * (0.5 + 0.5 * sin(uv.x * 5.0 + iTime * 0.25)) * smoothstep(-0.1, 0.5, uv.y)));
    g_time = iTime;
    Audio A = getAudio();
    float react = clamp(u_audioReact, 0.0, 2.0);
    int Q = clamp(u_quality, 0, 2);
    int SH_STEPS  = (Q == 0) ? 24 : ((Q == 1) ? 40 : 64);
    int AO_TAPS   = (Q == 0) ? 3 : 5;
    int REF_STEPS = (Q == 0) ? 40 : ((Q == 1) ? 64 : 96);
    int REF_SH    = (Q == 2) ? 24 : 0;
    int VOL_N     = (Q == 0) ? 6 : ((Q == 1) ? 10 : 18);
    int VOL_SH    = (Q == 0) ? 4 : ((Q == 1) ? 6 : 10);
    g_levels = clamp(u_branchDetail, 2, 6);

    // ---- palette
    // internal roles: g_colP = warm (pumpkins/window), g_colA = toxic green, g_colS = deep teal/purple
    g_colP = u_accentColor;
    g_colS = u_secondaryColor;
    g_colA = u_primaryColor;

    // ---- beats (stateless, SHADERS.md 3.4)
    float thr = clamp(u_beatThreshold, 0.05, 0.95);
    float drvM = A.Lm, drvB = A.Lb * (1.0 - A.pinned);
    float x  = max(beatAge(drvM, thr, 0.30), beatAge(drvB, thr, 0.30));
    float x2 = max(beatAge(drvM, thr, 0.60), beatAge(drvB, thr, 0.60));   // slower: ripples
    float rmul = min(react, 1.0);
    float snap = smoothstep(0.55, 0.95, x) * smoothstep(0.0, 0.12, 1.0 - x) * rmul;
    g_batX = (x > 0.0 && x < 0.999) ? x : 0.0;
    g_batX *= rmul > 0.0 ? 1.0 : 0.0;
    g_ringR = (1.0 - x2) * 7.0;
    g_ringA = pow(x2, 0.7) * smoothstep(0.0, 0.08, 1.0 - x2) * 0.035 * u_pondRipple * rmul;
    g_flare = 1.6 * snap;
    g_flash = clamp(u_skyFlash, 0.0, 2.0) * snap * snap * 0.6;
    g_bass = A.bass;
    g_writhe = clamp(u_writhe, 0.0, 3.0) * (0.3 + 1.2 * A.bass);         // bass: writhe amplitude
    g_flinch = sin(PI * x) * rmul * 1.4;                                  // beat: trees lurch in
    g_unease = clamp(u_unease, 0.0, 3.0) * (0.6 + 0.8 * A.mids);         // mids: breathing
    g_pulse = clamp(u_unease, 0.0, 3.0) * sin(PI * x) * rmul;            // beat: warp pulse
    g_crawl = 0.12 * clamp(u_unease, 0.0, 3.0);                           // shadows crawl
    // Windmill: speed from smoothed bass/overall. Phase = base rate + a level
    // term whose slope (<= ~1/s from the Att envelope) times 0.4 stays below
    // the base rate, so it never runs backwards; a beat adds a creaky lurch
    // (a forward bump that settles).
    float ws = clamp(u_windmillSpeed, 0.0, 3.0);
    float lvl = 0.5 * (A.Lb + A.Lm);
    float lurch = 0.5 * sin(PI * x) * rmul;
    g_bladeAng = -ws * (iTime * 0.45 + 0.4 * lvl * rmul + lurch);
    // clouds: churn time, bass adds a little (same no-reverse argument)
    g_cloudT = iTime * 0.08 * clamp(u_cloudChurn, 0.0, 3.0) + 0.06 * A.Lb * rmul * clamp(u_cloudChurn, 0.0, 3.0);

    // ---- moon (bass pulses its glow and size)
    g_moonDir = normalize(vec3(-0.25, 0.36, 1.0));
    g_moonU = normalize(cross(vec3(0.0, 1.0, 0.0), g_moonDir));
    g_moonV = cross(g_moonDir, g_moonU);
    g_moonR = 0.085 * clamp(u_moonSize, 0.4, 2.0) * (1.0 + 0.06 * A.bass * A.bass);
    g_moonGlow = clamp(u_moonGlow, 0.0, 3.0) * (1.0 + 0.7 * A.bass * A.bass);
    g_flicker = 1.0 + 0.45 * A.bass * sin(iTime * 23.0) * sin(iTime * 9.7) + 0.08 * sin(iTime * 31.0);
    g_swirl = 1.0 + 2.0 * A.mids;
    g_mids = A.mids;
    g_treb = A.treb;

    // ---- camera: fixed composed framing, optional slow pan/drift (no forward motion)
    float pa = clamp(u_panAmount, 0.0, 2.5);
    float pt = iTime * 0.12 * clamp(u_panSpeed, 0.0, 2.0);
    vec3 ro = vec3(CAM_XZ.x + pa * sin(pt), 2.4 + 0.12 * pa * sin(pt * 0.7 + 1.0), CAM_XZ.y + 0.3 * pa * sin(pt * 0.5));
    float mside; vec3 mpos = millAt(30.0, mside);
    vec3 ta = vec3(0.6 * mpos.x + 0.25 * pa * sin(pt + 0.6), 2.25, 22.0);
    vec3 fwd = normalize(ta - ro);
    vec3 side = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
    vec3 up = cross(fwd, side);
    float roll = (0.012 + 0.02 * g_unease) * sin(iTime * 0.17) + 0.01 * g_pulse;   // wobbling horizon
    side = side * cos(roll) + up * sin(roll);
    up = cross(fwd, side);
    float focal = 1.3 + 0.05 * g_unease * sin(iTime * 0.21) - 0.05 * snap;   // breathing perspective
    vec3 rd = normalize(fwd * focal + side * uv.x + up * uv.y);
    float px = 1.5 / (iResolution.y * focal);                         // pixel size in radians

    float jit = fract(52.9829189 * fract(dot(fragCoord + 5.588238 * float(int(iTime * 60.0) % 64), vec2(0.06711056, 0.00583715))));

    // ---- primary march (+ step-length-weighted pumpkin glow)
    int maxSteps = clamp(u_maxSteps, 32, STEP_CAP);
    float t = 0.05, mat = 0.0, pglow = 0.0;
    vec2 lastCell = vec2(1e9);
    bool hit = false;
    g_rd = rd; g_cellOn = true;
    for (int i = 0; i < STEP_CAP; i++) {
        if (i >= maxSteps) break;
        vec3 p = ro + rd * t;
        vec2 h = map(p);
        if (h.x < 0.0009 * t) { hit = true; mat = h.y; break; }
        if (p.y > CEIL && rd.y > 0.0) { t = MAX_DIST + 1.0; break; }   // escaped above canopy: true miss
        float stepLen = h.x * 0.9;
        // pumpkin bloom: once per visited cell, analytic closest approach of the
        // ray to that cell's lantern (seam-free; v1's per-step glow drew boxes)
        vec2 cid = cellId(p.xz);
        if (cid != lastCell) {
            lastCell = cid;
            vec4 pk = pumpkinCell(cid);
            if (pk.w > 0.0) {
                vec3 oc = pk.xyz + vec3(0.0, 0.02, 0.0) - ro;
                float tc = max(dot(oc, rd), 0.0);
                float d2 = dot(oc, oc) - tc * tc;
                pglow += pk.w * (exp(-d2 * 14.0) * 0.6 + exp(-d2 * 4.0) * 0.06) * exp(-tc * 0.03);
            }
        }
        t += stepLen;
        if (t > MAX_DIST) break;
    }
    if (!hit && t < MAX_DIST) { hit = true; mat = map(ro + rd * t).y; }   // step-exhausted = hit
    g_cellOn = false;
    t = min(t, MAX_DIST);

    vec3 col;
    float shadowDbg = 1.0;
    if (hit) {
        vec3 pos = ro + rd * t;
        if (mat > 1.5 && mat < 2.5) {
            // ---- pond: mirror reflection ray with fresnel and ripples
            vec3 n = waterNormal(pos.xz);
            vec3 rr = reflect(rd, n);
            rr.y = max(rr.y, 0.005);
            rr = normalize(rr);
            float fres = 0.06 + 0.94 * pow(1.0 - max(dot(n, -rd), 0.0), 5.0);
            vec3 rcol;
            float rt = 0.03; float rmat = 0.0; bool rhit = false;
            vec3 rro = pos + vec3(0.0, 0.01, 0.0);
            g_rd = rr; g_cellOn = true;
            for (int i = 0; i < 96; i++) {
                if (i >= REF_STEPS) break;
                vec2 h = map(rro + rr * rt);
                if (h.x < 0.001 * rt) { rhit = true; rmat = h.y; break; }
                if (rro.y + rr.y * rt > CEIL) { rt = 46.0; break; }
                rt += h.x * 0.95;
                if (rt > 45.0) break;
            }
            if (!rhit && rt < 45.0) { rhit = true; rmat = map(rro + rr * rt).y; }
            g_cellOn = false;
            if (rhit && rmat != 2.0) {
                vec3 rp = rro + rr * rt;
                vec3 rn = calcNormal(rp, rt);
                float dummy;
                rcol = shade(rp, rn, rr, rmat, REF_SH, 0, jit, dummy);
                rcol = mix(rcol, (g_colA * 0.06 + g_colS * 0.08), 1.0 - exp(-rt * 0.03 * u_fogDensity));
            } else {
                rcol = sky(rr, true, px * 1.5);
            }
            vec3 deep = g_colS * 0.015 + vec3(0.002, 0.004, 0.004);
            float w = clamp((0.35 + 0.65 * fres) * 1.25 * u_reflection, 0.0, 1.0);   // dark glass, crisp mirror
            col = mix(deep, rcol, w);
            col += pumpkinLights(pos, n) * 0.025;                       // warm glints
        } else {
            vec3 n = calcNormal(pos, t);
            col = shade(pos, n, rd, mat, SH_STEPS, AO_TAPS, jit, shadowDbg);
        }
    } else {
        col = sky(rd, true, px);
    }

    // ---- grass and reed silhouettes: blades drawn on world-fixed z-planes in
    // front of the hit, nearest first (cheap 2.5D layer, occludes properly)
    if (rd.z > 0.05 && u_grassAmount > 0.0) {
        const float GSTEP = 0.55;
        float z0 = floor(ro.z / GSTEP) * GSTEP;
        for (int k = 1; k <= 22; k++) {
            float zk = z0 + float(k) * GSTEP;
            float tk = (zk - ro.z) / rd.z;
            if (tk >= t) break;
            if (tk <= 0.25) continue;
            vec3 pp = ro + rd * tk;
            float gh = terrainH(pp.xz);
            float yl = pp.y - gh;
            if (yl < -0.05 || yl > 1.6) continue;
            float pm = pondMask(pp.xz);
            float edge = smoothstep(0.05, 0.4, pm) * smoothstep(0.98, 0.7, pm);   // reed ring around ponds
            float offAv = smoothstep(1.0, 3.0, abs(pp.x - pathX(pp.z)));
            float layer = floor(zk / GSTEP + 0.5);
            float sway = 0.12 * sin(g_time * 0.9 + pp.x * 0.7 + layer) + 0.1 * g_bass * sin(g_time * 3.0 + pp.x);
            float u = pp.x * 5.0 + layer * 3.17;
            float bid = floor(u);
            float hb = hash12(vec2(bid, layer));
            float keep = step(hb, (0.4 * offAv + 0.9 * edge) * clamp(u_grassAmount, 0.0, 2.0));
            if (keep < 0.5) continue;
            float hgt = mix(0.08, 0.3, hash12(vec2(layer, bid) + 3.1)) * (1.0 + 3.0 * edge) * (0.6 + 0.6 * offAv) * smoothstep(0.5, 6.0, tk);
            float yn = yl / hgt;
            float lean = sway * yn * yn + (hb - 0.5) * 0.4 * yn;
            float fx = (fract(u) - 0.5) / 5.0 - lean * hgt * 0.5;
            float wdt = (0.022 + 0.01 * edge) * (1.0 - yn);
            float aa = px * tk * 1.2;
            float cov = smoothstep(aa, -aa, abs(fx) - wdt) * step(yn, 1.0);
            // reed heads (cattails)
            float head = edge * step(0.6, hash12(vec2(bid, layer + 9.0)));
            cov = max(cov, head * smoothstep(aa, -aa, length(vec2(fx + lean * hgt * 0.5, (yl - hgt * 0.92) * 0.35)) - 0.03));
            if (cov > 0.01) {
                vec3 gc = vec3(0.016, 0.012, 0.007) + pumpkinLights(pp, vec3(0.0, 0.0, -1.0)) * 0.006 * yn
                        + g_colA * 0.03 * yn;                                   // faint green rim at tips
                col = mix(col, gc, cov);
                if (cov > 0.5) { t = tk; break; }
            }
        }
    }

    // ---- volumetric ground fog: single scattering with tree-shadowed god rays
    {
        float tEnd = min(t, 38.0);
        float dt = tEnd / float(VOL_N);
        float T = 1.0;
        vec3 scat = vec3(0.0);
        float cosTh = dot(rd, g_moonDir);
        float phase = 0.15 + 1.8 * pow(max(cosTh, 0.0), 6.0) + 0.6 * pow(max(cosTh, 0.0), 40.0);
        vec3 amb = (g_colA * 0.012 + g_colS * 0.02) * (1.0 + 4.0 * g_flash);
        float fd = clamp(u_fogDensity, 0.0, 3.0);
        for (int i = 0; i < 20; i++) {
            if (i >= VOL_N) break;
            float ts = (float(i) + jit) * dt;
            vec3 sp = ro + rd * ts;
            float sw = g_time * 0.12 * g_swirl;
            float nse = 0.55 + 0.45 * sin(sp.x * 0.45 + sin(sp.z * 0.3 + sw) * 2.0 + sw)
                                  * sin(sp.z * 0.35 - sw * 0.8 + sin(sp.x * 0.25 - sw));
            float dens = fd * (0.045 * exp(-max(sp.y, 0.0) * 1.1) + 0.006) * nse;
            // shadow toward the moon, trees only, few coarse steps
            float vis = 1.0;
            float st = 0.2 + 0.3 * jit;
            g_rd = g_moonDir; g_cellOn = true;
            int lv = g_levels; g_levels = min(lv, 4);   // god-ray shadows: coarser crown
            for (int j = 0; j < 10; j++) {
                if (j >= VOL_SH) break;
                float h = treesProxy(sp + g_moonDir * st);
                vis = min(vis, 6.0 * g_trueD / st);
                st += clamp(h, 0.3, 2.0);
                if (sp.y + g_moonDir.y * st > CEIL) break;
            }
            g_cellOn = false; g_levels = lv;
            vis = clamp(vis, 0.0, 1.0);
            vec3 Ls = moonLightCol() * vis * phase * clamp(u_godRays, 0.0, 3.0) * 0.35 + amb;
            scat += T * dens * Ls * dt;
            T *= exp(-dens * dt);
        }
        col = col * T + scat;
    }

    // ---- pumpkin bloom (step-length-weighted proximity glow)
    col += mix(g_colP, vec3(1.0, 0.6, 0.2), 0.4) * pglow * 0.55 * u_pumpkinGlow * g_flicker * (1.0 + g_flare);

    // ---- fireflies (treble twinkle) and wisps (mids glow), world-fixed layers
    if (rd.z > 0.05) {
        float amt = clamp(u_fireflyAmount, 0.0, 3.0);
        const float LSTEP = 1.6;
        float z0 = floor(ro.z / LSTEP) * LSTEP;
        vec3 acc = vec3(0.0);
        for (int k = 1; k <= 10; k++) {
            float zk = z0 + float(k) * LSTEP;
            float tk = (zk - ro.z) / rd.z;
            if (tk <= 0.3 || tk >= t) continue;
            vec3 pp = ro + rd * tk;
            float layer = floor(zk / LSTEP + 0.5);
            vec2 dr = vec2(0.25 * sin(g_time * 0.4 + layer), 0.15 * sin(g_time * 0.6 + layer * 1.7));
            vec2 g = (pp.xy + dr) / 0.7;
            vec2 id = floor(g);
            vec2 h = hash22(id + layer * 13.1);
            float exist = step(hash12(id * 1.7 + layer), 0.07 * amt) * step(0.25, pp.y) * step(pp.y, 3.2);
            vec2 f = (fract(g) - (0.2 + 0.6 * h)) * 0.7;
            float r2 = dot(f, f);
            float blink = pow(0.5 + 0.5 * sin(g_time * (1.2 + 2.0 * h.x) + h.y * 40.0), 3.0);
            blink *= 0.6 + 1.6 * g_treb * (0.5 + 0.5 * sin(g_time * 17.0 + h.x * 50.0));
            float spr = exist * (exp(-r2 * 9000.0 / (1.0 + tk * 0.15)) * 3.0 + exp(-r2 * 260.0) * 0.25) * blink;
            acc += g_colA * spr * exp(-tk * 0.04) * smoothstep(0.3, 1.5, tk);
        }
        // wisps: a few soft drifting will-o'-the-wisps
        for (int k = 1; k <= 4; k++) {
            float zk = (floor(ro.z / 6.0) + float(k)) * 6.0;
            float tk = (zk - ro.z) / rd.z;
            if (tk <= 0.5 || tk >= t) continue;
            vec3 pp = ro + rd * tk;
            float hk = hash12(vec2(zk, 4.4));
            vec2 c = vec2(pathX(zk) + (hk - 0.5) * 7.0 + 0.6 * sin(g_time * 0.5 + hk * 9.0),
                          0.8 + 0.4 * sin(g_time * 0.7 + hk * 20.0));
            float r2 = dot(pp.xy - c, pp.xy - c);
            vec3 wc = mix(g_colA, vec3(0.6, 1.0, 0.9), 0.3);
            acc += wc * (exp(-r2 * 30.0) * 1.2 + exp(-r2 * 3.0) * 0.08) * (0.4 + 1.4 * g_mids) * step(hk, 0.75) * amt * 0.5;
        }
        col += acc;
    }

    // ---- grade
    if (u_debugView == 2) col = vec3(shadowDbg);
    col *= clamp(u_exposure, 0.3, 3.0) * 1.25 * (1.0 + 0.25 * A.level);
    col = acesFilm(max(col, 0.0));
    col = pow(col, vec3(1.0 / 2.2));
    col *= mix(0.62, 1.0, smoothstep(1.2, 0.3, length(uv)));        // vignette
    float gr = hash12(fragCoord + fract(iTime * 7.31) * 517.0) - 0.5;
    col += gr * 0.05 * clamp(u_grain, 0.0, 2.0) * (1.0 - 0.5 * col); // film grain

    if (u_debugView == 1) {
        vec2 pxc = fragCoord;
        vec3 m = vec3(0.0);
        m += vec3(1.0, 0.3, 0.2) * meterBar(pxc, 20.0,  sqrt(iAudioBands.x));
        m += vec3(0.3, 1.0, 0.3) * meterBar(pxc, 38.0,  sqrt(iAudioBands.y));
        m += vec3(0.3, 0.5, 1.0) * meterBar(pxc, 56.0,  sqrt(iAudioBands.z));
        m += vec3(1.0, 0.3, 0.2) * meterBar(pxc, 92.0,  A.Lb);
        m += vec3(0.3, 1.0, 0.3) * meterBar(pxc, 110.0, A.Lm);
        m += vec3(0.3, 0.5, 1.0) * meterBar(pxc, 128.0, A.Lt);
        m += vec3(1.0, 1.0, 0.0) * meterBar(pxc, 146.0, A.pinned);
        m += vec3(1.0)           * meterBar(pxc, 164.0, x);
        col = max(col * (1.0 - 0.7 * step(pxc.x, 190.0) * step(pxc.y, 190.0)), m);
    }
    FragColor = vec4(col, 1.0);
}
