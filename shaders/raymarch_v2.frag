#version 330 core

out vec4 FragColor;

// --- Standard Uniforms (from your C++ app) ---
uniform vec2 iResolution;
uniform float iTime;

// --- Native Controls (from your C++ app) ---
uniform vec3 u_objectColor;   // Base color of the dodecahedron
uniform float u_scale;        // Overall scale of the dodecahedron
uniform float u_timeSpeed;    // Speed of rotation
uniform vec3 u_colorMod;      // For color effects (can be used for psychedelic patterns)
uniform float u_patternScale; // Can be repurposed (e.g., for surface pattern frequency)

// Camera Uniforms
uniform vec3 u_camPos;
uniform vec3 u_camTarget;
uniform float u_camFOV; // Field of View in degrees

// Light Uniforms
uniform vec3 u_lightPosition;
uniform vec3 u_lightColor;

// --- Helper Functions ---
float radians(float degrees) {
    return degrees * 3.14159265358979323846 / 180.0;
}

// Rotation matrix for Y-axis
mat3 rotateY(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c, 0, s,
        0, 1, 0,
        -s, 0, c
    );
}

// Rotation matrix for X-axis
mat3 rotateX(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        1, 0, 0,
        0, c, -s,
        0, s, c
    );
}

// --- SDF for a Regular Dodecahedron ---
// Adapted from Inigo Quilez's work.
// 'r_inradius' is the radius of the inscribed sphere (distance from center to the center of each pentagonal face).
float sdDodecahedron(vec3 p, float r_inradius) {
    p = abs(p); // Fold into the first octant

    // The dodecahedron's geometry is related to the golden ratio phi
    const float phi = 1.61803398875; // (1.0 + sqrt(5.0)) / 2.0

    // Normals to the three distinct face orientations in the first octant (not unit vectors here)
    // These vectors point from the origin to the vertices of an icosahedron,
    // which are the face normals of its dual, the dodecahedron.
    vec3 n1 = vec3(1.0, phi, 0.0);
    vec3 n2 = vec3(0.0, 1.0, phi);
    vec3 n3 = vec3(phi, 0.0, 1.0);

    // The SDF is the maximum of the dot products with these normals, minus the inradius.
    // We need to normalize the normals for the dot product to represent distance correctly.
    float d = dot(p, normalize(n1));
    d = max(d, dot(p, normalize(n2)));
    d = max(d, dot(p, normalize(n3)));
    
    return d - r_inradius;
}

// --- Scene Definition ---
float mapScene(vec3 p, float time) {
    float effectiveTime = time * u_timeSpeed * 0.2; // Slow down overall animation/rotation

    // Dodecahedron properties
    // The 'r_inradius' for sdDodecahedron is the distance from center to face.
    // If u_scale is meant to be an overall "outer" radius, we need a conversion.
    // For a dodecahedron, outer_radius (center to vertex) = r_inradius * phi * sqrt(3) / 2
    // Or, if u_scale is approx outer radius: r_inradius approx u_scale / (phi * sqrt(3)/2 * some_factor)
    // Let's make u_scale directly control the inradius for simplicity here, and adjust visually.
    float dodecaInradius = 0.5 * u_scale;

    // Animation: simple floating or fixed position
    vec3 objectOffset = vec3(0.0, 0.6 * u_scale, 0.0); // Centered slightly above origin

    vec3 p_transformed = p - objectOffset;

    // Apply slow rotation
    mat3 rotY = rotateY(effectiveTime * 0.5); // Slow rotation around Y
    mat3 rotX = rotateX(effectiveTime * 0.3); // Slow rotation around X
    p_transformed = rotX * rotY * p_transformed;

    float dodecaDist = sdDodecahedron(p_transformed, dodecaInradius);

    // Optional ground plane for context
    float planeDist = p.y + 0.0; // Plane at y = 0

    return min(dodecaDist, planeDist);
    // return dodecaDist; // Uncomment to see only the dodecahedron
}

