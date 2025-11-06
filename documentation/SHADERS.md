# RaymarchVibe Shader Specification (Single Source of Truth)

This document is the **authoritative specification** for all RaymarchVibe shaders and the Milk-Converter application. This replaces all other shader documentation and provides a comprehensive guide to creating and converting shaders.

**IMPORTANT:** This specification supercedes all other documentation. Agents and developers must reference this document for accurate shader standards.

## 1. GLSL Version Requirement

**All shaders MUST use GLSL version 3.30 core profile.** The first line of every shader must be:

```glsl
#version 330 core
```

## 2. Fragment Shader Requirements

### 2.1 Output Declaration
Shaders must declare a fragment output variable named `FragColor`:

```glsl
out vec4 FragColor;
```

### 2.2 Main Function
Shaders must include a `main()` function that assigns to `FragColor`:

```glsl
void main() {
    // Calculate UV coordinates from screen position
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

    // ... shader logic ...

    FragColor = finalColor;
}
```

**NOTE:** Shaders should calculate UV coordinates from `gl_FragCoord.xy / iResolution.xy`, not using an `in vec2 uv` attribute. This provides proper 0-1 normalized coordinates.

### 2.3 `main()` vs. `mainImage()` and "Shadertoy Mode"

**CRITICAL:** The application's behavior changes based on the name of your main rendering function. This is the most common source of `GL_ERROR: INVALID_OPERATION` and other hard-to-debug issues.

*   **Use `void main()` for Standard RaymarchVibe Shaders:**
    *   This is the **recommended and default** method.
    *   The application will provide uniforms like `iResolution` as a `vec2`.
    *   All rendering logic should be placed directly inside `void main()`.

    ```glsl
    // Correct structure for a standard shader
    #version 330 core

    out vec4 FragColor;

    // Standard uniforms
    uniform vec2 iResolution;
    uniform float iTime;
    // ... other uniforms

    void main() {
        vec2 uv = gl_FragCoord.xy / iResolution.xy;
        // ... your logic ...
        FragColor = vec4(uv, 0.5, 1.0);
    }
    ```

*   **Use `void mainImage(out vec4, in vec2)` for Shadertoy Compatibility:**
    *   If your shader contains a function with the signature `void mainImage(...)`, the application will automatically enter **"Shadertoy Mode"**.
    *   In this mode, the application assumes you are writing a shader compatible with the Shadertoy website.
    *   **Crucially, `iResolution` will be provided as a `vec3` (`x`, `y`, `aspect_ratio`), not a `vec2`.** Declaring it as `vec2` in your shader while in this mode will cause a uniform mismatch and `GL_ERROR`.
    *   The application will also automatically provide other Shadertoy-specific uniforms like `iMouse`, `iFrame`, etc.

    ```glsl
    // Correct structure for a Shadertoy-compatible shader
    #version 330 core

    // Note: iResolution is vec3 in Shadertoy mode!
    uniform vec3 iResolution;
    uniform float iTime;
    // ... other uniforms

    // Shadertoy entry point
    void mainImage(out vec4 fragColor, in vec2 fragCoord) {
        vec2 uv = fragCoord.xy / iResolution.xy;
        // ... your logic ...
        fragColor = vec4(uv, 0.5, 1.0);
    }

    // You must still provide a main() function to call mainImage
    void main() {
        mainImage(FragColor, gl_FragCoord.xy);
    }
    ```

**To avoid issues, follow this simple rule:** Unless you are specifically writing a shader for Shadertoy compatibility, **do not** name any function `mainImage`. Place all your code in `main()`. This will ensure your shader works correctly within the standard RaymarchVibe environment.

## 3. Mandatory Built-in Uniforms

These uniforms are automatically provided by RaymarchVibe's ShaderEffect class and must be referenced exactly as documented:

