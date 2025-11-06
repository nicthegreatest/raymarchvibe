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

#### 9.1.3 Kaleidoscopic & Angular Folding
Create mandala-like symmetry through angular repetition:

```glsl
// Kaleidoscopic folding - creates radial symmetry
vec2 foldRotate(in vec2 p, in float symmetry) {
    float angle = PI / symmetry - atan(p.x, p.y);
    float n = PI * 2.0 / symmetry;
    angle = floor(angle / n) * n;
    return p * mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
}

// Modular polar repetition - circular array of objects
vec2 modPolar(vec2 p, float repetitions) {
    float angle = TAU / repetitions;
    float a = atan(p.y, p.x) + angle * 0.5;
    a = mod(a, angle) - angle * 0.5;
    return vec2(cos(a), sin(a)) * length(p);
}

// Complex kaleidoscopic pattern with iteration
vec2 iterativeFold(vec2 p, float time) {
    for(int i = 0; i < 3; i++) {
        p = foldRotate(p, 8.0);
        p = abs(p) - 0.25;
        p *= mat2(cos(PI * 0.25), sin(PI * 0.25), 
                  -sin(PI * 0.25), cos(PI * 0.25));
    }
    return p;
}
```

#### 9.1.4 Fractal Systems: Kalibox & Mandelbox
Advanced iterative fractals for complex organic structures:

```glsl
// Kalibox fractal - creates intricate folded structures
float kalibox(vec3 pos, float time) {
    float Scale = 1.84;
    int Iterations = 14;
    float MinRad2 = 0.34;
    vec3 Trans = vec3(0.076, -1.86, 0.036);
    vec3 Julia = vec3(-0.66, -1.2 + time * 0.0125, -0.66);
    
    vec4 scale = vec4(Scale) / MinRad2;
    float absScalem1 = abs(Scale - 1.0);
    float AbsScaleRaisedTo1mIters = pow(abs(Scale), float(1 - Iterations));
    
    vec4 p = vec4(pos, 1.0);
    vec4 p0 = vec4(Julia, 1.0);
    
    for(int i = 0; i < Iterations; i++) {
        p.xyz = abs(p.xyz) + Trans;
        float r2 = dot(p.xyz, p.xyz);
        p *= clamp(max(MinRad2 / r2, MinRad2), 0.0, 1.0);
        p = p * scale + p0;
    }
    
    return (length(p.xyz) - absScalem1) / p.w - AbsScaleRaisedTo1mIters;
}

// Orbit trap for fractal coloring
vec4 orbitTrap = vec4(10.0);

float kaliboxWithColor(vec3 pos) {
    // Track minimum distances during iteration for coloring
    orbitTrap = vec4(10.0);
    vec4 p = vec4(pos, 1.0);
    
    for(int i = 0; i < 14; i++) {
        p.xyz = abs(p.xyz) + vec3(0.076, -1.86, 0.036);
        float r2 = dot(p.xyz, p.xyz);
        orbitTrap = min(orbitTrap, abs(vec4(p.xyz, r2)));
        // Continue iteration...
    }
    
    return distance;
}

// Extract color from orbit trap
vec3 getOrbitColor() {
    vec4 X = vec4(0.5, 0.6, 0.6, 0.2);
    vec4 Y = vec4(1.0, 0.5, 0.1, 0.7);
    vec4 Z = vec4(0.8, 0.7, 1.0, 0.3);
    vec4 R = vec4(0.7, 0.7, 0.5, 0.1);
    
    orbitTrap.w = sqrt(orbitTrap.w);
    vec3 color = X.xyz * X.w * orbitTrap.x + 
                 Y.xyz * Y.w * orbitTrap.y + 
                 Z.xyz * Z.w * orbitTrap.z + 
                 R.xyz * R.w * orbitTrap.w;
    return color;
}
```

#### 9.1.5 Procedural Noise: Worley & Simplex
Essential noise functions for organic variation:

```glsl
// Random hash function
float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec2 hash22(vec2 p) {
    float n = sin(dot(p, vec2(1.0, 113.0)));
    return fract(vec2(2097152.0, 262144.0) * n);
}

// Worley/Voronoi noise - cellular patterns
float worley(vec2 p) {
    float d = 1.0;
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    
    for(int xo = -1; xo <= 1; xo++)
    for(int yo = -1; yo <= 1; yo++) {
        vec2 cellPos = vec2(float(xo), float(yo));
        vec2 point = hash22(ip + cellPos);
        float dist = length(fp - cellPos - point);
        d = min(d, dist);
    }
    
    return d;
}

// Fractal Worley for detailed organic texture
float fWorley(vec2 p, float time) {
    float f = worley(p * 32.0 + time * 0.25);
    f *= worley(p * 64.0 - time * 0.125);
    f *= worley(p * 128.0);
    return pow(f, 0.125); // Multiple sqrt for brightness
}

// 2D Simplex grid setup
vec3 simplexGrid(vec2 p) {
    // Skew input space
    vec2 s = floor(p + (p.x + p.y) * 0.366025403784);
    
    // Unskew back to XY space
    p -= s - (s.x + s.y) * 0.211324865;
    
    // Determine which triangle
    float i = step(p.y, p.x);
    vec2 ioffs = vec2(1.0 - i, i);
    
    // Vectors to triangle vertices
    vec2 p1 = p - ioffs + 0.211324865;
    vec2 p2 = p - 0.577350269;
    
    // Simplex noise calculation
    vec3 d = max(0.5 - vec3(dot(p, p), dot(p1, p1), dot(p2, p2)), 0.0);
    vec3 w = vec3(dot(hash22(s), p), 
                  dot(hash22(s + ioffs), p1), 
                  dot(hash22(s + 1.0), p2));
    
    return d * d * d * w; // Weighted contribution
}
```

#### 9.1.6 Advanced Space Transformations
Sophisticated deformation and distortion techniques:

```glsl
// Triangle wave - continuous periodic distortion
vec3 tri(vec3 x) {
    return abs(x - floor(x) - 0.5);
}

// Multi-frequency distortion field
float distortField(vec3 p, float time) {
    return dot(tri(p + time) + sin(tri(p + time)), vec3(0.666));
}

// Twist deformation along axis
vec3 twist(vec3 p, float amount) {
    float c = cos(amount * p.y);
    float s = sin(amount * p.y);
    mat2 m = mat2(c, -s, s, c);
    return vec3(m * p.xz, p.y);
}

// Displacement along parametric curve
vec3 displacementCurve(vec3 p, float phase, float frequency) {
    float t = p.z * frequency;
    return vec3(sin(t), 
                cos(t * 0.5 + PI + phase) * 0.37, 
                0.0) * 1.7;
}

// Path-following transformation
vec3 followPath(vec3 p, float time) {
    vec3 offset = displacementCurve(p, time, 8.0);
    return p - offset;
}

// Rodrigues rotation formula - rotate around arbitrary axis
vec3 axisAngleRotation(vec3 p, vec3 axis, float angle) {
    return mix(dot(axis, p) * axis, p, cos(angle)) + 
           sin(angle) * cross(axis, p);
}

// Blob function using asin(sin()) for smooth periodic deformation
float blob(vec3 p, float frequency) {
    p = asin(sin(p * frequency)) / frequency;
    return length(p);
}
```

#### 9.1.7 Platonic Solids & Complex Primitives
Beyond basic shapes - precise mathematical constructions:

```glsl
// Dodecahedron - 12-sided platonic solid
float dodecahedron(vec3 p, float size) {
    p /= 1.732; // sqrt(3)
    vec3 b = vec3(size);
    
    float d = max(max(abs(p.x) + 0.5 * abs(p.y) - b.x,
                      abs(p.y) + 0.5 * abs(p.z) - b.y),
                      abs(p.z) + 0.5 * abs(p.x) - b.z);
    
    // Hollow version
    b *= 0.95;
    d = max(d, -max(max(abs(p.x) + 0.5 * abs(p.y) - b.x,
                        abs(p.y) - 0.5 * abs(p.z) - b.y),
                        abs(p.z) + 0.5 * abs(p.x) - b.z));
    return d;
}

// Custom distance metric - power-based
float powerDistance(vec2 p, float power) {
    p = pow(abs(p), vec2(power));
    return pow(p.x + p.y, 1.0 / power);
}

// Alternative: 8th power distance for sharp edges
float superEllipse(vec2 p) {
    vec2 p4 = p * p * p * p;
    vec2 p8 = p4 * p4;
    return pow(p8.x + p8.y, 1.0 / 8.0);
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

#### 9.2.3 Physically-Based Color: Blackbody Radiation
Convert temperature to realistic glow colors:

```glsl
// Blackbody radiation - physically accurate temperature-to-color
// Based on Planck's law approximations
vec3 blackbody(float temperature) {
    vec3 color = vec3(255.0);
    
    // Red channel
    color.x = 56100000.0 * pow(temperature, -3.0 / 2.0) + 148.0;
    
    // Green channel
    color.y = 100.04 * log(temperature) - 623.6;
    if(temperature > 6500.0) {
        color.y = 35200000.0 * pow(temperature, -3.0 / 2.0) + 184.0;
    }
    
    // Blue channel
    color.z = 194.18 * log(temperature) - 1448.6;
    
    // Normalize and handle low temperatures
    color = clamp(color, 0.0, 255.0) / 255.0;
    if(temperature < 1000.0) {
        color *= temperature / 1000.0;
    }
    
    return color;
}

// Practical usage: map distance/intensity to temperature
vec3 glowFromDistance(float dist, float intensity) {
    // Map distance to temperature range (1000K - 6500K)
    float temp = mix(6500.0, 1000.0, clamp(dist * 0.5, 0.0, 1.0));
    return blackbody(temp) * intensity;
}

