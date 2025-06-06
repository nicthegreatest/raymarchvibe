#include "ShadertoyIntegration.h" 
#include "ShaderManager.h"         
#include "UIManager.h"           
#include "Utils.h" // <<< ADDED Utils.h

#include "ShaderParser.h"        
#include "AudioSystem.h"         
#include "TextEditor.h"          
#include "imgui_internal.h"      

#include <fstream>               
#include <sstream>               
#include <iostream>              
#include <algorithm>             
// #include <cctype> // No longer needed if TrimString is removed and not used elsewhere for cctype functions

// Constructor
UIManager::UIManager(
    ShaderManager& sm, ShaderParser& sp, AudioSystem& as, TextEditor& editorRef,
    bool& shadertoyMode, bool& showGui,
    bool& showHelpWindow, bool& showAudioReactivityWindow, bool& g_ShowConfirmExitPopup,
    std::string& currentShaderCodeEditor, std::string& shaderLoadErrorUi,
    std::string& currentShaderPathUi, std::string& shaderCompileErrorLogUi,
    float* objectColorParam, float& scaleParam, float& timeSpeedParam,
    float* colorModParam, float& patternScaleParam,
    float* cameraPositionParam, float* cameraTargetParam, float& cameraFOVParam,
    float* lightPositionParam, float* lightColorParam,
    float& iUserFloat1Param, float* iUserColor1Param,
    char* filePathBufferLoad, char* filePathBufferSaveAs,
    char* shadertoyInputBuffer, std::string& shadertoyApiKey,
    const std::vector<ShaderSample_UI>& samples, size_t& currentSampleIdx
) : m_shaderManager(sm), m_shaderParser(sp), m_audioSystem(as), m_editor(editorRef),
    m_shadertoyMode(shadertoyMode), m_showGui(showGui),
    m_showHelpWindow_ref(showHelpWindow), m_showAudioReactivityWindow_ref(showAudioReactivityWindow),
    m_g_ShowConfirmExitPopup_ref(g_ShowConfirmExitPopup),
    m_currentShaderCode_editor_ref(currentShaderCodeEditor),
    m_shaderLoadError_ui_ref(shaderLoadErrorUi),
    m_currentShaderPath_ui_ref(currentShaderPathUi),
    m_shaderCompileErrorLog_ui_ref(shaderCompileErrorLogUi),
    p_objectColor_param(objectColorParam), r_scale_param(scaleParam), r_timeSpeed_param(timeSpeedParam),
    p_colorMod_param(colorModParam), r_patternScale_param(patternScaleParam),
    p_cameraPosition_param(cameraPositionParam), p_cameraTarget_param(cameraTargetParam), r_cameraFOV_param(cameraFOVParam),
    p_lightPosition_param(lightPositionParam), p_lightColor_param(lightColorParam),
    r_iUserFloat1_param(iUserFloat1Param), p_iUserColor1_param(iUserColor1Param),
    p_filePathBuffer_Load(filePathBufferLoad), p_filePathBuffer_SaveAs(filePathBufferSaveAs),
    p_shadertoyInputBuffer(shadertoyInputBuffer), r_shadertoyApiKey(shadertoyApiKey),
    m_shaderSamples_ref(samples), r_currentSampleIndex(currentSampleIdx)
{
    // Initialize shader templates
    m_nativeShaderTemplate = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec2 iResolution; uniform float iTime; uniform float iAudioAmp;
uniform vec3 u_objectColor = vec3(0.7,0.5,0.9); uniform float u_scale = 0.8; uniform float u_timeSpeed = 0.3; uniform vec3 u_colorMod = vec3(0.2,0.3,0.4); uniform float u_patternScale = 5.0;
uniform vec3 u_camPos = vec3(0,0.8,-2.5); uniform vec3 u_camTarget = vec3(0,0,0); uniform float u_camFOV = 60.0;
uniform vec3 u_lightPosition = vec3(3,2,-4); uniform vec3 u_lightColor = vec3(1,0.95,0.9);
const float PI=3.14159265359; const float PHI=1.618033988749895;
float r(float d){return d*PI/180.;} mat3 rX(float a){float s=sin(a),c=cos(a);return mat3(1,0,0,0,c,-s,0,s,c);} mat3 rY(float a){float s=sin(a),c=cos(a);return mat3(c,0,s,0,1,0,-s,0,c);} mat3 rZ(float a){float s=sin(a),c=cos(a);return mat3(c,-s,0,s,c,0,0,0,1);}
float sdD(vec3 p,float R){p=abs(p);vec3 n1=vec3(1,PHI,0);vec3 n2=vec3(0,1,PHI);vec3 n3=vec3(PHI,0,1);float d=dot(p,normalize(n1));d=max(d,dot(p,normalize(n2)));d=max(d,dot(p,normalize(n3)));return d-R;}
float map(vec3 p,float t){float et=t*u_timeSpeed;float R=0.5*u_scale;vec3 o=vec3(0,R*0.8,0);p-=o;mat3 rot=rY(et)*rX(et*0.7)*rZ(et*0.4);p=rot*p;return sdD(p,R);}
vec3 norm(vec3 p,float t){float e=0.0005*u_scale;vec2 E=vec2(e,0);return normalize(vec3(map(p+E.xyy,t)-map(p-E.xyy,t),map(p+E.yxy,t)-map(p-E.yxy,t),map(p+E.yyx,t)-map(p-E.yyx,t)));}
float march(vec3 ro,vec3 rd,float t){float T=0;for(int i=0;i<96;i++){vec3 p=ro+rd*T;float d=map(p,t);if(d<(0.001*T)||d<0.0001)return T;T+=d*0.75;if(T>30.)break;}return -1.;}
mat3 cam(vec3 ro,vec3 ta,vec3 up){vec3 f=normalize(ta-ro);vec3 r=normalize(cross(f,up));if(length(r)<.0001)r=normalize(cross(f,vec3(0,0,-1)));if(length(r)<.0001)r=normalize(cross(f,vec3(1,0,0)));vec3 u=normalize(cross(r,f));return mat3(r,u,f);}
void main(){vec2 P=(2.*gl_FragCoord.xy-iResolution.xy)/iResolution.y;vec3 ro=u_camPos;vec3 ta=u_camTarget;mat3 ctw=cam(ro,ta,vec3(0,1,0));float ff=tan(r(u_camFOV)*.5);vec3 rd_l=normalize(vec3(P.x*ff,P.y*ff,1));vec3 rd=ctw*rd_l;float T=march(ro,rd,iTime);vec3 col=vec3(.05,.02,.08)*(1.+iAudioAmp*2.);if(T>-.5){vec3 hp=ro+rd*T;vec3 N=norm(hp,iTime);vec3 ldir=normalize(u_lightPosition-hp);float diff=max(0.,dot(N,ldir));float ao=0;float aos=0.01*u_scale;float aotd=0;for(int j=0;j<4;j++){aotd+=aos;float dao=map(hp+N*aotd*.5,iTime);ao+=max(0.,(aotd*.5-dao));aos*=1.7;}ao=1.-clamp(ao*(0.1/(u_scale*u_scale+.01)),0.,1.);vec3 vdir=normalize(ro-hp);vec3 rdir=reflect(-ldir,N);float specA=max(dot(vdir,rdir),0.);float spec=pow(specA,32.+u_patternScale*10.);vec3 bc=u_objectColor;float pv=sin(hp.x*u_patternScale+iTime*u_timeSpeed*.5)*cos(hp.y*u_patternScale-iTime*u_timeSpeed*.3)*sin(hp.z*u_patternScale+iTime*u_timeSpeed*.7);bc+=u_colorMod*(.5+.5*pv);bc=clamp(bc,0.,1.);vec3 lit=bc*(diff*.7+.2*ao)*u_lightColor;lit+=u_lightColor*spec*.4*(.5+.5*bc.r);col=lit;float fog=smoothstep(u_scale*2.,u_scale*15.,T);col=mix(col,vec3(.05,.02,.08),fog);}col=pow(col,vec3(1./2.2));FragColor=vec4(col,1.);}
)GLSL";

    m_shadertoyShaderTemplate = R"GLSL(
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy;
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));
    col *= (0.5 + 0.5 * iAudioAmp);
    fragColor = vec4(col,1.0);
})GLSL";
}

