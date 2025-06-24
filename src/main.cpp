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
#include <queue>

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
#include "imnodes.h" // Re-enabled


// Window dimensions
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window); 
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void RenderTimelineWindow();
void RenderNodeEditorWindow();
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


std::string load_file_to_string(const char* filePath, std::string& errorMsg) { /* ... (implementation as before) ... */
    errorMsg.clear(); std::ifstream fs; std::stringstream ss;
    fs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try { fs.open(filePath); ss << fs.rdbuf(); fs.close(); }
    catch (const std::ifstream::failure& e) { errorMsg = "ERR: " + std::string(filePath) + " - " + e.what(); return ""; }
    return ss.str();
}
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log) { /* ... (implementation as before) ... */
    TextEditor::ErrorMarkers markers; std::stringstream ss(log); std::string line;
    std::regex r1(R"(^(?:[A-Z]+:\s*)?\d+:(\d+):\s*(.*))"), r2(R"(^\s*\d+\((\d+)\)\s*:\s*(.*))"); std::smatch m;
    auto trim_local = [](const std::string& s){ auto f=s.find_first_not_of(" \t\r\n"); return (f==std::string::npos)?"":s.substr(f, s.find_last_not_of(" \t\r\n")-f+1);};
    while (std::getline(ss, line)) { bool done=false; if(std::regex_search(line,m,r1)&&m.size()>=3){try{markers[std::stoi(m[1].str())]=trim_local(m[2].str());done=true;}catch(...){}} if(!done && std::regex_search(line,m,r2)&&m.size()>=3){try{markers[std::stoi(m[1].str())]=trim_local(m[2].str());}catch(...){}}} return markers;
}
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

// --- Node Editor ---
static Effect* FindEffectById(int effect_id) { // Was part of user's RenderNodeEditorWindow, made static global helper
    for (const auto& effect_ptr : g_scene) {
        if (effect_ptr && effect_ptr->id == effect_id) {
            return effect_ptr.get();
        }
    }
    return nullptr;
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
            ImNodes::BeginOutputAttribute(effect_ptr->id * 10); // Output pin 0: ID is effect_id * 10
            ImGui::Text("out");
            ImNodes::EndOutputAttribute();
        }

        for (int i = 0; i < effect_ptr->GetInputPinCount(); ++i) {
            ImNodes::BeginInputAttribute(effect_ptr->id * 10 + 1 + i); // Input pins: ID is effect_id * 10 + 1 + index
            ImGui::Text("in %d", i);
            ImNodes::EndInputAttribute();
        }
        // Optional: effect_ptr->RenderUI();
        ImNodes::EndNode();
    }

    // 2. Draw all existing links
    int link_id_counter = 1; // Ensure link IDs are unique and non-zero
    for (const auto& effect_ptr : g_scene) {
        if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
            const auto& inputs = se->GetInputs();
            for (size_t i = 0; i < inputs.size(); ++i) {
                if (inputs[i]) {
                    int start_pin_id = inputs[i]->id * 10 + 0; // Output pin 0 of the source effect
                    int end_pin_id = se->id * 10 + 1 + i;   // Input pin i of the current effect
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
        bool end_is_input = (end_attr % 10 != 0); // True if (ID % 10) is 1, 2, etc.

        // Case 1: Dragged from output to input
        if (start_effect && end_effect && start_node_id != end_node_id) {
            if (start_is_output && end_is_input) {
                int input_pin_index = (end_attr % 10) - 1;
                 if (input_pin_index >= 0 && input_pin_index < end_effect->GetInputPinCount()) {
                    end_effect->SetInputEffect(input_pin_index, start_effect);
                }
            }
            // Case 2: Dragged from input to output (reversed connection)
            else if (!start_is_output && end_is_input == false) { // !start_is_output means start_attr is an input pin, !end_is_input means end_attr is an output pin
                int input_pin_index = (start_attr % 10) - 1; // Pin index of start_effect (which is the target)
                 if (input_pin_index >= 0 && input_pin_index < start_effect->GetInputPinCount()){
                    start_effect->SetInputEffect(input_pin_index, end_effect); // end_effect is the source
                 }
            }
        }
    }
    // Link destruction handling (simplified as per plan)
    int link_id_destroyed;
    if (ImNodes::IsLinkDestroyed(&link_id_destroyed)) {
        // For robust removal, we'd need to map link_id_destroyed back to the specific
        // (target_effect, input_pin_index) and call SetInputEffect(pin, nullptr).
        // This requires storing link IDs or iterating.
        // Simplified for now: user has to manually manage if a link is visually gone but logically persists.
        // Or, one could iterate all effects and clear any input that no longer has a visual link.
        // For this step, we just acknowledge it as per the user's plan.
        g_shaderLoadError_global = "Link destroyed. (Note: precise model update for destruction not fully implemented).";
    }
    ImGui::End();
}


// --- Topological Sort for Rendering Order ---
std::vector<Effect*> GetRenderOrder(const std::vector<Effect*>& activeEffects) { /* ... (as before) ... */
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
    // Effect* plasmaPtr = plasmaEffect.get(); // Keep for manual linking if needed
    g_scene.push_back(std::move(plasmaEffect));

    auto passthroughEffect = std::make_unique<ShaderEffect>("shaders/passthrough.frag", SCR_WIDTH, SCR_HEIGHT);
    passthroughEffect->name = "Passthrough"; passthroughEffect->startTime = 0.f; passthroughEffect->endTime = 30.f;
    // Effect* passthroughPtr = passthroughEffect.get();
    g_scene.push_back(std::move(passthroughEffect));

    // Manual connection for testing (can be commented out once UI linking is robust)
    // if (g_scene.size() >= 2) {
    //     if (auto* se_passthrough = dynamic_cast<ShaderEffect*>(g_scene[1].get())) {
    //         se_passthrough->SetInputEffect(0, g_scene[0].get());
    //     }
    // }

    if (!g_scene.empty() && !g_selectedEffect) g_selectedEffect = g_scene.front().get();
    for (const auto& effect_ptr : g_scene) effect_ptr->Load();
    if (g_selectedEffect) if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) g_editor.SetText(se->GetShaderSource());

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImNodes::CreateContext(); // Re-enabled
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
            ImGui::Begin("Effect Properties"); /* ... (content as before, using g_selectedEffect->RenderUI()) ... */ ImGui::End();
            ImGui::Begin("Shader Editor"); /* ... (content as before, using editorSE) ... */ ImGui::End();
            ImGui::Begin("Console"); /* ... (content as before) ... */ ImGui::End();
            RenderTimelineWindow();
            RenderNodeEditorWindow(); // Re-enabled
            if(g_showHelpWindow) { ImGui::Begin("Help"); /* ... */ ImGui::End(); }
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
    ImNodes::DestroyContext(); // Re-enabled
    ImGui::DestroyContext();
    if (g_quadVAO != 0) glDeleteVertexArrays(1, &g_quadVAO);
    if (g_quadVBO != 0) glDeleteBuffers(1, &g_quadVBO);
    glfwTerminate(); return 0;
} 

void framebuffer_size_callback(GLFWwindow* w, int width, int height) { /* ... (as before) ... */ }
void processInput(GLFWwindow *w) { /* ... (as before) ... */ }
void mouse_cursor_position_callback(GLFWwindow* w, double x, double y) { /* ... (as before) ... */ }
void mouse_button_callback(GLFWwindow* w, int btn, int act, int mod) { /* ... (as before) ... */ }
