// THIS IS B4 THE NEXT PHASE...
// Phase 1: Enhancing Core Functionality & User Experience

#include <glad/glad.h> // Must be included before GLFW
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath> // For abs

// --- ImGui Includes ---
#include "imgui.h"              // Main ImGui header
#include "imgui_impl_glfw.h"    // ImGui Platform Backend for GLFW
#include "imgui_impl_opengl3.h" // ImGui Renderer Backend for OpenGL3

// Window dimensions
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
std::string loadShaderSource(const char* filePath);
GLuint compileShader(const char* source, GLenum type, std::string& errorLogString);
GLuint createShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString);

// Global/static state for mouse (for Shadertoy iMouse)
static float mouseState_iMouse[4] = {0.0f, 0.0f, 0.0f, 0.0f};
// <<< --- PLACE shadertoyMode HERE --- >>>
// This makes shadertoyMode a file-static variable, accessible throughout this file.
static bool shadertoyMode = false;

// --- Shader Sample Structure and List ---
struct ShaderSample {
    const char* name;     // Display name in dropdown
    const char* filePath; // Path to the .frag file
};

// List of available shader samples
// IMPORTANT: Make sure these files exist in the specified paths relative to your executable
// For example, create a "shaders/samples/" directory.
static const std::vector<ShaderSample> shaderSamples = {
    {"--- Select a Sample ---", ""}, // Placeholder, does nothing
    {"Simple Red", "shaders/samples/simple_red.frag"},
    {"UV Pattern", "shaders/samples/uv_pattern.frag"},
    // Add more samples here:
    // {"My Cool Shader", "shaders/samples/my_cool_shader.frag"}
};
static int currentSampleIndex = 0; // To keep track of the selected sample in the dropdown


