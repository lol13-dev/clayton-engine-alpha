// FIXED.
#include "SpectrumSimulator.h"
#include <cstdlib>
#include <ctime>

// CONSTRUCTOR.
SpectrumSimulator::SpectrumSimulator()
{
    // Seed random generator
    std::srand((unsigned int)std::time(nullptr));

    // Initialize 16 bars
    spectrum = {5, 5, 5, 5, 5,
                5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
}

// GENERATES FAKE SPECTRUM VALUES.
std::vector<int> SpectrumSimulator::GenerateSpectrum()
{
    // Update each bar's height
    // with a small random change
    // to create a smooth motion effect
    for (int &value : spectrum)
    {
        // Small random change for smooth motion (-2..+2)
        int change = (std::rand() % 5) - 2;
        value += change;

        // Clamp values to 1..20
        if (value < 1)
            value = 1;
        if (value > 20)
            value = 20;
    }

    return spectrum;
}