#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D iChannel0;

// #control float u_brightness "Brightness" {"default":0.0, "min":-1.0, "max":1.0}
// #control float u_contrast "Contrast" {"default":1.0, "min":0.0, "max":3.0}
uniform float u_brightness = 0.0;
uniform float u_contrast = 1.0;

void main() {
    vec4 color = texture(iChannel0, TexCoords);

    // Apply brightness
    color.rgb += u_brightness;

    // Apply contrast
    color.rgb = (color.rgb - 0.5) * u_contrast + 0.5;

    FragColor = clamp(color, 0.0, 1.0); // Clamp to valid color range
}
