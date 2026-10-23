# RaymarchVibe Shader Specification — Core & Visual Spec (v3.0, 2026-10-24)

This document is the **authoritative specification** for the format, rendering rules and UI conventions of all RaymarchVibe shaders. It is the single source of truth for those topics; audio and advanced techniques now live in companion documents (listed below) that are part of the same specification.

**IMPORTANT:** This specification supersedes all other shader documentation. Agents and developers must reference it (and its companion documents) for accurate shader standards.

> **Where sections live (v3.0).** Section numbers from v2.4 are kept so old cross-references still resolve:
> 3.1–3.4, 11.3 and the audio-starter notes (10.3) → `AUDIO_REACTIVITY.md` · 6 and 8.2 (Milk-Converter) were removed with MilkDrop support on 2026-10-24 · 9.1–9.11, 9.13, 10.1, 10.2 and 12 → `SHADER_TECHNIQUES.md` · everything else → this file. The full map is in `../../MIGRATION_NOTES.md`.

| Document | Contents |
|---|---|
| `SHADERS.md` (this file) | Format, uniforms, UI widgets, palette, node rules, rendering gotchas, camera safety, performance, testing, the `AUDIO_HOOK` convention |
| `SHADER_WORKFLOW.md` | The four-stage process with copy-paste prompts and Nic's sign-off checklist |
| `AUDIO_REACTIVITY.md` | Audio uniforms, measured levels, the helper block, beat/drop detection, effect recipes, Debug View, EQ tuning |
| `SHADER_TECHNIQUES.md` | Advanced SDF, colour, lighting, motion, post-processing, psychedelic toolkit, inspiration analysis |

## 0. How shaders are made: the four-stage workflow

Asking for a whole shader in one go (look + spec rules + audio reactivity) gives worse results. Every new shader goes through four stages instead. Details and prompts: `SHADER_WORKFLOW.md`.

1. **Visual pass.** Build the scene with **no audio code**. Only visual sliders (Quality, colours, fog, …) and optional *preview* sliders (e.g. a fake `Preview Pulse` 0–1) to judge motion. Leave named audio hooks (§6). Nic tests and iterates until it looks right, then signs off (§11.1).
2. **Reactivity plan.** The LLM reads the approved shader and proposes a short table: element / band or trigger / effect / feel / range. Nic edits it. No code.
3. **Audio pass.** Wire only the approved plan into the hooks with the standard helper (`AUDIO_REACTIVITY.md` 3.3). In silence the shader must look exactly like the approved visual pass.
4. **Tuning.** Nic plays real tracks and adjusts Beat Threshold, Audio Sensitivity and the app's 10-band graphic EQ (not per-band sliders).

Templates: `shaders/templates/visual_starter.frag` (stage 1) and `shaders/templates/audio_reactive_starter.frag` (stage 3 reference).

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

### 2.4 ⚠️ CRITICAL: Distance Function Return Types (Shadertoy Adaptation Warning)

**This is one of the most common errors when adapting shaders from Shadertoy or other sources.**

#### The Problem

Shadertoy and many shader libraries use a **Scene Distance Function (SDF)** named `map` that returns a **single `float`** representing the shortest distance from a point to any object in the scene:

```glsl
// Shadertoy-style map function - returns a float
float map(vec3 p) {
    float d = sdSphere(p, 1.0);
    d = min(d, sdBox(p - vec3(2.0, 0.0, 0.0), vec3(0.5)));
    return d;  // Single float return
}
```

However, **RaymarchVibe requires the `map` function to return a `vec2`**, where:
- **`.x` component**: The distance (equivalent to the Shadertoy float)
- **`.y` component**: A material ID (integer identifier for determining object properties, colors, etc.)

```glsl
// RaymarchVibe-compliant map function - returns a vec2
vec2 map(vec3 p) {
    float d = sdSphere(p, 1.0);
    float material = 1.0;
    
    float dBox = sdBox(p - vec3(2.0, 0.0, 0.0), vec3(0.5));
    if(dBox < d) {
        d = dBox;
        material = 2.0;
    }
    
    return vec2(d, material);  // vec2 return: (distance, material_id)
}
```

#### The Error

When adapting Shadertoy code, developers often forget to access the `.x` component when calling `map()` in functions like `rayMarch()` or `getNormal()`. This causes a type mismatch:

```glsl
// ❌ WRONG - Shadertoy-style call, expects float but gets vec2
float distance = map(current_position);
// ERROR: initializer of type 'vec2' cannot be assigned to variable of type 'float'

// ❌ WRONG - Trying to assign vec2 to float
float t = map(ro + rd * distance);
```

#### The Solution

**Always access the `.x` component when using the result of `map()`:**

```glsl
// ✅ CORRECT - Access the .x component for distance
float distance = map(current_position).x;

// ✅ CORRECT - In raymarching loops
for(int i = 0; i < MAX_STEPS; i++) {
    vec3 pos = ro + rd * t;
    float d = map(pos).x;  // Extract distance
    if(d < 0.0001) break;
    t += d * 0.8;
}

// ✅ CORRECT - In normal calculation (use .x for all map calls)
vec3 getNormal(vec3 p) {
    const float e = 0.0001;
    vec2 d = vec2(e, 0.0);
    return normalize(vec3(
        map(p + d.xyy).x - map(p - d.xyy).x,
        map(p + d.yxy).x - map(p - d.yxy).x,
        map(p + d.yyx).x - map(p - d.yyx).x
    ));
}

// ✅ CORRECT - Using material ID when needed (access .y for material)
vec2 result = map(position);
float dist = result.x;
float materialID = result.y;
vec3 color = getMaterialColor(int(materialID));
```

