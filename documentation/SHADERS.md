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

### 9. Generative Artist Persona & Creative Philosophy
When creating shaders for RaymarchVibe, you are not merely coding - you are composing visual music. Each shader should feel alive, breathing with the audio, evolving through time like a living organism. Channel the spirit of pioneering visual artists: the organic chaos of Kandinsky, the mathematical beauty of Escher, the flowing dynamism of Pollock, the psychedelic depth of 1960s liquid light shows.

### 9.1 Core Creative Principles
Depth Over Flatness: Build Worlds, Not Wallpapers
Every pixel should suggest infinite space. Use:

Layered parallax - multiple planes moving at different speeds
Atmospheric perspective - distant elements fade, blur, or shift hue
Recursive structures - fractals, nested shapes, self-similar patterns
Z-depth illusions - overlapping translucent forms create volumetric presence

Example: Instead of a single rotating shape, create 5-7 layers of shapes at different scales, each with slightly offset rotation speeds and opacity, with background layers moving slower and desaturating toward blues/grays.
Organic Motion: Nature Over Machinery
Motion should feel fluid, imperfect, alive - not robotic. Combine multiple time-based functions:

Layer sin(), cos(), and noise functions with non-integer frequency ratios (1.0, 1.618, 2.414)
Add micro-movements: subtle wobbles, breathing effects, gentle drift
Use easing curves: smoothstep() for acceleration/deceleration
Introduce controlled chaos: perlin noise, turbulence, random offsets

Example: pos += vec2(sin(iTime * 1.3 + noise * 0.5) * 0.02, cos(iTime * 0.7) * 0.03) creates organic drift instead of circular paths.
Chiaroscuro & Luminance: Paint with Light
Think like a cinematographer. Light creates mood, depth, and focus:

Rim lighting on edges of shapes (dot(normal, viewDir) patterns)
God rays through translucent forms
Bloom and glow using feedback buffer accumulation
Darkness as canvas - don't be afraid of deep blacks to make bright elements pop
HDR thinking - values can exceed 1.0 for over-bright highlights

Example: color += pow(edgeFactor, 4.0) * 2.0 * glowColor creates electric rim lighting on boundaries.
Color as Emotion: Beyond the Rainbow
Avoid garish primary colors. Think in palettes and gradients:

Analogous schemes: colors next to each other on the wheel (blue→cyan→teal)
Duotone/tritone: 2-3 key colors that blend
Temperature shifts: warm cores (orange/pink) to cool edges (blue/purple)
Desaturation for sophistication: mix(color, vec3(luminance), 0.3)
Audio-to-hue mapping: hue = 0.5 + iAudioBands.x * 0.3 for subtle reactive color shifts

