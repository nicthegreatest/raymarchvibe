cmake_minimum_required(VERSION 3.15)
project(RaymarchVibe LANGUAGES C CXX)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/dummy-gl/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR}/dummy-gl/include")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)
include(ExternalProject)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ImGuiColorTextEdit
FetchContent_Declare(
  ImGuiColorTextEdit
  GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit.git
  GIT_TAG 0a88824f7de8d0bd11d8419066caa7d3469395c4 # Pinned to latest master commit as v2.1 tag not found
)
FetchContent_MakeAvailable(ImGuiColorTextEdit)

option(RAYMARCHVIBE_ENABLE_SSL "Enable SSL support for httplib (requires OpenSSL)" ON)

FetchContent_Declare(
  httplib_fetch
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3
)
# FetchContent_MakeAvailable should handle population for header-only libraries if set up correctly by the lib
FetchContent_MakeAvailable(httplib_fetch) # Simpler way if httplib's CMake supports it
# If httplib_fetch doesn't create a target, direct include is still needed.
# For now, assume FetchContent_MakeAvailable might not add include dirs for all header-only libs.
include_directories(${httplib_fetch_SOURCE_DIR})


FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4 # GLFW 3.4 is fine, could update to 3.4 if desired, but 3.3 is often a baseline
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # Don't build examples
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)  # Don't build tests
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)   # Don't build docs
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE) # Disable Wayland support
set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE) # Disable X11 support
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # Policy for FetchContent with modern GLFW

FetchContent_GetProperties(glfw)
if(NOT glfw_POPULATED)
  FetchContent_Populate(glfw)

  set(GLFW_SRC_CMAKELIST_PATH "${glfw_SOURCE_DIR}/src/CMakeLists.txt")
  message(STATUS "Attempting to patch GLFW CMakeLists.txt at: ${GLFW_SRC_CMAKELIST_PATH}")

  if(EXISTS ${GLFW_SRC_CMAKELIST_PATH})
      message(STATUS "File exists. Reading content.")
      file(READ ${GLFW_SRC_CMAKELIST_PATH} GLFW_SRC_CMAKELIST_CONTENT)
      set(ORIGINAL_CONTENT "${GLFW_SRC_CMAKELIST_CONTENT}")

      string(REPLACE "message(FATAL_ERROR \"RandR headers not found; install libxrandr development package\")"
                       "#message(FATAL_ERROR \"RandR headers not found; install libxrandr development package\")"
                       GLFW_SRC_CMAKELIST_CONTENT "${GLFW_SRC_CMAKELIST_CONTENT}")

      string(REPLACE "message(FATAL_ERROR \"Xinerama headers not found; install libxinerama development package\")"
                       "#message(FATAL_ERROR \"Xinerama headers not found; install libxinerama development package\")"
                       GLFW_SRC_CMAKELIST_CONTENT "${GLFW_SRC_CMAKELIST_CONTENT}")

      string(REPLACE "message(FATAL_ERROR \"XKB headers not found; install X11 development package\")"
                       "#message(FATAL_ERROR \"XKB headers not found; install X11 development package\")"
                       GLFW_SRC_CMAKELIST_CONTENT "${GLFW_SRC_CMAKELIST_CONTENT}")

      string(REPLACE "message(FATAL_ERROR \"Xcursor headers not found; install libxcursor development package\")"
                       "#message(FATAL_ERROR \"Xcursor headers not found; install libxcursor development package\")"
                       GLFW_SRC_CMAKELIST_CONTENT "${GLFW_SRC_CMAKELIST_CONTENT}")

      string(REPLACE "message(FATAL_ERROR \"XInput headers not found; install libxi development package\")"
                       "#message(FATAL_ERROR \"XInput headers not found; install libxi development package\")"
                       GLFW_SRC_CMAKELIST_CONTENT "${GLFW_SRC_CMAKELIST_CONTENT}")

      string(REPLACE "message(FATAL_ERROR \"X Shape headers not found; install libxext development package\")"
                       "#message(FATAL_ERROR \"X Shape headers not found; install libxext development package\")"
                       GLFW_SRC_CMAKELIST_CONTENT "${GLFW_SRC_CMAKELIST_CONTENT}")

      if(NOT "${GLFW_SRC_CMAKELIST_CONTENT}" STREQUAL "${ORIGINAL_CONTENT}")
          message(STATUS "Content was modified. Writing back to file.")
          file(WRITE ${GLFW_SRC_CMAKELIST_PATH} "${GLFW_SRC_CMAKELIST_CONTENT}")
          message(STATUS "File written.")
      else()
          message(WARNING "Patching did not modify the content of glfw's CMakeLists.txt. The string to replace was likely not found.")
      endif()
  else()
      message(WARNING "Could not find glfw's CMakeLists.txt to patch at ${GLFW_SRC_CMAKELIST_PATH}")
  endif()
endif()
add_subdirectory(${glfw_SOURCE_DIR} ${glfw_BINARY_DIR})

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.90.8 # Pinned to a specific tag
)
FetchContent_MakeAvailable(imgui)

