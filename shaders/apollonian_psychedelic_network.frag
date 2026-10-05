#version 330 core
out vec4 FragColor;

// RaymarchVibe built-in uniforms
uniform vec2 iResolution;
uniform float iTime;
uniform float iAudioAmp;
uniform vec4 iAudioBands;      // x:bass, y:mids, z:treble, w:overall (raw normalised 0..1)
uniform vec4 iAudioBandsAtt;   // Enveloped audio bands (attack/decay 0..1)
uniform sampler2D iChannel0;   // Upstream input / feedback node

// --- UI Controls: PALETTE SYNCHRONIZATION & CREATIVE PARAMETERS ---
// Primary control generates master color palette
uniform vec3 u_primaryColor = vec3(0.9, 0.25, 0.95); // {"widget":"color", "palette":true, "label":"Primary Solar Core"}

// Secondary controls sync from primary gradient positions
uniform vec3 u_secondaryColor = vec3(0.2, 0.6, 1.0); // {"widget":"color", "palette":true, "label":"Secondary Network Hue"}
uniform vec3 u_accentColor = vec3(1.0, 0.4, 0.1);    // {"widget":"color", "palette":true, "label":"Accent Solar Corona"}
uniform vec3 u_highlightColor = vec3(0.2, 0.95, 0.8); // {"widget":"color", "palette":true, "label":"Highlight Orbital Rings"}
uniform vec3 u_glowSecondary = vec3(0.65, 0.2, 1.0);  // {"widget":"color", "palette":true, "label":"Secondary Nebula Glow"}

// Non-palette controls: Geometry & Fractal Recursion
uniform float u_recursion = 6.0;         // {"widget":"slider", "min":2.0, "max":9.0, "step":1.0, "label":"Recursion Depth", "smooth":true}
uniform float u_scale = 1.85;            // {"widget":"slider", "min":1.0, "max":3.0, "step":0.02, "label":"Inversion Scale"}
uniform float u_sphere_radius = 0.85;    // {"widget":"slider", "min":0.1, "max":2.5, "step":0.02, "label":"Sphere Inversion Radius"}
uniform float u_organic_morph = 0.25;    // {"widget":"slider", "min":0.0, "max":1.0, "step":0.02, "label":"Organic Nature Morph"}

// Non-palette controls: Orbital Rings System
uniform float u_ring_count = 3.0;        // {"widget":"slider", "min":1.0, "max":6.0, "step":1.0, "label":"Orbital Ring Count"}
uniform float u_ring_radius = 1.5;       // {"widget":"slider", "min":0.5, "max":4.0, "step":0.05, "label":"Orbital Ring Radius"}
uniform float u_ring_thickness = 0.02;   // {"widget":"slider", "min":0.005, "max":0.1, "step":0.005, "label":"Ring Thickness"}
uniform float u_ring_tilt = 0.6;         // {"widget":"slider", "min":0.0, "max":1.57, "step":0.05, "label":"Ring Tilt Angle"}
uniform float u_ring_speed = 1.0;        // {"widget":"slider", "min":0.0, "max":3.0, "step":0.1, "label":"Ring Spin Speed"}

// Non-palette controls: Motion, Lighting & Audio Reactivity
uniform float u_speed = 0.5;             // {"widget":"slider", "min":0.0, "max":2.5, "step":0.05, "label":"Cosmic Evolution Speed"}
uniform float u_audio_react = 0.6;       // {"widget":"slider", "min":0.0, "max":1.5, "step":0.05, "label":"Audio Reactivity Multiplier"}
uniform float u_glow_intensity = 1.2;    // {"widget":"slider", "min":0.1, "max":4.0, "step":0.1, "label":"Glow Intensity"}
uniform float u_brightness = 1.0;        // {"widget":"slider", "min":0.2, "max":3.0, "step":0.1, "label":"Brightness"}
uniform bool u_enable_fog = true;        // {"label":"Enable Atmospheric Fog"}

// Cosmic void background
uniform vec3 u_bg_color_bottom = vec3(0.01, 0.003, 0.025); // {"widget":"color", "label":"Void Bottom Color"}
uniform vec3 u_bg_color_top = vec3(0.03, 0.01, 0.06);      // {"widget":"color", "label":"Void Top Color"}

// --- Constants ---
const float PI = 3.14159265359;
const float TAU = 6.28318530718;

// Global orbit trap for fractal coloring
vec4 g_orbitTrap = vec4(100.0);

// --- Noise & Color Helpers ---
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x*34.0)+1.0)*x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

