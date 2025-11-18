// Palette Sync Demo - Modern Showcase for Advanced Color Palette System
// Demonstrates sync functionality with primary/secondary control roles

#version 330 core

out vec4 FragColor;
uniform vec2 iResolution;
uniform float iTime;

// SYNC PALETTE SYSTEM - SEMANTIC NAMING FOR AI-AGENTS
// =====================================================
// Primary Color (GENERATOR): Creates gradient, others sync from it
// Secondary, Accent, Highlight (CONSUMERS): Auto-sync at different positions

// PRIMARY CONTROL - Themed Generator
uniform vec3 PrimaryColor = vec3(1.0, 0.5, 0.0);    // {"widget":"color", "palette": true, "label":"Primary Color"}

// SECONDARY CONTROLS - Auto-sync consumers
uniform vec3 SecondaryColor = vec3(0.5, 1.0, 0.0);  // {"widget":"color", "palette": true, "label":"Secondary Color"}
uniform vec3 AccentColor = vec3(0.0, 0.5, 1.0);     // {"widget":"color", "palette": true, "label":"Accent Color"}
uniform vec3 HighlightColor = vec3(1.0, 0.0, 1.0);  // {"widget":"color", "palette": true, "label":"Highlight Color"}

// Animation controls
uniform float speed = 1.0;     // {"widget":"slider", "min":0.1, "max":3.0, "label":"Speed"}
uniform float intensity = 1.0; // {"widget":"slider", "min":0.1, "max":2.0, "label":"Intensity"}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

    // VISUAL SYNC DEMONSTRATION
    // ==========================
    // Top: Primary gradient cycling (creates theme)
    // Middle: Secondary controls synced (consumers)
    // Bottom: Manual override demonstration

    vec3 color = vec3(0.0);
    float t = iTime * speed;

    // REGION 1: Static Primary Color Display (Top 1/3)
    // Shows the exact primary color chosen by user - no animation
    if (uv.y > 0.666) {
        // Display the static primary color exactly as chosen by user
        color = PrimaryColor;

        // Add indicator stripes showing sync positions (no animation)
        if (mod(uv.x, 0.25) < 0.02 || mod(uv.x, 0.25) > 0.23) {
            color += vec3(0.1); // Static markers at 0.25, 0.5, 0.75 positions
        }
    }

    // REGION 2: Secondary Control Sync Display (Middle 1/3)
    // Shows what each synced control receives from primary gradient
    else if (uv.y > 0.333) {
        // Four vertical sections showing each control's synced color
        float section = floor(uv.x * 4.0) / 4.0;
        uv.x = fract(uv.x * 4.0);

        // Section markers
        if (uv.x < 0.05 || uv.x > 0.95) {
            color = vec3(0.9); // White section dividers
        } else {
            // Show sync colors with animated highlights
            if (section == 0.0) {
                color = SecondaryColor;
                if (mod(t, 4.0) < 1.0) color *= 1.3; // Pulse secondary
            } else if (section == 0.25) {
                color = mix(SecondaryColor, AccentColor, 0.5); // Transition
            } else if (section == 0.5) {
                color = AccentColor;
                if (mod(t + 1.0, 4.0) < 1.0) color *= 1.3; // Pulse accent
            } else {
                color = HighlightColor;
                if (mod(t + 2.0, 4.0) < 1.0) color *= 1.3; // Pulse highlight
            }
        }
    }

    // REGION 3: Interactive Demo (Bottom 1/3)
    // Shows manual control override vs sync
    else {
        // Moving pattern using all synced colors
        vec2 center = vec2(0.5 + sin(t) * 0.3, 0.5 + cos(t * 0.7) * 0.3);
        float dist = distance(uv, center);

        // Spiral pattern with synced colors
        float angle = atan(uv.y - center.y, uv.x - center.x);
        float spiral = sin(angle * 8.0 + dist * 20.0 - t * 2.0);

        if (spiral > 0.0) {
            color = SecondaryColor;  // Inner spiral sections
        } else {
            color = mix(AccentColor, HighlightColor, sin(dist * 10.0 + t));
        }

        // Add dynamic elements
        float pulse = sin(t * 4.0) * 0.5 + 0.5;
        color = mix(color, PrimaryColor, pulse * 0.2);

        // Distance-based color mixing
        color = mix(color, SecondaryColor, smoothstep(0.0, 0.3, dist));
    }

    // FINAL OUTPUT PROCESSING
    // Apply intensity and ensure nice visual range
    color *= intensity;
    color = clamp(color, 0.0, 1.0);

    FragColor = vec4(color, 1.0);
}
