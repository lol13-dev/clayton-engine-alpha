// SpectrumSimulator.h

// Prevents this file from being included multiple times
#pragma once

// Needed because I'll use std::vector
#include <vector>

/*
    SpectrumSimulator class
    Responsible for generating fake spectrum data
*/
class SpectrumSimulator
{
public:

    // CONSTRUCTOR
    SpectrumSimulator();

    // GENERATES FAKE SPECTRUM VALUES.
    std::vector<int> GenerateSpectrum();

private:
    std::vector<int> spectrum;
};