float snoise(vec3 v) {
    const vec2 C = vec2(1.0/6.0, 1.0/3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy;
    vec3 x3 = x0 - D.yyy;
    i = mod289(i);
    vec4 p = permute(permute(permute(
        i.z + vec4(0.0, i1.z, i2.z, 1.0))
        + i.y + vec4(0.0, i1.y, i2.y, 1.0))
        + i.x + vec4(0.0, i1.x, i2.x, 1.0));
    float n_ = 0.142857142857;
    vec3 ns = n_ * D.wyz - D.xzx;
    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);
    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);
    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);
    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));
    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2,p2), dot(p3,p3)));
    p0 *= norm.x; p1 *= norm.y; p2 *= norm.z; p3 *= norm.w;
    vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 blackbody(float temp) {
    vec3 col = vec3(255.0);
    col.x = 56100000.0 * pow(temp, -1.5) + 148.0;
    col.y = 100.04 * log(temp) - 623.6;
    if (temp > 6500.0) {
        col.y = 35200000.0 * pow(temp, -1.5) + 184.0;
    }
    col.z = 194.18 * log(temp) - 1448.6;
    col = clamp(col, 0.0, 255.0) / 255.0;
    if (temp < 1000.0) col *= temp / 1000.0;
    return col;
}

float gyroid(vec3 p) {
    return dot(cos(p * 1.5707963), sin(p.yzx * 1.5707963));
}

// 2D Rotation matrix
mat2 rot2D(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, -s, s, c);
}

// --- 3D Apollonian Fractal & Multi-Orbital Network SDF ---
// Returns vec2(distance, material_id)
vec2 map(vec3 p) {
    // Multi-frequency audio drivers (both enveloped for smooth flow & raw for transient bursts)
    float bass = u_audio_react * iAudioBandsAtt.x;
    float mids = u_audio_react * iAudioBandsAtt.y;
    float treb = u_audio_react * iAudioBandsAtt.z;
    float overall = u_audio_react * iAudioBandsAtt.w;

    float raw_bass = u_audio_react * iAudioBands.x;
    float raw_treb = u_audio_react * iAudioBands.z;

    float time = iTime * u_speed;

    // Organic space warping (Nature & Flow)
    if (u_organic_morph > 0.001) {
        vec3 morph_p = p * (1.2 + bass * 0.8);
        float g = gyroid(morph_p + time * 0.2) * u_organic_morph * 0.3;
        float n = snoise(p * 1.8 + time * 0.15) * u_organic_morph * 0.2;
        p += vec3(g, n, g) * (1.0 + overall * 1.2);
    }

    // Central Solar Core Star Distance (pulsing with audio bass)
    float core_radius = 0.30 + bass * 0.35 + raw_bass * 0.15;
    float dCore = length(p) - core_radius;

    // --- Multi-Orbital Ring System ---
    float dRings = 10000.0;
    float ringMat = 2.0;

    int numRings = int(clamp(u_ring_count, 1.0, 6.0));
    float baseRadius = u_ring_radius * (1.0 + bass * 0.25);

    for (int r = 0; r < 6; r++) {
        if (r >= numRings) break;

        float ringIdx = float(r);
        vec3 pR = p;

        // Individual orbital ring inclination angle and audio wobble
        float tiltAngle = u_ring_tilt * (1.0 + ringIdx * 0.35) + sin(time * 0.2 + ringIdx) * 0.15 + mids * 0.2;
        float spinAngle = time * u_ring_speed * (0.8 + ringIdx * 0.3) * (r % 2 == 0 ? 1.0 : -1.0) + treb * 0.5;

        // Apply 3D orbital rotations
        pR.xy = rot2D(tiltAngle) * pR.xy;
        pR.xz = rot2D(spinAngle) * pR.xz;

        float rDist = baseRadius * (0.7 + ringIdx * 0.45);
        float distToAxis = length(pR.xz);

        // Ring gap/segmentation (cosmic Saturn-style ring pattern with audio pulses)
        float angle = atan(pR.z, pR.x);
        float ringSegments = sin(angle * (6.0 + ringIdx * 2.0) + spinAngle * 2.0);
        float gapFactor = smoothstep(-0.2, 0.3, ringSegments);

        float dSingleRing = max(abs(distToAxis - rDist) - u_ring_thickness * (1.0 + raw_treb * 0.5), abs(pR.y) - u_ring_thickness * 0.5);
        dSingleRing = max(dSingleRing, -gapFactor * 0.01); // Subtle gaps in ring

        if (dSingleRing < dRings) {
            dRings = dSingleRing;
            ringMat = 2.0 + ringIdx; // Distinct material IDs for each orbital ring
        }
    }

    // --- Apollonian Sphere Packing Fractal ---
    vec3 q = p;

    // Slow domain rotations driven by time & audio
    q.xz = rot2D(time * 0.12 + bass * 0.4) * q.xz;
    q.xy = rot2D(time * 0.08 + mids * 0.3) * q.xy;

    float scale = u_scale + bass * 0.3;
    float total_scale = 1.0;

    g_orbitTrap = vec4(100.0);

    // FIXED: Sphere Inversion Radius parameter now directly drives inversion threshold!
    float fixedRadius2 = u_sphere_radius * u_sphere_radius * (1.0 + treb * 0.6);
    float minRadius2 = 0.08 * u_sphere_radius;

    int iterations = int(u_recursion);
    for (int i = 0; i < iterations; i++) {
        // Modulo domain folding (Lattice symmetry)
        q = -1.0 + 2.0 * fract(0.5 * q + 0.5);

        float r2 = dot(q, q);

        // Record orbit traps for psychedelic coloring
        g_orbitTrap = min(g_orbitTrap, vec4(abs(q), r2));

        // Apollonian / Mandelbox sphere folding inversion
        if (r2 < minRadius2) {
            float k = fixedRadius2 / minRadius2;
            q *= k;
            total_scale *= k;
        } else if (r2 < fixedRadius2) {
            float k = fixedRadius2 / r2;
            q *= k;
            total_scale *= k;
        }

        // Apply scale & iterative polyhedral rotations
        q *= scale;
        total_scale *= scale;

        float iter_rot = time * 0.04 + float(i) * 0.25 + mids * 0.25;
        q.yz = rot2D(iter_rot) * q.yz;
        q.xz = rot2D(iter_rot * 1.2) * q.xz;
    }

    // Distance to inverted Apollonian spheres
    float dApollonian = (length(q) - 0.70) / total_scale;

    // Interdimensional Energy Filaments (Recursive Nature Connections)
    float dFilaments = length(q.xy) / total_scale - 0.006 * (1.0 + treb * 0.8);

    // Combine scene geometry with material IDs
    vec2 res = vec2(dApollonian, 1.0); // Material 1 = Apollonian Fractal Spheres

    if (dRings < res.x) res = vec2(dRings, ringMat);    // Material 2.0..7.0 = Orbital Rings
    if (dFilaments < res.x) res = vec2(dFilaments, 8.0); // Material 8 = Energy Filaments
    if (dCore < res.x) res = vec2(dCore, 9.0);          // Material 9 = Central Solar Core Star

    return res;
}

