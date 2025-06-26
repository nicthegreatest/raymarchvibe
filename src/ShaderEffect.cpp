#include "ShaderEffect.h"
#include "Renderer.h"
#include "imgui.h"
#include <cmath> // For sin, cos in color cycling
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <regex>

// Local GL error checker for ShaderEffect
static void checkGLErrorInEffect(const std::string& label, const std::string& effectName) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        std::string errorStr;
        switch(err) {
            case GL_INVALID_ENUM: errorStr = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorStr = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "INVALID_FRAMEBUFFER_OPERATION"; break;
            default: errorStr = "UNKNOWN_ERROR (" + std::to_string(err) + ")"; break;
        }
        std::cerr << "GL_ERROR (Effect: " << effectName << ", Op: " << label << "): " << errorStr << std::endl;
    }
}

// Helper function declarations
static GLuint CompileShader(const char* source, GLenum type, std::string& errorLogString);
static GLuint CreateShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString);
static std::string LoadPassthroughVertexShaderSource(std::string& errorMsg);

ShaderEffect::ShaderEffect(const std::string& initialShaderPath, int initialWidth, int initialHeight, bool isShadertoy)
    : Effect(),
      m_shaderProgram(0),
      m_isShadertoyMode(isShadertoy),
      m_shaderLoaded(false),
      m_time(0.0f),
      m_deltaTime(0.0f),
      m_frameCount(0),
      m_scale(1.0f),
      m_timeSpeed(1.0f),
      m_patternScale(1.0f),
      m_cameraFOV(60.0f),
      m_iUserFloat1(0.5f),
      m_audioAmp(0.0f),
      m_iAudioAmpLoc(-1),
      m_shaderParser(),
      m_iChannel0SamplerLoc(-1),
      m_fboID(0),
      m_fboTextureID(0),
      m_rboID(0),
      m_fboWidth(initialWidth),
      m_fboHeight(initialHeight)
{
    m_inputs.resize(1, nullptr);

    // Initialize arrays
    m_objectColor[0] = 0.8f; m_objectColor[1] = 0.9f; m_objectColor[2] = 1.0f;
    m_colorMod[0] = 0.1f; m_colorMod[1] = 0.1f; m_colorMod[2] = 0.2f;
    m_cameraPosition[0] = 0.0f; m_cameraPosition[1] = 1.0f; m_cameraPosition[2] = -3.0f;
    m_cameraTarget[0] = 0.0f; m_cameraTarget[1] = 0.0f; m_cameraTarget[2] = 0.0f;
    m_lightPosition[0] = 2.0f; m_lightPosition[1] = 3.0f; m_lightPosition[2] = -2.0f;
    m_lightColor[0] = 1.0f; m_lightColor[1] = 1.0f; m_lightColor[2] = 0.9f;
    m_iUserColor1[0] = 0.2f; m_iUserColor1[1] = 0.5f; m_iUserColor1[2] = 0.8f;
    std::fill_n(m_mouseState, 4, 0.0f);

    if (!initialShaderPath.empty()) {
        m_shaderFilePath = initialShaderPath;
    }
}

ShaderEffect::~ShaderEffect() {
    if (m_shaderProgram != 0) glDeleteProgram(m_shaderProgram);
    if (m_fboID != 0) glDeleteFramebuffers(1, &m_fboID);
    if (m_fboTextureID != 0) glDeleteTextures(1, &m_fboTextureID);
    if (m_rboID != 0) glDeleteRenderbuffers(1, &m_rboID);
}

GLuint ShaderEffect::GetOutputTexture() const {
    return m_fboTextureID;
}

