#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    FragColor = texture(iChannel0, uv);
}
