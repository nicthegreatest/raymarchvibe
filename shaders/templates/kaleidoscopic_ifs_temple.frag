#version 330 core
out vec4 FragColor;

// Kaleidoscopic IFS Temple
// A Menger-style kaleidoscopic IFS (abs folds + sort folds + a rotation every
// iteration, then scale/offset, Knighty/Syntopia style) clipped by a stepped
// ziggurat. A ring nave, four aisles and a central sanctum are carved out of
// it, so the folds can reshape the walls, arches and coffered ceiling while
// the camera's path always stays clear. An altar in the sanctum holds a glowing
// core that lights the temple and throws shafts through the gaps.
//
// Audio (iAudioBandsAtt). App bands are FFT means / 32: bass is often pinned at
// 1.0 on loud masters, mids ~0.03-0.35, treble ~0.002-0.04. Each band is mapped
// to 0..1 in a log window (Audio Sensitivity shifts it in octaves). Pinned bass
// carries no beat, so it falls back to mids.
//   bass   -> fold rotation A (about y): walls and arches swing
//   mids   -> fold rotation B (about x): ceiling and window tracery tilt
//   treble -> fold rotation C (about z, small): fine detail shimmer
//   beats  -> a light pulse leaves the core and sweeps out through the temple.
//            The envelope decays at a steady rate in log units, so the
//            distance since the last hit maps to pulse radius: no state needed.
//   depth glow: coarse / middle / fine fractal levels glow with bass / mids / treble
//   overall -> exposure
// The app scales iTime by its own speed control, so nothing here multiplies by
// a speed uniform. Camera Speed only paces the camera path.

uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;
uniform vec4 iAudioBandsAtt;

uniform vec3 u_primaryColor = vec3(0.85, 0.55, 0.25);   // {"widget":"color", "palette":true, "label":"Primary Color"}
uniform vec3 u_secondaryColor = vec3(0.20, 0.45, 0.85); // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 u_accentColor = vec3(1.0, 0.35, 0.65);     // {"widget":"color", "palette":true, "label":"Accent Color"}

uniform float u_camSpeed = 1.0;      // {"widget":"slider", "label":"Camera Speed", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform int u_iterations = 5;        // {"widget":"slider", "label":"Iterations", "min":2, "max":7}
uniform float u_ifsScale = 3.0;      // {"widget":"slider", "label":"IFS Scale", "min":2.4, "max":3.4, "step":0.01, "smooth":true}
uniform float u_offsetXY = 1.0;      // {"widget":"slider", "label":"Fold Offset XY", "min":0.6, "max":1.4, "step":0.01, "smooth":true}
uniform float u_offsetZ = 1.0;       // {"widget":"slider", "label":"Fold Offset Z", "min":0.4, "max":1.6, "step":0.01, "smooth":true}
uniform float u_foldAngleA = 0.12;   // {"widget":"slider", "label":"Fold Angle A (base)", "min":-0.6, "max":0.6, "step":0.005, "smooth":true}
uniform float u_foldAngleB = 0.05;   // {"widget":"slider", "label":"Fold Angle B (base)", "min":-0.6, "max":0.6, "step":0.005, "smooth":true}
uniform float u_bassFold = 1.0;      // {"widget":"slider", "label":"Bass Fold Amount", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_midFold = 1.0;       // {"widget":"slider", "label":"Mid Fold Amount", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_trebleFold = 1.0;    // {"widget":"slider", "label":"Treble Fold Amount", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_audioReact = 1.0;    // {"widget":"slider", "label":"Audio Reactivity", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_audioSens = 0.0;     // {"widget":"slider", "label":"Audio Sensitivity (octaves)", "min":-3.0, "max":3.0, "step":0.05, "smooth":true}
uniform float u_beatThreshold = 0.45; // {"widget":"slider", "label":"Beat Threshold (hit level)", "min":0.1, "max":0.9, "step":0.01}
uniform float u_coreGlow = 1.0;      // {"widget":"slider", "label":"Core Glow", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_pulseIntensity = 1.0; // {"widget":"slider", "label":"Pulse Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_depthGlow = 1.0;     // {"widget":"slider", "label":"Depth Glow", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_fog = 1.0;           // {"widget":"slider", "label":"Fog", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_exposure = 1.0;      // {"widget":"slider", "label":"Exposure", "min":0.3, "max":3.0, "step":0.01, "smooth":true}
uniform float u_colorCycle = 0.0;    // {"widget":"slider", "label":"Colour Cycle", "min":0.0, "max":1.0, "step":0.01, "smooth":true}
uniform int u_maxSteps = 110;        // {"widget":"slider", "label":"Max Steps", "min":48, "max":200}
uniform int u_debugView = 0;         // {"widget":"slider", "label":"Debug View (0 off, 1 meters, 2 depth)", "min":0, "max":2}

