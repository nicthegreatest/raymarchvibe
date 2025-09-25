# How to Create New Shader Templates

This guide provides instructions for creating new fragment shaders that are fully compatible with the RaymarchVibe node editor, including dynamically generated UI controls for parameters.

## Part 1: Creating the Fragment Shader (`.frag`) File

### 1. File Location

New shader templates should be placed in the `shaders/templates/` directory. You can create subdirectories for organization (e.g., `shaders/templates/post_processing/`).

### 2. Basic Shader Boilerplate

All new shaders should start with a basic structure. At a minimum, you need:

```glsl
#version 330 core
out vec4 FragColor;

// Standard uniforms provided by the application
uniform vec2 iResolution;
uniform float iTime;

// For shaders that process input from another node
uniform sampler2D iChannel0;
uniform bool iChannel0_active;

// For audio reactivity
uniform float iAudioAmp;

void main() {
    // Your shader logic here
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    vec3 col = vec3(uv.x, uv.y, 0.5);

    FragColor = vec4(col, 1.0);
}
```

### 3. Defining Adjustable UI Parameters

This is the most critical step. To create a UI control (like a slider or color picker) for a shader variable, you must follow this exact syntax:

**`uniform <type> <name> = <default_value>; // { "key": "value", ... }`**

- The `uniform` declaration, default value, and the `//` comment with the JSON metadata **must be on the same line.**
- The JSON metadata determines the UI control's properties.

#### Supported JSON Keys:
- `label`: (string) The display name for the parameter in the UI. **Required.**
- `default`: (float, int, or array of floats) The initial value of the uniform.
- `min`: (float or int) The minimum value for sliders.
- `max`: (float or int) The maximum value for sliders.
- `widget`: (string) Use `"widget":"color"` for `vec3` or `vec4` to create a color picker.

---

#### **Example 1: Float Slider**
Creates a float uniform `u_brightness` with a UI slider labeled "Brightness".

```glsl
uniform float u_brightness = 0.0; // {"label":"Brightness", "default":0.0, "min":-1.0, "max":1.0}
```

#### **Example 2: Color Picker**
Creates a `vec3` uniform `u_color` with a UI color picker labeled "Object Color".

```glsl
uniform vec3 u_color = vec3(1.0, 0.5, 0.2); // {"widget":"color", "label":"Object Color"}
```

#### **Example 3: Boolean Toggle**
Creates a boolean uniform `u_invert` with a UI checkbox labeled "Invert".

```glsl
uniform bool u_invert = false; // {"label":"Invert"}
```

---

## Part 2: Integrating the Shader into the Node Editor

Creating the `.frag` file is not enough. You must register it with the application to make it appear in the "Add Node" menu. This involves editing 3 C++ files.

### Step 1: Add a Factory Function in `NodeTemplates.h`

Open `include/NodeTemplates.h` and add a new function declaration for your effect.

```cpp
// In include/NodeTemplates.h

// ... (other function declarations)

// Add your new function at the end of the list
std::unique_ptr<Effect> CreateMyNewEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);

} // namespace NodeTemplates
} // namespace RaymarchVibe
```

### Step 2: Implement the Factory Function in `NodeTemplates.cpp`

Open `src/NodeTemplates.cpp` and add the implementation for the function you just declared. This tells the application where to find your shader file.

```cpp
// In src/NodeTemplates.cpp

// ... (other function implementations)

// Add your new function at the end of the file
std::unique_ptr<Effect> CreateMyNewEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/my_new_shader.frag", // <-- IMPORTANT: Path to your shader
        initial_width,
        initial_height
    );
    effect->name = "My New Effect"; // The name that appears on the node
    return effect;
}

} // namespace NodeTemplates
} // namespace RaymarchVibe
```

### Step 3: Add the Node to the UI Menu in `main.cpp`

Open `src/main.cpp` and find the `RenderNodeEditorWindow` function. Inside, locate the `"AddNodeContextMenu"` popup and add a new `ImGui::MenuItem` that calls your factory function.

```cpp
// In src/main.cpp, inside RenderNodeEditorWindow()

// ... inside if (ImGui::BeginPopup("AddNodeContextMenu"))
// ... inside if (ImGui::BeginMenu("Add Effect"))

// Decide which category your shader belongs in (e.g., "Generators", "Filters")
if (ImGui::BeginMenu("Generators")) {
    // ... (other menu items)

    // Add your new menu item
    if (ImGui::MenuItem("My New Effect")) { // UI Label for the menu
        auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateMyNewEffect();
        if (newEffectUniquePtr) {
            Effect* newEffectRawPtr = newEffectUniquePtr.get();
            g_scene.push_back(std::move(newEffectUniquePtr));
            newEffectRawPtr->Load();
            g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
            g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
        }
    }
    ImGui::EndMenu();
}
// ...
```

## Appendix: Common Pitfalls & Deprecated Styles

You may see other parameter styles in older shader files. **Avoid them.** They are part of a broken or deprecated system.

- **DO NOT USE:** `//#control ...` on a separate line from the uniform.
- **DO NOT USE:** `// #control ...` on a separate line from the uniform.

**Always use the single-line `uniform ... // {json}` format described in Part 1 for any new shader templates.** This is the only method guaranteed to work correctly.

- **Shadertoy Shaders and `#version`**: When creating a shader that uses the Shadertoy `mainImage(...)` entry point, **do not** include a `#version` directive at the top of the file. The application's Shadertoy compatibility layer automatically adds one during compilation. Including it in your file will result in a duplicate `#version` error.
- **Shadertoy Standard Uniforms (`iTime`, `iResolution`, etc.)**: Similar to the `#version` directive, standard Shadertoy uniforms like `iTime`, `iResolution`, `iMouse`, `iFrame`, and `iTimeDelta` are automatically injected by the application's Shadertoy compatibility layer. **Do not** declare these uniforms explicitly in your Shadertoy-style shaders, as this will lead to redeclaration errors during compilation.
