#ifndef UIMANAGER_H
#define UIMANAGER_H

#include "imgui.h"
#include <cstdint>       // <<< ADDED for uintX_t types
#include "TextEditor.h" 
#include <string>
#include <vector>
#include <GLFW/glfw3.h> // For GLFWwindow*

// Forward declarations of our systems/managers
class ShaderManager;
class ShaderParser;
class AudioSystem;

struct ShaderSample_UI { 
    const char* name;
    const char* filePath;
};

class UIManager {
public:
    UIManager(
        ShaderManager& sm,
        ShaderParser& sp,
        AudioSystem& as,
        TextEditor& editorRef,
        bool& shadertoyMode,
        bool& showGui,
        bool& showHelpWindow,
        bool& showAudioReactivityWindow,
        bool& g_ShowConfirmExitPopup,
        std::string& currentShaderCodeEditor,
        std::string& shaderLoadErrorUi,
        std::string& currentShaderPathUi,
        std::string& shaderCompileErrorLogUi,
        float* objectColorParam, float& scaleParam, float& timeSpeedParam,
        float* colorModParam, float& patternScaleParam,
        float* cameraPositionParam, float* cameraTargetParam, float& cameraFOVParam,
        float* lightPositionParam, float* lightColorParam,
        float& iUserFloat1Param, float* iUserColor1Param,
        char* filePathBufferLoad, char* filePathBufferSaveAs,
        char* shadertoyInputBuffer, std::string& shadertoyApiKey,
        const std::vector<ShaderSample_UI>& samples,
        size_t& currentSampleIdx
    ); 

    ~UIManager();

    void Initialize(); 
    void RenderAllUI(GLFWwindow* window, bool isFullscreen, int& storedWindowX, int& storedWindowY, int& storedWindowWidth, int& storedWindowHeight, float& deltaTime, int& frameCount, float mouseState_iMouse[4]);
    
    void SetupInitialEditorAndShader();
    void RequestWindowSnap();
    void ApplyShaderFromEditorAndHandleResults();

private:
    ShaderManager& m_shaderManager;
    ShaderParser& m_shaderParser;
    AudioSystem& m_audioSystem;
    TextEditor& m_editor;

    bool& m_shadertoyMode;
    bool& m_showGui;
    bool& m_showHelpWindow_ref;        
    bool& m_showAudioReactivityWindow_ref; 
    bool& m_g_ShowConfirmExitPopup_ref;    

    std::string& m_currentShaderCode_editor_ref;
    std::string& m_shaderLoadError_ui_ref;
    std::string& m_currentShaderPath_ui_ref;
    std::string& m_shaderCompileErrorLog_ui_ref;

    float* p_objectColor_param;
    float& r_scale_param;
    float& r_timeSpeed_param;
    float* p_colorMod_param;
    float& r_patternScale_param;
    float* p_cameraPosition_param;
    float* p_cameraTarget_param;
    float& r_cameraFOV_param;
    float* p_lightPosition_param;
    float* p_lightColor_param;
    float& r_iUserFloat1_param;
    float* p_iUserColor1_param;

    char* p_filePathBuffer_Load;
    char* p_filePathBuffer_SaveAs;
    char* p_shadertoyInputBuffer;
    std::string& r_shadertoyApiKey;

    const std::vector<ShaderSample_UI>& m_shaderSamples_ref;
    size_t& r_currentSampleIndex;

    void RenderStatusWindow();
    void RenderShaderEditorWindow();
    void RenderConsoleWindow();
    void RenderHelpWindow(); 
    void RenderAudioReactivityWindow(); 
    
    void ClearEditorErrorMarkers();

    // Corrected signature to match definition and usage
    std::string FetchShadertoyCode(const std::string& idOrUrl, const std::string& apiKey, std::string& errorMsg); 
    std::string ExtractShadertoyId(const std::string& idOrUrl);
    std::string TrimString(const std::string& str);

    bool m_snapWindowsNextFrame = false;

    const char* m_nativeShaderTemplate;
    const char* m_shadertoyShaderTemplate;
};

#endif // UIMANAGER_H