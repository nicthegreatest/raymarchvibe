#version 330 core

out vec4 FragColor;

// RaymarchVibe Uniforms
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBands;
uniform vec4 iAudioBandsAtt;
uniform sampler2D iChannel0;

// --- Parameters for UI Controls ---

// Primary color for the palette system
uniform vec3 u_primaryColor = vec3(1.0, 0.2, 0.0); // {"widget":"color", "palette":true, "label":"Primary Color"}
// Accent color, will sync with the primary color's palette
uniform vec3 u_accentColor = vec3(0.0, 0.5, 1.0); // {"widget":"color", "palette":true, "label":"Accent Color"}
// A third color for more variation
uniform vec3 u_tertiaryColor = vec3(1.0, 1.0, 0.0); // {"widget":"color", "palette":true, "label":"Tertiary Color"}

// Controls for the Rubik's Cube effect
uniform float u_rotationSpeed = 0.2;      // {"widget":"slider", "min":0.0, "max":2.0, "step":0.01}
uniform float u_audioStrength = 0.5;      // {"widget":"slider", "min":0.0, "max":2.0, "step":0.01}
uniform float u_recursiveScale = 3.5;     // {"widget":"slider", "min":2.0, "max":5.0, "step":0.1}
uniform float u_cubeGap = 0.1;            // {"widget":"slider", "min":0.0, "max":0.5, "step":0.01}
uniform int u_iterations = 4;             // {"widget":"slider", "min":1, "max":8"}


// --- SDF & Math Functions ---

const float PI = 3.14159265359;

// Rotation matrix
mat2 rot(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

// Box SDF
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// Smooth minimum
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

// --- Scene Definition ---

vec2 map(vec3 p) {
    // --- Recursive Rubik's Cube ---
    float scale = u_recursiveScale;
    vec3 rubikOffset = vec3(1.0); // Size of the 3x3x3 grid
    vec3 cubeSize = vec3(0.45 - u_cubeGap * 0.5);

    vec4 orbit = vec4(p, 1.0); // w component for scale
    float finalDist = 1e6;
    float materialID = -1.0;

    for (int i = 0; i < u_iterations; i++) {
        // Center the grid and repeat it
        vec3 q = mod(orbit.xyz, rubikOffset * 2.0) - rubikOffset;

        // Add audio reactivity - displace cubes based on their position and audio
        float audio = iAudioBandsAtt.x * u_audioStrength;
        vec3 hash33 = vec3(fract(sin(floor(q*10.))*1234.5), fract(cos(floor(q*10.))*678.9), fract(sin(floor(q*10.))*345.6));
        q += hash33 * audio * 0.2;
        
        // Single cube SDF
        float d = sdBox(q, cubeSize);
        
        // Apply distance scaled by orbit
        float scaledDist = d / orbit.w;
        if (scaledDist < finalDist) {
            finalDist = scaledDist;
            // Assign material ID based on cube position for coloring
            vec3 cellID = floor(q + 0.5) + 1.0; // range [0, 2]
            materialID = mod(cellID.x + cellID.y*3.0 + cellID.z*9.0, 6.0);
        }

        // --- Recursion Step ---
        orbit.xyz = abs(orbit.xyz) - rubikOffset * scale;
        orbit.xyz *= rot(iTime * 0.1);
        orbit.w *= scale;
        orbit.xyz /= scale*scale;
    }

    return vec2(finalDist, materialID);
}


// --- Rendering ---

vec3 getNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        map(p + e.xyy).x - map(p - e.xyy).x,
        map(p + e.yxy).x - map(p - e.yxy).x,
        map(p + e.yyx).x - map(p - e.yyx).x
    ));
}

vec3 getColor(float materialID) {
    if (materialID < 1.0) return u_primaryColor;
    if (materialID < 2.0) return u_accentColor;
    if (materialID < 3.0) return u_tertiaryColor;
    if (materialID < 4.0) return mix(u_primaryColor, vec3(1), 0.5);
    if (materialID < 5.0) return mix(u_accentColor, vec3(1), 0.5);
    return mix(u_tertiaryColor, vec3(1), 0.5);
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // --- Camera ---
    vec3 ro = vec3(0.0, 0.0, -5.0 + iTime * u_rotationSpeed);
    vec3 rd = normalize(vec3(uv, 1.0));

    // Rotate camera for more dynamic view
    ro.yz *= rot(-PI / 6.0 + sin(iTime * 0.1) * 0.2);
    rd.yz *= rot(-PI / 6.0 + sin(iTime * 0.1) * 0.2);
    ro.xz *= rot(iTime * u_rotationSpeed * 0.5);
    rd.xz *= rot(iTime * u_rotationSpeed * 0.5);

    // --- Raymarching ---
    float t = 0.0;
    float materialID = -1.0;
    for (int i = 0; i < 100; i++) {
        vec3 p = ro + rd * t;
        vec2 res = map(p);
        if (res.x < 0.001 * t || t > 100.0) break;
        t += res.x * 0.8;
        materialID = res.y;
    }

    // --- Shading ---
    vec3 col = vec3(0.0);
    if (t < 100.0) {
        vec3 p = ro + rd * t;
        vec3 normal = getNormal(p);
        vec3 lightPos = ro + vec3(2.0, 2.0, -2.0);
        vec3 lightDir = normalize(lightPos - p);

        // Get color based on the cube face
        vec3 objectColor = getColor(materialID);

        // Lighting
        float diffuse = max(0.0, dot(normal, lightDir));
        float ambient = 0.2;
        float specular = pow(max(0.0, dot(reflect(-lightDir, normal), -rd)), 16.0);

        col = objectColor * (ambient + diffuse * 0.8) + vec3(1.0) * specular * 0.5;

        // Fog
        col = mix(col, vec3(0.1), smoothstep(0.0, 50.0, t));
    }
    
    // --- Post-processing and Feedback ---
    vec3 prevFrame = texture(iChannel0, gl_FragCoord.xy / iResolution.xy).rgb;
    col = mix(col, prevFrame, 0.85 - iAudioBands.z * 0.1);

    FragColor = vec4(sqrt(clamp(col, 0.0, 1.0)), 1.0); // Gamma correction
}
