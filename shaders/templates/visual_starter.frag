#version 330 core
out vec4 FragColor;

// ============================================================================
// Visual Starter (RaymarchVibe) - STAGE 1: VISUAL PASS
// ----------------------------------------------------------------------------
// No audio code. The look is built and approved with visual sliders only.
// Motion can be previewed with the "Preview Pulse" slider, which feeds the
// same hook variables the audio pass will fill later.
//
// AUDIO HOOKS: every element that may react to music has ONE named float in
// the hooks block at the top of main(). Each is 0.0 by default (= the approved
// look) and is tagged:
//     // AUDIO_HOOK: <name> - <what it drives, sensible range>
// Stage 3 (audio pass) only replaces the right-hand side of these lines.
// Do not restructure the shader to add audio later.
//
// Rules (SHADERS.md): native main(), map() returns vec2(dist, material),
// never multiply iTime by u_speed, exhausted rays count as hits, glow is
// weighted by step length, atan(0,0) guarded, camera stays clear.
// ============================================================================

uniform vec2 iResolution;
uniform float iTime;

// Palette (SHADERS.md 4.3): first palette control is Primary.
uniform vec3 u_primaryColor = vec3(0.10, 0.75, 1.00);   // {"widget":"color", "palette":true, "label":"Primary Color"}
uniform vec3 u_secondaryColor = vec3(0.55, 1.00, 0.70); // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 u_accentColor = vec3(1.00, 0.85, 0.40);    // {"widget":"color", "palette":true, "label":"Accent Color"}

uniform float u_camSpeed = 1.0;       // {"widget":"slider", "label":"Camera Speed", "min":0.0, "max":3.0}
uniform float u_glow = 1.0;           // {"widget":"slider", "label":"Glow", "min":0.0, "max":3.0, "smooth":true}
uniform float u_fog = 1.0;            // {"widget":"slider", "label":"Fog", "min":0.0, "max":3.0, "smooth":true}
uniform float u_exposure = 1.0;       // {"widget":"slider", "label":"Exposure", "min":0.3, "max":3.0, "smooth":true}
uniform int u_quality = 1;            // {"widget":"slider", "label":"Quality (0 low, 1 med, 2 high)", "min":0, "max":2}

// Preview-only slider: fakes a 0..1 "music" signal so hook motion can be judged
// without audio. Removed (or left at 0) in the audio pass.
uniform float u_previewPulse = 0.0;   // {"widget":"slider", "label":"Preview Pulse", "min":0.0, "max":1.0}

const float TAU = 6.2831853;
const int   STEP_CAP = 200;           // constant loop bound
const float MAX_DIST = 50.0;
const float TUNNEL_R = 2.2;
const float CLEAR_R  = 1.4;           // nothing solid closer than this to the camera axis

// ---------------------------------------------------------------------------
// Per-frame globals (set once in main, read in map)
// ---------------------------------------------------------------------------
float g_camZ;
float g_ribSwell;     // copy of hook ribSwell
float g_twist;        // total twist rate (base + hook)

vec3 acesFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// Scene: tunnel wall, ribs, pillars. Returns vec2(distance, material).
vec2 map(vec3 p) {
    // Radius-preserving twist about the camera axis (camera-safe)
    float r = length(p.xy);
    float a = (r > 1e-6 ? atan(p.y, p.x) : 0.0) + g_twist * (p.z - g_camZ);
    float S = r * abs(g_twist);
    float stepFac = 2.0 / (S + sqrt(S * S + 4.0));        // 1 / Lipschitz bound

    float wall = TUNNEL_R - r;
    float zc = p.z - 2.0 * floor(p.z / 2.0) - 1.0;
    // ribSwell thickens the ribs but is bounded well below the clear margin
    float ribD = max(abs(zc) - (0.08 + 0.06 * g_ribSwell), (TUNNEL_R - 0.25) - r);
    float seg = TAU / 8.0;
    float pa = mod(a + 0.5 * seg, seg) - 0.5 * seg;
    vec3 pp = vec3(r * cos(pa) - (TUNNEL_R - 0.3), r * sin(pa), zc);
    float pillar = sdBox(pp, vec3(0.25, 0.12, 0.35)) - 0.03;

    vec2 res = vec2(wall, 1.0);
    if (ribD < res.x) res = vec2(ribD, 2.0);
    if (pillar < res.x) res = vec2(pillar, 3.0);
    res.x *= stepFac;
    return res;
}

