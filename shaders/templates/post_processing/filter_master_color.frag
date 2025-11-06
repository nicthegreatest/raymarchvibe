#version 330 core
#define VIBRANCE_ENABLED

out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;

// Master Color Controls
uniform float u_brightness; // {"widget":"slider", "default": 1.0, "min": 0.0, "max": 2.0, "step": 0.01, "label": "Brightness"}
uniform float u_contrast;   // {"widget":"slider", "default": 1.0, "min": 0.0, "max": 3.0, "step": 0.01, "label": "Contrast"}
uniform float u_saturation; // {"widget":"slider", "default": 1.0, "min": 0.0, "max": 2.0, "step": 0.01, "label": "Saturation"}
uniform float u_hue;        // {"widget":"slider", "default": 0.0, "min": -1.0, "max": 1.0, "step": 0.01, "label": "Hue"}
uniform vec3 u_offset;      // {"widget":"color", "default": [0.0, 0.0, 0.0], "label": "Color Offset"}
uniform bool u_invert;      // {"widget":"checkbox", "default": false, "label": "Invert"}

#ifdef VIBRANCE_ENABLED
uniform float u_vibrance;   // {"widget":"slider", "default": 0.0, "min": -2.0, "max": 2.0, "step": 0.01, "label": "Vibrance"}
#endif

// Function to apply brightness
vec3 apply_brightness(vec3 color, float amount) {
    return color * amount;
}

// Function to apply contrast
vec3 apply_contrast(vec3 color, float amount) {
    return (color - 0.5) * amount + 0.5;
}

// Function to apply saturation
vec3 apply_saturation(vec3 color, float amount) {
    vec3 gray = vec3(dot(color, vec3(0.299, 0.587, 0.114)));
    return mix(gray, color, amount);
}

// Function to apply hue shift
vec3 apply_hue(vec3 color, float shift) {
    vec3 p = vec3(0.55735, 0.55735, 0.55735);
    vec3 q = vec3(0.7071067811865475, -0.7071067811865475, 0.0);
    float angle = shift * 3.14159265359;
    float s = sin(angle);
    float c = cos(angle);
    mat3 rotation_matrix = mat3(
        p.x * p.x + (1.0 - p.x * p.x) * c, p.x * p.y * (1.0 - c) - p.z * s, p.x * p.z * (1.0 - c) + p.y * s,
        p.x * p.y * (1.0 - c) + p.z * s, p.y * p.y + (1.0 - p.y * p.y) * c, p.y * p.z * (1.0 - c) - p.x * s,
        p.x * p.z * (1.0 - c) - p.y * s, p.y * p.z * (1.0 - c) + p.x * s, p.z * p.z + (1.0 - p.z * p.z) * c
    );
    return color * rotation_matrix;
}

// Function to apply vibrance
#ifdef VIBRANCE_ENABLED
vec3 apply_vibrance(vec3 color, float amount) {
    float avg = (color.r + color.g + color.b) / 3.0;
    float mx = max(color.r, max(color.g, color.b));
    float amt = (mx - avg) * (-amount * 3.0);
    return mix(color.rgb, vec3(mx), amt);
}
#endif

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec3 color = texture(iChannel0, uv).rgb;

    // Apply effects in order
    color = apply_brightness(color, u_brightness);
    color = apply_contrast(color, u_contrast);
    color = apply_saturation(color, u_saturation);
    color = apply_hue(color, u_hue);
    color += u_offset;

#ifdef VIBRANCE_ENABLED
    color = apply_vibrance(color, u_vibrance);
#endif

    if (u_invert) {
        color = 1.0 - color;
    }

    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
