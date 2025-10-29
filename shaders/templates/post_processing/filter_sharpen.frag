#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;

uniform float u_amount; // {"widget":"slider", "default": 1.0, "min": 0.0, "max": 5.0, "step": 0.1, "label": "Amount"}

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec2 texel = 1.0 / iResolution.xy;
    vec4 originalColor = texture(iChannel0, uv);

    // Simplified box blur for the "unsharp" part
    vec4 blur = texture(iChannel0, uv + vec2( texel.x, 0.0)) +
                texture(iChannel0, uv + vec2(-texel.x, 0.0)) +
                texture(iChannel0, uv + vec2(0.0,  texel.y)) +
                texture(iChannel0, uv + vec2(0.0, -texel.y));
    blur *= 0.25;

    // Unsharp masking: sharpened = original + (original - blurred) * amount
    vec4 sharpened = originalColor + (originalColor - blur) * u_amount;

    FragColor = vec4(sharpened.rgb, originalColor.a);
}