// --- Normal Calculation ---
vec3 calcNormal(vec3 p) {
    const float h = 0.0003;
    const vec2 k = vec2(1.0, -1.0);
    return normalize(k.xyy * map(p + k.xyy * h).x +
                     k.yyx * map(p + k.yyx * h).x +
                     k.yxy * map(p + k.yxy * h).x +
                     k.xxx * map(p + k.xxx * h).x);
}

// --- Iridescence & Shading ---
vec3 iridescence(vec3 normal, vec3 viewDir, float intensity) {
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.2);
    vec3 iridCol = hsv2rgb(vec3(fresnel * 2.0 + iTime * 0.15, 0.85, 1.0));
    return iridCol * intensity * fresnel;
}

// --- Material & Orbit Trap Color Mapping ---
vec3 getMaterialColor(float matID, vec3 pos, vec3 normal) {
    float bass = iAudioBandsAtt.x;
    float mids = iAudioBandsAtt.y;
    float treb = iAudioBandsAtt.z;
    float overall = iAudioBandsAtt.w;

    float raw_bass = iAudioBands.x;

    if (matID == 9.0) {
        // Central Solar Core Star: Balanced Blackbody emission modulated by audio
        float temp = 3500.0 + bass * 8000.0 + raw_bass * 4000.0;
        vec3 starCol = blackbody(temp) * u_primaryColor * (1.1 + bass * 0.8);
        return starCol;
    }

    if (matID == 1.0) {
        // Apollonian Fractal Spheres: Psychedelic Orbit Trap Shading
        g_orbitTrap.w = sqrt(max(g_orbitTrap.w, 0.0));

        float t1 = clamp(g_orbitTrap.x * 2.0, 0.0, 1.0);
        float t2 = clamp(g_orbitTrap.y * 2.0, 0.0, 1.0);
        float t3 = clamp(g_orbitTrap.z * 2.0, 0.0, 1.0);

        vec3 colA = mix(u_primaryColor, u_secondaryColor, t1);
        vec3 colB = mix(u_accentColor, u_highlightColor, t2);
        vec3 fracCol = mix(colA, colB, t3);

        // Position & audio-dependent HSV hue shifts
        float hueShift = fract(dot(pos, vec3(0.12, 0.18, 0.25)) + iTime * 0.05 + bass * 0.3);
        fracCol = mix(fracCol, hsv2rgb(vec3(hueShift, 0.9, 1.0)), 0.35);

        return fracCol * (0.9 + mids * 0.6);
    }

    if (matID >= 2.0 && matID <= 7.0) {
        // Multi-Orbital Rings with distinct gradient colors & audio flares
        float ringIdx = matID - 2.0;
        float ringPos = sin(atan(pos.z, pos.x) * (6.0 + ringIdx * 2.0) + iTime * (2.0 + ringIdx)) * 0.5 + 0.5;

        vec3 ringBaseA = mix(u_accentColor, u_highlightColor, ringIdx / 5.0);
        vec3 ringBaseB = mix(u_secondaryColor, u_primaryColor, ringIdx / 5.0);

        vec3 ringCol = mix(ringBaseA, ringBaseB, ringPos);
        return ringCol * (1.2 + treb * 1.5 + raw_bass * 0.8);
    }

    if (matID == 8.0) {
        // Interdimensional Energy Filaments
        vec3 filamentCol = u_glowSecondary * (1.5 + treb * 2.0);
        return filamentCol;
    }

    return u_primaryColor;
}

