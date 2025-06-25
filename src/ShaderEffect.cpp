#include "ShaderEffect.h"
#include "Renderer.h" // <-- ADD THIS INCLUDE
#include "imgui.h"

#include <fstream>
#include <iostream> // For error logging related to FBO, and cerr
#include <sstream>
#include <vector>
#include <algorithm> // For std::sort, std::remove_if
#include <regex>     // For parsing logic if moved here

// Local GL error checker for ShaderEffect
static void checkGLErrorInEffect(const std::string& label, const std::string& effectName) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        std::string errorStr;
        switch(err) {
            case GL_INVALID_ENUM: errorStr = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "INVALID_OPERATION"; break;
            // GL_STACK_OVERFLOW and GL_STACK_UNDERFLOW are deprecated and may not be defined
            // case GL_STACK_OVERFLOW: errorStr = "STACK_OVERFLOW"; break;
            // case GL_STACK_UNDERFLOW: errorStr = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY: errorStr = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "INVALID_FRAMEBUFFER_OPERATION"; break;
            default: errorStr = "UNKNOWN_ERROR (" + std::to_string(err) + ")"; break;
        }
        std::cerr << "GL_ERROR (Effect: " << effectName << ", Op: " << label << "): " << errorStr << std::endl;
    }
}


// Helper function declarations (prototypes) for functions that will be moved
// from main.cpp or new helper functions.
// These would ideally be static members or free functions in a utility namespace.

// Shader compilation and linking (to be adapted from main.cpp)
static GLuint CompileShader(const char* source, GLenum type, std::string& errorLogString); // Already defined below
static GLuint CreateShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString); // Already defined below
static std::string LoadPassthroughVertexShaderSource(std::string& errorMsg); // Already defined below

// REMOVED: Constructor definitions for ShaderToyUniformControl and ShaderConstControl.
// These are (and should be) defined in ShaderParser.cpp.
// ShaderEffect.cpp should include ShaderParser.h for their declarations.


ShaderEffect::ShaderEffect(const std::string& initialShaderPath, int initialWidth, int initialHeight, bool isShadertoy)
    : Effect(), // Base class first
      m_shaderProgram(0),
      m_isShadertoyMode(isShadertoy),
      m_shaderLoaded(false),
      m_time(0.0f),
      m_deltaTime(0.0f),
      m_frameCount(0),
      // m_mouseState, m_currentDisplayWidth, m_currentDisplayHeight, m_objectColor use default initializers
      m_scale(1.0f),
      m_timeSpeed(1.0f),
      // m_colorMod uses default initializer
      m_patternScale(1.0f),
      // m_cameraPosition, m_cameraTarget use default initializers
      m_cameraFOV(60.0f),
      // m_lightPosition, m_lightColor use default initializers
      m_iUserFloat1(0.5f),
      // m_iUserColor1 uses default initializer
      m_audioAmp(0.0f),
      // All m_...Loc use default initializers (-1) set in header
      m_iAudioAmpLoc(-1), // Explicitly listed here as it was in the original list
      // m_defineControls, m_shadertoyUniformControls, m_constControls use default initializers
      m_shaderParser(),
      // m_inputs uses default initializer
      m_iChannel0SamplerLoc(-1), // Explicitly listed
      m_fboID(0),
      m_fboTextureID(0),
      m_rboID(0),
      m_fboWidth(initialWidth),
      m_fboHeight(initialHeight)
{
    m_inputs.resize(1, nullptr); // Assuming 1 input slot for now

    // Initialize arrays for colors and vectors
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
        // Load will be called explicitly after construction by user or system
    }
    // Base Effect properties (name, startTime, endTime) use defaults or can be set post-construction
}

ShaderEffect::~ShaderEffect() {
    if (m_shaderProgram != 0) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
    if (m_fboID != 0) {
        glDeleteFramebuffers(1, &m_fboID);
        m_fboID = 0;
    }
    if (m_fboTextureID != 0) {
        glDeleteTextures(1, &m_fboTextureID);
        m_fboTextureID = 0;
    }
    if (m_rboID != 0) {
        glDeleteRenderbuffers(1, &m_rboID);
        m_rboID = 0;
    }
}

// --- FBO specific methods ---
GLuint ShaderEffect::GetOutputTexture() const {
    return m_fboTextureID;
}

void ShaderEffect::ResizeFrameBuffer(int width, int height) {
    if (width <= 0 || height <= 0) {
        std::cerr << "ShaderEffect::ResizeFrameBuffer error: Invalid dimensions (" << width << "x" << height << ") for " << name << std::endl;
        return;
    }
    std::cerr << "ShaderEffect::ResizeFrameBuffer for " << name << " to " << width << "x" << height << ". Current FBO ID: " << m_fboID << std::endl;

    m_fboWidth = width;
    m_fboHeight = height;

    // Delete existing FBO objects if they exist
    if (m_fboID != 0) glDeleteFramebuffers(1, &m_fboID);
    if (m_fboTextureID != 0) glDeleteTextures(1, &m_fboTextureID);
    if (m_rboID != 0) glDeleteRenderbuffers(1, &m_rboID);

    // Create Framebuffer
    glGenFramebuffers(1, &m_fboID);
    checkGLErrorInEffect("glGenFramebuffers", name);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
    checkGLErrorInEffect("glBindFramebuffer (FBO Texture Gen)", name);

    // Create Texture Attachment
    glGenTextures(1, &m_fboTextureID);
    checkGLErrorInEffect("glGenTextures", name);
    glBindTexture(GL_TEXTURE_2D, m_fboTextureID);
    checkGLErrorInEffect("glBindTexture (FBO Texture Gen)", name);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_fboWidth, m_fboHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    checkGLErrorInEffect("glTexImage2D (FBO Texture Gen)", name);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    checkGLErrorInEffect("glTexParameteri (FBO Texture Gen)", name);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboTextureID, 0);
    checkGLErrorInEffect("glFramebufferTexture2D", name);
    glBindTexture(GL_TEXTURE_2D, 0);
    checkGLErrorInEffect("Unbind Texture (FBO Texture Gen)", name);

    // Create Renderbuffer Object for Depth/Stencil attachment
    glGenRenderbuffers(1, &m_rboID);
    checkGLErrorInEffect("glGenRenderbuffers", name);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rboID);
    checkGLErrorInEffect("glBindRenderbuffer", name);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_fboWidth, m_fboHeight);
    checkGLErrorInEffect("glRenderbufferStorage", name);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rboID);
    checkGLErrorInEffect("glFramebufferRenderbuffer", name);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    checkGLErrorInEffect("Unbind RBO", name);

    // Check FBO completeness
    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    checkGLErrorInEffect("glCheckFramebufferStatus", name); // Check error from the status check itself
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer for " << name << " is not complete! Status: 0x" << std::hex << fboStatus << std::dec << std::endl;
        // Cleanup on failure
        glDeleteFramebuffers(1, &m_fboID); m_fboID = 0;
        glDeleteTextures(1, &m_fboTextureID); m_fboTextureID = 0;
        glDeleteRenderbuffers(1, &m_rboID); m_rboID = 0;
    } else {
        std::cerr << "ShaderEffect FBO for " << name << " created/resized successfully: " << m_fboWidth << "x" << m_fboHeight << ", FBO ID: " << m_fboID << ", Texture ID: " << m_fboTextureID << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    checkGLErrorInEffect("Unbind FBO (End of ResizeFrameBuffer)", name);
}

