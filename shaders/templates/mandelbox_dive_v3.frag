#version 330 core
out vec4 FragColor;

// ============================================================================
// Mandelbox Kaleidoscope Dive - v2 (shockwaves, per-band folds, depth glow)
// A raymarched Mandelbox (box fold + sphere fold) with a slow, continuous
// fly-through along a looping path. A small spherical "bubble" is carved out
// around the camera so it can never get stuck inside solid geometry.
//
// Audio (all optional - looks complete with no audio):
//   BAND CONDITIONING: the app's bands are mean FFT magnitudes / 32, clamped.
//     Measured on real tracks with the app's exact pipeline: bass sits pinned at
//     1.0 on loud masters (and ~0.01-0.15 on quiet sources), mids ~0.01-0.35,
//     treble ~0.001-0.03 (up to ~0.17 on hat-heavy mixes). So every band is
//     mapped through a per-band log window (Audio Sensitivity shifts it, in
//     octaves) to a usable 0..1 "level" L. When bass is pinned it carries no
//     dynamics, so bass-driven effects fall back to the mids envelope.
//   SHOCKWAVES (beat-synced, stateless): iAudioBandsAtt snaps up on a hit and
//     decays at a fixed rate (tau 250 ms), so in the log domain it falls
//     linearly in time. Each ring's radius = how far the envelope has decayed
//     below the hit level (Beat Threshold), i.e. time since the hit: every hit
//     launches a fresh ring from the camera. One ring follows mids (secondary
//     colour), one follows bass when bass is not pinned (primary colour).
//   PER-BAND FOLDS: bass L -> box-fold twist about y, mids L -> sphere-fold
//     min/fixed radius, treble L -> small per-iteration roll. All bounded.
//   DEPTH GLOW: hits classified by the fractal iteration that owns the
//     surface: outer structure = bass (primary), middle detail = mids
//     (secondary), fine dot/lace = treble (accent).
//   treble raw L -> chromatic fresnel edge shimmer; mean L -> brightness.
//   Debug View: 1 = audio meters (raw bands, conditioned L, ring drivers),
//   2 = depth classes in pure R/G/B with rings in white.
// Input channels: none used.
// ============================================================================

// Standard uniforms (native main() mode: iResolution is vec2)
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;      // raw: x=bass, y=mids, z=treble, w=overall (0..1)
uniform vec4 iAudioBandsAtt;   // enveloped: same layout (0..1)

// --- UI Controls ---
uniform float u_camSpeed = 1.0;        // {"widget":"slider", "label":"Camera Speed", "min":0.0, "max":4.0, "step":0.01, "smooth":true}
uniform float u_boxScale = 3.0;        // {"widget":"slider", "label":"Mandelbox Scale", "min":1.6, "max":4.0, "step":0.01, "smooth":true}
uniform float u_foldLimit = 1.0;       // {"widget":"slider", "label":"Fold Limit", "min":0.5, "max":1.5, "step":0.01, "smooth":true}
uniform float u_minRadius = 0.5;       // {"widget":"slider", "label":"Min Radius", "min":0.1, "max":1.0, "step":0.01, "smooth":true}
uniform float u_fixedRadius = 1.0;     // {"widget":"slider", "label":"Fixed Radius", "min":0.5, "max":1.5, "step":0.01, "smooth":true}
uniform int u_iterations = 11;         // {"widget":"slider", "label":"Fractal Iterations", "min":4, "max":20}
uniform int u_maxSteps = 110;          // {"widget":"slider", "label":"Max Raymarch Steps", "min":40, "max":220}
uniform float u_audioReact = 1.0;      // {"widget":"slider", "label":"Audio Reactivity", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_audioSens = 0.0;       // {"widget":"slider", "label":"Audio Sensitivity (octaves)", "min":-3.0, "max":3.0, "step":0.05, "smooth":true}
uniform int u_debugView = 0;           // {"widget":"slider", "label":"Debug View (0 off, 1 meters, 2 depth)", "min":0, "max":2}
uniform float u_glowIntensity = 1.0;   // {"widget":"slider", "label":"Glow Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_shimmer = 1.0;         // {"widget":"slider", "label":"Treble Shimmer", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_kaleido = 0.0;         // {"widget":"slider", "label":"Kaleidoscope Amount", "min":0.0, "max":1.0, "step":0.01, "smooth":true}
uniform int u_kaleidoSegments = 6;     // {"widget":"slider", "label":"Kaleidoscope Segments", "min":2, "max":16}
uniform float u_fogDensity = 0.15;     // {"widget":"slider", "label":"Fog Density", "min":0.0, "max":0.6, "step":0.005, "smooth":true}
uniform float u_colorCycle = 0.15;     // {"widget":"slider", "label":"Colour Cycle Speed", "min":0.0, "max":1.0, "step":0.01, "smooth":true}
// Shockwaves
uniform float u_shockIntensity = 1.0;  // {"widget":"slider", "label":"Shockwave Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_shockSpeed = 1.0;      // {"widget":"slider", "label":"Shockwave Speed", "min":0.3, "max":3.0, "step":0.01, "smooth":true}
uniform float u_ringWidth = 0.3;       // {"widget":"slider", "label":"Ring Width", "min":0.05, "max":1.0, "step":0.01, "smooth":true}
uniform float u_beatThreshold = 0.45;  // {"widget":"slider", "label":"Beat Threshold (hit level)", "min":0.1, "max":0.9, "step":0.01}
// Per-band folds
uniform float u_bassTwist = 1.0;       // {"widget":"slider", "label":"Bass Fold Twist", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_midSphereFold = 1.0;   // {"widget":"slider", "label":"Mid Sphere-Fold", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_trebleRotation = 1.0;  // {"widget":"slider", "label":"Treble Rotation", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
// Frequency-mapped depth glow
uniform float u_depthGlow = 1.0;       // {"widget":"slider", "label":"Depth Glow Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_depthBias = 0.0;       // {"widget":"slider", "label":"Depth Split Bias", "min":-0.5, "max":0.5, "step":0.01, "smooth":true}