#### When Adapting Shadertoy Shaders

When converting a Shadertoy shader to RaymarchVibe:

1. **Define `map()` to return `vec2`** instead of `float`:
   ```glsl
   vec2 map(vec3 p) {
       // ... your distance calculations ...
       return vec2(distance, materialID);
   }
   ```

2. **Update all `map()` calls** to access `.x`:
   - Search for every occurrence of `map(` in your code
   - If assigning to a `float` variable, append `.x`
   - If you need material information, extract `.y` as well

3. **Update `rayMarch()` function**:
   ```glsl
   float rayMarch(vec3 ro, vec3 rd) {
       float t = 0.0;
       for(int i = 0; i < MAX_STEPS; i++) {
           float d = map(ro + rd * t).x;  // Don't forget .x
           if(d < EPSILON) break;
           t += d;
       }
       return t;
   }
   ```

4. **Update normal calculation**:
   ```glsl
   vec3 getNormal(vec3 p) {
       const float e = 0.0001;
       vec2 d = vec2(e, 0.0);
       return normalize(vec3(
           map(p + d.xyy).x - map(p - d.xyy).x,
           map(p + d.yxy).x - map(p - d.yxy).x,
           map(p + d.yyx).x - map(p - d.yyx).x
       ));
   }
   ```

#### Prevention Checklist

When writing or adapting raymarching shaders:

- [ ] Define `map()` to return `vec2(distance, materialID)`
- [ ] Every `map()` call that expects a float uses `.x` (e.g., `map(p).x`)
- [ ] Normal calculation accesses `.x` for all `map()` calls
- [ ] Raymarching loop uses `.x` when comparing with epsilon
- [ ] Material-dependent coloring accesses `.y` when needed
- [ ] Test in RaymarchVibe to confirm no compilation errors

**Key Takeaway**: The `.x` component is mandatory when using the result of `map()` for distance calculations. This is the single most important difference between RaymarchVibe and Shadertoy raymarching code.


## 3. Built-in Uniforms

Declare the uniforms your shader uses. Each one is uploaded only if the compiled shader declares it (every lookup in `ShaderEffect::Render` is guarded with `!= -1`), and the app injects the *declaration* of `iResolution` and `iTime` for you if you leave them out (`InjectStandardUniforms` in `src/ShaderEffect.cpp`).

| Name | Type | Description | Example Usage |
|---|---|---|---|
| `iTime` | `float` | Time in seconds | `sin(iTime * 2.0)` |
| `iResolution` | `vec2` | Viewport resolution (width, height) in pixels | `iResolution.x` for aspect ratios |
| `iAudioAmp` | `float` | Overall level: the mean of the absolute samples over the analysed audio block. Reads `0.0` when Audio Link is off | `iAudioAmp * 0.5` |
| `iAudioBands` | `vec4` | Raw audio bands, clamped to 0..1 — see §3.1. Real music uses a small, band-dependent part of that range (§3.2); condition before use (§3.3) | `bandLevel(iAudioBands.z, 0.0005, 0.2, 0.0)` |
| `iAudioBandsAtt` | `vec4` | The same four values through a snap-attack / ~250 ms-decay envelope, 0..1 — see §3.1–3.4 | `iAudioBandsAtt.y` |
| `iChannel0` | `sampler2D` | Output texture of the node wired to input pin 0; a 1×1 black texture when nothing is connected | `texture(iChannel0, uv)` |
| `iChannel1` | `sampler2D` | As `iChannel0`, for input pin 1 | `texture(iChannel1, uv)` |
| `iChannel2` | `sampler2D` | As `iChannel0`, for input pin 2 | `texture(iChannel2, uv)` |
| `iChannel3` | `sampler2D` | As `iChannel0`, for input pin 3 | `texture(iChannel3, uv)` |
| `iFrame` | `int` | Frame counter, uploaded with `glUniform1i` **in Shadertoy mode only** — in the standard `void main()` path it is never uploaded, so a shader that declares it reads 0 | `float(iFrame)` |
| `iTimeDelta` | `float` | Seconds since the previous frame — **Shadertoy mode only** | `iTimeDelta` |
| `iMouse` | `vec4` | Mouse state — **Shadertoy mode only** | `iMouse.xy` |

**Not provided:** there is no `iFps` and no `iProgress` uniform — the app has no lookup for either, so a shader that declares them silently reads `0.0`. Five in-repo templates still declare them, and none of the five reads the value (`bezier_fractal_visualizer.frag`, `gemini_bubble.frag`, `organic_audio_viz.frag`, `organic_fractal_tree.frag`, `viz_circular_audio.frag`).


**Audio uniforms** (`iAudioAmp`, `iAudioBands`, `iAudioBandsAtt`): their exact semantics (bins, normalisation, envelope), measured real-world levels, the paste-ready helper and beat detection are in `AUDIO_REACTIVITY.md` (3.1–3.4). A visual-pass shader (stage 1) declares **none** of them.

## 4. UI Controls Specification

### 4.1 Syntax
Uniforms control UI generation via JSON comments. The syntax is:

