# RaymarchVibe: Rust Conversion Architectural Evaluation & Migration Plan

> **Document Version:** 1.0
> **Date:** October 2026
> **Author:** Jules (Senior Software Engineer)
> **Scope:** Technical feasibility study, pros/cons analysis, ecosystem mapping, and phased migration roadmap for converting RaymarchVibe from C++17 to Rust.

---

## Executive Summary

**RaymarchVibe** is currently a C++17 real-time node-based shader exploration tool utilizing OpenGL 3.3, GLFW, Dear ImGui, imnodes, ImGuiColorTextEdit, miniaudio, dj_fft, and an embedded FFmpeg backend. While the project is performant and feature-rich, maintaining the C++ codebase presents ongoing challenges in build system complexity (CMake + FetchContent + custom static FFmpeg compilation), cross-platform deployment (macOS, Windows, Web), thread safety (concurrent audio analysis and video recording), and manual C-style resource management.

This document evaluates converting RaymarchVibe to Rust. Converting to Rust offers significant advantages in memory safety, thread concurrency, unified dependency management via `cargo`, and potential WebAssembly deployment. However, it requires choosing between a **Direct C-Binding Port** (maintaining ImGui/OpenGL) and a **Rust-Native Re-architecture** (leveraging `wgpu` and `egui`).

---

## 1. Pros & Cons Analysis

### 1.1 Pros of Converting to Rust

1. **Elimination of Thread-Safety & Concurrency Bugs**
   - *Current C++ Issue:* RaymarchVibe runs multiple asynchronous background tasks—audio capture/FFT analysis, FFmpeg video frame encoding, and hot-shader compilation/relinking. Hand-rolled lock/mutex management and raw ring buffers in C++ are prone to data races or deadlocks.
   - *Rust Advantage:* Rust’s `Send` and `Sync` traits, lock-free channels (`crossbeam-channel` or `std::sync::mpsc`), and ownership model guarantee data-race safety at compile time.

2. **Unified & Reliable Dependency Management (`cargo`)**
   - *Current C++ Issue:* `CMakeLists.txt` is hundreds of lines long, patching GLFW build scripts, handling fallback fetches for `glm`, managing `FetchContent` for ImGui/miniaudio, and executing a multi-minute custom static `ExternalProject_Add` compilation of FFmpeg `n5.1.2`.
   - *Rust Advantage:* All dependencies (graphics, UI, audio, serialization, math) are managed cleanly via `Cargo.toml`. Building the project on any platform becomes a single standard command: `cargo build`.

3. **Type-Safe Abstract Data Types & Shader State**
   - *Current C++ Issue:* Uniform values, graph nodes, and shader parameters are represented using raw pointers, variant unions, or manual string maps.
   - *Rust Advantage:* Idiomatic Rust enums (Algebraic Data Types) naturally model node inputs/outputs (`enum UniformValue { Float(f32), Vec2([f32; 2]), Vec4([f32; 4]), Texture(TextureHandle) }`) with exhaustive pattern matching.

4. **WebAssembly & Multi-Platform Readiness**
   - *Current C++ Issue:* Porting GLFW + raw OpenGL 3.3 + FFmpeg + miniaudio to WebAssembly / WebGL2 or macOS Metal requires extensive platform-specific boilerplate.
   - *Rust Advantage:* A Rust architecture using `wgpu` and `egui` can compile directly to `wasm32-unknown-unknown` and render inside any web browser via WebGPU/WebGL2 without code changes.

5. **Robust Serialization & Shader Parsing**
   - *Current C++ Issue:* Custom JSON comment extraction (`ShaderParser.cpp`) and workspace serialization use manual string parsing and `nlohmann::json`.
   - *Rust Advantage:* `serde` provides compile-time zero-cost type serialization/deserialization. `regex` or `nom` provides fast, memory-safe shader metadata parsing.

---

### 1.2 Cons & Challenges of Converting to Rust

1. **Complete Rewrite Overhead (~10k+ Lines of Code)**
   - Translating all C++ systems (`ShaderEffect`, `VideoRecorder`, `AudioSystem`, `PresetFileParser`, `ShadertoyIntegration`, `UIManager`, custom node layouts) requires substantial engineering effort.