const int MAX_ITER = 7;
const int STEP_CAP = 200;
const float MAX_DIST = 30.0;

// Temple layout (world units)
const vec3  KCENTER = vec3(0.0, 3.6, 0.0);  // centre of the IFS cube
const float KSIZE   = 6.0;                  // half size of the IFS cube
const float ZIG_BASE = 6.4;                 // ziggurat base half width
const float ZIG_STEP = 1.0;                 // tier inset
const float ZIG_H    = 1.15;                // tier height
const float NAVE_R = 3.0;                   // camera orbit radius (middle of the ring nave)
const float NAVE_W = 0.8;                   // nave half width: the hall is open to r = 3.8
const float NAVE_H = 2.4;                   // hall height; the IFS ceiling starts here
const float OCULUS_R = 0.9;                 // opening to the sky above the core
const float COL_R = 1.95;                   // inner colonnade radius
const float COL_N = 12.0;                   // columns in the colonnade
const vec3  CORE = vec3(0.0, 1.35, 0.0);
const float CORE_R = 0.26;

// Per-frame state, set in main()
mat3 g_rot;
float g_scale;
vec3 g_offset;
float g_coreR;

// --------------------------------------------------------------------------
// Utilities
// --------------------------------------------------------------------------
float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

mat3 rotX(float a) { float c = cos(a), s = sin(a); return mat3(1.0, 0.0, 0.0, 0.0, c, s, 0.0, -s, c); }
mat3 rotY(float a) { float c = cos(a), s = sin(a); return mat3(c, 0.0, -s, 0.0, 1.0, 0.0, s, 0.0, c); }
mat3 rotZ(float a) { float c = cos(a), s = sin(a); return mat3(c, s, 0.0, -s, c, 0.0, 0.0, 0.0, 1.0); }

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// Hue rotation about the grey axis (Rodrigues)
vec3 hueShift(vec3 c, float a) {
    const vec3 k = vec3(0.57735);
    float ca = cos(a);
    return c * ca + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - ca);
}

vec3 acesFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// --------------------------------------------------------------------------
// Audio band conditioning (shared with Mandelbox Dive v2)
// --------------------------------------------------------------------------
// Log-domain window: v in [lo, hi] -> 0..1. Sensitivity (octaves) shifts the
// window down, so +1 treats a signal twice as loud.
float bandLevel(float v, float lo, float hi, float sens) {
    float a = log2(lo) - sens;
    float b = log2(hi) - sens;
    return clamp((log2(max(v, 1e-6)) - a) / (b - a), 0.0, 1.0);
}
// Windows chosen from measurements of real music through the app's pipeline
const vec2 WIN_BASS = vec2(0.01, 0.5);
const vec2 WIN_MID  = vec2(0.005, 0.5);
const vec2 WIN_TREB = vec2(0.0005, 0.2);

// --------------------------------------------------------------------------
// Core pulses (stateless, beat-synced): radius and intensity set in main()
// --------------------------------------------------------------------------
const int PULSES = 2;
float g_pR[PULSES];
float g_pI[PULSES];
vec3  g_pC[PULSES];
float g_pW;

// Light from all pulse fronts at distance dc from the core
vec3 pulseLight(float dc) {
    vec3 acc = vec3(0.0);
    for (int k = 0; k < PULSES; k++) {
        if (g_pI[k] <= 0.001) continue;
        float x = dc - g_pR[k];
        float front = exp(-x * x / (g_pW * g_pW));
        float after = (x < 0.0) ? exp(x / (g_pW * 4.0)) * 0.15 : 0.0;  // afterglow inside the front
        acc += g_pC[k] * (front * 1.6 + after) * g_pI[k];
    }
    return acc;
}

