#ifdef __APPLE__
#include "MacAudioSafety.h"
#import <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>
#include <iostream>

// THE GLOBAL atomic flag that Engine.cpp WILL check every frame.
std::atomic<bool> g_headphoneDetected{false};

// THE CALLBACK: FIRED BY macOS THE EXACT millisecond hardware changes.
OSStatus AudioDeviceChangeListener(AudioObjectID inObjectID, UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses, void *inClientData) {
    // THE DEFAULT output device CHANGED. (e.g., Wired Headphones or AirPods or any TWS connected).
    // I FLIP the safety flag to TRUE.
    g_headphoneDetected = true;
    std::cout << "[HARDWARE_SAFETY_WARNING] macOS DETECTED an Audio Route Change. Engaging Volume FAILSAFE.\n";

    return noErr;
}

// THE INITIALIZER.
void InitHardwareAudioListener() {
    @autoreleasepool {
        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };

        // COMMAND the macOS to CALL my FUNCTION whenever the audio jack/bluetooth changes.
        AudioObjectAddPropertyListener(kAudioObjectSystemObject, &address, AudioDeviceChangeListener, NULL);
        std::cout << "[HARDWARE_SAFETY_WARNING] CoreAudio Hot-plug Listener ACTIVATED.";

    }
}
#endif // __APPLE__