vec3 calcNormal(vec3 p, float t) {
    float e = 0.0005 * clamp(t, 1.0, 20.0);
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * e).x + k.yyx * map(p + k.yyx * e).x +
                     k.yxy * map(p + k.yxy * e).x + k.xxx * map(p + k.xxx * e).x);
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // ======================= AUDIO HOOKS BLOCK =============================
    // 0.0 = approved silent look. Preview Pulse drives them for visual tests.
    float pv = clamp(u_previewPulse, 0.0, 1.0);
    float ribSwell   = pv;          // AUDIO_HOOK: ribSwell - rib thickness swell, 0..1 (smooth)
    float glowBoost  = pv;          // AUDIO_HOOK: glowBoost - proximity glow gain, 0..1 (smooth)
    float tunnelTwist = 0.0;        // AUDIO_HOOK: tunnelTwist - extra twist rate, 0..1 (smooth, slow)
    float flash      = pv * pv;     // AUDIO_HOOK: flash - one-shot screen flash, 0..1 (snappy, beat)
    float sparkle    = pv;          // AUDIO_HOOK: sparkle - pillar rim shimmer, 0..1 (snappy)
    // =======================================================================

    g_ribSwell = clamp(ribSwell, 0.0, 1.0);
    g_twist = 0.02 + 0.04 * clamp(tunnelTwist, 0.0, 1.0);

    // Camera: path uses only unmodulated parameters (never a hook)
    g_camZ = iTime * 1.5 * u_camSpeed;
    vec3 ro = vec3(0.15 * sin(iTime * 0.3), 0.1 * sin(iTime * 0.23), g_camZ);
    vec3 fwd = normalize(vec3(0.05 * sin(iTime * 0.17), 0.04 * sin(iTime * 0.11), 1.0));
    vec3 side = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
    vec3 up = cross(fwd, side);
    vec3 rd = normalize(fwd * 1.1 + side * uv.x + up * uv.y);

    int maxSteps = (u_quality <= 0) ? 64 : ((u_quality == 1) ? 120 : 200);
    float t = 0.02, mat = 0.0, glowSum = 0.0;
    bool hit = false;
    for (int i = 0; i < STEP_CAP; i++) {
        if (i >= maxSteps) break;
        vec2 h = map(ro + rd * t);
        if (h.x < 0.0004 * t) { hit = true; mat = h.y; break; }
        float stepLen = h.x * 0.9;
        glowSum += exp(-max(h.x, 0.0) * 8.0) * min(stepLen, 0.5) * (h.y > 1.5 ? 1.0 : 0.2);
        t += stepLen;
        if (t > MAX_DIST) break;
    }
    if (!hit && t < MAX_DIST) { hit = true; mat = map(ro + rd * t).y; }   // exhausted = hit
    t = min(t, MAX_DIST);

    vec3 fogCol = u_secondaryColor * 0.03;
    vec3 col = fogCol;
    if (hit) {
        vec3 pos = ro + rd * t;
        vec3 n = calcNormal(pos, t);
        vec3 alb = (mat > 2.5) ? u_accentColor * 0.6 : ((mat > 1.5) ? u_primaryColor * 0.7 : vec3(0.18));
        vec3 l = normalize(vec3(0.3, 0.8, 0.4));
        float dif = max(dot(n, l), 0.0) * 0.6 + 0.4 * (0.5 + 0.5 * n.y);
        float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
        col = alb * dif * 0.6 + u_secondaryColor * fres * 0.3;
        if (mat > 2.5) col += u_accentColor * fres * 0.8 * clamp(sparkle, 0.0, 1.0);
        col = mix(col, fogCol, 1.0 - exp(-t * 0.04 * u_fog));
    }
    col += u_primaryColor * glowSum * 0.08 * u_glow * (1.0 + 1.5 * clamp(glowBoost, 0.0, 1.0));
    col += u_accentColor * 0.15 * clamp(flash, 0.0, 1.0);

    col *= u_exposure * 1.4;
    col = acesFilm(max(col, 0.0));
    col = pow(col, vec3(1.0 / 2.2));
    FragColor = vec4(col, 1.0);
}
