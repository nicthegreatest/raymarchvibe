#pragma once

#include <string>
#include <vector> // If managing shaderSamples or similar
#include <glad/glad.h> // For GLint, GLuint
// Forward declare dependencies if possible, otherwise include
#include "Renderer.h"
#include "ShaderParser.h"

class ShaderManager {
public:
    ShaderManager(Renderer& renderer, ShaderParser& shaderParser);

    // Loads shader source from a file
    std::string loadShaderSourceFromFile(const char* filePath);

    // Applies the provided fragment shader code.
    // Vertex shader is assumed to be a standard passthrough, loaded internally.
    // Returns true on success, false on failure.
    bool applyShaderCode(const std::string& fragmentShaderCode, bool isShadertoyMode);

    // Getters for status/error messages
    const std::string& getCompileErrorLog() const { return m_shaderCompileErrorLog; }
    const std::string& getGeneralStatus() const { return m_generalStatus; }

    // Getter for the currently active shader program managed by this class
    GLuint getActiveShaderProgram() const; // This would be the program for the raymarch scene

    // --- Uniform Location Getters (Consider if these are needed directly by main) ---
    // It might be cleaner for ShaderManager to also handle updating these based on UI values.
    // For now, providing getters allows main.cpp to continue its current update logic.
    GLint getResolutionLocation() const { return m_iResolutionLocation; }
    GLint getTimeLocation() const { return m_iTimeLocation; }
    GLint getObjectColorLocation() const { return m_u_objectColorLocation; }
    GLint getScaleLocation() const { return m_u_scaleLocation; }
    GLint getTimeSpeedLocation() const { return m_u_timeSpeedLocation; }
    GLint getColorModLocation() const { return m_u_colorModLocation; }
    GLint getPatternScaleLocation() const { return m_u_patternScaleLocation; }
    GLint getCamPosLocation() const { return m_u_camPosLocation; }
    GLint getCamTargetLocation() const { return m_u_camTargetLocation; }
    GLint getCamFOVLocation() const { return m_u_camFOVLocation; }
    GLint getLightPositionLocation() const { return m_u_lightPositionLocation; }
    GLint getLightColorLocation() const { return m_u_lightColorLocation; }
    GLint getTimeDeltaLocation() const { return m_iTimeDeltaLocation; }
    GLint getFrameLocation() const { return m_iFrameLocation; }
    GLint getMouseLocation() const { return m_iMouseLocation; }
    GLint getUserFloat1Location() const { return m_iUserFloat1Location; }
    GLint getUserColor1Location() const { return m_iUserColor1Location; }
    GLint getAudioAmpLocation() const { return m_iAudioAmpLocation; }


private:
    Renderer& m_renderer;
    ShaderParser& m_shaderParser;

    std::string m_shaderCompileErrorLog;
    std::string m_generalStatus; // Corresponds to generalStatusError_ref/shaderLoadError when it's about application status

    // Store the active program ID that this manager is responsible for
    GLuint m_activeRaymarchProgram = 0;


    // --- Uniform Locations (Owned by ShaderManager) ---
    GLint m_iResolutionLocation = -1, m_iTimeLocation = -1;
    GLint m_u_objectColorLocation = -1, m_u_scaleLocation = -1, m_u_timeSpeedLocation = -1;
    GLint m_u_colorModLocation = -1, m_u_patternScaleLocation = -1;
    GLint m_u_camPosLocation = -1, m_u_camTargetLocation = -1, m_u_camFOVLocation = -1;
    GLint m_u_lightPositionLocation = -1, m_u_lightColorLocation = -1;
    GLint m_iTimeDeltaLocation = -1, m_iFrameLocation = -1, m_iMouseLocation = -1;
    GLint m_iUserFloat1Location = -1;
    GLint m_iUserColor1Location = -1;
    GLint m_iAudioAmpLocation = -1;

    // Private helper method moved from main.cpp (was ApplyShaderFromEditorLogic_FetchUniforms)
    void fetchAndStoreUniformLocations(GLuint program, bool isShadertoyMode, std::string& warnings);
};