float meterBar(vec2 px, float x0, float v) {
    float inX = step(x0, px.x) * step(px.x, x0 + 14.0);
    float inY = step(20.0, px.y) * step(px.y, 20.0 + 160.0 * clamp(v, 0.0, 1.0));
    return inX * inY;
}

// --------------------------------------------------------------------------
// Scene
// --------------------------------------------------------------------------
// One KIFS iteration: abs fold, sort folds, rotation, scale about the offset.
void kifsStep(inout vec3 p) {
    p = abs(p);
    if (p.x < p.y) p.xy = p.yx;
    if (p.x < p.z) p.xz = p.zx;
    if (p.y < p.z) p.yz = p.zy;
    p = g_rot * p;
    p = p * g_scale - g_offset * (g_scale - 1.0);
    float h = 0.5 * g_offset.z * (g_scale - 1.0);
    if (p.z < -h) p.z += g_offset.z * (g_scale - 1.0);
}

float kifs(vec3 pos) {
    vec3 p = (pos - KCENTER) / KSIZE;
    float s = 1.0;
    for (int i = 0; i < MAX_ITER; i++) {
        if (i >= u_iterations) break;
        kifsStep(p);
        s *= g_scale;
    }
    return sdBox(p, vec3(1.0)) / s * KSIZE;
}

// Fractal level of a surface point: the first iteration whose box face it
// lies on. Outer walls come from level 0, window jambs from deep levels.
// 0 = coarse (bass), 1 = fine (treble). Faces cut by the carve or the
// ziggurat sit inside the IFS solid and count as coarse.
float kifsDepth(vec3 pos, float tol) {
    vec3 p = (pos - KCENTER) / KSIZE;
    float s = 1.0;
    float n = float(max(u_iterations, 1));
    if (abs(sdBox(p, vec3(1.0))) * KSIZE < tol) return 0.0;
    for (int i = 0; i < MAX_ITER; i++) {
        if (i >= u_iterations) break;
        kifsStep(p);
        s *= g_scale;
        float di = sdBox(p, vec3(1.0)) / s * KSIZE;
        if (abs(di) < tol) return float(i + 1) / n;
        if (i + 1 >= u_iterations && di < -tol) return 0.0;
    }
    return 0.5;   // rotated folds break exact nesting: unmatched faces count as middle
}

float ziggurat(vec3 p) {
    float d = 1e5;
    for (int k = 0; k < 5; k++) {
        float fk = float(k);
        float hw = ZIG_BASE - fk * ZIG_STEP;
        float top = ZIG_H * (fk + 1.0);
        // tiers start below the floor so no face is coplanar with it
        d = min(d, sdBox(p - vec3(0.0, (top - 0.5) * 0.5, 0.0), vec3(hw, (top + 0.5) * 0.5, hw)));
    }
    return d;
}

// Space that is always kept open: the round hall (nave + sanctum) under the
// IFS ceiling, and an oculus above the core. Carving starts below the floor so
// no carved face is coplanar with it. Negative inside.
float carve(vec3 p) {
    float r = length(p.xz);
    float hall = max(r - (NAVE_R + NAVE_W), max(-p.y - 0.5, p.y - NAVE_H));
    return min(hall, r - OCULUS_R);
}

// Inner colonnade: square columns with base and capital, polar-repeated.
float columns(vec3 p) {
    float r = length(p.xz);
    float sector = 6.2831853 / COL_N;
    float a = mod(atan(p.z, p.x + 1e-6) + 0.5 * sector, sector) - 0.5 * sector;
    vec3 q = vec3(r * cos(a) - COL_R, p.y, r * sin(a));
    float shaft = sdBox(q - vec3(0.0, NAVE_H * 0.5, 0.0), vec3(0.10, NAVE_H * 0.5, 0.10)) - 0.02;
    float base = sdBox(q - vec3(0.0, 0.10, 0.0), vec3(0.20, 0.10, 0.20));
    float cap  = sdBox(q - vec3(0.0, NAVE_H - 0.12, 0.0), vec3(0.21, 0.12, 0.21));
    // architrave ring tying the capitals together
    float ring = max(abs(r - COL_R) - 0.2, abs(p.y - NAVE_H + 0.05) - 0.09);
    return min(min(shaft, base), min(cap, ring));
}

