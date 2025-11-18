// Palette Demo Shader - Demonstrates color palette generation and harmonies
// This shader shows how to use palette-enabled colors for artistic effects

#version 330 core

out vec4 FragColor;
uniform vec2 iResolution;
uniform float iTime;

// Palette colors with palette support
// Use {"widget":"color", "palette": true} to enable palette mode
uniform vec3 paletteColor1 = vec3(1.0, 0.0, 0.0);     // {"widget":"color", "palette": true, "label":"Primary Color"}
uniform vec3 paletteColor2 = vec3(0.0, 1.0, 0.0);     // {"widget":"color", "palette": true, "label":"Secondary Color"}
uniform vec3 paletteColor3 = vec3(0.0, 0.0, 1.0);     // {"widget":"color", "palette": true, "label":"Tertiary Color"}
uniform vec3 paletteColor4 = vec3(1.0, 1.0, 0.0);     // {"widget":"color", "palette": true, "label":"Accent Color"}
uniform vec3 paletteColor5 = vec3(1.0, 0.0, 1.0);     // {"widget":"color", "palette": true, "label":"Highlight Color"}

// Animation controls
uniform float speed = 1.0;      // {"widget":"slider", "min":0.1, "max":5.0, "label":"Animation Speed"}
uniform float intensity = 1.0;   // {"widget":"slider", "min":0.0, "max":2.0, "label":"Intensity"}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    
    // Create different regions using palette colors
    vec3 color = vec3(0.0);
    
    // Time-based animation
    float t = iTime * speed;
    
    // Region 1: Horizontal stripes with primary color
    if (mod(uv.y + sin(t) * 0.1, 0.3) < 0.15) {
        color = paletteColor1;
    }
    // Region 2: Vertical stripes with secondary color
    else if (mod(uv.x + cos(t * 0.7) * 0.1, 0.3) < 0.15) {
        color = paletteColor2;
    }
    // Region 3: Diagonal pattern with tertiary color
    else if (mod(uv.x + uv.y + sin(t * 0.5) * 0.1, 0.3) < 0.15) {
        color = paletteColor3;
    }
    // Region 4: Circular pattern with accent color
    else {
        float dist = distance(uv, vec2(0.5 + sin(t * 0.3) * 0.2, 0.5 + cos(t * 0.2) * 0.2));
        color = mix(paletteColor4, paletteColor5, step(0.3, dist));
    }
    
    // Add some pulsing effect using all palette colors
    float pulse = sin(t * 2.0) * 0.5 + 0.5;
    color = mix(color, 
                mix(paletteColor1, paletteColor2, pulse),
                0.1);
    
    // Apply intensity and output
    color *= intensity;
    FragColor = vec4(color, 1.0);
}
