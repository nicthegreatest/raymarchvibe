#include "ColorPaletteGenerator.h"
#include <cmath>
#include <algorithm>

// ===== HSV/HSL Conversion Functions =====

glm::vec3 ColorPaletteGenerator::RgbToHsv(const glm::vec3& rgb) {
    float r = rgb.x, g = rgb.y, b = rgb.z;
    float maxC = std::max({r, g, b});
    float minC = std::min({r, g, b});
    float delta = maxC - minC;

    // Hue
    float h = 0.0f;
    if (delta != 0.0f) {
        if (maxC == r) {
            h = 60.0f * fmod((g - b) / delta, 6.0f);
        } else if (maxC == g) {
            h = 60.0f * ((b - r) / delta + 2.0f);
        } else {
            h = 60.0f * ((r - g) / delta + 4.0f);
        }
    }
    if (h < 0.0f) h += 360.0f;

    // Saturation
    float s = (maxC == 0.0f) ? 0.0f : (delta / maxC);

    // Value
    float v = maxC;

    return glm::vec3(h, s, v);
}

glm::vec3 ColorPaletteGenerator::HsvToRgb(const glm::vec3& hsv) {
    float h = hsv.x, s = hsv.y, v = hsv.z;
    
    // Normalize hue to [0, 360)
    h = fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;

    float c = v * s;
    float x = c * (1.0f - fabs(fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (h < 60.0f) {
        r = c; g = x;
    } else if (h < 120.0f) {
        r = x; g = c;
    } else if (h < 180.0f) {
        g = c; b = x;
    } else if (h < 240.0f) {
        g = x; b = c;
    } else if (h < 300.0f) {
        r = x; b = c;
    } else {
        r = c; b = x;
    }

    return glm::vec3(r + m, g + m, b + m);
}

glm::vec3 ColorPaletteGenerator::RgbToHsl(const glm::vec3& rgb) {
    float r = rgb.x, g = rgb.y, b = rgb.z;
    float maxC = std::max({r, g, b});
    float minC = std::min({r, g, b});
    float delta = maxC - minC;

    // Lightness
    float l = (maxC + minC) / 2.0f;

    // Hue
    float h = 0.0f;
    if (delta != 0.0f) {
        if (maxC == r) {
            h = 60.0f * fmod((g - b) / delta, 6.0f);
        } else if (maxC == g) {
            h = 60.0f * ((b - r) / delta + 2.0f);
        } else {
            h = 60.0f * ((r - g) / delta + 4.0f);
        }
    }
    if (h < 0.0f) h += 360.0f;

    // Saturation
    float s = 0.0f;
    if (delta != 0.0f) {
        s = delta / (1.0f - fabs(2.0f * l - 1.0f));
    }

    return glm::vec3(h, s, l);
}

float ColorPaletteGenerator::Hue2Rgb(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

glm::vec3 ColorPaletteGenerator::HslToRgb(const glm::vec3& hsl) {
    float h = hsl.x / 360.0f;  // Normalize hue to [0, 1]
    float s = hsl.y;
    float l = hsl.z;

    float r, g, b;

    if (s == 0.0f) {
        r = g = b = l;
    } else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = Hue2Rgb(p, q, h + 1.0f / 3.0f);
        g = Hue2Rgb(p, q, h);
        b = Hue2Rgb(p, q, h - 1.0f / 3.0f);
    }

    return glm::vec3(r, g, b);
}

// ===== Color Harmony Generation =====

std::vector<glm::vec3> ColorPaletteGenerator::GenerateMonochromatic(const glm::vec3& baseColor, int steps) {
    std::vector<glm::vec3> palette;
    glm::vec3 hsv = RgbToHsv(baseColor);

    // Keep hue constant, vary saturation and value
    for (int i = 0; i < steps; ++i) {
        float t = (steps > 1) ? static_cast<float>(i) / (steps - 1) : 0.5f;
        // Create variation by adjusting saturation and value
        glm::vec3 varied = hsv;
        varied.y = std::max(0.0f, hsv.y * (0.3f + 0.7f * t));  // Saturation from 30% to 100%
        varied.z = std::max(0.0f, std::min(1.0f, 0.4f + 0.6f * t));  // Value from 40% to 100%
        palette.push_back(HsvToRgb(varied));
    }

    return palette;
}

std::vector<glm::vec3> ColorPaletteGenerator::GenerateComplementary(const glm::vec3& baseColor, int steps) {
    std::vector<glm::vec3> palette;
    glm::vec3 baseHsv = RgbToHsv(baseColor);
    glm::vec3 complementHsv = baseHsv;
    complementHsv.x = fmod(baseHsv.x + 180.0f, 360.0f);

    palette.push_back(baseColor);

    // Interpolate between base and complement
    for (int i = 1; i <= steps; ++i) {
        float t = static_cast<float>(i) / (steps + 1);
        glm::vec3 interpHsv = baseHsv;
        interpHsv.x = fmod(baseHsv.x + (180.0f * t), 360.0f);
        palette.push_back(HsvToRgb(interpHsv));
    }

    palette.push_back(HsvToRgb(complementHsv));
    return palette;
}

std::vector<glm::vec3> ColorPaletteGenerator::GenerateTriadic(const glm::vec3& baseColor) {
    std::vector<glm::vec3> palette;
    glm::vec3 baseHsv = RgbToHsv(baseColor);

    for (int i = 0; i < 3; ++i) {
        glm::vec3 triHsv = baseHsv;
        triHsv.x = fmod(baseHsv.x + (120.0f * i), 360.0f);
        palette.push_back(HsvToRgb(triHsv));
    }

    return palette;
}

std::vector<glm::vec3> ColorPaletteGenerator::GenerateAnalogous(const glm::vec3& baseColor, float angleStep) {
    std::vector<glm::vec3> palette;
    glm::vec3 baseHsv = RgbToHsv(baseColor);

    for (int i = -1; i <= 1; ++i) {
        glm::vec3 anaHsv = baseHsv;
        anaHsv.x = fmod(baseHsv.x + (angleStep * i) + 360.0f, 360.0f);
        palette.push_back(HsvToRgb(anaHsv));
    }

    return palette;
}

std::vector<glm::vec3> ColorPaletteGenerator::GenerateSplitComplementary(const glm::vec3& baseColor) {
    std::vector<glm::vec3> palette;
    glm::vec3 baseHsv = RgbToHsv(baseColor);

    palette.push_back(baseColor);

    // Two colors 150° and 210° from base (adjacent to 180°)
    glm::vec3 split1 = baseHsv;
    split1.x = fmod(baseHsv.x + 150.0f, 360.0f);
    palette.push_back(HsvToRgb(split1));

    glm::vec3 split2 = baseHsv;
    split2.x = fmod(baseHsv.x + 210.0f, 360.0f);
    palette.push_back(HsvToRgb(split2));

    return palette;
}

std::vector<glm::vec3> ColorPaletteGenerator::GenerateSquare(const glm::vec3& baseColor) {
    std::vector<glm::vec3> palette;
    glm::vec3 baseHsv = RgbToHsv(baseColor);

    for (int i = 0; i < 4; ++i) {
        glm::vec3 sqHsv = baseHsv;
        sqHsv.x = fmod(baseHsv.x + (90.0f * i), 360.0f);
        palette.push_back(HsvToRgb(sqHsv));
    }

    return palette;
}

std::vector<glm::vec3> ColorPaletteGenerator::GeneratePalette(
    const glm::vec3& baseColor,
    HarmonyType harmonyType,
    int steps
) {
    switch (harmonyType) {
        case HarmonyType::Monochromatic:
            return GenerateMonochromatic(baseColor, steps);
        case HarmonyType::Complementary:
            return GenerateComplementary(baseColor, steps);
        case HarmonyType::Triadic:
            return GenerateTriadic(baseColor);
        case HarmonyType::Analogous:
            return GenerateAnalogous(baseColor, 30.0f);
        case HarmonyType::SplitComplementary:
            return GenerateSplitComplementary(baseColor);
        case HarmonyType::Square:
            return GenerateSquare(baseColor);
        default:
            return {baseColor};
    }
}

// ===== Gradient/Interpolation Functions =====

std::vector<glm::vec3> ColorPaletteGenerator::GenerateGradient(const std::vector<glm::vec3>& palette, int steps) {
    std::vector<glm::vec3> gradient;

    if (palette.empty()) return gradient;
    if (palette.size() == 1) {
        for (int i = 0; i < steps; ++i) {
            gradient.push_back(palette[0]);
        }
        return gradient;
    }

    int colorsPerSegment = steps / (palette.size() - 1);
    int remainder = steps % (palette.size() - 1);

    for (size_t i = 0; i < palette.size() - 1; ++i) {
        int segmentSteps = colorsPerSegment + (i < (size_t)remainder ? 1 : 0);
        
        for (int j = 0; j < segmentSteps; ++j) {
            float t = static_cast<float>(j) / (segmentSteps > 1 ? segmentSteps : 1);
            gradient.push_back(Lerp(palette[i], palette[i + 1], t));
        }
    }

    // Ensure we have exactly the requested number of colors
    while (gradient.size() < (size_t)steps) {
        gradient.push_back(palette.back());
    }
    while (gradient.size() > (size_t)steps) {
        gradient.pop_back();
    }

    return gradient;
}

glm::vec3 ColorPaletteGenerator::Lerp(const glm::vec3& colorA, const glm::vec3& colorB, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return colorA * (1.0f - t) + colorB * t;
}

// ===== String Conversion =====

std::string ColorPaletteGenerator::HarmonyTypeToString(HarmonyType type) {
    switch (type) {
        case HarmonyType::Monochromatic:      return "Monochromatic";
        case HarmonyType::Complementary:      return "Complementary";
        case HarmonyType::Triadic:            return "Triadic";
        case HarmonyType::Analogous:          return "Analogous";
        case HarmonyType::SplitComplementary: return "Split-Complementary";
        case HarmonyType::Square:             return "Square";
        default:                              return "Unknown";
    }
}

ColorPaletteGenerator::HarmonyType ColorPaletteGenerator::StringToHarmonyType(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("monochromatic") != std::string::npos) return HarmonyType::Monochromatic;
    if (lower.find("complementary") != std::string::npos) return HarmonyType::Complementary;
    if (lower.find("triadic") != std::string::npos) return HarmonyType::Triadic;
    if (lower.find("analogous") != std::string::npos) return HarmonyType::Analogous;
    if (lower.find("split") != std::string::npos) return HarmonyType::SplitComplementary;
    if (lower.find("square") != std::string::npos) return HarmonyType::Square;

    return HarmonyType::Monochromatic;
}
