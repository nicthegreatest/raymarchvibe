#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D iChannel0;
uniform vec2 iResolution;
uniform float intensity = 1.0;
uniform float radius = 0.5;

void main()
{
    vec2 uv = TexCoords;
    vec4 color = texture(iChannel0, uv);

    float dist = distance(uv, vec2(0.5, 0.5));
    float vignette = smoothstep(radius, radius - 0.4, dist * intensity);

    FragColor = vec4(color.rgb * vignette, color.a);
}
