#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec4 texColor = texture(iChannel0, uv);
    FragColor = vec4(1.0 - texColor.rgb, texColor.a);
}
