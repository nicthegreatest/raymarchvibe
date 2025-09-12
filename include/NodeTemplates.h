#pragma once

#include <memory> // For std::unique_ptr
#include <string> // For std::string

// Forward declare Effect class to avoid full include if only pointers/references are needed.
// However, since we return std::unique_ptr<Effect>, a full definition of Effect is needed
// by the caller of these factory functions. For simplicity in this module,
// we might just include Effect.h if it's lightweight.
// Assuming Effect.h is included where these functions are called (e.g., main.cpp)
// or we include it here. For now, forward declare.
class Effect;

namespace RaymarchVibe {
namespace NodeTemplates {

// Default width and height for new effects created from templates
const int DEFAULT_TEMPLATE_EFFECT_WIDTH = 800;
const int DEFAULT_TEMPLATE_EFFECT_HEIGHT = 600;

std::unique_ptr<Effect> CreateSimpleColorEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);

std::unique_ptr<Effect> CreateInvertColorEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);

std::unique_ptr<Effect> CreatePlasmaBasicEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);

std::unique_ptr<Effect> CreateTexturePassthroughEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);

std::unique_ptr<Effect> CreateCircleShapeEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);

std::unique_ptr<Effect> CreateValueNoiseEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);

std::unique_ptr<Effect> CreateBrightnessContrastEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);

std::unique_ptr<Effect> CreateVignetteEffect(
    int initial_width = DEFAULT_TEMPLATE_EFFECT_WIDTH,
    int initial_height = DEFAULT_TEMPLATE_EFFECT_HEIGHT);


} // namespace NodeTemplates
} // namespace RaymarchVibe
