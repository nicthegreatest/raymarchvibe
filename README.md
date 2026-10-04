# RaymarchVibe ✨ BETA/EXPERIMENTAL

![app-screenshot](https://raw.githubusercontent.com/nicthegreatest/raymarchvibe/refs/heads/main/documentation/raymarchvibe_screen.png)

## A Modern SDF / Raymarching Tool

This tool is designed for shader exploration and creative coding, inspired by the demoscene spirit. It allows artists and developers to intuitively build complex visual effects by connecting shaders in a graph, manipulating their parameters in real-time, and seeing the results instantly. It aims to lower the barrier to entry for creating generative art and encourages a workflow based on experimentation and discovery.

Dive in, tweak, explore, and vibe with your shaders!

## Key Features

*   **Node-Based Visual Programming:** Create complex, multi-pass effects by connecting shaders together in an intuitive node graph.
*   **Advanced Real-time Shader Editor:** Powered by ImGuiColorTextEdit for GLSL syntax highlighting, line numbers, and error marking.
*   **Dynamic UI Generation:** Automatically generate UI controls (sliders, color pickers) for shader uniforms by adding a single line of JSON in your shader comments.
*   **Enhanced Color Picker with Palettes:** Generate harmonious color palettes automatically using color theory (complementary, triadic, analogous, split-complementary, square, and monochromatic harmony types) with optional smooth gradient interpolation.
*   **Advanced Audio Reactivity:** Drive shader animations with real-time audio analysis: `iAudioAmp` (overall level), plus a `vec4` of normalised 0..1 bands — `iAudioBands.x` bass, `.y` mids, `.z` treble and `.w` overall energy — available raw (`iAudioBands`) and through a snap-attack / ~250 ms-decay envelope (`iAudioBandsAtt`). See [SHADERS.md §3.1](documentation/SHADERS.md#31-audio-bands).
*   **Video & Audio Recording:** Record your creations to high-quality video files (MP4, MOV) with synchronized audio using a dedicated, high-performance FFmpeg backend.
*   **Full Scene Serialization:** Save and load your entire workspace, including the node graph, shader code, and UI parameters, to a JSON file.
*   **Shadertoy Integration:** Fetch and load shaders directly from Shadertoy.com by ID or URL, and they are instantly available as nodes.
*   **Customizable UI Themes:** Switch between several built-in UI themes to customize the look and feel of the editor.

## Dependencies

RaymarchVibe needs a C++17 compiler, CMake 3.15+ and OpenGL 3.3+ drivers.

Downloaded and built from source by CMake's `FetchContent` on the first configure: GLFW 3.4, Dear ImGui v1.90.8, ImGuiColorTextEdit (pinned commit), nlohmann/json v3.11.3, cpp-httplib v0.15.3 and miniaudio (unpinned — no tag is set, so it follows upstream's default branch).

Vendored in this repository, so nothing to download: GLAD (`src/glad.c`, headers in `include/glad/`), imnodes and ImGuiFileDialog (`vendor/`), stb_image (`vendor/stb/`) and the dj_fft FFT headers (`vendor/dj_fft/`).

GLM is system-first: CMake tries `find_package(glm)` and only downloads a pinned copy (tag 1.0.1) if no system install exists. `libglm-dev` is therefore optional; pass `-DRAYMARCHVIBE_FETCH_GLM=OFF` to require the system package instead.

FFmpeg n5.1.2 is also downloaded and compiled from source on the first build (`ExternalProject`, static, H.264 through libx264). That takes a while, and is why `libx264-dev` is required below.

The GLFW here is built from source, Wayland included, so GLFW's own X11/Wayland dependencies are needed — the `libglfw3-dev` package is not used at all.

**System packages (Debian/Ubuntu):**

```bash
sudo apt update
sudo apt install build-essential cmake libglm-dev libgl1-mesa-dev \
    libx11-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev \
    libxkbcommon-dev libwayland-dev libwayland-bin wayland-protocols \
    libasound2-dev libssl-dev libx264-dev
```

*   `libgl1-mesa-dev` provides the OpenGL headers; the X11, Wayland and `libxkbcommon-dev` packages are what the source-built GLFW needs. Without `libwayland-bin` (which supplies `wayland-scanner`) the configure step stops at `Failed to find wayland-scanner`.
*   `libglm-dev` is optional (see above).
*   `libssl-dev` is only needed for HTTPS Shadertoy fetching (configure with `-DRAYMARCHVIBE_ENABLE_SSL=OFF` to build without it).
*   `libasound2-dev` provides the ALSA headers used by miniaudio for audio input; `libx264-dev` is required by the locally built FFmpeg.

## Build Instructions (Linux)

1.  Clone the repository:
    ```bash
    git clone https://github.com/nicthegreatest/raymarchvibe
    cd raymarchvibe/
    ```

2.  Install dependencies (if not already present) — see [Dependencies](#dependencies) for the package list and what each one is for.

3.  Configure with CMake:
    Create a build directory and run cmake from within it.
    ```bash
    mkdir build
    cd build
    cmake ..
    ```
    *   If you wish to disable HTTPS for Shadertoy fetching, you can turn off the SSL option from within the `build` directory:
        ```bash
        cmake .. -DRAYMARCHVIBE_ENABLE_SSL=OFF
        ```

4.  Build the project:
    From within the `build` directory, run `make` with one job per available core:
    ```bash
    make -j"$(nproc)"
    ```
    The first build also compiles FFmpeg, so it takes a while. On a machine with little RAM, use a lower number (for example `make -j2`) — several files are large and a high job count can exhaust memory.

5.  Run RaymarchVibe:
    The executable is located in the `build` directory.
    ```bash
    ./RaymarchVibe
    ```

#### Command-line flags

RaymarchVibe supports optional flags to speed up debugging and iteration:

- `-verbose=ON|OFF` (also `-v`, `--verbose`): Enables verbose terminal logging. If a value is omitted, the presence of the flag enables verbose mode.
  Examples:
  ```bash
  ./RaymarchVibe -verbose=ON
  ./RaymarchVibe -v
  ```

- `-load=PATH` (also `--load PATH` or `--load=PATH`): Load a fragment shader on startup.
  Examples:
  ```bash
  ./RaymarchVibe -load=shaders/raymarch_v2.frag
  ./RaymarchVibe --load /absolute/path/to/shader.frag
  ```

Notes:
- Paths beginning with `/shaders/...` are treated as relative to the current working directory (e.g. `./shaders/...`).
- If the provided shader fails to compile, the app will still start and show the error in the Console and mark lines in the editor.
- If no flag is provided, the default shader is `shaders/raymarch_v2.frag`.

### Milk-Converter

`Milk-Converter/` is empty in this repository: it is recorded only as a submodule reference with no matching entry in `.gitmodules`, so a fresh clone creates the directory and puts nothing in it, and there is no converter to build.

MilkDrop preset conversion is out of scope for now. `documentation/SHADERS.md` §6 ("Milk-Converter Translation Standards") records the variable-mapping rules a future converter would have to follow.


## Planned Features

See the [TODO.md](documentation/TODO.md) for a complete list of planned features and improvements, including:

- Switch between mouse input and WASD keys for camera control
- Improved handling of larger, more complex shaders while maintaining real-time performance
- Enhanced macOS and Windows compatibility
- Text rendering effects
- AppImage compilation for easier Linux distribution

## Documentation

For detailed information about the project, see the `documentation/` directory:

- **[SHADERS.md](documentation/SHADERS.md)** - Complete shader specification and creative guidelines (essential reading for shader authors!)
- **[PALETTE_FEATURE.md](documentation/PALETTE_FEATURE.md)** - Enhanced color picker with palettes, harmonies, and gradients
- **[CHANGELOG.md](documentation/CHANGELOG.md)** - Version history and release notes
- **[TODO.md](documentation/TODO.md)** - Planned features and improvements
- **[CODE_REVIEW.md](documentation/CODE_REVIEW.md)** - Architectural overview and code quality assessment (a 2025-10-30 snapshot — read its status banner for what has since been fixed)

## Development Notes

### Working with Shaders

The build copies the whole source `shaders/` directory into the build directory as a post-build step of the `RaymarchVibe` target, which is where the application loads shaders from.

That copy is not yet declared as depending on the `.frag` files (see the shader-copy item in [TODO.md](documentation/TODO.md)), so it only runs when the `RaymarchVibe` target itself is relinked. A shader-only edit is therefore **not** picked up by a plain `make` — force the copy yourself from the `build/` directory:

```bash
cmake -E copy_directory ../shaders shaders
```

or configure and build a clean build directory:

```bash
rm -rf build && mkdir build && cd build && cmake .. && make -j"$(nproc)"
```

You only need to re-run `cmake ..` before `make` if you add or remove C++ source files or change the project's structure in `CMakeLists.txt`.

## License

Copyright 2025 https://github.com/nicthegreatest/

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. See <http://www.gnu.org/licenses/>.
