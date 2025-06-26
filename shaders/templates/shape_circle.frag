#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;

// #control vec2 u_center "Center" {"default":[0.5,0.5], "min":[0.0,0.0], "max":[1.0,1.0]}
// #control float u_radius "Radius" {"default":0.25, "min":0.01, "max":1.0}
// #control vec3 u_shapeColor "Shape Color" {"colortype":"rgb", "default":[1.0,1.0,1.0]}
// #control vec3 u_bgColor "Background Color" {"colortype":"rgb", "default":[0.0,0.0,0.0]}
// #control float u_smoothness "Smoothness" {"default":0.01, "min":0.001, "max":0.1}

uniform vec2 u_center = vec2(0.5, 0.5);
uniform float u_radius = 0.25;
uniform vec3 u_shapeColor = vec3(1.0, 1.0, 1.0);
uniform vec3 u_bgColor = vec3(0.0, 0.0, 0.0);
uniform float u_smoothness = 0.01;

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    float dist = distance(uv, u_center);
    float circle = smoothstep(u_radius, u_radius - u_smoothness, dist);
    FragColor = vec4(mix(u_bgColor, u_shapeColor, circle), 1.0);
}
