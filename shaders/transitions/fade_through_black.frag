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
    float progress = clamp(u_progress, 0.0, 1.0);
    // First half fades the outgoing picture to black, second half fades the incoming picture up.
    if (progress < 0.5) {
        FragColor = texture(u_from, uv) * (1.0 - progress * 2.0);
    } else {
        FragColor = texture(u_to, uv) * ((progress - 0.5) * 2.0);
    }
    FragColor.a = 1.0;
}
