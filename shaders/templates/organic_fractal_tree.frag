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
// Tree Structure & Shape
uniform int u_iterations = 20;                  // {"widget":"slider", "min":2, "max":30, "label":"Iterations"}
uniform float u_angle = 0.8;                    // {"widget":"slider", "min":-2.0, "max":2.0, "label":"Base Angle"}
uniform float u_scale = 0.85;                    // {"widget":"slider", "min":0.5, "max":0.98, "label":"Branch Scale"}
uniform float u_thickness = 0.6;                // {"widget":"slider", "min":0.1, "max":2.0, "label":"Thickness"}
uniform float u_heart_shape = 0.5;              // {"widget":"slider", "min":-1.0, "max":1.0, "label":"Heart Shape"}
uniform float u_fractal_complexity = 1.0;        // {"widget":"slider", "min":0.0, "max":2.0, "label":"Fractal Complexity"}

// Audio Reactivity (Shape/Motion)
uniform float u_thickness_reactivity = 0.3;     // {"widget":"slider", "min":0.0, "max":3.0, "label":"Thickness Reactivity"}
uniform float u_audio_reactivity = 0.2;         // {"widget":"slider", "min":0.0, "max":2.0, "label":"Angle Reactivity"}
uniform float u_treble_heart_reactivity = 0.3;    // {"widget":"slider", "min":0.0, "max":2.0, "label":"Treble Heart Reactivity"}
uniform float u_sway = 0.15;                     // {"widget":"slider", "min":0.0, "max":0.5, "label":"Wind Sway"}

// Colors
uniform vec3 u_color = vec3(0.9, 0.6, 0.3);      // {"widget":"color", "label":"Tree Color"}
uniform vec3 u_colorB = vec3(0.8, 0.3, 0.5);    // {"widget":"color", "label":"Branch Tip Color"}
uniform float u_gradient_mix = 0.7;             // {"widget":"slider", "min":0.0, "max":1.0, "label":"Gradient Mix"}
uniform vec3 u_color_bass = vec3(1.0, 0.2, 0.2); // {"widget":"color", "label":"Color Bass"}
uniform vec3 u_color_mid = vec3(1.0, 0.8, 0.2);  // {"widget":"color", "label":"Color Mid"}
uniform vec3 u_color_treble = vec3(0.2, 0.8, 1.0); // {"widget":"color", "label":"Color Treble"}
uniform vec3 u_background_color = vec3(0.0, 0.0, 0.02); // {"widget":"color", "label":"Background Color"}

// Effects & Noise
uniform float u_color_reactivity = 0.3;         // {"widget":"slider", "min":0.0, "max":2.0, "label":"Color Reactivity"}
uniform float u_noise_strength = 0.08;          // {"widget":"slider", "min":0.0, "max":0.5, "label":"Color Noise"}
uniform float u_edge_noise = 0.3;               // {"widget":"slider", "min":0.0, "max":2.0, "label":"Edge Noise"}
uniform float u_warp_reactivity = 0.2;          // {"widget":"slider", "min":0.0, "max":2.0, "label":"Warp Reactivity"}
uniform float u_iridescence = 0.5;             // {"widget":"slider", "min":0.0, "max":2.0, "label":"Iridescence"}
uniform float u_glow_intensity = 1.2;          // {"widget":"slider", "min":0.0, "max":3.0, "label":"Glow Intensity"}

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
    float f = worley(p * 16.0 + time * 0.2);
    f *= worley(p * 32.0 - time * 0.1);
    f *= worley(p * 64.0);
    return pow(f, 0.25);
}

// --- Advanced Color Theory ---
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

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

vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// --- Advanced Mathematical Functions ---
mat2 rotate(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

// 2D random noise function
float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

// Kaleidoscopic folding for complex symmetry
vec2 foldRotate(in vec2 p, in float symmetry) {
    float angle = PI / symmetry - atan(p.x, p.y);
    float n = PI * 2.0 / symmetry;
    angle = floor(angle / n) * n;
    return p * mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
}

// Organic oscillator for natural motion
float organicOscillator(float time, vec2 p, float bass, float mids, float treble) {
    float osc1 = sin(time * 1.3 + p.x * 0.5) * 0.5 * (1.0 + bass);
    float osc2 = cos(time * 0.7 + p.y * 0.3) * 0.3 * (1.0 + mids);
    float osc3 = sin(time * 1.618 + length(p) * 0.2) * 0.2 * (1.0 + treble);
    return osc1 + osc2 + osc3;
}

// Organic deformation function
vec2 organicDeformation(vec2 p, float time, float audio_factor) {
    // Multiple frequency organic warping
    vec2 warp1 = vec2(
        sin(p.y * 3.0 + time * 0.7) * 0.1,
        cos(p.x * 2.5 + time * 0.5) * 0.08
    );
    
    vec2 warp2 = vec2(
        sin(length(p) * 4.0 + time * 1.2) * 0.05,
        cos(atan(p.y, p.x) * 6.0 - time * 0.8) * 0.06
    );
    
    // Noise-based deformation
    float noise1 = snoise(vec3(p * 2.0, time * 0.3));
    float noise2 = snoise(vec3(p * 4.0, time * 0.5));
    vec2 noise_warp = vec2(noise1 - noise2, noise1 + noise2) * 0.1;
    
    return p + (warp1 + warp2 + noise_warp) * audio_factor;
}

// --- Main Shader Logic ---
void main() {
    // 1. Normalize and center coordinates, shift origin
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    uv.y += 1.2;

    // --- Advanced Audio Processing ---
    float bass = smoothstep(0.0, 1.0, pow(iAudioBandsAtt.x, 0.7));
    float mids = iAudioBandsAtt.y;
    float treble = iAudioBandsAtt.z;
    float audio_total = (bass + mids + treble) * 0.33;

    // --- Advanced UV Warping (Audio Reactive) ---
    float audio_warp_amount = audio_total * u_warp_reactivity;
    
    // Multi-layer warping system
    vec2 warp_offset1 = vec2(
        sin(uv.y * 3.0 + iTime * 0.2) * 0.1,
        cos(uv.x * 2.5 + iTime * 0.3) * 0.08
    );
    
    vec2 warp_offset2 = vec2(
        worley(uv * 4.0 + iTime * 0.1) - 0.5,
        worley(uv * 4.0 + iTime * 0.1 + 15.7) - 0.5
    );
    
    vec2 total_warp = (warp_offset1 + warp_offset2 * 0.5) * audio_warp_amount;
    uv += total_warp;

    // 2. Advanced Background with Multiple Layers ---
    vec3 backgroundColor = u_background_color;
    
    // Layer 1: Base gradient with audio modulation
    float gradient_factor = smoothstep(-1.0, 1.0, uv.y);
    backgroundColor = mix(backgroundColor, backgroundColor * 2.0, gradient_factor * 0.3);
    backgroundColor *= (1.0 + bass * 0.3);
    
    // Layer 2: Worley noise pattern
    float bg_worley = fWorley(uv * 2.0 + iTime * 0.1, iTime);
    backgroundColor += vec3(0.02, 0.03, 0.08) * bg_worley * (1.0 + audio_total * 0.5);
    
    // Layer 3: Simplex noise for organic variation
    float bg_noise = snoise(vec3(uv * 3.0, iTime * 0.15)) * 0.5 + 0.5;
    backgroundColor += vec3(0.01, 0.02, 0.05) * bg_noise * (1.0 + treble * 0.3);
    
    vec3 finalColor = backgroundColor;

    // 3. Dynamic parameters for animation and audio reactivity
    // Angle sways with time and the mid audio band
    float timeSway = sin(iTime * 0.4) * u_sway;
    float midSway = mids * u_audio_reactivity;
    float dynamicAngle = u_angle + timeSway + midSway;

    // Thickness pulses with the bass
    float bassPulse = bass * u_thickness_reactivity;
    float dynamicThickness = u_thickness + bassPulse;

    // Heart shape sparkles with the treble
    float dynamicHeart = u_heart_shape + treble * u_treble_heart_reactivity;

    // 4. Advanced Fractal Generation with Multiple Techniques
    vec2 p = uv;
    float scale = u_scale;
    float thickness_mod = 1.0;
    float color_mix_factor = 0.0;

    // Apply kaleidoscopic transformation for complexity
    p = foldRotate(p, 6.0 + u_fractal_complexity * 3.0);

    for (int i = 0; i < u_iterations; i++) {
        // Fold the space for symmetry
        p.x = abs(p.x);

        // Advanced heart shape transformation with audio modulation
        float heart_factor = dynamicHeart + sin(iTime * 0.3 + float(i) * 0.2) * 0.1;
        p.y += heart_factor * p.x * p.x * (1.0 + bass * 0.2);

        // Organic branching with complex rotation
        float curve = sin(p.y * 0.5 + iTime * 0.2) * 0.1;
        float audio_rotation = dynamicAngle + mids * 0.5 + treble * 0.3;
        p *= rotate(audio_rotation + curve);

        // Apply organic deformation
        p = organicDeformation(p, iTime * 0.1, audio_total * 0.2);

        // Translate with audio-reactive scaling
        float audio_scale = 1.0 + bass * 0.1;
        p.y -= scale * audio_scale;

        // Vary thickness - thinner at the tips with audio modulation
        thickness_mod = mix(thickness_mod, 0.5, 0.2) * (1.0 + treble * 0.1);

        // Scale up coordinate system with fractal complexity
        scale *= u_scale * (1.0 + u_fractal_complexity * 0.05);

        // Track color mixing for gradient
        color_mix_factor += 1.0 / float(u_iterations);
    }

    // 5. Advanced Tree Shape Rendering with Noise
    float edge_noise = random(p * 100.0) * u_edge_noise;
    float organic_noise = snoise(vec3(p * 50.0, iTime * 0.5)) * u_edge_noise * 0.5;
    
    // Use dynamic thickness for bass reactivity
    float tree = abs(p.y) / (dynamicThickness * thickness_mod) - (edge_noise + organic_noise) * 0.1;

    // 6. Advanced Color and Glow System
    // Temperature-based color mapping based on audio
    float temperature = mix(1500.0, 8000.0, audio_total);
    vec3 audio_glow = blackbody(temperature) * u_glow_intensity;

    // HSV-based iridescent coloring
    float hue_shift = iTime * 0.2 + bass * 1.5 + length(p) * 0.5;
    vec3 iridescent_color = hsv2rgb(vec3(
        fract(hue_shift * 0.1 + tree * 3.0),
        0.8 + bass * 0.2,
        0.9 + treble * 0.1
    ));
    
    // Dynamic color mixing based on audio bands and position
    float audio_mix_factor = audio_total * u_color_reactivity;
    vec3 mixed_color = mix(u_color, u_colorB, color_mix_factor * u_gradient_mix + audio_mix_factor);
    
    // Add iridescence
    mixed_color = mix(mixed_color, mixed_color + iridescent_color, u_iridescence);

    // Audio-reactive color selection
    vec3 bass_color = mix(mixed_color, u_color_bass, bass);
    vec3 mid_color = mix(mixed_color, u_color_mid, mids);
    vec3 treble_color = mix(mixed_color, u_color_treble, treble);
    
    vec3 final_tree_color = mix(mixed_color, bass_color, bass * 0.5);
    final_tree_color = mix(final_tree_color, mid_color, mids * 0.3);
    final_tree_color = mix(final_tree_color, treble_color, treble * 0.2);

    // Add subtle noise to color for a more organic feel
    float color_noise = random(uv * 2.0) * u_noise_strength;
    final_tree_color += color_noise * (1.0 + audio_total * 0.5);

    // 7. Advanced Glow System with Multiple Layers
    // Multi-layer glow for bloom effect
    float glow1 = exp(-tree * 1.8) * u_glow_intensity;
    float glow2 = exp(-tree * 4.0) * u_glow_intensity * 0.5;
    float glow3 = exp(-tree * 8.0) * u_glow_intensity * 0.25;
    
    // Combine glows with audio-reactive colors
    vec3 glow_color = audio_glow + mix(final_tree_color, iridescent_color, u_iridescence);
    finalColor += glow_color * (glow1 + glow2 + glow3);

    // 8. Main Tree Shape Rendering
    float shape = smoothstep(0.0, 0.01, tree);
    finalColor = mix(final_tree_color, finalColor, shape);

    // 9. Advanced Post-Processing
    // Chromatic aberration for cinematic effect
    float ca_strength = length(uv) * 0.002 * audio_total;
    finalColor.r = mix(finalColor.r, finalColor.r * (1.0 + ca_strength), 0.5);
    finalColor.b = mix(finalColor.b, finalColor.b * (1.0 - ca_strength), 0.5);

    // Film grain
    float grain = fract(sin(dot(gl_FragCoord.xy + iTime, vec2(12.9898, 78.233))) * 43758.5453);
    finalColor += vec3(grain * 0.01 - 0.005);

    // Vignette with audio modulation
    float vignette = 1.0 - pow(length(uv) * 0.7, 2.0);
    vignette = mix(vignette, 1.0, bass * 0.3);
    finalColor *= vignette;

    // ACES tone mapping
    finalColor = acesFilm(finalColor);

    // Gamma correction
    finalColor = pow(finalColor, vec3(0.85));

    FragColor = vec4(finalColor, 1.0);
}
