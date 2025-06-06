// RaymarchVibe - Real-time Shader Exploration
// main.cpp

#include <glad/glad.h> 
#include <GLFW/glfw3.h>

#include <iostream> // For std::cerr
#include <string>   // For std::string
#include <vector>   // For std::vector (used by shaderSamples_main)
// #include <fstream> // Likely moved to UIManager
// #include <sstream> // Likely moved to UIManager
// #include <cmath>   // Likely moved to UIManager or not used directly
// #include <map>     // Likely moved to UIManager or not used directly
// #include <algorithm> // Likely moved to UIManager
// #include <cctype>  // Likely moved to UIManager
// #include <iomanip> // Likely moved to UIManager
// #include <regex>   // Likely moved to UIManager

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "TextEditor.h"

#include "AudioSystem.h"
#include "Renderer.h"
#include "ShaderParser.h"
#include "ShaderManager.h" 
#include "UIManager.h"

const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

void checkGLError(const char* stage) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL error at " << stage << " - Error Code: " << err;
        switch (err) {
            case GL_INVALID_ENUM: std::cerr << " (GL_INVALID_ENUM)"; break;
            case GL_INVALID_VALUE: std::cerr << " (GL_INVALID_VALUE)"; break;
            case GL_INVALID_OPERATION: std::cerr << " (GL_INVALID_OPERATION)"; break;
            case GL_OUT_OF_MEMORY: std::cerr << " (GL_OUT_OF_MEMORY)"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: std::cerr << " (GL_INVALID_FRAMEBUFFER_OPERATION)"; break;
            default: std::cerr << " (Unknown or Undefined Error Code)"; break; 
        }
        std::cerr << std::endl;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void window_close_callback(GLFWwindow* window);

