// No #version or precision

uniform float iUserFloat1; 
uniform vec3 iUserColor1; 

uniform float u_rotationSpeed;    // {"label":"Rotation Speed", "min":-2.0, "max":2.0, "default":0.5, "step":0.05}
uniform float u_noLabelDefaultOnly; // {"label":"Test No Label", "min":0.0, "max":10.0, "default":5.0}

uniform float u_cubeSize;         // {"label":"Cube Size", "min":0.1, "max":1.5, "default":0.5, "step":0.01}
uniform vec3  u_cubeBaseColor;    // {"label":"Cube Base Color", "widget":"color", "default":[0.8, 0.1, 0.1]}
uniform float u_anotherVal;       // No metadata here - should not get a UI control
uniform float u_malformedMeta;    // {"label":"Malformed", "min":0 "max":1, "default":0.2} // Intentionally malformed JSON

uniform vec2 u_uvPatternOffset;   // {"label":"UV Pattern Offset", "min":-1.0, "max":1.0, "default":[0.0, 0.0], "step":0.01}
uniform vec3 u_ambientLightColor; // {"label":"Ambient Light", "widget":"color", "default":[0.1, 0.1, 0.15]}
uniform vec3 u_lightDirection;    // {"label":"Light Direction", "default":[-0.577, 0.577, 0.577]} 
uniform vec4 u_highlightColor;    // {"label":"Highlight RGBA", "widget":"color", "default":[1.0, 1.0, 0.8, 0.5]}

uniform int  u_patternStyle;      // {"label":"Pattern Style (Int)", "min":0, "max":3, "default":1}
uniform bool u_enableSpecular;    // {"label":"Enable Specular (Bool)", "default":true}

// ++ NEW PARSER EDGE CASE UNIFORMS ADDED BELOW ++
uniform float u_noSemicolonBefore; // {"label":"No Semicolon Before Comment"}
uniform float u_semicolonAfter;    // {"label":"Semicolon After Comment"} ; // Semicolon after metadata
// End of new uniforms for this step


void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    vec2 uv_base = fragCoord.xy / iResolution.xy; 
    vec2 uv = uv_base + u_uvPatternOffset; 

    float timeBasedAnimation = sin(iTime * u_rotationSpeed + u_noLabelDefaultOnly) * 0.5 + 0.5; 
    timeBasedAnimation *= iUserFloat1; 

    vec3 finalColor = u_ambientLightColor; 

    float pattern = 0.0;
    if (u_patternStyle == 0) { 
        pattern = mod(floor(uv.x * 10.0) + floor(uv.y * 10.0), 2.0); 
    } else if (u_patternStyle == 1) { 
        pattern = mod(floor(uv.x * 20.0), 2.0); 
    } else if (u_patternStyle == 2) {
        pattern = step(0.8, sin(uv.x * 30.0) * sin(uv.y * 30.0)); 
    } else {
        pattern = 0.5; 
    }
    
    finalColor = mix(finalColor, u_cubeBaseColor, pattern); 
    finalColor += iUserColor1 * 0.2; 
    finalColor.rg += timeBasedAnimation * u_cubeSize * 0.2; 
    
    vec3 pseudoNormal = normalize(vec3(uv_base - 0.5, 0.5)); 
    float diffuse = max(0.0, dot(pseudoNormal, normalize(u_lightDirection)));
    finalColor *= (0.5 + 0.5 * diffuse);

    if (u_enableSpecular) {
        finalColor += vec3(0.2 * diffuse); 
    }

    finalColor = mix(finalColor, u_highlightColor.rgb, u_highlightColor.a * 0.5);
    finalColor.b += u_anotherVal * 0.001; // u_anotherVal is declared, so it's fine to use here. No UI for it.

    // ++ TEST THE NEWLY ADDED PARSER EDGE CASE UNIFORMS ++
    // This one SHOULD have been parsed and have a UI control:
    finalColor.r += u_semicolonAfter * 0.01; 

    // This one (u_noSemicolonBefore) should NOT have been parsed by C++ and thus should be undeclared in GLSL.
    // Uncommenting the next line should cause a GLSL compile error if the C++ parser behaved as expected.
    // finalColor.g += u_noSemicolonBefore * 0.01; 

    // Similarly, u_malformedMeta should have failed JSON parsing and not be an active GLSL uniform.
    // Uncommenting this would also likely cause a GLSL compile error.
    // finalColor.b += u_malformedMeta * 0.01;

    fragColor = vec4(finalColor, 1.0);
}
