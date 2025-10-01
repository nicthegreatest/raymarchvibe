#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform vec2 iResolution;
uniform float iTime;
uniform sampler2D iChannel0; // Texture input

// {"default": 1.0, "min": 0.1, "max": 5.0, "label": "Scale"}
uniform float u_scale = 1.0;

// {"default": 0.5, "min": -2.0, "max": 2.0, "label": "Rotation Speed"}
uniform float u_rotationSpeed = 0.5;

// {"default": 1.0, "min": 0.0, "max": 1.0, "label": "Rotation Dir X"}
uniform float u_rotationDirX = 1.0;
// {"default": 1.0, "min": 0.0, "max": 1.0, "label": "Rotation Dir Y"}
uniform float u_rotationDirY = 1.0;
// {"default": 1.0, "min": 0.0, "max": 1.0, "label": "Rotation Dir Z"}
uniform float u_rotationDirZ = 1.0;


mat3 rotate(float angle, vec3 axis) {
    vec3 a = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float r = 1.0 - c;
    return mat3(
        a.x * a.x * r + c,
        a.y * a.x * r + a.z * s,
        a.z * a.x * r - a.y * s,
        a.x * a.y * r - a.z * s,
        a.y * a.y * r + c,
        a.z * a.y * r + a.x * s,
        a.x * a.z * r + a.y * s,
        a.y * a.z * r - a.x * s,
        a.z * a.z * r + c
    );
}

float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

vec2 sphereTexCoords(vec3 p) {
    float phi = atan(p.z, p.x);
    float theta = asin(p.y / length(p));
    return vec2(phi / (2.0 * 3.14159265359) + 0.5, theta / 3.14159265359 + 0.5);
}

void main() {
    vec2 uv = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;

    vec3 ro = vec3(0.0, 0.0, -3.0);
    vec3 rd = normalize(vec3(uv, 1.0));

    float t = 0.0;
    for (int i = 0; i < 64; i++) {
        vec3 p = ro + rd * t;

        vec3 rotationAxis = normalize(vec3(u_rotationDirX, u_rotationDirY, u_rotationDirZ));
        mat3 rotationMatrix = rotate(iTime * u_rotationSpeed, rotationAxis);
        p = rotationMatrix * p;

        float d = sdSphere(p, u_scale);
        if (d < 0.001) {
            vec2 sphereUV = sphereTexCoords(p);
            FragColor = texture(iChannel0, sphereUV);
            return;
        }
        t += d;
        if (t > 100.0) {
            break;
        }
    }

    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
