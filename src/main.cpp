// RaymarchVibe - Real-time Shader Exploration
// main.cpp - FINAL BUILD VERSION
#define IMGUI_DEFINE_MATH_OPERATORS // Ensure this is defined before any ImGui header is included

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <memory>
#include <queue>
#include <nlohmann/json.hpp> // For scene save/load
#include <filesystem> // For std::filesystem::path

// --- Core App Headers ---
#include "Effect.h"
#include "ShaderEffect.h"
#include "Renderer.h"
#include "ShadertoyIntegration.h"

// --- ImGui and Widget Headers ---
// #define IMGUI_DEFINE_MATH_OPERATORS // Moved to top of file
#include "imgui.h"
#include "imgui_internal.h"      // For DockBuilder API
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "ImGuiSimpleTimeline.h"
#include "imnodes.h"
#include "ImGuiFileDialog.h"
#include "AudioSystem.h" // Added the actual AudioSystem header
#include "NodeTemplates.h" // For node template factory functions

// Placeholder Audio System has been removed.

// --- Window dimensions ---
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// --- Global Constants ---
static const std::string PASSTHROUGH_EFFECT_NAME = "Passthrough (Final Output)";

// --- Forward Declarations ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

void RenderMenuBar();
void RenderShaderEditorWindow();
void RenderEffectPropertiesWindow();
void RenderTimelineWindow();
void RenderNodeEditorWindow();
void RenderConsoleWindow();
void RenderHelpWindow();
void RenderAudioReactivityWindow();

std::vector<Effect*> GetRenderOrder(const std::vector<Effect*>& activeEffects);
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log);
void ClearErrorMarkers();
static std::string LoadFileContent(const std::string& path, std::string& errorMsg); // Forward declaration

// --- Shader File I/O Helpers (New) ---
bool LoadShaderFromFileToEditor(const std::string& filePath, ShaderEffect* targetEffect, TextEditor& editor, std::string& consoleLog) {
    if (!targetEffect) {
        consoleLog = "Error: No shader effect selected to load into.";
        return false;
    }
    std::string errorMsg;
    std::string fileContent = LoadFileContent(filePath, errorMsg); // Existing helper to read file
    if (!errorMsg.empty()) {
        consoleLog = errorMsg;
        return false;
    }

    targetEffect->SetSourceFilePath(filePath);
    targetEffect->LoadShaderFromSource(fileContent); // Sets internal source code
    targetEffect->Load(); // Compiles, links, fetches uniforms, parses controls

    editor.SetText(fileContent);
    ClearErrorMarkers(); // Clear previous error markers

    const std::string& compileLog = targetEffect->GetCompileErrorLog();
    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
        // It's a compile error/warning if "Successfully" or "applied successfully" is not found
        consoleLog = "Loaded shader: " + filePath + ", but with issues:\n" + compileLog;
        editor.SetErrorMarkers(ParseGlslErrorLog(compileLog)); // Parse and set new error markers
    } else {
        consoleLog = "Loaded shader: " + filePath + " successfully.";
    }
    return true;
}

bool SaveEditorContentToFile(const std::string& filePath, TextEditor& editor, ShaderEffect* targetEffectToUpdatePath, std::string& consoleLog) {
    if (filePath.empty()) {
        consoleLog = "Error: File path for saving cannot be empty.";
        return false;
    }
    std::string shaderCode = editor.GetText();
    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        consoleLog = "Error: Could not open file for saving: " + filePath;
        return false;
    }
    outFile << shaderCode;
    outFile.close(); // Close file before checking stream state

    if (!outFile.good()) { // Check if write operation was successful
        consoleLog = "Error: Failed to write shader to file: " + filePath;
        return false;
    }

    consoleLog = "Shader saved to: " + filePath;
    if (targetEffectToUpdatePath) {
        targetEffectToUpdatePath->SetSourceFilePath(filePath);
    }
    return true;
}

static void ApplyCustomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text]                  = ImVec4(0.67f, 0.69f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_Border]                = ImVec4(0.23f, 0.28f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.29f, 0.62f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.40f, 0.76f, 0.82f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.29f, 0.62f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.40f, 0.76f, 0.82f, 1.00f);
    style.Colors[ImGuiCol_Button]                = ImVec4(0.23f, 0.28f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.29f, 0.62f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_Header]                = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.40f, 0.76f, 0.82f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.29f, 0.62f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_Separator]             = ImVec4(0.23f, 0.28f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.29f, 0.62f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.40f, 0.76f, 0.82f, 1.00f);
    style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.29f, 0.62f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_Tab]                   = ImVec4(0.23f, 0.28f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_TabActive]             = ImVec4(0.29f, 0.62f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.23f, 0.28f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.29f, 0.62f, 0.68f, 1.00f);
    style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.67f, 0.69f, 0.75f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.56f, 0.93f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.40f, 0.76f, 0.82f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.34f, 0.71f, 0.76f, 0.35f);
    style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    style.Colors[ImGuiCol_NavHighlight]          = ImVec4(0.34f, 0.71f, 0.76f, 1.00f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}


void SaveScene(const std::string& filePath);
void LoadScene(const std::string& filePath);


// --- Global State ---
static std::vector<std::unique_ptr<Effect>> g_scene;
static Effect* g_selectedEffect = nullptr;
static int g_selectedTimelineItem = -1;
static Renderer g_renderer;
static TextEditor g_editor;
static AudioSystem g_audioSystem;
static bool g_showGui = true;

// Window visibility flags
static bool g_showShaderEditorWindow = true;
static bool g_showEffectPropertiesWindow = true;
static bool g_showConsoleWindow = true;
// static bool g_showRenderViewWindow = true; // Render View is not a typical window, handled by main loop's final render pass.
static bool g_showTimelineWindow = false;
static bool g_showNodeEditorWindow = false;
static bool g_showAudioWindow = false;
static bool g_showHelpWindow = false;
static float g_globalAlpha = 1.0f; // For global window transparency

static bool g_enableAudioLink = false; // Changed to false
static std::string g_consoleLog = "Welcome to RaymarchVibe Demoscene Tool!";
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static bool g_timeline_paused = false; // Reverted to false for default playback
// static float g_timeline_time = 0.0f; // This will be g_timelineState.currentTime_seconds // Unused variable
static bool g_timelineControlActive = false; // Added for explicit timeline UI control

// --- Timeline State (New) ---
#include "Timeline.h" // For TimelineState struct
static TimelineState g_timelineState;

// Demo shaders list - moved to global static for access by multiple UI functions
static const std::vector<std::pair<std::string, std::string>> g_demoShaders = {
    {"Plasma V1", "shaders/raymarch_v1.frag"},
    {"Plasma V2", "shaders/raymarch_v2.frag"},
    {"Passthrough", "shaders/passthrough.frag"},
    {"Texture Test", "shaders/texture.frag"},
    {"Sample: Fractal 1", "shaders/samples/fractal1.frag"},
    {"Sample: Fractal 2", "shaders/samples/fractal2.frag"},
    {"Sample: Fractal 3", "shaders/samples/fractal3.frag"},
    {"Sample: Simple Red", "shaders/samples/simple_red.frag"},
    {"Sample: UV Pattern", "shaders/samples/uv_pattern.frag"},
    {"Sample: Cube Test", "shaders/samples/tester_cube.frag"}
};

