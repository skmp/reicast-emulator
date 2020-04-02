/*
    This file is part of reicast-osx
*/
#include "license/bsd"

#import "AppDelegate.h"
#import "MainMenu.h"
#import "libswirl.h"
#import "gui/gui_renderer.h"

#if FEAT_HAS_SERIAL_TTY
#include <util.h>
#include <sys/stat.h>
bool common_serial_pty_setup();
#endif

void common_linux_setup();
bool common_serial_pty_setup();
void rend_resize(int width, int height);
extern int screen_dpi;

static AppDelegate *_sharedInstance;
static CGFloat _backingScaleFactor;
static CGSize _glViewSize;
static void gl_resize();

@interface AppDelegate()
@property (strong) NSTextStorage *consoleTextStorage;
@end

@implementation AppDelegate {
    NSThread *_uiThread;
    BOOL _removePTYSymlink;
    
    // Console redirection
    dispatch_source_t _stdoutSource;
    dispatch_source_t _stderrSource;
}

#pragma mark - Delegate Methods -

+ (AppDelegate *)sharedInstance {
    return _sharedInstance;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    _sharedInstance = self;
    [self mainSetup];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    // Exit the app when clicking the window's close button
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    // Start shutting down the emulator
    [self shutdownEmulator];
    
    // Keep the app open until it's safe to exit
    return NSTerminateLater;
}

// TODO: BEN This works while in BIOS or game, but doesn't correctly update the DPI for ImGUI
- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    // Handle DPI changes dragging between monitors
    _backingScaleFactor = _mainWindow.screen.backingScaleFactor;
    [self setupScreenDPI];
    gl_resize();
}

- (void)windowWillClose:(NSNotification *)notification {
    if (notification.object == _mainWindow) {
        // Make sure to close the console window if we're closing Reicast
        [_consoleWindow close];
    } else if (notification.object == _consoleWindow) {
        _consoleWindow = nil;
        _consoleViewController = nil;
        
        // Change menu item
        NSMenuItem *menuItem = [MainMenu toggleConsoleMenuItem];
        menuItem.title = @"Show Console";
        menuItem.action = @selector(showConsoleWindow:);
    }
}

- (void)emuGLViewIsResizing:(EmuGLView *)emuGLView {
    // Smoother rendering while resizing the window
    _glViewSize = emuGLView.frame.size;
    gl_resize();
}

#pragma mark - Action Methods -

- (void)showConsoleWindow:(NSMenuItem *)menuItem {
    if (!_consoleWindow) {
        // Create the ConsoleViewController
        _consoleViewController = [[ConsoleViewController alloc] initWithTextStorage:_consoleTextStorage];
        
        // Create the Window
        NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        _consoleWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
        _consoleWindow.delegate = self;
        _consoleWindow.contentViewController = _consoleViewController;
        _consoleWindow.releasedWhenClosed = NO;
        _consoleWindow.title = @"Console Output";
        [_consoleWindow makeKeyAndOrderFront:_consoleWindow];
        [_consoleWindow setFrameOrigin:NSMakePoint(NSMaxX(_mainWindow.frame) + 50, NSMinY(_mainWindow.frame))];
        [_mainWindow makeKeyAndOrderFront:_mainWindow];
        
        // Change menu item
        menuItem.title = @"Hide Console";
        menuItem.action = @selector(hideConsoleWindow:);
    }
}

- (void)hideConsoleWindow:(NSMenuItem *)menuItem {
    if (_consoleWindow) {
        [_consoleWindow close];
    }
}

#pragma mark - Setup Methods -

- (void)mainSetup {
    [self captureConsoleOutput];
    [self setupWindow];
    [self setupScreenDPI];
    [self setupPaths];
    common_linux_setup();
    if (reicast_init(0, NULL) != 0) {
        [self alertAndTerminateWithMessage:@"Reicast initialization failed"];
    }
    #if FEAT_HAS_SERIAL_TTY
    _removePTYSymlink = common_serial_pty_setup();
    #endif
    [self setupUIThread];
}