| Name | Type | Description | Example Usage |
|---|---|---|---|
| `iTime` | `float` | Current application time in seconds | `sin(iTime * 2.0)` |
| `iResolution` | `vec2` | Viewport resolution (width, height) in pixels | `iResolution.x` for aspect ratios |
| `iFps` | `float` | Current framerate | `smoothstep(30.0, 60.0, iFps)` |
| `iFrame` | `float` | Frame counter | `float currentFrame = iFrame;` |
| `iProgress` | `float` | Application progress (0.0-1.0) | For transitions |
| `iAudioBands` | `vec4` | Audio frequency bands (x=bass, y=mids, z=treble, w=all) | `iAudioBands.x * 2.0` |
| `iAudioBandsAtt` | `vec4` | Audio frequency bands with attack (smoothed) | `iAudioBandsAtt.y` |
| `iChannel0` | `sampler2D` | Previous frame/feedback buffer | `texture(iChannel0, uv)` |
| `iChannel1` | `sampler2D` | Additional texture input | `texture(iChannel1, uv)` |
| `iChannel2` | `sampler2D` | Additional texture input | `texture(iChannel2, uv)` |
| `iChannel3` | `sampler2D` | Additional texture input | `texture(iChannel3, uv)` |

## 4. UI Controls Specification

### 4.1 Syntax
Uniforms control UI generation via JSON comments. The syntax is:

```glsl
uniform <type> <name> = <default_value>; // {"widget":"<type>", "min":<min>, "max":<max>, "step":<step>}
```

### 4.2 Supported Widget Types

#### Slider (`"widget":"slider"`)
- **Required parameters:** `min`, `max`
- **Optional parameters:** `step` (default: 0.01)
- **Applicable types:** `float`, `int`

```glsl
uniform float u_speed = 1.0; // {"widget":"slider", "min":0.0, "max":10.0, "step":0.1}
uniform int u_count = 5; // {"widget":"slider", "min":1, "max":20}
```

#### Color Picker (`"widget":"color"`)
- **Applicable types:** `vec3`, `vec4`

#### Smoothing (`"smooth":true`)
- **Optional parameter:** `smooth` (default: `false`)
- **Applicable types:** `float`
- When set to `true`, the uniform's value will smoothly interpolate towards the target value over time, rather than snapping immediately. This creates a more organic and less jarring visual transition.

```glsl
uniform vec3 u_color = vec3(1.0, 0.0, 0.0); // {"widget":"color"}
uniform vec4 u_color_alpha = vec4(1.0, 0.0, 0.0, 1.0); // {"widget":"color"}
```

## 5. Node-Based Architecture Constraints

### 5.1 Feedback Loop Handling
Shaders must properly handle the feedback loop through `iChannel0`:

```glsl
void main() {
    // Sample previous frame
    vec4 feedback = texture(iChannel0, uv);

    // Apply temporal effects (decay, blending)
    vec4 newContent = computeContent(uv);

    // Combine feedback with new content
    FragColor = mix(feedback * 0.9, newContent, 0.1);
}
```

### 5.2 Input Pin Conventions
Shaders may receive inputs from other nodes via `iChannel1-3` uniforms. Each input pin should be documented in the shader comments.

### 5.3 Metalness/Roughness Outputs (Future)
Advanced shaders should output material properties as `.a` channel:
- **RGB:** Final color
- **A:** Material properties (metalness, roughness, etc.)

## 6. Milk-Converter Translation Standards

### 6.1 Variable Mapping
The Milk-Converter must map MilkDrop variables to RaymarchVibe uniforms:

| MilkDrop Variable | RaymarchVibe Uniform | Notes |
|---|---|---|
| `time` | `iTime` | Direct mapping |
| `fps` | `iFps` | Direct mapping |
| `frame` | `iFrame` | Direct mapping |
| `bass` | `iAudioBands.x` | Audio frequency band |
| `mid` | `iAudioBands.y` | Audio frequency band |
| `treb` | `iAudioBands.z` | Audio frequency band |
| `bass_att` | `iAudioBandsAtt.x` | Attack-smooth audio |
| `mid_att` | `iAudioBandsAtt.y` | Attack-smooth audio |
| `treb_att` | `iAudioBandsAtt.z` | Attack-smooth audio |
| `rad` | `length(uv - vec2(0.5))` | Computed in fragment shader |
| `ang` | `atan(uv.y - 0.5, uv.x - 0.5)` | Computed in fragment shader |

### 6.2 UI Control Generation
Milk-Converter must generate accurate UI controls based on `.milk` file defaults:

```glsl
// Convert MilkDrop q1-q32 variables with defaults
uniform float u_q1 = 1.0; // {"widget":"slider", "min":0.0, "max":1.0, "step":0.01}
```

## 7. Best Practices

### 7.1 Shader Organization
1. **Version directive first:** `#version 330 core`
2. **Output declaration:** `out vec4 FragColor;`
3. **Built-in uniforms:** Declare all required RaymarchVibe uniforms first
4. **UI control uniforms:** Declare custom uniforms for parameters
5. **Helper functions:** Define utility functions before main
6. **Main function:** Calculate UV, implement shader logic, assign FragColor

### 7.2 Performance Considerations
1. **Avoid expensive operations** in hot paths
2. **Use appropriate precision** (mediump for most cases)
3. **Minimize per-pixel computations** that can be precomputed
4. **Profile shader performance** using RaymarchVibe's built-in metrics

### 7.3 Cross-Platform Compatibility
1. **Test on different hardware** (desktop, laptop GPUs)
2. **Use GLSL 3.30 features only** (no extensions)
3. **Avoid vendor-specific optimizations**

### 7.4 Handling `iChannel` Uniforms
The `GL_INVALID_OPERATION` error can occur if `iChannel` uniforms (e.g., `iChannel0`, `iChannel1`, etc.) are declared in the shader but are not properly handled in the C++ `ShaderEffect` class. This typically happens when:
- A shader declares an `iChannelX` uniform, but the corresponding texture unit is not activated or a valid texture is not bound to it.
- The `ShaderEffect`'s rendering logic attempts to bind textures for all four `iChannel`s, even if the shader only uses a subset of them.

**Best Practice:**
If your shader declares `iChannelX` uniforms, ensure that the C++ `ShaderEffect` class correctly initializes the uniform locations (e.g., to `-1` if the uniform is not found in the shader) and that a valid texture (even a dummy 1x1 black texture) is bound to the corresponding texture unit (`GL_TEXTURE0 + X`) before rendering. This prevents OpenGL from encountering an invalid state.

## 8. Testing and Validation

### 8.1 Shader Verification
Shaders should be tested for:
1. **Compilation success** in ShaderEffect
2. **UI control generation** in ShaderParser
3. **Feedback loop stability** (no infinite feedback)
4. **Performance benchmark** (<10ms per effect)

### 8.2 Milk-Converter Validation
Converted shaders should reproduce original MilkDrop preset behavior including:
1. **Audio reactivity** matches original
2. **Beat detection** works correctly
3. **UI controls** expose appropriate parameters
4. **Visual fidelity** matches MilkDrop rendering

### 9. Advanced Shader Architecture & Professional Techniques

This section bridges the gap between basic shaders and production-quality visuals. The inspiration shaders demonstrate techniques that create stunning, professional results. Master these patterns to elevate your work beyond simple effects to immersive visual experiences.

### 9.1 Mathematical Foundations for Complex Scenes

#### 9.1.1 Advanced SDF Operations
Move beyond basic primitives to create complex, organic structures:

