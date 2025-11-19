#version 330 core

out vec4 FragColor;

// --- Mandatory Uniforms ---
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBandsAtt;   // Attack-smoothed audio bands (x:bass, y:mids, z:treble, w:all)
uniform sampler2D iChannel0;

// --- AI Finesse Parameters ---
uniform vec3 u_primaryColor = vec3(0.1, 0.1, 0.2);     // {"widget":"color", "palette":true, "label":"Background Color"}
uniform vec3 u_accentColor = vec3(0.9, 0.5, 0.2);      // {"widget":"color", "palette":true, "label":"Glow Color"}
uniform vec3 u_objectColor = vec3(0.8, 0.8, 1.0);      // {"widget":"color", "palette":true, "label":"Object Color"}
uniform float u_timeSpeed = 0.2;                        // {"widget":"slider", "min": -1.0, "max": 2.0, "label": "Time Speed"}
uniform float u_detail = 1.8;                           // {"widget":"slider", "min": 1.1, "max": 3.0, "label": "Fractal Detail"}
uniform float u_bloom = 0.15;                           // {"widget":"slider", "min": 0.0, "max": 1.0, "label": "Bloom"}
uniform float u_audioReact = 1.0;                       // {"widget":"slider", "min": 0.0, "max": 2.0, "label": "Audio Reactivity"}
uniform int u_iterations = 7;                           // {"widget":"slider", "min": 3, "max": 12, "label":"Fractal Iterations"}


// --- Mathematical Constants & Helpers ---
const float PI = 3.14159265359;
const float PHI = 1.61803398875; // Golden Ratio

mat3 rotateY(float angle) {
    float s = sin(angle); float c = cos(angle);
    return mat3(c, 0, s, 0, 1, 0, -s, 0, c);
}
mat3 rotateX(float angle) {
    float s = sin(angle); float c = cos(angle);
    return mat3(1, 0, 0, 0, c, -s, 0, s, c);
}

// HSV to RGB conversion for vibrant colors
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}


// --- SDF Scene Definition ---

// Smooth minimum function - essential for smooth fractal shapes
float smin( float a, float b, float k ) {
    float h = clamp( 0.5+0.5*(b-a)/k, 0.0, 1.0 );
    return mix( b, a, h ) - k*h*(1.0-h);
}

// SDF for a Dodecahedron - our core building block
float sdDodecahedron(vec3 p, float r) {
    p = abs(p);
    float d = dot(p, normalize(vec3(1.0, PHI, 0.0)));
    d = max(d, dot(p, normalize(vec3(0.0, 1.0, PHI))));
    d = max(d, dot(p, normalize(vec3(PHI, 0.0, 1.0))));
    return d - r;
}

// This is where the magic happens.
// We define the entire scene as a distance function.
vec2 mapScene(vec3 p) {
    float time = iTime * u_timeSpeed;

    // Create a hypnotic, slow rotation for the base object
    mat3 rotMat = rotateX(time * 0.2) * rotateY(time * 0.3); // Apply rotations in specific order
    p = rotMat * p;

    // --- Fractal Logic ---
    // We will iteratively fold space to create a fractal.
    // This is a common technique in modern raymarching.
    float bass = iAudioBandsAtt.x * u_audioReact;
    float scale = u_detail + bass * 0.3; // Audio makes the fractal 'breathe'
    
    float orbit_trap = 1e9; // Used to color the fractal later
    
    // The fractal loop
    for (int i=0; i<u_iterations; i++) {
        p = abs(p); // Fold space
        p = p - vec3(1.0, 0.9, 1.0); // Translate
        
        // Additional rotation inside the fractal for complexity
        p = rotateY(time * 0.4 + float(i)*0.2) * p;
        p = rotateX(time * 0.3 - float(i)*0.1) * p;
        
        p = p / scale; // Scale down for next iteration
        
        // We trap the minimum distance to the origin to create patterns on the surface
        orbit_trap = min(orbit_trap, length(p));
    }
    
    float dist = sdDodecahedron(p, 0.3) * scale;
    // We scale the final distance back up to world space
    for(int i=0; i<u_iterations; i++) { dist *= scale; }

    // Return distance and the orbit trap value for coloring
    return vec2(dist, orbit_trap);
}


