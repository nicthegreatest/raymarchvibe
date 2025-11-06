#version 330 core

// --- UI Controls ---
uniform float u_complexity = 6.0; // {"label": "Complexity", "min": 1.0, "max": 15.0, "default": 6.0, "smooth": true}
uniform float u_speed = 0.2; // {"label": "Speed", "min": 0.0, "max": 2.0, "default": 0.2}
uniform float u_zoom = 1.5; // {"label": "Zoom", "min": 0.1, "max": 5.0, "default": 1.5}
uniform vec3 u_color1 = vec3(0.1, 0.0, 0.2); // {"label": "Color 1 (Background)", "type": "color", "default": "0.1,0.0,0.2"}
uniform vec3 u_color2 = vec3(0.8, 0.2, 0.5); // {"label": "Color 2 (Curve)", "type": "color", "default": "0.8,0.2,0.5"}
uniform vec3 u_color3 = vec3(1.0, 1.0, 1.0); // {"label": "Color 3 (Glow)", "type": "color", "default": "1.0,1.0,1.0"}
uniform float u_bass_react = 0.5; // {"label": "Bass Reactivity", "min": 0.0, "max": 2.0, "default": 0.5}
uniform float u_mid_react = 0.3; // {"label": "Mid Reactivity", "min": 0.0, "max": 1.0, "default": 0.3}
uniform float u_treble_react = 0.8; // {"label": "Treble Reactivity", "min": 0.0, "max": 2.0, "default": 0.8}
uniform float u_line_width = 0.02; // {"label": "Line Width", "min": 0.001, "max": 0.2, "default": 0.02}

// --- End UI Controls ---
out vec4 FragColor; // Declare output variable for GLSL 3.30 Core Profile

// Global Uniforms
uniform float iTime;
uniform vec2 iResolution;
uniform vec4 iMouse;
uniform vec4 iAudioBands;

float dot2(in vec2 v) { return dot(v, v); }

// Function to calculate distance to a quadratic bezier curve
// Based on Inigo Quilez's article: https://iquilezles.org/articles/distfunctions2d/
float sdBezier(vec2 pos, vec2 A, vec2 B, vec2 C) {
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
    return sqrt(res);
}

void main() {
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;

    // --- Camera and initial setup ---
    float time = iTime; // Use the smoothed time directly
    uv *= u_zoom;

    // --- Audio influence ---
    float bass = iAudioBands.x * u_bass_react;
    float mid = iAudioBands.y * u_mid_react;
    float treble = iAudioBands.z * u_treble_react;
    
    float glow = 0.0;

    // --- Fractal Loop ---
    for (int i = 0; i < int(u_complexity); i++) {
        // Fold the space
        uv = abs(uv) - 0.5;
        
        // Rotate based on time and mid-range audio
        float angle = time * 0.2 * u_speed + mid * 0.5; // Apply speed here
        mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
        uv *= rot;

        // Translate to create "tunnel" effect
        uv -= vec2(0.5, 0.0);
    }

    // --- Bezier Curve Definition ---
    // Animate control points with time and bass
    vec2 p0 = vec2(-0.5, 0.0);
    vec2 p1 = vec2(0.0, 0.5 + bass * 0.5); // Bass makes the curve "jump"
    p1.x += sin(time * 0.5 * u_speed) * 0.2; // Apply speed here
    vec2 p2 = vec2(0.5, 0.0);

    // --- Distance Calculation & Shaping ---
    float d = sdBezier(uv, p0, p1, p2);
    
    // --- Coloring ---
    vec3 bg_col = u_color1;
    vec3 curve_col = u_color2;
    vec3 glow_col = u_color3;

    // Base shape
    float line = smoothstep(u_line_width, u_line_width - 0.01, d);
    
    // Glow effect
    glow = smoothstep(u_line_width + 0.2, u_line_width, d) * 0.5;
    
    // High-frequency shimmer from treble
    float shimmer = sin((d + time * 2.0 * u_speed) * 200.0 * treble) * 0.1 * line; // Apply speed here
    
    // Combine colors
    vec3 final_color = bg_col;
    final_color = mix(final_color, curve_col, line);
    final_color = mix(final_color, glow_col, glow);
    final_color += shimmer * treble;

    // Add a subtle vignette
    float vignette = 1.0 - dot(uv, uv) * 0.2;
    final_color *= vignette;

    FragColor = vec4(final_color, 1.0);
}
