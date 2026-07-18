#ifdef __APPLE__
#include "MacTouchBar.h"
#include <Cocoa/Cocoa.h>

// EXPOSE the hidden native Mac window buried inside GLFW.
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// =======================================================
// Step 1. THE OLED VIEW (DRAWS the bars on the Touch Bar)
// =======================================================
@interface SpectrumTouchBar : NSView
@property (nonatomic, assign) std::vector<float> heights;
@end

@implementation SpectrumTouchBar
- (void)drawRect:(NSRect)dirtyRect {
    // FILL the Touch Bar background with pure OLED black
    [[NSColor blackColor] setFill];
    NSRectFill(dirtyRect);

    if (self.heights.empty()) return;

    size_t count = self.heights.size();
    float barSpacing = self.bounds.size.width / (float)count;
    float barWidth = barSpacing * 0.75f;
    float maxHeight = self.bounds.size.height;

    for (size_t i = 0; i < count; i++) {
        // GRAB the C++ Lerped height.
        float h = self.heights[i] * maxHeight;
        if (h < 2.0f) h = 2.0f; // GIVE it a 2-pixel bump even when SILENT.

        // CREATE a seamless Cyan to Purple gradient across the Touch Bar.
        float hue = 0.5f + ((float)i / count) * 0.35f;
        [[NSColor colorWithHue:hue saturation:0.9f brightness:1.0f alpha:1.0f] setFill];
        
        // DRAW the bar from the bottom of the Touch Bar upwards.
        NSRect bar = NSMakeRect(i * barSpacing, 0, barWidth, h);
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bar xRadius:2.0 yRadius:2.0];
        [path fill];
    }
}
@end

// =======================================================
// Step 2. THE TOUCH BAR MANAGER (Connects it to the Mac System)
// =======================================================
@interface TouchBarManager : NSObject <NSTouchBarDelegate>
@property (nonatomic, strong) SpectrumTouchBar *spectrumView;
@end

@implementation TouchBarManager
- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier {
    if ([identifier isEqualToString:@"com.clayton.spectrum"]) {
        NSCustomTouchBarItem *item = [[NSCustomTouchBarItem alloc]initWithIdentifier:identifier];
        // 680x30 is PERFECT wide aspect ratio for the MacBook Pro Touch Bar (expect for 2023 MacBook Pro or Air, even Neo).
        self.spectrumView = [[SpectrumTouchBar alloc] initWithFrame:NSMakeRect(0, 0, 680, 30)];
        item.view = self.spectrumView;
        return item;
    }
    return nil;
}
@end

// GLOBAL instance to KEEP the Touch Bar alive.
static TouchBarManager* g_tbManager = nil;

// =======================================================
// Step 3. THE C++ BRIDGES
// =======================================================
bool InitTouchBar(GLFWwindow* window) {
    @autoreleasepool {
        // ONLY run on macOS 10.12.2 or HIGHER. (That was Touch Bar API Introduced)
        if (@available(macOS 10.12.2, *)) {
            NSWindow* macWindow = glfwGetCocoaWindow(window);
            if (!macWindow) return false;

            g_tbManager = [[TouchBarManager alloc] init];

            NSTouchBar *tb = [[NSTouchBar alloc] init];
            tb.delegate = g_tbManager;
            tb.defaultItemIdentifiers = @[@"com.clayton.spectrum"];

            macWindow.touchBar = tb;
            return true; // SUCCESSFULLY hooked into the OS Touch Bar API.
        }
        return false; // OS or Hardware doesn't support it! 
    }
}

void UpdateTouchBar(const std::vector<float>& frequencies) {
    @autoreleasepool {
        if (g_tbManager && g_tbManager.spectrumView) {
            // PUSH the new 60 FPS math to the Touch Bar and force an instant redraw.
            g_tbManager.spectrumView.heights = frequencies;
            [g_tbManager.spectrumView setNeedsDisplay:YES];
        }
    }
}
#endif // __APPLE__