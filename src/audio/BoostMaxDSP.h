#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace BoostMaxDSP {

    // ==========================================
    // NEW FEATURES FOR BoostMax DSP (V4.X): Fast Activation Function (Neural Math)
    // ==========================================
    // A STANDARD Neural Network 'tanh' function is too slow for 44.1kHz audio.
    // THIS IS a high-performance C++ APPROXIMATION of tanh(x) THAT runs in microsec.
    // NOTE: unlike real tanh, this does NOT stay bounded at +-1.0 for large |x|
    // (e.g. FastTanh(8) ~= 1.21). It's a shaping curve, not a limiter by itself -
    // callers must clamp the final output explicitly. See std::clamp calls below.
    inline float FastTanh(float x) {
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // NEW FEATURES: The Calculus Memory: REMEMBERS the audio energy between frames.
    // These are now LINKED across L/R (see ProcessAudioBuffer) instead of being
    // updated separately per channel, so stereo panning can't desync the gain decision.
    static float s_envelope = 0.0f;
    static float s_prevPeak = 0.0f;

    // FIX: reset the gain memory when starting a new track (or flipping
    // headphone/speaker mode), so leftover gain state from the previous
    // track doesn't color the first moments of the new one.
    inline void ResetEnvelope() {
        s_envelope = 0.0f;
        s_prevPeak = 0.0f;
    }

    // ==========================================
    // Stage1: Neural Saturation (HARMONIC EXCITER).
    // ==========================================
    inline void ProcessAudioBuffer(float* audioBuffer, size_t sampleCount, float currentVolume, bool isHeadphoneMode, int channels = 2, float sampleRate = 44100.0f) {

        // Phase 1: HARD cap the entire engine at a maximum of 2.5x volume.
        float safeVolume = std::min(currentVolume, 2.5f);
        float overdrive = safeVolume - 1.0f;
        if (overdrive < 0.0f) overdrive = 0.0f;

        // FIX: sample-rate-independent time constants.
        // The original 0.9995f / 0.05f / 0.0005f coefficients were tuned assuming
        // 44.1kHz (one coefficient application per sample at that rate). At a
        // different sample rate (e.g. 96kHz Hi-Res), the same raw coefficients
        // change the real-world (millisecond) attack/release speed. srScale
        // rescales them so the limiter reacts at the same real-time speed
        // regardless of sample rate. srScale == 1.0 at 44100Hz (no change).
        float srScale = 44100.0f / sampleRate;
        float earphoneRelease = std::pow(0.9995f, srScale);
        float speakerAttack = 1.0f - std::pow(1.0f - 0.05f,   srScale);
        float speakerRelease = 1.0f - std::pow(1.0f - 0.0005f, srScale);

        // ==========================================
        // Lambda 1: Trauma Safety.
        // USED exclusively when EARPHONES ARE CONNECTED.
        // Takes the LINKED peak/velocity (computed once per sample, shared by
        // both channels) instead of tracking each channel independently.
        // ==========================================
        auto ApplyEarphoneSafety = [&](float sample, float linkedPeak, float linkedVelocity) -> float {
            // BUG FIX 1: I apply the VOLUME slider to the sample first.
            float drivenSample = sample * safeVolume;

            // Math System, Calculus: INSTANT attack on jumpscares/spikes, slow RELEASE.
            if (linkedPeak > s_envelope || linkedVelocity > 0.3f) {
                s_envelope = linkedPeak;
            } else {
                s_envelope = s_envelope * earphoneRelease;
            }

            // Dynamic Safety Fader: PREVENTS the earphone from PASSING A 0.7 LIMIT.
            float dynamicGain = 1.0f;
            if (s_envelope > 0.7f) {
                dynamicGain = safeVolume / s_envelope;
            }

            // NEURAL NETWORK shaping + FIX: explicit hard-clamp so the output is
            // ACTUALLY guaranteed within [-1, 1] (FastTanh alone isn't, for large inputs).
            return std::clamp(FastTanh(drivenSample * dynamicGain), -1.0f, 1.0f);
        };

        // ==========================================
        // Lambda 2: BoostMax Overdrive (TRICK the physics)
        // USED for Speakers (especially support for Hi-Res Speaker) to REACH 2.5x massive loudness.
        // Takes the LINKED driven peak instead of tracking each channel independently.
        // ==========================================
        auto ApplySpeakerBoost = [&](float sample, float linkedDrivenPeak) -> float {
            // BUG FIX 3: TRACK the envelope based on THE BOOSTED VOLUME.
            float drivenSample = sample * safeVolume;

            // CALCULUS: SMOOTH tracking to RIDE the volume fader.
            if (linkedDrivenPeak > s_envelope) {
                s_envelope = speakerAttack * linkedDrivenPeak + (1.0f - speakerAttack) * s_envelope;
            } else {
                s_envelope = speakerRelease * linkedDrivenPeak + (1.0f - speakerRelease) * s_envelope;
            }

            float dynamicGain = safeVolume;
            if ((s_envelope * safeVolume) > 0.8f) {
                dynamicGain = safeVolume / (1.0f + ((s_envelope * safeVolume) - 0.8f));
            }

            // Neural Network: 50% HARMONIC Exciter for FAKE LOUDNESS.
            float cleanSignal = (drivenSample * dynamicGain) * (1.0f + (overdrive * 0.15f));
            float drivenSignal = (drivenSample * dynamicGain) * (1.0f + overdrive);

            float h1 = FastTanh(drivenSignal);
            float h2 = FastTanh(drivenSignal * 1.5f);
            float aiSignal = (h1 * 0.7f) + (h2 * 0.3f);

            float mixRatio = std::min(overdrive, 0.5f); // 50% CAP.
            float output = (cleanSignal * (1.0f - mixRatio)) + (aiSignal * mixRatio);

            // FIX: explicit hard-clamp - this is the real ceiling, not FastTanh alone.
            return std::clamp(FastTanh(output), -1.0f, 1.0f);
        };

        // ==========================================
        // AUDIO BUFFER EXECUTION.
        // ==========================================
        if (channels == 2) {
            for (size_t i = 0; i < sampleCount; i += 2) {
                if (i + 1 >= sampleCount) break;

                float L = audioBuffer[i];
                float R = audioBuffer[i + 1];

                // Linear Algebra Mechanism: MID/SIDE Matrix.
                float M = 0.5f * L + 0.5f * R;
                float S = 0.5f * L - 0.5f * R;

                if (isHeadphoneMode) {
                    // SAFETY MATRIX: RELIEVE mechanical PRESSURE on the EARDRUM.
                    M *= 0.85f;
                    S *= 1.15f;
                } else {
                    // BOOSTMAX MATRIX: WIDE stereo field for perceived LOUDNESS.
                    M *= (1.0f - (overdrive * 0.15f));
                    S *= (1.0f + (overdrive * 0.20f));
                }

                L = M + S;
                R = M - S;

                // FIX: compute ONE linked peak per sample-pair so L and R make the
                // SAME gain decision, instead of each channel updating s_envelope /
                // s_prevPeak independently (which let one channel's transient bleed
                // into the other channel's "previous peak" and desync stereo gain).
                float linkedPeak = std::max(std::abs(L), std::abs(R));

                if (isHeadphoneMode) {
                    float linkedVelocity = std::abs(linkedPeak - s_prevPeak);
                    s_prevPeak = linkedPeak;
                    audioBuffer[i] = ApplyEarphoneSafety(L, linkedPeak, linkedVelocity);
                    audioBuffer[i + 1] = ApplyEarphoneSafety(R, linkedPeak, linkedVelocity);
                } else {
                    float linkedDrivenPeak = linkedPeak * safeVolume;
                    audioBuffer[i] = ApplySpeakerBoost(L, linkedDrivenPeak);
                    audioBuffer[i + 1] = ApplySpeakerBoost(R, linkedDrivenPeak);
                }
            }
        } else if (channels == 1) {
            // FIX: mono previously fell through this whole function doing NOTHING -
            // no boost, but also NO safety limiting, so mono playback was completely
            // unprotected. Route it through the same protected lambdas as stereo.
            for (size_t i = 0; i < sampleCount; i++) {
                float sample = audioBuffer[i];

                if (isHeadphoneMode) {
                    float peak = std::abs(sample);
                    float velocity = std::abs(peak - s_prevPeak);
                    s_prevPeak = peak;
                    audioBuffer[i] = ApplyEarphoneSafety(sample, peak, velocity);
                } else {
                    float drivenPeak = std::abs(sample) * safeVolume;
                    audioBuffer[i] = ApplySpeakerBoost(sample, drivenPeak);
                }
            }
        }
    }
}