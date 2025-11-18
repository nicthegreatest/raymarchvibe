## [0.3.9] - 2025-11-18

### Added
- **Advanced Color Palette Sync System:** Implemented comprehensive color harmony synchronization for shader controls.
  - **Semantic Color Role Detection:** Automatically identifies primary, secondary, tertiary, and accent color controls using semantic naming patterns (e.g., "SecondaryColor", "AccentColor"). Primary controls generate palettes, secondary controls sync from primary gradients.
  - **Color Palette Generation:** Integrated ColorPaletteGenerator with support for Monochromatic, Complementary, Triadic, Analogous, Split-Complementary, and Square color harmonies.
  - **Gradient Mode:** Secondary controls can automatically sync to primary color gradients, sampling at semantic positions (e.g., "SecondaryColor" samples at 0.25 position, "AccentColor" at 0.75 position).
  - **Interactive Palette Preview:** Visual preview of generated palettes with clickable segments for real-time color editing.
  - **Demonstration Shader:** Added `palette_sync_demo.frag` as comprehensive test case showcasing all palette sync functionality.

### Changed
- **Professional Debugging:** Refined debug output to show clean shader load summaries without terminal flood. Moved from per-frame spam to once-per-shader-load logging.
- **Shader Parameter Controls:** Enhanced UI for color controls with palette mode switching (Individual, Palette, Sync options).

### Fixed
- **Palette Sync UI Bug:** Fixed critical type mismatch where `paletteMode` was declared as `bool` but used as `int` (0=Individual, 1=Palette, 2=Sync) throughout the codebase. Changed type from `bool` to `int` in `ShaderParser.h`, enabling proper Sync mode selection in UI dropdowns for secondary color controls.

---

## [0.3.8] - 2025-11-03

### Added
- **Drag-and-Drop Shader Loading:** Implemented the ability to drag and drop `.frag` shader files directly onto the application window. Dropped `.frag` files will automatically create a new `ShaderEffect` node in the Node Editor, loaded with the shader code, and placed at the mouse cursor's position. This feature uses a thread-safe queuing mechanism to ensure robust handling of file drop events.
- Implemented generic uniform smoothing for 'float' types, controlled by '{"smooth": true}' metadata in shader uniform comments.
- Added a 'Refresh' button with F5 keybinding to the Shader Editor for faster iteration.

### Fixed
- Resolved compilation errors caused by duplicate 'ShaderParser.h' files and incorrect header/source file synchronization.
- Corrected video recording errors related to non-even frame dimensions by ensuring framebuffer dimensions are divisible by 2.
- Improved video and audio recording quality by adjusting FFmpeg encoder settings (x264 preset from "fast" to "medium", CRF from 23 to 18, audio bitrate to 192kbps).
- Debugged and fixed the 'bezier_fractal_visualizer.frag' shader: corrected uniform types, adjusted UV source, changed 'iAudioBands' to 'vec4', and applied uniform smoothing to 'u_complexity'.

### Changed
- Updated 'documentation/SHADERS.md' to clarify shader structure and the implications of 'main()' vs. 'mainImage()' for "Shadertoy Mode" and uniform handling.

---

## [0.3.7] - 2025-10-30

### Added
- Introduced an advanced `shape_soap_bubble_advanced.frag` template with UI metadata for interactive parameter control.

### Changed
- Refined repository structure by moving ImGui theme logic into `src/themes.cpp` and cleaning up stale development artifacts.
- Updated `.gitignore` rules to prevent temporary shader backups from being committed.
- Clarified planned features in the README, pointing to the project TODO list for ongoing work.

---

## [0.3.6] - 2025-10-23

### Added
- **Shadertoy iChannels:** Expanded Shadertoy compatibility to support all four texture input channels (`iChannel0` to `iChannel3`).
- **Direct Texture Loading:** Added a "Load Texture" button to each iChannel input slot in the Node Properties panel, allowing for direct loading of an image file which automatically creates and connects an `Image Loader` node.
- **Input Thumbnails:** The Node Properties panel now displays a thumbnail preview of the connected texture for each iChannel, providing immediate visual feedback.
- **Input Unlinking:** Added an "Unlink" button next to each connected iChannel input for quick disconnection.

### Fixed
- **iChannel Loading Bug:** Fixed a critical bug where Shadertoy shaders would incorrectly load with only one input channel instead of the required four. The inputs now appear correctly on initial load.

### Changed
- Added a new `ichannel_visualizer.frag` shader for testing and visualizing all four iChannel inputs.

### Removed
- Deleted the old, incomplete `ichannel_test.frag` shader.

## [0.3.5] - 2025-10-19

### Added
- Command-line flags to improve debugging and startup workflows:
  - `-verbose=ON|OFF` (or `-v`, `--verbose`) to toggle verbose terminal logging.
  - `-load=PATH` (or `--load PATH`) to load a custom fragment shader at startup.
