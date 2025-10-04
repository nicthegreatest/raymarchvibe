#include "ShaderParser.h"
#include "Utils.h"

#include <sstream>
#include <regex>
#include <algorithm> 
#include <iomanip>   
#include <iostream>

ShaderToyUniformControl::ShaderToyUniformControl(const std::string& n, const std::string& type_str, const std::string& default_val_str, const json& meta)
    : name(n), glslType(type_str), metadata(meta) {
    location = -1; 
    isColor = metadata.value("widget", "") == "color" || metadata.value("type", "") == "color";

    // Helper to parse numbers from strings like "vec3(1.0, 0.5, 0.2)"
    auto extract_numbers = [](const std::string& s, int count) -> std::vector<float> {
        std::vector<float> nums;
        std::regex num_regex(R"([-+]?[0-9]*\.?[0-9]+f?)");
        auto words_begin = std::sregex_iterator(s.begin(), s.end(), num_regex);
        auto words_end = std::sregex_iterator();
        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            if (nums.size() < (size_t)count) {
                try {
                    nums.push_back(std::stof((*i).str()));
                } catch (...) {
                    // Ignore conversion errors
                }
            }
        }
        while (nums.size() < (size_t)count) {
            nums.push_back(0.0f);
        }
        return nums;
    };

    // 1. Set initial values from GLSL default string
    if (!default_val_str.empty()) {
        if (glslType == "float") {
            try { fValue = std::stof(default_val_str); } catch (...) { fValue = 0.0f; }
        } else if (glslType == "int" || glslType == "bool") {
            try { iValue = std::stoi(default_val_str); } catch (...) { iValue = 0; }
            if (glslType == "bool") bValue = (iValue != 0);
        } else if (glslType == "vec2") {
            auto nums = extract_numbers(default_val_str, 2);
            v2Value[0] = nums[0]; v2Value[1] = nums[1];
        } else if (glslType == "vec3") {
            auto nums = extract_numbers(default_val_str, 3);
            v3Value[0] = nums[0]; v3Value[1] = nums[1]; v3Value[2] = nums[2];
        } else if (glslType == "vec4") {
            auto nums = extract_numbers(default_val_str, 4);
            v4Value[0] = nums[0]; v4Value[1] = nums[1]; v4Value[2] = nums[2]; v4Value[3] = nums[3];
        }
    } else {
        // Fallback to zero if no GLSL default is present
        fValue = 0.0f; iValue = 0; bValue = false;
        std::fill_n(v2Value, 2, 0.0f);
        std::fill_n(v3Value, 3, 0.0f);
        std::fill_n(v4Value, 4, 0.0f);
    }

    // 2. Override with JSON "default" value if it exists
    if (metadata.contains("default") && metadata["default"].is_array()) {
        const auto& def_arr = metadata["default"];
        if (glslType == "float") {
            fValue = metadata.value("default", fValue);
        } else if (glslType == "int") {
            iValue = metadata.value("default", iValue);
        } else if (glslType == "bool") {
            bValue = metadata.value("default", bValue);
        } else if (glslType == "vec2" && def_arr.size() >= 2) {
            v2Value[0] = def_arr[0].get<float>();
            v2Value[1] = def_arr[1].get<float>();
        } else if (glslType == "vec3" && def_arr.size() >= 3) {
            v3Value[0] = def_arr[0].get<float>();
            v3Value[1] = def_arr[1].get<float>();
            v3Value[2] = def_arr[2].get<float>();
        } else if (glslType == "vec4" && def_arr.size() >= 4) {
            v4Value[0] = def_arr[0].get<float>();
            v4Value[1] = def_arr[1].get<float>();
            v4Value[2] = def_arr[2].get<float>();
            v4Value[3] = def_arr[3].get<float>();
        }
    }
}

ConstVariableControl::ConstVariableControl(const std::string& n, const std::string& type, int lIndex, const std::string& valStr)
    : name(n), glslType(type), originalValueString(valStr), lineIndex(lIndex), charPosition(0),
      fValue(0.0f), iValue(0), multiplier(1.0f), isColor(false) {
    std::fill_n(v2Value, 2, 0.0f);
    std::fill_n(v3Value, 3, 0.0f);
    std::fill_n(v4Value, 4, 0.0f);
}

ShaderParser::ShaderParser() {}
ShaderParser::~ShaderParser() {}

void ShaderParser::ScanAndPrepareDefineControls(const std::string& shaderCode) { 
    m_defineControls.clear();
    std::istringstream iss(shaderCode);
    std::string line;
    int currentLineNumber = 0;
    std::regex defineRegex(R"(^\s*(//)?\s*#define\s+([a-zA-Z_][a-zA-Z0-9_]*)(?:\s+([^\n\r]*))?.*)");
    std::smatch match;

    while (std::getline(iss, line)) {
        currentLineNumber++;
        std::string trimmedLine = Utils::Trim(line);

        if (std::regex_match(trimmedLine, match, defineRegex)) {
            DefineControl dc;
            dc.name = match[2].str(); 
            dc.isEnabled = !match[1].matched; 
            dc.originalLine = currentLineNumber;

            if (match[3].matched) { 
                dc.originalValueString = Utils::Trim(match[3].str());
                dc.hasValue = !dc.originalValueString.empty();
                if (dc.hasValue) {
                    try {
                        dc.floatValue = std::stof(dc.originalValueString);
                    } catch (const std::exception&) {
                    }
                }
            } else {
                dc.hasValue = false;
                dc.originalValueString.clear();
            }
            m_defineControls.push_back(dc);
        }
    }
    std::sort(m_defineControls.begin(), m_defineControls.end(),
              [](const DefineControl& a, const DefineControl& b) { return a.name < b.name; });
}

