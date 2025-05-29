RaymarchVibe ✨ BETA/EXPERIMENTAL

## Vision: Shader Exploration Made Intuitive

Ever found yourself deep in GLSL shader code, wishing you could just *see* what happens when you tweak that one value? RaymarchVibe is born from that exact feeling. Sometimes, you just want to drag a slider instead of digging through lines of code.

This tool is designed to bridge the gap between complex shader logic and immediate visual feedback. With RaymarchVibe, you can:

* Edit shader parameters in real-time using intuitive GUI controls.
* See your code changes instantly reflected in the live render.
* Load shaders directly from Shadertoy or local files.
* Experiment with `#define` directives and global `const` variables without manually editing and recompiling every time.

## An Educational Tool for Shader Beginners

RaymarchVibe is particularly crafted for those new to the fascinating worlds of fragment shaders, raymarching, and procedural generation. By providing a hands-on, interactive environment, it aims to:

* Demystify how different parameters affect the visual output of a shader.
* Help build an intuitive understanding of shader logic.
* Encourage experimentation and learning through direct manipulation.
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
    * Help window with keybinds and usage instructions.
    * Keybinds: F12 (Fullscreen), Spacebar (Toggle GUI), F5 (Apply Editor Code).
    * "Reset Parameters" button to revert changes to shader defaults.

## Dependencies

RaymarchVibe relies on the following libraries, which are fetched automatically using CMake's `FetchContent` where possible:

* **C++17 Compiler** (e.g., GCC, Clang, MSVC)
* **CMake** (version 3.15 or higher)
* **OpenGL** (version 3.3 or higher)
* **GLFW:** For windowing and input.
* **Dear ImGui:** For the graphical user interface.
* **ImGuiColorTextEdit:** For the enhanced shader code editor.
* **GLAD:** OpenGL Loading Library.
* **cpp-httplib:** For fetching shaders from Shadertoy.com (requires OpenSSL for HTTPS).
* **nlohmann/json:** For parsing JSON data (used with Shadertoy API).

System Libraries (typically needed for Linux builds):

* OpenGL development libraries (e.g., `libgl1-mesa-dev`, `freeglut3-dev`)
* GLFW development libraries (e.g., `libglfw3-dev`)
* X11 development libraries (e.g., `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxi-dev`)
* OpenSSL development libraries (e.g., `libssl-dev`) for HTTPS Shadertoy fetching.

## Build Instructions (Linux)

1.  Clone the repository:

    bash
    git clone <your-repository-url>
    cd RaymarchVibe
    

2.  Install Dependencies (if not already present):

    bash
    sudo apt update
    sudo apt install build-essential cmake libgl1-mesa-dev libglfw3-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libssl-dev
    
    (Package names may vary slightly on other distributions. For RHEL/Fedora use sudo dnf install.)

3.  Configure with CMake:

    bash
    mkdir build
    cd build
    cmake .. 
    
    * If you don't have OpenSSL or wish to disable HTTPS for Shadertoy fetching, you can turn off the SSL option:

        cmake .. -DRAYMARCHVIBE_ENABLE_SSL=OFF


4.  Build the project:
    
    make -j$(nproc) 
   
    (The '-j$(nproc)' flag tells make to use all available processor cores for a faster build, otherwise just use make)

5.  Run RaymarchVibe:
    The executable will be in the `build` directory:

    ./RaymarchVibe

    The `shaders/` directory should be copied to the build directory alongside the executable for the sample shaders to load correctly.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## NOTES

I've temporarily added some sample shaders from the user balkhan (https://www.shadertoy.com/user/balkhan) on Shadertoy, because I thought they looked cool and needed something kinda complex to test with.
I'll work on some custom shader samples SOON

This is probably version 0.3 in a beta. There's some features that might crash or not work exactly as intended. This one's fresh out of the oven folks ...
