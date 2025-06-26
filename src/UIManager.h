#pragma once
#ifndef UIMANAGER_H
#define UIMANAGER_H

#include "ShaderManager.h"
#include "ShaderParser.h"
#include "AudioSystem.h"
#include "TextEditor.h"
#include "Bess/Config/Themes.h" // Added Themes header
#include <string>
#include <vector>

// Forward declaration
struct GLFWwindow;

// Struct to hold shader sample info, kept here for UIManager's use
struct ShaderSample_UI {
    std::string name;
    std::string path;
};

class UIManager {
public:
    // Constructor now takes references to new window visibility flags
    UIManager(
        ShaderManager& shaderManager, ShaderParser& shaderParser, AudioSystem& audioSystem, TextEditor& editor,
        bool& shadertoyMode, bool& showGui,
        bool& showHelpWindow, bool& showAudioReactivityWindow, bool& g_ShowConfirmExitPopup,
        bool& showStatusWindow, bool& showEditorWindow, bool& showConsoleWindow, // New visibility flags
        std::string& currentShaderCode_editor, std::string& shaderLoadError_ui,
        std::string& currentShaderPath_ui, std::string& shaderCompileErrorLog_ui,
        float* objectColor_param, float& scale_param, float& timeSpeed_param,
        float* colorMod_param, float& patternScale_param,
        float* cameraPosition_param, float* cameraTarget_param, float& cameraFOV_param,
        float* lightPosition_param, float* lightColor_param,
        float& iUserFloat1_param, float* iUserColor1_param,
        char* filePathBuffer_Load, char* filePathBuffer_SaveAs,
        char* shadertoyInputBuffer, const std::string& shadertoyApiKey,
        const std::vector<ShaderSample_UI>& shaderSamples,
        size_t& currentSampleIndex
    );

    void Initialize();
    void SetupInitialEditorAndShader();
    
    // Main rendering function that orchestrates all UI components
    void RenderAllUI(GLFWwindow* window, bool isFullscreen, int storedWindowX, int storedWindowY, int storedWindowWidth, int storedWindowHeight, float deltaTime, int frameCount, const float* mouseState);

    // Public method to apply shader changes, callable from main (e.g., F5 hotkey)
    void ApplyShaderFromEditorAndHandleResults();
    void RequestWindowSnap();

    // --- Fullscreen Toggle Interface ---
    // Called by main.cpp when F12 is pressed
    void requestFullscreenToggle() { m_fullscreenRequest = true; } 
    // Checked by main.cpp in the main loop
    bool getFullscreenRequest() const { return m_fullscreenRequest; } 
    // Called by main.cpp after handling the request
    void acknowledgeFullscreenRequest() { m_fullscreenRequest = false; }

private:
    // --- UI Rendering Methods ---
    // These methods render specific parts of the UI.
    // RenderAllUI calls these based on visibility flags.
    void RenderMenuBar(GLFWwindow* window);
    void RenderStatusWindow(float deltaTime, int frameCount);
    void RenderShaderEditorWindow(const float* mouseState);
    void RenderConsoleWindow();
    void RenderAudioReactivityWindow();
    void RenderHelpWindow();
    void RenderConfirmExitPopup(GLFWwindow* window);
    void RenderAboutPopup(); // New popup for "About"

    // --- Helper Methods ---
    // Internal logic for UI actions
    void HandleShaderLoad(const std::string& path);
    void HandleShaderSave(const std::string& path);
    void HandleNewShader(bool isShadertoy);
    void HandleLoadFromShadertoy();
    void ResetAllParameters();

    // --- References to External State (from main.cpp) ---
    ShaderManager& m_shaderManager_ref;
    ShaderParser& m_shaderParser_ref;
    AudioSystem& m_audioSystem_ref;
    TextEditor& m_editor_ref;

    // --- UI State and Visibility Flags ---
    bool& m_shadertoyMode_ref;
    bool& m_showGui_ref;
    bool& m_showHelpWindow_ref;
    bool& m_showAudioReactivityWindow_ref;
    bool& m_g_ShowConfirmExitPopup_ref;
    // New visibility flags for windows, controlled by the menu bar
    bool& m_showStatusWindow_ref;
    bool& m_showEditorWindow_ref;
    bool& m_showConsoleWindow_ref;
    bool  m_showAboutPopup = false; // Internal state for the "About" popup
    
    // Flag to communicate fullscreen toggle request to main.cpp
    bool m_fullscreenRequest = false; 

    // --- References to Shader and File Data ---
    std::string& m_currentShaderCode_editor_ref;
    std::string& m_shaderLoadError_ui_ref;
    std::string& m_currentShaderPath_ui_ref;
    std::string& m_shaderCompileErrorLog_ui_ref;

    // --- References to Shader Parameters ---
    float* p_objectColor_param;
    float& m_scale_param_ref;
    float& m_timeSpeed_param_ref;
    float* p_colorMod_param;
    float& m_patternScale_param_ref;
    float* p_cameraPosition_param;
    float* p_cameraTarget_param;
    float& m_cameraFOV_param_ref;
    float* p_lightPosition_param;
    float* p_lightColor_param;
    float& m_iUserFloat1_param_ref;
    float* p_iUserColor1_param;

    // --- References to Input Buffers and Other UI Data ---
    char* p_filePathBuffer_Load;
    char* p_filePathBuffer_SaveAs;
    char* p_shadertoyInputBuffer;
    const std::string& m_shadertoyApiKey_ref;
    const std::vector<ShaderSample_UI>& m_shaderSamples_ref;
    size_t& m_currentSampleIndex_ref;

    // Internal state for UI
    bool m_snapWindows = false;
    Bess::Config::Themes m_themes; // Added Themes object

    // --- Find Functionality State ---
    char m_findText[256];
    TextEditor::Coordinates m_findStartCoord; // To store the starting point for the next search
    TextEditor::Coordinates m_lastMatchStartCoord; // To store the actual start of the last match
    TextEditor::Coordinates m_lastMatchEndCoord;   // To store the actual end of the last match
    bool m_foundText;
    bool m_findCaseSensitive;
    int m_currentFindIndex; // To keep track of multiple occurrences, or simply for "Find Next" logic

    void HandleFindNext();
    void HandleFindPrevious(); // Declaration for later
    TextEditor::Coordinates GetCoordinatesForOffset(const std::string& text, int offset);
    int GetOffsetForCoordinates(const std::string& text, const TextEditor::Coordinates& coords);


};

#endif // UIMANAGER_H