void ShaderEffect::ResizeFrameBuffer(int width, int height) {
    if (width <= 0 || height <= 0) {
        std::cerr << "ShaderEffect::ResizeFrameBuffer error: Invalid dimensions (" << width << "x" << height << ") for " << name << std::endl;
        return;
    }
    m_fboWidth = width;
    m_fboHeight = height;

    if (m_fboID != 0) glDeleteFramebuffers(1, &m_fboID);
    if (m_fboTextureID != 0) glDeleteTextures(1, &m_fboTextureID);
    if (m_rboID != 0) glDeleteRenderbuffers(1, &m_rboID);

    glGenFramebuffers(1, &m_fboID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);

    glGenTextures(1, &m_fboTextureID);
    glBindTexture(GL_TEXTURE_2D, m_fboTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fboWidth, m_fboHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboTextureID, 0);

    glGenRenderbuffers(1, &m_rboID);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rboID);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_fboWidth, m_fboHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rboID);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer for " << name << " is not complete!" << std::endl;
        glDeleteFramebuffers(1, &m_fboID); m_fboID = 0;
        glDeleteTextures(1, &m_fboTextureID); m_fboTextureID = 0;
        glDeleteRenderbuffers(1, &m_rboID); m_rboID = 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int ShaderEffect::GetInputPinCount() const {
    return m_inputs.size();
}

void ShaderEffect::SetInputEffect(int pinIndex, Effect* inputEffect) {
    if (pinIndex >= 0 && static_cast<size_t>(pinIndex) < m_inputs.size()) {
        m_inputs[static_cast<size_t>(pinIndex)] = inputEffect;
    }
}

bool ShaderEffect::LoadShaderFromFile(const std::string& filePath) {
    m_shaderFilePath = filePath;
    std::string loadError;
    m_shaderSourceCode = LoadShaderSourceFile(m_shaderFilePath, loadError);
    if (!loadError.empty()) {
        m_compileErrorLog = "File load error: " + loadError;
        m_shaderLoaded = false;
        return false;
    }
    if (m_shaderSourceCode.find("mainImage") != std::string::npos) {
        m_isShadertoyMode = true;
    } else {
        m_isShadertoyMode = false;
    }
    return true;
}

bool ShaderEffect::LoadShaderFromSource(const std::string& sourceCode) {
    m_shaderFilePath = "dynamic_source";
    m_shaderSourceCode = sourceCode;
    if (m_shaderSourceCode.find("mainImage") != std::string::npos) {
        m_isShadertoyMode = true;
    } else {
        m_isShadertoyMode = false;
    }
    return true;
}

void ShaderEffect::Load() {
    if (m_fboWidth > 0 && m_fboHeight > 0) {
        ResizeFrameBuffer(m_fboWidth, m_fboHeight);
    }

    if (m_shaderSourceCode.empty() && !m_shaderFilePath.empty()) {
        std::string loadError;
        m_shaderSourceCode = LoadShaderSourceFile(m_shaderFilePath, loadError);
        if (!loadError.empty()) {
            m_compileErrorLog = "File load error during Load(): " + loadError;
            m_shaderLoaded = false;
            return;
        }
        if (m_shaderSourceCode.find("mainImage") != std::string::npos) {
            m_isShadertoyMode = true;
        } else {
            m_isShadertoyMode = false;
        }
    }

    if (!m_shaderSourceCode.empty()) {
        ApplyShaderCode(m_shaderSourceCode);
    } else {
        m_compileErrorLog = "Shader source code for " + name + " is empty. Cannot load.";
        m_shaderLoaded = false;
    }
}

void ShaderEffect::ApplyShaderCode(const std::string& newShaderCode) {
    m_shaderSourceCode = newShaderCode;
    m_compileErrorLog.clear();
    CompileAndLinkShader();
    if (m_shaderProgram != 0) {
        FetchUniformLocations();
        ParseShaderControls();
        m_shaderLoaded = true;
        if (m_compileErrorLog.empty()) {
             m_compileErrorLog = "Shader applied successfully.";
        }
    } else {
        m_shaderLoaded = false;
    }
}

void ShaderEffect::SetShadertoyMode(bool mode) {
    if (m_isShadertoyMode != mode) {
        m_isShadertoyMode = mode;
        if (m_shaderProgram != 0) {
            FetchUniformLocations();
            ParseShaderControls();
        }
    }
}

void ShaderEffect::Update(float currentTime) {
    m_time = currentTime;

    if (m_colorCycleState.isEnabled) {
        m_colorCycleState.cycleTime += m_deltaTime * m_colorCycleState.speed;
        GetGradientColor(m_colorCycleState.cycleTime, m_objectColor);
    }
}

