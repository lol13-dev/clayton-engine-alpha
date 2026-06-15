// UPGRADED VERSION.
// INSTEAD of relying on a pre-built library like kissfft or Apple's Accelerate framework, we are going to write a custom   Cooley-Tukey Radix-2 FFT directly in pure C++.

#include "FFT.h"
#include <cmath>    
#include <algorithm>

// DEFINE PI for my sine/cosine angle calculations.
const float PI = 3.14159265358979323846f;

FFT::FFT() {}

// ==============================================
// THE COOLEY-TUKEY ALGORITHM (Hard Math Core)
// ==============================================
void FFT::ComputeFFT(std::vector<std::complex<float>>& data) {
    size_t n = data.size();
    if (n <= 1) return; // Base case: a single element is already "transformed".

    // STEP 1. Bit-Reversal Permutation.
    // This perfectly reorganizes the ARRAY so the math can be done in-place.
    size_t j = 0;
    for (size_t i = 1; i < n; i++) {
        size_t bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    // STEP 2. The Butterfly Operations.
    // CALCULATES the frequency bins exponentially (Radix-2).
    for (size_t len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * PI / len;
        // CALCULATE the twiddle factor for this length.
        // This is a complex number (cos(angle) + i(angle)) that we use to rotate the data in the complex domain.
        std::complex<float> wlen(std::cos(angle), std::sin(angle));

        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; k++) {
                std::complex<float> u = data[i + k];
                std::complex<float> v = data[i + k + len / 2] * w;

                // The "butterfly" operation: combining the even and odd parts.
                data[i + k] = u + v; // Even part + Odd part.
                data[i + k + len / 2] = u - v; // Even part
                w *= wlen; // Update the twiddle factor for the next iteration.
            }
        }
    }
}

// ==============================================
// THE PUBLIC PROCESSOR
// ==============================================
std::vector<float> FFT::Process(const std::vector<float>& audioSamples) {
    size_t n = audioSamples.size();

    // 1. CONVERT RAW float AUDIO INTO COMPLEX NUMBERS (Real part = audio, Imaginary = 0).
    std::vector<std::complex<float>> complexData(n);
    for (size_t i = 0; i < n; i++) {
        complexData[i] = std::complex<float>(audioSamples[i], 0.0f);
    }

    // 2. EXECUTE the FAST FOURIER TRANSFORM on the complex data.
    ComputeFFT(complexData);

    // 3. EXTRACT the MAGNITUDES (The Nyquist frequencies) from the complex results.
    // BECAUSE real audio data is symmetrical in the FREQUENCY domain,
    // the second half of the FFT output is just a mirror of the first half.
    // Therefore, we cut it exactly in half! (1024 samples -> 512 frequencies)
    size_t half_n = n / 2;
    std::vector<float> magnitudes(half_n);

    for (size_t i = 0; i < half_n; i++) {
        // CALCULATE the absolute length of the complex VECTOR.
        // I divide by 'n' to NORMALIZE the DATA so it doesn't EXPLODE OFF SCREEN.
        magnitudes[i] = std::abs(complexData[i]); // Get the magnitude of each frequency bin.
    }

    // THE FINAL CODE.
    return magnitudes;
}