2. **FFmpeg Integration Complexity**
   - Binding to FFmpeg C libraries in Rust (`ffmpeg-next` or `ffmpeg-sys-next`) requires local FFmpeg development headers installed on the target machine, or building C libraries via `build.rs`. Alternatively, spawning an external `ffmpeg` subprocess via stdout pipes is simpler but loses direct in-memory frame control.

3. **UI Engine Paradigm Shift (If Switching to `egui`)**
   - Dear ImGui and `imnodes` provide a specific immediate-mode UI look and feel. Transitioning to Rust-native `egui` + `egui_node_graph` alters the visual aesthetics and requires re-implementing custom UI widgets (like the harmonic color palette generator and GLSL text editor).

4. **GLSL Shader Compatibility with Modern Graphics APIs**
   - RaymarchVibe's shader library relies heavily on raw GLSL fragment shaders (`#version 330 core`). If migrating to `wgpu`, GLSL shaders must be converted at runtime to WGSL or SPIR-V using tools like `naga` or `shaderc`, or the renderer must stick to an OpenGL backend (`glow`).

---

## 2. Dependency & Library Mapping Matrix

| Functional Area | Current C++ Library / Implementation | Option A: Direct C-Binding Port | Option B: Rust-Native Re-architecture (Recommended) |
| :--- | :--- | :--- | :--- |
| **Windowing & Input** | GLFW 3.4 (`glfw`) | `glfw-rs` | `winit` |
| **Graphics API** | OpenGL 3.3 (`glad`) | `glow` (GL on Web/Desktop) | `wgpu` (WebGPU / Vulkan / Metal / DX12 / GL) |
| **Math Library** | GLM (`glm::glm`) | `cgmath` or `glam` | `glam` |
| **GUI Framework** | Dear ImGui v1.90.8 | `imgui-rs` or `cimgui-sys` | `egui` |
| **Node Graph UI** | `imnodes` (vendor) | `imnodes-rs` bindings | `egui_node_graph` |
| **Code Editor** | `ImGuiColorTextEdit` | C++ FFI wrapper | `egui_code_editor` or `syntax-highlighting` |
| **Audio Capture** | `miniaudio` (PCM capture) | `miniaudio-rs` | `cpal` |
| **FFT Analysis** | `dj_fft` (C++ header) | `rustfft` or `realfft` | `realfft` |
| **Video Recording** | FFmpeg C API (`libavcodec`, `libavformat`) | `ffmpeg-next` | `ffmpeg-next` or sub-process pipe to `ffmpeg` CLI |
| **JSON Serialization** | `nlohmann/json` v3.11.3 | `serde_json` | `serde` + `serde_json` |
| **HTTP / Shadertoy** | `cpp-httplib` + OpenSSL | `reqwest` or `ureq` | `reqwest` (with `tokio`) or `ureq` |
| **Image Loading** | `stb_image` | `stb_image-rs` | `image` crate |

---

## 3. Architectural Comparison: Direct Port vs. Native Re-architecture

### Strategy A: Direct Port (`glow` + `imgui-rs` + `imnodes-rs`)
* **Concept:** Maintain the exact same UI layout, OpenGL 3.3 graphics pipeline, and ImGui nodes by using C bindings (`cimgui`, `imnodes-sys`).
* **Pros:**
  * 100% visual parity with existing ImGui themes and node editor layout.
  * Direct execution of raw GLSL 330 shaders without translation.
* **Cons:**
  * Heavy reliance on C FFI (`unsafe` blocks in Rust).
  * Build scripts (`build.rs`) still need to compile underlying C/C++ libraries.
  * Limited WebAssembly/WebGPU forward compatibility.

### Strategy B: Rust-Native Re-architecture (`wgpu` + `winit` + `egui`) **[RECOMMENDED]**
* **Concept:** Rebuild the application using modern, memory-safe, pure-Rust foundations.
* **Pros:**
  * Zero `unsafe` C-FFI required for UI and rendering loops.
  * Native cross-platform execution (Linux, Windows, macOS, WebAssembly/Browser).
  * Seamless concurrency between audio streaming, video encoding, and UI rendering.
  * Clean, modular crate structure.
