#ifdef __APPLE__
#include "MacMenuBar.h"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

// =======================================================
// 1. THE VIEW
// =======================================================
@interface SpectrumMenuBarView : NSView
@property (nonatomic, assign) std::vector<float> heights;
@end

@implementation SpectrumMenuBarView
- (void)drawRect:(NSRect)dirtyRect {
    if (self.heights.empty()) return;

    size_t count = self.heights.size();
    // We downsample to 32 bars so it looks clean and fits perfectly in 100 pixels
    size_t displayCount = 32; 
    size_t step = count / displayCount;
    if (step < 1) step = 1;

    float width = self.bounds.size.width;
    float height = self.bounds.size.height;
    float cy = height / 2.0f; // The absolute vertical center!
    
    float barSpacing = width / (float)displayCount;
    float barWidth = barSpacing * 0.6f; // Leave a little gap between pills
    
    for (size_t i = 0; i < displayCount; i++) {
        size_t dataIndex = i * step;
        if (dataIndex >= count) break;
        
        float val = self.heights[dataIndex];
        // Scale it down slightly so it doesn't clip the top/bottom of the menu bar
        float h = val * (height * 0.8f);
        if (h < 3.0f) h = 3.0f; // Give it a minimum 3-pixel dot when silent
        
        // Beautiful Cyan to Purple spread -> TO WHITE MINIMALISM.
        // float hue = 0.5f + ((float)i / displayCount) * 0.35f;
        // [[NSColor colorWithHue:hue saturation:0.9f brightness:1.0f alpha:1.0f] setFill];
        // DISABLED IT FIRST.
        [[NSColor whiteColor] setFill];
        
        // DYNAMIC ISLAND MATH: Draw from the vertical center symmetrically
        float xPos = i * barSpacing + (barSpacing - barWidth) / 2.0f;
        float yPos = cy - (h / 2.0f);
        
        NSRect barRect = NSMakeRect(xPos, yPos, barWidth, h);
        
        // Corner radius is EXACTLY half the width, creating a perfect pill/capsule!
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:barRect xRadius:(barWidth/2.0f) yRadius:(barWidth/2.0f)];
        [path fill];
    }
}
@end

// =======================================================
// 2. THE C++ BRIDGES
// =======================================================
static NSStatusItem* g_statusItem = nil;
static SpectrumMenuBarView* g_menuBarView = nil;

bool InitMenuBar() {
    @autoreleasepool {
        if (!g_statusItem) {
            // FIX: We MUST call `retain` so Apple's memory manager doesn't instantly delete it!
            g_statusItem = [[[NSStatusBar systemStatusBar] statusItemWithLength:100.0] retain];
            
            NSMenu *menu = [[NSMenu alloc] init];
            [menu addItemWithTitle:@"Spevio Audio Engine" action:nil keyEquivalent:@""];
            [menu addItem:[NSMenuItem separatorItem]];
            [menu addItemWithTitle:@"Quit Spevio" action:@selector(terminate:) keyEquivalent:@"q"];
            g_statusItem.menu = menu;

            NSStatusBarButton *button = g_statusItem.button;
            if (button) {
                // macOS Big Sur+ Menu Bars are 24 pixels tall. 
                g_menuBarView = [[SpectrumMenuBarView alloc] initWithFrame:NSMakeRect(0, 0, 100, 24)];
                [button addSubview:g_menuBarView];
                g_menuBarView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
            }
            return true;
        }
        return false;
    }
}

void UpdateMenuBar(const std::vector<float>& frequencies) {
    @autoreleasepool {
        if (g_menuBarView) {
            // 1. Pass the raw physics data to the view
            g_menuBarView.heights = frequencies;
            
            // 2. MAIN THREAD FIX (Grand Central Dispatch)
            // We politely ask the native macOS UI thread to draw the frame, preventing starvation!
            dispatch_async(dispatch_get_main_queue(), ^{
                [g_menuBarView setNeedsDisplay:YES];
            });
        }
    }
}
#endif