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
// ShaderParameterControls.h and ShaderParser.h are included by ShaderEffect.h

// Window dimensions
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window); 

// Global instance of our main shader effect (for Phase 1)
// In Phase 2, this would become a list of effects.
std::unique_ptr<ShaderEffect> g_shaderEffect;
// Global mouse state, to be passed to ShaderEffect
static float g_mouseState_iMouse[4] = {0.0f, 0.0f, 0.0f, 0.0f};


// Callbacks need to be able to access g_shaderEffect and g_mouseState_iMouse
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);


// Helper to load shader source (used for passthrough.vert and initial fragment shader)
// This version is kept as it's simple and used by ShaderEffect internally for VS too.
// ShaderEffect has its own LoadShaderSourceFile, but this can be a general utility.
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
    
// Forward declaration for error parsing (kept global for TextEditor interaction)
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log);
// Helper to clear markers (kept global for TextEditor interaction)
static TextEditor editor; // Instance of the text editor
void ClearErrorMarkers() {
    TextEditor::ErrorMarkers emptyMarkers;
    editor.SetErrorMarkers(emptyMarkers);
}


// --- Global/Static Variables for UI State (not shader parameters) ---
static bool showGui = true; 
static bool snapWindowsNextFrame = false; 
static bool showHelpWindow = false; 
static std::string shaderLoadError_global = ""; // For status messages not directly tied to shader compilation log in ShaderEffect
static bool isFullscreen = false;
static int storedWindowX = 100, storedWindowY = 100; 
static int storedWindowWidth = SCR_WIDTH, storedWindowHeight = SCR_HEIGHT; 


struct ShaderSample {
    const char* name;
    const char* filePath;
};

// Shader samples list remains global for the UI
static const std::vector<ShaderSample> shaderSamples = {
    {"--- Select a Sample ---", ""},
    {"Native Shader Template 1", "shaders/raymarch_v1.frag"},
    {"Native Shader Template 2", "shaders/raymarch_v2.frag"},
    {"Simple Red", "shaders/samples/simple_red.frag"},
    {"UV Pattern", "shaders/samples/uv_pattern.frag"},
    {"Shadertoy Sampler1", "shaders/samples/tester_cube.frag"},
    {"Fractal Sampler1", "shaders/samples/fractal1.frag"},
    {"Fractal Sampler2", "shaders/samples/fractal2.frag"},
    {"Fractal Sampler3", "shaders/samples/fractal3.frag"},
};
static size_t currentSampleIndex = 0; 


// Shader templates remain global as they are used to populate editor for "New Shader"
const char* nativeShaderTemplate = R"GLSL(
// RaymarchVibe - Native Dodecahedron Template
#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

// Native Uniforms
uniform vec3 u_objectColor = vec3(0.7, 0.5, 0.9); 
uniform float u_scale = 0.8;        
uniform float u_timeSpeed = 0.3;    
uniform vec3 u_colorMod = vec3(0.2, 0.3, 0.4); 
uniform float u_patternScale = 5.0; 

// Camera Uniforms
uniform vec3 u_camPos = vec3(0.0, 0.8, -2.5);
uniform vec3 u_camTarget = vec3(0.0, 0.0, 0.0);
uniform float u_camFOV = 60.0;

// Light Uniforms
uniform vec3 u_lightPosition = vec3(3.0, 2.0, -4.0);
uniform vec3 u_lightColor = vec3(1.0, 0.95, 0.9);

// Constants
const float PI = 3.14159265359;
const float PHI = 1.618033988749895; // Golden ratio

// Helper Functions
float radians(float degrees) {
    return degrees * PI / 180.0;
}

mat3 rotateX(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        1.0, 0.0, 0.0,
        0.0, c,  -s,
        0.0, s,   c
    );
}

mat3 rotateY(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c,  0.0, s,
        0.0,1.0, 0.0,
        -s, 0.0, c
    );
}

