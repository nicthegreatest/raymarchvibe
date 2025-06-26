#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

// #control float u_scale "Scale" {"default":10.0, "min":1.0, "max":50.0}
uniform float u_scale = 10.0;

float rand(vec2 n) {
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float value_noise(vec2 p){
    vec2 ip = floor(p);
    vec2 u = fract(p);
    u = u*u*(3.0-2.0*u); // Smoothstep
    float res = mix(
        mix(rand(ip),rand(ip+vec2(1.0,0.0)),u.x),
        mix(rand(ip+vec2(0.0,1.0)),rand(ip+vec2(1.0,1.0)),u.x),u.y);
    return res*res; // Squaring to increase contrast
}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    float n = value_noise(uv * u_scale + iTime * 0.1); // Animate noise by adding time component
    FragColor = vec4(vec3(n), 1.0);
}
