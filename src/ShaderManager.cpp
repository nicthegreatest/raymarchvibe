#include "ShaderManager.h"
#include <fstream>
#include <sstream>
#include <iostream> // For cerr in loadShaderSourceFromFile (consider a proper logging strategy later)

// Constructor
ShaderManager::ShaderManager(Renderer& renderer, ShaderParser& shaderParser)
    : m_renderer(renderer), m_shaderParser(shaderParser) {}

// Definition of loadShaderSourceFromFile (moved from global scope in main.cpp)
std::string ShaderManager::loadShaderSourceFromFile(const char* filePath) {
    std::ifstream shaderFile;
    std::stringstream shaderStream;
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        shaderFile.open(filePath);
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
    } catch (std::ifstream::failure& e) {
        // It's better to propagate this error via return or an error member
        // For now, matching main.cpp's style:
        std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << filePath << " - " << e.what() << std::endl;
        m_generalStatus = "ERROR: Failed to read shader file: " + std::string(filePath); // Store error
        return "";
    }
    return shaderStream.str();
}


// Definition of fetchAndStoreUniformLocations (moved and adapted from ApplyShaderFromEditorLogic_FetchUniforms)
void ShaderManager::fetchAndStoreUniformLocations(
    GLuint program, bool isShadertoyMode, std::string& warnings)
{
    // Use m_ prefix for member variables
    if (isShadertoyMode) {
        m_iResolutionLocation = glGetUniformLocation(program, "iResolution");
        m_iTimeLocation = glGetUniformLocation(program, "iTime");
        m_iTimeDeltaLocation = glGetUniformLocation(program, "iTimeDelta");
        m_iFrameLocation = glGetUniformLocation(program, "iFrame");
        m_iMouseLocation = glGetUniformLocation(program, "iMouse");
        m_iUserFloat1Location = glGetUniformLocation(program, "iUserFloat1");
        m_iUserColor1Location = glGetUniformLocation(program, "iUserColor1");
        m_iAudioAmpLocation = glGetUniformLocation(program, "iAudioAmp");

        if(m_iResolutionLocation == -1) warnings += "STW:iR not found\n";
        if(m_iTimeLocation == -1) warnings += "STW:iT not found\n";
        // Add checks for other Shadertoy specific uniforms if they are always expected or commonly used.
        if(m_iUserFloat1Location == -1 && warnings.find("STW:iUF1") == std::string::npos) warnings += "STW:iUF1 not found\n";
        if(m_iUserColor1Location == -1 && warnings.find("STW:iUC1") == std::string::npos) warnings += "STW:iUC1 not found\n";
        if(m_iAudioAmpLocation == -1 && warnings.find("STW:iAA") == std::string::npos) warnings += "STW:iAA not found\n";
        
        // Update locations in ShaderParser's controls
        for (auto& control : m_shaderParser.GetUniformControls()) { // Assuming GetUniformControls is non-const or provides a way to update
            control.location = glGetUniformLocation(program, control.name.c_str());
        }

    } else { // Native Mode
        m_iResolutionLocation = glGetUniformLocation(program, "iResolution");
        m_iTimeLocation = glGetUniformLocation(program, "iTime");
        m_u_objectColorLocation = glGetUniformLocation(program, "u_objectColor");
        m_u_scaleLocation = glGetUniformLocation(program, "u_scale");
        m_u_timeSpeedLocation = glGetUniformLocation(program, "u_timeSpeed");
        m_u_colorModLocation = glGetUniformLocation(program, "u_colorMod");
        m_u_patternScaleLocation = glGetUniformLocation(program, "u_patternScale");
        m_u_camPosLocation = glGetUniformLocation(program, "u_camPos");
        m_u_camTargetLocation = glGetUniformLocation(program, "u_camTarget");
        m_u_camFOVLocation = glGetUniformLocation(program, "u_camFOV");
        m_u_lightPositionLocation = glGetUniformLocation(program, "u_lightPosition");
        m_u_lightColorLocation = glGetUniformLocation(program, "u_lightColor");
        m_iAudioAmpLocation = glGetUniformLocation(program, "iAudioAmp");
        
        // Reset Shadertoy specific ones not used in native mode to -1 if desired
        m_iTimeDeltaLocation = -1;
        m_iFrameLocation = -1;
        m_iMouseLocation = -1;
        m_iUserFloat1Location = -1;
        m_iUserColor1Location = -1;

        if(m_iResolutionLocation == -1 && warnings.find("W:iR") == std::string::npos) warnings += "W:iR not found\n";
        if(m_iTimeLocation == -1 && warnings.find("W:iT") == std::string::npos) warnings += "W:iT not found\n";
        if(m_u_objectColorLocation == -1 && warnings.find("W:uOC") == std::string::npos) warnings += "W:uOC not found\n";
        // ... add other warnings for native uniforms
        if(m_iAudioAmpLocation == -1 && warnings.find("W:iAA") == std::string::npos) warnings += "W:iAA not found\n";
    }
}

