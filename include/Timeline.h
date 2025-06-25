#pragma once
#ifndef TIMELINE_H
#define TIMELINE_H

#include <string>
#include <vector>
#include <nlohmann/json.hpp> // For JSON serialization

// Forward declaration of Effect, if TimelineEffect needs to know about it
// Or, if Effect itself will be part of TimelineEffect, include its header
// For now, assuming TimelineEffect is self-contained or uses basic types

struct TimelineEffect {
    int id;                         // Unique identifier for the effect (matches Effect::id)
    std::string name;               // Display name of the effect
    float startTime_seconds;        // Start time on the timeline
    float duration_seconds;         // Duration on the timeline
    // int trackIndex;              // Optional: Which track it belongs to on the timeline

    TimelineEffect(int _id = -1, const std::string& _name = "Effect", float _start = 0.0f, float _duration = 5.0f)
        : id(_id), name(_name), startTime_seconds(_start), duration_seconds(_duration) {}
};

// NLOHMANN_JSON_SERIALIZE_ENUM( Fruit, {
//     {_apple, "apple"},
//     {_orange, "orange"},
//     {_pear, "pear"},
// })
inline void to_json(nlohmann::json& j, const TimelineEffect& te) {
    j = nlohmann::json{
        {"id", te.id},
        {"name", te.name},
        {"startTime_seconds", te.startTime_seconds},
        {"duration_seconds", te.duration_seconds}
    };
}

inline void from_json(const nlohmann::json& j, TimelineEffect& te) {
    j.at("id").get_to(te.id);
    j.at("name").get_to(te.name);
    j.at("startTime_seconds").get_to(te.startTime_seconds);
    j.at("duration_seconds").get_to(te.duration_seconds);
}


struct TimelineState {
    bool isEnabled = false;                 // Is the timeline actively controlling master iTime?
    float currentTime_seconds = 0.0f;     // Current playback time of the timeline
    float totalDuration_seconds = 60.0f;  // Total visual duration of the timeline ruler
    std::vector<TimelineEffect> effects;  // Collection of *references* or simplified effect data for timeline display
                                          // Note: The actual Effect objects are in g_scene. This `effects` vector
                                          // might be redundant if g_scene's start/end times are directly used by ImGuiSimpleTimeline.
                                          // For serialization, we might save a representation of these.
                                          // For now, let's assume this `effects` vector is NOT what's directly saved,
                                          // but rather the `Effect::Serialize` from `g_scene` is the source of truth for effect data.
                                          // This `TimelineState::effects` might be deprecated or used differently.
                                          // For scene save/load, we'll serialize g_scene directly.
                                          // This struct will save its own state like isEnabled, currentTime, totalDuration, zoom, scroll.

    float zoomLevel = 1.0f;               // For timeline display scaling
    float horizontalScroll_seconds = 0.0f;// For timeline display scrolling
};

inline void to_json(nlohmann::json& j, const TimelineState& ts) {
    j = nlohmann::json{
        {"isEnabled", ts.isEnabled},
        {"currentTime_seconds", ts.currentTime_seconds},
        {"totalDuration_seconds", ts.totalDuration_seconds},
        {"zoomLevel", ts.zoomLevel},
        {"horizontalScroll_seconds", ts.horizontalScroll_seconds}
        // `effects` vector from TimelineState is NOT serialized here.
        // The full effect data including start/end times comes from g_scene's Effect::Serialize.
    };
}

inline void from_json(const nlohmann::json& j, TimelineState& ts) {
    j.at("isEnabled").get_to(ts.isEnabled);
    j.at("currentTime_seconds").get_to(ts.currentTime_seconds);
    j.at("totalDuration_seconds").get_to(ts.totalDuration_seconds);
    j.at("zoomLevel").get_to(ts.zoomLevel);
    j.at("horizontalScroll_seconds").get_to(ts.horizontalScroll_seconds);
    // `effects` vector is not deserialized here for the same reasons.
}


#endif // TIMELINE_H