```glsl
// Gyroid lattice - creates organic, flowing structures
float gyroid(vec3 p) {
    return dot(cos(p*1.5707963), sin(p.yzx*1.5707963));
}

// Smooth minimum with controlled blending
float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

// Space warping for organic deformation
vec3 warpSpace(vec3 p, float time) {
    p += cos(p.zxy*1.5707963)*0.2; // Subtle organic mutation
    p.xy += sin(time + p.z*0.1) * 0.1; // Flowing distortion
    return p;
}

// Domain repetition for infinite patterns
float opRepetition(vec3 p, vec3 c) {
    vec3 q = mod(p, c) - 0.5*c;
    return map(q); // Your base SDF here
}
```

#### 9.1.2 Lattice Structures & Architectural Patterns
Create complex geometric frameworks that define scene architecture:

```glsl
// Body-centered cubic lattice (from inspiration shaders)
float bccLattice(vec3 p) {
    p = abs(p - (p.x + p.y + p.z)*0.3333);
    return (p.x + p.y + p.z)*0.5 - thickness;
}

// Octahedral joins for lattice connections
float octahedralJoin(vec3 p) {
    p = abs(fract(p + vec3(0.5)) - 0.5);
    return (p.x + p.y + p.z) - size;
}

// Combined lattice structure
float complexLattice(vec3 p) {
    float lattice = bccLattice(p);
    float joins = octahedralJoin(p);
    return min(lattice, joins);
}
```

### 9.2 Professional Color Theory & Palette Design

#### 9.2.1 HSV-Based Color Systems
Work in HSV for more intuitive color control, then convert to RGB:

```glsl
// Efficient HSV to RGB conversion
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Sophisticated palette generation
vec3 generatePalette(float t, vec3 baseHue) {
    // Analogous harmony with temperature variation
    vec3 color1 = hsv2rgb(baseHue + vec3(0.0, 0.8, 0.9));
    vec3 color2 = hsv2rgb(baseHue + vec3(0.1, 0.7, 0.8));
    vec3 color3 = hsv2rgb(baseHue + vec3(-0.05, 0.6, 0.7));
    
    return mix(mix(color1, color2, t), color3, t*t);
}

// Temperature-based color mapping
vec3 temperatureColor(float temp) {
    // temp: 0.0 = cool (blue), 1.0 = warm (orange)
    vec3 cool = hsv2rgb(vec3(0.6, 0.8, 0.9));  // Blue-cyan
    vec3 warm = hsv2rgb(vec3(0.08, 0.9, 1.0)); // Orange-yellow
    return mix(cool, warm, temp);
}
```

#### 9.2.2 Cinematic Color Grading
Apply professional color grading techniques:

```glsl
// ACES tone mapping for filmic look
vec3 acesFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// Lift-Gamma-Gain color correction
vec3 liftGammaGain(vec3 color, vec3 lift, vec3 gamma, vec3 gain) {
    color = pow(color, 1.0/gamma);
    color = color * gain;
    color = color + lift;
    return color;
}

// Film grain for organic texture
vec3 filmGrain(vec2 uv, float time) {
    float grain = fract(sin(dot(uv + time, vec2(12.9898, 78.233))) * 43758.5453);
    return vec3(grain * 0.02 - 0.01); // Subtle grain
}
```

### 9.3 Advanced Lighting & Material Systems

#### 9.3.1 Multi-Light Cinematic Setup
Create professional lighting with multiple sources:

```glsl
// Multiple light sources with different colors
struct Light {
    vec3 position;
    vec3 color;
    float intensity;
    float radius; // For soft shadows
};

vec3 computeLighting(vec3 pos, vec3 normal, vec3 viewDir, vec3 baseColor) {
    Light lights[3];
    lights[0] = Light(vec3(4.0, 3.0, 1.5), vec3(1.0, 0.2, 0.8), 1.0, 0.1);
    lights[1] = Light(vec3(-2.0, -3.0, -4.0), vec3(0.0, 0.8, 1.0), 0.7, 0.15);
    lights[2] = Light(vec3(0.0, 10.0, 0.0), vec3(1.0, 1.0, 0.95), 0.3, 0.5);
    
    vec3 totalLighting = vec3(0.0);
    
    for(int i = 0; i < 3; i++) {
        vec3 lightDir = normalize(lights[i].position - pos);
        float distance = length(lights[i].position - pos);
        float attenuation = 1.0 / (1.0 + 0.1*distance + 0.01*distance*distance);
        
        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        
        // Specular with proper energy conservation
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
        
        totalLighting += (diff * baseColor + spec * 0.5) * lights[i].color * 
                        lights[i].intensity * attenuation;
    }
    
    return totalLighting;
}
```