// Shader Templates
static const std::string nativeShaderTemplate = R"(#version 330 core
out vec4 FragColor;

uniform vec2 iResolution; // viewport resolution (in pixels)
uniform float iTime;       // shader playback time (in seconds)
uniform vec4 iMouse;      // mouse pixel coords. xy: current (if MLB down), zw: click

void main() {
    vec2 uv = gl_FragCoord.xy / iResolution.xy;
    FragColor = vec4(uv.x, uv.y, 0.5 + 0.5 * sin(iTime), 1.0);
}
)";

static const std::string shadertoyShaderTemplate = R"(// Common uniforms provided by Shadertoy
// uniform vec3 iResolution; // viewport resolution (in pixels)
// uniform float iTime;       // shader playback time (in seconds)
// uniform float iTimeDelta;  // render time (in seconds)
// uniform int iFrame;        // shader playback frame
// uniform vec4 iMouse;      // mouse pixel coords. xy: current (if MLB down), zw: click
// uniform sampler2D iChannel0; // input channel. XX = 2D/Cube
// uniform sampler2D iChannel1; // input channel. XX = 2D/Cube
// uniform sampler2D iChannel2; // input channel. XX = 2D/Cube
// uniform sampler2D iChannel3; // input channel. XX = 2D/Cube
// uniform vec3 iChannelResolution[4]; // channel resolution (in pixels)
// uniform float iChannelTime[4];       // channel playback time (in seconds)

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
}
)";


// --- Helper Functions ---

// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon font.
static void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static Effect* FindEffectById(int effect_id) {
    for (const auto& effect_ptr : g_scene) {
        if (effect_ptr && effect_ptr->id == effect_id) {
            return effect_ptr.get();
        }
    }
    return nullptr;
}

// Helper to load file content
static std::string LoadFileContent(const std::string& path, std::string& errorMsg) {
    std::ifstream file(path);
    if (!file.is_open()) {
        errorMsg = "Error: Could not open file: " + path;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) { // Check stream state after reading
        errorMsg = "Error: Failed to read file content from: " + path;
        return "";
    }
    errorMsg = ""; // Clear error message on success
    return buffer.str();
}

// Helper function to check for GL errors
void checkGLError(const std::string& label, bool logToGlobalConsole = true) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        std::string errorStr;
        switch(err) {
            case GL_INVALID_ENUM: errorStr = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "INVALID_OPERATION"; break;
            // GL_STACK_OVERFLOW and GL_STACK_UNDERFLOW are deprecated and may not be defined
            // case GL_STACK_OVERFLOW: errorStr = "STACK_OVERFLOW"; break;
            // case GL_STACK_UNDERFLOW: errorStr = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY: errorStr = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "INVALID_FRAMEBUFFER_OPERATION"; break;
            default: errorStr = "UNKNOWN_ERROR (" + std::to_string(err) + ")"; break;
        }
        std::string logMsg = "GL_ERROR (" + label + "): " + errorStr;
        std::cerr << logMsg << std::endl;
        if (logToGlobalConsole && g_consoleLog.size() < 4096) { // Prevent g_consoleLog from getting too huge
             g_consoleLog += logMsg + "\n";
        }
    }
}


// --- UI Window Implementations ---

void RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Shader...")) {
                // Args: key, title, filters, path, fileName, count, flags, userDatas
                ImGuiFileDialog::Instance()->OpenDialog("LoadShaderDlgKey", "Choose Shader File", ".frag,.fs,.glsl,.*", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
            }
            bool canSave = (g_selectedEffect && dynamic_cast<ShaderEffect*>(g_selectedEffect));
            if (ImGui::MenuItem("Save Shader", nullptr, false, canSave)) {
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    const std::string& currentPath = se->GetSourceFilePath();
                    if (!currentPath.empty() && currentPath.find("shadertoy://") == std::string::npos && currentPath != "dynamic_source" && currentPath.rfind("Untitled", 0) != 0) {
                        SaveEditorContentToFile(currentPath, g_editor, se, g_consoleLog);
                    } else { // No valid path or is an "Untitled" default, so effectively "Save As"
                        ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
                    }
                }
            }
            if (ImGui::MenuItem("Save Shader As...", nullptr, false, canSave)) {
                 ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
            }

            ImGui::Separator();
            // --- Load Demo Shader Submenu ---
            if (ImGui::BeginMenu("Load Demo Shader")) {
                for (const auto& demo : g_demoShaders) { // Use global g_demoShaders
                    if (ImGui::MenuItem(demo.first.c_str())) {
                        if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                            // Using ShaderEffect's internal loader which also sets file path
                            if (se->LoadShaderFromFile(demo.second)) {
                                se->Load(); // This calls ApplyShaderCode
                                g_editor.SetText(se->GetShaderSource());
                                ClearErrorMarkers();
                                g_consoleLog = "Loaded demo shader: " + demo.first;
                                if (!se->GetCompileErrorLog().empty() && se->GetCompileErrorLog().find("Successfully") == std::string::npos && se->GetCompileErrorLog().find("applied successfully") == std::string::npos) {
                                    g_consoleLog += "\nCompile Log: " + se->GetCompileErrorLog();
                                    g_editor.SetErrorMarkers(ParseGlslErrorLog(se->GetCompileErrorLog()));
                                }
                            } else {
                                g_consoleLog = "Error loading demo shader " + demo.first + ". Log: " + se->GetCompileErrorLog();
                            }
                        } else {
                             // Create a new ShaderEffect if none is selected
                            auto newEffect = std::make_unique<ShaderEffect>(demo.second, SCR_WIDTH, SCR_HEIGHT);
                            newEffect->name = demo.first;
                            newEffect->Load(); // This will load from file path and apply
                            if (!newEffect->GetCompileErrorLog().empty() && newEffect->GetCompileErrorLog().find("Successfully") == std::string::npos && newEffect->GetCompileErrorLog().find("applied successfully") == std::string::npos) {
                                g_consoleLog = "Error loading demo shader " + demo.first + " into new effect. Log: " + newEffect->GetCompileErrorLog();
                            } else {
                                g_editor.SetText(newEffect->GetShaderSource());
                                ClearErrorMarkers();
                                g_scene.push_back(std::move(newEffect));
                                g_selectedEffect = g_scene.back().get(); // Select the new effect
                                g_consoleLog = "Loaded demo shader '" + demo.first + "' into a new effect.";
                            }
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene...")) {
                ImGuiFileDialog::Instance()->OpenDialog("SaveSceneDlgKey", "Save Scene File", ".json");
            }
            if (ImGui::MenuItem("Load Scene...")) {
                ImGuiFileDialog::Instance()->OpenDialog("LoadSceneDlgKey", "Load Scene File", ".json");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) { glfwSetWindowShouldClose(glfwGetCurrentContext(), true); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Shader Editor", nullptr, &g_showShaderEditorWindow);
            ImGui::MenuItem("Effect Properties", nullptr, &g_showEffectPropertiesWindow);
            ImGui::MenuItem("Console", nullptr, &g_showConsoleWindow);
            ImGui::Separator();
            ImGui::MenuItem("Timeline", nullptr, &g_showTimelineWindow);
            ImGui::MenuItem("Node Editor", nullptr, &g_showNodeEditorWindow);
            ImGui::MenuItem("Audio Reactivity", nullptr, &g_showAudioWindow);
            ImGui::Separator();
            ImGui::MenuItem("Toggle All GUI", "Spacebar", &g_showGui);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About RaymarchVibe", nullptr, &g_showHelpWindow);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Appearance")) {
            ImGui::SliderFloat("Global Alpha", &g_globalAlpha, 0.10f, 1.0f);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // --- Handle File Dialogs for Shader Load/Save ---
    if (ImGuiFileDialog::Instance()->Display("LoadShaderDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string justFileName = std::filesystem::path(filePathName).filename().string();

            ShaderEffect* se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            if (!se) { // If no ShaderEffect is selected, or if current is not a ShaderEffect
                g_consoleLog = "No ShaderEffect selected. Creating a new one for: " + justFileName + "\n";
                auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT); // Path will be set by LoadShaderFromFileToEditor
                newEffect->name = justFileName.empty() ? "Untitled Shader" : justFileName;

                g_scene.push_back(std::move(newEffect));
                g_selectedEffect = g_scene.back().get();
                se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            }

            if (se) { // se can be the initially selected one or the newly created one
                LoadShaderFromFileToEditor(filePathName, se, g_editor, g_consoleLog);
            } else {
                // This should not happen if the logic above is correct
                g_consoleLog = "Critical error: Failed to get/create a ShaderEffect for loading.\n";
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("SaveShaderAsDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            SaveEditorContentToFile(filePathName, g_editor, dynamic_cast<ShaderEffect*>(g_selectedEffect), g_consoleLog);
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void RenderShaderEditorWindow() {
    static char filePathBuffer_SaveAs[512] = ""; // Buffer for Save As path - MOVED TO FUNCTION SCOPE
    // static char shadertoyIdBuffer[256] = ""; // Already static at its use point, keep it there or move here too for consistency
    // static int currentSampleIndex = 0; // Already static at its use point
    static int lineToGo = 1; // Declaration for Go To Line functionality

    ImGui::Begin("Shader Editor");

    // Toolbar for Apply, Find, Go To Line
    if (ImGui::Button("Apply (F5)")) {
        if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            se->ApplyShaderCode(g_editor.GetText());
            const std::string& log = se->GetCompileErrorLog();
            if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) {
                g_editor.SetErrorMarkers(ParseGlslErrorLog(log));
                g_consoleLog = log;
            } else {
                ClearErrorMarkers();
                g_consoleLog = "Shader applied successfully!";
            }
        }
    }
    ImGui::SameLine();

    // Go To Line functionality (placeholder from old main.cpp, real one is in menu bar now)
    // static int lineToGo = 1;
    ImGui::PushItemWidth(80);
    ImGui::InputInt("##GoToLine", &lineToGo, 0, 0); // No step buttons
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Go")) {
        if (lineToGo > 0) {
            g_editor.SetCursorPosition(TextEditor::Coordinates(lineToGo - 1, 0)); // Line numbers are 0-indexed in SetCursorPosition
        }
    }
    ImGui::SameLine();
    ImGui::Text("Mouse: (%.1f, %.1f)", g_mouseState[0], g_mouseState[1]);

    ImGui::Separator(); // Separator after the toolbar

    g_editor.Render("TextEditor");
    ImGui::Separator();
    ImGui::Text("Fetch Shadertoy:");
    ImGui::SameLine();
    ImGui::Text("Mouse: (%.1f, %.1f)", g_mouseState[0], g_mouseState[1]);

    ImGui::Separator();

    // --- Shadertoy Fetching UI ---
    if (ImGui::CollapsingHeader("Load from Shadertoy")) {
        static char shadertoyIdBuffer[256] = ""; // Moved static buffer here
        ImGui::InputTextWithHint("##ShadertoyInput", "Shadertoy ID (e.g. Ms2SD1) or Full URL", shadertoyIdBuffer, sizeof(shadertoyIdBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Fetch & Apply##ShadertoyApply")) {
            std::string idOrUrl = shadertoyIdBuffer;
            if (!idOrUrl.empty()) {
                std::string shaderId = ShadertoyIntegration::ExtractId(idOrUrl);
                if (!shaderId.empty()) {
                    g_consoleLog = "Fetching Shadertoy " + shaderId + "...";
                    std::string fetchError;
                    // API Key can be a global static or configured elsewhere if needed
                    std::string fetchedCode = ShadertoyIntegration::FetchCode(shaderId, "", fetchError);

                    if (!fetchedCode.empty()) {
                        ShaderEffect* se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
                        if (!se) { // If no effect or wrong type, create a new one
                            auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT, true);
                            newEffect->name = "Shadertoy - " + shaderId;
                            g_scene.push_back(std::move(newEffect));
                            g_selectedEffect = g_scene.back().get();
                            se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
                        }

                        if (se) {
                            se->SetSourceFilePath("shadertoy://" + shaderId);
                            se->LoadShaderFromSource(fetchedCode); // This sets m_shaderSourceCode
                            se->SetShadertoyMode(true); // Explicitly set Shadertoy mode
                            se->Load(); // This compiles, links, fetches uniforms, parses controls

                            g_editor.SetText(se->GetShaderSource());
                            ClearErrorMarkers();
                            const std::string& compileLog = se->GetCompileErrorLog();
                            if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                                g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                                g_consoleLog = "Shadertoy '" + shaderId + "' fetched, but application failed. Log:\n" + compileLog;
                            } else {
                                g_consoleLog = "Shadertoy '" + shaderId + "' fetched and applied!";
                            }
                        }
                    } else {
                         g_consoleLog = fetchError;
                         if (g_consoleLog.empty()) {
                            g_consoleLog = "Failed to retrieve code for Shadertoy ID: " + shaderId;
                         }
                    }
                } else {
                    g_consoleLog = "Invalid Shadertoy ID or URL format.";
                }
            } else {
                g_consoleLog = "Please enter a Shadertoy ID or URL.";
            }
        }
        // Note: "Load to Editor" button from old code can be added if distinct functionality is needed.
        // For now, "Fetch & Apply" covers the main use case.
        ImGui::TextWrapped("Note: Requires network. Fetches shaders by ID from Shadertoy.com.");
        ImGui::Spacing();
    }

    // --- Sample Shader Loading UI ---
    if (ImGui::CollapsingHeader("Load Sample Shader")) {
        static int currentSampleIndex = 0; // UI state for the combo box
        if (ImGui::BeginCombo("##SampleShaderCombo",
            (currentSampleIndex >= 0 && static_cast<size_t>(currentSampleIndex) < g_demoShaders.size()) ? g_demoShaders[currentSampleIndex].first.c_str() : "Select Sample...")) {
            for (size_t n = 0; n < g_demoShaders.size(); n++) {
                const bool is_selected = (currentSampleIndex == static_cast<int>(n));
                if (ImGui::Selectable(g_demoShaders[n].first.c_str(), is_selected)) currentSampleIndex = n;
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load & Apply Sample##Editor")) {
            if (currentSampleIndex >= 0 && static_cast<size_t>(currentSampleIndex) < g_demoShaders.size()) {
                const auto& demo = g_demoShaders[currentSampleIndex];
                ShaderEffect* se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
                if (!se) { // Create new if no suitable effect selected
                    auto newEffect = std::make_unique<ShaderEffect>(demo.second, SCR_WIDTH, SCR_HEIGHT);
                    newEffect->name = demo.first;
                    g_scene.push_back(std::move(newEffect));
                    g_selectedEffect = g_scene.back().get();
                    se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
                }

                if (se) {
                    if (se->LoadShaderFromFile(demo.second)) {
                        se->Load(); // This calls ApplyShaderCode
                        g_editor.SetText(se->GetShaderSource());
                        ClearErrorMarkers();
                        g_consoleLog = "Sample '" + demo.first + "' loaded.";
                        const std::string& compileLog = se->GetCompileErrorLog();
                        if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                            g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                            g_consoleLog += " Applied with errors/warnings:\n" + compileLog;
                        } else {
                            g_consoleLog += " Applied successfully!";
                        }
                    } else {
                        g_consoleLog = "ERROR: Failed to load sample '" + demo.first + "'. Log: " + se->GetCompileErrorLog();
                    }
                }
            } else {
                g_consoleLog = "Please select a valid sample.";
            }
        }
        ImGui::Spacing();
    }

    // --- New Shader UI ---
    if (ImGui::CollapsingHeader("New Shader")) {
        if (ImGui::Button("New Native Shader")) {
            ShaderEffect* se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            if (!se) { /* Create new logic */
                auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT, false);
                newEffect->name = "Untitled Native";
                g_scene.push_back(std::move(newEffect));
                g_selectedEffect = g_scene.back().get();
                se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            }
            if(se) {
                se->SetSourceFilePath("Untitled_Native.frag");
                se->LoadShaderFromSource(nativeShaderTemplate);
                se->SetShadertoyMode(false);
                se->Load(); // Apply
                g_editor.SetText(nativeShaderTemplate);
                ClearErrorMarkers();
                g_consoleLog = "Native template loaded. Press Apply (F5) if needed or start editing.";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("New Shadertoy Shader")) {
            ShaderEffect* se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
             if (!se) { /* Create new logic */
                auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT, true);
                newEffect->name = "Untitled Shadertoy";
                g_scene.push_back(std::move(newEffect));
                g_selectedEffect = g_scene.back().get();
                se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            }
            if (se) {
                se->SetSourceFilePath("Untitled_Shadertoy.frag");
                se->LoadShaderFromSource(shadertoyShaderTemplate);
                se->SetShadertoyMode(true);
                se->Load(); // Apply
                g_editor.SetText(shadertoyShaderTemplate);
                ClearErrorMarkers();
                g_consoleLog = "Shadertoy template loaded. Press Apply (F5) if needed or start editing.";
            }
        }
        ImGui::Spacing();
    }

    // --- In-Window Save UI ---
    if (ImGui::CollapsingHeader("Save Current Shader")) {
        static char filePathBuffer_SaveAs[512] = ""; // Buffer for Save As path
        if (g_selectedEffect && dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            ImGui::Text("Current Path: %s", dynamic_cast<ShaderEffect*>(g_selectedEffect)->GetSourceFilePath().c_str());
            if (ImGui::Button("Save Current")) { // Identical to File > Save Shader
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    const std::string& currentPath = se->GetSourceFilePath();
                    if (!currentPath.empty() && currentPath.find("shadertoy://") == std::string::npos && currentPath != "dynamic_source" && currentPath.rfind("Untitled", 0) != 0) {
                        std::ofstream outFile(currentPath);
                        if (outFile.is_open()) {
                            outFile << g_editor.GetText();
                            outFile.close();
                            g_consoleLog = outFile.good() ? ("Saved to: " + currentPath) : ("ERROR saving to: " + currentPath);
                        } else { g_consoleLog = "ERROR opening file for saving: " + currentPath; }
                    } else { // No valid path or is untitled, trigger Save As
                        strncpy(filePathBuffer_SaveAs, se->GetSourceFilePath().rfind("Untitled", 0) == 0 ? "" : se->GetSourceFilePath().c_str(), sizeof(filePathBuffer_SaveAs) -1);
                        ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey_Editor", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
                    }
                }
            }
            ImGui::InputText("Save As Path", filePathBuffer_SaveAs, sizeof(filePathBuffer_SaveAs));
            ImGui::SameLine();
            if (ImGui::Button("Save As...##Editor")) { // Identical to File > Save Shader As
                std::string saveAsPathStr(filePathBuffer_SaveAs);
                if (saveAsPathStr.empty()) {
                    ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey_Editor", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
                } else {
                    std::ofstream outFile(saveAsPathStr);
                    if (outFile.is_open()) {
                        outFile << g_editor.GetText();
                        outFile.close();
                        if (outFile.good()) {
                            g_consoleLog = "Saved to: " + saveAsPathStr;
                            if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                                se->SetSourceFilePath(saveAsPathStr);
                            }
                        } else { g_consoleLog = "ERROR saving to: " + saveAsPathStr; }
                    } else { g_consoleLog = "ERROR opening file for saving: " + saveAsPathStr; }
                }
            }
        } else {
            ImGui::TextDisabled("No shader effect selected to save.");
        }
        ImGui::Spacing();
    }

    // Handle the Save As dialog opened from within Shader Editor window
    if (ImGuiFileDialog::Instance()->Display("SaveShaderAsDlgKey_Editor")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string shaderCode = g_editor.GetText();
            std::ofstream outFile(filePathName);
            if (outFile.is_open()) {
                outFile << shaderCode;
                outFile.close();
                if (outFile.good()) {
                    g_consoleLog = "Shader saved to: " + filePathName;
                     if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                        se->SetSourceFilePath(filePathName);
                        strncpy(filePathBuffer_SaveAs, filePathName.c_str(), sizeof(filePathBuffer_SaveAs) -1);
                    }
                } else { g_consoleLog = "Error: Failed to write shader to file: " + filePathName; }
            } else { g_consoleLog = "Error: Could not open file for saving: " + filePathName; }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::Separator(); // Separator before the editor itself
    // The original "Shader Code Editor" text and Apply (F5) button are kept from existing code.
    // The editor.Render call is also kept.
    // The status message display (shaderLoadError) from old code can be integrated with g_consoleLog or displayed separately.
    // For now, relying on g_consoleLog.

    g_editor.Render("TextEditor"); // Keep existing editor render call
    // The "Apply (F5)" button and Mouse position text are already above this section now.
    ImGui::End();
}