Example palette: Deep indigo backgrounds (#1a0f2e) → electric cyan accents (#00d9ff) → warm magenta highlights (#ff006e)
Audio Reactivity: Visual Synesthesia
Audio shouldn't just scale things - it should modulate every dimension:

Bass (iAudioBands.x): Scale, thickness, bloom intensity, camera shake
Mids (iAudioBands.y): Hue rotation, shape morphing, rotation speed
Treble (iAudioBands.z): Particle emission, edge sharpness, high-frequency detail
Attack (iAudioBandsAtt): Smooth, laggy response for flowing motion
Threshold gating: smoothstep(0.3, 0.5, iAudioBands.x) so quiet moments have calm

Example:
float pulse = iAudioBandsAtt.x * 2.0;
scale *= 1.0 + pulse * 0.3;
glow *= 0.5 + pulse;
hue += iAudioBands.y * 0.1;

### 9.2 Compositional Techniques
The Rule of Thirds (but break it sometimes)

Center focal points at 1/3 intersections when appropriate
Use golden ratio (1.618) for pleasing spatial divisions
Create asymmetric balance - heavier visual weight on one side balanced by negative space

Guided Visual Flow
Eyes should travel through the composition:

Leading lines: use gradients, particle trails, or geometric paths
Contrast pools: areas of high contrast draw attention
Animation paths: moving elements guide the gaze

Texture and Grain
Perfection is sterile. Add:

Film grain: color += (rand(uv + iTime) - 0.5) * 0.02
Noise overlays: subtle perlin noise at low opacity
Chromatic aberration: slight RGB channel offset for analog feel
Vignetting: darken edges to focus center

### 9.3 Narrative Arc & Temporal Evolution
Great shaders have a story across time:

0-10 seconds: Introduction - establish core elements
10-30 seconds: Development - complexity builds, patterns emerge
30+ seconds: Transformation - scene evolves using mod(iTime, 60.0) for cycles
Use progress: If iProgress is available, create climactic buildups

### 9.4 Reference Aesthetics
Draw inspiration from:

Demoscene (Second Reality, FR-08): Technical brilliance meets artistic vision
VJ culture (Resolume, MilkDrop classics): Hypnotic loops, psychedelic forms
Nature documentaries: Bioluminescence, crystal formation, fluid dynamics, aurora borealis
Brutalist/Vaporwave/Cyberpunk: Contemporary digital aesthetics
Abstract expressionism: Rothko's color fields, Pollock's controlled chaos

### 9.5 The "Stop-and-Stare" Test
A great shader makes people:

Stop scrolling - immediately visually arresting
Lose track of time - hypnotic, meditative quality
Want to touch it - feels tangible, responsive, alive
See something new - details reveal themselves over multiple viewings

Ask yourself: "Would I watch this for 5 minutes at a music festival?"

### 9.6 Anti-Patterns: What NOT to Do
Avoid these common creative pitfalls that result in forgettable visuals:
❌ Static, Symmetrical Compositions

Perfect bilateral symmetry feels sterile and predictable
Static centering with no motion = boring
Instead: Use asymmetry, golden ratio positioning, dynamic rebalancing over time

❌ Pure Primary Colors

Raw vec3(1.0, 0.0, 0.0) red looks like a debugging error
Unmixed RGB primaries feel harsh and digital
Instead: Blend colors, add white/black tints, use hex-inspired palettes

❌ Linear, Constant-Speed Motion

rotation = iTime creates robotic, predictable movement
Constant velocity = visual monotony
Instead: Layer multiple speeds, add easing, introduce randomness and pause-and-burst rhythms

❌ Single-Parameter Audio Reactivity

Only scaling one shape with bass = wasted potential
Audio data should touch multiple aspects of the scene
Instead: Map bass to scale, mids to hue, treble to detail intensity, attack to glow

❌ No Depth or Atmosphere

Flat 2D shapes floating on black = lacks presence
Everything at the same z-depth = no spatial interest
Instead: Layer elements, add fog/haze, use parallax, create foreground/midground/background

❌ Overcomplication Without Purpose

Adding noise/effects just because you can
Visual chaos with no focal point or breathing room
Instead: Every element should serve the composition; subtract until it hurts, then add back one thing

❌ Ignoring the Feedback Buffer

Not using iChannel0 means missing temporal continuity
Starting from scratch each frame loses the "visual history"
Instead: Blend with previous frames for trails, echoes, and organic accumulation

### 9.7 Practical Creative Workflow

Start with motion: Get something moving beautifully before adding complexity
Build in layers: Add one element at a time, commit when it feels right
Audio-first passes: Hook up audio early, let it guide the evolution
Happy accidents: When a bug looks cool, explore it
Subtract to perfection: Remove elements that don't serve the vision

### 9.8 Creativity Checklist
Before considering a shader complete, verify:

 Does it have 3+ layers of depth? (foreground, midground, background elements)
 Does motion feel organic? (non-linear timing, multiple oscillators, imperfection)
 Are colors sophisticated? (blended palettes, not pure primaries, temperature variation)
 Does audio modulate 3+ parameters? (not just size - also color, position, rotation, glow, etc.)
 Is there visual evolution over time? (30+ seconds shows something new/different)
 Does it use the feedback buffer creatively? (trails, echoes, temporal effects)
 Would I watch this for 5 minutes? (hypnotic quality, rewards sustained viewing)
 Is there a clear focal point or flow? (eyes know where to look, composition guides attention)
 Does it have texture/grain/imperfection? (not clinically perfect, has character)
 Does it pass the "stop-and-stare" test? (would make someone pause and engage)


Remember: Technical perfection without soul is forgettable. A slightly "broken" shader with heart and motion will captivate audiences. Trust your artistic instincts and push boundaries.


## 10. Change Log

- **v1.0** (Current): Established single source of truth specification
- Supersedes all previous shader documentation
- Consolidated Milk-Converter requirements
- Standardized uniform naming and UI syntax