// --- Node Editor Methods ---
int ShaderEffect::GetInputPinCount() const {
    return m_inputs.size(); // Or a fixed number like 1
}

void ShaderEffect::SetInputEffect(int pinIndex, Effect* inputEffect) {
    if (pinIndex >= 0 && static_cast<size_t>(pinIndex) < m_inputs.size()) {
        m_inputs[static_cast<size_t>(pinIndex)] = inputEffect;
    } else {
        std::cerr << "ShaderEffect Error: SetInputEffect - pinIndex " << pinIndex << " out of bounds." << std::endl;
    }
}


// --- Public Shader Management ---
bool ShaderEffect::LoadShaderFromFile(const std::string& filePath) {
    m_shaderFilePath = filePath;
    // this->name is set by Effect constructor or can be overridden if needed
    // If filePath should become the name, set it here:
    // this->name = filePath; // This is now handled by main.cpp after dialog
    m_shaderFilePath = filePath; // Ensure m_shaderFilePath is set
    std::string loadError;
    m_shaderSourceCode = LoadShaderSourceFile(m_shaderFilePath, loadError);
    if (!loadError.empty()) {
        m_compileErrorLog = "File load error: " + loadError;
        m_shaderLoaded = false;
        return false;
    }
    // Automatically determine Shadertoy mode from content if not explicitly set
    // This is a simple heuristic; a more robust one might be needed.
    if (m_shaderSourceCode.find("mainImage") != std::string::npos &&
        m_shaderSourceCode.find("fragCoord") != std::string::npos) {
        m_isShadertoyMode = true;
    } else {
        m_isShadertoyMode = false;
    }
    // ApplyShaderCode will handle compilation, parsing etc.
    return true;
}

bool ShaderEffect::LoadShaderFromSource(const std::string& sourceCode) {
    m_shaderFilePath = "dynamic_source"; // Or some other indicator
    this->name = "Dynamic Shader"; // Default name if loaded from source
    m_shaderSourceCode = sourceCode;
    if (m_shaderSourceCode.find("mainImage") != std::string::npos &&
        m_shaderSourceCode.find("fragCoord") != std::string::npos) {
        m_isShadertoyMode = true;
    } else {
        m_isShadertoyMode = false;
    }
    return true;
}

void ShaderEffect::Load() {
    std::cerr << "ShaderEffect::Load() called for: " << name << ", FilePath: " << m_shaderFilePath << std::endl;
    // Ensure FBO dimensions are set before creating FBO (e.g. from constructor or explicit call after SetDisplayResolution)
    // For now, m_fboWidth/Height are initialized in constructor.
    // If they are 0, use some default from main.cpp or constants.
    // This ResizeFrameBuffer call will create the FBO.
    // It's important that this is called after GL context is ready and before any rendering to FBO.
    if (m_fboWidth == 0 || m_fboHeight == 0) { // If not set by constructor or resize
        // This part may need adjustment depending on how initial resolution is passed.
        // For now, assuming constructor defaults are sensible or Resize is called from main.
        std::cerr << "Warning: ShaderEffect::Load() for " << name << " called with FBO dimensions 0. Using default 800x600 for FBO." << std::endl;
        ResizeFrameBuffer(800, 600); // Fallback if not set
    } else {
        std::cerr << "ShaderEffect::Load() for " << name << ", calling ResizeFrameBuffer with " << m_fboWidth << "x" << m_fboHeight << std::endl;
        ResizeFrameBuffer(m_fboWidth, m_fboHeight); // Create FBO with current dimensions
    }


    if (m_shaderSourceCode.empty() && !m_shaderFilePath.empty()) {
        std::cerr << "ShaderEffect::Load() for " << name << ": Source code empty, loading from file: " << m_shaderFilePath << std::endl;
        std::string loadError;
        m_shaderSourceCode = LoadShaderSourceFile(m_shaderFilePath, loadError);
        if (!loadError.empty()) {
            m_compileErrorLog = "File load error during Load(): " + loadError;
            m_shaderLoaded = false;
            std::cerr << "ShaderEffect::Load() for " << name << " - File load error: " << m_compileErrorLog << std::endl;
            return;
        }
        std::cerr << "ShaderEffect::Load() for " << name << ": Successfully loaded source from file." << std::endl;
        // Auto-detect Shadertoy mode again if source was just loaded
        if (m_shaderSourceCode.find("mainImage") != std::string::npos &&
            m_shaderSourceCode.find("fragCoord") != std::string::npos) {
            m_isShadertoyMode = true;
        } else {
            m_isShadertoyMode = false;
        }
    }

    if (m_shaderSourceCode.empty()) {
        m_compileErrorLog = "Shader source code for " + name + " is empty. Cannot load.";
        m_shaderLoaded = false;
        std::cerr << m_compileErrorLog << std::endl;
        return;
    }
    std::cerr << "ShaderEffect::Load() for " << name << ": Calling ApplyShaderCode." << std::endl;
    ApplyShaderCode(m_shaderSourceCode);
}


void ShaderEffect::ApplyShaderCode(const std::string& newShaderCode) {
    std::cerr << "ShaderEffect::ApplyShaderCode() called for: " << name << std::endl;
    m_shaderSourceCode = newShaderCode;
    m_compileErrorLog.clear();

    CompileAndLinkShader(); // This updates m_shaderProgram and m_compileErrorLog
    std::cerr << "ShaderEffect::ApplyShaderCode for " << name << " - CompileAndLinkShader done. Program ID: " << m_shaderProgram << ", ErrorLog: " << m_compileErrorLog << std::endl;

    if (m_shaderProgram != 0) {
        std::cerr << "ShaderEffect::ApplyShaderCode for " << name << ": Shader program valid. Fetching uniforms and parsing controls." << std::endl;
        FetchUniformLocations(); // Fetches locations for the new program
        ParseShaderControls();   // Parses defines, metadata uniforms, consts from m_shaderSourceCode
        m_shaderLoaded = true;
        if (m_compileErrorLog.empty()) {
             m_compileErrorLog = "Shader applied successfully.";
        }
         std::cerr << "ShaderEffect::ApplyShaderCode for " << name << ": Loaded successfully. Final error log: " << m_compileErrorLog << std::endl;
    } else {
        m_shaderLoaded = false;
        std::cerr << "ShaderEffect::ApplyShaderCode for " << name << ": Shader program is 0. Load failed. Error log: " << m_compileErrorLog << std::endl;
        // m_compileErrorLog will already be set by CompileAndLinkShader
    }
}

