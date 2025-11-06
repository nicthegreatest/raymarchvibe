#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;

uniform float u_levels; // {"widget":"slider", "default": 5.0, "min": 2.0, "max": 32.0, "step": 1.0, "label": "Levels"}

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec4 color = texture(iChannel0, uv);

    // Posterize effect
    color.rgb = floor(color.rgb * u_levels) / u_levels;

    FragColor = color;
}