// Palette (primary generates, secondary/accent sync)
uniform vec3 u_primaryColor = vec3(0.95, 0.45, 0.15);   // {"widget":"color", "palette":true, "label":"Primary Color"}
uniform vec3 u_secondaryColor = vec3(0.20, 0.55, 0.95); // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 u_accentColor = vec3(0.85, 0.25, 0.85);    // {"widget":"color", "palette":true, "label":"Accent Color"}
// --- End UI Controls ---

const float PI = 3.14159265359;
const float TAU = 6.28318530718;
const int MAX_ITER_CAP = 20;    // hard cap for fractal iterations
const int MAX_STEP_CAP = 220;   // hard cap for raymarch steps
const float MAX_DIST = 28.0;

// Per-frame fractal parameters (set once in main, read by map)
float g_scale;
float g_minR2;
float g_fixedR2;
float g_foldLimit;
float g_orbitR;     // radius of the circular flight path
float g_tunnelR;    // radius of the tunnel carved along the path
vec4 g_trap;
mat3 g_foldRot;     // per-iteration rotation of the fold frame:
                    //   bass twist about y (xz plane) * treble roll about z (xy plane)
bool g_useRot;      // skip the rotation entirely when it is ~0 (perf)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
mat2 rot2(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, s, -s, c);
}

// Cyclic 3-colour palette through the palette uniforms
vec3 palette3(float x) {
    x = fract(x) * 3.0;
    vec3 a = u_primaryColor, b = u_secondaryColor, c = u_accentColor;
    if (x < 1.0) return mix(a, b, smoothstep(0.0, 1.0, x));
    if (x < 2.0) return mix(b, c, smoothstep(0.0, 1.0, x - 1.0));
    return mix(c, a, smoothstep(0.0, 1.0, x - 2.0));
}

// Hue rotation (used for the chromatic treble shimmer)
vec3 hueShift(vec3 col, float a) {
    const vec3 k = vec3(0.57735);
    float c = cos(a);
    return col * c + cross(k, col) * sin(a) + k * dot(k, col) * (1.0 - c);
}

// Camera path: a closed circle (radius g_orbitR, in the xz-plane) through the
// interior, with a small wobble that always stays inside the carved tunnel.
vec3 camPath(float a) {
    vec3 c = vec3(cos(a), 0.0, sin(a)) * g_orbitR;
    vec3 radial = vec3(cos(a), 0.0, sin(a));
    float w = g_tunnelR * 0.35;
    return c + radial * (w * sin(a * 3.0 + 1.3)) + vec3(0.0, w * sin(a * 2.0), 0.0);
}

