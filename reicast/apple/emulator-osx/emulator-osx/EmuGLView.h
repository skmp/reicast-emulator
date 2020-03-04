//
//  EmuGLView.h
//  reicast-osx
//
//  Created by Benjamin Baron on 2/27/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol EmuGLViewDelegate;

@interface EmuGLView: NSOpenGLView
@property (weak) NSObject<EmuGLViewDelegate> *delegate;
@end

@protocol EmuGLViewDelegate
- (void)emuGLViewIsResizing:(EmuGLView *)emuGLView;
@end
