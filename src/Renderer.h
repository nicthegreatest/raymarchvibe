#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h> // For GLuint, GLenum, etc.
#include <GLFW/glfw3.h> // For GLFWwindow
#include <string>
#include <vector> 

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Initialization and Cleanup
    bool Initialize(GLFWwindow* window); 
    void Shutdown();

    // Shader Management (Core compilation and linking)
    GLuint CompileShader(const char* source, GLenum type, std::string& errorLogString);
    GLuint CreateShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString);

    // Frame Rendering
    void BeginFrame(float r, float g, float b, float a); // Clears the screen
    void RenderQuad(); // Renders the fullscreen quad (assumes program is already bound)
    // void EndFrame(GLFWwindow* window); // glfwSwapBuffers is in main.cpp, so this might not be needed

    // Utility to get current display size
    void GetDisplaySize(int& width, int& height) const;

private:
    GLFWwindow* glfwWindow = nullptr; 
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    // GLuint activeShaderProgram = 0; // <<< REMOVE THIS LINE

    void setupQuad();
};

#endif // RENDERER_H
