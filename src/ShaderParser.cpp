#include "ShaderParser.h"
#include <sstream>
#include <regex>
#include <vector>
#include <string>
#include <algorithm> // For std::sort, std::all_of
#include <cctype>    // For std::isspace, std::isalpha, std::isupper, std::isdigit
#include <iomanip>   // For std::setprecision
#include <iostream>  // For std::cerr (temporary debugging)
#include <map>       // For ScanAndPrepareDefineControls intermediate map

// Helper function from main.cpp (could be a static utility or in an anonymous namespace)
static std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (std::string::npos == start) {
        return ""; // Return empty string if only whitespace
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

// ReconstructConstValueString was a global helper in main.cpp, now a static helper here or part of ShaderParser
// It's needed by UpdateConstValueInString
static std::string ReconstructConstValueString(ShaderConstControl& control) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6); // Match original precision

    if (control.glslType == "float") {
        oss << control.fValue;
        // Consider adding 'f' suffix if original had it and it's a whole number.
        // Example: if (control.originalValueString.find('f') != std::string::npos && control.fValue == static_cast<int>(control.fValue)) oss << ".0f";
        // For simplicity, current output is often fine.
    } else if (control.glslType == "int") {
        oss << control.iValue;
    } else if (control.glslType == "vec2") {
        oss << "vec2(" << control.v2Value[0] << ", " << control.v2Value[1] << ")";
    } else if (control.glslType == "vec3") {
        oss << "vec3(" << control.v3Value[0] << ", " << control.v3Value[1] << ", " << control.v3Value[2] << ")";
    } else if (control.glslType == "vec4") {
        oss << "vec4(" << control.v4Value[0] << ", " << control.v4Value[1] << ", " << control.v4Value[2] << ", " << control.v4Value[3] << ")";
    } else {
        return control.originalValueString; // Fallback
    }
    // Note: Multiplier logic was simplified in main.cpp to store final values.
    // If the original string structure with multiplier needs to be preserved, this function would be more complex.
    return oss.str();
}


ShaderParser::ShaderParser() {}
ShaderParser::~ShaderParser() {}

void ShaderParser::ScanAndPrepareDefineControls(const std::string& shaderSource, std::vector<ShaderDefineControl>& defineControls) {
    defineControls.clear();
    std::map<std::string, ShaderDefineControl> foundDefinesMap;
    std::string sourceStr(shaderSource);
    std::stringstream ss(sourceStr);
    std::string line;
    int currentLineNumber = 0;

    while (std::getline(ss, line)) {
        currentLineNumber++;
        std::string trimmedLine = trim(line);
        std::string defineName;
        std::string originalValuePart;
        float parsedFloatValue = 0.0f;
        bool hasParsedValue = false;
        bool isActive = false;
        bool defineFoundThisLine = false;

        const std::string defineKeyword = "#define ";
        const std::string commentedDefineKeyword = "//#define ";
        size_t keywordLen = 0;

        if (trimmedLine.rfind(defineKeyword, 0) == 0) {
            isActive = true; defineFoundThisLine = true; keywordLen = defineKeyword.length();
        } else if (trimmedLine.rfind(commentedDefineKeyword, 0) == 0) {
            isActive = false; defineFoundThisLine = true; keywordLen = commentedDefineKeyword.length();
        }

        if (defineFoundThisLine) {
            std::string restOfLine = trim(trimmedLine.substr(keywordLen));
            std::stringstream line_ss(restOfLine);
            if (line_ss >> defineName) {
                if (!defineName.empty() && (std::isalpha(defineName[0]) || defineName[0] == '_')) {
                    size_t nameEndPos = restOfLine.find(defineName) + defineName.length();
                    if (nameEndPos < restOfLine.length()) {
                        originalValuePart = trim(restOfLine.substr(nameEndPos));
                    } else { originalValuePart = ""; }

                    if (!originalValuePart.empty()) {
                        std::stringstream value_ss(originalValuePart);
                        if (value_ss >> parsedFloatValue) {
                            std::string remainingAfterFloat; std::getline(value_ss, remainingAfterFloat);
                            remainingAfterFloat = trim(remainingAfterFloat);
                            if (remainingAfterFloat.empty() || remainingAfterFloat.rfind("//",0) == 0) {
                                hasParsedValue = true;
                            } else { hasParsedValue = false; }
                        } else { hasParsedValue = false; }
                    } else { hasParsedValue = false; }
                } else { defineName.clear(); defineFoundThisLine = false; }
            } else { defineFoundThisLine = false; }
        }

        if (defineFoundThisLine && !defineName.empty()) {
            ShaderDefineControl newDefine;
            newDefine.name = defineName;
            newDefine.isEnabled = isActive;
            newDefine.lineNumber = currentLineNumber;
            newDefine.hasValue = hasParsedValue;
            newDefine.floatValue = (hasParsedValue ? parsedFloatValue : 0.0f);
            newDefine.originalValueString = originalValuePart;

            // Logic from main.cpp to handle multiple definitions (prefer active, then later line)
            auto it = foundDefinesMap.find(defineName);
            if (it != foundDefinesMap.end()) {
                if (isActive && !it->second.isEnabled) { foundDefinesMap[defineName] = newDefine; }
                else if (isActive && it->second.isEnabled) { foundDefinesMap[defineName] = newDefine; }
                else if (!isActive && !it->second.isEnabled) { foundDefinesMap[defineName] = newDefine; }
            } else {
                foundDefinesMap[defineName] = newDefine;
            }
        }
    }
    for (const auto& pair : foundDefinesMap) { defineControls.push_back(pair.second); }
    std::sort(defineControls.begin(), defineControls.end(),
              [](const ShaderDefineControl& a, const ShaderDefineControl& b) { return a.name < b.name; });
}