// Definition of applyShaderCode (moved and adapted from ApplyShaderFromEditor)
bool ShaderManager::applyShaderCode(const std::string& fragmentShaderCode, bool isShadertoyMode) {
    m_shaderCompileErrorLog.clear();
    m_generalStatus.clear();

    std::string vsSource = loadShaderSourceFromFile("shaders/passthrough.vert");
    if (vsSource.empty()) {
        m_shaderCompileErrorLog = "CRITICAL: Vertex shader (shaders/passthrough.vert) load failed.";
        m_generalStatus = m_shaderCompileErrorLog;
        return false;
    }

    std::string finalFragmentCode;
    // Logic for preparing finalFragmentCode (Shadertoy wrapper, etc.)
    // This part is adapted from ApplyShaderFromEditor in main.cpp
    if (isShadertoyMode) {
        std::string processedUserCode;
        std::stringstream ss(fragmentShaderCode);
        std::string line;
        bool firstActualCodeLine = true;

        // Assuming Utils::trim or a local trim function is available
        // auto trim_func = [](const std::string& s){ /* ... trim logic ... */ return s; };


        while (std::getline(ss, line)) {
            // You'll need a trim function here. For now, let's assume it exists or you adapt it.
            // std::string trimmedLine = YourTrimFunction(line);
            // For simplicity, I'm omitting the trim call here, but it's important.
            // A proper trim should be used as in your original main.cpp.
            std::string trimmedLine = line; // Placeholder - use actual trim
            size_t first_char = trimmedLine.find_first_not_of(" \t\n\r\f\v");
            if (first_char != std::string::npos) {
                trimmedLine = trimmedLine.substr(first_char);
            } else {
                trimmedLine.clear();
            }


            if (trimmedLine.rfind("#version", 0) == 0 || trimmedLine.rfind("precision", 0) == 0) {
                continue;
            }
            if (!trimmedLine.empty()) { firstActualCodeLine = false; }
            if (!firstActualCodeLine || !trimmedLine.empty()) { processedUserCode += line + "\n"; }
        }
        if (!processedUserCode.empty() && processedUserCode.back() != '\n') { processedUserCode += "\n"; }

        finalFragmentCode =
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec3 iResolution;\n" // Note: Shadertoy often uses vec3 for iResolution (w,h,aspect)
            "uniform float iTime;\n"
            "uniform float iTimeDelta;\n"
            "uniform int iFrame;\n"
            "uniform vec4 iMouse;\n"
            "uniform float iAudioAmp;\n"
            // Potentially add iUserFloat1, iUserColor1 etc. if always expected by wrapper
            "\n" +
            processedUserCode +
            (processedUserCode.empty() || processedUserCode.back() == '\n' ? "" : "\n") +
            "\nvoid main() {\n"
            "    mainImage(FragColor, gl_FragCoord.xy);\n"
            "}\n";
    } else {
        finalFragmentCode = fragmentShaderCode;
    }

    std::string fsError, vsError, linkError;
    GLuint newFragmentShader = m_renderer.CompileShader(finalFragmentCode.c_str(), GL_FRAGMENT_SHADER, fsError);
    if (newFragmentShader == 0) {
        m_shaderCompileErrorLog = "FS compile failed:\n" + fsError;
        m_generalStatus = m_shaderCompileErrorLog;
        return false;
    }

    GLuint tempVertexShader = m_renderer.CompileShader(vsSource.c_str(), GL_VERTEX_SHADER, vsError);
    if (tempVertexShader == 0) {
        m_shaderCompileErrorLog = "VS compile failed:\n" + vsError;
        m_generalStatus = m_shaderCompileErrorLog;
        glDeleteShader(newFragmentShader);
        return false;
    }

    GLuint newShaderProgram = m_renderer.CreateShaderProgram(tempVertexShader, newFragmentShader, linkError);
    // Compiled shaders are linked, so they can be deleted now
    glDeleteShader(newFragmentShader);
    glDeleteShader(tempVertexShader);

    if (newShaderProgram != 0) {
        GLuint oldProgram = m_activeRaymarchProgram; // Use member variable
        if (oldProgram != 0 && oldProgram != newShaderProgram) {
            glDeleteProgram(oldProgram);
        }
        m_activeRaymarchProgram = newShaderProgram; // Store the new program
        // No need to call renderer.SetActiveShaderProgram here if the renderer's SetActiveShaderProgram
        // is only for a temporary context. The main render loop will use getActiveShaderProgram().

        std::string uniformWarnings;
        // These ScanAndPrepare calls should update the m_shaderParser's internal state.
        m_shaderParser.ScanAndPrepareDefineControls(fragmentShaderCode.c_str());
        if (isShadertoyMode) {
            m_shaderParser.ScanAndPrepareUniformControls(fragmentShaderCode.c_str());
        } else {
            m_shaderParser.GetUniformControls().clear(); // Or a ClearUniformControls() method
        }
        m_shaderParser.ScanAndPrepareConstControls(fragmentShaderCode);

        // Call the new member function to fetch and store locations
        fetchAndStoreUniformLocations(m_activeRaymarchProgram, isShadertoyMode, uniformWarnings);

        if (!uniformWarnings.empty()) {
            m_generalStatus = "Applied with warnings:\n" + uniformWarnings;
            m_shaderCompileErrorLog = m_generalStatus; // Or just keep it for uniform warnings
        } else {
            m_generalStatus = "Applied from editor!";
            m_shaderCompileErrorLog.clear();
        }
        return true;
    } else {
        m_shaderCompileErrorLog = "Link failed:\n" + linkError;
        m_generalStatus = m_shaderCompileErrorLog;
        // newFragmentShader and tempVertexShader are already deleted or would be if linking failed earlier
        return false;
    }
}

GLuint ShaderManager::getActiveShaderProgram() const {
    return m_activeRaymarchProgram;
}