```glsl
uniform <type> <name> = <default_value>; // {"widget":"<type>", "min":<min>, "max":<max>, "step":<step>}
```

### 4.2 Supported Widget Types

#### Slider (`"widget":"slider"`)
- **Parameters:** `min`, `max`. If omitted they default to **0..1 for `float`** and **0..100 for `int`** (`src/ShaderEffect.cpp:544–547`). Always give them.
- **`step` is ignored by sliders.** `float` controls are drawn with `ImGui::SliderFloat` and `int` controls with `ImGui::SliderInt`, neither of which takes a step. `step` (default 0.01) is used only as the drag speed of `vec2`/`vec3`/`vec4` drag widgets (`src/ShaderEffect.cpp:540–561`). Leaving `step` in slider metadata is harmless but does nothing.
- **Applicable types:** `float`, `int` (an `int` slider moves in whole numbers and is uploaded with `glUniform1i`). Every `float` and `int` control is drawn as a slider whatever `widget` says; `bool` controls become checkboxes.
- **Do not name a slider `u_speed`** unless you want it to scale the app clock — see 4.4.

```glsl
uniform float u_scale = 1.0; // {"widget":"slider", "min":0.0, "max":10.0}
uniform int u_count = 5; // {"widget":"slider", "min":1, "max":20}
```

#### Color Picker (`"widget":"color"`)
- **Applicable types:** `vec3`, `vec4`

#### Smoothing (`"smooth":true`)
- **Optional parameter:** `smooth` (default: `false`)
- **Applicable types:** `float`
- When set to `true`, the uploaded value eases toward the slider value by 5 % of the remaining gap **per frame** (`fCurrentValue += (fValue - fCurrentValue) * 0.05f`, `src/ShaderEffect.cpp:231–234`): about 0.3 s to settle 63 % at 60 fps, slower at lower frame rates. This creates a more organic and less jarring visual transition.
- Smoothing does not make discontinuous parameters safe. Changing a value that is multiplied by `iTime` (a camera speed, say) still sweeps the scene through a huge distance while it eases. Smooth sliders that scale *amounts*, not *rates × time*.

```glsl
uniform vec3 u_color = vec3(1.0, 0.0, 0.0); // {"widget":"color"}
uniform vec4 u_color_alpha = vec4(1.0, 0.0, 0.0, 1.0); // {"widget":"color"}
```

#### 4.3 ⚠️ CRITICAL: Color Palette Widgets (`"palette":true`)

**Requires palette metadata flag for advanced color harmony functionality.**

The RaymarchVibe palette system enables professional color coordination through semantic roles and automatic gradient synchronization. This system is active when `{"palette":true}` is specified in control metadata.

##### Color Control Semantics

Controls are automatically categorized into **Primary** or **Secondary** roles (`src/ShaderParser.cpp:31–66`):

- A palette control whose name contains a **secondary keyword** (below) is Secondary.
- Otherwise the **first** palette control (in declaration order) is Primary, and every later one is Secondary by index (`paletteControlIndex`, `src/ShaderParser.cpp:374–400`).
- There is **no primary keyword**: `PrimaryColor`, `MainColor` and `BaseColor` are not detected as such. They are Primary only because they come first. A `u_mainColor` declared second becomes a Secondary.

- **Primary Controls** (Palette Generators):
  - Generate color harmonies and gradient sequences
  - Default to "Palette" mode
  - Support all 6 harmony types: Monochromatic, Complementary, Triadic, Analogous, Split-Complementary, Square

- **Secondary Controls** (Gradient Consumers):
  - Automatically sync from primary control gradients
  - Default to "Sync" mode
  - Sample at semantic positions (SecondaryColor → 0.25, AccentColor → 0.75)

##### Semantic Naming Convention

Matching is a **case-sensitive substring** test for `Secondary`, `Tertiary`, `Accent`, `Highlight` or `_secondary`, `_tertiary`, `_accent`, `_highlight`. The source comment says "case-insensitive", but the code is not: `u_SECONDARY` does not match.

| Control name contains | Role | Default Mode | Gradient Position when synced (`src/ShaderEffect.cpp:1297–1316`) |
|---------------------|------|--------------|-------------------|
| *(first palette control, no secondary keyword)* e.g. `u_primaryColor` | Primary | Palette | N/A (generates gradients) |
| `Secondary` / `_secondary` | Secondary | Sync | 0.25 |
| `Tertiary` / `_tertiary` | Secondary | Sync | 0.5 |
| `Accent` / `_accent` | Secondary | Sync | 0.75 |
| `Highlight` / `_highlight` | Secondary | Sync | 1.0 |
| `Primary` / `_primary` / `_main`, if it ended up Secondary | Secondary | Sync | 0.0 |
| anything else after the first | Secondary | Sync | evenly spaced among the sync controls |

##### Index-Based Fallback

If semantic naming is not used, the first palette control becomes **Primary** and subsequent controls become **Secondary**:

```glsl
// Control order determines role when semantic naming is not used
uniform vec3 u_paletteColor1 = vec3(1.0, 0.5, 0.0); // {"widget":"color", "palette":true} → PRIMARY
uniform vec3 u_paletteColor2 = vec3(0.5, 0.8, 1.0); // {"widget":"color", "palette":true} → SECONDARY
```

##### Palette Mode Options

Each palette-enabled color control provides three operating modes:

1. **Individual Mode (0)**:
   - Standard color picker functionality
   - Independent color selection
   - No harmony or synchronization

