/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#import <AppKit/AppKit.h>
#import "EmuGLView.h"

@interface AppDelegate : NSObject <NSWindowDelegate, EmuGLViewDelegate>

@property (strong) NSWindow *window;
@property (strong) EmuGLView *glView;

+ (AppDelegate *)sharedInstance;

@end
