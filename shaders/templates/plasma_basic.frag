#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

// Basic plasma effect
// Credits to Inigo Quilez for the general plasma concepts
// This is a highly simplified version

float plot(vec2 st, float pct){
  return  smoothstep( pct-0.02, pct, st.y) -
          smoothstep( pct, pct+0.02, st.y);
}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

    float timeScaled = iTime * 0.5;

    float color = 0.0;
    color += sin(uv.x * 5.0 + timeScaled * 2.0);
    color += sin(uv.y * 8.0 + timeScaled * 1.5);
    color += sin((uv.x + uv.y) * 10.0 + timeScaled * 3.0);
    color = sin( (uv.x*sin(timeScaled*0.2)+uv.y*cos(timeScaled*0.3)) * 10.0 + color * 2.0); // More complex interaction
    color = mod(color, 1.0); // Ensure it wraps around

    // Simple color mapping
    vec3 plasmaColor = vec3(
        sin(color * 3.14159 + 0.0 + timeScaled*0.1) * 0.5 + 0.5,
        sin(color * 3.14159 + 2.09439 + timeScaled*0.2) * 0.5 + 0.5, // 2PI/3
        sin(color * 3.14159 + 4.18879 + timeScaled*0.3) * 0.5 + 0.5  // 4PI/3
    );

    FragColor = vec4(plasmaColor, 1.0);
}
