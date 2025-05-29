// RaymarchVibe - Real-time Shader Exploration
// main.cpp

#include <glad/glad.h> // Must be included before GLFW
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath> // For abs, M_PI if needed elsewhere
#include <map>      
#include <algorithm> 
#include <cctype>   
#include <iomanip> // For std::setprecision in string manipulation
#include <regex>   // For ParseGlslErrorLog and const parsing

// --- ImGui Includes ---
#include "imgui.h"              // Main ImGui header
#include "imgui_impl_glfw.h"    // ImGui Platform Backend for GLFW
#include "imgui_impl_opengl3.h" // ImGui Renderer Backend for OpenGL3

// --- HTTP & JSON Library Integration ---
#include "httplib.h" 
#include <nlohmann/json.hpp> // Use angle brackets for libraries found via FetchContent/system
using json = nlohmann::json;

// --- Use ImGuiColorTextEdit for the Shader Code Editor ---
#include "TextEditor.h"

// Window dimensions
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;

// Forward declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window); 
void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
std::string loadShaderSource(const char* filePath);
GLuint compileShader(const char* source, GLenum type, std::string& errorLogString);
GLuint createShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString);

void ApplyShaderFromEditorLogic_FetchUniforms(
    GLuint program, bool isShadertoyMode, std::string& warnings,
    GLint& iRes, GLint& iTime, GLint& objColor, GLint& scaleU, GLint& timeSpeedU,
    GLint& colorModU, GLint& patternScaleU, GLint& camPosU, GLint& camTargetU, GLint& camFOVU,
    GLint& lightPosU, GLint& lightColorU, GLint& iTimeDelta, GLint& iFrame, GLint& iMouse,
    GLint& iUserFloat1Location_param, GLint& iUserColor1Location_param);
    
// Forward declaration for error parsing
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log);
void ClearErrorMarkers(); // Helper to clear markers 

std::string FetchShadertoyCodeOnline(const std::string& shaderId, const std::string& apiKey, std::string& errorMsg);
std::string ExtractShaderId(const std::string& idOrUrl); 
void ScanAndPrepareDefineControls(const char* shaderSource);
// Refactored define modification functions
std::string ToggleDefineInString(const std::string& sourceCode, const std::string& defineName, bool enable, const std::string& originalValueStringIfKnown);
std::string UpdateDefineValueInString(const std::string& sourceCode, const std::string& defineName, float newValue);
void ScanAndPrepareUniformControls(const char* shaderSource); // Forward declaration

// --- NEW FOR CONST EDITING ---
struct ShaderConstControl; // Forward declare for use in UpdateConstValueInString
std::string UpdateConstValueInString(const std::string& sourceCode, ShaderConstControl& control);
void ScanAndPrepareConstControls(const std::string& shaderSource);
// --- END NEW FOR CONST EDITING ---


void ApplyShaderFromEditor( 
    const std::string& shaderCode_param,
    bool currentShadertoyMode,
    std::string& shaderCompileErrorLog_ref,
    GLuint& shaderProgram_ref, 
    GLint& iResolutionLocation_ref, GLint& iTimeLocation_ref, 
    GLint& u_objectColorLocation_ref, GLint& u_scaleLocation_ref, GLint& u_timeSpeedLocation_ref,
    GLint& u_colorModLocation_ref, GLint& u_patternScaleLocation_ref,
    GLint& u_camPosLocation_ref, GLint& u_camTargetLocation_ref, GLint& u_camFOVLocation_ref,
    GLint& u_lightPositionLocation_ref, GLint& u_lightColorLocation_ref,
    GLint& iTimeDeltaLocation_ref, GLint& iFrameLocation_ref, GLint& iMouseLocation_ref,
    GLint& iUserFloat1Location_ref, GLint& iUserColor1Location_ref 
);


// --- Global/Static Variables for Shader Logic and UI State ---
static float mouseState_iMouse[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static bool shadertoyMode = false;
static bool showGui = true; 
static bool snapWindowsNextFrame = false; 
static bool showHelpWindow = false; 
static TextEditor editor;             // Instance of the text editor
static std::string currentShaderCode; // To hold text from editor, especially for functions
static std::string shaderLoadError = "";
static std::string currentShaderPath = "shaders/raymarch_v1.frag"; // Default shader - CRITICAL: Must be global static
static std::string shaderCompileErrorLog = "";                     // CRITICAL: Must be global static
static bool isFullscreen = false;
static int storedWindowX = 100, storedWindowY = 100; 
static int storedWindowWidth = SCR_WIDTH, storedWindowHeight = SCR_HEIGHT; 


struct ShaderSample {
    const char* name;
    const char* filePath;
};

static const std::vector<ShaderSample> shaderSamples = {
    {"--- Select a Sample ---", ""},
    {"Native Shader Template 1", "shaders/raymarch_v1.frag"},
    {"Native Shader Template 2", "shaders/raymarch_v2.frag"},
    {"Simple Red", "shaders/samples/simple_red.frag"},
    {"UV Pattern", "shaders/samples/uv_pattern.frag"},
    {"Shadertoy Sampler1", "shaders/samples/tester_cube.frag"},
    {"Fractal Sampler1", "shaders/samples/fractal1.frag"},
    {"Fractal Sampler2", "shaders/samples/fractal2.frag"},
    {"Fractal Sampler3", "shaders/samples/fractal3.frag"},
};
static size_t currentSampleIndex = 0; 

struct ShaderDefineControl {
    std::string name;       
    bool isEnabled;         
    int lineNumber = -1; 
    bool hasValue = false;    
    float floatValue = 0.0f;  
    std::string originalValueString; 
};
static std::vector<ShaderDefineControl> shaderDefineControls;

struct ShaderToyUniformControl {
    std::string name;         
    std::string glslType;     
    GLint location = -1;      
    json metadata;            

    float fValue = 0.0f;
    float v2Value[2] = {0.0f, 0.0f};
    float v3Value[3] = {0.0f, 0.0f, 0.0f};
    float v4Value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int iValue = 0;     
    bool bValue = false;  

    bool isColor = false; 

    ShaderToyUniformControl(const std::string& n, const std::string& type_str, const json& meta)
        : name(n), glslType(type_str), metadata(meta) {
        
        if (metadata.contains("default")) {
            try { 
                if (glslType == "float" && metadata["default"].is_number()) {
                    fValue = metadata["default"].get<float>();
                } else if (glslType == "vec2" && metadata["default"].is_array() && metadata["default"].size() == 2) {
                    for(size_t i=0; i<2; ++i) if(metadata["default"][i].is_number()) v2Value[i] = metadata["default"][i].get<float>();
                } else if (glslType == "vec3" && metadata["default"].is_array() && metadata["default"].size() == 3) {
                    for(size_t i=0; i<3; ++i) if(metadata["default"][i].is_number()) v3Value[i] = metadata["default"][i].get<float>();
                } else if (glslType == "vec4" && metadata["default"].is_array() && metadata["default"].size() == 4) {
                    for(size_t i=0; i<4; ++i) if(metadata["default"][i].is_number()) v4Value[i] = metadata["default"][i].get<float>();
                } else if (glslType == "int" && metadata["default"].is_number_integer()) { 
                    iValue = metadata["default"].get<int>();
                } else if (glslType == "bool" && metadata["default"].is_boolean()) { 
                    bValue = metadata["default"].get<bool>();
                }
            } catch (const json::exception& e) {
                std::cerr << "Error accessing 'default' for uniform " << name << " of type " << glslType << ": " << e.what() << std::endl;
            }
        }
        
        std::string widgetType = metadata.value("widget", "");
        if ((glslType == "vec3" || glslType == "vec4") && widgetType == "color") {
            isColor = true;
        }
    }
};
static std::vector<ShaderToyUniformControl> shaderToyUniformControls;

// --- NEW FOR CONST EDITING (Step 1) ---
struct ShaderConstControl {
    std::string name;       // e.g., "SEA_HEIGHT"
    std::string glslType;   // e.g., "float", "int", "vec2", "vec3", "vec4"
    int lineNumber;         // Line number where the declaration was found
    std::string originalValueString; // The string representation of the value, e.g., "0.6" or "vec3(0.1, 0.2, 0.3)" or "vec3(0.1,0.2,0.3) * 0.5"

    // Parsed values for UI manipulation
    float fValue = 0.0f;
    int iValue = 0;
    float v2Value[2] = {0.0f, 0.0f};
    float v3Value[3] = {0.0f, 0.0f, 0.0f};
    float v4Value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float multiplier = 1.0f; // For cases like vecN(...) * scalar

    bool isColor = false; // Heuristic for vec3/vec4 color pickers

    ShaderConstControl(const std::string& n, const std::string& type, int line, const std::string& valStr)
        : name(n), glslType(type), lineNumber(line), originalValueString(valStr), multiplier(1.0f) {
        // Value parsing logic is now in ScanAndPrepareConstControls
    }
};
static std::vector<ShaderConstControl> shaderConstControls; // Global vector to hold these controls
// --- END NEW FOR CONST EDITING ---


const char* nativeShaderTemplate = R"GLSL(
// RaymarchVibe - Native Dodecahedron Template
#version 330 core
out vec4 FragColor;

uniform vec2 iResolution;
uniform float iTime;

// Native Uniforms
uniform vec3 u_objectColor = vec3(0.7, 0.5, 0.9); 
uniform float u_scale = 0.8;        
uniform float u_timeSpeed = 0.3;    
uniform vec3 u_colorMod = vec3(0.2, 0.3, 0.4); 
uniform float u_patternScale = 5.0; 

// Camera Uniforms
uniform vec3 u_camPos = vec3(0.0, 0.8, -2.5);
uniform vec3 u_camTarget = vec3(0.0, 0.0, 0.0);
uniform float u_camFOV = 60.0;

// Light Uniforms
uniform vec3 u_lightPosition = vec3(3.0, 2.0, -4.0);
uniform vec3 u_lightColor = vec3(1.0, 0.95, 0.9);

// Constants
const float PI = 3.14159265359;
const float PHI = 1.618033988749895; // Golden ratio

// Helper Functions
float radians(float degrees) {
    return degrees * PI / 180.0;
}

mat3 rotateX(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        1.0, 0.0, 0.0,
        0.0, c,  -s,
        0.0, s,   c
    );
}

mat3 rotateY(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c,  0.0, s,
        0.0,1.0, 0.0,
        -s, 0.0, c
    );
}

mat3 rotateZ(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c,  -s, 0.0,
        s,   c, 0.0,
        0.0,0.0,1.0
    );
}

// SDF for a Regular Dodecahedron
float sdDodecahedron(vec3 p, float r_inradius) {
    p = abs(p); 
    vec3 n1 = vec3(1.0, PHI, 0.0);
    vec3 n2 = vec3(0.0, 1.0, PHI);
    vec3 n3 = vec3(PHI, 0.0, 1.0);
    float d = dot(p, normalize(n1));
    d = max(d, dot(p, normalize(n2)));
    d = max(d, dot(p, normalize(n3)));
    return d - r_inradius;
}

// Scene Definition
float mapScene(vec3 p, float time) {
    float effectiveTime = time * u_timeSpeed;
    float dodecaInradius = 0.5 * u_scale; 
    vec3 objectOffset = vec3(0.0, dodecaInradius * 0.8, 0.0); 
    vec3 p_transformed = p - objectOffset;
    mat3 rotMat = rotateY(effectiveTime) * rotateX(effectiveTime * 0.7) * rotateZ(effectiveTime * 0.4);
    p_transformed = rotMat * p_transformed;
    float dodecaDist = sdDodecahedron(p_transformed, dodecaInradius);
    return dodecaDist;
}

