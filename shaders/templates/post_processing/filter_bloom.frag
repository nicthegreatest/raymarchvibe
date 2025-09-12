#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform vec2 iResolution;

// @control float _Threshold "Threshold" "min=0.0 max=2.0 step=0.05"
uniform float _Threshold = 0.8;
// @control float _Intensity "Intensity" "min=0.0 max=5.0 step=0.1"
uniform float _Intensity = 1.0;

void main()
{
    vec4 originalColor = texture(screenTexture, TexCoords);

    // A simple blur on the bright pass result.
    // This is not a "real" bloom, but a common single-pass approximation.
    vec2 texel = 1.0 / iResolution.xy;
    vec3 bloom_color = vec3(0.0);
    int samples = 0;
    for (int x = -4; x <= 4; x++) {
        for (int y = -4; y <= 4; y++) {
            vec2 offset = vec2(x, y) * texel * 2.0; // wider blur
            vec4 sample_color = texture(screenTexture, TexCoords + offset);
            // We only add the contribution of bright pixels to the bloom
            bloom_color += max(sample_color.rgb - _Threshold, 0.0);
            samples++;
        }
    }
    bloom_color /= float(samples);

    vec3 final_color = originalColor.rgb + bloom_color * _Intensity;
    FragColor = vec4(final_color, originalColor.a);
}
