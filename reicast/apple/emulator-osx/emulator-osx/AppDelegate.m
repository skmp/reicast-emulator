//
//  AppDelegate.m
//  reicast-osx
//
//  Created by Benjamin Baron on 2/27/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import "AppDelegate.h"
#import "osx-main.h"

static AppDelegate *sharedInstance;

@implementation AppDelegate

+ (AppDelegate *)sharedInstance {
    return sharedInstance;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    sharedInstance = self;
    
    // Set initial scale factor and view size
    self.backingScaleFactor = self.window.screen.backingScaleFactor;
    self.glViewSize = self.glView.bounds.size;
    
    // Watch for frame changes
    self.glView.postsFrameChangedNotifications = true;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(glViewFrameDidChange:) name:NSViewFrameDidChangeNotification object:self.glView];
    
    // Start UI Loop
    emu_start_ui_loop();
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    emu_dc_exit();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    self.backingScaleFactor = self.window.screen.backingScaleFactor;
}

- (void)glViewFrameDidChange:(NSNotification *)notification {
    self.glViewSize = self.glView.frame.size;
}

@end