#### 9.3.2 Advanced Material Effects
Create sophisticated surface properties:

```glsl
// Iridescence effect
vec3 iridescence(vec3 normal, vec3 viewDir, float intensity) {
    float fresnel = pow(1.0 - dot(normal, viewDir), 2.0);
    vec3 iridColor = hsv2rgb(vec3(fresnel * 2.0, 0.8, 1.0));
    return iridColor * intensity * fresnel;
}

// Subsurface scattering simulation
vec3 subsurfaceScattering(vec3 lightDir, vec3 normal, vec3 viewDir, float thickness) {
    float scattering = pow(max(dot(-viewDir, lightDir) + thickness, 0.0), 2.0);
    return vec3(scattering * 0.5); // Warm glow
}

// Atmospheric fog with height variation
vec3 atmosphericFog(vec3 color, float distance, float height) {
    float fogAmount = 1.0 - exp(-distance * 0.02);
    float heightFog = exp(-height * 0.1);
    vec3 fogColor = mix(vec3(0.6, 0.8, 1.0), vec3(1.0, 0.9, 0.7), heightFog);
    return mix(color, fogColor, fogAmount);
}
```

### 9.4 Organic Motion & Temporal Effects

#### 9.4.1 Layered Animation Systems
Create complex, natural motion patterns:

```glsl
// Multi-frequency oscillator for organic movement
float organicOscillator(float time, vec2 p) {
    float osc1 = sin(time * 1.3 + p.x * 0.5) * 0.5;
    float osc2 = cos(time * 0.7 + p.y * 0.3) * 0.3;
    float osc3 = sin(time * 1.618 + length(p) * 0.2) * 0.2;
    return osc1 + osc2 + osc3;
}

// Noise-based organic deformation
vec3 organicDeformation(vec3 pos, float time) {
    float noise1 = snoise(pos * 0.5 + time * 0.1);
    float noise2 = snoise(pos * 1.0 + time * 0.2);
    float noise3 = snoise(pos * 2.0 + time * 0.3);
    
    return pos + vec3(noise1, noise2, noise3) * 0.05;
}

// Easing functions for natural motion
float easeInOutCubic(float t) {
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

// Audio-reactive organic motion
vec3 audioReactiveMotion(vec3 pos, float time) {
    float bassPhase = iAudioBands.x * 6.28318;
    float midPhase = iAudioBands.y * 6.28318;
    float treblePhase = iAudioBands.z * 6.28318;
    
    pos.x += sin(bassPhase + time) * iAudioBandsAtt.x * 0.1;
    pos.y += cos(midPhase + time * 1.5) * iAudioBandsAtt.y * 0.05;
    pos.z += sin(treblePhase + time * 2.0) * iAudioBandsAtt.z * 0.02;
    
    return pos;
}
```

#### 9.4.2 Temporal Effects & Feedback
Use the feedback buffer for temporal continuity:

```glsl
// Motion blur using feedback buffer
vec3 motionBlur(vec2 uv, vec3 velocity) {
    vec2 blurDir = velocity.xy * 0.02;
    vec3 accumulated = vec3(0.0);
    float samples = 5.0;
    
    for(float i = 0.0; i < samples; i++) {
        float t = (i / (samples - 1.0) - 0.5) * 2.0;
        vec2 sampleUV = uv + blurDir * t;
        accumulated += texture(iChannel0, sampleUV).rgb;
    }
    
    return accumulated / samples;
}

// Trails and persistence effect
vec3 temporalTrails(vec2 uv, vec3 newColor, float persistence) {
    vec3 previous = texture(iChannel0, uv).rgb;
    return mix(previous, newColor, persistence);
}

// Temporal noise for organic variation
float temporalNoise(vec2 p, float time) {
    return snoise(vec3(p * 0.1, time * 0.1));
}
```

### 9.5 Post-Processing Pipeline

#### 9.5.1 Complete Color Pipeline
Implement a professional post-processing chain:

```glsl
// Complete post-processing pipeline
vec3 postProcess(vec3 color, vec2 uv, float time) {
    // 1. Tone mapping
    color = acesFilm(color);
    
    // 2. Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    // 3. Color grading
    color = liftGammaGain(color, vec3(0.0), vec3(1.0, 1.1, 0.9), vec3(1.1, 1.0, 0.95));
    
    // 4. Film grain
    color += filmGrain(uv, time);
    
    // 5. Vignette
    float vignette = 1.0 - length(uv * 0.8);
    vignette = smoothstep(0.0, 1.0, vignette);
    color *= vignette;
    
    // 6. Chromatic aberration (subtle)
    float ca = length(uv) * 0.001;
    color.r = texture(iChannel0, uv + vec2(ca, 0.0)).r;
    color.b = texture(iChannel0, uv - vec2(ca, 0.0)).b;
    
    return color;
}
```

### 9.6 Scene Architecture Patterns

#### 9.6.1 Layered Composition
Structure your scenes with depth and complexity:

```glsl
// Multi-layer scene composition
vec3 renderLayeredScene(vec2 uv, float time) {
    // Background layer (atmosphere, sky)
    vec3 background = renderAtmosphere(uv, time);
    
    // Midground layer (main structures)
    vec3 midground = renderMainStructures(uv, time);
    
    // Foreground layer (particles, effects)
    vec3 foreground = renderParticles(uv, time);
    
    // Atmospheric perspective
    float depth = getDepth(uv);
    vec3 atmosphericColor = mix(vec3(0.6, 0.8, 1.0), vec3(1.0, 0.9, 0.7), depth);
    
    // Composite with depth-based blending
    vec3 final = mix(background, midground, 0.7);
    final = mix(final, foreground, 0.3);
    final = mix(final, atmosphericColor, depth * 0.3);
    
    return final;
}
```

### 9.7 Performance Optimization for Complex Scenes

#### 9.7.1 Efficient Raymarching
Maintain quality while keeping performance acceptable:

```glsl
// Adaptive raymarching step count
float raymarch(vec3 ro, vec3 rd, float maxDist) {
    float d = 0.0;
    float total = 0.0;
    int maxSteps = 100;
    
    for(int i = 0; i < maxSteps; i++) {
        // Adaptive step size based on distance
        float stepSize = mix(0.001, 0.1, total/maxDist);
        d = map(ro + rd * total) * 0.8; // Slight overshoot reduction
        
        if(d < stepSize) break;
        if(total > maxDist) break;
        
        total += d;
    }
    
    return total;
}

// Level of detail system
float lodMap(vec3 p, float distance) {
    float detail = 1.0;
    if(distance > 5.0) detail = 0.5;
    if(distance > 10.0) detail = 0.25;
    
    return map(p * detail) / detail;
}
```

### 9.8 Creative Philosophy & Artistic Direction

When creating shaders for RaymarchVibe, you are composing visual music that breathes with audio and evolves through time. The inspiration shaders demonstrate how mathematical precision and artistic intuition combine to create immersive experiences.

#### 9.8.1 The "Wow Factor" Checklist
Before considering a shader complete, verify it has:

- **3+ layers of visual depth** (foreground, midground, background)
- **Organic, multi-frequency motion** (not just simple rotation)
- **Sophisticated color palette** (analogous schemes, temperature variation)
- **Multi-dimensional audio reactivity** (scale, color, position, glow)
- **Temporal evolution** (30+ seconds reveals new elements)
- **Professional post-processing** (tone mapping, color grading)
- **Atmospheric effects** (fog, haze, volumetric lighting)
- **Material sophistication** (iridescence, subsurface scattering)

#### 9.8.2 Anti-Patterns to Avoid
- Static, perfectly symmetric compositions
- Pure primary colors without blending
- Linear, constant-speed motion
- Single-parameter audio reactivity
- Flat 2D shapes without depth
- Overcomplication without compositional purpose
- Ignoring the feedback buffer for temporal effects

#### 9.8.3 The "Stop-and-Stare" Test
A great shader should make people:
- Stop scrolling immediately (visually arresting)
- Lose track of time (hypnotic, meditative quality)
- Want to interact (feels alive and responsive)
- Discover new details (rewards sustained viewing)
- Feel emotion (evokes mood and atmosphere)

Remember: Technical perfection without artistic vision creates forgettable visuals. A shader with heart and motion will captivate audiences even with minor technical imperfections. Trust your artistic instincts while implementing these professional techniques.

### 10. Implementation Templates & Starter Code

#### 10.1 Professional Shader Template
Use this as a starting point for production-quality shaders:

```glsl
#version 330 core
out vec4 FragColor;

// Standard RaymarchVibe uniforms
uniform vec2 iResolution;
uniform float iTime;
uniform float iFps;
uniform float iFrame;
uniform float iProgress;
uniform vec4 iAudioBands;
uniform vec4 iAudioBandsAtt;
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;

// --- Creative Parameters ---
uniform vec3 u_baseHue = vec3(0.6, 0.8, 0.9); // {"widget":"color", "label":"Base Hue"}
uniform float u_complexity = 1.0; // {"label":"Complexity", "min":0.0, "max":2.0}
uniform float u_organicMotion = 1.0; // {"label":"Organic Motion", "min":0.0, "max":2.0}
uniform float u_audioReactivity = 1.0; // {"label":"Audio Reactivity", "min":0.0, "max":2.0}

// Mathematical constants
const float PI = 3.14159265359;
const float TAU = 2.0 * PI;

// --- Core Functions (from sections above) ---
// Include: hsv2rgb, smin, gyroid, snoise, etc.

// Scene SDF with complexity layers
float map(vec3 p) {
    // Apply organic deformation
    p = organicDeformation(p, iTime * u_organicMotion);
    
    // Apply audio-reactive motion
    p = audioReactiveMotion(p, iTime);
    
    // Base structure (customize this)
    float structure = gyroid(p) * u_complexity;
    
    // Add detail layers
    float detail = snoise(p * 4.0 + iTime) * 0.1;
    
    return structure + detail;
}

// Advanced lighting
vec3 computeLighting(vec3 pos, vec3 normal, vec3 viewDir, vec3 baseColor) {
    // Implement multi-light setup from section 9.3.1
    vec3 lighting = baseColor * 0.1; // Ambient
    
    // Add iridescence
    lighting += iridescence(normal, viewDir, 0.5);
    
    return lighting;
}

// Main render function
vec3 render(vec2 uv) {
    // Camera setup
    vec3 ro = vec3(0.0, 0.0, 3.0);
    vec3 rd = normalize(vec3(uv, -1.0));
    
    // Raymarch
    float t = raymarch(ro, rd, 20.0);
    vec3 pos = ro + rd * t;
    vec3 normal = calcNormal(pos);
    vec3 viewDir = normalize(ro - pos);
    
    // Base color from palette
    vec3 baseColor = generatePalette(iTime * 0.1, u_baseHue);
    
    // Lighting
    vec3 color = computeLighting(pos, normal, viewDir, baseColor);
    
    // Atmospheric effects
    color = atmosphericFog(color, t, pos.y);
    
    return color;
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    
    // Render main scene
    vec3 color = render(uv);
    
    // Post-processing
    color = postProcess(color, uv, iTime);
    
    // Temporal effects
    color = temporalTrails(uv, color, 0.9);
    
    FragColor = vec4(color, 1.0);
}
```