2. **Palette Mode (1)**:
   - **Primary only**: Generates harmonic color schemes
   - Harmony selector: Mono/Comp/Triad/Anal/SplitComp/Square
   - Gradient toggle for smooth color transitions
   - Interactive preview with clickable palette segments
   - Real-time color editing through palette clicks

3. **Sync Mode (2)**:
   - **Secondary only**: Automatically samples primary gradients
   - Semantic position sampling (based on control name)
   - Real-time updates when primary changes
   - Read-only color swatch display

##### Advanced Sync Implementation

Secondary controls automatically sample primary gradient at semantically meaningful positions:

```glsl
// Primary control generates gradient from base color through harmony
uniform vec3 u_primaryColor = vec3(1.0, 0.5, 0.0); // {"widget":"color", "palette":true}
// → Generates 10-color gradient: [base, harmony1, harmony2, ..., blendMix]

uniform vec3 u_secondaryColor = vec3(0.5, 0.8, 1.0); // {"widget":"color", "palette":true}
// → Automatically set to gradientSample(0.25) via semantic sync

uniform vec3 u_accentColor = vec3(0.8, 0.3, 0.9); // {"widget":"color", "palette":true}
// → Automatically set to gradientSample(0.75) via semantic sync
```

##### Palette-Enabled Shader Creation

**Basic Pattern**: Single primary control for simple color theming:

```glsl
#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

// Primary color control - generates palette and gradient
uniform vec3 u_primaryColor = vec3(1.0, 0.5, 0.0); // {"widget":"color", "palette":true, "label":"Primary Color"}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

    // Generate palette from primary (Monochromatic harmony by default)
    vec3 baseColor = u_primaryColor;

    // Use color in your shader logic
    vec3 color = baseColor * vec3(uv.x);

    FragColor = vec4(color, 1.0);
}
```

**Advanced Pattern**: Multi-control palette synchronization:

```glsl
#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

// Primary control - generates the master palette
uniform vec3 u_primaryColor = vec3(1.0, 0.5, 0.0); // {"widget":"color", "palette":true, "label":"Primary Color"}

// Secondary controls - sync automatically from primary gradient
uniform vec3 u_secondaryColor = vec3(0.5, 0.8, 1.0); // {"widget":"color", "palette":true, "label":"Secondary Color"}
uniform vec3 u_accentColor = vec3(0.8, 0.3, 0.9); // {"widget":"color", "palette":true, "label":"Accent Color"}

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

    // Use primary color for base elements
    vec3 baseColor = u_primaryColor;

    // Use secondary color for highlights/details
    vec3 accentColor = u_secondaryColor;

    // Use accent color for special effects
    vec3 highlightColor = u_accentColor;

    // Create coordinated color scheme
    vec3 color = baseColor * accentColor * highlightColor;

    FragColor = vec4(color, 1.0);
}
```

##### Synchronization Rules

- **Sync needs Gradient Mode.** Secondaries only follow the primary once the user ticks **Gradient Mode** on the primary. `gradientMode` defaults to `false` (`include/ShaderParser.h:47`), and `updatePaletteSync()` returns early without a gradient (`src/ShaderEffect.cpp:1271–1287`). Until then, Sync-mode secondaries keep the **shader's declared defaults**, even though the UI says "Synced to primary gradient". Choose defaults that look good on their own.
- **Once synced, your defaults are overwritten every frame.** The secondaries then become harmonies of the primary, so do not rely on a specific secondary or accent hue. Use the colours as *roles* (base, detail, highlight) and derive the rest in the shader, e.g. by hue-shifting them.
- **Single Primary**: All secondary controls sync from that primary's gradient
- **No Primary**: Secondary controls become independent (fallback to Individual mode)
- **Multiple Primaries**: Undefined behavior - only one primary should generate gradients
- **Role Assignment Priority**: Semantic naming → Index-based fallback

##### Interactive Palette Editing

When in Palette mode, the primary control provides:
- **Harmony Selector**: 6 predefined color relationships
- **Gradient Toggle**: Smooth vs discrete palette transitions
- **Color Preview**: Visual palette display with hover effects
- **Direct Editing**: Click palette segments to modify colors

##### Best Practices

1. **Use semantic naming** for role-based color assignment
2. **One primary control** per shader (multiple creates confusion)
3. **Test sync relationships** by toggling primary base color
4. **Use demo shader** (`palette_sync_demo.frag`) as reference
5. **Document color roles** in shader comments for maintenance

##### Make every palette role visible (lasting lesson from the `morphing_particles.frag` fix)

A shader can declare perfect palette uniforms and still render white or grey if the colours are washed out downstream. Rules:

1. **Brighten multiplicatively, never with a large additive constant.** `col += 1.0` clamps everything to white after tonemapping; use `col *= 1.3` (or exposure) so hue survives.
2. **Let the palette drive the base colour.** Modulate the palette colour (`u_primaryColor * (0.6 + 0.4 * pattern)`), rather than mixing it with hard-coded light colours (`vec3(0.5, 0.6, 0.7)`, white stars) that compete with it.
3. **Tint highlights with a palette role** (`HighlightColor * stars`), not pure white.
4. **One role, one visible element:** Primary = dominant hue of the scene, Secondary = complementary/material tones, Accent = glows/veins, Highlight = hot spots. Document the mapping in shader comments.
5. **Test by dragging each palette control in turn**; every one must visibly change its element. This is part of the visual sign-off (`SHADER_WORKFLOW.md`).

