# GEMINI Agent Project Guide: RaymarchVibe

## 1. Project Overview

**RaymarchVibe** is a real-time shader exploration and demoscene creation tool. Its primary purpose is to allow users to write, test, and combine GLSL shaders (primarily fragment shaders) to create visual effects. It features a node-based interface for chaining effects, a timeline for sequencing, and a shader editor with live updates.

## 2. Core Architectural Components

*   **`main.cpp`**: Contains the main application loop, GLFW window setup, ImGui initialization, and orchestration of UI rendering and the main rendering pipeline. Most UI window logic (`RenderMenuBar`, `RenderShaderEditorWindow`, etc.) resides here. Manages the global `g_scene` of effects.
*   **Effect System (`Effect.h`, `ShaderEffect.h`/`.cpp`)**:
    *   `Effect`: Abstract base class for all visual effects. Defines a common interface (`Load`, `Update`, `Render`, `RenderUI`, `Serialize`, `Deserialize`).
    *   `ShaderEffect`: Concrete implementation for GLSL fragment shader-based effects. Handles shader compilation (vertex shader is fixed `passthrough.vert`), uniform management, FBO creation and rendering, and parsing shader metadata for controls.
*   **`ShaderParser.h`/`.cpp`**: Responsible for parsing GLSL shader source code to find metadata comments (e.g., `//#control`) that define tweakable parameters for the UI.
*   **`Renderer.h`/`.cpp`**: Handles low-level OpenGL rendering tasks, primarily rendering a fullscreen quad with a given texture (used to display the final output of the effect chain).
*   **`NodeTemplates.h`/`.cpp`**: Contains factory functions for creating pre-configured `ShaderEffect` instances (node templates) that can be added to the scene.
*   **UI Widgets & Libraries**:
    *   **ImGui (`imgui*.cpp`, `imgui*.h`)**: Core immediate mode GUI library.
    *   **ImNodes (`imnodes.h`/`.cpp`)**: Used for the node editor interface.
    *   **ImGuiColorTextEdit (`TextEditor.h`/`.cpp`)**: Advanced text editor widget used for the shader editor.
    *   **ImGuiFileDialog (`ImGuiFileDialog.h`/`.cpp`)**: Used for file open/save dialogs.
    *   **ImGuiSimpleTimeline (`ImGuiSimpleTimeline.h`)**: Custom widget for timeline display and interaction.
*   **Scene Management**:
    *   `g_scene` (in `main.cpp`): A `std::vector<std::unique_ptr<Effect>>` holding all active effects.
    *   `g_timelineState` (in `main.cpp`, type `TimelineState` from `Timeline.h`): Manages timeline playback state (current time, duration, enabled status, zoom/scroll).
    *   Scene Save/Load: Uses `nlohmann/json` to serialize `g_timelineState` and the `Effect` objects in `g_scene`.

## 3. Build Process

*   **CMake (`CMakeLists.txt`)**: Used as the build system generator.
*   **Dependencies (fetched via `FetchContent` or included as submodules/vendor files)**:
    *   `nlohmann_json`: JSON parsing.
    *   `ImGuiColorTextEdit`: Shader editor widget.
    *   `cpp-httplib`: For fetching Shadertoy shaders (network access required).
    *   `glfw`: Windowing and input.
    *   `imgui`: Core GUI library (includes ImNodes).
    *   `miniaudio`: Audio playback and capture.
    *   `glad`: OpenGL function loader.
    *   `ImGuiFileDialog`: File dialogs.
*   **Compilation**: Standard C++17.
*   **Typical Build Steps (Linux)**:
    1.  `mkdir build && cd build`
    2.  `cmake ..`
    3.  `make -j$(nproc)`

## 4. General Coding Conventions

*   **C++17**: Adhere to C++17 standards.
*   **ImGui Style**: Follow ImGui's immediate mode patterns for UI.
*   **Error Handling**: Log errors/warnings to `g_consoleLog` and/or `std::cerr`.
*   **File Paths**: Be mindful of relative vs. absolute paths, especially for shaders and scenes. Shaders are typically relative to the executable's runtime directory or a `shaders/` subdirectory.

## 5. Pointers to Other GEMINI.md Files

*   [`src/GEMINI.md`](./src/GEMINI.md): Detailed guidance for working with source files in the `src` directory.
*   [`include/GEMINI.md`](./include/GEMINI.md): Notes on API design and usage for headers in the `include` directory.
*   [`shaders/GEMINI.md`](./shaders/GEMINI.md): Guidelines for GLSL shader development and integration.

## 6. Key Future Development Areas / Known Issues

*   **ImGui Docking/Viewports**: There are persistent issues getting ImGui's Docking and Viewport features (specifically `ImGuiConfigFlags_DockingEnable`, `ImGuiConfigFlags_ViewportsEnable`, and `DockBuilder` API) to be recognized correctly by the compiler, despite using ImGui v1.90.8. This prevents programmatic initial window layouts. The current workaround is to have these features disabled/commented out. This needs a deeper investigation into the CMake setup, include paths, or potential version/compilation flag conflicts for ImGui.
*   **Node System Enhancements**:
    *   Robust multi-input handling for `ShaderEffect` (beyond `iChannel0`).
    *   Dedicated "Image Loader" node type.
    *   Serialization of node graph connections.
    *   More advanced node templates.
*   **Shader Parameter System**: Full activation and potential extension of the `//#control` metadata parsing in `ShaderParser.cpp` to handle a wider range of uniform types and UI controls automatically.
*   **Timeline Polish**: More advanced features like keyframing specific uniform parameters.
*   **Performance**: For complex scenes or many nodes, performance profiling and optimization may be needed.
*   **Error Reporting**: More user-friendly error reporting beyond console logs (e.g., in-UI notifications).

This guide should help future AI agents understand and contribute to the RaymarchVibe project. Remember to consult the more specific `GEMINI.md` files in subdirectories for detailed information.