void ShaderEffect::Render() {
    if (!m_shaderLoaded || m_shaderProgram == 0 || m_fboID == 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
    glViewport(0, 0, m_fboWidth, m_fboHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_shaderProgram);

    if (!m_inputs.empty() && m_inputs[0] != nullptr) {
        if (auto* inputSE = dynamic_cast<ShaderEffect*>(m_inputs[0])) {
            GLuint inputTextureID = inputSE->GetOutputTexture();
            if (inputTextureID != 0 && m_iChannel0SamplerLoc != -1) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, inputTextureID);
                glUniform1i(m_iChannel0SamplerLoc, 0);
            }
        }
    }

    if (m_isShadertoyMode) {
        if (m_iResolutionLocation != -1) glUniform3f(m_iResolutionLocation, (float)m_fboWidth, (float)m_fboHeight, (float)m_fboWidth / (float)m_fboHeight);
        if (m_iTimeLocation != -1) glUniform1f(m_iTimeLocation, m_time);
        if (m_iTimeDeltaLocation != -1) glUniform1f(m_iTimeDeltaLocation, m_deltaTime);
        if (m_iFrameLocation != -1) glUniform1i(m_iFrameLocation, m_frameCount);
        if (m_iMouseLocation != -1) glUniform4fv(m_iMouseLocation, 1, m_mouseState);
        if (m_iUserFloat1Loc != -1) glUniform1f(m_iUserFloat1Loc, m_iUserFloat1);
        if (m_iUserColor1Loc != -1) glUniform3fv(m_iUserColor1Loc, 1, m_iUserColor1);
    } else {
        if (m_iResolutionLocation != -1) glUniform2f(m_iResolutionLocation, (float)m_fboWidth, (float)m_fboHeight);
        if (m_iTimeLocation != -1) glUniform1f(m_iTimeLocation, m_time);
        if (m_uObjectColorLoc != -1) glUniform3fv(m_uObjectColorLoc, 1, m_objectColor);
        if (m_uScaleLoc != -1) glUniform1f(m_uScaleLoc, m_scale);
        if (m_uTimeSpeedLoc != -1) glUniform1f(m_uTimeSpeedLoc, m_timeSpeed);
        if (m_uColorModLoc != -1) glUniform3fv(m_uColorModLoc, 1, m_colorMod);
        if (m_uPatternScaleLoc != -1) glUniform1f(m_uPatternScaleLoc, m_patternScale);
        if (m_uCamPosLoc != -1) glUniform3fv(m_uCamPosLoc, 1, m_cameraPosition);
        if (m_uCamTargetLoc != -1) glUniform3fv(m_uCamTargetLoc, 1, m_cameraTarget);
        if (m_uCamFOVLoc != -1) glUniform1f(m_uCamFOVLoc, m_cameraFOV);
        if (m_uLightPosLoc != -1) glUniform3fv(m_uLightPosLoc, 1, m_lightPosition);
        if (m_uLightColorLoc != -1) glUniform3fv(m_uLightColorLoc, 1, m_lightColor);
    }
    if (m_iAudioAmpLoc != -1) {
        glUniform1f(m_iAudioAmpLoc, m_audioAmp);
    }

    Renderer::RenderQuad();
}

void ShaderEffect::SetAudioAmplitude(float amp) {
    m_audioAmp = amp;
}

void ShaderEffect::SetMouseState(float x, float y, float click_x, float click_y) {
    m_mouseState[0] = x; m_mouseState[1] = y; m_mouseState[2] = click_x; m_mouseState[3] = click_y;
}

void ShaderEffect::SetDisplayResolution(int width, int height) {
    m_currentDisplayWidth = width; m_currentDisplayHeight = height;
}

std::string ShaderEffect::LoadShaderSourceFile(const std::string& filePath, std::string& errorMsg) {
    errorMsg.clear();
    std::ifstream shaderFile;
    std::stringstream shaderStream;
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        shaderFile.open(filePath);
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
    } catch (const std::ifstream::failure& e) {
        errorMsg = "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " + filePath + " - " + e.what();
        return "";
    }
    return shaderStream.str();
}

