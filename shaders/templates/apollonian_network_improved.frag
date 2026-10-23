#version 330 core
out vec4 FragColor;

// Apollonian Network
// A flythrough of an Apollonian sphere packing. Repeated inversion of a plane
// under the gasket group turns that plane into tangent spheres: planets nested
// in planets. The bright cusps where those spheres kiss are the network edges.
// One primary palette color generates the harmony; Secondary, Accent and
// Highlight sync from it. Audio rides light and a little camera drift.
// It does not drive the packing: the app envelope snaps on the attack.
//
// Reviewed revision:
// - The app already advances iTime by the u_speed control, so the shader no
//   longer multiplies by u_speed again (that squared the speed and made the
//   scene lurch when the slider moved). FLOW keeps the default pacing.
// - App bands are FFT means / 32: bass often sits pinned at 1.0 on loud masters,
//   mids ~0.03-0.35, treble ~0.002-0.04. Each band is mapped to 0..1 in a log
//   window before use, and pinned bass falls back to mids so it still pulses.

uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBandsAtt;

// Primary generates the harmony. The other three sync along its gradient.
uniform vec3 PrimaryColor = vec3(0.62, 0.22, 0.95); // {"widget":"color", "palette":true, "label":"Primary Color"}
uniform vec3 SecondaryColor = vec3(0.15, 0.75, 0.82); // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 AccentColor = vec3(0.95, 0.35, 0.55); // {"widget":"color", "palette":true, "label":"Accent Color"}
uniform vec3 HighlightColor = vec3(1.0, 0.84, 0.45); // {"widget":"color", "palette":true, "label":"Highlight Color"}

uniform float u_speed = 0.65; // {"widget":"slider", "min":0.0, "max":2.0, "step":0.01, "smooth":true, "label":"Speed"}
// u_speed is read by the app itself: it scales the clock that drives iTime.
uniform float u_drift = 1.0; // {"widget":"slider", "min":0.0, "max":2.0, "step":0.01, "smooth":true, "label":"Drift"}
uniform float u_packing = 1.82; // {"widget":"slider", "min":1.35, "max":2.75, "step":0.01, "smooth":true, "label":"Packing"}
uniform int u_recursion = 11; // {"widget":"slider", "min":6, "max":14, "step":1, "label":"Recursion"}
uniform float u_body = 0.008; // {"widget":"slider", "min":0.0, "max":0.06, "step":0.001, "smooth":true, "label":"Body"}
uniform float u_melt = 0.55; // {"widget":"slider", "min":0.0, "max":2.0, "step":0.01, "smooth":true, "label":"Melt"}
uniform float u_glow = 1.25; // {"widget":"slider", "min":0.0, "max":2.5, "step":0.01, "smooth":true, "label":"Glow"}
uniform float u_network = 1.15; // {"widget":"slider", "min":0.0, "max":2.5, "step":0.01, "smooth":true, "label":"Network"}
uniform float u_sun = 1.25; // {"widget":"slider", "min":0.0, "max":3.0, "step":0.01, "smooth":true, "label":"Sun"}
uniform float u_iridescence = 0.9; // {"widget":"slider", "min":0.0, "max":2.0, "step":0.01, "smooth":true, "label":"Iridescence"}
uniform float u_exposure = 1.0; // {"widget":"slider", "min":0.3, "max":2.0, "step":0.01, "smooth":true, "label":"Exposure"}
uniform float u_fog = 0.05; // {"widget":"slider", "min":0.0, "max":0.25, "step":0.001, "smooth":true, "label":"Fog"}
uniform float u_audio = 1.0; // {"widget":"slider", "min":0.0, "max":2.0, "step":0.01, "label":"Audio Reactivity"}
uniform float u_audioSmooth = 1.5; // {"widget":"slider", "min":1.0, "max":4.0, "step":0.1, "label":"Audio Punch"}
uniform float u_audioSens = 0.0; // {"widget":"slider", "min":-3.0, "max":3.0, "step":0.05, "smooth":true, "label":"Audio Sensitivity (octaves)"}
uniform float u_rayDetail = 1.1; // {"widget":"slider", "min":0.5, "max":1.35, "step":0.01, "label":"Ray Detail"}

