#include "NodeTemplates.h"
#include "ShaderEffect.h" // Needs full definition of ShaderEffect
#include "ImageEffect.h"
#include "Effect.h"       // For std::unique_ptr<Effect>

// Note: ShaderEffect constructor takes:
// ShaderEffect(const std::string& initialShaderPath = "", int initialWidth = 800, int initialHeight = 600, bool isShadertoy = false);

namespace RaymarchVibe {
namespace NodeTemplates {

std::unique_ptr<Effect> CreateCircularAudioVizEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/viz_circular_audio.frag",
        initial_width,
        initial_height
    );
    effect->name = "Circular Audio Viz";
    return effect;
}

std::unique_ptr<Effect> CreateOrganicAudioVizEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/organic_audio_viz.frag",
        initial_width,
        initial_height
    );
    effect->name = "Organic Audio Viz";
    return effect;
}

std::unique_ptr<Effect> CreateImageLoaderEffect(int initial_width, int initial_height) {
    // initial_width and initial_height are ignored for now, as image size is determined on load
    // but they are kept for consistency with the other factory functions.
    (void)initial_width;
    (void)initial_height;
    auto effect = std::make_unique<ImageEffect>();
    effect->name = "Image Loader";
    return effect;
}

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

std::unique_ptr<Effect> CreateValueNoiseEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/noise_value.frag",
        initial_width,
        initial_height
    );
    effect->name = "Value Noise";
    return effect;
}

std::unique_ptr<Effect> CreateBrightnessContrastEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/filter_brightness_contrast.frag",
        initial_width,
        initial_height
    );
    effect->name = "Brightness/Contrast";
    return effect;
}

std::unique_ptr<Effect> CreateVignetteEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/filter_vignette.frag",
        initial_width,
        initial_height
    );
    effect->name = "Vignette";
    return effect;
}

std::unique_ptr<Effect> CreateSharpenEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/post_processing/filter_sharpen.frag",
        initial_width,
        initial_height
    );
    effect->name = "Sharpen";
    return effect;
}

std::unique_ptr<Effect> CreateColorCorrectionEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/post_processing/filter_color_correction.frag",
        initial_width,
        initial_height
    );
    effect->name = "Color Correction";
    return effect;
}

std::unique_ptr<Effect> CreateGrainEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/post_processing/filter_grain.frag",
        initial_width,
        initial_height
    );
    effect->name = "Grain";
    return effect;
}

std::unique_ptr<Effect> CreateChromaticAberrationEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/post_processing/filter_chromatic_aberration.frag",
        initial_width,
        initial_height
    );
    effect->name = "Chromatic Aberration";
    return effect;
}

std::unique_ptr<Effect> CreateBloomEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/post_processing/filter_bloom.frag",
        initial_width,
        initial_height
    );
    effect->name = "Bloom";
    return effect;
}

std::unique_ptr<Effect> CreateToneMappingEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/post_processing/filter_tonemapping.frag",
        initial_width,
        initial_height
    );
    effect->name = "Tone Mapping";
    return effect;
}

std::unique_ptr<Effect> CreateNoiseEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/post_processing/generator_noise.frag",
        initial_width,
        initial_height
    );
    effect->name = "Noise Generator";
    return effect;
}

std::unique_ptr<Effect> CreateRaymarchSphereEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/raymarch_sphere.frag",
        initial_width,
        initial_height
    );
    effect->name = "Raymarch Sphere";
    return effect;
}

std::unique_ptr<Effect> CreateDebugColorEffect(int initial_width, int initial_height) {
    auto effect = std::make_unique<ShaderEffect>(
        "shaders/templates/debug_color.frag",
        initial_width,
        initial_height
    );
    effect->name = "Debug Color";
    return effect;
}

} // namespace NodeTemplates
} // namespace RaymarchVibe
