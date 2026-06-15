// Engine.h

// PREVENT duplicate includes.
#pragma once


// Engine class DECLARATION.
class Engine {
public:

    // CONSTRUCTOR.
    Engine();

    // DESTRUCTOR.
    ~Engine();

    // Engine functions.
    void Initialize();

    void Run();

    void Shutdown();
};