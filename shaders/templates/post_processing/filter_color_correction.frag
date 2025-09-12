#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

uniform float u_exposure;    // {"default": 0.0, "min": -2.0, "max": 2.0, "step": 0.05, "label": "Exposure"}
uniform float u_contrast;    // {"default": 1.0, "min": 0.0, "max": 2.0, "step": 0.05, "label": "Contrast"}
uniform float u_saturation;  // {"default": 1.0, "min": 0.0, "max": 2.0, "step": 0.05, "label": "Saturation"}
uniform vec3  u_tint;        // {"default": [1.0, 1.0, 1.0], "widget": "color", "label": "Tint"}

vec3 change_saturation(vec3 color, float saturation) {
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(luma), color, saturation);
}

void main()
{
    vec4 color = texture(screenTexture, TexCoords);

    // Apply exposure
    vec3 final_color = color.rgb * pow(2.0, u_exposure);

    // Apply contrast
    final_color = mix(vec3(0.5), final_color, u_contrast);

    // Apply saturation
    final_color = change_saturation(final_color, u_saturation);

    // Apply tint
    final_color *= u_tint;

    FragColor = vec4(final_color, color.a);
}
