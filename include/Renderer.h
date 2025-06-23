#pragma once

#include <glad/glad.h>
#include <string>

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Initializes the renderer: loads compositing shader, creates quad VAO.
    // Returns true on success, false on failure.
    bool Init();

    // Renders a texture to a fullscreen quad using the compositing shader.
    void RenderFullscreenTexture(GLuint textureID);

    // Optional: A generic method to draw the managed fullscreen quad,
    // could be used by ShaderEffect if it doesn't manage its own VAO.
    // void DrawQuad();

private:
    GLuint m_textureDisplayProgram = 0;
    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;

    // Shader loading and compilation helper (can be static or private)
    GLuint CompileAndLinkShaderProgram(const char* vertexPath, const char* fragmentPath);
    // Helper to load shader source from file (can be static or private)
    std::string LoadShaderSource(const char* filePath, std::string& errorMsg);
};
