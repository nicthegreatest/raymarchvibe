#version 330 core
out vec4 FragColor;

// Infinite Cathedral Corridor
// An endless gothic nave, domain-repeated bay by bay along +z: clustered piers,
// pointed transverse arches, diagonal and ridge ribs in a pointed vault, and
// tall lancet windows with a rose in each head. Low sun on the +x side shines
// through the stained glass. Light is analytic: any point is traced back along
// the light direction to the glass plane and the glass pattern is read there.
// That gives coloured shafts in the air, coloured pools on the floor and the
// far wall, and lights the dust motes, with no extra marching.
//
// Audio (iAudioBandsAtt). App bands are FFT means / 32: bass is often pinned at
// 1.0 on loud masters, mids ~0.03-0.35, treble ~0.002-0.04. Each band is mapped
// to 0..1 in a log window (Audio Sensitivity shifts it in octaves). Pinned bass
// carries no beat, so it falls back to mids.
//   bass    -> forward surge plus FOV punch. The surge offset is bounded so its
//              fastest decay is slower than the glide: the camera never moves backwards.
//   beats   -> a band of light races down the corridor, flaring each window as it
//              passes. Stateless: the envelope decays at a steady rate in log units,
//              so the distance since the last hit maps to front distance.
//   mids    -> stained-glass brightness and shaft intensity
//   treble  -> dust mote density and sparkle
//   overall -> exposure
// The app scales iTime by its own speed control; nothing here multiplies by a
// speed uniform. Camera Speed paces the glide only.

uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;
uniform vec4 iAudioBandsAtt;

uniform vec3 u_primaryColor = vec3(0.85, 0.12, 0.10);   // {"widget":"color", "palette":true, "label":"Primary Color"}
uniform vec3 u_secondaryColor = vec3(0.10, 0.25, 0.85); // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 u_accentColor = vec3(1.0, 0.75, 0.20);     // {"widget":"color", "palette":true, "label":"Accent Color"}

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
uniform float u_trebleDust = 1.0;      // {"widget":"slider", "label":"Treble Dust", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_audioReact = 1.0;      // {"widget":"slider", "label":"Audio Reactivity", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_audioSens = 0.0;       // {"widget":"slider", "label":"Audio Sensitivity (octaves)", "min":-3.0, "max":3.0, "step":0.05, "smooth":true}
uniform float u_beatThreshold = 0.45;  // {"widget":"slider", "label":"Beat Threshold (hit level)", "min":0.1, "max":0.9, "step":0.01}
uniform float u_pulseIntensity = 1.0;  // {"widget":"slider", "label":"Pulse Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_fog = 1.0;             // {"widget":"slider", "label":"Fog", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_exposure = 1.0;        // {"widget":"slider", "label":"Exposure", "min":0.3, "max":3.0, "step":0.01, "smooth":true}
uniform float u_colorCycle = 0.0;      // {"widget":"slider", "label":"Colour Cycle", "min":0.0, "max":1.0, "step":0.01, "smooth":true}
uniform int u_maxSteps = 120;          // {"widget":"slider", "label":"Max Steps", "min":48, "max":220}
uniform int u_debugView = 0;           // {"widget":"slider", "label":"Debug View (0 off, 1 meters, 2 light only)", "min":0, "max":2}

const int STEP_CAP = 220;
const float MAX_DIST = 70.0;
const float GLASS_D = 0.30;          // window niche depth: glass plane sits at |x| = W + GLASS_D
const float SILL_Y = 1.3;
const vec3 LIGHT_DIR = vec3(-0.80, -0.50, 0.33);   // direction the sunlight travels (normalised in main)

// Per-frame state, set in main()
float g_W, g_H, g_R, g_B;            // nave half width, springing height, vault arc radius, bay length
float g_ww, g_wR, g_ys, g_wTop;      // window half width, head arc radius, head springing, apex
float g_roseY, g_roseR;
vec3 g_L;
vec3 g_colP, g_colS, g_colA;
float g_glassGain;                   // window glow incl. mids pulse
float g_camZ;

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

// ---------------------------------------------------------------------------
// Audio band conditioning (shared with Mandelbox Dive v2 / KIFS Temple)
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
// Windows: 2D shape in wall coordinates (v = offset from bay centre, y)
// ---------------------------------------------------------------------------
// Pointed lancet: straight jambs up to g_ys, then two arcs meeting at the apex.
float windowSDF(float v, float y) {
    float ax = abs(v);
    if (y < g_ys) return max(ax - g_ww, SILL_Y - y);
    return length(vec2(ax - (g_ww - g_wR), y - g_ys)) - g_wR;
}

