#pragma once

#include <glad/glad.h>

class Renderer {
public:
    bool Init();
    void RenderFullscreenTexture(GLuint textureID);
    static void RenderQuad();

private:
    bool setupCompositingShader();
    static void setupQuad();

    GLuint m_compositingProgram = 0;

    static GLuint s_quadVAO;
    static GLuint s_quadVBO;
};