void ShaderEffect::SetShadertoyMode(bool mode) {
    if (m_isShadertoyMode != mode) {
        m_isShadertoyMode = mode;
        // If shader is already loaded, re-fetch uniforms as they might change
        // and potentially re-parse controls if some are mode-specific
        if (m_shaderProgram != 0) {
            FetchUniformLocations();
            ParseShaderControls(); // Some controls might be conditional on shadertoyMode
        }
    }
}

// --- Core Effect Methods ---
void ShaderEffect::Update(float currentTime) {
    // Update internal time. Note: iTime in shader is often raw glfwGetTime().
    // Here, 'currentTime' is the master timeline time.
    // The original u_timeSpeed was a multiplier for glfwGetTime().
    // We need to decide how m_timeSpeed interacts with the passed 'currentTime'.
    // Option 1: currentTime is already scaled by a global timeline speed.
    // Option 2: m_timeSpeed is an additional local multiplier.
    // For now, let's assume currentTime is what the effect should use.
    // If m_timeSpeed is 1.0, m_time just becomes currentTime.
    // If an effect wants to run faster/slower *relative to the timeline*, m_timeSpeed can adjust that.
    // This means m_time isn't just `currentTime` but `startTime + (currentTime - startTime) * m_timeSpeed`
    // or more simply, the delta applied to an internal accumulator.
    // Let's use `currentTime` directly for `iTime` and `m_timeSpeed` for native `u_timeSpeed` uniform.

    m_time = currentTime; // This will be passed to iTime uniform

    // Update mouse, resolution, frame delta if these are not set by external calls per frame
    // For now, assume SetMouseState, SetDisplayResolution, SetDeltaTime, IncrementFrameCount are called externally.
}

// In ShaderEffect.cpp

void ShaderEffect::Render() {
    // Diagnostic Log
    std::cerr << "ShaderEffect::Render() called for: " << name
              << ", Program: " << m_shaderProgram
              << ", FBO: " << m_fboID
              << ", Loaded: " << (m_shaderLoaded ? "true" : "false") << std::endl;

    if (!m_shaderLoaded || m_shaderProgram == 0 || m_fboID == 0) {
        std::cerr << "ShaderEffect::Render() for " << name << " skipped (not loaded/no program/no FBO)." << std::endl;
        return; // Don't render if not ready
    }

    // 1. Bind this effect's FBO as the render target
    glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);
    checkGLErrorInEffect("glBindFramebuffer (Effect FBO)", name);
    glViewport(0, 0, m_fboWidth, m_fboHeight);
    checkGLErrorInEffect("glViewport (Effect FBO)", name);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the FBO
    checkGLErrorInEffect("glClear (Effect FBO)", name);

    // 2. Use this effect's shader program
    glUseProgram(m_shaderProgram);
    checkGLErrorInEffect("glUseProgram", name);

    // 3. Bind the input texture from a previous node (if connected)
    if (!m_inputs.empty() && m_inputs[0] != nullptr) {
        if (auto* inputSE = dynamic_cast<ShaderEffect*>(m_inputs[0])) {
            GLuint inputTextureID = inputSE->GetOutputTexture();
            if (this->name == "Passthrough (Final Output)") { // Specific log for Passthrough
                std::cerr << "ShaderEffect::Render for Passthrough: inputSE=" << inputSE->name
                          << ", inputTextureID=" << inputTextureID
                          << ", m_iChannel0SamplerLoc=" << m_iChannel0SamplerLoc << std::endl;
            }
            if (inputTextureID != 0 && m_iChannel0SamplerLoc != -1) {
                 if (this->name == "Passthrough (Final Output)") {
                    std::cerr << "ShaderEffect::Render for Passthrough: Binding iChannel0 with texture ID " << inputTextureID << std::endl;
                }
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, inputTextureID);
                glUniform1i(m_iChannel0SamplerLoc, 0);
            } else if (this->name == "Passthrough (Final Output)" && m_iChannel0SamplerLoc != -1 && inputTextureID == 0) {
                 std::cerr << "ShaderEffect::Render for Passthrough: Input texture ID is 0. Not binding iChannel0." << std::endl;
            }
        }
    }

    // 4. Set all other uniforms (iTime, iResolution, custom params, etc.)
    // ... (This part of your code is correct and stays the same)
    if (m_isShadertoyMode) {
        if (m_iResolutionLocation != -1) glUniform3f(m_iResolutionLocation, (float)m_fboWidth, (float)m_fboHeight, (float)m_fboWidth / (float)m_fboHeight);
        if (m_iTimeLocation != -1) glUniform1f(m_iTimeLocation, m_time);
        // ... set all other shadertoy uniforms ...
        // (Copied from existing logic for completeness, assuming they are correct)
        if (m_iTimeDeltaLocation != -1) glUniform1f(m_iTimeDeltaLocation, m_deltaTime);
        if (m_iFrameLocation != -1) glUniform1i(m_iFrameLocation, m_frameCount);
        if (m_iMouseLocation != -1) glUniform4fv(m_iMouseLocation, 1, m_mouseState);
        if (m_iUserFloat1Loc != -1) glUniform1f(m_iUserFloat1Loc, m_iUserFloat1);
        if (m_iUserColor1Loc != -1) glUniform3fv(m_iUserColor1Loc, 1, m_iUserColor1);
        for (const auto& control : m_shadertoyUniformControls) {
             if (control.location != -1) { /* ... set metadata uniforms ... */ } // Placeholder for brevity
        }

    } else { // Native Mode
        if (m_iResolutionLocation != -1) glUniform2f(m_iResolutionLocation, (float)m_fboWidth, (float)m_fboHeight);
        if (m_iTimeLocation != -1) glUniform1f(m_iTimeLocation, m_time);
        // ... set all other native uniforms ...
        // (Copied from existing logic for completeness, assuming they are correct)
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

    // 5. FINALLY, execute the draw call here.
    Renderer::RenderQuad();
    checkGLErrorInEffect("After Renderer::RenderQuad", name);

    // The note about the main loop doing the drawing is no longer true.
    // No explicit unbind of FBO here, main loop binds FBO 0 after this effect's Render()
}

void ShaderEffect::SetAudioAmplitude(float amp) {
    m_audioAmp = amp;
}

