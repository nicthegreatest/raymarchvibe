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
#include "ImGuiTimeline.h"
#include "imnodes.h" // Added for Node Editor


// Window dimensions
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window); 
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void RenderTimelineWindow();
void RenderNodeEditorWindow(); // Forward declaration for Node Editor

// Scene Management
static std::vector<std::unique_ptr<Effect>> g_scene;
static Effect* g_selectedEffect = nullptr;

// Input State
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// Renderer instance
Renderer g_renderer;
// Global Quad VAO for effects to use when rendering to their FBOs
// ShaderEffect.cpp will declare this as extern.
GLuint g_quadVAO = 0;
GLuint g_quadVBO = 0;


std::string load_file_to_string(const char* filePath, std::string& errorMsg) {
    errorMsg.clear(); std::ifstream fs; std::stringstream ss;
    fs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try { fs.open(filePath); ss << fs.rdbuf(); fs.close(); }
    catch (const std::ifstream::failure& e) { errorMsg = "ERR: " + std::string(filePath) + " - " + e.what(); return ""; }
    return ss.str();
}
    
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log) {
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
static const std::vector<ShaderSample> shaderSamples = {
    {"--- Select ---", ""}, {"Raymarch v1", "shaders/raymarch_v1.frag"}, {"UV Pattern", "shaders/samples/uv_pattern.frag"}, /* more */
};
static size_t g_currentSampleIndex = 0;

const char* nativeShaderTemplate = "// Native Template\nvoid main(){FragColor=vec4(0.1,0.1,0.1,1.0);}";
const char* shadertoyShaderTemplate = "// Shadertoy Template\nvoid mainImage(out vec4 C,in vec2 U){C=vec4(U.x/iResolution.x,U.y/iResolution.y,0.5+0.5*sin(iTime),1);}" ;

std::string FetchShadertoyCodeOnline(const std::string& id, const std::string& key, std::string& err) { /* Shortened for brevity */ return "";}


void RenderTimelineWindow() {
    ImGui::Begin("Timeline");
    static float timeline_start = 0.0f, timeline_end = 60.0f;
    float current_time_cursor = (float)glfwGetTime();
    ImGui::Timeline("Scene Timeline", &timeline_start, &timeline_end, &current_time_cursor, ImGuiTimelineFlags_None);
    ImGui::BeginTimelineGroup("Effects");
    for (size_t i = 0; i < g_scene.size(); ++i) {
        if (!g_scene[i]) continue;
        if (ImGui::TimelineEvent(g_scene[i]->name.c_str(), &g_scene[i]->startTime, &g_scene[i]->endTime)) {
            g_selectedEffect = g_scene[i].get();
            if (auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                g_editor.SetText(se->GetShaderSource());
                const std::string& log = se->GetCompileErrorLog(); // Refresh error markers if any
                if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos)
                    g_editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
            }
        }
    }
    ImGui::EndTimelineGroup();
    ImGui::End();
}

// --- Node Editor Helper Functions ---
static Effect* FindEffectById(int nodeId) {
    for (const auto& effect_ptr : g_scene) {
        if (effect_ptr && effect_ptr->id == nodeId) {
            return effect_ptr.get();
        }
    }
    return nullptr;
}

// Pin IDs are constructed as: (node_id << (is_output ? 16 : 8)) + pin_index
// Helper to decode Node ID from a pin attribute ID
static int GetNodeIdFromPinAttr(int attr_id) {
    if ((attr_id >> 16) != 0) return (attr_id >> 16); // Output pin
    if ((attr_id >> 8) != 0) return (attr_id >> 8);   // Input pin
    return 0; // Should not happen for valid pin IDs
}

// Helper to decode Pin Index from a pin attribute ID
static int GetPinIndexFromPinAttr(int attr_id) {
    return attr_id & 0xFF;
}

