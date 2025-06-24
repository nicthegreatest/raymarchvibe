// RaymarchVibe - Real-time Shader Exploration
// main.cpp

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
#include <queue>      // For topological sort
// #include <map> // Already included

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "httplib.h" 
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include "TextEditor.h"

#include "Effect.h"
#include "ShaderEffect.h"
#include "Renderer.h"
#include "ImGuiSimpleTimeline.h"
// #include "imnodes.h" // Temporarily commented out


// Window dimensions
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window); 
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void RenderTimelineWindow();
// void RenderNodeEditorWindow(); // Temporarily commented out
std::vector<Effect*> GetRenderOrder(const std::vector<Effect*>& activeEffects);

// Scene Management
static std::vector<std::unique_ptr<Effect>> g_scene;
static Effect* g_selectedEffect = nullptr;
static int g_selectedTimelineItem = -1;

// Input State
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// Renderer instance
Renderer g_renderer;
GLuint g_quadVAO = 0;
GLuint g_quadVBO = 0;


std::string load_file_to_string(const char* filePath, std::string& errorMsg) { /* ... */ return "";}
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log) { /* ... */ TextEditor::ErrorMarkers m; return m;}
static TextEditor g_editor;
void ClearErrorMarkers() { g_editor.SetErrorMarkers(TextEditor::ErrorMarkers()); }

static bool g_showGui = true;
static bool g_snapWindowsNextFrame = false;
static bool g_showHelpWindow = false;
static std::string g_shaderLoadError_global = "";
static bool g_isFullscreen = false;
static int g_storedWindowX=100, g_storedWindowY=100, g_storedWindowWidth=SCR_WIDTH, g_storedWindowHeight=SCR_HEIGHT;

struct ShaderSample { const char* name; const char* filePath; };
static const std::vector<ShaderSample> shaderSamples = { {"--- Select ---", ""}, {"Plasma", "shaders/raymarch_v1.frag"}, {"UV Pattern", "shaders/samples/uv_pattern.frag"}, {"Passthrough", "shaders/passthrough.frag"} };
static size_t g_currentSampleIndex = 0;
const char* nativeShaderTemplate = "// Native Template...";
const char* shadertoyShaderTemplate = "// Shadertoy Template...";
std::string FetchShadertoyCodeOnline(const std::string& id, const std::string& key, std::string& err) { err="Network disabled"; return "";}

void RenderTimelineWindow() {
    ImGui::Begin("Timeline");
    std::vector<ImGui::TimelineItem> timelineItems;
    std::vector<int> tracks; tracks.resize(g_scene.size());
    for (size_t i = 0; i < g_scene.size(); ++i) {
        if(!g_scene[i]) continue; tracks[i] = i % 4;
        timelineItems.push_back({g_scene[i]->name, &g_scene[i]->startTime, &g_scene[i]->endTime, &tracks[i] });
    }
    static float currentTimeForRuler = 0.0f;
    if (ImGui::SimpleTimeline("Scene", timelineItems, &currentTimeForRuler, &g_selectedTimelineItem, 4, 0.0f, 60.0f)) {
        if (g_selectedTimelineItem >= 0 && static_cast<size_t>(g_selectedTimelineItem) < g_scene.size()) {
            g_selectedEffect = g_scene[g_selectedTimelineItem].get();
            if(auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) { g_editor.SetText(se->GetShaderSource()); /* set markers */ }
        }
    } ImGui::End();
}

// Node Editor Functions (Temporarily commented out)
// static Effect* FindEffectById(int nodeId) { /* ... */ return nullptr;}
// static int GetNodeIdFromPinAttr(int attr_id) { /* ... */ return 0;}
// static int GetPinIndexFromPinAttr(int attr_id) { /* ... */ return 0;}
// static bool IsOutputPin(int attr_id) { /* ... */ return false;}
// void RenderNodeEditorWindow() { ImGui::Begin("Node Editor (Disabled)"); ImGui::Text("Node editor UI disabled for this test."); ImGui::End(); }


// --- Topological Sort for Rendering Order ---
std::vector<Effect*> GetRenderOrder(const std::vector<Effect*>& activeEffects) {
    if (activeEffects.empty()) return {};
    std::vector<Effect*> sortedOrder; std::map<int, Effect*> nodeMap;
    std::map<int, std::vector<int>> adjList; std::map<int, int> inDegree;
    for (Effect* effect : activeEffects) { if (!effect) continue; nodeMap[effect->id] = effect; inDegree[effect->id] = 0; adjList[effect->id] = {}; }
    for (Effect* effect : activeEffects) {
        if (!effect) continue;
        if (auto* se = dynamic_cast<ShaderEffect*>(effect)) {
            const auto& inputs = se->GetInputs();
            for (Effect* inputEffect : inputs) {
                if (inputEffect && nodeMap.count(inputEffect->id)) {
                    adjList[inputEffect->id].push_back(se->id);
                    inDegree[se->id]++;
                } } } }
    std::queue<Effect*> q;
    for (Effect* effect : activeEffects) { if (!effect) continue; if (inDegree[effect->id] == 0) q.push(effect); }
    while (!q.empty()) {
        Effect* u = q.front(); q.pop(); sortedOrder.push_back(u);
        for (int v_id : adjList[u->id]) {
            if (nodeMap.count(v_id)) {
                inDegree[v_id]--;
                if (inDegree[v_id] == 0) q.push(nodeMap[v_id]);
            } } }
    if (sortedOrder.size() != activeEffects.size()) std::cerr << "Error: Cycle detected in node graph!" << std::endl;
    return sortedOrder;
}


