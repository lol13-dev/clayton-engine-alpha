// main.cpp

// Include my engine header file
#include "core/Engine.h"

int main() {
    // Create an Engine objects.
    Engine engine;

    // START THE ENGINE.
    engine.Initialize();

    // RUN the engine.
    engine.Run();

    // SHUTDOWN the engine.
    engine.Shutdown();

    // return 0; = SUCCESSFUL PROGRAM EXIT.
    return 0;
}