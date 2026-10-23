# RaymarchVibe Shader Techniques (companion to SHADERS.md v3.0, 2026-10-24)

Advanced techniques moved out of `SHADERS.md` unchanged (v2.4 text and numbering). Use them in the **visual pass**. Where an example reads audio (9.2.3 `audioReactiveGlow`, 9.4.1 `audioReactiveMotion`, 9.13), do not add it in stage 1: give that element an `AUDIO_HOOK` (SHADERS.md §6) and let stage 3 feed it the conditioned levels from `AUDIO_REACTIVITY.md` 3.3. Section references to 3.x, 4.x, 7.x, 8.x and 9.12 point to `SHADERS.md` or `AUDIO_REACTIVITY.md` (3.x).

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

// Audio-reactive organic motion.
// Takes the CONDITIONED levels from 3.3 (0..1, ~0.4-0.9 on music), computed once
// in main(). Raw bands do not work here: bass is often pinned at 1.0 (a constant
// offset), treble sits around 0.007 (no motion), and a raw band used as a phase
// jitters every frame.
vec3 audioReactiveMotion(vec3 pos, float time, float bass, float mids, float treb) {
    pos.x += sin(time)       * bass * 0.10;
    pos.y += cos(time * 1.5) * mids * 0.05;
    pos.z += sin(time * 2.0) * treb * 0.02;
    return pos;
}
```

#### 9.4.2 Temporal Effects & Input Channels
Use the node wired to an input channel for temporal continuity. `iChannel0` is that node's output, not this node's previous frame (see 5.1):

```glsl
// Motion blur using the input node on pin 0
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

// Trails and persistence against the input node
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
        float d = map(pos).x;   // map() returns vec2 (see 2.4)
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
        float d = map(pos).x;
        
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

