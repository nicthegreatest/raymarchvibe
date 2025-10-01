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
uniform float u_radius = 0.4;          // {"label":"Radius", "default":0.4, "min":0.1, "max":2.0}
uniform int u_bands = 64;              // {"label":"Bands", "default":64, "min":8, "max":256}
uniform float u_bar_height = 0.2;      // {"label":"Bar Height", "default":0.2, "min":0.0, "max":1.0}
uniform float u_sensitivity = 2.0;     // {"label":"Sensitivity", "default":2.0, "min":0.0, "max":10.0}
uniform float u_glow = 0.05;           // {"label":"Glow", "default":0.05, "min":0.0, "max":1.0}
uniform float u_spin_speed = 0.1;      // {"label":"Spin Speed", "default":0.1, "min":-2.0, "max":2.0}
uniform vec3 u_color_start = vec3(0.1, 0.8, 1.0); // {"widget":"color", "label":"Gradient Start"}
uniform vec3 u_color_end = vec3(0.8, 0.1, 1.0);   // {"widget":"color", "label":"Gradient End"}
uniform vec3 u_color_bass = vec3(1.0, 0.2, 0.2); // {"widget":"color", "label":"Color Bass"}
uniform vec3 u_color_low_mid = vec3(1.0, 0.8, 0.2); // {"widget":"color", "label":"Color Low-Mid"}
uniform vec3 u_color_high_mid = vec3(0.2, 1.0, 0.2); // {"widget":"color", "label":"Color High-Mid"}
uniform vec3 u_color_treble = vec3(0.2, 0.8, 1.0); // {"widget":"color", "label":"Color Treble"}
uniform float u_band_color_mix = 0.5; // {"label":"Band Color Mix", "default":0.5, "min":0.0, "max":1.0}
uniform float u_bass_boost = 1.0;      // {"label":"Bass Boost", "default":1.0, "min":0.0, "max":5.0}
uniform float u_low_mid_boost = 1.5;   // {"label":"Low-Mid Boost", "default":1.5, "min":0.0, "max":5.0}
uniform float u_high_mid_boost = 2.0;  // {"label":"High-Mid Boost", "default":2.0, "min":0.0, "max":5.0}
uniform float u_treble_boost = 4.5;    // {"label":"Treble Boost", "default":4.5, "min":0.0, "max":5.0}
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
    // Center the coordinates and normalize
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // Get the background color from the input texture
    vec4 background = texture(iChannel0, gl_FragCoord.xy / iResolution.xy);

    // --- Polar Coordinates & Rotation ---
    float angle = atan(uv.y, uv.x) + iTime * u_spin_speed;
    float dist = length(uv);

    // --- Bar Calculation ---
    float num_bands = float(u_bands);
    float band_angle_width = 2.0 * PI / num_bands;

    // Find which band we are in
    float band_index = floor(angle / band_angle_width);

    // Map the band index to one of the 4 audio channels
    int audio_band_index = int(mod(band_index, 4.0));
    float audio_val = 0.0;
    if (audio_band_index == 0) audio_val = iAudioBands.x * u_bass_boost;
    else if (audio_band_index == 1) audio_val = iAudioBands.y * u_low_mid_boost;
    else if (audio_band_index == 2) audio_val = iAudioBands.z * u_high_mid_boost;
    else audio_val = iAudioBands.w * u_treble_boost;

    // Apply sensitivity and create a dynamic height for the bar
    float h = u_bar_height * pow(audio_val * u_sensitivity, 2.0);

    // --- SDF for a single bar ---
    // Center the angle for the current band
    float centered_angle = mod(angle, band_angle_width) - band_angle_width * 0.5;

    // Create a coordinate system for a single vertical bar
    vec2 bar_uv = vec2(
        centered_angle * u_radius, // x is along the circumference
        dist - u_radius            // y is distance from the main circle radius
    );

    // Define the dimensions of the bar
    vec2 bar_dims = vec2(
        band_angle_width * u_radius * 0.3, // width of the bar
        h * 0.5 // half-height of the bar
    );

    // The bar is centered at y = h * 0.5
    bar_uv.y -= h * 0.5;

    // Calculate the SDF of the rounded box
    float d = sdf_rounded_box(bar_uv, bar_dims, 0.02);

    // --- Coloring ---
    // Mix colors based on the angle for a smooth gradient around the circle
    float angle_mix = (angle + PI) / (2.0 * PI);
    vec3 angle_color = mix(u_color_start, u_color_end, angle_mix);

    // Create a color based on the audio band
    vec3 band_color;
    if (audio_band_index == 0) band_color = u_color_bass;
    else if (audio_band_index == 1) band_color = u_color_low_mid;
    else if (audio_band_index == 2) band_color = u_color_high_mid;
    else band_color = u_color_treble;

    // Mix the angle color and the band color
    vec3 viz_color = mix(angle_color, band_color, u_band_color_mix);

    // --- Final Composition ---
    // Use smoothstep to create an anti-aliased shape with a glow
    float glow_alpha = 1.0 - smoothstep(0.0, u_glow, d);
    float core_alpha = 1.0 - smoothstep(0.0, 0.005, d);

    // Combine core and glow, and apply the color
    vec3 final_viz = viz_color * (core_alpha + glow_alpha * 0.5);

    // If u_composite_over is true, blend the visualization on top of the background.
    // Otherwise, just show the visualization on a black background.
    if (u_composite_over) {
        FragColor = background + vec4(final_viz, 1.0);
    } else {
        FragColor = vec4(final_viz, 1.0);
    }
}
