# TODO

- [ ] **Add Voronoi Noise Generator Music Visualizer:** Create a new music visualizer based on the bezier curve shader, incorporating Voronoi noise generation.

- [ ] **Verify Audio Reactivity:** The new Circular Audio Visualizer appears static. Confirm that the user is enabling the global "Enable Audio Link" checkbox in the "Audio Reactivity" window. If the issue persists, investigate uniform passing.

- [ ] **Verify Compositing:** The user reported being unable to see the sphere behind the visualizer. The latest C++ fix should have resolved this, but it needs to be tested and confirmed.

- [ ] **Improve Build Process:** The current build requires a full `rm -rf build && cmake .. && make` to reliably copy new shader files. Investigate modifying the `CMakeLists.txt` to create a proper dependency between the shader files and the build target to allow `make` to detect shader changes automatically.

- [ ] **Implement WASD Camera Controls:** Add an alternative camera control scheme using WASD keys.

- [ ] **Improve Performance with Complex Shaders:** Investigate performance bottlenecks when dealing with very large or complex shaders.

- [ ] **Improve macOS and Windows Compatibility:** Add specific build instructions and code paths where necessary to ensure the application runs smoothly on other operating systems.

- [ ] **Organise Shader Folders:** The `shaders/`, `shaders/templates/`, and `shaders/presets/` folders are disorganized. Many files are broken, unused, or outdated. A cleanup is needed. This involves:
    - Identifying and removing broken or redundant shaders.
    - Creating a list of outdated presets that should be reimagined using the modern `SHADERS.md` template format.

- [ ] **Implement Text Rendering Effect:** Create a new `TextEffect` node that allows rendering text onto the screen. This should include the following features:
    - Wavy text animation.
    - Color cycling with adjustable direction.
    - Font selection.
    - Text alignment (left, center, right).
    - Outline and shadow effects.
    - Independent scale and rotation controls.

- [ ] **Investigate AppImage Compilation:** Research and implement a process for compiling the application into an AppImage for Linux distribution. This will involve:
    - Analyzing and updating build scripts.
    - Ensuring all dependencies are correctly bundled.
    - Verifying cross-platform compatibility of the build process.
    - Thoroughly testing the resulting AppImage.
