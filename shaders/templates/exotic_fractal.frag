#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

// Helper function for HSV to RGB conversion
vec3 hsv(float h, float s, float v) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(vec3(h) + K.xyz) * 6.0 - K.www);
    return v * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), s);
}

// Helper function for 2D rotation
mat2 rotate2D(float angle) {
    return mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
}

void main() {
    vec2 r = iResolution;
    vec2 FC = gl_FragCoord.xy;
    float t = iTime;
    vec4 o = vec4(0.0);
    float i=0., g=0., e=0., s=0.;

    for(++i<139.;){
        vec3 p=vec3((FC.xy-.5*r)/r*.1+vec2(0,1.18),g-1.4);
        p.zx*=rotate2D(t*.3-9.);
        s=1.;
        // Inner loop variable changed to 'j' to avoid shadowing 'i' from the outer loop.
        for(int j=0; j < 26; j++){
            p=vec3(.33,4,-.64)-abs(abs(p)*e-vec3(2.7,3.95+e*.15,3.5-g));
            s*=e=5./dot(p,p*.5);
        }
        g+=p.y/s;
        s=log(s)*g+g;
        o.rgb+=.01-hsv(e/i-.38,p.y*.4,s/1e3);
    }
    
    FragColor = o;
}
