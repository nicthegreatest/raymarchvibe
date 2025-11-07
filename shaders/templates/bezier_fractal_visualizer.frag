#version 330 core

out vec4 FragColor;

// --- Mandatory Uniforms ---
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;     // x:bass, y:mids, z:treble, w:all
uniform float iFps;
uniform float iFrame;
uniform float iProgress;
uniform vec4 iAudioBandsAtt;   // Attack-smoothed audio bands

// --- Advanced UI Controls ---
uniform float u_complexity = 10.0; // {"widget":"slider", "min":1.0, "max":20.0, "label":"Fractal Complexity"}
uniform float u_speed = 0.5; // {"widget":"slider", "min":0.0, "max":3.0, "label":"Animation Speed"}
uniform float u_zoom = 1.2; // {"widget":"slider", "min":0.1, "max":5.0, "label":"Zoom"}
uniform vec3 u_color1 = vec3(0.1, 0.05, 0.2); // {"widget":"color", "label":"Background Color"}
uniform vec3 u_color2 = vec3(0.8, 0.3, 0.6); // {"widget":"color", "label":"Primary Curve Color"}
uniform vec3 u_color3 = vec3(0.2, 0.9, 1.0); // {"widget":"color", "label":"Secondary Curve Color"}
uniform vec3 u_color4 = vec3(1.0, 0.7, 0.2); // {"widget":"color", "label":"Accent Color"}
uniform float u_bass_react = 0.8; // {"widget":"slider", "min":0.0, "max":3.0, "label":"Bass Reactivity"}
uniform float u_mid_react = 0.5; // {"widget":"slider", "min":0.0, "max":2.0, "label":"Mid Reactivity"}
uniform float u_treble_react = 1.2; // {"widget":"slider", "min":0.0, "max":3.0, "label":"Treble Reactivity"}
uniform float u_line_width = 0.015; // {"widget":"slider", "min":0.001, "max":0.1, "label":"Line Width"}
uniform float u_glow_intensity = 1.5; // {"widget":"slider", "min":0.0, "max":3.0, "label":"Glow Intensity"}
uniform float u_kaleidoscope_segments = 8.0; // {"widget":"slider", "min":3.0, "max":16.0, "label":"Kaleidoscope"}
uniform float u_iridescence = 0.7; // {"widget":"slider", "min":0.0, "max":2.0, "label":"Iridescence"}

// --- Mathematical Constants ---
const float PI = 3.14159265359;
const float TAU = 6.28318530718;
const float PHI = 1.61803398875;

// --- Advanced Noise Functions ---
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x*34.0)+1.0)*x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

