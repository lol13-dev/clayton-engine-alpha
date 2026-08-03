#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace BoostMaxDSP {

    // ==========================================
    // NEW FEATURES FOR BoostMax: Fast Activation Function (Neural Math)
    // ==========================================
    // A STANDARD Neural Network 'tanh' function is too slow for 44.1kHz audio.
    // THIS IS a high-performance C++ APPROXIMATION of tanh(x) THAT runs in microsec.
    inline float FastTanh(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // ==========================================
    // Step1: C++ Neural Network-based Wave Shaper (Micro-MLP)
    // NEW: Parallel Neural Limiter (Dry/Wet Blending)
    // ==========================================
    inline float NeuralSaturation(float sample, float drive) {
        // Phase 1: CALCULATE how EXTREME the slider is PUSHED (0.0 to 1.5).
        float overdrive = drive - 1.0f;
        if (overdrive < 0.0f) overdrive = 0.0f;

        // Phase 2: PARALLEL PROCESSING: THE "Dry" CLEAN SIGNAL
        // I let THE CLEAN SIGNAL naturally boost just a little bit.
        float cleanSignal = sample * (1.0f + (overdrive * 0.2f));

        // Phase 3: PARALLEL PROCESSING: THE WET "AI" Signal.
        float drivenSignal = sample * drive;
        float h1 = FastTanh(drivenSignal);
        float h2 = FastTanh(drivenSignal * 1.5f);
        float aiSignal = (h1 * 0.7f) + (h2 * 0.3f);

        // Phase 4: THE CROSSFADER.
        // AS VOLUME GOES UP, I blend in more AI, but NEVER EXCEED 80% AI.
        // THIS GUARANTEES 20% of THE ORIGINAL track's clarity always SURVIVES.
        float mixRatio = overdrive / 1.5f;
        if (mixRatio > 0.8f) mixRatio = 0.8f;

        // BLEND the 2 lanes back TOGETHER.
        float output = (cleanSignal * (1.0f - mixRatio)) + (aiSignal * mixRatio);

        // FINAL Phase: FINAL HARDWARE CEILING
        // A last resort SAFETY net to PROTECT the MacBook speakers.
        return FastTanh(output);
    }

    // ==========================================
    // Step2: Mid/Side MATRIX + Neural Processing.
    // ==========================================
    inline void ProcessAudioBuffer(float* audioBuffer, size_t sampleCount, float currentVolume, int channels = 1) {

        // IF volume is safe, BYPASS the heavy Neural Network to SAVE laptop battery.
        if (currentVolume <= 1.0f) {
            for (size_t i = 0; i < sampleCount; i++) {
                audioBuffer[i] *= currentVolume;
            }
            return;
        }

        if (channels == 2) {
            // BoostMax is ACTIVE: ENGAGE Linear Algebra & Neural Network.
            for (size_t i = 0; i < sampleCount; i += 2) {
                // SAFETY check for ODD-NUMBERED buffers.
                if (i + 1 >= sampleCount) break;

                // UPDATE: GRAB the sample (DO NOT pre-multiply VOLUME HERE ANYMORE)./
                float L = audioBuffer[i];
                float R = audioBuffer[i + 1];

                // Phase A: LINEAR ALGEBRA Forward Matrix (L/R -> Mid/Side).
                float M = 0.5f * L + 0.5f * R;
                float S = 0.5f * L - 0.5f * R;

                // Phase B: SPATIAL Widen.
                S *= 1.05f;

                // Phase C: INVERSE Matrix (Mid/Side -> L/R).
                L = M + S;
                R = M - S;

                // Phase D: FEED the spatial audio into the NEURAL NETWORK.
                audioBuffer[i] = NeuralSaturation(L, currentVolume);
                audioBuffer[i + 1] = NeuralSaturation(R, currentVolume);
            }
        } else {
            // MONO PROCESSING: DIRECT Neural Saturation per-sample.
            for (size_t i = 0; i < sampleCount; i++) {
                // float sample =  * currentVolume; <- DISABLED
                audioBuffer[i] = NeuralSaturation(audioBuffer[i], currentVolume);
            }
        }
    }
}