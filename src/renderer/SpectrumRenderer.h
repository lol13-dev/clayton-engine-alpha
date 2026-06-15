#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class SpectrumRenderer
{
public:
    // Constructor.
    SpectrumRenderer();

    // Destructor.
    ~SpectrumRenderer();

    // COMPILES the shaders and loads the rectangle into GPU memory.
    void Initialize();

    // The master drawing function.
    void DrawBar(float xPos, float width, float normalizedVolume);

private:
    // Vertex Array Object (VAO) and Vertex Buffer Object (VBO) IDs.
    unsigned int m_VAO, m_VBO, m_ShaderProgram;

    // Sets up the shaders and compiles them.
    void SetupShaders();

    // Loads the rectangle into GPU memory.
    void SetupShape();
};