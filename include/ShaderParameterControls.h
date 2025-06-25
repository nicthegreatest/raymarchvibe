#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp> // For ShaderToyUniformControl metadata (though STUC is now from ShaderParser.h)
#include <glad/glad.h>       // For GLint in ShaderToyUniformControl (though STUC is now from ShaderParser.h)

// The definitions for ShaderDefineControl, ShaderToyUniformControl, and ShaderConstControl
// have been moved to ShaderParser.h to be the single source of truth.
// This file will include ShaderParser.h if it needs to refer to these types.
// For now, if this file's purpose was *only* to define these, it might become very small
// or primarily include ShaderParser.h.

#include "ShaderParser.h" // Include the source of truth for control structs

// If ShaderParameterControls.h has other unique declarations/definitions, they would remain here.
// Otherwise, this file might just be a wrapper around #include "ShaderParser.h"
// or could be removed if ShaderEffect.h and other files directly include "ShaderParser.h".
// For now, let's assume it might have other purposes and just include ShaderParser.h.
