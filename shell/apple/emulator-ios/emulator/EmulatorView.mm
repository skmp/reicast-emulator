//
//  EmulatorView.m
//  emulator
//
//  Created by admin on 1/18/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//

#import "EmulatorView.h"

#include "types.h"
#include "hw/maple/maple_controller.h"

int dpad_or_btn = 0;

@implementation EmulatorView

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect {
    // Drawing code
}
*/

-(void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    
    if (dpad_or_btn &1)
        kcode[0] &= ~(DC_BTN_START|DC_BTN_A);
    else
        kcode[0] &= ~(DC_BTN_DPAD_LEFT);
}

-(void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    
    //	[event allTouches];
    
    if (dpad_or_btn &1)
        kcode[0] |= (DC_BTN_START|DC_BTN_A);
    else
        kcode[0] |= (DC_BTN_DPAD_LEFT);
    
    dpad_or_btn++;
}
@end
