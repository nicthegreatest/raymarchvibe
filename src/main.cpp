// RaymarchVibe - Real-time Shader Exploration
// main.cpp - FINAL, CORRECTED VERSION (with Docking disabled for compatibility)

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
#include "ShadertoyIntegration.h" // Added for Shadertoy fetching

// --- ImGui and Widget Headers ---
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "TextEditor.h"
#include "ImGuiSimpleTimeline.h"
#include "imnodes.h"
#include "ImGuiFileDialog.h" // Added for file dialog


// Window dimensions
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

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

std::vector<Effect*> GetRenderOrder(const std::vector<Effect*>& activeEffects);
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log);
void ClearErrorMarkers();

// --- Placeholder Audio System ---
// This is a dummy implementation. Replace with actual AudioSystem.
class AudioSystem {
public:
    AudioSystem() {}
    void Init() { /* In a real system: initialize audio input/processing */ }
    float GetCurrentAmplitude() {
        // Placeholder: return a value that changes over time, e.g., a sine wave
        // This is just for testing the uniform.
        return 0.5f + 0.5f * sin((float)glfwGetTime() * 2.0f);
    }
    void Shutdown() { /* In a real system: release audio resources */ }
};
// -------------------------------

// --- Global State ---

// Scene Management
static std::vector<std::unique_ptr<Effect>> g_scene;
static Effect* g_selectedEffect = nullptr;
static int g_selectedTimelineItem = -1;

// Core Systems
static Renderer g_renderer;
static TextEditor g_editor;
static AudioSystem g_audioSystem; // Global AudioSystem instance
// static GLuint g_quadVAO = 0; // Shared VAO for fullscreen quad rendering - REMOVED
// static GLuint g_quadVBO = 0; // REMOVED

// UI State
static bool g_showGui = true; // Overall GUI toggle
static bool g_showHelpWindow = false;
static std::string g_consoleLog = "Welcome to RaymarchVibe Demoscene Tool!";

// Individual window visibility toggles
static bool g_showShaderEditorWindow = true;
static bool g_showEffectPropertiesWindow = true;
static bool g_showTimelineWindow = true;
static bool g_showNodeEditorWindow = true;
static bool g_showConsoleWindow = true;
// g_showHelpWindow is already used for the About/Help window.

// Input State
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// Timeline State
static bool g_timeline_paused = false;
static float g_timeline_time = 0.0f;

// --- Helper Functions ---

// Helper to find an effect by its ID (used for node link creation)
static Effect* FindEffectById(int effect_id) {
    for (const auto& effect_ptr : g_scene) {
        if (effect_ptr && effect_ptr->id == effect_id) {
            return effect_ptr.get();
        }
    }
    return nullptr;
}


// --- UI Window Implementations ---

void RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load Shader...")) {
                // Define a key for the file dialog
                const char* dlgKey = "LoadShaderDlgKey";
                // Set the allowed file extensions
                const char* filters = "Shader Files (*.frag, *.fs){.frag,.fs},.*";
                // Open the dialog
                ImGuiFileDialog::Instance()->OpenDialog(dlgKey, "Choose Shader File", filters, ".");
            }
            bool canSave = false;
            if (g_selectedEffect) {
                if (dynamic_cast<ShaderEffect*>(g_selectedEffect.get())) {
                    canSave = true;
                }
            }
            if (ImGui::MenuItem("Save Shader", nullptr, false, canSave)) {
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect.get())) {
                    const std::string& currentPath = se->GetSourceFilePath();
                    if (!currentPath.empty() && currentPath.rfind("shadertoy://", 0) != 0 && currentPath != "dynamic_source") {
                        std::string codeToSave = g_editor.GetText();
                        std::ofstream outFile(currentPath);
                        if (outFile.is_open()) {
                            outFile << codeToSave;
                            outFile.close();
                            g_consoleLog = "Shader saved to: " + currentPath;
                            // Optional: update m_shaderSourceCode in ShaderEffect if g_editor is canonical
                            // se->ApplyShaderCode(codeToSave); // To ensure internal state matches saved state
                        } else {
                            g_consoleLog = "Error: Could not open file for saving: " + currentPath;
                        }
                    } else {
                        // No valid path, or it's a special path; behave like "Save As..."
                        // For now, just log. "Save As" will handle the dialog.
                        g_consoleLog = "Save Shader: No file path associated or special path. Use 'Save As...'.";
                        // Trigger Save As dialog directly (optional here, or user clicks Save As)
                        // ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Choose File Location", ".frag", ".");
                    }
                }
            }
            if (ImGui::MenuItem("Save Shader As...", nullptr, false, g_selectedEffect != nullptr)) {
                // if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                //     if (!se->GetShaderFilePath().empty() && se->GetShaderFilePath() != "dynamic_source") {
                //         std::ofstream outFile(se->GetShaderFilePath());
                //         if (outFile.is_open()) {
                //             outFile << se->GetShaderSource();
                //             outFile.close();
                //             g_consoleLog = "Shader saved to: " + se->GetShaderFilePath();
                //         } else {
                //             g_consoleLog = "Error: Could not open file for saving: " + se->GetShaderFilePath();
                //         }
                //     } else {
                //         // Act as "Save Shader As..."
                //         // std::string filePath;
                //         // if (ShowSaveFileDialog(filePath, "Fragment Shader (*.frag){.frag},.*", "shader.frag")) {
                //         //     se->SetShaderFilePath(filePath); // Update the effect's path
                //         //     std::ofstream outFile(filePath);
                //         //     if (outFile.is_open()) {
                //         //         outFile << se->GetShaderSource();
                //         //         outFile.close();
                //         //         g_consoleLog = "Shader saved to: " + filePath;
                //         //     } else {
                //         //         g_consoleLog = "Error: Could not open file for saving: " + filePath;
                //         //     }
                //         // }
                //         g_consoleLog = "Save Shader: File dialog for 'Save As' not yet implemented (selected shader has no path).";
                //     }
                // } else {
                //     g_consoleLog = "Save Shader: No ShaderEffect selected or selected effect is not a shader.";
                // }
                g_consoleLog = "Save Shader: File dialog functionality not yet implemented.";
            }
            // Note: The following "Save Shader As..." is duplicated from the one below.
            // This one will be replaced by the new logic.
            if (ImGui::MenuItem("Save Shader As...", nullptr, false, canSave)) { // Reuse canSave logic from "Save Shader"
                if (dynamic_cast<ShaderEffect*>(g_selectedEffect.get())) {
                    const char* dlgKey = "SaveShaderAsDlgKey";
                    const char* filters = "Shader Files (*.frag, *.fs){.frag,.fs},.*";
                    std::string defaultName = "untitled.frag";
                    if (g_selectedEffect) {
                        std::string currentEffectName = g_selectedEffect->GetEffectName();
                        const std::string& currentEffectPath = g_selectedEffect->GetSourceFilePath();

                        if (!currentEffectPath.empty() &&
                            currentEffectPath.find("dynamic_source") == std::string::npos &&
                            currentEffectPath.find("shadertoy://") == std::string::npos) {
                            size_t lastSlash = currentEffectPath.find_last_of("/\\");
                            if (lastSlash != std::string::npos) {
                                defaultName = currentEffectPath.substr(lastSlash + 1);
                            } else {
                                defaultName = currentEffectPath;
                            }
                        } else if (!currentEffectName.empty() && currentEffectName != "Untitled Effect") {
                            if (currentEffectName.length() > 5 && (currentEffectName.substr(currentEffectName.length() - 5) == ".frag" || currentEffectName.substr(currentEffectName.length() - 3) == ".fs")) {
                                defaultName = currentEffectName;
                            } else {
                                defaultName = currentEffectName + ".frag";
                            }
                        }
                    }
                    ImGuiFileDialog::Instance()->OpenDialog(dlgKey, "Save Shader As...", filters, ".", defaultName.c_str());
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene...")) {
                ImGuiFileDialog::Instance()->OpenDialog("SaveSceneDlgKey", "Save Scene File", ".json", ".");
            }
            if (ImGui::MenuItem("Load Scene...")) {
                ImGuiFileDialog::Instance()->OpenDialog("LoadSceneDlgKey", "Load Scene File", ".json", ".");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Shader Editor", nullptr, &g_showShaderEditorWindow);
            ImGui::MenuItem("Effect Properties", nullptr, &g_showEffectPropertiesWindow);
            ImGui::MenuItem("Timeline", nullptr, &g_showTimelineWindow);
            ImGui::MenuItem("Node Editor", nullptr, &g_showNodeEditorWindow);
            ImGui::MenuItem("Console", nullptr, &g_showConsoleWindow);
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

    // Handle "Load Shader" Dialog
    const char* loadShaderDlgKey = "LoadShaderDlgKey";
    if (ImGuiFileDialog::Instance()->Display(loadShaderDlgKey)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName(); // Shorter name for display

            // Use SCR_WIDTH, SCR_HEIGHT for now, consider dynamic sizing later
            auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT);

            // SetEffectName - will be added to Effect.h/ShaderEffect.h
            // newEffect->SetEffectName(fileName); // Plan step
            newEffect->name = fileName; // Direct assignment for now

            if (newEffect->LoadShaderFromFile(filePath)) {
                // SetSourceFilePath - will be added to Effect.h/ShaderEffect.h
                // newEffect->SetSourceFilePath(filePath); // Plan step

                newEffect->startTime = g_timeline_time;
                newEffect->endTime = g_timeline_time + 10.0f;

                // newEffect->Load() should compile the shader and parse controls.
                // ShaderEffect::LoadShaderFromFile already calls ApplyShaderCode, which does this.
                // So, an explicit newEffect->Load() might be redundant if LoadShaderFromFile is comprehensive.
                // For now, let's assume LoadShaderFromFile prepares it sufficiently or calls Load internally.
                // If ApplyShaderCode is not called by LoadShaderFromFile, then newEffect->Load() is needed.
                // ShaderEffect::Load() calls ApplyShaderCode(m_shaderSourceCode).
                // Let's ensure LoadShaderFromFile populates m_shaderSourceCode, then call Load().
                newEffect->Load(); // This will call ApplyShaderCode with the source loaded by LoadShaderFromFile

                g_scene.push_back(std::move(newEffect));
                g_selectedEffect = g_scene.back().get();
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    g_editor.SetText(se->GetShaderSource());
                    ClearErrorMarkers(); // Clear errors from previous shader
                    const std::string& compileLog = se->GetCompileErrorLog();
                    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                        g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                        g_consoleLog = "Loaded shader " + fileName + " with errors/warnings:\n" + compileLog;
                    } else {
                        g_consoleLog = "Loaded shader: " + fileName;
                    }
                } else {
                    g_consoleLog = "Loaded shader: " + fileName + " (not a ShaderEffect or casting failed)";
                }
            } else {
                g_consoleLog = "Error loading shader: " + filePath + "\n" + newEffect->GetCompileErrorLog();
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Handle "Save Shader As" Dialog
    const char* saveShaderAsDlgKey = "SaveShaderAsDlgKey";
    if (ImGuiFileDialog::Instance()->Display(saveShaderAsDlgKey)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName(); // Just the file name part

            if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect.get())) {
                std::string codeToSave = g_editor.GetText();
                std::ofstream outFile(filePath);
                if (outFile.is_open()) {
                    outFile << codeToSave;
                    outFile.close();
                    g_consoleLog = "Shader saved to: " + filePath;
                    se->SetSourceFilePath(filePath);
                    se->SetEffectName(fileName);
                    // Optional: If editor's text should become the new canonical source in ShaderEffect
                    // se->LoadShaderFromSource(codeToSave); // This would re-ApplyShaderCode
                    // se->Load(); // To re-trigger compilation and parsing if LoadShaderFromSource doesn't
                } else {
                    g_consoleLog = "Error: Could not open file for saving: " + filePath;
                }
            } else {
                g_consoleLog = "Error: No ShaderEffect selected for 'Save As'.";
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Handle "Save Scene" Dialog
    if (ImGuiFileDialog::Instance()->Display("SaveSceneDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            nlohmann::json sceneJson;
            sceneJson["effects"] = nlohmann::json::array();
            for (const auto& effect : g_scene) {
                if (effect) { // Should always be true with unique_ptr unless moved from
                    sceneJson["effects"].push_back(effect->Serialize());
                }
            }
            std::ofstream o(filePath);
            if (o.is_open()) {
                o << std::setw(4) << sceneJson << std::endl; // Pretty print
                o.close();
                g_consoleLog = "Scene saved to: " + filePath;
            } else {
                g_consoleLog = "Error: Could not open scene file for saving: " + filePath;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Handle "Load Scene" Dialog
    if (ImGuiFileDialog::Instance()->Display("LoadSceneDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
            std::ifstream i(filePath);
            if (i.is_open()) {
                nlohmann::json sceneJson;
                try {
                    i >> sceneJson;
                    i.close();

                    g_scene.clear();
                    g_selectedEffect = nullptr;
                    g_selectedTimelineItem = -1;
                    g_editor.SetText(""); // Clear editor
                    ClearErrorMarkers();


                    if (sceneJson.contains("effects") && sceneJson["effects"].is_array()) {
                        for (const auto& effectJson : sceneJson["effects"]) {
                            std::string type = effectJson.value("type", "Unknown");
                            if (type == "ShaderEffect") {
                                // Pass SCR_WIDTH, SCR_HEIGHT as default, Deserialize might override FBO settings if saved
                                auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT);
                                newEffect->Deserialize(effectJson);
                                // Important: Call Load() after Deserialize to compile shader, parse controls, create FBO
                                newEffect->Load();
                                g_scene.push_back(std::move(newEffect));
                            } else {
                                g_consoleLog += "Warning: Unknown effect type '" + type + "' in scene file. Skipping.\n";
                            }
                        }
                        g_consoleLog = "Scene loaded from: " + filePath;
                        if (!g_scene.empty()) {
                            g_selectedEffect = g_scene[0].get(); // Select first effect
                             if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                                g_editor.SetText(se->GetShaderSource());
                                // Optionally display compile log of first shader
                            }
                        }
                    } else {
                        g_consoleLog = "Error: Scene file " + filePath + " is missing 'effects' array.";
                    }
                } catch (const nlohmann::json::parse_error& e) {
                    i.close();
                    g_consoleLog = "Error parsing scene file " + filePath + ":\n" + e.what();
                } catch (const nlohmann::json::exception& e) {
                     i.close();
                    g_consoleLog = "Error processing scene data from " + filePath + ":\n" + e.what();
                }
            } else {
                g_consoleLog = "Error: Could not open scene file for loading: " + filePath;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

void RenderShaderEditorWindow() {
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
    static char shadertoyIdBuffer[256] = "";
    ImGui::InputText("##ShadertoyID", shadertoyIdBuffer, sizeof(shadertoyIdBuffer), ImGuiInputTextFlags_Chars tidigareNoSpace); // Allow typical Shadertoy ID chars
    ImGui::SameLine();
    if (ImGui::Button("Fetch##Shadertoy")) {
        std::string idOrUrl = shadertoyIdBuffer;
        std::string shadertoyId = ShadertoyIntegration::ExtractId(idOrUrl);
        if (!shadertoyId.empty()) {
            // API Key: For now, assuming FetchCode can fetch public shaders without an API key,
            // or it has a way to get it (e.g. environment variable).
            // If an API key is strictly needed and not globally available, this needs more setup.
            std::string apiKey = ""; // Placeholder for API key
            std::string errorMsg;
            std::string fetchedCode = ShadertoyIntegration::FetchCode(shadertoyId, apiKey, errorMsg);

            if (!fetchedCode.empty()) {
                auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT, true);
                newEffect->name = "Shadertoy - " + shadertoyId;
                newEffect->SetSourceFilePath("shadertoy://" + shadertoyId); // Virtual path

                // LoadShaderFromSource should set m_shaderSourceCode and m_isShadertoyMode
                if (newEffect->LoadShaderFromSource(fetchedCode)) {
                     // newEffect->SetShadertoyMode(true); // Should be handled by LoadShaderFromSource or constructor flag
                }
                newEffect->Load(); // Compile, parse controls, init FBO

                g_scene.push_back(std::move(newEffect));
                g_selectedEffect = g_scene.back().get();
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    g_editor.SetText(se->GetShaderSource());
                    ClearErrorMarkers();
                     const std::string& compileLog = se->GetCompileErrorLog();
                    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos && compileLog.find("applied successfully") == std::string::npos) {
                        g_editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
                         g_consoleLog = "Fetched Shadertoy " + shadertoyId + " with errors/warnings:\n" + compileLog;
                    } else {
                        g_consoleLog = "Fetched and loaded Shadertoy: " + shadertoyId;
                    }
                }
                shadertoyIdBuffer[0] = '\0'; // Clear input field
            } else {
                g_consoleLog = "Error fetching Shadertoy " + shadertoyId + ": " + errorMsg;
            }
        } else {
            g_consoleLog = "Invalid Shadertoy ID or URL: " + idOrUrl;
        }
    }

    ImGui::End();
}

void RenderEffectPropertiesWindow() {
    ImGui::Begin("Effect Properties");
    if (g_selectedEffect) {
        if (ImGui::Button("Reset Parameters")) {
            g_selectedEffect->ResetParameters();
            // If the selected effect is a ShaderEffect and its source was modified by ResetParameters
            // (e.g. define toggles), update the editor.
            if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                g_editor.SetText(se->GetShaderSource());
                // Also, clear and potentially update error markers based on the re-application of code
                ClearErrorMarkers();
                const std::string& log = se->GetCompileErrorLog();
                if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) {
                    g_editor.SetErrorMarkers(ParseGlslErrorLog(log));
                    g_consoleLog = "Parameters reset. Shader status:\n" + log;
                } else {
                    g_consoleLog = "Parameters reset successfully.";
                }
            } else {
                 g_consoleLog = "Parameters reset for non-shader effect.";
            }
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
    
    std::vector<ImGui::TimelineItem> timelineItems;
    std::vector<int> tracks;
    tracks.resize(g_scene.size());

    for (size_t i = 0; i < g_scene.size(); ++i) {
        if (!g_scene[i]) { continue; }
        tracks[i] = i % 4;
        timelineItems.push_back({
            g_scene[i]->name,
            &g_scene[i]->startTime,
            &g_scene[i]->endTime,
            &tracks[i]
        });
    }

    // float currentTime = (float)glfwGetTime(); // Replaced by g_timeline_time

    if (ImGui::Button(g_timeline_paused ? "Play" : "Pause")) {
        g_timeline_paused = !g_timeline_paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        g_timeline_time = 0.0f;
    }
    ImGui::SameLine();
    ImGui::Text("Time: %.2f", g_timeline_time);
    ImGui::Separator();

    if (ImGui::SimpleTimeline("Scene", timelineItems, &g_timeline_time, &g_selectedTimelineItem, 4, 0.0f, 60.0f)) {
        if (g_selectedTimelineItem >= 0 && static_cast<size_t>(g_selectedTimelineItem) < g_scene.size()) {
            g_selectedEffect = g_scene[g_selectedTimelineItem].get();
            if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                g_editor.SetText(se->GetShaderSource());
                const std::string& log = se->GetCompileErrorLog();
                if (!log.empty() && log.find("Successfully") == std::string::npos) {
                     g_editor.SetErrorMarkers(ParseGlslErrorLog(log));
                } else {
                    ClearErrorMarkers();
                }
            }
        }
    }
    ImGui::End();
}

void RenderNodeEditorWindow() {
    ImGui::Begin("Node Editor");
    ImNodes::BeginNodeEditor();

    // 1. Draw all the nodes
    for (const auto& effect_ptr : g_scene) {
        if (!effect_ptr) continue;

        ImNodes::BeginNode(effect_ptr->id);

        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(effect_ptr->name.c_str());
        ImNodes::EndNodeTitleBar();

        if (effect_ptr->GetOutputPinCount() > 0) {
            ImNodes::BeginOutputAttribute(effect_ptr->id * 10);
            ImGui::Text("out");
            ImNodes::EndOutputAttribute();
        }

        for (int i = 0; i < effect_ptr->GetInputPinCount(); ++i) {
            ImNodes::BeginInputAttribute(effect_ptr->id * 10 + 1 + i);
            ImGui::Text("in %d", i);
            ImNodes::EndInputAttribute();
        }

        ImNodes::EndNode();
    }

    // 2. Draw all existing links
    int link_id_counter = 1;
    for (const auto& effect_ptr : g_scene) {
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            const auto& inputs = se->GetInputs();
            for (size_t i = 0; i < inputs.size(); ++i) {
                if (inputs[i]) {
                    int start_pin_id = inputs[i]->id * 10;
                    int end_pin_id = se->id * 10 + 1 + i;
                    ImNodes::Link(link_id_counter++, start_pin_id, end_pin_id);
                }
            }
        }
    }

    ImNodes::EndNodeEditor();

    // 3. Handle new link creation
    int start_attr, end_attr;
    if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
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
    ImGui::Text("A real-time shader creation and sequencing tool.");
    ImGui::Text("Created by nicthegreatest & Gemini.");
    ImGui::Separator();
    if (ImGui::Button("Close")) {
        g_showHelpWindow = false;
    }
    ImGui::End();
}


