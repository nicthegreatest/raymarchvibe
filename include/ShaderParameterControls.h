#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp> // For ShaderToyUniformControl metadata
#include <glad/glad.h>       // For GLint in ShaderToyUniformControl

// --- Shader Define Controls ---
struct ShaderDefineControl {
    std::string name;
    bool isEnabled = false;
    int lineNumber = -1;
    bool hasValue = false;
    float floatValue = 0.0f;
    std::string originalValueString;
    // Potential future additions: type, min/max for floatValue if it becomes more generic
};

// --- ShaderToy Uniform Controls (from metadata comments) ---
struct ShaderToyUniformControl {
    std::string name;
    std::string glslType;
    nlohmann::json metadata; // Stores label, min, max, default, widget type etc.
    GLint location = -1;

    // Storage for different types of uniform values
    float fValue = 0.0f;
    float v2Value[2] = {0.0f, 0.0f};
    float v3Value[3] = {0.0f, 0.0f, 0.0f};
    float v4Value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int iValue = 0;
    bool bValue = false;

    bool isColor = false; // Hint for UI (e.g., use ColorEdit for vec3/vec4)

    ShaderToyUniformControl(const std::string& n, const std::string& type_str, const nlohmann::json& meta);
    // Default constructor
    ShaderToyUniformControl() = default;
};

// --- Shader Const Controls (for 'const float NAME = value;') ---
struct ShaderConstControl {
    std::string name;
    std::string glslType;
    int lineNumber = -1;
    std::string originalValueString; // e.g., "0.6" or "vec3(0.1, 0.2, 0.3)"

    // Parsed values for UI manipulation
    float fValue = 0.0f;
    int iValue = 0;
    float v2Value[2] = {0.0f, 0.0f};
    float v3Value[3] = {0.0f, 0.0f, 0.0f};
    float v4Value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float multiplier = 1.0f; // For parsing expressions like vecN(...) * scalar

    bool isColor = false; // Heuristic for vec3/vec4 color pickers

    ShaderConstControl(const std::string& n, const std::string& type, int line, const std::string& valStr);
    // Default constructor
    ShaderConstControl() = default;
};
