//
//  MainMenu.h
//  reicast-osx
//
//  Created by Benjamin Baron on 3/31/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import <AppKit/AppKit.h>

@interface MainMenu : NSObject

+ (void)create;
+ (NSMenuItem *)toggleConsoleMenuItem;

@end
