#version 330 core

out vec4 FragColor;

// --- Shader Parameters ---
uniform vec3 u_objectColor = vec3(0.8, 0.5, 0.3);      // {"widget":"color", "label":"Object Color"}
uniform vec3 u_lightColor = vec3(1.0, 0.9, 0.8);       // {"widget":"color", "label":"Light Color"}
uniform float u_smoothness = 0.1;                     // {"default":0.1, "min":0.0, "max":1.0, "label":"Smooth Union"}
uniform bool u_enableAO = true;                       // {"label":"Enable AO"}
uniform bool u_enableSoftShadows = true;              // {"label":"Enable Soft Shadows"}
uniform float u_scale = 1.0;                          // {"default": 1.0, "min": 0.1, "max": 2.0, "label": "Scale"}
uniform float u_timeSpeed = 0.2;                      // {"default": 0.2, "min": 0.0, "max": 2.0, "label": "Time Speed"}
uniform vec3 u_colorMod = vec3(0.1, 0.1, 0.2);        // {"widget":"color", "label": "Color Modulation"}
uniform float u_patternScale = 1.0;                   // {"default": 1.0, "min": 0.5, "max": 20.0, "label": "Pattern Scale"}
uniform vec3 u_lightPosition = vec3(2.0, 3.0, -2.0);  // {"label": "Light Position"}

// --- Standard Uniforms (from your C++ app) ---
uniform vec2 iResolution;
uniform float iTime;
uniform sampler2D iChannel0; // Input texture from another effect
uniform bool iChannel0_active;
uniform vec3 iCameraPosition;
uniform mat4 iCameraMatrix;

// --- Helper Functions ---
float radians(float degrees) {
    return degrees * 3.14159265358979323846 / 180.0;
}

// Smooth minimum function - for smooth union of SDFs
float opSmoothUnion( float d1, float d2, float k ) {
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h);
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
    float dodecaInradius = 0.5 * u_scale;

    // Animation: simple floating or fixed position
    vec3 objectOffset = vec3(0.0, 0.6 * u_scale, 0.0); // Centered slightly above origin

    vec3 p_transformed = p - objectOffset;

    // Apply slow rotation
    mat3 rotY = rotateY(effectiveTime * 0.5); // Slow rotation around Y
    mat3 rotX = rotateX(effectiveTime * 0.3); // Slow rotation around X
    p_transformed = rotX * rotY * p_transformed;

    float dodecaDist = sdDodecahedron(p_transformed, dodecaInradius);

    return dodecaDist;
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

// --- Soft Shadow Calculation ---
// Marches a ray from a point towards the light source to check for occlusion.
// 'k' is a softness parameter.
float getSoftShadow(vec3 ro, vec3 rd, float tmin, float tmax, float k, float time) {
    float res = 1.0;
    float t = tmin;
    for(int i=0; i<32; i++) { // Fewer steps for shadow rays
        if(t < tmax) {
            float h = mapScene(ro + rd * t, time);
            if(h < 0.001) return 0.0; // Hard shadow if we hit something
            res = min(res, k * h / t);
            t += h * 0.8;
        }
    }
    return clamp(res, 0.0, 1.0);
}



// --- Main Shader Logic ---
void main() {
    vec2 p_ndc = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;

    // Ray Setup
    vec3 ro = iCameraPosition;
    vec3 rd = (iCameraMatrix * vec4(normalize(vec3(p_ndc, -1.0)), 0.0)).xyz;

    float t = rayMarch(ro, rd, iTime);

    vec3 col = vec3(0.05, 0.05, 0.1); // Darker background

    if (t > -0.5) { // If we hit something
        vec3 hitPos = ro + rd * t;
        vec3 normal = getNormal(hitPos, iTime);

        // Lighting
        vec3 lightDir = normalize(u_lightPosition - hitPos);
        float diffuse = max(0.0, dot(normal, lightDir));
        
        float ao;
        if (u_enableAO) {
        // Ambient Occlusion (simple)
        ao = 0.0;
        float aoStep = 0.01 * u_scale;
        float aoTotalDist = 0.0;
        for(int j=0; j<4; j++){ // Fewer AO steps for performance
            aoTotalDist += aoStep;
            float d_ao = mapScene(hitPos + normal * aoTotalDist * 0.5, iTime); // Shorter AO rays
            ao += max(0.0, (aoTotalDist*0.5 - d_ao));
            aoStep *= 1.8;
        }
        ao = 1.0 - clamp(ao * (0.15 / (u_scale*u_scale) ), 0.0, 1.0);
        } else {
        ao = 1.0; // AO is disabled, so full ambient light
        }

        float shadow = 1.0; // Default to no shadow
        if (u_enableSoftShadows) {
        // Soft Shadows
        shadow = getSoftShadow(hitPos + normal * 0.002, lightDir, 0.001, 5.0, 16.0, iTime);
        }

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
        diffuse *= shadow; // Apply shadow to diffuse light
        col = baseColor * (diffuse * 0.6 + 0.3 * ao) * u_lightColor; // Adjusted ambient/AO contribution
        col += u_lightColor * specular * 0.35 * shadow; // Shadow affects specular too

        // Fog
        float fogFactor = smoothstep(u_scale * 1.5, u_scale * 10.0, t);
        col = mix(col, vec3(0.05, 0.05, 0.1), fogFactor);
    } else {
        // Background gradient
        col = vec3(0.1, 0.1, 0.3) + vec3(0.2, 0.1, 0.0) * pow(1.0 - abs(p_ndc.y), 2.0);
    }

    // Conditionally apply the input from iChannel0
    if (iChannel0_active) { // Check if the texture is valid
        vec2 uv = gl_FragCoord.xy / iResolution.xy;
        vec4 inputColor = texture(iChannel0, uv);
        col = mix(col, inputColor.rgb, inputColor.a); // Blend with input
    }

    FragColor = vec4(col, 1.0);
}
