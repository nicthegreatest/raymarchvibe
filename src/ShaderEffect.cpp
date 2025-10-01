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

// Define the static member
GLuint ShaderEffect::s_dummyTexture = 0;

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
      m_audioAmp(0.0f),
      m_iAudioAmpLoc(-1),
      m_iAudioBandsLoc(-1),
      m_shaderParser(),
      m_iChannel0SamplerLoc(-1),
      m_fboID(0),
      m_fboTextureID(0),
      m_rboID(0),
      m_fboWidth(initialWidth),
      m_fboHeight(initialHeight),
      m_lastWriteTime{}
{
    m_inputs.resize(1, nullptr);

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
    if (m_shaderSourceCode.find("mainImage") != std::string::npos) {
        m_isShadertoyMode = true;
    } else {
        m_isShadertoyMode = false;
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
    if (m_iChannel0SamplerLoc != -1) {
        glActiveTexture(GL_TEXTURE0);
        if (!m_inputs.empty() && m_inputs[0] != nullptr) {
            GLuint inputTextureID = m_inputs[0]->GetOutputTexture();
            glBindTexture(GL_TEXTURE_2D, inputTextureID != 0 ? inputTextureID : s_dummyTexture);
        } else {
            glBindTexture(GL_TEXTURE_2D, s_dummyTexture);
        }
        glUniform1i(m_iChannel0SamplerLoc, 0);
    }

    // Set uniforms from parsed controls
    for (const auto& control : m_shadertoyUniformControls) {
        if (control.location != -1) {
            if (control.glslType == "float") {
                glUniform1f(control.location, control.fValue);
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
    if (m_iAudioBandsLoc != -1) {
        glUniform4fv(m_iAudioBandsLoc, 1, m_audioBands.data());
    }

    Renderer::RenderQuad();
}

void ShaderEffect::SetAudioAmplitude(float amp) {
    m_audioAmp = amp;
}

void ShaderEffect::SetAudioBands(const std::array<float, 4>& bands) {
    m_audioBands = bands;
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

    if (ImGui::CollapsingHeader("Parsed Uniforms##EffectParsedUniforms", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderParsedUniformsUI();
    }

    if (ImGui::CollapsingHeader("Shader Defines##EffectDefines")) {
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
                ImGui::ColorEdit3(label.c_str(), control.v3Value);
            } else {
                ImGui::DragFloat3(label.c_str(), control.v3Value, step);
            }
        } else if (control.glslType == "vec4") {
            if (control.isColor) {
                ImGui::ColorEdit4(label.c_str(), control.v4Value);
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
            if (ImGui::InputFloat(control.name.c_str(), &control.floatValue)) {
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
        finalFragmentCode = 
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec3 iResolution;\n"
            "uniform float iTime;\n"
            "uniform float iTimeDelta;\n"
            "uniform int iFrame;\n"
            "uniform vec4 iMouse;\n"
            "uniform float iUserFloat1;\n"
            "uniform vec3 iUserColor1;\n" + m_shaderSourceCode +
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
    m_iChannel0ActiveLoc = glGetUniformLocation(m_shaderProgram, "iChannel0_active");

    m_iResolutionLocation = glGetUniformLocation(m_shaderProgram, "iResolution");
    m_iTimeLocation = glGetUniformLocation(m_shaderProgram, "iTime");
    m_iTimeDeltaLocation = glGetUniformLocation(m_shaderProgram, "iTimeDelta");
    m_iFrameLocation = glGetUniformLocation(m_shaderProgram, "iFrame");
    m_iMouseLocation = glGetUniformLocation(m_shaderProgram, "iMouse");
    m_iAudioAmpLoc = glGetUniformLocation(m_shaderProgram, "iAudioAmp");
    m_iAudioBandsLoc = glGetUniformLocation(m_shaderProgram, "iAudioBands");

    // This now runs for ALL effects
    for (auto& control : m_shadertoyUniformControls) {
        control.location = glGetUniformLocation(m_shaderProgram, control.name.c_str());
    }
}

void ShaderEffect::ParseShaderControls() {
    if (m_shaderSourceCode.empty()) return;
    m_shaderParser.ScanAndPrepareDefineControls(m_shaderSourceCode);
    m_defineControls = m_shaderParser.GetDefineControls();
    m_shaderParser.ScanAndPrepareConstControls(m_shaderSourceCode);
    m_constControls = m_shaderParser.GetConstControls();
    m_shaderParser.ScanAndPrepareUniformControls(m_shaderSourceCode);
    m_shadertoyUniformControls = m_shaderParser.GetUniformControls();

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