- Examples:
  - `./RaymarchVibe -verbose=ON -load=shaders/new_shader_test.frag`
  - `./RaymarchVibe -v --load /shaders/new_shader_test.frag`

### Notes
- When `-load` is provided, the app creates the initial effect from that file instead of the default `shaders/raymarch_v2.frag`.
- A path beginning with `/shaders/...` is treated as relative to the current working directory for convenience (e.g. `./shaders/...`).

## [0.3.4] - 2025-10-04

### Added
- **Lighting Control:** Added a new control to manipulate the light source position in a spherical manner. The light's position can be changed by holding `Ctrl` and dragging with the left mouse button.
- **Camera Control Help Text:** Added a text overlay that appears when camera controls are enabled, providing instructions for camera and light manipulation.

### Changed
- **Default Shader:** The default shader on application startup is now `raymarch_v2.frag`.
- **Default Colors:** The default object and light colors in `raymarch_v2.frag` are now white.

### Fixed
- **Default Color Bug:** Fixed a persistent bug where the default object and light colors were not being applied on startup. This was caused by ImGui's `imgui.ini` state-saving feature overriding the shader's default values. The fix involved disabling `imgui.ini` to ensure a clean state on every launch.

## [0.3.3] - 2025-10-03

### Changed
- **UI:** Main menu has been reorganized for a more intuitive workflow.
- **UI:** "Settings" menu has been removed and "Themes" moved to the "File" menu.
- **UI:** "Node Editor" and "Audio Reactivity" have been moved from the "View" menu to the main menu bar.
- **UI:** "Recording" menu is now "Recording (F1)".
- **UI:** "Node Editor" is now "Node Editor (F2)".
- **UI:** "Audio Reactivity" is now "Audio Reactivity (F3)".
- **Shortcuts:** F1 now starts/stops recording.
- **Shortcuts:** F2 now toggles the Node Editor window.
- **Shortcuts:** F3 now toggles the Audio Reactivity window.

### Removed
- **UI:** "Help" menu and its associated window and code have been removed.

# Changelog

## [0.3.2] - 2025-10-01

### Added
- **Node Context Menu:** Added "Duplicate" and "Unlink" options to the node context menu in the Node Editor.

### Changed
- **Node Placement:** New nodes are now created at the position of the right-click in the Node Editor.
- **Refactoring:** Refactored the node creation logic to use a helper function, improving code maintainability and simplifying the process of adding new nodes.

## [0.3.1] - 2025-10-01

### Added
- **Advanced Circular Audio Visualizer:** Replaced the old visualizer with a new, more advanced version (`viz_circular_audio.frag`).
  - Added individual color controls for each of the 4 frequency bands.
  - Added a "Band Color Mix" parameter to blend between a circular gradient and the individual band colors.
  - Added equalization controls (`u_bass_boost`, `u_low_mid_boost`, `u_high_mid_boost`, `u_treble_boost`) to fine-tune the sensitivity of each frequency band.

### Fixed
- **Audio Reactivity:** Fixed a bug that prevented the `iAudioBands` uniform from being updated correctly, causing shaders to not react to audio frequencies.
- **Video Recording (Slow Motion):** Fixed a major bug in the video recorder that caused videos to be recorded in slow motion. The frame submission rate is now correctly limited to the specified frame rate.
- **Video Recording (Audio Sync):** Fixed a slight audio sync issue in the recorded videos by synchronizing the start of the audio and video streams.

### Changed
- **Cleanup:** Removed old and unused shader templates:
  - `texture_passthrough.frag`
  - `shape_circle.frag`
  - the old `viz_circular_audio.frag`

## [0.3.0] - 2025-10-01

### Added
- **Raymarch Sphere Shader:** New node that renders a 3D sphere with advanced controls.
  - Phong lighting (ambient, diffuse, specular).
  - Controllable 3D rotation (axis, speed).
  - Full 2D texture mapping support (scale, offset, rotation).
- **Image Loader Node:** New node that can load JPG/PNG images from disk to provide textures to other effects.
  - Integrates `stb_image.h` for robust image loading.
- **Circular Audio Visualizer:** New audio-reactive shader that renders a circular bar-graph equalizer.
  - Reacts to 4 audio frequency bands.
  - Features customizable colors via a radial gradient.
  - Supports compositing over other shader effects.

### Fixed
- **Node Editor Crash:** Resolved a critical runtime crash when opening the Node Editor window.
- **Shader Loading:** Implemented a robust, engine-level fix for `GL_INVALID_OPERATION` errors that occurred when shaders with texture inputs were loaded without a connection.
- **Build System:** Corrected documentation and processes around the build system to ensure shader files are reliably updated.

### Changed
- Removed several old/broken shader experiments.
- Refactored `Renderer` and `ShaderEffect` classes to support new features and bug fixes.
