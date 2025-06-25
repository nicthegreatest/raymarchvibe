#version 330 core
out vec4 FragColor;

in vec2 TexCoords; // Assuming this comes from a standard fullscreen vert shader

uniform sampler2D iChannel0; // Texture from the previous effect in the chain (RENAMED from u_inputTexture)
uniform float iTime; // Just to make it do something if no input
uniform vec3 iResolution; // Added to silence warning & for fallback pattern

void main() {
    vec4 inputColor = texture(iChannel0, TexCoords); // Use iChannel0

    // If no input texture is properly bound, texture() might return black or undefined.
    // Let's add a fallback or a slight modification to see if the shader is working.
    // Check if input is effectively black (very dark) or fully transparent
    bool isBlack = (inputColor.r < 0.01 && inputColor.g < 0.01 && inputColor.b < 0.01);
    bool isTransparent = inputColor.a < 0.01;

    if (isBlack && isTransparent) { // If input is black AND transparent (typical for unbound/default texture)
        FragColor = vec4(1.0, 0.0, 1.0, 1.0); // Bright Magenta for unbound/empty input
    } else if (isBlack) { // If input is black but opaque (Plasma might be outputting black)
        FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Bright Red for black input
    }
    else {
        // Simple effect: desaturate or tint the input
        // float grayscale = dot(inputColor.rgb, vec3(0.299, 0.587, 0.114));
        // FragColor = vec4(vec3(grayscale), inputColor.a); // Grayscale
        FragColor = vec4(inputColor.r * 0.5, inputColor.g, inputColor.b * 0.8, inputColor.a); // Tint
    }
}
