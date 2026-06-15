// Unlock modern Mac OpenGL Core features
#define GLFW_INCLUDE_GLCOREARB 
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "SpectrumRenderer.h"

// ---------------------------------------------------------
// THE GPU SHADER PROGRAMS (Written in GLSL)
// ---------------------------------------------------------
const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    uniform mat4 model;
    uniform mat4 projection;
    void main() {
        // Multiplies the raw shape by our Audio Volume (model) and our Screen Camera (projection)
        gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec3 barColor;
    void main() {
        // Paints the pixels!
        FragColor = vec4(barColor, 1.0);
    }
)";

SpectrumRenderer::SpectrumRenderer() : m_VAO(0), m_VBO(0), m_ShaderProgram(0) {}
SpectrumRenderer::~SpectrumRenderer() {}

void SpectrumRenderer::Initialize()
{
    SetupShaders();
    SetupShape();
    std::cout << "[RENDERER] Spectrum Renderer and GLSL Shaders Compiled.\n";
}

void SpectrumRenderer::SetupShaders()
{
    // Compile Vertex Shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Compile Fragment Shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Link them together into one GPU Program
    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vertexShader);
    glAttachShader(m_ShaderProgram, fragmentShader);
    glLinkProgram(m_ShaderProgram);

    // Clean up
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void SpectrumRenderer::SetupShape()
{
    // Define a 1x1 unit square resting exactly on the bottom edge (Y=0)
    // This ensures that when we scale it up, it grows UPWARDS, not from the center!
    float vertices[] = {
         0.0f, 1.0f, // Top Left
         1.0f, 0.0f, // Bottom Right
         0.0f, 0.0f, // Bottom Left

         0.0f, 1.0f, // Top Left
         1.0f, 1.0f, // Top Right
         1.0f, 0.0f  // Bottom Right
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void SpectrumRenderer::DrawBar(float xPos, float width, float normalizedVolume)
{
    glUseProgram(m_ShaderProgram);

    // 1. The Camera (Maps math coordinates to our 1280x720 window perfectly)
    glm::mat4 projection = glm::ortho(0.0f, 1280.0f, 0.0f, 720.0f, -1.0f, 1.0f);
    
    // 2. The Audio Scale (Limits the max bar height to 600 pixels)
    float maxBarHeight = 600.0f; 
    float actualHeight = normalizedVolume * maxBarHeight;

    // 3. The Transformation Matrix
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(xPos, 60.0f, 0.0f)); // Start drawing 60 pixels from the bottom of the window
    model = glm::scale(model, glm::vec3(width, actualHeight, 1.0f)); // Stretch it up based on the music!

    // Send the matrices to the GPU
    glUniformMatrix4fv(glGetUniformLocation(m_ShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(m_ShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

    // Set the color to a sleek "Figma Neon Blue" (R: 0.1, G: 0.6, B: 1.0)
    glUniform3f(glGetUniformLocation(m_ShaderProgram, "barColor"), 0.1f, 0.6f, 1.0f); 

    // Draw the actual shape
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
