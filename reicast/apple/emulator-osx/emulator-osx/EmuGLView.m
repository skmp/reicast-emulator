//
//  EmuGLView.m
//  reicast-osx
//
//  Created by Benjamin Baron on 2/27/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import "EmuGLView.h"
#import "osx-main.h"

@implementation EmuGLView {
    int32_t mouse_prev_x;
    int32_t mouse_prev_y;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    [self.openGLContext makeCurrentContext];
    
    // TODO: BEN implement this correctly
//    if (emu_single_frame(dirtyRect.size.width, dirtyRect.size.height) != 0) {
//        [self.openGLContext flushBuffer];
//    }
}

- (void)awakeFromNib {
    NSTimer *renderTimer = [NSTimer scheduledTimerWithTimeInterval:0.001 target:self selector:@selector(timerTick) userInfo:nil repeats:YES];
    
    [NSRunLoop.currentRunLoop addTimer:renderTimer forMode:NSDefaultRunLoopMode];
    [NSRunLoop.currentRunLoop addTimer:renderTimer forMode:NSEventTrackingRunLoopMode];
        
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAStencilSize, 8,
        // Must specify the 3.2 Core Profile to use OpenGL 3.2
        NSOpenGLPFAOpenGLProfile,
        NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFABackingStore, true,
        0
    };

    self.pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    self.openGLContext = [[NSOpenGLContext alloc] initWithFormat:self.pixelFormat shareContext:nil];

    [self.openGLContext makeCurrentContext];
    emu_gles_init(self.frame.size.width, self.frame.size.height);

    if (emu_reicast_init() != 0) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.alertStyle = NSCriticalAlertStyle;
        alert.messageText = @"Reicast initialization failed";
        [alert runModal];
    }
    
    emu_start_ui_loop();
}

- (void)timerTick {
    if (emu_frame_pending()) {
        self.needsDisplay = YES;
    }
}

- (void)keyDown:(NSEvent *)event {
    if (!event.isARepeat) {
        emu_key_input(event.keyCode, true, event.modifierFlags & NSDeviceIndependentModifierFlagsMask);
    }
    emu_character_input([event.characters UTF8String]);
}

- (void)keyUp:(NSEvent *)event {
    emu_key_input(event.keyCode, false, event.modifierFlags & NSDeviceIndependentModifierFlagsMask);
}

- (void)setMousePos:(NSEvent *)event {
    NSPoint point = [self convertPoint:event.locationInWindow fromView:self];
    NSSize size = self.frame.size;
    CGFloat scale = 480.0 / size.height;
    mo_x_abs = (point.x - (size.width - 640.0 / scale) / 2.0) * scale;
    mo_y_abs = (size.height - point.y) * scale;
    mo_x_delta += (mo_x_abs - mouse_prev_x);
    mo_y_delta += (mo_y_abs - mouse_prev_y);
    mouse_prev_x = mo_x_abs;
    mouse_prev_y = mo_y_abs;
}

- (void)mouseDown:(NSEvent *)event {
    emu_mouse_buttons(1, true);
    mo_buttons &= ~(1 << 2);
    [self setMousePos:event];
}

- (void)mouseUp:(NSEvent *)event {
    emu_mouse_buttons(1, false);
    mo_buttons |= 1 << 2;
    [self setMousePos:event];
}

- (void)rightMouseDown:(NSEvent *)event {
    emu_mouse_buttons(2, true);
    mo_buttons &= ~(1 << 1);
    [self setMousePos:event];
}

- (void)rightMouseUp:(NSEvent *)event {
    emu_mouse_buttons(2, false);
    mo_buttons |= 1 << 1;
    [self setMousePos:event];
}

// Not dispatched by default. Need to set Window.acceptsMouseMovedEvents to true
- (void)mouseMoved:(NSEvent *)event {
    [self setMousePos:event];
}

- (void)mouseDragged:(NSEvent *)event {
    mo_buttons &= ~(1 << 2);
    [self setMousePos:event];
}

- (void)rightMouseDragged:(NSEvent *)event {
    mo_buttons &= ~(1 << 1);
    [self setMousePos:event];
}

- (void)otherMouseDown:(NSEvent *)event {
    emu_mouse_buttons(3, true);
    mo_buttons &= ~(1 << 2);
    [self setMousePos:event];
}

- (void)otherMouseUp:(NSEvent *)event {
    emu_mouse_buttons(3, false);
    mo_buttons |= 1 << 2;
    [self setMousePos:event];
}

- (void)scrollWheel:(NSEvent *)event {
    if (event.hasPreciseScrollingDeltas) {
        // 1 per "line"
        mo_wheel_delta -= event.scrollingDeltaY * 3.2;
    } else {
        // 0.1 per wheel notch
        mo_wheel_delta -= event.scrollingDeltaY * 160;
    }
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    self.window.delegate = self;
    self.window.acceptsMouseMovedEvents = YES;
}

- (void)windowWillClose:(NSNotification *)notification {
    emu_dc_exit();
}

@end