// Normal Calculation
vec3 getNormal(vec3 p, float time) {
    float eps = 0.0005 * u_scale; 
    vec2 e = vec2(eps, 0.0);
    return normalize(vec3(
        mapScene(p + e.xyy, time) - mapScene(p - e.xyy, time),
        mapScene(p + e.yxy, time) - mapScene(p - e.yxy, time),
        mapScene(p + e.yyx, time) - mapScene(p - e.yyx, time)
    ));
}

// Raymarching
float rayMarch(vec3 ro, vec3 rd, float time) {
    float t = 0.0;
    for (int i = 0; i < 96; i++) { 
        vec3 p = ro + rd * t;
        float dist = mapScene(p, time);
        if (dist < (0.001 * t) || dist < 0.0001) { 
             return t;
        }
        t += dist * 0.75; 
        if (t > 30.0) { 
            break;
        }
    }
    return -1.0; 
}

// Camera Setup
mat3 setCamera(vec3 ro, vec3 ta, vec3 worldUp) {
    vec3 f = normalize(ta - ro);
    vec3 r = normalize(cross(f, worldUp));
    if (length(r) < 0.0001) { r = normalize(cross(f, vec3(0.0,0.0,-1.0))); } 
    if (length(r) < 0.0001) { r = normalize(cross(f, vec3(1.0,0.0,0.0))); } 
    vec3 u = normalize(cross(r, f));
    return mat3(r, u, f);
}

// Main Shader Logic
void main() {
    vec2 p_ndc = (2.0 * gl_FragCoord.xy - iResolution.xy) / iResolution.y;
    vec3 ro = u_camPos;
    vec3 ta = u_camTarget;
    mat3 camToWorld = setCamera(ro, ta, vec3(0.0, 1.0, 0.0));
    float fovFactor = tan(radians(u_camFOV) * 0.5);
    vec3 rd_local = normalize(vec3(p_ndc.x * fovFactor, p_ndc.y * fovFactor, 1.0));
    vec3 rd = camToWorld * rd_local;
    float t = rayMarch(ro, rd, iTime);
    vec3 col = vec3(0.05, 0.02, 0.08); 

    if (t > -0.5) { 
        vec3 hitPos = ro + rd * t;
        vec3 normal = getNormal(hitPos, iTime);
        vec3 lightDir = normalize(u_lightPosition - hitPos);
        float diffuse = max(0.0, dot(normal, lightDir));
        float ao = 0.0;
        float aoStep = 0.01 * u_scale; 
        float aoTotalDist = 0.0;
        for(int j=0; j<4; j++){
            aoTotalDist += aoStep;
            float d_ao = mapScene(hitPos + normal * aoTotalDist * 0.5, iTime);
            ao += max(0.0, (aoTotalDist*0.5 - d_ao));
            aoStep *= 1.7;
        }
        ao = 1.0 - clamp(ao * (0.1 / (u_scale*u_scale + 0.01) ), 0.0, 1.0); 
        vec3 viewDir = normalize(ro - hitPos);
        vec3 reflectDir = reflect(-lightDir, normal);
        float specAngle = max(dot(viewDir, reflectDir), 0.0);
        float specular = pow(specAngle, 32.0 + u_patternScale * 10.0); 
        vec3 baseColour = u_objectColor;
        float patternVal = sin(hitPos.x * u_patternScale + iTime * u_timeSpeed * 0.5) *
                           cos(hitPos.y * u_patternScale - iTime * u_timeSpeed * 0.3) *
                           sin(hitPos.z * u_patternScale + iTime * u_timeSpeed * 0.7);
        baseColour += u_colorMod * (0.5 + 0.5 * patternVal);
        baseColour = clamp(baseColour, 0.0, 1.0);
        vec3 litColour = baseColour * (diffuse * 0.7 + 0.2 * ao) * u_lightColor; 
        litColour += u_lightColor * specular * 0.4 * (0.5 + 0.5 * baseColour.r); 
        col = litColour;
        float fogFactor = smoothstep(u_scale * 2.0, u_scale * 15.0, t); 
        col = mix(col, vec3(0.05, 0.02, 0.08), fogFactor);
    } else {
        col = vec3(0.1, 0.05, 0.15) + vec3(0.2, 0.1, 0.05) * pow(1.0 - abs(p_ndc.y), 3.0);
    }
    col = pow(col, vec3(1.0/2.2));
    FragColor = vec4(col, 1.0);
})GLSL";

const char* shadertoyShaderTemplate = R"GLSL(
// RaymarchVibe - Shadertoy Template
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy;
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));
    fragColor = vec4(col,1.0);
})GLSL";

// Helper function to trim whitespace from both ends of a string
std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (std::string::npos == start) {
        return ""; // Return empty string if only whitespace
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

// Helper to clear error markers from the TextEditor
void ClearErrorMarkers() {
    TextEditor::ErrorMarkers emptyMarkers; // Create an empty map of markers
    editor.SetErrorMarkers(emptyMarkers);  // Tell the editor to use this empty map
}

// Basic GLSL Error Log Parser for TextEditor
TextEditor::ErrorMarkers ParseGlslErrorLog(const std::string& log) {
    TextEditor::ErrorMarkers markers; 
    std::stringstream ss(log);        
    std::string line;
    std::regex errorRegex1(R"(^(?:[A-Z]+:\s*)?\d+:(\d+):\s*(.*))");
    std::regex errorRegex2(R"(^\s*\d+\((\d+)\)\s*:\s*(.*))");      
    std::smatch match; 

    while (std::getline(ss, line)) { 
        bool matched = false;
        if (std::regex_search(line, match, errorRegex1)) {
            if (match.size() >= 3) { 
                try {
                    int lineNumber = std::stoi(match[1].str()); 
                    std::string errorMessage = trim(match[2].str()); 
                    if (lineNumber > 0) { 
                        markers[lineNumber] = errorMessage; 
                    }
                    matched = true;
                } catch (const std::exception& e) {
                     // std::cerr << "Error parsing GLSL log line number (regex1): " << e.what() << " on line: " << line << std::endl;
                }
            }
        }
        if (!matched && std::regex_search(line, match, errorRegex2)) {
             if (match.size() >= 3) { 
                try {
                    int lineNumber = std::stoi(match[1].str()); 
                    std::string errorMessage = trim(match[2].str()); 
                    if (lineNumber > 0) {
                        markers[lineNumber] = errorMessage; 
                    }
                } catch (const std::exception& e) {
                    // std::cerr << "Error parsing GLSL log line number (regex2): " << e.what() << " on line: " << line << std::endl;
                }
            }
        }
    }
    return markers; 
}


void ScanAndPrepareDefineControls(const char* shaderSource) {
    shaderDefineControls.clear();
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
            auto it = foundDefinesMap.find(defineName);
            ShaderDefineControl newDefine = {defineName, isActive, currentLineNumber, hasParsedValue, (hasParsedValue ? parsedFloatValue : 0.0f), originalValuePart};
            if (it != foundDefinesMap.end()) { 
                if (isActive && !it->second.isEnabled) { foundDefinesMap[defineName] = newDefine; } 
                else if (isActive && it->second.isEnabled) { foundDefinesMap[defineName] = newDefine; } 
                else if (!isActive && !it->second.isEnabled) { foundDefinesMap[defineName] = newDefine; }
            } else { foundDefinesMap[defineName] = newDefine; }
        }
    }
    for (const auto& pair : foundDefinesMap) { shaderDefineControls.push_back(pair.second); }
    std::sort(shaderDefineControls.begin(), shaderDefineControls.end(), 
              [](const ShaderDefineControl& a, const ShaderDefineControl& b) { return a.name < b.name; });
}

std::string ToggleDefineInString(const std::string& sourceCode, const std::string& defineName, bool enable, const std::string& originalValueStringIfKnown) {
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

        if (trimmedLine.rfind(searchActiveDefine, 0) == 0) { 
            defineNameStartPos = searchActiveDefine.length() - defineName.length();
        } else if (trimmedLine.rfind(searchCommentedDefine, 0) == 0) {
            defineNameStartPos = searchCommentedDefine.length() - defineName.length();
        }

        if (defineNameStartPos != std::string::npos && defineNameStartPos < trimmedLine.length()) {
            std::stringstream tempSS(trimmedLine.substr(defineNameStartPos));
            tempSS >> currentDefineNameInLine; 

            if (currentDefineNameInLine == defineName) { 
                if (trimmedLine.rfind(searchActiveDefine, 0) == 0 && !enable) { 
                    size_t firstCharPos = lines[i].find_first_not_of(" \t");
                    if (firstCharPos == std::string::npos) firstCharPos = 0; 
                    lines[i].insert(firstCharPos, "//");
                    modified = true;
                    break;
                } else if (trimmedLine.rfind(searchCommentedDefine, 0) == 0 && enable) { 
                    size_t commentPos = lines[i].find("//");
                    if (commentPos != std::string::npos) { 
                        lines[i].erase(commentPos, 2); 
                        std::string tempLineCheck = trim(lines[i]);
                        if (tempLineCheck.rfind(activeDefineFull, 0) != 0 ||
                           (tempLineCheck.length() > activeDefineFull.length() && !std::isspace(tempLineCheck[activeDefineFull.length()]))) {
                            size_t firstChar = lines[i].find_first_not_of(" \t");
                            std::string indentation = (firstChar == std::string::npos) ? "" : lines[i].substr(0, firstChar);
                            lines[i] = indentation + activeDefineFull; 
                            if (!originalValueStringIfKnown.empty()) { 
                                lines[i] += " " + originalValueStringIfKnown;
                            }
                        }
                        modified = true;
                        break;
                    }
                }
            }
        }
    }

    if (modified) {
        std::string newSource;
        for (size_t i = 0; i < lines.size(); ++i) {
            newSource += lines[i];
            if (i < lines.size() - 1) { 
                newSource += "\n";
            }
        }
        if (!sourceCode.empty() && sourceCode.back() == '\n' && (newSource.empty() || newSource.back() != '\n')) {
            newSource += "\n";
        }
        return newSource;
    }
    return ""; 
}

std::string UpdateDefineValueInString(const std::string& sourceCode, const std::string& defineName, float newValue) {
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
        if (trimmedLine.rfind(activeDefinePrefix, 0) == 0) { 
            std::string namePartInLine;
            size_t nameStartPos = std::string("#define ").length();
            size_t nameEndPosActual = trimmedLine.find_first_of(" \t\n\r//", nameStartPos);
            if (nameEndPosActual == std::string::npos) { 
                nameEndPosActual = trimmedLine.length();
            }
            namePartInLine = trimmedLine.substr(nameStartPos, nameEndPosActual - nameStartPos);

            if (namePartInLine == defineName) { 
                std::string commentPart;
                size_t valueAndCommentStartPos = nameStartPos + namePartInLine.length();
                while(valueAndCommentStartPos < trimmedLine.length() && std::isspace(trimmedLine[valueAndCommentStartPos])) {
                    valueAndCommentStartPos++;
                }

                if (valueAndCommentStartPos < trimmedLine.length()){
                    size_t commentMarkerPos = trimmedLine.find("//", valueAndCommentStartPos);
                     if (commentMarkerPos != std::string::npos) {
                        commentPart = trimmedLine.substr(commentMarkerPos); 
                     }
                }

                std::ostringstream newValueStream;
                newValueStream << std::fixed << std::setprecision(6) << newValue; 
                std::string newLineContent = activeDefinePrefix + " " + newValueStream.str();
                if (!commentPart.empty()) {
                    newLineContent += " " + commentPart; 
                }

                size_t firstCharPos = lines[i].find_first_not_of(" \t");
                if (firstCharPos == std::string::npos) firstCharPos = 0; 
                lines[i] = lines[i].substr(0, firstCharPos) + newLineContent;
                modified = true;
                break; 
            }
        }
    }

    if (modified) {
        std::string newSource;
        for (size_t i = 0; i < lines.size(); ++i) {
            newSource += lines[i];
             if (i < lines.size() - 1 ) {
                newSource += "\n";
            }
        }
         if (!sourceCode.empty() && sourceCode.back() == '\n' && (newSource.empty() || newSource.back() != '\n')) {
            newSource += "\n";
        }
        return newSource;
    }
    return ""; 
}


