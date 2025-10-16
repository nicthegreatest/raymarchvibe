# TODO

- [ ] **Verify Audio Reactivity:** The new Circular Audio Visualizer appears static. Confirm that the user is enabling the global "Enable Audio Link" checkbox in the "Audio Reactivity" window. If the issue persists, investigate uniform passing.

- [ ] **Verify Compositing:** The user reported being unable to see the sphere behind the visualizer. The latest C++ fix should have resolved this, but it needs to be tested and confirmed.

- [ ] **Improve Build Process:** The current build requires a full `rm -rf build && cmake .. && make` to reliably copy new shader files. Investigate modifying the `CMakeLists.txt` to create a proper dependency between the shader files and the build target to allow `make` to detect shader changes automatically.

- [ ] **Implement WASD Camera Controls:** Add an alternative camera control scheme using WASD keys.

- [ ] **Improve Performance with Complex Shaders:** Investigate performance bottlenecks when dealing with very large or complex shaders.

- [ ] **Improve macOS and Windows Compatibility:** Add specific build instructions and code paths where necessary to ensure the application runs smoothly on other operating systems.