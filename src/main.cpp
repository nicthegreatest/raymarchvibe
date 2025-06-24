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

// --- Placeholder Audio System ---
// You should replace this with your actual #include "AudioSystem.h"
#define AUDIO_FILE_PATH_BUFFER_SIZE 256
class AudioSystem {
public:
    AudioSystem() { audioFilePathInputBuffer[0] = '\0'; }
    void Init() {}
    void Shutdown() {}
    float GetCurrentAmplitude() { return 0.5f + 0.5f * sin((float)glfwGetTime() * 2.0f); }
    int GetCurrentAudioSourceIndex() { return m_currentSourceIndex; }
    void SetCurrentAudioSourceIndex(int index) { m_currentSourceIndex = index; }
    const std::vector<const char*>& GetCaptureDeviceGUINames() { static std::vector<const char*> v = {"Default Mic"}; return v; }
    int* GetSelectedActualCaptureDeviceIndexPtr() { static int i = 0; return &i; }
    void SetSelectedActualCaptureDeviceIndex(int) {}
    void InitializeAndStartSelectedCaptureDevice() {}
    char* GetAudioFilePathBuffer() { return audioFilePathInputBuffer; }
    void LoadWavFile(const char*) {}
    bool IsAudioFileLoaded() { return false; }
private:
    char audioFilePathInputBuffer[AUDIO_FILE_PATH_BUFFER_SIZE];
    int m_currentSourceIndex = 0;
};


// --- Window dimensions ---
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
static bool g_enableAudioLink = true;
static std::string g_consoleLog = "Welcome to RaymarchVibe Demoscene Tool!";
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static bool g_timeline_paused = false;
static float g_timeline_time = 0.0f;


// --- Helper Functions ---
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
                ImGuiFileDialog::Instance()->OpenDialog("LoadShaderDlgKey", "Choose Shader File", ".frag,.fs,.*");
            }
            bool canSave = (g_selectedEffect && dynamic_cast<ShaderEffect*>(g_selectedEffect));
            if (ImGui::MenuItem("Save Shader", nullptr, false, canSave)) {
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    const std::string& currentPath = se->GetSourceFilePath();
                    if (!currentPath.empty() && currentPath.find("shadertoy://") == std::string::npos && currentPath != "dynamic_source") {
                        std::ofstream outFile(currentPath);
                        if (outFile.is_open()) {
                            outFile << g_editor.GetText();
                            g_consoleLog = "Shader saved to: " + currentPath;
                        } else {
                            g_consoleLog = "Error: Could not open file for saving: " + currentPath;
                        }
                    } else {
                        ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Save Shader As...", ".frag,.fs,.*");
                    }
                }
            }
            if (ImGui::MenuItem("Save Shader As...", nullptr, false, canSave)) {
                 ImGuiFileDialog::Instance()->OpenDialog("SaveShaderAsDlgKey", "Save Shader As...", ".frag,.fs,.*");
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
    ImGui::InputText("##ShadertoyID", shadertoyIdBuffer, sizeof(shadertoyIdBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Fetch##Shadertoy")) {
        std::string shadertoyId = ShadertoyIntegration::ExtractId(shadertoyIdBuffer);
        if (!shadertoyId.empty()) {
            std::string apiKey = "";
            std::string errorMsg;
            std::string fetchedCode = ShadertoyIntegration::FetchCode(shadertoyId, apiKey, errorMsg);
            if (!fetchedCode.empty()) {
                auto newEffect = std::make_unique<ShaderEffect>("", SCR_WIDTH, SCR_HEIGHT, true);
                newEffect->name = "Shadertoy - " + shadertoyId;
                newEffect->SetSourceFilePath("shadertoy://" + shadertoyId);
                newEffect->LoadShaderFromSource(fetchedCode);
                newEffect->Load();
                g_scene.push_back(std::move(newEffect));
                // CORRECTED: Use .get() to assign the raw pointer from the unique_ptr
                g_selectedEffect = g_scene.back().get();
                if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                    g_editor.SetText(se->GetShaderSource());
                    ClearErrorMarkers();
                    g_consoleLog = "Fetched and loaded Shadertoy: " + shadertoyId;
                }
                shadertoyIdBuffer[0] = '\0';
            } else {
                g_consoleLog = "Error fetching Shadertoy " + shadertoyId + ": " + errorMsg;
            }
        } else {
            g_consoleLog = "Invalid Shadertoy ID or URL.";
        }
    }
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
    g_audioSystem.Init();
    
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

        for (Effect* effect_ptr : renderQueue) {
            if(auto* se = dynamic_cast<ShaderEffect*>(effect_ptr)) {
                se->SetDisplayResolution(SCR_WIDTH, SCR_HEIGHT);
                se->SetMouseState(g_mouseState[0], g_mouseState[1], g_mouseState[2], g_mouseState[3]);
                se->SetDeltaTime(deltaTime);
                se->IncrementFrameCount();
                se->SetAudioAmplitude(audioAmp);
            }
            effect_ptr->Update(currentTime);
            effect_ptr->Render();
            g_renderer.RenderQuad();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        Effect* finalOutputEffect = nullptr;
        for (Effect* effect : renderQueue) {
            if (effect->name == "Passthrough (Final Output)") {
                finalOutputEffect = effect;
                break;
            }
        }
        if (!finalOutputEffect && !renderQueue.empty()) { finalOutputEffect = renderQueue.back(); }
        if (finalOutputEffect) {
            if (auto* se = dynamic_cast<ShaderEffect*>(finalOutputEffect)) {
                if (se->GetOutputTexture() != 0) g_renderer.RenderFullscreenTexture(se->GetOutputTexture());
            }
        }
        glDisable(GL_BLEND);
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
void SaveScene(const std::string& filePath) {
    nlohmann::json sceneJson;
    sceneJson["effects"] = nlohmann::json::array();
    for(const auto& effect : g_scene) {
        if(effect) {
            sceneJson["effects"].push_back(effect->Serialize());
        }
    }
    std::ofstream o(filePath);
    o << std::setw(4) << sceneJson << std::endl;
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
