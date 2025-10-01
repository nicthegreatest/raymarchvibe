#include "ImageEffect.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include "stb_image.h"
#include <glad/glad.h>
#include <iostream>

ImageEffect::ImageEffect()
    : Effect() { // Call the default base constructor
    name = "Image Loader";
    m_width = 0;
    m_height = 0;
    m_textureID = 0;
}

ImageEffect::~ImageEffect() {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
    }
}

void ImageEffect::Render() {
    // ImageEffect doesn't render in the traditional sense (like a shader),
    // it just provides its loaded texture as an output.
}

void ImageEffect::Update(float time) {
    // No-op
}

void ImageEffect::Load() {
    // No-op, loading is handled via UI
}

void ImageEffect::ResetParameters() {
    // No-op
}

GLuint ImageEffect::GetOutputTexture() const {
    return m_textureID;
}

bool ImageEffect::LoadImage(const std::string& path) {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "ImageEffect Error: Failed to load image: " << path << std::endl;
        stbi_image_free(data);
        return false;
    }

    m_imagePath = path;
    strncpy(m_imagePathBuffer, path.c_str(), sizeof(m_imagePathBuffer) - 1);
    m_imagePathBuffer[sizeof(m_imagePathBuffer) - 1] = '\0';

    GLenum format;
    if (channels == 1)
        format = GL_RED;
    else if (channels == 3)
        format = GL_RGB;
    else if (channels == 4)
        format = GL_RGBA;
    else {
        std::cerr << "ImageEffect Error: Unsupported number of channels: " << channels << std::endl;
        stbi_image_free(data);
        return false;
    }

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    m_width = width;
    m_height = height;
    
    return true;
}

void ImageEffect::RenderUI() {
    ImGui::InputText("##ImagePath", m_imagePathBuffer, sizeof(m_imagePathBuffer), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Load Image")) {
        ImGuiFileDialog::Instance()->OpenDialog("LoadImageDlgKey", "Choose Image File", ".jpg,.jpeg,.png,.bmp", IGFD::FileDialogConfig{".", "", "", 1, nullptr, ImGuiFileDialogFlags_None, {}, 250.0f, {}});
    }

    if (ImGuiFileDialog::Instance()->Display("LoadImageDlgKey")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            LoadImage(filePathName);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (m_textureID != 0) {
        ImGui::Text("Size: %d x %d", m_width, m_height);
        ImGui::Image((void*)(intptr_t)m_textureID, ImVec2(128, 128), ImVec2(0,1), ImVec2(1,0));
    }
}

nlohmann::json ImageEffect::Serialize() const {
    nlohmann::json j;
    j["id"] = this->id;
    j["name"] = this->name;
    j["startTime"] = this->startTime;
    j["endTime"] = this->endTime;
    j["type"] = "ImageEffect";
    j["imagePath"] = m_imagePath;
    return j;
}

void ImageEffect::Deserialize(const nlohmann::json& j) {
    if (j.contains("name")) name = j["name"].get<std::string>();
    if (j.contains("startTime")) startTime = j["startTime"].get<float>();
    if (j.contains("endTime")) endTime = j["endTime"].get<float>();

    if (j.contains("imagePath")) {
        std::string path = j["imagePath"].get<std::string>();
        if (!path.empty()) {
            LoadImage(path);
        }
    }
}

std::unique_ptr<Effect> ImageEffect::Clone() const {
    auto newEffect = std::make_unique<ImageEffect>();
    newEffect->name = this->name + " (Copy)";
    if (!this->m_imagePath.empty()) {
        newEffect->LoadImage(this->m_imagePath);
    }
    return newEffect;
}