// Audio-reactive temperature variation
vec3 audioReactiveGlow(float audioEnergy) {
    float temp = mix(2000.0, 8000.0, audioEnergy);
    return blackbody(temp);
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

### 9.7 Advanced Rendering Techniques

#### 9.7.1 Multi-Bounce Reflections
Create realistic environments with reflective surfaces:

```glsl
const int MAX_REFLECTIONS = 3;
const float MISS = 1e6;

vec3 renderWithReflections(vec3 ro, vec3 rd) {
    vec3 finalColor = vec3(0.0);
    float aggRefFactor = 1.0; // Accumulated reflection factor
    
    for(int bounce = 0; bounce < MAX_REFLECTIONS; bounce++) {
        if(aggRefFactor < 0.05) break; // Skip negligible contributions
        
        float t = raymarch(ro, rd, 20.0);
        
        if(t >= MISS) {
            // Hit sky
            finalColor += aggRefFactor * skyColor(rd);
            break;
        }
        
        vec3 pos = ro + rd * t;
        vec3 normal = calcNormal(pos);
        
        // Get material properties
        vec3 baseColor = getMaterialColor(pos);
        float reflectivity = getMaterialReflectivity(pos);
        
        // Compute lighting for this bounce
        vec3 lighting = computeLighting(pos, normal, rd, baseColor);
        finalColor += aggRefFactor * lighting * (1.0 - reflectivity);
        
        // Prepare for next bounce
        aggRefFactor *= reflectivity;
        ro = pos + normal * 0.001; // Offset to avoid self-intersection
        rd = reflect(rd, normal);
    }
    
    return finalColor;
}

// Material reflectivity based on viewing angle (Fresnel)
float getMaterialReflectivity(vec3 pos, vec3 normal, vec3 viewDir) {
    float baseReflectivity = 0.5;
    float fresnel = pow(1.0 - abs(dot(normal, viewDir)), 0.25);
    return baseReflectivity * fresnel;
}
```

#### 9.7.2 Advanced Sky & Environment
Professional atmospheric rendering:

```glsl
// Procedural sky with sun and atmosphere
vec3 skyColor(vec3 rd) {
    vec3 sunDir = normalize(vec3(0.5, 0.3, -0.5));
    float sunDot = max(dot(rd, sunDir), 0.0);
    
    // Base sky gradient
    vec3 skyCol1 = vec3(0.2, 0.4, 0.6);
    vec3 skyCol2 = vec3(0.4, 0.7, 1.0);
    vec3 sky = mix(skyCol1, skyCol2, rd.y * 0.5 + 0.5);
    
    // Volumetric clouds using rounded box
    float cloudBox = length(max(abs(rd.xz / max(0.0, rd.y)) - vec2(0.5), 0.0)) - 0.1;
    sky += vec3(0.75) * pow(saturate(1.0 - cloudBox * 0.5), 9.0);
    
    // Sun glow
    vec3 sunCol = vec3(1.0, 0.95, 0.8);
    sky += 0.5 * sunCol * pow(sunDot, 20.0);   // Soft glow
    sky += 4.0 * sunCol * pow(sunDot, 400.0);  // Sharp highlight
    
    return sky;
}

// Atmospheric fog with exponential falloff
vec3 applyFog(vec3 color, float distance, vec3 rd) {
    float fogAmount = 1.0 - exp(-distance * 0.02);
    vec3 fogColor = skyColor(rd);
    return mix(color, fogColor, fogAmount);
}
```

#### 9.7.3 Bokeh & Depth of Field Effects
Add focus and depth perception:

```glsl
// Bokeh circles for particle/light effects
float bokeh(vec2 uv, vec2 position, float size, float minIntensity, float blur) {
    float dist = length(uv - position);
    float circle = smoothstep(size, size * (1.0 - blur), dist);
    circle *= mix(minIntensity, 1.0, smoothstep(size * 0.8, size, dist));
    return circle;
}

// Dirt/particle layer using bokeh
float dirtLayer(vec2 uv, float density, float time) {
    vec2 cellUV = fract(uv * density);
    vec2 cellID = floor(uv * density);
    vec2 randomOffset = hash22(cellID);
    
    return bokeh(cellUV, 
                 vec2(0.5) + randomOffset * 0.2,
                 0.05,
                 abs(randomOffset.y * 0.4) + 0.3,
                 0.25 + randomOffset.x * randomOffset.y * 0.2);
}
```

### 9.8 Performance Optimization for Complex Scenes

#### 9.8.1 Efficient Raymarching
Maintain quality while keeping performance acceptable:

```glsl
// Logarithmic termination - advanced optimization
float raymarchLogarithmic(vec3 ro, vec3 rd, float maxDist) {
    float t = 0.01;
    float nearest = 1e6;
    
    for(int i = 0; i < 100; i++) {
        vec3 pos = ro + rd * t;
        float d = map(pos);
        nearest = min(nearest, d);
        
        // Logarithmic termination condition
        if(log(t * t / d / 1e5) > 0.0 || d < 0.0001 || t > maxDist) {
            break;
        }
        
        t += d * 0.8; // Relaxation factor
    }
    
    return t < maxDist ? t : maxDist;
}

// Dynamic epsilon based on distance
float raymarchAdaptive(vec3 ro, vec3 rd, float maxDist) {
    float t = 0.01;
    
    for(int i = 0; i < 100; i++) {
        vec3 pos = ro + rd * t;
        float d = map(pos);
        
        // Dynamic precision: closer = more precise
        float epsilon = 0.0001 + t * 0.001;
        
        if(d < epsilon || t > maxDist) break;
        
        t += d;
    }
    
    return t;
}

// Level of detail system
float lodMap(vec3 p, float distance) {
    float detail = 1.0;
    if(distance > 5.0) detail = 0.5;
    if(distance > 10.0) detail = 0.25;
    
    return map(p * detail) / detail;
}

// Glow accumulation during raymarching
vec3 raymarchWithGlow(vec3 ro, vec3 rd, out float hitDist) {
    float t = 0.01;
    vec3 glow = vec3(0.0);
    vec3 glowColor = vec3(0.8, 0.3, 1.0);
    
    for(int i = 0; i < 100; i++) {
        vec3 pos = ro + rd * t;
        float d = map(pos);
        
        // Accumulate glow from near misses
        glow += glowColor * 0.01 / (1.0 + abs(d) * 20.0);
        
        if(d < 0.001 || t > 20.0) break;
        t += d * 0.7; // Slower stepping for better glow
    }
    
    hitDist = t;
    return glow;
}
```

#### 9.8.2 Scanline & Retro Effects
Nostalgic visual elements for artistic flair:

```glsl
// Animated scanline borders (from inspiration shaders)
vec3 applyScanlineBorder(vec3 color, vec2 uv, float time, float blendFactor) {
    float borderTop = 0.5 + blendFactor * 0.5;
    float borderBottom = 0.5 - blendFactor * 0.5;
    
    // Top border effects
    if(uv.y >= borderTop + 0.07) {
        if(mod(uv.x - 0.06 * time, 0.18) <= 0.16) color *= 0.5;
    }
    if(uv.y >= borderTop + 0.05) {
        if(mod(uv.x - 0.04 * time, 0.12) <= 0.10) color *= 0.6;
    }
    
    // Bottom border effects
    if(uv.y <= borderBottom - 0.06) {
        if(mod(uv.x + 0.04 * time, 0.12) <= 0.10) color *= 0.6;
    }
    
    return color;
}

// CRT screen curvature and vignette
vec3 crtEffect(vec3 color, vec2 uv) {
    // Vignette
    float vignette = 1.0 - dot(uv * 0.8, uv * 0.8) * 0.15;
    
    // Scanlines
    color *= 0.9 + 0.2 * sin(uv.y * iResolution.y * 10.0);
    
    return color * vignette;
}
```

### 9.9 Alternative Rendering: 3D Projection Without Raymarching

Some effects don't require raymarching - manual 3D projection can be faster:

```glsl
// 3D to 2D perspective projection
vec2 project3D(vec3 point, vec3 camera, float focalLength, vec2 resolution) {
    vec3 relative = point - camera;
    float distance = length(relative);
    float scale = focalLength / distance;
    
    vec2 projected = relative.xy * scale * 100.0;
    return projected + resolution * 0.5;
}

// Line intersection test (for polygon filling)
bool lineIntersection(vec2 v1, vec2 v2, vec2 v3, vec2 v4) {
    float bx = v2.x - v1.x;
    float by = v2.y - v1.y;
    float dx = v4.x - v3.x;
    float dy = v4.y - v3.y;
    
    float b_dot_d_perp = bx * dy - by * dx;
    if(b_dot_d_perp == 0.0) return false;
    
    float cx = v3.x - v1.x;
    float cy = v3.y - v1.y;
    
    float t = (cx * dy - cy * dx) / b_dot_d_perp;
    if(t < 0.0 || t > 1.0) return false;
    
    float u = (cx * by - cy * bx) / b_dot_d_perp;
    if(u < 0.0 || u > 1.0) return false;
    
    return true;
}

// Test if point is inside quad
bool insideQuad(vec2 v1, vec2 v2, vec2 v3, vec2 v4, vec2 point) {
    vec2 testPoint = vec2(point.x - 10000.0, point.y);
    int intersections = 0;
    
    if(lineIntersection(point, testPoint, v1, v2)) intersections++;
    if(lineIntersection(point, testPoint, v2, v3)) intersections++;
    if(lineIntersection(point, testPoint, v3, v4)) intersections++;
    if(lineIntersection(point, testPoint, v4, v1)) intersections++;
    
    return (intersections == 1);
}

// Draw 2D line with thickness
void drawLine(vec2 screenPos, vec2 p1, vec2 p2, float thickness, 
              vec4 color, inout vec4 pixel) {
    float a = distance(p1, screenPos);
    float b = distance(p2, screenPos);
    float c = distance(p1, p2);
    
    if(a >= c || b >= c) return;
    
    // Heron's formula for triangle area, then distance to line
    float s = (a + b + c) * 0.5;
    float dist = 2.0 / c * sqrt(s * (s - a) * (s - b) * (s - c));
    
    if(dist < thickness) {
        pixel = mix(pixel, color, 1.0 / max(1.0, dist * 3.0));
    }
}
```

### 9.10 Procedural Patterns: Truchet & Simplex Weaving

Complex tiling patterns for backgrounds and textures:

```glsl
// Truchet pattern with simplex grid
vec3 truchetPattern(vec2 p, float time) {
    // Simplex grid setup
    float scale = 5.0;
    p *= scale;
    
    vec2 s = floor(p + (p.x + p.y) * 0.366025403784);
    p -= s - (s.x + s.y) * 0.211324865;
    
    float i = step(p.y, p.x);
    vec2 ioffs = vec2(1.0 - i, i);
    
    vec2 p1 = p - ioffs + 0.211324865;
    vec2 p2 = p - 0.577350269;
    
    // Random ordering for weave effect
    float randomOrder = hash21(s + s + ioffs + s + 1.0);
    
    // Distance to triangle vertices
    vec3 distances = vec3(length(p), length(p1), length(p2));
    
    // Create torus rings at each vertex
    float mid = length(p2 - p) * 0.5;
    float torusWidth = 0.25;
    vec3 torusDistances = abs(distances - mid) - torusWidth;
    
    // Apply pattern based on random order
    vec3 color = vec3(0.075, 0.125, 0.2);
    
    // Layer tori in random order for weaving
    if(randomOrder < 1.0 / 6.0) torusDistances = torusDistances.xzy;
    else if(randomOrder < 2.0 / 6.0) torusDistances = torusDistances.yxz;
    else if(randomOrder < 3.0 / 6.0) torusDistances = torusDistances.yzx;
    else if(randomOrder < 4.0 / 6.0) torusDistances = torusDistances.zxy;
    else if(randomOrder < 5.0 / 6.0) torusDistances = torusDistances.zyx;
    
    // Render layered pattern
    for(int layer = 0; layer < 3; layer++) {
        float d = torusDistances[layer] * scale;
        color = mix(color, vec3(0.2, 0.4, 1.0), 
                    1.0 - smoothstep(0.0, 0.005, d + 0.015));
    }
    
    return color;
}
```

### 9.11 Creative Philosophy & Artistic Direction

When creating shaders for RaymarchVibe, you are composing visual music that breathes with audio and evolves through time. The inspiration shaders demonstrate how mathematical precision and artistic intuition combine to create immersive experiences.

#### 9.11.1 The "Wow Factor" Checklist
Before considering a shader complete, verify it has:

- **3+ layers of visual depth** (foreground, midground, background)
- **Organic, multi-frequency motion** (not just simple rotation)
- **Sophisticated color palette** (analogous schemes, temperature variation)
- **Multi-dimensional audio reactivity** (scale, color, position, glow)
- **Temporal evolution** (30+ seconds reveals new elements)
- **Professional post-processing** (tone mapping, color grading)
- **Atmospheric effects** (fog, haze, volumetric lighting)
- **Material sophistication** (iridescence, subsurface scattering)

#### 9.11.2 Anti-Patterns to Avoid
- Static, perfectly symmetric compositions
- Pure primary colors without blending
- Linear, constant-speed motion
- Single-parameter audio reactivity
- Flat 2D shapes without depth
- Overcomplication without compositional purpose
- Ignoring the feedback buffer for temporal effects

#### 9.11.3 The "Stop-and-Stare" Test
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

The inspiration shaders (especially 11-20) demonstrate these key patterns:

#### 12.1 Common Mathematical Techniques
- **Gyroid surfaces** for organic structures (Section 9.1.1)
- **Kalibox fractals** for intricate detail (Section 9.1.4)
- **Worley/Voronoi noise** for cellular textures (Section 9.1.5)
- **Kaleidoscopic folding** for radial symmetry (Section 9.1.3)
- **Simplex grids** for triangular patterns (Section 9.10)
- **Space warping & twist deformations** for fluid shapes (Section 9.1.6)
- **Orbit traps** for fractal coloring (Section 9.1.4)
- **Custom distance metrics** (power-based) for unique shapes (Section 9.1.7)
- **Modular polar repetition** for circular arrays (Section 9.1.3)
- **Path following** for organic camera/object motion (Section 9.1.6)

#### 12.2 Signature Visual Elements
- **Blackbody radiation** for physically-accurate glows (Section 9.2.3)
- **Multi-bounce reflections** for realistic environments (Section 9.7.1)
- **Atmospheric perspective** with procedural sky (Section 9.7.2)
- **Bokeh effects** for depth perception (Section 9.7.3)
- **Scanline borders** for retro aesthetics (Section 9.8.2)
- **Temperature-based color gradients** (Section 9.2.3)
- **Multi-source lighting setups** (Section 9.3.1)
- **Temporal continuity** through feedback (Section 9.4.2)

#### 12.3 Advanced Rendering Approaches
- **Logarithmic ray termination** for optimization (Section 9.8.1)
- **Dynamic epsilon** for adaptive precision (Section 9.8.1)
- **Glow accumulation** during raymarching (Section 9.8.1)
- **3D projection without raymarching** for performance (Section 9.9)
- **Truchet weaving** for complex patterns (Section 9.10)

#### 12.4 Motion Characteristics
- **Non-integer frequency ratios** (1.3, 1.618, 2.414) for organic feel
- **Rodrigues rotation** for arbitrary-axis rotation (Section 9.1.6)
- **Blob functions** with asin(sin()) for smooth deformation (Section 9.1.6)
- **Displacement curves** for path animation (Section 9.1.6)
- **Noise-based organic deformation** (Section 9.4.1)
- **Easing functions** for natural movement (Section 9.4.1)
- **Layered oscillation systems** with multiple frequencies

Study these patterns, understand the underlying mathematics, then apply them with your own artistic vision. The goal is not to copy, but to learn the techniques and create something uniquely yours.

**Key Insight**: The inspiration shaders 11-20 show that professional visuals come from combining multiple advanced techniques - fractal systems with custom noise, blackbody lighting with reflection systems, parametric motion with kaleidoscopic symmetry. Master individual techniques, then layer them thoughtfully.

---

**Remember**: The inspiration shaders set a high bar, but they're achievable through systematic application of these techniques. Start simple, add complexity gradually, and always prioritize artistic impact over technical complexity.


## 13. Change Log

- **v2.1** (Current): Enhanced mathematical sophistication - Inspiration shaders 11-20 integration
  - Added **Kaleidoscopic & Angular Folding** techniques (Section 9.1.3)
  - Documented **Kalibox/Mandelbox fractals** with orbit trap coloring (Section 9.1.4)
  - Included **Worley/Voronoi noise** and **Simplex grid** systems (Section 9.1.5)
  - Added **Advanced Space Transformations**: twist, displacement curves, Rodrigues rotation (Section 9.1.6)
  - Documented **Platonic Solids** (dodecahedron) and custom distance metrics (Section 9.1.7)
  - Implemented **Blackbody radiation** for physically-based temperature colors (Section 9.2.3)
  - Added **Multi-bounce reflections** rendering system (Section 9.7.1)
  - Documented **Advanced sky & environment** rendering (Section 9.7.2)
  - Included **Bokeh & depth of field** effects (Section 9.7.3)
  - Added **Logarithmic ray termination** and dynamic epsilon optimization (Section 9.8.1)
  - Documented **Glow accumulation** during raymarching (Section 9.8.1)
  - Added **Scanline borders** and retro CRT effects (Section 9.8.2)
  - Included **3D projection without raymarching** techniques (Section 9.9)
  - Documented **Truchet patterns** and simplex weaving (Section 9.10)
  - Expanded inspiration analysis with specific technique cross-references (Section 12)
  - Total new techniques documented: 20+ advanced mathematical and rendering methods

- **v2.0** (Previous): Major upgrade - Professional shader architecture guide
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

- **v1.0** (Initial): Established single source of truth specification
  - Superseded all previous shader documentation
  - Consolidated Milk-Converter requirements
  - Standardized uniform naming and UI syntax
