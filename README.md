RaymarchVibe ✨ BETA/EXPERIMENTAL

![app-screenshot](https://raw.githubusercontent.com/nicthegreatest/raymarchvibe/refs/heads/main/documentation/raymarchvibe_screen.png)

## A Modern SDF / Raymarching Tool

This tool is designed for shader exploration and creative coding, inspired by the demoscene spirit. It allows artists and developers to intuitively build complex visual effects by connecting shaders in a graph, manipulating their parameters in real-time, and seeing the results instantly. It aims to lower the barrier to entry for creating generative art and encourages a workflow based on experimentation and discovery.

Dive in, tweak, explore, and vibe with your shaders!

## Key Features

*   **Node-Based Visual Programming:** Create complex, multi-pass effects by connecting shaders together in an intuitive node graph.
*   **Advanced Real-time Shader Editor:** Powered by ImGuiColorTextEdit for GLSL syntax highlighting, line numbers, and error marking.
*   **Dynamic UI Generation:** Automatically generate UI controls (sliders, color pickers) for shader uniforms by adding a single line of JSON in your shader comments.
*   **Enhanced Color Picker with Palettes:** Generate harmonious color palettes automatically using color theory (complementary, triadic, analogous, split-complementary, square, and monochromatic harmony types) with optional smooth gradient interpolation.
*   **Advanced Audio Reactivity:** Drive shader animations with real-time audio analysis, using not just overall amplitude but also four distinct frequency bands (bass, mids, highs).
*   **Video & Audio Recording:** Record your creations to high-quality video files (MP4, MOV) with synchronized audio using a dedicated, high-performance FFmpeg backend.
*   **Full Scene Serialization:** Save and load your entire workspace, including the node graph, shader code, and UI parameters, to a JSON file.
*   **Shadertoy Integration:** Fetch and load shaders directly from Shadertoy.com by ID or URL, and they are instantly available as nodes.
*   **Customizable UI Themes:** Switch between several built-in UI themes to customize the look and feel of the editor.

## Dependencies

RaymarchVibe needs a C++17 compiler, CMake 3.15+ and OpenGL 3.3+ drivers. Everything else is fetched automatically with CMake's `FetchContent`: GLFW, Dear ImGui, ImGuiColorTextEdit, nlohmann/json, cpp-httplib, miniaudio, GLAD (vendored in `src/`) and GLM.

FFmpeg n5.1.2 is also downloaded and compiled from source on the first build. That takes a while and is why `libx264-dev` is required below.

**System packages (Debian/Ubuntu):**

```bash
sudo apt update
sudo apt install build-essential cmake libglm-dev libglfw3-dev libgl1-mesa-dev \
    libx11-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev libxkbcommon-dev \
    libasound2-dev libssl-dev libx264-dev
```

*   `libglm-dev` is optional: if no system GLM is found, CMake downloads a pinned copy automatically (configure with `-DRAYMARCHVIBE_FETCH_GLM=OFF` to require a system install instead).
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
    From within the `build` directory, run `make`. This system has 12 processor cores.
    ```bash
    make -j12
    ```

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

### Building the Milk-Converter

The `Milk-Converter` is a command-line tool that converts MilkDrop presets (`.milk` files) to GLSL shaders (`.frag` files) that are compatible with RaymarchVibe.

1.  Navigate to the `Milk-Converter` directory:
    ```bash
    cd Milk-Converter
    ```

2.  Create a build directory and run cmake:
    ```bash
    mkdir build
    cd build
    cmake ..
    ```

3.  Build the project:
    ```bash
    make -j12
    ```

4.  Run the converter:
    ```bash
    ./MilkdropConverter /path/to/input.milk /path/to/output.frag
    ```


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
- **[CODE_REVIEW.md](documentation/CODE_REVIEW.md)** - Architectural overview and code quality assessment

## Development Notes

### Working with Shaders

The project is configured to automatically copy all files from the source `shaders/` directory to the application's runtime directory every time you build the project using `make`.

If you edit a `.frag` file, simply re-running `make` from the `build/` directory is sufficient to update it for the application.

```bash
# From within the build/ directory, after editing a shader
make -j12
```

You only need to re-run `cmake ..` before `make` if you add or remove C++ source files or change the project's structure in `CMakeLists.txt`.

## License

Copyright 2025 https://github.com/nicthegreatest/

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. See <http://www.gnu.org/licenses/>.