// Helper to check if a pin attribute ID is an output pin
static bool IsOutputPin(int attr_id) {
    return (attr_id >> 16) != 0;
}


void RenderNodeEditorWindow() {
    ImGui::Begin("Node Editor");
    imnodes::BeginNodeEditor();

    // Draw nodes
    for (const auto& effect_ptr : g_scene) {
        if (!effect_ptr) continue;

        imnodes::BeginNode(effect_ptr->id);

        imnodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(effect_ptr->name.c_str());
        imnodes::EndNodeTitleBar();

        // Input pins
        for (int i = 0; i < effect_ptr->GetInputPinCount(); ++i) {
            // Construct unique ID for input pin: (node_id << 8) + pin_index
            imnodes::BeginInputAttribute((effect_ptr->id << 8) + i);
            ImGui::Text("Input %d", i);
            imnodes::EndInputAttribute();
        }

        // Output pins
        for (int i = 0; i < effect_ptr->GetOutputPinCount(); ++i) {
            // Construct unique ID for output pin: (node_id << 16) + pin_index
            imnodes::BeginOutputAttribute((effect_ptr->id << 16) + i);
            ImGui::Text("Output %d", i);
            imnodes::EndOutputAttribute();
        }

        // Render effect's specific UI inside the node
        // ImGui::Spacing();
        // effect_ptr->RenderUI(); // This can make nodes very large, consider a button to show/hide params

        imnodes::EndNode();
    }

    // Draw links (iterate again after all nodes are drawn)
    // For a link, we need a unique ID. We can form one from the connected pin IDs.
    int link_uid_counter = 0; // Simple unique ID for links
    for (const auto& target_effect_ptr : g_scene) {
        if (!target_effect_ptr) continue;
        ShaderEffect* target_se = dynamic_cast<ShaderEffect*>(target_effect_ptr.get());
        if (target_se) {
            for (int i = 0; i < target_se->GetInputPinCount(); ++i) {
                if (i < static_cast<int>(target_se->m_inputs.size()) && target_se->m_inputs[i] != nullptr) {
                    Effect* source_effect = target_se->m_inputs[i];
                    if (source_effect) {
                        // Assuming output pin 0 for source for simplicity
                        int source_pin_attr = (source_effect->id << 16) + 0;
                        int target_pin_attr = (target_effect_ptr->id << 8) + i;
                        imnodes::Link(target_effect_ptr->id + source_effect->id + i + (++link_uid_counter), source_pin_attr, target_pin_attr);
                    }
                }
            }
        }
    }

    imnodes::EndNodeEditor();

    // Handle link creation
    int start_attr, end_attr;
    if (imnodes::IsLinkCreated(&start_attr, &end_attr)) {
        Effect* start_node = nullptr;
        Effect* end_node = nullptr;
        int start_pin_idx = -1;
        int end_pin_idx = -1;

        // Determine which is output and which is input based on our ID scheme
        if (IsOutputPin(start_attr) && !IsOutputPin(end_attr)) { // start is output, end is input
            start_node = FindEffectById(GetNodeIdFromPinAttr(start_attr));
            // start_pin_idx = GetPinIndexFromPinAttr(start_attr); // Output pin index (usually 0)
            end_node = FindEffectById(GetNodeIdFromPinAttr(end_attr));
            end_pin_idx = GetPinIndexFromPinAttr(end_attr);
        } else if (!IsOutputPin(start_attr) && IsOutputPin(end_attr)) { // start is input, end is output (dragged backwards)
            start_node = FindEffectById(GetNodeIdFromPinAttr(end_attr)); // end_attr is the output pin
            // start_pin_idx = GetPinIndexFromPinAttr(end_attr);    // Output pin index
            end_node = FindEffectById(GetNodeIdFromPinAttr(start_attr));   // start_attr is the input pin
            end_pin_idx = GetPinIndexFromPinAttr(start_attr);
        }

        if (start_node && end_node && end_pin_idx != -1) {
            // Ensure not connecting to self or creating cyclic dependencies (simple check for now)
            if (start_node->id != end_node->id) {
                 std::cout << "Linking output of Node " << start_node->id << " to input " << end_pin_idx << " of Node " << end_node->id << std::endl;
                end_node->SetInputEffect(end_pin_idx, start_node);
            }
        }
    }

    // Handle link destruction
    int link_id_destroyed;
    if (imnodes::IsLinkDestroyed(&link_id_destroyed)) {
        // This is more complex: link_id_destroyed is the one we passed to imnodes::Link().
        // We need to find which connection this ID corresponds to.
        // This requires storing the link IDs or iterating through connections to find the one to remove.
        // For now, a simple approach: iterate all effects and clear inputs that match this.
        // A more robust system would map link_id_destroyed back to the specific (target_effect, pin_index) pair.
        // For simplicity in this step, if a link is destroyed, we might just clear all inputs of all effects
        // and let them be re-established, or iterate and find the specific one if we stored enough info.
        // Placeholder:
        // Find the effects and pin involved in link_id_destroyed and call SetInputEffect(pin, nullptr)
        // This is non-trivial without storing the link IDs with their connections.
        // A simpler, though less precise, way if only one input per node is to find the target node and clear its input.
        // For now, we'll just log it. A full implementation needs a map from link_id to connection details.
        std::cout << "Link with ID " << link_id_destroyed << " destroyed. Manual input clearing might be needed or find the connection." << std::endl;
    }

    ImGui::End(); // End Node Editor window
}