See `documentation/PALETTE_FEATURE.md` for comprehensive technical details and `shaders/palette_sync_demo.frag` for working examples.

#### 4.4 ⚠️ Reserved control names

Some control names are read by the app itself, not only by your shader:

| Name | What the app does with it | Source |
|---|---|---|
| `u_speed` (float) | **Scales the clock**: each frame `m_internalTime += m_deltaTime * speed`, and `iTime` *is* that internal time. The raw slider value is used; `"smooth"` does not apply to it. | `src/ShaderEffect.cpp:225–239` |
| `u_objectColor` (vec3) | When the effect's colour-cycle option is enabled, it is overwritten every frame with the cycling gradient colour | `src/ShaderEffect.cpp:241–253` |

Consequences:

- **Never multiply `iTime` by `u_speed` in the shader.** The app has already applied it, so `iTime * u_speed` makes the effective speed `u_speed²` (and the in-shader product jumps when the slider moves). Write plain `iTime` and let the app scale it.
- **Do not name an unrelated slider `u_speed`** (rotation speed, pulse speed, …). It will silently change the speed of *everything* time-driven. Use a specific name such as `u_camSpeed` or `u_rotSpeed`, which scales only what you multiply it into.
- **Declaring `u_speed` on purpose is fine.** It is the idiomatic way to give users a global animation-speed slider (e.g. `apollonian_network_improved.frag`), as long as the shader never multiplies by it.
- Without a `u_speed` control the speed is 1.0, so `iTime` advances in real seconds. Camera-surge bounds that rely on real-time decay rates (9.12) hold only in that case.


#### 4.5 Visual-pass sliders and preview sliders

- Stage-1 shaders expose **visual** controls only: quality/step count, colours (palette), fog, glow, exposure, camera speed (`u_camSpeed`, never `u_speed` for that), shape parameters.
- A **Quality** control (an `int` slider, e.g. 0 low / 1 medium / 2 high, mapped to step counts and AO/shadow taps) is recommended so Nic can hold the FPS budget (§7.2, §11.2).
- **Preview sliders** fake the music so motion can be judged without audio: e.g. `uniform float u_previewPulse = 0.0; // {"widget":"slider", "label":"Preview Pulse", "min":0.0, "max":1.0}`. They only feed the hooks block (§6). Default them to 0 so the default view is the silent look. The audio pass removes them (or keeps them at 0 as a fallback, if Nic wants).
- Audio controls (Audio Reactivity, Audio Sensitivity, Beat Threshold, Pulse Intensity, Debug View) are added in stage 3, not before.
- Do **not** add per-band sensitivity sliders (bass gain, mid gain, …). Per-band balance is set with the app's 10-band graphic EQ during tuning (`AUDIO_REACTIVITY.md` §T).

## 5. Node-Based Architecture Constraints

### 5.1 Input Channels (`iChannel0`–`iChannel3`)

Each `iChannelN` is bound to the output texture of the node wired to input pin N of this node. With nothing connected the app binds a 1×1 black dummy texture (`ShaderEffect::InitializeDummyTexture`, `src/ShaderEffect.cpp`). Sampling `iChannel0` therefore reads an **upstream node's output, not this node's own previous frame** — the app has no ping-pong feedback buffer, and a cyclic graph is not rendered as feedback either — nodes are rendered in topological order and a cycle is reported as a graph error (`src/main.cpp:2210`). The persist/decay idiom below therefore blends against the input node's output (black if nothing is wired there), not against the last frame.

```glsl
void main() {
    // Sample the node wired to input pin 0
    vec4 inputNode = texture(iChannel0, uv);

    // Apply temporal effects (decay, blending)
    vec4 newContent = computeContent(uv);

    // Combine the input with the new content
    FragColor = mix(inputNode * 0.9, newContent, 0.1);
}
```

> **Open question (owner decision):** whether a node gets a real self-feedback buffer, or whether self-feedback stays documented as unsupported (tracked as H4). Until that is decided, do not write shaders that depend on reading their own previous frame.

### 5.2 Input Pin Conventions
Shaders may receive inputs from other nodes via `iChannel1-3` uniforms. Each input pin should be documented in the shader comments.

### 5.3 Metalness/Roughness Outputs (Future)
Advanced shaders should output material properties as `.a` channel:
- **RGB:** Final color
- **A:** Material properties (metalness, roughness, etc.)

## 6. The `AUDIO_HOOK` convention

Every element that may later react to music gets **one named float**, declared in a single **hooks block** near the top of `main()` (or of `render()` if that is where per-frame values are set). The visual pass sets each to `0.0` (or to a preview slider); the audio pass only replaces the right-hand side. No restructuring.

```glsl
// ======================= AUDIO HOOKS BLOCK =============================
// 0.0 = approved silent look. Preview Pulse drives them for visual tests.
float pv = clamp(u_previewPulse, 0.0, 1.0);
float bubblePulse = pv;    // AUDIO_HOOK: bubblePulse - bubble radius swell, 0..1 (smooth)
float ribbonTwist = 0.0;   // AUDIO_HOOK: ribbonTwist - extra ribbon twist rate, 0..1 (slow)
float flash       = pv*pv; // AUDIO_HOOK: flash - one-shot screen flash, 0..1 (snappy)
// =======================================================================
```

Rules:

1. **Marker format, exactly:** `// AUDIO_HOOK: <name> - <what it drives, sensible range>`, on the line that declares the variable. Optionally add the intended feel in brackets. Tools and the LLM search for `AUDIO_HOOK:`.
2. **One block, one place.** All hooks are declared together, after the uniforms are read and before any per-frame globals that depend on them. Nothing else in the shader reads audio.
3. **0 means silent and approved.** Each hook's neutral value is `0.0`, and the scene at all hooks = 0 is the signed-off look. Design ranges as 0..1 (0..2 allowed when the hook is a gain that may overshoot).
4. **Apply each hook where it belongs, bounded.** Clamp it where used, e.g. `radius * (1.0 + 0.15 * clamp(bubblePulse, 0.0, 1.0))`. Its maximum must stay camera-safe (§9.12) and must not blow out exposure.
5. **Hooks used inside `map()`** are copied once into globals (`g_bubblePulse = bubblePulse;`). Never compute audio inside `map()` (§7.2, §7.5 #5).
6. **Never drive the camera path from a hook** except a deliberately designed, non-reversing surge (§9.12).
7. **Name by element and effect** (`ribGlow`, `towerSway`, `starSparkle`), not by band (`bassAmount`). The band is chosen in stage 2.
8. Hooks the plan leaves unused stay at `0.0` with their marker, so they can be wired later.

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
5. **Hoist per-frame work out of `map()`**: audio conditioning, beat logic, palette hue shifts and warp amounts are the same for the whole frame. Compute them once in `main()` into globals.
6. **Colour once, not per step**: accumulate scalars in the march loop (glow weight, weighted depth) and turn them into colour after the loop.

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

### 7.5 Raymarching & audio gotchas (learned the hard way)

Each item below caused a real, visible bug in a shader that compiled cleanly.

1. **Treat step-exhausted rays as hits.** A ray that runs out of `u_maxSteps` while creeping along a surface otherwise falls through to the background, giving dark or sky-coloured **cracks** along folds and grazing walls. After the loop: `if (!hit && t < MAX_DIST) { hit = true; mat = map(ro + rd * t).y; }`.
2. **Weight per-step glow by step length.** `glow += f(d)` per iteration grows with the *number of steps*, and rays that graze geometry take hundreds of tiny steps. The result is a uniform milky **washout** near every surface. Integrate along the ray instead, `glow += f(d) * min(stepLen, cap)`, with a tight falloff such as `exp(-d * k)`, and scale the sum by one constant (normalise). Compare 9.8.1.
3. **Guard `atan(0, 0)`.** It is undefined in GLSL and some drivers return NaN, which ACES turns into black pixels. That matters on axes you fold or twist around: `float a = (r > 1e-6) ? atan(p.y, p.x) : 0.0;`, or offset the second argument by `1e-9`.
4. **Distance-scaled normal epsilon.** A fixed `e = 0.001` is noise far away and too coarse up close. Use `e = 0.0005 * clamp(t, 1.0, 20.0)`. When `map()` scales its distance by a warp step factor, normalising the gradient removes that factor anyway.
5. **Compute audio once per pixel.** Do the band conditioning (3.3) and beat logic (3.4) in `main()` and store them in globals. Calling them from `map()` repeats them for every march step (hundreds of times per pixel).
6. **Don't square raw bands.** Treble is ~0.007 (3.2), so `pow(treble, 2.0)` is 0.00005 and the effect never shows. Condition to 0..1 first, then shape.
7. **Warps stretch the SDF.** Twists, folds and displacements raise its Lipschitz constant: multiply the distance by `1/L`, or the march oversteps and tears. For a twist of `k` rad/unit at radius `r`, `L ≈ (S + sqrt(S² + 4)) / 2` with `S = r·k` (9.12).
8. **Loop bounds must be constant.** Use `for (int i = 0; i < STEP_CAP; i++) { if (i >= u_maxSteps) break; … }`, so an int slider can still set the step count.
9. **Reserved words.** `half`, `input`, `output`, `filter`, `active`, `common`, `partition` and similar are reserved in GLSL 3.30 and fail to compile. `glslangValidator` catches them (8.3).
10. **Don't put the camera speed on `u_speed`** (4.4).


> Audio-specific gotchas 5 and 6 and the measured levels behind them are explained in `AUDIO_REACTIVITY.md` 3.2–3.3.

## 8. Testing and Validation

### 8.1 Shader Verification
Shaders should be tested for:
1. **Compilation success** in ShaderEffect
2. **UI control generation** in ShaderParser
3. **Input channels** resolve to a bound texture (the app binds a 1x1 black dummy when a pin is unconnected)
4. **Performance benchmark** (<10ms per effect)


> 8.2 (Milk-Converter validation) was removed with MilkDrop support (2026-10-24).

### 8.3 Offline validation workflow

Before handing a shader over:

1. **Compile:** `glslangValidator -S frag my_shader.frag`. It catches reserved words, type errors, missing `.x` on `vec2` distances, and non-constant loop bounds.
2. **Render previews with realistic audio.** Feed real band values (the 3.2 table, or bands computed from actual tracks with the `ProcessAudio` maths), **not** idealised `vec4(0.8)`. Check silence, a quiet track, a loud master with pinned bass, and a short sequence around a beat to see pulses travel.
3. **Debug View:** confirm that the conditioned meters move and the effects respond.
4. **Camera clearance** for fly-through shaders: evaluate `map()` (unscaled) at and around the camera position over a few hundred time samples, at default **and** extreme slider settings, with silent and loud bands. The minimum must stay clearly above 0 (9.12).
5. **Performance:** time a 1080p frame and compare against a known-good shader on the same machine. Warm up first, since the first frame includes shader compilation.


### 8.4 Debugging in the app (from the former GEMINI.md)

- **Console window:** shader compile errors, file-loading problems and other messages appear here; compile errors are also marked on the editor lines. A failed live recompile keeps the **last good shader rendering**, so an unchanged picture after an edit usually means the edit did not compile: check the Console.
- **Node Properties panel:** selecting a node shows all generated controls; use it to check that every slider/colour was parsed from your JSON comments and has the intended range.
- **Isolate the chain:** wire a node from the middle of a graph directly to the `Scene Output` node to see its output alone.
- **Defaults come from the shader source.** ImGui `.ini` saving is disabled (`io.IniFilename = nullptr`), so the app always starts with the defaults declared in the `.frag`; editing a default and reloading is the reliable way to change the starting look. (Window layout is not saved either.)

### 9. Rendering techniques

The advanced technique library (former 9.1–9.11 and 9.13) is in `SHADER_TECHNIQUES.md`. Camera safety stays here because it is a rule, not a technique.

### 9.12 Camera Safety in Fly-Throughs

A camera that clips into geometry for even one frame breaks the spell. Make clearance a property of the construction, not of the tuning:

- **Warp and fold around the camera's own axis, preserving radius.** Express the space in polar coordinates about the axis the camera travels along. Twist by an angle (`a += k * (p.z - camZ)`), apply kaleidoscopic folds to the angle, and blend folds in Cartesian space (`mix(q, qFolded, w)` never moves a point farther from the axis). Points then keep their distance from the eye, so any disc of radius `CLEAR_R` that is empty in the unwarped scene stays empty. Twist relative to `camZ` so the geometry around the camera is always the untwisted original. Breathing/scale warps may change the radius only by a bounded factor, which shrinks the clear radius by the same factor.
- **Or carve a tunnel along the camera path:** `d = max(d, -(tunnelRadius - distanceToPath(p)))`. This suits fractals, where nothing is guaranteed clear.
- **Base the path on unmodulated parameters.** The camera path must not depend on audio-modulated fractal parameters or warp amounts. Otherwise geometry moves *through* the camera on a beat.
- **Bound every displacement:** drips, ripples and surface noise must sum to less than the clearance margin at the most extreme slider settings, after clamping (e.g. `breath = min(…, 0.18)`).
- **Audio surges that cannot reverse.** For a forward position `z = t * glide + A * S(t)`, the envelope bound in 3.2 limits how fast `S` can fall (≈ 1 level unit/s with the 3.3 windows). With `A < glide`, the camera therefore never moves backwards. Use a **fixed-weight** signal such as `S = 0.5 * (Lb + LmUnshifted)`. Do **not** use the stuck-bass crossfade `mix(Lb, Lm, bassPinned)`: it jumps when bass pins, and the camera jerks backwards. Simulation of real tracks caught that.
- **Verify numerically** (8.3, step 4).

### 10. Templates & starter code

| File | Stage | What it is |
|---|---|---|
| `shaders/templates/visual_starter.frag` | 1 (visual pass) | Compiling tunnel scene with **no audio**: palette, Quality, Fog/Glow/Exposure, Camera Speed, a `Preview Pulse` slider, and a hooks block with five example `AUDIO_HOOK` markers (`ribSwell`, `glowBoost`, `tunnelTwist`, `flash`, `sparkle`). Camera-safe twist, step-length glow, exhausted rays as hits, `atan` guard. |
| `shaders/templates/audio_reactive_starter.frag` | 3 (audio pass reference) | The same kind of scene with the standard audio helper, stateless beats, Debug View meters and the Trip Intensity master, wired **through** a hooks block. See `AUDIO_REACTIVITY.md` 10.3. |

The structural outline (former 10.1) and the progressive enhancement path (former 10.2) are in `SHADER_TECHNIQUES.md`.

### 11. Quality Assurance & Testing

#### 11.1 Visual Quality Checklist
- [ ] Multiple depth layers visible
- [ ] Organic, non-linear motion patterns
- [ ] Sophisticated color palette (no pure primaries)
- [ ] Scene evolves over 30+ seconds
- [ ] Professional post-processing applied
- [ ] Atmospheric depth present
- [ ] Materials show complexity (iridescence, etc.)
- [ ] Performance > 30fps on target hardware
- [ ] Passes "stop-and-stare" test

- [ ] Surface detail reaches the quality reference: `SHADER_TECHNIQUES.md` 9.14 (cathedral v2 `ornament()`, screenshot in `documentation/inspiration/`)
- [ ] Looks finished with every `AUDIO_HOOK` at 0 (the silent look), and hooks are present and named for each element that could react

> The audio item "Audio affects 3+ parameters simultaneously" moved to the audio-pass checks in `AUDIO_REACTIVITY.md` 11.3. Nic's full visual sign-off list is in `SHADER_WORKFLOW.md`.

#### 11.2 Performance Benchmarks
- **Simple scenes**: 60+ fps, <5ms per frame
- **Medium complexity**: 45+ fps, <10ms per frame  
- **Complex scenes**: 30+ fps, <15ms per frame
- **Maximum complexity**: 20+ fps, <20ms per frame


**FPS budget.** Target the "Medium complexity" row (≥ 45 fps, < 10 ms per effect, matching 8.1) at 1080p on Nic's machine with Quality at its default; Quality = high may drop to the "Complex" row. Audio wiring should cost almost nothing (the helper runs once per pixel); if the audio pass lowers FPS noticeably, something is being computed inside `map()`.

> 11.3 (Audio Reactivity Testing) moved to `AUDIO_REACTIVITY.md`. Section 12 (Inspiration Analysis) moved to `SHADER_TECHNIQUES.md`.

## 13. Change Log

- **v3.0** (Current, 2026-10-24): Four-stage workflow and document split
  - Added **0** (workflow overview), **4.5** (visual-pass and preview sliders), **6 The `AUDIO_HOOK` convention**, the **FPS budget** note in 11.2, and a silent-look item in 11.1
  - Moved audio material (3.1–3.4, 10.3, 11.3) to `AUDIO_REACTIVITY.md`, adding effect recipes, drop detection and EQ tuning
  - Moved the technique library (9.1–9.11, 9.13, 10.1, 10.2, 12) to `SHADER_TECHNIQUES.md`
  - Moved Milk-Converter material (6, 8.2) to `MILK_CONVERTER.md`; section 6 of this file is now the hook convention
  - Added `SHADER_WORKFLOW.md` and `shaders/templates/visual_starter.frag`; `audio_reactive_starter.frag` now routes audio through a hooks block
  - v2.4 section numbers kept in all moved headings
  - Added 4.3 "Make every palette role visible" (from `MORPHING_PARTICLES_PALETTE_FIX.md`) and 8.4 "Debugging in the app" (from `GEMINI.md` 3.5/3.6)
  - Added `SHADER_TECHNIQUES.md` **9.14 Surface Ornament** (quality reference) and its checklist line in 11.1

- **v2.4** (Current, 2026-10-09): Real-world audio, stateless beats, gotchas, camera safety, psychedelic toolkit
  - Added **3.2 Real-world audio levels**: measured band ranges (bass pinned at 1.0 on loud masters, mids ~0.01–0.35, treble ~0.0005–0.04) with the source evidence (un-windowed 1024 FFT, 4-bin bass mean, fixed 1/32 norm, native-rate file decoding, envelope decay bound)
  - Added **3.3 Audio helper block** (log-window rescaling, Audio Sensitivity, stuck-bass fallback, Debug View meters) and **3.4 Stateless beat-synced effects** (pulses/shockwaves from envelope decay, freshness-gated snaps)
  - Corrected **3** and **3.1** wording that implied the bands are comfortably 0..1
  - Corrected **4.2**: `step` is ignored by float/int sliders (used only by vec drag widgets), default min/max, `smooth` rate, and replaced the `u_speed` slider example
  - Corrected **4.3**: there is no primary keyword (Primary = first palette control without a secondary keyword); matching is a case-sensitive substring test; sync needs Gradient Mode (off by default), and synced values overwrite shader defaults; full gradient-position table
  - Added **4.4 Reserved control names** (`u_speed` scales `iTime`, `ShaderEffect.cpp:225–239`; `u_objectColor`)
  - Added **7.2** items, **7.5 Raymarching & audio gotchas**, **8.3 Offline validation workflow**
  - Fixed **9.4.1** (audio motion now takes conditioned levels) and **9.8.1** (`map().x`; step-length-weighted glow)
  - Added **9.12 Camera Safety in Fly-Throughs** and **9.13 Trip Intensity Master Slider & Psychedelic Toolkit**
  - Added **10.3** pointing to the new `shaders/templates/audio_reactive_starter.frag` and the worked example shaders; extended **11.3** audio testing

- **v2.3** (Previous): Audio band contract
  - Added **3.1 Audio bands**: the frozen contract for `iAudioBands` / `iAudioBandsAtt` (bin edges, DC bin excluded, `BAND_REFERENCE = 32`, `.w` as the mean of the three normalised bands, raw vs enveloped)
  - Rewrote **3 Built-in Uniforms**: the uniforms are not all mandatory, `iFps` and `iProgress` do not exist, `iFrame` is an `int` uploaded in Shadertoy mode only, `iChannel0` is an input node's output and not a feedback buffer
  - Corrected the input-channel and temporal-continuity sections (5.1, 9.4.2, 8.1) away from the missing feedback buffer
  - Retuned the 9.4.1 audio-reactive motion example for the normalised bands and added the normalisation note to the 10.1 template
  - Extended the Milk-Converter mapping (6.1) with the band edges and the overall-level rows

- **v2.2** (Previous): Advanced Color Palette System Documentation
  - Added comprehensive **Color Palette Widgets** section (4.3) with `{"palette":true}` semantics
  - Documented **Primary vs Secondary** color control roles and auto-detection
  - Implemented **semantic naming conventions** (PrimaryColor, SecondaryColor, AccentColor, etc.)
  - Added **palette mode explanations**: Individual, Palette, and Sync modes
  - Provided **implementation examples** for single and multi-control palettes
  - Included **synchronization rules** and best practices
  - Reference to `palette_sync_demo.frag` and `documentation/PALETTE_FEATURE.md`

- **v2.1** (Previous): Enhanced mathematical sophistication - Inspiration shaders 11-20 integration
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
