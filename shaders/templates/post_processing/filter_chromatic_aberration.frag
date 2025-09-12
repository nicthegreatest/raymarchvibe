#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D iChannel0;
uniform vec2 iResolution;

uniform float u_aberration;   // {"default": 0.01, "min": -0.1, "max": 0.1, "step": 0.001, "label": "Aberration"}
uniform float u_smoothness;   // {"default": 0.5, "min": 0.0, "max": 1.0, "step": 0.01, "label": "Smoothness"}

void main()
{
    vec2 uv = TexCoords;
    vec2 center_dist = uv - 0.5;
    float dist = length(center_dist);

    float aberration_amount = u_aberration * dist;

    // Smoothness controls the onset of the effect from the center.
    float smoothness_factor = smoothstep(0.0, u_smoothness, dist);
    aberration_amount *= smoothness_factor;

    vec4 color;
    color.r = texture(iChannel0, uv - vec2(aberration_amount, 0.0)).r;
    color.g = texture(iChannel0, uv).g; // Green channel is the reference
    color.b = texture(iChannel0, uv + vec2(aberration_amount, 0.0)).b;
    color.a = texture(iChannel0, uv).a;

    FragColor = color;
}
