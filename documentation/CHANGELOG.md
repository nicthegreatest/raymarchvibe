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