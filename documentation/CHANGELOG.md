# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- **Feature**: Implemented a robust, data-driven UI system for shader uniforms. The UI is now generated dynamically from comments in the shader source code.
- **Feature**: Added support for `bool`, `vec2`, `vec3`, `vec4`, and `color` types in shader control comments, which generate corresponding UI widgets (checkboxes, sliders, color pickers).
- **Feature**: Implemented shader hot-reloading. The application now watches for file changes and automatically reloads a shader and its UI when the source file is saved.
- **Feature**: Added a "Record Audio" checkbox to the UI to allow for video-only recordings.
- **Feature**: Added a running timer (`HH:MM:SS`) to the UI to display elapsed time during recording.
- **Feature**: Added a confirmation dialog to prevent accidental overwriting of existing files when starting a recording.
- **Feature**: Added a formatted timer display (`HH:MM:SS`) for audio file playback progress.
- **Feature**: Implemented a functional "Scene Output" node in the node editor for explicit graph termination and final output rendering.
- **Feature**: Implemented full scene serialization for node connections and shader parameters, allowing projects to be saved and loaded.
- **Feature**: Implemented amplitude scaling for audio reactivity.

### Fixed
- **Bugfix**: Fixed a persistent video recording bug where the output video's duration was incorrect. The final fix involved ensuring an active audio stream provides a master clock for the video PTS calculation.
- **Bugfix**: Fixed a critical bug in the shader property parser that prevented `//#control` uniforms from being detected, which resulted in a blank render and non-functional UI.
- **Bugfix**: Fixed console spam for the "No finalOutputEffect determined for rendering" error.
- **Bugfix**: Fixed critical performance issue causing a blank screen during video recording by re-architecting the recorder to use a threaded, asynchronous PBO-based pipeline.
- **Bugfix**: Fixed choppy/stuttering audio in recordings by implementing a robust audio buffering strategy within the encoding thread.
- **Bugfix**: Fixed unresponsive and unreliable audio-reactive visuals by implementing a circular buffer for FFT analysis, ensuring smooth and consistent processing.
- **Bugfix**: Video recording output was 10x longer than actual duration due to incorrect timebase scaling in FFmpeg.
- **Bugfix**: Video recording output was upside-down due to incorrect stride handling in `sws_scale`.
- **Bugfix**: Resolved `NaN/+-Inf` errors in audio encoding by sanitizing audio samples from file input.
- **Bugfix**: Eliminated segmentation fault on recording start by correcting `AVFrame` buffer handling for audio.
- **Bugfix**: Resolved "Failed to create GLFW window" runtime error by removing leftover "dummy-gl" references and re-enabling GLFW's X11/Wayland support in `CMakeLists.txt`. The "dummy-gl" setup was a remnant from an AI coding agent's virtual environment, which was incompatible with the workstation's actual graphics environment.
- **Bugfix**: Resolved an issue where the `fractal_tree_audio.frag` shader was rendering a blank black screen due to a logic error in the shader code (`length(p.y)` instead of `abs(p.y)`).
- **Bugfix**: Corrected the issue where `iResolution` and `iTime` were undeclared in shaders by ensuring standard uniforms are always injected, regardless of Shadertoy mode.
- **Bugfix**: Fixed a bug where the "Shadertoy Mode" flag was not being reset when loading new shaders, causing compilation errors.
- **Bugfix**: Fixed a bug where nodes were not created unless the node editor window was open.
- **Bugfix**: Fixed a bug where the "Browse" button in the audio reactivity component was not working.
- **Bugfix**: Fixed a build error related to duplicate definition of `AudioSystem::SetAmplitudeScale(float)`.
- **Bugfix**: Replaced broken FFT implementation with `dj_fft` to restore audio reactivity.
- **Bugfix**: Implemented a robust `iChannel0_active` boolean uniform to replace an unreliable `textureSize` check, resolving an issue where effects would not apply on initial application startup.
- **Bugfix**: Repaired multiple build failures (compiler and linker errors) caused by previous incorrect fixes during the parser rewrite.
- **Bugfix**: Audited and corrected all filter shaders that were using an incorrect `in vec2 TexCoords;` variable, ensuring they correctly calculate UVs and can be used in the node graph.
- **Bugfix**: Restored the renderer's internal fragment shader (`texture.frag`) to its correct state, fixing a bug that contributed to the blank render.
- **Bugfix**: Corrected shader logic for SDF nodes (`raymarch_v2.frag`) to properly accept and apply `iChannel0` texture inputs from preceding effects.
- **Bugfix**: Resolved a UI glitch where the connection link to the "Scene Output" node was not being drawn in the editor.
- **Bugfix**: Eliminated a segmentation fault that occurred when deleting a node from the middle of a connection chain by ensuring all dangling pointers are nullified.

### Changed
- **Refactor**: `VideoRecorder` now uses RAII (`std::unique_ptr`) for all FFmpeg objects to ensure robust, automatic resource management and prevent memory leaks.
- **Refactor**: Decoupled `AudioSystem` from `VideoRecorder` using an observer pattern (`IAudioListener`) to improve modularity and maintainability.
- **Refactor**: Replaced integer-based audio source identifiers with a strongly-typed `enum class` for improved code clarity and safety.
- **Cleanup**: Removed verbose debugging logs from the console to improve output clarity.
- **Changed**: Added more detailed OpenGL error logging to the main render loop to help diagnose future rendering issues.

### Known Issues
- **Video Issue**: Video recording length is incorrect. A recording of a specific duration results in an output file with a significantly longer, incorrect duration (e.g., 20s recording produces a 55s file).
- **Build Issue**: CMake parsing error in `CMakeLists.txt` related to `CONFIGURE_COMMAND` for FFmpeg's `ExternalProject_Add` when enabling `libx264`. This prevents `libx264` from being built and used.