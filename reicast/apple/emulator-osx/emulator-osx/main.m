/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#import <AppKit/AppKit.h>
#import "AppDelegate.h"

int main(int argc, char *argv[]) {
    @autoreleasepool {
        // Create Application and AppDelegate
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app setDelegate:(id)[[AppDelegate alloc] init]];
        
        // Create Main Menu without a XIB file
        NSString *appName = [[NSProcessInfo processInfo] processName];
        NSMenu *menuBar = [[NSMenu alloc] init];
        [app setMainMenu:menuBar];
        
        // App Menu
        //
        NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:appMenuItem];
        // Submenu
        NSMenu *appMenu = [[NSMenu alloc] init];
        [appMenuItem setSubmenu:appMenu];
        // About
        NSString *aboutTitle = [NSString stringWithFormat:@"About %@", appName];
        [appMenu addItem:[[NSMenuItem alloc] initWithTitle:aboutTitle action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""]];
        [appMenu addItem:[NSMenuItem separatorItem]];
        // Services
        NSMenuItem *servicesItem = [[NSMenuItem alloc] initWithTitle:@"Services" action:nil keyEquivalent:@""];
        [servicesItem setSubmenu:[app servicesMenu]];
        [appMenu addItem:servicesItem];
        [appMenu addItem:[NSMenuItem separatorItem]];
        // Hide
        NSString *hideTitle = [NSString stringWithFormat:@"Hide %@", appName];
        [appMenu addItem:[[NSMenuItem alloc] initWithTitle:hideTitle action:@selector(hide:) keyEquivalent:@"h"]];
        NSMenuItem *hideOthersItem = [[NSMenuItem alloc] initWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        [hideOthersItem setKeyEquivalentModifierMask:NSEventModifierFlagOption];
        [appMenu addItem:hideOthersItem];
        [appMenu addItem:[[NSMenuItem alloc] initWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""]];
        [appMenu addItem:[NSMenuItem separatorItem]];
        // Quit
        [appMenu addItem:[[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"]];
        
        // File Menu
        //
        NSMenuItem *fileMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:fileMenuItem];
        // Submenu
        NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
        [fileMenuItem setSubmenu:fileMenu];
        // Open
        [fileMenu addItem:[[NSMenuItem alloc] initWithTitle:@"Open..." action:@selector(openDocument:) keyEquivalent:@"o"]];
        // Open Recent
        // TODO: Re-create Open Recent menu manually
        //NSMenuItem *openRecentItem = [[NSMenuItem alloc] initWithTitle:@"Open Recent" action:nil keyEquivalent:@""];
        //[openRecentItem addSubmenu:[app openRecet]]
        //[fileMenu addItem:[NSMenuItem separatorItem]];
        // Close
        [fileMenu addItem:[[NSMenuItem alloc] initWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"]];
        
        // Window Menu
        //
        NSMenuItem *windowMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:windowMenuItem];
        // Submenu
        NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
        [windowMenuItem setSubmenu:windowMenu];
        // Minimize
        NSMenuItem *minimizeItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
        [minimizeItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
        [windowMenu addItem:minimizeItem];
        // Zoom
        [windowMenu addItem:[[NSMenuItem alloc] initWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""]];
        [fileMenu addItem:[NSMenuItem separatorItem]];
        // Bring All to Front
        [windowMenu addItem:[[NSMenuItem alloc] initWithTitle:@"Bring All to Front" action:@selector(arrangeInFront:) keyEquivalent:@""]];
        
        // Help Menu
        //
        NSMenuItem *helpMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:helpMenuItem];
        // Submenu
        NSMenu *helpMenu = [[NSMenu alloc] initWithTitle:@"Help"];
        [helpMenuItem setSubmenu:helpMenu];
        // Reicast Help
        NSString *reicastHelpTitle = [NSString stringWithFormat:@"%@ Help", appName];
        NSMenuItem *reicastHelpMenuItem = [[NSMenuItem alloc] initWithTitle:reicastHelpTitle action:@selector(showHelp:) keyEquivalent:@"?"];
        [reicastHelpMenuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
        [helpMenu addItem:reicastHelpMenuItem];
                
        // Start the Application
        [app activateIgnoringOtherApps:YES];
        [app run];
        
        // Return success when we exit
        return EXIT_SUCCESS;
    }
}