#### 10.2 Progressive Enhancement Path
Follow this sequence to build complex shaders:

1. **Foundation (Week 1)**: Basic SDF + simple lighting
2. **Motion (Week 2)**: Add organic animation systems
3. **Color (Week 3)**: Implement HSV palettes + grading
4. **Lighting (Week 4)**: Multi-light setup + materials
5. **Atmosphere (Week 5)**: Fog, haze, volumetric effects
6. **Post-Process (Week 6)**: Tone mapping + temporal effects
7. **Polish (Week 7)**: Performance optimization +细节完善

### 11. Quality Assurance & Testing

#### 11.1 Visual Quality Checklist
- [ ] Multiple depth layers visible
- [ ] Organic, non-linear motion patterns
- [ ] Sophisticated color palette (no pure primaries)
- [ ] Audio affects 3+ parameters simultaneously
- [ ] Scene evolves over 30+ seconds
- [ ] Professional post-processing applied
- [ ] Atmospheric depth present
- [ ] Materials show complexity (iridescence, etc.)
- [ ] Performance > 30fps on target hardware
- [ ] Passes "stop-and-stare" test

#### 11.2 Performance Benchmarks
- **Simple scenes**: 60+ fps, <5ms per frame
- **Medium complexity**: 45+ fps, <10ms per frame  
- **Complex scenes**: 30+ fps, <15ms per frame
- **Maximum complexity**: 20+ fps, <20ms per frame

#### 11.3 Audio Reactivity Testing
Test with different music genres:
- **Electronic**: Check bass response and stroboscopic effects
- **Classical**: Verify dynamic range and subtle response
- **Rock**: Test impact and energy response
- **Ambient**: Ensure gentle, flowing reactivity

### 12. Inspiration Analysis & Learning

The inspiration shaders demonstrate these key patterns:

#### 12.1 Common Mathematical Techniques
- Gyroid surfaces for organic structures
- Lattice architectures for complexity
- Multi-octave noise for natural variation
- Space warping for fluid deformation

#### 12.2 Signature Visual Elements
- Atmospheric perspective and depth
- Temperature-based color gradients
- Multi-source lighting setups
- Temporal continuity through feedback

#### 12.3 Motion Characteristics
- Non-integer frequency ratios (1.3, 1.618, 2.414)
- Noise-based organic deformation
- Easing functions for natural movement
- Layered oscillation systems

Study these patterns, understand the underlying mathematics, then apply them with your own artistic vision. The goal is not to copy, but to learn the techniques and create something uniquely yours.

---

**Remember**: The inspiration shaders set a high bar, but they're achievable through systematic application of these techniques. Start simple, add complexity gradually, and always prioritize artistic impact over technical complexity.


## 13. Change Log

- **v2.0** (Current): Major upgrade - Professional shader architecture guide
  - Added comprehensive mathematical foundations section
  - Implemented professional color theory and palette design guidance
  - Added advanced lighting and material systems
  - Included organic motion and temporal effects techniques
  - Added complete post-processing pipeline guidance
  - Implemented scene architecture patterns
  - Added performance optimization strategies
  - Created professional shader template
  - Added quality assurance and testing frameworks
  - Analyzed inspiration shaders for learning patterns
  - Established progressive enhancement path for skill development

- **v1.0** (Previous): Established single source of truth specification
  - Superseded all previous shader documentation
  - Consolidated Milk-Converter requirements
  - Standardized uniform naming and UI syntax
