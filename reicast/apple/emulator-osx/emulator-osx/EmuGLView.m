/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#import "EmuGLView.h"
#import "input.h"

@implementation EmuGLView {
    int32_t mouse_prev_x;
    int32_t mouse_prev_y;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    
    NSLog(@"initWithFrame");
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
    
    return self;
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)reshape {
    [super reshape];
    [_delegate emuGLViewIsResizing:self];
}

- (void)keyDown:(NSEvent *)event {
    if (!event.isARepeat) {
        emu_key_input(event.keyCode, true, event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask);
    }
    emu_character_input([event.characters UTF8String]);
}

- (void)keyUp:(NSEvent *)event {
    emu_key_input(event.keyCode, false, event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask);
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

@end
