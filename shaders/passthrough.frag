#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform float iTime;
uniform vec2 iResolution;

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec4 inputColor = texture(iChannel0, uv);

    FragColor = inputColor;
}
