cmake_minimum_required(VERSION 3.15)
project(RaymarchVibe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

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
  GIT_TAG master
  # GIT_SUBMODULES "" # Keeping this commented for now to be as minimal as possible with MakeAvailable
)
FetchContent_MakeAvailable(ImGuiColorTextEdit) # This should declare, populate if needed, and make target available

# The add_executable line (around your line 79) that uses ${ImGuiColorTextEdit_SOURCE_DIR}
# will fail if the _SOURCE_DIR is empty, leading to the //TextEditor.cpp error.
option(RAYMARCHVIBE_ENABLE_SSL "Enable SSL support for httplib (requires OpenSSL)" ON)

FetchContent_Declare(
  httplib_fetch
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3
)
FetchContent_GetProperties(httplib_fetch)
if(NOT httplib_fetch_POPULATED)
  FetchContent_Populate(httplib_fetch)
endif()
include_directories(${httplib_fetch_SOURCE_DIR})

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.90.8
)
FetchContent_MakeAvailable(imgui)

# ImGui Timeline Widget
FetchContent_Declare(
  imgui-timeline
  GIT_REPOSITORY https://github.com/nem0/imgui-timeline.git
  GIT_TAG v1.0.0 # Using latest release tag for stability
)
FetchContent_MakeAvailable(imgui-timeline)

# ImGui Node Editor Library
FetchContent_Declare(
  imnodes
  GIT_REPOSITORY https://github.com/Nelarius/imnodes.git
  GIT_TAG v0.5.1 # Use a specific tag for stability
)
FetchContent_MakeAvailable(imnodes)

add_library(glad_lib STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c
)
target_include_directories(glad_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(OpenGL REQUIRED)
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
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imguicolortextedit_SOURCE_DIR}/TextEditor.cpp
  ${imgui-timeline_SOURCE_DIR}/ImGuiTimeline.cpp
  ${imnodes_SOURCE_DIR}/imnodes.cpp # Added imnodes.cpp
)

target_include_directories(RaymarchVibe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${httplib_fetch_SOURCE_DIR}
  ${imguicolortextedit_SOURCE_DIR}
  ${imgui-timeline_SOURCE_DIR}
  ${imnodes_SOURCE_DIR} # Added include directory for imnodes
)

target_link_libraries(RaymarchVibe PRIVATE
  glad_lib
  nlohmann_json::nlohmann_json
  glfw
  OpenGL::GL
  Threads::Threads
)

if(RAYMARCHVIBE_ENABLE_SSL AND OpenSSL_FOUND)
  target_link_libraries(RaymarchVibe PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

target_compile_definitions(RaymarchVibe PRIVATE
  IMGUI_IMPL_OPENGL_LOADER_GLAD
)
if(RAYMARCHVIBE_ENABLE_SSL AND OpenSSL_FOUND)
  target_compile_definitions(RaymarchVibe PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
endif()

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