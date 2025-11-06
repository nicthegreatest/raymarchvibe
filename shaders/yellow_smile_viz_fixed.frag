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

// --- UI Controls ---
uniform float u_complexity = 8.0; // {"widget":"slider", "min":1.0, "max":15.0, "label":"Fractal Complexity"}
uniform float u_scale = 1.5; // {"widget":"slider", "min":0.1, "max":5.0, "label":"Scale"}
uniform float u_distortion = 0.15; // {"widget":"slider", "min":0.0, "max":2.0, "label":"Distortion"}
uniform float u_audio_reactivity = 1.5; // {"widget":"slider", "min":0.0, "max":5.0, "label":"Audio Reactivity"}
uniform float u_bass_boost = 1.8; // {"widget":"slider", "default":1.8, "min":0.0, "max":5.0, "label":"Bass Boost"}
uniform vec3 u_color1 = vec3(0.8, 0.2, 0.9); // {"widget":"color", "default":[0.8, 0.2, 0.9], "label":"Primary Color"}
uniform vec3 u_color2 = vec3(0.2, 0.9, 1.0); // {"widget":"color", "default":[0.2, 0.9, 1.0], "label":"Secondary Color"}
uniform vec3 u_color3 = vec3(1.0, 0.7, 0.1); // {"widget":"color", "default":[1.0, 0.7, 0.1], "label":"Accent Color"}
uniform vec3 u_bgColor = vec3(0.02, 0.01, 0.08); // {"widget":"color", "default":[0.02, 0.01, 0.08], "label":"Background Color"}
uniform float u_iridescence = 0.8; // {"widget":"slider", "min":0.0, "max":2.0, "label":"Iridescence"}
uniform float u_glow_intensity = 1.2; // {"widget":"slider", "min":0.0, "max":3.0, "label":"Glow Intensity"}
uniform float u_kaleidoscope_segments = 6.0; // {"widget":"slider", "min":3.0, "max":12.0, "label":"Kaleidoscope"}

// --- Mathematical Constants ---
const float PI = 3.14159265359;
const float TAU = 6.28318530718;

// --- Advanced Noise & Hashing Functions ---
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
        vec2 point = fract(sin(dot(ip + cellPos, vec2(12.9898, 78.233))) * 43758.5453);
        float dist = length(fp - cellPos - point);
        d = min(d, dist);
    }
    return d;
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

// --- Advanced SDF Operations ---
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

float opSubtraction(float d1, float d2) { return max(-d1,d2); }

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

// Kaleidoscopic folding - creates radial symmetry
vec2 foldRotate(in vec2 p, in float symmetry) {
    float angle = PI / symmetry - atan(p.x, p.y);
    float n = PI * 2.0 / symmetry;
    angle = floor(angle / n) * n;
    return p * mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
}

// --- Advanced Functions ---
float organicOscillator(float time, vec2 p) {
    float osc1 = sin(time * 1.3 + p.x * 0.5) * 0.5;
    float osc2 = cos(time * 0.7 + p.y * 0.3) * 0.3;
    float osc3 = sin(time * 1.618 + length(p) * 0.2) * 0.2;
    return osc1 + osc2 + osc3;
}