vec3 palettePick(float h) {
    vec3 c = (h < 0.38) ? g_colS : ((h < 0.72) ? g_colP : g_colA);
    return c * (0.65 + 0.6 * fract(h * 7.31));
}

// Stained glass colour at a point on the window plane (0 outside the window).
// Two lancets of diamond quarries split by a mullion, a rose in the head.
vec3 glassAt(float z, float y) {
    float bay = floor(z / g_B);
    float v = z - g_B * (bay + 0.5);
    float d = windowSDF(v, y);
    if (d > 0.0) return vec3(0.0);
    float edge = smoothstep(0.0, -0.03, d);

    vec3 c;
    float lead;
    vec2 rp = vec2(v, y - g_roseY);
    float rr = length(rp);
    if (rr < g_roseR) {
        // rose: 8 petals in two rings, lead along ring and spokes
        float a = atan(rp.y, rp.x + 1e-6);
        float sec = floor((a + 3.14159265) / 0.785398);
        float ring = step(g_roseR * 0.45, rr);
        float fa = fract((a + 3.14159265) / 0.785398);
        float spoke = min(fa, 1.0 - fa) * rr * 0.785398;
        lead = min(min(spoke, abs(rr - g_roseR * 0.45)), g_roseR - rr);
        lead = min(lead, rr - 0.0);
        c = palettePick(hash12(vec2(sec + 11.0 * ring, bay * 3.1 + 7.0)));
        c *= mix(1.3, 1.0, ring);
    } else {
        // lancets: diamond quarries, a few bright medallions
        vec2 q = vec2(v + y, y - v) * (1.0 / 0.32);
        vec2 id = floor(q);
        vec2 f = fract(q) - 0.5;
        lead = (0.5 - max(abs(f.x), abs(f.y))) * 0.32 * 0.7071;
        c = palettePick(hash12(id + bay * 17.0));
        lead = min(lead, abs(abs(v) - 0.0) - 0.035);     // central mullion
        lead = min(lead, abs(rr - g_roseR) - 0.03);      // rose surround
    }
    float leadMask = smoothstep(0.012, 0.028, lead);
    return c * leadMask * edge;
}

// Blurred glass: what the window projects once the sun's disc has smeared
// the lead lines. Coarse colour zones, soft edges. Used for shafts and dust.
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
        c = mix(c1, c2, 0.4) * 0.8;                 // average of the quarries plus lead
    }
    return c * edge;
}

// Light arriving at point p through the +x windows (analytic back-trace).
// sharpMix 1 = full lead-line detail (near the glass), 0 = blurred.
vec3 windowLightEx(vec3 p, bool detailed) {
    float k = (g_W + GLASS_D - p.x) / (-g_L.x);
    if (k <= 0.0) return vec3(0.0);
    vec3 g = p - g_L * k;
    vec3 c = glassSoft(g.z, g.y);
    if (detailed) {
        float sharp = 1.0 - smoothstep(0.8, 3.5, k);
        if (sharp > 0.01) c = mix(c, glassAt(g.z, g.y), sharp);
    }
    return c * (g_glassGain + 2.0 * dot(pulseAt(g.z), vec3(0.333)));
}
vec3 windowLight(vec3 p) { return windowLightEx(p, false); }

// ---------------------------------------------------------------------------
// Scene SDF. Returns vec2(distance, materialID):
// 1 wall/vault stone, 2 floor, 3 glass, 4 piers and ribs
// ---------------------------------------------------------------------------
float vaultVoid(float ax, float y) {
    // negative inside the nave: vertical walls up to the springing, then a pointed vault
    if (y < g_H) return ax - g_W;
    return length(vec2(ax - (g_W - g_R), y - g_H)) - g_R;
}

