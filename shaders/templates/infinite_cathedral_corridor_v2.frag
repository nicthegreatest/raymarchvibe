#version 330 core
out vec4 FragColor;

// Infinite Cathedral Corridor v2: "Hyperspace Nave"
// The v1 gothic nave (piers, ribbed pointed vault, lancet windows, analytic
// sunlight through the glass) pushed into DMT territory, with the cathedral
// kept as the anchor:
//  * Space warp around the camera's own axis: a helical twist that grows with
//    distance ahead, a kaleidoscopic polar fold that turns the distant nave
//    into a mandala tunnel, and walls that breathe and ripple. All three keep
//    distance from the eye axis (radius) unchanged, so the space around the
//    camera stays clear and it never clips.
//  * Melting: liquid streaks bulge and run down the walls, vault and glass;
//    colours flow downward; the whole image wobbles slowly.
//  * Fractal recursion: tracery of arches inside arches (glass and walls) and
//    branching fractal filigree crawling over the vault.
//  * Hyperspace patterning: a glowing hex lattice whose cells open into eyes and
//    faceted jewels, iridescent thin-film sheen, hyper-saturated hue cycling
//    driven by the palette uniforms, chromatic aberration on emissive layers,
//    glow around the windows, deep blacks.
//  * Kaleidoscopic, animated stained glass that dominates; shafts are back-traced
//    in warped space, so they swirl with the corridor.
//  * Beat moments: screen-wide flash and ring, kaleidoscope snap (more folds,
//    mandala rushing in), hue jump, zoom punch, light pulse racing down the nave.
//
// Trip Intensity: 0 is close to v1 calm, 1 is full psychedelic, 2 is insane.
//
// Audio (iAudioBandsAtt). App bands are FFT means / 32: bass is often pinned at
// 1.0 on loud masters, mids ~0.03-0.35, treble ~0.002-0.04. Each band is mapped
// to 0..1 in a log window (Audio Sensitivity shifts it in octaves). Pinned bass
// falls back to mids.
//   bass    -> forward surge (bounded, never backwards), zoom punch, warp/breath amplitude
//   beats   -> corridor pulse, screen flash and ring, kaleido snap, hue jump
//   mids    -> glass brightness, pattern intensity, twist rate (build-ups wind it up)
//   treble  -> ornament shimmer, chromatic aberration, dust sparkle
//   overall -> trip intensity and exposure
// The app scales iTime by its own speed control; nothing here multiplies by a
// speed uniform. Camera Speed paces the glide only.

uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;
uniform vec4 iAudioBandsAtt;

uniform vec3 u_primaryColor = vec3(1.0, 0.10, 0.55);    // {"widget":"color", "palette":true, "label":"Primary Color"}
uniform vec3 u_secondaryColor = vec3(0.10, 0.35, 1.0);  // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 u_accentColor = vec3(0.20, 1.0, 0.55);     // {"widget":"color", "palette":true, "label":"Accent Color"}

uniform float u_trip = 1.0;            // {"widget":"slider", "label":"Trip Intensity (0 calm, 1 full, 2 insane)", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_twist = 1.0;           // {"widget":"slider", "label":"Twist", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform int u_kaleidoFolds = 6;        // {"widget":"slider", "label":"Kaleidoscope Folds (0 off)", "min":0, "max":12}
uniform float u_meltDrip = 1.0;        // {"widget":"slider", "label":"Melt / Drip", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform int u_recursion = 3;           // {"widget":"slider", "label":"Recursion Depth", "min":1, "max":5}
uniform float u_patternIntensity = 1.0; // {"widget":"slider", "label":"Pattern Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_chromaticAb = 1.0;     // {"widget":"slider", "label":"Chromatic Aberration", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_hueCycle = 0.5;        // {"widget":"slider", "label":"Hue Cycle Speed", "min":0.0, "max":2.0, "step":0.01, "smooth":true}

uniform float u_camSpeed = 1.0;        // {"widget":"slider", "label":"Camera Speed", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_bayLength = 3.4;       // {"widget":"slider", "label":"Bay Length", "min":2.4, "max":5.0, "step":0.01, "smooth":true}
uniform float u_naveWidth = 2.6;       // {"widget":"slider", "label":"Nave Width (half)", "min":1.8, "max":3.6, "step":0.01, "smooth":true}
uniform float u_naveHeight = 6.0;      // {"widget":"slider", "label":"Nave Height (springing)", "min":4.0, "max":9.0, "step":0.01, "smooth":true}
uniform float u_archSharpness = 0.6;   // {"widget":"slider", "label":"Arch Sharpness", "min":0.0, "max":1.5, "step":0.01, "smooth":true}
uniform float u_windowGlow = 1.0;      // {"widget":"slider", "label":"Window Glow", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_shaftIntensity = 1.0;  // {"widget":"slider", "label":"Shaft Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_dustAmount = 1.0;      // {"widget":"slider", "label":"Dust Amount", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_bassSurge = 1.0;       // {"widget":"slider", "label":"Bass Surge", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_midGlassPulse = 1.0;   // {"widget":"slider", "label":"Mid Glass Pulse", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_trebleDust = 1.0;      // {"widget":"slider", "label":"Treble Dust / Shimmer", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_audioReact = 1.0;      // {"widget":"slider", "label":"Audio Reactivity", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_audioSens = 0.0;       // {"widget":"slider", "label":"Audio Sensitivity (octaves)", "min":-3.0, "max":3.0, "step":0.05, "smooth":true}
uniform float u_beatThreshold = 0.45;  // {"widget":"slider", "label":"Beat Threshold (hit level)", "min":0.1, "max":0.9, "step":0.01}
uniform float u_pulseIntensity = 1.0;  // {"widget":"slider", "label":"Pulse Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_fog = 1.0;             // {"widget":"slider", "label":"Fog", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_exposure = 1.0;        // {"widget":"slider", "label":"Exposure", "min":0.3, "max":3.0, "step":0.01, "smooth":true}
uniform int u_maxSteps = 160;          // {"widget":"slider", "label":"Max Steps", "min":64, "max":260}
uniform int u_debugView = 0;           // {"widget":"slider", "label":"Debug View (0 off, 1 meters, 2 light only)", "min":0, "max":2}