void RenderEffectPropertiesWindow() {
    ImGui::Begin("Effect Properties");
    if (g_selectedEffect) {
        if (ImGui::Button("Reset Parameters")) {
            g_selectedEffect->ResetParameters();
            if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                g_editor.SetText(se->GetShaderSource());
                ClearErrorMarkers();
            }
            g_consoleLog = "Parameters reset successfully.";
        }
        ImGui::Separator();
        g_selectedEffect->RenderUI();
    } else {
        ImGui::Text("No effect selected.");
    }
    ImGui::End();
}

void RenderTimelineWindow() {
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::Checkbox("Enable Timeline Master Control (Sets iTime)", &g_timelineState.isEnabled);
    ImGui::SameLine(); HelpMarker("If checked, the timeline's current time will be used as the master 'iTime' for shaders. Otherwise, shaders use system time.");

    ImGui::Checkbox("Enable Timeline UI Playback Control", &g_timelineControlActive);
    ImGui::SameLine(); HelpMarker("When enabled, use Pause/Play/Reset below to control this timeline's playhead. This playhead may or may not be the master 'iTime'.");

    // Disable timeline playback UI controls if g_timelineControlActive is false
    if (!g_timelineControlActive) {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }

    if (ImGui::Button(g_timeline_paused ? "Play" : "Pause")) {
        if (g_timelineControlActive) g_timeline_paused = !g_timeline_paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        if (g_timelineControlActive) g_timelineState.currentTime_seconds = 0.0f; // Use TimelineState
    }

    if (!g_timelineControlActive) {
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
    }

    ImGui::SameLine();
    ImGui::Text("Time: %.2f", g_timelineState.currentTime_seconds); // Use TimelineState

    // Add controls for zoom and scroll (basic for now)
    ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
    ImGui::PushItemWidth(100);
    ImGui::DragFloat("Zoom", &g_timelineState.zoomLevel, 0.1f, 0.1f, 10.0f);
    ImGui::SameLine();
    ImGui::DragFloat("Scroll", &g_timelineState.horizontalScroll_seconds, 0.5f, 0.0f, g_timelineState.totalDuration_seconds);
    ImGui::PopItemWidth();

    ImGui::Separator();
    std::vector<ImGui::TimelineItem> timelineItems;
    std::vector<int> tracks;
    tracks.resize(g_scene.size());
    for (size_t i = 0; i < g_scene.size(); ++i) {
        if (!g_scene[i]) { continue; }
        tracks[i] = i % 4; // Simple track assignment
        timelineItems.push_back({ g_scene[i]->name, &g_scene[i]->startTime, &g_scene[i]->endTime, &tracks[i] });
    }

    // TODO: Update this call with new parameters for zoom and scroll
    // For now, the timeline's total view is 0 to g_timelineState.totalDuration_seconds
    // The ImGuiSimpleTimeline will be modified to use these implicitly or explicitly
    if (ImGui::SimpleTimeline("Scene", timelineItems, &g_timelineState.currentTime_seconds, &g_selectedTimelineItem,
                               4, // num_tracks
                               0.0f, // sequence_total_start_time_seconds
                               g_timelineState.totalDuration_seconds, // sequence_total_end_time_seconds
                               g_timelineState.horizontalScroll_seconds, // Pass scroll
                               g_timelineState.zoomLevel                 // Pass zoom
                               )) {
        if (g_selectedTimelineItem >= 0 && static_cast<size_t>(g_selectedTimelineItem) < g_scene.size()) {
            g_selectedEffect = g_scene[g_selectedTimelineItem].get();
            if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                g_editor.SetText(se->GetShaderSource());
                ClearErrorMarkers();
            }
        }
    }
    ImGui::End();
}

