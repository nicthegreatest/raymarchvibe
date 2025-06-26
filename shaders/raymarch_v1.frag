#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

// Native Uniforms
uniform vec3 u_objectColor = vec3(0.7, 0.5, 0.9); // Default to a violet
uniform float u_scale = 0.8;        // Overall size of the dodecahedron
uniform float u_timeSpeed = 0.3;    // Speed of rotation
uniform vec3 u_colorMod = vec3(0.2, 0.3, 0.4); // For color variations
uniform float u_patternScale = 5.0; // For surface pattern frequency

// Camera Uniforms
uniform vec3 u_camPos = vec3(0.0, 0.8, -2.5);
uniform vec3 u_camTarget = vec3(0.0, 0.0, 0.0);
uniform float u_camFOV = 60.0;

// Light Uniforms
uniform vec3 u_lightPosition = vec3(3.0, 2.0, -4.0);
uniform vec3 u_lightColor = vec3(1.0, 0.95, 0.9);

uniform float iAudioAmp = 0.0; // Audio amplitude, 0 to 1

// Constants
const float PI = 3.14159265359;
const float PHI = 1.618033988749895; // Golden ratio

// Helper Functions
float radians(float degrees) {
    return degrees * PI / 180.0;
}

mat3 rotateX(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        1.0, 0.0, 0.0,
        0.0, c,  -s,
        0.0, s,   c
    );
}

mat3 rotateY(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c,  0.0, s,
        0.0,1.0, 0.0,
        -s, 0.0, c
    );
}

mat3 rotateZ(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c,  -s, 0.0,
        s,   c, 0.0,
        0.0,0.0,1.0
    );
}

// SDF for a Regular Dodecahedron
// p: point to sample
// r_inradius: radius of the inscribed sphere (distance from center to face center)
float sdDodecahedron(vec3 p, float r_inradius) {
    p = abs(p); // Fold into the first octant for symmetry

    // The normals of a dodecahedron's faces are related to the vertices of its dual, the icosahedron.
    // These vectors (not normalized here) point to icosahedron vertices.
    // Their normalized versions are the face normals of the dodecahedron.
    vec3 n1 = vec3(1.0, PHI, 0.0);
    vec3 n2 = vec3(0.0, 1.0, PHI);
    vec3 n3 = vec3(PHI, 0.0, 1.0);

    // The SDF is the maximum of the dot products of p with these three primary face normals (in the first octant),
    // minus the inradius 'r_inradius'.
    float d = dot(p, normalize(n1));
    d = max(d, dot(p, normalize(n2)));
    d = max(d, dot(p, normalize(n3)));
    
    return d - r_inradius;
}


// Scene Definition
float mapScene(vec3 p, float time) {
    float effectiveTime = time * u_timeSpeed;

    // Dodecahedron properties
    float audioEffect = 1.0 + iAudioAmp * 4.2; // Scale boost from audio
    float dodecaInradius = 0.5 * u_scale * audioEffect; // u_scale controls the inradius

    // Center the dodecahedron
    vec3 objectOffset = vec3(0.0, dodecaInradius * 0.8, 0.0); // Lift it slightly so it's not cut by a plane at y=0

    vec3 p_transformed = p - objectOffset;

    // Apply rotation
    mat3 rotMat = rotateY(effectiveTime) * rotateX(effectiveTime * 0.7) * rotateZ(effectiveTime * 0.4);
    p_transformed = rotMat * p_transformed;

    float dodecaDist = sdDodecahedron(p_transformed, dodecaInradius);
    
    // Optional: Add a ground plane for context
    // float planeDist = p.y + dodecaInradius * 1.2; // Place plane below the dodecahedron
    // return min(dodecaDist, planeDist);
    return dodecaDist;
}

// Normal Calculation
vec3 getNormal(vec3 p, float time) {
    float eps = 0.0005 * u_scale; // Epsilon relative to scale for better normals on smaller objects
    vec2 e = vec2(eps, 0.0);
    return normalize(vec3(
        mapScene(p + e.xyy, time) - mapScene(p - e.xyy, time),
        mapScene(p + e.yxy, time) - mapScene(p - e.yxy, time),
        mapScene(p + e.yyx, time) - mapScene(p - e.yyx, time)
    ));
}