const int STEP_CAP = 260;
const float MAX_DIST = 60.0;
const float GLASS_D = 0.30;
const float SILL_Y = 1.3;
const float CAM_Y = 2.3;             // camera height = warp axis height
const float KRAMP = 12.0;            // kaleido fold blends in over this many units
const vec3 LIGHT_DIR = vec3(-0.80, -0.50, 0.33);
const float TAU = 6.2831853;

// Per-frame state, set in main()
float g_W, g_H, g_R, g_B, g_sharp;
float g_ww, g_wR, g_ys, g_wTop, g_roseY, g_roseR, g_kcY;
vec3 g_L;
vec3 g_colP, g_colS, g_colA;
float g_glassGain, g_camZ, g_time;
float g_T, g_Tc;                     // trip intensity (audio-boosted), clamped 0..1 version
float g_twist, g_roll, g_kAmt, g_kN, g_kOn, g_kC, g_breath, g_ripple, g_melt, g_dispL;
float g_shaftGain;
float g_hue, g_mids, g_treb, g_snap, g_patI, g_ca, g_glassN;
int g_rec;
// set by map()
float g_stepFac;
float g_glassD;

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
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
vec3 hueShift(vec3 c, float a) {
    const vec3 k = vec3(0.57735);
    float ca = cos(a);
    return c * ca + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - ca);
}
vec3 acesFilm(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
vec3 saturateCol(vec3 c, float s) {
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    return max(mix(vec3(l), c, s), 0.0);
}
mat2 rot2(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }

// Hyper-saturated colour from the (hue-cycled) palette: blends the three
// palette colours around a loop, then adds an iridescent hue sweep.
vec3 trippy(float x) {
    vec3 c = g_colP * (0.5 + 0.5 * cos(TAU * x))
           + g_colS * (0.5 + 0.5 * cos(TAU * (x - 0.333)))
           + g_colA * (0.5 + 0.5 * cos(TAU * (x - 0.667)));
    c = hueShift(c, 1.4 * sin(TAU * x * 0.5));
    return saturateCol(c, 1.6);
}

// Hex lattice: returns (local xy, cell id). Inradius 0.5 in lattice units.
vec4 hexCell(vec2 p) {
    const vec2 s = vec2(1.0, 1.7320508);
    vec4 hC = floor(vec4(p, p - vec2(0.5, 1.0)) / s.xyxy) + 0.5;
    vec4 h = vec4(p - hC.xy * s, p - (hC.zw + 0.5) * s);
    return dot(h.xy, h.xy) < dot(h.zw, h.zw) ? vec4(h.xy, hC.xy) : vec4(h.zw, hC.zw + 0.5);
}
float hexEdge(vec2 l) {   // 0 at the cell edge, 0.5 at the centre
    l = abs(l);
    return 0.5 - max(dot(l, vec2(0.5, 0.8660254)), l.x);
}

// ---------------------------------------------------------------------------
// Audio band conditioning (shared with Mandelbox Dive v2 / KIFS Temple / v1)
// ---------------------------------------------------------------------------
float bandLevel(float v, float lo, float hi, float sens) {
    float a = log2(lo) - sens;
    float b = log2(hi) - sens;
    return clamp((log2(max(v, 1e-6)) - a) / (b - a), 0.0, 1.0);
}
const vec2 WIN_BASS = vec2(0.01, 0.5);
const vec2 WIN_MID  = vec2(0.005, 0.5);
const vec2 WIN_TREB = vec2(0.0005, 0.2);

// ---------------------------------------------------------------------------
// Corridor pulses (stateless, beat-synced): front distance ahead of the camera
// ---------------------------------------------------------------------------
const int PULSES = 2;
float g_pR[PULSES];
float g_pI[PULSES];
vec3  g_pC[PULSES];
const float PULSE_W = 1.6;

vec3 pulseAt(float z) {
    vec3 acc = vec3(0.0);
    float dz = z - g_camZ;
    for (int k = 0; k < PULSES; k++) {
        if (g_pI[k] <= 0.001) continue;
        float x = dz - g_pR[k];
        float front = exp(-x * x / (PULSE_W * PULSE_W));
        float wake = (x < 0.0 && dz > 0.0) ? exp(x / (PULSE_W * 3.0)) * 0.05 : 0.0;
        acc += g_pC[k] * (front + wake) * g_pI[k];
    }
    return acc;
}

float meterBar(vec2 px, float x0, float v) {
    float inX = step(x0, px.x) * step(px.x, x0 + 14.0);
    float inY = step(20.0, px.y) * step(px.y, 20.0 + 160.0 * clamp(v, 0.0, 1.0));
    return inX * inY;
}

// ---------------------------------------------------------------------------
// Space warp (applied to every scene lookup). Polar coordinates about the
// camera axis (x = 0, y = CAM_Y): twist and kaleido change only the angle,
// and breathing scales the radius by at most ~18%. Points near the axis stay
// near the axis, so the clear disc around the eye is never filled.
// ---------------------------------------------------------------------------
vec3 warp(vec3 p, out float twistAng) {
    float dz = p.z - g_camZ;
    vec2 q = p.xy - vec2(0.0, CAM_Y);
    float r = length(q);
    float a0 = atan(q.x, -q.y);
    twistAng = g_twist * dz + g_roll;
    float a = a0 + twistAng;
    vec2 qr = r * vec2(sin(a), -cos(a));
    float S = r * (abs(g_twist) + 0.6 * g_breath) + 2.0 * g_breath;   // shear bound for the step size
    if (g_kAmt > 0.0) {
        float wk = g_kAmt * smoothstep(g_kOn, g_kOn + KRAMP, abs(dz));
        if (wk > 0.0) {
            float seg = TAU / g_kN;
            float af = a - g_kC;
            af = abs(mod(af + 0.5 * seg, seg) - 0.5 * seg) + g_kC;
            vec2 qf = r * vec2(sin(af), -cos(af));
            qr = mix(qr, qf, wk);
            S += g_kAmt * r * 1.5 / KRAMP * step(abs(dz), g_kOn + KRAMP);
        }
    }
    float br = 1.0 + g_breath * sin(dz * 0.55 - g_time * 1.6 + a * 2.0) * smoothstep(0.5, 3.0, abs(dz));
    qr *= br;
    g_stepFac = 2.0 / (S + sqrt(S * S + 4.0)) / g_dispL;
    return vec3(qr.x, qr.y + CAM_Y, p.z);
}
vec3 warp(vec3 p) { float t; return warp(p, t); }

// ---------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------
float windowSDF(float v, float y) {
    float ax = abs(v);
    if (y < g_ys) return max(ax - g_ww, SILL_Y - y);
    return length(vec2(ax - (g_ww - g_wR), y - g_ys)) - g_wR;
}

// Recursive tracery: pointed arch outline, an oculus in its head, then two
// half-size arches inside it, recursively. Returns distance to the lines.
float archNest(float x, float y, float w, float ys, int depth) {
    float d = 1e3;
    for (int i = 0; i < 6; i++) {
        if (i > depth) break;
        float R = w * (1.0 + g_sharp);
        float ax = abs(x);
        float o = (y < ys) ? abs(ax - w) : abs(length(vec2(ax - (w - R), y - ys)) - R);
        float hh = sqrt(max(R * R - (R - w) * (R - w), 0.0));
        if (i > 0) o = min(o, abs(length(vec2(ax, y - (ys + hh * 0.32))) - w * 0.42));
        d = min(d, o);
        x = ax - w * 0.5;
        w *= 0.5;
        float Rc = w * (1.0 + g_sharp);
        float hc = sqrt(max(Rc * Rc - (Rc - w) * (Rc - w), 0.0));
        ys = ys - hc * 0.95;
    }
    return d;
}

vec3 palettePick(float h) {
    vec3 c = (h < 0.38) ? g_colS : ((h < 0.72) ? g_colP : g_colA);
    return c * (0.65 + 0.6 * fract(h * 7.31));
}

// v1 glass (calm end of the Trip slider)
vec3 glassCalm(float v, float y, float bay) {
    vec3 c;
    float lead;
    vec2 rp = vec2(v, y - g_roseY);
    float rr = length(rp);
    if (rr < g_roseR) {
        float a = atan(rp.y, rp.x + 1e-6);
        float sec = floor((a + 3.14159265) / 0.785398);
        float ring = step(g_roseR * 0.45, rr);
        float fa = fract((a + 3.14159265) / 0.785398);
        float spoke = min(fa, 1.0 - fa) * rr * 0.785398;
        lead = min(min(spoke, abs(rr - g_roseR * 0.45)), g_roseR - rr);
        c = palettePick(hash12(vec2(sec + 11.0 * ring, bay * 3.1 + 7.0)));
        c *= mix(1.3, 1.0, ring);
    } else {
        vec2 q = vec2(v + y, y - v) * (1.0 / 0.32);
        vec2 id = floor(q);
        vec2 f = fract(q) - 0.5;
        lead = (0.5 - max(abs(f.x), abs(f.y))) * 0.32 * 0.7071;
        c = palettePick(hash12(id + bay * 17.0));
        lead = min(lead, abs(v) - 0.035);
        lead = min(lead, abs(rr - g_roseR) - 0.03);
    }
    return c * smoothstep(0.012, 0.028, lead);
}

// Psychedelic glass: a rotating kaleidoscope of hex jewels flowing outward,
// colours melting downward, recursive arch tracery in the lead.
vec3 glassTrip(float v, float y, float bay) {
    // drips: the glass sags in smooth columns
    float sag = pow(0.5 + 0.5 * sin(v * 9.0 + bay * 2.1 + sin(v * 23.0)), 4.0) * (0.5 + 0.5 * sin(g_time * 0.7 + bay));
    float yy = y + g_melt * 0.6 * sag;
    vec2 c = vec2(v, yy - g_kcY);
    float r = length(c);
    float a = atan(c.y, c.x) + g_time * 0.3 * (mod(bay, 2.0) * 2.0 - 1.0);
    float seg = TAU / g_glassN;
    a = abs(mod(a, seg) - 0.5 * seg);
    vec2 k = r * vec2(cos(a), sin(a));
    vec4 hx = hexCell(k * 5.5 - vec2(g_time * 0.9, 0.0));
    float h = hash12(hx.zw + bay * 7.0);
    float e = hexEdge(hx.xy);
    vec3 col = trippy(h * 0.5 + r * 0.6 + (yy + g_time * 0.6) * 0.18 + bay * 0.13);
    // faceted jewel shading inside each cell
    float facet = 0.5 + 0.5 * cos(6.0 * atan(hx.y, hx.x) + g_time * 2.0 + h * 9.0);
    col *= 0.55 + 0.45 * smoothstep(0.0, 0.3, e) + 0.35 * facet * smoothstep(0.1, 0.35, e);
    col += vec3(1.0) * exp(-length(hx.xy) * 9.0) * 0.6 * (0.5 + 0.5 * sin(g_time * 3.0 + h * 20.0));
    float rings = 0.5 + 0.5 * sin(r * 18.0 - g_time * 4.0);
    col *= 0.8 + 0.4 * rings;
    float lead = e / 5.5;
    lead = min(lead, archNest(v, y, g_ww, g_ys, g_rec) - 0.006);
    return col * smoothstep(0.006, 0.018, lead);
}

vec3 glassAt(float z, float y) {
    float bay = floor(z / g_B);
    float v = z - g_B * (bay + 0.5);
    float d = windowSDF(v, y);
    if (d > 0.0) return vec3(0.0);
    float edge = smoothstep(0.0, -0.03, d);
    float kw = smoothstep(0.1, 0.7, g_Tc);
    vec3 c = vec3(0.0);
    if (kw < 1.0) c = glassCalm(v, y, bay) * (1.0 - kw);
    if (kw > 0.0) c += glassTrip(v, y, bay) * kw * 0.75;
    return c * edge;
}

// Blurred glass for shafts and dust (v1), plus a swirling hue drift with trip.
vec3 glassSoft(float z, float y) {
    float bay = floor(z / g_B);
    float v = z - g_B * (bay + 0.5);
    float d = windowSDF(v, y);
    if (d > 0.18) return vec3(0.0);
    float edge = smoothstep(0.18, -0.18, d);
    vec3 c;
    if (length(vec2(v, y - g_roseY)) < g_roseR) {
        c = palettePick(hash12(vec2(bay * 3.1 + 7.0, 5.0))) * 0.95;
    } else {
        float zone = floor((y - SILL_Y) / 0.75);
        float sideW = step(0.0, v);
        vec3 c1 = palettePick(hash12(vec2(zone + sideW * 7.0, bay * 17.0)));
        vec3 c2 = palettePick(hash12(vec2(zone + sideW * 7.0 + 31.0, bay * 17.0)));
        c = mix(c1, c2, 0.4) * 0.8;
    }
    if (g_Tc > 0.0) {
        // swirling, streaked colour: bright ribbons with dark gaps between
        vec3 tc = trippy((y + g_time * 0.6) * 0.22 + z * 0.07 + v * 0.3) * 0.8;
        float streak = 0.5 + 0.5 * sin(y * 5.0 + v * 7.0 + sin(z * 1.3 + g_time) * 2.0 - g_time * 2.2);
        tc *= 0.15 + 1.1 * streak * streak;
        c = mix(c, tc, 0.8 * g_Tc);
    }
    return c * edge;
}

vec3 windowLightEx(vec3 p, bool detailed, float caOff) {
    float k = (g_W + GLASS_D - p.x) / (-g_L.x);
    if (k <= 0.0) return vec3(0.0);
    vec3 g = p - g_L * k;
    vec3 c;
    if (caOff > 0.0) {
        c = vec3(glassSoft(g.z + caOff, g.y).r, glassSoft(g.z, g.y).g, glassSoft(g.z - caOff, g.y + caOff * 0.5).b);
    } else {
        c = glassSoft(g.z, g.y);
    }
    if (detailed) {
        float sharp = 1.0 - smoothstep(0.8, 3.5, k);
        if (sharp > 0.01) c = mix(c, glassAt(g.z, g.y), sharp);
    }
    return c * (g_shaftGain + 2.0 * dot(pulseAt(g.z), vec3(0.333)));
}

// ---------------------------------------------------------------------------
// Scene SDF (v1 cathedral, evaluated in warped coordinates)
// 1 wall/vault stone, 2 floor, 3 glass, 4 piers and ribs
// ---------------------------------------------------------------------------
float vaultVoid(float ax, float y) {
    if (y < g_H) return ax - g_W;
    return length(vec2(ax - (g_W - g_R), y - g_H)) - g_R;
}

vec2 mapV1(vec3 p) {
    float ax = abs(p.x);
    float u = p.z - g_B * floor(p.z / g_B);
    float v = u - 0.5 * g_B;
    float w = (u > 0.5 * g_B) ? u - g_B : u;

    float vd = vaultVoid(ax, p.y);
    float stone = -vd;
    float wn = windowSDF(v, p.y);
    float niche = max(wn, abs(ax - (g_W + GLASS_D * 0.5 - 0.05)) - (GLASS_D * 0.5 + 0.05));
    stone = max(stone, -niche);
    g_glassD = max(wn, abs(ax - (g_W + GLASS_D)));

    vec2 res = vec2(stone, 1.0);
    if (ax > g_W + GLASS_D - 0.03 && wn < 0.0) res.y = 3.0;

    float fl = p.y;
    if (fl < res.x) res = vec2(fl, 2.0);

    vec2 pc = vec2(ax - g_W, w);
    float pier = length(pc) - 0.30;
    pier = min(pier, length(pc + vec2(0.30, 0.0)) - 0.11);
    pier = min(pier, length(vec2(pc.x + 0.17, abs(pc.y) - 0.25)) - 0.10);
    pier = max(pier, p.y - g_H);
    float plinth = sdBox(vec3(pc.x, p.y - 0.22, pc.y), vec3(0.50, 0.22, 0.48)) - 0.02;
    float capital = sdBox(vec3(pc.x + 0.05, p.y - (g_H - 0.12), pc.y), vec3(0.42, 0.12, 0.42)) - 0.02;
    float pr = min(pier, min(plinth, capital));

    float hug = max(vd + 0.16, g_H - 0.05 - p.y);
    float transverse = max(abs(w) - 0.15, hug);
    float hb = 0.5 * g_B;
    float vv = abs(v);
    float dpl = abs(ax * hb - g_W * (hb - vv)) * inversesqrt(hb * hb + g_W * g_W);
    float diag = max(dpl - 0.08, max(vd + 0.12, g_H - p.y));
    float ridge = max(ax - 0.08, max(vd + 0.12, g_H - p.y));
    pr = min(pr, min(transverse, min(diag, ridge)));

    if (pr < res.x) res = vec2(pr, 4.0);
    return res;
}

// Liquid streaks running down walls, vault and glass
float dripField(vec3 q) {
    float x = q.z * 3.0 + (q.x > 0.0 ? 0.37 : 0.0);
    float id = floor(x);
    float f = fract(x) - 0.5;
    float h = hash12(vec2(id, q.x > 0.0 ? 7.0 : 3.0));
    float prof = 0.5 + 0.5 * cos(TAU * f);
    prof = prof * prof * prof;
    float flow = 0.5 + 0.5 * sin(q.y * 1.8 + g_time * (1.2 + 1.5 * h) + h * 30.0);
    return prof * (0.35 + 0.65 * flow * flow) * (0.5 + h);
}

vec2 map(vec3 p) {
    vec3 q = warp(p);
    vec2 r = mapV1(q);
    if (r.y < 1.5 || r.y > 2.5) {
        float mask = max(smoothstep(g_W - 0.7, g_W, abs(q.x)), smoothstep(g_H - 0.3, g_H + 0.5, q.y));
        r.x -= g_melt * 0.07 * dripField(q) * mask;
    }
    r.x += g_ripple * sin(q.y * 2.5 + q.z * 1.7 - g_time * 2.2) * sin(q.z * 1.3 + q.x * 2.0 + g_time * 1.1);
    r.x *= g_stepFac;
    return r;
}

vec3 calcNormal(vec3 p, float t) {
    float e = 0.001 * clamp(t, 1.0, 10.0);
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * e).x + k.yyx * map(p + k.yyx * e).x +
                     k.yxy * map(p + k.yxy * e).x + k.xxx * map(p + k.xxx * e).x);
}