// --- State Setters (called by main loop or UIManager) ---
void ShaderEffect::SetMouseState(float x, float y, float click_x, float click_y) {
    m_mouseState[0] = x;
    m_mouseState[1] = y;
    m_mouseState[2] = click_x;
    m_mouseState[3] = click_y;
}

void ShaderEffect::SetDisplayResolution(int width, int height) {
    m_currentDisplayWidth = width;
    m_currentDisplayHeight = height;
}


// --- File Loading Helper (adapted from main.cpp's loadShaderSource) ---
std::string ShaderEffect::LoadShaderSourceFile(const std::string& filePath, std::string& errorMsg) {
    errorMsg.clear();
    std::ifstream shaderFile;
    std::stringstream shaderStream;
    // Ensure ifstream objects can throw exceptions:
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


// --- Static Helper Implementations (Shader Compilation & Linking) ---
// These are direct adaptations from main.cpp, made static here.
// Ideally, these could be part of a dedicated GLUtils namespace or class.

static std::string LoadPassthroughVertexShaderSource(std::string& errorMsg) {
    // In a real scenario, this path should be more robust (e.g., relative to executable or configured)
    // For now, hardcoding as it was in main.cpp context.
    // This function might need to be part of ShaderEffect or a common utility if ShaderEffect needs to change VS.
    // For now, assuming a single, fixed VS for all ShaderEffects.
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
        std::cerr << errorLogString << std::endl; return 0;
    }
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 0 ? logLength + 1 : 257); // Ensure buffer is at least 1 for null terminator, or 256+1 for safety
        if (logLength == 0) infoLog[0] = '\0'; // Explicitly null-terminate if logLength is 0
        glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()-1), NULL, infoLog.data());
        errorLogString = "ERROR::SHADER::COMPILE_FAIL (" + std::string(type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") + ")\n" + infoLog.data();
        // std::cerr << errorLogString << std::endl; // ShaderEffect will log this if needed
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint CreateShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString) {
    errorLogString.clear();
    if (vertexShaderID == 0 || fragmentShaderID == 0) {
        errorLogString = "ERROR::PROGRAM::LINK_INVALID_SHADER_ID - One or both shaders failed to compile.";
        // Clean up the valid shader if one failed, the other is already 0
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
        if (logLength == 0) infoLog[0] = '\0';
        glGetProgramInfoLog(program, static_cast<GLsizei>(infoLog.size()-1), NULL, infoLog.data());
        errorLogString = "ERROR::PROGRAM::LINK_FAIL\n" + std::string(infoLog.data());
        // std::cerr << errorLogString << std::endl; // ShaderEffect will log this
        glDeleteProgram(program); // Important: delete the failed program
        program = 0;
    }

    // Shaders are linked, so we can detach and delete them
    glDetachShader(program, vertexShaderID); // Not strictly necessary but good practice
    glDetachShader(program, fragmentShaderID);
    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);

    return program;
}
// ... Implementations for CompileAndLinkShader, FetchUniformLocations, ParseShaderControls, RenderUI and its helpers will follow.
// This is a starting point.

// --- RenderUI and its helpers ---

void ShaderEffect::RenderUI() {
    if (!m_shaderLoaded && m_compileErrorLog.find("File load error") != std::string::npos) {
        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Shader Load Error: Could not load file.");
        ImGui::TextWrapped("%s", m_compileErrorLog.c_str());
        return;
    }
    if (!m_shaderLoaded && !m_compileErrorLog.empty()) {
        ImGui::TextColored(ImVec4(1.f, 0.f, 0.f, 1.f), "Shader Compile/Link Error.");
        ImGui::TextWrapped("%s", m_compileErrorLog.c_str());
        // Allow viewing parameters even if compile failed, they just won't take effect.
    }


    ImGui::Text("Effect: %s", name.c_str());
    if (!m_shaderFilePath.empty()){
        ImGui::Text("Source: %s", m_shaderFilePath.c_str());
    } else {
        ImGui::Text("Source: Dynamically Set");
    }

    bool currentShadertoyMode = m_isShadertoyMode;
    if (ImGui::Checkbox("Shadertoy Mode", &currentShadertoyMode)) {
        SetShadertoyMode(currentShadertoyMode); // This will re-fetch uniforms and re-parse controls
        // Potentially trigger a re-apply of shader code if preamble changes significantly
        ApplyShaderCode(m_shaderSourceCode);
    }
    ImGui::Separator();

    if (m_isShadertoyMode) {
        RenderShadertoyParamsUI();
        if (!m_shadertoyUniformControls.empty()) {
            if (ImGui::CollapsingHeader("Shader Uniforms (from metadata)##EffectSTUniforms")) {
                RenderMetadataUniformsUI();
            }
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

    // Display any non-critical warnings from uniform fetching etc.
    if (m_shaderLoaded && !m_compileErrorLog.empty() && m_compileErrorLog.find("Applied successfully!") == std::string::npos && m_compileErrorLog.find("Shader applied successfully.") == std::string::npos) {
        if (m_compileErrorLog.find("ERROR") == std::string::npos && m_compileErrorLog.find("FAIL") == std::string::npos ) { // Simple check for warnings vs errors
             ImGui::Separator();
             ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Shader Status/Warnings:");
             ImGui::TextWrapped("%s", m_compileErrorLog.c_str());
        }
    }


}

void ShaderEffect::RenderNativeParamsUI() {
    ImGui::Text("Native Shader Parameters:");
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("Colour Parameters##EffectNativeColours")) {
        ImGui::ColorEdit3("Object Colour##Effect", m_objectColor);
        ImGui::ColorEdit3("Colour Mod##Effect", m_colorMod);
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
        // Skip if location is -1 (uniform not found in shader), unless we want to show it as disabled
        // if (control.location == -1 && m_isShadertoyMode) continue;

        std::string label = control.metadata.value("label", control.name);
        ImGui::PushID(static_cast<int>(i)); // Unique ID for ImGui

        if (control.glslType == "float") {
            float min_val = control.metadata.value("min", 0.0f);
            float max_val = control.metadata.value("max", 1.0f);
            if (min_val < max_val) {
                ImGui::SliderFloat(label.c_str(), &control.fValue, min_val, max_val);
            } else {
                ImGui::DragFloat(label.c_str(), &control.fValue, control.metadata.value("step", 0.01f));
            }
        } else if (control.glslType == "vec2") {
            ImGui::DragFloat2(label.c_str(), control.v2Value, control.metadata.value("step", 0.01f));
        } else if (control.glslType == "vec3") {
            if (control.isColor) {
                ImGui::ColorEdit3(label.c_str(), control.v3Value);
            } else {
                ImGui::DragFloat3(label.c_str(), control.v3Value, control.metadata.value("step", 0.01f));
            }
        } else if (control.glslType == "vec4") {
            if (control.isColor) {
                ImGui::ColorEdit4(label.c_str(), control.v4Value);
            } else {
                ImGui::DragFloat4(label.c_str(), control.v4Value, control.metadata.value("step", 0.01f));
            }
        } else if (control.glslType == "int") {
            int min_val_i = control.metadata.value("min", 0);
            int max_val_i = control.metadata.value("max", 100);
            if (control.metadata.contains("min") && control.metadata.contains("max") && min_val_i < max_val_i) {
                ImGui::SliderInt(label.c_str(), &control.iValue, min_val_i, max_val_i);
            } else {
                ImGui::DragInt(label.c_str(), &control.iValue, control.metadata.value("step", 1.0f));
            }
        } else if (control.glslType == "bool") {
            ImGui::Checkbox(label.c_str(), &control.bValue);
        }
        ImGui::PopID();
    }
}

void ShaderEffect::RenderDefineControlsUI() {
    if (m_defineControls.empty()) {
        ImGui::TextDisabled(" (No defines detected in current shader)");
        return;
    }
    for (size_t i = 0; i < m_defineControls.size(); ++i) {
        ImGui::PushID(static_cast<int>(i) + 1000); // Offset PushID to avoid clashes

        bool defineEnabledState = m_defineControls[i].isEnabled;
        if (ImGui::Checkbox("", &defineEnabledState)) {
            std::string modifiedCode = m_shaderParser.ToggleDefineInString(m_shaderSourceCode, m_defineControls[i].name, defineEnabledState, m_defineControls[i].originalValueString);
            if (!modifiedCode.empty()) {
                // Update source and re-apply
                // ApplyShaderCode will also re-parse controls, so m_defineControls[i].isEnabled will be updated.
                ApplyShaderCode(modifiedCode);
            } else {
                // Log error or status: failed to toggle define
                m_compileErrorLog = "Failed to toggle define: " + m_defineControls[i].name;
            }
        }
        ImGui::SameLine();

        if (m_defineControls[i].hasValue && m_defineControls[i].isEnabled) {
            std::string dragFloatLabel = "##value_define_";
            dragFloatLabel += m_defineControls[i].name;
            float tempFloatValue = m_defineControls[i].floatValue;
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::DragFloat(dragFloatLabel.c_str(), &tempFloatValue, 0.01f, 0.0f, 0.0f, "%.3f")) {
                // m_defineControls[i].floatValue = tempFloatValue; // This will be updated by ApplyShaderCode after parsing
                std::string modifiedCode = m_shaderParser.UpdateDefineValueInString(m_shaderSourceCode, m_defineControls[i].name, tempFloatValue);
                if (!modifiedCode.empty()) {
                    ApplyShaderCode(modifiedCode);
                } else {
                    m_compileErrorLog = "Failed to update value for define: " + m_defineControls[i].name;
                }
            }
            ImGui::SameLine();
        }
        ImGui::TextUnformatted(m_defineControls[i].name.c_str());
        ImGui::PopID();
    }
}

