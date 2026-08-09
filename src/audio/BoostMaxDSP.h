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

    // ==========================================
    // NEW (MULTIBAND): tiny one-pole low-pass filter, used two-in-a-row to build
    // a ~12dB/octave BASS/HIGH crossover. This is what lets BoostMax react to
    // 808s and sub-bass SEPARATELY from vocals/hats, instead of one loud bass
    // hit pulling the gain down for the whole mix ("pumping").
    // ==========================================
    struct OnePoleLP {
        float state = 0.0f;
        inline float Process(float x, float alpha) {
            state += alpha * (x - state);
            return state;
        }
    };

    // Crossover filter memory - one 2-stage cascade per channel.
    static OnePoleLP s_bassLP_L1, s_bassLP_L2;
    static OnePoleLP s_bassLP_R1, s_bassLP_R2;

    // FIX (multiband): SEPARATE gain memory per band. A loud bass hit now only
    // pulls down gain on the BASS band; the HIGH band (vocals, snare, hats) has
    // its own independent envelope and isn't dragged down with it.
    // These are LINKED across L/R (see ProcessAudioBuffer) so stereo panning
    // can't make L and R make different gain decisions.
    static float s_envelopeBass = 0.0f;
    static float s_envelopeHigh = 0.0f;
    static float s_prevPeakBass = 0.0f; // earphone-mode "velocity spike" detection, bass band
    static float s_prevPeakHigh = 0.0f; // earphone-mode "velocity spike" detection, high band

    // FIX: reset ALL gain + filter memory when starting a new track (or flipping
    // headphone/speaker mode), so leftover state from the previous track doesn't
    // color the first moments of the new one.
    inline void ResetEnvelope() {
        s_envelopeBass = 0.0f;
        s_envelopeHigh = 0.0f;
        s_prevPeakBass = 0.0f;
        s_prevPeakHigh = 0.0f;
        s_bassLP_L1.state = s_bassLP_L2.state = 0.0f;
        s_bassLP_R1.state = s_bassLP_R2.state = 0.0f;
    }

    // ==========================================
    // Per-band gain control (Trauma Safety / earphone mode).
    // Called ONCE per stereo pair per band (not once per channel), using the
    // LINKED peak, so both channels always get the identical gain decision.
    // ==========================================
    inline float UpdateEarphoneGain(float linkedPeak, float linkedVelocity, float safeVolume, float& envelope, float releaseCoeff) {
        // Math System, Calculus: INSTANT attack on jumpscares/spikes, slow RELEASE.
        if (linkedPeak > envelope || linkedVelocity > 0.3f) {
            envelope = linkedPeak;
        } else {
            envelope = envelope * releaseCoeff;
        }
        // Dynamic Safety Fader: PREVENTS the earphone from PASSING A 0.7 LIMIT.
        float dynamicGain = 1.0f;
        if (envelope > 0.7f) {
            dynamicGain = safeVolume / envelope;
        }
        return dynamicGain;
    }

    // NOTE: earphone-mode saturation is applied ONCE on the recombined signal
    // (see ApplyFinalSaturation below) - a band's gain is computed here, but the
    // actual FastTanh/clamp happens after L/R are put back together.

    // ==========================================
    // Per-band gain control (BoostMax Overdrive / speaker mode).
    // Same "called once per band per pair, using the linked peak" pattern as above.
    // IMPORTANT: this ONLY computes a linear gain multiplier per band. It does NOT
    // saturate. An earlier version ran FastTanh independently on the bass band and
    // the high band and then summed the two already-saturated results - that
    // double-compresses the signal (tanh(a)+tanh(b) != tanh(a+b)) and made normal,
    // bass-silent playback measurably quieter than the original single-band code,
    // which is the opposite of what we want. So now: gain per band, saturate once.
    // ==========================================
    inline float UpdateSpeakerGain(float linkedDrivenPeak, float safeVolume,
                                    float& envelope, float attackCoeff, float releaseCoeff) {
        // CALCULUS: SMOOTH tracking to RIDE the volume fader.
        if (linkedDrivenPeak > envelope) {
            envelope = attackCoeff * linkedDrivenPeak + (1.0f - attackCoeff) * envelope;
        } else {
            envelope = releaseCoeff * linkedDrivenPeak + (1.0f - releaseCoeff) * envelope;
        }
        float dynamicGain = safeVolume;
        if ((envelope * safeVolume) > 0.8f) {
            dynamicGain = safeVolume / (1.0f + ((envelope * safeVolume) - 0.8f));
        }
        return dynamicGain;
    }

    // Harmonic exciter + final hard-clamp, applied ONCE to the already gain-reduced,
    // recombined (bass*bassGain + high*highGain) signal - identical structure to the
    // original single-band formula, so normal (non-bass-heavy) playback is unaffected.
    inline float ApplyFinalSaturation(float gainedAndDrivenSample, float overdrive) {
        float cleanSignal  = gainedAndDrivenSample * (1.0f + (overdrive * 0.15f));
        float drivenSignal = gainedAndDrivenSample * (1.0f + overdrive);

        float h1 = FastTanh(drivenSignal);
        float h2 = FastTanh(drivenSignal * 1.5f);
        float aiSignal = (h1 * 0.7f) + (h2 * 0.3f);

        float mixRatio = std::min(overdrive, 0.5f); // 50% CAP.
        float output = (cleanSignal * (1.0f - mixRatio)) + (aiSignal * mixRatio);

        return std::clamp(FastTanh(output), -1.0f, 1.0f);
    }

    // ==========================================
    // Stage1: Neural Saturation (HARMONIC EXCITER).
    // ==========================================
    inline void ProcessAudioBuffer(float* audioBuffer, size_t sampleCount, float currentVolume, bool isHeadphoneMode, int channels = 2, float sampleRate = 44100.0f) {

        // Phase 1: HARD cap the entire engine at a maximum of 2.5x volume.
        float safeVolume = std::min(currentVolume, 2.5f);
        float overdrive = safeVolume - 1.0f;
        if (overdrive < 0.0f) overdrive = 0.0f;

        // FIX: sample-rate-independent time constants (same fix as before - untouched by
        // the multiband change). Rescales the original 44.1kHz-tuned coefficients so
        // attack/release speed in milliseconds stays the same at any sample rate.
        float srScale = 44100.0f / sampleRate;
        float earphoneRelease = std::pow(0.9995f, srScale);
        float speakerAttack   = 1.0f - std::pow(1.0f - 0.05f,   srScale);
        float speakerRelease  = 1.0f - std::pow(1.0f - 0.0005f, srScale);

        // NEW (MULTIBAND): crossover cutoff between the "bass" and "everything else" bands.
        // TUNABLE BY EAR:
        //  - Raise toward 250-300Hz if dubstep "wobble bass" growl (which has energy
        //    well above pure sub-bass) still ducks the vocal band.
        //  - Lower toward 100-120Hz if kicks/snare body get pulled into the bass band
        //    and sound over-limited.
        const float kBassCrossoverHz = 180.0f;

        // One-pole alpha for the given cutoff at the ACTUAL sample rate -
        // this is why sampleRate had to be threaded through in the first place.
        float lpAlpha = 1.0f - std::exp(-2.0f * 3.14159265358979f * kBassCrossoverHz / sampleRate);

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

                // NEW (MULTIBAND): split each channel into bass + everything-else.
                // Complementary split (high = original - low) guarantees bass+high
                // sums back to exactly the original signal, so nothing is lost.
                float bassL = s_bassLP_L2.Process(s_bassLP_L1.Process(L, lpAlpha), lpAlpha);
                float bassR = s_bassLP_R2.Process(s_bassLP_R1.Process(R, lpAlpha), lpAlpha);
                float highL = L - bassL;
                float highR = R - bassR;

                // Linked (shared L/R) peak per band - same anti-image-drift fix as before,
                // just applied per band now instead of to the whole signal at once.
                float linkedBassPeak = std::max(std::abs(bassL), std::abs(bassR));
                float linkedHighPeak = std::max(std::abs(highL), std::abs(highR));

                if (isHeadphoneMode) {
                    float linkedBassVelocity = std::abs(linkedBassPeak - s_prevPeakBass);
                    float linkedHighVelocity = std::abs(linkedHighPeak - s_prevPeakHigh);
                    s_prevPeakBass = linkedBassPeak;
                    s_prevPeakHigh = linkedHighPeak;

                    float bassGain = UpdateEarphoneGain(linkedBassPeak, linkedBassVelocity, safeVolume, s_envelopeBass, earphoneRelease);
                    float highGain = UpdateEarphoneGain(linkedHighPeak, linkedHighVelocity, safeVolume, s_envelopeHigh, earphoneRelease);

                    // Gain per band, THEN recombine, THEN saturate ONCE (see note above).
                    float drivenGainedL = (bassL * bassGain + highL * highGain) * safeVolume;
                    float drivenGainedR = (bassR * bassGain + highR * highGain) * safeVolume;

                    audioBuffer[i]     = std::clamp(FastTanh(drivenGainedL), -1.0f, 1.0f);
                    audioBuffer[i + 1] = std::clamp(FastTanh(drivenGainedR), -1.0f, 1.0f);
                } else {
                    float linkedBassDrivenPeak = linkedBassPeak * safeVolume;
                    float linkedHighDrivenPeak = linkedHighPeak * safeVolume;

                    float bassGain = UpdateSpeakerGain(linkedBassDrivenPeak, safeVolume, s_envelopeBass, speakerAttack, speakerRelease);
                    float highGain = UpdateSpeakerGain(linkedHighDrivenPeak, safeVolume, s_envelopeHigh, speakerAttack, speakerRelease);

                    // Gain per band (this is what stops the bass hit from ducking vocals),
                    // THEN recombine, THEN run the ORIGINAL single harmonic-exciter +
                    // saturation pipeline once - same tonal character as before.
                    float drivenGainedL = (bassL * bassGain + highL * highGain) * safeVolume;
                    float drivenGainedR = (bassR * bassGain + highR * highGain) * safeVolume;

                    audioBuffer[i]     = ApplyFinalSaturation(drivenGainedL, overdrive);
                    audioBuffer[i + 1] = ApplyFinalSaturation(drivenGainedR, overdrive);
                }
            }
        } else if (channels == 1) {
            // Mono: same protected multiband path, single channel.
            for (size_t i = 0; i < sampleCount; i++) {
                float sample = audioBuffer[i];

                float bass = s_bassLP_L2.Process(s_bassLP_L1.Process(sample, lpAlpha), lpAlpha);
                float high = sample - bass;

                if (isHeadphoneMode) {
                    float bassPeak = std::abs(bass);
                    float highPeak = std::abs(high);
                    float bassVel = std::abs(bassPeak - s_prevPeakBass);
                    float highVel = std::abs(highPeak - s_prevPeakHigh);
                    s_prevPeakBass = bassPeak;
                    s_prevPeakHigh = highPeak;

                    float bassGain = UpdateEarphoneGain(bassPeak, bassVel, safeVolume, s_envelopeBass, earphoneRelease);
                    float highGain = UpdateEarphoneGain(highPeak, highVel, safeVolume, s_envelopeHigh, earphoneRelease);

                    float drivenGained = (bass * bassGain + high * highGain) * safeVolume;
                    audioBuffer[i] = std::clamp(FastTanh(drivenGained), -1.0f, 1.0f);
                } else {
                    float bassDrivenPeak = std::abs(bass) * safeVolume;
                    float highDrivenPeak = std::abs(high) * safeVolume;

                    float bassGain = UpdateSpeakerGain(bassDrivenPeak, safeVolume, s_envelopeBass, speakerAttack, speakerRelease);
                    float highGain = UpdateSpeakerGain(highDrivenPeak, safeVolume, s_envelopeHigh, speakerAttack, speakerRelease);

                    float drivenGained = (bass * bassGain + high * highGain) * safeVolume;
                    audioBuffer[i] = ApplyFinalSaturation(drivenGained, overdrive);
                }
            }
        }
    }
}