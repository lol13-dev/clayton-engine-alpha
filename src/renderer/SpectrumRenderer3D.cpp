// FIX: SILENCE the TRAE/VSCode FAKE ERRORS on Mac
#ifdef __APPLE__
    #include <OpenGL/gl3.h>
#endif

#include "SpectrumRenderer3D.h"
#include <iostream>

// CONSTRUCTOR.
SpectrumRenderer3D::SpectrumRenderer3D() : cubeVAO(0), cubeVBO(0), floorVAO(0), floorVBO(0), fftUBO(0), m_SpectrumShader(0), m_FloorShader(0) {}

// DESTRUCTOR.
SpectrumRenderer3D::~SpectrumRenderer3D() {
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteVertexArrays(1, &floorVAO);
    glDeleteBuffers(1, &floorVBO);
    glDeleteBuffers(1, &fftUBO);
}

// ENGINE INITIALIZE.
void SpectrumRenderer3D::Initialize(unsigned int spectrumShaderID, unsigned int floorShaderID) {
    m_SpectrumShader = spectrumShaderID;
    m_FloorShader = floorShaderID;

    SetupCubeGeometry();
    SetupFloorGeometry();

    // Mac-Safe AUDIO PIPE: INITIALIZE the Uniform Buffer Object (UBO).
    // THIS ALLOWS me to SEND 256 float to GPU instantly without a LOOP.
    glGenBuffers(1, &fftUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, fftUBO);
    // ALLOCATE space for 256 floats (ZEROED OUT initially).
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 1024, nullptr, GL_DYNAMIC_DRAW);

    // BIND it to BINDING POINT 0 to match THE SHADER: layout(std140, binding = 0).
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, fftUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    std::cout << "[RENDERER_STATUS] 3D Spectrum Engine & UBO INITIALIZED.\n";
}

