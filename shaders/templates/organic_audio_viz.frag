#version 330 core
out vec4 FragColor;

// Input from previous effect (e.g., Raymarch Sphere)
uniform sampler2D iChannel0;

// Standard uniforms
uniform vec2 iResolution;
uniform float iTime;

// Audio uniforms
uniform float iAudioAmp;   // Overall amplitude (not used, but good to have)
uniform vec4 iAudioBands; // .x: bass, .y: low-mid, .z: high-mid, .w: treble

// --- UI Controls ---

// --- General Visuals ---
uniform float u_radius = 0.4;          // {"label":"Radius", "default":0.4, "min":0.1, "max":2.0}
uniform int u_bands = 64;              // {"label":"Bands", "default":64, "min":4, "max":256}
uniform float u_bar_height = 0.2;      // {"label":"Bar Height", "default":0.2, "min":0.0, "max":1.0}
uniform float u_sensitivity = 2.0;     // {"label":"Sensitivity", "default":2.0, "min":0.0, "max":10.0}
uniform float u_spin_speed = 0.1;      // {"label":"Spin Speed", "default":0.1, "min":-2.0, "max":2.0}
uniform float u_rounding = 0.5;        // {"label":"Bar Rounding", "default":0.5, "min":0.0, "max":2.0}

// --- Warping ---
uniform float u_warp_amount = 0.1;     // {"label":"Warp Amount", "default":0.1, "min":0.0, "max":1.0}

// --- Organic Surface ---
uniform float u_noise_amount = 0.05;   // {"label":"Noise Amount", "default":0.05, "min":0.0, "max":0.5}
uniform float u_noise_scale = 10.0;    // {"label":"Noise Scale", "default":10.0, "min":1.0, "max":50.0}

// --- Lighting ---
uniform vec3 u_light_pos = vec3(0.5, 0.5, 0.5); // {"label":"Light Position", "default_x": 0.5, "default_y": 0.5, "default_z": 0.5, "min":-2.0, "max":2.0}

// --- Glow ---
uniform float u_glow_power = 0.4;      // {"label":"Glow Power", "default":0.4, "min":0.0, "max":1.0}
uniform float u_softness = 1.0;        // {"label":"Edge Softness", "default":1.0, "min":0.0, "max":5.0}

// --- Organic Motion ---
uniform float u_oscillation_amount = 0.02; // {"label":"Organic Motion", "default":0.02, "min":0.0, "max":0.2}
uniform float u_oscillation_speed = 1.0;   // {"label":"Motion Speed", "default":1.0, "min":0.0, "max":5.0}

// --- Color Palette ---
uniform vec3 u_color_start = vec3(0.1, 0.8, 1.0); // {"widget":"color", "label":"Gradient Start"}
uniform vec3 u_color_end = vec3(0.8, 0.1, 1.0);   // {"widget":"color", "label":"Gradient End"}
uniform float u_band_color_mix = 0.5; // {"label":"Band Color Mix", "default":0.5, "min":0.0, "max":1.0}

// --- Audio Reactivity ---
uniform vec3 u_color_bass = vec3(1.0, 0.2, 0.2); // {"widget":"color", "label":"Color Bass"}
uniform float u_bass_boost = 1.0;      // {"label":"Bass Boost", "default":1.0, "min":0.0, "max":5.0}

uniform vec3 u_color_low_mid = vec3(1.0, 0.8, 0.2); // {"widget":"color", "label":"Color Low-Mid"}
uniform float u_low_mid_boost = 1.5;   // {"label":"Low-Mid Boost", "default":1.5, "min":0.0, "max":5.0}

uniform vec3 u_color_high_mid = vec3(0.2, 1.0, 0.2); // {"widget":"color", "label":"Color High-Mid"}
uniform float u_high_mid_boost = 4.0;  // {"label":"High-Mid Boost", "default":4.0, "min":0.0, "max":5.0}

uniform vec3 u_color_treble = vec3(0.2, 0.8, 1.0); // {"widget":"color", "label":"Color Treble"}
uniform float u_treble_boost = 6.0;    // {"label":"Treble Boost", "default":6.0, "min":0.0, "max":10.0}

// --- Composition ---
uniform bool u_composite_over = true; // {"label":"Composite Over Input", "default":true}

// --- End UI Controls ---

const float PI = 3.14159265359;

