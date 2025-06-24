#include "ShaderParser.h"
#include "Utils.h" // <<< This include is necessary

#include <sstream>
#include <regex>
#include <algorithm> 
#include <iomanip>   
#include <iostream>

// --- ShaderToyUniformControl Constructor Definition ---
ShaderToyUniformControl::ShaderToyUniformControl(const std::string& n, const std::string& type_str, const json& meta)
    : name(n), glslType(type_str), metadata(meta) {
    location = -1; 
    isColor = metadata.value("widget", "") == "color" || metadata.value("type", "") == "color";

    if (glslType == "float") {
        fValue = metadata.value("default", 0.0f);
    } else if (glslType == "int") {
        iValue = metadata.value("default", 0);
    } else if (glslType == "bool") {
        bValue = metadata.value("default", false);
    } else if (glslType == "vec2") {
        auto def_arr = metadata.value("default", json::array({0.0, 0.0}));
        v2Value[0] = metadata.value("default_x", def_arr.size() > 0 ? def_arr[0].get<float>() : 0.0f);
        v2Value[1] = metadata.value("default_y", def_arr.size() > 1 ? def_arr[1].get<float>() : 0.0f);
    } else if (glslType == "vec3") {
        auto def_arr = metadata.value("default", json::array({0.0, 0.0, 0.0}));
        v3Value[0] = metadata.value("default_x", def_arr.size() > 0 ? def_arr[0].get<float>() : 0.0f);
        v3Value[1] = metadata.value("default_y", def_arr.size() > 1 ? def_arr[1].get<float>() : 0.0f);
        v3Value[2] = metadata.value("default_z", def_arr.size() > 2 ? def_arr[2].get<float>() : 0.0f);
    } else if (glslType == "vec4") {
        auto def_arr = metadata.value("default", json::array({0.0, 0.0, 0.0, 0.0}));
        v4Value[0] = metadata.value("default_x", def_arr.size() > 0 ? def_arr[0].get<float>() : 0.0f);
        v4Value[1] = metadata.value("default_y", def_arr.size() > 1 ? def_arr[1].get<float>() : 0.0f);
        v4Value[2] = metadata.value("default_z", def_arr.size() > 2 ? def_arr[2].get<float>() : 0.0f);
        v4Value[3] = metadata.value("default_w", def_arr.size() > 3 ? def_arr[3].get<float>() : 0.0f);
    }
}

// --- ShaderParser Implementation ---
ShaderParser::ShaderParser() {}
ShaderParser::~ShaderParser() {}

// <<< The old definition of ShaderParser::trim is REMOVED from here. >>>

TextEditor::ErrorMarkers ShaderParser::ParseGlslErrorLog(const std::string& log) {
    TextEditor::ErrorMarkers markers;
    std::regex errorRegex1(R"(ERROR:\s*(\d+):(\d+):\s*(.*))"); 
    std::regex errorRegex2(R"((\d+):(\d+):\s*(?:error|warning):\s*(.*))"); 
    std::regex errorRegex3(R"(WARNING:\s*(\d+):(\d+):\s*(.*))"); 
    std::regex errorRegex4(R"((\d+)\((\d+)\)\s*:\s*(?:error|warning)\s*C\d+:\s*(.*))"); 

    std::istringstream iss(log);
    std::string line;
    std::smatch match;

    while (std::getline(iss, line)) {
        int errorLine = -1;
        std::string errorMessage;

        if (std::regex_search(line, match, errorRegex1)) { 
            try { errorLine = std::stoi(match[2].str()); } catch (const std::exception&) {} 
            errorMessage = Utils::Trim(match[3].str()); // <<< USING Utils::Trim
        } else if (std::regex_search(line, match, errorRegex2)) { 
            try { errorLine = std::stoi(match[1].str()); } catch (const std::exception&) {}
            errorMessage = Utils::Trim(match[3].str()); // <<< USING Utils::Trim
        } else if (std::regex_search(line, match, errorRegex3)) { 
             try { errorLine = std::stoi(match[2].str()); } catch (const std::exception&) {}
            errorMessage = Utils::Trim(match[3].str()); // <<< USING Utils::Trim
        } else if (std::regex_search(line, match, errorRegex4)) { 
            try { errorLine = std::stoi(match[2].str()); } catch (const std::exception&) {}
            errorMessage = Utils::Trim(match[3].str()); // <<< USING Utils::Trim
        }

        if (errorLine > 0 && !errorMessage.empty()) {
            markers[errorLine] = errorMessage;
        }
    }
    return markers;
}

