#version 330 core
out vec4 FragColor;

// ============================================================================
// Audio-Reactive Raymarch Starter (RaymarchVibe) - STAGE 3 REFERENCE
// ----------------------------------------------------------------------------
// Reference for the AUDIO PASS (SHADER_WORKFLOW.md stage 3). It shows the
// standard helper wired into a hooks block: the "AUDIO HOOKS BLOCK" in main()
// is the only place where audio values meet the scene. With silence every
// conditioned level is 0, so every hook is 0 and the look is the visual one.
// Section numbers: 3.x / 11.3 -> AUDIO_REACTIVITY.md, 9.13 ->
// SHADER_TECHNIQUES.md, everything else -> SHADERS.md.
//   * audio helper: per-band log windows, Audio Sensitivity, stuck-bass
//     fallback to mids, computed ONCE per pixel in main()            (3.2, 3.3)
//   * stateless beat-synced pulse travelling down the tunnel         (3.4)
//   * Debug View: raw / conditioned band meters                      (3.3)
//   * Trip Intensity master slider scaling every "extra" effect      (9.13)
//   * camera-safe warps: twist and kaleido fold around the camera's
//     own axis (radius-preserving), so the eye never clips            (9.12)
//   * glow weighted by step length and normalised, step-exhausted rays
//     treated as hits, distance-scaled normal epsilon                 (7.5)
//   * palette uniforms (palette:true), hue cycle, exposure, ACES + gamma
// The scene is an endless tunnel of ribbed rings and pillars along +z.
// Replace sceneSDF() with your own geometry; keep the clear disc around the
// camera axis (see CLEAR_R) when you do.
//
// Never multiply iTime by a speed uniform: the app already advances iTime by
// a control named u_speed if one exists (ShaderEffect.cpp:225-239). This
// shader has its own Camera Speed and does not declare u_speed.
// ============================================================================

uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;      // raw, per frame (transients)
uniform vec4 iAudioBandsAtt;   // snap attack, ~250 ms decay (motion)

// Palette: the first palette control is the Primary; names containing
// "Secondary"/"Accent" become Sync controls (see SHADERS.md 4.3).
uniform vec3 u_primaryColor = vec3(1.0, 0.35, 0.15);   // {"widget":"color", "palette":true, "label":"Primary Color"}
uniform vec3 u_secondaryColor = vec3(0.15, 0.45, 1.0); // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 u_accentColor = vec3(0.9, 0.2, 1.0);      // {"widget":"color", "palette":true, "label":"Accent Color"}

uniform float u_trip = 1.0;           // {"widget":"slider", "label":"Trip Intensity", "min":0.0, "max":2.0, "smooth":true}
uniform float u_camSpeed = 1.0;       // {"widget":"slider", "label":"Camera Speed", "min":0.0, "max":3.0, "smooth":true}
uniform float u_glow = 1.0;           // {"widget":"slider", "label":"Glow", "min":0.0, "max":3.0, "smooth":true}
uniform float u_audioReact = 1.0;     // {"widget":"slider", "label":"Audio Reactivity", "min":0.0, "max":2.0, "smooth":true}
uniform float u_audioSens = 0.0;      // {"widget":"slider", "label":"Audio Sensitivity (octaves)", "min":-3.0, "max":3.0, "smooth":true}
uniform float u_beatThreshold = 0.45; // {"widget":"slider", "label":"Beat Threshold (hit level)", "min":0.1, "max":0.9}
uniform float u_pulseIntensity = 1.0; // {"widget":"slider", "label":"Pulse Intensity", "min":0.0, "max":3.0, "smooth":true}
uniform float u_fog = 1.0;            // {"widget":"slider", "label":"Fog", "min":0.0, "max":3.0, "smooth":true}
uniform float u_exposure = 1.0;       // {"widget":"slider", "label":"Exposure", "min":0.3, "max":3.0, "smooth":true}
uniform float u_hueCycle = 0.3;       // {"widget":"slider", "label":"Hue Cycle Speed", "min":0.0, "max":2.0, "smooth":true}
uniform int u_maxSteps = 120;         // {"widget":"slider", "label":"Max Steps", "min":32, "max":200}
uniform int u_debugView = 0;          // {"widget":"slider", "label":"Debug View (0 off, 1 meters)", "min":0, "max":1}

const float TAU = 6.2831853;
const int   STEP_CAP = 200;           // loop bound must be constant; u_maxSteps breaks earlier
const float MAX_DIST = 50.0;
const float TUNNEL_R = 2.2;           // tunnel radius
const float CLEAR_R  = 1.4;           // nothing solid closer than this to the axis