// --- Rendering & Lighting ---

// Calculate the surface normal using the distance function
vec3 getNormal(vec3 p) {
    float e = 0.0005;
    return normalize(vec3(
        mapScene(p + vec3(e,0,0)).x - mapScene(p - vec3(e,0,0)).x,
        mapScene(p + vec3(0,e,0)).x - mapScene(p - vec3(0,e,0)).x,
        mapScene(p + vec3(0,0,e)).x - mapScene(p - vec3(0,0,e)).x
    ));
}

// Raymarch the scene to find the distance to the surface
float rayMarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    for (int i = 0; i < 128; i++) {
        vec3 p = ro + rd * t;
        float d = mapScene(p).x;
        if (d < (0.001 * t) || d < 0.0001) return t;
        t += d * 0.7; // March with a factor for safety and speed
        if (t > 40.0) break;
    }
    return -1.0;
}

// Main shader logic
void main() {
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;

    // --- Camera Setup ---
    float time = iTime * u_timeSpeed;
    vec3 ro = vec3(0, 0, -4.0 + sin(time*0.2)*0.5); // Smoothly move camera in and out
    vec3 rd = normalize(vec3(uv, 1.5)); // 1.5 is FOV

    // --- Raymarch and get surface info ---
    float t = rayMarch(ro, rd);

    vec3 col = u_primaryColor; // Start with the background color

    if (t > -0.5) { // If we hit something
        vec3 p = ro + rd * t;
        vec3 normal = getNormal(p);
        float orbit = mapScene(p).y;

        // --- Lighting ---
        // We'll use a strong key light and a subtle rim light for dramatic effect.
        
        // Key light
        vec3 lightPos1 = vec3(3.0, 2.0, -5.0);
        vec3 lightDir1 = normalize(lightPos1 - p);
        float diffuse = max(0.0, dot(normal, lightDir1));
        
        // Rim light (from behind)
        vec3 lightPos2 = vec3(-2.0, -1.0, 2.0);
        vec3 lightDir2 = normalize(lightPos2 - p);
        float rim = pow(max(0.0, dot(normal, -lightDir2)), 3.0);

        // --- Coloring ---
        // Base color for the object
        vec3 objectBaseColor = u_objectColor;
        
        // The "magic" part: Use the orbit trap and audio to create a pulsing, glowing surface pattern
        float treble = iAudioBandsAtt.z * u_audioReact;
        float mids = iAudioBandsAtt.y * u_audioReact;
        
        // Use HSV for rich, controllable color shifts
        vec3 patternColor = hsv2rgb(vec3(0.5 + orbit * 2.0 + treble * 2.0, 0.8, 1.0));
        patternColor = mix(u_accentColor, patternColor, 0.7);

        // Blend the pattern into the object color based on the orbit trap value
        objectBaseColor = mix(objectBaseColor, patternColor, smoothstep(0.05, 0.2, orbit));

        // --- Final Shading ---
        vec3 finalColor = objectBaseColor * (diffuse + 0.1); // Diffuse + Ambient
        finalColor += u_accentColor * rim * 2.0; // Add rim light
        
        // Add subtle bloom/glow based on the pattern
        finalColor += patternColor * smoothstep(0.1, 0.0, orbit) * 0.8;

        // Fade to background color with distance (fog)
        col = mix(finalColor, u_primaryColor, smoothstep(5.0, 20.0, t));

    }

    // --- Post-Processing ---
    // Add a bloom effect to make the glowing parts pop
    col += u_bloom * u_accentColor * pow(max(0.0, 1.0 - length(uv) * 0.8), 2.0);
    
    // Add feedback from previous frame for a dreamy effect
    vec4 prevFrame = texture(iChannel0, gl_FragCoord.xy/iResolution.xy);
    col = mix(col, prevFrame.rgb, 0.8 - iAudioBandsAtt.w * 0.2); // More feedback on silence

    // Gamma correction for final output
    FragColor = vec4(pow(col, vec3(1.0/2.2)), 1.0);
}