static std::string LoadPassthroughVertexShaderSource(std::string& errorMsg) {
    const char* vsPath = "shaders/passthrough.vert";
    std::ifstream vsFile;
    std::stringstream vsStream;
    vsFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        vsFile.open(vsPath);
        vsStream << vsFile.rdbuf();
        vsFile.close();
    } catch (std::ifstream::failure& e) {
        errorMsg = "CRITICAL: Vertex shader (" + std::string(vsPath) + ") load failed: " + e.what();
        return "";
    }
    errorMsg.clear();
    return vsStream.str();
}

static GLuint CompileShader(const char* source, GLenum type, std::string& errorLogString) {
    errorLogString.clear();
    if (!source || std::string(source).empty()) {
        errorLogString = "ERROR::SHADER::COMPILE_EMPTY_SOURCE Type: " + std::to_string(type);
        return 0;
    }
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 0 ? logLength + 1 : 257);
        glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()-1), NULL, infoLog.data());
        errorLogString = "ERROR::SHADER::COMPILE_FAIL (" + std::string(type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") + ")\n" + infoLog.data();
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint CreateShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString) {
    errorLogString.clear();
    if (vertexShaderID == 0 || fragmentShaderID == 0) {
        errorLogString = "ERROR::PROGRAM::LINK_INVALID_SHADER_ID - One or both shaders failed to compile.";
        if (vertexShaderID != 0) glDeleteShader(vertexShaderID);
        if (fragmentShaderID != 0) glDeleteShader(fragmentShaderID);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShaderID);
    glAttachShader(program, fragmentShaderID);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 0 ? logLength + 1 : 257);
        glGetProgramInfoLog(program, static_cast<GLsizei>(infoLog.size()-1), NULL, infoLog.data());
        errorLogString = "ERROR::PROGRAM::LINK_FAIL\n" + std::string(infoLog.data());
        glDeleteProgram(program);
        program = 0;
    }

    glDetachShader(program, vertexShaderID);
    glDetachShader(program, fragmentShaderID);
    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);

    return program;
}

void ShaderEffect::RenderUI() {
    if (!m_shaderLoaded && !m_compileErrorLog.empty()) {
        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Shader Error:");
        ImGui::TextWrapped("%s", m_compileErrorLog.c_str());
    }

    ImGui::Text("Effect: %s", name.c_str());
    ImGui::Text("Source: %s", m_shaderFilePath.c_str());

    bool currentShadertoyMode = m_isShadertoyMode;
    if (ImGui::Checkbox("Shadertoy Mode", &currentShadertoyMode)) {
        SetShadertoyMode(currentShadertoyMode);
        ApplyShaderCode(m_shaderSourceCode);
    }
    ImGui::Separator();

    if (m_isShadertoyMode) {
        RenderShadertoyParamsUI();
        if (ImGui::CollapsingHeader("Shader Uniforms (from metadata)##EffectSTUniforms")) {
            RenderMetadataUniformsUI();
        }
    } else {
        RenderNativeParamsUI();
    }

    if (ImGui::CollapsingHeader("Shader Defines##EffectDefines")) {
        RenderDefineControlsUI();
    }

    if (ImGui::CollapsingHeader("Global Constants##EffectConsts")) {
        RenderConstControlsUI();
    }
}

void ShaderEffect::RenderNativeParamsUI() {
    ImGui::Text("Native Shader Parameters:");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Colour Parameters##EffectNativeColours", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Object Colour##Effect", m_objectColor);
        ImGui::ColorEdit3("Colour Mod##Effect", m_colorMod);
        RenderColorCycleUI();
    }
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Patterns of Time and Space##EffectNativePatterns")) {
        ImGui::SliderFloat("Scale##Effect", &m_scale, 0.1f, 3.0f);
        ImGui::SliderFloat("Pattern Scale##Effect", &m_patternScale, 0.1f, 10.0f);
        ImGui::SliderFloat("Time Speed##Effect", &m_timeSpeed, 0.0f, 5.0f);
    }
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Camera Controls##EffectNativeCamera")) {
        ImGui::DragFloat3("Position##EffectCam", m_cameraPosition, 0.1f);
        ImGui::DragFloat3("Target##EffectCam", m_cameraTarget, 0.1f);
        ImGui::SliderFloat("FOV##EffectCam", &m_cameraFOV, 15.0f, 120.0f);
    }
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Lighting Controls##EffectNativeLighting")) {
        ImGui::DragFloat3("Light Pos##EffectLight", m_lightPosition, 0.1f);
        ImGui::ColorEdit3("Light Colour##EffectLight", m_lightColor);
    }
}