// --- Noise Function (Value Noise) ---
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f*f*(3.0-2.0*f);
    float a = hash(i + vec2(0.0, 0.0));
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// --- SDF Functions ---
float sdf_rounded_box(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

// Combined map function for the scene distance
float map(vec2 p, vec2 bar_dims) {
    // Increase rounding influence to make bars more pill-shaped
    float radius = bar_dims.x * u_rounding * 1.5;
    float d = sdf_rounded_box(p, bar_dims, radius);
    // Displace the surface with noise for an organic look
    float displacement = noise(p * u_noise_scale) * u_noise_amount;
    return d - displacement;
}

// Calculate the normal of the SDF surface by checking nearby points
vec2 calc_normal(vec2 p, vec2 bar_dims) {
    const vec2 eps = vec2(0.001, 0.0);
    return normalize(vec2(
        map(p + eps.xy, bar_dims) - map(p - eps.xy, bar_dims),
        map(p + eps.yx, bar_dims) - map(p - eps.yx, bar_dims)
    ));
}

void main() {
    // --- Coordinate Setup ---
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    vec4 background = texture(iChannel0, gl_FragCoord.xy / iResolution.xy);

    // --- UV Warping ---
    vec2 warp_offset = vec2(noise(uv * 4.0 + iTime * 0.2), noise(uv * 4.0 + iTime * 0.2 + 15.7)) - 0.5;
    uv += warp_offset * u_warp_amount;

    // --- Polar Coordinates & Rotation ---
    float base_angle = atan(uv.y, uv.x) + PI;
    float dist = length(uv);
    float spinning_angle = base_angle + iTime * u_spin_speed;

    // --- Bar Calculation ---
    float num_bands = float(u_bands);
    float band_angle_width = 2.0 * PI / num_bands;
    float band_index = floor(spinning_angle / band_angle_width);

    // --- Audio Value ---
    int audio_band_index = int(mod(band_index, 4.0));
    float audio_val = 0.0;
    if (audio_band_index == 0) audio_val = iAudioBands.x * u_bass_boost;
    else if (audio_band_index == 1) audio_val = iAudioBands.y * u_low_mid_boost;
    else if (audio_band_index == 2) audio_val = iAudioBands.z * u_high_mid_boost;
    else audio_val = iAudioBands.w * u_treble_boost;

    // --- Height Calculation ---
    float audio_height = u_bar_height * pow(audio_val * u_sensitivity, 2.0);
    float organic_motion = sin(iTime * u_oscillation_speed + band_index * 0.5) * u_oscillation_amount;
    float h = audio_height + organic_motion;

    // --- SDF Evaluation ---
    float centered_angle = mod(spinning_angle, band_angle_width) - band_angle_width * 0.5;
    vec2 bar_uv = vec2(centered_angle * u_radius, dist - u_radius);
    vec2 bar_dims = vec2(band_angle_width * u_radius * 0.3, h * 0.5);
    bar_uv.y -= h * 0.5;
    float d = map(bar_uv, bar_dims);

    // --- Fade bar edges to fix 'dart board' effect ---
    float angle_fade_width = band_angle_width * 0.5;
    float angle_blend = smoothstep(angle_fade_width * 0.8, angle_fade_width, abs(centered_angle));
    d = mix(d, 1.0, angle_blend); // Mix distance towards 1.0 (far away/invisible) at the edges

    // --- Lighting ---
    vec2 normal = calc_normal(bar_uv, bar_dims);
    vec3 light_dir_3d = normalize(u_light_pos - vec3(bar_uv, 0.0));
    float diffuse = max(0.0, dot(normal, light_dir_3d.xy)) * 0.5 + 0.5; // Add ambient light

    // --- Coloring ---
    float angle_mix = fract(spinning_angle / (2.0 * PI));
    vec3 angle_color = mix(u_color_start, u_color_end, angle_mix);
    vec3 band_color;
    if (audio_band_index == 0) band_color = u_color_bass;
    else if (audio_band_index == 1) band_color = u_color_low_mid;
    else if (audio_band_index == 2) band_color = u_color_high_mid;
    else band_color = u_color_treble;
    vec3 viz_color = mix(angle_color, band_color, u_band_color_mix);

    // Apply lighting to the color
    viz_color *= diffuse;

    // --- Final Composition (Softer Edges & Glow) ---
    vec3 final_viz = vec3(0.0);

    // 1. Core shape with adaptive soft edges
    // u_softness provides a controllable blur width on top of the standard fwidth() anti-aliasing.
    float softness_width = u_softness * 0.05; // Scale down the uniform for finer control
    float edge_width = fwidth(d) + softness_width;
    float core_alpha = smoothstep(edge_width, -edge_width, d);
    
    // 2. Glow
    // The glow is a larger, fainter, smoother version of the shape.
    float glow_falloff = pow(u_glow_power, 2.0) * 0.5 + 0.01; // Map glow power to a usable range
    float glow_alpha = smoothstep(glow_falloff, 0.0, d);

    // Combine them: Add the glow, then draw the core on top.
    // We multiply by viz_color to give them color.
    final_viz = viz_color * glow_alpha * 0.5; // Glow is at 50% opacity
    final_viz = mix(final_viz, viz_color, core_alpha); // Draw core over the glow

    // --- Final Output ---
    if (u_composite_over) {
        FragColor = background + vec4(final_viz, 1.0);
    } else {
        FragColor = vec4(final_viz, 1.0);
    }
}