vec2 map(vec3 p) {
    float ax = abs(p.x);
    float u = p.z - g_B * floor(p.z / g_B);          // 0..B within the bay
    float v = u - 0.5 * g_B;                          // from the bay centre (window axis)
    float w = (u > 0.5 * g_B) ? u - g_B : u;          // from the nearest pier line

    float vd = vaultVoid(ax, p.y);
    float stone = -vd;

    // window niches cut into the side walls, glass at the back
    float wn = windowSDF(v, p.y);
    float niche = max(wn, abs(ax - (g_W + GLASS_D * 0.5 - 0.05)) - (GLASS_D * 0.5 + 0.05));
    stone = max(stone, -niche);

    vec2 res = vec2(stone, 1.0);
    if (ax > g_W + GLASS_D - 0.03 && wn < 0.0) res.y = 3.0;

    float fl = p.y;
    if (fl < res.x) res = vec2(fl, 2.0);

    // clustered pier: core with three attached shafts, plinth and capital
    vec2 pc = vec2(ax - g_W, w);
    float pier = length(pc) - 0.30;
    pier = min(pier, length(pc + vec2(0.30, 0.0)) - 0.11);
    pier = min(pier, length(vec2(pc.x + 0.17, abs(pc.y) - 0.25)) - 0.10);
    pier = max(pier, p.y - g_H);
    float plinth = sdBox(vec3(pc.x, p.y - 0.22, pc.y), vec3(0.50, 0.22, 0.48)) - 0.02;
    float capital = sdBox(vec3(pc.x + 0.05, p.y - (g_H - 0.12), pc.y), vec3(0.42, 0.12, 0.42)) - 0.02;
    float pr = min(pier, min(plinth, capital));

    // ribs hugging the vault: transverse (over the piers), diagonal, ridge
    float hug = max(vd + 0.16, g_H - 0.05 - p.y);
    float transverse = max(abs(w) - 0.15, hug);
    float hb = 0.5 * g_B;
    float vv = abs(v);
    float dpl = abs(ax * hb - g_W * (hb - vv)) * inversesqrt(hb * hb + g_W * g_W);
    float diag = max(dpl - 0.08, max(vd + 0.12, g_H - p.y));
    float ridge = max(ax - 0.08, max(vd + 0.12, g_H - p.y));
    float ribs = min(transverse, min(diag, ridge));
    pr = min(pr, ribs);

    if (pr < res.x) res = vec2(pr, 4.0);
    return res;
}

vec3 calcNormal(vec3 p, float t) {
    float e = 0.0008 * clamp(t, 1.0, 10.0);
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * e).x + k.yyx * map(p + k.yyx * e).x +
                     k.yxy * map(p + k.yxy * e).x + k.xxx * map(p + k.xxx * e).x);
}

float calcAO(vec3 p, vec3 n) {
    float occ = 0.0, sca = 1.0;
    for (int i = 0; i < 4; i++) {
        float h = 0.04 + 0.16 * float(i);
        occ += (h - map(p + n * h).x) * sca;
        sca *= 0.7;
    }
    return clamp(1.0 - 1.6 * occ, 0.0, 1.0);
}

// Ashlar coursing on walls and vault: mortar lines and per-block tone
vec2 ashlar(vec3 p, vec3 n) {
    vec2 q = (abs(n.x) > 0.5) ? vec2(p.z, p.y) : vec2(p.x + p.z * 0.0, p.y + p.z);
    q.y *= 1.0 / 0.42;
    float row = floor(q.y);
    q.x = q.x / 0.85 + 0.5 * mod(row, 2.0);
    vec2 id = vec2(floor(q.x), row);
    vec2 f = fract(vec2(q.x, q.y));
    float m = min(min(f.x, 1.0 - f.x) * 0.85, min(f.y, 1.0 - f.y) * 0.42);
    return vec2(smoothstep(0.008, 0.022, m), hash12(id));
}