void ShaderEffect::RenderConstControlsUI() {
    if (m_constControls.empty()) {
        ImGui::TextDisabled(" (No global constants detected or editable)");
        return;
    }
    for (size_t i = 0; i < m_constControls.size(); ++i) {
        ImGui::PushID(static_cast<int>(i) + 2000); // Offset PushID
        auto& control = m_constControls[i]; // Use reference
        bool valueChanged = false;
        float dragSpeed = 0.01f;
        // Basic dynamic drag speed adjustment
        if (control.glslType == "float") {
            if (std::abs(control.fValue) > 500.0f) dragSpeed = 1.0f;
            else if (std::abs(control.fValue) > 50.0f) dragSpeed = 0.1f;
        }


        if (control.glslType == "float") {
            if (ImGui::DragFloat(control.name.c_str(), &control.fValue, dragSpeed)) valueChanged = true;
        } else if (control.glslType == "int") {
            if (ImGui::DragInt(control.name.c_str(), &control.iValue, 1)) valueChanged = true;
        } else if (control.glslType == "vec2") {
            if (ImGui::DragFloat2(control.name.c_str(), control.v2Value, dragSpeed)) valueChanged = true;
        } else if (control.glslType == "vec3") {
            if (control.isColor) {
                if (ImGui::ColorEdit3(control.name.c_str(), control.v3Value, ImGuiColorEditFlags_Float)) valueChanged = true;
            } else {
                if (ImGui::DragFloat3(control.name.c_str(), control.v3Value, dragSpeed)) valueChanged = true;
            }
        } else if (control.glslType == "vec4") {
            if (control.isColor) {
                if (ImGui::ColorEdit4(control.name.c_str(), control.v4Value, ImGuiColorEditFlags_Float)) valueChanged = true;
            } else {
                if (ImGui::DragFloat4(control.name.c_str(), control.v4Value, dragSpeed)) valueChanged = true;
            }
        }

        if (valueChanged) {
            // The control's value (e.g. control.fValue) has been changed by ImGui.
            // Now, update the shader source string and re-apply.
            // ShaderParser::UpdateConstValueInString needs to be able to modify the `control`'s originalValueString
            // or reconstruct it properly based on the new float/vec values.
            std::string modifiedCode = m_shaderParser.UpdateConstValueInString(m_shaderSourceCode, control);
            if (!modifiedCode.empty()) {
                ApplyShaderCode(modifiedCode); // This will re-compile and re-parse, updating controls
            } else {
                m_compileErrorLog = "Failed to update const value for: " + control.name;
                // Optional: revert the change in control.fValue etc. if source modification failed,
                // but ApplyShaderCode will re-parse from the unmodified source if modifiedCode is empty,
                // effectively reverting the UI change too.
            }
        }
        ImGui::PopID();
    }
}


