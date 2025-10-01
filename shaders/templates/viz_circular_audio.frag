#version 330 core
out vec4 FragColor;

// Standard uniforms
uniform vec2 iResolution;
uniform float iTime;

// Input texture
uniform sampler2D iChannel0;

// Audio uniforms
uniform float iAudioAmp; // Overall amplitude
uniform vec4 iAudioBands; // .x: bass, .y: low-mid, .z: high-mid, .w: treble

// --- UI Controls ---
uniform float u_innerRadius = 0.25; // {"label":"Inner Radius", "default":0.25, "min":0.0, "max":1.0}
uniform float u_maxBarHeight = 0.5; // {"label":"Max Bar Height", "default":0.5, "min":0.0, "max":1.0}
uniform float u_sensitivity = 1.5; // {"label":"Audio Sensitivity", "default":1.5, "min":0.0, "max":5.0}
uniform int u_segments = 64; // {"label":"Bars", "default":64, "min":8, "max":128}
uniform float u_gapWidth = 0.25; // {"label":"Gap Width", "default":0.25, "min":0.0, "max":0.9}
uniform vec3 u_colorA = vec3(0.0, 1.0, 0.8); // {"widget":"color", "label":"Gradient Start"}
uniform vec3 u_colorB = vec3(1.0, 0.0, 1.0); // {"widget":"color", "label":"Gradient End"}
uniform float u_glow = 0.05; // {"label":"Glow", "default":0.05, "min":0.0, "max":0.5}
uniform bool u_enableComposite = false; // {"label":"Enable Compositing", "default": false}
// --- End UI Controls ---

const float PI = 3.14159265359;

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    
    vec4 background = vec4(0.0, 0.0, 0.0, 1.0); // Default to a black background
    if (u_enableComposite) {
        background = texture(iChannel0, gl_FragCoord.xy / iResolution.xy);
    }

    // Convert to polar coordinates
    float dist = length(uv);
    float angle = atan(uv.y, uv.x);

    // --- Segment & Bar Calculation ---
    float segmentAngleWidth = 2.0 * PI / float(u_segments);
    float segmentIndex = floor((angle + PI) / segmentAngleWidth);

    // Map segment index to one of the 4 audio bands
    int bandIndex = int(mod(segmentIndex, 4.0));
    float bandValue = 0.0;
    if (bandIndex == 0) bandValue = iAudioBands.x;
    else if (bandIndex == 1) bandValue = iAudioBands.y;
    else if (bandIndex == 2) bandValue = iAudioBands.z;
    else bandValue = iAudioBands.w;

    // Smooth and amplify the audio value
    bandValue = pow(bandValue, 2.0) * u_sensitivity;

    // Calculate the length of the bar for this segment
    float barHeight = bandValue * u_maxBarHeight;
    float outerRadius = u_innerRadius + barHeight;

    // --- SDF Rendering ---
    // 1. SDF for the radial part (the bar's length)
    float radialSDF = max(u_innerRadius - dist, dist - outerRadius);

    // 2. SDF for the angular part (the gaps between bars)
    float barAngleWidth = segmentAngleWidth * (1.0 - u_gapWidth);
    float centeredAngle = mod(angle + PI, segmentAngleWidth) - segmentAngleWidth * 0.5;
    float angularSDF = abs(centeredAngle) - barAngleWidth * 0.5;

    // 3. Combine SDFs to get the final bar shape
    float barSDF = max(radialSDF, angularSDF);

    // --- Color & Final Output ---
    // Create a radial gradient
    float gradientMix = smoothstep(u_innerRadius, u_innerRadius + u_maxBarHeight, dist);
    vec3 color = mix(u_colorA, u_colorB, gradientMix);

    // Use smoothstep on the final SDF for anti-aliasing and glow
    vec3 finalViz = color * (1.0 - smoothstep(0.0, 0.01, barSDF));
    finalViz += color * (1.0 - smoothstep(0.0, u_glow, barSDF)) * 0.5;

    FragColor = background + vec4(finalViz, 1.0);
}

