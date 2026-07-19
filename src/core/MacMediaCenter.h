#pragma once
#include <string>
#include <atomic>

#ifdef __APPLE__

// NEW: ATOMIC FLAGS that let the macOS background thread safely tell our C++ engine that a button was clicked in the Control Center.
extern std::atomic<bool> g_mediaPlayPauseToggle;
extern std::atomic<bool> g_mediaNextTrack;
extern std::atomic<bool> g_mediaPrevTrack;

// THE BRIDGE FUNCTIONS.
void InitMediaCenter();
void UpdateMediaCenter(const std::string& title, float duration, float currentTime, bool isPlaying, float volume);

#endif  // __APPLE__