#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;
uniform float iTime;

uniform vec2 u_linear_velocity; // {"widget":"slider", "default": [0.0, 0.0], "min": [-0.1, -0.1], "max": [0.1, 0.1], "step": 0.001, "label": "Linear Velocity"}
uniform float u_twist_amount;    // {"widget":"slider", "default": 0.0, "min": -3.14, "max": 3.14, "step": 0.01, "label": "Twist Amount"}
uniform float u_warp_amount;     // {"widget":"slider", "default": 0.0, "min": 0.0, "max": 0.1, "step": 0.001, "label": "Warp Amount"}

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec2 center = vec2(0.5, 0.5);
    vec2 tex_coord = uv;

    // 1. Linear Movement
    tex_coord += u_linear_velocity * iTime;

    // 2. Twist
    vec2 to_center = tex_coord - center;
    float angle = atan(to_center.y, to_center.x);
    float dist = length(to_center);
    angle += u_twist_amount;
    tex_coord.x = center.x + dist * cos(angle);
    tex_coord.y = center.y + dist * sin(angle);

    // 3. Warp (simple sine wave distortion)
    tex_coord.x += sin(tex_coord.y * 10.0 + iTime * 2.0) * u_warp_amount;
    tex_coord.y += cos(tex_coord.x * 10.0 + iTime * 2.0) * u_warp_amount;

    FragColor = texture(iChannel0, tex_coord);
}
