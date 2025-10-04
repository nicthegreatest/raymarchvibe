#version 330 core
// organic_fractal_tree.frag

/*
    Organic Fractal Tree with Audio Reactivity
    This shader generates a 2D fractal tree with organic, curved branches,
    and a shape that can be manipulated to resemble a heart.
    The branching angle and sway are modulated by audio input and time.
*/

// --- UI Controls ---
// Tree Structure & Shape
uniform int u_iterations = 25;                  // {"label":"Iterations", "default":25, "min":2, "max":25}
uniform float u_angle = 0.8;                    // {"label":"Base Angle", "default":0.8, "min":-2.0, "max":2.0}
uniform float u_scale = 0.9;                    // {"label":"Branch Scale", "default":0.9, "min":0.5, "max":0.98}
uniform float u_thickness = 0.5;                // {"label":"Thickness", "default":0.5, "min":0.1, "max":2.0}
uniform float u_heart_shape = 0.5;              // {"label":"Heart Shape", "default":0.5, "min":-1.0, "max":1.0}

// Audio Reactivity (Shape/Motion)
uniform float u_thickness_reactivity = 0.2;     // {"label":"Thickness Reactivity", "default":0.2, "min":0.0, "max":3.0}
uniform float u_audio_reactivity = 0.1;         // {"label":"Angle Reactivity", "default":0.1, "min":0.0, "max":2.0}
uniform float u_treble_heart_reactivity = 0.2;    // {"label":"Treble Heart Reactivity", "default":0.2, "min":0.0, "max":2.0}
uniform float u_sway = 0.1;                     // {"label":"Wind Sway", "default":0.1, "min":0.0, "max":0.5}

// Colors
uniform vec3 u_color = vec3(1.0, 0.7, 0.3);      // {"label":"Tree Color", "widget":"color"}
uniform vec3 u_colorB = vec3(0.8, 0.2, 0.4);    // {"label":"Branch Tip Color", "widget":"color"}
uniform float u_gradient_mix = 0.6;             // {"label":"Gradient Mix", "default":0.6, "min":0.0, "max":1.0}
uniform vec3 u_color_bass = vec3(1.0, 0.2, 0.2); // {"label":"Color Bass", "widget":"color"}
uniform vec3 u_color_mid = vec3(1.0, 0.8, 0.2);  // {"label":"Color Mid", "widget":"color"}
uniform vec3 u_color_treble = vec3(0.2, 0.8, 1.0); // {"label":"Color Treble", "widget":"color"}
uniform vec3 u_background_color = vec3(0.0, 0.0, 0.0); // {"label":"Background Color", "widget":"color"}

// Effects & Noise
uniform float u_color_reactivity = 0.2;         // {"label":"Color Reactivity", "default":0.2, "min":0.0, "max":2.0}
uniform float u_noise_strength = 0.05;          // {"label":"Color Noise", "default":0.05, "min":0.0, "max":0.5}
uniform float u_edge_noise = 0.2;               // {"label":"Edge Noise", "default":0.2, "min":0.0, "max":2.0}
uniform float u_warp_reactivity = 0.1;          // {"label":"Warp Reactivity", "default":0.1, "min":0.0, "max":2.0}
// --- End UI Controls ---
out vec4 FragColor; // Declare output variable for GLSL 3.30 Core Profile

// Global Uniforms
uniform float iAudioAmp;
uniform vec4 iAudioBands; // For future use
uniform float iTime;
uniform vec2 iResolution;

// 2D rotation matrix
mat2 rotate(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

// 2D random noise function
float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main() {
    // 1. Normalize and center coordinates, shift origin
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    uv.y += 1.2;

    // --- UV Warping (Audio Reactive) ---
    float audio_warp_amount = iAudioAmp * u_warp_reactivity;
    vec2 warp_offset = vec2(random(uv * 4.0 + iTime * 0.2), random(uv * 4.0 + iTime * 0.2 + 15.7)) - 0.5;
    uv += warp_offset * audio_warp_amount;

    // 2. Colors
    // Background color pulses with overall audio amplitude
    vec3 backgroundColor = u_background_color * (1.0 + iAudioAmp * 0.5);
    vec3 finalColor = backgroundColor;

    // 3. Dynamic parameters for animation and audio reactivity
    // Angle sways with time and the mid audio band
    float timeSway = sin(iTime * 0.4) * u_sway;
    float midSway = iAudioBands.y * u_audio_reactivity; // Use mids for angle
    float dynamicAngle = u_angle + timeSway + midSway;

    // Thickness pulses with the bass
    float bassPulse = iAudioBands.x * u_thickness_reactivity;
    float dynamicThickness = u_thickness + bassPulse;

    // Heart shape sparkles with the treble
    float dynamicHeart = u_heart_shape + iAudioBands.w * u_treble_heart_reactivity;

    // 4. Fractal generation
    vec2 p = uv;
    float scale = u_scale;
    float thickness_mod = 1.0;

    for (int i = 0; i < u_iterations; i++) {
        // Fold the space for symmetry
        p.x = abs(p.x);

        // Heart shape transformation (now audio-reactive)
        p.y += dynamicHeart * p.x * p.x;

        // Organic branching - apply rotation with some curvature
        float curve = sin(p.y * 0.5) * 0.1;
        p *= rotate(dynamicAngle + curve);

        // Translate and scale
        p.y -= scale;

        // Vary thickness - thinner at the tips
        thickness_mod = mix(thickness_mod, 0.5, 0.2);

        // Scale up coordinate system
        scale *= u_scale;
    }

    // 5. Draw the tree shape with noisy edges
    float edge_noise = random(p*100.0) * u_edge_noise;
    // Use dynamic thickness for bass reactivity
    float tree = abs(p.y) / (dynamicThickness * thickness_mod) - edge_noise * 0.1;

    // 6. Color and Glow
    // Mix color based on vertical position and audio bands
    float audio_mix_factor = (iAudioBands.x + iAudioBands.y + iAudioBands.z + iAudioBands.w) * 0.25 * u_color_reactivity;
    vec3 mixed_color = mix(u_color, u_colorB, uv.y * u_gradient_mix + audio_mix_factor);

    // Add subtle noise to color for a more organic feel
    float color_noise = random(uv * 2.0) * u_noise_strength;
    mixed_color += color_noise;

    // Softer glow effect
    float glow = exp(-tree * 1.8);
    finalColor += glow * mixed_color * 0.9;

    // Main tree shape
    float shape = smoothstep(0.0, 0.01, tree);
    finalColor = mix(mixed_color, finalColor, shape);

    // Final Output
    FragColor = vec4(finalColor, 1.0);
}
