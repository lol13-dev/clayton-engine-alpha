// Engine.h

// PREVENT duplicate includes.
#pragma once

// INCLUDES for the DUAL RENDERING pipelines.
#include "../renderer/SpectrumRenderer.h"
#include "../renderer/SpectrumRenderer3D.h"

// DEFINE the TWO VISUALIZER states.
enum class VisualizerMode {
    CLASSIC_2D,
    DISCO_3D
};

// Engine class DECLARATION.
class Engine {
public:

    // CONSTRUCTOR.
    Engine();

    // DESTRUCTOR.=
    ~Engine();

    // Engine functions.
    void Initialize();

    void Run();

    void Shutdown();

    // UI CONTROLS: FLIPS the switch BETWEEN 2D and 3D.
    void ToggleVisualizerMode();

private:

    // DUAL-ENGINE INSTANCES: STORES both renderers in MEMORY.
    SpectrumRenderer m_Renderer2D;
    SpectrumRenderer3D m_Renderer3D;
    
    // STATE TRACKER: Defaults to the new 3D mode on boot.
    VisualizerMode m_CurrentMode = VisualizerMode::DISCO_3D;
};