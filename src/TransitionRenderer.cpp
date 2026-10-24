#include "TransitionRenderer.h"

#include "Effect.h"
#include "Renderer.h"

#include "imgui.h"

#include <glad/glad.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

std::string LoadTextFile(const std::string& path, std::string& error) {
    error.clear();
    std::ifstream file(path);
    if (!file.is_open()) {
        error = "Could not open " + path;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        error = "Could not read " + path;
        return "";
    }
    return buffer.str();
}

unsigned int CompileShader(unsigned int type, const std::string& source, std::string& error) {
    error.clear();
    const char* text = source.c_str();
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength > 0 ? static_cast<size_t>(logLength) + 1 : 256);
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        error = log.data();
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned int LinkProgram(unsigned int vertexShader, const std::string& fragmentSource, std::string& error) {
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (fragmentShader == 0) return 0;
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glDeleteShader(fragmentShader);
    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        int logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength > 0 ? static_cast<size_t>(logLength) + 1 : 256);
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        error = log.data();
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool DrawTypeCombo(const char* label, std::string& type, const std::vector<std::string>& types) {
    std::vector<std::string> shown = types;
    if (std::find(shown.begin(), shown.end(), type) == shown.end())
        shown.push_back(type);
    if (shown.empty()) shown.push_back("cut");

    int current = 0;
    for (int i = 0; i < static_cast<int>(shown.size()); ++i) {
        if (shown[static_cast<size_t>(i)] == type) current = i;
    }

    bool changed = false;
    const std::string preview = TransitionRenderer::LabelFor(shown[static_cast<size_t>(current)]);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (int i = 0; i < static_cast<int>(shown.size()); ++i) {
            const bool selected = i == current;
            const std::string itemLabel = TransitionRenderer::LabelFor(shown[static_cast<size_t>(i)]);
            if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                type = shown[static_cast<size_t>(i)];
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool DrawEasingCombo(const char* label, TransitionEasing& easing) {
    const char* preview = easing == TransitionEasing::Linear ? "Linear" : "Smoothstep";
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        const TransitionEasing options[] = {TransitionEasing::Linear, TransitionEasing::Smoothstep};
        const char* names[] = {"Linear", "Smoothstep"};
        for (int i = 0; i < 2; ++i) {
            const bool selected = easing == options[i];
            if (ImGui::Selectable(names[i], selected)) {
                easing = options[i];
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

void DrawSettingFields(const char* idSuffix, std::string& type, float& duration, TransitionEasing& easing, const std::vector<std::string>& types) {
    ImGui::PushID(idSuffix);
    DrawTypeCombo("Type", type, types);
    ImGui::SliderFloat("Duration", &duration, 0.1f, 5.0f, "%.2f s");
    duration = ClampTransitionDuration(duration);
    DrawEasingCombo("Easing", easing);
    ImGui::PopID();
}

} // namespace

std::string TransitionRenderer::LabelFor(const std::string& type) {
    if (type == "cut") return "Cut";
    if (type == "crossfade") return "Crossfade";
    if (type == "fade_through_black") return "Fade Through Black";
    if (type == "luma_dissolve") return "Luma Dissolve";
    if (type == "zoom_blur") return "Zoom Blur";
    std::string label = type;
    for (char& character : label) {
        if (character == '_') character = ' ';
    }
    return label;
}

bool TransitionRenderer::IsSafeTypeName(const std::string& type) {
    if (type.empty()) return false;
    for (unsigned char character : type) {
        if (!(std::isalnum(character) || character == '_' || character == '-')) return false;
    }
    return true;
}

void TransitionRenderer::Init(Renderer& renderer, int width, int height) {
    m_renderer = &renderer;
    m_width = width;
    m_height = height;
    std::string error;
    const std::string vertexSource = LoadTextFile("shaders/texture.vert", error);
    if (vertexSource.empty()) {
        std::cerr << "Transition: " << error << ". Blends will fall back to Cut." << std::endl;
        m_vertexFailed = true;
    } else {
        m_vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource, error);
        if (m_vertexShader == 0) {
            std::cerr << "Transition vertex shader: " << error << std::endl;
            m_vertexFailed = true;
        }
    }
    CreateTargets();
    ScanTypes();
}

void TransitionRenderer::Shutdown() {
    DestroyProgramCache();
    DestroyTargets();
    if (m_vertexShader != 0) {
        glDeleteShader(m_vertexShader);
        m_vertexShader = 0;
    }
    Cancel();
    m_renderer = nullptr;
}

void TransitionRenderer::Resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height && m_blend.fbo != 0) return;
    m_width = width;
    m_height = height;
    // The snapshot was the previous size. Drop the blend instead of showing a stretched frame.
    const bool wasActive = m_active;
    CreateTargets();
    if (wasActive) Cancel();
}

void TransitionRenderer::CreateTargets() {
    DestroyTargets();
    if (m_width <= 0 || m_height <= 0) return;

    const int width = m_width;
    const int height = m_height;
    auto makeBuffer = [width, height](TargetBuffer& buffer) {
        glGenFramebuffers(1, &buffer.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo);
        glGenTextures(1, &buffer.texture);
        glBindTexture(GL_TEXTURE_2D, buffer.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer.texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Transition: blend framebuffer is incomplete." << std::endl;
            glDeleteFramebuffers(1, &buffer.fbo);
            glDeleteTextures(1, &buffer.texture);
            buffer.fbo = 0;
            buffer.texture = 0;
        }
    };

    makeBuffer(m_snapshot);
    makeBuffer(m_blend);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_blendValid = false;
}

void TransitionRenderer::DestroyTargets() {
    if (m_snapshot.fbo != 0) glDeleteFramebuffers(1, &m_snapshot.fbo);
    if (m_snapshot.texture != 0) glDeleteTextures(1, &m_snapshot.texture);
    if (m_blend.fbo != 0) glDeleteFramebuffers(1, &m_blend.fbo);
    if (m_blend.texture != 0) glDeleteTextures(1, &m_blend.texture);
    m_snapshot = {};
    m_blend = {};
    m_blendValid = false;
}

void TransitionRenderer::DestroyProgramCache() {
    for (auto& entry : m_programs) {
        if (entry.second != 0) glDeleteProgram(entry.second);
    }
    m_programs.clear();
    m_failedTypes.clear();
}

void TransitionRenderer::ScanTypes() {
    m_types.clear();
    m_types.push_back("cut");
    m_failedTypes.clear();

    std::vector<std::string> found;
    try {
        const std::filesystem::path directory("shaders/transitions");
        if (std::filesystem::is_directory(directory)) {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".frag") continue;
                const std::string stem = entry.path().stem().string();
                if (stem == "cut" || !IsSafeTypeName(stem)) continue;
                found.push_back(stem);
            }
        } else if (!m_scanLogged) {
            std::cerr << "Transition: shaders/transitions is missing. Only Cut is available." << std::endl;
            m_scanLogged = true;
        }
    } catch (const std::filesystem::filesystem_error& error) {
        if (!m_scanLogged) {
            std::cerr << "Transition: " << error.what() << std::endl;
            m_scanLogged = true;
        }
    }

    const char* preferred[] = {"crossfade", "fade_through_black", "luma_dissolve", "zoom_blur"};
    for (const char* name : preferred) {
        auto it = std::find(found.begin(), found.end(), name);
        if (it != found.end()) {
            m_types.push_back(*it);
            found.erase(it);
        }
    }
    std::sort(found.begin(), found.end());
    m_types.insert(m_types.end(), found.begin(), found.end());
}