void ShaderEffect::RenderShadertoyParamsUI() {
    ImGui::Text("Shadertoy User Parameters:");
    ImGui::SliderFloat("iUserFloat1##Effect", &m_iUserFloat1, 0.0f, 1.0f);
    ImGui::ColorEdit3("iUserColour1##Effect", m_iUserColor1);
}

void ShaderEffect::RenderMetadataUniformsUI() {
    for (size_t i = 0; i < m_shadertoyUniformControls.size(); ++i) {
        auto& control = m_shadertoyUniformControls[i];
        std::string label = control.metadata.value("label", control.name);
        ImGui::PushID(static_cast<int>(i));

        if (control.glslType == "float") {
            ImGui::SliderFloat(label.c_str(), &control.fValue, control.metadata.value("min", 0.0f), control.metadata.value("max", 1.0f));
        } else if (control.glslType == "vec3" && control.isColor) {
            ImGui::ColorEdit3(label.c_str(), control.v3Value);
        } // ... other types
        ImGui::PopID();
    }
}

void ShaderEffect::RenderDefineControlsUI() {
    if (m_defineControls.empty()) {
        ImGui::TextDisabled(" (No defines detected)");
        return;
    }
    for (size_t i = 0; i < m_defineControls.size(); ++i) {
        ImGui::PushID(static_cast<int>(i) + 1000);
        bool defineEnabledState = m_defineControls[i].isEnabled;
        if (ImGui::Checkbox(m_defineControls[i].name.c_str(), &defineEnabledState)) {
            std::string modifiedCode = m_shaderParser.ToggleDefineInString(m_shaderSourceCode, m_defineControls[i].name, defineEnabledState, m_defineControls[i].originalValueString);
            if (!modifiedCode.empty()) {
                ApplyShaderCode(modifiedCode);
            }
        }
        ImGui::PopID();
    }
}

void ShaderEffect::RenderConstControlsUI() {
    // Implementation for const controls UI
}

void ShaderEffect::RenderColorCycleUI() {
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Color Cycling##EffectNativeColorCycle")) {
        ImGui::Checkbox("Enable Color Cycling", &m_colorCycleState.isEnabled);
        if (m_colorCycleState.isEnabled) {
            ImGui::SliderFloat("Speed", &m_colorCycleState.speed, 0.1f, 10.0f);
            const char* gradients[] = { "Rainbow", "Fire", "Ice" };
            ImGui::Combo("Gradient", &m_colorCycleState.currentGradient, gradients, IM_ARRAYSIZE(gradients));
        }
    }
}

void ShaderEffect::GetGradientColor(float t, float* outColor) {
    t = fmod(t, 1.0f); // Wrap time to 0-1 range
    switch (m_colorCycleState.currentGradient) {
        case 0: // Rainbow
            outColor[0] = 0.5f * (1.0f + sin(t * 2.0f * 3.14159f));
            outColor[1] = 0.5f * (1.0f + sin((t + 0.333f) * 2.0f * 3.14159f));
            outColor[2] = 0.5f * (1.0f + sin((t + 0.667f) * 2.0f * 3.14159f));
            break;
        case 1: // Fire
            outColor[0] = pow(t, 0.5f);
            outColor[1] = pow(t, 2.0f);
            outColor[2] = pow(t, 8.0f);
            break;
        case 2: // Ice
            outColor[0] = pow(t, 8.0f);
            outColor[1] = pow(t, 2.0f);
            outColor[2] = pow(t, 0.5f);
            break;
    }
}


