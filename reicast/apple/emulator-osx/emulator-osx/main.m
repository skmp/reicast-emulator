/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#import <AppKit/AppKit.h>
#import "AppDelegate.h"
#import "MainMenu.h"

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Create Application and AppDelegate
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app setDelegate:(id)[[AppDelegate alloc] init]];
        
        // Create the Main Menu
        [MainMenu create];
                
        // Start the Application
        [app activateIgnoringOtherApps:YES];
        [app run];
        
        // Return success when we exit
        return EXIT_SUCCESS;
    }
}
