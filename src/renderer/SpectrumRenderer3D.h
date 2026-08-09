#pragma once

// UNLOCK modern Mac OpenGL CORE FEATURES.
// #define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class SpectrumRenderer3D {
public:
    // Constructor.
    SpectrumRenderer3D();

    // Destructor.
    ~SpectrumRenderer3D();

    // INITIALIZES the 3D Geometry AND the Mac-Safe UBO for the AUDIO DATA.
    void Initialize(unsigned int spectrumShaderID, unsigned int floorShaderID);

    // THE MASTER 3d drawing FUNCTION.
    void Render(float* fftData, const glm::mat4& view, const glm::mat4& projection);

private:
    // VERTEX ARRAY OBJECT (VAO) and VERTEX BUFFER OBJECT (VBO) IDs.
    unsigned int cubeVAO, cubeVBO;
    unsigned int floorVAO, floorVBO;

    // UNIFORM BUFFER OBJECT (UBO) for high-speed Mac audio data TRANSFER.
    unsigned int fftUBO;

    // STORED SHADER PROGRAMS.
    unsigned int m_SpectrumShader;
    unsigned int m_FloorShader;

    // GEOMETRY Generators.
    void SetupCubeGeometry();
    void SetupFloorGeometry();
};