#pragma once
#include <chrono>
#include <GLFW/glfw3.h> // <- NEW: REQUIRED for GLuint and OpenGL queries.

// 2. DEFINE the exact rendering instructions TrumFaster passes to the Engine.
struct TrumFasterProfile {
    int activeBars;
    bool enableShadows;
    float lerpAttackSpeed; // CAN be SLOWED down to save CPU cycles IF NEEDED.
};

// ========================================================================
// TRUMFASTER (ALPHA 1.0) - ADAPTIVE RENDERING & FRAME PACER
// ========================================================================
class TrumFaster {
public:
    // Construtor.
    TrumFaster(int targetFPS);

    // Destructor.
    ~TrumFaster();

    // MARKS the start and end of a RENDERING frame
    void StartFrame();
    
    // END.
    void EndFrame();
        
    // DIAGNOSTICS.
    float GetActualFPS() const;
    float GetGPUFrameTime() const; // NEW DIAGNOTICS.

    // CORE OPTIMIZATION ENGINE.
    TrumFasterProfile GetOptimizedProfile(int defaultBars, int visualMode);

private:
    int m_targetFPS;
    float m_actualFPS;
    float m_gpuFrameTimeMs; // TRACKES the GPU render time.

    std::chrono::milliseconds m_targetFrameTime;
    std::chrono::time_point<std::chrono::steady_clock> m_frameStartTime;

    // OpenGL Hardware Stopwatch ID.
    GLuint m_gpuQueryID;
};