bool TransitionRenderer::EnsureProgram(const std::string& type) {
    if (type == "cut") return false;
    if (!IsSafeTypeName(type) || m_vertexFailed || m_vertexShader == 0) return false;
    if (m_failedTypes.count(type) != 0) return false;
    if (m_programs.count(type) != 0 && m_programs[type] != 0) return true;

    std::string error;
    const std::string path = "shaders/transitions/" + type + ".frag";
    const std::string source = LoadTextFile(path, error);
    if (source.empty()) {
        std::cerr << "Transition: " << error << std::endl;
        m_failedTypes.insert(type);
        return false;
    }
    const unsigned int program = LinkProgram(m_vertexShader, source, error);
    if (program == 0) {
        std::cerr << "Transition shader '" << type << "': " << error << std::endl;
        m_failedTypes.insert(type);
        return false;
    }
    m_programs[type] = program;
    return true;
}

void TransitionRenderer::CopyTextureToSnapshot(unsigned int texture) {
    if (texture == 0 || m_snapshot.fbo == 0 || m_renderer == nullptr) return;

    GLint viewport[4] = {0, 0, m_width, m_height};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const GLboolean depthWasOn = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasOn = glIsEnabled(GL_BLEND);
    const GLboolean scissorWasOn = glIsEnabled(GL_SCISSOR_TEST);
    if (depthWasOn) glDisable(GL_DEPTH_TEST);
    if (blendWasOn) glDisable(GL_BLEND);
    if (scissorWasOn) glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, m_snapshot.fbo);
    glViewport(0, 0, m_width, m_height);
    m_renderer->RenderFullscreenTexture(texture);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (depthWasOn) glEnable(GL_DEPTH_TEST);
    if (blendWasOn) glEnable(GL_BLEND);
    if (scissorWasOn) glEnable(GL_SCISSOR_TEST);
}