// --- NEW FOR CONST EDITING (Step 5) ---
// Function to reconstruct the value string for a const variable
std::string ReconstructConstValueString(ShaderConstControl& control) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6); // Set precision for floats

    if (control.glslType == "float") {
        oss << control.fValue;
        // Check if original string had an 'f' and the value is whole, to potentially add ".0f"
        // For simplicity, direct to_string or oss output is often fine.
        // If the value is an integer, like 1.0, it will be "1.000000".
        // Appending 'f' can be done if desired: e.g. if (control.originalValueString.find('f') != std::string::npos) oss << "f";
    } else if (control.glslType == "int") {
        oss << control.iValue;
    } else if (control.glslType == "vec2") {
        // If multiplier was used, the vNValue fields store the final, multiplied values.
        // So we reconstruct with these final values.
        oss << "vec2(" << control.v2Value[0] << ", " << control.v2Value[1] << ")";
    } else if (control.glslType == "vec3") {
        oss << "vec3(" << control.v3Value[0] << ", " << control.v3Value[1] << ", " << control.v3Value[2] << ")";
    } else if (control.glslType == "vec4") {
        oss << "vec4(" << control.v4Value[0] << ", " << control.v4Value[1] << ", " << control.v4Value[2] << ", " << control.v4Value[3] << ")";
    } else {
        return control.originalValueString; // Fallback for unrecognized type
    }
    
    // If the original had a multiplier and we want to preserve that structure,
    // this part would be more complex. For now, we are writing out the computed values.
    // If control.multiplier was not 1.0, the values in vNValue are already the result of that.
    // If the UI was designed to edit pre-multiplied values, then here you would do:
    // oss << " * " << control.multiplier;
    // But current parsing puts final values into vNValue.
    return oss.str();
}


std::string UpdateConstValueInString(const std::string& sourceCode, ShaderConstControl& control) {
    std::vector<std::string> lines;
    std::stringstream ss(sourceCode);
    std::string line;
    bool foundAndModified = false;
    std::string newSourceCode;

    std::string newValueString = ReconstructConstValueString(control);

    for (int currentLineIndex = 0; std::getline(ss, line); ++currentLineIndex) {
        if (currentLineIndex + 1 == control.lineNumber) { // Line numbers are 1-based
            std::string pattern_str = R"(^\s*const\s+)" + control.glslType + R"(\s+)" + control.name + R"(\s*=\s*(.*?)\s*;)";
            // std::cerr << "DEBUG: Regex pattern for UpdateConstValueInString: >>>" << pattern_str << "<<< (Type: " << control.glslType << ", Name: " << control.name << ")" << std::endl; // Keep for debugging if needed
            
            std::regex lineRegex(pattern_str); 
            std::smatch match;

            if (std::regex_search(line, match, lineRegex) && match.size() > 1) { // CORRECTED: Check for match.size() > 1 (or == 2)
                // match[0] is the whole matched line part
                // match[1] is the captured value part (content of (.*?))
                
                size_t valueStartPos = match.position(1); // Start of the value (group 1)
                std::string linePrefix = line.substr(0, valueStartPos);
                std::string lineSuffix = line.substr(valueStartPos + match.length(1)); // Suffix starts after the value

                line = linePrefix + newValueString + lineSuffix;
                foundAndModified = true;
                control.originalValueString = newValueString; // Update stored string for consistency
            } else {
                std::cerr << "Warning: Regex failed to match const declaration for update on line: " << control.lineNumber << std::endl;
                std::cerr << "  Line content: " << line << std::endl;
                std::cerr << "  Expected pattern for: " << control.name << std::endl;
                 std::cerr << "  Attempted Regex: " << pattern_str << std::endl;
            }
        }
        newSourceCode += line + "\n";
    }

    if (!sourceCode.empty() && sourceCode.back() != '\n' && !newSourceCode.empty() && newSourceCode.back() == '\n') {
        newSourceCode.pop_back();
    }

    return foundAndModified ? newSourceCode : ""; 
}
// --- END NEW FOR CONST EDITING ---


void ScanAndPrepareUniformControls(const char* shaderSource) {
    shaderToyUniformControls.clear();
    std::string sourceStr(shaderSource);
    std::stringstream ss(sourceStr);
    std::string line;

    const std::string uniformKeyword = "uniform";

    while (std::getline(ss, line)) {
        std::string trimmedLine = trim(line);
        size_t uniformPos = trimmedLine.find(uniformKeyword);
        
        if (uniformPos == 0) { 
            size_t commentPos = trimmedLine.find("//");
            if (commentPos == std::string::npos || commentPos < uniformKeyword.length()) continue; 

            std::string declStmt = trim(trimmedLine.substr(0, commentPos));
            std::string metadataComment = trim(trimmedLine.substr(commentPos + 2)); 

            if (declStmt.empty() || metadataComment.empty() || metadataComment[0] != '{' || metadataComment.back() != '}') {
                continue; 
            }

            std::string tempDecl = declStmt.substr(uniformKeyword.length()); 
            size_t semiColonPos = tempDecl.find(';');
            if (semiColonPos != std::string::npos) {
                tempDecl = trim(tempDecl.substr(0, semiColonPos));
            } else {
                continue; 
            }
            
            std::stringstream decl_ss(tempDecl);
            std::string glslType, uniformName;
            decl_ss >> glslType >> uniformName;

            if (uniformName.empty() || glslType.empty()) continue;
            if (uniformName.rfind("iChannel", 0) == 0 || uniformName == "iResolution" ||
                uniformName == "iTime" || uniformName == "iTimeDelta" || uniformName == "iFrame" ||
                uniformName == "iMouse" || uniformName == "iDate" || uniformName == "iSampleRate" ||
                uniformName == "iUserFloat1" || uniformName == "iUserColor1" ) {
                continue;
            }
            if (glslType != "float" && glslType != "vec2" && glslType != "vec3" && glslType != "vec4" && glslType != "int" && glslType != "bool") {
                std::cerr << "Skipping uniform " << uniformName << " due to unsupported GLSL type for UI: " << glslType << std::endl;
                continue;
            }

            try {
                json metadataJson = json::parse(metadataComment);
                if (metadataJson.is_object()) {
                    shaderToyUniformControls.emplace_back(uniformName, glslType, metadataJson);
                }
            } catch (json::parse_error& e) {
                std::cerr << "JSON metadata parse error for uniform " << uniformName << ": " << e.what() << " on metadata: " << metadataComment << std::endl;
            }
        }
    }
    std::sort(shaderToyUniformControls.begin(), shaderToyUniformControls.end(),
              [](const ShaderToyUniformControl& a, const ShaderToyUniformControl& b) {
                  return a.metadata.value("label", a.name) < b.metadata.value("label", b.name);
              });
}


// --- NEW FOR CONST EDITING (Step 2) ---
void ScanAndPrepareConstControls(const std::string& shaderSource) {
    shaderConstControls.clear();
    std::stringstream ss(shaderSource);
    std::string line;
    int currentLineNumber = 0;

    std::regex constRegex(R"(^\s*const\s+(float|int|vec2|vec3|vec4)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(.*?)\s*;)");
    std::smatch match;
    
    // Regex for vecN components: vecN( num, num, [num, [num]] )
    // And optional multiplier: * num
    // This is a bit simplified, assumes standard vecN constructor format.
    std::regex vecValRegex(R"(vec([234])\s*\(\s*(-?\d+\.?\d*f?)\s*,\s*(-?\d+\.?\d*f?)\s*(?:,\s*(-?\d+\.?\d*f?))?\s*(?:,\s*(-?\d+\.?\d*f?))?\s*\)\s*(?:L?\s*\*\s*(-?\d+\.?\d*f?))?)");
                               //  1: type (2,3,4)  // 2: x      // 3: y        // 4: z (opt) // 5: w (opt)                                // 6: multiplier (opt)
    while (std::getline(ss, line)) {
        currentLineNumber++;
        std::string trimmedLine = trim(line); 
        if (trimmedLine.rfind("//", 0) == 0 || trimmedLine.rfind("/*",0) == 0) continue; 

        if (std::regex_search(trimmedLine, match, constRegex)) {
            if (match.size() == 4) { 
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
                            if (vecMatch[6].matched) { // Multiplier
                                std::string multStr = vecMatch[6].str();
                                if (!multStr.empty() && (multStr.back() == 'f' || multStr.back() == 'F')) multStr.pop_back();
                                m = std::stof(multStr);
                            }
                            control.multiplier = m; // Store original multiplier for potential reconstruction nuances

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
                             std::cerr << "Warning: Could not parse vecN value for const '" << name << "' with regex: '" << valueStrFull << "'" << std::endl;
                        }

                        if (parsedSuccessfully && (type == "vec3" || type == "vec4")) {
                            if (name.find("color") != std::string::npos || name.find("Color") != std::string::npos || name.find("COLOUR") != std::string::npos) {
                                control.isColor = true;
                            } else { 
                                bool allUnit = true;
                                if(type == "vec3") for(int k=0; k<3; ++k) if(control.v3Value[k]<0.f || control.v3Value[k]>1.0001f) allUnit=false; // allow slight over 1 for precision
                                if(type == "vec4") for(int k=0; k<4; ++k) if(control.v4Value[k]<0.f || control.v4Value[k]>1.0001f) allUnit=false;
                                if(allUnit) control.isColor = true;
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Exception parsing value for const '" << name << "': '" << valueStrFull << "' (" << e.what() << ")" << std::endl;
                    parsedSuccessfully = false; 
                }

                if(parsedSuccessfully){
                    // Skip PI or other all-caps potential true constants if desired (heuristic)
                    // bool isAllCaps = std::all_of(name.begin(), name.end(), [](char c){ return std::isupper(c) || std::isdigit(c) || c == '_'; });
                    // if (name == "PI" || name == "PHI" || (isAllCaps && name.length() > 2)) { continue; }
                    shaderConstControls.push_back(control);
                }
            }
        }
    }
    std::sort(shaderConstControls.begin(), shaderConstControls.end(),
              [](const ShaderConstControl& a, const ShaderConstControl& b) { return a.name < b.name; });
}
// --- END NEW FOR CONST EDITING ---


std::string ExtractShaderId(const std::string& idOrUrl) {
    std::string id = idOrUrl;
    size_t lastSlash = id.find_last_of('/');
    if (lastSlash != std::string::npos) { id = id.substr(lastSlash + 1); }
    size_t questionMark = id.find('?');
    if (questionMark != std::string::npos) { id = id.substr(0, questionMark); }
    if (id.length() == 6 && std::all_of(id.begin(), id.end(), ::isalnum)) { return id; }
    return ""; 
}