void RenderNodeEditorWindow() {
    ImGui::Begin("Node Editor");

    // Use a child window to make the empty space context-clickable
    ImGui::BeginChild("NodeEditorCanvas", ImVec2(0,0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImNodes::BeginNodeEditor();
    for (const auto& effect_ptr : g_scene) {
        if (!effect_ptr) continue;
        ImNodes::BeginNode(effect_ptr->id);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(effect_ptr->name.c_str());
        ImNodes::EndNodeTitleBar();
        if (effect_ptr->GetOutputPinCount() > 0) {
            ImNodes::BeginOutputAttribute(effect_ptr->id * 10); ImGui::Text("out"); ImNodes::EndOutputAttribute();
        }
        for (int i = 0; i < effect_ptr->GetInputPinCount(); ++i) {
            ImNodes::BeginInputAttribute(effect_ptr->id * 10 + 1 + i); ImGui::Text("in %d", i); ImNodes::EndInputAttribute();
        }
        ImNodes::EndNode();
    }
    int link_id_counter = 1;
    for (const auto& effect_ptr : g_scene) {
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            const auto& inputs = se->GetInputs();
            for (size_t i = 0; i < inputs.size(); ++i) {
                if (inputs[i]) {
                    ImNodes::Link(link_id_counter++, inputs[i]->id * 10, se->id * 10 + 1 + i);
                }
            }
        }
    }
    ImNodes::EndNodeEditor();
    int start_attr, end_attr;
    if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
        int start_node_id = start_attr / 10; int end_node_id = end_attr / 10;
        Effect* start_effect = FindEffectById(start_node_id);
        Effect* end_effect = FindEffectById(end_node_id);
        bool start_is_output = (start_attr % 10 == 0);
        bool end_is_input = (end_attr % 10 != 0);
        if (start_effect && end_effect && start_node_id != end_node_id) {
            if (start_is_output && end_is_input) { end_effect->SetInputEffect((end_attr % 10) - 1, start_effect); }
            else if (!start_is_output && !end_is_input) { start_effect->SetInputEffect((start_attr % 10) - 1, end_effect); }
        }
    }

    // Context menu for adding nodes
    if (ImGui::BeginPopupContextWindow("NodeEditorContextMenu")) {
        if (ImGui::BeginMenu("Add Effect")) {
            if (ImGui::BeginMenu("Generators")) {
                if (ImGui::MenuItem("Basic Plasma")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreatePlasmaBasicEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        ImNodes::SetNodeScreenSpacePos(newEffectRawPtr->id, ImGui::GetMousePos());
                    }
                }
                if (ImGui::MenuItem("Simple Color")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateSimpleColorEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        ImNodes::SetNodeScreenSpacePos(newEffectRawPtr->id, ImGui::GetMousePos());
                    }
                }
                if (ImGui::MenuItem("Value Noise")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateValueNoiseEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        ImNodes::SetNodeScreenSpacePos(newEffectRawPtr->id, ImGui::GetMousePos());
                    }
                }
                if (ImGui::MenuItem("Circle Shape")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateCircleShapeEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        ImNodes::SetNodeScreenSpacePos(newEffectRawPtr->id, ImGui::GetMousePos());
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Filters")) {
                if (ImGui::MenuItem("Invert Color")) {
                     auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateInvertColorEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        ImNodes::SetNodeScreenSpacePos(newEffectRawPtr->id, ImGui::GetMousePos());
                    }
                }
                if (ImGui::MenuItem("Brightness/Contrast")) {
                     auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateBrightnessContrastEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        ImNodes::SetNodeScreenSpacePos(newEffectRawPtr->id, ImGui::GetMousePos());
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Image")) { // Changed from "Image Operations"
                if (ImGui::MenuItem("Texture Passthrough")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateTexturePassthroughEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        ImNodes::SetNodeScreenSpacePos(newEffectRawPtr->id, ImGui::GetMousePos());
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImNodes::EndNodeEditor(); // End node editor before ending child window
    ImGui::EndChild(); // End NodeEditorCanvas
    ImGui::End(); // End Node Editor window
}

void RenderConsoleWindow() {
    ImGui::Begin("Console");
    if (ImGui::Button("Clear")) { g_consoleLog = ""; }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) { ImGui::SetClipboardText(g_consoleLog.c_str()); }
    ImGui::Separator();
    ImGui::TextWrapped("%s", g_consoleLog.c_str());
    ImGui::End();
}

void RenderHelpWindow() {
    ImGui::Begin("About RaymarchVibe", &g_showHelpWindow, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("RaymarchVibe Demoscene Tool");
    ImGui::Separator();
    ImGui::Text("Created by nicthegreatest & Gemini.");
    ImGui::Separator();
    if (ImGui::Button("Close")) { g_showHelpWindow = false; }
    ImGui::End();
}

void RenderAudioReactivityWindow() {
    ImGui::Begin("Audio Reactivity", &g_showAudioWindow);
    ImGui::Checkbox("Enable Audio Link (iAudioAmp)", &g_enableAudioLink);
    ImGui::Separator();
    int currentSourceIndex = g_audioSystem.GetCurrentAudioSourceIndex();
    if (ImGui::RadioButton("Microphone", &currentSourceIndex, 0)) { g_audioSystem.SetCurrentAudioSourceIndex(0); }
    ImGui::SameLine();
    if (ImGui::RadioButton("Audio File", &currentSourceIndex, 2)) { g_audioSystem.SetCurrentAudioSourceIndex(2); }
    ImGui::Separator();
    if (currentSourceIndex == 0) {
        if (ImGui::CollapsingHeader("Microphone Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& devices = g_audioSystem.GetCaptureDeviceGUINames();
            int* selectedDeviceIndex = g_audioSystem.GetSelectedActualCaptureDeviceIndexPtr();
            if (devices.empty()) {
                ImGui::Text("No capture devices found.");
            } else if (ImGui::BeginCombo("Input Device", (*selectedDeviceIndex >= 0 && (size_t)*selectedDeviceIndex < devices.size()) ? devices[*selectedDeviceIndex] : "None")) {
                for (size_t i = 0; i < devices.size(); ++i) {
                    const bool is_selected = (*selectedDeviceIndex == (int)i);
                    if (ImGui::Selectable(devices[i], is_selected)) {
                        if (*selectedDeviceIndex != (int)i) {
                            g_audioSystem.SetSelectedActualCaptureDeviceIndex(i);
                            g_audioSystem.InitializeAndStartSelectedCaptureDevice();
                        }
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
    } else if (currentSourceIndex == 2) {
        if (ImGui::CollapsingHeader("Audio File Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            char* audioFilePath = g_audioSystem.GetAudioFilePathBuffer();
            ImGui::InputText("File Path", audioFilePath, AUDIO_FILE_PATH_BUFFER_SIZE);
            ImGui::SameLine();
            if (ImGui::Button("Load##AudioFile")) { g_audioSystem.LoadWavFile(audioFilePath); }
            ImGui::Text("Status: %s", g_audioSystem.IsAudioFileLoaded() ? "Loaded" : "Not Loaded");
        }
    }
    ImGui::Separator();
    ImGui::Text("Live Amplitude:");
    ImGui::ProgressBar(g_audioSystem.GetCurrentAmplitude(), ImVec2(-1.0f, 0.0f));
    ImGui::End();
}

// Main Application
int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    g_renderer.Init();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyCustomStyle(); // Apply custom theme
    ImNodes::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Commenting these out again as they cause "not declared in scope" errors,
    // suggesting a deeper issue with ImGui version/include paths.
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;


    ImGui::StyleColorsDark();
    // When viewports are enabled we tweak WindowRounding/WindowBg to make them look like main window.
    // ImGuiStyle& style = ImGui::GetStyle(); // Unused due to docking code being commented out
    // Commenting out viewport-specific style changes as ViewportsEnable flag is causing issues
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    // {
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    g_audioSystem.Initialize();

    // Minimal Startup Node: A single Passthrough Output ShaderEffect
    auto passthroughEffect = std::make_unique<ShaderEffect>("shaders/passthrough.frag", SCR_WIDTH, SCR_HEIGHT);
    passthroughEffect->name = PASSTHROUGH_EFFECT_NAME; // Use the constant
    passthroughEffect->startTime = 0.0f;
    // Make it span a default total duration, or a very long time if totalDuration is dynamic
    passthroughEffect->endTime = g_timelineState.totalDuration_seconds;
    g_scene.push_back(std::move(passthroughEffect));

    // Load all effects in the scene (just the passthrough for now)
    for (const auto& effect_ptr : g_scene) {
        effect_ptr->Load();
    }

    // Select the passthrough effect and load its code into the editor
    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get(); // Should be the passthrough effect
        if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource());
            ClearErrorMarkers(); // Clear any previous errors
            // Log any compile issues with the passthrough shader itself
            const std::string& compileLog = se->GetCompileErrorLog();
            if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                g_consoleLog += "Default passthrough shader issue: " + compileLog + "\n";
            }
        }
    }

    // No auto-linking needed for a single node setup

    float deltaTime = 0.0f, lastFrameTime = 0.0f;
    // static bool first_time_docking = true; // Unused due to docking code being commented out

    while(!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // Advance g_timelineState.currentTime_seconds based on its own UI controls (play/pause)
        // This happens if the timeline's UI playback controls are active AND it's not paused.
        if (g_timelineControlActive && !g_timeline_paused) {
            g_timelineState.currentTime_seconds += deltaTime;
        }

        // Loop timeline's current time if its UI controls are active or if it's the master time source.
        // This ensures that even if it's just being scrubbed (UI active) but not master, it still loops.
        // And if it IS master, it must loop.
        if (g_timelineControlActive || g_timelineState.isEnabled) {
             // Basic looping, ensure it stays within [0, totalDuration)
             if (g_timelineState.totalDuration_seconds > 0.0f) {
                 g_timelineState.currentTime_seconds = fmod(g_timelineState.currentTime_seconds, g_timelineState.totalDuration_seconds);
                 if (g_timelineState.currentTime_seconds < 0.0f) {
                     g_timelineState.currentTime_seconds += g_timelineState.totalDuration_seconds;
                 }
             } else {
                 g_timelineState.currentTime_seconds = 0.0f; // Avoid issues if totalDuration is zero or negative
             }
        }

        // Determine the time value to be used by effects (iTime)
        // This is the core of "Timeline Control Logic" for shaders
        float currentTimeForEffects = g_timelineState.isEnabled ? g_timelineState.currentTime_seconds : (float)glfwGetTime();

        processInput(window);

        std::vector<Effect*> activeEffects;
        for(const auto& effect_ptr : g_scene) {
            // Use currentTimeForEffects to determine if an effect is active on the timeline
            if(effect_ptr && currentTimeForEffects >= effect_ptr->startTime && currentTimeForEffects < effect_ptr->endTime) {
                activeEffects.push_back(effect_ptr.get());
            }
        }
        std::vector<Effect*> renderQueue = GetRenderOrder(activeEffects);
        float audioAmp = g_enableAudioLink ? g_audioSystem.GetCurrentAmplitude() : 0.0f;

        // g_consoleLog += "MainLoop: renderQueue size: " + std::to_string(renderQueue.size()) + "\n"; // Can be verbose

        for (Effect* effect_ptr : renderQueue) {
            // g_consoleLog += "MainLoop: Processing effect: " + effect_ptr->name + "\n"; // Can be verbose
            if(auto* se = dynamic_cast<ShaderEffect*>(effect_ptr)) {
                se->SetDisplayResolution(SCR_WIDTH, SCR_HEIGHT);
                se->SetMouseState(g_mouseState[0], g_mouseState[1], g_mouseState[2], g_mouseState[3]);
                se->SetDeltaTime(deltaTime);
                se->IncrementFrameCount();
                se->SetAudioAmplitude(audioAmp);
            }
            effect_ptr->Update(currentTimeForEffects); // Use the correctly determined time
            effect_ptr->Render();
            checkGLError("After Effect->Render for " + effect_ptr->name);
            // g_renderer.RenderQuad(); // This line was confirmed to be removed/not present in current file.
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        checkGLError("After Unbinding FBOs (to default)");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::GetStyle().Alpha = g_globalAlpha; // Apply global alpha
        ImGui::NewFrame();

        // Create the main dockspace on the first frame
        // Commenting out the entire docking setup due to persistent compilation errors with ImGui docking/viewport flags and functions.
        /*
        if (first_time_docking) {
            first_time_docking = false;
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewportId(viewport->ID); // Corrected function name
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            window_flags |= ImGuiWindowFlags_NoBackground;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("MainDockSpaceViewport", nullptr, window_flags);
            ImGui::PopStyleVar(3);

            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

            // Programmatic DockBuilder layout was already commented out.
            // static bool initial_layout_built = false;
            // if (!initial_layout_built && (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)) {
            //     ...
            // }
            ImGui::End();
        } else if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
             ImGui::DockSpaceOverViewport(viewport, ImGuiDockNodeFlags_None);
        }
        */
        /*
        // Create the main dockspace on the first frame
        static bool first_time_docking = true;
        if (first_time_docking) {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewportId(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            window_flags |= ImGuiWindowFlags_NoBackground;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::Begin("MainDockSpaceViewport", nullptr, window_flags);
            ImGui::PopStyleVar(3); // For WindowRounding, WindowBorderSize, WindowPadding

            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            // Use ImGuiDockNodeFlags_PassthruCentralNode if available and docking is working, otherwise ImGuiDockNodeFlags_None
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

            // Programmatic DockBuilder layout
            // This might fail if ImGuiConfigFlags_DockingEnable was not successfully set or if DockBuilder symbols are not found
            static bool initial_layout_built = false;
            if (!initial_layout_built) {
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
                ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, nullptr, &dock_main_id);
                ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);
                ImGuiID dock_bottom_right_id = ImGui::DockBuilderSplitNode(dock_bottom_id, ImGuiDir_Right, 0.50f, nullptr, &dock_bottom_id);

                ImGui::DockBuilderDockWindow("Shader Editor", dock_left_id);
                // Assuming RenderView is the central node (dock_main_id) or handled differently.
                // If RenderView is a specific window, it should be docked here.
                // For now, the central space (dock_main_id after splits) will be the "Render View".
                ImGui::DockBuilderDockWindow("Console", dock_bottom_id);
                ImGui::DockBuilderDockWindow("Effect Properties", dock_bottom_right_id);

                ImGui::DockBuilderFinish(dockspace_id);
                initial_layout_built = true;
            }
            ImGui::End();
            first_time_docking = false;
        } else {
            // Fallback or regular frame: ensure a dockspace is available if docking is somehow active.
            // This might also error if DockingEnable flag wasn't processed.
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::DockSpaceOverViewport(viewport, ImGuiDockNodeFlags_PassthruCentralNode);
        }
        */
        RenderMenuBar();
        if (g_showGui) {
            if (g_showShaderEditorWindow) RenderShaderEditorWindow();
            if (g_showEffectPropertiesWindow) RenderEffectPropertiesWindow();
            if (g_showConsoleWindow) RenderConsoleWindow();
            if (g_showTimelineWindow) RenderTimelineWindow();
            if (g_showNodeEditorWindow) RenderNodeEditorWindow();
            if (g_showAudioWindow) RenderAudioReactivityWindow();
            if (g_showHelpWindow) RenderHelpWindow();
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        checkGLError("Before Main Screen Clear");
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        checkGLError("After Main Screen Clear");
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        Effect* finalOutputEffect = nullptr;
        // --- ORIGINAL LOGIC FOR finalOutputEffect ---
        for (Effect* effect : renderQueue) {
            if (effect->name == PASSTHROUGH_EFFECT_NAME) { // Used constant
                finalOutputEffect = effect;
                break;
            }
        }
        if (!finalOutputEffect && !renderQueue.empty()) { finalOutputEffect = renderQueue.back(); }
        // --- END ORIGINAL LOGIC ---

        if (finalOutputEffect) {
            if (auto* se = dynamic_cast<ShaderEffect*>(finalOutputEffect)) {
                // g_consoleLog += "MainLoop: Attempting to render finalOutputEffect: " + se->name + ", TextureID: " + std::to_string(se->GetOutputTexture()) + "\n"; // Verbose, remove if too much
                if (se->GetOutputTexture() != 0) {
                    checkGLError("Before RenderFullscreenTexture (" + se->name + ")");
                    g_renderer.RenderFullscreenTexture(se->GetOutputTexture());
                    checkGLError("After RenderFullscreenTexture (" + se->name + ")");
                } else {
                    g_consoleLog += "MainLoop: FinalOutputEffect " + se->name + " has TextureID 0. Cannot render.\n";
                }
            } else {
                g_consoleLog += "MainLoop: FinalOutputEffect '" + finalOutputEffect->name + "' is not a ShaderEffect.\n";
            }
        } else {
            g_consoleLog += "MainLoop: No finalOutputEffect could be determined to render.\n";
        }
        glDisable(GL_BLEND);
        checkGLError("After Disabling Blend, Before ImGui Render");
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_scene.clear();
    g_audioSystem.Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// All callbacks and other helpers here
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    static bool space_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!space_pressed) { g_showGui = !g_showGui; space_pressed = true; }
    } else { space_pressed = false; }
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
    for (const auto& effect_ptr : g_scene) {
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            se->ResizeFrameBuffer(width, height);
        }
    }
}
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        int height;
        glfwGetWindowSize(window, NULL, &height);
        g_mouseState[0] = (float)xpos;
        g_mouseState[1] = (float)height - (float)ypos;
    }
}
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)window; (void)mods;
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if(button == GLFW_MOUSE_BUTTON_LEFT) {
            if(action == GLFW_PRESS) {
                g_mouseState[2] = g_mouseState[0];
                g_mouseState[3] = g_mouseState[1];
            } else if (action == GLFW_RELEASE) {
                g_mouseState[2] = -std::abs(g_mouseState[2]);
                g_mouseState[3] = -std::abs(g_mouseState[3]);
            }
        }
    }
}
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log) {
    TextEditor::ErrorMarkers markers;
    std::stringstream ss(log);
    std::string line;
    std::regex r(R"((\d+):(\d+)\s*:\s*(.*))");
    std::regex r2(R"(ERROR:\s*(\d+):(\d+)\s*:)");
    std::smatch m;
    auto trim_local = [](const std::string& s){ auto f=s.find_first_not_of(" \t\r\n"); return (f==std::string::npos)?"":s.substr(f, s.find_last_not_of(" \t\r\n")-f+1);};
    while(std::getline(ss, line)) {
        if(std::regex_search(line, m, r) && m.size() > 2) {
            try { markers[std::stoi(m[2].str())] = trim_local(m[3].str()); }
            catch (const std::invalid_argument& ia) { /* g_consoleLog += "GLSL Parser: Invalid argument for stoi: " + std::string(ia.what()) + "\n"; */ }
            catch (const std::out_of_range& oor) { /* g_consoleLog += "GLSL Parser: Out of range for stoi: " + std::string(oor.what()) + "\n"; */ }
        } else if (std::regex_search(line, m, r2) && m.size() > 2) {
             try { markers[std::stoi(m[2].str())] = trim_local(line); }
             catch (const std::invalid_argument& ia) { /* g_consoleLog += "GLSL Parser: Invalid argument for stoi: " + std::string(ia.what()) + "\n"; */ }
             catch (const std::out_of_range& oor) { /* g_consoleLog += "GLSL Parser: Out of range for stoi: " + std::string(oor.what()) + "\n"; */ }
        }
    }
    return markers;
}
void ClearErrorMarkers() {
    g_editor.SetErrorMarkers(TextEditor::ErrorMarkers());
}
std::vector<Effect*> GetRenderOrder(const std::vector<Effect*>& activeEffects) {
    if (activeEffects.empty()) return {};
    std::vector<Effect*> sortedOrder;
    std::map<int, Effect*> nodeMap;
    std::map<int, std::vector<int>> adjList;
    std::map<int, int> inDegree;
    for (Effect* effect : activeEffects) {
        if (!effect) continue;
        nodeMap[effect->id] = effect;
        inDegree[effect->id] = 0;
        adjList[effect->id] = {};
    }
    for (Effect* effect : activeEffects) {
        if (!effect) continue;
        if (auto* se = dynamic_cast<ShaderEffect*>(effect)) {
            const auto& inputs = se->GetInputs();
            for (Effect* inputEffect : inputs) {
                if (inputEffect && nodeMap.count(inputEffect->id)) {
                    adjList[inputEffect->id].push_back(se->id);
                    inDegree[se->id]++;
                }
            }
        }
    }
    std::queue<Effect*> q;
    for (Effect* effect : activeEffects) {
        if (!effect) continue;
        if (inDegree[effect->id] == 0) q.push(effect);
    }
    while (!q.empty()) {
        Effect* u = q.front(); q.pop();
        sortedOrder.push_back(u);
        for (int v_id : adjList[u->id]) {
            if (nodeMap.count(v_id)) {
                inDegree[v_id]--;
                if (inDegree[v_id] == 0) q.push(nodeMap[v_id]);
            }
        }
    }
    if (sortedOrder.size() != activeEffects.size()) {
        std::cerr << "Error: Cycle detected in node graph!" << std::endl;
        g_consoleLog = "ERROR: Cycle detected in node graph! Rendering may be incorrect.";
    }
    return sortedOrder;
}
void SaveScene(const std::string& filePath) {
    nlohmann::json sceneJson;

    // Serialize TimelineState
    sceneJson["timelineState"] = g_timelineState; // Uses to_json for TimelineState

    // Serialize actual effects from g_scene
    sceneJson["effects"] = nlohmann::json::array();
    for(const auto& effect_ptr : g_scene) { // Changed variable name for clarity
        if(effect_ptr) {
            sceneJson["effects"].push_back(effect_ptr->Serialize());
        }
    }
    std::ofstream o(filePath);
    if (!o.is_open()) {
        g_consoleLog = "Error: Could not open file for saving scene: " + filePath;
        return;
    }
    o << std::setw(4) << sceneJson << std::endl;
    if (!o.good()) {
        g_consoleLog = "Error: Failed to write scene to file: " + filePath;
    } else {
        g_consoleLog = "Scene saved to: " + filePath; // Success message
    }
}
void LoadScene(const std::string& filePath) {
    std::ifstream i(filePath);
    if (!i.is_open()) {
        g_consoleLog = "Error: Could not open scene file: " + filePath;
        return;
    }
    nlohmann::json sceneJson;
    try {
        i >> sceneJson;
    } catch(const nlohmann::json::parse_error& e) {
        g_consoleLog = "Error parsing scene file: " + std::string(e.what());
        return;
    }
    g_scene.clear();
    g_selectedEffect = nullptr;
    g_editor.SetText(""); // Clear editor
    ClearErrorMarkers();

    // Deserialize TimelineState
    if (sceneJson.contains("timelineState")) {
        g_timelineState = sceneJson["timelineState"].get<TimelineState>(); // Uses from_json for TimelineState
    } else {
        // If no timelineState in json, reset to default (or handle as error)
        g_timelineState = TimelineState();
        g_consoleLog += "Warning: Scene file does not contain timeline state. Using default.\n";
    }

    // Deserialize actual effects into g_scene
    if (sceneJson.contains("effects") && sceneJson["effects"].is_array()) {
        for (const auto& effectJson : sceneJson["effects"]) {
            std::string type = effectJson.value("type", "Unknown");
            if (type == "ShaderEffect") { // Currently only ShaderEffect is supported by this logic
                auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT); // Pass default width/height
                newEffect->Deserialize(effectJson); // Deserialize all properties, including path/source
                newEffect->Load(); // This will load source if path is not enough, then compile, link, parse controls.
                                   // FBO will also be created/resized here.
                g_scene.push_back(std::move(newEffect));
            }
            // TODO: Add handling for other effect types if they exist in the future
        }
    }

    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get(); // Select the first effect by default
        if(auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource()); // Update editor with selected effect's source
            const std::string& compileLog = se->GetCompileErrorLog();
            if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                 g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                 g_consoleLog += "Loaded scene, selected effect '" + se->name + "' has issues: " + compileLog + "\n";
            } else {
                 g_consoleLog += "Loaded scene, selected effect: '" + se->name + "'.\n";
            }
        }
    } else {
        g_consoleLog += "Loaded scene with no effects.\n";
    }
    // UI should refresh automatically as it reads from g_scene and g_timelineState.
    // Explicit refresh call might be needed if ImGui doesn't pick up all changes,
    // but usually not for data changes that its widgets are bound to.
}
