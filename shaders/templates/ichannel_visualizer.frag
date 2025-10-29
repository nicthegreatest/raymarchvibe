// A utility shader to visualize the inputs of all four iChannels.

// This is automatically provided by the app's Shadertoy wrapper:
// uniform vec3 iResolution;
// uniform float iTime;
// uniform vec4 iMouse;
// uniform sampler2D iChannel0;
// uniform sampler2D iChannel1;
// uniform sampler2D iChannel2;
// uniform sampler2D iChannel3;

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy/iResolution.xy;

    // Use mouse X position to select which channel to display
    float selector = iMouse.x / iResolution.x;

    // If mouse is not on screen, animate the selector over time
    if (iMouse.x <= 0.0) { 
        selector = fract(iTime * 0.2);
    }

    // Display the texture from the selected channel
    if (selector < 0.25) {
        fragColor = texture(iChannel0, uv);
    } else if (selector < 0.5) {
        fragColor = texture(iChannel1, uv);
    } else if (selector < 0.75) {
        fragColor = texture(iChannel2, uv);
    } else {
        fragColor = texture(iChannel3, uv);
    }

    // Add a visual indicator bar at the bottom of the screen
    if (uv.y < 0.05) {
        if (selector < 0.25) {
            fragColor.rgb = vec3(1.0, 0.0, 0.0); // Red for iChannel0
        } else if (selector < 0.5) {
            fragColor.rgb = vec3(0.0, 1.0, 0.0); // Green for iChannel1
        } else if (selector < 0.75) {
            fragColor.rgb = vec3(0.0, 0.0, 1.0); // Blue for iChannel2
        } else {
            fragColor.rgb = vec3(1.0, 1.0, 0.0); // Yellow for iChannel3
        }
    }
}
