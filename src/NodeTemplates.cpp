#include "NodeTemplates.h"
#include "ShaderEffect.h" // Needs full definition of ShaderEffect
#include "Effect.h"       // For std::unique_ptr<Effect>

// Note: ShaderEffect constructor takes:
// ShaderEffect(const std::string& initialShaderPath = "", int initialWidth = 800, int initialHeight = 600, bool isShadertoy = false);

namespace RaymarchVibe {
namespace NodeTemplates {

std::unique_ptr<Effect> CreateSimpleColorEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/simple_color.frag",
        initial_width,
        initial_height
    );
    effect->name = "Simple Color";
    // effect->Load(); // Important: Call Load() after creation to compile shader, etc.
                     // This should ideally be done after adding to scene and before first render of it.
                     // For now, let's assume the caller (e.g. main.cpp) will call Load().
    return effect;
}

std::unique_ptr<Effect> CreateInvertColorEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/invert_color.frag",
        initial_width,
        initial_height
    );
    effect->name = "Invert Color";
    // effect->Load();
    return effect;
}

std::unique_ptr<Effect> CreatePlasmaBasicEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/plasma_basic.frag",
        initial_width,
        initial_height
    );
    effect->name = "Basic Plasma";
    // effect->Load();
    return effect;
}

// Implement other factory functions here

} // namespace NodeTemplates
} // namespace RaymarchVibe
