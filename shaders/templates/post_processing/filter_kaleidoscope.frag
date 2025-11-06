#version 330 core
out vec4 FragColor;

uniform sampler2D iChannel0;
uniform vec2 iResolution;
uniform float iTime;

uniform int u_complexity;   // {"widget":"slider", "default": 6, "min": 2, "max": 24, "step": 1, "label": "Complexity"}
uniform float u_angle;      // {"widget":"slider", "default": 0.0, "min": -3.14, "max": 3.14, "step": 0.01, "label": "Angle"}

void main()
{
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;

    float angle = atan(uv.y, uv.x) + u_angle;
    float dist = length(uv);

    float num_slices = float(u_complexity);
    float slice_angle = 3.14159265359 * 2.0 / num_slices;

    angle = mod(angle, slice_angle);
    angle = abs(angle - slice_angle / 2.0);

    uv.x = dist * cos(angle);
    uv.y = dist * sin(angle);

    FragColor = texture(iChannel0, uv + 0.5);
}