int main() {
    if (!glfwInit()) { std::cerr << "GLFW Init failed." << std::endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe", NULL, NULL);
    if (!window) { std::cerr << "Window creation failed." << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD Init failed." << std::endl; glfwTerminate(); return -1; }

    if (!g_renderer.Init()) { std::cerr << "Renderer Init failed." << std::endl; glfwTerminate(); return -1; }

    float quadVerts[] = { -1.f,1.f, -1.f,-1.f, 1.f,-1.f, -1.f,1.f, 1.f,-1.f, 1.f,1.f };
    glGenVertexArrays(1, &g_quadVAO); glGenBuffers(1, &g_quadVBO);
    glBindVertexArray(g_quadVAO); glBindBuffer(GL_ARRAY_BUFFER, g_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

    static char filePathBuffer_Load[256] = "shaders/raymarch_v1.frag";
    static char shadertoyApiKey[128] = "YourApiKey";

    float deltaTime = 0.0f, lastFrameTime_main = 0.0f;

    auto plasmaEffect = std::make_unique<ShaderEffect>("shaders/raymarch_v1.frag", SCR_WIDTH, SCR_HEIGHT);
    plasmaEffect->name = "Plasma"; plasmaEffect->startTime = 0.f; plasmaEffect->endTime = 30.f;
    Effect* plasmaPtr = plasmaEffect.get();
    g_scene.push_back(std::move(plasmaEffect));

    auto passthroughEffect = std::make_unique<ShaderEffect>("shaders/passthrough.frag", SCR_WIDTH, SCR_HEIGHT);
    passthroughEffect->name = "Passthrough"; passthroughEffect->startTime = 0.f; passthroughEffect->endTime = 30.f;
    Effect* passthroughPtr = passthroughEffect.get();
    g_scene.push_back(std::move(passthroughEffect));

    // Manual connection for testing
    if (plasmaPtr && passthroughPtr) {
        if (auto* se_passthrough = dynamic_cast<ShaderEffect*>(passthroughPtr)) {
            se_passthrough->SetInputEffect(0, plasmaPtr);
            // std::cout << "Manually connected Plasma to Passthrough input 0 for testing." << std::endl;
        }
    }

    if (!g_scene.empty() && !g_selectedEffect) g_selectedEffect = g_scene.front().get();
    for (const auto& effect_ptr : g_scene) effect_ptr->Load();
    if (g_selectedEffect) if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) g_editor.SetText(se->GetShaderSource());

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); // imnodes::CreateContext(); // Temporarily commented out
    ImGuiIO& io = ImGui::GetIO(); ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 330");
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

    lastFrameTime_main = (float)glfwGetTime();
    int f5State = GLFW_RELEASE;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastFrameTime_main;
        lastFrameTime_main = currentTime;
        processInput(window);

        std::vector<Effect*> activeEffects;
        for(const auto& effect_ptr : g_scene){
            if(effect_ptr && currentTime >= effect_ptr->startTime && currentTime < effect_ptr->endTime){
                activeEffects.push_back(effect_ptr.get());
            }
        }
        std::vector<Effect*> renderQueue = GetRenderOrder(activeEffects);

        for (Effect* effect_ptr : renderQueue) {
            ShaderEffect* se = dynamic_cast<ShaderEffect*>(effect_ptr);
            if(se){
                if(effect_ptr == g_selectedEffect){
                    int cd_w, cd_h; glfwGetFramebufferSize(window, &cd_w, &cd_h);
                    se->SetDisplayResolution(cd_w, cd_h);
                    se->SetMouseState(g_mouseState[0],g_mouseState[1],g_mouseState[2],g_mouseState[3]);
                }
                se->SetDeltaTime(deltaTime); se->IncrementFrameCount();
            }
            effect_ptr->Update(currentTime);
            effect_ptr->Render();
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        if (g_showGui) {
            ImGui::Begin("Effect Properties"); if(g_selectedEffect) g_selectedEffect->RenderUI(); ImGui::End();
            ImGui::Begin("Shader Editor"); /* ... UI ... (ensure it uses editorSE from g_selectedEffect) */ ImGui::End();
            ImGui::Begin("Console"); /* ... UI ... */ ImGui::End();
            RenderTimelineWindow();
            // RenderNodeEditorWindow(); // Temporarily commented out
            if(g_showHelpWindow) { ImGui::Begin("Help"); ImGui::End(); }
        }

        int display_w, display_h; glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h); glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (Effect* effect_ptr : renderQueue) {
            if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr)) {
                if (se->GetOutputTexture() != 0) g_renderer.RenderFullscreenTexture(se->GetOutputTexture());
            }
        }
        glDisable(GL_BLEND);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    } 

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown();
    // imnodes::DestroyContext(); // Temporarily commented out
    ImGui::DestroyContext();
    if (g_quadVAO != 0) glDeleteVertexArrays(1, &g_quadVAO);
    if (g_quadVBO != 0) glDeleteBuffers(1, &g_quadVBO);
    glfwTerminate(); return 0;
} 

void framebuffer_size_callback(GLFWwindow* w, int width, int height) { /* ... */ }
void processInput(GLFWwindow *w) { /* ... */ }
void mouse_cursor_position_callback(GLFWwindow* w, double x, double y) { /* ... */ }
void mouse_button_callback(GLFWwindow* w, int btn, int act, int mod) { /* ... */ }
