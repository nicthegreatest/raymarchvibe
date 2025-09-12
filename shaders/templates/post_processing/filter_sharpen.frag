#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform vec2 iResolution;

// @control float _Amount "Amount" "min=0.0 max=5.0 step=0.1"
uniform float _Amount = 1.0;

void main()
{
    vec2 texel = 1.0 / iResolution.xy;
    vec4 originalColor = texture(screenTexture, TexCoords);

    // Simplified box blur for the "unsharp" part
    vec4 blur = texture(screenTexture, TexCoords + vec2( texel.x, 0.0)) +
                texture(screenTexture, TexCoords + vec2(-texel.x, 0.0)) +
                texture(screenTexture, TexCoords + vec2(0.0,  texel.y)) +
                texture(screenTexture, TexCoords + vec2(0.0, -texel.y));
    blur *= 0.25;

    // Unsharp masking: sharpened = original + (original - blurred) * amount
    vec4 sharpened = originalColor + (originalColor - blur) * _Amount;

    FragColor = vec4(sharpened.rgb, originalColor.a);
}
