#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;

uniform int u_mode; // {"widget":"slider", "default": 0, "min": 0, "max": 2, "step": 1, "label": "Mode (0=ACES, 1=Reinhard, 2=Filmic)"}

// ACES tone mapping (approximate)
vec3 aces(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard tone mapping
vec3 reinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

// Filmic tone mapping (Uncharted 2)
vec3 uncharted2_tonemap_partial(vec3 x) {
    const float A = 0.15; // Shoulder Strength
    const float B = 0.50; // Linear Strength
    const float C = 0.10; // Linear Angle
    const float D = 0.20; // Toe Strength
    const float E = 0.02; // Toe Numerator
    const float F = 0.30; // Toe Denominator
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}
vec3 filmic(vec3 color) {
    const float W = 11.2; // Linear White Point Value
    vec3 curr = uncharted2_tonemap_partial(color * 2.0); // Exposure bias
    vec3 white_scale = 1.0 / uncharted2_tonemap_partial(vec3(W));
    return curr * white_scale;
}


void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec4 color = texture(iChannel0, uv);
    vec3 final_color = color.rgb;

    if (u_mode == 0) { // ACES
        final_color = aces(color.rgb);
    } else if (u_mode == 1) { // Reinhard
        final_color = reinhard(color.rgb);
    } else { // Filmic
        final_color = filmic(color.rgb);
    }

    FragColor = vec4(final_color, color.a);
}