// --- Normal Calculation (using the scene map) ---
vec3 getNormal(vec3 p, float time) {
    float epsilon = 0.0005; // Smaller epsilon for potentially sharper features
    vec2 e = vec2(epsilon, 0.0);
    return normalize(vec3(
        mapScene(p + e.xyy, time) - mapScene(p - e.xyy, time),
        mapScene(p + e.yxy, time) - mapScene(p - e.yxy, time),
        mapScene(p + e.yyx, time) - mapScene(p - e.yyx, time)
    ));
}

// --- Raymarching ---
float rayMarch(vec3 ro, vec3 rd, float time) {
    float t = 0.0;
    for (int i = 0; i < 96; i++) { // Max steps (can be adjusted)
        vec3 p = ro + rd * t;
        float dist = mapScene(p, time);
        // Adjust hit threshold based on distance to avoid overshooting
        if (dist < (0.001 * t) || dist < 0.0001) { 
             return t;
        }
        t += dist * 0.8; // March slightly less than full distance for stability
        if (t > 30.0) { // Max ray distance
            break;
        }
    }
    return -1.0; // No hit / hit too far
}

// --- Camera Setup ---
mat3 setCamera(vec3 ro, vec3 ta, vec3 worldUp) {
    vec3 f = normalize(ta - ro);
    vec3 r = normalize(cross(f, worldUp));
    if (length(r) < 0.0001) { r = normalize(cross(f, vec3(1.0,0.0,0.0))); }
    vec3 u = normalize(cross(r, f));
    return mat3(r, u, f);
}

// --- Main Shader Logic ---
void main() {
    vec2 p_ndc = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;

    // Ray Setup
    vec3 ro = u_camPos;
    vec3 ta = u_camTarget;
    mat3 camToWorld = setCamera(ro, ta, vec3(0.0, 1.0, 0.0));
    float fovFactor = tan(radians(u_camFOV) * 0.5);
    vec3 rd_local = normalize(vec3(p_ndc.x * fovFactor, p_ndc.y * fovFactor, 1.0));
    vec3 rd = camToWorld * rd_local;

    float t = rayMarch(ro, rd, iTime);

    vec3 col = vec3(0.05, 0.05, 0.1); // Darker background

    if (t > -0.5) { // If we hit something
        vec3 hitPos = ro + rd * t;
        vec3 normal = getNormal(hitPos, iTime);

        // Lighting
        vec3 lightDir = normalize(u_lightPosition - hitPos);
        float diffuse = max(0.0, dot(normal, lightDir));
        
        // Ambient Occlusion (simple)
        float ao = 0.0;
        float aoStep = 0.01 * u_scale;
        float aoTotalDist = 0.0;
        for(int j=0; j<4; j++){ // Fewer AO steps for performance
            aoTotalDist += aoStep;
            float d_ao = mapScene(hitPos + normal * aoTotalDist * 0.5, iTime); // Shorter AO rays
            ao += max(0.0, (aoTotalDist*0.5 - d_ao));
            aoStep *= 1.8;
        }
        ao = 1.0 - clamp(ao * (0.15 / (u_scale*u_scale) ), 0.0, 1.0);


        // Specular
        vec3 viewDir = normalize(ro - hitPos);
        vec3 reflectDir = reflect(-lightDir, normal);
        float specAngle = max(dot(viewDir, reflectDir), 0.0);
        float specular = pow(specAngle, 32.0);

        vec3 baseColor = u_objectColor;
        // Add some color variation based on normals and u_colorMod
        baseColor += u_colorMod * (0.4 + 0.6 * abs(normal.x * sin(iTime * u_timeSpeed * 0.5 + normal.y * u_patternScale * 5.0)));
        baseColor = clamp(baseColor, 0.0, 1.0);

        // Combine lighting components
        col = baseColor * (diffuse * 0.6 + 0.3 * ao) * u_lightColor; // Adjusted ambient/AO contribution
        col += u_lightColor * specular * 0.35; // Adjusted specular

        // Fog
        float fogFactor = smoothstep(u_scale * 1.5, u_scale * 10.0, t);
        col = mix(col, vec3(0.05, 0.05, 0.1), fogFactor);
    } else {
        // Background gradient
        col = vec3(0.1, 0.1, 0.3) + vec3(0.2, 0.1, 0.0) * pow(1.0 - abs(p_ndc.y), 2.0);
    }

    FragColor = vec4(col, 1.0);
}