void ShaderParser::ScanAndPrepareUniformControls(const std::string& shaderSource, std::vector<ShaderToyUniformControl>& uniformControls) {
    uniformControls.clear();
    std::string sourceStr(shaderSource);
    std::stringstream ss(sourceStr);
    std::string line;
    const std::string uniformKeyword = "uniform";

    while (std::getline(ss, line)) {
        std::string trimmedLine = trim(line);
        size_t uniformPos = trimmedLine.find(uniformKeyword);

        if (uniformPos == 0) { // Starts with "uniform"
            size_t commentPos = trimmedLine.find("//");
            // Ensure it's not a commented-out uniform itself, and there is a comment for metadata
            if (commentPos == std::string::npos || commentPos < uniformKeyword.length()) continue;

            std::string declStmt = trim(trimmedLine.substr(0, commentPos));
            std::string metadataComment = trim(trimmedLine.substr(commentPos + 2)); // +2 to skip "//"

            // Basic validation for metadata format
            if (declStmt.empty() || metadataComment.empty() || metadataComment[0] != '{' || metadataComment.back() != '}') {
                continue;
            }

            std::string tempDecl = declStmt.substr(uniformKeyword.length()); // Remove "uniform "
            size_t semiColonPos = tempDecl.find(';');
            if (semiColonPos != std::string::npos) {
                tempDecl = trim(tempDecl.substr(0, semiColonPos));
            } else {
                continue; // Malformed uniform declaration (no semicolon before comment)
            }

            std::stringstream decl_ss(tempDecl);
            std::string glslType, uniformName;
            decl_ss >> glslType >> uniformName;

            if (uniformName.empty() || glslType.empty()) continue;

            // Skip standard Shadertoy uniforms or ones not meant for general UI control
            if (uniformName.rfind("iChannel", 0) == 0 || uniformName == "iResolution" ||
                uniformName == "iTime" || uniformName == "iTimeDelta" || uniformName == "iFrame" ||
                uniformName == "iMouse" || uniformName == "iDate" || uniformName == "iSampleRate" ||
                uniformName == "iUserFloat1" || uniformName == "iUserColor1" ) { // These are handled by ShaderEffect directly
                continue;
            }
            // Filter by supported GLSL types for UI
            if (glslType != "float" && glslType != "vec2" && glslType != "vec3" && glslType != "vec4" && glslType != "int" && glslType != "bool") {
                // std::cerr << "Skipping uniform " << uniformName << " due to unsupported GLSL type for UI: " << glslType << std::endl;
                continue;
            }

            try {
                nlohmann::json metadataJson = nlohmann::json::parse(metadataComment);
                if (metadataJson.is_object()) {
                    uniformControls.emplace_back(uniformName, glslType, metadataJson);
                }
            } catch (nlohmann::json::parse_error& e) {
                // std::cerr << "JSON metadata parse error for uniform " << uniformName << ": " << e.what() << " on metadata: " << metadataComment << std::endl;
            }
        }
    }
    std::sort(uniformControls.begin(), uniformControls.end(),
              [](const ShaderToyUniformControl& a, const ShaderToyUniformControl& b) {
                  return a.metadata.value("label", a.name) < b.metadata.value("label", b.name);
              });
}

