#pragma once

#include <string>
#include <vector>
#include "ShaderParameterControls.h" // Needs access to the control structs

class ShaderParser {
public:
    ShaderParser();
    ~ShaderParser();

    // Parses shader source to find #define statements and populate ShaderDefineControl structures.
    void ScanAndPrepareDefineControls(const std::string& shaderSource, std::vector<ShaderDefineControl>& defineControls);

    // Parses shader source for Shadertoy-style metadata comments to populate ShaderToyUniformControl structures.
    void ScanAndPrepareUniformControls(const std::string& shaderSource, std::vector<ShaderToyUniformControl>& uniformControls);

    // Parses shader source for 'const type NAME = value;' declarations to populate ShaderConstControl structures.
    void ScanAndPrepareConstControls(const std::string& shaderSource, std::vector<ShaderConstControl>& constControls);

    // Helper to parse GLSL error logs for error markers in a text editor
    // This might live elsewhere or be static, but fits the "parsing" theme.
    // For now, let's assume it's part of this parser's responsibilities.
    // TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log); // TextEditor dependency

    // Utility to extract Shadertoy ID from URL or string
    static std::string ExtractShaderId(const std::string& idOrUrl);

    // Functions to modify shader code strings (for defines, consts)
    // These might be better as static utility functions or part of ShaderEffect if it directly manipulates its source string.
    // For now, placing them here conceptually.
    std::string ToggleDefineInString(const std::string& sourceCode, const std::string& defineName, bool enable, const std::string& originalValueStringIfKnown);
    std::string UpdateDefineValueInString(const std::string& sourceCode, const std::string& defineName, float newValue);
    std::string UpdateConstValueInString(const std::string& sourceCode, ShaderConstControl& control /* may need to update control.originalValueString */);

private:
    // Internal helper methods for parsing if needed
};