// --- Main Application ---
int main() {
    // --- Initialization ---
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe Demoscene Tool", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
    g_renderer.Init(); // This now also sets up the quad VAO internally

    // --- ImGui & ImNodes Setup ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // <-- DISABLED FOR COMPATIBILITY (as per original file and compiler errors)
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

    // --- Initialize Audio System ---
    g_audioSystem.Init();
    
    // --- Scene Setup ---
    auto plasmaEffect_unique = std::make_unique<ShaderEffect>("shaders/raymarch_v1.frag", SCR_WIDTH, SCR_HEIGHT);
    ShaderEffect* plasmaEffect_raw = plasmaEffect_unique.get(); // Get raw pointer
    plasmaEffect_raw->name = "Plasma";
    plasmaEffect_raw->startTime = 0.0f;
    plasmaEffect_raw->endTime = 10.0f;
    g_scene.push_back(std::move(plasmaEffect_unique));
    
    auto passthroughEffect_unique = std::make_unique<ShaderEffect>("shaders/passthrough.frag", SCR_WIDTH, SCR_HEIGHT);
    ShaderEffect* passthroughEffect_raw = passthroughEffect_unique.get(); // Get raw pointer
    passthroughEffect_raw->name = "Passthrough (Final Output)";
    passthroughEffect_raw->startTime = 0.0f;
    passthroughEffect_raw->endTime = 10.0f;
    g_scene.push_back(std::move(passthroughEffect_unique));
    
    for (const auto& effect : g_scene) {
        effect->Load();
    }

    // ---- ADD THIS LINE HERE ----
    // Link the plasma effect's output to the passthrough effect's input (channel 0)
    if (passthroughEffect_raw && plasmaEffect_raw) { // Check pointers
        passthroughEffect_raw->SetInputEffect(0, plasmaEffect_raw);
    }
    // ----------------------------
    
    if (!g_scene.empty()) {
        g_selectedEffect = g_scene[0].get();
        if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
            g_editor.SetText(se->GetShaderSource());
        }
    }
    
    // --- Main Loop ---
    float deltaTime = 0.0f;
    float lastFrameTime = 0.0f;
    
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = (float)glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        if (!g_timeline_paused) {
            g_timeline_time += deltaTime;
        }
        float currentTime = g_timeline_time; // Use this for all logic

        processInput(window);
        
        std::vector<Effect*> activeEffects;
        for(const auto& effect_ptr : g_scene){
            if(effect_ptr && currentTime >= effect_ptr->startTime && currentTime < effect_ptr->endTime){
                activeEffects.push_back(effect_ptr.get());
            }
        }
        std::vector<Effect*> renderQueue = GetRenderOrder(activeEffects);

        float audioAmp = g_audioSystem.GetCurrentAmplitude(); // Keep this

        // --- RENDER-TO-FBO PASS ---
        for (Effect* effect_ptr : renderQueue) {
            // 1. Prepare the effect's shader and uniforms. This binds the FBO and shader program.
            //    (Update audio amplitude if it's a ShaderEffect)
            if(auto* se = dynamic_cast<ShaderEffect*>(effect_ptr)) {
                se->SetDisplayResolution(SCR_WIDTH, SCR_HEIGHT); // Should this be FBO width/height? Assuming main screen for now.
                se->SetMouseState(g_mouseState[0], g_mouseState[1], g_mouseState[2], g_mouseState[3]);
                se->SetDeltaTime(deltaTime);
                se->IncrementFrameCount();
                se->SetAudioAmplitude(audioAmp);
            }
            effect_ptr->Update(currentTime);

            // 2. Render the effect. This single call now does everything.
            effect_ptr->Render();

            // The draw call below is no longer needed here.
            // g_renderer.RenderQuad(); // <-- REMOVE THIS LINE
        }

        // After the loop, unbind the FBO to ensure subsequent rendering (like ImGui) targets the main window
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Dockspace creation commented out due to compilation errors, likely ImGui version/config without docking.
        // // Create the main dockspace
        // ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        // const ImGuiViewport* viewport = ImGui::GetMainViewport();
        // ImGui::SetNextWindowPos(viewport->WorkPos);
        // ImGui::SetNextWindowSize(viewport->WorkSize);
        // ImGui::SetNextWindowViewport(viewport->ID);
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        // window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        // window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        // ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        // ImGui::Begin("MainDockspace", nullptr, window_flags);
        // ImGui::PopStyleVar(3);
        // ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        // ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        
        if (g_showGui) {
            RenderMenuBar();
            if (g_showShaderEditorWindow) RenderShaderEditorWindow();
            if (g_showEffectPropertiesWindow) RenderEffectPropertiesWindow();
            if (g_showTimelineWindow) RenderTimelineWindow();
            if (g_showNodeEditorWindow) RenderNodeEditorWindow();
            if (g_showConsoleWindow) RenderConsoleWindow();
            if (g_showHelpWindow) RenderHelpWindow(); // This one is typically modal or less frequent
        }

        // --- Compositing Pass to Screen ---
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Find the designated output effect and render it.
        Effect* finalOutputEffect = nullptr;
        for (Effect* effect : renderQueue) {
            if (effect->name == "Passthrough (Final Output)") {
                finalOutputEffect = effect;
                break;
            }
        }
        // If we didn't find it, fall back to the last effect in the queue.
        if (!finalOutputEffect && !renderQueue.empty()) {
            finalOutputEffect = renderQueue.back();
        }

        if (finalOutputEffect) {
            if (auto* se = dynamic_cast<ShaderEffect*>(finalOutputEffect)) {
                if (se->GetOutputTexture() != 0) {
                    g_renderer.RenderFullscreenTexture(se->GetOutputTexture());
                }
            }
        }

        glDisable(GL_BLEND);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    g_scene.clear();
    // g_quadVAO and g_quadVBO cleanup removed as they are no longer global.
    // Renderer's destructor would handle its own VAO/VBO if it had one,
    // but we removed the explicit destructor. OpenGL resources are typically
    // cleaned up when context is destroyed, or manually in a Renderer::Shutdown().
    g_audioSystem.Shutdown(); // Shutdown audio system
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImNodes::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// --- Callback and Helper Implementations ---
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    static bool space_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!space_pressed) {
            g_showGui = !g_showGui;
            space_pressed = true;
        }
    } else {
        space_pressed = false;
    }
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
            try { markers[std::stoi(m[2].str())] = trim_local(m[3].str()); } catch (...) {}
        } else if (std::regex_search(line, m, r2) && m.size() > 2) {
             try { markers[std::stoi(m[2].str())] = trim_local(line); } catch (...) {}
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
