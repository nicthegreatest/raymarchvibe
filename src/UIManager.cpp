#include "UIManager.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ShadertoyIntegration.h"
#include "Utils.h"
#include <fstream>
#include <sstream>
#include <algorithm> // For std::transform
#include <cctype>    // For std::tolower

#include <GLFW/glfw3.h>

UIManager::UIManager(
    ShaderManager& shaderManager, ShaderParser& shaderParser, AudioSystem& audioSystem, TextEditor& editor,
    bool& shadertoyMode, bool& showGui,
    bool& showHelpWindow, bool& showAudioReactivityWindow, bool& g_ShowConfirmExitPopup,
    bool& showStatusWindow, bool& showEditorWindow, bool& showConsoleWindow,
    std::string& currentShaderCode_editor, std::string& shaderLoadError_ui,
    std::string& currentShaderPath_ui, std::string& shaderCompileErrorLog_ui,
    float* objectColor_param, float& scale_param, float& timeSpeed_param,
    float* colorMod_param, float& patternScale_param,
    float* cameraPosition_param, float* cameraTarget_param, float& cameraFOV_param,
    float* lightPosition_param, float* lightColor_param,
    float& iUserFloat1_param, float* iUserColor1_param,
    char* filePathBuffer_Load, char* filePathBuffer_SaveAs,
    char* shadertoyInputBuffer, const std::string& shadertoyApiKey,
    const std::vector<ShaderSample_UI>& shaderSamples,
    size_t& currentSampleIndex
) : m_shaderManager_ref(shaderManager), m_shaderParser_ref(shaderParser), m_audioSystem_ref(audioSystem), m_editor_ref(editor),
    m_shadertoyMode_ref(shadertoyMode), m_showGui_ref(showGui),
    m_showHelpWindow_ref(showHelpWindow), m_showAudioReactivityWindow_ref(showAudioReactivityWindow), m_g_ShowConfirmExitPopup_ref(g_ShowConfirmExitPopup),
    m_showStatusWindow_ref(showStatusWindow), m_showEditorWindow_ref(showEditorWindow), m_showConsoleWindow_ref(showConsoleWindow),
    m_currentShaderCode_editor_ref(currentShaderCode_editor), m_shaderLoadError_ui_ref(shaderLoadError_ui),
    m_currentShaderPath_ui_ref(currentShaderPath_ui), m_shaderCompileErrorLog_ui_ref(shaderCompileErrorLog_ui),
    p_objectColor_param(objectColor_param), m_scale_param_ref(scale_param), m_timeSpeed_param_ref(timeSpeed_param),
    p_colorMod_param(colorMod_param), m_patternScale_param_ref(patternScale_param),
    p_cameraPosition_param(cameraPosition_param), p_cameraTarget_param(cameraTarget_param), m_cameraFOV_param_ref(cameraFOV_param),
    p_lightPosition_param(lightPosition_param), p_lightColor_param(lightColor_param),
    m_iUserFloat1_param_ref(iUserFloat1_param), p_iUserColor1_param(iUserColor1_param),
    p_filePathBuffer_Load(filePathBuffer_Load), p_filePathBuffer_SaveAs(filePathBuffer_SaveAs),
    p_shadertoyInputBuffer(shadertoyInputBuffer), m_shadertoyApiKey_ref(shadertoyApiKey),
    m_shaderSamples_ref(shaderSamples), m_currentSampleIndex_ref(currentSampleIndex)
{
    // Initialize find functionality state
    m_findText[0] = '\0';
    m_findStartCoord = {0, 0};
    m_lastMatchStartCoord = {0,0};
    m_lastMatchEndCoord = {0,0};
    m_foundText = false;
    m_findCaseSensitive = false;
    m_currentFindIndex = -1; // Or 0, depending on how "Find Next" logic is implemented
}


void UIManager::Initialize() {
    auto lang = TextEditor::LanguageDefinition::GLSL();
    m_editor_ref.SetLanguageDefinition(lang);
    m_editor_ref.SetShowWhitespaces(false);
    m_themes.applyTheme("Bess Dark"); // Apply a default theme
}

void UIManager::SetupInitialEditorAndShader() {
    HandleShaderLoad(m_currentShaderPath_ui_ref);
}