// Raymarching
float rayMarch(vec3 ro, vec3 rd, float time) {
    float t = 0.0;
    for (int i = 0; i < 96; i++) { // Max steps
        vec3 p = ro + rd * t;
        float dist = mapScene(p, time);
        if (dist < (0.001 * t) || dist < 0.0001) { // Adjusted hit threshold
             return t;
        }
        t += dist * 0.75; // March factor, can be tuned
        if (t > 30.0) { // Max ray distance
            break;
        }
    }
    return -1.0; // No hit
}

// Camera Setup
mat3 setCamera(vec3 ro, vec3 ta, vec3 worldUp) {
    vec3 f = normalize(ta - ro);
    vec3 r = normalize(cross(f, worldUp));
    if (length(r) < 0.0001) { r = normalize(cross(f, vec3(0.0,0.0,-1.0))); } // Fallback if up is parallel
    if (length(r) < 0.0001) { r = normalize(cross(f, vec3(1.0,0.0,0.0))); } // Further fallback
    vec3 u = normalize(cross(r, f));
    return mat3(r, u, f);
}

// Main Shader Logic
void main() {
    vec2 p_ndc = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;

    vec3 ro = u_camPos;
    vec3 ta = u_camTarget;
    mat3 camToWorld = setCamera(ro, ta, vec3(0.0, 1.0, 0.0));
    float fovFactor = tan(radians(u_camFOV) * 0.5);
    vec3 rd_local = normalize(vec3(p_ndc.x * fovFactor, p_ndc.y * fovFactor, 1.0));
    vec3 rd = camToWorld * rd_local;

    float t = rayMarch(ro, rd, iTime);

    vec3 col = vec3(0.05, 0.02, 0.08); // Dark purple background

    if (t > -0.5) { // If we hit something
        vec3 hitPos = ro + rd * t;
        vec3 normal = getNormal(hitPos, iTime);

        // Lighting
        vec3 lightDir = normalize(u_lightPosition - hitPos);
        float diffuse = max(0.0, dot(normal, lightDir));
        
        // Ambient Occlusion (simple approximation)
        float ao = 0.0;
        float aoStep = 0.01 * u_scale; 
        float aoTotalDist = 0.0;
        for(int j=0; j<4; j++){
            aoTotalDist += aoStep;
            float d_ao = mapScene(hitPos + normal * aoTotalDist * 0.5, iTime);
            ao += max(0.0, (aoTotalDist*0.5 - d_ao));
            aoStep *= 1.7;
        }
        ao = 1.0 - clamp(ao * (0.1 / (u_scale*u_scale + 0.01) ), 0.0, 1.0); // Adjust AO strength

        // Specular (Phong)
        vec3 viewDir = normalize(ro - hitPos);
        vec3 reflectDir = reflect(-lightDir, normal);
        float specAngle = max(dot(viewDir, reflectDir), 0.0);
        float specular = pow(specAngle, 32.0 + u_patternScale * 10.0); // Shininess affected by patternScale

        // Base Colour with psychedelic modulation
        vec3 baseColour = u_objectColor;
        float patternVal = sin(hitPos.x * u_patternScale + iTime * u_timeSpeed * 0.5) *
                           cos(hitPos.y * u_patternScale - iTime * u_timeSpeed * 0.3) *
                           sin(hitPos.z * u_patternScale + iTime * u_timeSpeed * 0.7);
        baseColour += u_colorMod * (0.5 + 0.5 * patternVal);
        baseColour = clamp(baseColour, 0.0, 1.0);

        // Combine lighting components
        vec3 litColour = baseColour * (diffuse * 0.7 + 0.2 * ao) * u_lightColor; 
        litColour += u_lightColor * specular * 0.4 * (0.5 + 0.5 * baseColour.r); // Specular tinted by base red

        col = litColour;

        // Fog
        float fogFactor = smoothstep(u_scale * 2.0, u_scale * 15.0, t); // Adjusted fog distance
        col = mix(col, vec3(0.05, 0.02, 0.08), fogFactor);
    } else {
        // Background gradient if no hit
        col = vec3(0.1, 0.05, 0.15) + vec3(0.2, 0.1, 0.05) * pow(1.0 - abs(p_ndc.y), 3.0);
    }

    // Basic gamma correction
    col = pow(col, vec3(1.0/2.2));
    FragColor = vec4(col, 1.0);

    // DEBUG: Force output to green (REVERTED)
    // FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