void ShaderEffect::CompileAndLinkShader() {
    if (m_shaderProgram != 0) glDeleteProgram(m_shaderProgram);
    m_compileErrorLog.clear();

    std::string vsError, fsError, linkError;
    std::string vsSource = LoadPassthroughVertexShaderSource(vsError);
    if (vsSource.empty()) {
        m_compileErrorLog = "Vertex Shader Load Error: " + vsError;
        return;
    }

    std::string finalFragmentCode = m_shaderSourceCode;
    if (m_isShadertoyMode && m_shaderSourceCode.find("void main()") == std::string::npos) {
        finalFragmentCode = 
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec3 iResolution;\n"
            "uniform float iTime;\n"
            "uniform float iTimeDelta;\n"
            "uniform int iFrame;\n"
            "uniform vec4 iMouse;\n"
            "uniform float iUserFloat1;\n"
            "uniform vec3 iUserColor1;\n"
            + m_shaderSourceCode +
            "\nvoid main() {\n"
            "    mainImage(FragColor, gl_FragCoord.xy);\n"
            "}\n";
    }

    GLuint vertexShader = CompileShader(vsSource.c_str(), GL_VERTEX_SHADER, vsError);
    if (vertexShader == 0) {
        m_compileErrorLog = "Vertex Shader Compile Error:\n" + vsError;
        return;
    }

    GLuint fragmentShader = CompileShader(finalFragmentCode.c_str(), GL_FRAGMENT_SHADER, fsError);
    if (fragmentShader == 0) {
        m_compileErrorLog = "Fragment Shader Compile Error:\n" + fsError;
        glDeleteShader(vertexShader);
        return;
    }

    m_shaderProgram = CreateShaderProgram(vertexShader, fragmentShader, linkError);
    if (m_shaderProgram == 0) {
        m_compileErrorLog += (fsError.empty() ? "" : ("Fragment Shader Log:\n" + fsError + "\n")) +
                             "Shader Link Error:\n" + linkError;
    }
}

void ShaderEffect::FetchUniformLocations() {
    if (m_shaderProgram == 0) return;
    m_iChannel0SamplerLoc = glGetUniformLocation(m_shaderProgram, "iChannel0");

    if (m_isShadertoyMode) {
        m_iResolutionLocation = glGetUniformLocation(m_shaderProgram, "iResolution");
        m_iTimeLocation = glGetUniformLocation(m_shaderProgram, "iTime");
        m_iTimeDeltaLocation = glGetUniformLocation(m_shaderProgram, "iTimeDelta");
        m_iFrameLocation = glGetUniformLocation(m_shaderProgram, "iFrame");
        m_iMouseLocation = glGetUniformLocation(m_shaderProgram, "iMouse");
        m_iUserFloat1Loc = glGetUniformLocation(m_shaderProgram, "iUserFloat1");
        m_iUserColor1Loc = glGetUniformLocation(m_shaderProgram, "iUserColor1");
        for (auto& control : m_shadertoyUniformControls) {
            control.location = glGetUniformLocation(m_shaderProgram, control.name.c_str());
        }
    } else {
        m_iResolutionLocation = glGetUniformLocation(m_shaderProgram, "iResolution");
        m_iTimeLocation = glGetUniformLocation(m_shaderProgram, "iTime");
        m_uObjectColorLoc = glGetUniformLocation(m_shaderProgram, "u_objectColor");
        m_uScaleLoc = glGetUniformLocation(m_shaderProgram, "u_scale");
        m_uTimeSpeedLoc = glGetUniformLocation(m_shaderProgram, "u_timeSpeed");
        m_uColorModLoc = glGetUniformLocation(m_shaderProgram, "u_colorMod");
        m_uPatternScaleLoc = glGetUniformLocation(m_shaderProgram, "u_patternScale");
        m_uCamPosLoc = glGetUniformLocation(m_shaderProgram, "u_camPos");
        m_uCamTargetLoc = glGetUniformLocation(m_shaderProgram, "u_camTarget");
        m_uCamFOVLoc = glGetUniformLocation(m_shaderProgram, "u_camFOV");
        m_uLightPosLoc = glGetUniformLocation(m_shaderProgram, "u_lightPosition");
        m_uLightColorLoc = glGetUniformLocation(m_shaderProgram, "u_lightColor");
    }
    m_iAudioAmpLoc = glGetUniformLocation(m_shaderProgram, "iAudioAmp");
}

