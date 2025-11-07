#version 330 core
out vec4 FragColor;

// Standard uniforms
uniform vec2 iResolution;
uniform float iTime;
uniform vec3 iMouse;

// Input texture
uniform sampler2D iChannel0;

// Audio uniforms
uniform float iAudioAmp;
uniform vec4 iAudioBands;

// --- UI Controls ---
uniform bool u_useTexture = false; // {"label":"Use Texture"}
uniform vec3 u_color = vec3(0.2, 0.5, 1.0); // {"widget":"color", "label":"Sphere Color"}
uniform float u_radius = 0.5; // {"label":"Radius", "default":0.5, "min":0.1, "max":2.0}
uniform float u_shininess = 32.0; // {"label":"Shininess", "default":32.0, "min":2.0, "max":256.0}
uniform float u_lightIntensity = 1.5; // {"label":"Light Intensity", "default":1.5, "min":0.0, "max":5.0}
uniform vec3 u_lightColor = vec3(1.0, 1.0, 0.95); // {"widget":"color", "label":"Light Color"}
uniform bool u_moveLight = true; // {"label":"Animate Light"}
uniform vec3 u_lightPosition = vec3(1.5, 1.5, 2.5); // {"label":"Light Position", "min":[-5.0, -5.0, -5.0], "max":[5.0, 5.0, 5.0]}
uniform float u_rotationSpeed = 0.5; // {"label":"Rotation Speed", "default":0.5, "min":-2.0, "max":2.0}
uniform vec3 u_rotationAxis = vec3(0.0, 1.0, 0.0); // {"label":"Rotation Axis", "default":[0.0, 1.0, 0.0]}
uniform vec2 u_texScale = vec2(1.0, 1.0); // {"label":"Texture Scale", "default":[1.0, 1.0], "min":[0.1, 0.1], "max":[10.0, 10.0]}
uniform vec2 u_texOffset = vec2(0.0, 0.0); // {"label":"Texture Offset", "default":[0.0, 0.0], "min":[-1.0, -1.0], "max":[1.0, 1.0]}
uniform float u_texRotation = -90.0; // {"label":"Texture Rotation", "default":-90.0, "min":-180.0, "max":180.0}
// --- End UI Controls ---

const float PI = 3.14159265359;

// Function to create a 3D rotation matrix from an axis and an angle
mat3 rotationMatrix(vec3 axis, float angle) {
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat3(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c);
}

// Function to create a 2D rotation matrix
mat2 rotationMatrix2D(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

// Signed Distance Function for a sphere
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

// Scene SDF
vec2 map(vec3 pos) {
    // Rotation
    float angle = iTime * u_rotationSpeed;
    mat3 rotM = rotationMatrix(u_rotationAxis, angle);
    pos = transpose(rotM) * pos; // Apply inverse rotation to the point

    // Pulse the radius with the audio amplitude
    float audioPulse = iAudioAmp * 0.2;
    float distance = sdSphere(pos, u_radius + audioPulse);
    
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

// Get spherical UV coordinates for a point on the sphere surface
vec2 getSphereUV(vec3 p) {
    p = normalize(p);
    float u = 0.5 + atan(p.z, p.x) / (2.0 * PI);
    float v = 0.5 - asin(p.y) / PI;
    return vec2(u, v);
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    // --- Camera and Ray Setup ---
    vec3 ro = vec3(0.0, 0.0, 2.5); // Ray Origin (camera position)
    vec3 rd = normalize(vec3(uv, -1.0)); // Ray Direction

    // --- Light Position ---
    vec3 lightPos;
    if (u_moveLight) {
        lightPos = vec3(cos(iTime * 0.5) * 2.0, sin(iTime * 0.5) * 2.0, 2.0);
    } else {
        lightPos = u_lightPosition;
    }

    // --- Raymarching ---
    float d = 0.0;
    vec3 p = ro; // Current point along the ray
    for (int i = 0; i < 100; i++) {
        d = map(p).x;
        if (d < 0.001) break; // Hit
        if (d > 100.0) break; // Miss
        p += rd * d;
    }

    // --- Shading ---
    vec3 col = vec3(0.0);

    if (d < 0.001) { // If the ray hit the sphere
        vec3 nor = calcNormal(p);
        vec3 lightDir = normalize(lightPos - p);
        vec3 viewDir = normalize(ro - p);

        // --- Get Base Color (from texture or uniform) ---
        vec3 baseColor;
        if (u_useTexture) {
            // To make the texture rotate with the sphere, we must find the surface
            // point in the sphere's own "object space" before getting the UVs.
            float angle = iTime * u_rotationSpeed;
            mat3 rotM = rotationMatrix(u_rotationAxis, angle);
            vec3 objectSpaceP = transpose(rotM) * p;

            vec2 sphereUV = getSphereUV(objectSpaceP);
            
            // Apply scale and offset to the texture coordinates
            sphereUV = (sphereUV * u_texScale) + u_texOffset;

            // Apply 2D rotation to the texture coordinates
            float angleRad = radians(u_texRotation);
            mat2 rot2D = rotationMatrix2D(angleRad);
            sphereUV = rot2D * (sphereUV - 0.5) + 0.5; // Rotate around the center of the texture

            baseColor = texture(iChannel0, sphereUV).rgb;
        } else {
            baseColor = u_color;
        }

        // --- Lighting Calculation ---
        // Ambient
        float ambientStrength = 0.1;
        vec3 ambient = ambientStrength * u_lightColor;

        // Diffuse
        float diff = max(dot(nor, lightDir), 0.0);
        vec3 diffuse = diff * u_lightColor;

        // Specular
        vec3 reflectDir = reflect(-lightDir, nor);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_shininess);
        vec3 specular = spec * u_lightColor;

        // Combine
        col = (ambient + (diffuse + specular) * u_lightIntensity) * baseColor;

    }

    // --- Final Output ---
    float vignette = 1.0 - length(uv * 0.7);
    col *= smoothstep(0.0, 1.0, vignette);

    FragColor = vec4(col, 1.0);
}