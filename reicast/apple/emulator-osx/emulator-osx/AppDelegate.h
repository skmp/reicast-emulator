//
//  AppDelegate.h
//  reicast-osx
//
//  Created by Benjamin Baron on 2/27/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "EmuGLView.h"

@interface AppDelegate : NSObject <NSWindowDelegate>

@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet EmuGLView *glView;

@property CGFloat backingScaleFactor;
@property CGSize glViewSize;

+ (AppDelegate *)sharedInstance;

@end