void ShaderParser::ScanAndPrepareConstControls(const std::string& shaderSource, std::vector<ShaderConstControl>& constControls) {
    constControls.clear();
    std::stringstream ss(shaderSource);
    std::string line;
    int currentLineNumber = 0;

    // Regex for 'const <type> <name> = <value>;'
    // Handles float, int, vec2, vec3, vec4
    std::regex constRegex(R"(^\s*const\s+(float|int|vec2|vec3|vec4)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(.*?)\s*;)");
    std::smatch match;

    // Regex for parsing vecN constructor and optional multiplier
    // e.g. vecN( num, num, [num, [num]] ) * num
    std::regex vecValRegex(R"(vec([234])\s*\(\s*(-?\d+\.?\d*f?)\s*,\s*(-?\d+\.?\d*f?)\s*(?:,\s*(-?\d+\.?\d*f?))?\s*(?:,\s*(-?\d+\.?\d*f?))?\s*\)\s*(?:L?\s*\*\s*(-?\d+\.?\d*f?))?)");
                            //type(2,3,4)     x-comp          y-comp          z-comp (opt)    w-comp (opt)                                multiplier (opt)
                            //  1             2               3               4               5                                           6
    while (std::getline(ss, line)) {
        currentLineNumber++;
        std::string trimmedLine = trim(line);
        // Skip commented lines
        if (trimmedLine.rfind("//", 0) == 0 || trimmedLine.rfind("/*",0) == 0) continue;

        if (std::regex_search(trimmedLine, match, constRegex)) {
            if (match.size() == 4) { // 0:full, 1:type, 2:name, 3:value_str
                std::string type = trim(match[1].str());
                std::string name = trim(match[2].str());
                std::string valueStrFull = trim(match[3].str());

                ShaderConstControl control(name, type, currentLineNumber, valueStrFull);
                bool parsedSuccessfully = false;

                try {
                    if (type == "float") {
                        std::string valToParse = valueStrFull;
                        if (!valToParse.empty() && (valToParse.back() == 'f' || valToParse.back() == 'F')) {
                            valToParse.pop_back();
                        }
                        control.fValue = std::stof(valToParse);
                        parsedSuccessfully = true;
                    } else if (type == "int") {
                        control.iValue = std::stoi(valueStrFull);
                        parsedSuccessfully = true;
                    } else if (type == "vec2" || type == "vec3" || type == "vec4") {
                        std::smatch vecMatch;
                        if (std::regex_match(valueStrFull, vecMatch, vecValRegex)) {
                            float m = 1.0f;
                            if (vecMatch[6].matched) { // Multiplier is group 6
                                std::string multStr = vecMatch[6].str();
                                if (!multStr.empty() && (multStr.back() == 'f' || multStr.back() == 'F')) multStr.pop_back();
                                m = std::stof(multStr);
                            }
                            control.multiplier = m;

                            if (type == "vec2" && vecMatch[2].matched && vecMatch[3].matched) {
                                control.v2Value[0] = std::stof(vecMatch[2].str()) * m;
                                control.v2Value[1] = std::stof(vecMatch[3].str()) * m;
                                parsedSuccessfully = true;
                            } else if (type == "vec3" && vecMatch[2].matched && vecMatch[3].matched && vecMatch[4].matched) {
                                control.v3Value[0] = std::stof(vecMatch[2].str()) * m;
                                control.v3Value[1] = std::stof(vecMatch[3].str()) * m;
                                control.v3Value[2] = std::stof(vecMatch[4].str()) * m;
                                parsedSuccessfully = true;
                            } else if (type == "vec4" && vecMatch[2].matched && vecMatch[3].matched && vecMatch[4].matched && vecMatch[5].matched) {
                                control.v4Value[0] = std::stof(vecMatch[2].str()) * m;
                                control.v4Value[1] = std::stof(vecMatch[3].str()) * m;
                                control.v4Value[2] = std::stof(vecMatch[4].str()) * m;
                                control.v4Value[3] = std::stof(vecMatch[5].str()) * m;
                                parsedSuccessfully = true;
                            }
                        } else {
                            // std::cerr << "Warning: Could not parse vecN value for const '" << name << "' with regex: '" << valueStrFull << "'" << std::endl;
                        }

                        // Color heuristic (moved from main.cpp)
                        if (parsedSuccessfully && (type == "vec3" || type == "vec4")) {
                            if (name.find("color") != std::string::npos || name.find("Color") != std::string::npos || name.find("COLOUR") != std::string::npos) {
                                control.isColor = true;
                            } else {
                                bool allUnit = true; // Are all components between 0 and 1?
                                if(type == "vec3") for(int k=0; k<3; ++k) if(control.v3Value[k]<0.f || control.v3Value[k]>1.0001f) allUnit=false;
                                if(type == "vec4") for(int k=0; k<4; ++k) if(control.v4Value[k]<0.f || control.v4Value[k]>1.0001f) allUnit=false;
                                if(allUnit) control.isColor = true; // If so, good chance it's a color
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    // std::cerr << "Warning: Exception parsing value for const '" << name << "': '" << valueStrFull << "' (" << e.what() << ")" << std::endl;
                    parsedSuccessfully = false;
                }

                if(parsedSuccessfully){
                    // Skip PI, etc. (from main.cpp heuristic, if desired)
                    // bool isAllCaps = std::all_of(name.begin(), name.end(), [](char c){ return std::isupper(c) || std::isdigit(c) || c == '_'; });
                    // if (name == "PI" || name == "PHI" || (isAllCaps && name.length() > 2)) { continue; }
                    constControls.push_back(control);
                }
            }
        }
    }
    std::sort(constControls.begin(), constControls.end(),
              [](const ShaderConstControl& a, const ShaderConstControl& b) { return a.name < b.name; });
}

std::string ShaderParser::ToggleDefineInString(const std::string& sourceCode, const std::string& defineName, bool enable, const std::string& originalValueStringIfKnown) {
    std::vector<std::string> lines;
    std::stringstream ss(sourceCode);
    std::string line;
    bool modified = false;

    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    const std::string activeDefineFull = "#define " + defineName;
    const std::string searchActiveDefine = "#define " + defineName;
    const std::string searchCommentedDefine = "//#define " + defineName;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmedLine = trim(lines[i]);
        std::string currentDefineNameInLine;
        size_t defineNameStartPos = std::string::npos;

        // Check if the line starts with the active define pattern
        if (trimmedLine.rfind(searchActiveDefine, 0) == 0) {
            defineNameStartPos = searchActiveDefine.length() - defineName.length();
        }
        // Check if the line starts with the commented define pattern
        else if (trimmedLine.rfind(searchCommentedDefine, 0) == 0) {
            defineNameStartPos = searchCommentedDefine.length() - defineName.length();
        }

        if (defineNameStartPos != std::string::npos && defineNameStartPos < trimmedLine.length()) {
            // Extract the define name from the line to ensure it's an exact match
            // (e.g., avoid matching MY_DEFINE when toggling MY_DEFINE_EXTRA)
            std::stringstream tempSS(trimmedLine.substr(defineNameStartPos));
            tempSS >> currentDefineNameInLine;

            if (currentDefineNameInLine == defineName) { // Exact match of define name
                if (trimmedLine.rfind(searchActiveDefine, 0) == 0 && !enable) { // Active, want to disable
                    size_t firstCharPos = lines[i].find_first_not_of(" \t"); // Preserve indentation
                    if (firstCharPos == std::string::npos) firstCharPos = 0;
                    lines[i].insert(firstCharPos, "//");
                    modified = true;
                    break; // Found and modified, exit loop
                } else if (trimmedLine.rfind(searchCommentedDefine, 0) == 0 && enable) { // Commented, want to enable
                    size_t commentPos = lines[i].find("//");
                    if (commentPos != std::string::npos) {
                        lines[i].erase(commentPos, 2); // Remove "//"
                        // Ensure the line is correctly formatted after uncommenting
                        std::string tempLineCheck = trim(lines[i]);
                        if (tempLineCheck.rfind(activeDefineFull, 0) != 0 ||
                           (tempLineCheck.length() > activeDefineFull.length() && !std::isspace(tempLineCheck[activeDefineFull.length()]))) {
                            // If it's not correctly formatted (e.g. just `define MY_VAR`), reconstruct it
                            size_t firstChar = lines[i].find_first_not_of(" \t");
                            std::string indentation = (firstChar == std::string::npos) ? "" : lines[i].substr(0, firstChar);
                            lines[i] = indentation + activeDefineFull;
                            if (!originalValueStringIfKnown.empty()) { // Add back original value if known
                                lines[i] += " " + originalValueStringIfKnown;
                            }
                        }
                        modified = true;
                        break; // Found and modified, exit loop
                    }
                }
            }
        }
    }

    if (modified) {
        std::string newSource;
        for (size_t i = 0; i < lines.size(); ++i) {
            newSource += lines[i];
            if (i < lines.size() - 1) { // Add newline for all but last line
                newSource += "\n";
            }
        }
        // Preserve trailing newline if original had one and new one doesn't (or vice versa)
        if (!sourceCode.empty() && sourceCode.back() == '\n' && (newSource.empty() || newSource.back() != '\n')) {
            newSource += "\n";
        }
        return newSource;
    }
    return ""; // Return empty if no modification was made (or error)
}

std::string ShaderParser::UpdateDefineValueInString(const std::string& sourceCode, const std::string& defineName, float newValue) {
    std::vector<std::string> lines;
    std::stringstream ss(sourceCode);
    std::string line;
    bool modified = false;

    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    const std::string activeDefinePrefix = "#define " + defineName;

    for (size_t i = 0; i < lines.size(); ++i) {
        std::string trimmedLine = trim(lines[i]);
        if (trimmedLine.rfind(activeDefinePrefix, 0) == 0) { // Line starts with "#define MY_DEFINE"
            // Ensure it's the exact define name (e.g. not MY_DEFINE_EXTRA)
            std::string namePartInLine;
            size_t nameStartPos = std::string("#define ").length();
            size_t nameEndPosActual = trimmedLine.find_first_of(" \t\n\r//", nameStartPos);
            if (nameEndPosActual == std::string::npos) { // If no space/comment after name, it's the end of line
                nameEndPosActual = trimmedLine.length();
            }
            namePartInLine = trimmedLine.substr(nameStartPos, nameEndPosActual - nameStartPos);

            if (namePartInLine == defineName) { // Exact match
                std::string commentPart;
                // Find where the value part ends and comment might begin
                size_t valueAndCommentStartPos = nameStartPos + namePartInLine.length();
                // Skip spaces between name and value/comment
                while(valueAndCommentStartPos < trimmedLine.length() && std::isspace(trimmedLine[valueAndCommentStartPos])) {
                    valueAndCommentStartPos++;
                }

                if (valueAndCommentStartPos < trimmedLine.length()){ // If there's something after the name (value or comment)
                    size_t commentMarkerPos = trimmedLine.find("//", valueAndCommentStartPos);
                     if (commentMarkerPos != std::string::npos) {
                        commentPart = trimmedLine.substr(commentMarkerPos); // Capture comment part
                     }
                }

                std::ostringstream newValueStream;
                newValueStream << std::fixed << std::setprecision(6) << newValue; // Control float formatting
                std::string newLineContent = activeDefinePrefix + " " + newValueStream.str();
                if (!commentPart.empty()) {
                    newLineContent += " " + commentPart; // Append comment if it existed
                }

                // Preserve original indentation
                size_t firstCharPos = lines[i].find_first_not_of(" \t");
                if (firstCharPos == std::string::npos) firstCharPos = 0; // Line was all whitespace (shouldn't happen for a define)
                lines[i] = lines[i].substr(0, firstCharPos) + newLineContent;
                modified = true;
                break; // Found and modified
            }
        }
    }

    if (modified) {
        std::string newSource;
        for (size_t i = 0; i < lines.size(); ++i) {
            newSource += lines[i];
             if (i < lines.size() - 1 ) { // Add newline for all but last line
                newSource += "\n";
            }
        }
         // Preserve trailing newline if original had one and new one doesn't
        if (!sourceCode.empty() && sourceCode.back() == '\n' && (newSource.empty() || newSource.back() != '\n')) {
            newSource += "\n";
        }
        return newSource;
    }
    return ""; // Return empty if no modification
}

std::string ShaderParser::UpdateConstValueInString(const std::string& sourceCode, ShaderConstControl& control) {
    std::vector<std::string> lines;
    std::stringstream ss(sourceCode);
    std::string line;
    bool foundAndModified = false;
    std::string newSourceCode;

    // Reconstruct the value string from the control's current state (which was updated by UI)
    std::string newValueString = ReconstructConstValueString(control);

    for (int currentLineIndex = 0; std::getline(ss, line); ++currentLineIndex) {
        if (currentLineIndex + 1 == control.lineNumber) { // Line numbers are 1-based
            // Regex to find 'const <type> <name> = <value_to_replace>;'
            // This regex needs to be robust enough for various spacings.
            // (.*?); captures everything up to the first semicolon greedily.
            // More precise: const\s+float\s+MY_CONST\s*=\s*(.+?);
            std::string pattern_str = R"(^\s*const\s+)" + control.glslType + R"(\s+)" + control.name + R"(\s*=\s*(.*?)\s*;)";
            std::regex lineRegex(pattern_str);
            std::smatch match;

            if (std::regex_search(line, match, lineRegex) && match.size() > 1) {
                // match[0] is the whole matched part (e.g. "const float FOO = 0.5;")
                // match[1] is the captured value part (e.g. "0.5")

                // Find the start position of the value within the original line string
                size_t valueStartPosInOriginalLine = match.position(1); // Start of group 1 (the value)
                std::string linePrefix = line.substr(0, valueStartPosInOriginalLine);
                std::string lineSuffix = line.substr(valueStartPosInOriginalLine + match.length(1)); // Suffix starts after the captured value

                line = linePrefix + newValueString + lineSuffix;
                foundAndModified = true;
                control.originalValueString = newValueString; // Update the control's stored string for consistency
            } else {
                // std::cerr << "Warning: Regex failed to match const declaration for update on line: " << control.lineNumber << std::endl;
                // std::cerr << "  Line content: " << line << std::endl;
                // std::cerr << "  Expected pattern for: " << control.name << " of type " << control.glslType << std::endl;
                // std::cerr << "  Attempted Regex: " << pattern_str << std::endl;
            }
        }
        newSourceCode += line + "\n"; // Append current line (modified or not)
    }

    // Remove last newline if source didn't have one
    if (!newSourceCode.empty() && newSourceCode.back() == '\n') {
        if (sourceCode.empty() || sourceCode.back() != '\n') {
            newSourceCode.pop_back();
        }
    }

    return foundAndModified ? newSourceCode : ""; // Return modified code or empty if no change
}


std::string ShaderParser::ExtractShaderId(const std::string& idOrUrl) {
    std::string id = idOrUrl;
    // Try to extract from URL: https://www.shadertoy.com/view/Ms2SD1
    size_t lastSlash = id.find_last_of('/');
    if (lastSlash != std::string::npos) {
        id = id.substr(lastSlash + 1);
    }
    // Remove query parameters like ?key=...
    size_t questionMark = id.find('?');
    if (questionMark != std::string::npos) {
        id = id.substr(0, questionMark);
    }
    // A typical Shadertoy ID is 6 alphanumeric characters
    if (id.length() == 6 && std::all_of(id.begin(), id.end(), ::isalnum)) {
        return id;
    }
    return ""; // Return empty if not a valid-looking ID
}