std::string FetchShadertoyCodeOnline(const std::string& shaderId, const std::string& apiKey, std::string& errorMsg) {
    errorMsg.clear();
    std::string shaderCode;
    try {
        #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            httplib::SSLClient cli("www.shadertoy.com"); 
        #else
            httplib::Client cli("www.shadertoy.com", 80); 
            errorMsg = "Warning: Attempting HTTP. HTTPS is required for Shadertoy API. ";
        #endif
        std::string path = "/api/v1/shaders/" + shaderId + "?key=" + apiKey;
        std::cerr << "Fetching Shadertoy: https://www.shadertoy.com" << path << std::endl;
        auto res = cli.Get(path.c_str());
        if (res) {
            if (res->status == 200) {
                json j = json::parse(res->body);
                if (j.contains("Shader") && j["Shader"].is_object() &&
                    j["Shader"].contains("renderpass") && j["Shader"]["renderpass"].is_array() &&
                    !j["Shader"]["renderpass"].empty() &&
                    j["Shader"]["renderpass"][0].is_object() &&
                    j["Shader"]["renderpass"][0].contains("code") && 
                    j["Shader"]["renderpass"][0]["code"].is_string()) {
                    shaderCode = j["Shader"]["renderpass"][0]["code"].get<std::string>();
                } else {
                    errorMsg += "Error: JSON structure from Shadertoy unexpected for ID: " + shaderId;
                    if (j.contains("Error") && j["Error"].is_string()) { 
                        errorMsg += ". API Error: " + j["Error"].get<std::string>();
                    } else { errorMsg += ". Response body (first 300 chars): " + res->body.substr(0, 300); }
                }
            } else {
                errorMsg += "Error fetching Shadertoy " + shaderId + ": HTTP Status " + std::to_string(res->status);
                if (!res->body.empty()) { errorMsg += "\nResponse: " + res->body.substr(0, 300); }
            }
        } else {
            errorMsg += "HTTP request failed for Shadertoy " + shaderId + ". Error: " + httplib::to_string(res.error());
        }
    } catch (const json::parse_error& e) {
        errorMsg += "JSON parsing error: " + std::string(e.what());
    } catch (const std::exception& e) {
        errorMsg += "Exception during Shadertoy fetch: " + std::string(e.what());
    } catch (...) { errorMsg += "Unknown exception during Shadertoy fetch."; }
    if (shaderCode.empty() && errorMsg.empty()){ 
        errorMsg = "Failed to fetch or parse shader for " + shaderId + ". Check library setup and API key.";
    }
    return shaderCode;
}

void ApplyShaderFromEditor(
    const std::string& shaderCode_param,
    bool currentShadertoyMode,
    std::string& shaderCompileErrorLog_ref, 
    GLuint& shaderProgram_ref, 
    GLint& iResolutionLocation_ref, GLint& iTimeLocation_ref, 
    GLint& u_objectColorLocation_ref, GLint& u_scaleLocation_ref, GLint& u_timeSpeedLocation_ref,
    GLint& u_colorModLocation_ref, GLint& u_patternScaleLocation_ref,
    GLint& u_camPosLocation_ref, GLint& u_camTargetLocation_ref, GLint& u_camFOVLocation_ref,
    GLint& u_lightPositionLocation_ref, GLint& u_lightColorLocation_ref,
    GLint& iTimeDeltaLocation_ref, GLint& iFrameLocation_ref, GLint& iMouseLocation_ref,
    GLint& iUserFloat1Location_ref, GLint& iUserColor1Location_ref 
) 
{
    shaderCompileErrorLog_ref.clear();
    std::string vsSource_apply = loadShaderSource("shaders/passthrough.vert");
    if (vsSource_apply.empty()) {
        shaderCompileErrorLog_ref = "CRITICAL: Vertex shader (shaders/passthrough.vert) load failed for apply.";
        return;
    }

    std::string userShaderContent = shaderCode_param; 
    std::string finalFragmentCode_apply;

    if (currentShadertoyMode) {
        std::string processedUserCode;
        std::stringstream ss(userShaderContent);
        std::string line;
        bool firstActualCodeLine = true;

        while (std::getline(ss, line)) {
            std::string trimmedLine = trim(line); 

            if (trimmedLine.rfind("#version", 0) == 0 || trimmedLine.rfind("precision", 0) == 0) {
                continue;
            }
            
            if (!trimmedLine.empty()) {
                firstActualCodeLine = false;
            }
            if (!firstActualCodeLine || !trimmedLine.empty()) {
                 processedUserCode += line + "\n";
            }
        }
        if (!processedUserCode.empty() && processedUserCode.back() != '\n') {
            processedUserCode += "\n";
        }

        finalFragmentCode_apply = 
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec3 iResolution;\n"
            "uniform float iTime;\n"
            "uniform float iTimeDelta;\n"
            "uniform int iFrame;\n"
            "uniform vec4 iMouse;\n"
            "\n" + 
            processedUserCode +  
            (processedUserCode.empty() || processedUserCode.back() == '\n' ? "" : "\n") + 
            "\nvoid main() {\n"
            "    mainImage(FragColor, gl_FragCoord.xy);\n"
            "}\n";
    } else {
        finalFragmentCode_apply = userShaderContent;
    }

    std::string fsError_apply, vsError_apply, linkError_apply;
    GLuint newFragmentShader_apply = compileShader(finalFragmentCode_apply.c_str(), GL_FRAGMENT_SHADER, fsError_apply);
    if (newFragmentShader_apply == 0) {
        shaderCompileErrorLog_ref = "FS compile failed for apply:\n" + fsError_apply;
        return;
    }

    GLuint tempVertexShader_apply = compileShader(vsSource_apply.c_str(), GL_VERTEX_SHADER, vsError_apply);
    if (tempVertexShader_apply == 0) {
        shaderCompileErrorLog_ref = "VS compile failed for apply:\n" + vsError_apply;
        glDeleteShader(newFragmentShader_apply); 
        return;
    }
    
    GLuint newShaderProgram_apply = createShaderProgram(tempVertexShader_apply, newFragmentShader_apply, linkError_apply);
    if (newShaderProgram_apply != 0) {
        if (shaderProgram_ref != 0) glDeleteProgram(shaderProgram_ref);
        shaderProgram_ref = newShaderProgram_apply;
        
        std::string uniformWarnings_apply;
        ScanAndPrepareDefineControls(shaderCode_param.c_str()); 
        if (currentShadertoyMode) {
            ScanAndPrepareUniformControls(shaderCode_param.c_str()); 
        } else {
            shaderToyUniformControls.clear(); 
        }
        // --- NEW FOR CONST EDITING (Step 4) ---
        ScanAndPrepareConstControls(shaderCode_param); 
        // --- END NEW FOR CONST EDITING ---
        
        ApplyShaderFromEditorLogic_FetchUniforms(
            shaderProgram_ref, currentShadertoyMode, uniformWarnings_apply,
            iResolutionLocation_ref, iTimeLocation_ref, u_objectColorLocation_ref, u_scaleLocation_ref, u_timeSpeedLocation_ref,
            u_colorModLocation_ref, u_patternScaleLocation_ref, u_camPosLocation_ref, u_camTargetLocation_ref, u_camFOVLocation_ref,
            u_lightPositionLocation_ref, u_lightColorLocation_ref, iTimeDeltaLocation_ref, iFrameLocation_ref, iMouseLocation_ref,
            iUserFloat1Location_ref, iUserColor1Location_ref 
        );
        shaderCompileErrorLog_ref = uniformWarnings_apply.empty() ? "Applied from editor!" : "Applied with warnings:\n" + uniformWarnings_apply;
    } else {
        shaderCompileErrorLog_ref = "Link failed for apply:\n" + linkError_apply;
    }
}


