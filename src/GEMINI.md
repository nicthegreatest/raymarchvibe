# GEMINI Agent Guide: `src` Directory (RaymarchVibe)

This document provides guidance for AI agents working with the C++ source files (`.cpp`, `.h`) located in the `src` directory of the RaymarchVibe project. For general project overview, see `../GEMINI.md`.

## 1. Key Classes and Responsibilities

### 1.1. `Effect.h` (Interface)
*   **Role**: Abstract base class defining the interface for all visual or processing effects in the system.
*   **Key Methods**:
    *   `virtual void Load() = 0;`: Load resources (shaders, textures).
    *   `virtual void Update(float currentTime) = 0;`: Update internal state based on time.
    *   `virtual void Render() = 0;`: Render the effect, typically to an internal Framebuffer Object (FBO).
    *   `virtual void RenderUI() = 0;`: Render ImGui controls specific to this effect's parameters.
    *   `virtual GLuint GetOutputTexture() const = 0;`: Get the OpenGL texture ID of the effect's output (e.g., its FBO color attachment).
    *   `virtual void SetInputEffect(int pinIndex, Effect* inputEffect)`: Connect another effect as an input.
    *   `virtual nlohmann::json Serialize() const = 0;`: Serialize effect state to JSON.
    *   `virtual void Deserialize(const nlohmann::json& data) = 0;`: Deserialize effect state from JSON.
*   **Notes**: Effects have a unique `id` and `name`, `startTime`, `endTime` for timeline management.

### 1.2. `ShaderEffect.h` / `ShaderEffect.cpp` (Concrete Implementation)
*   **Role**: Implements the `Effect` interface for GLSL fragment shader-based effects. This is the most common effect type.
*   **Key Features**:
    *   **Shader Management**: Loads GLSL fragment shaders from file or source string. Uses a fixed passthrough vertex shader (`shaders/passthrough.vert`).
    *   **Compilation & Linking**: Handles OpenGL shader compilation and program linking. Stores compile logs.
    *   **Uniform Handling**:
        *   Standard uniforms: `iResolution`, `iTime`, `iMouse`, `iChannel0` (for texture input from another effect), `iTimeDelta`, `iFrame`.
        *   Custom uniforms defined in shaders can be made controllable via `//#control` metadata (see `ShaderParser.h`).
        *   `SetUniform*` methods are used internally based on parsed controls or direct calls.
    *   **FBO Management**: Creates and manages its own Framebuffer Object (FBO) to render into. `GetOutputTexture()` returns the FBO's color attachment texture ID. `ResizeFrameBuffer()` handles resolution changes.
    *   **Parameter UI**: `RenderUI()` uses data from `ShaderParser` (parsed `//#control` metadata) to automatically generate ImGui widgets for tweaking shader uniforms.
    *   **Serialization**: Saves/loads shader path or source, uniform values, and other state.
*   **Working with `ShaderEffect`**:
    *   To add a new controllable uniform:
        1.  Declare the uniform in the GLSL shader.
        2.  Add `//#control` metadata comment above it (see `shaders/GEMINI.md`).
        3.  The `ShaderParser` and `ShaderEffect::RenderUI` should automatically pick it up. If not, `ShaderParser` logic may need extension.
    *   To add more texture inputs (e.g., `iChannel1`):
        1.  Add new `GLint m_iChannel1SamplerLoc` member.
        2.  Update `FetchUniformLocations()` to get its location.
        3.  Update `Render()` to bind the texture from `m_inputs[1]` to this sampler unit.
        4.  Update `GetInputPinCount()` and `SetInputEffect()` to handle more inputs.
        5.  Update `Effect::Serialize/Deserialize` if input connections need to be saved/loaded.

