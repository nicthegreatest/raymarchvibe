#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_from;
uniform sampler2D u_to;
uniform float u_progress;
uniform vec2 u_resolution;

float Hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec4 fromColor = texture(u_from, TexCoords);
    vec4 toColor = texture(u_to, TexCoords);
    float luma = dot(fromColor.rgb, vec3(0.299, 0.587, 0.114));
    vec2 pixel = TexCoords * max(u_resolution, vec2(1.0));
    float grain = Hash(floor(pixel));
    // Bright pixels open first. The grain keeps a flat colour from dissolving as one sheet.
    float key = clamp(luma * 0.65 + grain * 0.35, 0.0, 1.0);
    float reveal = smoothstep(key - 0.08, key + 0.08, u_progress);
    FragColor = mix(fromColor, toColor, reveal);
    FragColor.a = 1.0;
}