void ShaderParser::ScanAndPrepareDefineControls(const char* shaderCode) {
    m_defineControls.clear();
    std::string code(shaderCode);
    std::istringstream iss(code);
    std::string line;
    int currentLineNumber = 0;
    std::regex defineRegex(R"(^\s*(//)?\s*#define\s+([a-zA-Z_][a-zA-Z0-9_]*)(?:\s+([^\n\r]*))?.*)");
    std::smatch match;

    while (std::getline(iss, line)) {
        currentLineNumber++;
        std::string trimmedLine = Utils::Trim(line); // <<< USING Utils::Trim

        if (std::regex_match(trimmedLine, match, defineRegex)) {
            DefineControl dc;
            dc.name = match[2].str(); 
            dc.isEnabled = !match[1].matched; 
            dc.originalLine = currentLineNumber;

            if (match[3].matched) { 
                dc.originalValueString = Utils::Trim(match[3].str()); // <<< USING Utils::Trim
                dc.hasValue = !dc.originalValueString.empty();
                if (dc.hasValue) {
                    try {
                        dc.floatValue = std::stof(dc.originalValueString);
                    } catch (const std::exception&) {
                        // Not a simple float
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
        std::string trimmedLine = Utils::Trim(lines[i]); // <<< USING Utils::Trim
        std::regex defineLineRegex(R"(^\s*(//)?\s*#define\s+)" + defineName + R"((?:\s+[^\n\r]*)?.*)");
        std::smatch match;

        if (std::regex_match(trimmedLine, match, defineLineRegex)) {
            bool currentlyEnabled = !match[1].matched;
            if (enable && !currentlyEnabled) { 
                size_t commentPos = lines[i].find("//");
                if (commentPos != std::string::npos) {
                    lines[i].erase(commentPos, 2); 
                    lines[i] = Utils::Trim(lines[i]); // <<< USING Utils::Trim
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

std::string ShaderParser::UpdateDefineValueInString(const std::string& shaderCode, const std::string& defineName, float newValue) {
    std::vector<std::string> lines;
    std::istringstream iss(shaderCode);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    std::ostringstream ossValue;
    ossValue << std::fixed << std::setprecision(3) << newValue; 
    std::string newValueStr = ossValue.str();

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmedLine = Utils::Trim(lines[i]); // <<< USING Utils::Trim
        std::regex activeDefineRegex(R"(^\s*#define\s+)" + defineName + R"((?:\s+[^\n\r]*)?.*)");
        std::smatch match;

        if (std::regex_match(trimmedLine, match, activeDefineRegex)) {
            size_t defineKeywordPos = lines[i].find("#define");
            std::string leadingWhitespace = lines[i].substr(0, defineKeywordPos);
            lines[i] = leadingWhitespace + "#define " + defineName + " " + newValueStr;
        }
    }

    std::ostringstream oss;
    for (const auto& l : lines) {
        oss << l << '\n';
    }
    return oss.str();
}

void ShaderParser::ScanAndPrepareUniformControls(const char* shaderCode) {
    m_uniformControls.clear();
    std::string code(shaderCode);
    std::istringstream iss(code);
    std::string line;
    std::regex uniformRegex(R"(^\s*uniform\s+(float|vec2|vec3|vec4|int|bool)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;.*\/\/\s*(\{.*\})\s*$)");
    std::smatch match;

    while (std::getline(iss, line)) {
        std::string trimmedLine = Utils::Trim(line); // <<< USING Utils::Trim
        if (std::regex_match(trimmedLine, match, uniformRegex)) {
            std::string typeStr = match[1].str();
            std::string nameStr = match[2].str();
            std::string jsonStr = match[3].str();
            try {
                json metadataJson = json::parse(jsonStr);
                m_uniformControls.emplace_back(nameStr, typeStr, metadataJson);
            } catch (const json::parse_error& e) {
                // Handle or log JSON parsing error
            }
        }
    }
     std::sort(m_uniformControls.begin(), m_uniformControls.end(),
              [](const ShaderToyUniformControl& a, const ShaderToyUniformControl& b) { return a.name < b.name; });
}

void ShaderParser::ClearAllControls() {
    m_defineControls.clear();
    m_uniformControls.clear();
    m_constControls.clear();
}

void ShaderParser::ParseConstValueString(const std::string& valueStr, ConstVariableControl& control) {
    std::string valToParse = Utils::Trim(valueStr); // <<< USING Utils::Trim
    control.isColor = false; 
    control.multiplier = 1.0f;

    try {
        if (control.glslType == "float") {
            control.fValue = std::stof(valToParse);
        } else if (control.glslType == "int") {
            control.iValue = std::stoi(valToParse);
        } else if (control.glslType == "vec2" || control.glslType == "vec3" || control.glslType == "vec4") {
            std::regex vecRegex(R"(vec([234])\s*\((.*)\))");
            std::smatch vecMatch;
            if (std::regex_match(valToParse, vecMatch, vecRegex)) {
                std::string argsStr = Utils::Trim(vecMatch[2].str()); // <<< USING Utils::Trim
                std::stringstream ss(argsStr);
                std::string segment;
                std::vector<float> components;
                while(std::getline(ss, segment, ',')) {
                    components.push_back(std::stof(Utils::Trim(segment))); // <<< USING Utils::Trim
                }
                if (control.glslType == "vec2") {
                    if (components.size() == 1) { control.v2Value[0] = control.v2Value[1] = components[0]; }
                    else if (components.size() >= 2) { control.v2Value[0] = components[0]; control.v2Value[1] = components[1]; }
                } else if (control.glslType == "vec3") {
                    if (components.size() == 1) { control.v3Value[0] = control.v3Value[1] = control.v3Value[2] = components[0]; }
                    else if (components.size() >= 3) { for(int k=0; k<3; ++k) control.v3Value[k] = components[k]; }
                } else if (control.glslType == "vec4") {
                     if (components.size() == 1) { for(int k=0; k<4; ++k) control.v4Value[k] = components[0]; }
                    else if (components.size() >= 4) { for(int k=0; k<4; ++k) control.v4Value[k] = components[k]; }
                }
            } else {
                std::regex vecMulRegex(R"((vec[234]\s*\([^)]*\))\s*\*\s*([+-]?\d*\.?\d+f?))");
                std::smatch mulMatch;
                if (std::regex_match(valToParse, mulMatch, vecMulRegex)) {
                    std::string vecPart = mulMatch[1].str();
                    float multiplier = std::stof(mulMatch[2].str());
                    control.multiplier = multiplier; 
                    ParseConstValueString(vecPart, control); 
                } else {
                    float single_val = std::stof(valToParse);
                     if (control.glslType == "vec2") { control.v2Value[0] = control.v2Value[1] = single_val; }
                     else if (control.glslType == "vec3") { control.v3Value[0] = control.v3Value[1] = control.v3Value[2] = single_val; }
                     else if (control.glslType == "vec4") { for(int k=0; k<4; ++k) control.v4Value[k] = single_val; }
                }
            }
            bool allUnit = true;
            if(control.glslType == "vec3") for(int k=0; k<3; ++k) if(control.v3Value[k]<0.f || control.v3Value[k]>1.0001f) allUnit=false;
            if(control.glslType == "vec4") for(int k=0; k<4; ++k) if(control.v4Value[k]<0.f || control.v4Value[k]>1.0001f) allUnit=false;
            if(allUnit && (control.glslType == "vec3" || control.glslType == "vec4")) control.isColor = true;
        }
    } catch (const std::exception& e) {
        // Handle or log parsing failure
    }
}

std::string ShaderParser::ReconstructConstValueString(const ConstVariableControl& control) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4); 

    if (control.glslType == "float") {
        oss << control.fValue << "f";
    } else if (control.glslType == "int") {
        oss << control.iValue;
    } else if (control.glslType == "vec2") {
        oss << "vec2(" << control.v2Value[0] << "f, " << control.v2Value[1] << "f)";
    } else if (control.glslType == "vec3") {
        oss << "vec3(" << control.v3Value[0] << "f, " << control.v3Value[1] << "f, " << control.v3Value[2] << "f)";
    } else if (control.glslType == "vec4") {
        oss << "vec4(" << control.v4Value[0] << "f, " << control.v4Value[1] << "f, " << control.v3Value[2] << "f, " << control.v4Value[3] << "f)";
    } else {
        return control.originalValueString; 
    }
    return oss.str();
}

std::string ShaderParser::UpdateConstValueInString(const std::string& shaderCode, const ConstVariableControl& control) {
    if (control.lineIndex < 0) return ""; 

    std::vector<std::string> lines;
    std::istringstream iss(shaderCode);
    std::string lineContent;
    while (std::getline(iss, lineContent)) {
        lines.push_back(lineContent);
    }

    if (static_cast<size_t>(control.lineIndex) >= lines.size()) return ""; 

    std::string& targetLine = lines[control.lineIndex];
    std::string newValueStr = ReconstructConstValueString(control);

    size_t equalsPos = targetLine.find('=', control.charPosition > 20 ? control.charPosition - 20 : 0); 
    if (equalsPos == std::string::npos) { 
         equalsPos = targetLine.find('=');
    }

    if (equalsPos != std::string::npos) {
        size_t valueStartPos = equalsPos + 1;
        while (valueStartPos < targetLine.length() && std::isspace(targetLine[valueStartPos])) {
            valueStartPos++;
        }
        size_t valueEndPos = targetLine.find(';', valueStartPos);
        if (valueEndPos == std::string::npos) {
            valueEndPos = targetLine.length();
        }
        std::string postSemicolon = "";
        size_t semicolonPos = targetLine.find(';', valueStartPos);
        if (semicolonPos != std::string::npos) {
            postSemicolon = targetLine.substr(semicolonPos);
        } else {
            postSemicolon = ";"; 
        }

        targetLine.replace(valueStartPos, (semicolonPos != std::string::npos ? semicolonPos : targetLine.length()) - valueStartPos, " " + newValueStr);
        size_t checkSemi = targetLine.rfind(';');
        if (checkSemi == std::string::npos || checkSemi < targetLine.find(newValueStr)) {
             targetLine += ";";
        }
    } else {
        return ""; 
    }

    std::ostringstream oss;
    for (size_t i = 0; i < lines.size(); ++i) {
        oss << lines[i] << (i == lines.size() - 1 ? "" : "\n");
    }
    return oss.str();
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
        std::string trimmedLine = Utils::Trim(line); // <<< USING Utils::Trim

        if (std::regex_search(trimmedLine, match, constRegex)) {
            ConstVariableControl control;
            control.glslType = match[1].str();
            control.name = match[2].str();
            control.originalValueString = Utils::Trim(match[3].str()); // <<< USING Utils::Trim
            control.lineIndex = currentLineNumber;
            
            size_t equalsPos = line.find('=');
            if (equalsPos != std::string::npos) {
                control.charPosition = equalsPos + 1;
                while(control.charPosition < line.length() && std::isspace(line[control.charPosition])) {
                    control.charPosition++;
                }
            }
            ParseConstValueString(control.originalValueString, control);
            m_constControls.push_back(control);
        }
    }
    std::sort(m_constControls.begin(), m_constControls.end(),
              [](const ConstVariableControl& a, const ConstVariableControl& b) { return a.name < b.name; });
}

// --- Getter Definitions ---
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
