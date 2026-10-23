#version 330 core
out vec4 FragColor;

// ============================================================================
// Mandelbox Kaleidoscope Dive
// A raymarched Mandelbox (box fold + sphere fold) with a slow, continuous
// fly-through along a looping path. A small spherical "bubble" is carved out
// around the camera so it can never get stuck inside solid geometry.
//
// Audio (all optional - looks complete with no audio):
//   bass    (iAudioBandsAtt.x, smoothed) -> fold scale + min radius (bounded),
//                                            the structure rearranges on drops
//   mids    (iAudioBandsAtt.y, smoothed) -> emissive glow / orbit-trap colour
//   treble  (iAudioBands.z, raw)         -> chromatic fresnel edge shimmer
//   overall (iAudioBandsAtt.w, smoothed) -> subtle overall brightness
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
uniform float u_glowIntensity = 1.0;   // {"widget":"slider", "label":"Glow Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_shimmer = 1.0;         // {"widget":"slider", "label":"Treble Shimmer", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_kaleido = 0.0;         // {"widget":"slider", "label":"Kaleidoscope Amount", "min":0.0, "max":1.0, "step":0.01, "smooth":true}
uniform int u_kaleidoSegments = 6;     // {"widget":"slider", "label":"Kaleidoscope Segments", "min":2, "max":16}
uniform float u_fogDensity = 0.15;     // {"widget":"slider", "label":"Fog Density", "min":0.0, "max":0.6, "step":0.005, "smooth":true}
uniform float u_colorCycle = 0.15;     // {"widget":"slider", "label":"Colour Cycle Speed", "min":0.0, "max":1.0, "step":0.01, "smooth":true}

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
// Scene SDF - returns vec2(distance, materialID) per RaymarchVibe spec 2.4
// ---------------------------------------------------------------------------
vec2 map(vec3 pos) {
    vec4 z = vec4(pos, 1.0);
    vec3 c = pos;
    float absScale = abs(g_scale);
    vec4 trap = vec4(1e5);

    for (int i = 0; i < MAX_ITER_CAP; i++) {
        if (i >= u_iterations) break;
        // Box fold
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
    float react  = clamp(u_audioReact, 0.0, 2.0);
    float bass   = clamp(iAudioBandsAtt.x, 0.0, 1.0) * react;
    float mids   = clamp(iAudioBandsAtt.y, 0.0, 1.0) * react;
    float treble = clamp(iAudioBands.z, 0.0, 1.0) * react;
    float level  = clamp(iAudioBandsAtt.w, 0.0, 1.0) * react;

    // --- Fractal parameters (keep |scale| away from 1, minR < fixedR) ---
    float baseScale = clamp(u_boxScale, 1.5, 4.0);
    g_scale = baseScale + 0.12 * min(bass, 1.5);   // bass gently expands fold scale
    g_foldLimit = clamp(u_foldLimit, 0.5, 1.5);
    // Approximate half-extent of the Mandelbox (from the un-modulated scale so
    // the flight path does not jump with the bass)
    float extent = min(2.0 * g_foldLimit * (baseScale + 1.0) / (baseScale - 1.0), 12.0);
    g_orbitR = 0.4 * extent;
    g_tunnelR = 0.05 * extent;
    float fixedR = clamp(u_fixedRadius, 0.5, 1.5);
    float minR = clamp(u_minRadius, 0.05, 1.0) * (1.0 - 0.22 * min(bass, 1.5));
    minR = clamp(minR, 0.05, fixedR * 0.95);
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

    float cyc = iTime * u_colorCycle * 0.1;
    vec3 bg = mix(vec3(0.006, 0.006, 0.012), u_secondaryColor * 0.035, 0.5 + 0.5 * uv.y);
    vec3 col = bg;

    if (hit) {
        vec3 p = ro + rd * t;
        float eps = max(0.0005, pixel * t * 0.7);
        vec3 n = calcNormal(p, eps);

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
        col += mix(u_accentColor, u_primaryColor, band) * emiss * u_glowIntensity * (0.3 + 1.6 * mids) * (0.3 + 0.7 * ao);

        // Chromatic fresnel edge shimmer (treble raw): per-channel fresnel
        // exponents give an RGB-split rim, hue-shifted by treble
        float rim = 1.0 - head;
        vec3 edge = vec3(pow(rim, 2.4), pow(rim, 3.0), pow(rim, 3.8));
        float ph = iTime * 4.0 + dot(p, vec3(7.0, 5.0, 3.0)) / g_tunnelR;
        vec3 edgeCol = hueShift(u_accentColor, (0.25 + 1.8 * treble) * sin(ph));
        float shimmer = u_shimmer * (0.08 + 1.4 * treble);
        col += edge * edgeCol * shimmer * (0.65 + 0.35 * sin(ph * 1.7)) * (0.4 + 0.6 * ao);

        // Fog to background
        float tn = t * 4.0 / (g_orbitR * 2.5);   // distance normalised to fractal size
        float fog = 1.0 - exp(-u_fogDensity * (tn * tn * 0.5 + tn));
        col = mix(col, bg, clamp(fog, 0.0, 1.0));
    }

    // Volumetric near-miss glow (mids)
    vec3 glowCol = mix(u_primaryColor, u_secondaryColor, 0.5 + 0.5 * sin(cyc * TAU));
    col += glowCol * glow * u_glowIntensity * (0.35 + 1.2 * mids) * 0.6;

    // Overall level: subtle brightness lift
    col *= 1.3 * (1.0 + 0.25 * min(level, 1.5));   // exposure + overall level lift

    // Vignette, tonemap, gamma
    vec2 q = gl_FragCoord.xy / iResolution.xy;
    col *= 0.55 + 0.45 * pow(16.0 * q.x * q.y * (1.0 - q.x) * (1.0 - q.y), 0.2);
    col = max(col, 0.0);
    col = col * (2.51 * col + 0.03) / (col * (2.43 * col + 0.59) + 0.14); // ACES approx
    col = pow(clamp(col, 0.0, 1.0), vec3(1.0 / 2.2));

    FragColor = vec4(col, 1.0);
}
