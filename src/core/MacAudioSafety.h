#pragma once
#include <atomic>

#ifdef __APPLE__
// THREAD-SAFE flag that my background MAC motherboard listener FLIPS when hardware changes.
extern std::atomic<bool> g_headphoneDetected;

// THE BRIDGE FUNCTION.
void InitHardwareAudioListener();
#endif