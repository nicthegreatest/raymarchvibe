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
#include "ImGuiSimpleTimeline.h" // Use the custom simple timeline
// #include "imnodes.h" // Temporarily commented out for timeline test


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

// Scene Management
static std::vector<std::unique_ptr<Effect>> g_scene;
static Effect* g_selectedEffect = nullptr;
static int g_selectedTimelineItem = -1; // For ImGuiSimpleTimeline

// Input State
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// Renderer instance
Renderer g_renderer;
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
    {"--- Select ---", ""}, {"Plasma", "shaders/raymarch_v1.frag"}, {"UV Pattern", "shaders/samples/uv_pattern.frag"}, {"Passthrough", "shaders/passthrough.frag"}
};
static size_t g_currentSampleIndex = 0;

const char* nativeShaderTemplate = "// Native Template\n#version 330 core\nout vec4 FragColor; uniform vec2 iResolution; uniform float iTime; uniform vec3 u_objectColor; void main(){FragColor=vec4(u_objectColor,1.0);}";
const char* shadertoyShaderTemplate = "// Shadertoy Template\nvoid mainImage(out vec4 C,in vec2 U){vec2 R=iResolution.xy;C=vec4(U/R,0.5+0.5*sin(iTime),1);}" ;
std::string FetchShadertoyCodeOnline(const std::string& id, const std::string& key, std::string& err) { /* Placeholder */ err="Network disabled"; return "";}


void RenderTimelineWindow() {
    ImGui::Begin("Timeline");

    std::vector<ImGui::TimelineItem> timelineItems;
    std::vector<int> tracks;
    tracks.resize(g_scene.size());

    for (size_t i = 0; i < g_scene.size(); ++i) {
        if(!g_scene[i]) continue;
        tracks[i] = i % 4; // Simple track assignment
        timelineItems.push_back({
            g_scene[i]->name,
            &g_scene[i]->startTime,
            &g_scene[i]->endTime,
            &tracks[i]
        });
    }

    // float currentTimeForRuler = (float)glfwGetTime(); // Old: auto-plays with app time
    static float currentTimeForRuler = 0.0f; // New: Static, starts at 0, no auto-play from glfwGetTime()
                                             // User can drag this, or future play/pause controls would modify it.

    if (ImGui::SimpleTimeline("Scene", timelineItems, &currentTimeForRuler, &g_selectedTimelineItem, 4, 0.0f, 60.0f)) {
        if (g_selectedTimelineItem >= 0 && static_cast<size_t>(g_selectedTimelineItem) < g_scene.size()) {
            g_selectedEffect = g_scene[g_selectedTimelineItem].get();
            if(auto* se = dynamic_cast<ShaderEffect*>(g_selectedEffect)) {
                g_editor.SetText(se->GetShaderSource());
                 const std::string& log = se->GetCompileErrorLog();
                if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos)
                    g_editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
            }
        }
    }
    ImGui::End();
}

// Node Editor stubs from previous phase, will be filled if plan continues to Phase 5
// static Effect* FindEffectById(int nodeId) { /* ... */ return nullptr;}
// static int GetNodeIdFromPinAttr(int attr_id) { /* ... */ return 0;}
// static int GetPinIndexFromPinAttr(int attr_id) { /* ... */ return 0;}
// static bool IsOutputPin(int attr_id) { /* ... */ return false;}
// void RenderNodeEditorWindow() { ImGui::Begin("Node Editor (Placeholder)"); ImGui::Text("Node editor UI will be here."); ImGui::End(); } // Temporarily commented out


