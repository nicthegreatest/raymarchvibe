#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_from;
uniform sampler2D u_to;
uniform float u_progress;
uniform vec2 u_resolution;

void main() {
    vec2 uv = TexCoords;
    if (u_resolution.x <= 0.0 || u_resolution.y <= 0.0)
        uv = clamp(TexCoords, 0.0, 1.0);
    FragColor = mix(texture(u_from, uv), texture(u_to, uv), clamp(u_progress, 0.0, 1.0));
}
