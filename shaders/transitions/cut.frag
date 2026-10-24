#version 330 core
// The app treats "cut" as an instant switch and does not run this pass.
// The file keeps the shared uniform interface so a later drop-in can replace it.
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
    // A cut shows the incoming picture. u_progress and u_from stay in the shared interface.
    FragColor = mix(texture(u_to, uv), texture(u_from, uv), 0.0);
    FragColor.a = mix(FragColor.a, 1.0, clamp(u_progress, 0.0, 1.0));
}