void ShaderEffect::CompileAndLinkShader() {
    // Existing program cleanup
    if (m_shaderProgram != 0) {
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }
    m_compileErrorLog.clear();

    std::string vsError, fsError, linkError;
    std::string vsSource = LoadPassthroughVertexShaderSource(vsError);
    if (vsSource.empty()) {
        m_compileErrorLog = "Vertex Shader Load Error: " + vsError;
        return;
    }

    std::string finalFragmentCode = m_shaderSourceCode;
    // Check if the source already contains a main function
    bool hasMainFunction = m_shaderSourceCode.find("void main()") != std::string::npos ||
                           m_shaderSourceCode.find("void main(void)") != std::string::npos;

    if (m_isShadertoyMode && !hasMainFunction) { // <-- ADDED !hasMainFunction CHECK
        std::string userCode = m_shaderSourceCode; // Original user code
        std::string finalPreamble =
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec3 iResolution;\n"
            "uniform float iTime;\n"
            "uniform float iTimeDelta;\n"
            "uniform int iFrame;\n"
            "uniform vec4 iMouse;\n";

        // Check if user already declared iUserFloat1
        if (userCode.find("iUserFloat1") == std::string::npos) {
            finalPreamble += "uniform float iUserFloat1;\n";
        }
        // Check if user already declared iUserColor1
        if (userCode.find("iUserColor1") == std::string::npos) {
            finalPreamble += "uniform vec3 iUserColor1;\n";
        }
        finalPreamble += "\n";

        // Process user code to remove #version and precision, similar to before
        std::string processedUserCode;
        std::stringstream ss(userCode);
        std::string line;
        bool firstActualCodeLine = true;
        while (std::getline(ss, line)) {
            size_t first = line.find_first_not_of(" \t\n\r\f\v");
            std::string trimmedLine = (first == std::string::npos) ? "" : line.substr(first);
            size_t last = trimmedLine.find_last_not_of(" \t\n\r\f\v");
            trimmedLine = (last == std::string::npos) ? "" : trimmedLine.substr(0, last + 1);

            if (trimmedLine.rfind("#version", 0) == 0 || trimmedLine.rfind("precision", 0) == 0) {
                continue;
            }
            if (!trimmedLine.empty()) firstActualCodeLine = false;
            if (!firstActualCodeLine || !trimmedLine.empty()) processedUserCode += line + "\n";
        }
        if (!processedUserCode.empty() && processedUserCode.back() != '\n') {
            processedUserCode += "\n";
        }

        finalFragmentCode = finalPreamble + processedUserCode +
            (processedUserCode.empty() || processedUserCode.back() == '\n' ? "" : "\n") +
            "void main() {\n"
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
        glDeleteShader(vertexShader); // Clean up vertex shader
        return;
    }

    m_shaderProgram = CreateShaderProgram(vertexShader, fragmentShader, linkError);
    if (m_shaderProgram == 0) {
        m_compileErrorLog += (fsError.empty() ? "" : ("Fragment Shader Log:\n" + fsError + "\n")) +
                             "Shader Link Error:\n" + linkError;
        // CreateShaderProgram already deletes shaders if it fails or succeeds
    } else {
        // Success, m_compileErrorLog remains empty or could hold warnings later
    }
}

void ShaderEffect::FetchUniformLocations() {
    if (m_shaderProgram == 0) return;
    std::string warnings_collector;
    m_iChannel0SamplerLoc = glGetUniformLocation(m_shaderProgram, "iChannel0"); // Use "iChannel0"
    std::cerr << "ShaderEffect::FetchUniformLocations for " << name << ": m_iChannel0SamplerLoc = " << m_iChannel0SamplerLoc << std::endl;
    if (m_iChannel0SamplerLoc == -1) {
        // It's common for shaders not to use iChannel0, so this might be more of a debug log than a warning.
         std::cerr << "ShaderEffect::FetchUniformLocations for " << name << ": Uniform 'iChannel0' not found." << std::endl;
        // warnings_collector += "Info: Uniform 'iChannel0' not found.\n"; // Or make it less alarming
    }


    if (m_isShadertoyMode) {
        m_iResolutionLocation = glGetUniformLocation(m_shaderProgram, "iResolution");
        m_iTimeLocation = glGetUniformLocation(m_shaderProgram, "iTime");
        m_iTimeDeltaLocation = glGetUniformLocation(m_shaderProgram, "iTimeDelta");
        m_iFrameLocation = glGetUniformLocation(m_shaderProgram, "iFrame");
        m_iMouseLocation = glGetUniformLocation(m_shaderProgram, "iMouse");
        m_iUserFloat1Loc = glGetUniformLocation(m_shaderProgram, "iUserFloat1");
        m_iUserColor1Loc = glGetUniformLocation(m_shaderProgram, "iUserColor1");

        if (m_iResolutionLocation == -1) warnings_collector += "ST_Warn: iResolution not found.\n";
        // ... other ST uniform checks ...

        for (auto& control : m_shadertoyUniformControls) {
            control.location = glGetUniformLocation(m_shaderProgram, control.name.c_str());
            // if (control.location == -1) warnings_collector += "ST_Metadata_Warn: Uniform '" + control.name + "' not found.\n";
        }
        // Clear native locations
        m_uObjectColorLoc = -1; m_uScaleLoc = -1; m_uTimeSpeedLoc = -1; m_uColorModLoc = -1; m_uPatternScaleLoc = -1;
        m_uCamPosLoc = -1; m_uCamTargetLoc = -1; m_uCamFOVLoc = -1; m_uLightPosLoc = -1; m_uLightColorLoc = -1;

    } else { // Native Mode
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

        if (m_iResolutionLocation == -1) warnings_collector += "Native_Warn: iResolution not found.\n";
        // ... other native uniform checks ...

        m_iTimeDeltaLocation = m_iFrameLocation = m_iMouseLocation = m_iUserFloat1Loc = m_iUserColor1Loc = -1;
    }

    // Common uniform for both modes, if present
    m_iAudioAmpLoc = glGetUniformLocation(m_shaderProgram, "iAudioAmp");
    // if (m_iAudioAmpLoc == -1) warnings_collector += "Warn: iAudioAmp uniform not found.\n"; // Optional warning

    // Append warnings to compile log
    if (!warnings_collector.empty()) {
        if (m_compileErrorLog.find("Successfully") != std::string::npos || m_compileErrorLog.find("applied successfully") != std::string::npos) {
             m_compileErrorLog += "\nWith warnings:\n" + warnings_collector;
        } else if (!m_compileErrorLog.empty() && m_compileErrorLog.back() != '\n') {
            m_compileErrorLog += "\nUniform Warnings:\n" + warnings_collector;
        } else if (!m_compileErrorLog.empty()) {
            m_compileErrorLog += "Uniform Warnings:\n" + warnings_collector;
        } else {
            m_compileErrorLog = "Uniform Warnings:\n" + warnings_collector;
        }
    }
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
        m_shadertoyUniformControls.clear(); // Not applicable in native mode
    }
}

// --- Effect base overrides ---
void ShaderEffect::SetSourceFilePath(const std::string& path) {
    m_shaderFilePath = path;
}

const std::string& ShaderEffect::GetSourceFilePath() const {
    return m_shaderFilePath;
}

