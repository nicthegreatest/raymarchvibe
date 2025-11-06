#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;
#define VIGNETTE_INTENSITY 1.0 // {"widget":"slider", "min": 0.0, "max": 2.0, "step": 0.01, "label": "Intensity"}
#define VIGNETTE_RADIUS 0.5    // {"widget":"slider", "min": 0.0, "max": 1.0, "step": 0.01, "label": "Radius"}

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec4 color = texture(iChannel0, uv);

    float dist = distance(uv, vec2(0.5, 0.5));
    float vignette = smoothstep(VIGNETTE_RADIUS, VIGNETTE_RADIUS - 0.4, dist * VIGNETTE_INTENSITY);

    FragColor = vec4(color.rgb * vignette, color.a);
}
