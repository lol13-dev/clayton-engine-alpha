#pragma once
#include <chrono>

// 1. DEFINE the Quality States.
enum class TF_QualityState {
    ULTRA,          // 60+ FPS: FULL GEOMETRY, ALL SHADOWS, MAX CALCULATIONS.
    BALANCED,       // 45-59 FPS: REDUCED GEOMETRY, SHADOWS INTACT.
    PERFORMANCE     // <45 FPS: MINIMAL GEOMETRY, SHADOWS DISABLED. EMERGENCY MODE.
};

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
    TrumFaster(int targetFPS = 60);

    // Destructor.
    ~TrumFaster();

    // MARKS the start and end of a RENDERING frame
    void StartFrame();
    
    // END.
    void EndFrame();
        
    // DIAGNOSTICS.
    float GetActualFPS() const;

    // CORE OPTIMIZATION ENGINE.
    TrumFasterProfile GetOptimizedProfile(int defaultBars, int visualMode);

private:
    int m_targetFPS;
    float m_targetFrameTimeMs;
    float m_currentFPS;
    float m_averageFrameTimeMs;

    // Hysteresis.
    TF_QualityState m_currentState;
    int m_framesBelowThreshold;
    int m_framesAboveThreshold;
    const int HYSTERESIS_LIMIT = 30; // WAIT 30 frames before changing states to PREVENT flickering.

    std::chrono::high_resolution_clock::time_point m_frameStart;
};