float calcAO(vec3 p, vec3 n) {
    float occ = 0.0, sca = 1.0;
    for (int i = 0; i < 4; i++) {
        float h = 0.04 + 0.16 * float(i);
        occ += (h - map(p + n * h).x / max(g_stepFac, 0.2)) * sca;
        sca *= 0.7;
    }
    return clamp(1.0 - 1.6 * occ, 0.0, 1.0);
}

vec2 ashlar(vec3 p, vec3 n) {
    vec2 q = (abs(n.x) > 0.5) ? vec2(p.z, p.y) : vec2(p.x, p.y + p.z);
    q.y *= 1.0 / 0.42;
    float row = floor(q.y);
    q.x = q.x / 0.85 + 0.5 * mod(row, 2.0);
    vec2 id = vec2(floor(q.x), row);
    vec2 f = fract(q);
    float m = min(min(f.x, 1.0 - f.x) * 0.85, min(f.y, 1.0 - f.y) * 0.42);
    return vec2(smoothstep(0.008, 0.022, m), hash12(id));
}

// Branching fractal filigree (vault): fold-rotate-scale IFS of short segments.
float filigree(vec2 c) {
    vec2 p = vec2(mod(c.x, g_B) - 0.5 * g_B, mod(c.y + g_time * 0.12, 2.0) - 1.0);
    float d = 1e3, s = 1.0;
    for (int i = 0; i < 6; i++) {
        if (i > g_rec) break;
        p = abs(p) - vec2(0.40, 0.30);
        p = rot2(0.55 + 0.18 * sin(g_time * 0.23 + float(i) * 1.3)) * p;
        d = min(d, (abs(p.x) + max(abs(p.y) - 0.30, 0.0)) / s);
        p *= 1.7;
        s *= 1.7;
    }
    return d;
}

