#version 330 core
out vec4 FragColor;

// Standard uniforms
uniform vec2 iResolution;
uniform float iTime;

// --- UI Controls ---
uniform int u_points = 8; // {"label":"Points", "default":8, "min":3, "max":16}
uniform float u_innerRadius = 0.2; // {"label":"Inner Radius", "default":0.2, "min":0.1, "max":2.0}
uniform float u_outerRadius = 0.5; // {"label":"Outer Radius", "default":0.5, "min":0.2, "max":2.5}
uniform float u_height = 0.1; // {"label":"Density", "default":0.1, "min":0.05, "max":1.0}
uniform float u_pulseSpeed = 0.5; // {"label":"Pulse", "default":0.5, "min":0.0, "max":8.0}
uniform float u_glow = 0.45; // {"label":"Glow", "default":0.45, "min":0.0, "max":4.0}
uniform float u_opacity = 0.8; // {"label":"Energy", "default":0.8, "min":0.0, "max":1.0}
uniform vec3 u_colorA = vec3(0.31, 0.43, 1.0); // {"widget":"color", "label":"Core Color"}
uniform vec3 u_colorB = vec3(0.77, 0.38, 1.0); // {"widget":"color", "label":"Aura Color"}
uniform vec3 u_colorC = vec3(1.0, 0.31, 0.49); // {"widget":"color", "label":"Edge Color"}
// --- End UI Controls ---

// SDF for a 2D star, adapted for GLSL
// p: coordinate, n: points, r1: inner radius, r2: outer radius
float sdStar(vec2 p, int n, float r1, float r2) {
    float angle = atan(p.y, p.x);
    float segment = 2.0 * 3.1415926535 / float(n);
    float angle_segment = mod(angle + segment/2.0, segment) - segment/2.0;
    float t = abs(angle_segment) / (segment/2.0);
    float r = mix(r2, r1, t);
    return length(p) - r;
}

// Scene SDF: A single 3D star
vec2 map(vec3 pos) {
    // Pulse animation
    float pulse = sin(iTime * u_pulseSpeed) * 0.05;
    
    // Create the 3D star by extruding the 2D star shape
    float d2d = sdStar(pos.xy, u_points, u_innerRadius, u_outerRadius);
    
    // Apply pulse to the 2D distance
    d2d -= pulse;

    // Combine with height to make it 3D, with slightly rounded edges
    vec2 d = vec2(d2d, abs(pos.z) - u_height);
    float distance = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - 0.01;
    
    return vec2(distance, 1.0);
}

// Calculate the normal of the surface at a point p
vec3 calcNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        map(p + e.xyy).x - map(p - e.xyy).x,
        map(p + e.yxy).x - map(p - e.yxy).x,
        map(p + e.yyx).x - map(p - e.yyx).x
    ));
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    
    // Camera setup
    vec3 ro = vec3(0.0, 0.0, 1.5); // Ray Origin (camera position)
    vec3 rd = normalize(vec3(uv, -1.0)); // Ray Direction

    float d = 0.0;
    vec3 p = ro;

    // Raymarching loop
    for (int i = 0; i < 100; i++) {
        d = map(p).x;
        if (d < 0.001) {
            break;
        }
        if (d > 20.0) {
            break;
        }
        p += rd * d;
    }

    // If the ray hit something, apply shading
    if (d < 0.001) {
        vec3 nor = calcNormal(p);
        
        // --- Recreate the shading from the original shader ---
        float radius = length(p.xy);
        float gradient = smoothstep(0.0, u_outerRadius * 1.5, radius);
        float pulse_color = sin(iTime * u_pulseSpeed * 2.0) * 0.3 + 0.7;

        vec3 baseColor = mix(u_colorA, u_colorB, gradient);
        
        float noise = sin(p.x * 15.0 + iTime) * cos(p.y * 15.0 + iTime) * 0.2;
        vec3 finalColor = mix(baseColor, u_colorC, noise);
        
        // Fresnel for edge glow
        float fresnel = pow(1.0 - dot(normalize(-p), nor), 3.0);
        finalColor += vec3(1.0) * fresnel * pulse_color * u_glow;
        
        // Energy flow effect
        float energyFlow = sin(radius * 10.0 - iTime * 2.0) * 0.5 + 0.5;
        finalColor *= (1.0 + energyFlow * 0.3);

        FragColor = vec4(finalColor, u_opacity);
    } else {
        // Background (if ray hits nothing)
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
