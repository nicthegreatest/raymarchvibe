#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;
uniform float iTime;

uniform float u_intensity; // {"default": 0.1, "min": 0.0, "max": 1.0, "step": 0.01, "label": "Intensity"}
uniform float u_size;      // {"default": 1.0, "min": 1.0, "max": 10.0, "step": 0.1, "label": "Size"}

// Simple random function
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec4 color = texture(iChannel0, uv);

    vec2 noise_uv = uv * u_size;
    float noise = rand(noise_uv + iTime) * 2.0 - 1.0; // centered noise

    vec3 final_color = color.rgb + noise * u_intensity;

    FragColor = vec4(final_color, color.a);
}