// Glow accumulation during raymarching.
// Weight each sample by the step length so the sum is an integral along the ray.
// An unweighted `glow += f(d)` grows with the NUMBER of steps, and rays grazing
// a surface take hundreds of tiny ones, so it washes the image out (see 7.5).
vec3 raymarchWithGlow(vec3 ro, vec3 rd, out float hitDist) {
    float t = 0.01;
    float glow = 0.0;
    vec3 glowColor = vec3(0.8, 0.3, 1.0);
    
    for(int i = 0; i < 100; i++) {
        vec3 pos = ro + rd * t;
        float d = map(pos).x;
        
        if(d < 0.001 || t > 20.0) break;
        float stepLen = d * 0.8;
        glow += exp(-d * 10.0) * min(stepLen, 0.5);   // tight falloff x step length
        t += stepLen;
    }
    
    hitDist = t;
    return glowColor * glow * 0.2;                     // one normalising constant, colour once
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
- Ignoring the input channels for temporal continuity

#### 9.11.3 The "Stop-and-Stare" Test
A great shader should make people:
- Stop scrolling immediately (visually arresting)
- Lose track of time (hypnotic, meditative quality)
- Want to interact (feels alive and responsive)
- Discover new details (rewards sustained viewing)
- Feel emotion (evokes mood and atmosphere)

Remember: Technical perfection without artistic vision creates forgettable visuals. A shader with heart and motion will captivate audiences even with minor technical imperfections. Trust your artistic instincts while implementing these professional techniques.


### 9.13 Trip Intensity Master Slider & Psychedelic Toolkit

**One master, many effects.** Expose a single *Trip Intensity* slider (0 = calm/realistic, 1 = full, 2 = insane) and drive every "extra" effect from it, so users get a coherent dial instead of fifteen sliders that must be moved together:

```glsl
uniform float u_trip = 1.0; // {"widget":"slider", "label":"Trip Intensity", "min":0.0, "max":2.0, "smooth":true}
float T  = clamp(u_trip, 0.0, 2.0) * (1.0 + 0.25 * level);  // overall level lifts it
float Tc = clamp(T, 0.0, 1.0);                               // for blends that must stop at 1
// twist  = 0.045 * u_twist * T * (1.0 + 0.6 * mids*mids);
// kaleidoAmount = smoothstep(0.25, 0.9, T);   melt = u_melt * T;   patterns *= Tc;
```

Keep individual sliders (twist, folds, melt, pattern, chromatic aberration, hue speed) as *multipliers* under the master. Check that `u_trip = 0` really reproduces the calm look.

**Toolkit** (worked examples: `infinite_cathedral_corridor_v2.frag`, `templates/audio_reactive_starter.frag`):

| Effect | Technique | Audio |
|---|---|---|
| Helical twist | rotate the cross-section by `k·(z − camZ)` about the camera axis | mids raise `k`, so build-ups wind it up |
| Mandala tunnel | mirror-fold the polar angle into `N` wedges, blended in by distance ahead (`smoothstep(on, on+12, dz)`) | beat snap: `N += 2`, fold onset pulled closer |
| Breathing / ripple | radius scale `1 + b·sin(...)`, small additive SDF ripple, both bounded | bass amplitude |
| Drip / melt | vertical streak bulges `sin(y·k + t·w)` (phase moves down), glass coordinates sagged in smooth columns, colour bands driven by `y + t` | — |
| Recursive ornament | arch-in-arch tracery (`x = abs(x) − w/2; w *= 0.5` per level), fold-rotate-scale IFS filigree, hex lattice with eye/jewel cells | mids open cells; treble shimmers |
| Iridescence | thin-film sheen `hueShift(palette, fresnel·k)` and a palette-driven `trippy(x)` colour loop with saturation boost | hue cycle; beat hue jump |
| Chromatic aberration | sample emissive layers (glass, light pools) three times with per-channel offsets; cheaper than three marches | treble, beat |
| Glow / bloom | step-length-weighted proximity glow (7.5 #2), colour computed once after the loop | mids |
| Beat moments | screen flash + expanding ring, hue jump, zoom punch (`focal −= …·snap`), fold snap, corridor pulse | `snap` from 3.4 |
| Deep blacks | darken albedo and ambient as `Tc` rises, add contrast after gamma, so emissive colour dominates | — |

Budget: these layers cost roughly 3× a plain scene. Compute colour functions once per pixel, not per march step, and limit chromatic-aberration resampling to the layers that show it.


### 9.14 Surface Ornament: Tiled Emissive Patterns (Quality Reference)

> **Reference: `infinite_cathedral_corridor_v2.frag` `ornament()` (~line 494) — the target level of surface detail.** Nic's reference screenshot (high Trip Intensity) is saved at `documentation/inspiration/quality_ref_cathedral_v2_ornament.png`. When judging a visual pass, compare surface detail against it.

#### 9.14.1 Core idea

Keep the **3D geometry simple** (a box corridor, piers, a vault) and put **all fine detail into crisp emissive 2D SDF ornament** painted onto the surfaces. The ornament is evaluated **once per pixel at the hit point** (not in `map()`), so it costs almost nothing compared with marching extra geometry. Because it is analytic 2D distance (`exp(-d * k)`, `smoothstep` on SDFs), it stays sharp at any distance and resolution, with no texture blur and no raymarch noise.

#### 9.14.2 Surface UV mapping: unfold the wall onto the floor

The floor uses `q.xz`. Walls use `(z, height)`, but the height is offset by how far the point is from the corridor wall, so the wall is "unfolded" onto the floor plane:

```glsl
bool isFloor = (mat > 1.5 && mat < 2.5);
vec2 c = isFloor ? q.xz : vec2(q.z, q.y + (g_W - min(abs(q.x), g_W)));
```

At the floor/wall seam (`|x| = W`, `y = 0`) both mappings agree, and walking up the wall continues the floor's lattice row by row. The floor pattern **flows seamlessly up the walls** with no visible join. `q` is the (warped) surface position; `g_W` the corridor half-width.

#### 9.14.3 Hex lattice, per-cell variants, glowing edges

- `hexCell(p)` returns the local coordinate inside the nearest hex (`.xy`) and a unique cell id (`.zw`). `hexEdge(l)` is 0 at the cell edge and 0.5 at the centre. Both are verbatim in the kit below.
- **Per-cell hash → variant:** `h = hash12(hx.zw)`; `h < 0.5` draws an **eye** (almond outline, glowing iris ring, slit pupil), otherwise a **faceted jewel** (hex gem with 6-fold rotating highlights from `cos(6 * atan(...))`).
- **Colour per cell:** `trippy(h * 0.7 + t * 0.05 + c.y * 0.05)`, i.e. the palette loop indexed by the cell hash, so neighbouring cells differ but stay in palette.
- **Glowing edges:** `jc * exp(-edge * 45.0)`, a tight exponential falloff from the cell edge. It never washes out because it is 2D, not accumulated along the ray.

#### 9.14.4 Liveliness

- **Travelling wave, not all-on:** `wave = 0.5 + 0.5 * sin(hx.z * 0.9 + hx.w * 1.3 + t * 0.6 + h * TAU)`, `emerge = smoothstep(0.7, 1.0, wave + ...)`. Only some cells are open at any time. A diagonal wave rolls across the lattice and cells **emerge and fade**. Edges keep a dim base (`0.03 + 0.6 * emerge`) so the lattice is always readable.
- **Slow crawl:** `hp = c * 2.4 + vec2(0.0, t * 0.35)`. The whole lattice drifts (down the walls, along the floor); wall arches crawl too (`mod(c.y + t * 0.25, 1.6)`), and the vault filigree flows (`c.y + t * 0.12`).
- **Audio hooks** (in the original these read `g_mids`, `g_treb`, `g_snap` directly; in the new workflow they are hooks, SHADERS.md §6):

```glsl
float emergeBoost = 0.0;  // AUDIO_HOOK: emergeBoost - more cells emerge (lowers emerge threshold), 0..1.5 (smooth, mids)
float edgeShimmer = 0.0;  // AUDIO_HOOK: edgeShimmer - cell-edge shimmer flicker, 0..1 (snappy, treble)
float pupilWiden  = 0.0;  // AUDIO_HOOK: pupilWiden - eye pupils widen on hits, 0..1 (snappy, beat/snap)
```

  They are used as `wave + 0.3 * emergeBoost`, `1.0 + 2.5 * edgeShimmer * (0.5 + 0.5 * sin(h * 50.0 + t * 23.0))`, and `0.025 + 0.02 * pupilWiden`. The overall ornament intensity in the original (`g_patI`) also rises with mids; treat that as a fourth hook (`ornamentGain`) if the plan wants it.

#### 9.14.5 Layering and compositing

- **Walls and piers** (below the springing height `g_H`): nested pointed arches, `archNest(ax, ay, P * 0.42, 0.9, g_rec - 1)` tiled every `P = bay / 4`, drawn as `exp(-ad * 70.0) * 0.45`. `archNest` draws an arch outline, an oculus in its head, then two half-size arches inside, recursively (depth = Recursion slider).
- **Vault** (above `g_H + 0.1`): `filigree()`, a fold-rotate-scale IFS of short segments (6 levels max, each scaled 1.7×), drawn as `exp(-fd * 70.0) * 0.8 * shimmer`.
- The hex layer sits underneath both, everywhere. Result × `g_patI * 0.7`.
- **Composite in `main()`:** `lit += e * (0.35 + 0.65 * ao)`, after diffuse, pulse light and thin-film sheen, and **before fog**. Ornament is emissive but softened in crevices, and fog gives it depth. It is also added to the `lightOnly` debug layer.
- **Chromatic offset (`caOff`):** `ornament(q, mat, caOff)` shifts the lookup coordinate (`c.x += caOff`). For a chromatic split, call it three times with `+caOff`, `0`, `-caOff` and take `.r`, `.g`, `.b`, exactly as `windowLightEx` does for the stained glass (`caW * 2.0`, with `caW = g_ca * 0.018`). **Note:** in the current v2 shader the ornament call passes `caOff = 0.0` (the comment says "chromatic split" but only the glass and window light are split). The 3× cost is the reason. Enable it only if FPS allows.

#### 9.14.6 Paste-ready kit (compiles)

Self-contained. `hash12`, `hueShift`, `saturateCol`, `trippy`, `hexCell`, `hexEdge` are verbatim from the shader. `hexOrnament` is the hex part of `ornament()` with hooks as globals and an added `atan(0,0)` guard (SHADERS.md 7.5 #3). Verified with `glslangValidator -S frag` embedded in a minimal test shader. If your shader already defines `TAU`, `g_colP/S/A`, `hash12` or `hueShift`, drop the duplicates. `archNest`/`filigree` depend on that shader's globals (`g_sharp`, `g_B`, `g_rec`, `g_time`); copy them from the source if you want those layers.

```glsl
// ===== Surface Ornament kit (from infinite_cathedral_corridor_v2.frag) =====
// Needs: g_colP/g_colS/g_colA (palette, set once per frame in main), TAU.
const float TAU = 6.2831853;
vec3 g_colP, g_colS, g_colA;

// Per-frame hook copies (set from the AUDIO HOOKS BLOCK in main; 0 = silent look)
float g_ornEmerge;   // <- emergeBoost  (mids)
float g_ornShimmer;  // <- edgeShimmer  (treble)
float g_ornPupil;    // <- pupilWiden   (snap / beat)

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec3 hueShift(vec3 c, float a) {
    const vec3 k = vec3(0.57735);
    float ca = cos(a);
    return c * ca + cross(k, c) * sin(a) + k * dot(k, c) * (1.0 - ca);
}
vec3 saturateCol(vec3 c, float s) {
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    return max(mix(vec3(l), c, s), 0.0);
}
// Hyper-saturated colour from the (hue-cycled) palette: blends the three
// palette colours around a loop, then adds an iridescent hue sweep.
vec3 trippy(float x) {
    vec3 c = g_colP * (0.5 + 0.5 * cos(TAU * x))
           + g_colS * (0.5 + 0.5 * cos(TAU * (x - 0.333)))
           + g_colA * (0.5 + 0.5 * cos(TAU * (x - 0.667)));
    c = hueShift(c, 1.4 * sin(TAU * x * 0.5));
    return saturateCol(c, 1.6);
}
// Hex lattice: returns (local xy, cell id). Inradius 0.5 in lattice units.
vec4 hexCell(vec2 p) {
    const vec2 s = vec2(1.0, 1.7320508);
    vec4 hC = floor(vec4(p, p - vec2(0.5, 1.0)) / s.xyxy) + 0.5;
    vec4 h = vec4(p - hC.xy * s, p - (hC.zw + 0.5) * s);
    return dot(h.xy, h.xy) < dot(h.zw, h.zw) ? vec4(h.xy, hC.xy) : vec4(h.zw, hC.zw + 0.5);
}
float hexEdge(vec2 l) {   // 0 at the cell edge, 0.5 at the centre
    l = abs(l);
    return 0.5 - max(dot(l, vec2(0.5, 0.8660254)), l.x);
}

// Surface UV. Floor: xz. Wall: (z, y + unfolded width) so the floor lattice
// continues seamlessly up the wall (W = half-width of the corridor).
vec2 surfaceUV(vec3 q, bool isFloor, float W) {
    return isFloor ? q.xz : vec2(q.z, q.y + (W - min(abs(q.x), W)));
}

// Emissive hex ornament: eyes / faceted jewels, glowing edges, travelling
// emergence wave, slow crawl. c = surface UV, t = time, caOff = lookup shift.
vec3 hexOrnament(vec2 c, float t, float caOff) {
    c.x += caOff;
    vec3 e = vec3(0.0);
    vec2 hp = c * 2.4 + vec2(0.0, t * 0.35);                     // slow crawl
    vec4 hx = hexCell(hp);
    float h = hash12(hx.zw);                                     // per-cell id hash
    float edge = hexEdge(hx.xy);
    float wave = 0.5 + 0.5 * sin(hx.z * 0.9 + hx.w * 1.3 + t * 0.6 + h * TAU);
    float emerge = smoothstep(0.7, 1.0, wave + 0.3 * g_ornEmerge);
    vec3 jc = trippy(h * 0.7 + t * 0.05 + c.y * 0.05);           // colour per cell
    float shimmer = 1.0 + 2.5 * g_ornShimmer * (0.5 + 0.5 * sin(h * 50.0 + t * 23.0));
    e += jc * exp(-edge * 45.0) * (0.03 + 0.6 * emerge) * shimmer;
    vec2 l = hx.xy;
    if (h < 0.5) {
        // eye: almond outline, glowing iris ring, slit pupil
        float almond = abs(l.y) - 0.23 * max(1.0 - (l.x * l.x) / (0.40 * 0.40), 0.0);
        float iris = length(l) - 0.12;
        float outline = smoothstep(0.03, 0.0, abs(almond));
        float ring = smoothstep(0.025, 0.0, abs(iris));
        float fill = step(iris, 0.0) * step(almond, 0.0);
        float pupil = step(abs(l.x), 0.025 + 0.02 * g_ornPupil) * step(length(l), 0.1);
        vec3 ic = trippy(h * 3.0 + 0.5 + t * 0.2);
        e += emerge * (jc * outline * 1.2 + ic * (ring * 1.6 + fill * 0.7) * (1.0 - pupil));
    } else {
        // jewel: faceted hexagonal gem with rotating highlights
        float gem = smoothstep(0.02, 0.0, max(dot(abs(l), vec2(0.5, 0.866)), abs(l.x)) - 0.22);
        float a = (dot(l, l) > 1e-12) ? atan(l.y, l.x) : 0.0;    // atan(0,0) guard
        float facet = 0.5 + 0.5 * cos(6.0 * a + t * 2.5 + h * 9.0);
        e += emerge * gem * trippy(h * 2.0 + facet * 0.3) * (0.4 + 1.2 * facet * facet);
    }
    return e;
}

// Usage: emissive added after lighting, softened by AO, before fog.
vec3 applyOrnament(vec3 lit, vec3 q, bool isFloor, float W, float ao,
                   float intensity, float t) {
    if (intensity <= 0.001) return lit;
    vec2 c = surfaceUV(q, isFloor, W);
    vec3 e = hexOrnament(c, t, 0.0) * intensity * 0.7;
    return lit + e * (0.35 + 0.65 * ao);
}
// ===== end kit =====
```

Minimal usage in `main()` (after the hooks block; `q` = hit position, `mat` = material):

```glsl
g_colP = u_primaryColor; g_colS = u_secondaryColor; g_colA = u_accentColor;
g_ornEmerge = emergeBoost; g_ornShimmer = edgeShimmer; g_ornPupil = pupilWiden;
// ... after lighting a hit:
lit = applyOrnament(lit, q, mat > 1.5 && mat < 2.5, W, ao, u_patternIntensity, iTime);
// ... then fog, tonemap
```

#### 9.14.7 Variations / future tile ideas

- **Other lattices:** square grid (`fract`/`floor`), triangle/simplex grid (9.10), Truchet arcs (9.10), Penrose-ish (overlapping 5-fold rotated grids, de Bruijn style), Voronoi cells (9.1.5) with `F2 − F1` edges.
- **Other cell glyphs:** runes, sigils, clock faces (hands from `iTime`), mini mandalas (polar fold inside the cell), circuit traces (random right-angle segments per cell, pads at ends), hexed/occult sigils and pentacles for spooky scenes (e.g. `moonlit_spooky_forest`).
- **Depth / parallax inset tiles:** offset the lookup by `rd.xy / rd.z * depth` per cell so glyphs look recessed behind the surface; or a 2–3 layer parallax stack.
- **Tiles that flip or rotate on beats:** per-cell rotation `rot2(floor(h * 4.0) * TAU / 4.0 + flipHook * PI)`, or a variant swap when `h < threshold + flipHook`. Hook it (`tileFlip`, beat) per §6.
- **Cell-level kaleidoscopes:** mirror-fold the local cell coordinate into N wedges (N from the hash) before drawing the glyph.
- **Per-surface glyph sets:** floor = jewels, walls = eyes, vault = filigree. Pick the variant from material as well as hash.
- **Emissive lines that cast light:** add a faint `ornament` term to the volumetric/glow sum near surfaces for halo (watch cost).

### 10. Implementation Templates (moved from SHADERS.md)

#### 10.1 Professional Shader Template
Use this as a structural outline. The helper functions it calls (`organicDeformation`, `raymarch`, `postProcess`, …) are placeholders for the techniques above. For a complete, compiling, audio-reactive starting point, use `templates/audio_reactive_starter.frag` (10.3).

```glsl
#version 330 core
out vec4 FragColor;

// Injected by the app if you do not declare them; declare them explicitly for clarity.
uniform vec2 iResolution;      // native mode: (width, height)
uniform float iTime;           // seconds since start

// Declare the ones you use - the app uploads each only if it is declared.
uniform float iAudioAmp;       // mean |sample| over the audio block; 0 when Audio Link is off
uniform vec4 iAudioBands;      // normalised bands: x=bass, y=mids, z=treble, w=overall (see 3.1)
uniform vec4 iAudioBandsAtt;   // the same through the snap-attack / ~250 ms-decay envelope (see 3.1)
uniform sampler2D iChannel0;   // output of the node on input pin 0 (black if unconnected)
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;
// Shadertoy mode only (uploaded only for mainImage() shaders): iFrame (int), iTimeDelta, iMouse.

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


> For a compiling starting point use `shaders/templates/visual_starter.frag` (stage 1) and `audio_reactive_starter.frag` (stage 3). In the four-stage workflow, the "Organic Motion" and "Audio Reactivity" lines of the outline above are split: motion in stage 1, audio through hooks in stage 3.

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
- **Temporal continuity** by blending with an input channel (Section 9.4.2)

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

