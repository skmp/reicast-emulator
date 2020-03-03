//
//  AppDelegate.m
//  reicast-osx
//
//  Created by Benjamin Baron on 2/27/20.
//  Copyright © 2020 reicast. All rights reserved.
//

#import "AppDelegate.h"
#import "libswirl.h"

#ifdef FEAT_HAS_SERIAL_TTY
#import <util.h>
bool cleanup_pty_symlink = false;
int pty_master;
#endif

void common_linux_setup();
void rend_resize(int width, int height);
extern int screen_width, screen_height, screen_dpi;

static AppDelegate *_sharedInstance;
static CGFloat _backingScaleFactor;
static CGSize _glViewSize;

@implementation AppDelegate

#pragma mark - Delegate Methods -

+ (AppDelegate *)sharedInstance {
    return _sharedInstance;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    _sharedInstance = self;
    [self mainSetup];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    [self shutdownEmulator];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    _backingScaleFactor = _window.screen.backingScaleFactor;
}

- (void)windowWillClose:(NSNotification *)notification {
    NSLog(@"windowWillClose");
    [self shutdownEmulator];
}

- (void)glViewFrameDidChange:(NSNotification *)notification {
    _glViewSize = _glView.frame.size;
}

#pragma mark - Setup Methods -

- (void)mainSetup {
    [self setupWindow];
    [self setupScreenSize];
    [self setupPaths];
    
    common_linux_setup();
    if (reicast_init(0, NULL) != 0) {
        [self alertAndTerminateWithMessage:@"Reicast initialization failed"];
    }
    [self setupSerialPort];
    
    // Start UI Loop
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void) {
        NSLog(@"Starting UI Loop");
        reicast_ui_loop();
        NSLog(@"UI Loop ended");
    });
}

- (void)setupWindow {
    // Create the Window
    _glView = [[EmuGLView alloc] init];
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
    _window.delegate = self;
    _window.contentView = _glView;
    _window.acceptsMouseMovedEvents = YES;
    [_window makeKeyAndOrderFront:_window];
    [_window center];
    
    // Set initial scale factor and view size
    _backingScaleFactor = _window.screen.backingScaleFactor;
    _glViewSize = _glView.bounds.size;
    
    // Watch for frame changes
    _glView.postsFrameChangedNotifications = true;
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(glViewFrameDidChange:) name:NSViewFrameDidChangeNotification object:_glView];
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

- (void)setupScreenSize {
    // Calculate screen DPI
    // TODO: BEN This should be done from the EmuGLView so we get the correct screen
    // TODO: BEN Also this should properly handle the case where the window is moved to another screen
    NSScreen *screen = [NSScreen mainScreen];
    NSDictionary *description = [screen deviceDescription];
    NSSize displayPixelSize = [[description objectForKey:NSDeviceSize] sizeValue];
    // Must multiply by the scale factor to account for retina displays
    displayPixelSize.width *= screen.backingScaleFactor;
    displayPixelSize.height *= screen.backingScaleFactor;
    CGSize displayPhysicalSize = CGDisplayScreenSize([[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);
    screen_dpi = (int)(displayPixelSize.width / displayPhysicalSize.width) * 25.4f;
    NSLog(@"displayPixelSize: %@  displayPhysicalSize: %@  screen_dpi: %d", NSStringFromSize(displayPixelSize), NSStringFromSize(displayPhysicalSize), screen_dpi);
    screen_width = _glView.frame.size.width;
    screen_height = _glView.frame.size.height;
}

- (void)setupSerialPort {
#ifdef FEAT_HAS_SERIAL_TTY
    if (settings.debug.VirtualSerialPort) {
        int slave;
        char slave_name[2048];
        pty_master = -1;
        if (openpty(&pty_master, &slave, slave_name, nullptr, nullptr) >= 0)
        {
            // Turn ECHO off, we don't want to loop-back
            struct termios tp;
            tcgetattr(pty_master, &tp);
            tp.c_lflag &= ~ECHO;
            tcsetattr(pty_master, TCSAFLUSH, &tp);

            printf("Serial: Created virtual serial port at %s\n", slave_name);

            if (settings.debug.VirtualSerialPortFile.size())
            {
                if (symlink(slave_name, settings.debug.VirtualSerialPortFile.c_str()) == 0)
                {
                    cleanup_pty_symlink = true;
                    printf("Serial: Created symlink to %s\n",  settings.debug.VirtualSerialPortFile.c_str());
                }
                else
                {
                    printf("Serial: Failed to create symlink to %s, %d\n", settings.debug.VirtualSerialPortFile.c_str(), errno);
                }
            }
            // not for us to use, we use master
            // do not close to avoid EIO though
            // close(slave);
        }
        else
        {
            printf("Serial: Failed to create PTY: %d\n", errno);
        }
    }
#endif
}

- (void)shutdownEmulator {
#ifdef FEAT_HAS_SERIAL_TTY
    if (cleanup_pty_symlink) {
        unlink(settings.debug.VirtualSerialPortFile.c_str());
    }
#endif
        
    reicast_term();
}

- (void)alertAndTerminateWithMessage:(NSString *)message {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSCriticalAlertStyle;
    alert.messageText = message;
    [alert addButtonWithTitle:@"Exit"];
    [alert runModal];
    [NSApp terminate:nil];
}

#pragma mark - Reicast OS Functions -

// TODO: BEN This was copied directly from the Linux main.cpp file, no idea if it works on macOS
void os_LaunchFromURL(const string& url) {
    auto cmd = "xdg-open " + url;
    auto rv = system(cmd.c_str());
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

bool os_gl_init(void* hwnd, void* hdc) {
    // OpenGL is initialized in EmuGLView wakeFromNib method
    // All we need to do here is make the GLView's context the current context on this thread
    [_sharedInstance.glView.openGLContext makeCurrentContext];
    return true;
}

// Called when the application terminates
void os_gl_term() {
    NSLog(@"os_gl_term");
    reicast_term();
}

// Called on every frame to swap the buffer
bool os_gl_swap() {
    NSOpenGLContext *glContext = _sharedInstance.glView.openGLContext;
    [glContext makeCurrentContext];
    [glContext flushBuffer];
    
    // Handle HDPI (possibly don't do this on every swap and just set it once when they change?)
    rend_resize(_glViewSize.width * _backingScaleFactor, _glViewSize.height * _backingScaleFactor);
    return true;
}

void* libPvr_GetRenderTarget() {
    return (__bridge void*)_sharedInstance.window;
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

    wchar temp[2048];
    va_start(args, text);
    vsprintf(temp, text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

@end
