#version 330 core
out vec4 FragColor;

// ============================================================================
// Neon Truchet City
// A raymarched slab of 3D Truchet tiles (three quarter-torus conduits per cubic
// cell, random per-cell orientation) sitting on a dark rain-slick ground, with
// glowing "via" nodes on top and a few towers rising out of the grid. The
// camera glides and banks overhead; it always stays above the tallest tower.
// Light pulses race along every conduit using the arc parameter of each
// quarter-torus segment.
//
// Audio (all optional - looks complete with no audio):
//   bass raw  (iAudioBands.x, thresholded) -> beat: extra bright pulse train +
//                                             pulse boost
//   bass att  (iAudioBandsAtt.x)           -> overall pulse speed
//   mids att  (iAudioBandsAtt.y)           -> base neon glow of the tubes
//   treble    (iAudioBands.z, raw)         -> sparkle/flicker on pulse heads
//   overall   (iAudioBandsAtt.w)           -> brightness / haze glow
// Input channels: none used.
// ============================================================================

// Standard uniforms (native main() mode: iResolution is vec2)
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;      // raw: x=bass, y=mids, z=treble, w=overall (0..1)
uniform vec4 iAudioBandsAtt;   // enveloped: same layout (0..1)

// --- UI Controls ---
uniform float u_camSpeed = 1.0;        // {"widget":"slider", "label":"Camera Speed", "min":0.0, "max":4.0, "step":0.01, "smooth":true}
uniform float u_camHeight = 4.0;       // {"widget":"slider", "label":"Camera Height", "min":3.3, "max":14.0, "step":0.01, "smooth":true}
uniform float u_tubeRadius = 0.05;     // {"widget":"slider", "label":"Tube Radius (rel. to cell)", "min":0.02, "max":0.16, "step":0.001, "smooth":true}
uniform float u_cellScale = 1.0;       // {"widget":"slider", "label":"Grid Cell Size", "min":0.5, "max":2.0, "step":0.01, "smooth":true}
uniform float u_pulseSpeed = 1.0;      // {"widget":"slider", "label":"Pulse Speed", "min":0.0, "max":4.0, "step":0.01, "smooth":true}
uniform float u_pulseDensity = 0.6;    // {"widget":"slider", "label":"Pulse Density", "min":0.2, "max":4.0, "step":0.01, "smooth":true}
uniform float u_pulseLength = 0.25;    // {"widget":"slider", "label":"Pulse Length", "min":0.05, "max":0.8, "step":0.01, "smooth":true}
uniform float u_audioReact = 1.0;      // {"widget":"slider", "label":"Audio Reactivity", "min":0.0, "max":2.0, "step":0.01, "smooth":true}
uniform float u_beatThreshold = 0.45;  // {"widget":"slider", "label":"Beat Threshold", "min":0.05, "max":0.95, "step":0.01}
uniform float u_glowIntensity = 1.0;   // {"widget":"slider", "label":"Glow Intensity", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_fogDensity = 0.035;    // {"widget":"slider", "label":"Haze / Fog Density", "min":0.0, "max":0.15, "step":0.001, "smooth":true}
uniform float u_sparkle = 1.0;         // {"widget":"slider", "label":"Treble Sparkle", "min":0.0, "max":3.0, "step":0.01, "smooth":true}
uniform float u_colorCycle = 0.1;      // {"widget":"slider", "label":"Colour Cycle Speed", "min":0.0, "max":1.0, "step":0.01, "smooth":true}
uniform int u_maxSteps = 100;          // {"widget":"slider", "label":"Max Raymarch Steps", "min":40, "max":200}

// Palette (primary generates, secondary/accent sync)
uniform vec3 u_primaryColor = vec3(0.10, 0.85, 1.00);   // {"widget":"color", "palette":true, "label":"Primary Color"}
uniform vec3 u_secondaryColor = vec3(1.00, 0.18, 0.75); // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 u_accentColor = vec3(1.00, 0.62, 0.12);    // {"widget":"color", "palette":true, "label":"Accent Color"}
// --- End UI Controls ---

const float PI = 3.14159265359;
const float TAU = 6.28318530718;
const int MAX_STEP_CAP = 200;
const int REFL_STEPS = 40;
const float MAX_DIST = 45.0;      // in cell units
const float SLAB_TOP = 1.0;       // one layer of Truchet cells: y in [0, 1]
const float TOWER_MAX = 1.5;      // towers rise at most this far above the slab
const float CAM_MIN_Y = 3.3;      // camera floor in cell units (> SLAB_TOP + TOWER_MAX)