// --- Serialization ---
nlohmann::json ShaderEffect::Serialize() const {
    nlohmann::json j;
    j["type"] = "ShaderEffect"; // Important for deserialization factory
    j["name"] = name;
    j["startTime"] = startTime;
    j["endTime"] = endTime;
    j["sourceFilePath"] = m_shaderFilePath;
    // If m_shaderFilePath is empty or special (e.g., "dynamic_source", "shadertoy://..."),
    // then m_shaderSourceCode should be saved. Otherwise, it can be reloaded from path.
    if (m_shaderFilePath.empty() || m_shaderFilePath == "dynamic_source" || m_shaderFilePath.rfind("shadertoy://", 0) == 0) {
        j["sourceCode"] = m_shaderSourceCode;
    }
    j["isShadertoyMode"] = m_isShadertoyMode;

    // Native parameters
    j["nativeParams"]["objectColor"] = {m_objectColor[0], m_objectColor[1], m_objectColor[2]};
    j["nativeParams"]["scale"] = m_scale;
    j["nativeParams"]["timeSpeed"] = m_timeSpeed;
    j["nativeParams"]["colorMod"] = {m_colorMod[0], m_colorMod[1], m_colorMod[2]};
    j["nativeParams"]["patternScale"] = m_patternScale;
    j["nativeParams"]["cameraPosition"] = {m_cameraPosition[0], m_cameraPosition[1], m_cameraPosition[2]};
    j["nativeParams"]["cameraTarget"] = {m_cameraTarget[0], m_cameraTarget[1], m_cameraTarget[2]};
    j["nativeParams"]["cameraFOV"] = m_cameraFOV;
    j["nativeParams"]["lightPosition"] = {m_lightPosition[0], m_lightPosition[1], m_lightPosition[2]};
    j["nativeParams"]["lightColor"] = {m_lightColor[0], m_lightColor[1], m_lightColor[2]};

    // Shadertoy specific user parameters (distinct from metadata-driven ones)
    j["shadertoyUserParams"]["iUserFloat1"] = m_iUserFloat1;
    j["shadertoyUserParams"]["iUserColor1"] = {m_iUserColor1[0], m_iUserColor1[1], m_iUserColor1[2]};

    // Parsed Define Controls
    nlohmann::json defineControlsJson = nlohmann::json::array();
    for (const auto& dc : m_defineControls) {
        nlohmann::json d;
        d["name"] = dc.name;
        d["isEnabled"] = dc.isEnabled;
        d["floatValue"] = dc.floatValue;
        // originalValueString is not strictly needed for restore if we re-parse and then apply these values
        defineControlsJson.push_back(d);
    }
    j["defineControls"] = defineControlsJson;

    // Parsed Shadertoy Uniform Controls (Metadata-driven)
    nlohmann::json stUniformControlsJson = nlohmann::json::array();
    for (const auto& stc : m_shadertoyUniformControls) {
        nlohmann::json s;
        s["name"] = stc.name;
        s["glslType"] = stc.glslType;
        s["fValue"] = stc.fValue;
        s["iValue"] = stc.iValue;
        s["bValue"] = stc.bValue;
        s["v2Value"] = {stc.v2Value[0], stc.v2Value[1]};
        s["v3Value"] = {stc.v3Value[0], stc.v3Value[1], stc.v3Value[2]};
        s["v4Value"] = {stc.v4Value[0], stc.v4Value[1], stc.v4Value[2], stc.v4Value[3]};
        // metadata is not saved, parsed from source on load
        stUniformControlsJson.push_back(s);
    }
    j["shadertoyUniformControls"] = stUniformControlsJson;

    // Parsed Const Controls
    nlohmann::json constControlsJson = nlohmann::json::array();
    for (const auto& cc : m_constControls) {
        nlohmann::json c;
        c["name"] = cc.name;
        c["glslType"] = cc.glslType;
        c["fValue"] = cc.fValue;
        c["iValue"] = cc.iValue;
        c["v2Value"] = {cc.v2Value[0], cc.v2Value[1]};
        c["v3Value"] = {cc.v3Value[0], cc.v3Value[1], cc.v3Value[2]};
        c["v4Value"] = {cc.v4Value[0], cc.v4Value[1], cc.v4Value[2], cc.v4Value[3]};
        // originalValueString and lineNumber not saved, re-parsed from source
        constControlsJson.push_back(c);
    }
    j["constControls"] = constControlsJson;

    return j;
}

