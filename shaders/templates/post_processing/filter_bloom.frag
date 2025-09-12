#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform vec2 iResolution;

uniform float u_threshold; // {"default": 0.8, "min": 0.0, "max": 2.0, "step": 0.05, "label": "Threshold"}
uniform float u_intensity; // {"default": 1.0, "min": 0.0, "max": 5.0, "step": 0.1, "label": "Intensity"}

void main()
{
    vec4 originalColor = texture(screenTexture, TexCoords);

    vec2 texel = 1.0 / iResolution.xy;
    vec3 bloom_color = vec3(0.0);
    int samples = 0;
    for (int x = -4; x <= 4; x++) {
        for (int y = -4; y <= 4; y++) {
            vec2 offset = vec2(x, y) * texel * 2.0; // wider blur
            vec4 sample_color = texture(screenTexture, TexCoords + offset);
            // We only add the contribution of bright pixels to the bloom
            bloom_color += max(sample_color.rgb - u_threshold, 0.0);
            samples++;
        }
    }
    bloom_color /= float(samples);

    vec3 final_color = originalColor.rgb + bloom_color * u_intensity;
    FragColor = vec4(final_color, originalColor.a);
}