### 1.3. `ShaderParser.h` / `ShaderParser.cpp`
*   **Role**: Parses GLSL shader source code for metadata comments that define controllable parameters.
*   **Key Metadata**:
    *   `//#control <type> <name> "Label" {options}`: Defines a UI control for a uniform.
        *   Example: `//#control float u_speed "Speed" {"default":1.0, "min":0.0, "max":10.0}`
    *   `//#define_control <name> [optional_default_value]`: Allows toggling preprocessor defines.
    *   `//#const_control <type> <name> ...`: Allows modifying values of global `const` variables.
*   **Workflow**:
    1.  `ShaderEffect::ApplyShaderCode()` calls `ParseShaderControls()`.
    2.  `ParseShaderControls()` uses `ShaderParser` instance methods (`ScanAndPrepare...Controls()`) to find metadata.
    3.  `ShaderEffect::RenderUI()` then uses the parsed control structures (e.g., `m_shadertoyUniformControls`, `m_defineControls`) to generate ImGui widgets.
*   **Extending**: To support new `//#control` options or types, modify the parsing logic within `ShaderParser.cpp` and the UI generation in `ShaderEffect::RenderUI()`.

### 1.4. `Renderer.h` / `Renderer.cpp`
*   **Role**: Handles basic OpenGL rendering tasks, specifically rendering a fullscreen textured quad.
*   **Key Methods**:
    *   `Init()`: Sets up VAO/VBO for the quad and compiles its internal passthrough shader (`shaders/texture.vert`, `shaders/texture.frag`).
    *   `RenderFullscreenTexture(GLuint textureID)`: Renders the given texture ID to the screen using its internal shader. This is used by `main.cpp` to display the final output effect.
    *   `static RenderQuad()`: Renders a quad using the currently bound shader and VAO. Used by `ShaderEffect` to render into its FBO.
*   **Notes**: Relatively self-contained. Unlikely to need frequent changes unless the core rendering approach for final output changes.

### 1.5. `main.cpp`
*   **Role**: Main application entry point, event loop, ImGui setup, window management, scene management.
*   **Key Sections**:
    *   Global variables (`g_scene`, `g_editor`, `g_timelineState`, window visibility flags).
    *   Helper functions (e.g., `LoadFileContent`, `ParseGlslErrorLog`).
    *   `RenderMenuBar()`, `RenderShaderEditorWindow()`, `RenderEffectPropertiesWindow()`, `RenderTimelineWindow()`, `RenderNodeEditorWindow()`, `RenderConsoleWindow()`, etc.: Each responsible for a specific UI panel.
    *   `main()` function:
        *   Initialization (GLFW, GLAD, ImGui, ImNodes, AudioSystem, `g_renderer`, TextEditor language).
        *   Creation of initial "Passthrough" effect.
        *   Main loop (`while(!glfwWindowShouldClose(window))`):
            *   Time calculation (`deltaTime`).
            *   Timeline state updates (`g_timelineState.currentTime_seconds`).
            *   Input processing (`processInput`).
            *   Determining active effects and render order (`GetRenderOrder`).
            *   Updating and Rendering effects in `renderQueue` (each `Effect::Render()` draws to its FBO).
            *   Unbinding FBO (`glBindFramebuffer(GL_FRAMEBUFFER, 0)`).
            *   ImGui frame setup (`ImGui_ImplOpenGL3_NewFrame`, etc.).
            *   Docking setup (currently problematic/commented out).
            *   Rendering UI panels by calling `RenderXXXWindow()` functions based on visibility flags.
            *   Rendering final output: `g_renderer.RenderFullscreenTexture()` with the output of the `finalOutputEffect`.
            *   Swapping buffers and polling events.
    *   Event callbacks (`framebuffer_size_callback`, `mouse_cursor_position_callback`, etc.).
    *   Scene I/O (`SaveScene`, `LoadScene`).
*   **Adding New UI Panels**:
    1.  Declare a new `static bool g_showMyNewWindow = false;`.
    2.  Create `void RenderMyNewWindow() { if (!g_showMyNewWindow) return; ImGui::Begin("My New Window", &g_showMyNewWindow); /* ... UI ... */ ImGui::End(); }`.
    3.  Add a `ImGui::MenuItem("My New Window", nullptr, &g_showMyNewWindow);` to `RenderMenuBar()`.
    4.  Call `if (g_showMyNewWindow) RenderMyNewWindow();` in the main loop's UI rendering section.