// Material IDs (map().y)
const float MAT_GROUND = 0.0;
const float MAT_TUBE = 1.0;
const float MAT_VIA = 2.0;
const float MAT_TOWER = 3.0;

// Per-frame values set in main()
float g_R;          // tube radius (cell units)
float g_time;       // pulse clock
float g_beat;       // thresholded bass transient 0..1
float g_density;
float g_len;

// Written by map() for the closest surface
float g_segId;      // per-segment hash
float g_segS;       // 0..1 arc parameter along the segment (direction applied)
float g_pulse;      // pulse intensity at the closest tube point

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
float hash31(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}
float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Tube colour: cycles between primary and secondary (accent is kept for pulses)
vec3 tubeColor(float x) {
    return mix(u_primaryColor, u_secondaryColor, smoothstep(0.2, 0.8, 0.5 + 0.5 * sin(TAU * x)));
}

vec3 palette3(float x) {
    x = fract(x) * 3.0;
    vec3 a = u_primaryColor, b = u_secondaryColor, c = u_accentColor;
    if (x < 1.0) return mix(a, b, smoothstep(0.0, 1.0, x));
    if (x < 2.0) return mix(b, c, smoothstep(0.0, 1.0, x - 1.0));
    return mix(c, a, smoothstep(0.0, 1.0, x - 2.0));
}

float sdRoundBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

// Pulse train along a segment: x in 0..1 along the arc
float pulseTrain(float u, float id, float density, float speed, float len) {
    float x = fract(u * density - g_time * speed + id * 7.13);
    float tail = smoothstep(1.0 - len, 1.0, x);
    float carrier = step(fract(id * 31.7), 0.65);   // not every conduit carries pulses
    return tail * tail * tail * carrier;
}

// Quarter torus of radius 0.5 centred on the cell edge at (+.5,+.5) in the
// (u,v) plane, w = out-of-plane axis. Returns distance, writes arc param.
float qTorus(vec2 uv, float w, out float s) {
    vec2 pp = uv - 0.5;
    s = clamp(atan(-pp.y, -pp.x) / (0.5 * PI), 0.0, 1.0);
    return length(vec2(length(pp) - 0.5, w)) - g_R;
}

// Tower info for an xz column: x = has tower (0/1), y = height
vec2 towerInfo(vec2 cell) {
    float h = hash21(cell + 17.3);
    float has = step(h, 0.09);
    float height = 0.5 + TOWER_MAX * 0.75 * fract(h * 53.1) + 0.25;
    return vec2(has, min(height, TOWER_MAX));
}

// ---------------------------------------------------------------------------
// Scene SDF (cell units) - returns vec2(distance, materialID) per spec 2.4
// ---------------------------------------------------------------------------
vec2 map(vec3 p) {
    // Ground plane with a slight bevel at tile seams
    vec2 gq = abs(fract(p.xz) - 0.5);
    float seam = smoothstep(0.47, 0.5, max(gq.x, gq.y));
    float dGround = p.y + 0.01 * seam;
    vec2 res = vec2(dGround, MAT_GROUND);
    g_pulse = 0.0;

    // Truchet slab: cells with y index 0..1
    vec3 cell = floor(p);
    cell.y = clamp(cell.y, 0.0, SLAB_TOP - 1.0);
    vec3 q = p - cell - 0.5;
    float h = hash31(cell);

    // Random orientation: per-axis flips + cyclic axis permutation
    vec3 fl = sign(vec3(fract(h * 3.1), fract(h * 7.7), fract(h * 13.3)) - 0.5);
    q *= fl;
    float perm = fract(h * 29.7);
    if (perm < 0.333) q = q.yzx; else if (perm < 0.666) q = q.zxy;

    float s1, s2, s3;
    float d1 = qTorus(q.xy, q.z, s1);
    float d2 = qTorus(vec2(-q.y, q.z), q.x, s2);
    float d3 = qTorus(vec2(-q.x, -q.z), q.y, s3);
    float dT = d1; float sT = s1; float k = 0.0;
    if (d2 < dT) { dT = d2; sT = s2; k = 1.0; }
    if (d3 < dT) { dT = d3; sT = s3; k = 2.0; }
    float segId = fract(h * 91.7 + k * 0.371);
    if (fract(segId * 13.0) < 0.5) sT = 1.0 - sT;   // random flow direction
    if (dT < res.x) {
        res = vec2(dT, MAT_TUBE);
        g_segId = segId; g_segS = sT;
    }

    // Glowing "via" node where each top-layer conduit exits the slab
    vec2 colId = floor(p.xz);
    vec3 vp = p - vec3(colId.x + 0.5, SLAB_TOP, colId.y + 0.5);
    float dVia = length(vp) - g_R * 1.7;
    if (dVia < res.x) { res = vec2(dVia, MAT_VIA); g_segId = hash21(colId); g_segS = 0.5; }

    // Towers on a few columns
    vec2 tw = towerInfo(colId);
    if (tw.x > 0.5) {
        vec3 tp = p - vec3(colId.x + 0.5, SLAB_TOP + tw.y * 0.5, colId.y + 0.5);
        float dTow = sdRoundBox(tp, vec3(0.3, tw.y * 0.5, 0.3), 0.03);
        if (dTow < res.x) { res = vec2(dTow, MAT_TOWER); g_segId = hash21(colId + 3.7); g_segS = tw.y; }
    }

    // Pulse value for glow accumulation
    if (res.y == MAT_TUBE) {
        g_pulse = pulseTrain(g_segS, g_segId, g_density, 1.0, g_len)
                + g_beat * 1.5 * pulseTrain(g_segS, g_segId + 0.5, g_density * 2.0, 1.7, g_len * 0.7);
    }
    return res;
}