// ---------------------------------------------------------------------------
// Audio helper (paste-ready block, AUDIO_REACTIVITY.md 3.3)
// ---------------------------------------------------------------------------
// Map a band value to 0..1 in a log window [lo, hi]. sens shifts the window
// down by that many octaves (positive = more sensitive).
float bandLevel(float v, float lo, float hi, float sens) {
    float a = log2(lo) - sens;
    float b = log2(hi) - sens;
    return clamp((log2(max(v, 1e-6)) - a) / (b - a), 0.0, 1.0);
}
// Windows measured from real music through the app (AUDIO_REACTIVITY.md 3.2)
const vec2 WIN_BASS = vec2(0.01, 0.5);
const vec2 WIN_MID  = vec2(0.005, 0.5);
const vec2 WIN_TREB = vec2(0.0005, 0.2);

struct Audio {
    float bass, mids, treb, level;   // conditioned 0..1 (x Audio Reactivity)
    float Lb, Lm, Lt, pinned;        // un-scaled levels + stuck-bass amount (for meters)
};

Audio getAudio() {
    Audio A;
    float react = clamp(u_audioReact, 0.0, 2.0);
    float sens = clamp(u_audioSens, -3.0, 3.0);
    // Loud masters pin bass at 1.0: it carries no beat information then, so
    // fall back to mids (pushed up 1.5 octaves to compensate).
    A.pinned = smoothstep(0.85, 0.98, iAudioBandsAtt.x);
    A.Lb = bandLevel(iAudioBandsAtt.x, WIN_BASS.x, WIN_BASS.y, sens);
    A.Lm = bandLevel(iAudioBandsAtt.y, WIN_MID.x,  WIN_MID.y,  sens - 1.5 * A.pinned);
    A.Lt = bandLevel(iAudioBandsAtt.z, WIN_TREB.x, WIN_TREB.y, sens - 0.75 * A.pinned);
    float LtRaw = bandLevel(iAudioBands.z, WIN_TREB.x, WIN_TREB.y, sens - 0.75 * A.pinned);
    float LbEff = mix(A.Lb, A.Lm, A.pinned);
    A.bass  = LbEff * react;
    A.mids  = A.Lm * react;
    A.treb  = max(A.Lt, 0.8 * LtRaw) * react;   // raw treble keeps the sparkle crisp
    A.level = (LbEff + A.Lm + A.Lt) / 3.0 * react;
    return A;
}

// Beat freshness from an enveloped level (AUDIO_REACTIVITY.md 3.4): x = 1 just after a
// hit, falling to 0 as the envelope decays through [thr, thr + span].
float beatAge(float L, float thr, float span) {
    return clamp((L - thr) / span, 0.0, 1.0);
}

float meterBar(vec2 px, float x0, float v) {
    return step(x0, px.x) * step(px.x, x0 + 14.0) *
           step(20.0, px.y) * step(px.y, 20.0 + 160.0 * clamp(v, 0.0, 1.0));
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
vec3 hueShift(vec3 c, float a) {
    const vec3 k = vec3(0.57735);
    float ca = cos(a);
    return c * ca + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - ca);
}
vec3 acesFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// ---------------------------------------------------------------------------
// Per-frame globals, set once in main()
// ---------------------------------------------------------------------------
float g_camZ, g_twist, g_kAmt, g_kN, g_stepFac;
vec3 g_colP, g_colS, g_colA;
float g_pR, g_pI;                 // beat pulse: distance ahead, intensity

// Camera-safe warp (SHADERS.md 9.12): only the ANGLE around the camera axis
// changes (twist grows with distance ahead; kaleido fold blends in far away),
// so every point keeps its distance from the axis and the clear disc stays
// clear. Also returns a step scale for the extra Lipschitz stretch.
vec3 warp(vec3 p) {
    float dz = p.z - g_camZ;
    float r = length(p.xy);
    float a = (r > 1e-6 ? atan(p.y, p.x) : 0.0) + g_twist * dz;   // atan(0,0) guard
    vec2 q = r * vec2(cos(a), sin(a));
    float S = r * abs(g_twist);
    if (g_kAmt > 0.0) {
        float seg = TAU / g_kN;
        float af = abs(mod(a + 0.5 * seg, seg) - 0.5 * seg);      // mirror fold
        float w = g_kAmt * smoothstep(8.0, 20.0, abs(dz));         // far away only
        q = mix(q, r * vec2(cos(af), sin(af)), w);                  // blend in Cartesian
        S += g_kAmt * r * 1.5 / 12.0;
    }
    g_stepFac = 2.0 / (S + sqrt(S * S + 4.0));                      // 1 / Lipschitz bound
    return vec3(q, p.z);
}