void TransitionRenderer::Cancel() {
    m_active = false;
    m_justStarted = false;
    m_useSnapshot = false;
    m_blendValid = false;
    m_from = nullptr;
    m_to = nullptr;
    m_progress = 0.0f;
    m_activeType = "cut";
}

void TransitionRenderer::OnPresentedChanged(Effect* outgoing, Effect* incoming, bool forceCut) {
    if (outgoing == incoming) return;
    if (forceCut || incoming == nullptr) {
        Cancel();
        return;
    }
    const TransitionSettings resolved = ResolveTransitionSettings(m_global, incoming->transitionIn);
    Begin(outgoing, incoming, resolved);
}

void TransitionRenderer::Begin(Effect* outgoing, Effect* incoming, const TransitionSettings& settings) {
    if (incoming == nullptr || incoming->GetOutputTexture() == 0 || settings.type == "cut") {
        Cancel();
        return;
    }
    if (!IsSafeTypeName(settings.type) || !EnsureProgram(settings.type)) {
        Cancel();
        return;
    }

    // A click mid-blend must start from the picture on screen, not from black and not from
    // the raw outgoing node (that picture is already mixed away).
    if (m_active) {
        unsigned int source = 0;
        if (m_blendValid) source = m_blend.texture;
        else if (m_useSnapshot) source = m_snapshot.texture;
        else if (m_from != nullptr) source = m_from->GetOutputTexture();
        if (source == 0) {
            Cancel();
            return;
        }
        CopyTextureToSnapshot(source);
        m_useSnapshot = true;
        m_from = nullptr;
    } else {
        if (outgoing == nullptr || outgoing->GetOutputTexture() == 0) {
            Cancel();
            return;
        }
        m_useSnapshot = false;
        m_from = outgoing;
    }

    m_to = incoming;
    m_activeType = settings.type;
    m_duration = ClampTransitionDuration(settings.duration);
    m_easing = settings.easing;
    m_progress = 0.0f;
    m_justStarted = true;
    m_active = true;
}

void TransitionRenderer::NotifyRemoved(Effect* effect) {
    if (!m_active || effect == nullptr) return;
    if (effect == m_to) {
        Cancel();
        return;
    }
    if (effect == m_from) {
        // The node's FBO is about to be deleted. Keep its last picture for the rest of the blend.
        const unsigned int texture = effect->GetOutputTexture();
        if (texture == 0) {
            Cancel();
            return;
        }
        CopyTextureToSnapshot(texture);
        m_useSnapshot = true;
        m_from = nullptr;
    }
}

