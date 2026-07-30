#ifdef __APPLE__
#include "MacMediaCenter.h"
#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>
#import <AppKit/AppKit.h> // Required for NSApp

// Define the global atomic flags
std::atomic<bool> g_mediaPlayPauseToggle{false};
std::atomic<bool> g_mediaNextTrack{false};
std::atomic<bool> g_mediaPrevTrack{false};
std::atomic<bool> g_mediaSeekRequested{false};
std::atomic<float> g_mediaSeekPosition{0.0f};

void InitMediaCenter() {
    @autoreleasepool {
        MPRemoteCommandCenter *commandCenter = [MPRemoteCommandCenter sharedCommandCenter];

        [commandCenter.playCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull event) {
            g_mediaPlayPauseToggle = true;
            return MPRemoteCommandHandlerStatusSuccess;
        }];

        // NEW: LISTEN for the user dragging the slider in the Mac Control Center.
        [commandCenter.changePlaybackPositionCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull event) {
            MPChangePlaybackPositionCommandEvent *positionEvent = (MPChangePlaybackPositionCommandEvent *)event;
            g_mediaSeekPosition = (float)positionEvent.positionTime;
            g_mediaSeekRequested = true;
            return MPRemoteCommandHandlerStatusSuccess;
        }];

        [commandCenter.pauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull event) {
            g_mediaPlayPauseToggle = true;
            return MPRemoteCommandHandlerStatusSuccess;
        }];

        [commandCenter.nextTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull event) {
            g_mediaNextTrack = true;
            return MPRemoteCommandHandlerStatusSuccess;
        }];

        [commandCenter.previousTrackCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent * _Nonnull event) {
            g_mediaPrevTrack = true;
            return MPRemoteCommandHandlerStatusSuccess;
        }];
    }
}

void UpdateMediaCenter(const std::string& title, float duration, float currentTime, bool isPlaying, float volume) {
    @autoreleasepool {
        // Convert C++ strings to Apple NSStrings
        NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
        
        // UX UPGRADE: Hijack the Artist field to show the BoostMax status in the Control Center!
        NSString *nsArtist;
        int displayVolume = (int)(volume * 100);
        if (volume > 1.0f) {
            nsArtist = [NSString stringWithFormat:@"Spevio | ⚠ BoostMax: %d%%", displayVolume];
        } else {
            nsArtist = [NSString stringWithFormat:@"Spevio | Vol: %d%%", displayVolume];
        }
        
        // Build the metadata dictionary
        NSMutableDictionary *songInfo = [[NSMutableDictionary alloc] init];
        
        [songInfo setObject:nsTitle forKey:MPMediaItemPropertyTitle];
        [songInfo setObject:nsArtist forKey:MPMediaItemPropertyArtist]; 
        
        [songInfo setObject:[NSNumber numberWithFloat:duration] forKey:MPMediaItemPropertyPlaybackDuration];
        [songInfo setObject:[NSNumber numberWithFloat:currentTime] forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
        [songInfo setObject:[NSNumber numberWithFloat:(isPlaying ? 1.0f : 0.0f)] forKey:MPNowPlayingInfoPropertyPlaybackRate];

        // Push it to the macOS Control Center
        MPNowPlayingInfoCenter *center = [MPNowPlayingInfoCenter defaultCenter];
        [center setNowPlayingInfo:songInfo];

        // CRITICAL FIX: macOS 11+ requires this EXACT property to force the widget to display our app!
        if (@available(macOS 10.12.2, *)) {
            center.playbackState = isPlaying ? MPNowPlayingPlaybackStatePlaying : MPNowPlayingPlaybackStatePaused;
        }
    }
}
#endif