// Distance to the circular flight path (a torus core)
float pathDist(vec3 p) {
    return length(vec2(length(p.xz) - g_orbitR, p.y));
}

// Screen-space kaleidoscopic fold
vec2 kaleido(vec2 p, float segs) {
    float seg = TAU / segs;
    float a = atan(p.y, p.x);
    float r = length(p);
    a = mod(a + seg * 0.5, seg) - seg * 0.5;
    a = abs(a);
    return vec2(cos(a), sin(a)) * r;
}

// ---------------------------------------------------------------------------
// Audio band conditioning
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Shockwaves: SHOCK_RINGS rings, radius/intensity/colour set per frame in main
// from the decaying band envelopes (see header).
// ---------------------------------------------------------------------------
const int SHOCK_RINGS = 2;
const float DEPTH_SPLIT_A = 0.15;   // bass/mid boundary in iteration depth
const float DEPTH_SPLIT_B = 0.42;   // mid/treble boundary
float g_ringR[SHOCK_RINGS];
float g_ringI[SHOCK_RINGS];
vec3  g_ringC[SHOCK_RINGS];
float g_ringW;

// Light from all ring fronts at distance dist from the camera
vec3 shockwave(float dist) {
    vec3 acc = vec3(0.0);
    for (int k = 0; k < SHOCK_RINGS; k++) {
        if (g_ringI[k] <= 0.001) continue;
        float x = dist - g_ringR[k];
        float front = exp(-x * x / (g_ringW * g_ringW));                    // bright front
        float after = (x < 0.0) ? exp(x / (g_ringW * 6.0)) * 0.4 : 0.0;    // afterglow behind
        acc += g_ringC[k] * (front * 3.0 + after) * g_ringI[k];
    }
    return acc;
}

// Faint air tint for rays whose surface lies beyond a ring front
vec3 shockAir(float hitDist) {
    vec3 acc = vec3(0.0);
    for (int k = 0; k < SHOCK_RINGS; k++) {
        acc += g_ringC[k] * g_ringI[k] * step(g_ringR[k], hitDist);
    }
    return acc * 0.035;
}

// Debug meter bar (0..1 value) in a screen-space column
float meterBar(vec2 px, float x0, float v) {
    float inX = step(x0, px.x) * step(px.x, x0 + 14.0);
    float inY = step(20.0, px.y) * step(px.y, 20.0 + 160.0 * clamp(v, 0.0, 1.0));
    return inX * inY;
}

// ---------------------------------------------------------------------------
// Scene SDF - returns vec2(distance, materialID) per RaymarchVibe spec 2.4
// ---------------------------------------------------------------------------
vec2 map(vec3 pos) {
    vec4 z = vec4(pos, 1.0);
    vec3 c = pos;
    float absScale = abs(g_scale);
    vec4 trap = vec4(1e5);

    for (int i = 0; i < MAX_ITER_CAP; i++) {
        if (i >= u_iterations) break;
        // Rotate the fold frame (bass twist + treble roll), then box fold
        if (g_useRot) z.xyz = g_foldRot * z.xyz;
        z.xyz = clamp(z.xyz, -g_foldLimit, g_foldLimit) * 2.0 - z.xyz;
        // Sphere fold
        float r2 = dot(z.xyz, z.xyz);
        if (r2 < g_minR2) {
            z *= g_fixedR2 / g_minR2;
        } else if (r2 < g_fixedR2) {
            z *= g_fixedR2 / r2;
        }
        // Scale + translate
        z.xyz = z.xyz * g_scale + c;
        z.w = z.w * absScale + 1.0;
        trap = min(trap, vec4(abs(z.xyz), r2));
    }
    g_trap = trap;

    float d = length(z.xyz) / abs(z.w);
    // Carve a tunnel along the flight path so the camera never sits in solid
    d = max(d, g_tunnelR - pathDist(pos));
    return vec2(d, 1.0);
}

