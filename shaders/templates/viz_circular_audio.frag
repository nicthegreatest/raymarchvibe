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

// --- Glow ---
uniform float u_glow_power = 0.4;      // {"label":"Glow Power", "default":0.4, "min":0.0, "max":1.0}

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

// Function to create a rounded box SDF
float sdf_rounded_box(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    // --- Coordinate Setup ---
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    vec4 background = texture(iChannel0, gl_FragCoord.xy / iResolution.xy);

    // --- Polar Coordinates & Rotation ---
    // Get a base angle in the [0, 2*PI] range and add PI to avoid negative values.
    float base_angle = atan(uv.y, uv.x) + PI;
    float dist = length(uv);

    // Create a spinning angle for all calculations to use.
    float spinning_angle = base_angle + iTime * u_spin_speed;

    // --- Bar Calculation ---
    float num_bands = float(u_bands);
    float band_angle_width = 2.0 * PI / num_bands;

    // Find which band we are in using the spinning angle.
    float band_index = floor(spinning_angle / band_angle_width);

    // --- Audio Value ---
    int audio_band_index = int(mod(band_index, 4.0));
    float audio_val = 0.0;
    if (audio_band_index == 0) audio_val = iAudioBands.x * u_bass_boost;
    else if (audio_band_index == 1) audio_val = iAudioBands.y * u_low_mid_boost;
    else if (audio_band_index == 2) audio_val = iAudioBands.z * u_high_mid_boost;
    else audio_val = iAudioBands.w * u_treble_boost;

    // --- Height Calculation with Organic Motion ---
    // We can't easily do a stateful smooth decay, so we add organic oscillation.
    float audio_height = u_bar_height * pow(audio_val * u_sensitivity, 2.0);
    float organic_motion = sin(iTime * u_oscillation_speed + band_index * 0.5) * u_oscillation_amount;
    float h = audio_height + organic_motion;


    // --- SDF for a single bar ---
    // Center the angle for the current band, using the spinning angle.
    float centered_angle = mod(spinning_angle, band_angle_width) - band_angle_width * 0.5;
    vec2 bar_uv = vec2(
        centered_angle * u_radius,
        dist - u_radius
    );
    vec2 bar_dims = vec2(
        band_angle_width * u_radius * 0.3,
        h * 0.5
    );
    bar_uv.y -= h * 0.5;
    float d = sdf_rounded_box(bar_uv, bar_dims, 0.02);

    // --- Coloring ---
    // Use fract() on the normalized spinning angle to create a smooth, wrapping gradient.
    float angle_mix = fract(spinning_angle / (2.0 * PI));
    vec3 angle_color = mix(u_color_start, u_color_end, angle_mix);
    vec3 band_color;
    if (audio_band_index == 0) band_color = u_color_bass;
    else if (audio_band_index == 1) band_color = u_color_low_mid;
    else if (audio_band_index == 2) band_color = u_color_high_mid;
    else band_color = u_color_treble;
    vec3 viz_color = mix(angle_color, band_color, u_band_color_mix);

    // --- Final Composition (Enhanced Glow) ---
    // We layer multiple glows using additive blending for a bloom effect.
    vec3 final_viz = vec3(0.0);
    
    // By raising the Glow Power to an exponent, we get finer control over smaller values.
    float effective_glow = pow(u_glow_power, 3.0);

    // 1. Outer, wide, dim glow (using the base viz color)
    float outer_glow_alpha = 1.0 - smoothstep(0.0, effective_glow * 2.5, d);
    final_viz += viz_color * outer_glow_alpha * 0.4;

    // 2. Inner, tighter, brighter glow
    float inner_glow_alpha = 1.0 - smoothstep(0.0, effective_glow * 0.8, d);
    final_viz += viz_color * inner_glow_alpha * 0.6;

    // 3. A hot, white core for the bar itself to make it pop
    float core_alpha = 1.0 - smoothstep(-0.005, 0.005, d);
    final_viz += vec3(1.0) * core_alpha; // Additive white core

    // If u_composite_over is true, blend the visualization on top of the background.
    if (u_composite_over) {
        FragColor = background + vec4(final_viz, 1.0);
    } else {
        FragColor = vec4(final_viz, 1.0);
    }
}
