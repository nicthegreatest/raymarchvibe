#include "ShaderEffect.h"
#include "Renderer.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <cmath> // For sin, cos in color cycling
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <regex>
#include <glm/gtc/type_ptr.hpp>

// Define the static member
GLuint ShaderEffect::s_dummyTexture = 0;

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
      m_audioAmp(0.0f),
      m_iAudioAmpLoc(-1),
      m_iAudioBandsAttLoc(-1),
      m_shaderParser(),
      m_iChannel0SamplerLoc(-1),
      m_iChannel1SamplerLoc(-1),
      m_iChannel2SamplerLoc(-1),
      m_iChannel3SamplerLoc(-1),
      m_fboID(0),
      m_fboTextureID(0),
      m_rboID(0),
      m_fboWidth(initialWidth),
      m_fboHeight(initialHeight),
      m_lastWriteTime{},
      m_debugLogged(false)
{
    if (m_isShadertoyMode) {
        m_inputs.resize(4, nullptr);
    } else {
        m_inputs.resize(1, nullptr);
    }

    std::fill_n(m_mouseState, 4, 0.0f);
    m_audioBands.fill(0.0f);

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
        m_fboID = 0;
        m_fboTextureID = 0;
        m_rboID = 0;
    } else {
        std::cout << "SUCCESS::FRAMEBUFFER:: Framebuffer for '" << name << "' (ID: " << m_fboID << ") is complete. Texture ID: " << m_fboTextureID << std::endl;
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
    try {
        m_lastWriteTime = std::filesystem::last_write_time(filePath);
    } catch (const std::filesystem::filesystem_error& e) {
        m_compileErrorLog = "Failed to get file timestamp: " + std::string(e.what());
        // Not returning false, as the file was read successfully
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
    bool oldMode = m_isShadertoyMode;
    if (m_shaderSourceCode.find("mainImage") != std::string::npos) {
        m_isShadertoyMode = true;
    } else {
        m_isShadertoyMode = false;
    }

    if (oldMode != m_isShadertoyMode) {
        if (m_isShadertoyMode) {
            m_inputs.resize(4, nullptr);
        } else {
            m_inputs.resize(1, nullptr);
        }
    }

    m_compileErrorLog.clear();
    CompileAndLinkShader();
    if (m_shaderProgram != 0) {
        ParseShaderControls();
        FetchUniformLocations();
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
        if (m_isShadertoyMode) {
            m_inputs.resize(4, nullptr);
        } else {
            m_inputs.resize(1, nullptr);
        }
        if (m_shaderProgram != 0) {
            FetchUniformLocations();
            ParseShaderControls();
        }
    }
}

void ShaderEffect::Update(float currentTime) {
    float speed = 1.0f;
    for (auto& control : m_shadertoyUniformControls) {
        if (control.name == "u_speed") {
            speed = control.fValue;
        }
        if (control.smooth) {
            if (control.glslType == "float") {
                control.fCurrentValue += (control.fValue - control.fCurrentValue) * 0.05f;
            }
            // Add smoothing for other types here if needed
        }
    }
    m_internalTime += m_deltaTime * speed;
    m_time = m_internalTime;

    if (m_colorCycleState.isEnabled) {
        m_colorCycleState.cycleTime += m_deltaTime * m_colorCycleState.speed;
        for (auto& control : m_shadertoyUniformControls) {
            if (control.name == "u_objectColor") { // Or another designated target uniform for cycling
                GetGradientColor(m_colorCycleState.cycleTime, control.v3Value);
                break;
            }
        }
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

    // Bind inputs, using a dummy texture for any unbound input channels
    GLint samplerLocs[] = {m_iChannel0SamplerLoc, m_iChannel1SamplerLoc, m_iChannel2SamplerLoc, m_iChannel3SamplerLoc};
    for (int i = 0; i < 4; ++i) {
        if (samplerLocs[i] != -1) {
            glActiveTexture(GL_TEXTURE0 + i);
            if (static_cast<size_t>(i) < m_inputs.size() && m_inputs[i] != nullptr) {
                GLuint inputTextureID = m_inputs[i]->GetOutputTexture();
                glBindTexture(GL_TEXTURE_2D, inputTextureID != 0 ? inputTextureID : s_dummyTexture);
            } else {
                glBindTexture(GL_TEXTURE_2D, s_dummyTexture);
            }
            glUniform1i(samplerLocs[i], i);
        }
    }

    // Set uniforms from parsed controls
    for (auto& control : m_shadertoyUniformControls) {
        if (control.location != -1) {
            if (control.glslType == "float") {
                if (control.smooth) {
                    glUniform1f(control.location, control.fCurrentValue);
                } else {
                    glUniform1f(control.location, control.fValue);
                }
            } else if (control.glslType == "int") {
                glUniform1i(control.location, control.iValue);
            } else if (control.glslType == "vec2") {
                glUniform2fv(control.location, 1, control.v2Value);
            } else if (control.glslType == "vec3") {
                glUniform3fv(control.location, 1, control.v3Value);
            } else if (control.glslType == "vec4") {
                glUniform4fv(control.location, 1, control.v4Value);
            } else if (control.glslType == "bool") {
                glUniform1i(control.location, control.bValue);
            }
        }
    }

    if (m_isShadertoyMode) {
        if (m_iResolutionLocation != -1) glUniform3f(m_iResolutionLocation, (float)m_fboWidth, (float)m_fboHeight, (float)m_fboWidth / (float)m_fboHeight);
        if (m_iTimeLocation != -1) glUniform1f(m_iTimeLocation, m_time);
        if (m_iTimeDeltaLocation != -1) glUniform1f(m_iTimeDeltaLocation, m_deltaTime);
        if (m_iFrameLocation != -1) glUniform1i(m_iFrameLocation, m_frameCount);
        if (m_iMouseLocation != -1) glUniform4fv(m_iMouseLocation, 1, m_mouseState);
    } else {
        if (m_iResolutionLocation != -1) glUniform2f(m_iResolutionLocation, (float)m_fboWidth, (float)m_fboHeight);
        if (m_iTimeLocation != -1) glUniform1f(m_iTimeLocation, m_time);
    }
    if (m_iAudioAmpLoc != -1) {
        glUniform1f(m_iAudioAmpLoc, m_audioAmp);
    }
    if (m_iAudioBandsAttLoc != -1) {
        glUniform4fv(m_iAudioBandsAttLoc, 1, m_audioBands.data());
    }

    if (m_iCameraPositionLocation != -1) {
        glUniform3fv(m_iCameraPositionLocation, 1, glm::value_ptr(m_cameraPosition));
    }
    if (m_iCameraMatrixLocation != -1) {
        glUniformMatrix4fv(m_iCameraMatrixLocation, 1, GL_FALSE, glm::value_ptr(m_cameraMatrix));
    }

    if (m_iLightPositionLocation != -1) {
        glUniform3fv(m_iLightPositionLocation, 1, glm::value_ptr(m_lightPosition));
    }

    Renderer::RenderQuad();
}

void ShaderEffect::SetAudioAmplitude(float amp) {
    m_audioAmp = amp;
}

void ShaderEffect::SetAudioBands(const std::array<float, 4>& bands) {
    m_audioBands = bands;
}

void ShaderEffect::SetCameraState(const glm::vec3& pos, const glm::mat4& viewMatrix) {
    m_cameraPosition = pos;
    m_cameraMatrix = viewMatrix;
}

void ShaderEffect::SetLightPosition(const glm::vec3& pos) {
    m_lightPosition = pos;
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

    if (ImGui::CollapsingHeader("Inputs##EffectInputs")) {
        for (size_t i = 0; i < m_inputs.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("iChannel%zu", i);
            ImGui::SameLine();

            if (m_inputs[i]) {
                // Input is connected
                ImGui::Text("Connected: %s", m_inputs[i]->name.c_str());
                ImGui::SameLine();
                if (ImGui::Button("Unlink")) {
                    m_inputs[i] = nullptr;
                }
                GLuint textureID = m_inputs[i]->GetOutputTexture();
                if (textureID != 0) {
                    ImGui::Image((void*)(intptr_t)textureID, ImVec2(64, 64), ImVec2(0,1), ImVec2(1,0)); // Flipped UVs for OpenGL texture
                }
            } else {
                // Input is not connected
                if (ImGui::Button("Load Texture")) {
                    SetChannelPendingTextureLoad(i);
                    ImGuiFileDialog::Instance()->OpenDialog("LoadTextureForIChannelDlgKey", "Choose Texture File", ".png,.jpg,.jpeg,.bmp,.tga", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
                }
            }
            ImGui::PopID();
            ImGui::Separator();
        }
    }

    if (ImGui::CollapsingHeader("Parsed Uniforms##EffectParsedUniforms", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderParsedUniformsUI();
    }

    if (ImGui::CollapsingHeader("Shader Defines##EffectDefines", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderDefineControlsUI();
    }

    if (ImGui::CollapsingHeader("Global Constants##EffectConsts")) {
        RenderConstControlsUI();
    }
}

bool ShaderEffect::IsShadertoyMode() const {
    return m_isShadertoyMode;
}

// Obsolete function, replaced by data-driven UI from shader parsing.

void ShaderEffect::RenderParsedUniformsUI() {
    if (m_shadertoyUniformControls.empty()) {
        ImGui::TextDisabled(" (No parsed uniforms detected)");
        return;
    }

    for (size_t i = 0; i < m_shadertoyUniformControls.size(); ++i) {
        auto& control = m_shadertoyUniformControls[i];
        std::string label = control.metadata.value("label", control.name);
        ImGui::PushID(static_cast<int>(i));

        float step = control.metadata.value("step", 0.01f);

        if (control.glslType == "bool") {
            ImGui::Checkbox(label.c_str(), &control.bValue);
        } else if (control.glslType == "float") {
            ImGui::SliderFloat(label.c_str(), &control.fValue, control.metadata.value("min", 0.0f), control.metadata.value("max", 1.0f));
        } else if (control.glslType == "int") {
            ImGui::SliderInt(label.c_str(), &control.iValue, control.metadata.value("min", 0), control.metadata.value("max", 100));
        } else if (control.glslType == "vec2") {
            ImGui::DragFloat2(label.c_str(), control.v2Value, step);
        } else if (control.glslType == "vec3") {
            if (control.isColor) {
                RenderEnhancedColorControl(control, label, 3);
            } else {
                ImGui::DragFloat3(label.c_str(), control.v3Value, step);
            }
        } else if (control.glslType == "vec4") {
            if (control.isColor) {
                RenderEnhancedColorControl(control, label, 4);
            } else {
                ImGui::DragFloat4(label.c_str(), control.v4Value, step);
            }
        }

        ImGui::PopID();
    }
}

void ShaderEffect::RenderDefineControlsUI() {
    if (m_defineControls.empty()) {
        ImGui::TextDisabled(" (No defines detected)");
        return;
    }
    for (size_t i = 0; i < m_defineControls.size(); ++i) {
        auto& control = m_defineControls[i];
        ImGui::PushID(static_cast<int>(i) + 1000);

        if (control.hasValue) {
            bool valueChanged = false;
            if (!control.metadata.is_null() && control.metadata.value("widget", "") == "slider") {
                std::string label = control.metadata.value("label", control.name);
                float min_val = control.metadata.value("min", 0.0f);
                float max_val = control.metadata.value("max", 1.0f);
                valueChanged = ImGui::SliderFloat(label.c_str(), &control.floatValue, min_val, max_val);
            } else {
                valueChanged = ImGui::InputFloat(control.name.c_str(), &control.floatValue);
            }

            if (valueChanged) {
                std::string modifiedCode = m_shaderParser.UpdateDefineValueInString(m_shaderSourceCode, control.name, control.floatValue);
                if (!modifiedCode.empty()) {
                    ApplyShaderCode(modifiedCode);
                }
            }
        } else {
            bool defineEnabledState = control.isEnabled;
            if (ImGui::Checkbox(control.name.c_str(), &defineEnabledState)) {
                std::string modifiedCode = m_shaderParser.ToggleDefineInString(m_shaderSourceCode, control.name, defineEnabledState, control.originalValueString);
                if (!modifiedCode.empty()) {
                    ApplyShaderCode(modifiedCode);
                }
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

void ShaderEffect::RenderEnhancedColorControl(ShaderToyUniformControl& control, const std::string& label, int components) {
    if (!control.isPalette) {
        // Backward compatibility: render as standard color picker
        if (components == 3) {
            ImGui::ColorEdit3(label.c_str(), control.v3Value);
        } else {
            ImGui::ColorEdit4(label.c_str(), control.v4Value);
        }
        return;
    }

    // Enhanced palette-based color picker
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.25f, 0.5f, 0.7f));
    bool expanded = ImGui::CollapsingHeader((label + " [Palette]").c_str());
    ImGui::PopStyleColor();

    if (expanded) {
        // Determine if this is a secondary color control (should have Sync option)
        // Use the same detection logic as the parser - check paletteControlIndex
        bool isSecondaryControl = false;
        std::string detectionMethod = "INDEX_FALLBACK";
        std::string baseName = control.name;

        // Phase 1: Semantic name detection (matches parser logic)
        if (baseName.find("Secondary") != std::string::npos ||
            baseName.find("Tertiary") != std::string::npos ||
            baseName.find("Accent") != std::string::npos ||
            baseName.find("Highlight") != std::string::npos ||
            baseName.find("_secondary") != std::string::npos ||
            baseName.find("_tertiary") != std::string::npos ||
            baseName.find("_accent") != std::string::npos ||
            baseName.find("_highlight") != std::string::npos) {
            isSecondaryControl = true;
            detectionMethod = "SEMANTIC_NAME";
        }

        // Phase 2: Index-based fallback (matches parser logic)
        int paletteIndex = control.metadata.value("paletteControlIndex", 0);
        if (!isSecondaryControl && paletteIndex > 0) {
            isSecondaryControl = true;  // 2nd and subsequent palette controls are secondary
            detectionMethod = "INDEX_FALLBACK";
        }

        // Determine if this is a secondary color control (should have Sync option)
        // Use the same detection logic as the parser - check paletteControlIndex
        if (baseName.find("Secondary") != std::string::npos ||
            baseName.find("Tertiary") != std::string::npos ||
            baseName.find("Accent") != std::string::npos ||
            baseName.find("Highlight") != std::string::npos ||
            baseName.find("_secondary") != std::string::npos ||
            baseName.find("_tertiary") != std::string::npos ||
            baseName.find("_accent") != std::string::npos ||
            baseName.find("_highlight") != std::string::npos) {
            isSecondaryControl = true;
        }

        // Phase 2: Index-based fallback (matches parser logic)
        if (!isSecondaryControl && paletteIndex > 0) {
            isSecondaryControl = true;  // 2nd and subsequent palette controls are secondary
        }

        // Mode selector - different options for primary vs secondary controls
        if (isSecondaryControl) {
            // Secondary controls: ALWAYS all 3 options (Individual | Palette | Sync)
            const char* currentPreview = nullptr;
            if (control.paletteMode == 0) currentPreview = "Individual";
            else if (control.paletteMode == 1) currentPreview = "Palette";
            else if (control.paletteMode == 2) currentPreview = "Sync";
            else currentPreview = "Invalid"; // Fallback should not happen

            if (ImGui::BeginCombo(("Mode##" + control.name).c_str(), currentPreview)) {
                if (ImGui::Selectable("Individual", control.paletteMode == 0)) control.paletteMode = 0;
                if (ImGui::Selectable("Palette", control.paletteMode == 1)) control.paletteMode = 1;
                if (ImGui::Selectable("Sync", control.paletteMode == 2)) control.paletteMode = 2;
                ImGui::EndCombo();
            }
        } else {
            // Primary controls: Individual | Palette only (no Sync option)
            // Clamp to valid range for primary controls
            if (control.paletteMode >= 2) control.paletteMode = 1;
            if (ImGui::BeginCombo(("Mode##" + control.name).c_str(), control.paletteMode == 0 ? "Individual" : "Palette")) {
                if (ImGui::Selectable("Individual", control.paletteMode == 0)) control.paletteMode = 0;
                if (ImGui::Selectable("Palette", control.paletteMode == 1)) control.paletteMode = 1;
                ImGui::EndCombo();
            }
        }

        if (control.paletteMode == 0) {
            // Individual color picking mode
            if (components == 3) {
                ImGui::ColorEdit3(("Color##" + control.name).c_str(), control.v3Value);
            } else {
                ImGui::ColorEdit4(("Color##" + control.name).c_str(), control.v4Value);
            }
        } else if (control.paletteMode == 1) {
            // Palette generation mode
            // Harmony type selector
            const char* harmonies[] = {
                "Monochromatic",
                "Complementary",
                "Triadic",
                "Analogous",
                "Split-Complementary",
                "Square"
            };
            
            int oldHarmony = control.selectedHarmonyType;
            ImGui::Combo(("Harmony##" + control.name).c_str(), &control.selectedHarmonyType, harmonies, IM_ARRAYSIZE(harmonies));

            // Base color picker
            bool baseColorChanged = false;
            if (components == 3) {
                baseColorChanged = ImGui::ColorEdit3(("Base Color##" + control.name).c_str(), control.v3Value);
            } else {
                baseColorChanged = ImGui::ColorEdit4(("Base Color##" + control.name).c_str(), control.v4Value);
            }



            // Generate palette if harmony type changed or base color changed
            if (oldHarmony != control.selectedHarmonyType || baseColorChanged || control.generatedPalette.empty()) {
                glm::vec3 baseRgb(control.v3Value[0], control.v3Value[1], control.v3Value[2]);
                ColorPaletteGenerator::HarmonyType harmonyType = static_cast<ColorPaletteGenerator::HarmonyType>(control.selectedHarmonyType);
                control.generatedPalette = ColorPaletteGenerator::GeneratePalette(baseRgb, harmonyType, 5);
            }

            // Gradient mode toggle
            ImGui::Checkbox(("Gradient Mode##" + control.name).c_str(), &control.gradientMode);

            // Generate gradient if needed
            if (control.gradientMode && (control.gradientColors.empty() || oldHarmony != control.selectedHarmonyType || baseColorChanged)) {
                control.gradientColors = ColorPaletteGenerator::GenerateGradient(control.generatedPalette, 10);
            }


            // Make palette clickable for editing individual colors
            ImGui::Text("Palette Preview (click to edit):");
            const std::vector<glm::vec3>& displayPalette = control.gradientMode ? control.gradientColors : control.generatedPalette;

            if (!displayPalette.empty()) {
                float colorBoxSize = ImGui::GetContentRegionAvail().x / displayPalette.size();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                float boxHeight = 30.0f;
                ImVec2 mousePos = ImGui::GetMousePos();

                static int hoveredIndex = -1;
                static size_t clickedIndex = 0;

                hoveredIndex = -1;
                bool segmentClicked = false;

                // Check for hover and clicks
                for (size_t j = 0; j < displayPalette.size(); ++j) {
                    ImVec2 boxMin(cursorPos.x + j * colorBoxSize, cursorPos.y);
                    ImVec2 boxMax(boxMin.x + colorBoxSize, boxMin.y + boxHeight);

                    if (mousePos.x >= boxMin.x && mousePos.x <= boxMax.x &&
                        mousePos.y >= boxMin.y && mousePos.y <= boxMax.y) {
                        hoveredIndex = static_cast<int>(j);
                        if (ImGui::IsMouseClicked(0) && !ImGui::IsPopupOpen("ColorPicker")) {
                            clickedIndex = j;
                            segmentClicked = true;
                        }
                    }
                }

                // Render the preview with hover highlighting
                for (size_t j = 0; j < displayPalette.size(); ++j) {
                    ImVec2 boxMin(cursorPos.x + j * colorBoxSize, cursorPos.y);
                    ImVec2 boxMax(boxMin.x + colorBoxSize, boxMin.y + boxHeight);
                    ImU32 color = ImGui::GetColorU32(ImVec4(
                        displayPalette[j].x,
                        displayPalette[j].y,
                        displayPalette[j].z,
                        1.0f
                    ));

                    // Highlight hovered segment
                    if (static_cast<int>(j) == hoveredIndex) {
                        // Lighten the color for hover effect
                        float h, s, v;
                        ImGui::ColorConvertRGBtoHSV(displayPalette[j].x, displayPalette[j].y, displayPalette[j].z, h, s, v);
                        v = std::min(v + 0.2f, 1.0f);  // Increase brightness
                        float r, g, b;
                        ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
                        color = ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));
                    }

                    drawList->AddRectFilled(boxMin, boxMax, color);
                    ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    if (static_cast<int>(j) == hoveredIndex) {
                        // Thicker border for hover
                        drawList->AddRect(boxMin, boxMax, borderColor, 0.0f, 0, 2.0f);
                    } else {
                        drawList->AddRect(boxMin, boxMax, borderColor);
                    }
                }

                ImGui::Dummy(ImVec2(0, boxHeight + 5.0f));

                // Handle color editing popup
                if (segmentClicked) {
                    ImGui::OpenPopup("ColorPicker");
                }

                if (ImGui::BeginPopup("ColorPicker")) {
                    std::vector<glm::vec3>& editablePalette = control.gradientMode ? control.gradientColors : control.generatedPalette;
                    float tempColor[3] = {editablePalette[clickedIndex].x, editablePalette[clickedIndex].y, editablePalette[clickedIndex].z};

                    ImGui::Text("Edit Segment %zu", clickedIndex + 1);
                    if (ImGui::ColorPicker3("##palettecolor", tempColor)) {
                        editablePalette[clickedIndex] = glm::vec3(tempColor[0], tempColor[1], tempColor[2]);

                        // Auto-reapply palette to uniforms after color change
                        std::vector<size_t> colorUniformIndices;
                        for (size_t j = 0; j < m_shadertoyUniformControls.size(); ++j) {
                            if (m_shadertoyUniformControls[j].isColor && m_shadertoyUniformControls[j].isPalette) {
                                colorUniformIndices.push_back(j);
                            }
                        }

                        // Distribute palette colors to available uniforms using even sampling across the gradient
                        size_t paletteSize = displayPalette.size();
                        size_t numUniforms = colorUniformIndices.size();
                        for (size_t j = 0; j < numUniforms; ++j) {
                            size_t paletteIndex = (numUniforms > 1) ?
                                (j * (paletteSize - 1)) / (numUniforms - 1) : 0;
                            paletteIndex = std::min(paletteIndex, paletteSize - 1);

                            size_t idx = colorUniformIndices[j];
                            m_shadertoyUniformControls[idx].v3Value[0] = displayPalette[paletteIndex].x;
                            m_shadertoyUniformControls[idx].v3Value[1] = displayPalette[paletteIndex].y;
                            m_shadertoyUniformControls[idx].v3Value[2] = displayPalette[paletteIndex].z;
                            if (m_shadertoyUniformControls[idx].glslType == "vec4") {
                                m_shadertoyUniformControls[idx].v4Value[3] = 1.0f;
                            }
                        }
                    }

                    ImGui::Separator();
                    if (ImGui::Button("Close")) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
        } else if (control.paletteMode == 2) {
            // Sync mode - automatically sync from primary gradient
            ImGui::Text("Synced to primary gradient");

            // Find the primary control that generates gradients
            const ShaderToyUniformControl* primaryControl = nullptr;
            for (const auto& otherControl : m_shadertoyUniformControls) {
                if (&otherControl != &control && otherControl.isColor && otherControl.paletteMode == 1 && otherControl.gradientMode && !otherControl.gradientColors.empty()) {
                    primaryControl = &otherControl;
                    break;
                }
            }

            if (primaryControl && !primaryControl->gradientColors.empty()) {
                const auto& gradient = primaryControl->gradientColors;
                size_t gradientSize = gradient.size();

                // Determine sampling position based on control name
                float samplePos = 0.0f;
                std::string baseName = control.name;

                // Find "Primary", "Secondary", etc. patterns
                if (baseName.find("Primary") != std::string::npos || baseName.find("_main") != std::string::npos) {
                    samplePos = 0.0f;
                } else if (baseName.find("Secondary") != std::string::npos || baseName.find("_secondary") != std::string::npos) {
                    samplePos = 0.25f;
                } else if (baseName.find("Tertiary") != std::string::npos || baseName.find("_tertiary") != std::string::npos) {
                    samplePos = 0.5f;
                } else if (baseName.find("Accent") != std::string::npos || baseName.find("_accent") != std::string::npos) {
                    samplePos = 0.75f;
                } else if (baseName.find("Highlight") != std::string::npos || baseName.find("_highlight") != std::string::npos) {
                    samplePos = 1.0f;
                } else {
                    // Default to evenly spaced based on control index among sync controls
                    int syncCount = 0;
                    int syncIndex = 0;
                    for (size_t i = 0; i < m_shadertoyUniformControls.size(); ++i) {
                        const auto& other = m_shadertoyUniformControls[i];
                        if (other.isColor && other.paletteMode == 2) {
                            syncCount++;
                            if (&other == &control) {
                                syncIndex = syncCount - 1;
                            }
                        }
                    }
                    samplePos = syncCount > 1 ? static_cast<float>(syncIndex) / (syncCount - 1) : 0.0f;
                }

                // Sample from the gradient
                size_t sampleIndex = static_cast<size_t>(samplePos * (gradientSize - 1));
                sampleIndex = std::min(sampleIndex, gradientSize - 1);

                const glm::vec3& sampledColor = gradient[sampleIndex];

                // Update control value
                control.v3Value[0] = sampledColor.x;
                control.v3Value[1] = sampledColor.y;
                control.v3Value[2] = sampledColor.z;
                if (control.glslType == "vec4") {
                    control.v4Value[3] = 1.0f;
                }

                // Display read-only color swatch
                ImGui::ColorButton(("Synced Color##" + control.name).c_str(),
                    ImVec4(sampledColor.x, sampledColor.y, sampledColor.z, 1.0f),
                    ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop);

                ImGui::SameLine();
                ImGui::Text("(%.2f position)", samplePos);

            } else {
                // No primary gradient found
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                    "No primary gradient available for sync");
                ImGui::Text("Set a primary color control to Palette mode with Gradient enabled");
            }
        }
    }
}


static std::string InjectStandardUniforms(const std::string& source, bool isShadertoy) {
    std::string uniforms = "";
    if (source.find("uniform vec2 iResolution") == std::string::npos && source.find("uniform vec3 iResolution") == std::string::npos) {
        if (isShadertoy) {
            uniforms += "uniform vec3 iResolution;\n";
        } else {
            uniforms += "uniform vec2 iResolution;\n";
        }
    }
    if (source.find("uniform float iTime") == std::string::npos) {
        uniforms += "uniform float iTime;\n";
    }
    if (uniforms.empty()) {
        return source;
    }

    std::string versionLine;
    std::string sourceWithoutVersion;
    size_t versionPos = source.find("#version");
    if (versionPos != std::string::npos) {
        size_t lineEnd = source.find("\n", versionPos);
        versionLine = source.substr(0, lineEnd + 1);
        sourceWithoutVersion = source.substr(lineEnd + 1);
    } else {
        versionLine = "#version 330 core\n";
        sourceWithoutVersion = source;
    }

    return versionLine + uniforms + sourceWithoutVersion;
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
        const std::string shadertoy_helpers =
            "#ifndef GEMINI_SHADER_HELPERS\n"
            "#define GEMINI_SHADER_HELPERS\n"
            "// Compatibility shim for texture2D function\n"
            "vec4 texture2D(sampler2D s, vec2 uv) { return texture(s, uv); }\n"
            "vec4 texture2D(sampler2D s, vec3 uvw) { return texture(s, uvw.xy); }\n"
            "vec4 texture2D(sampler2D s, vec4 uvw) { return texture(s, uvw.xy / uvw.w); }\n"
            "#endif\n\n";

        std::string processedSource = std::regex_replace(m_shaderSourceCode, std::regex("\\btexture\\("), "texture2D(");

        finalFragmentCode = 
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec3 iResolution;\n"
            "uniform float iTime;\n"
            "uniform float iTimeDelta;\n"
            "uniform int iFrame;\n"
            "uniform vec4 iMouse;\n"
            "uniform sampler2D iChannel0;\n"
            "uniform sampler2D iChannel1;\n"
            "uniform sampler2D iChannel2;\n"
            "uniform sampler2D iChannel3;\n"
            "uniform float iUserFloat1;\n"
            "uniform vec3 iUserColor1;\n" + shadertoy_helpers + processedSource +
            "\nvoid main() {\n"
            "    mainImage(FragColor, gl_FragCoord.xy);\n"
            "}\n";
    } else {
        finalFragmentCode = InjectStandardUniforms(m_shaderSourceCode, m_isShadertoyMode);
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
    m_iChannel1SamplerLoc = glGetUniformLocation(m_shaderProgram, "iChannel1");
    m_iChannel2SamplerLoc = glGetUniformLocation(m_shaderProgram, "iChannel2");
    m_iChannel3SamplerLoc = glGetUniformLocation(m_shaderProgram, "iChannel3");

    m_iChannel0ActiveLoc = glGetUniformLocation(m_shaderProgram, "iChannel0_active");
    m_iChannel1ActiveLoc = glGetUniformLocation(m_shaderProgram, "iChannel1_active");
    m_iChannel2ActiveLoc = glGetUniformLocation(m_shaderProgram, "iChannel2_active");
    m_iChannel3ActiveLoc = glGetUniformLocation(m_shaderProgram, "iChannel3_active");

    m_iResolutionLocation = glGetUniformLocation(m_shaderProgram, "iResolution");
    m_iTimeLocation = glGetUniformLocation(m_shaderProgram, "iTime");
    m_iTimeDeltaLocation = glGetUniformLocation(m_shaderProgram, "iTimeDelta");
    m_iFrameLocation = glGetUniformLocation(m_shaderProgram, "iFrame");
    m_iMouseLocation = glGetUniformLocation(m_shaderProgram, "iMouse");
    m_iAudioAmpLoc = glGetUniformLocation(m_shaderProgram, "iAudioAmp");
    m_iAudioBandsAttLoc = glGetUniformLocation(m_shaderProgram, "iAudioBandsAtt");
    m_iCameraPositionLocation = glGetUniformLocation(m_shaderProgram, "iCameraPosition");
    m_iCameraMatrixLocation = glGetUniformLocation(m_shaderProgram, "iCameraMatrix");
    m_iLightPositionLocation = glGetUniformLocation(m_shaderProgram, "iLightPos");

    // This now runs for ALL effects
    for (auto& control : m_shadertoyUniformControls) {
        control.location = glGetUniformLocation(m_shaderProgram, control.name.c_str());
    }
}

void ShaderEffect::ParseShaderControls() {
    if (m_shaderSourceCode.empty()) return;

    // Let the parser scan the source
    m_shaderParser.ScanAndPrepareDefineControls(m_shaderSourceCode);
    m_shaderParser.ScanAndPrepareConstControls(m_shaderSourceCode);
    m_shaderParser.ScanAndPrepareUniformControls(m_shaderSourceCode);

    // Now, copy the results into the effect's own storage.
    // This is the single source of truth for the UI and renderer.
    m_defineControls = m_shaderParser.GetDefineControls();
    m_constControls = m_shaderParser.GetConstControls();
    m_shadertoyUniformControls = m_shaderParser.GetUniformControls();

    // ===================== PALETTE CONTROL SUMMARY =====================
    // Log palette control summary once per shader load (not on every UI render)
    if (!m_debugLogged && !m_shadertoyUniformControls.empty()) {
        int primaryCount = 0, secondaryBySemantic = 0, secondaryByIndex = 0;

        for (const auto& control : m_shadertoyUniformControls) {
            if (control.isPalette) {
                std::string baseName = control.name;
                int paletteIndex = control.metadata.value("paletteControlIndex", 0);

                // Check semantic detection
                bool isSecondarySemantic = (
                    baseName.find("Secondary") != std::string::npos ||
                    baseName.find("Tertiary") != std::string::npos ||
                    baseName.find("Accent") != std::string::npos ||
                    baseName.find("Highlight") != std::string::npos ||
                    baseName.find("_secondary") != std::string::npos ||
                    baseName.find("_tertiary") != std::string::npos ||
                    baseName.find("_accent") != std::string::npos ||
                    baseName.find("_highlight") != std::string::npos
                );

                if (paletteIndex == 0) {
                    primaryCount++;
                } else if (isSecondarySemantic) {
                    secondaryBySemantic++;
                } else if (paletteIndex > 0) {
                    secondaryByIndex++;
                }
            }
        }

        // Clean, once-per-load summary
        std::cout << "[PALETTE] Shader loaded: '" << m_shaderFilePath << "'"
                  << " → Primary:" << primaryCount
                  << ", Secondary:" << (secondaryBySemantic + secondaryByIndex)
                  << " (Semantic:" << secondaryBySemantic << ", Index:" << secondaryByIndex << ")"
                  << std::endl;

        // Mark debug as logged for this shader load
        m_debugLogged = true;
    }
    // =================================================================

    // Apply any deserialized control values
    if (!m_deserialized_controls.is_null()) {
        for (auto& control : m_shadertoyUniformControls) {
            if (m_deserialized_controls.contains(control.name)) {
                if (control.glslType == "float") {
                    control.fValue = m_deserialized_controls[control.name];
                } else if (control.glslType == "int" || control.glslType == "bool") {
                    control.iValue = m_deserialized_controls[control.name];
                } else if (control.glslType == "vec3") {
                    std::vector<float> v = m_deserialized_controls[control.name];
                    if (v.size() >= 3) {
                        control.v3Value[0] = v[0];
                        control.v3Value[1] = v[1];
                        control.v3Value[2] = v[2];
                    }
                }
            }
        }
        m_deserialized_controls.clear(); // Clear after applying
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
    j["id"] = id;
    j["name"] = name;
    j["startTime"] = startTime;
    j["endTime"] = endTime;
    j["sourceFilePath"] = m_shaderFilePath;
    if (m_shaderFilePath.empty() || m_shaderFilePath == "dynamic_source" || m_shaderFilePath.rfind("shadertoy://", 0) == 0) {
        j["sourceCode"] = m_shaderSourceCode;
    }
    j["isShadertoyMode"] = m_isShadertoyMode;

    // Serialize controllable uniforms
    nlohmann::json uniform_values;
    for (const auto& control : m_shadertoyUniformControls) {
        if (control.glslType == "float") {
            uniform_values[control.name] = control.fValue;
        } else if (control.glslType == "int" || control.glslType == "bool") {
            uniform_values[control.name] = control.iValue;
        } else if (control.glslType == "vec3") {
            uniform_values[control.name] = {control.v3Value[0], control.v3Value[1], control.v3Value[2]};
        }
        // Add other types as needed
    }
    j["control_values"] = uniform_values;

    // Serialize input connections
    nlohmann::json input_ids = nlohmann::json::array();
    for (const auto& input_effect : m_inputs) {
        if (input_effect) {
            input_ids.push_back(input_effect->id);
        } else {
            input_ids.push_back(nullptr); // Use null for empty input slots
        }
    }
    j["input_ids"] = input_ids;

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

    // Cache control values and input IDs for later application
    if (data.contains("control_values")) {
        m_deserialized_controls = data["control_values"];
    }
    if (data.contains("input_ids")) {
        m_deserialized_input_ids = data["input_ids"].get<std::vector<int>>();
    }
}

void ShaderEffect::ResetParameters() {
    // This function will now effectively re-parse the defaults from the shader source.
    m_deserialized_controls.clear();
    if (!m_shaderSourceCode.empty()) {
        ApplyShaderCode(m_shaderSourceCode);
    }
}

bool ShaderEffect::CheckForUpdatesAndReload() {
    if (m_shaderFilePath.empty() || m_shaderFilePath == "dynamic_source" || m_shaderFilePath.rfind("shadertoy://", 0) == 0) {
        return false; // Not a reloadable file
    }

    try {
        auto current_write_time = std::filesystem::last_write_time(m_shaderFilePath);
        if (current_write_time > m_lastWriteTime) {
            m_lastWriteTime = current_write_time;
            LoadShaderFromFile(m_shaderFilePath);
            ApplyShaderCode(m_shaderSourceCode);
            return true; // Reloaded
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // File might have been deleted or is otherwise inaccessible
        // Log this, but don't spam
        static std::string last_error;
        if (last_error != e.what()) {
            std::cerr << "[Hot-Reload] Filesystem error for " << m_shaderFilePath << ": " << e.what() << std::endl;
            last_error = e.what();
        }
        return false;
    }

    return false; // No changes
}

std::unique_ptr<Effect> ShaderEffect::Clone() const {
    // Create a new ShaderEffect with the same basic configuration
    auto newEffect = std::make_unique<ShaderEffect>(m_shaderFilePath, m_fboWidth, m_fboHeight, m_isShadertoyMode);

    // Copy the name
    newEffect->name = this->name + " (Copy)";

    // Copy the source code if it's not file-based
    if (m_shaderFilePath.empty() || m_shaderFilePath == "dynamic_source" || m_shaderFilePath.rfind("shadertoy://", 0) == 0) {
        newEffect->m_shaderSourceCode = this->m_shaderSourceCode;
    }

    // Copy the parameter values by copying the control structures
    newEffect->m_shadertoyUniformControls = this->m_shadertoyUniformControls;
    newEffect->m_defineControls = this->m_defineControls;
    newEffect->m_constControls = this->m_constControls;
    newEffect->m_colorCycleState = this->m_colorCycleState;

    // The new effect will be loaded by the main loop, which will compile the shader
    // and create the FBO. We don't copy inputs here, the user can reconnect them.

    return newEffect;
}

void ShaderEffect::InitializeDummyTexture() {
    if (s_dummyTexture == 0) {
        glGenTextures(1, &s_dummyTexture);
        glBindTexture(GL_TEXTURE_2D, s_dummyTexture);
        unsigned char blackPixel[] = {0, 0, 0, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        std::cout << "Initialized Dummy Texture for unbound shader inputs." << std::endl;
    }
}
