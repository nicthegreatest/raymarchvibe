#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Renderer {
public:
    Renderer() = default;
    bool Init();
    void RenderFullscreenTexture(GLuint textureID);

    // Make this method static so it can be called from anywhere
    static void RenderQuad();

private:
    GLuint m_compositingProgram = 0;

    // Make the VAO and VBO static as well
    static GLuint s_quadVAO;
    static GLuint s_quadVBO;

    void setupQuad();
    bool setupCompositingShader();
};

#endif // RENDERER_H
