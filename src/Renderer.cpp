#include "Renderer.h"
#include <iostream> 
#include <vector>   

// Constructor
Renderer::Renderer() : glfwWindow(nullptr), quadVAO(0), quadVBO(0) { // Removed activeShaderProgram init
    // Initialization of members is done via initializer list or in Initialize()
}

// Destructor
Renderer::~Renderer() {
    // Shutdown() should be called explicitly.
}

// Initialization
bool Renderer::Initialize(GLFWwindow* window) {
    if (!window) {
        std::cerr << "RENDERER ERROR: GLFW window pointer is null." << std::endl;
        return false;
    }
    glfwWindow = window;

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "RENDERER ERROR: Failed to initialize GLAD" << std::endl;
        return false;
    }
    std::cout << "Renderer: GLAD Initialized." << std::endl;

    setupQuad();
    std::cout << "Renderer: Fullscreen Quad Initialized." << std::endl;
    
    int width, height;
    GetDisplaySize(width, height);
    glViewport(0, 0, width, height);

    return true;
}

// Shutdown
void Renderer::Shutdown() {
    if (quadVAO != 0) {
        glDeleteVertexArrays(1, &quadVAO);
        quadVAO = 0;
    }
    if (quadVBO != 0) {
        glDeleteBuffers(1, &quadVBO);
        quadVBO = 0;
    }
    std::cout << "Renderer: Shutdown complete." << std::endl;
}

// Setup Fullscreen Quad
void Renderer::setupQuad() {
    float quadVertices[] = { 
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Shader Compilation
GLuint Renderer::CompileShader(const char* source, GLenum type, std::string& errorLogString) {
    errorLogString.clear();
    if (!source || std::string(source).empty()) {
        errorLogString = "RENDERER ERROR::SHADER::COMPILE_EMPTY_SOURCE Type: " + std::to_string(type);
        std::cerr << errorLogString << std::endl; 
        return 0;
    }
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    int success; 
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength; 
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLogVec(logLength > 0 ? logLength + 1 : 257); 
        if (logLength == 0) infoLogVec[0] = '\0'; 
        glGetShaderInfoLog(shader, static_cast<GLsizei>(infoLogVec.size()-1), NULL, infoLogVec.data());
        errorLogString = "RENDERER ERROR::SHADER::COMPILE_FAIL (" + std::string(type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") + ")\n" + infoLogVec.data();
        std::cerr << errorLogString << std::endl;
        glDeleteShader(shader); 
        return 0;
    }
    return shader;
}

// Shader Program Creation
GLuint Renderer::CreateShaderProgram(GLuint vertexShaderID, GLuint fragmentShaderID, std::string& errorLogString) {
    errorLogString.clear();
    if (vertexShaderID == 0 || fragmentShaderID == 0) {
        errorLogString = "RENDERER ERROR::PROGRAM::LINK_INVALID_SHADER_ID - One or both shaders failed to compile.";
        if (vertexShaderID != 0 && fragmentShaderID == 0) glDeleteShader(vertexShaderID); 
        else if (vertexShaderID == 0 && fragmentShaderID != 0) glDeleteShader(fragmentShaderID);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShaderID);
    glAttachShader(program, fragmentShaderID);
    glLinkProgram(program);
    
    int success; 
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength; 
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> infoLogVec(logLength > 0 ? logLength + 1 : 257);
        if (logLength == 0) infoLogVec[0] = '\0';
        glGetProgramInfoLog(program, static_cast<GLsizei>(infoLogVec.size()-1), NULL, infoLogVec.data());
        errorLogString = "RENDERER ERROR::PROGRAM::LINK_FAIL\n" + std::string(infoLogVec.data());
        std::cerr << errorLogString << std::endl;
        glDeleteProgram(program);
        program = 0; 
    }
    
    glDeleteShader(vertexShaderID); 
    glDeleteShader(fragmentShaderID);
    
    return program; 
}

// Set Active Shader Program - REMOVED
// void Renderer::SetActiveShaderProgram(GLuint programID) {
//     activeShaderProgram = programID;
// }

// Get Active Shader Program - REMOVED
// GLuint Renderer::GetActiveShaderProgram() const {
//     return activeShaderProgram;
// }

// Begin Frame
void Renderer::BeginFrame(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
}

// Render Quad
void Renderer::RenderQuad() {
    // Assumes glUseProgram has already been called by the main render loop
    // with the program from ShaderManager.
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    // It's usually good practice for the code that called glUseProgram(X) 
    // to also be responsible for calling glUseProgram(0) when done with X,
    // or before using another program.
    // main.cpp's render loop already calls glUseProgram(currentProgram),
    // and does not call glUseProgram(0) until the end of the loop or if currentProgram becomes 0.
    // So, removing glUseProgram(0) from here is fine.
}

// Get Display Size
void Renderer::GetDisplaySize(int& width, int& height) const {
    if (glfwWindow) {
        glfwGetFramebufferSize(glfwWindow, &width, &height);
    } else {
        width = 0;
        height = 0;
    }
}