mat3 rotateZ(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c,  -s, 0.0,
        s,   c, 0.0,
        0.0,0.0,1.0
    );
}

// SDF for a Regular Dodecahedron
float sdDodecahedron(vec3 p, float r_inradius) {
    p = abs(p); 
    vec3 n1 = vec3(1.0, PHI, 0.0);
    vec3 n2 = vec3(0.0, 1.0, PHI);
    vec3 n3 = vec3(PHI, 0.0, 1.0);
    float d = dot(p, normalize(n1));
    d = max(d, dot(p, normalize(n2)));
    d = max(d, dot(p, normalize(n3)));
    return d - r_inradius;
}

// Scene Definition
float mapScene(vec3 p, float time) {
    float effectiveTime = time * u_timeSpeed;
    float dodecaInradius = 0.5 * u_scale; 
    vec3 objectOffset = vec3(0.0, dodecaInradius * 0.8, 0.0); 
    vec3 p_transformed = p - objectOffset;
    mat3 rotMat = rotateY(effectiveTime) * rotateX(effectiveTime * 0.7) * rotateZ(effectiveTime * 0.4);
    p_transformed = rotMat * p_transformed;
    float dodecaDist = sdDodecahedron(p_transformed, dodecaInradius);
    return dodecaDist;
}

// Normal Calculation
vec3 getNormal(vec3 p, float time) {
    float eps = 0.0005 * u_scale; 
    vec2 e = vec2(eps, 0.0);
    return normalize(vec3(
        mapScene(p + e.xyy, time) - mapScene(p - e.xyy, time),
        mapScene(p + e.yxy, time) - mapScene(p - e.yxy, time),
        mapScene(p + e.yyx, time) - mapScene(p - e.yyx, time)
    ));
}

// Raymarching
float rayMarch(vec3 ro, vec3 rd, float time) {
    float t = 0.0;
    for (int i = 0; i < 96; i++) { 
        vec3 p = ro + rd * t;
        float dist = mapScene(p, time);
        if (dist < (0.001 * t) || dist < 0.0001) { 
             return t;
        }
        t += dist * 0.75; 
        if (t > 30.0) { 
            break;
        }
    }
    return -1.0; 
}

// Camera Setup
mat3 setCamera(vec3 ro, vec3 ta, vec3 worldUp) {
    vec3 f = normalize(ta - ro);
    vec3 r = normalize(cross(f, worldUp));
    if (length(r) < 0.0001) { r = normalize(cross(f, vec3(0.0,0.0,-1.0))); } 
    if (length(r) < 0.0001) { r = normalize(cross(f, vec3(1.0,0.0,0.0))); } 
    vec3 u = normalize(cross(r, f));
    return mat3(r, u, f);
}

