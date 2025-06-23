// RaymarchVibe - Real-time Shader Exploration
// main.cpp

#include <glad/glad.h> // Must be included before GLFW
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
#include <memory> // For std::unique_ptr

// --- ImGui Includes ---
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// --- HTTP & JSON Library Integration ---
#include "httplib.h" 
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// --- Use ImGuiColorTextEdit for the Shader Code Editor ---
#include "TextEditor.h"

// --- Project Specific Headers ---
#include "Effect.h"
#include "ShaderEffect.h"

// Window dimensions
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window); 

// Scene Management
static std::vector<std::unique_ptr<Effect>> g_scene;
static Effect* g_selectedEffect = nullptr;

// Input State
static float g_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// Callbacks
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

// Helper to load file to string (general utility)
std::string load_file_to_string(const char* filePath, std::string& errorMsg) {
    errorMsg.clear();
    std::ifstream fileStream;
    std::stringstream contentStream;
    fileStream.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        fileStream.open(filePath);
        contentStream << fileStream.rdbuf();
        fileStream.close();
    } catch (const std::ifstream::failure& e) {
        errorMsg = "ERROR::FILE::READ_FAILED: " + std::string(filePath) + " - " + e.what();
        return "";
    }
    return contentStream.str();
}
    
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log);
static TextEditor g_editor;
void ClearErrorMarkers() {
    TextEditor::ErrorMarkers emptyMarkers;
    g_editor.SetErrorMarkers(emptyMarkers);
}

// Global UI & Window State
static bool g_showGui = true;
static bool g_snapWindowsNextFrame = false;
static bool g_showHelpWindow = false;
static std::string g_shaderLoadError_global = "";
static bool g_isFullscreen = false;
static int g_storedWindowX = 100, g_storedWindowY = 100;
static int g_storedWindowWidth = SCR_WIDTH, g_storedWindowHeight = SCR_HEIGHT;

struct ShaderSample { const char* name; const char* filePath; };
static const std::vector<ShaderSample> shaderSamples = {
    {"--- Select a Sample ---", ""}, {"Native Template 1", "shaders/raymarch_v1.frag"},
    {"Native Template 2", "shaders/raymarch_v2.frag"}, {"Simple Red", "shaders/samples/simple_red.frag"},
    // ... add other samples if they exist ...
};
static size_t g_currentSampleIndex = 0;

const char* nativeShaderTemplate = "// RaymarchVibe - Native Template\n// ... (content as before) ...\nvoid main() { FragColor = vec4(u_objectColor,1.0); }";
const char* shadertoyShaderTemplate = "// RaymarchVibe - Shadertoy Template\nvoid mainImage( out vec4 fragColor, in vec2 fragCoord )\n{\n    vec2 uv = fragCoord/iResolution.xy;\n    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));\n    fragColor = vec4(col,1.0);\n}";

// HTTP Fetching (simplified for brevity, ensure it's robust in actual code)
// static std::string ExtractLocalShaderId(const std::string& idOrUrl) { ... } // Removed, use ShaderParser::ExtractShaderId
std::string FetchShadertoyCodeOnline(const std::string& shaderId, const std::string& apiKey, std::string& errorMsg) { /* ... implementation ... */
    errorMsg.clear(); std::string shaderCode;
    try {
        #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            httplib::SSLClient cli("www.shadertoy.com"); 
        #else
            httplib::Client cli("www.shadertoy.com", 80); 
            errorMsg = "Warning: HTTP only for Shadertoy. ";
        #endif
        std::string path = "/api/v1/shaders/" + shaderId + "?key=" + apiKey;
        auto res = cli.Get(path.c_str());
        if (res && res->status == 200) {
            json j = json::parse(res->body);
            if (j.contains("Shader") && j["Shader"]["renderpass"].is_array() && !j["Shader"]["renderpass"].empty()) {
                shaderCode = j["Shader"]["renderpass"][0]["code"].get<std::string>();
            } else { errorMsg += "Unexpected JSON. "; }
        } else { errorMsg += "HTTP Error " + std::to_string(res ? res->status : -1) + ". "; }
    } catch (const std::exception& e) { errorMsg += std::string("Exception: ") + e.what(); }
    return shaderCode;
}

TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log) { /* ... implementation from before ... */
    TextEditor::ErrorMarkers markers; std::stringstream ss(log); std::string line;
    std::regex errorRegex1(R"(^(?:[A-Z]+:\s*)?\d+:(\d+):\s*(.*))");
    std::regex errorRegex2(R"(^\s*\d+\((\d+)\)\s*:\s*(.*))");
    std::smatch match;
    auto local_trim = [](const std::string& s){ size_t f = s.find_first_not_of(" \t\n\r\f\v"); if(f==std::string::npos) return std::string(); size_t l = s.find_last_not_of(" \t\n\r\f\v"); return s.substr(f, (l-f+1));};
    while (std::getline(ss, line)) {
        bool matched = false;
        if (std::regex_search(line, match, errorRegex1) && match.size() >= 3) {
            try { markers[std::stoi(match[1].str())] = local_trim(match[2].str()); matched = true; } catch (const std::exception&) {}
        }
        if (!matched && std::regex_search(line, match, errorRegex2) && match.size() >= 3) {
            try { markers[std::stoi(match[1].str())] = local_trim(match[2].str()); } catch (const std::exception&) {}
        }
    } return markers;
}


// --- Main Application ---
int main() {
    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe", NULL, NULL);
    if (!window) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD" << std::endl; glfwTerminate(); return -1; }

    static char filePathBuffer_Load[256] = "shaders/raymarch_v1.frag";
    static char filePathBuffer_SaveAs[256] = "shaders/my_new_shader.frag";
    static char shadertoyInputBuffer[256] = "Ms2SD1"; 
    static std::string shadertoyApiKey = "REPLACE_ME"; // User should replace this

    float deltaTime = 0.0f;
    float lastFrameTime_main = 0.0f;

    // Initialize Scene
    g_scene.push_back(std::make_unique<ShaderEffect>(filePathBuffer_Load));
    if (!g_scene.empty()) {
        g_selectedEffect = g_scene.front().get();
        if (g_selectedEffect) {
            g_selectedEffect->Load();
            ShaderEffect* currentSE = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            if (currentSE) {
                g_editor.SetText(currentSE->GetShaderSource());
                const std::string& log = currentSE->GetCompileErrorLog();
                if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos)
                    g_editor.SetErrorMarkers(ParseGlslErrorLog(log));
                else ClearErrorMarkers();
                g_shaderLoadError_global = log.find("failed") != std::string::npos || log.find("ERROR") != std::string::npos ? "Initial load failed: " + log : "Initial shader: " + log;
            } else { g_shaderLoadError_global = "Initial effect not ShaderEffect."; g_editor.SetText("// Error.");}
        } else { g_shaderLoadError_global = "No initial effect."; g_editor.SetText("// Error.");}
    } else { g_shaderLoadError_global = "Scene empty."; g_editor.SetText("// Error.");}

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 330");
    g_editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());

    float quadVertices[] = { -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f };
    GLuint quadVAO, quadVBO; 
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
    
    lastFrameTime_main = (float)glfwGetTime();
    int spacebarState = GLFW_RELEASE, f12State = GLFW_RELEASE, f5State = GLFW_RELEASE;

    // The NEW main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastFrameTime_main;
        lastFrameTime_main = currentTime;

        int currentSpacebarState = glfwGetKey(window, GLFW_KEY_SPACE);
        if (currentSpacebarState == GLFW_PRESS && spacebarState == GLFW_RELEASE && !io.WantTextInput) g_showGui = !g_showGui;
        spacebarState = currentSpacebarState;

        int currentF12State = glfwGetKey(window, GLFW_KEY_F12);
        if (currentF12State == GLFW_PRESS && f12State == GLFW_RELEASE && !io.WantTextInput) {
            g_isFullscreen = !g_isFullscreen;
            if (g_isFullscreen) {
                glfwGetWindowPos(window, &g_storedWindowX, &g_storedWindowY);
                glfwGetWindowSize(window, &g_storedWindowWidth, &g_storedWindowHeight);
                GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            } else {
                glfwSetWindowMonitor(window, NULL, g_storedWindowX, g_storedWindowY, g_storedWindowWidth, g_storedWindowHeight, 0);
            }
        }
        f12State = currentF12State;
        processInput(window);

        if (g_selectedEffect) {
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            ShaderEffect* currentSE = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            if (currentSE) {
                currentSE->SetDisplayResolution(display_w, display_h);
                currentSE->SetMouseState(g_mouseState[0], g_mouseState[1], g_mouseState[2], g_mouseState[3]);
                currentSE->SetDeltaTime(deltaTime);
                currentSE->IncrementFrameCount();
            }
            g_selectedEffect->Update(currentTime);
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        if (g_showGui) {
            if (g_snapWindowsNextFrame) { /* ... snapping ... */ g_snapWindowsNextFrame = false; }

            ImGui::Begin("Effect Properties");
            ImGui::Text("RaymarchVibe");
            if(ImGui::Button("Show Help")) g_showHelpWindow = true;
            ImGui::SameLine();
            if(ImGui::Button("Snap Windows")) g_snapWindowsNextFrame = true;
            ImGui::Separator();
            if(g_selectedEffect) g_selectedEffect->RenderUI();
            else ImGui::Text("No effect selected.");
            ImGui::Separator(); ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::End();

            ImGui::Begin("Shader Editor");
            ShaderEffect* editorSE = dynamic_cast<ShaderEffect*>(g_selectedEffect);
            int currentF5State_Editor = glfwGetKey(window, GLFW_KEY_F5);
            if (currentF5State_Editor == GLFW_PRESS && f5State == GLFW_RELEASE && !io.WantTextInput && editorSE) {
                editorSE->ApplyShaderCode(g_editor.GetText());
                const std::string& log = editorSE->GetCompileErrorLog();
                if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) g_editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                g_shaderLoadError_global = "Applied (F5). Status: " + log;
            }
            f5State = currentF5State_Editor; // Update global F5 state tracker

            if (ImGui::CollapsingHeader("Load/Save Actions")) {
                 // Shadertoy Load
                ImGui::InputTextWithHint("##STInput", "Shadertoy ID/URL", shadertoyInputBuffer, sizeof(shadertoyInputBuffer)); ImGui::SameLine();
                if (ImGui::Button("Fetch & Apply##STApply") && editorSE) {
                    std::string id = ShaderParser::ExtractShaderId(shadertoyInputBuffer); // Use ShaderParser's static method
                    if (!id.empty()) {
                        std::string fetchErr, code = FetchShadertoyCodeOnline(id, shadertoyApiKey, fetchErr);
                        if (!code.empty()) {
                            editorSE->LoadShaderFromSource(code);
                            editorSE->SetShadertoyMode(true);
                            editorSE->Load(); // This is ApplyShaderCode essentially
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
                // Sample Load
                if (ImGui::BeginCombo("##SampleCombo", shaderSamples[g_currentSampleIndex].name)) { /* ... */ ImGui::EndCombo(); } ImGui::SameLine();
                if (ImGui::Button("Load & Apply Sample##SampleApply") && editorSE && g_currentSampleIndex > 0) { /* ... */ }
                // File Load
                ImGui::InputText("Path##LoadFile", filePathBuffer_Load, sizeof(filePathBuffer_Load)); ImGui::SameLine();
                if (ImGui::Button("Load & Apply##FileApply") && editorSE) { /* ... */ }
                // New Shader
                if (ImGui::Button("New Native") && editorSE) { /* ... */ } ImGui::SameLine();
                if (ImGui::Button("New Shadertoy") && editorSE) { /* ... */ }
                // Save
                ImGui::Text("Current: %s", editorSE ? editorSE->GetShaderFilePath().c_str() : "N/A");
                if (ImGui::Button("Save Current") && editorSE) { /* ... */ }
                ImGui::InputText("Save As Path", filePathBuffer_SaveAs, sizeof(filePathBuffer_SaveAs)); ImGui::SameLine();
                if (ImGui::Button("Save As...") && editorSE) { /* ... */ }
            }
            // Status & Editor
            const std::string& currentEffectLog = editorSE ? editorSE->GetCompileErrorLog() : "";
            if (!g_shaderLoadError_global.empty() && (currentEffectLog.empty() || currentEffectLog.find("Successfully") != std::string::npos || currentEffectLog.find("applied successfully") != std::string::npos))
                 ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "Status: %s", g_shaderLoadError_global.c_str());
            else if (!currentEffectLog.empty())
                 ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "Effect Status: %s", currentEffectLog.c_str());
            ImGui::Separator();
            g_editor.Render("ShaderSourceEditor", ImVec2(-1, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()*1.2f));
            if (ImGui::Button("Apply from Editor") && editorSE) {
                editorSE->ApplyShaderCode(g_editor.GetText());
                const std::string& log = editorSE->GetCompileErrorLog();
                if (!log.empty() && log.find("Successfully") == std::string::npos && log.find("applied successfully") == std::string::npos) g_editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                g_shaderLoadError_global = "Applied (Button). Status: " + log;
            }
            ImGui::End();

            ImGui::Begin("Console");
            std::string fullLog = g_shaderLoadError_global;
            if (editorSE && !editorSE->GetCompileErrorLog().empty()){
                if(!fullLog.empty() && fullLog.back()!='\n' && (fullLog.find("Status: ") != 0 || editorSE->GetCompileErrorLog().find(g_shaderLoadError_global.substr(8)) == std::string::npos ) ) fullLog += "\n";
                if (fullLog.find(editorSE->GetCompileErrorLog()) == std::string::npos) {
                    fullLog += (fullLog.empty() ? "" : "Effect Log: ") + editorSE->GetCompileErrorLog();
                }
            }
            if (!fullLog.empty()) ImGui::TextWrapped("%s", fullLog.c_str()); else ImGui::TextDisabled("[Log is empty]");
            if (ImGui::Button("Clear Log")) { g_shaderLoadError_global.clear(); ClearErrorMarkers(); }
            ImGui::End();

            if (g_showHelpWindow) { ImGui::Begin("Help", &g_showHelpWindow); ImGui::Text("Help content."); ImGui::End(); }
        } 

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0,0,display_w,display_h);
        glClearColor(0.02f, 0.02f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);

        bool effectWasRendered = false;
        for (const auto& effect_ptr : g_scene) {
            if (currentTime >= effect_ptr->startTime && currentTime < effect_ptr->endTime) {
                effect_ptr->Render();
                effectWasRendered = true;
                break;
            }
        }
        if(effectWasRendered){ // Assuming direct rendering for now, this quad is part of the effect's render pass
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    } 

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO);
    glfwTerminate(); return 0;
} 

void framebuffer_size_callback(GLFWwindow* w, int width, int height) {
    (void)w; glViewport(0,0,width,height);
    if(g_selectedEffect) {
        ShaderEffect* se = dynamic_cast<ShaderEffect*>(g_selectedEffect);
        if(se) se->SetDisplayResolution(width,height);
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
        if(se) se->SetMouseState(g_mouseState[0],g_mouseState[1],g_mouseState[2],g_mouseState[3]);
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
    if(se) se->SetMouseState(g_mouseState[0],g_mouseState[1],g_mouseState[2],g_mouseState[3]);
    if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT)==GLFW_RELEASE) {
        if(g_mouseState[2]>0.f) g_mouseState[2]=-g_mouseState[2];
        if(g_mouseState[3]>0.f) g_mouseState[3]=-g_mouseState[3];
        if(se) se->SetMouseState(g_mouseState[0],g_mouseState[1],g_mouseState[2],g_mouseState[3]);
    }
}
