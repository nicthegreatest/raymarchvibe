# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- **Feature**: Implemented a functional "Scene Output" node in the node editor for explicit graph termination and final output rendering.
- **Feature**: Implemented full scene serialization for node connections and shader parameters, allowing projects to be saved and loaded.
- **Feature**: Implemented amplitude scaling for audio reactivity.

### Fixed
- **Bugfix**: Video recording output length was 10x longer than actual duration due to incorrect timebase scaling in FFmpeg.
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
- **Bugfix**: Corrected a critical bug in the shader property parser that prevented `//#control` uniforms from being detected, which resulted in a blank render and non-functional UI.
- **Bugfix**: Repaired multiple build failures (compiler and linker errors) caused by previous incorrect fixes during the parser rewrite.
- **Bugfix**: Audited and corrected all filter shaders that were using an incorrect `in vec2 TexCoords;` variable, ensuring they correctly calculate UVs and can be used in the node graph.
- **Bugfix**: Restored the renderer's internal fragment shader (`texture.frag`) to its correct state, fixing a bug that contributed to the blank render.
- **Bugfix**: Corrected shader logic for SDF nodes (`raymarch_v2.frag`) to properly accept and apply `iChannel0` texture inputs from preceding effects.
- **Bugfix**: Resolved a UI glitch where the connection link to the "Scene Output" node was not being drawn in the editor.
- **Bugfix**: Eliminated a segmentation fault that occurred when deleting a node from the middle of a connection chain by ensuring all dangling pointers are nullified.

### Changed
- **Cleanup**: Removed verbose debugging logs from the console to improve output clarity.
- **Changed**: Added more detailed OpenGL error logging to the main render loop to help diagnose future rendering issues.

### Known Issues
- **Build Issue**: CMake parsing error in `CMakeLists.txt` related to `CONFIGURE_COMMAND` for FFmpeg's `ExternalProject_Add` when enabling `libx264`. This prevents `libx264` from being built and used.
- **Video Issue**: Video output is a blank black screen when recording. This is likely due to the `MPEG4` encoder or its configuration, as `libx264` is currently not building.
- **Audio Issue**: Audio from MP3/WAV file input is still choppy and slowed down in recordings, despite `NaN/+-Inf` errors being resolved and buffering logic implemented. Microphone audio records correctly.