// Main Shader Logic
void main() {
    vec2 p_ndc = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    vec3 ro = u_camPos;
    vec3 ta = u_camTarget;
    mat3 camToWorld = setCamera(ro, ta, vec3(0.0, 1.0, 0.0));
    float fovFactor = tan(radians(u_camFOV) * 0.5);
    vec3 rd_local = normalize(vec3(p_ndc.x * fovFactor, p_ndc.y * fovFactor, 1.0));
    vec3 rd = camToWorld * rd_local;
    float t = rayMarch(ro, rd, iTime);
    vec3 col = vec3(0.05, 0.02, 0.08); 

    if (t > -0.5) { 
        vec3 hitPos = ro + rd * t;
        vec3 normal = getNormal(hitPos, iTime);
        vec3 lightDir = normalize(u_lightPosition - hitPos);
        float diffuse = max(0.0, dot(normal, lightDir));
        float ao = 0.0;
        float aoStep = 0.01 * u_scale; 
        float aoTotalDist = 0.0;
        for(int j=0; j<4; j++){
            aoTotalDist += aoStep;
            float d_ao = mapScene(hitPos + normal * aoTotalDist * 0.5, iTime);
            ao += max(0.0, (aoTotalDist*0.5 - d_ao));
            aoStep *= 1.7;
        }
        ao = 1.0 - clamp(ao * (0.1 / (u_scale*u_scale + 0.01) ), 0.0, 1.0); 
        vec3 viewDir = normalize(ro - hitPos);
        vec3 reflectDir = reflect(-lightDir, normal);
        float specAngle = max(dot(viewDir, reflectDir), 0.0);
        float specular = pow(specAngle, 32.0 + u_patternScale * 10.0); 
        vec3 baseColour = u_objectColor;
        float patternVal = sin(hitPos.x * u_patternScale + iTime * u_timeSpeed * 0.5) *
                           cos(hitPos.y * u_patternScale - iTime * u_timeSpeed * 0.3) *
                           sin(hitPos.z * u_patternScale + iTime * u_timeSpeed * 0.7);
        baseColour += u_colorMod * (0.5 + 0.5 * patternVal);
        baseColour = clamp(baseColour, 0.0, 1.0);
        vec3 litColour = baseColour * (diffuse * 0.7 + 0.2 * ao) * u_lightColor; 
        litColour += u_lightColor * specular * 0.4 * (0.5 + 0.5 * baseColour.r); 
        col = litColour;
        float fogFactor = smoothstep(u_scale * 2.0, u_scale * 15.0, t); 
        col = mix(col, vec3(0.05, 0.02, 0.08), fogFactor);
    } else {
        col = vec3(0.1, 0.05, 0.15) + vec3(0.2, 0.1, 0.05) * pow(1.0 - abs(p_ndc.y), 3.0);
    }
    col = pow(col, vec3(1.0/2.2));
    FragColor = vec4(col, 1.0);
})GLSL";

const char* shadertoyShaderTemplate = R"GLSL(
// RaymarchVibe - Shadertoy Template
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy;
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));
    fragColor = vec4(col,1.0);
})GLSL";

// Basic GLSL Error Log Parser for TextEditor
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log) {
    TextEditor::ErrorMarkers markers; 
    std::stringstream ss(log);        
    std::string line;
    std::regex errorRegex1(R"(^(?:[A-Z]+:\s*)?\d+:(\d+):\s*(.*))");
    std::regex errorRegex2(R"(^\s*\d+\((\d+)\)\s*:\s*(.*))");      
    std::smatch match; 

        auto local_trim = [](const std::string& str_to_trim) -> std::string {
        const std::string whitespace = " \t\n\r\f\v";
        size_t start = str_to_trim.find_first_not_of(whitespace);
            if (std::string::npos == start) return ""; // Empty string literal implicitly converts to std::string
        size_t end = str_to_trim.find_last_not_of(whitespace);
        return str_to_trim.substr(start, end - start + 1);
    };

    while (std::getline(ss, line)) { 
        bool matched = false;
        if (std::regex_search(line, match, errorRegex1)) {
            if (match.size() >= 3) { 
                try {
                    int lineNumber = std::stoi(match[1].str()); 
                    std::string errorMessage = local_trim(match[2].str());
                    if (lineNumber > 0) { markers[lineNumber] = errorMessage; }
                    matched = true;
                } catch (const std::exception&) { /* ignore parse error */ }
            }
        }
        if (!matched && std::regex_search(line, match, errorRegex2)) {
             if (match.size() >= 3) { 
                try {
                    int lineNumber = std::stoi(match[1].str()); 
                    std::string errorMessage = local_trim(match[2].str());
                    if (lineNumber > 0) { markers[lineNumber] = errorMessage; }
                } catch (const std::exception&) { /* ignore parse error */ }
            }
        }
    }
    return markers; 
}

