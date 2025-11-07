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
uniform sampler2D iChannel0;
uniform bool iChannel0_active;

// --- Advanced UI Controls ---
// --- General Visuals ---
uniform float u_radius = 0.5;          // {"widget":"slider", "min":0.1, "max":2.0, "label":"Radius"}
uniform int u_bands = 128;              // {"widget":"slider", "min":4, "max":256, "label":"Bands"}
uniform float u_bar_height = 0.3;      // {"widget":"slider", "min":0.0, "max":1.0, "label":"Bar Height"}
uniform float u_sensitivity = 3.0;     // {"widget":"slider", "min":0.0, "max":10.0, "label":"Sensitivity"}
uniform float u_spin_speed = 0.2;      // {"widget":"slider", "min":-2.0, "max":2.0, "label":"Spin Speed"}
uniform float u_kaleidoscope_segments = 12.0; // {"widget":"slider", "min":3.0, "max":24.0, "label":"Kaleidoscope"}

// --- Warping & Distortion ---
uniform float u_warp_amount = 0.2;     // {"widget":"slider", "min":0.0, "max":1.0, "label":"Warp Amount"}
uniform float u_distortion_complexity = 1.5; // {"widget":"slider", "min":0.0, "max":3.0, "label":"Distortion Complexity"}

// --- Organic Surface ---
uniform float u_noise_amount = 0.08;   // {"widget":"slider", "min":0.0, "max":0.5, "label":"Noise Amount"}
uniform float u_noise_scale = 15.0;    // {"widget":"slider", "min":1.0, "max":50.0, "label":"Noise Scale"}
uniform float u_organic_deformation = 0.15; // {"widget":"slider", "min":0.0, "max":0.5, "label":"Organic Deformation"}

// --- Advanced Lighting ---
uniform vec3 u_light_pos = vec3(0.5, 0.5, 0.5); // {"widget":"slider", "default_x": 0.5, "default_y": 0.5, "default_z": 0.5, "min":-2.0, "max":2.0, "label":"Light Position"}
uniform vec3 u_light_color = vec3(1.0, 0.9, 0.7); // {"widget":"color", "label":"Light Color"}
uniform float u_light_intensity = 1.5; // {"widget":"slider", "min":0.0, "max":3.0, "label":"Light Intensity"}
uniform float u_ambient_strength = 0.3; // {"widget":"slider", "min":0.0, "max":1.0, "label":"Ambient Strength"}

// --- Glow & Effects ---
uniform float u_glow_power = 0.6;      // {"widget":"slider", "min":0.0, "max":2.0, "label":"Glow Power"}
uniform float u_softness = 1.5;        // {"widget":"slider", "min":0.0, "max":5.0, "label":"Edge Softness"}
uniform float u_iridescence = 0.7;    // {"widget":"slider", "min":0.0, "max":2.0, "label":"Iridescence"}

// --- Organic Motion ---
uniform float u_oscillation_amount = 0.03; // {"widget":"slider", "min":0.0, "max":0.2, "label":"Organic Motion"}
uniform float u_oscillation_speed = 1.2;   // {"widget":"slider", "min":0.0, "max":5.0, "label":"Motion Speed"}
uniform float u_organic_flow = 0.1;    // {"widget":"slider", "min":0.0, "max":0.5, "label":"Organic Flow"}

// --- Advanced Color Palette ---
uniform vec3 u_color_start = vec3(0.1, 0.8, 1.0); // {"widget":"color", "label":"Gradient Start"}
uniform vec3 u_color_end = vec3(0.8, 0.1, 1.0);   // {"widget":"color", "label":"Gradient End"}
uniform float u_band_color_mix = 0.6; // {"widget":"slider", "min":0.0, "max":1.0, "label":"Band Color Mix"}

// --- Advanced Audio Reactivity ---
uniform vec3 u_color_bass = vec3(1.0, 0.2, 0.2); // {"widget":"color", "label":"Color Bass"}
uniform float u_bass_boost = 2.0;      // {"widget":"slider", "min":0.0, "max":5.0, "label":"Bass Boost"}

uniform vec3 u_color_low_mid = vec3(1.0, 0.8, 0.2); // {"widget":"color", "label":"Color Low-Mid"}
uniform float u_low_mid_boost = 2.5;   // {"widget":"slider", "min":0.0, "max":5.0, "label":"Low-Mid Boost"}

uniform vec3 u_color_high_mid = vec3(0.2, 1.0, 0.2); // {"widget":"color", "label":"Color High-Mid"}
uniform float u_high_mid_boost = 5.0;  // {"widget":"slider", "min":0.0, "max":10.0, "label":"High-Mid Boost"}

uniform vec3 u_color_treble = vec3(0.2, 0.8, 1.0); // {"widget":"color", "label":"Color Treble"}
uniform float u_treble_boost = 8.0;    // {"widget":"slider", "min":0.0, "max":10.0, "label":"Treble Boost"}