void UIManager::RenderAllUI(GLFWwindow* window, bool isFullscreen, int storedWindowX, int storedWindowY, int storedWindowWidth, int storedWindowHeight, float deltaTime, int frameCount, const float* mouseState) {
    (void)isFullscreen; (void)storedWindowX; (void)storedWindowY; (void)storedWindowWidth; (void)storedWindowHeight;

    RenderMenuBar(window);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (m_snapWindows) {
        ImGui::DockBuilderRemoveNode(dockspace_id); 
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.35f, nullptr, &dock_main_id);
        ImGuiID dock_right_bottom_id = ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Down, 0.40f, nullptr, &dock_right_id);

        ImGui::DockBuilderDockWindow("Shader Editor", dock_main_id);
        ImGui::DockBuilderDockWindow("Status & Parameters", dock_right_id);
        ImGui::DockBuilderDockWindow("Console", dock_right_bottom_id);
        
        ImGui::DockBuilderFinish(dockspace_id);
        m_snapWindows = false; 
    }

    ImGui::End();

    if (m_showStatusWindow_ref) RenderStatusWindow(deltaTime, frameCount);
    if (m_showEditorWindow_ref) RenderShaderEditorWindow(mouseState);
    if (m_showConsoleWindow_ref) RenderConsoleWindow();
    if (m_showAudioReactivityWindow_ref) RenderAudioReactivityWindow();
    if (m_showHelpWindow_ref) RenderHelpWindow();
    if (m_g_ShowConfirmExitPopup_ref) RenderConfirmExitPopup(window);
    if (m_showAboutPopup) RenderAboutPopup();
}