static std::string ExtractLocalShaderId(const std::string& idOrUrl) {
    std::string id = idOrUrl;
    size_t lastSlash = id.find_last_of('/');
    if (lastSlash != std::string::npos) { id = id.substr(lastSlash + 1); }
    size_t questionMark = id.find('?');
    if (questionMark != std::string::npos) { id = id.substr(0, questionMark); }
    if (id.length() == 6 && std::all_of(id.begin(), id.end(), ::isalnum)) { return id; }
    return ""; 
}

std::string FetchShadertoyCodeOnline(const std::string& shaderId, const std::string& apiKey, std::string& errorMsg) {
    errorMsg.clear();
    std::string shaderCode;
    try {
        #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            httplib::SSLClient cli("www.shadertoy.com"); 
        #else
            httplib::Client cli("www.shadertoy.com", 80); 
            errorMsg = "Warning: Attempting HTTP. HTTPS is required for Shadertoy API. ";
        #endif
        std::string path = "/api/v1/shaders/" + shaderId + "?key=" + apiKey;
        auto res = cli.Get(path.c_str());
        if (res) {
            if (res->status == 200) {
                json j = json::parse(res->body);
                if (j.contains("Shader") && j["Shader"].is_object() &&
                    j["Shader"].contains("renderpass") && j["Shader"]["renderpass"].is_array() &&
                    !j["Shader"]["renderpass"].empty() &&
                    j["Shader"]["renderpass"][0].is_object() &&
                    j["Shader"]["renderpass"][0].contains("code") && 
                    j["Shader"]["renderpass"][0]["code"].is_string()) {
                    shaderCode = j["Shader"]["renderpass"][0]["code"].get<std::string>();
                } else { /* ... error handling ... */ }
            } else { /* ... error handling ... */ }
        } else {
             auto err = res.error();
             errorMsg += "HTTP request failed. Error: " + httplib::to_string(err);
        }
    } catch (const std::exception& e) { errorMsg += "Exception: " + std::string(e.what()); }
    return shaderCode;
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
    static std::string shadertoyApiKey = "REPLACE_WITH_YOUR_KEY";

    float deltaTime = 0.0f;
    float lastFrameTime_main = 0.0f;

    g_shaderEffect = std::make_unique<ShaderEffect>();
    if (g_shaderEffect->LoadShaderFromFile(filePathBuffer_Load)) {
        g_shaderEffect->Load();
    } else {
        shaderLoadError_global = "Initial load failed: " + std::string(filePathBuffer_Load) + ". Log: " + g_shaderEffect->GetCompileErrorLog();
        g_shaderEffect->LoadShaderFromSource(nativeShaderTemplate); // Fallback
        g_shaderEffect->Load();
    }
    editor.SetText(g_shaderEffect->GetShaderSource());
    const std::string& compileLog = g_shaderEffect->GetCompileErrorLog();
    if (!compileLog.empty() && compileLog.find("Successfully") == std::string::npos) {
        editor.SetErrorMarkers(ParseGlslErrorLog(compileLog));
    } else { ClearErrorMarkers(); }


    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io; 
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true); 
    ImGui_ImplOpenGL3_Init("#version 330");
    
    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    editor.SetShowWhitespaces(false); editor.SetTabSize(4);

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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        int currentSpacebarState = glfwGetKey(window, GLFW_KEY_SPACE);
        if (currentSpacebarState == GLFW_PRESS && spacebarState == GLFW_RELEASE && !io.WantTextInput) showGui = !showGui;
        spacebarState = currentSpacebarState;

        int currentF12State = glfwGetKey(window, GLFW_KEY_F12);
        if (currentF12State == GLFW_PRESS && f12State == GLFW_RELEASE && !io.WantTextInput) {
            isFullscreen = !isFullscreen;
            if (isFullscreen) { /* ... set fullscreen ... */ } else { /* ... restore windowed ... */ }
        }
        f12State = currentF12State;

        int currentF5State = glfwGetKey(window, GLFW_KEY_F5);
        if (currentF5State == GLFW_PRESS && f5State == GLFW_RELEASE && !io.WantTextInput && g_shaderEffect) {
            g_shaderEffect->ApplyShaderCode(editor.GetText());
            const std::string& log = g_shaderEffect->GetCompileErrorLog();
            if (!log.empty() && log.find("Successfully") == std::string::npos) editor.SetErrorMarkers(ParseGlslErrorLog(log));
            else ClearErrorMarkers();
            shaderLoadError_global = "Applied from Editor (F5). Status: " + log;
        }
        f5State = currentF5State;
        
        processInput(window); 

        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastFrameTime_main;
        lastFrameTime_main = currentTime;

        if (g_shaderEffect) {
            g_shaderEffect->IncrementFrameCount();
            g_shaderEffect->SetDeltaTime(deltaTime);
            g_shaderEffect->Update(currentTime);
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        if (showGui) {
            if (snapWindowsNextFrame) { /* ... snapping logic ... */ snapWindowsNextFrame = false; }

            ImGui::Begin("Status");
            ImGui::Text("RaymarchVibe Refactored!");
            ImGui::TextWrapped("F12: Fullscreen | Spacebar: Toggle GUI | F5: Apply Editor Code | ESC: Close");
            if(ImGui::Button("Show Help")) showHelpWindow = true;
            ImGui::Separator();
            if (g_shaderEffect) {
                ImGui::Text("Effect: %s", g_shaderEffect->name.c_str());
                ImGui::Text("Path: %s", g_shaderEffect->GetShaderFilePath().c_str());
                g_shaderEffect->RenderUI();
            } else ImGui::Text("No shader effect loaded.");
            ImGui::Separator(); ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::End();

            ImGui::Begin("Shader Editor");
            // --- UI for Load from Shadertoy ---
            if (ImGui::CollapsingHeader("Load from Shadertoy" )) {
                ImGui::InputTextWithHint("##STInput", "Shadertoy ID/URL", shadertoyInputBuffer, sizeof(shadertoyInputBuffer)); ImGui::SameLine();
                if (ImGui::Button("Fetch & Apply##STApply") && g_shaderEffect) {
                    std::string id = ExtractLocalShaderId(shadertoyInputBuffer);
                    if (!id.empty()) {
                        std::string fetchErr, code = FetchShadertoyCodeOnline(id, shadertoyApiKey, fetchErr);
                        if (!code.empty()) {
                            g_shaderEffect->LoadShaderFromSource(code); g_shaderEffect->SetShadertoyMode(true); g_shaderEffect->Load();
                            editor.SetText(g_shaderEffect->GetShaderSource());
                            // Update path buffers for UI consistency
                            std::string stPath = "Shadertoy_" + id + ".frag";
                            strncpy(filePathBuffer_Load, stPath.c_str(), sizeof(filePathBuffer_Load)-1); filePathBuffer_Load[sizeof(filePathBuffer_Load)-1]=0;
                            strncpy(filePathBuffer_SaveAs, stPath.c_str(), sizeof(filePathBuffer_SaveAs)-1); filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs)-1]=0;

                            const std::string& log = g_shaderEffect->GetCompileErrorLog();
                            if (!log.empty() && log.find("Successfully") == std::string::npos) editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                            shaderLoadError_global = "Fetched Shadertoy '" + id + "'. Status: " + log;
                        } else shaderLoadError_global = "Fetch Error: " + fetchErr;
                    } else shaderLoadError_global = "Invalid Shadertoy ID.";
                }
            }
            // --- UI for Load Sample Shader ---
            if (ImGui::CollapsingHeader("Load Sample Shader" )) {
                if (ImGui::BeginCombo("##SampleCombo", shaderSamples[currentSampleIndex].name)) { /* ... combo items ... */ ImGui::EndCombo(); }
                ImGui::SameLine();
                if (ImGui::Button("Load & Apply Sample##SampleApply") && g_shaderEffect && currentSampleIndex > 0) {
                    std::string path = shaderSamples[currentSampleIndex].filePath;
                    if (g_shaderEffect->LoadShaderFromFile(path)) {
                        g_shaderEffect->Load(); editor.SetText(g_shaderEffect->GetShaderSource());
                        strncpy(filePathBuffer_Load, path.c_str(), sizeof(filePathBuffer_Load)-1); filePathBuffer_Load[sizeof(filePathBuffer_Load)-1]=0;
                        strncpy(filePathBuffer_SaveAs, path.c_str(), sizeof(filePathBuffer_SaveAs)-1); filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs)-1]=0;
                        const std::string& log = g_shaderEffect->GetCompileErrorLog();
                        if (!log.empty() && log.find("Successfully") == std::string::npos) editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                        shaderLoadError_global = "Loaded sample. Status: " + log;
                    } else shaderLoadError_global = "Failed to load sample file.";
                }
            }
            // --- UI for Load From File ---
            if (ImGui::CollapsingHeader("Load From File" )) {
                ImGui::InputText("Path##LoadFile", filePathBuffer_Load, sizeof(filePathBuffer_Load)); ImGui::SameLine();
                if (ImGui::Button("Load & Apply##FileApply") && g_shaderEffect) {
                    if (g_shaderEffect->LoadShaderFromFile(filePathBuffer_Load)) {
                        g_shaderEffect->Load(); editor.SetText(g_shaderEffect->GetShaderSource());
                        strncpy(filePathBuffer_SaveAs, filePathBuffer_Load, sizeof(filePathBuffer_SaveAs)-1); filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs)-1]=0;
                        const std::string& log = g_shaderEffect->GetCompileErrorLog();
                        if (!log.empty() && log.find("Successfully") == std::string::npos) editor.SetErrorMarkers(ParseGlslErrorLog(log)); else ClearErrorMarkers();
                        shaderLoadError_global = "Loaded file. Status: " + log;
                    } else shaderLoadError_global = "Failed to load file.";
                }
            }
            // --- UI for New Shader ---
            if (ImGui::CollapsingHeader("New Shader" )) {
                if (ImGui::Button("New Native") && g_shaderEffect) { /* ... load native template ... */ } ImGui::SameLine();
                if (ImGui::Button("New Shadertoy") && g_shaderEffect) { /* ... load shadertoy template ... */ }
            }
            // --- UI for Save Shader ---
            if (ImGui::CollapsingHeader("Save Shader" )) {
                ImGui::Text("Editing: %s", g_shaderEffect ? g_shaderEffect->GetShaderFilePath().c_str() : "N/A");
                if (ImGui::Button("Save Current") && g_shaderEffect) { /* ... save logic ... */ }
                ImGui::InputText("Save As Path", filePathBuffer_SaveAs, sizeof(filePathBuffer_SaveAs)); ImGui::SameLine();
                if (ImGui::Button("Save As...") && g_shaderEffect) { /* ... save as logic ... */ }
            }
            // --- Editor and its buttons ---
            if (!shaderLoadError_global.empty() && (!g_shaderEffect || g_shaderEffect->GetCompileErrorLog().empty() || g_shaderEffect->GetCompileErrorLog().find("Successfully") != std::string::npos)) {
                 ImGui::TextColored(ImVec4(1.f,1.f,0.f,1.f), "Status: %s", shaderLoadError_global.c_str());
            }
            ImGui::Separator(); ImGui::Text("Shader Code Editor"); ImGui::Dummy(ImVec2(0,5));
            editor.Render("ShaderSourceEditor", ImVec2(-1, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()*2.2f));
            if (ImGui::Button("Apply from Editor (F5)") && g_shaderEffect) { /* ... apply logic (same as F5 keybind) ... */ }
            // ImGui::SameLine(); if(ImGui::Button("Reset Parameters") && g_shaderEffect) { /* TODO: ShaderEffect::ResetParameters() */ }
            ImGui::End();

            ImGui::Begin("Console");
            // Display combined global status and effect log
            std::string fullLog = shaderLoadError_global;
            if (g_shaderEffect && !g_shaderEffect->GetCompileErrorLog().empty()){
                if(!fullLog.empty() && fullLog.back()!='\n') fullLog += "\n";
                fullLog += "Effect Log: " + g_shaderEffect->GetCompileErrorLog();
            }
            if (!fullLog.empty()) ImGui::TextWrapped("%s", fullLog.c_str()); else ImGui::TextDisabled("[Log is empty]");
            if (ImGui::Button("Clear Log")) { shaderLoadError_global.clear(); /* g_shaderEffect->ClearLog(); */ ClearErrorMarkers(); }
            ImGui::End();

            if (showHelpWindow) { /* ... help window ... */ }
        } 

        int dw, dh; glfwGetFramebufferSize(window, &dw, &dh); glViewport(0,0,dw,dh);
        glClearColor(0.02f, 0.02f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);

        if (g_shaderEffect) {
            g_shaderEffect->SetDisplayResolution(dw, dh);
            g_shaderEffect->Render();
            glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, 6);
        } 

        ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    } 

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glDeleteVertexArrays(1, &quadVAO); glDeleteBuffers(1, &quadVBO);
    glfwTerminate(); return 0;
} 