// --- Balanced Lighting ---
vec3 computeLighting(vec3 pos, vec3 normal, vec3 viewDir, vec3 baseColor, float matID) {
    if (matID == 9.0) return baseColor; // Central Solar core is self-luminous

    float bass = iAudioBandsAtt.x;
    float mids = iAudioBandsAtt.y;
    float treb = iAudioBandsAtt.z;

    // Central Solar Core Light
    vec3 lightPos1 = vec3(0.0);
    vec3 lightCol1 = blackbody(4500.0 + bass * 6000.0) * u_primaryColor * 1.2;

    // Orbiting Satellite Light 1
    vec3 lightPos2 = vec3(cos(iTime * 0.8) * 3.0, sin(iTime * 0.5) * 2.0, sin(iTime * 0.8) * 3.0);
    vec3 lightCol2 = u_secondaryColor * (0.8 + mids * 1.0);

    // Orbiting Satellite Light 2
    vec3 lightPos3 = vec3(sin(iTime * 0.6) * -3.5, cos(iTime * 0.7) * 2.5, cos(iTime * 0.6) * -3.5);
    vec3 lightCol3 = u_accentColor * (0.8 + treb * 1.0);

    vec3 totalLighting = baseColor * 0.2; // Ambient

    // Light 1 (Solar Core)
    vec3 lDir1 = normalize(lightPos1 - pos);
    float dist1 = length(lightPos1 - pos);
    float diff1 = max(dot(normal, lDir1), 0.0);
    float att1 = 1.0 / (1.0 + 0.3 * dist1 + 0.08 * dist1 * dist1);
    totalLighting += diff1 * baseColor * lightCol1 * att1;

    // Light 2 & 3
    vec3 lDir2 = normalize(lightPos2 - pos);
    float diff2 = max(dot(normal, lDir2), 0.0);
    float spec2 = pow(max(dot(viewDir, reflect(-lDir2, normal)), 0.0), 32.0);
    totalLighting += (diff2 * baseColor + spec2 * 0.4) * lightCol2;

    vec3 lDir3 = normalize(lightPos3 - pos);
    float diff3 = max(dot(normal, lDir3), 0.0);
    float spec3 = pow(max(dot(viewDir, reflect(-lDir3, normal)), 0.0), 32.0);
    totalLighting += (diff3 * baseColor + spec3 * 0.4) * lightCol3;

    // Add Iridescence on fractal surface
    if (matID == 1.0) {
        totalLighting += iridescence(normal, viewDir, 0.35);
    }

    return totalLighting;
}

