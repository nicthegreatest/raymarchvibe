#version 330 core
out vec4 FragColor;

in vec2 TexCoords; // Assuming this comes from a standard fullscreen vert shader

uniform sampler2D iChannel0; // Texture from the previous effect in the chain (RENAMED from u_inputTexture)
uniform float iTime; // Just to make it do something if no input
uniform vec3 iResolution; // Added to silence warning & for fallback pattern

void main() {
    vec4 inputColor = texture(iChannel0, TexCoords); // Use iChannel0

    // Simple effect: desaturate or tint the input
    // float grayscale = dot(inputColor.rgb, vec3(0.299, 0.587, 0.114));
    // FragColor = vec4(vec3(grayscale), inputColor.a); // Grayscale
    FragColor = vec4(inputColor.r * 0.5, inputColor.g, inputColor.b * 0.8, inputColor.a); // Tint

    // Fallback diagnostic colors removed. iResolution can also be removed if not used by tint.
    // For now, keeping iResolution declaration in case it's used by other effects or future passthrough versions.
}
