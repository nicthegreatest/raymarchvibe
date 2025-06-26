#version 330 core
out vec4 FragColor;

uniform vec3 u_color = vec3(1.0, 0.7, 0.2); // Default to an orange color
// #control vec3 u_color "Color" { "default": [1.0, 0.7, 0.2], "min": [0.0,0.0,0.0], "max": [1.0,1.0,1.0], "colortype": "rgb" }

void main() {
    FragColor = vec4(u_color, 1.0);
}