UIManager::~UIManager() {}

void UIManager::Initialize() {
    // Currently no specific initialization needed here beyond what main.cpp does
}

void UIManager::SetupInitialEditorAndShader() {
    auto lang = TextEditor::LanguageDefinition::GLSL();
    m_editor.SetLanguageDefinition(lang);
    m_editor.SetShowWhitespaces(false);
    m_editor.SetTabSize(4);

    std::string initialShaderContent = m_shaderManager.loadShaderSourceFromFile(m_currentShaderPath_ui_ref.c_str());
    if (!initialShaderContent.empty()) {
        m_editor.SetText(initialShaderContent);
        m_currentShaderCode_editor_ref = initialShaderContent;
        m_shadertoyMode = (initialShaderContent.find("mainImage") != std::string::npos);
    } else {
        if (!m_shaderLoadError_ui_ref.empty()) m_shaderLoadError_ui_ref += "\n";
        m_shaderLoadError_ui_ref += m_shaderManager.getGeneralStatus();
        if (m_shaderLoadError_ui_ref.find("Failed to read shader file") == std::string::npos && m_shaderLoadError_ui_ref.find("ERROR") == std::string::npos) {
             m_shaderLoadError_ui_ref += "Failed to load initial shader '" + m_currentShaderPath_ui_ref + "' into editor.";
        }
        const char* errorFallbackMaterial = "void mainImage(out vec4 C,vec2 U){C=vec4(0.5,0,0,1);}";
        const char* errorFallback = m_shadertoyMode ? errorFallbackMaterial : "// Initial shader load failed.\nvoid main(){gl_FragColor=vec4(0.5,0,0,1);}";
        m_editor.SetText(errorFallback);
        m_currentShaderCode_editor_ref = errorFallback;
    }
    
    ApplyShaderFromEditorAndHandleResults(); 
}


void UIManager::RequestWindowSnap() {
    m_snapWindowsNextFrame = true;
}