// Surface hyperspace ornament: hex lattice whose cells open into eyes or
// faceted jewels, nested arches crawling down the walls, branching filigree
// on the vault. Returns emissive colour (caOff: optional lookup shift).
vec3 ornament(vec3 q, float mat, float caOff) {
    bool isFloor = (mat > 1.5 && mat < 2.5);
    vec2 c = isFloor ? q.xz : vec2(q.z, q.y + (g_W - min(abs(q.x), g_W)));
    c.x += caOff;
    vec3 e = vec3(0.0);

    // hex lattice, crawling downward
    vec2 hp = c * 2.4 + vec2(0.0, g_time * 0.35);
    vec4 hx = hexCell(hp);
    float h = hash12(hx.zw);
    float edge = hexEdge(hx.xy);
    float wave = 0.5 + 0.5 * sin(hx.z * 0.9 + hx.w * 1.3 + g_time * 0.6 + h * TAU);
    float emerge = smoothstep(0.7, 1.0, wave + 0.3 * g_mids);
    vec3 jc = trippy(h * 0.7 + g_time * 0.05 + c.y * 0.05);
    float shimmer = 1.0 + 2.5 * g_treb * (0.5 + 0.5 * sin(h * 50.0 + g_time * 23.0));
    e += jc * exp(-edge * 45.0) * (0.03 + 0.6 * emerge) * shimmer;
    vec2 l = hx.xy;
    if (h < 0.5) {
        // eye: almond outline, glowing iris ring, slit pupil
        float almond = abs(l.y) - 0.23 * max(1.0 - (l.x * l.x) / (0.40 * 0.40), 0.0);
        float iris = length(l) - 0.12;
        float outline = smoothstep(0.03, 0.0, abs(almond));
        float ring = smoothstep(0.025, 0.0, abs(iris));
        float fill = step(iris, 0.0) * step(almond, 0.0);
        float pupil = step(abs(l.x), 0.025 + 0.02 * g_snap) * step(length(l), 0.1);
        vec3 ic = trippy(h * 3.0 + 0.5 + g_time * 0.2);
        e += emerge * (jc * outline * 1.2 + ic * (ring * 1.6 + fill * 0.7) * (1.0 - pupil));
    } else {
        // jewel: faceted hexagonal gem with rotating highlights
        float gem = smoothstep(0.02, 0.0, max(dot(abs(l), vec2(0.5, 0.866)), abs(l.x)) - 0.22);
        float facet = 0.5 + 0.5 * cos(6.0 * atan(l.y, l.x) + g_time * 2.5 + h * 9.0);
        e += emerge * gem * trippy(h * 2.0 + facet * 0.3) * (0.4 + 1.2 * facet * facet);
    }

    if (!isFloor) {
        if (q.y > g_H + 0.1) {
            // vault: branching fractal ribs of light
            float fd = filigree(vec2(c.x, c.y - g_H));
            e += trippy(c.y * 0.15 - g_time * 0.15) * exp(-fd * 70.0) * 0.8 * shimmer;
        } else {
            // walls and piers: nested arches crawling downward
            float P = g_B * 0.25;
            float ax = mod(c.x, P) - 0.5 * P;
            float ay = mod(c.y + g_time * 0.25, 1.6);
            float ad = archNest(ax, ay, P * 0.42, 0.9, g_rec - 1);
            e += trippy(c.x * 0.1 + c.y * 0.2 + g_time * 0.1) * exp(-ad * 70.0) * 0.45;
        }
    }
    return e * g_patI * 0.7;
}