// --- Main Application ---
int main() {
    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "RaymarchVibe", NULL, NULL);
    if (window == NULL) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD" << std::endl; glfwTerminate(); return -1; }

    static char filePathBuffer_Load[256] = "shaders/raymarch_v1.frag";
    static char filePathBuffer_SaveAs[256] = "shaders/my_new_shader.frag";
    static char shadertoyInputBuffer[256] = "Ms2SD1"; 
    static std::string shadertoyApiKey = "fdHjR1"; 

    static float objectColor[3] = {0.8f, 0.9f, 1.0f};
    static float scale = 1.0f;
    static float timeSpeed = 1.0f;
    static float colorMod[3] = {0.1f, 0.1f, 0.2f};
    static float patternScale = 1.0f;
    static float cameraPosition[3] = {0.0f, 1.0f, -3.0f};
    static float cameraTarget[3] = {0.0f, 0.0f, 0.0f};
    static float cameraFOV = 60.0f;
    static float lightPosition[3] = {2.0f, 3.0f, -2.0f};
    static float lightColor[3] = {1.0f, 1.0f, 0.9f};
    static float deltaTime = 0.0f;
    static float lastFrameTime_main = 0.0f;
    static int frameCount = 0;
    static float iUserFloat1 = 0.5f;
    static float iUserColor1[3] = {0.2f, 0.5f, 0.8f}; 

    static GLuint shaderProgram = 0; 
    static GLint iResolutionLocation = -1, iTimeLocation = -1;
    static GLint u_objectColorLocation = -1, u_scaleLocation = -1, u_timeSpeedLocation = -1;
    static GLint u_colorModLocation = -1, u_patternScaleLocation = -1;
    static GLint u_camPosLocation = -1, u_camTargetLocation = -1, u_camFOVLocation = -1;
    static GLint u_lightPositionLocation = -1, u_lightColorLocation = -1;
    static GLint iTimeDeltaLocation = -1, iFrameLocation = -1, iMouseLocation = -1;
    static GLint iUserFloat1Location = -1; 
    static GLint iUserColor1Location = -1; 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io; 
    ImGui::StyleColorsDark();
    const char* glsl_version = "#version 330";
    ImGui_ImplGlfw_InitForOpenGL(window, true); 
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    auto lang = TextEditor::LanguageDefinition::GLSL();
    editor.SetLanguageDefinition(lang);
    editor.SetShowWhitespaces(false); 
    editor.SetTabSize(4); 

    float quadVertices[] = { -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f };
    GLuint quadVAO, quadVBO; 
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO); glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
    
    std::string initialShaderContent = loadShaderSource(currentShaderPath.c_str()); 
    if (!initialShaderContent.empty()) {
        editor.SetText(initialShaderContent); 
        shadertoyMode = (initialShaderContent.find("mainImage") != std::string::npos);
    } else {
        shaderLoadError = "Failed to load initial shader '" + currentShaderPath + "' into editor.";
        const char* errorFallback = shadertoyMode ? "// Initial shader load failed.\nvoid mainImage(out vec4 C,vec2 U){C=vec4(0.5,0,0,1);}" : "// Initial shader load failed.\nvoid main(){gl_FragColor=vec4(0.5,0,0,1);}";
        editor.SetText(errorFallback); 
        if (shaderCompileErrorLog.empty() && !shaderLoadError.empty()) shaderCompileErrorLog += shaderLoadError + "\n"; 
    }
    currentShaderCode = editor.GetText(); 

    ApplyShaderFromEditor(currentShaderCode, shadertoyMode, shaderCompileErrorLog, 
                      shaderProgram, iResolutionLocation, iTimeLocation,
                      u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                      u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                      u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                      u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                      iUserFloat1Location, iUserColor1Location);

    if (!shaderCompileErrorLog.empty() &&
        (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
         shaderCompileErrorLog.find("Applied with warnings") == std::string::npos) ) {
        editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
    } else {
        ClearErrorMarkers(); 
    }

    lastFrameTime_main = (float)glfwGetTime();
    int spacebarState = GLFW_RELEASE; 
    int f12State = GLFW_RELEASE; 
    int f5State = GLFW_RELEASE; // For F5 keybind

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        int currentSpacebarState = glfwGetKey(window, GLFW_KEY_SPACE);
        if (currentSpacebarState == GLFW_PRESS && spacebarState == GLFW_RELEASE && !io.WantTextInput) {
            showGui = !showGui;
        }
        spacebarState = currentSpacebarState;

        int currentF12State = glfwGetKey(window, GLFW_KEY_F12);
        if (currentF12State == GLFW_PRESS && f12State == GLFW_RELEASE && !io.WantTextInput) {
            isFullscreen = !isFullscreen;
            if (isFullscreen) {
                glfwGetWindowPos(window, &storedWindowX, &storedWindowY);
                glfwGetWindowSize(window, &storedWindowWidth, &storedWindowHeight);
                GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
                glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            } else {
                glfwSetWindowMonitor(window, NULL, storedWindowX, storedWindowY, storedWindowWidth, storedWindowHeight, 0);
            }
        }
        f12State = currentF12State;

        // F5 Keybind to Apply from Editor
        int currentF5State = glfwGetKey(window, GLFW_KEY_F5);
        if (currentF5State == GLFW_PRESS && f5State == GLFW_RELEASE && !io.WantTextInput) {
            std::string codeToApply = editor.GetText();
            // currentShaderCode = codeToApply; // Not strictly needed as ApplyShaderFromEditor uses editor.GetText()
            ApplyShaderFromEditor(codeToApply, shadertoyMode, shaderCompileErrorLog,
                                  shaderProgram, iResolutionLocation, iTimeLocation,
                                  u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                  u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                  u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                  u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                  iUserFloat1Location, iUserColor1Location);
            if (!shaderCompileErrorLog.empty() &&
                (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                 shaderCompileErrorLog.find("Applied with warnings") == std::string::npos)) {
                editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
            } else {
                ClearErrorMarkers();
            }
             shaderLoadError = "Applied from Editor (F5)"; // Give feedback
        }
        f5State = currentF5State;
        
        processInput(window); 

        deltaTime = (float)glfwGetTime() - lastFrameTime_main;
        lastFrameTime_main = (float)glfwGetTime();
        frameCount++;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (showGui) 
        {
            if (snapWindowsNextFrame) { 
                const ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImVec2 work_pos = viewport->WorkPos;
                ImVec2 work_size = viewport->WorkSize;
                float statusWidthRatio = 0.25f; 
                float consoleHeightRatio = 0.25f; 
                float statusWidth = work_size.x * statusWidthRatio;
                float consoleHeight = work_size.y * consoleHeightRatio;
                float editorHeight = work_size.y - consoleHeight;
                float editorWidth = work_size.x - statusWidth;

                ImGui::SetNextWindowPos(ImVec2(work_pos.x, work_pos.y), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(statusWidth, editorHeight), ImGuiCond_Always);
                                
                ImGui::SetNextWindowPos(ImVec2(work_pos.x + statusWidth, work_pos.y), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(editorWidth, editorHeight), ImGuiCond_Always);
                
                ImGui::SetNextWindowPos(ImVec2(work_pos.x, work_pos.y + editorHeight), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(work_size.x, consoleHeight), ImGuiCond_Always);
            }

            // --- Status Window ---
            {
                ImGui::Begin("Status");
                ImGui::Text("Hello from drewp/darkrange!"); 
                ImGui::TextWrapped("F12: Fullscreen | Spacebar: Toggle GUI | F5: Apply Editor Code | ESC: Close"); // Updated and wrapped
                if(ImGui::Button("Show Help")) { showHelpWindow = true; } 
                ImGui::Separator(); 
                ImGui::Text("Current: %s", currentShaderPath.c_str()); 
                ImGui::Checkbox("Shadertoy Mode", &shadertoyMode);
                ImGui::Separator();

                if (!shadertoyMode) { // Native Mode Specific Uniforms
                    ImGui::Text("Shader Parameters:");
                    ImGui::Spacing(); 

                    if (ImGui::CollapsingHeader("Colour Parameters##StatusColours" )) {
                        ImGui::ColorEdit3("Object Colour##Params", objectColor); 
                        ImGui::ColorEdit3("Colour Mod##Params", colorMod); 
                    }
                    ImGui::Spacing(); 

                    if (ImGui::CollapsingHeader("Patterns of Time and Space##StatusPatterns" )) {
                        ImGui::SliderFloat("Scale##Params", &scale, 0.1f, 3.0f); 
                        ImGui::SliderFloat("Pattern Scale##Params", &patternScale, 0.1f, 10.0f);
                        ImGui::SliderFloat("Time Speed##Params", &timeSpeed, 0.0f, 5.0f);
                    }
                    ImGui::Spacing();
                    
                    if (ImGui::CollapsingHeader("Camera Controls##StatusCamera" )) {
                        ImGui::DragFloat3("Position##Cam", cameraPosition, 0.1f);
                        ImGui::DragFloat3("Target##Cam", cameraTarget, 0.1f); 
                        ImGui::SliderFloat("FOV##Cam", &cameraFOV, 15.0f, 120.0f);
                    }
                    ImGui::Spacing();
                    
                    if (ImGui::CollapsingHeader("Lighting Controls##StatusLighting" )) {
                        ImGui::DragFloat3("Light Pos##Light", lightPosition, 0.1f);
                        ImGui::ColorEdit3("Light Colour##Light", lightColor); 
                    }
                     ImGui::Separator();
                }
                
                // Shadertoy Specific Predefined Uniforms (iUserFloat1, iUserColor1)
                if (shadertoyMode) {
                    ImGui::Text("Shadertoy User Parameters:");
                    ImGui::SliderFloat("iUserFloat1", &iUserFloat1, 0.0f, 1.0f);
                    ImGui::ColorEdit3("iUserColour1", iUserColor1);
                    ImGui::Separator();
                }

                // Shadertoy Metadata Uniforms
                if (shadertoyMode && !shaderToyUniformControls.empty()) {
                    if (ImGui::CollapsingHeader("Shader Uniforms (from metadata)##STUniformsGUI")) {
                        for (size_t i = 0; i < shaderToyUniformControls.size(); ++i) {
                            auto& control = shaderToyUniformControls[i]; 
                            if (control.location == -1 && shadertoyMode) continue; 

                            std::string label = control.metadata.value("label", control.name);
                            ImGui::PushID(static_cast<int>(i) + 1000); 

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
                    ImGui::Separator();
                }
                
                // Global Constants (Editable for both modes)
                if (ImGui::CollapsingHeader("Global Constants##ConstControlsGUI")) {
                    if (shaderConstControls.empty()) {
                        ImGui::TextDisabled(" (No global constants detected or editable)");
                    } else {
                        for (size_t i = 0; i < shaderConstControls.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i) + 2000); 
                            auto& control = shaderConstControls[i]; 
                            bool valueChanged = false;
                            float dragSpeed = 0.01f; 
                            if (control.glslType == "float" && std::abs(control.fValue) > 50.0f) dragSpeed = 0.1f;
                            else if (control.glslType == "float" && std::abs(control.fValue) > 500.0f) dragSpeed = 1.0f;


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
                                std::string currentEditorCode = editor.GetText();
                                std::string modifiedCode = UpdateConstValueInString(currentEditorCode, control);

                                if (!modifiedCode.empty()) {
                                    editor.SetText(modifiedCode);
                                    
                                    ApplyShaderFromEditor(editor.GetText(), 
                                                          shadertoyMode, shaderCompileErrorLog,
                                                          shaderProgram, iResolutionLocation, iTimeLocation,
                                                          u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                                          u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                                          u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                                          u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                                          iUserFloat1Location, iUserColor1Location);
                                    
                                    if (!shaderCompileErrorLog.empty() &&
                                        (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                                         shaderCompileErrorLog.find("Applied with warnings") == std::string::npos) ) {
                                        editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
                                    } else {
                                        ClearErrorMarkers();
                                    }
                                } else {
                                    shaderLoadError = "Failed to update const value for: " + control.name;
                                }
                            }
                            ImGui::PopID();
                        }
                    }
                    ImGui::Separator();
                }

                if (ImGui::CollapsingHeader("Shader Defines##DefineControlsGUI")) { 
                    if (shaderDefineControls.empty()) {
                        ImGui::TextDisabled(" (No defines detected in current shader)");
                    } else {
                        for (size_t i = 0; i < shaderDefineControls.size(); ++i) {
                            ImGui::PushID(static_cast<int>(i) + 3000); 
                            
                            bool defineEnabledState = shaderDefineControls[i].isEnabled; 
                            if (ImGui::Checkbox("", &defineEnabledState)) { 
                                std::string currentEditorCode = editor.GetText();
                                std::string modifiedCode = ToggleDefineInString(currentEditorCode, shaderDefineControls[i].name, defineEnabledState, shaderDefineControls[i].originalValueString);
                                if (!modifiedCode.empty()) {
                                    editor.SetText(modifiedCode);
                                    ApplyShaderFromEditor(editor.GetText(), shadertoyMode, shaderCompileErrorLog, 
                                                          shaderProgram, iResolutionLocation, iTimeLocation,
                                                          u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                                          u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                                          u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                                          u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                                          iUserFloat1Location, iUserColor1Location);
                                    if (!shaderCompileErrorLog.empty() &&
                                        (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                                         shaderCompileErrorLog.find("Applied with warnings") == std::string::npos) ) {
                                        editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
                                    } else {
                                        ClearErrorMarkers();
                                    }
                                } else {
                                     shaderLoadError = "Failed to toggle define: " + shaderDefineControls[i].name;
                                }
                            }
                            ImGui::SameLine();
                            if (shaderDefineControls[i].hasValue && shaderDefineControls[i].isEnabled) { 
                                std::string dragFloatLabel = "##value_define_"; 
                                dragFloatLabel += shaderDefineControls[i].name;
                                float tempFloatValue = shaderDefineControls[i].floatValue; 
                                ImGui::SetNextItemWidth(100.0f); 
                                if (ImGui::DragFloat(dragFloatLabel.c_str(), &tempFloatValue, 0.01f, 0.0f, 0.0f, "%.3f")) {
                                   shaderDefineControls[i].floatValue = tempFloatValue; 
                                   std::string currentEditorCode = editor.GetText();
                                   std::string modifiedCode = UpdateDefineValueInString(currentEditorCode, shaderDefineControls[i].name, tempFloatValue);
                                   if (!modifiedCode.empty()) {
                                        editor.SetText(modifiedCode);
                                        ApplyShaderFromEditor(editor.GetText(), shadertoyMode, shaderCompileErrorLog, 
                                                          shaderProgram, iResolutionLocation, iTimeLocation,
                                                          u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                                          u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                                          u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                                          u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                                          iUserFloat1Location, iUserColor1Location);
                                        if (!shaderCompileErrorLog.empty() &&
                                            (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                                             shaderCompileErrorLog.find("Applied with warnings") == std::string::npos) ) {
                                            editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
                                        } else {
                                            ClearErrorMarkers();
                                        }
                                    } else {
                                        shaderLoadError = "Failed to update value for define: " + shaderDefineControls[i].name;
                                    }
                                }
                                ImGui::SameLine();
                            }
                            ImGui::TextUnformatted(shaderDefineControls[i].name.c_str()); 
                            ImGui::PopID();
                        }
                    }
                    ImGui::Separator();
                } 
                ImGui::Text("FPS: %.1f", io.Framerate);
                ImGui::End(); 
            }

            // --- Shader Editor Window ---
            {
                ImGui::Begin("Shader Editor");
                
                if (ImGui::CollapsingHeader("Load from Shadertoy" )) {
                    ImGui::InputTextWithHint("##ShadertoyInput", "Shadertoy ID (e.g. Ms2SD1) or Full URL", shadertoyInputBuffer, sizeof(shadertoyInputBuffer));
                    ImGui::SameLine();
                    if (ImGui::Button("Fetch & Apply##ShadertoyApply")) { 
                        std::string idOrUrl = shadertoyInputBuffer;
                        if (!idOrUrl.empty()) {
                            std::string shaderId = ExtractShaderId(idOrUrl);
                            if (!shaderId.empty()) { 
                                shaderLoadError = "Fetching Shadertoy " + shaderId + "...";
                                shaderCompileErrorLog.clear(); 
                                std::string fetchError;
                                std::string fetchedCode = FetchShadertoyCodeOnline(shaderId, shadertoyApiKey, fetchError); 
                                if (!fetchedCode.empty()) {
                                    editor.SetText(fetchedCode);
                                    currentShaderPath = "Shadertoy_" + shaderId + ".frag"; 
                                    strncpy(filePathBuffer_Load, currentShaderPath.c_str(), sizeof(filePathBuffer_Load) - 1);
                                    filePathBuffer_Load[sizeof(filePathBuffer_Load) - 1] = 0;
                                    strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) - 1);
                                    filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;
                                    
                                    shadertoyMode = true; 
                                    ApplyShaderFromEditor(editor.GetText(), shadertoyMode, shaderCompileErrorLog, 
                                                          shaderProgram, iResolutionLocation, iTimeLocation,
                                                          u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                                          u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                                          u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                                          u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                                          iUserFloat1Location, iUserColor1Location);
                                    if (!shaderCompileErrorLog.empty() &&
                                        (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                                         shaderCompileErrorLog.find("Applied with warnings") == std::string::npos) ) {
                                        editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
                                    } else {
                                        ClearErrorMarkers();
                                    }
                                    if(shaderCompileErrorLog.find("Applied from editor!") != std::string::npos || shaderCompileErrorLog.find("Applied with warnings") != std::string::npos) { 
                                         shaderLoadError = "Shadertoy '" + shaderId + "' fetched and applied!"; 
                                    } else {
                                        shaderLoadError = "Shadertoy '" + shaderId + "' fetched, but application failed. Check log.";
                                    }
                                } else {
                                     shaderLoadError = fetchError; 
                                     if (shaderLoadError.empty()) { 
                                        shaderLoadError = "Failed to retrieve code for Shadertoy ID: " + shaderId;
                                     }
                                }
                            } else {
                                shaderLoadError = "Invalid Shadertoy ID or URL format. Please use 6-char ID or full URL.";
                            }
                        } else {
                            shaderLoadError = "Please enter a Shadertoy ID or URL.";
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Load to Editor##ShadertoyLoadToEditor")) {
                        std::string idOrUrl = shadertoyInputBuffer;
                        if (!idOrUrl.empty()) {
                            std::string shaderId = ExtractShaderId(idOrUrl);
                            if (!shaderId.empty()) { 
                                shaderLoadError = "Fetching Shadertoy " + shaderId + "...";
                                shaderCompileErrorLog.clear(); 
                                std::string fetchError;
                                std::string fetchedCode = FetchShadertoyCodeOnline(shaderId, shadertoyApiKey, fetchError); 
                                if (!fetchedCode.empty()) {
                                    editor.SetText(fetchedCode);
                                    std::string codeFromEditor = editor.GetText(); 
                                    currentShaderPath = "Shadertoy_" + shaderId + ".frag"; 
                                    strncpy(filePathBuffer_Load, currentShaderPath.c_str(), sizeof(filePathBuffer_Load) - 1);
                                    filePathBuffer_Load[sizeof(filePathBuffer_Load) - 1] = 0;
                                    strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) - 1);
                                    filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;
                                    
                                    shadertoyMode = true; 
                                    shaderLoadError = "Shadertoy '" + shaderId + "' loaded to editor. Press Apply from Editor.";
                                    ScanAndPrepareDefineControls(codeFromEditor.c_str()); 
                                    ScanAndPrepareUniformControls(codeFromEditor.c_str()); 
                                    ScanAndPrepareConstControls(codeFromEditor);
                                    ClearErrorMarkers();
                                } else {
                                     shaderLoadError = fetchError; 
                                     if (shaderLoadError.empty()) { 
                                        shaderLoadError = "Failed to retrieve code for Shadertoy ID: " + shaderId;
                                     }
                                }
                            } else {
                                shaderLoadError = "Invalid Shadertoy ID or URL format. Please use 6-char ID or full URL.";
                            }
                        } else {
                            shaderLoadError = "Please enter a Shadertoy ID or URL.";
                        }
                    }

                    ImGui::TextWrapped("Note: Requires network. Live fetches shaders by ID (or URL) from Shadertoy.com");
                    ImGui::TextWrapped("Also doesn't currently support textures.");
                    ImGui::Spacing();
                }

                if (ImGui::CollapsingHeader("Load Sample Shader" )) {
                    if (ImGui::BeginCombo("##SampleShaderCombo", shaderSamples[currentSampleIndex].name)) {
                        for (size_t n = 0; n < shaderSamples.size(); n++) { 
                            const bool is_selected = (currentSampleIndex == n);
                            if (ImGui::Selectable(shaderSamples[n].name, is_selected)) currentSampleIndex = n;
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Load & Apply Sample")) { 
                        if (currentSampleIndex > 0 && currentSampleIndex < shaderSamples.size()) { 
                            std::string selectedPath = shaderSamples[currentSampleIndex].filePath;
                            if (!selectedPath.empty()) {
                                shaderLoadError.clear();
                                std::string newShaderContent = loadShaderSource(selectedPath.c_str());
                                if (!newShaderContent.empty()) {
                                    editor.SetText(newShaderContent);
                                    currentShaderPath = selectedPath;
                                    strncpy(filePathBuffer_Load, currentShaderPath.c_str(), sizeof(filePathBuffer_Load) - 1);
                                    filePathBuffer_Load[sizeof(filePathBuffer_Load) - 1] = 0;
                                    strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) - 1);
                                    filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;
                                    
                                    shadertoyMode = (newShaderContent.find("mainImage") != std::string::npos);
                                    shaderLoadError = "Sample '" + std::string(shaderSamples[currentSampleIndex].name) + "' loaded.";

                                    ApplyShaderFromEditor(editor.GetText(), shadertoyMode, shaderCompileErrorLog, 
                                                          shaderProgram, iResolutionLocation, iTimeLocation,
                                                          u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                                          u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                                          u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                                          u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                                          iUserFloat1Location, iUserColor1Location 
                                                          );
                                    if (!shaderCompileErrorLog.empty() &&
                                        (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                                         shaderCompileErrorLog.find("Applied with warnings") == std::string::npos) ) {
                                        editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
                                    } else {
                                        ClearErrorMarkers();
                                    }
                                    if(shaderCompileErrorLog.find("Applied from editor!") != std::string::npos || shaderCompileErrorLog.find("Applied with warnings") != std::string::npos) { 
                                         shaderLoadError += " Applied successfully!"; 
                                    } else {
                                        shaderLoadError += " Application failed. Check log.";
                                    }

                                } else {
                                    shaderLoadError = "ERROR: Failed to load sample '" + std::string(shaderSamples[currentSampleIndex].name) + "'.";
                                }
                            }
                        } else {
                            shaderLoadError = "Please select a valid sample.";
                        }
                    }
                    ImGui::Spacing();
                }
                
                if (ImGui::CollapsingHeader("Load From File" )) {
                    ImGui::InputText("Path##LoadFile", filePathBuffer_Load, sizeof(filePathBuffer_Load)); 
                    
                    if (ImGui::Button("Load to editor##LoadToEditorFileBtn")) { 
                        shaderLoadError.clear();
                        shaderCompileErrorLog.clear();
                        std::string newShaderContent = loadShaderSource(filePathBuffer_Load);
                        if (!newShaderContent.empty()) {
                            editor.SetText(newShaderContent);
                            std::string codeFromEditor = editor.GetText();
                            currentShaderPath = filePathBuffer_Load; 
                            strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) -1);
                            filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;
                            shaderLoadError = "Loaded '" + currentShaderPath + "' to editor. Press Apply from Editor.";
                            shadertoyMode = (newShaderContent.find("mainImage") != std::string::npos);
                            ScanAndPrepareDefineControls(codeFromEditor.c_str()); 
                            if(shadertoyMode) ScanAndPrepareUniformControls(codeFromEditor.c_str()); else shaderToyUniformControls.clear();
                            ScanAndPrepareConstControls(codeFromEditor);
                            ClearErrorMarkers();
                        } else {
                            shaderLoadError = "Failed to load: " + std::string(filePathBuffer_Load);
                        }
                    }
                    ImGui::SameLine(); 
                    if (ImGui::Button("Load and Apply##ReloadAndApplyFileBtn")) { 
                        shaderLoadError.clear(); shaderCompileErrorLog.clear();
                        std::string reloadedContent_raw = loadShaderSource(filePathBuffer_Load); 
                        if (!reloadedContent_raw.empty()) {
                            editor.SetText(reloadedContent_raw);
                            currentShaderPath = filePathBuffer_Load; 
                            strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) -1);
                            filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;
                            shadertoyMode = (reloadedContent_raw.find("mainImage") != std::string::npos); 
                            ApplyShaderFromEditor(editor.GetText(), shadertoyMode, shaderCompileErrorLog, 
                                              shaderProgram, iResolutionLocation, iTimeLocation,
                                              u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                              u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                              u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                              u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                              iUserFloat1Location, iUserColor1Location 
                                              );
                            if (!shaderCompileErrorLog.empty() &&
                                (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                                 shaderCompileErrorLog.find("Applied with warnings") == std::string::npos) ) {
                                editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
                            } else {
                                ClearErrorMarkers();
                            }
                            if(shaderCompileErrorLog.find("Applied from editor!") != std::string::npos || shaderCompileErrorLog.find("Applied with warnings") != std::string::npos) {
                                shaderLoadError = "'" + currentShaderPath + "' loaded & applied successfully!";
                            } else {
                                shaderLoadError = "'" + currentShaderPath + "' loaded, but application failed. Check log.";
                            }
                        } else {
                            shaderLoadError = "Failed to load and apply: " + std::string(filePathBuffer_Load);
                            shaderCompileErrorLog += shaderLoadError; 
                        }
                    }
                    ImGui::Separator(); 
                    ImGui::Spacing();
                }

                if (ImGui::CollapsingHeader("New Shader" )) {
                    if (ImGui::Button("New Native Shader")) {
                        editor.SetText(nativeShaderTemplate);
                        std::string codeFromEditor = editor.GetText();
                        currentShaderPath = "Untitled_Native.frag";
                        strncpy(filePathBuffer_Load, currentShaderPath.c_str(), sizeof(filePathBuffer_Load) - 1);
                        filePathBuffer_Load[sizeof(filePathBuffer_Load) - 1] = 0;
                        strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) - 1);
                        filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;
                        shadertoyMode = false;
                        shaderCompileErrorLog.clear(); 
                        shaderLoadError = "Native template loaded. Press Apply to compile."; 
                        ScanAndPrepareDefineControls(codeFromEditor.c_str()); 
                        shaderToyUniformControls.clear(); 
                        ScanAndPrepareConstControls(codeFromEditor);
                        ClearErrorMarkers();
                    }
                    ImGui::SameLine(); 
                    if (ImGui::Button("New Shadertoy Shader")) {
                        editor.SetText(shadertoyShaderTemplate);
                        std::string codeFromEditor = editor.GetText();
                        currentShaderPath = "Untitled_Shadertoy.frag";
                        strncpy(filePathBuffer_Load, currentShaderPath.c_str(), sizeof(filePathBuffer_Load) - 1);
                        filePathBuffer_Load[sizeof(filePathBuffer_Load) - 1] = 0;
                        strncpy(filePathBuffer_SaveAs, currentShaderPath.c_str(), sizeof(filePathBuffer_SaveAs) - 1);
                        filePathBuffer_SaveAs[sizeof(filePathBuffer_SaveAs) - 1] = 0;
                        shadertoyMode = true;
                        shaderCompileErrorLog.clear(); 
                        shaderLoadError = "Shadertoy template loaded. Press Apply to compile."; 
                        ScanAndPrepareDefineControls(codeFromEditor.c_str()); 
                        ScanAndPrepareUniformControls(codeFromEditor.c_str());
                        ScanAndPrepareConstControls(codeFromEditor);
                        ClearErrorMarkers();
                    }
                    ImGui::Spacing();
                }

                if (ImGui::CollapsingHeader("Save Shader" )) {
                    ImGui::Text("Editing: %s", currentShaderPath.c_str()); 
                    if (ImGui::Button("Save Current File")) { 
                        std::ofstream outFile(currentShaderPath);
                        if (outFile.is_open()) {
                            outFile << editor.GetText(); outFile.close();
                            shaderLoadError = "Saved to: " + currentShaderPath;
                        } else { shaderLoadError = "ERROR saving to: " + currentShaderPath; }
                    }
                    ImGui::InputText("Save As Path", filePathBuffer_SaveAs, sizeof(filePathBuffer_SaveAs));
                    ImGui::SameLine();
                    if (ImGui::Button("Save As...")) {
                        std::string saveAsPathStr(filePathBuffer_SaveAs);
                        if (saveAsPathStr.empty()) {
                            shaderLoadError = "ERROR: 'Save As' path empty.";
                        } else {
                            std::ofstream outFile(saveAsPathStr);
                            if (outFile.is_open()) {
                                outFile << editor.GetText(); outFile.close();
                                currentShaderPath = saveAsPathStr; 
                                strncpy(filePathBuffer_Load, currentShaderPath.c_str(), sizeof(filePathBuffer_Load) -1); 
                                filePathBuffer_Load[sizeof(filePathBuffer_Load) -1] = 0;
                                shaderLoadError = "Saved to: " + saveAsPathStr;
                            } else { shaderLoadError = "ERROR saving to: " + saveAsPathStr; }
                        }
                    }
                    ImGui::Spacing();
                }

                if (!shaderLoadError.empty() && shaderCompileErrorLog.empty()) { 
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: %s", shaderLoadError.c_str()); 
                }
                ImGui::Separator(); 

                ImGui::Text("Shader Code Editor"); 
                ImGui::Dummy(ImVec2(0.0f, 5.0f)); 

                float bottom_button_height = ImGui::GetFrameHeightWithSpacing() * 2.2f ; // Adjusted for two buttons potentially
                ImVec2 editorSize(-1.0f, ImGui::GetContentRegionAvail().y - bottom_button_height);
                if (editorSize.y < ImGui::GetTextLineHeight() * 10) editorSize.y = ImGui::GetTextLineHeight() * 10;
                
                editor.Render("ShaderSourceEditor", editorSize, true);

                if (ImGui::Button("Apply from Editor (F5)")) { // Updated button text
                    std::string codeToApply = editor.GetText();            
                    ApplyShaderFromEditor(codeToApply, shadertoyMode, shaderCompileErrorLog, 
                                          shaderProgram, iResolutionLocation, iTimeLocation,
                                          u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                          u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                          u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                          u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                          iUserFloat1Location, iUserColor1Location 
                                          );
                    if (!shaderCompileErrorLog.empty() &&
                        (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                         shaderCompileErrorLog.find("Applied with warnings") == std::string::npos) ) {
                        editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
                    } else {
                        ClearErrorMarkers();
                    }
                     shaderLoadError = "Applied from Editor (Button)"; // Give feedback
                }
                ImGui::SameLine();
                if(ImGui::Button("Reset Parameters")) {
                    // Reset static native C++ uniform variables to their initial defaults
                    objectColor[0] = 0.8f; objectColor[1] = 0.9f; objectColor[2] = 1.0f;
                    scale = 1.0f;
                    timeSpeed = 1.0f;
                    colorMod[0] = 0.1f; colorMod[1] = 0.1f; colorMod[2] = 0.2f;
                    patternScale = 1.0f;
                    cameraPosition[0] = 0.0f; cameraPosition[1] = 1.0f; cameraPosition[2] = -3.0f;
                    cameraTarget[0] = 0.0f; cameraTarget[1] = 0.0f; cameraTarget[2] = 0.0f;
                    cameraFOV = 60.0f;
                    lightPosition[0] = 2.0f; lightPosition[1] = 3.0f; lightPosition[2] = -2.0f;
                    lightColor[0] = 1.0f; lightColor[1] = 1.0f; lightColor[2] = 0.9f;
                    
                    // Reset Shadertoy user uniforms
                    iUserFloat1 = 0.5f;
                    iUserColor1[0] = 0.2f; iUserColor1[1] = 0.5f; iUserColor1[2] = 0.8f;

                    // Re-scan the current shader code in the editor to repopulate controls
                    // This effectively resets UI-controlled defines, metadata uniforms, and global consts
                    // to their values as written in the current shader text.
                    std::string codeToRescan = editor.GetText();
                    ScanAndPrepareDefineControls(codeToRescan.c_str());
                    if (shadertoyMode) {
                        ScanAndPrepareUniformControls(codeToRescan.c_str());
                    } else {
                        shaderToyUniformControls.clear(); // Ensure ST uniforms are cleared if not in ST mode
                    }
                    ScanAndPrepareConstControls(codeToRescan);

                    // Apply the shader to reflect these reset parameters
                    ApplyShaderFromEditor(codeToRescan, shadertoyMode, shaderCompileErrorLog,
                                          shaderProgram, iResolutionLocation, iTimeLocation,
                                          u_objectColorLocation, u_scaleLocation, u_timeSpeedLocation,
                                          u_colorModLocation, u_patternScaleLocation, u_camPosLocation,
                                          u_camTargetLocation, u_camFOVLocation, u_lightPositionLocation,
                                          u_lightColorLocation, iTimeDeltaLocation, iFrameLocation, iMouseLocation,
                                          iUserFloat1Location, iUserColor1Location);
                    if (!shaderCompileErrorLog.empty() &&
                        (shaderCompileErrorLog.find("Applied from editor!") == std::string::npos &&
                         shaderCompileErrorLog.find("Applied with warnings") == std::string::npos)) {
                        editor.SetErrorMarkers(ParseGlslErrorLog(shaderCompileErrorLog));
                    } else {
                        ClearErrorMarkers();
                    }
                    shaderLoadError = "Parameters reset to defaults from current shader.";
                }

                ImGui::End(); 
            } 

            // --- Console Window ---
            {
                ImGui::Begin("Console");
                std::string fullLogContent;
                bool hasLoadError_console = !shaderLoadError.empty(); 
                bool hasCompileError_console = !shaderCompileErrorLog.empty(); 

                if (hasLoadError_console && !hasCompileError_console) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: ");
                    ImGui::SameLine(); 
                    ImGui::TextWrapped("%s", shaderLoadError.c_str());
                    fullLogContent = "Status: " + shaderLoadError;
                } else if (hasCompileError_console) {
                    if (hasLoadError_console) { 
                        fullLogContent = "Status: " + shaderLoadError + "\n";
                    }
                    const char* logPrefix = "Log: ";
                    ImVec4 logColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); 
                    if (shaderCompileErrorLog.find("ERROR") != std::string::npos || 
                        shaderCompileErrorLog.find("Failed") != std::string::npos || 
                        shaderCompileErrorLog.find("CRITICAL") != std::string::npos) {
                        logColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); 
                    } else if (shaderCompileErrorLog.find("Warning") != std::string::npos || 
                               shaderCompileErrorLog.find("Warn:") != std::string::npos) {
                        logColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); 
                    }
                    ImGui::TextColored(logColor, "%s", logPrefix);
                    ImGui::SameLine(); 
                    ImGui::TextWrapped("%s", shaderCompileErrorLog.c_str());
                    fullLogContent += std::string(logPrefix) + shaderCompileErrorLog;
                } else { 
                    ImGui::TextDisabled("[Log is empty]");
                    fullLogContent = "[Log is empty]";
                }
                ImGui::Spacing();
                if (ImGui::Button("Clear Log & Status")) { 
                    shaderCompileErrorLog.clear(); 
                    shaderLoadError.clear(); 
                    ClearErrorMarkers(); 
                }
                ImGui::SameLine(); 
                if (ImGui::Button("Copy Log")) {
                    if (!fullLogContent.empty()) {
                        ImGui::SetClipboardText(fullLogContent.c_str());
                    } else {
                        ImGui::SetClipboardText("[Log was empty]");
                    }
                }
                ImGui::End(); 
            } 
            
            if (showHelpWindow) {
                ImGui::SetNextWindowSize(ImVec2(520, 460), ImGuiCond_FirstUseEver); // Slightly wider/taller for new text
                if (ImGui::Begin("Help / Instructions##HelpWindow", &showHelpWindow)) { 
                    ImGui::TextWrapped("RaymarchVibe Help:\n\n"
                                     "Keybinds:\n"
                                     "- F12: Toggle Fullscreen\n"
                                     "- Spacebar: Toggle GUI Visibility\n"
                                     "- F5: Apply code from Shader Editor\n" // Added F5
                                     "- ESC: Close Application\n\n"
                                     "Shader Editor:\n"
                                     "- Standard text editing features (Ctrl+C, V, X, Z, Y).\n"
                                     "- Line numbers and basic GLSL syntax highlighting.\n"
                                     "- 'Apply from Editor (F5)' recompiles and runs the current code.\n" // Updated button text
                                     "- 'Reset Parameters' resets UI controls to defaults from current shader and C++ initial values, then re-applies.\n\n" // Added Reset
                                     "Loading Shaders:\n"
                                     "- Shadertoy: Enter a 6-character ID (e.g., Ms2SD1) or full URL.\n"
                                     "  - 'Fetch & Apply': Downloads and runs.\n"
                                     "  - 'Load to Editor': Downloads to editor without running.\n"
                                     "- Sample Shader: Select from dropdown and 'Load & Apply Sample'.\n"
                                     "- Load From File: Enter path and 'Load to editor' or 'Load and Apply'.\n"
                                     "- New Shader: Loads a basic native or Shadertoy template.\n\n"
                                     "Saving Shaders:\n"
                                     "- 'Save Current File': Saves to the path shown in 'Current:' field.\n"
                                     "- 'Save As...': Enter new path and save.\n\n"
                                     "Status Window:\n"
                                     "- Native Mode: Adjust u_... uniforms for color, scale, camera, light.\n"
                                     "- Shadertoy Mode / All Modes:\n"
                                     "  - iUserFloat1, iUserColor1: Predefined user uniforms (Shadertoy).\n"
                                     "  - Shader Uniforms (from metadata): If Shadertoy code has comments like '// {\"label\"...}', controls appear.\n"
                                     "  - Global Constants: If shader has 'const float NAME = value;' etc., controls appear.\n"
                                     "  - Shader Defines: Toggle or edit values for #defines found in the shader.\n\n"
                                     "Console Window:\n"
                                     "- Shows status messages and compilation errors/warnings.\n"
                                     "- Errors in the editor are also highlighted on the relevant line.\n");
                    if (ImGui::Button("Close Help")) {
                        showHelpWindow = false; 
                    }
                    ImGui::End(); 
                }
            }

            if (snapWindowsNextFrame) {
                snapWindowsNextFrame = false;
            }
        } 

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.02f, 0.02f, 0.03f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT);

        if (shaderProgram != 0) {
            glUseProgram(shaderProgram);
            if (shadertoyMode) { 
                if (iResolutionLocation != -1) glUniform3f(iResolutionLocation, (float)display_w, (float)display_h, (float)display_w / (float)display_h);
                if (iTimeLocation != -1) glUniform1f(iTimeLocation, (float)glfwGetTime());
                if (iTimeDeltaLocation != -1) glUniform1f(iTimeDeltaLocation, deltaTime);
                if (iFrameLocation != -1) glUniform1i(iFrameLocation, frameCount);
                if (iMouseLocation != -1) glUniform4fv(iMouseLocation, 1, mouseState_iMouse);
                if (iUserFloat1Location != -1) glUniform1f(iUserFloat1Location, iUserFloat1);
                if (iUserColor1Location != -1) glUniform3fv(iUserColor1Location, 1, iUserColor1);
                
                for (const auto& control : shaderToyUniformControls) {
                    if (control.location != -1) {
                        if (control.glslType == "float") {
                            glUniform1f(control.location, control.fValue);
                        } else if (control.glslType == "vec2") {
                            glUniform2fv(control.location, 1, control.v2Value);
                        } else if (control.glslType == "vec3") {
                            glUniform3fv(control.location, 1, control.v3Value);
                        } else if (control.glslType == "vec4") {
                            glUniform4fv(control.location, 1, control.v4Value);
                        } else if (control.glslType == "int") { 
                            glUniform1i(control.location, control.iValue);
                        } else if (control.glslType == "bool") { 
                            glUniform1i(control.location, control.bValue ? 1 : 0); 
                        }
                    }
                }
            } else { 
                if (iResolutionLocation != -1) glUniform2f(iResolutionLocation, (float)display_w, (float)display_h); 
                if (iTimeLocation != -1) glUniform1f(iTimeLocation, (float)glfwGetTime());
                if (u_objectColorLocation != -1) glUniform3fv(u_objectColorLocation, 1, objectColor);
                if (u_scaleLocation != -1) glUniform1f(u_scaleLocation, scale);
                if (u_timeSpeedLocation != -1) glUniform1f(u_timeSpeedLocation, timeSpeed);
                if (u_colorModLocation != -1) glUniform3fv(u_colorModLocation, 1, colorMod);
                if (u_patternScaleLocation != -1) glUniform1f(u_patternScaleLocation, patternScale);
                if (u_camPosLocation != -1) glUniform3fv(u_camPosLocation, 1, cameraPosition);
                if (u_camTargetLocation != -1) glUniform3fv(u_camTargetLocation, 1, cameraTarget);
                if (u_camFOVLocation != -1) glUniform1f(u_camFOVLocation, cameraFOV);
                if (u_lightPositionLocation != -1) glUniform3fv(u_lightPositionLocation, 1, lightPosition);
                if (u_lightColorLocation != -1) glUniform3fv(u_lightColorLocation, 1, lightColor);
            } 
            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        } 

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    } 

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (shaderProgram != 0) glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &quadVAO); 
    glDeleteBuffers(1, &quadVBO);     
    glfwTerminate();
    return 0;
} 

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window; 
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void mouse_cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse || shadertoyMode) { 
        int windowHeight;
        glfwGetWindowSize(window, NULL, &windowHeight);
        mouseState_iMouse[0] = (float)xpos;
        mouseState_iMouse[1] = (float)windowHeight - (float)ypos; 
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods; 
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse || shadertoyMode) { 
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                double xpos_press, ypos_press;
                glfwGetCursorPos(window, &xpos_press, &ypos_press);
                int windowHeight;
                glfwGetWindowSize(window, NULL, &windowHeight);
                mouseState_iMouse[0] = (float)xpos_press;
                mouseState_iMouse[1] = (float)windowHeight - (float)ypos_press; 
                mouseState_iMouse[2] = mouseState_iMouse[0]; 
                mouseState_iMouse[3] = mouseState_iMouse[1]; 
            } else if (action == GLFW_RELEASE) {
                 mouseState_iMouse[2] = -mouseState_iMouse[0];
                 mouseState_iMouse[3] = -mouseState_iMouse[1];
            }
        }
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
        if (mouseState_iMouse[2] > 0.0f || mouseState_iMouse[3] > 0.0f) { 
             mouseState_iMouse[2] = -mouseState_iMouse[0]; 
             mouseState_iMouse[3] = -mouseState_iMouse[1]; 
        }
    } 
}

