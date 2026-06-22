#include "TrumFaster.h"
#include <thread>
#include <algorithm>
#include <iostream>

// Construtor.
TrumFaster::TrumFaster(int targetFPS)
    : m_targetFPS(targetFPS), 
    m_currentFPS(static_cast<float>(targetFPS)),
    m_currentState(TF_QualityState::ULTRA),
    m_framesBelowThreshold(0),
    m_framesAboveThreshold(0)
{
    m_targetFrameTimeMs = 1000.0f / targetFPS;
    m_averageFrameTimeMs = m_targetFrameTimeMs;
    std::cout << "[TrumFaster] Initialized. Target FPS: " << m_targetFPS << "\n";
}

// Destructor.
TrumFaster::~TrumFaster() {}

void TrumFaster::StartFrame() {
    // START the stopwatch.
    m_frameStart = std::chrono::high_resolution_clock::now();
}

void TrumFaster::EndFrame() {
    // END the stopwatch.
    // 1. MATH time (how long did audio + RENDERING take?)
    auto frameMathEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> mathTime = frameMathEnd - m_frameStart;
    
    // (FIXED) 2. SLEEP for the remainder of the 16.66ms frame.
    float sleepTimeMs = m_targetFrameTimeMs - mathTime.count();
    if (sleepTimeMs > 0.0f) {
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(sleepTimeMs));
    }

    // 3. NOW calculate the TRUE FPS including the SLEEP time.
    auto trueFrameEnd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> totalFrameTime = trueFrameEnd - m_frameStart;

    m_averageFrameTimeMs = (m_averageFrameTimeMs * 0.9f) + (totalFrameTime.count() * 0.1f);
    m_currentFPS = 1000.0f / m_averageFrameTimeMs;
    
}

float TrumFaster::GetActualFPS() const {
    return m_currentFPS;
}

TrumFasterProfile TrumFaster::GetOptimizedProfile(int defaultBars, int visualMode) {
    // 1. EVALUATE Hardware performance with Hysteresis.
    if (m_currentFPS < 45.0f) {
        m_framesAboveThreshold++;
        m_framesBelowThreshold = 0;
    } else if (m_currentFPS >= 58.0f) {
        m_framesBelowThreshold++;
        m_framesAboveThreshold = 0;
    } else {
        // DEADZONE (45-58 FPS). Do nothing to PREVENT flickering.
        m_framesAboveThreshold = 0;
        m_framesBelowThreshold = 0;
    }

    // 2. STATE machine transition.
    if (m_framesBelowThreshold > HYSTERESIS_LIMIT) {
        if (m_currentState == TF_QualityState::ULTRA) m_currentState = TF_QualityState::BALANCED;
        else if (m_currentState == TF_QualityState::BALANCED) m_currentState = TF_QualityState::PERFORMANCE;
        m_framesBelowThreshold = 0; // RESET counter after shifting.
    } else if (m_framesAboveThreshold > HYSTERESIS_LIMIT * 4) { // TAKES 4x longer to UPGRADE to ensure stability.
        if (m_currentState == TF_QualityState::PERFORMANCE) m_currentState = TF_QualityState::BALANCED;
        else if (m_currentState == TF_QualityState::BALANCED) m_currentState = TF_QualityState::ULTRA;
        m_framesAboveThreshold = 0; // RESET counter after shifting.
    }

    // 3. BUILD THE RENDER PROFILE based on current state.
    TrumFasterProfile profile;
    profile.lerpAttackSpeed = 0.92f; // DEFAULT STANDARD.

    switch (m_currentState) {
        case TF_QualityState::ULTRA:
            profile.activeBars = defaultBars;
            profile.enableShadows = true;
            break;

        case TF_QualityState::BALANCED:
            profile.activeBars = std::max(16, defaultBars / 2);
            profile.enableShadows = true;
            break;
                
        case TF_QualityState::PERFORMANCE:
            profile.activeBars = std::max(16, defaultBars / 4);
            profile.enableShadows = false;   // DISABLE expensive double-drawing for Neon Polyline.
            profile.lerpAttackSpeed = 0.50f; // SLOWER math CALCULATION.
            break;

    }
    return profile;
}