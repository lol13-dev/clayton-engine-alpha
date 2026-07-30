#ifdef __APPLE__
#include "MacAudioSafety.h"
#import <CoreAudio/CoreAudio.h>
#import <Foundation/Foundation.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>

// THE GLOBAL atomic flag that Engine.cpp WILL check every frame.
std::atomic<bool> g_headphoneDetected{false};

// NEW FEATURE FOR THE CALLBACK BEFORE.
std::string GetCurrentAudioDeviceName() {
    @autoreleasepool {
        AudioObjectID deviceID = 0;
        UInt32 size = sizeof(AudioObjectID);
        AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, &deviceID) != noErr) return "";

        CFStringRef deviceName = NULL;
        size = sizeof(CFStringRef);
        AudioObjectPropertyAddress nameAddress = { kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

        if (AudioObjectGetPropertyData(deviceID, &nameAddress, 0, NULL, &size, &deviceName) == noErr && deviceName) {
            NSString* name = (__bridge NSString*)deviceName;
            std::string result = [name UTF8String];
            CFRelease(deviceName); // PREVENT MEMORY LEAKS.
            return result;
        }
        return "";
    }
}

// THE INITIALIZER, NEW: REPLACES buggy Apple Callbacks with a SMART POLLING THREAD.
void InitHardwareAudioListener() {
    std::cout << "[HARDWARE_SAFETY_WARNING] CoreAudio SMART POLLING activated.\n";

    // SPIN up a DETACHED background thread that CHECKS the AUDIO JACK twice a sec (500ms).
    std::thread([]() {
        std::string lastDevice = GetCurrentAudioDeviceName();

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::string currentDevice = GetCurrentAudioDeviceName();
            
            // IF the name of SPEAKER changed...
            if (!currentDevice.empty() && currentDevice != lastDevice) {
                std::cout << "[HARDWARE_SAFETY_WARNING] Audio Route changed to: " << currentDevice << "\n";

                // CONVERT device name to lowercase so I can search IT EASILY.
                std::string lowerName = currentDevice;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c){ return std::tolower(c); });

                // ONLY TRIGGER the VOLUME DROP if the new DEVICE is a PERSONAL listening DEVICE.
                if (lowerName.find("headphone") != std::string::npos || lowerName.find("airpods") != std::string::npos || lowerName.find("earpods") != std::string::npos || lowerName.find("headset") != std::string::npos) {
                    g_headphoneDetected = true; // [NOTE] TRIGGER the C++(17) ENGINE FAILSAFE.
                }

                lastDevice = currentDevice; // <- UPDATE the MEMORY.
            }
        }
    }).detach();
}
#endif // __APPLE__