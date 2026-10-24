#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_from;
uniform sampler2D u_to;
uniform float u_progress;
uniform vec2 u_resolution;

void main() {
    vec2 uv = TexCoords;
    if (u_resolution.x <= 0.0 || u_resolution.y <= 0.0)
        uv = clamp(TexCoords, 0.0, 1.0);
    float progress = clamp(u_progress, 0.0, 1.0);
    vec2 center = vec2(0.5);
    vec4 toColor = vec4(0.0);
    const int kSamples = 8;
    // The incoming picture settles from a short radial blur. Strength dies as the blend finishes.
    float strength = (1.0 - progress) * 0.18;
    for (int i = 0; i < kSamples; ++i) {
        float k = float(i) / float(kSamples - 1);
        vec2 sampleUv = clamp(uv + (center - uv) * strength * k, vec2(0.001), vec2(0.999));
        toColor += texture(u_to, sampleUv);
    }
    toColor /= float(kSamples);
    FragColor = mix(texture(u_from, uv), toColor, progress);
    FragColor.a = 1.0;
}