// Scene in unwarped coordinates: tunnel wall, ring ribs, polar pillars.
// Returns vec2(distance, material) - always read .x for distance.
vec2 sceneSDF(vec3 p) {
    float r = length(p.xy);
    float wall = TUNNEL_R - r;                                      // inside of a tube
    float zc = p.z - 2.0 * floor(p.z / 2.0) - 1.0;                  // repeat every 2 units
    float ribD = max(abs(zc) - 0.08, (TUNNEL_R - 0.25) - r);        // ring rib: solid from r = 1.95 out
    float a = atan(p.y, p.x + 1e-9);
    float seg = TAU / 8.0;
    float pa = mod(a + 0.5 * seg, seg) - 0.5 * seg;
    vec3 pp = vec3(r * cos(pa) - (TUNNEL_R - 0.3), r * sin(pa), zc);
    float pillar = sdBox(pp, vec3(0.25, 0.12, 0.35)) - 0.03;        // inner face at r = 1.62
    vec2 res = vec2(wall, 1.0);
    if (ribD < res.x) res = vec2(ribD, 2.0);
    if (pillar < res.x) res = vec2(pillar, 3.0);
    return res;
}

vec2 map(vec3 p) {
    vec2 d = sceneSDF(warp(p));
    d.x *= g_stepFac;
    return d;
}

vec3 calcNormal(vec3 p, float t) {
    float e = 0.0005 * clamp(t, 1.0, 20.0);                         // distance-scaled epsilon
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
    return clamp(1.0 - 2.0 * occ, 0.0, 1.0);
}