### 1.6. `NodeTemplates.h` / `NodeTemplates.cpp`
*   **Role**: Provides factory functions to create pre-configured `ShaderEffect` instances for use as node templates.
*   **Structure**:
    *   Namespace `RaymarchVibe::NodeTemplates`.
    *   Functions like `std::unique_ptr<Effect> CreatePlasmaBasicEffect(...)`.
    *   Each function:
        *   Creates `std::make_unique<ShaderEffect>(shader_path, width, height)`.
        *   Sets `effect->name`.
        *   Returns the `unique_ptr`.
*   **Adding New Templates**:
    1.  Create the new `.frag` shader in `shaders/templates/`.
    2.  Declare a new factory function in `NodeTemplates.h`.
    3.  Implement it in `NodeTemplates.cpp`.
    4.  Add a `MenuItem` in `RenderNodeEditorWindow()` in `main.cpp` to call this factory.
    5.  The calling code in `main.cpp` is responsible for adding the effect to `g_scene` and calling `effect->Load()`.

### 1.7. `AudioSystem.h` / `AudioSystem.cpp`
*   **Role**: Manages audio input (microphone, file) and processing (FFT, amplitude) using the `miniaudio` library.
*   **Key Features**: Device enumeration, audio capture, WAV file loading, amplitude calculation.
*   **Integration**: `main.cpp` initializes it and polls `GetCurrentAmplitude()` to pass to shaders (e.g., `iAudioAmp` uniform). `RenderAudioReactivityWindow()` provides UI.

## 2. Common Patterns & Tips

*   **State Management**: Global static variables in `main.cpp` hold much of the application state (`g_scene`, `g_selectedEffect`, UI window visibility flags). Scene serialization aims to capture this.
*   **Error Logging**: Use `g_consoleLog += "...\n";` for user-visible messages in the Console window. Use `std::cerr` for developer-level debug messages.
*   **ImGui IDs**: Use `##` to hide labels while providing unique IDs (e.g., `ImGui::Button("Go##GoToLine")`). Use `PushID()`/`PopID()` for uniqueness in loops.
*   **Shader Compilation**: Triggered by `Effect::Load()`. For `ShaderEffect`, this involves `ApplyShaderCode()` -> `CompileAndLinkShader()`. Errors are stored in `m_compileErrorLog` and displayed in the editor via `ParseGlslErrorLog` and `SetErrorMarkers`.
*   **File Dialogs**: `ImGuiFileDialog` is used. Standard pattern: `ImGuiFileDialog::Instance()->OpenDialog(...)`, then in the main loop `if (ImGuiFileDialog::Instance()->Display(...)) { if (IsOk()) { ... } Close(); }`.

## 3. Debugging Tips

*   **Console Window**: Check for error messages and diagnostic output from `g_consoleLog`.
*   **`checkGLError()`**: This function is called after significant OpenGL operations in `main.cpp`. Check `std::cerr` output for GL errors.
*   **Shader Compilation Errors**: `ShaderEffect::GetCompileErrorLog()` provides detailed logs. These are displayed in the Console and as error markers in the Shader Editor.
*   **ImGui Demo Window**: If unsure about ImGui widget usage, `ImGui::ShowDemoWindow()` is an invaluable resource.
*   **Node Connections**: If nodes aren't connecting or data isn't flowing, check:
    *   `Effect::SetInputEffect()` calls.
    *   `ShaderEffect::m_inputs` vector and `m_iChannelN_loc` uniform locations.
    *   `ShaderEffect::Render()` texture binding for `iChannelN`.
    *   `GetRenderOrder()` in `main.cpp` to ensure correct topological sort of effects.

This guide should assist in navigating and modifying the `src` directory code. Always refer to specific header files for detailed API contracts.
