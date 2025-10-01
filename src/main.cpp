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
#include <chrono> // For recording timer

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
#include "OutputNode.h" // For the Scene Output node
#include "Bess/Config/Themes.h" // Added Themes header
#include "VideoRecorder.h"

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

// New: GLFW error callback function
void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error (" << error << "): " << description << std::endl;
}

void RenderMenuBar();
void RenderShaderEditorWindow();
void RenderEffectPropertiesWindow();
void RenderTimelineWindow();
void RenderNodeEditorWindow();
void RenderConsoleWindow();
void RenderHelpWindow();
void RenderAudioReactivityWindow();
void RenderShadertoyWindow();

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


void SaveScene(const std::string& filePath);
void LoadScene(const std::string& filePath);


// --- Global State ---
static std::vector<std::unique_ptr<Effect>> g_scene;
static Effect* g_selectedEffect = nullptr;
static int g_selectedTimelineItem = -1;
static Renderer g_renderer;
static TextEditor g_editor;
static AudioSystem g_audioSystem;
VideoRecorder g_videoRecorder;
static Bess::Config::Themes g_themes; // Global Themes object
static bool g_showGui = true;

// Recording state
static std::chrono::steady_clock::time_point g_recordingStartTime;

// Window visibility flags
static bool g_showShaderEditorWindow = true;
// static bool g_showEffectPropertiesWindow = true; // Removed, integrated into Node Editor
static bool g_showConsoleWindow = true;
// static bool g_showRenderViewWindow = true; // Render View is not a typical window, handled by main loop's final render pass.
static bool g_showTimelineWindow = false;
static bool g_showNodeEditorWindow = false;
static bool g_showAudioWindow = false;
static bool g_showHelpWindow = false;
static bool g_showShadertoyWindow = false;

static char g_shadertoyApiKeyBuffer[256] = ""; // For user's API key

static bool g_enableAudioLink = false; // Changed to false
static std::string g_consoleLog = "Welcome to RaymarchVibe Demoscene Tool!";
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static bool g_timeline_paused = false; // Reverted to false for default playback
// static float g_timeline_time = 0.0f; // This will be g_timelineState.currentTime_seconds // Unused variable
static bool g_timelineControlActive = false; // Added for explicit timeline UI control
static bool g_no_output_logged = false;

// --- Node Editor State for Delayed Positioning ---
#include <set> // Required for std::set
// static std::map<int, ImVec2> g_new_node_initial_positions; // Already included by ImGui
static std::set<int> g_nodes_requiring_initial_position;
static std::map<int, ImVec2> g_new_node_initial_positions;
static std::vector<int> g_nodes_to_delete;


// --- Timeline State (New) ---
#include "Timeline.h" // For TimelineState struct
static TimelineState g_timelineState;

