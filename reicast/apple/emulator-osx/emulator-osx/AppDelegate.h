//
//  AppDelegate.h
//  reicast-osx
//
//  Created by Benjamin Baron on 2/27/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import <AppKit/AppKit.h>
#import "EmuGLView.h"

@interface AppDelegate : NSObject <NSWindowDelegate, EmuGLViewDelegate>

@property (strong) NSWindow *window;
@property (strong) EmuGLView *glView;

+ (AppDelegate *)sharedInstance;

@end
