#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform float iTime;

uniform float u_intensity; // {"default": 0.1, "min": 0.0, "max": 1.0, "step": 0.01, "label": "Intensity"}
uniform float u_size;      // {"default": 1.0, "min": 1.0, "max": 10.0, "step": 0.1, "label": "Size"}

// Simple random function
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vec4 color = texture(screenTexture, TexCoords);

    vec2 uv = TexCoords * u_size;
    float noise = rand(uv + iTime) * 2.0 - 1.0; // centered noise

    vec3 final_color = color.rgb + noise * u_intensity;

    FragColor = vec4(final_color, color.a);
}
