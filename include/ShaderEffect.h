#pragma once

#include "Effect.h"
#include "ShaderParser.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <glm/glm.hpp>

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
    void SetCameraState(const glm::vec3& pos, const glm::mat4& viewMatrix);
    void SetLightPosition(const glm::vec3& pos);

    const std::string& GetShaderSource() const { return m_shaderSourceCode; }
    const std::string& GetCompileErrorLog() const { return m_compileErrorLog; }
    void SetSourceFilePath(const std::string& path);
    const std::string& GetSourceFilePath() const;

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& data) override;
    void ResetParameters() override;

    std::unique_ptr<Effect> Clone() const override;

    bool CheckForUpdatesAndReload();

    const std::vector<int>& GetDeserializedInputIds() const { return m_deserialized_input_ids; }

    // For UI-driven texture loading
    void SetChannelPendingTextureLoad(int channelIndex) { m_channelPendingTextureLoad = channelIndex; }
    int GetChannelPendingTextureLoad() const { return m_channelPendingTextureLoad; }
    void ClearChannelPendingTextureLoad() { m_channelPendingTextureLoad = -1; }

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

    ShaderParser m_shaderParser;
    std::vector<ShaderToyUniformControl> m_shadertoyUniformControls;
    std::vector<DefineControl> m_defineControls;
    std::vector<ConstVariableControl> m_constControls;

    // Uniform locations
    GLint m_iResolutionLocation = -1;
    GLint m_iTimeLocation = -1;
    GLint m_iTimeDeltaLocation = -1;
    GLint m_iFrameLocation = -1;
    GLint m_iMouseLocation = -1;
    GLint m_iChannel0SamplerLoc = -1;
    GLint m_iChannel1SamplerLoc = -1;
    GLint m_iChannel2SamplerLoc = -1;
    GLint m_iChannel3SamplerLoc = -1;
    GLint m_iChannel0ActiveLoc = -1;
    GLint m_iChannel1ActiveLoc = -1;
    GLint m_iChannel2ActiveLoc = -1;
    GLint m_iChannel3ActiveLoc = -1;
    GLint m_iAudioAmpLoc = -1;
    GLint m_iAudioBandsLoc = -1;
    GLint m_iCameraPositionLocation = -1;
    GLint m_iCameraMatrixLocation = -1;
    GLint m_iLightPositionLocation = -1;

    glm::vec3 m_cameraPosition;
    glm::mat4 m_cameraMatrix;
    glm::vec3 m_lightPosition;

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

    int m_channelPendingTextureLoad = -1;

    static GLuint s_dummyTexture;
};