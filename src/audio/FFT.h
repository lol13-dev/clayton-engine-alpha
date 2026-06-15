// UPGRADED VERSION
#pragma once

#include <vector>
#include <complex>

class FFT 
{
// This is a simple implementation of the Cooley-Tukey FFT algorithm.
public:
    // CONSTRUCTOR.
    FFT();

    // Convert AUDIO SAMPLES INTO FREQUENCY MAGNITUDES.
    std::vector<float> Process(const std::vector<float>& audioSamples);

// PRIVATE MEMBERS.
private:
    // The core RECURSIVE/ITERATIVE mathematical sorting algorithm.
    void ComputeFFT(std::vector<std::complex<float>>& data);
};