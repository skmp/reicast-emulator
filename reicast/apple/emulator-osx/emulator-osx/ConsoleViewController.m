//
//  ConsoleViewController.m
//  reicast-osx
//
//  Created by Benjamin Baron on 3/31/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import "ConsoleViewController.h"

@implementation ConsoleViewController {
    NSTextStorage *_textStorage;
    NSTextContainer *_textContainer;
    NSLayoutManager *_layoutManager;
    NSTextView *_textView;
    NSScrollView *_scrollView;
}

- (instancetype)initWithTextStorage:(NSTextStorage *)textStorage {
    self = [super initWithNibName:nil bundle:nil];
    _textStorage = textStorage;
    _textStorage.delegate = self;
    _textContainer = [[NSTextContainer alloc] init];
    _layoutManager = [[NSLayoutManager alloc] init];
    [_textStorage addLayoutManager:_layoutManager];
    [_layoutManager addTextContainer:_textContainer];
    return self;
}

- (void)loadView {
    self.view = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 640, 480)];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    _textContainer.containerSize = NSMakeSize(FLT_MAX, FLT_MAX);
    _textContainer.widthTracksTextView = NO;
    
    _textView = [[NSTextView alloc] initWithFrame:NSZeroRect textContainer:_textContainer];
    _textView.minSize = NSZeroSize;
    _textView.maxSize = NSMakeSize(FLT_MAX, FLT_MAX);
    _textView.verticallyResizable = YES;
    _textView.horizontallyResizable = YES;
    _textView.frame = (NSRect){.origin = NSZeroPoint, .size = _scrollView.contentSize};
    _textView.editable = NO;
    _textView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _textView.textContainerInset = NSMakeSize(3, 5);

    _scrollView = [[NSScrollView alloc] initWithFrame:self.view.frame];
    _scrollView.borderType = NSNoBorder;
    _scrollView.hasVerticalScroller = YES;
    _scrollView.hasHorizontalScroller = YES;
    _scrollView.documentView = _textView;
    _scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self.view addSubview:_scrollView];
}

- (void)dealloc {
    [_textStorage removeLayoutManager:_layoutManager];
    _textStorage.delegate = nil;
}

- (void)textStorage:(NSTextStorage *)textStorage didProcessEditing:(NSTextStorageEditActions)editedMask range:(NSRange)editedRange changeInLength:(NSInteger)delta {
    [_textView scrollToEndOfDocument:nil];
}

@end
