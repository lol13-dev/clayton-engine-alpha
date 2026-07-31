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

// THE INITIALIZER, NEW: REPLACES buggy Apple Callbacks with a SMART POLLING THREAD.
void InitHardwareAudioListener() {
    @autoreleasepool {
        dispatch_queue_t audioQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);

        // Step1: GET the current default Audio Chip ID.
        AudioObjectID defaultDeviceID = kAudioObjectUnknown;
        UInt32 dataSize = sizeof(defaultDeviceID);
        AudioObjectPropertyAddress defaultDeviceAddress = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultDeviceAddress, 0, NULL, &dataSize, &defaultDeviceID);

        // Step2: THE 3.5mm Headphone JACK LISTENER (TRACKS internal audio routing).
        AudioObjectPropertyAddress dataSourceAddress = {
            kAudioDevicePropertyDataSource,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMain
        };

        AudioObjectAddPropertyListenerBlock(defaultDeviceID, &dataSourceAddress, audioQueue, ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses) {
            g_headphoneDetected = true; // FLIP THE C++ FLAG.
            std::cout << "[HARDWARE_SAFETY_WARNING] 3.5mm Audio Jack Route Changed. Engaging Volume FAILSAFE.\n";
        });

        // Step3: THE Bluetooth & USB Listener (TRACKS entirely NEW DEVICES like AirPods).
        AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &defaultDeviceAddress, audioQueue, ^(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses) {
            g_headphoneDetected = true; // FLIP THE C++ FLAG
            std::cout << "[HARDWARE_SAFETY_WARNING] Bluetooth/USB Device Route Changed. Engaging Volume FAILSAFE.\n";
        });

        std::cout << "CoreAudio Dual-Listener ACTIVATED (Tracking 3.5mm Jack & Bluetooth).\n";
    }
}
#endif // __APPLE__