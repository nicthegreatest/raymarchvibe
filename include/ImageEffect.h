#pragma once

#include "Effect.h"
#include <string>

class ImageEffect : public Effect {
public:
    ImageEffect();
    ~ImageEffect() override;

    void Load() override;
    void Render() override;
    void RenderUI() override;
    void Update(float time) override;
    void ResetParameters() override;

    GLuint GetOutputTexture() const override;
    bool LoadImage(const std::string& path);

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& j) override;

private:
    GLuint m_textureID = 0;
    int m_width = 0;
    int m_height = 0;
    std::string m_imagePath;
    char m_imagePathBuffer[256] = "";
};