float altar(vec3 p) {
    float d = sdBox(p - vec3(0.0, 0.12, 0.0), vec3(0.75, 0.12, 0.75));
    d = min(d, sdBox(p - vec3(0.0, 0.36, 0.0), vec3(0.52, 0.12, 0.52)));
    d = min(d, sdBox(p - vec3(0.0, 0.60, 0.0), vec3(0.32, 0.12, 0.32)));
    // four short pillars holding the core
    vec2 q = abs(p.xz) - 0.2;
    d = min(d, max(length(q) - 0.045, abs(p.y - 0.85) - 0.25));
    return d;
}

// Everything except the core, which is the light source. Shadow and shaft
// visibility rays use this so the core does not occlude its own light.
vec2 mapScene(vec3 p) {
    float d = max(kifs(p), ziggurat(p));
    d = max(d, -carve(p));
    vec2 res = vec2(d, 1.0);

    float fl = p.y;
    if (fl < res.x) res = vec2(fl, 2.0);

    float al = min(altar(p), columns(p));
    if (al < res.x) res = vec2(al, 4.0);

    return res;
}

// Returns vec2(distance, materialID): 1 IFS stone, 2 floor, 3 core, 4 altar and columns
vec2 map(vec3 p) {
    vec2 res = mapScene(p);
    float co = length(p - CORE) - g_coreR;
    if (co < res.x) res = vec2(co, 3.0);
    return res;
}

vec3 calcNormal(vec3 p, float t) {
    float e = 0.0006 * clamp(t, 1.0, 8.0);
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * e).x + k.yyx * map(p + k.yyx * e).x +
                     k.yxy * map(p + k.yxy * e).x + k.xxx * map(p + k.xxx * e).x);
}

float calcAO(vec3 p, vec3 n) {
    float occ = 0.0, sca = 1.0;
    for (int i = 0; i < 4; i++) {
        float h = 0.03 + 0.12 * float(i);
        occ += (h - map(p + n * h).x) * sca;
        sca *= 0.7;
    }
    return clamp(1.0 - 2.2 * occ, 0.0, 1.0);
}

// Soft shadow toward the core
// Improved soft shadow (triangulates with the previous step). Fine minimum
// steps avoid the contour banding plain soft shadows show on the floor.
float coreShadow(vec3 p, vec3 l, float maxT) {
    float res = 1.0, t = 0.04, ph = 1e10;
    for (int i = 0; i < 48; i++) {
        if (t >= maxT) break;
        float h = mapScene(p + l * t).x;
        float y = h * h / (2.0 * ph);
        float d = sqrt(max(h * h - y * y, 0.0));
        res = min(res, 16.0 * d / max(t - y, 1e-4));
        if (res < 0.01) break;
        ph = h;
        t += clamp(h, 0.01, 0.12);
    }
    res = clamp(res, 0.0, 1.0);
    return res * res * (3.0 - 2.0 * res);
}

// Visibility of the core from a point in the air (cheap 3-tap test for shafts)
float coreVis(vec3 p) {
    vec3 v = CORE - p;
    float vis = 1.0;
    for (int i = 1; i <= 3; i++) {
        float h = mapScene(p + v * (float(i) * 0.24)).x;
        vis *= clamp(h * 12.0, 0.0, 1.0);
    }
    return vis;
}

vec3 sky(vec3 rd, vec3 colSec) {
    float g = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 c = mix(vec3(0.004, 0.004, 0.008), colSec * 0.05, g);
    float band = exp(-abs(rd.y - 0.35 + 0.08 * sin(atan(rd.z, rd.x + 1e-6) * 3.0)) * 10.0);
    return c + colSec * band * 0.025;
}

