#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D iChannel0;

void main() {
    FragColor = texture(iChannel0, TexCoords);
}