static float mouseState_iMouse[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static bool shadertoyMode = false; 
static bool showGui = true;
static bool showHelpWindow = false; 
static bool showAudioReactivityWindow = false; 
static bool g_ShowConfirmExitPopup = false; 

static TextEditor editor; 
static std::string currentShaderCode_editor; 
static std::string shaderLoadError_ui = ""; 
static std::string currentShaderPath_ui = "shaders/raymarch_v1.frag"; 
static std::string shaderCompileErrorLog_ui = ""; 

static bool isFullscreen = false;
static int storedWindowX = 100, storedWindowY = 100;
static int storedWindowWidth = SCR_WIDTH, storedWindowHeight = SCR_HEIGHT;

static float objectColor_param[3] = {0.8f, 0.9f, 1.0f};
static float scale_param = 1.0f;
static float timeSpeed_param = 1.0f;
static float colorMod_param[3] = {0.1f, 0.1f, 0.2f};
static float patternScale_param = 1.0f;
static float cameraPosition_param[3] = {0.0f, 1.0f, -3.0f};
static float cameraTarget_param[3] = {0.0f, 0.0f, 0.0f};
static float cameraFOV_param = 60.0f;
static float lightPosition_param[3] = {2.0f, 3.0f, -2.0f};
static float lightColor_param[3] = {1.0f, 1.0f, 0.9f};
static float iUserFloat1_param = 0.5f;
static float iUserColor1_param[3] = {0.2f, 0.5f, 0.8f};

static const std::vector<ShaderSample_UI> shaderSamples_main = { 
    {"--- Select a Sample ---", ""}, {"Native Shader Template 1", "shaders/raymarch_v1.frag"},
    {"Native Shader Template 2", "shaders/raymarch_v2.frag"}, {"Simple Red", "shaders/samples/simple_red.frag"},
    {"UV Pattern", "shaders/samples/uv_pattern.frag"}, {"Shadertoy Sampler1", "shaders/samples/tester_cube.frag"},
    {"Fractal Sampler1", "shaders/samples/fractal1.frag"}, {"Fractal Sampler2", "shaders/samples/fractal2.frag"},
    {"Fractal Sampler3", "shaders/samples/fractal3.frag"},
};
static size_t currentSampleIndex = 0;

int main() {
    AudioSystem audioSystem; 
    audioSystem.Initialize(); 

    Renderer renderer; 
    ShaderParser shaderParser; 

    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe", NULL, NULL);
    if (window == NULL) { std::cerr << "Failed to create GLFW window" << std::endl; audioSystem.Shutdown(); glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);

    if (!renderer.Initialize(window)) { 
        std::cerr << "Failed to initialize Renderer" << std::endl;
        audioSystem.Shutdown();
        glfwTerminate();
        return -1;
    }
    checkGLError("after renderer.Initialize"); 

    ShaderManager shaderManager(renderer, shaderParser); 

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetWindowCloseCallback(window, window_close_callback);

    static char filePathBuffer_Load[256] = "shaders/raymarch_v1.frag";
    static char filePathBuffer_SaveAs[256] = "shaders/my_new_shader.frag";
    static char shadertoyInputBuffer[256] = "Ms2SD1";
    static std::string shadertoyApiKey = "fdHjR1"; 

    static float deltaTime = 0.0f;
    static float lastFrameTime_main = 0.0f;
    static int frameCount = 0;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    const char* glsl_version = "#version 330";
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    checkGLError("after ImGui Init");

    UIManager uiManager(
        shaderManager, shaderParser, audioSystem, editor,
        shadertoyMode, showGui,
        showHelpWindow, showAudioReactivityWindow, g_ShowConfirmExitPopup,
        currentShaderCode_editor, shaderLoadError_ui,
        currentShaderPath_ui, shaderCompileErrorLog_ui,
        objectColor_param, scale_param, timeSpeed_param,
        colorMod_param, patternScale_param,
        cameraPosition_param, cameraTarget_param, cameraFOV_param,
        lightPosition_param, lightColor_param,
        iUserFloat1_param, iUserColor1_param,
        filePathBuffer_Load, filePathBuffer_SaveAs,
        shadertoyInputBuffer, shadertoyApiKey,
        shaderSamples_main, 
        currentSampleIndex
    );
    uiManager.Initialize(); 
    uiManager.SetupInitialEditorAndShader(); 
    checkGLError("after initial shader setup via UIManager");


    if (*audioSystem.GetCurrentAudioSourceIndexPtr() == 0) { 
        audioSystem.InitializeAndStartSelectedCaptureDevice(); 
    }
    if (!audioSystem.GetLastError().empty()) { 
        if (!shaderLoadError_ui.empty()) shaderLoadError_ui += "\n"; 
        shaderLoadError_ui += "Audio Init: " + audioSystem.GetLastError();
    }

    lastFrameTime_main = (float)glfwGetTime();
    int spacebarState = GLFW_RELEASE;
    int f12State = GLFW_RELEASE;
    int f5State = GLFW_RELEASE;

    // --- Main Loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        checkGLError("L0: after PollEvents");

        int currentSpacebarState = glfwGetKey(window, GLFW_KEY_SPACE);
        if (currentSpacebarState == GLFW_PRESS && spacebarState == GLFW_RELEASE && !io.WantTextInput) { showGui = !showGui; }
        spacebarState = currentSpacebarState;

        int currentF12State = glfwGetKey(window, GLFW_KEY_F12);
        if (currentF12State == GLFW_PRESS && f12State == GLFW_RELEASE && !io.WantTextInput) {
            isFullscreen = !isFullscreen;
            if (isFullscreen) {
                glfwGetWindowPos(window, &storedWindowX, &storedWindowY);
                glfwGetWindowSize(window, &storedWindowWidth, &storedWindowHeight);
                GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
                glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            } else {
                glfwSetWindowMonitor(window, NULL, storedWindowX, storedWindowY, storedWindowWidth, storedWindowHeight, 0);
            }
             uiManager.RequestWindowSnap();
        }
        f12State = currentF12State;

        int currentF5State = glfwGetKey(window, GLFW_KEY_F5);
        if (currentF5State == GLFW_PRESS && f5State == GLFW_RELEASE && !io.WantTextInput) {
            uiManager.ApplyShaderFromEditorAndHandleResults(); 
            checkGLError("L1: after F5 shader apply");
        }
        f5State = currentF5State;
        
        processInput(window); 

        float currentTime = (float)glfwGetTime();
        deltaTime = currentTime - lastFrameTime_main;
        lastFrameTime_main = currentTime;
        frameCount++;

        if (audioSystem.GetCurrentAudioSourceIndex() == 2 && audioSystem.IsAudioFileLoaded()) { 
            if (audioSystem.IsCaptureDeviceInitialized()) { 
                audioSystem.StopCaptureDevice(); 
            }
            audioSystem.ProcessAudioFileSamples(1024); 
        } else if (audioSystem.GetCurrentAudioSourceIndex() == 0 && !audioSystem.IsCaptureDeviceInitialized()){ 
            // Potentially re-initialize
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        checkGLError("L2: after ImGui NewFrame");
      
        if (showGui) {
            uiManager.RenderAllUI(window, isFullscreen, storedWindowX, storedWindowY, storedWindowWidth, storedWindowHeight, deltaTime, frameCount, mouseState_iMouse);
        }
        
        // --- Rendering ---
        renderer.BeginFrame(0.02f, 0.02f, 0.03f, 1.0f); 
        checkGLError("L5: after BeginFrame"); 

        GLuint currentProgram = shaderManager.getActiveShaderProgram(); 
        if (currentProgram != 0) {
            glUseProgram(currentProgram); 
            checkGLError("L6: after glUseProgram"); 

            float currentAmpValueForShader = 0.0f;
            if (audioSystem.IsAudioLinkEnabled()) { 
                 if ((audioSystem.GetCurrentAudioSourceIndex() == 0 && audioSystem.IsCaptureDeviceInitialized()) || 
                     (audioSystem.GetCurrentAudioSourceIndex() == 2 && audioSystem.IsAudioFileLoaded())) { 
                    currentAmpValueForShader = audioSystem.GetCurrentAmplitude(); 
                }
            }

            int display_w, display_h;
            renderer.GetDisplaySize(display_w, display_h);

            if (shadertoyMode) {
                if(shaderManager.getResolutionLocation() != -1) glUniform3f(shaderManager.getResolutionLocation(), (float)display_w, (float)display_h, (float)display_w / (float)display_h); 
                if(shaderManager.getTimeLocation() != -1) glUniform1f(shaderManager.getTimeLocation(), (float)glfwGetTime()); 
                if(shaderManager.getTimeDeltaLocation() != -1) glUniform1f(shaderManager.getTimeDeltaLocation(), deltaTime); 
                if(shaderManager.getFrameLocation() != -1) glUniform1i(shaderManager.getFrameLocation(), frameCount); 
                if(shaderManager.getMouseLocation() != -1) glUniform4fv(shaderManager.getMouseLocation(), 1, mouseState_iMouse); 
                if(shaderManager.getUserFloat1Location() != -1) glUniform1f(shaderManager.getUserFloat1Location(), iUserFloat1_param); 
                if(shaderManager.getUserColor1Location() != -1) glUniform3fv(shaderManager.getUserColor1Location(), 1, iUserColor1_param); 
                if(shaderManager.getAudioAmpLocation() != -1) glUniform1f(shaderManager.getAudioAmpLocation(), currentAmpValueForShader); 

                for(const auto& control : shaderParser.GetUniformControls()){ 
                    if(control.location != -1){
                        if(control.glslType=="float")glUniform1f(control.location,control.fValue);
                        else if(control.glslType=="vec2")glUniform2fv(control.location,1,control.v2Value);
                        else if(control.glslType=="vec3")glUniform3fv(control.location,1,control.v3Value);
                        else if(control.glslType=="vec4")glUniform4fv(control.location,1,control.v4Value);
                        else if(control.glslType=="int")glUniform1i(control.location,control.iValue);
                        else if(control.glslType=="bool")glUniform1i(control.location,control.bValue?1:0);
                    }
                }
            } else { // Native Mode
                if(shaderManager.getResolutionLocation() != -1) glUniform2f(shaderManager.getResolutionLocation(), (float)display_w, (float)display_h); 
                if(shaderManager.getTimeLocation() != -1) glUniform1f(shaderManager.getTimeLocation(), (float)glfwGetTime()); 
                if(shaderManager.getObjectColorLocation() != -1) glUniform3fv(shaderManager.getObjectColorLocation(), 1, objectColor_param); 
                if(shaderManager.getScaleLocation() != -1) glUniform1f(shaderManager.getScaleLocation(), scale_param); 
                if(shaderManager.getTimeSpeedLocation() != -1) glUniform1f(shaderManager.getTimeSpeedLocation(), timeSpeed_param); 
                if(shaderManager.getColorModLocation() != -1) glUniform3fv(shaderManager.getColorModLocation(), 1, colorMod_param); 
                if(shaderManager.getPatternScaleLocation() != -1) glUniform1f(shaderManager.getPatternScaleLocation(), patternScale_param); 
                if(shaderManager.getCamPosLocation() != -1) glUniform3fv(shaderManager.getCamPosLocation(), 1, cameraPosition_param); 
                if(shaderManager.getCamTargetLocation() != -1) glUniform3fv(shaderManager.getCamTargetLocation(), 1, cameraTarget_param); 
                if(shaderManager.getCamFOVLocation() != -1) glUniform1f(shaderManager.getCamFOVLocation(), cameraFOV_param); 
                if(shaderManager.getLightPositionLocation() != -1) glUniform3fv(shaderManager.getLightPositionLocation(), 1, lightPosition_param); 
                if(shaderManager.getLightColorLocation() != -1) glUniform3fv(shaderManager.getLightColorLocation(), 1, lightColor_param); 
                if(shaderManager.getAudioAmpLocation() != -1) glUniform1f(shaderManager.getAudioAmpLocation(), currentAmpValueForShader); 
            }
            checkGLError("L7: after All Uniforms Set"); 

            renderer.RenderQuad(); 
            checkGLError("L8: after RenderQuad"); 
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        checkGLError("L9: after ImGui RenderDrawData"); 

        glfwSwapBuffers(window);
        checkGLError("L10: after SwapBuffers"); 
    } // End Main Loop

    // --- Cleanup ---
    renderer.Shutdown(); 
    audioSystem.Shutdown(); 

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* w, int width, int height){ (void)w; glViewport(0,0,width,height); }
void processInput(GLFWwindow* w){ if(glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS) g_ShowConfirmExitPopup = true; } 
void mouse_cursor_position_callback(GLFWwindow* w, double x, double y){ ImGuiIO& io = ImGui::GetIO(); if(!io.WantCaptureMouse || shadertoyMode){ int h; glfwGetWindowSize(w, NULL, &h); mouseState_iMouse[0] = (float)x; mouseState_iMouse[1] = (float)h - (float)y; }}
void mouse_button_callback(GLFWwindow* w, int b, int a, int m){ (void)m; ImGuiIO& io = ImGui::GetIO(); if(!io.WantCaptureMouse || shadertoyMode){ if(b == GLFW_MOUSE_BUTTON_LEFT){ if(a == GLFW_PRESS){ double xpos,ypos; glfwGetCursorPos(w, &xpos, &ypos); int h; glfwGetWindowSize(w, NULL, &h); mouseState_iMouse[0] = (float)xpos; mouseState_iMouse[1] = (float)h-(float)ypos; mouseState_iMouse[2] = mouseState_iMouse[0]; mouseState_iMouse[3] = mouseState_iMouse[1]; } else if (a == GLFW_RELEASE) {  mouseState_iMouse[2] = -std::abs(mouseState_iMouse[2]); mouseState_iMouse[3] = -std::abs(mouseState_iMouse[3]); } } } if(glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE){ if(mouseState_iMouse[2] > 0 || mouseState_iMouse[3] > 0){ mouseState_iMouse[2] = -std::abs(mouseState_iMouse[0]); mouseState_iMouse[3] = -std::abs(mouseState_iMouse[1]);}}}
void window_close_callback(GLFWwindow* w){ g_ShowConfirmExitPopup = true; glfwSetWindowShouldClose(w, GLFW_FALSE); }