void main() {
    // Calculate UV coordinates (normalized 0-1)
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    uv /= u_scale;

    vec2 p = uv;

    // --- Advanced Audio Processing ---
    float bass = smoothstep(0.0, 1.0, pow(iAudioBandsAtt.x * u_bass_boost, 0.6)) * (0.8 + 0.2 * sin(iTime * 0.5));
    float mids = iAudioBandsAtt.y * 0.8;
    float treble = iAudioBandsAtt.z * 1.2;
    float audio_total = (bass + mids + treble) * 0.33;

    // --- Kaleidoscopic Transformation ---
    p = foldRotate(p, u_kaleidoscope_segments);

    // --- Advanced Fractal Loop with Multiple Techniques ---
    for (int i = 0; i < int(u_complexity); i++) {
        // Fold the space
        p = abs(p) - 0.5;
        
        // Apply rotation based on time and multiple audio bands
        float angle = iTime * 0.3 * (1.0 + bass * 0.2) + mids * 0.8;
        mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
        p *= rot;

        // Apply advanced distortion field
        float distort = dot(abs(sin(p * 2.0 + iTime * 0.2)), vec3(0.666));
        p += vec2(distort * u_distortion * audio_total);

        // Worley noise for organic texture
        float worley_noise = worley(p * 8.0 + iTime * 0.1);
        p += (worley_noise - 0.5) * u_distortion * 0.3;

        // Create tunnel effect with audio modulation
        p -= vec2(0.5 + bass * 0.2, 0.0);
    }

    // --- Advanced SDF Scene ---
    // Eyes with audio-reactive sizing
    float eye_size = 0.08 + bass * 0.05;
    float eye1 = sdCircle(p - vec2(-0.2, 0.15), eye_size);
    float eye2 = sdCircle(p - vec2(0.2, 0.15), eye_size);

    // Advanced smile with organic deformation
    vec2 smile_p = p - vec2(0.0, -0.15);
    float smile_outer = sdCircle(smile_p, 0.32);
    float smile_inner_offset = 0.11 + bass * 0.3 * u_audio_reactivity;
    smile_inner_offset += sin(iTime * 2.0 + mids * 3.0) * 0.05;
    float smile_inner = sdCircle(smile_p - vec2(0.0, smile_inner_offset), 0.28);
    float smile = opSubtraction(smile_inner, smile_outer);

    // Combine shapes with smooth minimum
    float d1 = smin(eye1, eye2, 0.02);
    float d = smin(d1, smile, 0.03);

    // --- Advanced Background with Multiple Layers ---
    vec3 col = u_bgColor;
    
    // Layer 1: Base gradient with audio modulation
    float gradient_factor = smoothstep(-1.0, 1.0, uv.y);
    col = mix(col, u_bgColor * 2.0, gradient_factor);
    
    // Layer 2: Worley noise pattern
    float bg_worley = worley(uv * 4.0 + iTime * 0.1);
    col += vec3(0.02, 0.05, 0.1) * bg_worley * (1.0 + audio_total * 0.5);
    
    // Layer 3: Simplex noise for organic variation
    float bg_noise = snoise(vec3(uv * 3.0, iTime * 0.15)) * 0.5 + 0.5;
    col += vec3(0.01, 0.03, 0.08) * bg_noise * (1.0 + treble * 0.3);

    // --- Advanced Coloring with Temperature Mapping ---
    // Dynamic temperature mapping based on audio
    float temperature = mix(2000.0, 8000.0, audio_total);
    vec3 audio_glow = blackbody(temperature) * u_glow_intensity;

    // HSV-based iridescent coloring
    float hue_shift = iTime * 0.5 + bass * 2.0;
    vec3 iridescent_color = hsv2rgb(vec3(
        fract(hue_shift * 0.1 + d * 3.0),
        0.8 + bass * 0.2,
        0.9 + treble * 0.1
    ));

    // Mix primary colors with iridescence
    vec3 smile_color = mix(u_color1, u_color2, smoothstep(0.0, 1.0, sin(iTime * 1.5) * 0.5 + 0.5));
    smile_color = mix(smile_color, iridescent_color, u_iridescence);

    // Apply color to shapes
    col = mix(col, smile_color, 1.0 - smoothstep(0.0, 0.01, d));

    // --- Advanced Glow System ---
    // Multiple glow layers for bloom effect
    float glow1 = exp(-d * 2.0) * u_glow_intensity;
    float glow2 = exp(-d * 5.0) * u_glow_intensity * 0.5;
    float glow3 = exp(-d * 10.0) * u_glow_intensity * 0.3;
    
    vec3 glow_color = audio_glow + mix(u_color2, u_color3, bass);
    col += glow_color * (glow1 + glow2 + glow3);

    // --- Advanced Post-Processing ---
    // Add chromatic aberration for cinematic effect
    float ca_strength = length(uv) * 0.002 * audio_total;
    vec2 ca_offset = vec2(ca_strength, 0.0);
    col.r = mix(col.r, col.r * (1.0 + ca_strength), 0.5);
    col.b = mix(col.b, col.b * (1.0 - ca_strength), 0.5);

    // Film grain
    float grain = fract(sin(dot(gl_FragCoord.xy + iTime, vec2(12.9898, 78.233))) * 43758.5453);
    col += vec3(grain * 0.01 - 0.005);

    // Vignette with audio modulation
    float vignette = 1.0 - pow(length(uv) * 0.8, 2.0);
    vignette = mix(vignette, 1.0, bass * 0.3); // Reduce vignette on bass hits
    col *= vignette;

    // ACES tone mapping
    col = acesFilm(col);

    // Gamma correction
    col = pow(col, vec3(0.8));

    FragColor = vec4(col, 1.0);
}