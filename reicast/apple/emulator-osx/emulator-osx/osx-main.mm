//
//  osx-main.cpp
//  emulator-osx
//
//  Created by admin on 8/5/15.
//  Copyright (c) 2015 reicast. All rights reserved.
//
#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>
#import <OpenGL/gl3.h>
#import <sys/stat.h>

#import "libswirl.h"
#import "types.h"
#import "hw/maple/maple_cfg.h"
#import "gui/gui.h"
#import "osx_keyboard.h"
#import "osx_gamepad.h"
#import "AppDelegate.h"
#import "EmuGLView.h"

void common_linux_setup();
void rend_resize(int width, int height);
extern int screen_width, screen_height, screen_dpi;

int main(int argc, char *argv[]) {
    NSLog(@"main called");
    return NSApplicationMain(argc,  (const char **) argv);
}

OSXKeyboardDevice keyboard(0);
static std::shared_ptr<OSXKbGamepadDevice> kb_gamepad(0);
static std::shared_ptr<OSXMouseGamepadDevice> mouse_gamepad(0);

int darw_printf(const wchar* text,...) {
    va_list args;

    wchar temp[2048];
    va_start(args, text);
    vsprintf(temp, text, args);
    va_end(args);

    NSLog(@"%s", temp);

    return 0;
}

u16 kcode[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

// TODO: BEN This was copied directly from the Linux main.cpp file, no idea if it works on macOS
void os_LaunchFromURL(const string& url) {
    auto cmd = "xdg-open " + url;
    auto rv = system(cmd.c_str());
}

void os_SetWindowText(const char * text) {
    puts(text);
}

void os_DoEvents() {

}


void UpdateInputState(u32 port) {

}

void os_CreateWindow() {

}

void os_SetupInput() {
	kb_gamepad = std::make_shared<OSXKbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
	mouse_gamepad = std::make_shared<OSXMouseGamepadDevice>(0);
	GamepadDevice::Register(mouse_gamepad);
}

void* libPvr_GetRenderTarget() {
    return (__bridge void*)AppDelegate.sharedInstance.window;
}

void* libPvr_GetRenderSurface() {
    EmuGLView *glView = (EmuGLView*)AppDelegate.sharedInstance.glView;
    return (__bridge void*)glView.openGLContext;
}

bool os_gl_init(void* hwnd, void* hdc) {
    // Handled in EmuGLView wakeFromNib method
    EmuGLView *glView = (EmuGLView*)AppDelegate.sharedInstance.glView;
    [glView.openGLContext makeCurrentContext];
    return true;
}

void os_gl_term() {
    reicast_term();
}

bool os_gl_swap() {
    AppDelegate *appDelegate = AppDelegate.sharedInstance;
    EmuGLView *glView = appDelegate.glView;
    
    [glView.openGLContext makeCurrentContext];
    [glView.openGLContext flushBuffer];
    
    // Handle HDPI (possibly don't do this on every swap and just set it once when they change?)
    CGFloat screenScale = appDelegate.backingScaleFactor;
    CGSize glViewSize = appDelegate.glViewSize;
    rend_resize(glViewSize.width * screenScale, glViewSize.height * screenScale);
    return true;
}


//int reicast_init(int argc, char* argv[]);

//void rend_init_renderer();

extern "C" void emu_dc_exit()
{
    // TODO: BEN probably do some cleanup here
}

//bool rend_single_frame();
bool rend_framePending();

extern "C" bool emu_frame_pending()
{
    return rend_framePending() || g_GUI->IsOpen();
}

extern "C" int emu_single_frame(int w, int h) {
    if (!emu_frame_pending())
        return 0;
    screen_width = w;
    screen_height = h;

    // TODO: BEN
    //return rend_single_frame();
    return 0;
}

extern "C" void emu_gles_init(int width, int height) {
    char *home = getenv("HOME");
    if (home != NULL)
    {
        string config_dir = string(home) + "/.reicast";
        mkdir(config_dir.c_str(), 0755); // create the directory if missing
        set_user_config_dir(config_dir);
        set_user_data_dir(config_dir);
    }
    else
    {
        set_user_config_dir(".");
        set_user_data_dir(".");
    }
    // Add bundle resources path
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX))
        add_system_data_dir(string(path));
    CFRelease(resourcesURL);
    CFRelease(mainBundle);

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
	screen_width = width;
	screen_height = height;
    
    /*
    // Calculate screen DPI
    // Set the DPI so that libswirl calculates the scaling to match the NSScreen.backingScaleFactor
    // TODO: BEN This should be done from the EmuGLView so we get the correct screen
    // TODO: BEN Also this should properly handle the case where the window is moved to another screen
    CGFloat scaleFactor = NSScreen.mainScreen.backingScaleFactor;
    CGFloat noScalingDPI = 133.33;
    screen_dpi = (int)(noScalingDPI * scaleFactor);
    NSLog(@"scaleFactor: %f, noScalingDPI: %f, screen_dpi: %d", scaleFactor, noScalingDPI, screen_dpi);
    screen_width = width;
    screen_height = height;
     */

	//rend_init_renderer();
}

extern "C" int emu_reicast_init()
{
	common_linux_setup();
	return reicast_init(0, NULL);
}

extern "C" void emu_start_ui_loop() {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void) {
        NSLog(@"Starting UI Loop");
        reicast_ui_loop();
        NSLog(@"UI Loop ended");
    });
    
//   reicast_ui_loop();
}

extern "C" void emu_key_input(UInt16 keyCode, bool pressed, UInt modifierFlags) {
	keyboard.keyboard_input(keyCode, pressed, keyboard.convert_modifier_keys(modifierFlags));
	if ((modifierFlags
		 & (NSEventModifierFlagShift | NSEventModifierFlagControl | NSEventModifierFlagOption | NSEventModifierFlagCommand)) == 0)
		kb_gamepad->gamepad_btn_input(keyCode, pressed);
}
extern "C" void emu_character_input(const char *characters) {
	if (characters != NULL)
		while (*characters != '\0')
			keyboard.keyboard_character(*characters++);
}

extern "C" void emu_mouse_buttons(int button, bool pressed)
{
	mouse_gamepad->gamepad_btn_input(button, pressed);
}
