#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

// @control float _Exposure "Exposure" "min=-2.0 max=2.0 step=0.05"
uniform float _Exposure = 0.0;
// @control float _Contrast "Contrast" "min=0.0 max=2.0 step=0.05"
uniform float _Contrast = 1.0;
// @control float _Saturation "Saturation" "min=0.0 max=2.0 step=0.05"
uniform float _Saturation = 1.0;

vec3 change_saturation(vec3 color, float saturation) {
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    return mix(vec3(luma), color, saturation);
}

void main()
{
    vec4 color = texture(screenTexture, TexCoords);

    // Apply exposure
    vec3 final_color = color.rgb * pow(2.0, _Exposure);

    // Apply contrast
    final_color = mix(vec3(0.5), final_color, _Contrast);

    // Apply saturation
    final_color = change_saturation(final_color, _Saturation);

    FragColor = vec4(final_color, color.a);
}