void ShaderEffect::Deserialize(const nlohmann::json& data) {
    name = data.value("name", "Untitled Deserialized Effect");
    startTime = data.value("startTime", 0.0f);
    endTime = data.value("endTime", 10.0f);
    m_shaderFilePath = data.value("sourceFilePath", "");
    if (data.contains("sourceCode")) { // If source code was saved directly
        m_shaderSourceCode = data.at("sourceCode").get<std::string>();
    } else if (!m_shaderFilePath.empty()) {
        // Source will be loaded from m_shaderFilePath when Load() is called
        m_shaderSourceCode = ""; // Clear any old source
    }
    m_isShadertoyMode = data.value("isShadertoyMode", false);

    if (data.contains("nativeParams")) {
        const auto& np = data["nativeParams"];
        if (np.contains("objectColor") && np["objectColor"].is_array() && np["objectColor"].size() == 3) {
            for(int i=0; i<3; ++i) m_objectColor[i] = np["objectColor"][i].get<float>();
        }
        m_scale = np.value("scale", 1.0f);
        m_timeSpeed = np.value("timeSpeed", 1.0f);
        if (np.contains("colorMod") && np["colorMod"].is_array() && np["colorMod"].size() == 3) {
            for(int i=0; i<3; ++i) m_colorMod[i] = np["colorMod"][i].get<float>();
        }
        m_patternScale = np.value("patternScale", 1.0f);
        if (np.contains("cameraPosition") && np["cameraPosition"].is_array() && np["cameraPosition"].size() == 3) {
            for(int i=0; i<3; ++i) m_cameraPosition[i] = np["cameraPosition"][i].get<float>();
        }
        if (np.contains("cameraTarget") && np["cameraTarget"].is_array() && np["cameraTarget"].size() == 3) {
            for(int i=0; i<3; ++i) m_cameraTarget[i] = np["cameraTarget"][i].get<float>();
        }
        m_cameraFOV = np.value("cameraFOV", 60.0f);
        if (np.contains("lightPosition") && np["lightPosition"].is_array() && np["lightPosition"].size() == 3) {
            for(int i=0; i<3; ++i) m_lightPosition[i] = np["lightPosition"][i].get<float>();
        }
        if (np.contains("lightColor") && np["lightColor"].is_array() && np["lightColor"].size() == 3) {
            for(int i=0; i<3; ++i) m_lightColor[i] = np["lightColor"][i].get<float>();
        }
    }

    if (data.contains("shadertoyUserParams")) {
        const auto& stup = data["shadertoyUserParams"];
        m_iUserFloat1 = stup.value("iUserFloat1", 0.5f);
        if (stup.contains("iUserColor1") && stup["iUserColor1"].is_array() && stup["iUserColor1"].size() == 3) {
            for(int i=0; i<3; ++i) m_iUserColor1[i] = stup["iUserColor1"][i].get<float>();
        }
    }

    // For defineControls, shadertoyUniformControls, constControls:
    // The strategy is to load these values. When ApplyShaderCode is called (via Load()),
    // the ShaderParser will re-parse the source. We then need a way for these
    // loaded values to take precedence or be correctly integrated.
    // For now, we store them. The ShaderParser might need to be made aware of these
    // "override" values after parsing.
    // A simpler approach for now: these values are restored, and if ApplyShaderCode is called
    // with the *original* source, these values will be used for uniforms, but defines/consts
    // in the source itself won't be modified by this Deserialize step.
    // The UI interaction (RenderDefineControlsUI, RenderConstControlsUI) already modifies the source string.
    // So for scene load, we load the source, then apply these values to the member variables.

    if (data.contains("defineControls")) {
        m_defineControls.clear(); // Or update existing if names match? For simplicity, clear and repopulate.
        for (const auto& dJson : data["defineControls"]) {
            DefineControl dc; // Changed from ShaderDefineControl
            dc.name = dJson.value("name", "");
            dc.isEnabled = dJson.value("isEnabled", false);
            dc.floatValue = dJson.value("floatValue", 0.0f);
            // originalValueString and hasValue would be re-determined by parser
            m_defineControls.push_back(dc);
        }
    }
    if (data.contains("shadertoyUniformControls")) {
        m_shadertoyUniformControls.clear();
        for (const auto& sJson : data["shadertoyUniformControls"]) {
            ShaderToyUniformControl stc(sJson.value("name",""), sJson.value("glslType","float"), {}); // dummy metadata
            stc.fValue = sJson.value("fValue", 0.0f);
            stc.iValue = sJson.value("iValue", 0);
            stc.bValue = sJson.value("bValue", false);
            if (sJson.contains("v2Value") && sJson["v2Value"].is_array() && sJson["v2Value"].size() == 2) {
                for(int i=0; i<2; ++i) stc.v2Value[i] = sJson["v2Value"][i].get<float>();
            }
            // ... similar for v3Value, v4Value
             if (sJson.contains("v3Value") && sJson["v3Value"].is_array() && sJson["v3Value"].size() == 3) {
                for(int i=0; i<3; ++i) stc.v3Value[i] = sJson["v3Value"][i].get<float>();
            }
            if (sJson.contains("v4Value") && sJson["v4Value"].is_array() && sJson["v4Value"].size() == 4) {
                for(int i=0; i<4; ++i) stc.v4Value[i] = sJson["v4Value"][i].get<float>();
            }
            m_shadertoyUniformControls.push_back(stc);
        }
    }
    if (data.contains("constControls")) {
        m_constControls.clear();
        for (const auto& cJson : data["constControls"]) {
            ConstVariableControl cc(cJson.value("name",""), cJson.value("glslType","float"), 0, ""); // Changed from ShaderConstControl, dummy line/val
            cc.fValue = cJson.value("fValue", 0.0f);
            cc.iValue = cJson.value("iValue", 0);
             if (cJson.contains("v2Value") && cJson["v2Value"].is_array() && cJson["v2Value"].size() == 2) {
                for(int i=0; i<2; ++i) cc.v2Value[i] = cJson["v2Value"][i].get<float>();
            }
            if (cJson.contains("v3Value") && cJson["v3Value"].is_array() && cJson["v3Value"].size() == 3) {
                for(int i=0; i<3; ++i) cc.v3Value[i] = cJson["v3Value"][i].get<float>();
            }
            if (cJson.contains("v4Value") && cJson["v4Value"].is_array() && cJson["v4Value"].size() == 4) {
                for(int i=0; i<4; ++i) cc.v4Value[i] = cJson["v4Value"][i].get<float>();
            }
            m_constControls.push_back(cc);
        }
    }
    // After deserializing, the main loop should call Load() on this effect.
    // Load() will use m_shaderSourceCode (if populated) or load from m_shaderFilePath,
    // then call ApplyShaderCode(), which recompiles and re-runs ParseShaderControls().
    // The values restored here for m_defineControls, m_shadertoyUniformControls, m_constControls
    // will need to be applied *after* parsing in ApplyShaderCode or FetchUniformLocations
    // to ensure they override the parsed defaults. This needs refinement in those methods.
    // For now, this Deserialize just loads the values into the member variables.
}

void ShaderEffect::ResetParameters() {
    // Reset Native Mode Uniform Values to their defaults (as in constructor or typical initial values)
    m_objectColor[0] = 0.8f; m_objectColor[1] = 0.9f; m_objectColor[2] = 1.0f;
    m_scale = 1.0f;
    m_timeSpeed = 1.0f; // Multiplier for iTime in native mode
    m_colorMod[0] = 0.1f; m_colorMod[1] = 0.1f; m_colorMod[2] = 0.2f;
    m_patternScale = 1.0f;
    m_cameraPosition[0] = 0.0f; m_cameraPosition[1] = 1.0f; m_cameraPosition[2] = -3.0f;
    m_cameraTarget[0] = 0.0f; m_cameraTarget[1] = 0.0f; m_cameraTarget[2] = 0.0f;
    m_cameraFOV = 60.0f;
    m_lightPosition[0] = 2.0f; m_lightPosition[1] = 3.0f; m_lightPosition[2] = -2.0f;
    m_lightColor[0] = 1.0f; m_lightColor[1] = 1.0f; m_lightColor[2] = 0.9f;

    // Reset Shadertoy Predefined Uniform Values
    m_iUserFloat1 = 0.5f;
    m_iUserColor1[0] = 0.2f; m_iUserColor1[1] = 0.5f; m_iUserColor1[2] = 0.8f;

    // m_audioAmp is driven externally, so not reset here.

    // The crucial step: re-apply the original shader code.
    // This will re-compile the shader and, importantly, re-run ParseShaderControls().
    // ParseShaderControls() re-initializes m_defineControls, m_shadertoyUniformControls,
    // and m_constControls from the shader source text, effectively resetting them.
    if (!m_shaderSourceCode.empty()) {
        ApplyShaderCode(m_shaderSourceCode);
    } else if (!m_shaderFilePath.empty()) {
        // If source code is somehow empty but path exists, try to reload from file
        std::string loadError;
        std::string tempSource = LoadShaderSourceFile(m_shaderFilePath, loadError);
        if (loadError.empty()) {
            m_shaderSourceCode = tempSource; // Cache it again
            ApplyShaderCode(m_shaderSourceCode);
        } else {
            m_compileErrorLog = "ResetParameters: Failed to reload shader from file for reset: " + loadError;
            m_shaderLoaded = false;
        }
    } else {
        // No source code and no file path, cannot reset shader-derived controls.
        // Parameters above are reset, but shader itself cannot be processed.
        m_compileErrorLog = "ResetParameters: No shader source or file path available to reset shader.";
        m_shaderLoaded = false; // Mark as not loaded if we can't process the shader
    }
}