* **Cons:**
  * Shader parsing requires translating user GLSL shaders to SPIR-V/WGSL via `naga`.
  * Node UI must be recreated using `egui_node_graph`.

---

## 4. Proposed Rust Architecture

```
raymarchvibe/
├── Cargo.toml                   # Workspace manifest
├── crates/
│   ├── raymarchvibe-core/       # Pure logic: Node graph, uniform values, preset parsing
│   ├── raymarchvibe-audio/      # CPAL audio capture + RealFFT frequency band processing
│   ├── raymarchvibe-shader/     # GLSL parsing (regex/nom), WGSL/SPIR-V translation
│   ├── raymarchvibe-export/     # Video recording pipeline (FFmpeg / pipeline encoder)
│   └── raymarchvibe-app/        # Main executable: Winit + Wgpu + Egui UI & Node Editor
```

### Core Data Structure Example (Idiomatic Rust)

```rust
use std::collections::HashMap;
use serde::{Serialize, Deserialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum UniformValue {
    Float(f32),
    Vec2([f32; 2]),
    Vec3([f32; 3]),
    Vec4([f32; 4]),
    Color([f32; 4]),
    Texture(String),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShaderNode {
    pub id: u64,
    pub name: String,
    pub shader_source: String,
    pub uniforms: HashMap<String, UniformValue>,
    pub inputs: Vec<String>,
    pub output_texture_id: Option<u64>,
}
```

---

## 5. Phased Migration Strategy & Roadmap

```
  Phase 1: Core Engine & Parsing (Weeks 1-2)
  ├── Setup Cargo workspace
  ├── Port ShaderParser (Regex / Uniform metadata extraction)
  └── Implement Serde serialization for workspace save/load

  Phase 2: Audio Pipeline (Weeks 3-4)
  ├── CPAL audio input capture stream
  ├── RealFFT multi-band analysis (Bass, Mid, Treble, Envelope)
  └── Crossbeam channel audio reactivity state sharing

  Phase 3: Graphics & Node UI (Weeks 5-8)
  ├── Winit window setup + Wgpu render pipeline
  ├── Shader compilation engine (GLSL execution via Glow/Naga)
  └── Egui + egui_node_graph UI integration & Color Palette Generator

  Phase 4: Video Recording Engine (Weeks 9-10)
  ├── Frame buffer pixel extraction
  ├── Offscreen asynchronous frame queue
  └── FFmpeg encoder backend integration

  Phase 5: Parity Verification & Polish (Weeks 11-12)
  ├── Shadertoy API integration (`reqwest`)
  ├── Testing against existing preset library
  └── Documentation & Release
```

---

## 6. Risk Assessment Matrix

| Risk | Impact | Likelihood | Mitigation Strategy |
| :--- | :--- | :--- | :--- |
| **FFmpeg build issues across operating systems** | High | High | Support dual export modes: `ffmpeg-next` static bindings OR CLI pipe streaming (`ffmpeg -f rawvideo ...`). |
| **GLSL 330 compatibility in `wgpu`** | High | Medium | Use `glow` (OpenGL ES / WebGL2 / GL 3.3) for the render backend if WGSL conversion via `naga` drops specific GLSL extensions. |
| **Loss of ImGui custom widget styling** | Low | Medium | Customize `egui` visuals with custom frame drawing and color palette controls. |
| **Audio latency / dropouts in FFT analysis** | Medium | Low | Use lock-free ring buffers (`rtrb`) for PCM samples between CPAL callback and FFT processing thread. |

---

## 7. Conclusion & Recommendation

Converting RaymarchVibe to Rust is **highly feasible and strongly beneficial** for long-term maintainability, platform portability, thread safety, and developer experience.

* **Short-Term Goal:** Maintain and polish the current C++17 codebase for immediate releases.
* **Long-Term Recommendation:** Adopt **Strategy B (Rust-Native Re-architecture)** using a modular Cargo workspace (`winit`, `wgpu`, `egui`, `cpal`, `realfft`, `serde`). This will modernize RaymarchVibe into a safer, more extensible visual synthesis platform capable of running both as a desktop app and in web browsers.