// Demo shaders list - moved to global static for access by multiple UI functions
static const std::vector<std::pair<std::string, std::string>> g_demoShaders = {
    {"Sample: Fractal 1", "shaders/samples/fractal1.frag"},
    {"Sample: Fractal 2", "shaders/samples/fractal2.frag"},
    {"Sample: Fractal 3", "shaders/samples/fractal3.frag"},
    {"Sample: Simple Red", "shaders/samples/simple_red.frag"},
    {"Sample: UV Pattern", "shaders/samples/uv_pattern.frag"},
    {"Sample: Fractal Tree Audio", "shaders/samples/fractal_tree_audio.frag"},
    {"Sample: Soap Bubbles", "shaders/samples/shape_soap_bubble.frag"},
    {"Sample: Heart Shape", "shaders/samples/shape_heart.frag"}
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

void MarkNodeForDeletion(int node_id) {
    // Add node to the deletion queue if it's not already there
    if (std::find(g_nodes_to_delete.begin(), g_nodes_to_delete.end(), node_id) == g_nodes_to_delete.end())
    {
        g_nodes_to_delete.push_back(node_id);
    }
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
                for (const auto& demo : g_demoShaders) {
                    if (ImGui::MenuItem(demo.first.c_str())) {
                        // Always create a new effect for a demo shader
                        auto newEffect = std::make_unique<ShaderEffect>(demo.second, SCR_WIDTH, SCR_HEIGHT);
                        newEffect->name = demo.first;
                        newEffect->Load(); // This will load from file path and apply
                        
                        const std::string& compileLog = newEffect->GetCompileErrorLog();
                        if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                            g_consoleLog = "Error loading demo shader " + demo.first + ". Log: " + compileLog;
                            // Don't add the broken effect to the scene
                        } else {
                            g_editor.SetText(newEffect->GetShaderSource());
                            ClearErrorMarkers();
                            g_scene.push_back(std::move(newEffect));
                            g_selectedEffect = g_scene.back().get(); // Select the new effect
                            g_consoleLog = "Loaded demo shader '" + demo.first + "' into a new effect.";
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
            // ImGui::MenuItem("Effect Properties", nullptr, &g_showEffectPropertiesWindow); // Removed
            ImGui::MenuItem("Console", nullptr, &g_showConsoleWindow);
            ImGui::Separator();
            ImGui::MenuItem("Timeline", nullptr, &g_showTimelineWindow);
            ImGui::MenuItem("Node Editor", nullptr, &g_showNodeEditorWindow);
            ImGui::MenuItem("Audio Reactivity", nullptr, &g_showAudioWindow);
            ImGui::MenuItem("Shadertoy", nullptr, &g_showShadertoyWindow);
            ImGui::Separator();
            ImGui::MenuItem("Toggle All GUI", "Spacebar", &g_showGui);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) { // New Settings Menu
            if (ImGui::BeginMenu("Themes")) {
                const auto& availableThemes = g_themes.getThemes();
                // TODO: Get current theme to show a checkmark next to it.
                for (const auto& pair : availableThemes) {
                    if (ImGui::MenuItem(pair.first.c_str())) {
                        g_themes.applyTheme(pair.first);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About RaymarchVibe", nullptr, &g_showHelpWindow);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Recording")) {
            static char filename[128] = "output.mp4";
            ImGui::InputText("Filename", filename, 128);
            ImGui::SameLine();
            if (ImGui::Button("Browse")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("SaveRecordingDlgKey", "Choose Output File", ".mp4,.mov,.mpg", config);
            }

            if (ImGuiFileDialog::Instance()->Display("SaveRecordingDlgKey")) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                    strncpy(filename, filePath.c_str(), 128);
                }
                ImGuiFileDialog::Instance()->Close();
            }

            static int format_idx = 0;
            const char* formats[] = { "mp4", "mov", "mpg" };
            if (ImGui::BeginCombo("Format", formats[format_idx])) {
                for (int i = 0; i < IM_ARRAYSIZE(formats); i++) {
                    const bool is_selected = (format_idx == i);
                    if (ImGui::Selectable(formats[i], is_selected))
                        format_idx = i;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            static bool g_recordAudio = true;
            ImGui::Checkbox("Record Audio", &g_recordAudio);

            if (g_videoRecorder.is_recording()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                if (ImGui::Button("Stop Recording")) {
                    g_videoRecorder.stop_recording();
                }
                ImGui::PopStyleColor(4);
                ImGui::SameLine();
                
                auto duration = std::chrono::steady_clock::now() - g_recordingStartTime;
                auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
                duration -= hours;
                auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
                duration -= minutes;
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
                ImGui::Text("Status: Recording... %02d:%02d:%02d", (int)hours.count(), (int)minutes.count(), (int)seconds.count());

            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                if (ImGui::Button("Start Recording")) {
                    // Ensure the audio device is started if we are recording with mic input
                    if (g_recordAudio && g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::Microphone && !g_audioSystem.IsCaptureDeviceInitialized()) {
                        g_audioSystem.InitializeAndStartSelectedCaptureDevice();
                    }

                    if (std::filesystem::exists(filename)) {
                        ImGui::OpenPopup("Overwrite File?");
                    } else {
                        g_videoRecorder.start_recording(filename, SCR_WIDTH, SCR_HEIGHT, 60, formats[format_idx], g_recordAudio,
                                                    g_audioSystem.GetCurrentInputSampleRate(),
                                                    g_audioSystem.GetCurrentInputChannels());
                        g_recordingStartTime = std::chrono::steady_clock::now();
                    }
                }
                ImGui::PopStyleColor(4);
                ImGui::Text("Status: Idle");
            }

            // Overwrite confirmation popup
            if (ImGui::BeginPopupModal("Overwrite File?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("File '%s' already exists.\nDo you want to overwrite it?", filename);
                ImGui::Separator();
                if (ImGui::Button("Overwrite", ImVec2(120, 0))) {
                    // Ensure the audio device is started if we are recording with mic input
                    if (g_recordAudio && g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::Microphone && !g_audioSystem.IsCaptureDeviceInitialized()) {
                        g_audioSystem.InitializeAndStartSelectedCaptureDevice();
                    }
                    g_videoRecorder.start_recording(filename, SCR_WIDTH, SCR_HEIGHT, 60, formats[format_idx], g_recordAudio,
                                                g_audioSystem.GetCurrentInputSampleRate(),
                                                g_audioSystem.GetCurrentInputChannels());
                    g_recordingStartTime = std::chrono::steady_clock::now();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // --- Handle File Dialogs for Shader Load/Save ---
    if (ImGuiFileDialog::Instance()->Display("LoadShaderDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string justFileName = std::filesystem::path(filePathName).filename().string();

            auto newEffect = std::make_unique<ShaderEffect>(filePathName, SCR_WIDTH, SCR_HEIGHT);
            newEffect->name = justFileName.empty() ? "Untitled Shader" : justFileName;
            newEffect->Load();
            if (!newEffect->GetCompileErrorLog().empty() && newEffect->GetCompileErrorLog().find("Successfully") == std::string::npos && newEffect->GetCompileErrorLog().find("applied successfully") == std::string::npos) {
                g_consoleLog = "Error loading shader " + justFileName + " into new effect. Log: " + newEffect->GetCompileErrorLog();
            } else {
                g_editor.SetText(newEffect->GetShaderSource());
                ClearErrorMarkers();
                g_scene.push_back(std::move(newEffect));
                g_selectedEffect = g_scene.back().get(); // Select the new effect
                g_consoleLog = "Loaded shader '" + justFileName + "' into a new effect.";
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

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20.0f, 0.0f)); // Add some spacing
    ImGui::SameLine();

    // Shadertoy Mode checkbox
    if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
        bool isShadertoy = se->IsShadertoyMode();
        if (ImGui::Checkbox("Shadertoy Mode", &isShadertoy)) {
            se->SetShadertoyMode(isShadertoy);
            se->ApplyShaderCode(g_editor.GetText()); // Re-apply to wrap/unwrap mainImage
            const std::string& log = se->GetCompileErrorLog();
            if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) {
                g_editor.SetErrorMarkers(ParseGlslErrorLog(log));
                g_consoleLog = log;
            } else {
                ClearErrorMarkers();
                g_consoleLog = "Toggled Shadertoy mode and re-applied shader.";
            }
        }
    }

    ImGui::Separator(); // Separator after the toolbar

    g_editor.Render("TextEditor");



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


    ImGui::End();
}

// void RenderEffectPropertiesWindow() { // Function removed as properties are now in Node Editor
//     ImGui::Begin("Effect Properties");
//     if (g_selectedEffect) {
//         if (ImGui::Button("Reset Parameters")) {
//             g_selectedEffect->ResetParameters();
//             if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
//                 g_editor.SetText(se->GetShaderSource());
//                 ClearErrorMarkers();
//             }
//             g_consoleLog = "Parameters reset successfully.";
//         }
//         ImGui::Separator();
//         g_selectedEffect->RenderUI();
//     } else {
//         ImGui::Text("No effect selected.");
//     }
//     ImGui::End();
// }

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
    static std::vector<int> track_indices_storage;

    if (track_indices_storage.size() < g_scene.size()) {
        track_indices_storage.resize(g_scene.size());
    }

    for (size_t i = 0; i < g_scene.size(); ++i) {
        if (!g_scene[i]) {
            continue;
        }
        track_indices_storage[i] = i % 4;
        timelineItems.push_back({
            g_scene[i]->name.c_str(),
            &g_scene[i]->startTime,
            &g_scene[i]->endTime,
            &track_indices_storage[i]
        });
    }

    bool timeline_event = ImGui::SimpleTimeline("Scene", timelineItems, &g_timelineState.currentTime_seconds, &g_selectedTimelineItem,
                               4, // num_tracks
                               0.0f, // sequence_total_start_time_seconds
                               g_timelineState.totalDuration_seconds, // sequence_total_end_time_seconds
                               g_timelineState.horizontalScroll_seconds, // Pass scroll
                               g_timelineState.zoomLevel                 // Pass zoom
                               );

    if (timeline_event) {
        if (g_selectedTimelineItem >= 0 && static_cast<size_t>(g_selectedTimelineItem) < g_scene.size()) {
            if (g_scene[g_selectedTimelineItem]) {
                g_selectedEffect = g_scene[g_selectedTimelineItem].get();
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    g_editor.SetText(se->GetShaderSource());
                    ClearErrorMarkers();
                }
            }
        }
    }

    ImGui::End();
}

void RenderNodeEditorWindow() {
    ImGui::Begin("Node Editor");

    static float sidebar_width = 350.0f;
    const float canvas_width = ImGui::GetContentRegionAvail().x - sidebar_width;

    // Node Editor Canvas (Left Side)
    ImGui::BeginChild("NodeEditorCanvas", ImVec2(canvas_width > 0 ? canvas_width : 1, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImNodes::BeginNodeEditor();
    for (const auto& effect_ptr : g_scene) {
        if (!effect_ptr) continue;
        ImNodes::BeginNode(effect_ptr->id);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(effect_ptr->name.c_str());
        ImNodes::EndNodeTitleBar();

        if (ImGui::BeginPopupContextItem("Node Context Menu"))
        {
            if (ImGui::MenuItem("Delete"))
            {
                MarkNodeForDeletion(effect_ptr->id);
            }
            ImGui::EndPopup();
        }

        // Delayed positioning for newly added nodes
        if (g_nodes_requiring_initial_position.count(effect_ptr->id)) {
            ImVec2 initial_pos = g_new_node_initial_positions[effect_ptr->id];
            ImNodes::SetNodeScreenSpacePos(effect_ptr->id, initial_pos);

            g_nodes_requiring_initial_position.erase(effect_ptr->id);
            g_new_node_initial_positions.erase(effect_ptr->id);
        }

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
        if (!effect_ptr) continue;

        // Handle ShaderEffect inputs
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            const auto& inputs = se->GetInputs();
            for (size_t i = 0; i < inputs.size(); ++i) {
                if (inputs[i]) {
                    ImNodes::Link(link_id_counter++, inputs[i]->id * 10, se->id * 10 + 1 + i);
                }
            }
        }
        // Handle OutputNode input
        else if (auto* on = dynamic_cast<OutputNode*>(effect_ptr.get())) {
            if (on->GetInputEffect()) {
                ImNodes::Link(link_id_counter++, on->GetInputEffect()->id * 10, on->id * 10 + 1);
            }
        }
    }

    // Context menu for adding new nodes, triggered by right-click on the editor canvas
    // Ensuring this is called within BeginNodeEditor / EndNodeEditor
    if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("AddNodeContextMenu");
    }

    if (ImGui::BeginPopup("AddNodeContextMenu")) {
        if (ImGui::BeginMenu("Add Effect")) {
            if (ImGui::BeginMenu("Generators")) {
                if (ImGui::MenuItem("Basic Plasma")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreatePlasmaBasicEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Simple Color")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateSimpleColorEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Value Noise")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateValueNoiseEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Circle Shape")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateCircleShapeEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Noise Generator")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateNoiseEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Sphere")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateSphereEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
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
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Brightness/Contrast")) {
                     auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateBrightnessContrastEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Color Correction")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateColorCorrectionEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Sharpen")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateSharpenEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Grain")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateGrainEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Chromatic Aberration")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateChromaticAberrationEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Post-Processing")) {
                if (ImGui::MenuItem("Bloom")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateBloomEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Tone Mapping")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateToneMappingEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                if (ImGui::MenuItem("Vignette")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateVignetteEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Image")) {
                if (ImGui::MenuItem("Texture Passthrough")) {
                    auto newEffectUniquePtr = RaymarchVibe::NodeTemplates::CreateTexturePassthroughEffect();
                    if (newEffectUniquePtr) {
                        Effect* newEffectRawPtr = newEffectUniquePtr.get();
                        g_scene.push_back(std::move(newEffectUniquePtr));
                        newEffectRawPtr->Load();
                        g_nodes_requiring_initial_position.insert(newEffectRawPtr->id);
                        g_new_node_initial_positions[newEffectRawPtr->id] = ImGui::GetMousePos();
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Output")) {
            if (ImGui::MenuItem("Scene Output")) {
                auto newEffect = std::make_unique<OutputNode>();
                newEffect->Load();
                g_scene.push_back(std::move(newEffect));
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    ImNodes::EndNodeEditor();

    // Handle deletion of nodes via Delete key
    if (ImGui::IsKeyReleased(ImGuiKey_Delete))
    {
        const int num_selected_nodes = ImNodes::NumSelectedNodes();
        if (num_selected_nodes > 0)
        {
            std::vector<int> selected_node_ids(num_selected_nodes);
            ImNodes::GetSelectedNodes(selected_node_ids.data());
            for (const int node_id : selected_node_ids)
            {
                MarkNodeForDeletion(node_id);
            }
        }
    }

    // Handle link creation (should remain associated with the canvas where interaction happens)
    int start_attr, end_attr;
    if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
        // ... (link creation logic remains the same) ...
        int start_node_id = start_attr / 10;
        int end_node_id = end_attr / 10;
        Effect* start_effect = FindEffectById(start_node_id);
        Effect* end_effect = FindEffectById(end_node_id);
        bool start_is_output = (start_attr % 10 == 0);
        bool end_is_input = (end_attr % 10 != 0);

        if (start_effect && end_effect && start_node_id != end_node_id) {
            if (start_is_output && end_is_input) {
                int input_pin_index = (end_attr % 10) - 1;
                end_effect->SetInputEffect(input_pin_index, start_effect);
            } else if (!start_is_output && !end_is_input) {
                int input_pin_index = (start_attr % 10) - 1;
                start_effect->SetInputEffect(input_pin_index, end_effect);
            }
        }
    }

    // Handle Ctrl+Click on node to break links
    // Check if the main node editor canvas (where nodes are) is hovered.
    // Using IsWindowHovered with RootAndChildWindows might be more robust if popups interfere.
    // However, GetHoveredNode should only return a valid ID if a node (not empty canvas) is truly under mouse.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int hovered_node_id = -1;
        // ImNodes::GetHoveredNode() might be what we need, or iterate nodes and check ImGui::IsItemHovered()
        // For ImNodes, it's better to use its API if available for this specific check,
        // or if it refers to a different kind of hover state.
        // A simpler way for ImNodes:
        ImNodes::IsNodeHovered(&hovered_node_id); // This updates hovered_node_id if a node is hovered by mouse

        if (hovered_node_id != -1) {
            Effect* clicked_effect = FindEffectById(hovered_node_id);
            if (clicked_effect) {
                g_consoleLog += "Ctrl+Clicked on node ID: " + std::to_string(clicked_effect->id) + " Name: " + clicked_effect->GetEffectName() + ". Breaking links.\n";
                // 1. Clear inputs OF the clicked node
                if (auto* se_clicked = dynamic_cast<ShaderEffect*>(clicked_effect)) {
                    for (int i = 0; i < se_clicked->GetInputPinCount(); ++i) {
                        se_clicked->SetInputEffect(i, nullptr);
                    }
                }

                // 2. Clear inputs TO the clicked node from other nodes
                for (const auto& effect_ptr_unique : g_scene) {
                    if (auto* se_target = dynamic_cast<ShaderEffect*>(effect_ptr_unique.get())) {
                        if (se_target == clicked_effect) continue; // Already handled its own inputs

                        const auto& inputs = se_target->GetInputs(); // Need to ensure GetInputs provides enough info or direct access
                        for (size_t i = 0; i < inputs.size(); ++i) {
                            if (inputs[i] && inputs[i]->id == clicked_effect->id) {
                                se_target->SetInputEffect(i, nullptr);
                            }
                        }
                    }
                }
            }
        }
    }

    ImGui::EndChild(); // End NodeEditorCanvas

    // --- VERTICAL SPLITTER ---
    ImGui::SameLine();
    ImGui::InvisibleButton("vsplit", ImVec2(8.0f, ImGui::GetContentRegionAvail().y));
    if (ImGui::IsItemActive()) {
        sidebar_width -= ImGui::GetIO().MouseDelta.x;
        // Clamp width to reasonable values
        sidebar_width = std::max(250.0f, std::min(sidebar_width, 800.0f));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    ImGui::SameLine();

    // Sidebar (Right Side) - will contain Properties and Instructions
    ImGui::BeginChild("SidebarChild", ImVec2(sidebar_width, 0), true); // true for border

    // --- Node Properties Panel ---
    ImGui::Text("Node Properties");
    ImGui::Separator();

    const int num_selected_nodes = ImNodes::NumSelectedNodes();
    std::vector<int> selected_node_ids;
    if (num_selected_nodes > 0) {
        selected_node_ids.resize(num_selected_nodes);
        ImNodes::GetSelectedNodes(selected_node_ids.data());
    }

    if (selected_node_ids.size() == 1) {
        int node_id = selected_node_ids[0];
        Effect* current_effect = FindEffectById(node_id);
        if (current_effect) {
            // Update global selected effect if it's different
            if (g_selectedEffect != current_effect) {
                g_selectedEffect = current_effect;
                // If it's a ShaderEffect, update the editor text
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    g_editor.SetText(se->GetShaderSource());
                    ClearErrorMarkers(); // Clear previous errors
                    // Optionally, display compile errors if any for this shader
                    const std::string& compileLog = se->GetCompileErrorLog();
                    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                        g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                    }
                }
            }

            // Display editable name
            char name_buffer[128];
            strncpy(name_buffer, current_effect->GetEffectName().c_str(), sizeof(name_buffer) - 1);
            name_buffer[sizeof(name_buffer) - 1] = '\0'; // Ensure null termination
            if (ImGui::InputText("Name", name_buffer, sizeof(name_buffer))) {
                current_effect->SetEffectName(name_buffer);
            }

            // Display ID (read-only)
            ImGui::Text("ID: %d", current_effect->id);
            ImGui::Separator();

            // Render specific UI for the effect
            current_effect->RenderUI();

        } else {
            ImGui::Text("Error: Selected node ID %d not found.", node_id);
            g_selectedEffect = nullptr; // Clear global selection if node not found
        }
    } else if (selected_node_ids.size() > 1) {
        ImGui::Text("%zu nodes selected.", selected_node_ids.size()); // Use %zu for size_t
        ImGui::TextWrapped("Multi-selection actions not yet implemented. Please select a single node to view/edit properties.");
        // Consider clearing g_selectedEffect or setting it to a primary if ImNodes supports that
        // For now, let's clear it to avoid confusion with shader editor
        if (g_selectedEffect != nullptr && std::find(selected_node_ids.begin(), selected_node_ids.end(), g_selectedEffect->id) == selected_node_ids.end()){
             g_selectedEffect = nullptr; // Clear if current g_selectedEffect is not in multi-selection
        }
    } else {
        ImGui::Text("No node selected.");
        if (g_selectedEffect != nullptr) { // If a node was previously selected
             g_selectedEffect = nullptr;
        }
    }
    ImGui::Separator(); // Separator after Node Properties section

    // --- Instructions Panel ---
    ImGui::Text("Instructions");
    ImGui::Separator();
    ImGui::TextWrapped("Right-click canvas: Add Node");
    ImGui::TextWrapped("Drag pin to pin: Create Link");
    ImGui::TextWrapped("Select Node: Edit Properties (above)");
    ImGui::TextWrapped("Shift+Click Node: Add to selection");
    ImGui::TextWrapped("Ctrl+Click Node: Break all links to/from node");
    ImGui::Separator();
    ImGui::TextWrapped("Middle-mouse drag: Pan canvas");
    ImGui::TextWrapped("Mouse wheel: Zoom canvas");

    ImGui::EndChild(); // End SidebarChild

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

void RenderShadertoyWindow() {
    if (!g_showShadertoyWindow) return;

    ImGui::Begin("Shadertoy", &g_showShadertoyWindow);

    static char shadertoyIdBuffer[256] = "";

    ImGui::InputTextWithHint("API Key", "Enter your key", g_shadertoyApiKeyBuffer, sizeof(g_shadertoyApiKeyBuffer), ImGuiInputTextFlags_Password);
    ImGui::InputTextWithHint("ID/URL", "e.g., Ms2SD1 or full URL", shadertoyIdBuffer, sizeof(shadertoyIdBuffer));
    
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button("Load", ImVec2(-1, 0))) { // Full width button
        std::string idOrUrl = shadertoyIdBuffer;
        if (!idOrUrl.empty()) {
            std::string shaderId = ShadertoyIntegration::ExtractId(idOrUrl);
            if (!shaderId.empty()) {
                g_consoleLog = "Fetching Shadertoy " + shaderId + "...";
                std::string fetchError;
                std::string apiKey = g_shadertoyApiKeyBuffer;
                std::string fetchedCode = ShadertoyIntegration::FetchCode(shaderId, apiKey, fetchError);

                if (!fetchedCode.empty()) {
                    auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT, true);
                    newEffect->name = "Shadertoy - " + shaderId;
                    newEffect->SetSourceFilePath("shadertoy://" + shaderId);
                    newEffect->LoadShaderFromSource(fetchedCode);
                    newEffect->SetShadertoyMode(true);
                    newEffect->Load();

                    const std::string& compileLog = newEffect->GetCompileErrorLog();
                    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                        g_consoleLog = "Shadertoy '" + shaderId + "' fetched, but compilation failed. Log:\n" + compileLog;
                    } else {
                        g_editor.SetText(newEffect->GetShaderSource());
                        ClearErrorMarkers();
                        g_scene.push_back(std::move(newEffect));                        g_consoleLog = "Shadertoy '" + shaderId + "' fetched and applied!";
                        g_showShadertoyWindow = false; // Close window on success
                    }
                } else {
                    g_consoleLog = fetchError;
                    if (g_consoleLog.empty()) {
                        g_consoleLog = "Failed to retrieve code for Shadertoy ID: " + shaderId + ". Check API key and ID.";
                    }
                }
            } else {
                g_consoleLog = "Invalid Shadertoy ID or URL format.";
            }
        } else {
            g_consoleLog = "Please enter a Shadertoy ID or URL.";
        }
    }
    ImGui::PopStyleColor(4);

    ImGui::End();
}

// Helper struct to manage and format time values, like a UI-specific hook.
struct ChronoTimer {
    float currentTime = 0.0f;
    float duration = 0.0f;
    float progress = 0.0f;

    void update(float newProgress, float totalDuration) {
        progress = newProgress;
        duration = totalDuration;
        currentTime = progress * duration;
    }

    std::string getFormattedTime(float timeInSeconds) {
        if (timeInSeconds < 0) timeInSeconds = 0;
        int hours = static_cast<int>(timeInSeconds) / 3600;
        timeInSeconds -= hours * 3600;
        int minutes = static_cast<int>(timeInSeconds) / 60;
        timeInSeconds -= minutes * 60;
        int seconds = static_cast<int>(timeInSeconds);
        
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, seconds);
        return std::string(buffer);
    }
};

void RenderAudioReactivityWindow() {
    ImGui::Begin("Audio Reactivity", &g_showAudioWindow);
    ImGui::Checkbox("Enable Audio Link (iAudioAmp)", &g_enableAudioLink);
    ImGui::Separator();

    auto currentSource = g_audioSystem.GetCurrentAudioSource();
    int currentSourceInt = static_cast<int>(currentSource);

    if (ImGui::RadioButton("Microphone", &currentSourceInt, static_cast<int>(AudioSystem::AudioSource::Microphone))) {
        if (currentSource != AudioSystem::AudioSource::Microphone) {
            g_audioSystem.SetCurrentAudioSource(AudioSystem::AudioSource::Microphone);
        }
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Audio File", &currentSourceInt, static_cast<int>(AudioSystem::AudioSource::AudioFile))) {
        if (currentSource != AudioSystem::AudioSource::AudioFile) {
            g_audioSystem.SetCurrentAudioSource(AudioSystem::AudioSource::AudioFile);
        }
    }
    ImGui::Separator();

    if (g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::Microphone) {
        if (ImGui::CollapsingHeader("Microphone Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& devices = g_audioSystem.GetCaptureDeviceGUINames();
            int selectedDeviceIndex = g_audioSystem.GetSelectedCaptureDeviceIndex();
            
            if (devices.empty()) {
                ImGui::Text("No capture devices found.");
            } else if (ImGui::BeginCombo("Input Device", (selectedDeviceIndex >= 0 && (size_t)selectedDeviceIndex < devices.size()) ? devices[selectedDeviceIndex] : "None")) {
                for (size_t i = 0; i < devices.size(); ++i) {
                    const bool is_selected = (selectedDeviceIndex == (int)i);
                    if (ImGui::Selectable(devices[i], is_selected)) {
                        if (selectedDeviceIndex != (int)i) {
                            g_audioSystem.SetSelectedCaptureDeviceIndex(i);
                            g_audioSystem.InitializeAndStartSelectedCaptureDevice();
                        }
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
    } else if (g_audioSystem.GetCurrentAudioSource() == AudioSystem::AudioSource::AudioFile) {
        if (ImGui::CollapsingHeader("Audio File Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            char* audioFilePath = g_audioSystem.GetAudioFilePathBuffer();
            ImGui::InputText("File Path", audioFilePath, AUDIO_FILE_PATH_BUFFER_SIZE);
            ImGui::SameLine();
            if (ImGui::Button("Browse##AudioFile")) {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseAudioFileDlgKey", "Choose Audio File", ".mp3,.wav", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::Button("Load##AudioFile")) { g_audioSystem.LoadWavFile(audioFilePath); }
            ImGui::PopStyleColor(4);
            ImGui::Text("Status: %s", g_audioSystem.IsAudioFileLoaded() ? "Loaded" : "Not Loaded");

            if (g_audioSystem.IsAudioFileLoaded()) {
                static ChronoTimer timer;
                timer.update(g_audioSystem.GetPlaybackProgress(), g_audioSystem.GetPlaybackDuration());

                if (ImGui::Button("Play")) { g_audioSystem.Play(); }
                ImGui::SameLine();
                if (ImGui::Button("Pause")) { g_audioSystem.Pause(); }
                ImGui::SameLine();
                if (ImGui::Button("Stop")) { g_audioSystem.Stop(); }

                ImGui::Text("%s", timer.getFormattedTime(timer.currentTime).c_str());
                ImGui::SameLine();
                if (ImGui::SliderFloat("##Progress", &timer.progress, 0.0f, 1.0f, "")) {
                    g_audioSystem.SetPlaybackProgress(timer.progress);
                }
                ImGui::SameLine();
                ImGui::Text("%s", timer.getFormattedTime(timer.duration).c_str());
            }
        }
    }

    if (ImGuiFileDialog::Instance()->Display("ChooseAudioFileDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            g_audioSystem.SetAudioFilePath(filePathName.c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::Separator();
    ImGui::ProgressBar(g_audioSystem.GetCurrentAmplitude(), ImVec2(-1.0f, 0.0f));

    const auto& fftData = g_audioSystem.GetFFTData();
    if (!fftData.empty()) {
        ImGui::PlotLines("##FFT", fftData.data(), fftData.size(), 0, NULL, 0.0f, 1.0f, ImVec2(0, 80));
    }
    ImGui::End();
}

// Main Application
int main() {
    // Set GLFW error callback BEFORE glfwInit()
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Add GLFW window hints for OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    g_renderer.Init();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Commenting these out again as they cause "not declared in scope" errors,
    // suggesting a deeper issue with ImGui version/include paths.
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;


    ImGui::StyleColorsDark();
    // When viewports are enabled we tweak WindowRounding/WindowBg to make them look like main window.
    // ImGuiStyle& style = ImGui::GetStyle(); // Unused variable
    // Commenting out viewport-specific style changes as ViewportsEnable flag is causing issues
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    // {
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    g_themes.applyTheme("Bess Dark"); // Apply default theme
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    g_audioSystem.Initialize();
    g_audioSystem.RegisterListener(&g_videoRecorder); // Connect audio system to video recorder

    // Load raymarch_v1.frag as the default effect
    auto defaultEffect = std::make_unique<ShaderEffect>("shaders/raymarch_v1.frag", SCR_WIDTH, SCR_HEIGHT);
    defaultEffect->name = "Raymarch Plasma v1"; // Or derive from filename
    defaultEffect->startTime = 0.0f;
    defaultEffect->endTime = g_timelineState.totalDuration_seconds; // Default duration
    g_scene.push_back(std::move(defaultEffect));

    // Load all effects in the scene (just the default for now)
    for (const auto& effect_ptr : g_scene) {
        effect_ptr->Load();
    }

    // Select the default effect and load its code into the editor
    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get();
        if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource());
            ClearErrorMarkers();
            const std::string& compileLog = se->GetCompileErrorLog();
            if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                g_consoleLog += "Default shader (" + se->name + ") issue: " + compileLog + "\n";
            } else {
                g_consoleLog += "Default shader (" + se->name + ") loaded successfully.\n";
            }
        }
    }

    // No auto-linking needed for a single node setup

    float deltaTime = 0.0f, lastFrameTime = 0.0f;
    // static bool first_time_docking = true; // Unused variable
    static size_t last_scene_size = 0;

    while(!glfwWindowShouldClose(window)) {
        // Check if a node was added to reset the console log spam guard
        if (g_scene.size() > last_scene_size) {
            g_no_output_logged = false;
        }
        last_scene_size = g_scene.size();

        // Process deferred deletions at the start of the frame
        if (!g_nodes_to_delete.empty()) {
            for (int node_id : g_nodes_to_delete) {
                // First, remove any connections TO this node from other nodes
                for (const auto& effect : g_scene) {
                    if (!effect) continue;

                    // Handle ShaderEffect inputs
                    if (auto* se = dynamic_cast<ShaderEffect*>(effect.get())) {
                        const auto& inputs = se->GetInputs();
                        for (size_t i = 0; i < inputs.size(); ++i) {
                            if (inputs[i] && inputs[i]->id == node_id) {
                                se->SetInputEffect(i, nullptr);
                            }
                        }
                    }
                    // Handle OutputNode input
                    else if (auto* on = dynamic_cast<OutputNode*>(effect.get())) {
                        if (on->GetInputEffect() && on->GetInputEffect()->id == node_id) {
                            on->SetInputEffect(0, nullptr);
                        }
                    }
                }

                // If the deleted node was selected, deselect it
                if (g_selectedEffect && g_selectedEffect->id == node_id) {
                    g_selectedEffect = nullptr;
                }

                // Now, find and remove the node from the scene
                auto it = std::remove_if(g_scene.begin(), g_scene.end(), [node_id](const std::unique_ptr<Effect>& effect) {
                    return effect && effect->id == node_id;
                });
                if (it != g_scene.end()) {
                    g_scene.erase(it, g_scene.end());
                }
            }
            g_nodes_to_delete.clear();
        }

        float currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // --- Hot-reloading Check (every second) ---
        static float hot_reload_timer = 0.0f;
        hot_reload_timer += deltaTime;
        if (hot_reload_timer > 1.0f) {
            for (const auto& effect_ptr : g_scene) {
                if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
                    if (se->CheckForUpdatesAndReload()) {
                        g_consoleLog += "Hot-reloaded shader: " + se->GetEffectName() + "\n";
                        if (se == g_selectedEffect) {
                            g_editor.SetText(se->GetShaderSource());
                        }
                    }
                }
            }
            hot_reload_timer = 0.0f;
        }

        g_audioSystem.ProcessAudio(); // Process audio for FFT

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
        if (g_timelineState.isEnabled) {
            for(const auto& effect_ptr : g_scene) {
                if(effect_ptr && currentTimeForEffects >= effect_ptr->startTime && currentTimeForEffects < effect_ptr->endTime) {
                    activeEffects.push_back(effect_ptr.get());
                }
            }
        } else {
            // If timeline master control is not enabled, all effects are considered active
            for(const auto& effect_ptr : g_scene) {
                if(effect_ptr) {
                    activeEffects.push_back(effect_ptr.get());
                }
            }
        }
        std::vector<Effect*> renderQueue = GetRenderOrder(activeEffects);
        float audioAmp = g_enableAudioLink ? g_audioSystem.GetCurrentAmplitude() : 0.0f;
        const auto& audioBands = g_audioSystem.GetAudioBands();

        checkGLError("Before Effect Render Loop");
        checkGLError("Before Effect Render Loop");
        checkGLError("Before Effect Render Loop");
        for (Effect* effect_ptr : renderQueue) {
            if(auto* se = dynamic_cast<ShaderEffect*>(effect_ptr)) {
                se->SetDisplayResolution(SCR_WIDTH, SCR_HEIGHT);
                se->SetMouseState(g_mouseState[0], g_mouseState[1], g_mouseState[2], g_mouseState[3]);
                se->SetDeltaTime(deltaTime);
                se->IncrementFrameCount();
                se->SetAudioAmplitude(audioAmp);
                if (g_enableAudioLink) {
                    se->SetAudioBands(audioBands);
                }
            }
            effect_ptr->Update(currentTimeForEffects); 
            effect_ptr->Render();
        }
        checkGLError("After Effect Render Loop");
        checkGLError("After Effect Render Loop");
        checkGLError("After Effect Render Loop");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        checkGLError("After Unbinding FBOs (to default)");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        RenderMenuBar();
        if (g_showGui) {
            if (g_showShaderEditorWindow) RenderShaderEditorWindow();
            // if (g_showEffectPropertiesWindow) RenderEffectPropertiesWindow(); // Call removed
            if (g_showConsoleWindow) RenderConsoleWindow();
            if (g_showTimelineWindow) RenderTimelineWindow();
            if (g_showNodeEditorWindow) RenderNodeEditorWindow();
            if (g_showAudioWindow) RenderAudioReactivityWindow();
            if (g_showHelpWindow) RenderHelpWindow();
            if (g_showShadertoyWindow) RenderShadertoyWindow();
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
        for (const auto& effect : g_scene) {
            if (auto* outputNode = dynamic_cast<OutputNode*>(effect.get())) {
                if (outputNode->GetInputEffect()) {
                    finalOutputEffect = outputNode->GetInputEffect();
                    break;
                }
            }
        }

        if (!finalOutputEffect) {
            if (g_selectedEffect) {
                finalOutputEffect = g_selectedEffect;
            } else if (!renderQueue.empty()) {
                finalOutputEffect = renderQueue.back();
            }
        }

        if (finalOutputEffect) {
            checkGLError("Before Final RenderFullscreenTexture");
            GLuint finalTextureID = finalOutputEffect->GetOutputTexture();
            
            g_renderer.RenderFullscreenTexture(finalTextureID);
            checkGLError("After Final RenderFullscreenTexture");
        } else {
            if (!g_no_output_logged) {
                std::cout << "No finalOutputEffect determined for rendering." << std::endl;
                g_no_output_logged = true;
            }
        }

        if (g_videoRecorder.is_recording()) {
            g_videoRecorder.add_video_frame_from_pbo();
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
    g_editor.SetText("");
    ClearErrorMarkers();

    if (sceneJson.contains("timelineState")) {
        g_timelineState = sceneJson["timelineState"].get<TimelineState>();
    }

    std::map<int, Effect*> oldIdToNewEffectMap;

    // First pass: create all effects
    if (sceneJson.contains("effects") && sceneJson["effects"].is_array()) {
        for (const auto& effectJson : sceneJson["effects"]) {
            std::string type = effectJson.value("type", "Unknown");
            std::unique_ptr<Effect> newEffect = nullptr;

            if (type == "ShaderEffect") {
                newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT);
            } else if (type == "OutputNode") {
                newEffect = std::make_unique<OutputNode>();
            }

            if (newEffect) {
                int oldId = effectJson.value("id", -1);
                newEffect->Deserialize(effectJson);
                newEffect->Load();
                oldIdToNewEffectMap[oldId] = newEffect.get();
                g_scene.push_back(std::move(newEffect));
            }
        }
    }

    // Second pass: link effects
    for (auto& effect_ptr : g_scene) {
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            const auto& input_ids = se->GetDeserializedInputIds();
            for (size_t i = 0; i < input_ids.size(); ++i) {
                int old_id = input_ids[i];
                if (oldIdToNewEffectMap.count(old_id)) {
                    se->SetInputEffect(i, oldIdToNewEffectMap[old_id]);
                }
            }
        } else if (auto* on = dynamic_cast<OutputNode*>(effect_ptr.get())) {
            int old_id = on->GetDeserializedInputId();
            if (oldIdToNewEffectMap.count(old_id)) {
                on->SetInputEffect(0, oldIdToNewEffectMap[old_id]);
            }
        }
    }

    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get();
        if(auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource());
        }
    }

    int max_id = 0;
    for (const auto& effect_ptr : g_scene) {
        if (effect_ptr && effect_ptr->id > max_id) {
            max_id = effect_ptr->id;
        }
    }
    Effect::UpdateNextId(max_id + 1);
    g_consoleLog = "Scene loaded from: " + filePath;
}
