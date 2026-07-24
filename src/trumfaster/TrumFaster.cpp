#include "TrumFaster.h"
#include <iostream>
#include <algorithm> // For std::max
#include <thread>    // For std::this_thread::sleep_for
#include <GLFW/glfw3.h> // For raw OpenGL calls.

TrumFaster::TrumFaster(int targetFPS) 
    : m_targetFPS(targetFPS), m_actualFPS(targetFPS), m_gpuFrameTimeMs(0.0f) {
    
    glGenQueries(1, &m_gpuQueryID);
    m_frameStartTime = std::chrono::steady_clock::now();
}

TrumFaster::~TrumFaster() {
    glDeleteQueries(1, &m_gpuQueryID);
}

void TrumFaster::StartFrame() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float, std::milli> fullFrameTime = now - m_frameStartTime;
    
    // 1. TRUE FPS: Measured from start of last frame to start of this frame!
    if (fullFrameTime.count() > 0.0f) {
        float instantFPS = 1000.0f / fullFrameTime.count();
        // Ignore crazy startup spikes
        if (instantFPS < 1000.0f) {
            m_actualFPS = (m_actualFPS * 0.95f) + (instantFPS * 0.05f);
        }
    }
    
    // 2. Restart the clock for the NEXT frame math
    m_frameStartTime = now; 

    // 3. Start the GPU Hardware Stopwatch
    glBeginQuery(GL_TIME_ELAPSED, m_gpuQueryID);
}

void TrumFaster::EndFrame() {
    // 1. Stop the GPU Stopwatch
    glEndQuery(GL_TIME_ELAPSED);
    
    // 2. NON-BLOCKING GPU QUERY!
    GLuint available = 0;
    glGetQueryObjectuiv(m_gpuQueryID, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        GLuint64 gpuTimeNs = 0;
        glGetQueryObjectui64v(m_gpuQueryID, GL_QUERY_RESULT, &gpuTimeNs);
        m_gpuFrameTimeMs = static_cast<float>(gpuTimeNs) / 1000000.0f;
    }

    // ==========================================
    // 3. THE HYBRID THROTTLE (THE 60 FPS LOCK FIX)
    // ==========================================
    // macOS V-Sync is notoriously buggy and often ignores glfwSwapInterval.
    // If V-Sync fails, we MUST manually force the CPU to sleep to hit 60 FPS!
    
    auto workEndTime = std::chrono::steady_clock::now();
    std::chrono::duration<float, std::milli> timeSinceStart = workEndTime - m_frameStartTime;
    
    float targetMs = 1000.0f / static_cast<float>(m_targetFPS); // 16.66ms for 60FPS

    // IF THE frame finished faster than 16.66ms (meaning V-Sync failed to hold us back)
    if (timeSinceStart.count() < targetMs) {
        float sleepMs = targetMs - timeSinceStart.count();
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(sleepMs));
    }
}

float TrumFaster::GetActualFPS() const {
    return m_actualFPS;
}

float TrumFaster::GetGPUFrameTime() const {
    return m_gpuFrameTimeMs;
}

TrumFasterProfile TrumFaster::GetOptimizedProfile(int defaultBars, int visualMode) {
    TrumFasterProfile profile;
    profile.activeBars = defaultBars;
    profile.enableShadows = true;
    profile.lerpAttackSpeed = 0.92f;

    // PANIC MODE logic.
    if (m_gpuFrameTimeMs > 15.0f || m_actualFPS < 55.0f) {
        profile.activeBars = std::max(16, defaultBars / 2);
        profile.enableShadows = false;
        profile.lerpAttackSpeed = 0.85f;
    }

    return profile;
}