// --- Main Application ---
int main() {
    // 1. Initialize GLFW
    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // 2. Create a GLFW window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe - Native Uniforms!", NULL, NULL);
    if (window == NULL) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // 3. Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD" << std::endl; glfwTerminate(); return -1; }

    // State Variables for Shader Editing and UI (these are local to main)
    static char shaderCodeBuffer[16384] = "";
    static std::string shaderLoadError = "";
    static std::string currentShaderPath = "shaders/raymarch_v1.frag";
    static std::string shaderCompileErrorLog = "";
    static char filePathBuffer_Load[256] = "shaders/raymarch_v1.frag";
    static char filePathBuffer_SaveAs[256] = "shaders/my_new_shader.frag";
    static float objectColor[3] = {0.8f, 0.9f, 1.0f};
    static float scale = 1.0f;
    static float timeSpeed = 1.0f;
    static float colorMod[3] = {0.0f, 0.0f, 0.0f};
    static float patternScale = 1.0f;
    static float cameraPosition[3] = {0.0f, 1.0f, -3.0f};
    static float cameraTarget[3] = {0.0f, 0.0f, 0.0f};
    static float cameraFOV = 60.0f;
    static float lightPosition[3] = {2.0f, 3.0f, -2.0f};
    static float deltaTime = 0.0f;
    static float lastFrameTime_main = 0.0f;
    static int frameCount = 0;
    // Note: shadertoyMode is now declared globally static above main()

    // OpenGL state variables
    GLuint shaderProgram = 0;
    GLint iResolutionLocation = -1;
    GLint iTimeLocation = -1;
    GLint u_objectColorLocation = -1;
    GLint u_scaleLocation = -1;
    GLint u_timeSpeedLocation = -1;
    GLint u_colorModLocation = -1;
    GLint u_patternScaleLocation = -1;
    GLint u_camPosLocation = -1;
    GLint u_camTargetLocation = -1;
    GLint u_camFOVLocation = -1;
    GLint u_lightPositionLocation = -1;
    GLint iTimeDeltaLocation = -1;
    GLint iFrameLocation = -1;
    GLint iMouseLocation = -1;

    // --- Setup Dear ImGui context ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    const char* glsl_version = "#version 330";
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- Quad Vertices, VAO/VBO ---
    float quadVertices[] = { -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f };
    GLuint quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);

    // --- Initial Shader Load ---
    std::string vertexShaderSource = loadShaderSource("shaders/passthrough.vert");
    std::string fragmentShaderSource_initial = loadShaderSource(currentShaderPath.c_str());

    if (vertexShaderSource.empty() || fragmentShaderSource_initial.empty()) {
        shaderCompileErrorLog = "Failed to load initial shader files!\n";
        if (vertexShaderSource.empty()) shaderCompileErrorLog += "Vertex shader (shaders/passthrough.vert) missing or unreadable.\n";
        if (fragmentShaderSource_initial.empty()) shaderCompileErrorLog += "Fragment shader (" + currentShaderPath + ") missing or unreadable.\n";
        std::cerr << shaderCompileErrorLog << std::endl;
    }

    std::string tempErrorLog;
    GLuint vertexShader_init = compileShader(vertexShaderSource.c_str(), GL_VERTEX_SHADER, tempErrorLog);
    if (!tempErrorLog.empty()) shaderCompileErrorLog += "Initial Vertex Shader Load: " + tempErrorLog + "\n";

    GLuint fragmentShader_init = compileShader(fragmentShaderSource_initial.c_str(), GL_FRAGMENT_SHADER, tempErrorLog);
    if (!tempErrorLog.empty()) shaderCompileErrorLog += "Initial Fragment Shader Load ("+currentShaderPath+"): " + tempErrorLog + "\n";

    if (vertexShader_init != 0 && fragmentShader_init != 0) {
        shaderProgram = createShaderProgram(vertexShader_init, fragmentShader_init, tempErrorLog);
        if (!tempErrorLog.empty()) shaderCompileErrorLog += "Initial Shader Link: " + tempErrorLog + "\n";
    } else {
        shaderCompileErrorLog += "Initial shader link skipped due to compilation errors.\n";
        if (vertexShader_init != 0) glDeleteShader(vertexShader_init);
        if (fragmentShader_init != 0) glDeleteShader(fragmentShader_init);
    }

    if (shaderProgram == 0) {
        std::cerr << "Initial shader program creation failed! Check log." << std::endl;
    }

    if (shaderProgram != 0) {
        if (shadertoyMode) {
            iResolutionLocation = glGetUniformLocation(shaderProgram, "iResolution");
            iTimeLocation = glGetUniformLocation(shaderProgram, "iTime");
            iTimeDeltaLocation = glGetUniformLocation(shaderProgram, "iTimeDelta");
            iFrameLocation = glGetUniformLocation(shaderProgram, "iFrame");
            iMouseLocation = glGetUniformLocation(shaderProgram, "iMouse");
            if (iResolutionLocation == -1) { shaderCompileErrorLog += "ST_Warn: iResolution (vec3) not found.\n"; }
            if (iTimeLocation == -1) { shaderCompileErrorLog += "ST_Warn: iTime not found.\n"; }
            if (iTimeDeltaLocation == -1) { shaderCompileErrorLog += "ST_Warn: iTimeDelta not found.\n"; }
            if (iFrameLocation == -1) { shaderCompileErrorLog += "ST_Warn: iFrame not found.\n"; }
            if (iMouseLocation == -1) { shaderCompileErrorLog += "ST_Warn: iMouse not found.\n"; }
        } else {
            iResolutionLocation = glGetUniformLocation(shaderProgram, "iResolution");
            iTimeLocation = glGetUniformLocation(shaderProgram, "iTime");
            u_objectColorLocation = glGetUniformLocation(shaderProgram, "u_objectColor");
            u_scaleLocation = glGetUniformLocation(shaderProgram, "u_scale");
            u_timeSpeedLocation = glGetUniformLocation(shaderProgram, "u_timeSpeed");
            u_colorModLocation = glGetUniformLocation(shaderProgram, "u_colorMod");
            u_patternScaleLocation = glGetUniformLocation(shaderProgram, "u_patternScale");
            u_camPosLocation = glGetUniformLocation(shaderProgram, "u_camPos");
            u_camTargetLocation = glGetUniformLocation(shaderProgram, "u_camTarget");
            u_camFOVLocation = glGetUniformLocation(shaderProgram, "u_camFOV");
            u_lightPositionLocation = glGetUniformLocation(shaderProgram, "u_lightPosition");
            if (iResolutionLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'iResolution' not found.\n"; }
            if (iTimeLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'iTime' not found.\n"; }
            if (u_objectColorLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_objectColor' not found.\n"; }
            if (u_scaleLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_scale' not found.\n"; }
            if (u_timeSpeedLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_timeSpeed' not found.\n"; }
            if (u_colorModLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_colorMod' not found.\n"; }
            if (u_patternScaleLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_patternScale' not found.\n"; }
            if (u_camPosLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_camPos' not found.\n"; }
            if (u_camTargetLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_camTarget' not found.\n"; }
            if (u_camFOVLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_camFOV' not found.\n"; }
            if (u_lightPositionLocation == -1) { shaderCompileErrorLog += "Warning: Initial 'u_lightPosition' not found.\n"; }
        }
    }

    std::ifstream shaderFileStream_editorLoad(currentShaderPath);
    if (shaderFileStream_editorLoad) {
        std::stringstream ss_editorLoad;
        ss_editorLoad << shaderFileStream_editorLoad.rdbuf();
        strncpy(shaderCodeBuffer, ss_editorLoad.str().c_str(), sizeof(shaderCodeBuffer) - 1);
        shaderCodeBuffer[sizeof(shaderCodeBuffer) - 1] = 0;
        shaderFileStream_editorLoad.close();
    } else {
        shaderLoadError = "Failed to load initial shader '" + currentShaderPath + "' into editor buffer.";
        shaderCompileErrorLog += shaderLoadError + "\n";
        std::cerr << shaderLoadError << std::endl;
        const char* errorShaderFallback = "// Could not load shader.\nvoid main() { gl_FragColor = vec4(0.5, 0.0, 0.0, 1.0); }";
        strncpy(shaderCodeBuffer, errorShaderFallback, sizeof(shaderCodeBuffer) - 1);
        shaderCodeBuffer[sizeof(shaderCodeBuffer) - 1] = 0;
    }

    lastFrameTime_main = (float)glfwGetTime();

    // --- Render loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float currentTime_render = (float)glfwGetTime();
        deltaTime = currentTime_render - lastFrameTime_main;
        lastFrameTime_main = currentTime_render;
        frameCount++;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("Status");
            ImGui::Text("Current Shader: %s", currentShaderPath.c_str());
            ImGui::Checkbox("Shadertoy Mode", &shadertoyMode); // Uses the file-static shadertoyMode
            ImGui::Separator();
            if (!shadertoyMode) {
                ImGui::Text("Shader Parameters:");
                ImGui::ColorEdit3("Object Color", objectColor);
                ImGui::SliderFloat("Scale", &scale, 0.1f, 3.0f);
                ImGui::SliderFloat("Time Speed", &timeSpeed, 0.0f, 5.0f);
                ImGui::ColorEdit3("Color Modulation", colorMod);
                ImGui::SliderFloat("Pattern Scale", &patternScale, 0.1f, 10.0f);
                ImGui::Separator();
                ImGui::Text("Camera Controls:");
                ImGui::DragFloat3("Position", cameraPosition, 0.1f);
                ImGui::DragFloat3("Target", cameraTarget, 0.1f);
                ImGui::SliderFloat("FOV (degrees)", &cameraFOV, 15.0f, 120.0f);
                ImGui::Separator();
                ImGui::Text("Lighting Controls:");
                ImGui::DragFloat3("Light Position", lightPosition, 0.1f);
            }
            ImGui::Separator();
            ImGui::Text("FPS: %.1f", io.Framerate);
            ImGui::End();
        }

        ImGui::Begin("Shader Editor");
        // --- Samples Dropdown ---
if (ImGui::CollapsingHeader("Load Sample Shader", ImGuiTreeNodeFlags_DefaultOpen)) { // Optional: make it collapsible
    if (ImGui::BeginCombo("##SampleShaderCombo", shaderSamples[currentSampleIndex].name)) {
        for (int n = 0; n < shaderSamples.size(); n++) {
            const bool is_selected = (currentSampleIndex == n);
            if (ImGui::Selectable(shaderSamples[n].name, is_selected)) {
                if (n != currentSampleIndex && n > 0) { // n > 0 to skip placeholder if it's index 0
                    currentSampleIndex = n;
                    std::string selectedPath = shaderSamples[currentSampleIndex].filePath;
                    if (!selectedPath.empty()) {
                        shaderLoadError.clear(); // Clear previous load errors
                        std::string newShaderContent = loadShaderSource(selectedPath.c_str());
                        if (!newShaderContent.empty()) {
                            strncpy(shaderCodeBuffer, newShaderContent.c_str(), sizeof(shaderCodeBuffer) - 1);
                            shaderCodeBuffer[sizeof(shaderCodeBuffer) - 1] = 0; // Ensure null termination

                            currentShaderPath = selectedPath;
                            strncpy(filePathBuffer_Load, currentShaderPath.c_str(), sizeof(filePathBuffer_Load) - 1);
                            filePathBuffer_Load[sizeof(filePathBuffer_Load) - 1] = 0;
                            strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) - 1);
                            filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;

                            shaderCompileErrorLog = "Sample '" + std::string(shaderSamples[currentSampleIndex].name) + "' loaded into editor from '" + currentShaderPath + "'.\nPress 'Apply Shader from Editor' to compile and run.";
                        } else {
                            shaderCompileErrorLog = "ERROR: Failed to load sample '" + std::string(shaderSamples[currentSampleIndex].name) + "' from: " + selectedPath;
                        }
                    }
                } else if (n == 0) { // If placeholder is selected
                    currentSampleIndex = n; // Select it but do nothing else
                }
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator(); // Add a separator after the dropdown section
} // End of CollapsingHeader for samples
ImGui::Spacing(); // Add a bit of space
        ImGui::InputText("Load Path", filePathBuffer_Load, sizeof(filePathBuffer_Load));
        ImGui::SameLine();
        if (ImGui::Button("Load from Path")) {
            shaderLoadError.clear();
            std::string newShaderContent = loadShaderSource(filePathBuffer_Load);
            if (!newShaderContent.empty()) {
                strncpy(shaderCodeBuffer, newShaderContent.c_str(), sizeof(shaderCodeBuffer) - 1);
                shaderCodeBuffer[sizeof(shaderCodeBuffer) - 1] = 0;
                currentShaderPath = filePathBuffer_Load;
                strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) -1);
                filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;
                shaderLoadError = "Loaded '" + currentShaderPath + "'. Press 'Apply' to compile.";
            } else {
                shaderLoadError = "Failed to load: " + std::string(filePathBuffer_Load);
            }
        }

        if (ImGui::Button("Reload & Apply Current")) {
            shaderLoadError.clear(); shaderCompileErrorLog.clear();
            std::string reloadedContent = loadShaderSource(currentShaderPath.c_str());
            if (!reloadedContent.empty()) {
                strncpy(shaderCodeBuffer, reloadedContent.c_str(), sizeof(shaderCodeBuffer) - 1);
                shaderCodeBuffer[sizeof(shaderCodeBuffer) - 1] = 0;
                shaderCompileErrorLog = "'" + currentShaderPath + "' reloaded. Applying...\n";

                std::string vsSource_reload = loadShaderSource("shaders/passthrough.vert");
                if (vsSource_reload.empty()) {
                    shaderCompileErrorLog += "CRITICAL: Vertex shader load failed for auto-apply.";
                } else {
                    std::string finalFragmentCode_reload = shaderCodeBuffer;
                    if (shadertoyMode) {
                        finalFragmentCode_reload = "#version 330 core\nout vec4 FragColor;\nuniform vec3 iResolution;\nuniform float iTime;\nuniform float iTimeDelta;\nuniform int iFrame;\nuniform vec4 iMouse;\n\n" + std::string(shaderCodeBuffer) + "\nvoid main() {\n    mainImage(FragColor, gl_FragCoord.xy);\n}\n";
                    }
                    std::string fsError_reload, vsError_reload, linkError_reload;
                    GLuint newFragmentShader_reload = compileShader(finalFragmentCode_reload.c_str(), GL_FRAGMENT_SHADER, fsError_reload);
                    if (newFragmentShader_reload != 0) {
                        GLuint tempVertexShader_reload = compileShader(vsSource_reload.c_str(), GL_VERTEX_SHADER, vsError_reload);
                        if (tempVertexShader_reload != 0) {
                            GLuint newShaderProgram_reload = createShaderProgram(tempVertexShader_reload, newFragmentShader_reload, linkError_reload);
                            if (newShaderProgram_reload != 0) {
                                if (shaderProgram != 0) { glDeleteProgram(shaderProgram); }
                                shaderProgram = newShaderProgram_reload;
                                std::string uniformWarnings_reload;
                                // (Uniform location fetching logic as before)
                                if (shadertoyMode) {
                                    iResolutionLocation = glGetUniformLocation(shaderProgram, "iResolution"); iTimeLocation = glGetUniformLocation(shaderProgram, "iTime"); iTimeDeltaLocation = glGetUniformLocation(shaderProgram, "iTimeDelta"); iFrameLocation = glGetUniformLocation(shaderProgram, "iFrame"); iMouseLocation = glGetUniformLocation(shaderProgram, "iMouse");
                                    if (iResolutionLocation == -1) uniformWarnings_reload += "ST_Warn: iResolution not found.\n"; if (iTimeLocation == -1) uniformWarnings_reload += "ST_Warn: iTime not found.\n"; if (iTimeDeltaLocation == -1) uniformWarnings_reload += "ST_Warn: iTimeDelta not found.\n"; if (iFrameLocation == -1) uniformWarnings_reload += "ST_Warn: iFrame not found.\n"; if (iMouseLocation == -1) uniformWarnings_reload += "ST_Warn: iMouse not found.\n";
                                } else {
                                    iResolutionLocation = glGetUniformLocation(shaderProgram, "iResolution"); iTimeLocation = glGetUniformLocation(shaderProgram, "iTime"); u_objectColorLocation = glGetUniformLocation(shaderProgram, "u_objectColor"); u_scaleLocation = glGetUniformLocation(shaderProgram, "u_scale"); u_timeSpeedLocation = glGetUniformLocation(shaderProgram, "u_timeSpeed"); u_colorModLocation = glGetUniformLocation(shaderProgram, "u_colorMod"); u_patternScaleLocation = glGetUniformLocation(shaderProgram, "u_patternScale"); u_camPosLocation = glGetUniformLocation(shaderProgram, "u_camPos"); u_camTargetLocation = glGetUniformLocation(shaderProgram, "u_camTarget"); u_camFOVLocation = glGetUniformLocation(shaderProgram, "u_camFOV"); u_lightPositionLocation = glGetUniformLocation(shaderProgram, "u_lightPosition");
                                    if (iResolutionLocation == -1) uniformWarnings_reload += "Warn: iResolution not found.\n"; if (iTimeLocation == -1) uniformWarnings_reload += "Warn: iTime not found.\n"; if (u_objectColorLocation == -1) uniformWarnings_reload += "Warn: u_objectColor not found.\n"; if (u_scaleLocation == -1) uniformWarnings_reload += "Warn: u_scale not found.\n"; if (u_timeSpeedLocation == -1) uniformWarnings_reload += "Warn: u_timeSpeed not found.\n"; if (u_colorModLocation == -1) uniformWarnings_reload += "Warn: u_colorMod not found.\n"; if (u_patternScaleLocation == -1) uniformWarnings_reload += "Warn: u_patternScale not found.\n"; if (u_camPosLocation == -1) uniformWarnings_reload += "Warn: u_camPos not found.\n"; if (u_camTargetLocation == -1) uniformWarnings_reload += "Warn: u_camTarget not found.\n"; if (u_camFOVLocation == -1) uniformWarnings_reload += "Warn: u_camFOV not found.\n"; if (u_lightPositionLocation == -1) uniformWarnings_reload += "Warn: u_lightPosition not found.\n";
                                }
                                shaderCompileErrorLog += uniformWarnings_reload.empty() ? "Reloaded shader applied!" : "Reloaded with warnings:\n" + uniformWarnings_reload;
                            } else { shaderCompileErrorLog += "Link failed for reload:\n" + linkError_reload; }
                        } else { shaderCompileErrorLog += "VS compile failed for reload:\n" + vsError_reload; if(newFragmentShader_reload !=0) glDeleteShader(newFragmentShader_reload); }
                    } else { shaderCompileErrorLog += "FS compile failed for reload:\n" + fsError_reload; }
                }
            } else {
                shaderLoadError = "Failed to reload: " + currentShaderPath;
                shaderCompileErrorLog = shaderLoadError;
            }
        }

        if (!shaderLoadError.empty()) { ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", shaderLoadError.c_str()); }

        ImGui::Text("Editing: %s", currentShaderPath.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Save Current")) {
            std::ofstream outFile(currentShaderPath);
            if (outFile.is_open()) {
                outFile << shaderCodeBuffer; outFile.close();
                shaderCompileErrorLog = "Saved to: " + currentShaderPath;
            } else { shaderCompileErrorLog = "ERROR saving to: " + currentShaderPath; }
        }
        ImGui::Separator();
        ImGui::InputText("Save As Path", filePathBuffer_SaveAs, sizeof(filePathBuffer_SaveAs));
        ImGui::SameLine();
        if (ImGui::Button("Save As...")) {
            std::string saveAsPathStr(filePathBuffer_SaveAs);
            if (saveAsPathStr.empty()) {
                shaderCompileErrorLog = "ERROR: 'Save As' path empty.";
            } else {
                std::ofstream outFile(saveAsPathStr);
                if (outFile.is_open()) {
                    outFile << shaderCodeBuffer; outFile.close();
                    shaderCompileErrorLog = "Saved to: " + saveAsPathStr;
                    currentShaderPath = saveAsPathStr;
                    strncpy(filePathBuffer_Load, currentShaderPath.c_str(), sizeof(filePathBuffer_Load) -1);
                    filePathBuffer_Load[sizeof(filePathBuffer_Load) -1] = 0;
                } else { shaderCompileErrorLog = "ERROR saving to: " + saveAsPathStr; }
            }
        }
        ImGui::Separator();

        float bottom_ui_height = ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y * 2;
        ImVec2 editorSize(-1.0f, ImGui::GetContentRegionAvail().y - bottom_ui_height);
        if (editorSize.y < ImGui::GetTextLineHeight() * 5) editorSize.y = ImGui::GetTextLineHeight() * 5;
        ImGui::InputTextMultiline("##ShaderSource", shaderCodeBuffer, sizeof(shaderCodeBuffer), editorSize, ImGuiInputTextFlags_AllowTabInput);

        if (ImGui::Button("Apply from Editor")) {
            shaderCompileErrorLog.clear();
            std::string vsSource_apply = loadShaderSource("shaders/passthrough.vert");
            if (vsSource_apply.empty()) {
                shaderCompileErrorLog = "CRITICAL: Vertex shader load failed for apply.";
            } else {
                std::string finalFragmentCode_apply = shaderCodeBuffer;
                if (shadertoyMode) {
                    finalFragmentCode_apply = "#version 330 core\nout vec4 FragColor;\nuniform vec3 iResolution;\nuniform float iTime;\nuniform float iTimeDelta;\nuniform int iFrame;\nuniform vec4 iMouse;\n\n" + std::string(shaderCodeBuffer) + "\nvoid main() {\n    mainImage(FragColor, gl_FragCoord.xy);\n}\n";
                }
                std::string fsError_apply, vsError_apply, linkError_apply;
                GLuint newFragmentShader_apply = compileShader(finalFragmentCode_apply.c_str(), GL_FRAGMENT_SHADER, fsError_apply);
                if (newFragmentShader_apply != 0) {
                    GLuint tempVertexShader_apply = compileShader(vsSource_apply.c_str(), GL_VERTEX_SHADER, vsError_apply);
                    if (tempVertexShader_apply != 0) {
                        GLuint newShaderProgram_apply = createShaderProgram(tempVertexShader_apply, newFragmentShader_apply, linkError_apply);
                        if (newShaderProgram_apply != 0) {
                            if (shaderProgram != 0) glDeleteProgram(shaderProgram);
                            shaderProgram = newShaderProgram_apply;
                            std::string uniformWarnings_apply;
                            // (Uniform location fetching logic as before)
                             if (shadertoyMode) {
                                iResolutionLocation = glGetUniformLocation(shaderProgram, "iResolution"); iTimeLocation = glGetUniformLocation(shaderProgram, "iTime"); iTimeDeltaLocation = glGetUniformLocation(shaderProgram, "iTimeDelta"); iFrameLocation = glGetUniformLocation(shaderProgram, "iFrame"); iMouseLocation = glGetUniformLocation(shaderProgram, "iMouse");
                                if (iResolutionLocation == -1) uniformWarnings_apply += "ST_Warn: iResolution not found.\n"; if (iTimeLocation == -1) uniformWarnings_apply += "ST_Warn: iTime not found.\n"; if (iTimeDeltaLocation == -1) uniformWarnings_apply += "ST_Warn: iTimeDelta not found.\n"; if (iFrameLocation == -1) uniformWarnings_apply += "ST_Warn: iFrame not found.\n"; if (iMouseLocation == -1) uniformWarnings_apply += "ST_Warn: iMouse not found.\n";
                            } else {
                                iResolutionLocation = glGetUniformLocation(shaderProgram, "iResolution"); iTimeLocation = glGetUniformLocation(shaderProgram, "iTime"); u_objectColorLocation = glGetUniformLocation(shaderProgram, "u_objectColor"); u_scaleLocation = glGetUniformLocation(shaderProgram, "u_scale"); u_timeSpeedLocation = glGetUniformLocation(shaderProgram, "u_timeSpeed"); u_colorModLocation = glGetUniformLocation(shaderProgram, "u_colorMod"); u_patternScaleLocation = glGetUniformLocation(shaderProgram, "u_patternScale"); u_camPosLocation = glGetUniformLocation(shaderProgram, "u_camPos"); u_camTargetLocation = glGetUniformLocation(shaderProgram, "u_camTarget"); u_camFOVLocation = glGetUniformLocation(shaderProgram, "u_camFOV"); u_lightPositionLocation = glGetUniformLocation(shaderProgram, "u_lightPosition");
                                 if (iResolutionLocation == -1) uniformWarnings_apply += "Warn: iResolution not found.\n"; if (iTimeLocation == -1) uniformWarnings_apply += "Warn: iTime not found.\n"; if (u_objectColorLocation == -1) uniformWarnings_apply += "Warn: u_objectColor not found.\n"; if (u_scaleLocation == -1) uniformWarnings_apply += "Warn: u_scale not found.\n"; if (u_timeSpeedLocation == -1) uniformWarnings_apply += "Warn: u_timeSpeed not found.\n"; if (u_colorModLocation == -1) uniformWarnings_apply += "Warn: u_colorMod not found.\n"; if (u_patternScaleLocation == -1) uniformWarnings_apply += "Warn: u_patternScale not found.\n"; if (u_camPosLocation == -1) uniformWarnings_apply += "Warn: u_camPos not found.\n"; if (u_camTargetLocation == -1) uniformWarnings_apply += "Warn: u_camTarget not found.\n"; if (u_camFOVLocation == -1) uniformWarnings_apply += "Warn: u_camFOV not found.\n"; if (u_lightPositionLocation == -1) uniformWarnings_apply += "Warn: u_lightPosition not found.\n";
                            }
                            shaderCompileErrorLog = uniformWarnings_apply.empty() ? "Applied from editor!" : "Applied with warnings:\n" + uniformWarnings_apply;
                        } else { shaderCompileErrorLog = "Link failed for apply:\n" + linkError_apply; }
                    } else { shaderCompileErrorLog = "VS compile failed for apply:\n" + vsError_apply; glDeleteShader(newFragmentShader_apply); }
                } else { shaderCompileErrorLog = "FS compile failed for apply:\n" + fsError_apply; }
            }
        }
        ImGui::End();

        ImGui::Begin("Console");
        if (!shaderCompileErrorLog.empty()) {
            if (shaderCompileErrorLog.find("ERROR") != std::string::npos || shaderCompileErrorLog.find("Failed") != std::string::npos || shaderCompileErrorLog.find("CRITICAL") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Log:");
            else if (shaderCompileErrorLog.find("Warning") != std::string::npos || shaderCompileErrorLog.find("Warn:") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Log:");
            else ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Log:");
            ImGui::SameLine(); ImGui::TextWrapped("%s", shaderCompileErrorLog.c_str());
        }
        if (ImGui::Button("Clear Log")) shaderCompileErrorLog.clear();
        ImGui::End();

        processInput(window);

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (shaderProgram != 0) {
            glUseProgram(shaderProgram);
            if (shadertoyMode) { // Uses file-static shadertoyMode
                if (iResolutionLocation != -1) glUniform3f(iResolutionLocation, (float)display_w, (float)display_h, (float)display_w / (float)display_h);
                if (iTimeLocation != -1) glUniform1f(iTimeLocation, (float)glfwGetTime());
                if (iTimeDeltaLocation != -1) glUniform1f(iTimeDeltaLocation, deltaTime);
                if (iFrameLocation != -1) glUniform1i(iFrameLocation, frameCount);
                if (iMouseLocation != -1) glUniform4fv(iMouseLocation, 1, mouseState_iMouse);
            } else {
                if (iResolutionLocation != -1) glUniform2f(iResolutionLocation, (float)display_w, (float)display_h);
                if (iTimeLocation != -1) glUniform1f(iTimeLocation, (float)glfwGetTime());
                if (u_objectColorLocation != -1) glUniform3fv(u_objectColorLocation, 1, objectColor);
                if (u_scaleLocation != -1) glUniform1f(u_scaleLocation, scale);
                if (u_timeSpeedLocation != -1) glUniform1f(u_timeSpeedLocation, timeSpeed);
                if (u_colorModLocation != -1) glUniform3fv(u_colorModLocation, 1, colorMod);
                if (u_patternScaleLocation != -1) glUniform1f(u_patternScaleLocation, patternScale);
                if (u_camPosLocation != -1) glUniform3fv(u_camPosLocation, 1, cameraPosition);
                if (u_camTargetLocation != -1) glUniform3fv(u_camTargetLocation, 1, cameraTarget);
                if (u_camFOVLocation != -1) glUniform1f(u_camFOVLocation, cameraFOV);
                if (u_lightPositionLocation != -1) glUniform3fv(u_lightPositionLocation, 1, lightPosition);
            }
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (shaderProgram != 0) glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse || shadertoyMode) { // shadertoyMode is now accessible
        int windowHeight;
        glfwGetWindowSize(window, NULL, &windowHeight);
        mouseState_iMouse[0] = (float)xpos;
        mouseState_iMouse[1] = (float)windowHeight - (float)ypos;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse || shadertoyMode) { // shadertoyMode is now accessible
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                double xpos_press, ypos_press;
                glfwGetCursorPos(window, &xpos_press, &ypos_press);
                int windowHeight;
                glfwGetWindowSize(window, NULL, &windowHeight);
                mouseState_iMouse[0] = (float)xpos_press;
                mouseState_iMouse[1] = (float)windowHeight - (float)ypos_press;
                mouseState_iMouse[2] = mouseState_iMouse[0];
                mouseState_iMouse[3] = mouseState_iMouse[1];
            } else if (action == GLFW_RELEASE) {
                if (mouseState_iMouse[2] > 0.0f) mouseState_iMouse[2] = -mouseState_iMouse[2];
                if (mouseState_iMouse[3] > 0.0f) mouseState_iMouse[3] = -mouseState_iMouse[3];
            }
        }
    }
}

std::string loadShaderSource(const char* filePath) {
    std::ifstream shaderFile;
    std::stringstream shaderStream;
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        shaderFile.open(filePath);
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
    } catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_READ: " << filePath << " - " << e.what() << std::endl;
        return "";
    }
    return shaderStream.str();
}

GLuint compileShader(const char* source, GLenum type, std::string& errorLogString) {
    errorLogString.clear();
    if (!source || std::string(source).empty()) {
        errorLogString = "ERROR::SHADER::COMPILE_EMPTY_SOURCE Type: " + std::to_string(type);
        std::cerr << errorLogString << std::endl; return 0;
    }
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 0 ? logLength + 1 : 257);
        if (logLength == 0) infoLog[0] = '\0';
        glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()-1), NULL, infoLog.data());
        errorLogString = "ERROR::SHADER::COMPILE_FAIL (" + std::string(type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") + ")\n" + infoLog.data();
        std::cerr << errorLogString << std::endl;
        glDeleteShader(shader); return 0;
    }
    return shader;
}

GLuint createShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString) {
    errorLogString.clear();
    if (vertexShaderID == 0 || fragmentShaderID == 0) {
        errorLogString = "ERROR::PROGRAM::LINK_INVALID_SHADER_ID";
        if (vertexShaderID != 0) glDeleteShader(vertexShaderID);
        if (fragmentShaderID != 0) glDeleteShader(fragmentShaderID);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShaderID);
    glAttachShader(program, fragmentShaderID);
    glLinkProgram(program);
    glDetachShader(program, vertexShaderID);
    glDetachShader(program, fragmentShaderID);
    int success; glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength; glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 0 ? logLength + 1 : 257);
        if (logLength == 0) infoLog[0] = '\0';
        glGetProgramInfoLog(program, static_cast<GLsizei>(infoLog.size()-1), NULL, infoLog.data());
        errorLogString = "ERROR::PROGRAM::LINK_FAIL\n" + std::string(infoLog.data());
        std::cerr << errorLogString << std::endl;
        glDeleteProgram(program);
        glDeleteShader(vertexShaderID);
        glDeleteShader(fragmentShaderID);
        return 0;
    }
    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);
    return program;
}