// ---------------------------------------------------------------------------
void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;

    // ---- layout
    g_B = clamp(u_bayLength, 2.0, 6.0);
    g_W = clamp(u_naveWidth, 1.5, 4.0);
    g_H = clamp(u_naveHeight, 3.5, 10.0);
    float sharp = clamp(u_archSharpness, 0.0, 1.5);
    g_R = g_W * (1.0 + sharp);
    g_ww = clamp(g_B * 0.22, 0.4, 1.0);
    g_wR = g_ww * (1.0 + sharp);
    float headH = sqrt(max(g_wR * g_wR - (g_wR - g_ww) * (g_wR - g_ww), 0.0));
    g_ys = max(g_H - 0.35 - headH, SILL_Y + 0.8);
    g_wTop = g_ys + headH;
    g_roseR = g_ww * 0.62;
    g_roseY = g_ys + headH * 0.35;
    g_L = normalize(LIGHT_DIR);

    float hue = iTime * 0.08 * u_colorCycle;
    g_colP = hueShift(u_primaryColor, hue);
    g_colS = hueShift(u_secondaryColor, hue);
    g_colA = hueShift(u_accentColor, hue);

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

    g_glassGain = clamp(u_windowGlow, 0.0, 3.0) * (1.0 + 1.1 * min(mids * mids, 2.25) * u_midGlassPulse);

    // ---- camera: glide forward forever
    // Surge signal: fixed-weight mean of bass and mids levels. The app envelope
    // decays exponentially, i.e. linearly in log units, so each level falls at
    // most ~1.0/s over these windows (bass 5.6 oct, mids 6.6 oct, tau 0.25 s).
    // With no pin-dependent weights the mean is bounded the same way, so the
    // offset recedes at most 0.95*A units/s < glide: the camera never moves backwards.
    // Pinned bass makes Lb constant and the mids carry the surge (the fallback).
    // (A LbEff crossfade jumped backwards when bass pinned; caught in simulation.)
    float glide = 1.1 * clamp(u_camSpeed, 0.0, 3.0);
    float surgeSig = 0.5 * (Lb + bandLevel(iAudioBandsAtt.y, WIN_MID.x, WIN_MID.y, sens));
    float surgeA = 0.42 * clamp(u_bassSurge, 0.0, 2.0) * min(react, 1.0) * glide;
    g_camZ = iTime * glide + surgeA * surgeSig;
    float T = iTime;
    vec3 ro = vec3(0.12 * sin(T * 0.13), 1.85 + 0.10 * sin(T * 0.21), g_camZ);
    float yaw = 0.10 * sin(T * 0.071) + 0.05 * sin(T * 0.13);
    float pitch = 0.16 + 0.06 * sin(T * 0.053);
    vec3 fwd = normalize(vec3(sin(yaw), pitch, cos(yaw)));
    vec3 side = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
    vec3 up = cross(fwd, side);
    float roll = 0.02 * sin(T * 0.09);
    side = side * cos(roll) + up * sin(roll);
    up = cross(fwd, side);
    float focal = 1.05 - 0.22 * min(bass * bass, 1.5) * clamp(u_bassSurge, 0.0, 2.0) * 0.5;   // FOV punch
    vec3 rd = normalize(fwd * focal + side * uv.x + up * uv.y);

    // ---- beat pulses racing down the corridor
    float thr = clamp(u_beatThreshold, 0.05, 0.95);
    const float SPAN = 0.30;
    const float MAX_PR = 45.0;
    float pulseAmt = clamp(u_pulseIntensity, 0.0, 3.0) * min(react, 1.5);
    float drv[PULSES];
    drv[0] = Lm;
    drv[1] = Lb * (1.0 - bassPinned);
    g_pC[0] = mix(g_colA, vec3(1.0), 0.35);
    g_pC[1] = mix(g_colP, vec3(1.0), 0.35);
    for (int k = 0; k < PULSES; k++) {
        float x = clamp((drv[k] - thr) / SPAN, 0.0, 1.0);
        g_pR[k] = (1.0 - x) * MAX_PR;
        g_pI[k] = pow(x, 0.6) * smoothstep(0.0, 0.06, 1.0 - x) * pulseAmt;
    }

    // ---- march
    int maxSteps = clamp(u_maxSteps, 32, STEP_CAP);
    float t = 0.05;
    float mat = 0.0;
    bool hit = false;
    for (int i = 0; i < STEP_CAP; i++) {
        if (i >= maxSteps) break;
        vec2 h = map(ro + rd * t);
        if (h.x < 0.0005 * t) { hit = true; mat = h.y; break; }
        t += h.x * 0.95;
        if (t > MAX_DIST) break;
    }
    if (!hit && t < MAX_DIST) { hit = true; mat = map(ro + rd * t).y; }
    t = min(t, MAX_DIST);

    vec3 fogCol = mix(g_colS, vec3(0.5), 0.6) * 0.035;
    vec3 col = fogCol;
    vec3 lightOnly = vec3(0.0);

    if (hit) {
        vec3 pos = ro + rd * t;
        vec3 n = calcNormal(pos, t);
        if (mat > 2.5 && mat < 3.5) {
            // stained glass seen from inside: emissive, flared by the passing pulse
            vec3 g = glassAt(pos.z, pos.y);
            float lum = (pos.x > 0.0) ? 1.0 : 0.55;      // the sun side is brighter
            col = g * (g_glassGain * 1.3 * lum + 3.0 * dot(pulseAt(pos.z), vec3(0.333)));
            col += vec3(0.01);                            // lead and tracery are not pure black
        } else {
            float ao = calcAO(pos, n);
            vec3 alb;
            if (mat < 2.5 && mat > 1.5) {
                // floor: two-tone marble checker
                vec2 c = floor(pos.xz / 0.9);
                float chk = mod(c.x + c.y, 2.0);
                alb = mix(vec3(0.30, 0.28, 0.26), vec3(0.09, 0.085, 0.08), chk);
                alb *= 0.9 + 0.2 * hash12(c);
            } else {
                vec2 a = ashlar(pos, n);
                alb = vec3(0.36, 0.33, 0.29) * (0.82 + 0.3 * a.y);
                alb *= mix(0.55, 1.0, a.x);
                if (mat > 3.5) alb = vec3(0.40, 0.37, 0.33);
            }
            // sunlight through the glass
            float ndl = max(dot(n, -g_L), 0.0);
            vec3 wl = windowLightEx(pos, true) * ndl * 1.8 * clamp(u_shaftIntensity, 0.0, 3.0);
            lightOnly += wl;
            vec3 lit = alb * wl;
            // soft skylight bouncing from the windows, cool from above
            lit += alb * mix(g_colS, vec3(0.6), 0.7) * 0.05 * ao * (0.6 + 0.4 * n.y);
            // gentle side fill from the window walls so the forms read
            lit += alb * vec3(0.9, 0.85, 0.75) * 0.05 * ao * (0.5 + 0.5 * abs(n.x));
            // the pulse sweeping over the stone
            vec3 pl = pulseAt(pos.z);
            lit += pl * (alb * 0.9 + 0.02) * ao;
            lightOnly += pl * 0.3;
            col = lit;
        }
        float fogAmt = 1.0 - exp(-t * 0.03 * u_fog);
        col = mix(col, fogCol, fogAmt);
    }

    // ---- volumetric shafts: in-scattering along the view ray
    // interleaved gradient noise, rotated per frame: finer, less blotchy grain than white noise
    float jit = fract(52.9829189 * fract(dot(fragCoord + 5.588238 * float(int(iTime * 60.0) % 64), vec2(0.06711056, 0.00583715))));
    {
        const int SH_N = 24;
        float tEnd = min(t, 32.0);
        float dt = tEnd / float(SH_N);
        float phase = 0.55 + 1.2 * pow(max(dot(rd, -g_L), 0.0), 5.0);
        vec3 acc = vec3(0.0);
        for (int i = 0; i < SH_N; i++) {
            float ts = (float(i) + jit) * dt;
            vec3 sp = ro + rd * ts;
            float fade = exp(-ts * 0.03 * u_fog);
            acc += windowLight(sp) * fade;
            acc += pulseAt(sp.z) * 0.06 * fade;
        }
        vec3 shafts = acc * dt * 0.045 * phase * clamp(u_shaftIntensity, 0.0, 3.0) * (0.4 + 0.6 * u_fog);
        col += shafts;
        lightOnly += shafts;
    }

    // ---- dust motes: sprites on fixed world planes, lit only inside the shafts
    {
        const int LAYERS = 9;
        const float LSTEP = 0.85;
        float dens = clamp(u_dustAmount, 0.0, 3.0) * (0.35 + 0.9 * min(treb, 1.5) * u_trebleDust);
        float sparkle = 1.0 + 3.0 * min(treb * treb, 2.25) * u_trebleDust;
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
                vec3 lightHere = windowLight(pp);
                acc += spr * (lightHere * 2.5 + vec3(0.015)) * exp(-tk * 0.05);
            }
        }
        col += acc;
        lightOnly += acc;
    }

    if (u_debugView == 2) col = lightOnly;

    // ---- tonemap
    col *= clamp(u_exposure, 0.3, 3.0) * 1.5 * (1.0 + 0.15 * level);
    col = acesFilm(max(col, 0.0));
    col = pow(col, vec3(1.0 / 2.2));
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
        m += vec3(1.0)           * meterBar(px, 200.0, g_pI[0] / max(pulseAmt, 1e-3));
        m += vec3(1.0)           * meterBar(px, 218.0, g_pI[1] / max(pulseAmt, 1e-3));
        col = max(col, m);
    }

    FragColor = vec4(col, 1.0);
}
