// organic_fractal_tree.frag

/*
    Organic Fractal Tree with Audio Reactivity
    This shader generates a 2D fractal tree with organic, curved branches,
    and a shape that can be manipulated to resemble a heart.
    The branching angle and sway are modulated by audio input and time.
*/

// Metadata for UI Controls
//#control vec3 u_color "Tree Color" {"default":[1.0, 0.7, 0.3]}
//#control int u_iterations "Iterations" {"default":15, "min":2, "max":25}
//#control float u_angle "Base Angle" {"default":0.8, "min":-2.0, "max":2.0}
//#control float u_scale "Branch Scale" {"default":0.8, "min":0.5, "max":0.98}
//#control float u_thickness "Thickness" {"default":0.5, "min":0.1, "max":2.0}
//#control float u_audio_reactivity "Audio Reactivity" {"default":0.3, "min":0.0, "max":2.0}
//#control float u_sway "Wind Sway" {"default":0.1, "min":0.0, "max":0.5}
//#control vec3 u_colorB "Branch Tip Color" {"default":[0.8, 0.2, 0.4]}
//#control float u_gradient_mix "Gradient Mix" {"default":0.6, "min":0.0, "max":1.0}
//#control float u_heart_shape "Heart Shape" {"default":0.5, "min":-1.0, "max":1.0}
//#control float u_noise_strength "Color Noise" {"default":0.1, "min":0.0, "max":0.5}
//#control float u_edge_noise "Edge Noise" {"default":0.5, "min":0.0, "max":2.0}

// Global Uniforms
uniform float iAudioAmp;
uniform float iTime;
uniform vec2 iResolution;

// Custom Uniforms
uniform vec3 u_color;
uniform int u_iterations;
uniform float u_angle;
uniform float u_scale;
uniform float u_thickness;
uniform float u_audio_reactivity;
uniform float u_sway;
uniform vec3 u_colorB;
uniform float u_gradient_mix;
uniform float u_heart_shape;
uniform float u_noise_strength;
uniform float u_edge_noise;

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

    // 2. Colors
    vec3 backgroundColor = vec3(0.0, 0.0, 0.0);
    vec3 finalColor = backgroundColor;

    // 3. Dynamic angle for animation and audio reactivity
    float timeSway = sin(iTime * 0.4) * u_sway;
    float audioSway = iAudioAmp * u_audio_reactivity;
    float dynamicAngle = u_angle + timeSway + audioSway;

    // 4. Fractal generation
    vec2 p = uv;
    float scale = u_scale;
    float thickness_mod = 1.0;

    for (int i = 0; i < u_iterations; i++) {
        // Fold the space for symmetry
        p.x = abs(p.x);

        // Heart shape transformation
        p.y += u_heart_shape * p.x * p.x;

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
    float tree = abs(p.y) / (u_thickness * thickness_mod) - edge_noise * 0.1;

    // 6. Color and Glow
    // Mix color based on vertical position
    vec3 mixed_color = mix(u_color, u_colorB, uv.y * u_gradient_mix);

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
    gl_FragColor = vec4(finalColor, 1.0);
}