vec3 calcNormal(vec3 p, float eps) {
    vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * eps).x +
                     k.yyx * map(p + k.yyx * eps).x +
                     k.yxy * map(p + k.yxy * eps).x +
                     k.xxx * map(p + k.xxx * eps).x);
}

// Raymarch with neon-haze accumulation. Steps are clamped to the current
// cell's exit inside the slab because the SDF only looks at one cell.
float march(vec3 ro, vec3 rd, int maxSteps, float pixel, out vec3 haze, out bool hit) {
    float t = 0.0;
    haze = vec3(0.0);
    hit = false;
    vec3 inv = 1.0 / max(abs(rd), vec3(1e-4));
    for (int i = 0; i < MAX_STEP_CAP; i++) {
        if (i >= maxSteps) break;
        vec3 p = ro + rd * t;
        vec2 m = map(p);
        float d = m.x;
        if (m.y == MAT_TUBE || m.y == MAT_VIA) {
            vec3 c = (m.y == MAT_VIA) ? u_accentColor : tubeColor(g_segId + iTime * u_colorCycle * 0.1);
            float w = exp(-max(d, 0.0) * 25.0);
            haze += c * w * (0.15 + 3.0 * g_pulse) * 0.004;
        }
        if (d < max(0.0008, pixel * t)) { hit = true; break; }
        float stepLen = d * 0.9;
        if (p.y < SLAB_TOP + 0.2) {
            vec3 f = fract(p);
            vec3 tb = mix(f, 1.0 - f, step(0.0, rd)) * inv;
            stepLen = min(stepLen, min(tb.x, min(tb.y, tb.z)) + 0.01);
        }
        t += stepLen;
        if (t > MAX_DIST) break;
    }
    return t;
}

