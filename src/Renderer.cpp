#include "Renderer.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream> // For error logging

Renderer::Renderer() : m_textureDisplayProgram(0), m_quadVAO(0), m_quadVBO(0) {}

Renderer::~Renderer() {
    if (m_textureDisplayProgram != 0) {
        glDeleteProgram(m_textureDisplayProgram);
    }
    if (m_quadVAO != 0) {
        glDeleteVertexArrays(1, &m_quadVAO);
    }
    if (m_quadVBO != 0) {
        glDeleteBuffers(1, &m_quadVBO);
    }
}

std::string Renderer::LoadShaderSource(const char* filePath, std::string& errorMsg) {
    errorMsg.clear();
    std::ifstream shaderFile;
    std::stringstream shaderStream;
    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        shaderFile.open(filePath);
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
    } catch (const std::ifstream::failure& e) {
        errorMsg = "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " + std::string(filePath) + " - " + e.what();
        std::cerr << errorMsg << std::endl;
        return "";
    }
    return shaderStream.str();
}

GLuint Renderer::CompileAndLinkShaderProgram(const char* vertexPath, const char* fragmentPath) {
    std::string errorMsg;
    std::string vertSource = LoadShaderSource(vertexPath, errorMsg);
    if (vertSource.empty()) {
        std::cerr << "Failed to load vertex shader: " << vertexPath << std::endl;
        return 0;
    }
    std::string fragSource = LoadShaderSource(fragmentPath, errorMsg);
    if (fragSource.empty()) {
        std::cerr << "Failed to load fragment shader: " << fragmentPath << std::endl;
        return 0;
    }

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vSrc = vertSource.c_str();
    glShaderSource(vertexShader, 1, &vSrc, NULL);
    glCompileShader(vertexShader);
    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        return 0;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fSrc = fragSource.c_str();
    glShaderSource(fragmentShader, 1, &fSrc, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader); // Don't forget to delete vertex shader if frag fails
        glDeleteShader(fragmentShader);
        return 0;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(shaderProgram);
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

bool Renderer::Init() {
    // Load compositing shader
    m_textureDisplayProgram = CompileAndLinkShaderProgram("shaders/texture.vert", "shaders/texture.frag");
    if (m_textureDisplayProgram == 0) {
        std::cerr << "Renderer Error: Failed to initialize texture display shader program." << std::endl;
        return false;
    }

    // Fullscreen quad vertices: Pos (x,y), TexCoords (s,t)
    // TexCoords are flipped on Y for standard OpenGL texture coordinate system (0,0 at bottom-left)
    // if your texture loading or FBO setup results in textures that need flipping.
    // If textures are already oriented correctly for direct UV mapping, use (0,0), (1,0), (0,1), (1,1) etc.
    // The provided texture.vert doesn't flip, so TexCoords should match quad orientation.
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0); // Unbind VAO

    return true;
}

void Renderer::RenderFullscreenTexture(GLuint textureID) {
    if (m_textureDisplayProgram == 0 || m_quadVAO == 0) {
        std::cerr << "Renderer::RenderFullscreenTexture - Renderer not initialized or error." << std::endl;
        return;
    }

    glUseProgram(m_textureDisplayProgram);

    // Set the texture uniform
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(m_textureDisplayProgram, "screenTexture"), 0);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
}

// void Renderer::DrawQuad() {
//     if (m_quadVAO == 0) return;
//     glBindVertexArray(m_quadVAO);
//     glDrawArrays(GL_TRIANGLES, 0, 6);
//     glBindVertexArray(0);
// }
