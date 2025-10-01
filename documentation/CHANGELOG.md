# Changelog

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