int main() {
    if (!glfwInit()) { std::cerr << "GLFW Init failed." << std::endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe - Phase 4", NULL, NULL);
    if (!window) { std::cerr << "Window creation failed." << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD Init failed." << std::endl; glfwTerminate(); return -1; }

    if (!g_renderer.Init()) { // Initialize Renderer (loads compositing shader, creates its own quad)
        std::cerr << "Failed to initialize Renderer." << std::endl;
        glfwTerminate(); return -1;
    }

    // Setup global quad VAO for effects (ShaderEffect will use this via extern)
    float quadVertices[] = { -1.0f,  1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,  1.0f, 1.0f, -1.0f, 1.0f,  1.0f };
    glGenVertexArrays(1, &g_quadVAO); glGenBuffers(1, &g_quadVBO);
    glBindVertexArray(g_quadVAO); glBindBuffer(GL_ARRAY_BUFFER, g_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);


    static char filePathBuffer_Load[256] = "shaders/raymarch_v1.frag";
    static char filePathBuffer_SaveAs[256] = "shaders/my_new_shader.frag";
    // ... other static UI buffers ...

    float deltaTime = 0.0f, lastFrameTime_main = 0.0f;

    // Initialize Scene
    // Effect 1: Plasma (Output)
    auto plasmaEffect = std::make_unique<ShaderEffect>("shaders/raymarch_v1.frag", SCR_WIDTH, SCR_HEIGHT);
    plasmaEffect->name = "Plasma";
    plasmaEffect->startTime = 0.0f;
    plasmaEffect->endTime = 30.0f; // Active for a longer duration
    g_scene.push_back(std::move(plasmaEffect));

    if (!g_selectedEffect && !g_scene.empty()) {
        g_selectedEffect = g_scene.front().get(); // Select Plasma by default
    }

    // Effect 2: Passthrough (Input from Plasma)
    auto passthroughEffect = std::make_unique<ShaderEffect>("shaders/passthrough.frag", SCR_WIDTH, SCR_HEIGHT);
    passthroughEffect->name = "Passthrough";
    passthroughEffect->startTime = 0.0f; // Can be active at the same time
    passthroughEffect->endTime = 30.0f;
    // We will manually set its input for now for basic testing,
    // node editor will handle this dynamically.
    // if (g_scene.size() >= 1) { // Ensure plasmaEffect was added
    //    passthroughEffect->SetInputEffect(0, g_scene[0].get()); // Input from Plasma
    // }
    g_scene.push_back(std::move(passthroughEffect));

    // Load all effects in the scene initially.
    // In a more complex setup, effects might be loaded on demand or when they become active.
    for (const auto& effect_ptr : g_scene) {
        effect_ptr->Load(); // This will also create their FBOs with initial SCR_WIDTH, SCR_HEIGHT
    }

    // If g_selectedEffect was set, populate editor
    if (g_selectedEffect) {
        ShaderEffect* se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
        if (se) {
            g_editor.SetText(se->GetShaderSource());
            const std::string& log = se->GetCompileErrorLog();
            if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos)
                g_editor.SetErrorMarkers(ParseGlslErrorLog(log));
            else ClearErrorMarkers();
            g_shaderLoadError_global = log.find("failed") != std::string::npos || log.find("ERROR") != std::string::npos ? "Initial load failed for " + se->name + ": " + log : "Initial shader (" + se->name + "): " + log;
        }
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imnodes::CreateContext(); // Initialize imnodes context
    ImGuiIO& io = ImGui::GetIO(); (void)io; // Prevent unused variable warning if io not used
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 330");
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

    lastFrameTime_main = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastFrameTime_main;
        lastFrameTime_main = currentTime;

        // Keybinds for GUI, Fullscreen (no changes needed here from previous structure)
        // ...

        processInput(window);

        // --- RENDER-TO-FBO PASS ---
        for (const auto& effect_ptr : g_scene) {
            if (currentTime >= effect_ptr->startTime && currentTime < effect_ptr->endTime) {
                // Update effect state (mouse, resolution for uniforms if needed by effect logic)
                ShaderEffect* se = dynamic_cast<ShaderEffect*>(effect_ptr.get());
                if(se){ // If it's a ShaderEffect, it might need these updates
                    int fbo_w, fbo_h; // ShaderEffect should know its own FBO dimensions
                                      // For now, assume g_selectedEffect's updates are sufficient if it's this effect
                                      // Or each effect gets its own specific update from main
                    if(effect_ptr.get() == g_selectedEffect && se){
                        int current_display_w, current_display_h;
                        glfwGetFramebufferSize(window, &current_display_w, &current_display_h); // For iResolution if needed by selected
                        se->SetDisplayResolution(current_display_w, current_display_h); // For uniforms like iResolution
                        se->SetMouseState(g_mouseState[0],g_mouseState[1],g_mouseState[2],g_mouseState[3]);
                        se->SetDeltaTime(deltaTime);
                        se->IncrementFrameCount();
                    } else if (se) { // Non-selected but active ShaderEffect
                        // Minimal update: time, frame count
                        se->SetDeltaTime(deltaTime); // If it uses iTimeDelta
                        se->IncrementFrameCount();   // If it uses iFrame
                    }
                }
                effect_ptr->Update(currentTime); // General effect update
                effect_ptr->Render();            // Renders to its own FBO
            }
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        if (g_showGui) {
            // Window Snapping, Effect Properties, Shader Editor, Console, Timeline, Help window logic
            // (This part is complex and involves many ImGui calls, assuming structure from previous main.cpp,
            // with "Effect Properties" calling g_selectedEffect->RenderUI(), and Shader Editor calls
            // targeting methods of g_selectedEffect (cast to ShaderEffect*).
            // Timeline window is rendered by RenderTimelineWindow().
            // For brevity, I'm not reproducing all ImGui calls here, but they follow the established pattern.
            ImGui::Begin("Effect Properties"); /* ... calls g_selectedEffect->RenderUI() ... */ ImGui::End();
            ImGui::Begin("Shader Editor"); /* ... editor and buttons interacting with g_selectedEffect ... */ ImGui::End();
            ImGui::Begin("Console"); /* ... log display ... */ ImGui::End();
            RenderTimelineWindow();
            RenderNodeEditorWindow(); // Call the new node editor window
            if(g_showHelpWindow) { /* ... help window ... */ }
        }

        // --- COMPOSITING PASS ---
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Ensure we're rendering to the screen
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT); // No depth clear needed for final composite usually

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (const auto& effect_ptr : g_scene) {
            if (currentTime >= effect_ptr->startTime && currentTime < effect_ptr->endTime) {
                if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
                    if (se->GetOutputTexture() != 0) { // Ensure texture is valid
                        g_renderer.RenderFullscreenTexture(se->GetOutputTexture());
                    }
                }
                // Later, other effect types might also have GetOutputTexture() or a similar mechanism
            }
        }
        glDisable(GL_BLEND);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    } 

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown();
    imnodes::DestroyContext(); // Destroy imnodes context
    ImGui::DestroyContext();
    // g_scene unique_ptrs handle deleting Effects, which clean up their FBOs
    // Renderer g_renderer cleans up its shader/VAO in its destructor (if global) or when it goes out of scope
    if (g_quadVAO != 0) glDeleteVertexArrays(1, &g_quadVAO);
    if (g_quadVBO != 0) glDeleteBuffers(1, &g_quadVBO);
    glfwTerminate(); return 0;
} 