- (void)captureConsoleOutput {
    // Create the console text storage
    _consoleTextStorage = [[NSTextStorage alloc] init];
    
    // Block to consolidate common code
    void (^handleConsoleEvent)(NSPipe*, void*, int) = ^void(NSPipe *pipe, void *buffer, int fdOrig) {
        // Read from stderr/out
        ssize_t readResult = 0;
        do {
            errno = 0;
            readResult = read(pipe.fileHandleForReading.fileDescriptor, buffer, 4096);
        } while (readResult == -1 && errno == EINTR);
        
        if(readResult > 0) {
            // There was output, so write it to the console window's text storage
            NSString* string = [[NSString alloc] initWithBytesNoCopy:buffer length:readResult encoding:NSUTF8StringEncoding freeWhenDone:NO];
            NSDictionary *attributes = [ConsoleViewController defaultTextAttributes];
            NSAttributedString* attributedString = [[NSAttributedString alloc] initWithString:string attributes:attributes];
            dispatch_sync(dispatch_get_main_queue(),^{
                [self.consoleTextStorage appendAttributedString:attributedString];
            });

            // Write to normal console output as well
            write(fdOrig, buffer, readResult);
        }
    };
    
    // Read buffers
    static char stderrBuffer[4096];
    static char stdoutBuffer[4096];
    
    // Duplicate original file descriptors to allow writing to the standard console
    int stderrOrig = dup(fileno(stderr));
    int stdoutOrig = dup(fileno(stdout));
    
    // Create the pipes and divert the standard outputs
    NSPipe *stderrPipe = [NSPipe pipe];
    NSPipe *stdoutPipe = [NSPipe pipe];
    dup2(stderrPipe.fileHandleForWriting.fileDescriptor, fileno(stderr));
    dup2(stdoutPipe.fileHandleForWriting.fileDescriptor, fileno(stdout));
    
    // Set up dispatch event handlers to listen for output
    _stderrSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, stderrPipe.fileHandleForReading.fileDescriptor, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0));
    _stdoutSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, stdoutPipe.fileHandleForReading.fileDescriptor, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0));
    dispatch_source_set_event_handler(_stderrSource, ^{ handleConsoleEvent(stderrPipe, &stderrBuffer, stderrOrig); });
    dispatch_source_set_event_handler(_stdoutSource, ^{ handleConsoleEvent(stdoutPipe, &stdoutBuffer, stdoutOrig); });
    
    // Start listening
    dispatch_resume(_stderrSource);
    dispatch_resume(_stdoutSource);
}

- (void)setupWindow {
    // Create the OpenGL view and context
    // TODO: BEN See why background color is red when resizing window tall (it's likely due to default GL framebuffer color)
    _glView = [[EmuGLView alloc] init];
    _glView.delegate = self;
    
    // Create the Window
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    _mainWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
    _mainWindow.delegate = self;
    _mainWindow.contentView = _glView;
    _mainWindow.acceptsMouseMovedEvents = YES;
    [_mainWindow makeKeyAndOrderFront:_mainWindow];
    [_mainWindow center];
    
    // Set initial scale factor and view size
    _backingScaleFactor = _mainWindow.screen.backingScaleFactor;
    _glViewSize = _glView.bounds.size;
}

