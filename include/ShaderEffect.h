#pragma once

#include "Effect.h"
#include "ShaderParameterControls.h"
#include "ShaderParser.h" // Will own an instance of this
#include <string>
#include <vector>
#include <map>
#include <glad/glad.h>   // For GLuint, GLint
// #include "TextEditor.h" // If ShaderEffect directly interacts with TextEditor for errors (deferred)

// Forward declare ImGui if only used in .cpp for RenderUI
// struct ImGuiIO;

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
    const std::string& GetShaderFilePath() const { return m_shaderFilePath; }
    const std::string& GetCompileErrorLog() const { return m_compileErrorLog; }
    bool IsShadertoyMode() const { return m_isShadertoyMode; }
    void SetShadertoyMode(bool mode);

    // Potentially manage TextEditor error markers if needed directly
    // TextEditor::ErrorMarkers GetErrorMarkers() const;


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

    // --- Shadertoy Predefined Uniform Values ---
    float m_iUserFloat1 = 0.5f;
    float m_iUserColor1[3] = {0.2f, 0.5f, 0.8f};

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

    // --- Parsed Controls from Shader Code ---
    std::vector<ShaderDefineControl> m_defineControls;
    std::vector<ShaderToyUniformControl> m_shadertoyUniformControls; // For uniforms parsed from metadata comments
    std::vector<ShaderConstControl> m_constControls;

    // --- Parser Instance ---
    ShaderParser m_shaderParser;

    // --- Private Helper Methods ---
    void CompileAndLinkShader(); // Uses m_shaderSourceCode, populates m_shaderProgram and m_compileErrorLog
    void FetchUniformLocations();  // Populates all m_...Loc variables and locations in m_shadertoyUniformControls
    void ParseShaderControls();    // Uses m_shaderParser to populate m_defineControls, m_shadertoyUniformControls, m_constControls

    // Helpers for loading shader source file
    std::string LoadShaderSourceFile(const std::string& filePath, std::string& errorMsg);

    // UI helper
    void RenderNativeParamsUI();
    void RenderShadertoyParamsUI();
    void RenderDefineControlsUI();
    void RenderConstControlsUI();
    void RenderMetadataUniformsUI();

    // To update display resolution, mouse state etc. from main loop if needed for uniforms
public:
    void SetMouseState(float x, float y, float click_x, float click_y); // click_x/y can be z/w of iMouse
    void SetDisplayResolution(int width, int height); // This will be used for iResolution uniform
    void SetDeltaTime(float dt) { m_deltaTime = dt; }
    void IncrementFrameCount() { m_frameCount++; }

    // --- FBO specific methods ---
    GLuint GetOutputTexture() const override; // Override from Effect base
    // Resizes the FBO and its attachments. Called on init and if window resizes.
    void ResizeFrameBuffer(int width, int height);

    // --- Node Editor specific methods ---
    int GetInputPinCount() const override;
    // int GetOutputPinCount() const override; // Already defaults to 1 in Effect.h, can override if different
    void SetInputEffect(int pinIndex, Effect* inputEffect) override;
    const std::vector<Effect*>& GetInputs() const { return m_inputs; }


private:
    // --- Node Editor Inputs ---
    std::vector<Effect*> m_inputs; // Stores pointers to input effects
    GLint m_iChannel0SamplerLoc = -1; // Uniform location for iChannel0 input texture sampler

    // --- FBO Members ---
    GLuint m_fboID = 0;
    GLuint m_fboTextureID = 0;
    GLuint m_rboID = 0; // For depth/stencil attachment
    int m_fboWidth = 0;
    int m_fboHeight = 0;
};