// Stateless beat pulse: a band of light at distance g_pR ahead of the camera.
float pulseAt(float z) {
    float x = (z - g_camZ) - g_pR;
    return exp(-x * x / 1.2) * g_pI;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;

    // ---- audio: once per pixel, never inside map()
    Audio A = getAudio();

    // ---- trip intensity: one master that scales every "extra" effect
    float T = clamp(u_trip, 0.0, 2.0) * (1.0 + 0.25 * A.level);
    float Tc = clamp(T, 0.0, 1.0);

    // ---- beat: mids drive it; bass too when it is not pinned
    float thr = clamp(u_beatThreshold, 0.05, 0.95);
    const float SPAN = 0.30;
    float x = max(beatAge(A.Lm, thr, SPAN), beatAge(A.Lb * (1.0 - A.pinned), thr, SPAN));
    float react = clamp(u_audioReact, 0.0, 2.0);
    g_pR = (1.0 - x) * 40.0;                                        // front travels 0 -> 40
    g_pI = pow(x, 0.6) * smoothstep(0.0, 0.06, 1.0 - x) * u_pulseIntensity * min(react, 1.0);
    // "snap": only the fresh top of the envelope, not a sustained loud passage
    float snap = smoothstep(0.55, 0.95, x) * smoothstep(0.0, 0.12, 1.0 - x) * min(react, 1.0);

    // ======================= AUDIO HOOKS BLOCK =============================
    // Approved plan filled in here; every hook is 0 in silence.
    float hueJump    = snap;            // AUDIO_HOOK: hueJump - beat hue jump, 0..1 (snappy, beat)
    float twistWind  = A.mids;          // AUDIO_HOOK: twistWind - extra twist rate, 0..2 (smooth, mids)
    float foldSnap   = snap;            // AUDIO_HOOK: foldSnap - kaleido +2 wedges, 0..1 (snappy, beat)
    float camSurge   = 0.5 * (A.Lb + A.Lm) * min(react, 1.0); // AUDIO_HOOK: camSurge - forward surge, 0..1 (smooth; fixed-weight, never the pinned crossfade)
    float zoomPunch  = A.bass * A.bass; // AUDIO_HOOK: zoomPunch - focal pull, 0..2 (bass)
    float glowBoost  = A.mids;          // AUDIO_HOOK: glowBoost - proximity glow gain, 0..2 (smooth, mids)
    float flash      = pow(snap, 3.0);  // AUDIO_HOOK: flash - screen flash, 0..1 (snappy, beat)
    float exposureLift = A.level;       // AUDIO_HOOK: exposureLift - master brightness, 0..2 (smooth, overall)
    // =======================================================================

    // ---- palette + hue cycle + beat hue jump
    float hue = iTime * 0.25 * u_hueCycle * Tc + hueJump * 1.5 * Tc;
    g_colP = hueShift(u_primaryColor, hue);
    g_colS = hueShift(u_secondaryColor, hue);
    g_colA = hueShift(u_accentColor, hue);

    // ---- warp amounts (all zero at Trip 0)
    g_twist = 0.04 * T * (1.0 + 0.6 * twistWind);
    g_kAmt = smoothstep(0.3, 1.0, T);
    g_kN = 6.0 + 2.0 * floor(foldSnap * 1.99);                          // kaleido snap on beats

    // ---- camera on the tunnel axis; bass surge bounded so it never reverses
    // (the envelope falls at most ~1 level unit per second, so an offset of
    //  A * level recedes slower than the glide when A < glide; SHADERS.md 9.12)
    float glide = 1.5 * u_camSpeed;
    float surge = camSurge * 0.6 * glide;
    g_camZ = iTime * glide + surge;
    vec3 ro = vec3(0.15 * sin(iTime * 0.3), 0.1 * sin(iTime * 0.23), g_camZ);  // sway << CLEAR_R
    vec3 fwd = normalize(vec3(0.05 * sin(iTime * 0.17), 0.04 * sin(iTime * 0.11), 1.0));
    vec3 side = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
    vec3 up = cross(fwd, side);
    float focal = 1.1 - 0.15 * zoomPunch - 0.2 * hueJump * Tc;   // zoom punch
    vec3 rd = normalize(fwd * focal + side * uv.x + up * uv.y);

    // ---- raymarch with step-length-weighted glow
    int maxSteps = clamp(u_maxSteps, 16, STEP_CAP);
    float t = 0.02, mat = 0.0, glowSum = 0.0;
    bool hit = false;
    for (int i = 0; i < STEP_CAP; i++) {
        if (i >= maxSteps) break;
        vec3 p = ro + rd * t;
        vec2 h = map(p);
        if (h.x < 0.0004 * t) { hit = true; mat = h.y; break; }
        float stepLen = h.x * 0.9;
        // weight by step length: the sum becomes an integral along the ray, so
        // it does not grow with the number of steps (no washout near surfaces)
        glowSum += exp(-max(h.x, 0.0) * 8.0) * min(stepLen, 0.5) * (h.y > 1.5 ? 1.0 : 0.2);
        t += stepLen;
        if (t > MAX_DIST) break;
    }
    // Ran out of steps while still inside the scene: call it a hit (no cracks)
    if (!hit && t < MAX_DIST) { hit = true; mat = map(ro + rd * t).y; }
    t = min(t, MAX_DIST);

    vec3 fogCol = g_colS * 0.03;
    vec3 col = fogCol;
    if (hit) {
        vec3 pos = ro + rd * t;
        vec3 n = calcNormal(pos, t);
        float ao = calcAO(pos, n);
        vec3 alb = (mat > 2.5) ? g_colA * 0.6 : ((mat > 1.5) ? g_colP * 0.7 : vec3(0.18));
        vec3 l = normalize(vec3(0.3, 0.8, 0.4));
        float dif = max(dot(n, l), 0.0) * 0.6 + 0.4 * (0.5 + 0.5 * n.y);
        float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
        col = alb * dif * ao * 0.6;
        col += hueShift(g_colS, fres * 3.0) * fres * 0.4 * Tc;      // iridescent rim (trip)
        col += mix(g_colA, vec3(1.0), 0.4) * pulseAt(pos.z) * ao;   // pulse light
        col = mix(col, fogCol, 1.0 - exp(-t * 0.04 * u_fog));
    }
    // Glow is a length integral (see the loop): scale it by a fixed constant
    col += g_colP * glowSum * 0.08 * u_glow * (1.0 + 1.5 * glowBoost);

    // ---- beat flash
    col += g_colA * flash * 0.15 * Tc;

    // ---- exposure, tonemap, gamma
    col *= u_exposure * 1.4 * (1.0 + 0.2 * exposureLift);
    col = acesFilm(max(col, 0.0));
    col = pow(col, vec3(1.0 / 2.2));

    if (u_debugView == 1) {
        vec2 px = fragCoord;
        vec3 m = vec3(0.0);
        m += vec3(1.0, 0.3, 0.2) * meterBar(px, 20.0,  sqrt(iAudioBands.x));   // raw (sqrt to show small values)
        m += vec3(0.3, 1.0, 0.3) * meterBar(px, 38.0,  sqrt(iAudioBands.y));
        m += vec3(0.3, 0.5, 1.0) * meterBar(px, 56.0,  sqrt(iAudioBands.z));
        m += vec3(1.0, 0.3, 0.2) * meterBar(px, 92.0,  A.Lb);                  // conditioned
        m += vec3(0.3, 1.0, 0.3) * meterBar(px, 110.0, A.Lm);
        m += vec3(0.3, 0.5, 1.0) * meterBar(px, 128.0, A.Lt);
        m += vec3(1.0, 1.0, 0.0) * meterBar(px, 146.0, A.pinned);              // bass stuck
        m += vec3(1.0)           * meterBar(px, 164.0, x);                     // beat age
        col = max(col * (1.0 - 0.7 * step(px.x, 190.0) * step(px.y, 190.0)), m);
    }
    FragColor = vec4(col, 1.0);
}
