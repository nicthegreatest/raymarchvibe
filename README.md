RaymarchVibe ✨ BETA/EXPERIMENTAL

![app-screenshot](https://github.com/user-attachments/assets/f05a5f99-e497-449d-8df6-46cab469311d)



## Vision: Shader Exploration Made Intuitive

Sometimes, you just want to drag a slider instead of digging through lines of shader code.

This tool aims to:

* Demystify how different parameters affect the visual output of a shader.
* Help build an intuitive understanding of shader logic.
* Encourage experimentation and learning through direct manipulation and seeing the results in real-time.
* Lower the barrier to entry for creating and understanding complex visual effects.

Dive in, tweak, explore, and vibe with your shaders!

## Key Features

* Advanced Shader Editor: Powered by ImGuiColorTextEdit for GLSL syntax highlighting, line numbers, and error marking.
* Real-time Parameter Editing:
    * Control native shader uniforms & support for standard GLSL shaders.
    * Dynamically generate UI for Shadertoy uniforms defined with metadata comments (`// {"label":...}`).
    * Toggle and modify values of `#define` directives.
    * Edit global `const` variables (float, int, vecN) directly from the UI.
* Shadertoy Integration:
    * Fetch and load shaders directly from Shadertoy.com by ID or URL.
    * Automatic stripping of `#version` and `precision` from Shadertoy code for compatibility.
* File Management: Load, save, and create new shader files (native and Shadertoy templates).
* Interactive UI:
    * Collapsible sections for organized parameter control.
    * In-app console for status messages and compilation errors.
    * Help window with keybinds and usage instructions. [needs simplifying]
    * Keybinds: F12 (Fullscreen), Spacebar (Toggle GUI), F5 (Apply Editor Code).
    * "Reset Parameters" button to revert changes to shader defaults.

## Dependencies

RaymarchVibe relies on the following libraries, which are fetched automatically using CMake's `FetchContent` where possible:

* C++17 Compiler: (e.g., GCC, Clang, MSVC)
* CMake: (version 3.15 or higher)
* OpenGL: (version 3.3 or higher)
* GLFW: For windowing and input.
* Dear ImGui: For the graphical user interface.
* ImGuiColorTextEdit: For the enhanced shader code editor.
* GLAD: OpenGL Loading Library.
* cpp-httplib: For fetching shaders from Shadertoy.com (requires OpenSSL for HTTPS).
* nlohmann/json: For parsing JSON data (used with Shadertoy API).

System Libraries (typically needed for Linux builds):

* OpenGL development libraries (e.g `libgl1-mesa-dev`, `freeglut3-dev`)
* GLFW development libraries (e.g `libglfw3-dev`)
* X11 development libraries (e.g `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`)
* OpenSSL development libraries (e.g `libssl-dev`) for HTTPS Shadertoy fetching.

## Build Instructions (Linux)

1.  Clone the repository:

    bash
    git clone https://github.com/nicthegreatest/raymarchvibe
    cd raymarchvibe/
    

2.  Install Dependencies (if not already present):

    bash
    sudo apt update
    sudo apt install build-essential cmake libgl1-mesa-dev libglfw3-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libssl-dev
    
    (Package names may vary slightly on other distributions. For RHEL/Fedora use sudo dnf install.)

3.  Configure with CMake (cmake from same dir you placed CMakeLists.txt which should be raymarchvibe/):

    cmake 
    
    * If you don't have OpenSSL or wish to disable HTTPS for Shadertoy fetching, you can turn off the SSL option:

        cmake .. -DRAYMARCHVIBE_ENABLE_SSL=OFF


4.  Build the project (from within the dir of raymarchvibe/):
    
    make -j$(nproc) 
   
    (The '-j$(nproc)' flag tells make to use all available processor cores for a faster build, otherwise just use make)

5.  Run RaymarchVibe:

    ./RaymarchVibe

    The `shaders/` directory should be copied to the build directory alongside the executable for the sample shaders to load correctly.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## NOTES

I've temporarily added some sample shaders from the user balkhan (https://www.shadertoy.com/user/balkhan) on Shadertoy, because I thought they looked cool and needed something quick and easy to test with stored locally.
I'll work on some custom shader samples SOON

This is probably version 0.2 in a beta. There's some features that might crash or not work exactly as intended. This one's fresh out of the oven folks ...

## In the works

> Ability to handle buffers/textures.
* Audio reactivity.
> Switch between mouse input and WASD keys for camera control.
> Ability to handle larger more complex shaders in a way that preserves that 'instantaneous feel'.
> GUI themes
> Easier macOS and Windows compatability 
acOS and Windows compatability 
