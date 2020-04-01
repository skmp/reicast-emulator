//
//  ConsoleViewController.h
//  reicast-osx
//
//  Created by Benjamin Baron on 3/31/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface ConsoleViewController : NSViewController <NSTextStorageDelegate>

- (instancetype)initWithTextStorage:(NSTextStorage *)textStorage;

@end