- (void)setupScreenDPI {
    // Calculate screen DPI
    NSScreen *screen = _mainWindow.screen;
    NSDictionary *description = [screen deviceDescription];
    NSSize displayPixelSize = [[description objectForKey:NSDeviceSize] sizeValue];
    // Must multiply by the scale factor to account for retina displays
    displayPixelSize.width *= screen.backingScaleFactor;
    displayPixelSize.height *= screen.backingScaleFactor;
    CGSize displayPhysicalSize = CGDisplayScreenSize([[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);
    screen_dpi = (int)(displayPixelSize.width / displayPhysicalSize.width) * 25.4f;
    NSLog(@"displayPixelSize: %@  displayPhysicalSize: %@  screen_dpi: %d", NSStringFromSize(displayPixelSize), NSStringFromSize(displayPhysicalSize), screen_dpi);
}

- (void)setupPaths {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *configDirPath = [[NSURL fileURLWithPathComponents:@[NSHomeDirectory(), @".reicast"]] path];
    BOOL isDir = false;
    BOOL exists = [fileManager fileExistsAtPath:configDirPath isDirectory:&isDir];
    if (exists && !isDir) {
        NSString *formatString = @"Config directory exists, but is not a directory: %@";
        [self alertAndTerminateWithMessage:[NSString stringWithFormat:formatString, configDirPath]];
    } else if (!exists) {
        NSError *error;
        [fileManager createDirectoryAtPath:configDirPath withIntermediateDirectories:YES attributes:nil error:&error];
        if (error) {
            NSString *formatString = @"Failed to create config directory at %@ with error: %@";
            [self alertAndTerminateWithMessage:[NSString stringWithFormat:formatString, configDirPath, error]];
        }
    }
    
    // Set user config path
    std::string config_dir([configDirPath UTF8String]);
    set_user_config_dir(config_dir);
    set_user_data_dir(config_dir);
    
    // Set system data path
    std::string data_dir([[[NSBundle mainBundle] resourcePath] UTF8String]);
    add_system_data_dir(data_dir);
}

- (void)setupUIThread {
    // Run the rendering in a dedicated thread
    _uiThread = [[NSThread alloc] initWithBlock:^{
        NSLog(@"Starting UI Loop");
        reicast_ui_loop();
        NSLog(@"UI Loop ended");
        
        // Terminate emulator (must be done after UI Loop ends or it may crash, must be run on main thread or it will crash)
        [_sharedInstance performSelectorOnMainThread:@selector(uiThreadEnded) withObject:nil waitUntilDone:YES];
    }];
    
    // Ensure the thread has the highest priority for the best performance
    _uiThread.threadPriority = 1.0;
    _uiThread.qualityOfService = NSQualityOfServiceUserInteractive;
    [_uiThread start];
}

// Terminate emulator and safely exit the app (must be called from the main thread)
- (void)uiThreadEnded {
    // Ensure that the gl view's context is the current context for this thread or it may crash
    [_glView.openGLContext makeCurrentContext];
    
    // Terminate the emulator
    reicast_term();
    
    // Notify the app that it can safely exit now
    [NSApp replyToApplicationShouldTerminate:YES];
}

- (void)shutdownEmulator {
    #if FEAT_HAS_SERIAL_TTY
    if (_removePTYSymlink) {
        unlink(settings.debug.VirtualSerialPortFile.c_str());
    }
    #endif
    
    // Shut down emulator
    if (virtualDreamcast && sh4_cpu->IsRunning()) {
        virtualDreamcast->Stop([] {
            // Stop the UI thread (which will then complete the shutdown, otherwise doing it here will crash)
            g_GUIRenderer->Stop();
        });
    } else {
        // Stop the UI thread (which will then complete the shutdown, otherwise doing it here will crash)
        g_GUIRenderer->Stop();
    }
}

- (void)alertAndTerminateWithMessage:(NSString *)message {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleCritical;
    alert.messageText = message;
    [alert addButtonWithTitle:@"Exit"];
    [alert runModal];
    [NSApp terminate:nil];
}

#pragma mark - Reicast OS Functions -

void os_LaunchFromURL(const string& url) {
    NSString *urlString = [NSString stringWithUTF8String:url.c_str()];
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:urlString]];
}

void os_SetWindowText(const char * text) {
    puts(text);
}

// Code that runs at the beginning of every frame
void os_DoEvents() {
    // Left empty on purpose as we have nothing to run here
}

// Handle controller input
void UpdateInputState(u32 port) {
    // TODO: BEN not sure what, if anything, we need here
}

// Called by libswirl to create the emulator window
void os_CreateWindow() {
    // Left empty on purpose as we have nothing to run here
}

// Called by libswirl when OpenGL is initialized
bool os_gl_init(void* hwnd, void* hdc) {
    [_sharedInstance.glView.openGLContext makeCurrentContext];
    gl_resize();
    return true;
}

// Called on every frame to swap the buffer
bool os_gl_swap() {
    NSOpenGLContext *glContext = _sharedInstance.glView.openGLContext;
    [glContext makeCurrentContext];
    [glContext flushBuffer];
    
    return true;
}

// Called when the application terminates
void os_gl_term() {
    // Left empty on purpose as we have nothing to run here
}

static void gl_resize() {
    // Handle HDPI by multiplying the scale factor
    rend_resize(_glViewSize.width * _backingScaleFactor, _glViewSize.height * _backingScaleFactor);
}

void* libPvr_GetRenderTarget() {
    return (__bridge void*)_sharedInstance.mainWindow;
}

void* libPvr_GetRenderSurface() {
    return (__bridge void*)_sharedInstance.glView.openGLContext;
}

int get_mic_data(u8* buffer) {
    return 0;
}

int push_vmu_screen(u8* buffer) {
    return 0;
}

int darw_printf(const wchar* text,...) {
    va_list args;

    static wchar temp[2048];
    va_start(args, text);
    vsprintf(temp, text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

@end