std::string ShaderParser::ToggleDefineInString(const std::string& shaderCode, const std::string& defineName, bool enable, const std::string& originalValue) {
    std::vector<std::string> lines;
    std::istringstream iss(shaderCode);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    bool found = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmedLine = Utils::Trim(lines[i]);
        std::regex defineLineRegex(R"(^\s*(//)?\s*#define\s+)" + defineName + R"((?:\s+[^\n\r]*)?.*)");
        std::smatch match;

        if (std::regex_match(trimmedLine, match, defineLineRegex)) {
            bool currentlyEnabled = !match[1].matched;
            if (enable && !currentlyEnabled) { 
                size_t commentPos = lines[i].find("//");
                if (commentPos != std::string::npos) {
                    lines[i].erase(commentPos, 2); 
                    lines[i] = Utils::Trim(lines[i]);
                }
            } else if (!enable && currentlyEnabled) { 
                lines[i] = "//" + lines[i];
            }
            found = true;
        }
    }

    if (!found && enable) { 
        std::string newDefineLine = "#define " + defineName;
        if (!originalValue.empty()) {
            newDefineLine += " " + originalValue;
        }
        lines.insert(lines.begin(), newDefineLine); 
    }

    std::ostringstream oss;
    for (const auto& l : lines) {
        oss << l << '\n';
    }
    return oss.str();
}

void ShaderParser::ScanAndPrepareUniformControls(const std::string& shaderCode) {
    m_uniformControls.clear();
    std::istringstream iss(shaderCode);
    std::string line;

    // Stricter regex: explicitly requires a default value for uniforms.
    // It captures: 1:type, 2:name, 3:default value, 4:json blob
    std::regex uniform_control_regex(R"(uniform\s+(float|int|bool|vec2|vec3|vec4)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);\s*//\s*(\{.*\}))");

    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, uniform_control_regex)) {
            std::string glsl_type = match[1].str();
            std::string name_str = match[2].str();
            std::string default_val_str = match[3].matched ? Utils::Trim(match[3].str()) : "";
            std::string json_str = match[4].str();

            try {
                json metadata = json::parse(json_str);
                if (!metadata.contains("label")) {
                    metadata["label"] = name_str;
                }
                
                if (metadata.value("type", "") == "color") {
                    if (glsl_type == "vec3" || glsl_type == "vec4") {
                         metadata["widget"] = "color";
                    }
                }

                m_uniformControls.emplace_back(name_str, glsl_type, default_val_str, metadata);
            } catch (const json::parse_error& e) {
                std::cerr << "[ShaderParser] JSON parse error for control '" << name_str << "': " << e.what() << std::endl;
            }
        }
    }
}

const std::vector<DefineControl>& ShaderParser::GetDefineControls() const {
    return m_defineControls;
}

std::vector<DefineControl>& ShaderParser::GetDefineControls() {
    return m_defineControls;
}

const std::vector<ShaderToyUniformControl>& ShaderParser::GetUniformControls() const {
    return m_uniformControls;
}

std::vector<ShaderToyUniformControl>& ShaderParser::GetUniformControls() {
    return m_uniformControls;
}

const std::vector<ConstVariableControl>& ShaderParser::GetConstControls() const {
    return m_constControls;
}

std::vector<ConstVariableControl>& ShaderParser::GetConstControls() {
    return m_constControls;
}

void ShaderParser::ScanAndPrepareConstControls(const std::string& shaderCode) {
    m_constControls.clear();
    std::istringstream iss(shaderCode);
    std::string line;
    int currentLineNumber = -1; 
    std::regex constRegex(R"(^\s*const\s+(float|int|vec2|vec3|vec4)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);)");
    std::smatch match;

    while (std::getline(iss, line)) {
        currentLineNumber++;
        std::string trimmedLine = Utils::Trim(line);

        if (std::regex_search(trimmedLine, match, constRegex)) {
            ConstVariableControl control;
            control.glslType = match[1].str();
            control.name = match[2].str();
            control.originalValueString = Utils::Trim(match[3].str());
            control.lineIndex = currentLineNumber;
            
            size_t equalsPos = line.find('=');
            if (equalsPos != std::string::npos) {
                control.charPosition = equalsPos + 1;
                while(control.charPosition < line.length() && std::isspace(line[control.charPosition])) {
                    control.charPosition++;
                }
            }
            m_constControls.push_back(control);
        }
    }
    std::sort(m_constControls.begin(), m_constControls.end(),
              [](const ConstVariableControl& a, const ConstVariableControl& b) { return a.name < b.name; });
}

std::string ShaderParser::UpdateDefineValueInString(const std::string& shaderCode, const std::string& defineName, float newValue) {
    std::vector<std::string> lines;
    std::istringstream iss(shaderCode);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    // Regex to find an active (not commented out) #define with a value
    std::regex defineLineRegex(R"(^(\s*#define\s+)" + defineName + R"(\s+)(.*))");
    std::smatch match;

    for (size_t i = 0; i < lines.size(); ++i) {
        if (std::regex_match(lines[i], match, defineLineRegex)) {
            // Found the line. Reconstruct it with the new value.
            std::ostringstream oss;
            oss << match[1].str() << newValue;
            lines[i] = oss.str();
            break; // Assume only one definition
        }
    }

    std::ostringstream oss;
    for (const auto& l : lines) {
        oss << l << '\n';
    }
    return oss.str();
}