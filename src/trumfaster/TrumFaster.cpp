#include "TrumFaster.h"
#include <iostream>
#include <algorithm> // For std::max
#include <thread>    // For std::this_thread::sleep_for
#include <GLFW/glfw3.h> // For raw OpenGL calls.

TrumFaster::TrumFaster(int targetFPS) : m_targetFPS(targetFPS), m_actualFPS(targetFPS), m_gpuFrameTimeMs(0.0f) {
    
    // EXACT decimal math for any refresh rate (e.g., 144Hz = 6.944ms)
    m_targetFrameTime = std::chrono::duration<float, std::milli>(1000.0f / m_targetFPS);
    glGenQueries(1, &m_gpuQueryID);

    m_frameStartTime = std::chrono::steady_clock::now();
}

TrumFaster::~TrumFaster() {
    glDeleteQueries(1, &m_gpuQueryID);
}

void TrumFaster::StartFrame() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float, std::milli> fullFrameTime = now - m_frameStartTime;
    
    // Step 1: MEASURED from start of last frame to start of this frame!
    if (fullFrameTime.count() > 0.0f) {
        float instantFPS = 1000.0f / fullFrameTime.count();
        // IGNORE crazy startup spikes
        if (instantFPS < 1000.0f) {
            m_actualFPS = (m_actualFPS * 0.95f) + (instantFPS * 0.05f);
        }
    }
    
    // Step 2: RESTART the clock for the NEXT frame math
    m_frameStartTime = now; 

    // Step 3: READ THE GPU DATA (SAFELY grabs the previous frame's render time)
    GLuint available = 0;
    glGetQueryObjectuiv(m_gpuQueryID, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        GLuint64 gpuTimeNs = 0;
        glGetQueryObjectui64v(m_gpuQueryID, GL_QUERY_RESULT, &gpuTimeNs);
        m_gpuFrameTimeMs = static_cast<float>(gpuTimeNs) / 1000000.0f;
    }

    // Step 4: START the GPU Hardware Stopwatch
    glBeginQuery(GL_TIME_ELAPSED, m_gpuQueryID);
}

void TrumFaster::EndFrame() {
    glEndQuery(GL_TIME_ELAPSED);

    auto frameEndTime = std::chrono::steady_clock::now();
    std::chrono::duration<float, std::milli> workTime = frameEndTime - m_frameStartTime;

    // FEATURE Adaptive Sleep: WORKS flawlessly for 60Hz, 144Hz, 360Hz, or 1000Hz.
    if (workTime < m_targetFrameTime && m_gpuFrameTimeMs < m_targetFrameTime.count()) {
        std::chrono::duration<float, std::milli> sleepTime = m_targetFrameTime - workTime;
        std::this_thread::sleep_for(sleepTime);
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

    // Adaptive Panic_Mode: MATH automatically scales based on the monitor's native Hz.
    float panicTimeMs = 2.0f;   // 1.5ms SAFETY BUFFER. I NEED TO CHANGE FIRST
    float panicFps = m_targetFPS - 5.0f;                    // DROP 5 FRAMES below TARGET = PANIC.

    // PANIC MODE logic.
    if (m_gpuFrameTimeMs > panicTimeMs || m_actualFPS < panicFps) {
        profile.activeBars = std::max(16, defaultBars / 2);
        profile.enableShadows = false;
        profile.lerpAttackSpeed = 0.85f;
    }

    return profile;
}