// Iteration depth of a surface point (evaluated once per pixel at the hit):
// soft-argmin of the per-iteration sphere trap |z|^2 - which iteration "owns"
// the surface (early = big outer structure, later = fine lace). Spatially
// smooth, so it reads as regions rather than pixel noise. Same fold sequence
// as map().
float depthAt(vec3 pos) {
    vec4 z = vec4(pos, 1.0);
    vec3 c = pos;
    float depW = 0.0, depWI = 0.0;
    for (int i = 0; i < MAX_ITER_CAP; i++) {
        if (i >= u_iterations) break;
        // Rotate the fold frame (bass twist + treble roll), then box fold
        if (g_useRot) z.xyz = g_foldRot * z.xyz;
        z.xyz = clamp(z.xyz, -g_foldLimit, g_foldLimit) * 2.0 - z.xyz;
        float r2 = dot(z.xyz, z.xyz);
        if (r2 < g_minR2) {
            z *= g_fixedR2 / g_minR2;
        } else if (r2 < g_fixedR2) {
            z *= g_fixedR2 / r2;
        }
        z.xyz = z.xyz * g_scale + c;
        float wgt = exp(-r2 * 3.3333);   // sigma 0.3
        depW += wgt; depWI += wgt * float(i);
    }
    return clamp(depWI / max(depW, 1e-6) / 3.3, 0.0, 1.0);   // ~iterations 0..3.3
}

vec3 calcNormal(vec3 p, float eps) {
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * eps).x +
                     k.yyx * map(p + k.yyx * eps).x +
                     k.yxy * map(p + k.yxy * eps).x +
                     k.xxx * map(p + k.xxx * eps).x);
}

