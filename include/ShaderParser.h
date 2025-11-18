#pragma once

#include <string>
#include <vector>
#include <map>
#include <glad/glad.h>      // For GLint
#include <nlohmann/json.hpp> // For json
#include <cstdint>           // For uint8_t, uint64_t
#include "TextEditor.h"      // ImGuiColorTextEdit include
#include <glm/glm.hpp>       // For glm::vec3

using json = nlohmann::json;

// --- Struct Definitions ---

struct DefineControl {
    std::string name;
    std::string originalValueString;
    float floatValue = 0.0f;
    bool hasValue = false;
    bool isEnabled = true;
    int originalLine = -1;
    json metadata;        
};

struct ShaderToyUniformControl {
    std::string name;
    std::string glslType; 
    GLint location = -1;  
    json metadata;        

    float fValue = 0.0f;
    float fCurrentValue = 0.0f;
    float v2Value[2] = {0.0f, 0.0f};
    float v3Value[3] = {0.0f, 0.0f, 0.0f};
    float v4Value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int iValue = 0;
    bool bValue = false;
    bool isColor = false; 
    bool smooth = false;

    // Palette-related fields
    bool isPalette = false;
    int paletteMode = 0;  // 0=Individual, 1=Palette, 2=Sync
    int selectedHarmonyType = 0;  // Index into harmony types
    std::vector<glm::vec3> generatedPalette;
    bool gradientMode = false;
    std::vector<glm::vec3> gradientColors;

    ShaderToyUniformControl(const std::string& n, const std::string& type_str, const std::string& default_val_str, const json& meta);
};

struct ConstVariableControl {
    std::string name;
    std::string glslType;
    std::string originalValueString;
    int lineIndex = -1;
    size_t charPosition = 0;

    float fValue = 0.0f;
    int iValue = 0;
    float v2Value[2] = {0.0f, 0.0f};
    float v3Value[3] = {0.0f, 0.0f, 0.0f};
    float v4Value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    bool isColor = false;
    float multiplier = 1.0f;

    ConstVariableControl() = default; // Add default constructor if needed elsewhere
    ConstVariableControl(const std::string& name, const std::string& glslType, int lineIndex, const std::string& originalValueString);
};


class ShaderParser {
public:
    ShaderParser();
    ~ShaderParser();

    TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log);

    // --- Define Controls ---
    void ScanAndPrepareDefineControls(const std::string& shaderCode); // Changed to const std::string&
    const std::vector<DefineControl>& GetDefineControls() const;
    std::vector<DefineControl>& GetDefineControls(); // Should this be const? Probably for getting.
    std::string ToggleDefineInString(const std::string& shaderCode, const std::string& defineName, bool enable, const std::string& originalValue);
    std::string UpdateDefineValueInString(const std::string& shaderCode, const std::string& defineName, float newValue);

    // --- Shadertoy Uniform Controls ---
    void ScanAndPrepareUniformControls(const std::string& shaderCode); // Changed to const std::string&
    const std::vector<ShaderToyUniformControl>& GetUniformControls() const;
    std::vector<ShaderToyUniformControl>& GetUniformControls(); // Should this be const?
    void ClearAllControls();

    // --- Const Variable Controls ---
    void ScanAndPrepareConstControls(const std::string& shaderCode);
    const std::vector<ConstVariableControl>& GetConstControls() const;
    std::vector<ConstVariableControl>& GetConstControls(); // Should this be const?
    std::string UpdateConstValueInString(const std::string& shaderCode, const ConstVariableControl& control);

private:
    std::vector<DefineControl> m_defineControls;
    std::vector<ShaderToyUniformControl> m_uniformControls;
    std::vector<ConstVariableControl> m_constControls;

    void ParseConstValueString(const std::string& valueStr, ConstVariableControl& control);
    std::string ReconstructConstValueString(const ConstVariableControl& control) const;

    // The local static trim function has been removed.
};
