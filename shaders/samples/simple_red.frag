// Simple Red Sample for Shadertoy Mode

// NO #version directive here
// NO out vec4 FragColor; here
// NO uniform declarations for iTime, iResolution, etc. if provided by wrapper

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    // vec2 uv = fragCoord/iResolution.xy; // iResolution is provided by the wrapper
    fragColor = vec4(1.0, 0.0, 0.0, 1.0); // Solid red
}