// Shade a hit (emissive-heavy neon look). Returns colour.
vec3 shade(vec3 p, vec3 rd, vec3 n, float mat, float segId, float segS, float mids, float treble, float cyc) {
    vec3 col = vec3(0.0);
    vec3 L = normalize(vec3(-0.4, 0.8, 0.3));
    float dif = clamp(dot(n, L), 0.0, 1.0);
    float fres = pow(1.0 - clamp(dot(n, -rd), 0.0, 1.0), 3.0);

    if (mat == MAT_TUBE) {
        vec3 c = tubeColor(segId + cyc);
        vec3 pc = mix(u_accentColor, c, 0.25 + 0.25 * sin(segId * 40.0));
        float base = (0.14 + 0.35 * mids) * u_glowIntensity;
        float pA = pulseTrain(segS, segId, g_density, 1.0, g_len);
        float pB = g_beat * pulseTrain(segS, segId + 0.5, g_density * 2.0, 1.7, g_len * 0.7);
        // pulse head sparkle (treble)
        float head = smoothstep(0.9, 1.0, fract(segS * g_density - g_time + segId * 7.13));
        float flick = hash21(vec2(floor(iTime * 30.0), segId * 97.0));
        float sparkle = u_sparkle * head * (0.15 + 2.5 * treble) * step(0.35, flick);
        float pulse = pA * (1.0 + 1.5 * g_beat) + pB * 2.0;
        col = c * 0.015 * (0.2 + dif);                       // dark body
        col += c * base * (0.2 + 0.8 * fres);                 // neon skin
        col += (pc * 2.4 * pulse + vec3(1.0) * pow(pulse, 4.0) * 0.6) * u_glowIntensity;
        col += mix(u_accentColor, vec3(1.0), 0.6) * sparkle * 2.0;
    } else if (mat == MAT_VIA) {
        vec3 c = mix(u_accentColor, palette3(segId + cyc), 0.3);
        float throb = 0.6 + 0.4 * sin(iTime * 2.0 + segId * TAU);
        col = c * (0.6 + 1.2 * mids + 2.0 * g_beat) * throb * u_glowIntensity;
    } else if (mat == MAT_TOWER) {
        vec3 c = palette3(segId * 0.5 + 0.33 + cyc);
        col = vec3(0.012, 0.014, 0.025) * (0.4 + dif);
        // window bands on vertical faces
        vec2 wuv = vec2((abs(n.x) > 0.5 ? p.z : p.x) * 10.0, p.y * 16.0);
        vec2 wid = floor(wuv);
        vec2 wf = fract(wuv);
        float on = step(0.55, hash21(wid + segId * 41.0 + floor(iTime * 0.5 + segId * 5.0)));
        float win = smoothstep(0.1, 0.2, wf.x) * smoothstep(0.9, 0.8, wf.x) * smoothstep(0.2, 0.3, wf.y) * smoothstep(0.8, 0.7, wf.y);
        win *= on * (1.0 - abs(n.y));
        col += c * win * (0.6 + 0.8 * mids) * u_glowIntensity;
        // neon roof outline
        col += c * smoothstep(0.85, 1.0, n.y) * 0.03;
        col += c * fres * 0.08;
    } else {
        // wet ground (reflection added by caller)
        vec2 gq = abs(fract(p.xz) - 0.5);
        float seam = smoothstep(0.46, 0.5, max(gq.x, gq.y));
        col = vec3(0.008, 0.009, 0.016) * (0.5 + dif);
        col += u_primaryColor * seam * 0.03;
    }
    return col;
}

