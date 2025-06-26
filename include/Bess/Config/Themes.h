#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include "imgui.h" // Assuming imgui.h is accessible in the include path

namespace Bess {
namespace Config {

class Themes {
public:
    Themes();
    void applyTheme(const std::string &theme);
    void addTheme(const std::string &name, const std::function<void()> &callback);
    const std::unordered_map<std::string, std::function<void()>> &getThemes() const;

    // Utility function, can be public if needed elsewhere, or kept private / moved to .cpp
    static ImVec4 BlendColors(const ImVec4 &base, const ImVec4 &accent, float blendFactor);

private:
    std::unordered_map<std::string, std::function<void()>> m_themes;

    void setDarkThemeColors();
    void setModernDarkColors();
    void setCatpuccinMochaColors();
    void setBessDarkColors();
    void setFluentUIColors();
    // Add any other private theme-setting methods if new themes are added directly in themes.cpp
};

} // namespace Config
} // namespace Bess