int main() {
    if (!glfwInit()) { std::cerr << "GLFW Init failed." << std::endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe", NULL, NULL); // Title updated
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
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

    static char filePathBuffer_Load[256] = "shaders/raymarch_v1.frag";
    static char filePathBuffer_SaveAs[256] = "shaders/my_new_shader.frag";
    static char shadertoyInputBuffer[256] = "Ms2SD1";
    static std::string shadertoyApiKey = "REPLACE_ME";

    float deltaTime = 0.0f, lastFrameTime_main = 0.0f;

    // Initialize Scene
    auto effect1 = std::make_unique<ShaderEffect>(filePathBuffer_Load, SCR_WIDTH, SCR_HEIGHT);
    effect1->name = "Plasma"; effect1->startTime = 0.0f; effect1->endTime = 10.0f;
    g_scene.push_back(std::move(effect1));

    auto effect2 = std::make_unique<ShaderEffect>("shaders/samples/uv_pattern.frag", SCR_WIDTH, SCR_HEIGHT);
    effect2->name = "UV Pattern"; effect2->startTime = 8.0f; effect2->endTime = 18.0f;
    g_scene.push_back(std::move(effect2));

    auto effect3 = std::make_unique<ShaderEffect>("shaders/passthrough.frag", SCR_WIDTH, SCR_HEIGHT);
    effect3->name = "Passthrough"; effect3->startTime = 5.0f; effect3->endTime = 15.0f;
    // To test input: effect3->SetInputEffect(0, g_scene[0].get()); // Plasma feeds Passthrough
    g_scene.push_back(std::move(effect3));


    if (!g_scene.empty()) g_selectedEffect = g_scene.front().get();

    for (const auto& effect_ptr : g_scene) effect_ptr->Load();

    if (g_selectedEffect) {
        ShaderEffect* se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
        if (se) { g_editor.SetText(se->GetShaderSource()); /* set markers, global error log */ }
    }

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); // imnodes::CreateContext(); // Temporarily commented out
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 330");
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

    lastFrameTime_main = (float)glfwGetTime();
    int f5State = GLFW_RELEASE; // Moved F5 state here as it's used in editor UI logic

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastFrameTime_main;
        lastFrameTime_main = currentTime;

        processInput(window);

        for (const auto& effect_ptr : g_scene) {
            if (currentTime >= effect_ptr->startTime && currentTime < effect_ptr->endTime) {
                ShaderEffect* se = dynamic_cast<ShaderEffect*>(effect_ptr.get());
                if(se){
                    if(effect_ptr.get() == g_selectedEffect){ // Only update selected effect fully for now
                        int current_display_w, current_display_h;
                        glfwGetFramebufferSize(window, &current_display_w, &current_display_h);
                        se->SetDisplayResolution(current_display_w, current_display_h);
                        se->SetMouseState(g_mouseState[0],g_mouseState[1],g_mouseState[2],g_mouseState[3]);
                    }
                    se->SetDeltaTime(deltaTime); se->IncrementFrameCount();
                }
                effect_ptr->Update(currentTime);
                effect_ptr->Render();
            }
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        if (g_showGui) {
            ImGui::Begin("Effect Properties");
            if(ImGui::Button("Help")) g_showHelpWindow = true; ImGui::SameLine();
            if(ImGui::Button("Snap Win")) g_snapWindowsNextFrame = true;
            ImGui::Separator();
            if(g_selectedEffect) g_selectedEffect->RenderUI(); else ImGui::Text("No effect selected.");
            ImGui::Separator(); ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::End();

            ImGui::Begin("Shader Editor");
            ShaderEffect* editorSE = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            // F5 keybind logic
            int currentF5State_Editor = glfwGetKey(window, GLFW_KEY_F5);
            if (currentF5State_Editor == GLFW_PRESS && f5State == GLFW_RELEASE && !io.WantTextInput && editorSE) {
                editorSE->ApplyShaderCode(g_editor.GetText());
                const std::string& log = editorSE->GetCompileErrorLog();
                if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) g_editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                g_shaderLoadError_global = "Applied (F5). Status: " + log;
            }
            f5State = currentF5State_Editor;

            if (ImGui::CollapsingHeader("Load/Save Actions")) {
                // --- Load from Shadertoy ---
                ImGui::InputTextWithHint("##STInput", "Shadertoy ID/URL", shadertoyInputBuffer, sizeof(shadertoyInputBuffer)); ImGui::SameLine();
                if (ImGui::Button("Fetch & Apply##STApply") && editorSE) {
                    std::string id = ShaderParser::ExtractShaderId(shadertoyInputBuffer);
                    if (!id.empty()) {
                        std::string fetchErr, code = FetchShadertoyCodeOnline(id, shadertoyApiKey, fetchErr);
                        if (!code.empty()) {
                            editorSE->LoadShaderFromSource(code);
                            editorSE->SetShadertoyMode(true);
                            editorSE->Load();
                            g_editor.SetText(editorSE->GetShaderSource());
                            std::string stPath = "Shadertoy_" + id + ".frag";
                            strncpy(filePathBuffer_Load, stPath.c_str(), sizeof(filePathBuffer_Load)-1); filePathBuffer_Load[sizeof(filePathBuffer_Load)-1]=0;
                            strncpy(filePathBuffer_SaveAs, stPath.c_str(), sizeof(filePathBuffer_SaveAs)-1); filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs)-1]=0;
                            const std::string& log = editorSE->GetCompileErrorLog();
                            if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) g_editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                            g_shaderLoadError_global = "Fetched Shadertoy '" + id + "'. Status: " + log;
                        } else g_shaderLoadError_global = "Fetch Error: " + fetchErr;
                    } else g_shaderLoadError_global = "Invalid Shadertoy ID.";
                }
                // ImGui::SameLine(); // Optional: "Load to Editor" button for ST
                // if (ImGui::Button("Load to Editor##STLoadToEditor") && editorSE) { /* ... similar logic but only editorSE->LoadShaderFromSource() and g_editor.SetText() ... */ }


                // --- Load Sample Shader ---
                if (ImGui::BeginCombo("##SampleCombo", shaderSamples[g_currentSampleIndex].name)) {
                    for (size_t n = 0; n < shaderSamples.size(); n++) {
                        const bool is_selected = (g_currentSampleIndex == n);
                        if (ImGui::Selectable(shaderSamples[n].name, is_selected)) g_currentSampleIndex = n;
                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button("Load & Apply Sample##SampleApply") && editorSE && g_currentSampleIndex > 0 && g_currentSampleIndex < shaderSamples.size()) {
                    std::string path = shaderSamples[g_currentSampleIndex].filePath;
                    if (editorSE->LoadShaderFromFile(path)) {
                        editorSE->Load();
                        g_editor.SetText(editorSE->GetShaderSource());
                        strncpy(filePathBuffer_Load, path.c_str(), sizeof(filePathBuffer_Load)-1); filePathBuffer_Load[sizeof(filePathBuffer_Load)-1]=0;
                        strncpy(filePathBuffer_SaveAs, path.c_str(), sizeof(filePathBuffer_SaveAs)-1); filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs)-1]=0;
                        const std::string& log = editorSE->GetCompileErrorLog();
                        if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) g_editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                        g_shaderLoadError_global = "Loaded sample '" + std::string(shaderSamples[g_currentSampleIndex].name) + "'. Status: " + log;
                    } else g_shaderLoadError_global = "Failed to load sample file " + path;
                }
                ImGui::Separator();

                // --- Load From File ---
                ImGui::InputText("Path##LoadFile", filePathBuffer_Load, sizeof(filePathBuffer_Load)); ImGui::SameLine();
                if (ImGui::Button("Load & Apply##FileApply") && editorSE) {
                    if (editorSE->LoadShaderFromFile(filePathBuffer_Load)) {
                        editorSE->Load();
                        g_editor.SetText(editorSE->GetShaderSource());
                        strncpy(filePathBuffer_SaveAs, filePathBuffer_Load, sizeof(filePathBuffer_SaveAs)-1); filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs)-1]=0;
                        const std::string& log = editorSE->GetCompileErrorLog();
                        if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) g_editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                        g_shaderLoadError_global = "Loaded file '" + std::string(filePathBuffer_Load) + "'. Status: " + log;
                    } else g_shaderLoadError_global = "Failed to load file " + std::string(filePathBuffer_Load);
                }
                ImGui::Separator();

                // --- New Shader ---
                if (ImGui::Button("New Native") && editorSE) {
                    editorSE->LoadShaderFromSource(nativeShaderTemplate);
                    editorSE->SetShadertoyMode(false);
                    // Don't call Load() immediately, let user Apply from Editor
                    g_editor.SetText(editorSE->GetShaderSource());
                    strncpy(filePathBuffer_Load, "Untitled_Native.frag", sizeof(filePathBuffer_Load)-1);filePathBuffer_Load[sizeof(filePathBuffer_Load)-1]=0;
                    strncpy(filePathBuffer_SaveAs, "Untitled_Native.frag", sizeof(filePathBuffer_SaveAs)-1);filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs)-1]=0;
                    ClearErrorMarkers();
                    g_shaderLoadError_global = "Native template loaded to editor. Apply to compile.";
                } ImGui::SameLine();
                if (ImGui::Button("New Shadertoy") && editorSE) {
                    editorSE->LoadShaderFromSource(shadertoyShaderTemplate);
                    editorSE->SetShadertoyMode(true);
                    g_editor.SetText(editorSE->GetShaderSource());
                    strncpy(filePathBuffer_Load, "Untitled_Shadertoy.frag", sizeof(filePathBuffer_Load)-1);filePathBuffer_Load[sizeof(filePathBuffer_Load)-1]=0;
                    strncpy(filePathBuffer_SaveAs, "Untitled_Shadertoy.frag", sizeof(filePathBuffer_SaveAs)-1);filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs)-1]=0;
                    ClearErrorMarkers();
                    g_shaderLoadError_global = "Shadertoy template loaded to editor. Apply to compile.";
                }
                ImGui::Separator();

                // --- Save Shader ---
                ImGui::Text("Current Editing File: %s", editorSE ? editorSE->GetShaderFilePath().c_str() : "N/A");
                if (ImGui::Button("Save Current File") && editorSE) {
                    std::ofstream outFile(editorSE->GetShaderFilePath());
                    if (outFile.is_open()) { outFile << g_editor.GetText(); outFile.close(); g_shaderLoadError_global = "Saved: " + editorSE->GetShaderFilePath(); }
                    else { g_shaderLoadError_global = "ERROR saving to: " + editorSE->GetShaderFilePath(); }
                }
                ImGui::InputText("Save As Path", filePathBuffer_SaveAs, sizeof(filePathBuffer_SaveAs)); ImGui::SameLine();
                if (ImGui::Button("Save As...") && editorSE) {
                    std::string saveAsPathStr(filePathBuffer_SaveAs);
                    if(!saveAsPathStr.empty()){
                        std::ofstream outFile(saveAsPathStr);
                        if (outFile.is_open()) {
                            outFile << g_editor.GetText(); outFile.close();
                            // Update the ShaderEffect's path and the load buffer
                            editorSE->LoadShaderFromFile(saveAsPathStr); // This updates internal path and source
                            // editorSE->ApplyShaderCode(g_editor.GetText()); // Re-apply if needed, or just update path
                            strncpy(filePathBuffer_Load, saveAsPathStr.c_str(), sizeof(filePathBuffer_Load) -1); filePathBuffer_Load[sizeof(filePathBuffer_Load)-1] = 0;
                            g_shaderLoadError_global = "Saved to: " + saveAsPathStr;
                        } else { g_shaderLoadError_global = "ERROR saving to: " + saveAsPathStr; }
                    } else { g_shaderLoadError_global = "Save As path empty."; }
                }
            } // End Load/Save Actions Collapsing Header

            // Editor and Apply button
            const std::string& currentEffectLog = editorSE ? editorSE->GetCompileErrorLog() : "";
            if (!g_shaderLoadError_global.empty() && (currentEffectLog.empty() || currentEffectLog.find("Successfully") != std::string::npos || currentEffectLog.find("applied successfully") != std::string::npos))
                 ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "Status: %s", g_shaderLoadError_global.c_str());
            else if (!currentEffectLog.empty())
                 ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "Effect Status: %s", currentEffectLog.c_str());
            ImGui::Separator();
            g_editor.Render("ShaderSourceEditor", ImVec2(-1, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()*1.2f));
            if (ImGui::Button("Apply from Editor") && editorSE) { /* Apply logic */ }
            ImGui::End();

            ImGui::Begin("Console"); /* ... Console UI ... */ ImGui::End();
            RenderTimelineWindow();
            // RenderNodeEditorWindow(); // Temporarily commented out
            if(g_showHelpWindow) { ImGui::Begin("Help", &g_showHelpWindow); /* ... */ ImGui::End(); }
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (const auto& effect_ptr : g_scene) {
            if (currentTime >= effect_ptr->startTime && currentTime < effect_ptr->endTime) {
                if (auto* se = dynamic_cast<ShaderEffect*>(effect_ptr.get())) {
                    if (se->GetOutputTexture() != 0) g_renderer.RenderFullscreenTexture(se->GetOutputTexture());
                }
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

void framebuffer_size_callback(GLFWwindow* w, int width, int height) {
    (void)w; if (width>0 && height>0) { glViewport(0,0,width,height); for(const auto& e:g_scene) if(auto* se=dynamic_cast<ShaderEffect*>(e.get())) se->ResizeFrameBuffer(width,height); }
}
void processInput(GLFWwindow *w) { if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(w, true); }

void mouse_cursor_position_callback(GLFWwindow* w, double x, double y) {
    ImGuiIO& io = ImGui::GetIO();
    ShaderEffect* se = g_selectedEffect ? dynamic_cast<ShaderEffect*>(g_selectedEffect) : nullptr;
    bool stMode = se ? se->IsShadertoyMode() : false;
    if (!io.WantCaptureMouse || stMode) { int H; glfwGetWindowSize(w,NULL,&H); g_mouseState[0]=(float)x; g_mouseState[1]=(float)H-(float)y; }
}
void mouse_button_callback(GLFWwindow* w, int btn, int act, int mod) {
    (void)mod; ImGuiIO& io = ImGui::GetIO();
    ShaderEffect* se = g_selectedEffect ? dynamic_cast<ShaderEffect*>(g_selectedEffect) : nullptr;
    bool stMode = se ? se->IsShadertoyMode() : false;
    if (!io.WantCaptureMouse || stMode) { if(btn==GLFW_MOUSE_BUTTON_LEFT){if(act==GLFW_PRESS){g_mouseState[2]=g_mouseState[0];g_mouseState[3]=g_mouseState[1];}else if(act==GLFW_RELEASE){g_mouseState[2]=-std::abs(g_mouseState[2]);g_mouseState[3]=-std::abs(g_mouseState[3]);}}}
    if(glfwGetMouseButton(w,GLFW_MOUSE_BUTTON_LEFT)==GLFW_RELEASE){if(g_mouseState[2]>0.f)g_mouseState[2]=-g_mouseState[2]; if(g_mouseState[3]>0.f)g_mouseState[3]=-g_mouseState[3];}
}