// ---------------------------------------------------------------------------
void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // --- Audio (bounded) ---
    float react   = clamp(u_audioReact, 0.0, 2.0);
    float bassRaw = clamp(iAudioBands.x, 0.0, 1.0) * react;
    float bassAtt = clamp(iAudioBandsAtt.x, 0.0, 1.0) * react;
    float mids    = clamp(iAudioBandsAtt.y, 0.0, 1.0) * react;
    float treble  = clamp(iAudioBands.z, 0.0, 1.0) * react;
    float level   = clamp(iAudioBandsAtt.w, 0.0, 1.0) * react;
    g_beat = smoothstep(u_beatThreshold, u_beatThreshold + 0.15, bassRaw);

    // --- Global parameters (cell units) ---
    float cs = clamp(u_cellScale, 0.5, 2.0);
    g_R = clamp(u_tubeRadius, 0.02, 0.16);   // relative to the cell size
    // Pulse clock: smoothed bass speeds pulses up (bounded)
    g_time = iTime * u_pulseSpeed * 0.6 * (1.0 + 0.8 * min(bassAtt, 1.5));
    g_density = u_pulseDensity;
    g_len = clamp(u_pulseLength, 0.05, 0.95);
    float cyc = iTime * u_colorCycle * 0.1;

    // --- Camera: glide + bank overhead, always above the city ---
    float T = iTime * 0.6 * u_camSpeed / cs;
    float camY = max(u_camHeight / cs, CAM_MIN_Y);
    vec3 ro = vec3(T, camY + 0.3 * sin(iTime * 0.21), 5.0 * sin(T * 0.11) + 2.5 * sin(T * 0.047 + 1.0));
    vec3 ro2 = vec3(T + 0.5, 0.0, 5.0 * sin((T + 0.5) * 0.11) + 2.5 * sin((T + 0.5) * 0.047 + 1.0));
    vec2 heading = normalize(ro2.xz - ro.xz + vec2(1e-4, 0.0));
    vec3 fwdFlat = vec3(heading.x, 0.0, heading.y);
    vec3 ta = ro + fwdFlat * (camY * 1.1) - vec3(0.0, camY, 0.0);
    vec3 fw = normalize(ta - ro);
    // bank from path curvature
    float curv = -(5.0 * 0.11 * 0.11 * sin(T * 0.11) + 2.5 * 0.047 * 0.047 * sin(T * 0.047 + 1.0));
    float roll = clamp(curv * 8.0, -0.35, 0.35);
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 rt = normalize(cross(fw, up));
    vec3 cu = cross(rt, fw);
    vec3 rtR = rt * cos(roll) + cu * sin(roll);
    vec3 cuR = cu * cos(roll) - rt * sin(roll);
    vec3 rd = normalize(uv.x * rtR + uv.y * cuR + 1.4 * fw);

    // --- Background: dark synthwave sky ---
    float sky = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 bg = mix(u_secondaryColor * 0.035, vec3(0.002, 0.003, 0.008), smoothstep(0.35, 0.7, sky));
    vec3 hazeCol = mix(u_primaryColor, u_secondaryColor, 0.5) * 0.025;

    // --- Primary ray ---
    float pixel = 1.5 / iResolution.y;
    int maxSteps = clamp(u_maxSteps, 20, MAX_STEP_CAP);
    vec3 haze;
    bool hit;
    float t = march(ro, rd, maxSteps, pixel, haze, hit);
    vec3 col = bg;

    if (hit) {
        vec3 p = ro + rd * t;
        vec2 m = map(p);
        float mat = m.y;
        float segId = g_segId, segS = g_segS;
        vec3 n = calcNormal(p, max(0.0008, pixel * t));
        col = shade(p, rd, n, mat, segId, segS, mids, treble, cyc);

        // Rain-slick reflections on the ground and (weaker) on towers
        float reflAmt = (mat == MAT_GROUND) ? 1.0 : ((mat == MAT_TOWER) ? 0.3 * (1.0 - abs(n.y)) : 0.0);
        if (reflAmt > 0.0) {
            // ripple-perturbed normal for a wet look
            vec2 rp = p.xz * 9.0;
            vec3 nn = normalize(n + vec3(sin(rp.x + sin(rp.y * 1.3)) , 0.0, cos(rp.y + sin(rp.x * 0.7))) * 0.015);
            vec3 rr = reflect(rd, nn);
            vec3 ro2r = p + nn * 0.01;
            vec3 rHaze; bool rHit;
            float rt2 = march(ro2r, rr, REFL_STEPS, pixel * 2.0, rHaze, rHit);
            vec3 rc = mix(u_secondaryColor * 0.02, vec3(0.0), clamp(rr.y * 2.0, 0.0, 1.0));
            if (rHit) {
                vec3 rpnt = ro2r + rr * rt2;
                vec2 rm = map(rpnt);
                float rSeg = g_segId, rS = g_segS;
                vec3 rn = calcNormal(rpnt, 0.002);
                rc = shade(rpnt, rr, rn, rm.y, rSeg, rS, mids, treble, cyc);
                rc *= exp(-rt2 * 0.25);
            }
            rc += rHaze;
            float fr = 0.25 + 0.75 * pow(1.0 - clamp(dot(nn, -rd), 0.0, 1.0), 4.0);
            col += rc * fr * reflAmt * 0.7;
        }

        float fog = 1.0 - exp(-u_fogDensity * t * (0.6 + 0.4 * t * 0.08));
        col = mix(col, bg + hazeCol, clamp(fog, 0.0, 1.0));
    }

    // Neon haze from near misses (overall level lifts it)
    col += haze * u_glowIntensity * (0.6 + 0.6 * level) * (0.5 + 10.0 * u_fogDensity);

    // Overall level: brightness / bloom-ish lift
    col *= 1.1 * (1.0 + 0.3 * min(level, 1.5));

    // Vignette, tonemap, gamma
    vec2 q2 = gl_FragCoord.xy / iResolution.xy;
    col *= 0.5 + 0.5 * pow(16.0 * q2.x * q2.y * (1.0 - q2.x) * (1.0 - q2.y), 0.25);
    col = max(col, 0.0);
    col = col * (2.51 * col + 0.03) / (col * (2.43 * col + 0.59) + 0.14); // ACES approx
    col = pow(clamp(col, 0.0, 1.0), vec3(1.0 / 2.2));

    FragColor = vec4(col, 1.0);
}