# miniaudio
FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  
)
FetchContent_MakeAvailable(miniaudio) # This makes miniaudio_SOURCE_DIR available

# FFmpeg
set(FFMPEG_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/ffmpeg)

ExternalProject_Add(
    ffmpeg_project
    GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
    GIT_TAG n5.1.2
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND <SOURCE_DIR>/configure
        --prefix=${FFMPEG_INSTALL_DIR}
        --disable-programs
        --disable-doc
        --disable-shared
        --enable-static
        --disable-everything
        --enable-encoder=mpeg4
        --enable-encoder=aac
        --enable-muxer=mp4
        --enable-muxer=mov
        --enable-muxer=mpg
        --enable-protocol=file
        --enable-avcodec
        --enable-avformat
        --enable-avutil
        --enable-swresample
        --enable-swscale
        --extra-cflags="-fPIC"
        --disable-x86asm
        --disable-asm
    BUILD_COMMAND make -j4
    INSTALL_COMMAND make install
)

# Create imported target for FFmpeg
add_library(ffmpeg INTERFACE)
add_dependencies(ffmpeg ffmpeg_project)

target_include_directories(ffmpeg INTERFACE ${FFMPEG_INSTALL_DIR}/include)

# Find all the static libraries in the ffmpeg install lib dir
file(GLOB FFMPEG_LIBS "${FFMPEG_INSTALL_DIR}/lib/*.a")

target_link_libraries(ffmpeg INTERFACE ${FFMPEG_LIBS})

add_library(glad_lib STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c
)
target_include_directories(glad_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)



set(OPENGL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dummy-gl/include)
set(OPENGL_opengl_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/dummy-gl/lib/libGL.so)
set(OPENGL_glx_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/dummy-gl/lib/libGLX.so)

find_package(OpenGL)
find_package(Threads REQUIRED)

if(RAYMARCHVIBE_ENABLE_SSL)
  find_package(OpenSSL)
  if(OpenSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OpenSSL_VERSION}")
    include_directories(SYSTEM ${OpenSSL_INCLUDE_DIR})
  else()
    message(WARNING "OpenSSL not found, but RAYMARCHVIBE_ENABLE_SSL is ON. Shadertoy fetching might fail for HTTPS or require disabling SSL in httplib.")
  endif()
endif()

add_executable(RaymarchVibe
  src/main.cpp
  src/ShaderEffect.cpp
  src/ShaderParser.cpp
  src/Renderer.cpp          # <-- ADDED Renderer.cpp
  src/VideoRecorder.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp # Consider making this conditional for release builds
  ${imguicolortextedit_SOURCE_DIR}/TextEditor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imnodes/imnodes.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuiFileDialog/ImGuiFileDialog.cpp # Corrected path
  src/AudioSystem.cpp # Ensure this file defines MINIAUDIO_IMPLEMENTATION if using miniaudio header-only
  src/Utils.cpp
  src/ShadertoyIntegration.cpp
  src/NodeTemplates.cpp
  src/OutputNode.cpp
  themes.cpp # Added themes.cpp
)

target_include_directories(RaymarchVibe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imnodes
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuiFileDialog
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${httplib_fetch_SOURCE_DIR} # For cpp-httplib
  ${imguicolortextedit_SOURCE_DIR} # For ImGuiColorTextEdit
  ${miniaudio_SOURCE_DIR} # For miniaudio
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dj_fft
)

target_link_libraries(RaymarchVibe PRIVATE
  glad_lib
  nlohmann_json::nlohmann_json
  glfw # Modern CMake target for GLFW, trying non-namespaced
  Threads::Threads
  ffmpeg
  # imgui::imgui or imgui if provided by FetchContent for ImGui
  # imnodes is compiled directly
)
# If imgui FetchContent provides a linkable target (e.g., imgui::imgui or imgui), link it:
# target_link_libraries(RaymarchVibe PRIVATE imgui)


if(RAYMARCHVIBE_ENABLE_SSL AND OpenSSL_FOUND)
  target_link_libraries(RaymarchVibe PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  # Moved CPPHTTPLIB_OPENSSL_SUPPORT definition here for correctness
  target_compile_definitions(RaymarchVibe PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
endif()

target_compile_definitions(RaymarchVibe PRIVATE
  IMGUI_IMPL_OPENGL_LOADER_GLAD
  GLFW_INCLUDE_NONE
)
# Removed CPPHTTPLIB_OPENSSL_SUPPORT from here, it's now conditional

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
  add_custom_command(
    TARGET RaymarchVibe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:RaymarchVibe>/shaders"
    COMMENT "Copying shaders to build directory"
  )
endif()

if(APPLE)
  target_link_libraries(RaymarchVibe PRIVATE "-framework Cocoa -framework IOKit -framework CoreVideo")
endif()

if(MSVC)
  target_compile_options(RaymarchVibe PRIVATE /W3)
else()
  target_compile_options(RaymarchVibe PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "RaymarchVibe configured.")
message(STATUS "  Source directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Build directory:  ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "  SSL for httplib: ${RAYMARCHVIBE_ENABLE_SSL} (OpenSSL Found: ${OpenSSL_FOUND})")
