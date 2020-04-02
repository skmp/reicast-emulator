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
    
    NSTextField *_filterTextField;
    BOOL _shouldFilter;
    NSTextStorage *_filteredTextStorage;
}

+ (NSDictionary *)defaultTextAttributes {
    return @{NSForegroundColorAttributeName: [NSColor textColor]};
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
    
    NSRect frame = self.view.frame;
    CGFloat inset = 5;
    
    CGFloat filterTextHeight = 25;
    NSRect filterTextFrame = NSMakeRect(inset, NSHeight(frame) - filterTextHeight - inset, NSWidth(frame) - (inset * 2), filterTextHeight);
    _filterTextField = [[NSTextField alloc] initWithFrame:filterTextFrame];
    _filterTextField.delegate = self;
    _filterTextField.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    _filterTextField.placeholderString = @"Filter (Supports Regex)";
    [self.view addSubview:_filterTextField];
    
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
    _textView.textContainerInset = NSMakeSize(inset, inset);

    NSRect scrollViewFrame = NSMakeRect(inset, inset, NSWidth(filterTextFrame), NSHeight(frame) - NSHeight(filterTextFrame) - (inset * 3));
    _scrollView = [[NSScrollView alloc] initWithFrame:scrollViewFrame];
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
    if (_shouldFilter) {
        [self filterConsole:_filterTextField.stringValue];
    } else {
        [_textView scrollToEndOfDocument:nil];
    }
}

- (void)controlTextDidChange:(NSNotification *)notification {
    NSString *regexString = _filterTextField.stringValue;
    BOOL shouldFilter = regexString.length > 0;
    if (shouldFilter && !_shouldFilter) {
        // Start filtering
        _filteredTextStorage = [[NSTextStorage alloc] init];
        [_textStorage removeLayoutManager:_layoutManager];
        [_filteredTextStorage addLayoutManager:_layoutManager];
    } else if (!shouldFilter && _shouldFilter) {
        // Stop filtering
        [_filteredTextStorage removeLayoutManager:_layoutManager];
        [_textStorage addLayoutManager:_layoutManager];
        _filteredTextStorage = nil;
    }
    _shouldFilter = shouldFilter;
    
    if (_shouldFilter) {
        [self filterConsole:regexString];
    }
}

- (void)filterConsole:(NSString *)regexString {
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSFont *boldFont = [fontManager convertFont:_textStorage.font toHaveTrait:NSBoldFontMask];
    NSDictionary *attributes = [self.class defaultTextAttributes];
    NSMutableAttributedString *attributedString = [[NSMutableAttributedString alloc] initWithString:@"" attributes:attributes];
    
    [_textStorage.string enumerateLinesUsingBlock:^(NSString *line, BOOL *stop) {
        NSRange range = [line rangeOfString:regexString options:NSRegularExpressionSearch | NSCaseInsensitiveSearch];
        if (range.location != NSNotFound) {
            // Add the line and bold the match
            NSMutableAttributedString *attributedLine = [[NSMutableAttributedString alloc] initWithString:[NSString stringWithFormat:@"%@\n", line] attributes:attributes];
                        
            // Highlight the match
            [attributedLine addAttribute:NSFontAttributeName value:boldFont range:range];
            [attributedLine addAttribute:NSUnderlineStyleAttributeName value:@(NSUnderlineStyleSingle) range:range];
            [attributedLine addAttribute:NSForegroundColorAttributeName value:[NSColor redColor] range:range];
            [attributedString appendAttributedString:attributedLine];
        }
    }];
    
    [_filteredTextStorage setAttributedString:attributedString];
    [_textView scrollToEndOfDocument:nil];
}

@end