// ---------------------------------------------------------------------------
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    g_time = iTime;

    // ---- layout
    g_B = clamp(u_bayLength, 2.0, 6.0);
    g_W = clamp(u_naveWidth, 1.5, 4.0);
    g_H = clamp(u_naveHeight, 3.5, 10.0);
    g_sharp = clamp(u_archSharpness, 0.0, 1.5);
    g_R = g_W * (1.0 + g_sharp);
    g_ww = clamp(g_B * 0.22, 0.4, 1.0);
    g_wR = g_ww * (1.0 + g_sharp);
    float headH = sqrt(max(g_wR * g_wR - (g_wR - g_ww) * (g_wR - g_ww), 0.0));
    g_ys = max(g_H - 0.35 - headH, SILL_Y + 0.8);
    g_wTop = g_ys + headH;
    g_roseR = g_ww * 0.62;
    g_roseY = g_ys + headH * 0.35;
    g_kcY = 0.5 * (SILL_Y + g_wTop);
    g_L = normalize(LIGHT_DIR);

    // ---- audio conditioning
    float react = clamp(u_audioReact, 0.0, 2.0);
    float sens = clamp(u_audioSens, -3.0, 3.0);
    float bassPinned = smoothstep(0.85, 0.98, iAudioBandsAtt.x);
    float Lb  = bandLevel(iAudioBandsAtt.x, WIN_BASS.x, WIN_BASS.y, sens);
    float Lm  = bandLevel(iAudioBandsAtt.y, WIN_MID.x,  WIN_MID.y,  sens - 1.5 * bassPinned);
    float Lt  = bandLevel(iAudioBandsAtt.z, WIN_TREB.x, WIN_TREB.y, sens - 0.75 * bassPinned);
    float LtR = bandLevel(iAudioBands.z,    WIN_TREB.x, WIN_TREB.y, sens - 0.75 * bassPinned);
    float LbEff = mix(Lb, Lm, bassPinned);
    float bass = LbEff * react;
    float mids = Lm * react;
    float treb = max(Lt, LtR * 0.8) * react;
    float level = (LbEff + Lm + Lt) / 3.0 * react;
    g_mids = mids;
    g_treb = treb * clamp(u_trebleDust, 0.0, 2.0);

    // ---- beats (stateless): x = 1 at a fresh hit, decaying to 0
    float thr = clamp(u_beatThreshold, 0.05, 0.95);
    const float SPAN = 0.30;
    const float MAX_PR = 45.0;
    float pulseAmt = clamp(u_pulseIntensity, 0.0, 3.0) * min(react, 1.5);
    float drv[PULSES];
    drv[0] = Lm;
    drv[1] = Lb * (1.0 - bassPinned);
    float snap = 0.0;
    float xs[PULSES];
    for (int k = 0; k < PULSES; k++) {
        float x = clamp((drv[k] - thr) / SPAN, 0.0, 1.0);
        xs[k] = x;
        // freshness of a hit: high just after the envelope peaks, zero while a
        // sustained loud passage keeps the level pinned above the window
        snap = max(snap, smoothstep(0.55, 0.95, x) * smoothstep(0.0, 0.12, 1.0 - x));
    }
    g_snap = snap * min(react, 1.0) * min(pulseAmt, 1.5);

    // ---- trip intensity: master slider, lifted by the overall level
    g_T = clamp(u_trip, 0.0, 2.0) * (1.0 + 0.25 * level);
    g_Tc = clamp(g_T, 0.0, 1.0);

    // ---- colour: palette drives the base hues; cycle + beat hue jump
    g_hue = iTime * 0.25 * clamp(u_hueCycle, 0.0, 2.0) * smoothstep(0.0, 0.3, g_T) + g_snap * 2.2 * g_Tc;
    g_colP = hueShift(u_primaryColor, g_hue);
    g_colS = hueShift(u_secondaryColor, g_hue);
    g_colA = hueShift(u_accentColor, g_hue);

    // ---- warp parameters
    g_twist = 0.045 * clamp(u_twist, 0.0, 3.0) * g_T * (1.0 + 0.6 * min(mids * mids, 1.5));
    g_roll = 0.35 * g_Tc * sin(iTime * 0.07) * clamp(u_twist, 0.0, 3.0) / max(clamp(u_twist, 0.0, 3.0), 1.0);
    float folds = float(clamp(u_kaleidoFolds, 0, 12));
    g_kAmt = (folds >= 2.0) ? smoothstep(0.25, 0.9, g_T) : 0.0;
    g_kN = max(folds + 2.0 * floor(g_snap * 1.99), 2.0);
    g_kOn = max(mix(22.0, 6.0, clamp(g_T - 1.0, 0.0, 1.0)) - 12.0 * g_snap * g_Tc, 3.0);
    g_kC = 1.99 - 0.25 * TAU / g_kN;           // fold wedge centred on the window direction
    g_breath = min(0.04 * g_T * (1.0 + 1.3 * bass * bass + 0.5 * level), 0.18);
    g_ripple = min(0.03 * g_T * (1.0 + 1.2 * bass * bass), 0.12);
    g_melt = clamp(u_meltDrip, 0.0, 2.0) * g_T;
    g_melt = min(g_melt, 2.5);
    g_dispL = 1.0 + 3.0 * g_ripple + 0.07 * g_melt * 1.5 * 3.0;
    g_rec = clamp(u_recursion, 1, 5);
    g_patI = clamp(u_patternIntensity, 0.0, 3.0) * g_Tc * (0.7 + 0.9 * min(mids, 1.5)) * (g_T > 1.0 ? g_T : 1.0);
    g_ca = clamp(u_chromaticAb, 0.0, 3.0) * g_Tc * (0.5 + 1.2 * min(treb * treb, 1.5)) * (1.0 + 1.5 * g_snap);
    g_glassN = 6.0 + 2.0 * floor(g_snap * 1.99);
    g_shaftGain = clamp(u_windowGlow, 0.0, 3.0) * (1.0 + 1.1 * min(mids * mids, 2.25) * u_midGlassPulse);
    g_glassGain = g_shaftGain * (1.0 + 0.2 * g_Tc);

    // ---- camera: glide forward forever (surge proof from v1: never backwards)
    float glide = 1.1 * clamp(u_camSpeed, 0.0, 3.0);
    float surgeSig = 0.5 * (Lb + bandLevel(iAudioBandsAtt.y, WIN_MID.x, WIN_MID.y, sens));
    float surgeA = 0.42 * clamp(u_bassSurge, 0.0, 2.0) * min(react, 1.0) * glide;
    g_camZ = iTime * glide + surgeA * surgeSig;
    float T = iTime;

    // slow melting of the whole image (applied to the view ray only)
    float meltImg = clamp(u_meltDrip, 0.0, 2.0) * g_Tc;
    uv.x += 0.012 * meltImg * sin(uv.y * 4.0 + T * 0.7);
    uv.y += 0.025 * meltImg * (0.5 + 0.5 * sin(uv.x * 3.0 + T * 0.45)) * (0.5 + 0.5 * sin(T * 0.3));

    vec3 ro = vec3(0.10 * sin(T * 0.13), CAM_Y + 0.08 * sin(T * 0.21), g_camZ);
    float yaw = 0.10 * sin(T * 0.071) + 0.05 * sin(T * 0.13);
    float pitch = 0.08 + 0.06 * sin(T * 0.053);
    vec3 fwd = normalize(vec3(sin(yaw), pitch, cos(yaw)));
    vec3 side = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
    vec3 up = cross(fwd, side);
    float focal = 1.0 - 0.11 * min(bass * bass, 1.5) * clamp(u_bassSurge, 0.0, 2.0)
                      - 0.22 * g_snap * g_Tc;   // zoom punch on beats
    vec3 rd = normalize(fwd * focal + side * uv.x + up * uv.y);

    // ---- corridor pulses
    g_pC[0] = mix(g_colA, vec3(1.0), 0.35);
    g_pC[1] = mix(g_colP, vec3(1.0), 0.35);
    for (int k = 0; k < PULSES; k++) {
        float x = xs[k];
        g_pR[k] = (1.0 - x) * MAX_PR;
        g_pI[k] = pow(x, 0.6) * smoothstep(0.0, 0.06, 1.0 - x) * pulseAmt;
    }

    // ---- march, collecting window glow (bloom-like halo)
    int maxSteps = clamp(u_maxSteps, 32, STEP_CAP);
    float t = 0.05;
    float mat = 0.0;
    bool hit = false;
    float glowW = 0.0, glowZ = 0.0;
    for (int i = 0; i < STEP_CAP; i++) {
        if (i >= maxSteps) break;
        vec3 sp = ro + rd * t;
        vec2 h = map(sp);
        if (h.x < 0.0005 * t) { hit = true; mat = h.y; break; }
        float stepLen = h.x * 0.9;
        float gw = exp(-max(g_glassD, 0.0) * 3.0) * min(stepLen, 0.6) * exp(-t * 0.04);
        glowW += gw;
        glowZ += gw * sp.z;
        t += stepLen;
        if (t > MAX_DIST) break;
    }
    if (!hit && t < MAX_DIST) { hit = true; mat = map(ro + rd * t).y; }
    t = min(t, MAX_DIST);

    vec3 fogCol = mix(mix(g_colS, vec3(0.5), 0.6) * 0.035, trippy(0.6 + g_time * 0.02) * 0.02, g_Tc);
    vec3 col = fogCol;
    vec3 lightOnly = vec3(0.0);
    float caW = g_ca * 0.018;

    if (hit) {
        vec3 pos = ro + rd * t;
        float twistAng;
        vec3 q = warp(pos, twistAng);
        vec3 n = calcNormal(pos, t);
        // normal into the unwarped cathedral frame (twist only) for the sun term
        float an = atan(n.x, -n.y) + twistAng;
        vec3 nV = vec3(length(n.xy) * vec2(sin(an), -cos(an)), n.z);
        float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);

        if (mat > 2.5 && mat < 3.5) {
            vec3 g;
            if (caW > 0.0005) {
                g = vec3(glassAt(q.z + caW, q.y).r, glassAt(q.z, q.y).g, glassAt(q.z - caW, q.y + caW * 0.6).b);
            } else {
                g = glassAt(q.z, q.y);
            }
            float lum = (q.x > 0.0) ? 1.0 : 0.6;
            col = g * (g_glassGain * 1.3 * lum + 3.0 * dot(pulseAt(pos.z), vec3(0.333)));
            col += vec3(0.01);
        } else {
            float ao = calcAO(pos, n);
            vec3 alb;
            if (mat < 2.5 && mat > 1.5) {
                vec2 c = floor(q.xz / 0.9);
                float chk = mod(c.x + c.y, 2.0);
                alb = mix(vec3(0.30, 0.28, 0.26), vec3(0.09, 0.085, 0.08), chk);
                alb *= 0.9 + 0.2 * hash12(c);
            } else {
                vec2 a = ashlar(q, nV);
                alb = vec3(0.36, 0.33, 0.29) * (0.82 + 0.3 * a.y);
                alb *= mix(0.55, 1.0, a.x);
                if (mat > 3.5) alb = vec3(0.40, 0.37, 0.33);
            }
            alb *= mix(1.0, 0.45, g_Tc);             // deeper blacks when tripping
            float ndl = max(dot(nV, -g_L), 0.0);
            vec3 wl = windowLightEx(q, true, caW * 2.0) * ndl * 1.8 * clamp(u_shaftIntensity, 0.0, 3.0);
            lightOnly += wl;
            vec3 lit = alb * wl;
            lit += alb * mix(g_colS, vec3(0.6), 0.7) * 0.05 * ao * (0.6 + 0.4 * n.y) * mix(1.0, 0.5, g_Tc);
            lit += alb * vec3(0.9, 0.85, 0.75) * 0.05 * ao * (0.5 + 0.5 * abs(nV.x)) * mix(1.0, 0.5, g_Tc);
            vec3 pl = pulseAt(pos.z);
            lit += pl * (alb * 0.9 + 0.02) * ao;
            lightOnly += pl * 0.3;
            // iridescent thin-film sheen
            lit += trippy(fres * 1.7 + q.y * 0.08 + q.z * 0.03) * fres * 0.35 * g_Tc * ao;
            // hyperspace ornament (chromatic split on the emissive lines)
            if (g_patI > 0.001) {
                vec3 e = ornament(q, mat, 0.0);
                lit += e * (0.35 + 0.65 * ao);
                lightOnly += e;
            }
            col = lit;
        }
        float fogAmt = 1.0 - exp(-t * 0.03 * u_fog);
        col = mix(col, fogCol, fogAmt);
    }

    col += trippy(glowZ / max(glowW, 1e-4) * 0.08) * glowW * 0.012 * g_shaftGain * (0.5 + g_Tc);

    // ---- volumetric shafts, swirling with the warp
    float jit = fract(52.9829189 * fract(dot(fragCoord + 5.588238 * float(int(iTime * 60.0) % 64), vec2(0.06711056, 0.00583715))));
    {
        const int SH_N = 16;
        float tEnd = min(t, 32.0);
        float dt = tEnd / float(SH_N);
        float phase = 0.55 + 1.2 * pow(max(dot(rd, -g_L), 0.0), 5.0);
        vec3 acc = vec3(0.0);
        for (int i = 0; i < SH_N; i++) {
            float ts = (float(i) + jit) * dt;
            vec3 sp = ro + rd * ts;
            vec3 sq = warp(sp);
            float fade = exp(-ts * 0.03 * u_fog);
            acc += windowLightEx(sq, false, 0.0) * fade;
            acc += pulseAt(sp.z) * 0.06 * fade;
        }
        vec3 shafts = acc * dt * 0.045 * mix(1.0, 0.6, g_Tc) * phase * clamp(u_shaftIntensity, 0.0, 3.0) * (0.4 + 0.6 * u_fog);
        shafts = saturateCol(shafts, 1.0 + 0.6 * g_Tc);
        col += shafts;
        lightOnly += shafts;
    }

    // ---- dust motes: sparkling, iridescent, lit by the shafts
    {
        const int LAYERS = 7;
        const float LSTEP = 1.0;
        float dens = clamp(u_dustAmount, 0.0, 3.0) * (0.35 + 0.9 * min(g_treb, 1.5));
        float sparkle = 1.0 + 3.0 * min(g_treb * g_treb, 2.25);
        float z0 = floor(ro.z / LSTEP) * LSTEP;
        vec3 acc = vec3(0.0);
        if (dens > 0.0 && rd.z > 0.05) {
            for (int k = 1; k <= LAYERS; k++) {
                float zk = z0 + float(k) * LSTEP;
                float tk = (zk - ro.z) / rd.z;
                if (tk <= 0.25 || tk >= t) continue;
                vec3 pp = ro + rd * tk;
                float layer = floor(zk / LSTEP + 0.5);
                vec2 dr = vec2(0.05 * sin(iTime * 0.3 + layer), -0.03 * iTime + 0.02 * cos(iTime * 0.21 + layer * 1.7));
                vec2 g = (pp.xy + dr) / 0.16;
                vec2 id = floor(g);
                vec2 h = hash22(id + layer * 13.1);
                vec2 f = fract(g) - (0.2 + 0.6 * h);
                float exist = step(hash12(id * 1.7 + layer), 0.12 * dens);
                float r = length(f) * 0.16;
                float sz = 0.006 + 0.004 * h.x;
                float tw = 0.6 + 0.4 * sin(iTime * (3.0 + 6.0 * h.y) + h.x * 40.0);
                tw = mix(tw, tw * tw * sparkle, 0.7);
                float spr = exist * smoothstep(sz * (1.0 + 0.06 * tk), 0.0, r) * tw * smoothstep(0.3, 1.6, tk);
                vec3 lightHere = windowLightEx(warp(pp), false, 0.0);
                vec3 mc = mix(vec3(1.0), trippy(h.y + iTime * 0.3), 0.7 * g_Tc);
                acc += spr * (lightHere * 2.5 + vec3(0.015) + mc * 0.2 * g_Tc) * mc * exp(-tk * 0.05);
            }
        }
        col += acc;
        lightOnly += acc;
    }

    // ---- beat moment: screen-wide flash and expanding ring
    {
        float s = g_snap * g_Tc;
        float ringR = (1.0 - g_snap) * 1.5;
        float ring = exp(-pow((length(uv) - ringR) / 0.07, 2.0)) * s;
        vec3 rc = trippy(atan(uv.y, uv.x) / TAU * 2.0 + g_time * 0.2);
        col += rc * ring * 0.35 + trippy(g_time * 0.1) * pow(s, 3.0) * 0.10;
    }

    if (u_debugView == 2) col = lightOnly;

    // ---- grade: saturation, exposure, tonemap, contrast
    col = saturateCol(col, 1.0 + 0.5 * g_Tc);
    col *= clamp(u_exposure, 0.3, 3.0) * 1.5 * (1.0 + 0.15 * level);
    col = acesFilm(max(col, 0.0));
    col = pow(col, vec3(1.0 / 2.2));
    col = mix(col, col * col * (3.0 - 2.0 * col), 0.45 * g_Tc);   // punchier contrast
    col *= mix(0.72, 1.0, smoothstep(1.25, 0.35, length(uv)));
    col += (jit - 0.5) * 0.006;

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
        m += vec3(1.0)           * meterBar(px, 200.0, g_snap);
        m += vec3(1.0, 0.0, 1.0) * meterBar(px, 218.0, clamp(g_T * 0.5, 0.0, 1.0));
        col = max(col, m);
    }

    FragColor = vec4(col, 1.0);
}
