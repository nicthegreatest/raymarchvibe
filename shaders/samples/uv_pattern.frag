void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord.xy / iResolution.xy;

    // Create a wavy, animated effect on the UV coordinates
    vec2 wavy_uv = uv;
    wavy_uv.x += sin(uv.y * 15.0 + iTime * 2.0) * 0.05;
    wavy_uv.y += cos(uv.x * 12.0 + iTime * 1.5) * 0.05;

    // Generate a color pattern based on the distorted UVs and time
    vec3 col = 0.5 + 0.5 * cos(iTime * 1.0 + wavy_uv.xyx + vec3(0, 2, 4));

    // Output to screen
    fragColor = vec4(col, 1.0);
}