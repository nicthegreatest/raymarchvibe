#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform vec2 iResolution;

// @control float _Aberration "Aberration" "min=-0.1 max=0.1 step=0.001"
uniform float _Aberration = 0.01;
// @control float _AberrationSmoothness "Smoothness" "min=0.0 max=1.0 step=0.01"
uniform float _AberrationSmoothness = 0.5;

void main()
{
    vec2 uv = TexCoords;
    vec2 center_dist = uv - 0.5;
    float dist = length(center_dist);

    float aberration_amount = _Aberration * dist;

    // Smoothness controls the onset of the effect from the center.
    float smoothness_factor = smoothstep(0.0, _AberrationSmoothness, dist);
    aberration_amount *= smoothness_factor;

    vec4 color;
    color.r = texture(screenTexture, uv - vec2(aberration_amount, 0.0)).r;
    color.g = texture(screenTexture, uv).g; // Green channel is the reference
    color.b = texture(screenTexture, uv + vec2(aberration_amount, 0.0)).b;
    color.a = texture(screenTexture, uv).a;

    FragColor = color;
}
