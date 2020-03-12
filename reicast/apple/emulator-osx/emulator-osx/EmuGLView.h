/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#import <Cocoa/Cocoa.h>

@protocol EmuGLViewDelegate;

@interface EmuGLView: NSOpenGLView
@property (weak) NSObject<EmuGLViewDelegate> *delegate;
@end

@protocol EmuGLViewDelegate
- (void)emuGLViewIsResizing:(EmuGLView *)emuGLView;
@end