void ShaderEffect::ParseShaderControls() {
    if (m_shaderSourceCode.empty()) return;
    m_shaderParser.ScanAndPrepareDefineControls(m_shaderSourceCode);
    m_defineControls = m_shaderParser.GetDefineControls();
    m_shaderParser.ScanAndPrepareConstControls(m_shaderSourceCode);
    m_constControls = m_shaderParser.GetConstControls();
    if (m_isShadertoyMode) {
        m_shaderParser.ScanAndPrepareUniformControls(m_shaderSourceCode);
        m_shadertoyUniformControls = m_shaderParser.GetUniformControls();
    } else {
        m_shadertoyUniformControls.clear();
    }
}

void ShaderEffect::SetSourceFilePath(const std::string& path) {
    m_shaderFilePath = path;
}

const std::string& ShaderEffect::GetSourceFilePath() const {
    return m_shaderFilePath;
}

nlohmann::json ShaderEffect::Serialize() const {
    nlohmann::json j;
    j["type"] = "ShaderEffect";
    j["name"] = name;
    j["startTime"] = startTime;
    j["endTime"] = endTime;
    j["sourceFilePath"] = m_shaderFilePath;
    if (m_shaderFilePath.empty() || m_shaderFilePath == "dynamic_source" || m_shaderFilePath.rfind("shadertoy://", 0) == 0) {
        j["sourceCode"] = m_shaderSourceCode;
    }
    j["isShadertoyMode"] = m_isShadertoyMode;
    j["colorCycleState"] = m_colorCycleState;

    j["nativeParams"]["objectColor"] = {m_objectColor[0], m_objectColor[1], m_objectColor[2]};
    // ... serialize other native params
    
    return j;
}

void ShaderEffect::Deserialize(const nlohmann::json& data) {
    name = data.value("name", "Untitled");
    startTime = data.value("startTime", 0.0f);
    endTime = data.value("endTime", 10.0f);
    m_shaderFilePath = data.value("sourceFilePath", "");
    if (data.contains("sourceCode")) {
        m_shaderSourceCode = data.at("sourceCode").get<std::string>();
    }
    m_isShadertoyMode = data.value("isShadertoyMode", false);
    if (data.contains("colorCycleState")) {
        m_colorCycleState = data["colorCycleState"].get<ColorCycleState>();
    }

    if (data.contains("nativeParams")) {
        const auto& np = data["nativeParams"];
        if (np.contains("objectColor")) {
            np.at("objectColor").get_to(m_objectColor);
        }
        // ... deserialize other native params
    }
}

void ShaderEffect::ResetParameters() {
    // Reset native params
    m_objectColor[0] = 0.8f; m_objectColor[1] = 0.9f; m_objectColor[2] = 1.0f;
    m_scale = 1.0f;
    m_timeSpeed = 1.0f;
    m_colorMod[0] = 0.1f; m_colorMod[1] = 0.1f; m_colorMod[2] = 0.2f;
    m_patternScale = 1.0f;
    m_cameraPosition[0] = 0.0f; m_cameraPosition[1] = 1.0f; m_cameraPosition[2] = -3.0f;
    m_cameraTarget[0] = 0.0f; m_cameraTarget[1] = 0.0f; m_cameraTarget[2] = 0.0f;
    m_cameraFOV = 60.0f;
    m_lightPosition[0] = 2.0f; m_lightPosition[1] = 3.0f; m_lightPosition[2] = -2.0f;
    m_lightColor[0] = 1.0f; m_lightColor[1] = 1.0f; m_lightColor[2] = 0.9f;
    
    // Reset Shadertoy params
    m_iUserFloat1 = 0.5f;
    m_iUserColor1[0] = 0.2f; m_iUserColor1[1] = 0.5f; m_iUserColor1[2] = 0.8f;

    // Reset color cycle state
    m_colorCycleState = ColorCycleState();

    if (!m_shaderSourceCode.empty()) {
        ApplyShaderCode(m_shaderSourceCode);
    }
}