float TransitionRenderer::EasedProgress() const {
    const float t = std::clamp(m_progress, 0.0f, 1.0f);
    if (m_easing == TransitionEasing::Linear) return t;
    return t * t * (3.0f - 2.0f * t);
}

unsigned int TransitionRenderer::Compose(float deltaTime) {
    if (!m_active) return 0;
    if (m_blend.fbo == 0 || m_programs.count(m_activeType) == 0) {
        Cancel();
        return 0;
    }

    // Hold the first frame on the outgoing picture so a click does not skip ahead,
    // and so a mid-blend click starts exactly on the frame that was just on screen.
    if (m_justStarted) {
        m_justStarted = false;
    } else if (m_duration > 0.0f) {
        m_progress += deltaTime / m_duration;
    }
    const bool finished = m_progress >= 1.0f;
    if (finished) m_progress = 1.0f;

    const unsigned int fromTexture = m_useSnapshot ? m_snapshot.texture : (m_from != nullptr ? m_from->GetOutputTexture() : 0);
    const unsigned int toTexture = m_to != nullptr ? m_to->GetOutputTexture() : 0;
    if (fromTexture == 0 || toTexture == 0) {
        Cancel();
        return 0;
    }

    GLint viewport[4] = {0, 0, m_width, m_height};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const GLboolean depthWasOn = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasOn = glIsEnabled(GL_BLEND);
    const GLboolean scissorWasOn = glIsEnabled(GL_SCISSOR_TEST);
    if (depthWasOn) glDisable(GL_DEPTH_TEST);
    if (blendWasOn) glDisable(GL_BLEND);
    if (scissorWasOn) glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, m_blend.fbo);
    glViewport(0, 0, m_width, m_height);

    const unsigned int program = m_programs[m_activeType];
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fromTexture);
    glUniform1i(glGetUniformLocation(program, "u_from"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, toTexture);
    glUniform1i(glGetUniformLocation(program, "u_to"), 1);
    glUniform1f(glGetUniformLocation(program, "u_progress"), EasedProgress());
    glUniform2f(glGetUniformLocation(program, "u_resolution"), static_cast<float>(m_width), static_cast<float>(m_height));
    Renderer::RenderQuad();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (depthWasOn) glEnable(GL_DEPTH_TEST);
    if (blendWasOn) glEnable(GL_BLEND);
    if (scissorWasOn) glEnable(GL_SCISSOR_TEST);

    m_blendValid = true;
    if (finished) {
        // This frame is exactly the incoming picture. The next frame shows that node's live texture.
        m_active = false;
        m_justStarted = false;
        m_useSnapshot = false;
        m_from = nullptr;
        m_to = nullptr;
    }
    return m_blend.texture;
}

void TransitionRenderer::RenderSettingsUI(Effect* selectedNode) {
    if (!ImGui::CollapsingHeader("Transition")) return;
    ScanTypes();

    ImGui::TextWrapped("Blends the previous node's picture into the one you select. Cut is instant. This does not use pins or links. Shift+click a node to cut.");
    DrawSettingFields("global", m_global.type, m_global.duration, m_global.easing, m_types);

    if (selectedNode == nullptr) return;

    ImGui::Separator();
    ImGui::TextUnformatted("Transition In");
    TransitionSettings& nodeSettings = selectedNode->transitionIn;
    bool overrideOn = nodeSettings.overrideGlobal;
    if (ImGui::Checkbox("Override global", &overrideOn)) {
        // Turning the override on starts from the global values so the picture does not jump to Cut.
        if (overrideOn && !nodeSettings.overrideGlobal) {
            nodeSettings.type = m_global.type;
            nodeSettings.duration = m_global.duration;
            nodeSettings.easing = m_global.easing;
        }
        nodeSettings.overrideGlobal = overrideOn;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Used when this node becomes the selection. A later playlist mode will use the same settings.");
    }
    if (!nodeSettings.overrideGlobal) ImGui::BeginDisabled();
    DrawSettingFields("node", nodeSettings.type, nodeSettings.duration, nodeSettings.easing, m_types);
    if (!nodeSettings.overrideGlobal) ImGui::EndDisabled();
}