std::string loadShaderSource(const char* filePath) {
    std::ifstream shaderFile;
    std::stringstream shaderStream;
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        shaderFile.open(filePath);
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
    } catch (std::ifstream::failure& e) {
        std::cerr << "ERROR::SHADER::FILE_NOT_READ: " << filePath << " - " << e.what() << std::endl;
        return "";
    }
    return shaderStream.str();
}

GLuint compileShader(const char* source, GLenum type, std::string& errorLogString) {
    errorLogString.clear();
    if (!source || std::string(source).empty()) {
        errorLogString = "ERROR::SHADER::COMPILE_EMPTY_SOURCE Type: " + std::to_string(type);
        std::cerr << errorLogString << std::endl; return 0;
    }
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success; glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 0 ? logLength + 1 : 257); 
        if (logLength == 0) infoLog[0] = '\0'; 
        glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLog.size()-1), NULL, infoLog.data());
        errorLogString = "ERROR::SHADER::COMPILE_FAIL (" + std::string(type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") + ")\n" + infoLog.data();
        std::cerr << errorLogString << std::endl;
        glDeleteShader(shader); return 0;
    }
    return shader;
}

void ApplyShaderFromEditorLogic_FetchUniforms(
    GLuint program, bool isShadertoyMode, std::string& warnings,
    GLint& iRes, GLint& iTime, GLint& objColor, GLint& scaleU, GLint& timeSpeedU,
    GLint& colorModU, GLint& patternScaleU, GLint& camPosU, GLint& camTargetU, GLint& camFOVU,
    GLint& lightPosU, GLint& lightColorU, GLint& iTimeDelta, GLint& iFrame, GLint& iMouse,
    GLint& iUserFloat1Location_param, GLint& iUserColor1Location_param 
)
{
    if (isShadertoyMode) {
        iRes = glGetUniformLocation(program, "iResolution");
        iTime = glGetUniformLocation(program, "iTime");
        iTimeDelta = glGetUniformLocation(program, "iTimeDelta");
        iFrame = glGetUniformLocation(program, "iFrame");
        iMouse = glGetUniformLocation(program, "iMouse");
        iUserFloat1Location_param = glGetUniformLocation(program, "iUserFloat1"); 
        iUserColor1Location_param = glGetUniformLocation(program, "iUserColor1"); 

        if (iRes == -1) warnings += "ST_Warn: iResolution not found.\n";
        if (iTime == -1) warnings += "ST_Warn: iTime not found.\n";
        if (iUserFloat1Location_param == -1 && warnings.find("iUserFloat1 not found") == std::string::npos) warnings += "ST_Warn: iUserFloat1 not found in shader.\n"; 
        if (iUserColor1Location_param == -1 && warnings.find("iUserColor1 not found") == std::string::npos) warnings += "ST_Warn: iUserColor1 not found in shader.\n"; 
        
        for (auto& control : shaderToyUniformControls) { 
            control.location = glGetUniformLocation(program, control.name.c_str());
        }
    } else { 
        iRes = glGetUniformLocation(program, "iResolution"); 
        iTime = glGetUniformLocation(program, "iTime");
        objColor = glGetUniformLocation(program, "u_objectColor");
        scaleU = glGetUniformLocation(program, "u_scale");
        timeSpeedU = glGetUniformLocation(program, "u_timeSpeed");
        colorModU = glGetUniformLocation(program, "u_colorMod");
        patternScaleU = glGetUniformLocation(program, "u_patternScale");
        camPosU = glGetUniformLocation(program, "u_camPos");
        camTargetU = glGetUniformLocation(program, "u_camTarget");
        camFOVU = glGetUniformLocation(program, "u_camFOV");
        lightPosU = glGetUniformLocation(program, "u_lightPosition");
        lightColorU = glGetUniformLocation(program, "u_lightColor");

        shaderToyUniformControls.clear(); 

        if (iRes == -1 && warnings.find("iResolution not found") == std::string::npos) warnings += "Warn: iResolution not found.\n";
        if (iTime == -1 && warnings.find("iTime not found") == std::string::npos) warnings += "Warn: iTime not found.\n";
        if (objColor == -1 && warnings.find("u_objectColor not found") == std::string::npos) warnings += "Warn: u_objectColor not found.\n";
        if (scaleU == -1 && warnings.find("u_scale not found") == std::string::npos) warnings += "Warn: u_scale not found.\n";
    }
}


GLuint createShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString) {
    errorLogString.clear();
    if (vertexShaderID == 0 || fragmentShaderID == 0) {
        errorLogString = "ERROR::PROGRAM::LINK_INVALID_SHADER_ID - One or both shaders failed to compile.";
        if (vertexShaderID != 0 && fragmentShaderID == 0) glDeleteShader(vertexShaderID); 
        else if (vertexShaderID == 0 && fragmentShaderID != 0) glDeleteShader(fragmentShaderID);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShaderID);
    glAttachShader(program, fragmentShaderID);
    glLinkProgram(program);
    
    int success; glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength; glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLog(logLength > 0 ? logLength + 1 : 257);
        if (logLength == 0) infoLog[0] = '\0';
        glGetShaderInfoLog(program, static_cast<GLsizei>(infoLog.size()-1), NULL, infoLog.data());
        errorLogString = "ERROR::PROGRAM::LINK_FAIL\n" + std::string(infoLog.data());
        std::cerr << errorLogString << std::endl;
        glDeleteProgram(program);
        program = 0; 
    }
    
    glDeleteShader(vertexShaderID); 
    glDeleteShader(fragmentShaderID);
    
    return program; 
}
