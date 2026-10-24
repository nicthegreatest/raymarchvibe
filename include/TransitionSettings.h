#pragma once

#include <nlohmann/json.hpp>
#include <string>

// Per-node "how do I blend in when I become the selected picture".
// overrideGlobal false means use the global settings. This is not a graph pin.
// Phase 2 (not implemented) can step a playlist by resolving these settings and
// starting the same blend. That mode must stay outside the node links.
enum class TransitionEasing {
    Linear = 0,
    Smoothstep = 1
};

struct TransitionSettings {
    std::string type = "cut";
    float duration = 0.5f;
    TransitionEasing easing = TransitionEasing::Smoothstep;
    bool overrideGlobal = false;
};

inline float ClampTransitionDuration(float duration) {
    if (duration < 0.1f) return 0.1f;
    if (duration > 5.0f) return 5.0f;
    return duration;
}

inline const char* TransitionEasingName(TransitionEasing easing) {
    return easing == TransitionEasing::Linear ? "linear" : "smoothstep";
}

inline TransitionEasing TransitionEasingFromName(const std::string& name) {
    return name == "linear" ? TransitionEasing::Linear : TransitionEasing::Smoothstep;
}

inline TransitionSettings ResolveTransitionSettings(const TransitionSettings& global, const TransitionSettings& node) {
    TransitionSettings resolved = node.overrideGlobal ? node : global;
    resolved.duration = ClampTransitionDuration(resolved.duration);
    if (resolved.type.empty()) resolved.type = "cut";
    return resolved;
}

inline void WriteTransitionObject(nlohmann::json& object, const TransitionSettings& settings, bool includeOverride) {
    object = nlohmann::json{
        {"type", settings.type},
        {"duration", settings.duration},
        {"easing", TransitionEasingName(settings.easing)}
    };
    if (includeOverride)
        object["overrideGlobal"] = settings.overrideGlobal;
}

inline void ReadTransitionObject(const nlohmann::json& object, TransitionSettings& settings, bool readOverride) {
    if (!object.is_object()) return;
    if (object.contains("type") && object["type"].is_string())
        settings.type = object["type"].get<std::string>();
    if (settings.type.empty()) settings.type = "cut";
    if (object.contains("duration") && object["duration"].is_number())
        settings.duration = ClampTransitionDuration(object["duration"].get<float>());
    if (object.contains("easing") && object["easing"].is_string())
        settings.easing = TransitionEasingFromName(object["easing"].get<std::string>());
    if (readOverride && object.contains("overrideGlobal") && object["overrideGlobal"].is_boolean())
        settings.overrideGlobal = object["overrideGlobal"].get<bool>();
}

inline void WriteTransitionIn(nlohmann::json& nodeJson, const TransitionSettings& settings) {
    WriteTransitionObject(nodeJson["transitionIn"], settings, true);
}

inline void ReadTransitionIn(const nlohmann::json& nodeJson, TransitionSettings& settings) {
    if (nodeJson.contains("transitionIn"))
        ReadTransitionObject(nodeJson["transitionIn"], settings, true);
}
