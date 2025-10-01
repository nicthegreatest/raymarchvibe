# Changelog

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