void framebuffer_size_callback(GLFWwindow* w, int width, int height) {
    (void)w;
    if (width > 0 && height > 0) { // Ensure valid dimensions
        glViewport(0,0,width,height);
        // Resize FBOs for all ShaderEffects in the scene
        for (const auto& effect_ptr : g_scene) {
            if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
                se->ResizeFrameBuffer(width, height);
            }
        }
        // The iResolution uniform for ShaderEffects is based on their FBO dimensions,
        // which are updated by ResizeFrameBuffer.
        // If an effect needs to know the final screen resolution for other purposes,
        // that would require a separate mechanism (e.g., another method on Effect).
    }
}
void processInput(GLFWwindow *w) { if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(w, true); }

void mouse_cursor_position_callback(GLFWwindow* w, double x, double y) {
    ImGuiIO& io = ImGui::GetIO();
    ShaderEffect* se = g_selectedEffect ? dynamic_cast<ShaderEffect*>(g_selectedEffect) : nullptr;
    bool effectNeedsMouse = se ? se->IsShadertoyMode() : false;

    if (!io.WantCaptureMouse || effectNeedsMouse) {
        int winH; glfwGetWindowSize(w, NULL, &winH);
        g_mouseState[0]=(float)x; g_mouseState[1]=(float)winH-(float)y;
        // Mouse state is passed to selected effect during its update phase if it's a ShaderEffect
    }
}
void mouse_button_callback(GLFWwindow* w, int btn, int act, int mod) {
    (void)mod; ImGuiIO& io = ImGui::GetIO();
    ShaderEffect* se = g_selectedEffect ? dynamic_cast<ShaderEffect*>(g_selectedEffect) : nullptr;
    bool effectNeedsMouse = se ? se->IsShadertoyMode() : false;

    if (!io.WantCaptureMouse || effectNeedsMouse) {
        if (btn == GLFW_MOUSE_BUTTON_LEFT) {
            if (act == GLFW_PRESS) { g_mouseState[2]=g_mouseState[0]; g_mouseState[3]=g_mouseState[1]; }
            else if (act == GLFW_RELEASE) { g_mouseState[2]=-std::abs(g_mouseState[2]); g_mouseState[3]=-std::abs(g_mouseState[3]); }
        }
    }
    // Mouse state is passed to selected effect during its update phase
    if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT)==GLFW_RELEASE) {
        if(g_mouseState[2]>0.f) g_mouseState[2]=-g_mouseState[2];
        if(g_mouseState[3]>0.f) g_mouseState[3]=-g_mouseState[3];
    }
}
