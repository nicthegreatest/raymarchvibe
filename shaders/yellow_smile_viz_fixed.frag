#version 330 core

out vec4 FragColor;

// --- Mandatory Uniforms ---
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;     // x:bass, y:mids, z:treble, w:all

// --- UI Controls ---
uniform float u_scale = 1.5; // {"widget":"slider", "min":0.1, "max":5.0, "label":"Scale"}
uniform float u_distortion = 0.1; // {"widget":"slider", "min":0.0, "max":2.0, "label":"Distortion"}
uniform float u_audio_reactivity = 1.0; // {"widget":"slider", "min":0.0, "max":5.0, "label":"Audio Reactivity"}
uniform float u_bass_boost = 1.0; // {"widget":"slider", "default":1.0, "min":0.0, "max":5.0, "label":"Bass Boost"}
uniform vec3 u_color1 = vec3(0.0, 1.0, 0.3); // {"widget":"color", "default":[0.0, 1.0, 0.3], "label":"Smile Color"}
uniform vec3 u_color2 = vec3(0.0, 0.5, 1.0); // {"widget":"color", "default":[0.0, 0.5, 1.0], "label":"Glow Color"}
uniform vec3 u_bgColor = vec3(0.0, 0.0, 0.2); // {"widget":"color", "default":[0.0, 0.0, 0.2], "label":"Background Color"}

// --- SDF Functions ---
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

// SDF operators
float opSubtraction( float d1, float d2 ) { return max(-d1,d2); }

// Simplex noise functions
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
    float n_ = 0.142857142857; // 1.0/7.0
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
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;
    vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

void main() {
    // Calculate UV coordinates (normalized 0-1)
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    uv /= u_scale;

    vec2 p = uv;

    // Audio smoothing for smoother, more liquid reactivity (using bass with boost)
    float smoothed_audio = smoothstep(0.0, 1.0, pow(iAudioBands.x * u_bass_boost, 0.5)) * (0.8 + 0.2 * sin(iTime * 0.5));

    // Psychedelic warping
    float warp_amount = u_distortion + smoothed_audio * 0.5 * u_audio_reactivity;
    float warp_speed = iTime * 0.5;
    p += warp_amount * vec2(
        snoise(vec3(p * 1.5, warp_speed)),
        snoise(vec3(p * 1.5, warp_speed + 10.0))
    );
    p = mix(uv, p, 0.8); // Blend with original UV

    // --- SDF DEFINITIONS ---
    // Eyes
    float eye1 = sdCircle(p - vec2(-0.2, 0.15), 0.08);
    float eye2 = sdCircle(p - vec2(0.2, 0.15), 0.08);

    // Prominent Smile ( thicker, more rounded like Amazon smile)
    vec2 smile_p = p - vec2(0.0, -0.15);
    float smile_outer = sdCircle(smile_p, 0.32);
    float smile_inner_offset = 0.11 + smoothed_audio * 0.2 * u_audio_reactivity;
    float smile_inner = sdCircle(smile_p - vec2(0.0, smile_inner_offset), 0.28);
    float smile = opSubtraction(smile_inner, smile_outer);

    // --- COMBINE SHAPES ---
    // Combine eyes and smile SDFs
    float d1 = min(eye1, eye2);
    float d = min(d1, smile);

    // --- COLORING ---
    vec3 col = vec3(0.0);

    // Background with subtle noise
    float bg_noise = snoise(vec3(uv * 2.0, iTime * 0.1)) * 0.5 + 0.5;
    bg_noise = pow(bg_noise, 2.0);
    vec3 bg_color = u_bgColor + vec3(0.0, 0.03, 0.1) * bg_noise * (1.0 + iAudioBands.y * 0.5 * u_audio_reactivity);
    col = bg_color;

    // Smile color
    vec3 smile_color = mix(u_color1, u_color2, smoothstep(0.0, 1.0, sin(iTime * 1.5) * 0.5 + 0.5));
    col = mix(col, smile_color, 1.0 - smoothstep(0.0, 0.01, d));

    // Glow around shapes
    float glow = 1.0 - smoothstep(0.0, 0.15, d);
    col += glow * u_color2 * (0.5 + iAudioBands.w * 0.3 * u_audio_reactivity);

    // Final gamma correction
    col = pow(col, vec3(0.8));

    FragColor = vec4(col, 1.0);
}
