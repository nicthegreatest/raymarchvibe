// RaymarchVibe - Real-time Shader Exploration
// main.cpp - FINAL BUILD VERSION

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

// --- Core App Headers ---
#include "Effect.h"
#include "ShaderEffect.h"
#include "Renderer.h"
#include "ShadertoyIntegration.h"

// --- ImGui and Widget Headers ---
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "ImGuiSimpleTimeline.h"
#include "imnodes.h"
#include "ImGuiFileDialog.h"
#include "AudioSystem.h" // Added the actual AudioSystem header

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
static bool g_showHelpWindow = false;
static bool g_showShaderEditorWindow = true;
static bool g_showEffectPropertiesWindow = true;
static bool g_showTimelineWindow = true;
static bool g_showNodeEditorWindow = true;
static bool g_showConsoleWindow = true;
static bool g_showAudioWindow = false;
static bool g_enableAudioLink = false; // Changed to false
static std::string g_consoleLog = "Welcome to RaymarchVibe Demoscene Tool!";
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static bool g_timeline_paused = true; // Changed to true
static float g_timeline_time = 0.0f;

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
            case GL_STACK_OVERFLOW: errorStr = "STACK_OVERFLOW"; break; // Should not happen with modern GL
            case GL_STACK_UNDERFLOW: errorStr = "STACK_UNDERFLOW"; break; // Should not happen with modern GL
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
                ImGuiFileDialog::Instance()->OpenDialog("LoadShaderDlgKey", "Choose Shader File", ".frag,.fs,.glsl,.*", IGFD::FileDialogConfig{ .path = "." });
            }
            bool canSave = (g_selectedEffect && dynamic_cast<ShaderEffect*>(g_selectedEffect));
            if (ImGui::MenuItem("Save Shader", nullptr, false, canSave)) {
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    const std::string& currentPath = se->GetSourceFilePath();
                    if (!currentPath.empty() && currentPath.find("shadertoy://") == std::string::npos && currentPath != "dynamic_source") {
                        std::ofstream outFile(currentPath);
                        if (outFile.is_open()) {
                            outFile << g_editor.GetText();
                            outFile.close(); // Close file before checking good()
                            if (!outFile.good()) {
                                g_consoleLog = "Error: Failed to write shader to file: " + currentPath;
                            } else {
                                g_consoleLog = "Shader saved to: " + currentPath;
                            }
                        } else {
                            g_consoleLog = "Error: Could not open file for saving shader: " + currentPath;
                        }
                    } else { // No valid path, so effectively "Save As"
                        ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{ .path = "." });
                    }
                }
            }
            if (ImGui::MenuItem("Save Shader As...", nullptr, false, canSave)) {
                 ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{ .path = "." });
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
            ImGui::MenuItem("Timeline", nullptr, &g_showTimelineWindow);
            ImGui::MenuItem("Node Editor", nullptr, &g_showNodeEditorWindow);
            ImGui::MenuItem("Console", nullptr, &g_showConsoleWindow);
            ImGui::MenuItem("Audio Reactivity", nullptr, &g_showAudioWindow);
            ImGui::Separator();
            ImGui::MenuItem("Toggle All GUI", "Spacebar", &g_showGui);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("About RaymarchVibe", nullptr, &g_showHelpWindow);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // --- Handle File Dialogs for Shader Load/Save ---
    if (ImGuiFileDialog::Instance()->Display("LoadShaderDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string errorMsg;
            std::string fileContent = LoadFileContent(filePathName, errorMsg);
            if (!errorMsg.empty()) {
                g_consoleLog = errorMsg;
            } else {
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    se->SetSourceFilePath(filePathName); // Set the path first
                    se->LoadShaderFromSource(fileContent); // Load the new source
                    se->Load(); // This compiles and applies
                    g_editor.SetText(fileContent);
                    ClearErrorMarkers();
                    g_consoleLog = "Loaded shader: " + filePathName;
                    if (!se->GetCompileErrorLog().empty() && se->GetCompileErrorLog().find("Successfully") == std::string::npos) {
                        g_consoleLog += "\nCompile Log: " + se->GetCompileErrorLog();
                        g_editor.SetErrorMarkers(ParseGlslErrorLog(se->GetCompileErrorLog()));
                    }
                } else {
                    // Option: Create a new ShaderEffect if none (or wrong type) is selected
                    g_consoleLog = "No ShaderEffect selected. Please select or create one to load the shader into.";
                    // auto newEffect = std::make_unique<ShaderEffect>(filePathName, SCR_WIDTH, SCR_HEIGHT);
                    // newEffect->name = "Loaded: " + filePathName;
                    // newEffect->Load(); // This will load from file path and apply
                    // if (newEffect->IsShaderLoaded()) {
                    //     g_editor.SetText(newEffect->GetShaderSource());
                    //     g_scene.push_back(std::move(newEffect));
                    //     g_selectedEffect = g_scene.back().get();
                    //     g_consoleLog = "Loaded shader into new effect: " + filePathName;
                    // } else {
                    //     g_consoleLog = "Failed to load shader into new effect: " + filePathName + "\n" + newEffect->GetCompileErrorLog();
                    // }
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("SaveShaderAsDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string shaderCode = g_editor.GetText();
            std::ofstream outFile(filePathName);
            if (outFile.is_open()) {
                outFile << shaderCode;
                outFile.close();
                if (!outFile.good()) {
                     g_consoleLog = "Error: Failed to write shader to file: " + filePathName;
                } else {
                    g_consoleLog = "Shader saved to: " + filePathName;
                    if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                        se->SetSourceFilePath(filePathName); // Update the effect's path
                    }
                }
            } else {
                g_consoleLog = "Error: Could not open file for saving: " + filePathName;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void RenderShaderEditorWindow() {
    static char filePathBuffer_SaveAs[512] = ""; // Buffer for Save As path - MOVED TO FUNCTION SCOPE
    // static char shadertoyIdBuffer[256] = ""; // Already static at its use point, keep it there or move here too for consistency
    // static int currentSampleIndex = 0; // Already static at its use point

    ImGui::Begin("Shader Editor");
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
    ImGui::Text("Mouse: (%.1f, %.1f)", g_mouseState[0], g_mouseState[1]);
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
                        ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey_Editor", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{ .path = "." });
                    }
                }
            }
            ImGui::InputText("Save As Path", filePathBuffer_SaveAs, sizeof(filePathBuffer_SaveAs));
            ImGui::SameLine();
            if (ImGui::Button("Save As...##Editor")) { // Identical to File > Save Shader As
                std::string saveAsPathStr(filePathBuffer_SaveAs);
                if (saveAsPathStr.empty()) {
                    ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey_Editor", "Save Shader As...", ".frag,.fs,.glsl", IGFD::FileDialogConfig{ .path = "." });
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
    if (ImGui::Button(g_timeline_paused ? "Play" : "Pause")) { g_timeline_paused = !g_timeline_paused; }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) { g_timeline_time = 0.0f; }
    ImGui::SameLine();
    ImGui::Text("Time: %.2f", g_timeline_time);
    ImGui::Separator();
    std::vector<ImGui::TimelineItem> timelineItems;
    std::vector<int> tracks;
    tracks.resize(g_scene.size());
    for (size_t i = 0; i < g_scene.size(); ++i) {
        if (!g_scene[i]) { continue; }
        tracks[i] = i % 4;
        timelineItems.push_back({ g_scene[i]->name, &g_scene[i]->startTime, &g_scene[i]->endTime, &tracks[i] });
    }
    if (ImGui::SimpleTimeline("Scene", timelineItems, &g_timeline_time, &g_selectedTimelineItem, 4, 0.0f, 60.0f)) {
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
    ImGui::End();
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
    ImNodes::CreateContext();
    (void)ImGui::GetIO(); // Suppress unused variable warning
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    g_audioSystem.Initialize(); // Changed from Init() to Initialize()
    
    auto plasmaEffect = std::make_unique<ShaderEffect>("shaders/raymarch_v1.frag", SCR_WIDTH, SCR_HEIGHT);
    plasmaEffect->name = "Plasma";
    plasmaEffect->startTime = 0.0f;
    plasmaEffect->endTime = 10.0f;
    g_scene.push_back(std::move(plasmaEffect));
    
    auto passthroughEffect = std::make_unique<ShaderEffect>("shaders/passthrough.frag", SCR_WIDTH, SCR_HEIGHT);
    passthroughEffect->name = "Passthrough (Final Output)";
    passthroughEffect->startTime = 0.0f;
    passthroughEffect->endTime = 10.0f;
    g_scene.push_back(std::move(passthroughEffect));
    
    for (const auto& effect : g_scene) {
        effect->Load();
    }
    
    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get();
        if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource());
        }
    }

    // Programmatically link Plasma to Passthrough
    if (plasmaEffect && passthroughEffect) {
        if (auto* passthrough_se = dynamic_cast<ShaderEffect*>(passthroughEffect.get())) {
            passthrough_se->SetInputEffect(0, plasmaEffect.get());
            g_consoleLog += "AUTO-LINK: Programmatically linked Plasma to Passthrough input 0.\n";
        }
    }


    float deltaTime = 0.0f, lastFrameTime = 0.0f;

    while(!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        if (!g_timeline_paused) { g_timeline_time += deltaTime; }
        float currentTime = g_timeline_time;

        processInput(window);

        std::vector<Effect*> activeEffects;
        for(const auto& effect_ptr : g_scene) {
            if(effect_ptr && currentTime >= effect_ptr->startTime && currentTime < effect_ptr->endTime) {
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
            effect_ptr->Update(currentTime);
            effect_ptr->Render();
            checkGLError("After Effect->Render for " + effect_ptr->name);
            // g_renderer.RenderQuad(); // This line was confirmed to be removed/not present in current file.
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        checkGLError("After Unbinding FBOs (to default)");

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        RenderMenuBar();
        if (g_showGui) {
            if (g_showShaderEditorWindow) RenderShaderEditorWindow();
            if (g_showEffectPropertiesWindow) RenderEffectPropertiesWindow();
            if (g_showTimelineWindow) RenderTimelineWindow();
            if (g_showNodeEditorWindow) RenderNodeEditorWindow();
            if (g_showConsoleWindow) RenderConsoleWindow();
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
        for (Effect* effect : renderQueue) {
            if (effect->name == PASSTHROUGH_EFFECT_NAME) { // Used constant
                finalOutputEffect = effect;
                break;
            }
        }
        if (!finalOutputEffect && !renderQueue.empty()) { finalOutputEffect = renderQueue.back(); }

        if (finalOutputEffect) {
            if (auto* se = dynamic_cast<ShaderEffect*>(finalOutputEffect)) {
                // g_consoleLog += "MainLoop: FinalOutputEffect: " + se->name + ", TextureID: " + std::to_string(se->GetOutputTexture()) + "\n"; // Already logged this
                if (se->GetOutputTexture() != 0) {
                    checkGLError("Before RenderFullscreenTexture");
                    g_renderer.RenderFullscreenTexture(se->GetOutputTexture());
                    checkGLError("After RenderFullscreenTexture");
                } else {
                    // g_consoleLog += "MainLoop: FinalOutputEffect " + se->name + " has TextureID 0.\n"; // Already logged
                }
            } else {
                // g_consoleLog += "MainLoop: FinalOutputEffect '" + finalOutputEffect->name + "' is not a ShaderEffect.\n"; // Already logged
            }
        } else {
            // g_consoleLog += "MainLoop: No finalOutputEffect found to render.\n"; // Already logged
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
    sceneJson["effects"] = nlohmann::json::array();
    for(const auto& effect : g_scene) {
        if(effect) {
            sceneJson["effects"].push_back(effect->Serialize());
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
    if (sceneJson.contains("effects") && sceneJson["effects"].is_array()) {
        for (const auto& effectJson : sceneJson["effects"]) {
            std::string type = effectJson.value("type", "Unknown");
            if (type == "ShaderEffect") {
                auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT);
                newEffect->Deserialize(effectJson);
                newEffect->Load();
                g_scene.push_back(std::move(newEffect));
            }
        }
    }
    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get();
        if(auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource());
        }
    }
}