// 3D Engine Progress & Flow.
void SpectrumRenderer3D::Render(float* fftData, const glm::mat4& view, const glm::mat4& projection) {
    // ==========================================
    // Step for Fix: THE STD140 fix.
    // ==========================================
    // CREATE a temporary array of 1024 floats.
    float paddedData[1024] = {0.0f};

    // Put the real audio data into every 4th slot so the Mac GPU reads it correctly!
    for (int i = 0; i < 256; i++) {
        paddedData[i * 4] = fftData[i]; 
    }

    // Send the PADDED data to the GPU
    glBindBuffer(GL_UNIFORM_BUFFER, fftUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float) * 1024, paddedData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // // ==========================================
    // // Step1: STREAM Audio to GPUs.
    // // ==========================================
    // glBindBuffer(GL_UNIFORM_BUFFER, fftUBO);
    // glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float) * 256, fftData);
    // glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // ==========================================
    // Step2: PREPARE Matrices.
    // ==========================================
    glm::mat4 baseModel = glm::mat4(1.0f);
    // Linear Algebra: INVERT the Y-axis TO MIRROR the WORLD under the FLOOR.
    glm::mat4 mirrorMatrix = glm::scale(baseModel, glm::vec3(1.0f, -1.0f, 1.0f));

    // ==========================================
    // Step3: Pass 1, RENDER the Upside-Down REFLECTION.
    // ==========================================
    glDepthMask(GL_FALSE); // [NOTE!] PREVENT REFLECTION FROM WRITING TO THE DEPTH BUFFER.
    glUseProgram(m_SpectrumShader);

    glUniformMatrix4fv(glGetUniformLocation(m_SpectrumShader, "model"), 1, GL_FALSE, glm::value_ptr(mirrorMatrix));
    glUniformMatrix4fv(glGetUniformLocation(m_SpectrumShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(m_SpectrumShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // DARKEN the REFLECTION FOR THE GLOSSY EFFECT.
    glUniform1f(glGetUniformLocation(m_SpectrumShader, "alphaMultiplier"), 0.25f);

    glBindVertexArray(cubeVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, 256); // [NOTE!] DRAW all 256 BAR INSTANTLY.
    
    // ==========================================
    // Step4: Pass 2, Render the Glossy Floor.
    // ==========================================
    glDepthMask(GL_TRUE); // [NOTE!] Re-enable depth writing.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_FloorShader);
    glUniformMatrix4fv(glGetUniformLocation(m_FloorShader, "model"), 1, GL_FALSE, glm::value_ptr(baseModel));
    glUniformMatrix4fv(glGetUniformLocation(m_FloorShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(m_FloorShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(floorVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ==========================================
    // Step5: Pass 3, RENDER the real 3D Spectrum.
    // ==========================================
    glUseProgram(m_SpectrumShader);
    glUniformMatrix4fv(glGetUniformLocation(m_SpectrumShader, "model"), 1, GL_FALSE, glm::value_ptr(baseModel));
    
    // [NOTE!] SET TO full brightness for the real cubes.
    glUniform1f(glGetUniformLocation(m_SpectrumShader, "alphaMultiplier"), 1.0f); 

    glBindVertexArray(cubeVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, 256); 
    
    glDisable(GL_BLEND);
}

// CUBE RENDERING
void SpectrumRenderer3D::SetupCubeGeometry() {
    // STANDARD 36-vertex 1x1x1 OpenGL CUBE DEFINITION.
    // Y-axis SITS perfectly on 0.0 at the BOTTOM, GROWING to 1.0 at the TOP.
    float vertices[] = {
        // Back face
        -0.08f,  0.0f, -0.08f,  0.08f,  0.0f, -0.08f,  0.08f,  1.0f, -0.08f,  
        0.08f,  1.0f, -0.08f, -0.08f,  1.0f, -0.08f, -0.08f,  0.0f, -0.08f,
        // Front face
        -0.08f,  0.0f,  0.08f,  0.08f,  0.0f,  0.08f,  0.08f,  1.0f,  0.08f,  
        0.08f,  1.0f,  0.08f, -0.08f,  1.0f,  0.08f, -0.08f,  0.0f,  0.08f,
        // Left face
        -0.08f,  1.0f,  0.08f, -0.08f,  1.0f, -0.08f, -0.08f,  0.0f, -0.08f,  
        -0.08f,  0.0f, -0.08f, -0.08f,  0.0f,  0.08f, -0.08f,  1.0f,  0.08f,
        // Right face
        0.08f,  1.0f,  0.08f,  0.08f,  1.0f, -0.08f,  0.08f,  0.0f, -0.08f,  
        0.08f,  0.0f, -0.08f,  0.08f,  0.0f,  0.08f,  0.08f,  1.0f,  0.08f,
        // Bottom face
        -0.08f,  0.0f, -0.08f,  0.08f,  0.0f, -0.08f,  0.08f,  0.0f,  0.08f,  
        0.08f,  0.0f,  0.08f, -0.08f,  0.0f,  0.08f, -0.08f,  0.0f, -0.08f,
        // Top face
        -0.08f,  1.0f, -0.08f,  0.08f,  1.0f, -0.08f,  0.08f,  1.0f,  0.08f,  
        0.08f,  1.0f,  0.08f, -0.08f,  1.0f,  0.08f, -0.08f,  1.0f, -0.08f
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // POSITION ATTRIBUTE.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// FLOOR GEOMETRY
void SpectrumRenderer3D::SetupFloorGeometry() {
    // A MASSIVE flat plane on the XZ axis to act as our mirror floor.
    float floorVerts[] = {
        -50.0f, 0.0f, -50.0f,   50.0f, 0.0f, -50.0f,   50.0f, 0.0f,  50.0f,
        50.0f, 0.0f,  50.0f,  -50.0f, 0.0f,  50.0f,  -50.0f, 0.0f, -50.0f
    };

    glGenVertexArrays(1, &floorVAO);
    glGenBuffers(1, &floorVBO);
    glBindVertexArray(floorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floorVerts), floorVerts, GL_STATIC_DRAW);
    
    // POSITION ATTRIBUTE.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}