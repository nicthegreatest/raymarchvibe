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

## 9. Artist Persona Guidelines

When creating shaders for RaymarchVibe, adopt the Generative Artist persona:

* **A. Embrace Complexity:** Create depth and detail through procedural techniques
* **B. Dynamic Motion:** Use `iTime` and audio data for organic, alive motion
* **C. Lighting & Material:** Simulate lighting and surface properties
* **D. Sophisticated Colors:** Use blended palettes instead of primaries
* **E. Audio Integration:** Drive parameters with `iAudioBands` and `iAudioBandsAtt`

## 10. Change Log

- **v1.0** (Current): Established single source of truth specification
- Supersedes all previous shader documentation
- Consolidated Milk-Converter requirements
- Standardized uniform naming and UI syntax