// --- Balanced Volumetric Glow Accumulation ---
vec3 raymarchWithGlow(vec3 ro, vec3 rd, out vec2 materialInfo, out float hitT) {
    float t = 0.001;
    float maxDist = 30.0;
    vec3 accumulatedGlow = vec3(0.0);

    float bass = iAudioBandsAtt.x;
    float overall = iAudioBandsAtt.w;

    vec3 glowColorCore = u_accentColor * (0.8 + bass * 1.5);
    vec3 glowColorFractal = u_glowSecondary * (0.8 + overall * 1.5);

    for (int i = 0; i < 160; i++) {
        vec3 pos = ro + rd * t;
        vec2 res = map(pos);
        float d = res.x;

        // Controlled volumetric glow calculation (preventing whiteout exposure)
        float glowFactor = 0.0006 * u_glow_intensity / (0.005 + abs(d) * abs(d) * 20.0);
        if (res.y == 9.0) {
            accumulatedGlow += glowColorCore * glowFactor * 1.2;
        } else {
            accumulatedGlow += glowColorFractal * glowFactor;
        }

        if (d < 0.001) {
            materialInfo = res;
            hitT = t;
            return accumulatedGlow;
        }

        t += d * 0.75;
        if (t > maxDist) break;
    }

    materialInfo = vec2(-1.0, -1.0);
    hitT = maxDist;
    return accumulatedGlow;
}

// --- Atmospheric Fog ---
vec3 applyFog(vec3 color, float distance, vec3 rd) {
    if (!u_enable_fog) return color;
    float fogAmount = 1.0 - exp(-distance * 0.035);
    vec3 fogColor = mix(u_bg_color_bottom, u_glowSecondary * 0.15, 0.5 + 0.5 * rd.y);
    return mix(color, fogColor, fogAmount);
}

// --- ACES Filmic Tone Mapping ---
vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// --- Main Render Function ---
void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    float time = iTime * u_speed;
    float bass = iAudioBandsAtt.x;

    // Interactive / Evolving Camera Orbiting Path
    float camDist = 4.2 - bass * 0.8;
    float camAngleX = time * 0.15;
    float camAngleY = sin(time * 0.10) * 0.35;

    vec3 ro = vec3(camDist * sin(camAngleX) * cos(camAngleY),
                   camDist * sin(camAngleY),
                   camDist * cos(camAngleX) * cos(camAngleY));
    vec3 ta = vec3(0.0, 0.0, 0.0);

    // Camera look-at matrix
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
    vec3 vv = cross(uu, ww);
    vec3 rd = normalize(uv.x * uu + uv.y * vv + 1.4 * ww);

    // Raymarch with Volumetric Glow
    vec2 materialInfo;
    float hitT;
    vec3 glow = raymarchWithGlow(ro, rd, materialInfo, hitT);

    // Cosmic void background gradient
    float bgGradient = pow(0.5 - 0.5 * uv.y, 1.8);
    vec3 col = mix(u_bg_color_bottom, u_bg_color_top, bgGradient);

    if (hitT < 25.0 && materialInfo.x >= 0.0) {
        vec3 pos = ro + rd * hitT;
        vec3 normal = calcNormal(pos);
        vec3 viewDir = normalize(ro - pos);

        // Material surface color
        vec3 baseColor = getMaterialColor(materialInfo.y, pos, normal);

        // Lighting calculation
        vec3 lighting = computeLighting(pos, normal, viewDir, baseColor, materialInfo.y);

        col = lighting;

        // Apply atmospheric fog
        col = applyFog(col, hitT, rd);
    } else {
        // Starfield / Cosmic dust in background void with audio bursts
        float starSeed = fract(sin(dot(uv * 120.0 + time * 0.01, vec2(12.9898, 78.233))) * 43758.5453);
        if (starSeed > 0.994) {
            col += u_highlightColor * (starSeed - 0.994) * 150.0 * (1.0 + iAudioBandsAtt.w * 3.0);
        }
    }

    // Add accumulated volumetric glow
    col += glow;

    // Apply brightness parameter after tone mapping / properly scaled
    col *= u_brightness;

    // ACES Tone Mapping
    col = acesFilm(col);

    // Gamma correction
    col = pow(col, vec3(1.0 / 2.2));

    // Temporal Blending / Motion Persistence using input node on iChannel0
    vec2 screenUV = gl_FragCoord.xy / iResolution.xy;
    vec3 prevFrame = texture(iChannel0, screenUV).rgb;
    if (length(prevFrame) > 0.001) {
        col = mix(col, prevFrame, 0.12); // Subtle persistence trails
    }

    // Subtle Chromatic Aberration toward screen edges
    float distFromCenter = length(uv);
    if (distFromCenter > 0.25) {
        float caAmount = (distFromCenter - 0.25) * 0.008;
        col.r = mix(col.r, col.r * (1.0 + caAmount), 0.5);
        col.b = mix(col.b, col.b * (1.0 - caAmount), 0.5);
    }

    FragColor = vec4(col, 1.0);
}