// ---------------------------------------------------------------------------
void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // --- Audio (bounded) ---
    // Conditioned levels (0..1). Raw bands -> log windows; pinned bass falls
    // back to the mids envelope for "bass" motion/glow.
    float react = clamp(u_audioReact, 0.0, 2.0);
    float sens  = clamp(u_audioSens, -3.0, 3.0);
    // Pinned bass (clamped at 1.0) = a loud, mastered source: also use it as a
    // loudness cue and make mids/treble 1.5 / 0.75 octaves less sensitive, so
    // loud and quiet material land in similar ranges (tuned on real tracks).
    float bassPinned = smoothstep(0.85, 0.98, iAudioBandsAtt.x);
    float Lb  = bandLevel(iAudioBandsAtt.x, WIN_BASS.x, WIN_BASS.y, sens);
    float Lm  = bandLevel(iAudioBandsAtt.y, WIN_MID.x,  WIN_MID.y,  sens - 1.5 * bassPinned);
    float Lt  = bandLevel(iAudioBandsAtt.z, WIN_TREB.x, WIN_TREB.y, sens - 0.75 * bassPinned);
    float LtR = bandLevel(iAudioBands.z,    WIN_TREB.x, WIN_TREB.y, sens - 0.75 * bassPinned);
    float LbEff = mix(Lb, Lm, bassPinned);
    float bass    = LbEff * react;                 // motion / glow drivers
    float mids    = Lm * react;
    float trebAtt = Lt * react;
    float treble  = LtR * react;
    float level   = (LbEff + Lm + Lt) / 3.0 * react;

    // --- Fractal parameters (keep |scale| away from 1, minR < fixedR) ---
    float baseScale = clamp(u_boxScale, 1.5, 4.0);
    g_scale = baseScale + 0.015 * min(bass, 1.5);  // tiny remnant of v1's bass bounce
    g_foldLimit = clamp(u_foldLimit, 0.5, 1.5);
    // Approximate half-extent of the Mandelbox (from the un-modulated scale so
    // the flight path does not jump with the bass)
    float extent = min(2.0 * g_foldLimit * (baseScale + 1.0) / (baseScale - 1.0), 12.0);
    g_orbitR = 0.4 * extent;
    g_tunnelR = 0.05 * extent;
    // Mids breathe the sphere fold: min radius shrinks, fixed radius swells
    float midF = min(mids * mids, 2.25) * clamp(u_midSphereFold, 0.0, 2.0);
    float fixedR = clamp(u_fixedRadius, 0.5, 1.5) * (1.0 + 0.05 * midF);
    float minR = clamp(u_minRadius, 0.05, 1.0) * (1.0 - 0.22 * midF);
    minR = clamp(minR, 0.05, fixedR * 0.95);
    // Bass twists the box fold; treble adds a tiny per-iteration rotation
    // Squared so the motion concentrates on the loud part of each hit
    float twistA = 0.07 * min(bass * bass, 2.25) * clamp(u_bassTwist, 0.0, 2.0);
    float iterA  = 0.025 * min(trebAtt * trebAtt, 2.25) * clamp(u_trebleRotation, 0.0, 2.0);
    float cb = cos(twistA), sb = sin(twistA), ct = cos(iterA), st = sin(iterA);
    mat3 rotY = mat3(cb, 0.0, -sb,  0.0, 1.0, 0.0,  sb, 0.0, cb);   // column-major
    mat3 rotZ = mat3(ct, st, 0.0,  -st, ct, 0.0,  0.0, 0.0, 1.0);
    g_foldRot = rotZ * rotY;
    g_useRot = (twistA + iterA) > 1e-5;
    g_minR2 = minR * minR;
    g_fixedR2 = fixedR * fixedR;

    // --- Optional screen kaleidoscope ---
    float kAmt = clamp(u_kaleido, 0.0, 1.0);
    if (kAmt > 0.001) {
        vec2 ku = kaleido(uv * rot2(iTime * 0.05), float(max(u_kaleidoSegments, 2)));
        uv = mix(uv, ku, kAmt);
    }

    // --- Camera: fly along a looping path, look slightly ahead ---
    float T = iTime * 0.05 * u_camSpeed + 0.7;
    vec3 ro = camPath(T);
    vec3 ta = camPath(T + 0.12) + vec3(0.0, g_tunnelR * 0.3 * sin(T * 1.7), 0.0);
    vec3 fw = normalize(ta - ro);
    float roll = 0.35 * sin(T * 0.27);
    vec3 up = normalize(vec3(sin(roll), cos(roll), 0.0));
    vec3 rt = normalize(cross(fw, up));
    vec3 cu = cross(rt, fw);
    float fov = 1.25 + 0.15 * sin(T * 0.5);   // slow breathing zoom
    vec3 rd = normalize(uv.x * rt + uv.y * cu + fov * fw);

    // --- Shockwave rings, beat-synced through the envelope decay ---
    // x = how far the envelope still sits above the hit level (1 = just hit).
    // As the envelope decays (linear in log domain), x falls and the ring grows.
    float thr = clamp(u_beatThreshold, 0.05, 0.95);
    const float SPAN = 0.25;                       // L units the ring lives for
    float maxR = extent * 1.8 * clamp(u_shockSpeed, 0.3, 3.0);
    float shockAmt = clamp(u_shockIntensity, 0.0, 3.0) * min(react, 1.5);
    g_ringW = clamp(u_ringWidth, 0.02, 1.5) * extent * 0.15;
    float drv[SHOCK_RINGS];
    drv[0] = Lm;                                   // mids: always informative
    drv[1] = Lb * (1.0 - bassPinned);              // bass: only when not pinned
    g_ringC[0] = mix(u_secondaryColor, vec3(1.0), 0.35);
    g_ringC[1] = mix(u_primaryColor, vec3(1.0), 0.35);
    for (int k = 0; k < SHOCK_RINGS; k++) {
        float x = clamp((drv[k] - thr) / SPAN, 0.0, 1.0);
        g_ringR[k] = (1.0 - x) * maxR;
        g_ringI[k] = pow(x, 0.6) * smoothstep(0.0, 0.06, 1.0 - x) * shockAmt;
    }

    // --- Raymarch with glow accumulation ---
    float pixel = 1.6 / iResolution.y;
    float t = 0.0;
    float glow = 0.0;
    int steps = 0;
    bool hit = false;
    int maxSteps = clamp(u_maxSteps, 20, MAX_STEP_CAP);
    for (int i = 0; i < MAX_STEP_CAP; i++) {
        if (i >= maxSteps) break;
        vec3 p = ro + rd * t;
        float d = map(p).x;
        glow += exp(-d * 40.0 / g_tunnelR) * 0.008;
        float eps = max(0.0004, pixel * t);
        if (d < eps) { hit = true; steps = i; break; }
        t += d * 0.85;
        steps = i;
        if (t > MAX_DIST) break;
    }
    vec4 trap = g_trap;
    float depthDbg = 0.0;

    float cyc = iTime * u_colorCycle * 0.1;
    vec3 bg = mix(vec3(0.006, 0.006, 0.012), u_secondaryColor * 0.035, 0.5 + 0.5 * uv.y);
    vec3 col = bg;

    if (hit) {
        vec3 p = ro + rd * t;
        float eps = max(0.0005, pixel * t * 0.7);
        vec3 n = calcNormal(p, eps);
        float depth = depthAt(p);
        depthDbg = depth;

        // Orbit-trap colouring through the palette uniforms
        float tx = clamp(trap.x * 0.6, 0.0, 1.0);
        float ty = clamp(trap.y * 0.6, 0.0, 1.0);
        float tw = clamp(sqrt(trap.w) * 0.6, 0.0, 1.0);
        float band = 0.5 + 0.5 * sin(TAU * (tw * 0.8 + tx * 0.3 + cyc));
        vec3 base = mix(u_primaryColor, u_secondaryColor, smoothstep(0.15, 0.85, band));
        base = mix(base, u_accentColor, smoothstep(0.2, 0.0, ty) * 0.45);
        base = mix(vec3(dot(base, vec3(0.3333))), base, 0.85) * 0.85;

        // Soft AO: raymarch step count + a short 3-tap distance probe
        float aoSteps = 1.0 - float(steps) / float(maxSteps);
        float aoProbe = 0.0;
        float aoStep = g_tunnelR * 0.35;
        for (int k = 1; k <= 3; k++) {
            float h = aoStep * float(k);
            aoProbe += (h - map(p + n * h).x) / h * (1.0 / float(k));
        }
        float ao = clamp(1.0 - 0.55 * aoProbe, 0.0, 1.0) * (0.35 + 0.65 * aoSteps);
        ao = ao * ao;

        // Lighting: warm key + cool fill + head light
        vec3 L = normalize(vec3(0.6, 0.8, -0.3));
        float dif = clamp(dot(n, L), 0.0, 1.0);
        float fill = clamp(0.5 + 0.5 * dot(n, -L), 0.0, 1.0);
        float head = clamp(dot(n, -rd), 0.0, 1.0);
        vec3 h = normalize(L - rd);
        float spec = pow(clamp(dot(n, h), 0.0, 1.0), 40.0);
        vec3 lin = vec3(1.0, 0.92, 0.8) * dif * 0.9
                 + u_secondaryColor * fill * 0.18
                 + vec3(0.25) * head * head
                 + vec3(0.03);
        col = base * lin * ao;
        col += spec * 0.4 * ao * dif;

        // Emissive orbit-trap veins (mids)
        float emiss = smoothstep(0.12, 0.0, trap.x) + 0.6 * smoothstep(0.12, 0.0, trap.z);
        col += mix(u_accentColor, u_primaryColor, band) * emiss * u_glowIntensity * (0.3 + 0.3 * mids) * (0.3 + 0.7 * ao);

        // Chromatic fresnel edge shimmer (treble raw): per-channel fresnel
        // exponents give an RGB-split rim, hue-shifted by treble
        float rim = 1.0 - head;
        vec3 edge = vec3(pow(rim, 2.4), pow(rim, 3.0), pow(rim, 3.8));
        float ph = iTime * 4.0 + dot(p, vec3(7.0, 5.0, 3.0)) / g_tunnelR;
        vec3 edgeCol = hueShift(u_accentColor, (0.25 + 1.2 * treble) * sin(ph));
        float shimmer = u_shimmer * (0.08 + 1.0 * treble * treble);
        col += edge * edgeCol * shimmer * (0.65 + 0.35 * sin(ph * 1.7)) * (0.4 + 0.6 * ao);

        // Frequency-mapped depth glow (depth = soft-argmin trap iteration): outer structure = bass (primary),
        // middle detail = mids (secondary), fine lace = treble (accent)
        float dd = clamp(depth + u_depthBias, 0.0, 1.0);
        float wB = 1.0 - smoothstep(DEPTH_SPLIT_A - 0.08, DEPTH_SPLIT_A + 0.08, dd);
        float wT = smoothstep(DEPTH_SPLIT_B - 0.08, DEPTH_SPLIT_B + 0.08, dd);
        float wM = clamp(1.0 - wB - wT, 0.0, 1.0);
        float trebMix = max(trebAtt, treble * 0.8);
        // pow(.,2): quiet passages stay calm, loud parts of each band flare
        vec3 depthCol = u_primaryColor * wB * (0.08 + 1.1 * bass * bass)
                      + u_secondaryColor * wM * (0.08 + 1.4 * mids * mids)
                      + u_accentColor * wT * (0.08 + 1.8 * trebMix * trebMix);
        col += depthCol * u_depthGlow * (0.25 + 0.75 * ao) * (0.45 + 0.55 * (1.0 - head));

        // Fog to background
        float tn = t * 4.0 / (g_orbitR * 2.5);   // distance normalised to fractal size
        float fog = clamp(1.0 - exp(-u_fogDensity * (tn * tn * 0.5 + tn)), 0.0, 1.0);
        col = mix(col, bg, fog);

        // Shockwave light on surfaces (partly cuts through fog so it reads deep)
        // (acts as a light: tints by the surface colour so texture survives)
        vec3 shock = shockwave(t);
        col += shock * (base * 1.5 + 0.2) * (0.35 + 0.65 * ao) * (0.5 + 0.5 * head) * (1.0 - 0.6 * fog);
    }
    // Faint "air" tint for rays whose surface lies beyond a ring front
    col += shockAir(hit ? t : MAX_DIST);

    // Volumetric near-miss glow (mids)
    vec3 glowCol = mix(u_primaryColor, u_secondaryColor, 0.5 + 0.5 * sin(cyc * TAU));
    col += glowCol * glow * u_glowIntensity * (0.35 + 0.6 * mids) * 0.6;

    // Overall level: subtle brightness lift
    col *= 1.3 * (1.0 + 0.2 * min(level, 1.5));    // exposure + overall level lift

    // Vignette, tonemap, gamma
    vec2 q = gl_FragCoord.xy / iResolution.xy;
    col *= 0.55 + 0.45 * pow(16.0 * q.x * q.y * (1.0 - q.x) * (1.0 - q.y), 0.2);
    col = max(col, 0.0);
    col = col * (2.51 * col + 0.03) / (col * (2.43 * col + 0.59) + 0.14); // ACES approx
    col = pow(clamp(col, 0.0, 1.0), vec3(1.0 / 2.2));

    // --- Debug views ---
    if (u_debugView == 2 && hit) {
        float dd = clamp(depthDbg + u_depthBias, 0.0, 1.0);
        float wB = 1.0 - smoothstep(DEPTH_SPLIT_A - 0.08, DEPTH_SPLIT_A + 0.08, dd);
        float wT = smoothstep(DEPTH_SPLIT_B - 0.08, DEPTH_SPLIT_B + 0.08, dd);
        vec3 dc = vec3(wB, clamp(1.0 - wB - wT, 0.0, 1.0), wT) * 0.8;
        dc += vec3(dot(shockwave(t), vec3(0.33)));
        col = clamp(dc, 0.0, 1.0);
    }
    if (u_debugView == 1) {
        // Bars (bottom-left): raw x,y,z,w (sqrt-scaled so tiny values show) |
        // conditioned L bass, mid, treble, bass-pinned | ring 0/1 intensity
        vec2 px = gl_FragCoord.xy;
        vec3 m = vec3(0.0);
        m += vec3(1.0, 0.3, 0.2) * meterBar(px, 20.0,  sqrt(iAudioBands.x));
        m += vec3(0.3, 1.0, 0.3) * meterBar(px, 38.0,  sqrt(iAudioBands.y));
        m += vec3(0.3, 0.5, 1.0) * meterBar(px, 56.0,  sqrt(iAudioBands.z));
        m += vec3(0.8)           * meterBar(px, 74.0,  sqrt(iAudioBands.w));
        m += vec3(1.0, 0.3, 0.2) * meterBar(px, 110.0, Lb);
        m += vec3(0.3, 1.0, 0.3) * meterBar(px, 128.0, Lm);
        m += vec3(0.3, 0.5, 1.0) * meterBar(px, 146.0, Lt);
        m += vec3(1.0, 1.0, 0.0) * meterBar(px, 164.0, bassPinned);
        m += vec3(1.0)           * meterBar(px, 200.0, g_ringI[0] / max(shockAmt, 1e-3));
        m += vec3(1.0)           * meterBar(px, 218.0, g_ringI[1] / max(shockAmt, 1e-3));
        float panel = step(10.0, px.x) * step(px.x, 242.0) * step(10.0, px.y) * step(px.y, 190.0);
        col = mix(col, vec3(0.0), panel * 0.6);
        col = mix(col, m, step(0.001, dot(m, vec3(1.0))));
    }

    FragColor = vec4(col, 1.0);
}
