// fractal_tree_audio.frag

/*
    Fractal Tree with Audio Reactivity
    This shader generates a 2D fractal tree shape.
    The branching angle of the tree is modulated by the global `iAudioAmp` uniform.
*/

// Metadata for UI Controls
//#control vec3 u_color "Tree Color" {"default":[1.0, 0.9, 0.7]}
//#control int u_iterations "Iterations" {"default":12, "min":2, "max":25}
//#control float u_angle "Base Angle" {"default":1.1, "min":0.1, "max":2.5}
//#control float u_scale "Branch Scale" {"default":0.75, "min":0.5, "max":0.98}
//#control float u_thickness "Thickness" {"default":0.7, "min":0.1, "max":2.0}
//#control float u_audio_reactivity "Audio Reactivity" {"default":0.4, "min":0.0, "max":2.0}
//#control float u_sway "Wind Sway" {"default":0.05, "min":0.0, "max":0.2}
//#control vec3 u_colorB "Color B" {"default":[0.2, 0.3, 0.9]}
//#control float u_gradient_mix "Gradient Mix" {"default":0.5, "min":0.0, "max":1.0}

// Global Uniforms provided by the application
uniform float iAudioAmp; // Expected to be a normalized amplitude [0, 1]

// Custom Uniforms for UI control
uniform vec3 u_color;
uniform int u_iterations;
uniform float u_angle;
uniform float u_scale;
uniform float u_thickness;
uniform float u_audio_reactivity;
uniform float u_sway;
uniform vec3 u_colorB;
uniform float u_gradient_mix;

// Rotation matrix function
mat2 rotate(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

void main()
{
    // 1. Normalize and center coordinates
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    uv.y += 0.9; // Shift the tree down

    // 2. Setup colors
    vec3 backgroundColor = vec3(0.05, 0.0, 0.0);
    vec3 finalColor = backgroundColor;

    // 3. Calculate dynamic angle for animation and audio reactivity
    float timeSway = sin(iTime * 0.5) * u_sway;
    float audioSway = iAudioAmp * u_audio_reactivity;
    float dynamicAngle = u_angle + timeSway + audioSway;

    // 4. Fractal generation loop
    vec2 p = uv;
    float scale = u_scale;

    for (int i = 0; i < u_iterations; i++) {
        // Fold the space to create symmetry
        p.x = abs(p.x);

        // Translate and rotate to create branches
        p.y -= scale;
        p *= rotate(dynamicAngle);

        // Scale up the coordinate system (which scales down the shape)
        scale *= u_scale;
    }

    // 5. Draw the tree
    // Use the y-component of the final coordinate as a distance estimate
    float tree = abs(p.y) / u_thickness;
    
    vec3 mixed_color = mix(u_color, u_colorB, uv.y * u_gradient_mix);

    // Add a glow effect
    float glow = exp(-tree * 2.0);
    finalColor += glow * mixed_color * 0.8;

    // Add the hard shape of the tree
    float shape = smoothstep(0.0, 0.01, tree);
    finalColor = mix(mixed_color, finalColor, shape);

    // Final Output
    gl_FragColor = vec4(finalColor, 1.0);
}
