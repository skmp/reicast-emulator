/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#import <AppKit/AppKit.h>
#import "EmuGLView.h"
#import "ConsoleViewController.h"

@interface AppDelegate : NSObject <NSWindowDelegate, EmuGLViewDelegate>

// Main UI display window
@property (strong) NSWindow *mainWindow;
@property (strong) EmuGLView *glView;

// Console window
@property (strong) NSWindow *consoleWindow;
@property (strong) ConsoleViewController *consoleViewController;

+ (AppDelegate *)sharedInstance;

- (void)showConsoleWindow:(NSMenuItem *)menuItem;
- (void)hideConsoleWindow:(NSMenuItem *)menuItem;

@end
