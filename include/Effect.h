#pragma once

#include <string>
#include <vector> // For potential future use (e.g., list of input textures)

// Forward declaration for ImGui (if RenderUI directly uses ImGui types)
// Alternatively, include ImGui headers if specific types are needed in the interface.
// For now, assuming RenderUI doesn't need specific ImGui types in its signature.

class Effect {
public:
    virtual ~Effect() = default;

    // Load resources like shaders, textures, etc.
    // This should be called once when the effect is added or its source changes.
    virtual void Load() = 0;

    // Update effect's internal state based on the current time.
    // Called every frame if the effect is active.
    virtual void Update(float currentTime) = 0;

    // Render the effect.
    // This typically involves binding shaders, setting uniforms, and drawing geometry.
    // If rendering to an FBO, this method would handle that.
    virtual void Render() = 0;

    // Render ImGui controls specific to this effect.
    // This allows each effect to define its own UI for parameters.
    virtual void RenderUI() = 0;

    // --- Common properties for all effects ---
    std::string name = "Untitled Effect";
    float startTime = 0.0f;
    float endTime = 10.0f;

    // Optional: Add a unique ID if needed for management
    // int id;
};