// --------------------------------------------------------------------------
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;

    // ---- palette (optional slow hue cycle)
    float hue = iTime * 0.08 * u_colorCycle;
    vec3 colP = hueShift(u_primaryColor, hue);
    vec3 colS = hueShift(u_secondaryColor, hue);
    vec3 colA = hueShift(u_accentColor, hue);
    vec3 coreCol = mix(colP, vec3(1.0, 0.95, 0.85), 0.35);

    // ---- audio conditioning
    float react = clamp(u_audioReact, 0.0, 2.0);
    float sens = clamp(u_audioSens, -3.0, 3.0);
    float bassPinned = smoothstep(0.85, 0.98, iAudioBandsAtt.x);
    float Lb  = bandLevel(iAudioBandsAtt.x, WIN_BASS.x, WIN_BASS.y, sens);
    float Lm  = bandLevel(iAudioBandsAtt.y, WIN_MID.x,  WIN_MID.y,  sens - 1.5 * bassPinned);
    float Lt  = bandLevel(iAudioBandsAtt.z, WIN_TREB.x, WIN_TREB.y, sens - 0.75 * bassPinned);
    float LtR = bandLevel(iAudioBands.z,    WIN_TREB.x, WIN_TREB.y, sens - 0.75 * bassPinned);
    float LbEff = mix(Lb, Lm, bassPinned);           // pinned bass carries no beat
    float bass = LbEff * react;
    float mids = Lm * react;
    float treb = max(Lt, LtR * 0.8) * react;
    float level = (LbEff + Lm + Lt) / 3.0 * react;

    // ---- fractal parameters. Squared drivers keep quiet passages calm.
    float aA = u_foldAngleA + 0.12 * min(bass * bass, 2.25) * u_bassFold;
    float aB = u_foldAngleB + 0.09 * min(mids * mids, 2.25) * u_midFold;
    float aC = 0.05 * min(treb * treb, 2.25) * u_trebleFold;
    g_rot = rotY(aA) * rotX(aB) * rotZ(aC);
    g_scale = clamp(u_ifsScale, 2.2, 3.6);
    g_offset = vec3(u_offsetXY, u_offsetXY, u_offsetZ);
    g_coreR = CORE_R * (1.0 + 0.25 * min(level, 1.5));

    // ---- beat-synced pulses from the core
    float thr = clamp(u_beatThreshold, 0.05, 0.95);
    const float SPAN = 0.25;                  // conditioned-level units a pulse lives for
    const float MAX_PR = 9.0;                 // core to the outer walls
    float pulseAmt = clamp(u_pulseIntensity, 0.0, 3.0) * min(react, 1.5);
    g_pW = 0.45;
    float drv[PULSES];
    drv[0] = Lm;
    drv[1] = Lb * (1.0 - bassPinned);
    g_pC[0] = mix(colA, vec3(1.0), 0.3);
    g_pC[1] = mix(colP, vec3(1.0), 0.3);
    for (int k = 0; k < PULSES; k++) {
        float x = clamp((drv[k] - thr) / SPAN, 0.0, 1.0);
        g_pR[k] = (1.0 - x) * MAX_PR;
        g_pI[k] = pow(x, 0.6) * smoothstep(0.0, 0.06, 1.0 - x) * pulseAmt;
    }

    // ---- camera: orbit the ring nave, drifting in and out, looking toward the sanctum
    float T = iTime * clamp(u_camSpeed, 0.0, 3.0);
    float ang = T * 0.055;
    float rad = NAVE_R + 0.32 * sin(T * 0.021);
    vec3 ro = vec3(cos(ang) * rad, 0.72 + 0.22 * sin(T * 0.033), sin(ang) * rad);
    vec3 tangent = vec3(-sin(ang), 0.0, cos(ang));
    vec3 inward = -vec3(cos(ang), 0.0, sin(ang));
    float yaw = 0.55 + 0.35 * sin(T * 0.017);
    vec3 fwd = normalize(tangent * cos(yaw) + inward * sin(yaw) + vec3(0.0, 0.10 + 0.08 * sin(T * 0.027), 0.0));
    vec3 side = normalize(cross(fwd, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(side, fwd);
    vec3 rd = normalize(fwd * 1.0 + side * uv.x + up * uv.y);

    // ---- march
    int maxSteps = clamp(u_maxSteps, 32, STEP_CAP);
    float t = 0.0;
    float mat = 0.0;
    bool hit = false;
    for (int i = 0; i < STEP_CAP; i++) {
        if (i >= maxSteps) break;
        vec2 h = map(ro + rd * t);
        if (h.x < 0.0004 * max(t, 1.0)) { hit = true; mat = h.y; break; }
        t += h.x * 0.9;
        if (t > MAX_DIST) break;
    }
    // A ray that runs out of steps inside the temple is grazing a surface.
    if (!hit && t < MAX_DIST) { hit = true; mat = map(ro + rd * t).y; }

    vec3 col;
    vec3 fogCol = colS * 0.012 + vec3(0.002);
    float depthDbg = -1.0;                  // -1: not IFS stone
    vec3 pulseDbg = vec3(0.0);

    if (hit) {
        vec3 pos = ro + rd * t;
        vec3 n = calcNormal(pos, t);
        vec3 lv = CORE - pos;
        float ld = length(lv);
        vec3 l = lv / ld;

        if (mat > 2.5 && mat < 3.5) {
            // the core itself
            float fr = pow(1.0 - max(dot(n, -rd), 0.0), 2.0);
            // swirling bands on the core so it reads as an object, not a blown-out disc
            float sw = 0.5 + 0.5 * sin(dot(n, vec3(7.0, 11.0, 5.0)) + iTime * 1.3);
            col = coreCol * (0.9 + 0.5 * sw + 1.2 * level) * u_coreGlow + colA * fr * 1.5 * u_coreGlow;
        } else {
            float ao = calcAO(pos, n);
            float sh = coreShadow(pos + n * 0.015, l, ld - g_coreR - 0.02);
            float diff = max(dot(n, l), 0.0);
            float atten = 1.0 / (1.0 + 0.35 * ld * ld);

            vec3 alb;
            float depth = 0.0;
            if (mat < 1.5) {
                depth = kifsDepth(pos, 0.003 * max(t, 1.0));
                depthDbg = depth;
                alb = mix(vec3(0.20, 0.19, 0.18), colP * 0.35 + 0.06, 0.45);
                alb *= 0.75 + 0.35 * depth;
            } else if (mat < 2.5) {
                vec2 g = abs(fract(pos.xz * 1.25) - 0.5);
                float groove = smoothstep(0.47, 0.5, max(g.x, g.y)) * exp(-t * 0.25);
                alb = vec3(0.12, 0.11, 0.11) * (1.0 - 0.6 * groove);
                // concentric inlay rings around the sanctum
                float rr = length(pos.xz);
                alb += colA * 0.06 * smoothstep(0.03, 0.0, abs(fract(rr * 0.5) - 0.5) - 0.47) * exp(-t * 0.2);
            } else {
                alb = mix(vec3(0.25, 0.22, 0.2), colP * 0.5, 0.5);
            }

            vec3 lit = vec3(0.0);
            // core light
            lit += alb * coreCol * diff * sh * atten * 6.0 * u_coreGlow;
            // cool sky fill from above
            lit += alb * colS * 0.06 * ao * (0.4 + 0.6 * clamp(n.y * 0.5 + 0.5, 0.0, 1.0));
            // rim light
            float fres = pow(1.0 - clamp(dot(n, -rd), 0.0, 1.0), 3.0);
            lit += colS * fres * 0.10 * ao;
            // specular glint from the core
            float spec = pow(max(dot(reflect(-l, n), -rd), 0.0), 32.0);
            lit += coreCol * spec * sh * atten * 1.5 * u_coreGlow;

            // depth glow per band, on the fractal stone only
            if (mat < 1.5) {
                float wB = 1.0 - smoothstep(0.30, 0.45, depth);
                float wT = smoothstep(0.70, 0.85, depth);
                float wM = max(1.0 - wB - wT, 0.0);
                float rimW = 0.35 + 0.65 * pow(1.0 - clamp(dot(n, -rd), 0.0, 1.0), 1.5);
                vec3 dg = colP * wB * (0.03 + 0.55 * bass * bass)
                        + colS * wM * (0.03 + 0.65 * mids * mids)
                        + colA * wT * (0.03 + 0.90 * treb * treb);
                lit += dg * rimW * ao * u_depthGlow;
            }

            // core pulse washing over the surfaces
            vec3 pl = pulseLight(length(pos - CORE));
            pulseDbg = pl;
            lit += pl * (alb * 1.4 + 0.06) * (0.4 + 0.6 * ao) * (0.2 + 0.8 * max(dot(n, l), 0.0));

            col = lit;
        }
        float fogAmt = 1.0 - exp(-t * 0.045 * u_fog);
        col = mix(col, fogCol, fogAmt);
    } else {
        col = sky(rd, colS);
    }

    // ---- volumetric shafts and pulse haze from the core
    {
        const int SHAFT_N = 8;
        float tEnd = min(t, 14.0);
        float dt = tEnd / float(SHAFT_N);
        float jit = hash12(fragCoord + fract(iTime) * 37.0);
        vec3 acc = vec3(0.0);
        for (int i = 0; i < SHAFT_N; i++) {
            float ts = (float(i) + jit) * dt;
            vec3 sp = ro + rd * ts;
            float dc = length(sp - CORE);
            float vis = coreVis(sp);
            float phase = 0.6 + 0.4 * max(dot(rd, normalize(CORE - sp)), 0.0);
            acc += coreCol * vis * phase / (1.0 + 3.0 * dc * dc) * 0.9 * u_coreGlow;
            acc += pulseLight(dc) * 0.05 * (0.3 + 0.7 * vis);
        }
        col += acc * dt * 0.12 * u_fog;
    }

    // ---- analytic halo around the core (only if the core is in front of the hit)
    {
        float tc = max(dot(CORE - ro, rd), 0.0);
        if (tc < t || !hit) {
            float dmin = length(ro + rd * tc - CORE);
            col += coreCol * u_coreGlow * (0.06 / (1.0 + dmin * dmin * 18.0) + 0.012 / (1.0 + dmin * dmin * 1.5)) * (1.0 + level);
        }
    }

    // ---- tonemap
    col *= u_exposure * 1.6 * (1.0 + 0.3 * level);
    col = acesFilm(max(col, 0.0));
    col = pow(col, vec3(1.0 / 2.2));
    col *= mix(0.75, 1.0, smoothstep(1.2, 0.3, length(uv)));

    // ---- debug views
    if (u_debugView == 2 && hit) {
        float dd = depthDbg;
        vec3 cls = (dd < 0.0) ? vec3(0.25) :
                   ((dd < 0.375) ? vec3(1.0, 0.0, 0.0) : ((dd < 0.775) ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0)));
        col = cls * 0.8 + min(pulseDbg, vec3(1.0)) * 0.6;
    }
    if (u_debugView == 1) {
        vec2 px = fragCoord;
        float panel = step(px.x, 236.0) * step(px.y, 190.0) * step(10.0, px.x) * step(10.0, px.y);
        col = mix(col, vec3(0.02), panel * 0.8);
        vec3 m = vec3(0.0);
        m += vec3(1.0, 0.3, 0.2) * meterBar(px, 20.0,  sqrt(iAudioBands.x));
        m += vec3(0.3, 1.0, 0.3) * meterBar(px, 38.0,  sqrt(iAudioBands.y));
        m += vec3(0.3, 0.5, 1.0) * meterBar(px, 56.0,  sqrt(iAudioBands.z));
        m += vec3(0.8)           * meterBar(px, 74.0,  sqrt(iAudioBands.w));
        m += vec3(1.0, 0.3, 0.2) * meterBar(px, 110.0, Lb);
        m += vec3(0.3, 1.0, 0.3) * meterBar(px, 128.0, Lm);
        m += vec3(0.3, 0.5, 1.0) * meterBar(px, 146.0, Lt);
        m += vec3(1.0, 1.0, 0.0) * meterBar(px, 164.0, bassPinned);
        m += vec3(1.0)           * meterBar(px, 200.0, g_pI[0] / max(pulseAmt, 1e-3));
        m += vec3(1.0)           * meterBar(px, 218.0, g_pI[1] / max(pulseAmt, 1e-3));
        col = max(col, m);
    }

    FragColor = vec4(col, 1.0);
}
