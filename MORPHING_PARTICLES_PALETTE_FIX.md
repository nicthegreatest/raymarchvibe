# Morphing Particles Shader - Color Palette Fix

## Problem
The `shaders/morphing_particles.frag` shader had color uniforms (PrimaryColor, SecondaryColor, AccentColor, HighlightColor) correctly declared with palette metadata, but the colors were not being visibly applied to the final render - everything appeared white.

## Root Causes
1. **Excessive Color Washing**: Line 337 was adding `1.0` to all color channels (`nebulaColor += 1.0;`), which overexposed the entire image and washed out the palette colors
2. **Hard-coded Material Patterns Override**: Lines 138-153 had fixed parametric color patterns (with light offsets like 0.5, 0.6, 0.7) that were competing with palette colors instead of being modulated by them
3. **White Star Dominance**: Line 156 was adding pure white/blue matrix stars (`vec3(0.8, 0.9, 1.0)`) which further washed out the palette
4. **Weak Palette Color Influence**: The primary nebula color wasn't strongly influenced by the PrimaryColor uniform

## Changes Made

### 1. Fixed Color Brightness (Line 337)
**Before:**
```glsl
nebulaColor += 1.0; // Bright boost like original
```

**After:**
```glsl
nebulaColor *= 1.3; // Moderate brightening to preserve palette colors
```

**Impact**: Changed from additive (which clamps to max value) to multiplicative brightening, preserving color information and allowing palette colors to shine through.

### 2. Enhanced Primary Color Dominance (Lines 123-126)
**Before:**
```glsl
vec3 baseNebula = mix(PrimaryColor,
                      mix(SecondaryColor, PrimaryColor, nebula * 0.5),
                      0.7 + all_smooth * 0.3);
```

**After:**
```glsl
vec3 baseNebula = mix(PrimaryColor * (0.8 + nebula * 0.2),
                      PrimaryColor * (0.6 + nebula * 0.4),
                      abs(sin(gradTime)));
```

**Impact**: Primary color now modulates the nebula directly, ensuring dragging the Primary control visibly shifts the dominant hue across the entire weave.

### 3. Material-Based Modulation with Secondary Color (Lines 136-153)
**Before:**
```glsl
if(material_type == 0) {
    vec3 helicalColor = vec3(
        cos(point.x * 0.08 + iTime * 0.4) * 0.3 + 0.5,  // Fixed light color
        ...
    );
    baseColor = mix(baseColor, helicalColor, 0.5);  // Simple mix
}
```

**After:**
```glsl
if(material_type == 0) {
    vec3 helicalMod = vec3(
        cos(point.x * 0.08 + iTime * 0.4) * 0.5 + 0.5,  // Normalized modulation
        ...
    );
    // Blend palette color with modulated base
    baseColor = mix(baseColor, baseColor * helicalMod + SecondaryColor * (1.0 - helicalMod), 0.6);
}
```

**Impact**: Material patterns now modulate the SecondaryColor instead of being fixed light colors. Dragging SecondaryColor now noticeably shifts complementary tones throughout the helical and tendril systems.

### 4. Highlight Color Tinting on Stars (Line 156)
**Before:**
```glsl
baseColor += vec3(0.8, 0.9, 1.0) * matrix_stars * 0.6;  // Pure white/blue
```

**After:**
```glsl
baseColor += HighlightColor * matrix_stars * 0.5;  // Palette-driven
```

**Impact**: Matrix stars now tint with the HighlightColor uniform. Dragging Highlight now visibly tints the "rainbow/hot spots" (matrix stars flowing through the weave).

### 5. Accent Color Already Optimal (Line 159)
```glsl
vec3 accentGlow = AccentColor * veins * 0.8 + AccentColor * all_smooth * 0.3;
baseColor += accentGlow;
```

**Status**: This was already correct - Accent colors are applied to the energy veins with strong contribution. No changes needed.

## Behavior After Fix

Now the palette controls work as intended:

- **Dragging Primary**: Clearly shifts the dominant hue of the whole weave (nebula base colors all shift)
- **Dragging Secondary**: Noticeably shifts complementary tones (helical and tendril material patterns shift)
- **Dragging Accent**: Obviously recolors the glowing veins (energy veins that pulse through)
- **Dragging Highlight**: Noticeably tints the rainbow/hot spots (matrix-code stars streaming through)

## Shader Compliance Notes

- All uniforms use correct semantic naming: PrimaryColor, SecondaryColor, AccentColor, HighlightColor
- All uniforms have correct palette metadata: `{"widget":"color", "palette":true}`
- Shader is a 3D raymarching effect (uses `main()` not `mainImage()`)
- All palette uniforms are vec3 and properly mapped to float[3] by the rendering system