// --- Composition ---
uniform bool u_composite_over = true; // {"label":"Composite Over Input", "default":true}
uniform float u_fade_amount = 0.1;   // {"widget":"slider", "min":0.0, "max":1.0, "label":"Fade Amount"}

// --- Mathematical Constants ---
const float PI = 3.14159265359;
const float TAU = 6.28318530718;

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

// --- Advanced SDF Functions ---
float sdf_rounded_box(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

// Kaleidoscopic folding for complex symmetry
vec2 foldRotate(in vec2 p, in float symmetry) {
    float angle = PI / symmetry - atan(p.x, p.y);
    float n = PI * 2.0 / symmetry;
    angle = floor(angle / n) * n;
    return p * mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
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
    
    return p + (warp1 + warp2 + noise_warp) * u_organic_deformation * audio_factor;
}

// Combined map function with advanced features
float map(vec2 p, vec2 bar_dims, float time, float audio_factor) {
    // Apply organic deformation
    p = organicDeformation(p, time, audio_factor);
    
    // Increase rounding influence for more organic shapes
    float radius = bar_dims.x * 0.8;
    
    // Base SDF
    float d = sdf_rounded_box(p, bar_dims, radius);
    
    // Multi-layer displacement for organic surface
    float displacement1 = snoise(vec3(p * u_noise_scale, time * 0.2)) * u_noise_amount;
    float displacement2 = snoise(vec3(p * u_noise_scale * 2.3, time * 0.3)) * u_noise_amount * 0.5;
    float displacement3 = worley(p * u_noise_scale * 0.7 + time * 0.1) * u_noise_amount * 0.3;
    
    float total_displacement = displacement1 + displacement2 + displacement3;
    total_displacement *= (1.0 + audio_factor * 0.5);
    
    return d - total_displacement;
}

// Advanced normal calculation
vec2 calc_normal(vec2 p, vec2 bar_dims, float time, float audio_factor) {
    const vec2 eps = vec2(0.001, 0.0);
    return normalize(vec2(
        map(p + eps.xy, bar_dims, time, audio_factor) - map(p - eps.xy, bar_dims, time, audio_factor),
        map(p + eps.yx, bar_dims, time, audio_factor) - map(p - eps.yx, bar_dims, time, audio_factor)
    ));
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
    // --- Coordinate Setup ---
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    vec4 background = texture(iChannel0, gl_FragCoord.xy / iResolution.xy);

    // --- Advanced Audio Processing ---
    float bass = smoothstep(0.0, 1.0, pow(iAudioBandsAtt.x * u_bass_boost, 0.6));
    float mids = iAudioBandsAtt.y * u_low_mid_boost;
    float treble = iAudioBandsAtt.z * u_treble_boost;
    float audio_total = (bass + mids + treble) * 0.33;

    // --- Advanced UV Warping ---
    // Multi-layer warping system
    vec2 warp_offset1 = vec2(
        sin(uv.y * 4.0 + iTime * 0.2) * 0.1,
        cos(uv.x * 3.0 + iTime * 0.3) * 0.08
    );
    
    vec2 warp_offset2 = vec2(
        worley(uv * 6.0 + iTime * 0.1) - 0.5,
        worley(uv * 6.0 + iTime * 0.1 + 15.7) - 0.5
    );
    
    vec2 warp_offset3 = vec2(
        snoise(vec3(uv * 2.0, iTime * 0.15)) - 0.5,
        snoise(vec3(uv * 2.0, iTime * 0.15 + 100.0)) - 0.5
    );
    
    vec2 total_warp = (warp_offset1 + warp_offset2 * 0.5 + warp_offset3 * 0.3) * u_warp_amount;
    total_warp *= (1.0 + audio_total * u_distortion_complexity);
    
    uv += total_warp;

    // --- Kaleidoscopic Transformation ---
    uv = foldRotate(uv, u_kaleidoscope_segments);

    // --- Polar Coordinates & Rotation ---
    float base_angle = atan(uv.y, uv.x) + PI;
    float dist = length(uv);
    float spinning_angle = base_angle + iTime * u_spin_speed + organicOscillator(iTime, uv, bass, mids, treble) * u_organic_flow;

    // --- Advanced Bar Calculation ---
    float num_bands = float(u_bands);
    float band_angle_width = 2.0 * PI / num_bands;
    float band_index = floor(spinning_angle / band_angle_width);

    // --- Enhanced Audio Value with Smoothing ---
    int audio_band_index = int(mod(band_index, 4.0));
    float audio_val = 0.0;
    vec3 band_color;
    
    if (audio_band_index == 0) {
        audio_val = iAudioBandsAtt.x * u_bass_boost;
        band_color = u_color_bass;
    } else if (audio_band_index == 1) {
        audio_val = iAudioBandsAtt.y * u_low_mid_boost;
        band_color = u_color_low_mid;
    } else if (audio_band_index == 2) {
        audio_val = iAudioBandsAtt.z * u_high_mid_boost;
        band_color = u_color_high_mid;
    } else {
        audio_val = iAudioBandsAtt.w * u_treble_boost;
        band_color = u_color_treble;
    }

    // --- Advanced Height Calculation with Organic Motion ---
    float base_height = u_bar_height * pow(audio_val * u_sensitivity, 2.0);
    
    // Multi-frequency organic oscillation
    float organic_motion1 = sin(iTime * u_oscillation_speed + band_index * 0.5) * u_oscillation_amount;
    float organic_motion2 = cos(iTime * u_oscillation_speed * 1.5 + band_index * 0.3) * u_oscillation_amount * 0.5;
    float organic_motion3 = sin(iTime * u_oscillation_speed * 2.1 + band_index * 0.7) * u_oscillation_amount * 0.25;
    
    float h = base_height * (1.0 + organic_motion1 + organic_motion2 + organic_motion3);

    // --- Advanced SDF Evaluation ---
    float centered_angle = mod(spinning_angle, band_angle_width) - band_angle_width * 0.5;
    vec2 bar_uv = vec2(centered_angle * u_radius, dist - u_radius);
    vec2 bar_dims = vec2(band_angle_width * u_radius * 0.4, h * 0.5);
    bar_uv.y -= h * 0.5;
    
    float d = map(bar_uv, bar_dims, iTime, audio_total);

    // --- Advanced Edge Fading ---
    float angle_fade_width = band_angle_width * 0.6;
    float angle_blend = smoothstep(angle_fade_width * 0.8, angle_fade_width, abs(centered_angle));
    d = mix(d, 1.0, angle_blend);

    // --- Advanced Lighting ---
    vec2 normal = calc_normal(bar_uv, bar_dims, iTime, audio_total);
    vec3 light_dir_3d = normalize(u_light_pos - vec3(bar_uv, 0.0));
    
    // Multi-light system
    float diffuse = max(0.0, dot(normal, light_dir_3d.xy)) * 0.7 + u_ambient_strength;
    
    // Add rim lighting
    float rim = 1.0 - dot(normal, light_dir_3d.xy);
    rim = pow(rim, 2.0) * 0.3;
    
    // Advanced lighting with audio modulation
    float lighting = diffuse + rim * (1.0 + audio_total * 0.5);

    // --- Advanced Coloring System ---
    // Temperature-based color mapping
    float temperature = mix(2000.0, 10000.0, audio_total);
    vec3 audio_glow = blackbody(temperature) * u_glow_power;
    
    // HSV-based iridescent coloring
    float hue_shift = iTime * 0.3 + bass * 1.5 + centered_angle * 2.0;
    vec3 iridescent_color = hsv2rgb(vec3(
        fract(hue_shift * 0.1 + d * 3.0),
        0.8 + bass * 0.2,
        0.9 + treble * 0.1
    ));
    
    // Dynamic angle-based coloring
    float angle_mix = fract(spinning_angle / (2.0 * PI));
    vec3 angle_color = mix(u_color_start, u_color_end, angle_mix);
    
    // Combine colors with iridescence
    vec3 viz_color = mix(angle_color, band_color, u_band_color_mix);
    viz_color = mix(viz_color, viz_color + iridescent_color, u_iridescence);
    
    // Apply lighting
    viz_color *= lighting;
    viz_color += audio_glow * 0.3;

    // --- Advanced Final Composition ---
    vec3 final_viz = vec3(0.0);

    // Multi-layer glow system
    float effective_glow = pow(u_glow_power, 2.5) * 0.5 + 0.01;
    
    // 1. Outer, wide, dim glow
    float outer_glow_alpha = 1.0 - smoothstep(0.0, effective_glow * 3.0, d);
    final_viz += viz_color * outer_glow_alpha * 0.4;
    
    // 2. Inner, tighter, brighter glow
    float inner_glow_alpha = 1.0 - smoothstep(0.0, effective_glow * 1.0, d);
    final_viz += viz_color * inner_glow_alpha * 0.7;
    
    // 3. Hot, white core
    float core_alpha = 1.0 - smoothstep(-0.008, 0.008, d);
    vec3 core_color = mix(viz_color, vec3(1.0), 0.7); // Mix with white for hot core
    final_viz += core_color * core_alpha;

    // --- Advanced Post-Processing ---
    // Add shimmer effect from treble
    float shimmer = sin((d + iTime * 4.0) * 300.0 * (1.0 + treble * 2.0)) * 0.05;
    final_viz += shimmer * vec3(0.8, 0.9, 1.0) * treble;
    
    // Composite with background
    if (u_composite_over) {
        vec3 composite_color = mix(background.rgb, final_viz, 1.0 - smoothstep(0.0, u_fade_amount, d));
        FragColor = vec4(composite_color, 1.0);
    } else {
        // Apply ACES tone mapping
        final_viz = acesFilm(final_viz);
        
        // Gamma correction
        final_viz = pow(final_viz, vec3(0.9));
        
        FragColor = vec4(final_viz, 1.0);
    }
}