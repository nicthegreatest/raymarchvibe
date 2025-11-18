#version 330 core

out vec4 FragColor;

// RaymarchVibe built-in uniforms
uniform vec2 iResolution;
uniform float iTime;
uniform float iAudioAmp;
uniform vec4 iAudioBands;
uniform sampler2D iChannel0;

// --- UI Controls ---
uniform float u_radius = 0.5; // {"widget":"slider", "min":0.1, "max":2.0, "default":0.5}
uniform float u_twist = 4.0; // {"widget":"slider", "min":0.1, "max":10.0, "default":4.0}
uniform float u_speed = 0.5; // {"widget":"slider", "min":0.0, "max":2.0, "default":0.5}
uniform vec3 u_color1 = vec3(0.1, 0.5, 1.0); // {"widget":"color", "default":[0.1, 0.5, 1.0]}
uniform vec3 u_color2 = vec3(1.0, 0.2, 0.8); // {"widget":"color", "default":[1.0, 0.2, 0.8]}
uniform float u_audio_react = 1.0; // {"widget":"slider", "min":0.0, "max":10.0, "default":1.0}
uniform float u_recursion = 3.0; // {"widget":"slider", "min":1.0, "max":6.0, "default":3.0, "smooth":true}
uniform float u_morph = 0.0; // {"widget":"slider", "min":0.0, "max":1.0, "default":0.0}

const float PI = 3.14159265359;

// SDF for a rounded cylinder
float sdRoundCylinder(vec3 p, float ra, float h) {
    vec2 d = vec2(length(p.xz) - ra, abs(p.y) - h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// Main distance function for the DNA helix
// Returns vec2(distance, material_id) as per SHADERS.md spec
vec2 map(vec3 p) {
    // Apply morphing via space warping
    vec3 q = p + sin(p.yzx * PI + iTime * 0.2) * u_morph * 0.5;

    // --- Fractal Domain Morphing ---
    float audio_bass = iAudioBands.x * u_audio_react;
    float scale = 2.0 + audio_bass * 2.0;
    float total_scale = 1.0;

    for (int i = 0; i < int(u_recursion); i++) {
        q = abs(q) - vec3(1.5, 0.5, 1.5);
        q = q * scale - vec3(1.0, 0.5, 1.0) * (scale - 1.0);
        q = mat3(0.707, -0.707, 0.0, 0.707, 0.707, 0.0, 0.0, 0.0, 1.0) * q;
        total_scale *= scale;
    }

    // --- DNA Helix Structure ---
    float time = iTime * u_speed;
    float audio_amp_react = iAudioAmp * u_audio_react;

    // The two helices (Material ID 1.0)
    float twist_val = q.y * u_twist;
    vec3 p1 = q + vec3(u_radius * cos(twist_val + time), 0, u_radius * sin(twist_val + time));
    vec3 p2 = q + vec3(u_radius * cos(twist_val + time + 3.14159), 0, u_radius * sin(twist_val + time + 3.14159));

    float helix_strand_radius = 0.1 + audio_amp_react * 0.1;
    vec2 res = vec2(sdRoundCylinder(p1, helix_strand_radius, 2.0), 1.0);
    res = min(res, vec2(sdRoundCylinder(p2, helix_strand_radius, 2.0), 1.0));

    // The rungs connecting the helices (Material ID 2.0)
    vec3 pr = q;
    pr.y = mod(q.y, 0.5) - 0.25;
    float rung_angle = floor(q.y / 0.5) * (PI / u_twist) + time;
    pr = mat3(cos(rung_angle), 0, -sin(rung_angle), 0, 1, 0, sin(rung_angle), 0, cos(rung_angle)) * pr;
    float rung_dist = sdRoundCylinder(pr, u_radius, 0.02);
    
    if (rung_dist < res.x) {
        res = vec2(rung_dist, 2.0);
    }

    // Correctly scale the distance back to world space
    res.x /= total_scale;
    return res;
}

// Calculate the normal of the surface at a point
// Correctly uses map(p).x as per SHADERS.md spec
vec3 calcNormal(vec3 p) {
    const float h = 0.001;
    const vec2 k = vec2(1, -1);
    return normalize(k.xyy * map(p + k.xyy * h).x +
                     k.yyx * map(p + k.yyx * h).x +
                     k.yxy * map(p + k.yxy * h).x +
                     k.xxx * map(p + k.xxx * h).x);
}

void main() {
    // Correct UV calculation for standard main() function
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // Fixed camera setup
    vec3 ro = vec3(0.0, 0.0, 5.0); // Fixed camera position
    vec3 rd = normalize(vec3(uv, -1.0)); // Fixed ray direction

    // Raymarching
    float t = 0.0;
    vec3 col = vec3(0.0);
    float max_dist = 20.0;

    for (int i = 0; i < 100; i++) {
        vec3 p = ro + rd * t;
        vec2 res = map(p);
        float d = res.x;

        if (d < 0.001) {
            float materialID = res.y;
            vec3 normal = calcNormal(p);
            
            // Lighting
            vec3 lightPos = vec3(2.0, 3.0, -3.0);
            vec3 lightDir = normalize(lightPos - p);
            float diff = max(dot(normal, lightDir), 0.1);

            // Coloring based on Material ID
            vec3 base_color;
            if (materialID == 1.0) {
                float color_mix = smoothstep(0.0, 1.0, sin(p.y * 2.0 + iTime) * 0.5 + 0.5);
                color_mix += iAudioBands.z * u_audio_react;
                base_color = mix(u_color1, u_color2, color_mix);
            } else { // materialID == 2.0
                base_color = mix(u_color1, u_color2, iAudioAmp * u_audio_react * 2.0);
            }
            
            // Ambient Occlusion (simple)
            float ao = clamp(map(p + normal * 0.1).x * 10.0, 0.0, 1.0);

            col = base_color * diff * ao + base_color * 0.1; // Diffuse + Ambient
            break;
        }
        t += d;
        if (t > max_dist) {
            break;
        }
    }

    FragColor = vec4(col, 1.0);
}