void UIManager::RenderMenuBar(GLFWwindow* window) {
    (void)window;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("New Shader")) {
                if (ImGui::MenuItem("New Native Shader")) { HandleNewShader(false); }
                if (ImGui::MenuItem("New Shadertoy Shader")) { HandleNewShader(true); }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Load Shader...")) { HandleShaderLoad(p_filePathBuffer_Load); }
            if (ImGui::BeginMenu("Load Sample Shader...")) {
                for (size_t i = 1; i < m_shaderSamples_ref.size(); ++i) {
                    if (ImGui::MenuItem(m_shaderSamples_ref[i].name.c_str())) {
                        m_currentSampleIndex_ref = i;
                        HandleShaderLoad(m_shaderSamples_ref[i].path);
                        strcpy(p_filePathBuffer_Load, m_shaderSamples_ref[i].path.c_str());
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Load from Shadertoy...")) { HandleLoadFromShadertoy(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) { HandleShaderSave(m_currentShaderPath_ui_ref); }
            if (ImGui::MenuItem("Save As...")) { HandleShaderSave(p_filePathBuffer_SaveAs); }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) { m_g_ShowConfirmExitPopup_ref = true; }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Apply Shader Code", "F5")) { ApplyShaderFromEditorAndHandleResults(); }
            if (ImGui::MenuItem("Reset Parameters")) { ResetAllParameters(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy Console Log")) { ImGui::SetClipboardText(m_shaderCompileErrorLog_ui_ref.c_str()); }
            if (ImGui::MenuItem("Clear Console Log & Status")) {
                m_shaderCompileErrorLog_ui_ref.clear();
                m_shaderLoadError_ui_ref.clear();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Toggle Status Window", nullptr, &m_showStatusWindow_ref);
            ImGui::MenuItem("Toggle Shader Editor", nullptr, &m_showEditorWindow_ref);
            ImGui::MenuItem("Toggle Console", nullptr, &m_showConsoleWindow_ref);
            ImGui::Separator();
            ImGui::MenuItem("Toggle Audio Reactivity", nullptr, &m_showAudioReactivityWindow_ref);
            ImGui::Separator();
            if (ImGui::MenuItem("Snap Windows to Default Layout")) { RequestWindowSnap(); }
            if (ImGui::MenuItem("Toggle Fullscreen", "F12")) { requestFullscreenToggle(); }
            if (ImGui::MenuItem("Toggle GUI", "Spacebar")) { m_showGui_ref = !m_showGui_ref; }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) { // New Settings Menu
            if (ImGui::BeginMenu("Themes")) {
                const auto& availableThemes = m_themes.getThemes();
                // TODO: Get current theme to show a checkmark next to it. For now, just list them.
                for (const auto& pair : availableThemes) {
                    if (ImGui::MenuItem(pair.first.c_str())) {
                        m_themes.applyTheme(pair.first);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Mode")) {
            if (ImGui::MenuItem("Shadertoy Mode", nullptr, &m_shadertoyMode_ref)) {
                ApplyShaderFromEditorAndHandleResults();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("View Help", nullptr, &m_showHelpWindow_ref);
            ImGui::Separator();
            ImGui::MenuItem("About RaymarchVibe...", nullptr, &m_showAboutPopup);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void UIManager::RenderStatusWindow(float deltaTime, int frameCount) {
    ImGui::Begin("Status & Parameters", &m_showStatusWindow_ref);

    if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
        ImGui::Text("Frame: %d", frameCount);
        ImGui::Text("Current Shader: %s", m_currentShaderPath_ui_ref.c_str());
        if (!m_shaderLoadError_ui_ref.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Load/System Error:");
            ImGui::TextWrapped("%s", m_shaderLoadError_ui_ref.c_str());
        }
    }
    
    if (ImGui::CollapsingHeader("File Operations")) {
        ImGui::InputText("##loadpath", p_filePathBuffer_Load, 256);
        ImGui::SameLine();
        if (ImGui::Button("Load")) { HandleShaderLoad(p_filePathBuffer_Load); }

        ImGui::InputText("##savepath", p_filePathBuffer_SaveAs, 256);
        ImGui::SameLine();
        if (ImGui::Button("Save As")) { HandleShaderSave(p_filePathBuffer_SaveAs); }
    }

    if (ImGui::CollapsingHeader("Shadertoy Integration")) {
        ImGui::InputText("ID/URL", p_shadertoyInputBuffer, 256);
        ImGui::SameLine();
        if (ImGui::Button("Fetch")) { HandleLoadFromShadertoy(); }
    }

    if (ImGui::CollapsingHeader("Shader Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_shadertoyMode_ref) {
            ImGui::DragFloat("iUserFloat1", &m_iUserFloat1_param_ref, 0.01f);
            ImGui::ColorEdit3("iUserColor1", p_iUserColor1_param);

            for(auto& control : m_shaderParser_ref.GetUniformControls()){
                 if(control.glslType == "float") ImGui::DragFloat(control.name.c_str(), &control.fValue, 0.01f);
                 else if(control.glslType == "vec2") ImGui::DragFloat2(control.name.c_str(), control.v2Value, 0.01f);
                 else if(control.glslType == "vec3") ImGui::DragFloat3(control.name.c_str(), control.v3Value, 0.01f);
                 else if(control.glslType == "vec4") ImGui::DragFloat4(control.name.c_str(), control.v4Value, 0.01f);
                 else if(control.glslType == "int") ImGui::DragInt(control.name.c_str(), &control.iValue);
                 else if(control.glslType == "bool") ImGui::Checkbox(control.name.c_str(), &control.bValue);
            }
        } else {
            ImGui::ColorEdit3("Object Color", p_objectColor_param);
            ImGui::DragFloat("Scale", &m_scale_param_ref, 0.01f);
            ImGui::DragFloat("Time Speed", &m_timeSpeed_param_ref, 0.01f);
            ImGui::ColorEdit3("Color Mod", p_colorMod_param);
            ImGui::DragFloat("Pattern Scale", &m_patternScale_param_ref, 0.05f);
            ImGui::DragFloat3("Camera Position", p_cameraPosition_param, 0.1f);
            ImGui::DragFloat3("Camera Target", p_cameraTarget_param, 0.1f);
            ImGui::DragFloat("Camera FOV", &m_cameraFOV_param_ref, 1.0f, 1.0f, 179.0f);
            ImGui::DragFloat3("Light Position", p_lightPosition_param, 0.1f);
            ImGui::ColorEdit3("Light Color", p_lightColor_param);
        }

        // FIX: The member in your DefineControl struct is 'value' not 'currentValue'
        for(auto& control : m_shaderParser_ref.GetDefineControls()){
            ImGui::DragFloat(control.name.c_str(), &control.value, 0.01f);
        }

        for(auto& control : m_shaderParser_ref.GetConstControls()){
             bool changed = false;
             if(control.glslType == "float") changed = ImGui::DragFloat(control.name.c_str(), &control.fValue, 0.01f);
             else if(control.glslType == "int") changed = ImGui::DragInt(control.name.c_str(), &control.iValue);
             else if(control.glslType == "vec2") changed = ImGui::DragFloat2(control.name.c_str(), control.v2Value, 0.01f);
             else if(control.glslType == "vec3") changed = ImGui::DragFloat3(control.name.c_str(), control.v3Value, 0.01f);
             else if(control.glslType == "vec4") changed = ImGui::DragFloat4(control.name.c_str(), control.v4Value, 0.01f);
             
             if (changed) {
                 m_currentShaderCode_editor_ref = m_shaderParser_ref.UpdateConstValueInString(m_currentShaderCode_editor_ref, control);
                 m_editor_ref.SetText(m_currentShaderCode_editor_ref);
                 ApplyShaderFromEditorAndHandleResults();
             }
        }
    }

    ImGui::End();
}


void UIManager::RenderShaderEditorWindow(const float* mouseState) {
    ImGui::Begin("Shader Editor", &m_showEditorWindow_ref, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button("Apply (F5)")) { ApplyShaderFromEditorAndHandleResults(); }
        if (ImGui::Button("Reset Params")) { ResetAllParameters(); }

        ImGui::Separator(); // Separator for Find controls

        // Find UI
        ImGui::PushItemWidth(150); // Adjust width as needed
        bool findTextChanged = ImGui::InputText("##Find", m_findText, sizeof(m_findText), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Next") || findTextChanged) { // findTextChanged allows Enter to trigger find
            // When text changes, we should reset the search start position or assume it's a new search
            if (findTextChanged) {
                m_foundText = false; // Treat as a new search if text changed
                m_findStartCoord = m_editor_ref.GetCursorPosition(); // Start from current cursor
                m_lastMatchEndCoord = {-1,-1}; // Reset last match
            }
            HandleFindNext();
        }
        ImGui::SameLine();
        if (ImGui::Button("Prev")) {
             if (findTextChanged) { // Also consider enter for previous, though less common
                m_foundText = false;
                m_findStartCoord = m_editor_ref.GetCursorPosition();
                m_lastMatchStartCoord = {-1,-1}; // Reset last match for prev
            }
            HandleFindPrevious();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Case Sens.", &m_findCaseSensitive);

        ImGui::Separator(); // Separator after Find controls

        ImGui::Text("Mouse: (%.1f, %.1f)", mouseState[0], mouseState[1]);
        ImGui::Text("Click: (%.1f, %.1f)", mouseState[2], mouseState[3]);
        ImGui::EndMenuBar();
    }
    
    // Display find status (optional)
    // if (strlen(m_findText) > 0 && !m_foundText && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    //     ImGui::TextColored(ImVec4(1,0,0,1), "Text not found");
    // }


    auto cpos = m_editor_ref.GetCursorPosition();
    ImGui::Text("%6d/%-6d %6d lines | %s | %s | %s", cpos.mLine + 1, cpos.mColumn + 1, m_editor_ref.GetTotalLines(),
        m_editor_ref.IsOverwrite() ? "Ovr" : "Ins",
        m_editor_ref.CanUndo() ? "*" : " ",
        m_editor_ref.GetLanguageDefinition().mName.c_str());

    m_editor_ref.Render("TextEditor");

    ImGui::End();
}

// Helper function to convert string offset to TextEditor::Coordinates
TextEditor::Coordinates UIManager::GetCoordinatesForOffset(const std::string& text, int offset) {
    int line = 0, col = 0;
    for (int i = 0; i < offset && i < (int)text.length(); ++i) {
        if (text[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    return {line, col};
}

// Helper function to convert TextEditor::Coordinates to string offset
int UIManager::GetOffsetForCoordinates(const std::string& text, const TextEditor::Coordinates& coords) {
    int offset = 0;
    int currentLine = 0;
    int currentCol = 0; // Keep track of column within the current line processed so far

    for (char c : text) {
        if (currentLine == coords.mLine && currentCol == coords.mColumn) {
            return offset;
        }
        if (c == '\n') {
            currentLine++;
            currentCol = 0; // Reset column for the new line
            if (currentLine > coords.mLine) break; // Optimization: if we passed the target line
        } else {
            currentCol++;
        }
        offset++;
    }
    // If coords are beyond the text (e.g., end of file cursor might be one past last char on a line)
    // or if the exact coordinate wasn't matched (e.g. column beyond line length),
    // this will return the offset to the end of text or the closest valid position processed.
    // For precise behavior, ensure coords are valid within the text.
    if (currentLine == coords.mLine && currentCol == coords.mColumn) {
         return offset;
    }
    // Fallback: if coords are at the very end of the text
    if (coords.mLine == currentLine && coords.mColumn == currentCol) return (int)text.length();

    // If coordinates are invalid (e.g., line number too high, or column too high for a line)
    // return -1 or handle as an error. For simplicity, returning text.length() or a clamped value.
    // The TextEditor itself usually handles out-of-bounds coordinates gracefully.
    return (int)text.length(); // Or a more sophisticated clamping/error
}


void UIManager::HandleFindNext() {
    if (strlen(m_findText) == 0) {
        m_foundText = false;
        return;
    }

    std::string editorText = m_editor_ref.GetText();
    std::string searchText = m_findText;

    // Determine start offset for search
    // If a previous match was found, start searching after it. Otherwise, start from m_findStartCoord.
    int startOffset = 0;
    if (m_foundText && m_lastMatchEndCoord.mLine >=0 && m_lastMatchEndCoord.mColumn >=0) { // m_foundText indicates a previous successful search
        startOffset = GetOffsetForCoordinates(editorText, m_lastMatchEndCoord);
    } else {
         // If no previous match, or starting a new search, use m_findStartCoord (typically cursor pos or start of doc)
         // For simplicity, let's reset to current cursor if not actively in a "find next" sequence from a specific match
         // This behavior might need refinement based on desired UX.
         // For now, if not m_foundText, we start from where m_findStartCoord was last set (e.g. by user interaction or default)
         // Or more simply, from current cursor position for a new "Find Next" click if nothing was selected.
        if (!m_editor_ref.HasSelection()) { // If nothing selected, start from cursor
            m_findStartCoord = m_editor_ref.GetCursorPosition();
        } else { // If something IS selected, and it's not our previous find, start search from end of current selection
            auto selectionStart = m_editor_ref.GetSelectionStart();
            auto selectionEnd = m_editor_ref.GetSelectionEnd();
            if (!(selectionStart == m_lastMatchStartCoord && selectionEnd == m_lastMatchEndCoord)) {
                 m_findStartCoord = selectionEnd;
            }
            // else, it IS our previous find, so startOffset is already correctly set to search after m_lastMatchEndCoord
        }
         startOffset = GetOffsetForCoordinates(editorText, m_findStartCoord);
    }

    // Ensure startOffset is within bounds
    if (startOffset >= (int)editorText.length()) {
        startOffset = 0; // Wrap around to the beginning
    }


    std::string textToSearch = editorText;
    if (!m_findCaseSensitive) {
        std::transform(textToSearch.begin(), textToSearch.end(), textToSearch.begin(), ::tolower);
        std::transform(searchText.begin(), searchText.end(), searchText.begin(), ::tolower);
    }

    size_t foundPos = textToSearch.find(searchText, startOffset);

    if (foundPos != std::string::npos) {
        m_foundText = true;
        m_lastMatchStartCoord = GetCoordinatesForOffset(editorText, (int)foundPos);
        m_lastMatchEndCoord = GetCoordinatesForOffset(editorText, (int)foundPos + (int)searchText.length());

        m_editor_ref.SetSelection(m_lastMatchStartCoord, m_lastMatchEndCoord);
        m_editor_ref.SetCursorPosition(m_lastMatchStartCoord);
        m_currentFindIndex = (int)foundPos; // Store flat index for now
    } else {
        // If not found from startOffset, try searching from the beginning of the document (wrap around)
        foundPos = textToSearch.find(searchText, 0);
        if (foundPos != std::string::npos) {
            m_foundText = true;
            m_lastMatchStartCoord = GetCoordinatesForOffset(editorText, (int)foundPos);
            m_lastMatchEndCoord = GetCoordinatesForOffset(editorText, (int)foundPos + (int)searchText.length());

            m_editor_ref.SetSelection(m_lastMatchStartCoord, m_lastMatchEndCoord);
            m_editor_ref.SetCursorPosition(m_lastMatchStartCoord);
            m_currentFindIndex = (int)foundPos;
        } else {
            m_foundText = false;
            m_currentFindIndex = -1;
            // Optionally, provide feedback to the user that the text was not found (e.g., status bar message)
        }
    }
}

void UIManager::HandleFindPrevious() {
    // Basic implementation sketch - proper previous search needs careful offset management
    if (strlen(m_findText) == 0) {
        m_foundText = false;
        return;
    }

    std::string editorText = m_editor_ref.GetText();
    std::string searchText = m_findText;

    int startOffset = 0;
    // If a previous match was found, start searching before it.
    if (m_foundText && m_lastMatchStartCoord.mLine >=0 && m_lastMatchStartCoord.mColumn >=0) {
        startOffset = GetOffsetForCoordinates(editorText, m_lastMatchStartCoord);
        if (startOffset > 0) startOffset--; // Start search just before the current match
        else startOffset = (int)editorText.length() -1; // wrap to end if at beginning
    } else {
        // If no previous match, start from current cursor or end of document
        m_findStartCoord = m_editor_ref.GetCursorPosition();
        startOffset = GetOffsetForCoordinates(editorText, m_findStartCoord);
         if (startOffset > 0) startOffset--;
         else startOffset = (int)editorText.length() -1;
    }
    if (startOffset < 0) startOffset = (int)editorText.length() -1; // Ensure it's not negative

    std::string textToSearch = editorText;
    if (!m_findCaseSensitive) {
        std::transform(textToSearch.begin(), textToSearch.end(), textToSearch.begin(), ::tolower);
        std::transform(searchText.begin(), searchText.end(), searchText.begin(), ::tolower);
    }

    size_t foundPos = std::string::npos;
    // Iterate backwards using rfind
    if (startOffset >= (int)searchText.length() -1 ) { // ensure startOffset allows for full searchText
         foundPos = textToSearch.rfind(searchText, startOffset);
    } else if (!searchText.empty()){ // if search text is not empty but startOffset is too small, search the whole doc from end
         foundPos = textToSearch.rfind(searchText, std::string::npos);
    }


    if (foundPos != std::string::npos) {
        m_foundText = true;
        m_lastMatchStartCoord = GetCoordinatesForOffset(editorText, (int)foundPos);
        m_lastMatchEndCoord = GetCoordinatesForOffset(editorText, (int)foundPos + (int)searchText.length());

        m_editor_ref.SetSelection(m_lastMatchStartCoord, m_lastMatchEndCoord);
        m_editor_ref.SetCursorPosition(m_lastMatchStartCoord);
        m_currentFindIndex = (int)foundPos;
    } else {
         // Wrap around to search from the end if not found
        foundPos = textToSearch.rfind(searchText, std::string::npos); // Search from the very end
        if (foundPos != std::string::npos) {
            m_foundText = true;
            m_lastMatchStartCoord = GetCoordinatesForOffset(editorText, (int)foundPos);
            m_lastMatchEndCoord = GetCoordinatesForOffset(editorText, (int)foundPos + (int)searchText.length());

            m_editor_ref.SetSelection(m_lastMatchStartCoord, m_lastMatchEndCoord);
            m_editor_ref.SetCursorPosition(m_lastMatchStartCoord);
            m_currentFindIndex = (int)foundPos;
        } else {
            m_foundText = false;
            m_currentFindIndex = -1;
        }
    }
}


void UIManager::RenderConsoleWindow() {
    ImGui::Begin("Console", &m_showConsoleWindow_ref);
    if (ImGui::Button("Clear")) { m_shaderCompileErrorLog_ui_ref.clear(); }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) { ImGui::SetClipboardText(m_shaderCompileErrorLog_ui_ref.c_str()); }
    ImGui::Separator();
    ImGui::TextWrapped("%s", m_shaderCompileErrorLog_ui_ref.c_str());
    ImGui::End();
}


void UIManager::RenderAudioReactivityWindow() {
    ImGui::Begin("Audio Reactivity", &m_showAudioReactivityWindow_ref);

    bool audioLinkEnabled = m_audioSystem_ref.IsAudioLinkEnabled();
    if (ImGui::Checkbox("Enable Audio Link (iAudioAmp)", &audioLinkEnabled)) {
        m_audioSystem_ref.SetAudioLinkEnabled(audioLinkEnabled);
    }
    ImGui::Separator();

    int currentSourceIndex = m_audioSystem_ref.GetCurrentAudioSourceIndex();
    if (ImGui::RadioButton("Microphone", &currentSourceIndex, 0)) {
        m_audioSystem_ref.SetCurrentAudioSourceIndex(0);
        m_audioSystem_ref.InitializeAndStartSelectedCaptureDevice();
    }
    ImGui::RadioButton("System Audio (Loopback) - NYI", &currentSourceIndex, 1);
    if (ImGui::RadioButton("Audio File", &currentSourceIndex, 2)) {
         m_audioSystem_ref.SetCurrentAudioSourceIndex(2);
         if(m_audioSystem_ref.IsCaptureDeviceInitialized()) m_audioSystem_ref.StopActiveDevice();
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("Microphone Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        // FIX: Reverted to the correct function names from your original, working code.
        int selectedDevice = m_audioSystem_ref.GetSelectedCaptureDeviceIndex();
        const auto& devices = m_audioSystem_ref.GetCaptureDevices();
        if (ImGui::BeginCombo("Input Device", devices.empty() ? "None" : devices[selectedDevice].name.c_str())) {
            for (size_t i = 0; i < devices.size(); ++i) {
                const bool is_selected = (selectedDevice == (int)i);
                if (ImGui::Selectable(devices[i].name.c_str(), is_selected)) {
                    if (selectedDevice != (int)i) {
                        m_audioSystem_ref.SetSelectedCaptureDeviceIndex(i);
                        m_audioSystem_ref.InitializeAndStartSelectedCaptureDevice();
                    }
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Live Amplitude:");
        ImGui::ProgressBar(m_audioSystem_ref.GetCurrentAmplitude(), ImVec2(-1.0f, 0.0f));
    }

    if (ImGui::CollapsingHeader("Audio File Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        char* audioFilePath = m_audioSystem_ref.GetAudioFilePathBuffer();
        ImGui::InputText("File Path", audioFilePath, 256);
        ImGui::SameLine();
        if(ImGui::Button("Load##AudioFile")){
             m_audioSystem_ref.LoadWavFile(audioFilePath);
        }
        ImGui::Text("Status: %s", m_audioSystem_ref.IsAudioFileLoaded() ? "Loaded" : "Not Loaded");
    }

    ImGui::End();
}


void UIManager::RenderHelpWindow() {
    ImGui::Begin("Help", &m_showHelpWindow_ref);
    ImGui::Text("RaymarchVibe - Help");
    ImGui::Separator();
    ImGui::TextWrapped("Welcome to RaymarchVibe, a real-time tool for exploring and creating fragment shaders.");
    ImGui::BulletText("Toggle GUI: Press Spacebar");
    ImGui::BulletText("Toggle Fullscreen: Press F12");
    ImGui::BulletText("Apply Shader Changes: Press F5");
    ImGui::BulletText("Exit: Press ESC");
    ImGui::Separator();
    ImGui::Text("Features:");
    ImGui::BulletText("Live shader editing with syntax highlighting.");
    ImGui::BulletText("Dynamic UI controls parsed from shader code.");
    ImGui::BulletText("Shadertoy compatibility mode.");
    ImGui::BulletText("Audio reactivity via microphone or audio file.");
    ImGui::End();
}


void UIManager::RenderConfirmExitPopup(GLFWwindow* window) {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Confirm Exit", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to exit?\nHave you saved your work?\n\n");
        ImGui::Separator();
        if (ImGui::Button("Yes, Exit", ImVec2(120, 0))) {
            glfwSetWindowShouldClose(window, true);
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("No, Cancel", ImVec2(120, 0))) {
            m_g_ShowConfirmExitPopup_ref = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (m_g_ShowConfirmExitPopup_ref) {
        ImGui::OpenPopup("Confirm Exit");
    }
}

void UIManager::RenderAboutPopup() {
     ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("About RaymarchVibe", &m_showAboutPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("RaymarchVibe v1.0");
        ImGui::Text("A real-time shader exploration tool.");
        ImGui::Separator();
        ImGui::Text("Created by: nicthegreatest & Gemini");
        ImGui::Text("Powered by: ImGui, GLFW, GLAD, Miniaudio");
        ImGui::Separator();
        if(ImGui::Button("Close")){
            m_showAboutPopup = false;
        }
        ImGui::End();
    }
}

void UIManager::ApplyShaderFromEditorAndHandleResults() {
    m_currentShaderCode_editor_ref = m_editor_ref.GetText();
    m_shaderLoadError_ui_ref.clear();
    m_shaderCompileErrorLog_ui_ref.clear();
    
    // Your ShaderManager returns a string. A non-empty string indicates an error.
    std::string result = m_shaderManager_ref.applyShaderCode(m_currentShaderCode_editor_ref, m_shadertoyMode_ref);
    if (!result.empty()) {
        m_shaderCompileErrorLog_ui_ref = result;
        m_editor_ref.SetErrorMarkers(m_shaderParser_ref.GetErrorMarkers());
    } else {
        m_shaderCompileErrorLog_ui_ref.clear();
        m_editor_ref.SetErrorMarkers({});
    }
}

void UIManager::HandleShaderLoad(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        m_shaderLoadError_ui_ref = "Error: Could not open file: " + path;
        return;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    m_currentShaderCode_editor_ref = buffer.str();
    m_editor_ref.SetText(m_currentShaderCode_editor_ref);
    m_currentShaderPath_ui_ref = path;
    strcpy(p_filePathBuffer_Load, path.c_str());
    ApplyShaderFromEditorAndHandleResults();
}

void UIManager::HandleShaderSave(const std::string& path) {
    if (path.empty()) {
        m_shaderLoadError_ui_ref = "Error: Save path cannot be empty.";
        return;
    }
    std::ofstream file(path);
    if (!file.is_open()) {
        m_shaderLoadError_ui_ref = "Error: Could not open file for writing: " + path;
        return;
    }
    file << m_editor_ref.GetText();
    m_currentShaderPath_ui_ref = path;
    m_shaderLoadError_ui_ref = "Success: Saved to " + path;
}


void UIManager::HandleNewShader(bool isShadertoy) {
    m_shadertoyMode_ref = isShadertoy;
    std::string templatePath = isShadertoy ? "shaders/templates/shadertoy_template.frag" : "shaders/templates/native_template.frag";
    HandleShaderLoad(templatePath);
    m_currentShaderPath_ui_ref = "untitled.frag";
}

void UIManager::HandleLoadFromShadertoy() {
    m_shaderLoadError_ui_ref.clear();
    std::string shaderId = ShadertoyIntegration::ExtractId(p_shadertoyInputBuffer);
    if (shaderId.empty()) {
        m_shaderLoadError_ui_ref = "Error: Could not extract a valid Shadertoy ID.";
        return;
    }
    
    std::string errorMsg;
    std::string code = ShadertoyIntegration::FetchCode(shaderId, m_shadertoyApiKey_ref, errorMsg);
    
    if(!errorMsg.empty()){
        m_shaderLoadError_ui_ref = "Shadertoy API Error: " + errorMsg;
    } else {
        m_editor_ref.SetText(code);
        m_shadertoyMode_ref = true;
        m_currentShaderPath_ui_ref = "shadertoy://" + shaderId;
        ApplyShaderFromEditorAndHandleResults();
        m_shaderLoadError_ui_ref = "Success: Loaded shader " + shaderId + " from Shadertoy.";
    }
}

void UIManager::ResetAllParameters() {
    p_objectColor_param[0] = 0.8f; p_objectColor_param[1] = 0.9f; p_objectColor_param[2] = 1.0f;
    m_scale_param_ref = 1.0f;
    m_timeSpeed_param_ref = 1.0f;
    p_colorMod_param[0] = 0.1f; p_colorMod_param[1] = 0.1f; p_colorMod_param[2] = 0.2f;
    m_patternScale_param_ref = 1.0f;
    p_cameraPosition_param[0] = 0.0f; p_cameraPosition_param[1] = 1.0f; p_cameraPosition_param[2] = -3.0f;
    p_cameraTarget_param[0] = 0.0f; p_cameraTarget_param[1] = 0.0f; p_cameraTarget_param[2] = 0.0f;
    m_cameraFOV_param_ref = 60.0f;
    p_lightPosition_param[0] = 2.0f; p_lightPosition_param[1] = 3.0f; p_lightPosition_param[2] = -2.0f;
    p_lightColor_param[0] = 1.0f; p_lightColor_param[1] = 1.0f; p_lightColor_param[2] = 0.9f;
    
    m_iUserFloat1_param_ref = 0.5f;
    p_iUserColor1_param[0] = 0.2f; p_iUserColor1_param[1] = 0.5f; p_iUserColor1_param[2] = 0.8f;
}

void UIManager::RequestWindowSnap() {
    m_snapWindows = true;
}
