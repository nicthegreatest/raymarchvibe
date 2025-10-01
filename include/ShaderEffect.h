#pragma once

#include "Effect.h"
#include "ShaderParser.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>

class ShaderEffect : public Effect {
public:
    ShaderEffect(const std::string& initialShaderPath = "", int initialWidth = 800, int initialHeight = 600, bool isShadertoy = false);
    ~ShaderEffect() override;

    void Load() override;
    void Update(float currentTime) override;
    void Render() override;
    void RenderUI() override;
    GLuint GetOutputTexture() const override;
    void ResizeFrameBuffer(int width, int height);

    int GetInputPinCount() const override;
    void SetInputEffect(int pinIndex, Effect* inputEffect) override;
    const std::vector<Effect*>& GetInputs() const { return m_inputs; }

    bool LoadShaderFromFile(const std::string& filePath);
    bool LoadShaderFromSource(const std::string& sourceCode);
    void ApplyShaderCode(const std::string& newShaderCode);
    void SetShadertoyMode(bool mode);
    bool IsShadertoyMode() const; // Added getter

    void SetMouseState(float x, float y, float click_x, float click_y);
    void SetDisplayResolution(int width, int height);
    void SetDeltaTime(float dt) { m_deltaTime = dt; }
    void IncrementFrameCount() { m_frameCount++; }
    void SetAudioAmplitude(float amp);
    void SetAudioBands(const std::array<float, 4>& bands);

    const std::string& GetShaderSource() const { return m_shaderSourceCode; }
    const std::string& GetCompileErrorLog() const { return m_compileErrorLog; }
    void SetSourceFilePath(const std::string& path);
    const std::string& GetSourceFilePath() const;

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& data) override;
    void ResetParameters() override;

    bool CheckForUpdatesAndReload();

    const std::vector<int>& GetDeserializedInputIds() const { return m_deserialized_input_ids; }

    static void InitializeDummyTexture();

private:
    void CompileAndLinkShader();
    void FetchUniformLocations();
    void ParseShaderControls();
    std::string LoadShaderSourceFile(const std::string& filePath, std::string& errorMsg);
    void RenderNativeParamsUI();
    void RenderShadertoyParamsUI();
    void RenderParsedUniformsUI();
    void RenderDefineControlsUI();
    void RenderConstControlsUI();
    void RenderColorCycleUI();
    void GetGradientColor(float t, float* outColor);

    GLuint m_shaderProgram;
    bool m_isShadertoyMode;
    bool m_shaderLoaded;
    std::string m_shaderFilePath;
    std::string m_shaderSourceCode;
    std::string m_compileErrorLog;

    std::vector<Effect*> m_inputs;

    float m_time;
    float m_deltaTime;
    int m_frameCount;
    float m_mouseState[4];
    int m_currentDisplayWidth, m_currentDisplayHeight;

    // Shadertoy user uniforms

    float m_audioAmp;
    std::array<float, 4> m_audioBands;

    // Uniform locations
    GLint m_iResolutionLocation = -1;
    GLint m_iTimeLocation = -1;
    GLint m_iTimeDeltaLocation = -1;
    GLint m_iFrameLocation = -1;
    GLint m_iMouseLocation = -1;
    GLint m_iChannel0SamplerLoc = -1;
    GLint m_iChannel0ActiveLoc = -1;
    GLint m_iAudioAmpLoc = -1;
    GLint m_iAudioBandsLoc = -1;

    ShaderParser m_shaderParser;
    std::vector<ShaderToyUniformControl> m_shadertoyUniformControls;
    std::vector<DefineControl> m_defineControls;
    std::vector<ConstVariableControl> m_constControls;

    struct ColorCycleState {
        bool isEnabled = false;
        float speed = 1.0f;
        float cycleTime = 0.0f;
        int currentGradient = 0;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ColorCycleState, isEnabled, speed, cycleTime, currentGradient);
    } m_colorCycleState;

    GLuint m_fboID, m_fboTextureID, m_rboID;
    int m_fboWidth, m_fboHeight;

    nlohmann::json m_deserialized_controls;
    std::vector<int> m_deserialized_input_ids;
    std::filesystem::file_time_type m_lastWriteTime;

    static GLuint s_dummyTexture;
};