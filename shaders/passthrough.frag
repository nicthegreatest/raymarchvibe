#version 330 core
out vec4 FragColor;

in vec2 TexCoords; // Assuming this comes from a standard fullscreen vert shader

uniform sampler2D iChannel0; // Texture from the previous effect in the chain (RENAMED from u_inputTexture)
uniform float iTime; // Just to make it do something if no input

void main() {
    vec4 inputColor = texture(iChannel0, TexCoords); // Use iChannel0

    // If no input texture is properly bound, texture() might return black or undefined.
    // Let's add a fallback or a slight modification to see if the shader is working.
    if (inputColor.a == 0.0 && inputColor.r == 0.0 && inputColor.g == 0.0 && inputColor.b == 0.0) {
        // If input is black/transparent (possibly unbound), draw a time-based pattern
        vec2 uv = TexCoords * 2.0 - 1.0; // -1 to 1
        float color = 0.5 + 0.5 * sin(uv.x * 10.0 + iTime);
        FragColor = vec4(color, uv.y, 0.5, 1.0);
    } else {
        // Simple effect: desaturate or tint the input
        float grayscale = dot(inputColor.rgb, vec3(0.299, 0.587, 0.114));
        // FragColor = vec4(vec3(grayscale), inputColor.a); // Grayscale
        FragColor = vec4(inputColor.r * 0.5, inputColor.g, inputColor.b * 0.8, inputColor.a); // Tint
    }
}
