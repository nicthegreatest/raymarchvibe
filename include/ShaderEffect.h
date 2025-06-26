#pragma once

#include "Effect.h"
#include "ShaderParameterControls.h"
#include "ShaderParser.h" // Will own an instance of this
#include <nlohmann/json.hpp> // For serialization
#include <string>
#include <vector>
#include <map>
#include <glad/glad.h>   // For GLuint, GLint

// Forward declare ImGui if only used in .cpp for RenderUI
// struct ImGuiIO;

// New struct to hold the state for the color cycling feature
struct ColorCycleState {
    bool isEnabled = false;
    float speed = 1.0f;
    int currentGradient = 0; // 0: Rainbow, 1: Fire, 2: Ice
    float cycleTime = 0.0f;

    // For serialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ColorCycleState, isEnabled, speed, currentGradient, cycleTime);
};


class ShaderEffect : public Effect {
public:
    // Constructor now takes initial FBO dimensions
    ShaderEffect(const std::string& initialShaderPath = "", int initialWidth = 800, int initialHeight = 600, bool isShadertoy = false);
    ~ShaderEffect() override;

    void Load() override; // Loads from m_shaderFilePath or m_shaderSourceCode
    void Update(float currentTime) override;
    void Render() override;
    void RenderUI() override;

    // --- Shader Management ---
    bool LoadShaderFromFile(const std::string& filePath);
    bool LoadShaderFromSource(const std::string& sourceCode);
    void ApplyShaderCode(const std::string& newShaderCode); // Compiles, links, gets uniforms, parses controls
    const std::string& GetShaderSource() const { return m_shaderSourceCode; }
    const std::string& GetCompileErrorLog() const { return m_compileErrorLog; }
    bool IsShadertoyMode() const { return m_isShadertoyMode; }
    void SetShadertoyMode(bool mode);

    // --- Effect base overrides ---
    void SetSourceFilePath(const std::string& path) override;
    const std::string& GetSourceFilePath() const override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& data) override;
    void ResetParameters() override;

private:
    // --- Shader Program & Source ---
    GLuint m_shaderProgram = 0;
    std::string m_shaderSourceCode; // Current full source code of the fragment shader
    std::string m_shaderFilePath;   // Path to the loaded .frag file, if any
    std::string m_compileErrorLog;
    bool m_isShadertoyMode = false;
    bool m_shaderLoaded = false; // To prevent rendering/UI if load failed

    // --- Time and Resolution ---
    float m_time = 0.0f; // Internal time, potentially modified by m_timeSpeed
    float m_deltaTime = 0.0f; // Frame delta time
    int m_frameCount = 0;
    float m_mouseState[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // iMouse (x, y, click_x, click_y)
    int m_currentDisplayWidth = 0;
    int m_currentDisplayHeight = 0;


    // --- Native Mode Uniform Values ---
    float m_objectColor[3] = {0.8f, 0.9f, 1.0f};
    float m_scale = 1.0f;
    float m_timeSpeed = 1.0f; // Multiplier for iTime in native mode
    float m_colorMod[3] = {0.1f, 0.1f, 0.2f};
    float m_patternScale = 1.0f;
    float m_cameraPosition[3] = {0.0f, 1.0f, -3.0f};
    float m_cameraTarget[3] = {0.0f, 0.0f, 0.0f};
    float m_cameraFOV = 60.0f;
    float m_lightPosition[3] = {2.0f, 3.0f, -2.0f};
    float m_lightColor[3] = {1.0f, 1.0f, 0.9f};

    // --- Color Cycling State ---
    ColorCycleState m_colorCycleState;

    // --- Shadertoy Predefined Uniform Values ---
    float m_iUserFloat1 = 0.5f;
    float m_iUserColor1[3] = {0.2f, 0.5f, 0.8f};
    float m_audioAmp = 0.0f; // Audio amplitude

    // --- Uniform Locations ---
    // Common
    GLint m_iResolutionLocation = -1;
    GLint m_iTimeLocation = -1;
    // Native
    GLint m_uObjectColorLoc = -1;
    GLint m_uScaleLoc = -1;
    GLint m_uTimeSpeedLoc = -1;
    GLint m_uColorModLoc = -1;
    GLint m_uPatternScaleLoc = -1;
    GLint m_uCamPosLoc = -1;
    GLint m_uCamTargetLoc = -1;
    GLint m_uCamFOVLoc = -1;
    GLint m_uLightPosLoc = -1;
    GLint m_uLightColorLoc = -1;
    // Shadertoy
    GLint m_iTimeDeltaLocation = -1;
    GLint m_iFrameLocation = -1;
    GLint m_iMouseLocation = -1;
    GLint m_iUserFloat1Loc = -1;
    GLint m_iUserColor1Loc = -1;
    // Note: Locations for metadata-driven Shadertoy uniforms are stored in ShaderToyUniformControl structs
    GLint m_iAudioAmpLoc = -1; // Location for iAudioAmp uniform

    // --- Parsed Controls from Shader Code ---
    std::vector<DefineControl> m_defineControls;
    std::vector<ShaderToyUniformControl> m_shadertoyUniformControls;
    std::vector<ConstVariableControl> m_constControls;

    // --- Parser Instance ---
    ShaderParser m_shaderParser;

    // --- Private Helper Methods ---
    void CompileAndLinkShader();
    void FetchUniformLocations();
    void ParseShaderControls();

    std::string LoadShaderSourceFile(const std::string& filePath, std::string& errorMsg);

    // UI helpers
    void RenderNativeParamsUI();
    void RenderShadertoyParamsUI();
    void RenderDefineControlsUI();
    void RenderConstControlsUI();
    void RenderMetadataUniformsUI();
    void RenderColorCycleUI(); // New UI for color cycling
    void GetGradientColor(float t, float* outColor); // Helper to calculate gradient color

public:
    void SetMouseState(float x, float y, float click_x, float click_y);
    void SetDisplayResolution(int width, int height);
    void SetDeltaTime(float dt) { m_deltaTime = dt; }
    void IncrementFrameCount() { m_frameCount++; }
    void SetAudioAmplitude(float amp);

    // --- FBO specific methods ---
    GLuint GetOutputTexture() const override;
    void ResizeFrameBuffer(int width, int height);

    // --- Node Editor specific methods ---
    int GetInputPinCount() const override;
    void SetInputEffect(int pinIndex, Effect* inputEffect) override;
    const std::vector<Effect*>& GetInputs() const { return m_inputs; }

private:
    // --- Node Editor Inputs ---
    std::vector<Effect*> m_inputs;
    GLint m_iChannel0SamplerLoc = -1;

    // --- FBO Members ---
    GLuint m_fboID = 0;
    GLuint m_fboTextureID = 0;
    GLuint m_rboID = 0;
    int m_fboWidth = 0;
    int m_fboHeight = 0;
};