float snoise(vec3 v) {
    const vec2 C = vec2(1.0/6.0, 1.0/3.0);
    const vec4 D = vec2(0.0, 0.5, 1.0, 2.0).xyxy;
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
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;
    vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

// Worley noise for cellular patterns
float worley(vec2 p) {
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    float d = 1.0;
    for(int xo = -1; xo <= 1; xo++)
    for(int yo = -1; yo <= 1; yo++) {
        vec2 cellPos = vec2(float(xo), float(yo));
        float rand1 = fract(sin(dot(ip + cellPos, vec2(12.9898, 78.233))) * 43758.5453);
        float rand2 = fract(sin(dot(ip + cellPos, vec2(94.673, 39.759))) * 43758.5453);
        vec2 point = vec2(rand1, rand2);
        float dist = length(fp - cellPos - point);
        d = min(d, dist);
    }
    return d;
}

// Fractal Worley for detailed organic texture
float fWorley(vec2 p, float time) {
    float f = worley(p * 32.0 + time * 0.25);
    f *= worley(p * 64.0 - time * 0.125);
    f *= worley(p * 128.0);
    return pow(f, 0.125);
}

// --- Advanced Color Theory ---
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Blackbody radiation for physically accurate glows
vec3 blackbody(float temperature) {
    vec3 color = vec3(255.0);
    color.x = 56100000.0 * pow(temperature, -3.0 / 2.0) + 148.0;
    color.y = 100.04 * log(temperature) - 623.6;
    if(temperature > 6500.0) {
        color.y = 35200000.0 * pow(temperature, -3.0 / 2.0) + 184.0;
    }
    color.z = 194.18 * log(temperature) - 1448.6;
    color = clamp(color, 0.0, 255.0) / 255.0;
    if(temperature < 1000.0) {
        color *= temperature / 1000.0;
    }
    return color;
}

// ACES tone mapping for filmic look
vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// --- Advanced Mathematical Functions ---
float dot2(in vec2 v) { return dot(v, v); }

// Advanced Bezier distance calculation with audio modulation
float sdBezierAdvanced(vec2 pos, vec2 A, vec2 B, vec2 C, float audio_mod) {
    vec2 a = B - A;
    vec2 b = A - 2.0 * B + C;
    vec2 c = a * 2.0;
    vec2 d = A - pos;
    float kk = 1.0 / dot(b, b);
    float kx = kk * dot(a, b);
    float ky = kk * (2.0 * dot(a, a) + dot(d, b)) / 3.0;
    float kz = kk * dot(d, a);
    float res = 0.0;
    float p = ky - kx * kx;
    float p3 = p * p * p;
    float q = kx * (2.0 * kx * kx - 3.0 * ky) + kz;
    float h = q * q + 4.0 * p3;
    
    if (h >= 0.0) {
        h = sqrt(h);
        vec2 x = (vec2(h, -h) - q) / 2.0;
        vec2 uv = sign(x) * pow(abs(x), vec2(1.0 / 3.0));
        float t = clamp(uv.x + uv.y - kx, 0.0, 1.0);
        res = dot2(d + (c + b * t) * t);
    } else {
        float z = sqrt(-p);
        float v = acos(q / (p * z * 2.0)) / 3.0;
        float m = cos(v);
        float n = sin(v) * 1.732050808;
        vec3 t = clamp(vec3(m + m, -n - m, n - m) * z - kx, 0.0, 1.0);
        res = min(dot2(d + (c + b * t.x) * t.x),
                  min(dot2(d + (c + b * t.y) * t.y),
                      dot2(d + (c + b * t.z) * t.z)));
    }
    
    // Add audio-reactive distortion to the curve
    res += audio_mod * sin(pos.x * 10.0 + pos.y * 10.0) * 0.01;
    
    return sqrt(res);
}

// Kaleidoscopic folding for complex symmetry
vec2 foldRotate(in vec2 p, in float symmetry) {
    float angle = PI / symmetry - atan(p.x, p.y);
    float n = PI * 2.0 / symmetry;
    angle = floor(angle / n) * n;
    return p * mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
}

// Modular polar repetition
vec2 modPolar(vec2 p, float repetitions) {
    float angle = TAU / repetitions;
    float a = atan(p.y, p.x) + angle * 0.5;
    a = mod(a, angle) - angle * 0.5;
    return vec2(cos(a), sin(a)) * length(p);
}

// Complex iterative folding
vec2 iterativeFold(vec2 p, float time, float audio_factor) {
    for(int i = 0; i < 4; i++) {
        p = foldRotate(p, u_kaleidoscope_segments + audio_factor * 2.0);
        p = abs(p) - 0.25;
        p *= mat2(cos(PI * 0.25), sin(PI * 0.25), 
                  -sin(PI * 0.25), cos(PI * 0.25));
        
        // Add organic warping
        float warp = snoise(vec3(p * 3.0, time + float(i) * 0.5)) * 0.1 * audio_factor;
        p += vec2(warp, warp * 0.7);
    }
    return p;
}

// Organic oscillator for natural motion
float organicOscillator(float time, vec2 p, float bass, float mids, float treble) {
    float osc1 = sin(time * 1.3 + p.x * 0.5) * 0.5 * (1.0 + bass);
    float osc2 = cos(time * 0.7 + p.y * 0.3) * 0.3 * (1.0 + mids);
    float osc3 = sin(time * 1.618 + length(p) * 0.2) * 0.2 * (1.0 + treble);
    return osc1 + osc2 + osc3;
}

// --- Main Shader Logic ---
void main() {
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    
    // --- Advanced Audio Processing ---
    float bass = smoothstep(0.0, 1.0, pow(iAudioBandsAtt.x * u_bass_react, 0.7));
    float mids = iAudioBandsAtt.y * u_mid_react;
    float treble = iAudioBandsAtt.z * u_treble_react;
    float audio_total = (bass + mids + treble) * 0.33;
    
    // --- Camera and Initial Setup ---
    float time = iTime * u_speed;
    uv *= u_zoom;
    
    // --- Advanced Fractal Transformation ---
    // Apply iterative folding with audio modulation
    uv = iterativeFold(uv, time, audio_total);
    
    // Apply polar repetition for circular patterns
    uv = modPolar(uv, 6.0 + bass * 2.0);
    
    // --- Multi-Layer Fractal Loop ---
    float complexity_factor = u_complexity + treble * 2.0;
    
    for (int i = 0; i < int(complexity_factor); i++) {
        // Fold space with varying symmetry
        float symmetry = u_kaleidoscope_segments + sin(time + float(i) * 0.5) * 2.0;
        uv = foldRotate(uv, symmetry);
        
        // Apply organic transformation
        uv = abs(uv) - 0.5;
        
        // Rotate based on time and audio
        float angle = time * 0.2 + mids * 0.5 + float(i) * 0.1;
        mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
        uv *= rot;
        
        // Apply distortion field
        float distortion = dot(abs(sin(uv * 2.0 + time)), vec2(0.666));
        uv += vec2(distortion * 0.1 * audio_total);
        
        // Translate for tunnel effect
        uv -= vec2(0.3 + bass * 0.2, 0.0);
        
        // Scale down for next iteration
        uv *= 0.95;
    }
    
    // --- Advanced Bezier Curve System ---
    // Multiple animated control points
    vec2 p0 = vec2(-0.6, 0.0);
    vec2 p1 = vec2(0.0, 0.6 + bass * 0.3);
    vec2 p2 = vec2(0.6, 0.0);
    
    // Animate control points with organic motion
    p1.x += sin(time * 0.7 + mids * 2.0) * 0.3;
    p1.y += cos(time * 0.5 + treble * 3.0) * 0.2;
    
    // Add secondary bezier curves for complexity
    vec2 p0_2 = vec2(-0.4, -0.2);
    vec2 p1_2 = vec2(0.0, -0.4 + mids * 0.2);
    vec2 p2_2 = vec2(0.4, -0.2);
    
    // Calculate distances to multiple bezier curves
    float d1 = sdBezierAdvanced(uv, p0, p1, p2, audio_total);
    float d2 = sdBezierAdvanced(uv, p0_2, p1_2, p2_2, audio_total * 0.7);
    
    // Combine distances with smooth minimum
    float d = min(d1, d2);
    
    // --- Advanced Background System ---
    vec3 col = u_color1;
    
    // Layer 1: Base gradient
    float gradient = smoothstep(-1.0, 1.0, uv.y);
    col = mix(col, u_color1 * 1.5, gradient * 0.3);
    
    // Layer 2: Fractal Worley noise
    float worley_pattern = fWorley(uv * 2.0 + time * 0.1, time);
    col += vec3(0.05, 0.02, 0.1) * worley_pattern * (1.0 + audio_total * 0.5);
    
    // Layer 3: Simplex noise for organic variation
    float simplex_noise = snoise(vec3(uv * 4.0, time * 0.15)) * 0.5 + 0.5;
    col += vec3(0.03, 0.05, 0.08) * simplex_noise * (1.0 + treble * 0.3);
    
    // --- Advanced Coloring System ---
    // Temperature-based color mapping
    float temperature = mix(2000.0, 10000.0, audio_total);
    vec3 audio_glow = blackbody(temperature) * u_glow_intensity;
    
    // HSV-based iridescent coloring
    float hue_shift = time * 0.3 + bass * 1.5 + uv.x * uv.y * 2.0;
    vec3 iridescent_color = hsv2rgb(vec3(
        fract(hue_shift * 0.1 + d * 5.0),
        0.7 + bass * 0.3,
        0.9 + treble * 0.1
    ));
    
    // Dynamic color mixing based on audio and position
    float color_mix_factor = smoothstep(0.0, 1.0, sin(time * 0.8 + mids * 2.0) * 0.5 + 0.5);
    vec3 primary_color = mix(u_color2, u_color3, color_mix_factor);
    primary_color = mix(primary_color, iridescent_color, u_iridescence);
    
    vec3 secondary_color = mix(u_color3, u_color4, bass);
    
    // Apply colors to curves
    float line1 = smoothstep(u_line_width, u_line_width - 0.01, d1);
    float line2 = smoothstep(u_line_width, u_line_width - 0.01, d2);
    
    col = mix(col, primary_color, line1);
    col = mix(col, secondary_color, line2);
    
    // --- Advanced Glow System ---
    // Multi-layer glow with different falloffs
    float glow1 = exp(-d * 3.0) * u_glow_intensity;
    float glow2 = exp(-d * 8.0) * u_glow_intensity * 0.6;
    float glow3 = exp(-d * 15.0) * u_glow_intensity * 0.3;
    
    // Combine glows with audio-reactive colors
    vec3 glow_color = audio_glow + mix(iridescent_color, u_color4, bass);
    col += glow_color * (glow1 + glow2 + glow3);
    
    // Add shimmer effect from treble
    float shimmer = sin((d + time * 3.0) * 200.0 * (1.0 + treble)) * 0.1;
    col += shimmer * u_color4 * treble;
    
    // --- Advanced Post-Processing ---
    // Chromatic aberration
    float ca_strength = length(uv) * 0.003 * audio_total;
    col.r = mix(col.r, col.r * (1.0 + ca_strength), 0.5);
    col.b = mix(col.b, col.b * (1.0 - ca_strength), 0.5);
    
    // Film grain
    float grain = fract(sin(dot(gl_FragCoord.xy + time, vec2(12.9898, 78.233))) * 43758.5453);
    col += vec3(grain * 0.008 - 0.004);
    
    // Vignette with audio modulation
    float vignette = 1.0 - pow(length(uv) * 0.7, 2.5);
    vignette = mix(vignette, 1.0, bass * 0.4);
    col *= vignette;
    
    // ACES tone mapping
    col = acesFilm(col);
    
    // Gamma correction
    col = pow(col, vec3(0.85));
    
    FragColor = vec4(col, 1.0);
}