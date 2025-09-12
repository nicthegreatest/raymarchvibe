#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform float iTime;

// @control float _Intensity "Intensity" "min=0.0 max=1.0 step=0.01"
uniform float _Intensity = 0.1;
// @control float _Size "Size" "min=1.0 max=10.0 step=0.1"
uniform float _Size = 1.0;

// Simple random function
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vec4 color = texture(screenTexture, TexCoords);

    vec2 uv = TexCoords * _Size;
    float noise = rand(uv + iTime) * 2.0 - 1.0; // centered noise

    vec3 final_color = color.rgb + noise * _Intensity;

    FragColor = vec4(final_color, color.a);
}
