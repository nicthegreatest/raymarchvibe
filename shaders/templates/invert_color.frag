#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D iChannel0; // Input texture

void main() {
    vec4 texColor = texture(iChannel0, TexCoords);
    FragColor = vec4(1.0 - texColor.rgb, texColor.a);
}
