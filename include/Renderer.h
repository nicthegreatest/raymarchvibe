#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Renderer {
public:
    Renderer() = default;
    bool Init();
    void RenderFullscreenTexture(GLuint textureID);
    void RenderQuad(); // This will draw the quad using its own VAO

private:
    GLuint m_compositingProgram = 0;
    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;

    void setupQuad();
    bool setupCompositingShader();
};

#endif // RENDERER_H
