#pragma once

#include <string>
#include <vector>
#include <glad/glad.h> // For GLuint in GetOutputTexture

// Forward declaration for ImGui (if RenderUI directly uses ImGui types)
// For now, assuming RenderUI doesn't need specific ImGui types in its signature.

class Effect {
public:
    virtual ~Effect() = default;

    // Load resources like shaders, textures, etc.
    virtual void Load() = 0;

    // Update effect's internal state based on the current time.
    virtual void Update(float currentTime) = 0;

    // Render the effect (potentially to an FBO).
    virtual void Render() = 0;

    // Render ImGui controls specific to this effect.
    virtual void RenderUI() = 0;

    // --- New for Node Editor ---
    virtual int GetInputPinCount() const { return 0; }
    virtual int GetOutputPinCount() const { return 1; } // Default to one output texture
    // Sets the effect that provides input to a given input pin of this effect.
    virtual void SetInputEffect(int pinIndex, Effect* inputEffect) {}
    // Gets the primary output texture of this effect (e.g., its FBO texture).
    virtual GLuint GetOutputTexture() const { return 0; }


    // --- Common properties for all effects ---
    std::string name = "Untitled Effect";
    float startTime = 0.0f;
    float endTime = 10.0f;
    const int id; // Unique ID for each effect instance

private:
    static inline int nextId = 1; // Start IDs from 1 (0 can be invalid/none)

protected:
    // Protected constructor to ensure ID assignment
    Effect() : id(nextId++) {}
};
