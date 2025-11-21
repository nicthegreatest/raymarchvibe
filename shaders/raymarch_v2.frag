#version 330 core

out vec4 FragColor;

// --- Mandatory Uniforms ---
uniform vec2 iResolution;
uniform float iTime;
uniform vec4 iAudioBandsAtt;   // Attack-smoothed audio bands (x:bass, y:mids, z:treble, w:all)
uniform sampler2D iChannel0;

// --- AI Finesse Parameters ---
uniform vec3 u_primaryColor = vec3(0.0, 0.0, 0.1);      // {"widget":"color", "palette":true, "label":"Deep Space"}
uniform vec3 u_accentColor = vec3(0.0, 1.0, 1.0);       // {"widget":"color", "palette":true, "label":"Plasma Color"}
uniform vec3 u_objectColor = vec3(1.0, 0.0, 0.5);       // {"widget":"color", "palette":true, "label":"Core Color"}
uniform float u_timeSpeed = 0.5;                        // {"widget":"slider", "min": 0.0, "max": 2.0, "label": "Warp Speed"}
uniform float u_detail = 1.0;                           // {"widget":"slider", "min": 0.1, "max": 3.0, "label": "Lattice Density"}
uniform float u_bloom = 0.4;                            // {"widget":"slider", "min": 0.0, "max": 1.0, "label": "Glow Intensity"}
uniform float u_audioReact = 1.0;                       // {"widget":"slider", "min": 0.0, "max": 2.0, "label": "Audio Pulse"}
uniform int u_iterations = 60;                          // {"widget":"slider", "min": 20, "max": 100, "label":"Ray Steps"}

// --- Constants ---
const float PI = 3.14159265359;

// --- Rotation Matrices ---
mat2 rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

// --- Palette Function (Cosine based) ---
// Creates a rainbow-like palette based on t
vec3 palette(float t) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263, 0.416, 0.557);
    return a + b * cos(6.28318 * (c * t + d));
}

// --- SDF & Geometry ---

// Box SDF
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// The "Cosmic Lattice" map function
// Returns distance to the nearest object
float map(vec3 p) {
    // Audio modulation
    float bass = iAudioBandsAtt.x * u_audioReact;
    float mids = iAudioBandsAtt.y * u_audioReact;
    
    // Domain repetition (infinite field)
    // We use a larger spacing to create a "cathedral" feel
    float spacing = 4.0 / u_detail;
    vec3 id = floor((p + spacing * 0.5) / spacing);
    p = mod(p + spacing * 0.5, spacing) - spacing * 0.5;
    
    // Rotate each cell differently based on its ID and time
    float t = iTime * u_timeSpeed;
    p.xz *= rot(t * 0.5 + id.y * 0.2);
    p.xy *= rot(t * 0.3 + id.z * 0.2);
    
    // Create a morphing shape
    // Mix between a box and a sphere based on audio/time
    float box = sdBox(p, vec3(0.5));
    float sphere = length(p) - 0.6;
    
    // Smooth morph
    float morph = sin(t + length(id) * 0.5) * 0.5 + 0.5; // 0 to 1
    morph = mix(morph, bass, 0.3); // Audio influences shape
    
    float d = mix(box, sphere, morph);
    
    // Hollow it out for complexity
    d = max(d, -(length(p) - 0.4));
    
    // Add some small details
    d -= sin(p.x * 10.0 + t) * 0.02 * mids;
    
    return d;
}

void main() {
    // Normalized pixel coordinates (from -1 to 1)
    vec2 uv = (gl_FragCoord.xy * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv; // Save original UVs for post-processing
    
    // --- Camera ---
    // Fly through the lattice
    float time = iTime * u_timeSpeed;
    vec3 ro = vec3(0.0, 0.0, time * 2.0); // Moving forward
    
    // Add some camera shake/sway based on audio
    ro.x += sin(time * 0.5) * 1.0;
    ro.y += cos(time * 0.3) * 1.0;
    
    vec3 rd = normalize(vec3(uv, 1.5));
    
    // Camera rotation (look around)
    rd.xz *= rot(sin(time * 0.2) * 0.3);
    rd.xy *= rot(cos(time * 0.1) * 0.2);
    
    // --- Raymarching with Accumulation (Glow) ---
    vec3 finalColor = vec3(0.0);
    float t = 0.0;
    
    // Accumulate glow
    float acc = 0.0;
    
    int maxIter = u_iterations;
    
    for (int i = 0; i < maxIter; i++) {
        vec3 p = ro + rd * t;
        float d = map(p);
        
        // Volumetric accumulation logic
        // The closer we are to a surface, the more light we accumulate
        // We use an exponential falloff for a "neon" look
        // abs(d) is crucial because we might step inside objects
        float glow = exp(-abs(d) * 3.0); 
        
        // Modulate glow with audio
        float treble = iAudioBandsAtt.z * u_audioReact;
        glow *= (1.0 + treble * 2.0);
        
        acc += glow * 0.05 * u_bloom; // Scale by bloom parameter
        
        // Standard raymarching step, but with a minimum step to catch volume
        t += max(abs(d) * 0.6, 0.02); 
        
        if (t > 40.0) break; // Far plane
    }
    
    // --- Coloring ---
    // Base the color on the accumulated value and a palette
    vec3 col = palette(length(uv0) + iTime * 0.2 + acc * 0.5);
    
    // Mix with user colors
    col = mix(u_primaryColor, col, 0.5); // Background influence
    col += u_accentColor * acc * 0.5;    // Glow influence
    col += u_objectColor * acc * 0.2;    // Core influence
    
    // Tone mapping
    finalColor = col * acc; // The accumulation defines the brightness
    
    // Fog / Distance fade
    finalColor *= 1.0 / (1.0 + t * t * 0.05);
    
    // --- Post-Processing ---
    // Vignette
    finalColor *= 1.0 - length(uv0) * 0.5;
    
    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0 / 2.2));
    
    FragColor = vec4(finalColor, 1.0);
}
