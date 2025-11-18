#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

/**
 * ColorPaletteGenerator: Utility class for color harmony generation and palette manipulation.
 * 
 * Features:
 * - HSV/HSL color space conversions
 * - Color harmony generation (complementary, triadic, analogous, split-complementary, square, monochromatic)
 * - Gradient interpolation
 * - Efficient color mathematics for real-time rendering
 */
class ColorPaletteGenerator {
public:
    enum class HarmonyType {
        Monochromatic,
        Complementary,
        Triadic,
        Analogous,
        SplitComplementary,
        Square
    };

    // ===== HSV/HSL Conversion Functions =====
    
    /**
     * Convert RGB color (0-1 range) to HSV
     * @param rgb Input color as glm::vec3 (x=R, y=G, z=B), values in [0,1]
     * @return HSV as glm::vec3 (x=H[0-360], y=S[0-1], z=V[0-1])
     */
    static glm::vec3 RgbToHsv(const glm::vec3& rgb);

    /**
     * Convert HSV to RGB color (0-1 range)
     * @param hsv Input color as glm::vec3 (x=H[0-360], y=S[0-1], z=V[0-1])
     * @return RGB as glm::vec3 (x=R, y=G, z=B), values in [0,1]
     */
    static glm::vec3 HsvToRgb(const glm::vec3& hsv);

    /**
     * Convert RGB color (0-1 range) to HSL
     * @param rgb Input color as glm::vec3 (x=R, y=G, z=B), values in [0,1]
     * @return HSL as glm::vec3 (x=H[0-360], y=S[0-1], z=L[0-1])
     */
    static glm::vec3 RgbToHsl(const glm::vec3& rgb);

    /**
     * Convert HSL to RGB color (0-1 range)
     * @param hsl Input color as glm::vec3 (x=H[0-360], y=S[0-1], z=L[0-1])
     * @return RGB as glm::vec3 (x=R, y=G, z=B), values in [0,1]
     */
    static glm::vec3 HslToRgb(const glm::vec3& hsl);

    // ===== Color Harmony Generation =====

    /**
     * Generate monochromatic color palette (same hue, varying saturation/lightness)
     * @param baseColor Input color in RGB (0-1 range)
     * @param steps Number of colors to generate
     * @return Vector of RGB colors
     */
    static std::vector<glm::vec3> GenerateMonochromatic(const glm::vec3& baseColor, int steps = 5);

    /**
     * Generate complementary color palette (base + opposite on color wheel)
     * @param baseColor Input color in RGB (0-1 range)
     * @param steps Number of intermediate colors to generate (returned vector will have steps + 1 colors)
     * @return Vector of RGB colors
     */
    static std::vector<glm::vec3> GenerateComplementary(const glm::vec3& baseColor, int steps = 3);

    /**
     * Generate triadic color palette (3 colors evenly spaced 120° apart)
     * @param baseColor Input color in RGB (0-1 range)
     * @return Vector of 3 RGB colors
     */
    static std::vector<glm::vec3> GenerateTriadic(const glm::vec3& baseColor);

    /**
     * Generate analogous color palette (colors adjacent on color wheel)
     * @param baseColor Input color in RGB (0-1 range)
     * @param angleStep Hue angle step in degrees (typically 30-60)
     * @return Vector of RGB colors (typically 3)
     */
    static std::vector<glm::vec3> GenerateAnalogous(const glm::vec3& baseColor, float angleStep = 30.0f);

    /**
     * Generate split-complementary palette (base + two colors adjacent to complement)
     * @param baseColor Input color in RGB (0-1 range)
     * @return Vector of 3 RGB colors
     */
    static std::vector<glm::vec3> GenerateSplitComplementary(const glm::vec3& baseColor);

    /**
     * Generate square (tetradic) palette (4 colors evenly spaced 90° apart)
     * @param baseColor Input color in RGB (0-1 range)
     * @return Vector of 4 RGB colors
     */
    static std::vector<glm::vec3> GenerateSquare(const glm::vec3& baseColor);

    /**
     * Generate palette based on harmony type
     * @param baseColor Input color in RGB (0-1 range)
     * @param harmonyType Type of harmony to generate
     * @param steps Additional steps for modes that support it (default 3)
     * @return Vector of RGB colors
     */
    static std::vector<glm::vec3> GeneratePalette(
        const glm::vec3& baseColor,
        HarmonyType harmonyType,
        int steps = 3
    );

    // ===== Gradient/Interpolation Functions =====

    /**
     * Generate smooth gradient between palette colors
     * @param palette Vector of palette colors
     * @param steps Number of interpolated colors to generate
     * @return Vector of interpolated RGB colors
     */
    static std::vector<glm::vec3> GenerateGradient(const std::vector<glm::vec3>& palette, int steps = 10);

    /**
     * Linear interpolation between two colors
     * @param colorA Start color
     * @param colorB End color
     * @param t Interpolation factor [0, 1]
     * @return Interpolated RGB color
     */
    static glm::vec3 Lerp(const glm::vec3& colorA, const glm::vec3& colorB, float t);

    /**
     * Get string representation of harmony type
     * @param type Harmony type enum
     * @return String name (e.g., "Complementary")
     */
    static std::string HarmonyTypeToString(HarmonyType type);

    /**
     * Get harmony type from string
     * @param str String name (case-insensitive)
     * @return Harmony type enum
     */
    static HarmonyType StringToHarmonyType(const std::string& str);

private:
    // Helper functions
    static float Hue2Rgb(float p, float q, float t);
    static void NormalizeHue(float& hue);
};
