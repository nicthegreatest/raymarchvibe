# GEMINI Agent Guide: `include` Directory (RaymarchVibe)

This document provides guidance for AI agents on the header files (`.h`) located in the `include` directory, focusing on their API design and usage. For general project overview, see `../GEMINI.md`, and for `src` implementation details, see `../src/GEMINI.md`.

## 1. Core Interfaces and Data Structures

### 1.1. `Effect.h`
*   **Purpose**: Defines the abstract `Effect` base class, which is the cornerstone of the visual processing pipeline. All specific effect types (like shaders, and potentially image loaders, etc., in the future) must inherit from this class.
*   **Key API Members**:
    *   `virtual void Load() = 0;`: Implement to load any necessary resources (e.g., compile shaders, load textures from disk if the effect type manages its own persistent resources). Called once when the effect is added to the scene or when a scene is loaded.
    *   `virtual void Update(float currentTime) = 0;`: Implement to update any time-dependent internal state of the effect. `currentTime` is the master time, potentially from the timeline.
    *   `virtual void Render() = 0;`: Implement to perform the core rendering logic of the effect. This usually means binding an FBO, setting up uniforms, binding input textures, and drawing a quad.
    *   `virtual void RenderUI() = 0;`: Implement to draw ImGui controls for tweaking the effect's parameters. This is displayed in the "Effect Properties" window.
    *   `virtual GLuint GetOutputTexture() const = 0;`: Must return the OpenGL texture ID that holds the output of this effect. This is typically the color attachment of the effect's internal FBO.
    *   `virtual void SetInputEffect(int pinIndex, Effect* inputEffect)`: Connects the output of `inputEffect` to the specified input `pinIndex` of this effect. The derived class needs to manage these input pointers.
    *   `virtual int GetInputPinCount() const` / `virtual int GetOutputPinCount() const`: Used by the node editor to draw pins. Output is typically 1. Input count varies.
    *   `virtual nlohmann::json Serialize() const = 0;` / `virtual void Deserialize(const nlohmann::json& data) = 0;`: For saving and loading the effect's state (including parameters, file paths, start/end times).
    *   `name`, `startTime`, `endTime`, `id`: Base members for identification and timeline management.
*   **Designing New Effect Types**:
    *   Inherit from `Effect`.
    *   Implement all pure virtual functions.
    *   Manage internal resources (shaders, textures, FBOs).
    *   Ensure `Serialize` and `Deserialize` correctly save and restore all necessary state to reproduce the effect.
    *   If the effect uses shaders with controllable uniforms, consider leveraging or adapting patterns from `ShaderEffect` and `ShaderParser` for `RenderUI` and parameter management.

### 1.2. `ShaderEffect.h`
*   **Purpose**: A concrete implementation of `Effect` for GLSL fragment shaders. (See `src/GEMINI.md` for more implementation details).
*   **Key API Aspects from an include perspective**:
    *   Constructor: `ShaderEffect(const std::string& initialShaderPath, int initialWidth, int initialHeight, bool isShadertoy = false)`
    *   Public methods like `LoadShaderFromFile()`, `LoadShaderFromSource()`, `ApplyShaderCode()`, `GetShaderSource()`, `GetCompileErrorLog()`, `SetShadertoyMode()`, `SetMouseState()`, `SetDisplayResolution()`, `ResizeFrameBuffer()`. These are the primary ways `main.cpp` interacts with shader effects beyond the base `Effect` interface.
*   **Using `ShaderEffect`**: When creating instances (e.g., in `NodeTemplates.cpp` or `main.cpp`), ensure `Load()` is called after construction and addition to the scene to properly initialize shaders and FBOs.

### 1.3. `Timeline.h`
*   **Purpose**: Defines data structures for managing timeline state and individual items on the timeline.
*   **Key Structures**:
    *   `struct TimelineEffect`: A simple struct to represent an effect on the timeline, primarily for potential use if `ImGuiSimpleTimeline` were to directly consume a list of these. Currently, `g_scene` (of `Effect*`) is the main driver for timeline items. Contains `id`, `name`, `startTime_seconds`, `duration_seconds`. Includes `to_json`/`from_json`.
    *   `struct TimelineState`: Holds the overall state of the timeline, such as `isEnabled`, `currentTime_seconds`, `totalDuration_seconds`, `zoomLevel`, `horizontalScroll_seconds`. Also includes `to_json`/`from_json` for its own members (but *not* for its `effects` vector, as that data is sourced from `g_scene`).
*   **Usage**: `g_timelineState` in `main.cpp` is an instance of `TimelineState`. `RenderTimelineWindow` uses its members to control the `ImGuiSimpleTimeline` widget and for playback logic.

### 1.4. `ImGuiSimpleTimeline.h`
*   **Purpose**: A custom ImGui widget for displaying and interacting with a timeline.
*   **Key API**: `bool ImGui::SimpleTimeline(...)`
    *   Takes a vector of `ImGui::TimelineItem` (which contains pointers to `startTime`, `endTime`, `track`, and a `name`).
    *   Modifies `*current_time` via scrubbing.
    *   Modifies `*startTime` and `*endTime` of `TimelineItem`s via dragging/resizing items on the timeline tracks.
*   **Integration**: `main.cpp`'s `RenderTimelineWindow` populates `TimelineItem`s using data from `g_scene` (specifically, pointers to `Effect::startTime` and `Effect::endTime`) and `g_timelineState.currentTime_seconds`.
*   **Extending**: If more complex timeline interactions or displays are needed, this file would be the place to modify. Current enhancements include dynamic tick/label rendering.

### 1.5. `ShaderParameterControls.h`
*   **Purpose**: Defines structures used by `ShaderParser` and `ShaderEffect` to represent controllable shader parameters (defines, uniforms from metadata, consts).
*   **Key Structures**:
    *   `DefineControl`
    *   `ShaderToyUniformControl`
    *   `ConstVariableControl`
*   **Usage**: These are primarily internal data structures for the shader parameter system. Direct interaction by an agent is unlikely unless modifying the `ShaderParser` or how `ShaderEffect::RenderUI` generates controls.

### 1.6. `TextEditor.h` (from ImGuiColorTextEdit)
*   **Purpose**: Provides the advanced text editor widget API.
*   **Key API used in `main.cpp`**:
    *   `TextEditor g_editor;` (global instance)
    *   `g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());`
    *   `g_editor.Render("TextEditor");`
    *   `g_editor.SetText(content);`
    *   `g_editor.GetText();`
    *   `g_editor.SetErrorMarkers(markersMap);`
    *   `g_editor.SetCursorPosition(coordinates);`
    *   Built-in Find (Ctrl+F).
*   **Notes**: Refer to `TextEditor.h` for its full API if more advanced interactions are needed (e.g., undo/redo programmatically, syntax highlighting for other languages).

## 2. General Guidelines for Headers in `include/`

*   **Include Guards**: Always use `#pragma once` or traditional `#ifndef MY_HEADER_H ... #endif`.
*   **Forward Declarations**: Use forward declarations where possible to reduce include dependencies and compilation times, especially if only pointers or references to a type are needed in the header.
*   **Namespaces**: Consider using namespaces (e.g., `RaymarchVibe::`) to organize code and prevent name collisions, especially for new, self-contained systems. (Currently, `Timeline.h` is a good candidate if it were to be expanded significantly).
*   **Documentation**: Briefly comment on the purpose of classes, structs, and public API functions.

This guide provides an overview of the key headers and their roles. For detailed function signatures and behavior, always refer to the header files themselves.