const int MAX_ITER = 14;
const int MAX_STEPS = 108;
const float FAR = 6.5;
// Pacing that matches the old default (iTime already includes u_speed).
const float FLOW = 0.65;
const float CUSP_K = 70.0;

// Orbit trap from the last map() call. Shading copies it before the normal
// and occlusion samples, which call map() again and overwrite this.
vec4 gTrap;
// Conditioned audio, computed once in main(): bass, mids, treble, overall.
vec4 gEnv = vec4(0.0);

vec2 rot2(vec2 p, float a) {
    float c = cos(a);
    float s = sin(a);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// Map a raw band into 0..1 over a log window [lo, hi]. Sensitivity slides the window.
float bandLevel(float v, float lo, float hi, float sens) {
    float a = log2(lo) - sens;
    float b = log2(hi) - sens;
    return clamp((log2(max(v, 1e-6)) - a) / (b - a), 0.0, 1.0);
}

// Windows fitted to measured app band ranges. A higher power ("Audio Punch")
// keeps the quiet floor dark and leaves the hits; it does not slow the attack.
vec4 audioEnv() {
    vec4 a = iAudioBandsAtt;
    float sens = clamp(u_audioSens, -3.0, 3.0);
    float pinned = smoothstep(0.85, 0.98, a.x);          // bass clipped by the app's /32 scale
    float b = bandLevel(a.x, 0.01, 0.5, sens);
    float m = bandLevel(a.y, 0.005, 0.5, sens - 1.5 * pinned);
    float t = bandLevel(a.z, 0.0005, 0.2, sens - 0.75 * pinned);
    b = mix(b, m, pinned);                                 // pinned bass carries no beat: follow mids
    vec3 l = pow(vec3(b, m, t), vec3(clamp(u_audioSmooth, 1.0, 4.0)));
    return vec4(l, (l.x + l.y + l.z) / 3.0) * clamp(u_audio, 0.0, 2.0);
}

vec3 sunPosition(float time) {
    float s = time * 0.16 * FLOW;
    return vec3(sin(s) * 0.36, cos(s * 0.79) * 0.20, cos(s) * 0.36);
}

vec3 moonPosition(float id, vec3 sun, float time) {
    float ang = time * FLOW * (0.27 + 0.11 * id) + id * 2.399963;
    float radius = 0.22 + id * 0.15 + gEnv.x * 0.03;
    float inc = 0.25 + 0.20 * id;
    return sun + vec3(cos(ang) * radius, sin(ang) * sin(inc) * radius * 0.65, sin(ang) * cos(inc) * radius);
}

// Distance in x, a generation hint in y. The y-plane is the seed surface:
// each inversion turns it into another generation of tangent spheres.
// Every sample must run the same iteration count. A per-iteration twist,
// a spatial warp, or an early exit changes the group and the copies stop meeting.
vec2 map(vec3 p) {
    // One rigid tumble of the whole packing. Melt only speeds it.
    // A single rotation is a congruence, so the spheres stay tangent.
    float tumble = iTime * FLOW * (0.04 + 0.035 * u_melt);
    p.xz = rot2(p.xz, tumble);
    p.xy = rot2(p.xy, tumble * 0.37);

    // Global breathing stays a valid packing. It is the same value everywhere.
    float packing = u_packing + 0.018 * u_melt * sin(iTime * 0.15 * FLOW);

    vec4 trap = vec4(1e5);
    float scale = 1.0;

    for (int i = 0; i < MAX_ITER; i++) {
        if (i >= u_recursion) break;

        p = -1.0 + 2.0 * fract(0.5 * p + 0.5);

        float r2 = max(dot(p, p), 1e-6);
        trap = min(trap, vec4(abs(p), r2));

        float k = packing / r2;
        p *= k;
        scale *= k;
    }

    gTrap = trap;

    // Thickness in world space. Subtracting u_body before dividing by scale
    // makes each generation a different shell, so the joints gap.
    float d = 0.25 * abs(p.y) / max(scale, 1e-6) - u_body;
    float generation = clamp(trap.w * 4.0, 0.0, 3.0);
    return vec2(d, generation);
}

vec3 calcNormal(vec3 p, float t) {
    float e = 0.00075 * clamp(t / 0.6, 0.5, 3.0);
    vec2 h = vec2(e, 0.0);
    vec3 n = vec3(
        map(p + h.xyy).x - map(p - h.xyy).x,
        map(p + h.yxy).x - map(p - h.yxy).x,
        map(p + h.yyx).x - map(p - h.yyx).x
    );
    float len = length(n);
    return len > 1e-5 ? n / len : vec3(0.0, 1.0, 0.0);
}

float starField(vec3 rd) {
    vec3 q = rd * 32.0;
    vec3 id = floor(q);
    vec3 f = fract(q) - 0.5;
    float n = hash13(id);
    vec3 jitter = vec3(
        hash13(id + vec3(1.2, 3.4, 5.6)),
        hash13(id + vec3(7.8, 9.1, 2.3)),
        hash13(id + vec3(4.5, 6.7, 8.9))
    ) - 0.5;
    float star = smoothstep(0.045, 0.0, length(f - jitter * 0.65)) * step(0.84, n);
    float twinkle = 0.55 + 0.45 * sin(iTime * 2.2 + n * 57.0);
    twinkle = mix(twinkle, 1.6, clamp(gEnv.z, 0.0, 1.0));
    return star * twinkle;
}

vec3 sky(vec3 rd, vec3 ro) {
    vec3 sun = sunPosition(iTime);
    vec3 sunDir = normalize(sun - ro);
    float sunDot = max(dot(rd, sunDir), 0.0);

    float grade = rd.y * 0.5 + 0.5;
    vec3 col = mix(PrimaryColor, SecondaryColor, 0.35 + 0.25 * grade) * 0.12;

    float arm = abs(sin(atan(rd.z, rd.x + 1e-6) * 2.0 - log(length(rd.xz) + 0.25) * 3.5 + iTime * FLOW * 0.05));
    col += SecondaryColor * pow(arm, 14.0) * 0.07;

    col += HighlightColor * starField(rd) * (1.0 - pow(sunDot, 4.0)) * 0.9;
    col += HighlightColor * pow(sunDot, 6.0) * 0.28 * u_sun;
    col += mix(HighlightColor, vec3(1.0), 0.55) * pow(sunDot, 220.0) * 1.4 * u_sun;
    return col;
}

vec3 oneMoon(vec3 pos, vec3 n, vec3 rd, vec3 albedo, vec3 sun, float id, vec3 lightColor, float power) {
    vec3 lv = moonPosition(id, sun, iTime) - pos;
    float d2 = dot(lv, lv);
    vec3 l = lv * inversesqrt(max(d2, 1e-4));
    float diff = clamp(dot(n, l) * 0.5 + 0.5, 0.0, 1.0);
    float spec = pow(clamp(dot(reflect(-l, n), -rd), 0.0, 1.0), 48.0);
    float atten = power / (1.0 + d2 * 5.0);
    return (albedo * lightColor * diff + lightColor * spec) * atten;
}

vec3 shade(vec3 pos, vec3 n, vec3 rd, vec3 ro, vec4 trap) {
    float seam = 1.0 - smoothstep(0.0, 0.16, min(trap.x, min(trap.y, trap.z)));
    seam *= seam;
    float core = exp(-trap.w * 7.0);
    float generation = clamp(trap.y * 2.4, 0.0, 1.0);

    vec3 albedo = mix(PrimaryColor, SecondaryColor, 0.55 + 0.45 * generation);
    albedo = mix(albedo, AccentColor, clamp(trap.x * 1.4, 0.0, 0.75));
    albedo = mix(albedo, HighlightColor, core * 0.25);

    float bands = 0.5 + 0.5 * sin(pos.y * 14.0 + trap.z * 6.0 + iTime * FLOW * 0.05);
    albedo *= 0.90 + 0.10 * bands;

    vec3 sun = sunPosition(iTime);
    vec3 lv = sun - pos;
    float dist2 = dot(lv, lv);
    vec3 l = lv * inversesqrt(max(dist2, 1e-4));

    float lambert = clamp(dot(n, l), 0.0, 1.0);
    float wrap = clamp(dot(n, l) * 0.4 + 0.6, 0.0, 1.0);
    float diff = mix(lambert, wrap, 0.6);
    float spec = pow(clamp(dot(reflect(-l, n), -rd), 0.0, 1.0), 64.0);
    float fres = pow(clamp(1.0 - dot(n, -rd), 0.0, 1.0), 2.4);

    float aoNear = clamp(map(pos + n * 0.05).x / 0.05, 0.0, 1.0);
    float aoFar = clamp(map(pos + n * 0.18).x / 0.18, 0.0, 1.0);
    float ao = mix(0.58, 1.0, min(aoNear, aoFar * 0.85 + 0.15));

    vec3 ambient = PrimaryColor * 0.20 + SecondaryColor * 0.06;
    vec3 sunColor = mix(HighlightColor, vec3(1.0), 0.35);
    vec4 env = gEnv;
    float trebleSheen = 0.55 + env.z * 0.90;

    vec3 col = albedo * ambient * ao;
    col += albedo * sunColor * diff * u_sun * 1.2 / (1.0 + dist2 * 0.35) * ao;
    col += sunColor * spec * u_sun * trebleSheen;

    col += oneMoon(pos, n, rd, albedo, sun, 0.0, SecondaryColor, 0.70);
    col += oneMoon(pos, n, rd, albedo, sun, 1.0, AccentColor, 0.55);
    col += oneMoon(pos, n, rd, albedo, sun, 2.0, HighlightColor, 0.40);

    vec3 rim = mix(AccentColor, HighlightColor, fres);
    rim = mix(rim, rim.bgr * AccentColor, 0.35 * fres);
    col += rim * fres * (0.28 + u_iridescence * (1.0 + 0.6 * env.y));
    col += sky(reflect(rd, n), ro) * fres * (0.18 + 0.22 * u_iridescence);

    col += HighlightColor * seam * u_network * (0.40 + env.x * 1.10 + env.z * 0.40);
    col += AccentColor * seam * fres * u_network * 0.45;

    return col;
}

vec3 march(vec3 ro, vec3 rd, out float travel, out bool hit) {
    float t = 0.012;
    hit = false;
    vec3 glow = vec3(0.0);
    vec3 sun = sunPosition(iTime);
    int steps = int(clamp(u_rayDetail * 80.0, 36.0, float(MAX_STEPS)));
    vec4 env = gEnv;

    for (int i = 0; i < MAX_STEPS; i++) {
        if (i >= steps) break;

        vec3 p = ro + rd * t;
        vec2 res = map(p);
        float d = res.x;

        float seam = 1.0 - smoothstep(0.02, 0.16, min(gTrap.x, min(gTrap.y, gTrap.z)));
        // Tight falloff: inside the packing d is small almost everywhere, so a soft
        // falloff accumulated a uniform haze that washed the frame toward white.
        float cusp = exp(-abs(d) * CUSP_K);
        // Weight by step length: rays creeping along a surface take hundreds of tiny
        // steps, and an unweighted per-step sum turned that into a white haze.
        float dt = max(d * 0.82, 0.00045);
        float w = clamp(dt / 0.012, 0.05, 1.0);
        vec3 filament = mix(SecondaryColor, AccentColor, clamp(res.y / 3.0, 0.0, 1.0));
        glow += HighlightColor * cusp * seam * u_network * (0.016 + env.x * 0.020) * u_glow * w;
        glow += filament * cusp * 0.007 * u_glow * w;

        float ds = length(p - sun);
        float sunGlow = u_sun * 0.018 / (1.0 + ds * ds * 28.0);
        glow += mix(HighlightColor, vec3(1.0), 0.40) * sunGlow * (1.0 + env.w * 0.40) * clamp(dt / 0.012, 0.0, 2.5);

        vec3 rel = p - sun;
        rel.yz = rot2(rel.yz, 0.45 * sin(iTime * 0.05 * FLOW + 0.3));
        float ringRadius = 0.46 + env.x * 0.035;
        float ring = length(vec2(length(rel.xz) - ringRadius, rel.y));
        glow += AccentColor * u_sun * 0.006 / (1.0 + ring * ring * 140.0);

        float eps = 0.0005 * max(t, 0.15);
        if (d < eps) {
            hit = true;
            break;
        }

        t += max(d * 0.82, 0.00045);
        if (t > FAR) break;
    }

    // Running out of steps in this dense packing means a grazing surface, not sky.
    // Treating it as a miss punched dark cracks through the spheres.
    if (!hit && t < FAR) hit = true;

    travel = t;
    // Per-step sums scale with the step budget; normalise so Ray Detail does not change exposure.
    glow *= 80.0 / float(steps);
    glow *= 1.0 + env.w * 0.50;
    return glow;
}

void main() {
    gEnv = audioEnv();
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    float travelAmt = u_drift;
    float az = iTime * FLOW * 0.13 * travelAmt;
    float pol = sin(iTime * FLOW * 0.07) * 0.42 * clamp(travelAmt, 0.0, 1.5);
    vec4 env = gEnv;
    float rad = 0.50 + 0.16 * sin(iTime * FLOW * 0.19) * travelAmt;
    rad += env.x * 0.012;  // the envelope snaps on attack: keep the nudge small

    vec3 ro = vec3(cos(az) * cos(pol), sin(pol) * 0.85, sin(az) * cos(pol)) * rad;
    float azAhead = az + 0.55;
    vec3 target = vec3(cos(azAhead) * cos(pol), sin(pol) * 0.40, sin(azAhead) * cos(pol)) * rad * 0.35;

    vec3 toTarget = target - ro;
    if (dot(toTarget, toTarget) < 1e-6) toTarget = vec3(-1.0, 0.0, 0.0);
    vec3 fwd = normalize(toTarget);

    // Keep the eye in the void between spheres so the flight reads as a journey
    // through the packing rather than a swim inside a shell.
    for (int i = 0; i < 5; i++) {
        float clearance = map(ro).x;
        if (clearance > 0.07) break;
        ro -= fwd * (0.09 - min(clearance, 0.09));
    }

    toTarget = target - ro;
    fwd = normalize(toTarget);
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    if (abs(dot(fwd, worldUp)) > 0.98) worldUp = vec3(1.0, 0.0, 0.0);
    vec3 side = normalize(cross(fwd, worldUp));
    vec3 up = cross(side, fwd);

    float roll = sin(iTime * FLOW * 0.11) * 0.12 * travelAmt + env.y * 0.04;
    float cs = cos(roll);
    float sn = sin(roll);
    vec3 sideR = side * cs + up * sn;
    vec3 upR = up * cs - side * sn;
    vec3 rd = normalize(fwd * 0.90 + sideR * uv.x + upR * uv.y);

    float t;
    bool hit;
    vec3 glow = march(ro, rd, t, hit);
    vec3 pos = ro + rd * t;
    vec3 background = sky(rd, ro);
    vec3 color = glow;

    if (hit) {
        map(pos);
        vec4 trap = gTrap;
        vec3 n = calcNormal(pos, t);
        if (dot(n, rd) > 0.0) n = -n;
        color += shade(pos, n, rd, ro, trap);

        float fogAmt = clamp(1.0 - exp(-t * u_fog * 6.0), 0.0, 0.80);
        color = mix(color, background, fogAmt);
    } else {
        color += background;
    }

    color *= (1.05 + env.w * 0.25) * u_exposure;
    color = aces(max(color, 0.0));
    color = pow(color, vec3(1.0 / 2.2));

    vec2 q = uv;
    float vignette = smoothstep(1.05, 0.28, length(q));
    color *= mix(0.72, 1.0, vignette);

    float grain = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233)) + iTime) * 43758.5453);
    color += (grain - 0.5) * 0.012;

    FragColor = vec4(color, 1.0);
}