void framebuffer_size_callback(GLFWwindow* w, int width, int height) { (void)w; glViewport(0,0,width,height); if(g_shaderEffect) g_shaderEffect->SetDisplayResolution(width,height); }
void processInput(GLFWwindow *w) { if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(w, true); }

void mouse_cursor_position_callback(GLFWwindow* w, double x, double y) {
    ImGuiIO& io = ImGui::GetIO();
    bool stMode = g_shaderEffect ? g_shaderEffect->IsShadertoyMode() : false;
    if (!io.WantCaptureMouse || stMode) {
        int winH; glfwGetWindowSize(w, NULL, &winH);
        g_mouseState_iMouse[0]=(float)x; g_mouseState_iMouse[1]=(float)winH-(float)y;
        if(g_shaderEffect) g_shaderEffect->SetMouseState(g_mouseState_iMouse[0],g_mouseState_iMouse[1],g_mouseState_iMouse[2],g_mouseState_iMouse[3]);
    }
}
void mouse_button_callback(GLFWwindow* w, int btn, int act, int mod) {
    (void)mod; ImGuiIO& io = ImGui::GetIO();
    bool stMode = g_shaderEffect ? g_shaderEffect->IsShadertoyMode() : false;
    if (!io.WantCaptureMouse || stMode) {
        if (btn == GLFW_MOUSE_BUTTON_LEFT) {
            if (act == GLFW_PRESS) { g_mouseState_iMouse[2]=g_mouseState_iMouse[0]; g_mouseState_iMouse[3]=g_mouseState_iMouse[1]; }
            else if (act == GLFW_RELEASE) { g_mouseState_iMouse[2]=-std::abs(g_mouseState_iMouse[2]); g_mouseState_iMouse[3]=-std::abs(g_mouseState_iMouse[3]); }
        }
    }
    if(g_shaderEffect) g_shaderEffect->SetMouseState(g_mouseState_iMouse[0],g_mouseState_iMouse[1],g_mouseState_iMouse[2],g_mouseState_iMouse[3]);
    if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT)==GLFW_RELEASE) { // Ensure negative z/w if mouse up
        if(g_mouseState_iMouse[2]>0.f) g_mouseState_iMouse[2]=-g_mouseState_iMouse[2];
        if(g_mouseState_iMouse[3]>0.f) g_mouseState_iMouse[3]=-g_mouseState_iMouse[3];
        if(g_shaderEffect) g_shaderEffect->SetMouseState(g_mouseState_iMouse[0],g_mouseState_iMouse[1],g_mouseState_iMouse[2],g_mouseState_iMouse[3]);
    }
}
