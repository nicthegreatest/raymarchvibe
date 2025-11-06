#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;

uniform float u_dither_amount; // {"widget":"slider", "default": 1.0, "min": 0.0, "max": 1.0, "step": 0.01, "label": "Dither Amount"}
uniform float u_dither_scale;  // {"widget":"slider", "default": 4.0, "min": 1.0, "max": 16.0, "step": 1.0, "label": "Dither Scale"}

// 4x4 Bayer matrix
const mat4 bayer4x4 = mat4(
    vec4( 0.0/16.0,  8.0/16.0,  2.0/16.0, 10.0/16.0),
    vec4(12.0/16.0,  4.0/16.0, 14.0/16.0,  6.0/16.0),
    vec4( 3.0/16.0, 11.0/16.0,  1.0/16.0,  9.0/16.0),
    vec4(15.0/16.0,  7.0/16.0, 13.0/16.0,  5.0/16.0)
);

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec4 originalColor = texture(iChannel0, uv);

    // Convert to grayscale
    float lum = dot(originalColor.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Get dither threshold
    int x = int(mod(gl_FragCoord.x / u_dither_scale, 4.0));
    int y = int(mod(gl_FragCoord.y / u_dither_scale, 4.0));
    float threshold = bayer4x4[x][y];

    // Dither
    vec3 ditheredColor = (lum > threshold) ? vec3(1.0) : vec3(0.0);

    // Mix original and dithered color
    vec3 finalColor = mix(originalColor.rgb, ditheredColor, u_dither_amount);

    FragColor = vec4(finalColor, originalColor.a);
}
