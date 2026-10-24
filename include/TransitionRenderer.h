#pragma once

#include "TransitionSettings.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Effect;
class Renderer;

// Blends the already-rendered pictures of the previously selected node and the
// newly selected node. It does not create nodes and it does not read or write pins.
// Phase 2 can call Begin() for each playlist step; the trigger (click, timer, beat)
// stays outside this class.
class TransitionRenderer {
public:
    void Init(Renderer& renderer, int width, int height);
    void Resize(int width, int height);
    void Shutdown();

    TransitionSettings& Global() { return m_global; }
    const TransitionSettings& Global() const { return m_global; }

    // `forceCut` is the Shift+click path. Cut, or a node with no picture, shows `incoming` immediately.
    void OnPresentedChanged(Effect* outgoing, Effect* incoming, bool forceCut);
    void Begin(Effect* outgoing, Effect* incoming, const TransitionSettings& settings);
    void NotifyRemoved(Effect* effect);
    void Cancel();

    bool IsActive() const { return m_active; }
    Effect* Target() const { return m_to; }

    // Draws the blend into a reused FBO. Returns that texture, or 0 when the caller
    // should show the presented node directly (cut, finished, or failed).
    unsigned int Compose(float deltaTime);

    void RenderSettingsUI(Effect* selectedNode);

    static std::string LabelFor(const std::string& type);

private:
    struct TargetBuffer {
        unsigned int fbo = 0;
        unsigned int texture = 0;
    };

    void ScanTypes();
    bool EnsureProgram(const std::string& type);
    void DestroyProgramCache();
    void CreateTargets();
    void DestroyTargets();
    void CopyTextureToSnapshot(unsigned int texture);
    float EasedProgress() const;
    static bool IsSafeTypeName(const std::string& type);

    Renderer* m_renderer = nullptr;
    TransitionSettings m_global;
    std::vector<std::string> m_types;

    Effect* m_from = nullptr;
    Effect* m_to = nullptr;
    std::string m_activeType = "cut";
    float m_duration = 0.5f;
    TransitionEasing m_easing = TransitionEasing::Smoothstep;
    float m_progress = 0.0f;
    bool m_active = false;
    bool m_justStarted = false;
    bool m_useSnapshot = false;
    bool m_blendValid = false;

    int m_width = 0;
    int m_height = 0;
    unsigned int m_vertexShader = 0;
    TargetBuffer m_snapshot;
    TargetBuffer m_blend;
    std::unordered_map<std::string, unsigned int> m_programs;
    std::unordered_set<std::string> m_failedTypes;
    bool m_vertexFailed = false;
    bool m_scanLogged = false;
};
