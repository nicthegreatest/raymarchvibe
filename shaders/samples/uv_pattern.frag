uniform float iUserFloat1; 
uniform vec3 iUserColor1; 

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord.xy / iResolution.xy;
    
    // Now USE the uniforms:
    float brightness = iUserFloat1; 
    vec3 baseCol = iUserColor1;

    vec3 col = baseCol * (0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4)));
    col *= brightness;

    fragColor = vec4(col,1.0);
}