void UIManager::RenderAllUI(GLFWwindow* window, bool isFullscreen, int& storedWindowX, int& storedWindowY, int& storedWindowWidth, int& storedWindowHeight, float& deltaTime, int& frameCount, float mouseState_iMouse[4]) {
    (void)isFullscreen; (void)storedWindowX; (void)storedWindowY; (void)storedWindowWidth; (void)storedWindowHeight;
    (void)deltaTime; (void)frameCount; (void)mouseState_iMouse;

    if (m_snapWindowsNextFrame) {
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
        
        m_snapWindowsNextFrame = false;
    }

    RenderStatusWindow();
    RenderShaderEditorWindow(); 
    RenderConsoleWindow();      

    if (m_showHelpWindow_ref) {
        RenderHelpWindow(); 
    }
    if (m_showAudioReactivityWindow_ref) {
        RenderAudioReactivityWindow(); 
    }

    if (m_g_ShowConfirmExitPopup_ref) {
        ImGui::OpenPopup("Confirm Exit##UIMgr");
    }
    if (ImGui::BeginPopupModal("Confirm Exit##UIMgr", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Are you sure you want to exit?"); ImGui::Text("Have you saved your work?"); ImGui::Separator();
        if (ImGui::Button("Yes, Exit", ImVec2(120, 0))) {
            glfwSetWindowShouldClose(window, true);
            m_g_ShowConfirmExitPopup_ref = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus(); ImGui::SameLine();
        if (ImGui::Button("No, Cancel", ImVec2(120, 0))) {
            m_g_ShowConfirmExitPopup_ref = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void UIManager::ClearEditorErrorMarkers() {
    TextEditor::ErrorMarkers emptyMarkers;
    m_editor.SetErrorMarkers(emptyMarkers);
}

void UIManager::ApplyShaderFromEditorAndHandleResults() {
    m_currentShaderCode_editor_ref = m_editor.GetText();
    if (m_shaderManager.applyShaderCode(m_currentShaderCode_editor_ref, m_shadertoyMode)) {
        m_shaderLoadError_ui_ref = m_shaderManager.getGeneralStatus();
        std::string compileLog = m_shaderManager.getCompileErrorLog();
        if (!compileLog.empty() && (m_shaderLoadError_ui_ref.find("Applied from editor!") == std::string::npos && m_shaderLoadError_ui_ref.find("Applied with warnings") == std::string::npos)) {
            m_editor.SetErrorMarkers(m_shaderParser.ParseGlslErrorLog(compileLog));
            m_shaderCompileErrorLog_ui_ref = compileLog;
        } else {
            ClearEditorErrorMarkers();
            m_shaderCompileErrorLog_ui_ref.clear();
        }
    } else {
        m_shaderLoadError_ui_ref = m_shaderManager.getGeneralStatus();
        m_shaderCompileErrorLog_ui_ref = m_shaderManager.getCompileErrorLog();
        if (!m_shaderCompileErrorLog_ui_ref.empty()) {
            m_editor.SetErrorMarkers(m_shaderParser.ParseGlslErrorLog(m_shaderCompileErrorLog_ui_ref));
        }
    }
}

void UIManager::RenderStatusWindow() {
    ImGui::Begin("Status");
    ImGuiIO& io = ImGui::GetIO(); 

    ImGui::Text("Hello from drewp/darkrange!");
    ImGui::TextWrapped("F12: Fullscreen | Spacebar: Toggle GUI | F5: Apply Editor Code | ESC: Close");
    if(ImGui::Button("Show Help")) { m_showHelpWindow_ref = true; } ImGui::SameLine();
    if(ImGui::Button("Audio")) { m_showAudioReactivityWindow_ref = true; }

    ImGui::Separator(); ImGui::Text("Current: %s", m_currentShaderPath_ui_ref.c_str()); 
    if (ImGui::Checkbox("Shadertoy Mode", &m_shadertoyMode)) {
        ApplyShaderFromEditorAndHandleResults();
    }
    ImGui::Separator();

    if (!m_shadertoyMode) {
        ImGui::Text("Shader Parameters:"); ImGui::Spacing();
        if (ImGui::CollapsingHeader("Colour Parameters##StatusColours" )) { ImGui::ColorEdit3("Object Colour##Params", p_objectColor_param); ImGui::ColorEdit3("Colour Mod##Params", p_colorMod_param); } ImGui::Spacing();
        if (ImGui::CollapsingHeader("Patterns of Time and Space##StatusPatterns" )) { ImGui::SliderFloat("Scale##Params", &r_scale_param, 0.1f, 3.0f); ImGui::SliderFloat("Pattern Scale##Params", &r_patternScale_param, 0.1f, 10.0f); ImGui::SliderFloat("Time Speed##Params", &r_timeSpeed_param, 0.0f, 5.0f); } ImGui::Spacing();
        if (ImGui::CollapsingHeader("Camera Controls##StatusCamera" )) { ImGui::DragFloat3("Position##Cam", p_cameraPosition_param, 0.1f); ImGui::DragFloat3("Target##Cam", p_cameraTarget_param, 0.1f); ImGui::SliderFloat("FOV##Cam", &r_cameraFOV_param, 15.0f, 120.0f); } ImGui::Spacing();
        if (ImGui::CollapsingHeader("Lighting Controls##StatusLighting" )) { ImGui::DragFloat3("Light Pos##Light", p_lightPosition_param, 0.1f); ImGui::ColorEdit3("Light Colour##Light", p_lightColor_param); } ImGui::Separator();
    }
    if (m_shadertoyMode) { ImGui::Text("Shadertoy User Parameters:"); ImGui::SliderFloat("iUserFloat1", &r_iUserFloat1_param, 0.0f, 1.0f); ImGui::ColorEdit3("iUserColour1", p_iUserColor1_param); ImGui::Separator(); }

    if (m_shadertoyMode && !m_shaderParser.GetUniformControls().empty()) { 
        if (ImGui::CollapsingHeader("Shader Uniforms (from metadata)##STUniformsGUI")) {
            for (size_t i = 0; i < m_shaderParser.GetUniformControls().size(); ++i) { 
                auto& control = m_shaderParser.GetUniformControls()[i]; 
                if (control.location == -1 && m_shadertoyMode) continue;
                std::string label = control.metadata.value("label", control.name);
                ImGui::PushID(static_cast<int>(i) + 1000);
                if (control.glslType == "float") { float min_val = control.metadata.value("min", 0.0f); float max_val = control.metadata.value("max", 1.0f); if (min_val < max_val) { ImGui::SliderFloat(label.c_str(), &control.fValue, min_val, max_val); } else { ImGui::DragFloat(label.c_str(), &control.fValue, control.metadata.value("step", 0.01f)); } }
                else if (control.glslType == "vec2") { ImGui::DragFloat2(label.c_str(), control.v2Value, control.metadata.value("step", 0.01f)); }
                 else if (control.glslType == "vec3") { if (control.isColor) { ImGui::ColorEdit3(label.c_str(), control.v3Value); } else { ImGui::DragFloat3(label.c_str(), control.v3Value, control.metadata.value("step", 0.01f)); } }
                 else if (control.glslType == "vec4") { if (control.isColor) { ImGui::ColorEdit4(label.c_str(), control.v4Value); } else { ImGui::DragFloat4(label.c_str(), control.v4Value, control.metadata.value("step", 0.01f)); } }
                 else if (control.glslType == "int") { int min_val_i = control.metadata.value("min", 0); int max_val_i = control.metadata.value("max", 100); if (control.metadata.contains("min") && control.metadata.contains("max") && min_val_i < max_val_i) { ImGui::SliderInt(label.c_str(), &control.iValue, min_val_i, max_val_i); } else { ImGui::DragInt(label.c_str(), &control.iValue, control.metadata.value("step", 1.0f)); } }
                 else if (control.glslType == "bool") { ImGui::Checkbox(label.c_str(), &control.bValue); }
                ImGui::PopID();
            }
        } ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Global Constants##ConstControlsGUI")) { 
        if (m_shaderParser.GetConstControls().empty()) { ImGui::TextDisabled(" (No global constants detected)"); } 
        else {
            for (size_t i = 0; i < m_shaderParser.GetConstControls().size(); ++i) { 
                ImGui::PushID(static_cast<int>(i) + 2000);
                auto& control = m_shaderParser.GetConstControls()[i]; 
                bool valueChanged = false;
                float dragSpeed = 0.01f;
                if (control.glslType == "float" && std::abs(control.fValue) > 50.0f) dragSpeed = 0.1f;
                else if (control.glslType == "float" && std::abs(control.fValue) > 500.0f) dragSpeed = 1.0f;
                if (control.glslType == "float") { if (ImGui::DragFloat(control.name.c_str(), &control.fValue, dragSpeed)) valueChanged = true; }
                else if (control.glslType == "int") { if (ImGui::DragInt(control.name.c_str(), &control.iValue, 1)) valueChanged = true; }
                else if (control.glslType == "vec2") { if (ImGui::DragFloat2(control.name.c_str(), control.v2Value, dragSpeed)) valueChanged = true; }
                else if (control.glslType == "vec3") { if (control.isColor) { if (ImGui::ColorEdit3(control.name.c_str(), control.v3Value, ImGuiColorEditFlags_Float)) valueChanged = true; } else { if (ImGui::DragFloat3(control.name.c_str(), control.v3Value, dragSpeed)) valueChanged = true; } }
                else if (control.glslType == "vec4") { if (control.isColor) { if (ImGui::ColorEdit4(control.name.c_str(), control.v4Value, ImGuiColorEditFlags_Float)) valueChanged = true; } else { if (ImGui::DragFloat4(control.name.c_str(), control.v4Value, dragSpeed)) valueChanged = true; } }

                if (valueChanged) {
                    std::string currentEditorCode = m_editor.GetText(); 
                    std::string modifiedCode = m_shaderParser.UpdateConstValueInString(currentEditorCode, control); 
                    if (!modifiedCode.empty()) {
                        m_editor.SetText(modifiedCode);
                        ApplyShaderFromEditorAndHandleResults(); 
                    } else { m_shaderLoadError_ui_ref = "Failed to update const: " + control.name; }
                } ImGui::PopID();
            }
        } ImGui::Separator();
    }
    if (ImGui::CollapsingHeader("Shader Defines##DefineControlsGUI")) { 
         if (m_shaderParser.GetDefineControls().empty()) { ImGui::TextDisabled(" (No defines detected)"); } 
         else {
            for (size_t i = 0; i < m_shaderParser.GetDefineControls().size(); ++i) { 
                ImGui::PushID(static_cast<int>(i) + 3000);
                auto& control = m_shaderParser.GetDefineControls()[i]; 
                bool defineEnabledState = control.isEnabled;
                if (ImGui::Checkbox("", &defineEnabledState)) { 
                    std::string currentEditorCode = m_editor.GetText();
                    std::string modifiedCode = m_shaderParser.ToggleDefineInString(currentEditorCode, control.name, defineEnabledState, control.originalValueString); 
                    if (!modifiedCode.empty()) {
                        m_editor.SetText(modifiedCode);
                        ApplyShaderFromEditorAndHandleResults();
                    } else { m_shaderLoadError_ui_ref = "Failed to toggle define: " + control.name; }
                } ImGui::SameLine();
                if (control.hasValue && control.isEnabled) {
                    std::string dragFloatLabel = "##value_define_"; dragFloatLabel += control.name;
                    float tempFloatValue = control.floatValue;
                    ImGui::SetNextItemWidth(100.0f);
                    if (ImGui::DragFloat(dragFloatLabel.c_str(), &tempFloatValue, 0.01f, 0.0f, 0.0f, "%.3f")) {
                       control.floatValue = tempFloatValue; 
                       std::string currentEditorCode = m_editor.GetText();
                       std::string modifiedCode = m_shaderParser.UpdateDefineValueInString(currentEditorCode, control.name, tempFloatValue); 
                       if (!modifiedCode.empty()) {
                            m_editor.SetText(modifiedCode);
                            ApplyShaderFromEditorAndHandleResults();
                        } else { m_shaderLoadError_ui_ref = "Failed to update define: " + control.name; }
                    } ImGui::SameLine();
                } ImGui::TextUnformatted(control.name.c_str()); ImGui::PopID();
            }
         } ImGui::Separator();
    }
    ImGui::Text("FPS: %.1f", io.Framerate); 
    ImGui::End();
}

void UIManager::RenderShaderEditorWindow() {
    ImGui::Begin("Shader Editor");
    
    if (ImGui::CollapsingHeader("Load from Shadertoy" )) { 
        ImGui::InputTextWithHint("##ShadertoyInput", "Shadertoy ID/URL", p_shadertoyInputBuffer, 256); 
        ImGui::SameLine(); 
        if (ImGui::Button("Fetch & Apply##ShadertoyApply")) { 
            std::string idOrUrl = p_shadertoyInputBuffer; 
            if(!idOrUrl.empty()){
                std::string sId = ShadertoyIntegration::ExtractId(idOrUrl); 
                if(!sId.empty()){ 
                    m_shaderLoadError_ui_ref="Fetching Shadertoy "+sId+"..."; 
                    m_shaderCompileErrorLog_ui_ref.clear(); 
                    m_audioSystem.ClearLastError(); 
                    std::string fErr; 
                    std::string fCode = ShadertoyIntegration::FetchCode(sId, r_shadertoyApiKey, fErr);
                    if(!fCode.empty()){
                        m_editor.SetText(fCode);
                        m_currentShaderCode_editor_ref=fCode; 
                        m_currentShaderPath_ui_ref="Shadertoy_"+sId+".frag";
                        strncpy(p_filePathBuffer_Load, m_currentShaderPath_ui_ref.c_str(), 256-1);
                        p_filePathBuffer_Load[256-1]=0;
                        strncpy(p_filePathBuffer_SaveAs, m_currentShaderPath_ui_ref.c_str(), 256-1);
                        p_filePathBuffer_SaveAs[256-1]=0;
                        m_shadertoyMode=true;
                        this->ApplyShaderFromEditorAndHandleResults();
                    } else {
                        m_shaderLoadError_ui_ref=fErr;
                        if(m_shaderLoadError_ui_ref.empty()) m_shaderLoadError_ui_ref="Failed to retrieve for ID: "+sId;
                    }
                } else { m_shaderLoadError_ui_ref="Invalid Shadertoy ID/URL."; }
            } else { m_shaderLoadError_ui_ref="Enter Shadertoy ID/URL."; }
        } 
        ImGui::SameLine(); 
        if (ImGui::Button("Load to Editor##ShadertoyLoad")) { 
            std::string idOrUrl=p_shadertoyInputBuffer; 
            if(!idOrUrl.empty()){
                std::string sId = ShadertoyIntegration::ExtractId(idOrUrl); 
                if(!sId.empty()){ 
                    m_shaderLoadError_ui_ref="Fetching Shadertoy "+sId+"..."; 
                    m_shaderCompileErrorLog_ui_ref.clear();
                    m_audioSystem.ClearLastError();
                    std::string fErr;
                    std::string fCode = ShadertoyIntegration::FetchCode(sId, r_shadertoyApiKey, fErr);
                    if(!fCode.empty()){
                        m_editor.SetText(fCode);
                        m_currentShaderCode_editor_ref=fCode;
                        m_currentShaderPath_ui_ref="Shadertoy_"+sId+".frag";
                        strncpy(p_filePathBuffer_Load,m_currentShaderPath_ui_ref.c_str(),256-1);
                        p_filePathBuffer_Load[256-1]=0;
                        strncpy(p_filePathBuffer_SaveAs,m_currentShaderPath_ui_ref.c_str(),256-1);
                        p_filePathBuffer_SaveAs[256-1]=0;
                        m_shadertoyMode=true;
                        m_shaderLoadError_ui_ref="Shadertoy '"+sId+"' loaded. Press Apply.";
                        m_shaderParser.ScanAndPrepareDefineControls(m_currentShaderCode_editor_ref.c_str());
                        m_shaderParser.ScanAndPrepareUniformControls(m_currentShaderCode_editor_ref.c_str());
                        m_shaderParser.ScanAndPrepareConstControls(m_currentShaderCode_editor_ref);
                        this->ClearEditorErrorMarkers();
                    } else {
                        m_shaderLoadError_ui_ref=fErr;
                        if(m_shaderLoadError_ui_ref.empty())m_shaderLoadError_ui_ref="Failed to retrieve for ID: "+sId;
                    }
                } else {m_shaderLoadError_ui_ref="Invalid Shadertoy ID/URL.";}
            } else {m_shaderLoadError_ui_ref="Enter Shadertoy ID/URL.";}
        }
        ImGui::TextWrapped("Note: Requires network. Live fetches shaders by ID (or URL) from Shadertoy.com. Also doesn't currently support textures (iChannel)."); 
        ImGui::Spacing(); 
    }

    if (ImGui::CollapsingHeader("Load Sample Shader" )) { 
        if (ImGui::BeginCombo("##SampleShaderCombo", m_shaderSamples_ref[r_currentSampleIndex].name)) { 
            for (size_t n = 0; n < m_shaderSamples_ref.size(); n++) { 
                const bool is_selected = (r_currentSampleIndex == n); 
                if (ImGui::Selectable(m_shaderSamples_ref[n].name, is_selected)) r_currentSampleIndex = n; 
                if (is_selected) ImGui::SetItemDefaultFocus(); 
            } 
            ImGui::EndCombo(); 
        } 
        ImGui::SameLine(); 
        if (ImGui::Button("Load & Apply Sample")) { 
            if (r_currentSampleIndex > 0 && r_currentSampleIndex < m_shaderSamples_ref.size()) { 
                std::string selPath=m_shaderSamples_ref[r_currentSampleIndex].filePath; 
                if(!selPath.empty()){
                    m_shaderLoadError_ui_ref.clear();
                    m_audioSystem.ClearLastError();
                    std::string newContent=m_shaderManager.loadShaderSourceFromFile(selPath.c_str());
                    if(!newContent.empty()){
                        m_editor.SetText(newContent);
                        m_currentShaderCode_editor_ref=newContent;
                        m_currentShaderPath_ui_ref=selPath;
                        strncpy(p_filePathBuffer_Load,m_currentShaderPath_ui_ref.c_str(),256-1);
                        p_filePathBuffer_Load[256-1]=0;
                        strncpy(p_filePathBuffer_SaveAs,m_currentShaderPath_ui_ref.c_str(),256-1);
                        p_filePathBuffer_SaveAs[256-1]=0;
                        m_shadertoyMode=(newContent.find("mainImage")!=std::string::npos);
                        this->ApplyShaderFromEditorAndHandleResults();
                        m_shaderLoadError_ui_ref = "Sample '"+std::string(m_shaderSamples_ref[r_currentSampleIndex].name)+"' loaded & " + m_shaderLoadError_ui_ref;
                    } else {
                        m_shaderLoadError_ui_ref="ERROR: Failed to load sample '"+std::string(m_shaderSamples_ref[r_currentSampleIndex].name)+"'. "+m_shaderManager.getGeneralStatus();
                    }
                }
            } else {m_shaderLoadError_ui_ref="Please select a valid sample.";}
        } 
        ImGui::Spacing(); 
    }

    if (ImGui::CollapsingHeader("Load From File" )) { 
        ImGui::InputText("Path##LoadFile", p_filePathBuffer_Load, 256); 
        if (ImGui::Button("Load to editor##LoadFileBtn")) { 
            m_shaderLoadError_ui_ref.clear();
            m_shaderCompileErrorLog_ui_ref.clear();
            m_audioSystem.ClearLastError();
            std::string newContent=m_shaderManager.loadShaderSourceFromFile(p_filePathBuffer_Load);
            if(!newContent.empty()){
                m_editor.SetText(newContent);
                m_currentShaderCode_editor_ref=newContent;
                m_currentShaderPath_ui_ref=p_filePathBuffer_Load;
                strncpy(p_filePathBuffer_SaveAs,m_currentShaderPath_ui_ref.c_str(),256-1);
                p_filePathBuffer_SaveAs[256-1]=0;
                m_shaderLoadError_ui_ref="Loaded '"+m_currentShaderPath_ui_ref+"'. Press Apply.";
                m_shadertoyMode=(newContent.find("mainImage")!=std::string::npos);
                m_shaderParser.ScanAndPrepareDefineControls(m_currentShaderCode_editor_ref.c_str());
                if(m_shadertoyMode) m_shaderParser.ScanAndPrepareUniformControls(m_currentShaderCode_editor_ref.c_str());
                else m_shaderParser.ClearAllControls();
                m_shaderParser.ScanAndPrepareConstControls(m_currentShaderCode_editor_ref);
                this->ClearEditorErrorMarkers();
            } else {
                m_shaderLoadError_ui_ref="Failed to load: "+std::string(p_filePathBuffer_Load)+". "+m_shaderManager.getGeneralStatus();
            }
        } 
        ImGui::SameLine(); 
        if (ImGui::Button("Load and Apply##ReloadFileBtn")) { 
            m_shaderLoadError_ui_ref.clear();
            m_shaderCompileErrorLog_ui_ref.clear();
            m_audioSystem.ClearLastError();
            std::string rlContent=m_shaderManager.loadShaderSourceFromFile(p_filePathBuffer_Load);
            if(!rlContent.empty()){
                m_editor.SetText(rlContent);
                m_currentShaderCode_editor_ref=rlContent;
                m_currentShaderPath_ui_ref=p_filePathBuffer_Load;
                strncpy(p_filePathBuffer_SaveAs,m_currentShaderPath_ui_ref.c_str(),256-1);
                p_filePathBuffer_SaveAs[256-1]=0;
                m_shadertoyMode=(rlContent.find("mainImage")!=std::string::npos);
                this->ApplyShaderFromEditorAndHandleResults();
                m_shaderLoadError_ui_ref="'"+m_currentShaderPath_ui_ref+"' loaded & "+m_shaderLoadError_ui_ref;
            } else {
                m_shaderLoadError_ui_ref="Failed to load/apply: "+std::string(p_filePathBuffer_Load)+". "+m_shaderManager.getGeneralStatus();
            }
        } 
        ImGui::Separator(); 
        ImGui::Spacing(); 
    }

    if (ImGui::CollapsingHeader("New Shader" )) { 
        if (ImGui::Button("New Native Shader")) { 
            m_editor.SetText(m_nativeShaderTemplate);
            m_currentShaderCode_editor_ref=m_nativeShaderTemplate;
            m_currentShaderPath_ui_ref="Untitled_Native.frag";
            strncpy(p_filePathBuffer_Load,m_currentShaderPath_ui_ref.c_str(),256-1);
            p_filePathBuffer_Load[256-1]=0;
            strncpy(p_filePathBuffer_SaveAs,m_currentShaderPath_ui_ref.c_str(),256-1);
            p_filePathBuffer_SaveAs[256-1]=0;
            m_shadertoyMode=false;
            m_shaderCompileErrorLog_ui_ref.clear();
            m_shaderLoadError_ui_ref="Native template. Press Apply.";
            m_audioSystem.ClearLastError();
            m_shaderParser.ScanAndPrepareDefineControls(m_currentShaderCode_editor_ref.c_str());
            m_shaderParser.ClearAllControls(); 
            m_shaderParser.ScanAndPrepareConstControls(m_currentShaderCode_editor_ref);
            this->ClearEditorErrorMarkers();
        } 
        ImGui::SameLine(); 
        if (ImGui::Button("New Shadertoy Shader")) { 
            m_editor.SetText(m_shadertoyShaderTemplate);
            m_currentShaderCode_editor_ref=m_shadertoyShaderTemplate;
            m_currentShaderPath_ui_ref="Untitled_Shadertoy.frag";
            strncpy(p_filePathBuffer_Load,m_currentShaderPath_ui_ref.c_str(),256-1);
            p_filePathBuffer_Load[256-1]=0;
            strncpy(p_filePathBuffer_SaveAs,m_currentShaderPath_ui_ref.c_str(),256-1);
            p_filePathBuffer_SaveAs[256-1]=0;
            m_shadertoyMode=true;
            m_shaderCompileErrorLog_ui_ref.clear();
            m_shaderLoadError_ui_ref="Shadertoy template. Press Apply.";
            m_audioSystem.ClearLastError();
            m_shaderParser.ScanAndPrepareDefineControls(m_currentShaderCode_editor_ref.c_str());
            m_shaderParser.ScanAndPrepareUniformControls(m_currentShaderCode_editor_ref.c_str());
            m_shaderParser.ScanAndPrepareConstControls(m_currentShaderCode_editor_ref);
            this->ClearEditorErrorMarkers();
        } 
        ImGui::Spacing(); 
    }

    if (ImGui::CollapsingHeader("Save Shader" )) { 
        ImGui::Text("Editing: %s", m_currentShaderPath_ui_ref.c_str()); 
        if (ImGui::Button("Save Current File")) { 
            std::ofstream oF(m_currentShaderPath_ui_ref); 
            if(oF.is_open()){
                oF<<m_editor.GetText();
                oF.close();
                m_shaderLoadError_ui_ref="Saved: "+m_currentShaderPath_ui_ref;
            } else {
                m_shaderLoadError_ui_ref="ERROR saving: "+m_currentShaderPath_ui_ref;
            }
        } 
        ImGui::InputText("Save As Path", p_filePathBuffer_SaveAs, 256); 
        ImGui::SameLine(); 
        if (ImGui::Button("Save As...")) { 
            std::string sap(p_filePathBuffer_SaveAs); 
            if(sap.empty()){
                m_shaderLoadError_ui_ref="ERROR: 'Save As' path empty.";
            } else {
                std::ofstream oF(sap);
                if(oF.is_open()){
                    oF<<m_editor.GetText();
                    oF.close();
                    m_currentShaderPath_ui_ref=sap;
                    strncpy(p_filePathBuffer_Load,m_currentShaderPath_ui_ref.c_str(),256-1);
                    p_filePathBuffer_Load[256-1]=0;
                    m_shaderLoadError_ui_ref="Saved: "+sap;
                } else {
                    m_shaderLoadError_ui_ref="ERROR saving: "+sap;
                }
            }
        } 
        ImGui::Spacing(); 
    }

    if (!m_shaderLoadError_ui_ref.empty() && m_shaderCompileErrorLog_ui_ref.empty()) { 
        ImGui::TextColored(ImVec4(1.0f,1.0f,0.0f,1.0f), "Status: %s", m_shaderLoadError_ui_ref.c_str()); 
    }
    ImGui::Separator();
    ImGui::Text("Shader Code Editor"); ImGui::Dummy(ImVec2(0.0f,5.0f));
    float bbh = ImGui::GetFrameHeightWithSpacing()*2.2f; 
    ImVec2 es(-1.0f,ImGui::GetContentRegionAvail().y-bbh); 
    if(es.y<ImGui::GetTextLineHeight()*10)es.y=ImGui::GetTextLineHeight()*10;
    m_editor.Render("ShaderSourceEditor", es, true);

    if (ImGui::Button("Apply from Editor (F5)")) {
        this->ApplyShaderFromEditorAndHandleResults();
    } 
    ImGui::SameLine();
    if(ImGui::Button("Reset Parameters")) {
        p_objectColor_param[0]=0.8f;p_objectColor_param[1]=0.9f;p_objectColor_param[2]=1.0f;
        r_scale_param=1.f;r_timeSpeed_param=1.f;
        p_colorMod_param[0]=.1f;p_colorMod_param[1]=.1f;p_colorMod_param[2]=.2f;
        r_patternScale_param=1.f;
        p_cameraPosition_param[0]=0;p_cameraPosition_param[1]=1;p_cameraPosition_param[2]=-3;
        p_cameraTarget_param[0]=0;p_cameraTarget_param[1]=0;p_cameraTarget_param[2]=0;
        r_cameraFOV_param=60.f;
        p_lightPosition_param[0]=2;p_lightPosition_param[1]=3;p_lightPosition_param[2]=-2;
        p_lightColor_param[0]=1;p_lightColor_param[1]=1;p_lightColor_param[2]=.9f;
        r_iUserFloat1_param=.5f;
        p_iUserColor1_param[0]=.2f;p_iUserColor1_param[1]=.5f;p_iUserColor1_param[2]=.8f;
        
        m_currentShaderCode_editor_ref = m_editor.GetText();
        m_shaderParser.ScanAndPrepareDefineControls(m_currentShaderCode_editor_ref.c_str());
        if(m_shadertoyMode) m_shaderParser.ScanAndPrepareUniformControls(m_currentShaderCode_editor_ref.c_str()); 
        else m_shaderParser.ClearAllControls();
        m_shaderParser.ScanAndPrepareConstControls(m_currentShaderCode_editor_ref);

        this->ApplyShaderFromEditorAndHandleResults(); 
        m_shaderLoadError_ui_ref = "Parameters reset. " + m_shaderLoadError_ui_ref;
    }
    ImGui::End();
} // <<< CORRECTED: Added missing closing brace

void UIManager::RenderConsoleWindow() {
    ImGui::Begin("Console");
    std::string fullLogContent;
    std::string currentAudioError = m_audioSystem.GetLastError();
    bool generalErrorDisplayed = false;

    if (!m_shaderLoadError_ui_ref.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: "); ImGui::SameLine();
        ImGui::TextWrapped("%s", m_shaderLoadError_ui_ref.c_str());
        fullLogContent = "Status: " + m_shaderLoadError_ui_ref;
        generalErrorDisplayed = true;
    }
    if (!currentAudioError.empty()) {
        if (generalErrorDisplayed && !fullLogContent.empty()) { fullLogContent += "\n"; ImGui::Spacing(); }
        else if (fullLogContent.empty()) { generalErrorDisplayed = true;}
        ImVec4 audioColor = (currentAudioError.find("ERROR") != std::string::npos) ? ImVec4(1.0f,0.3f,0.3f,1.0f) : ImVec4(0.7f,0.7f,1.0f,1.0f);
        if (currentAudioError.find("WARN") != std::string::npos) audioColor = ImVec4(1.0f,1.0f,0.0f,1.0f);
        else if (currentAudioError.find("active") != std::string::npos || currentAudioError.find("loaded") != std::string::npos || currentAudioError.find("stopped") != std::string::npos) {
             audioColor = ImVec4(0.7f,1.0f,0.7f,1.0f);
        }
        ImGui::TextColored(audioColor, "Audio: "); ImGui::SameLine();
        ImGui::TextWrapped("%s", currentAudioError.c_str());
        fullLogContent += (fullLogContent.empty() ? "" : "\n") + std::string("Audio: ") + currentAudioError;
    }
    if (!m_shaderCompileErrorLog_ui_ref.empty() && (m_shaderCompileErrorLog_ui_ref.find("Applied from editor!") == std::string::npos && m_shaderCompileErrorLog_ui_ref.find("Applied with warnings") == std::string::npos)) {
        if (generalErrorDisplayed && !fullLogContent.empty()) { fullLogContent += "\n"; ImGui::Spacing(); }
        else if (fullLogContent.empty()) { generalErrorDisplayed = true;}
        ImVec4 logColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        if (m_shaderCompileErrorLog_ui_ref.find("Warning") != std::string::npos || m_shaderCompileErrorLog_ui_ref.find("Warn:") != std::string::npos) { logColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); }
        ImGui::TextColored(logColor, "Shader Log: "); ImGui::SameLine();
        ImGui::TextWrapped("%s", m_shaderCompileErrorLog_ui_ref.c_str());
        fullLogContent += (fullLogContent.empty() ? "" : "\n") + std::string("Shader Log: ") + m_shaderCompileErrorLog_ui_ref;
    }
    if (!generalErrorDisplayed) { ImGui::TextDisabled("[Log is empty]"); fullLogContent = "[Log is empty]"; }
    ImGui::Spacing();
    if (ImGui::Button("Clear Log & Status")) { 
        m_shaderCompileErrorLog_ui_ref.clear(); 
        m_shaderLoadError_ui_ref.clear(); 
        m_audioSystem.ClearLastError(); 
        this->ClearEditorErrorMarkers(); 
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy Log")) { ImGui::SetClipboardText(fullLogContent.c_str()); }
    ImGui::End();
}

void UIManager::RenderHelpWindow() {
    ImGui::SetNextWindowSize(ImVec2(520,460),ImGuiCond_FirstUseEver); 
    if(ImGui::Begin("Help / Instructions##HelpWindow", &m_showHelpWindow_ref)){
        ImGui::TextWrapped("RaymarchVibe Help:\n\nKeybinds:\n- F12: Fullscreen\n- Spacebar: Toggle GUI\n- F5: Apply Editor Code\n- ESC: Close\n\nEditor:\n- Standard text editing.\n- 'Apply' recompiles.\n- 'Reset Parameters' resets UI to defaults.\n\nLoading:\n- Shadertoy: ID/URL. 'Fetch & Apply' or 'Load to Editor'.\n- Sample: Select & 'Load & Apply'.\n- File: Path & 'Load to editor' or 'Load & Apply'.\n- New: Loads templates.\n\nSaving:\n- 'Save Current' or 'Save As...'.\n\nStatus Window:\n- Adjust uniforms, constants, defines.\n\nAudio Window:\n- Select source (Mic, System, File).\n- Link to iAudioAmp.\n\nConsole:\n- Status, errors, warnings.");
        if(ImGui::Button("Close Help")) m_showHelpWindow_ref = false;
        ImGui::End();
    }
}

void UIManager::RenderAudioReactivityWindow() {
    ImGui::Begin("Audio Reactivity", &m_showAudioReactivityWindow_ref);
    bool audioLink = m_audioSystem.IsAudioLinkEnabled();
    if (m_shaderManager.getAudioAmpLocation() == -1 && audioLink) { ImGui::SameLine(); ImGui::TextDisabled("(iAudioAmp not in shader!)"); }
    
    const char* audioSourceItems[] = { "Microphone", "System Audio", "Audio File" };
    int currentUiAudioSourceIndex = m_audioSystem.GetCurrentAudioSourceIndex();
    if (ImGui::Combo("Audio Source", &currentUiAudioSourceIndex, audioSourceItems, IM_ARRAYSIZE(audioSourceItems))) {
        m_audioSystem.SetCurrentAudioSourceIndex(currentUiAudioSourceIndex);
        if (m_shaderLoadError_ui_ref.find("Audio") != std::string::npos || m_shaderLoadError_ui_ref.find("audio") != std::string::npos) {
            m_shaderLoadError_ui_ref.clear(); 
        }
         m_audioSystem.ClearLastError(); 
    }
    ImGui::Spacing();

    if (m_audioSystem.GetCurrentAudioSourceIndex() == 0) { 
        ImGui::Text("Microphone Settings:");
        if (m_audioSystem.WereDevicesEnumerated() && !m_audioSystem.GetCaptureDeviceGUINames().empty()) {
            int currentSelectedDeviceIdx = *m_audioSystem.GetSelectedActualCaptureDeviceIndexPtr();
            if (ImGui::Combo("Input Device", &currentSelectedDeviceIdx, m_audioSystem.GetCaptureDeviceGUINames().data(), static_cast<int>(m_audioSystem.GetCaptureDeviceGUINames().size()))) {
                m_audioSystem.SetSelectedActualCaptureDeviceIndex(currentSelectedDeviceIdx);
                m_audioSystem.InitializeAndStartSelectedCaptureDevice(); 
            }
        } else if (m_audioSystem.WereDevicesEnumerated()) { ImGui::TextDisabled("No microphone input devices found."); }
        else { ImGui::TextDisabled("Audio devices not enumerated or failed."); }

        if (m_audioSystem.IsCaptureDeviceInitialized()) {
            ImGui::Spacing(); ImGui::Text("Live Microphone Amplitude:");
            float visualAmplitude = m_audioSystem.GetCurrentAmplitude() * 4.0f; 
            visualAmplitude = std::max(0.0f, std::min(visualAmplitude, 1.0f)); 
            ImGui::ProgressBar(visualAmplitude, ImVec2(-1.0f, 0.0f));
            ImGui::Text("Raw Avg. Abs: %.4f, Visual: %.2f", m_audioSystem.GetCurrentAmplitude(), visualAmplitude); ImGui::Spacing();
            bool currentAudioLinkState = m_audioSystem.IsAudioLinkEnabled();
            if (ImGui::Checkbox("Link Audio to Shader (iAudioAmp)", &currentAudioLinkState)) {
                m_audioSystem.SetAudioLinkEnabled(currentAudioLinkState);
            }
            if (m_shaderManager.getAudioAmpLocation() == -1 && currentAudioLinkState) { ImGui::SameLine(); ImGui::TextDisabled("(iAudioAmp not in shader!)"); }
        } else { ImGui::TextDisabled("Microphone not active or failed."); }
    } else if (m_audioSystem.GetCurrentAudioSourceIndex() == 1) { 
        ImGui::Text("System Audio Settings:"); ImGui::TextDisabled(" (System audio/loopback NYI)"); ImGui::Spacing();
        bool currentAudioLinkState = m_audioSystem.IsAudioLinkEnabled();
        if (ImGui::Checkbox("Link Audio to Shader (iAudioAmp)##System", &currentAudioLinkState)) { m_audioSystem.SetAudioLinkEnabled(currentAudioLinkState); }
        if (m_shaderManager.getAudioAmpLocation() == -1 && currentAudioLinkState) { ImGui::SameLine(); ImGui::TextDisabled("(iAudioAmp not in shader!)"); }
    } else if (m_audioSystem.GetCurrentAudioSourceIndex() == 2) { 
        ImGui::Text("Audio File Settings:");
        ImGui::InputText("File Path", m_audioSystem.GetAudioFilePathBuffer(), AudioSystem::AUDIO_FILE_PATH_BUFFER_SIZE);
        if (ImGui::Button("Load & Prepare Audio File")) {
            m_audioSystem.LoadWavFile(m_audioSystem.GetAudioFilePathBuffer()); 
        }
        if(m_audioSystem.IsAudioFileLoaded()){
            ImGui::Text("Loaded: %s", m_audioSystem.GetAudioFilePathBuffer());
            ImGui::Text("Live Audio File Amplitude:");
            float visualAmplitude = m_audioSystem.GetCurrentAmplitude() * 4.0f; 
            visualAmplitude = std::max(0.0f, std::min(visualAmplitude, 1.0f)); 
            ImGui::ProgressBar(visualAmplitude, ImVec2(-1.0f, 0.0f));
            ImGui::Text("Raw Avg. Abs: %.4f, Visual: %.2f", m_audioSystem.GetCurrentAmplitude(), visualAmplitude); ImGui::Spacing();
        } else { ImGui::TextDisabled("No audio file loaded or failed."); }
        bool currentAudioLinkState = m_audioSystem.IsAudioLinkEnabled();
        if (ImGui::Checkbox("Link Audio to Shader (iAudioAmp)##File", &currentAudioLinkState)) { m_audioSystem.SetAudioLinkEnabled(currentAudioLinkState); }
        if (m_shaderManager.getAudioAmpLocation() == -1 && currentAudioLinkState) { ImGui::SameLine(); ImGui::TextDisabled("(iAudioAmp not in shader!)"); }
    }
    ImGui::Separator(); ImGui::TextDisabled("\n (Advanced mapping UI later)");
    ImGui::End();
}

std::string UIManager::TrimString(const std::string& str) {
    // This method is now unused as Utils::Trim should be used.
    // It can be removed from UIManager.h and UIManager.cpp
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (std::string::npos == start) {
        return "";
    }
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}