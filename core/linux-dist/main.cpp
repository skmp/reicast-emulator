#include "types.h"
#include "cfg/cfg.h"

#if HOST_OS==OS_LINUX
#include <poll.h>
#include <termios.h>
//#include <curses.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/personality.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "hw/sh4/dyna/blockmanager.h"
#include <unistd.h>
#include "bcm_host.h"

#include <stdio.h>
#include <map>

#if defined(SUPPORT_X11)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#if !defined(GLES)
#include <GL/gl.h>
#include <GL/glx.h>
#endif

map<int, int> x11_keymap;
#endif

#if !defined(ANDROID) && HOST_OS != OS_DARWIN
#include <linux/joystick.h>
#include <sys/stat.h> 
#include <sys/types.h> 
#endif

#ifdef TARGET_PANDORA
#include <signal.h>
#include <execinfo.h>
#include <sys/soundcard.h>

#define WINDOW_WIDTH	800
#else
#define WINDOW_WIDTH	640
#endif
#define WINDOW_HEIGHT	480

#define NUM_PORTS 4

void* x11_win = 0, * x11_disp = 0;

void* libPvr_GetRenderTarget() {
    return x11_win;
}

void* libPvr_GetRenderSurface() {
    return x11_disp;
}

int msgboxf(const wchar* text, unsigned int type, ...) {
    va_list args;

    wchar temp[2048];
    va_start(args, type);
    vsprintf(temp, text, args);
    va_end(args);

    //printf(NULL,temp,VER_SHORTNAME,type | MB_TASKMODAL);
    puts(temp);
    return MBX_OK;
}



u16 kcode[NUM_PORTS];
u32 vks[NUM_PORTS];
s8 joyx[NUM_PORTS], joyy[NUM_PORTS];
u8 rt[NUM_PORTS], lt[NUM_PORTS];

enum DCPad {
    Nothing = 0,
    Btn_C = 1,
    Btn_B = 1 << 1,
    Btn_A = 1 << 2,
    Btn_Start = 1 << 3,
    DPad_Up = 1 << 4,
    DPad_Down = 1 << 5,
    DPad_Left = 1 << 6,
    DPad_Right = 1 << 7,
    Btn_Z = 1 << 8,
    Btn_Y = 1 << 9,
    Btn_X = 1 << 10,
    Btn_D = 1 << 11,
    DPad2_Up = 1 << 12,
    DPad2_Down = 1 << 13,
    DPad2_Left = 1 << 14,
    DPad2_Right = 1 << 15,

    Axis_LT = 0x10000,
    Axis_RT = 0x10001,

    Quit = 0x10002,

    Axis_X = 0x20000,
    Axis_Y = 0x20001
};

map<const char*, u32> createControlNameMap() {
    map<const char*, u32> m;
    m["Nothing"] = Nothing;
    m["Btn_C"] = Btn_C;
    m["Btn_B"] = Btn_B;
    m["Btn_A"] = Btn_A;
    m["Btn_Start"] = Btn_Start;
    m["DPad_Up"] = DPad_Up;
    m["DPad_Down"] = DPad_Down;
    m["DPad_Left"] = DPad_Left;
    m["DPad_Right"] = DPad_Right;
    m["Btn_Z"] = Btn_Z;
    m["Btn_Y"] = Btn_Y;
    m["Btn_X"] = Btn_X;
    m["Btn_D"] = Btn_D;
    m["DPad2_Up"] = DPad2_Up;
    m["DPad2_Down"] = DPad2_Down;
    m["DPad2_Left"] = DPad2_Left;
    m["DPad2_Right"] = DPad2_Right;
    m["Axis_LT"] = Axis_LT;
    m["Axis_RT"] = Axis_RT;
    m["Axis_X"] = Axis_X;
    m["Axis_Y"] = Axis_Y;
    m["Quit"] = Quit;
    return m;
}
map<const char*, u32> dreamcastControlNameToEnumMapping = createControlNameMap();

void emit_WriteCodeCache();

static int JoyFD[NUM_PORTS] = {-1, -1, -1, -1}; // Joystick file descriptors
static int kbfd = -1;
char stringConvertScratch[32];

#ifdef TARGET_PANDORA
static int audio_fd = -1;
#endif


#define MAP_SIZE 32

//const u32 JMapBtn_360[MAP_SIZE] ={Btn_A, Btn_B, Btn_X, Btn_Y, 0, 0, 0, Btn_Start, 0, 0};
//
//const u32 JMapAxis_360[MAP_SIZE] ={Axis_X, Axis_Y, Axis_LT, 0, 0, Axis_RT, DPad_Left, DPad_Up, 0, 0};
//
////PS3 controller mappings
//const u32 JMapBtn_PS3[MAP_SIZE] ={Btn_Z, Btn_C, Btn_D, Btn_Start, DPad_Up, DPad_Right, DPad_Down, DPad_Left, Axis_LT, Axis_RT, DPad2_Left, DPad2_Right, Btn_Y, Btn_B, Btn_A, Btn_X, Quit};
//
//const u32 JMapAxis_PS3[MAP_SIZE] ={Axis_X, Axis_Y, DPad2_Up, DPad2_Down, 0, 0, 0, 0, 0, 0};

//TODO: Initialize defaults in better way
u32 JMapBtn[NUM_PORTS][MAP_SIZE] = {
    {Btn_Y, Btn_B, Btn_A, Btn_X, 0, 0, 0, 0, 0, Btn_Start},
    {Btn_Y, Btn_B, Btn_A, Btn_X, 0, 0, 0, 0, 0, Btn_Start},
    {Btn_Y, Btn_B, Btn_A, Btn_X, 0, 0, 0, 0, 0, Btn_Start},
    {Btn_Y, Btn_B, Btn_A, Btn_X, 0, 0, 0, 0, 0, Btn_Start}
};
u32 JMapAxis[NUM_PORTS][MAP_SIZE] = {
    {Axis_X, Axis_Y, 0, 0, 0, 0, 0, 0, 0, 0},
    {Axis_X, Axis_Y, 0, 0, 0, 0, 0, 0, 0, 0},
    {Axis_X, Axis_Y, 0, 0, 0, 0, 0, 0, 0, 0},
    {Axis_X, Axis_Y, 0, 0, 0, 0, 0, 0, 0, 0}
};

void SetupInput() {
    for (int port = 0; port < NUM_PORTS; port++) {
        kcode[port] = 0xFFFF;
        rt[port] = 0;
        lt[port] = 0;

        // Open joystick devices!
        sprintf(stringConvertScratch, "/dev/input/js%d", port);
        JoyFD[port] = open(stringConvertScratch, O_RDONLY);
#if HOST_OS != OS_DARWIN        
        if (JoyFD[port] >= 0) {
            int AxisCount, ButtonCount;
            char Name[128];

            AxisCount = 0;
            ButtonCount = 0;
            Name[0] = '\0';

            fcntl(JoyFD[port], F_SETFL, O_NONBLOCK);
            ioctl(JoyFD[port], JSIOCGAXES, &AxisCount);
            ioctl(JoyFD[port], JSIOCGBUTTONS, &ButtonCount);
            ioctl(JoyFD[port], JSIOCGNAME(sizeof (Name)), &Name);

            printf("SDK port %d: Found '%s' joystick with %d axis and %d buttons\n", port, Name, AxisCount, ButtonCount);

            if (cfgOpen()) {
                //Map controllers from config file, its gonna have to be strings mapped to the enum so the cfg makes sense
                //Should be in this format:
                //[PLAYSTATION(R)3 Controller]
                //button.0=Btn_Z
                //axis.0=Axis_X
                void checkForCustomControlMapping(u32, const char*, int, const char*);

                for (int i = 0; i < MAP_SIZE; i++) {

                    checkForCustomControlMapping(port, (new string(Name))->c_str(), i, "button");
                    checkForCustomControlMapping(port, (new string(Name))->c_str(), i, "axis");
                }
                printf("Controller mapping for port %d complete\n", port);
            }
        }
#endif
    }

#if HOST_OS != OS_DARWIN
    if (true) {
#ifdef TARGET_PANDORA
        const char* device = "/dev/input/event4";
#else
        const char* device = "/dev/event2";
#endif
        char name[256] = "Unknown";

        if ((kbfd = open(device, O_RDONLY)) > 0) {
            fcntl(kbfd, F_SETFL, O_NONBLOCK);
            if (ioctl(kbfd, EVIOCGNAME(sizeof (name)), name) < 0) {
                perror("evdev ioctl");
            }

            printf("The device on %s says its name is %s\n", device, name);

        } else
            perror("evdev open");
#endif
    }
}

void checkForCustomControlMapping(u32 port, const char* Name, int controllerIndex, const char* prefix) {
    //for 0 to MAP_SIZE, check for mapped buttons:
    sprintf(stringConvertScratch, "%s.%d", prefix, controllerIndex);
    string cfgControlMapButton = cfgLoadStr(Name, stringConvertScratch, NULL);
    if (cfgControlMapButton.empty()) {
        printf("No %s, ", stringConvertScratch);
    } else {
        //For each potential defined emulator control, see if this emu.cfg entry matches up
        typedef std::map<const char*, u32>::iterator it_type;
        for (it_type iterator = dreamcastControlNameToEnumMapping.begin(); iterator != dreamcastControlNameToEnumMapping.end(); iterator++) {
            // iterator->first = key
            // iterator->second = value
            if (cfgControlMapButton == iterator->first) {
                printf("Mapping port %d controller: [%s]%s.%d=%s, ", port, Name, prefix, controllerIndex, cfgControlMapButton.c_str());
                JMapBtn[port][controllerIndex] = iterator->second;
            }
        }
    }
}

bool HandleKb(u32 port) {
#if HOST_OS != OS_DARWIN
    if (kbfd < 0)
        return false;

    input_event ie;

#if defined(TARGET_GCW0)

#define KEY_A           0x1D
#define KEY_B           0x38
#define KEY_X           0x2A
#define KEY_Y           0x39
#define KEY_L           0xF
#define KEY_R           0xE
#define KEY_SELECT      0x1
#define KEY_START       0x1C
#define KEY_LEFT        0x69
#define KEY_RIGHT       0x6A
#define KEY_UP          0x67
#define KEY_DOWN        0x6C
#define KEY_LOCK        0x77    // Note that KEY_LOCK is a switch and remains pressed until it's switched back

    static int keys[13];
    while (read(kbfd, &ie, sizeof (ie)) == sizeof (ie)) {
        //printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
        if (ie.type = EV_KEY)
            switch (ie.code) {
                case KEY_SELECT: keys[9] = ie.value;
                    break;
                case KEY_UP: keys[1] = ie.value;
                    break;
                case KEY_DOWN: keys[2] = ie.value;
                    break;
                case KEY_LEFT: keys[3] = ie.value;
                    break;
                case KEY_RIGHT: keys[4] = ie.value;
                    break;
                case KEY_Y:keys[5] = ie.value;
                    break;
                case KEY_B:keys[6] = ie.value;
                    break;
                case KEY_A: keys[7] = ie.value;
                    break;
                case KEY_X: keys[8] = ie.value;
                    break;
                case KEY_START: keys[12] = ie.value;
                    break;
            }
    }
    if (keys[6]) {
        kcode[port] &= ~Btn_A;
    }
    if (keys[7]) {
        kcode[port] &= ~Btn_B;
    }
    if (keys[5]) {
        kcode[port] &= ~Btn_Y;
    }
    if (keys[8]) {
        kcode[port] &= ~Btn_X;
    }
    if (keys[1]) {
        kcode[port] &= ~DPad_Up;
    }
    if (keys[2]) {
        kcode[port] &= ~DPad_Down;
    }
    if (keys[3]) {
        kcode[port] &= ~DPad_Left;
    }
    if (keys[4]) {
        kcode[port] &= ~DPad_Right;
    }
    if (keys[12]) {
        kcode[port] &= ~Btn_Start;
    }
    if (keys[9]) {
        die("death by escape key");
    }
    if (keys[10]) rt[port] = 255;
    if (keys[11]) lt[port] = 255;

    return true;

#elif defined(TARGET_PANDORA)
    static int keys[13];
    while (read(kbfd, &ie, sizeof (ie)) == sizeof (ie)) {
        if (ie.type = EV_KEY)
            //printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
            switch (ie.code) {
                case KEY_SPACE: keys[0] = ie.value;
                    break;
                case KEY_UP: keys[1] = ie.value;
                    break;
                case KEY_DOWN: keys[2] = ie.value;
                    break;
                case KEY_LEFT: keys[3] = ie.value;
                    break;
                case KEY_RIGHT: keys[4] = ie.value;
                    break;
                case KEY_PAGEUP:keys[5] = ie.value;
                    break;
                case KEY_PAGEDOWN:keys[6] = ie.value;
                    break;
                case KEY_END: keys[7] = ie.value;
                    break;
                case KEY_HOME: keys[8] = ie.value;
                    break;
                case KEY_MENU: keys[9] = ie.value;
                    break;
                case KEY_RIGHTSHIFT: keys[10] = ie.value;
                    break;
                case KEY_RIGHTCTRL: keys[11] = ie.value;
                    break;
                case KEY_LEFTALT: keys[12] = ie.value;
                    break;
            }
    }

    if (keys[0]) {
        kcode[port] &= ~Btn_C;
    }
    if (keys[6]) {
        kcode[port] &= ~Btn_A;
    }
    if (keys[7]) {
        kcode[port] &= ~Btn_B;
    }
    if (keys[5]) {
        kcode[port] &= ~Btn_Y;
    }
    if (keys[8]) {
        kcode[port] &= ~Btn_X;
    }
    if (keys[1]) {
        kcode[port] &= ~DPad_Up;
    }
    if (keys[2]) {
        kcode[port] &= ~DPad_Down;
    }
    if (keys[3]) {
        kcode[port] &= ~DPad_Left;
    }
    if (keys[4]) {
        kcode[port] &= ~DPad_Right;
    }
    if (keys[12]) {
        kcode[port] &= ~Btn_Start;
    }
    if (keys[9]) {
        die("death by escape key");
    }
    if (keys[10]) rt[port] = 255;
    if (keys[11]) lt[port] = 255;

    return true;
#else
    while (read(kbfd, &ie, sizeof (ie)) == sizeof (ie)) {
        printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
    }
#endif
#endif
    return true;
}

bool HandleJoystick(u32 port) {

    // Joystick must be connected
    if (JoyFD[port] < 0) return false;

#if HOST_OS != OS_DARWIN
    struct js_event JE;
    while (read(JoyFD[port], &JE, sizeof (JE)) == sizeof (JE))
        if (JE.number < MAP_SIZE) {
            switch (JE.type & ~JS_EVENT_INIT) {
                case JS_EVENT_AXIS:
                {
                    u32 mt = JMapAxis[port][JE.number] >> 16;
                    u32 mo = JMapAxis[port][JE.number]&0xFFFF;
                    //Disabling all this string processing and console logging during emulation!
                    //				 printf("AXIS %d,%d\n",JE.number,JE.value);
                    s8 v = (s8) (JE.value / 256); //-127 ... + 127 range

                    if (mt == 0) {
                        kcode[port] |= mo;
                        kcode[port] |= mo * 2;
                        if (v<-64) {
                            kcode[port] &= ~mo;
                        } else if (v > 64) {
                            kcode[port] &= ~(mo * 2);
                        }

                        //					  printf("Mapped to %d %d %d\n",mo,kcode[port]&mo,kcode[port]&(mo*2));
                    } else if (mt == 1) {
                        if (v >= 0) v++; //up to 255

                        //					   printf("AXIS %d,%d Mapped to %d %d %d\n",JE.number,JE.value,mo,v,v+127);

                        if (mo == 0)
                            lt[port] = v + 127;
                        else if (mo == 1)
                            rt[port] = v + 127;
                    } else if (mt == 2) {
                        //					  printf("AXIS %d,%d Mapped to %d %d [%d]",JE.number,JE.value,mo,v);
                        if (mo == 0)
                            joyx[port] = v;
                        else if (mo == 1)
                            joyy[port] = v;
                    }
                }
                    break;

                case JS_EVENT_BUTTON:
                {
                    u32 mt = JMapBtn[port][JE.number] >> 16;
                    u32 mo = JMapBtn[port][JE.number]&0xFFFF;

                    //				 printf("BUTTON %d,%d\n",JE.number,JE.value);

                    if (mt == 0) {
                        //					  printf("Mapped to %d\n",mo);
                        if (JE.value)
                            kcode[port] &= ~mo;
                        else
                            kcode[port] |= mo;
                    } else if (mt == 1) {
                        //					  printf("Mapped to %d %d\n",mo,JE.value?255:0);
                        if ((mo == 2) && (port == 0) && (JE.value)) {
                            printf("Detected Port 0 Quit button!");
                            die("Dying an honorable death, via controller mapping.  QAPLA!!");
                        } else if (mo == 0) {
                            lt[port] = JE.value ? 255 : 0;
                        }
                        else if (mo == 1) {
                            rt[port] = JE.value ? 255 : 0;
                        }
                    }
                }
                    break;
            }
        }
#endif
    return true;

}

extern bool KillTex;

#ifdef TARGET_PANDORA

static Cursor CreateNullCursor(Display *display, Window root) {
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc = XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
            &dummycolour, &dummycolour, 0, 0);
    XFreePixmap(display, cursormask);
    XFreeGC(display, gc);
    return cursor;
}
#endif

int x11_dc_buttons = 0xFFFF;

void UpdateInputState(u32 port) {
    static char key = 0;

    //Try removing this block, seems like we would want to be able to hold down a damn button or trigger.... ~free5ty1e
#if defined(SUPPORT_X11)
    kcode[port] = x11_dc_buttons;
#endif
    //	rt[port]=0;
    //	lt[port]=0;

    //hardcoded for rapi. Really, some configuration would make more sense than this ~skmp
    HandleKb(port);
    HandleJoystick(port);
    return;

#if defined(TARGET_GCW0) || defined(TARGET_PANDORA)
#endif
    for (;;) {
        key = 0;
        read(STDIN_FILENO, &key, 1);

        if (0 == key || EOF == key) break;
        if ('k' == key) KillTex = true;

#ifdef TARGET_PANDORA
        if (' ' == key) {
            kcode[port] &= ~Btn_C;
        }
        if ('6' == key) {
            kcode[port] &= ~Btn_A;
        }
        if ('O' == key) {
            kcode[port] &= ~Btn_B;
        }
        if ('5' == key) {
            kcode[port] &= ~Btn_Y;
        }
        if ('H' == key) {
            kcode[port] &= ~Btn_X;
        }
        if ('A' == key) {
            kcode[port] &= ~DPad_Up;
        }
        if ('B' == key) {
            kcode[port] &= ~DPad_Down;
        }
        if ('D' == key) {
            kcode[port] &= ~DPad_Left;
        }
        if ('C' == key) {
            kcode[port] &= ~DPad_Right;
        }
#else
        if ('b' == key) {
            kcode[port] &= ~Btn_C;
        }
        if ('v' == key) {
            kcode[port] &= ~Btn_A;
        }
        if ('c' == key) {
            kcode[port] &= ~Btn_B;
        }
        if ('x' == key) {
            kcode[port] &= ~Btn_Y;
        }
        if ('z' == key) {
            kcode[port] &= ~Btn_X;
        }
        if ('i' == key) {
            kcode[port] &= ~DPad_Up;
        }
        if ('k' == key) {
            kcode[port] &= ~DPad_Down;
        }
        if ('j' == key) {
            kcode[port] &= ~DPad_Left;
        }
        if ('l' == key) {
            kcode[port] &= ~DPad_Right;
        }
#endif
        if (0x0A == key) {
            kcode[port] &= ~Btn_Start;
        }
#ifdef TARGET_PANDORA
        if ('q' == key) {
            die("death by escape key");
        }
#endif
        //if (0x1b == key){ die("death by escape key"); } //this actually quits when i press left for some reason

        if ('a' == key) rt[port] = 255;
        if ('s' == key) lt[port] = 255;
#if !defined(HOST_NO_REC)
        if ('b' == key) emit_WriteCodeCache();
        if ('n' == key) bm_Reset();
        if ('m' == key) bm_Sort();
        if (',' == key) {
            emit_WriteCodeCache();
            bm_Sort();
        }
#endif
    }
}

void os_DoEvents() {
#if defined(SUPPORT_X11)
    if (x11_win) {
        //Handle X11
        XEvent e;
        if (XCheckWindowEvent((Display*) x11_disp, (Window) x11_win, KeyPressMask | KeyReleaseMask, &e)) {
            switch (e.type) {

                case KeyPress:
                case KeyRelease:
                {
                    int dc_key = x11_keymap[e.xkey.keycode];

                    if (e.type == KeyPress)
                        x11_dc_buttons &= ~dc_key;
                    else
                        x11_dc_buttons |= dc_key;

                    //printf("KEY: %d -> %d: %d\n",e.xkey.keycode, dc_key, x11_dc_buttons );
                }
                    break;


                {
                    printf("KEYRELEASE\n");
                }
                    break;

            }
        }
    }
#endif
}

void os_SetWindowText(const char * text) {
    if (0 == x11_win || 0 == x11_disp || 1)
        printf("%s\n", text);
#if defined(SUPPORT_X11)
    else if (x11_win) {
        XChangeProperty((Display*) x11_disp, (Window) x11_win,
                XInternAtom((Display*) x11_disp, "WM_NAME", False), //WM_NAME,
                XInternAtom((Display*) x11_disp, "UTF8_STRING", False), //UTF8_STRING,
                8, PropModeReplace, (const unsigned char *) text, strlen(text));
    }
#endif
}


void* x11_glc;

int ndcid = 0;

void os_CreateWindow() {
#if defined(SUPPORT_X11)
    if (cfgLoadInt("pvr", "nox11", 0) == 0) {
        XInitThreads();
        // X11 variables
        Window x11Window = 0;
        Display* x11Display = 0;
        long x11Screen = 0;
        XVisualInfo* x11Visual = 0;
        Colormap x11Colormap = 0;

        /*
        Step 0 - Create a NativeWindowType that we can use it for OpenGL ES output
         */
        Window sRootWindow;
        XSetWindowAttributes sWA;
        unsigned int ui32Mask;
        int i32Depth;

        // Initializes the display and screen
        x11Display = XOpenDisplay(0);
        if (!x11Display && !(x11Display = XOpenDisplay(":0"))) {
            printf("Error: Unable to open X display\n");
            return;
        }
        x11Screen = XDefaultScreen(x11Display);

        // Gets the window parameters
        sRootWindow = RootWindow(x11Display, x11Screen);

        int depth = CopyFromParent;

#if !defined(GLES)
        // Get a matching FB config
        static int visual_attribs[] = {
            GLX_X_RENDERABLE, True,
            GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            GLX_STENCIL_SIZE, 8,
            GLX_DOUBLEBUFFER, True,
            //GLX_SAMPLE_BUFFERS  , 1,
            //GLX_SAMPLES         , 4,
            None
        };

        int glx_major, glx_minor;

        // FBConfigs were added in GLX version 1.3.
        if (!glXQueryVersion(x11Display, &glx_major, &glx_minor) ||
                ((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1)) {
            printf("Invalid GLX version");
            exit(1);
        }

        int fbcount;
        GLXFBConfig* fbc = glXChooseFBConfig(x11Display, x11Screen, visual_attribs, &fbcount);
        if (!fbc) {
            printf("Failed to retrieve a framebuffer config\n");
            exit(1);
        }
        printf("Found %d matching FB configs.\n", fbcount);

        GLXFBConfig bestFbc = fbc[ 0 ];
        XFree(fbc);

        // Get a visual
        XVisualInfo *vi = glXGetVisualFromFBConfig(x11Display, bestFbc);
        printf("Chosen visual ID = 0x%x\n", vi->visualid);


        depth = vi->depth;
        x11Visual = vi;

        x11Colormap = XCreateColormap(x11Display, RootWindow(x11Display, x11Screen), vi->visual, AllocNone);
#else
        i32Depth = DefaultDepth(x11Display, x11Screen);
        x11Visual = new XVisualInfo;
        XMatchVisualInfo(x11Display, x11Screen, i32Depth, TrueColor, x11Visual);
        if (!x11Visual) {
            printf("Error: Unable to acquire visual\n");
            return;
        }
        x11Colormap = XCreateColormap(x11Display, sRootWindow, x11Visual->visual, AllocNone);
#endif

        sWA.colormap = x11Colormap;

        // Add to these for handling other events
        sWA.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
        ui32Mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

#ifdef TARGET_PANDORA
        int width = 800;
        int height = 480;
#else
        int width = cfgLoadInt("x11", "width", WINDOW_WIDTH);
        int height = cfgLoadInt("x11", "height", WINDOW_HEIGHT);
#endif

        if (width == -1) {
            width = XDisplayWidth(x11Display, x11Screen);
            height = XDisplayHeight(x11Display, x11Screen);
        }

        // Creates the X11 window
        x11Window = XCreateWindow(x11Display, RootWindow(x11Display, x11Screen), (ndcid % 3)*640, (ndcid / 3)*480, width, height,
                0, depth, InputOutput, x11Visual->visual, ui32Mask, &sWA);
#ifdef TARGET_PANDORA
        // fullscreen
        Atom wmState = XInternAtom(x11Display, "_NET_WM_STATE", False);
        Atom wmFullscreen = XInternAtom(x11Display, "_NET_WM_STATE_FULLSCREEN", False);
        XChangeProperty(x11Display, x11Window, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *) &wmFullscreen, 1);

        XMapRaised(x11Display, x11Window);
#else
        XMapWindow(x11Display, x11Window);

#if !defined(GLES)

#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
        typedef GLXContext(*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

        glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
        glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
                glXGetProcAddressARB((const GLubyte *) "glXCreateContextAttribsARB");

        verify(glXCreateContextAttribsARB != 0);

        int context_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 1,
            GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };

        x11_glc = glXCreateContextAttribsARB(x11Display, bestFbc, 0, True, context_attribs);
        XSync(x11Display, False);

        if (!x11_glc) {
            die("Failed to create GL3.1 context\n");
        }
#endif
#endif
        XFlush(x11Display);



        //(EGLNativeDisplayType)x11Display;
        x11_disp = (void*) x11Display;
        x11_win = (void*) x11Window;
    } else
        printf("Not creating X11 window ..\n");
#endif
}

termios tios, orig_tios;

int setup_curses() {
    //initscr();
    //cbreak();
    //noecho();


    /* Get current terminal settings */
    if (tcgetattr(STDIN_FILENO, &orig_tios)) {
        printf("Error getting current terminal settings\n");
        return -1;
    }

    memcpy(&tios, &orig_tios, sizeof (struct termios));
    tios.c_lflag &= ~ICANON; //(ECHO|ICANON);&= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);

    tios.c_cc[VTIME] = 0;
    tios.c_cc[VMIN] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &tios)) {
        printf("Error applying terminal settings\n");
        return -2;
    }

    if (tcgetattr(STDIN_FILENO, &tios)) {
        tcsetattr(0, TCSANOW, &orig_tios);
        printf("Error while asserting terminal settings\n");
        return -3;
    }

    if ((tios.c_lflag & ICANON) || !(tios.c_lflag & ECHO)) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_tios);
        printf("Could not apply all terminal settings\n");
        return -4;
    }

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    return 1;
}

void common_linux_setup();
int dc_init(int argc, wchar* argv[]);
void dc_run();

#ifdef TARGET_PANDORA
void gl_term();

void clean_exit(int sig_num) {
    void *array[10];
    size_t size;

    // close files
    if (JoyFD[0] >= 0) close(JoyFD[0]);
    if (JoyFD[1] >= 0) close(JoyFD[1]);
    if (JoyFD[2] >= 0) close(JoyFD[2]);
    if (JoyFD[3] >= 0) close(JoyFD[3]);
    if (kbfd >= 0) close(kbfd);
    if (audio_fd >= 0) close(audio_fd);

    // Close EGL context ???
    if (sig_num != 0)
        gl_term();

    // close XWindow
    if (x11_win) {
        XDestroyWindow(x11_disp, x11_win);
        x11_win = 0;
    }
    if (x11_disp) {
        XCloseDisplay(x11_disp);
        x11_disp = 0;
    }

    // finish cleaning
    if (sig_num != 0) {
        write(2, "\nSignal received\n", sizeof ("\nSignal received\n"));

        size = backtrace(array, 10);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    }
}

void init_sound() {
    if ((audio_fd = open("/dev/dsp", O_WRONLY)) < 0) {
        printf("Couldn't open /dev/dsp.\n");
    } else {
        printf("sound enabled, dsp openned for write\n");
        int tmp = 44100;
        int err_ret;
        err_ret = ioctl(audio_fd, SNDCTL_DSP_SPEED, &tmp);
        printf("set Frequency to %i, return %i (rate=%i)\n", 44100, err_ret, tmp);
        int channels = 2;
        err_ret = ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &channels);
        printf("set dsp to stereo (%i => %i)\n", channels, err_ret);
        int format = AFMT_S16_LE;
        err_ret = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format);
        printf("set dsp to %s audio (%i/%i => %i)\n", "16bits signed", AFMT_S16_LE, format, err_ret);
    }
}
#endif

#ifdef TARGET_RPI

#include <sys/soundcard.h>

static int audio_fd = -1;

void init_sound() {
    if ((audio_fd = open("/dev/dsp", O_WRONLY)) < 0)
        printf("Couldn't open /dev/dsp.\n");
    else {
        printf("sound enabled, dsp openned for write\n");
        int tmp = 44100;
        int err_ret;
        err_ret = ioctl(audio_fd, SNDCTL_DSP_SPEED, &tmp);
        printf("set Frequency to %i, return %i (rate=%i)\n", 44100, err_ret, tmp);
        int channels = 2;
        err_ret = ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &channels);
        printf("set dsp to stereo (%i => %i)\n", channels, err_ret);
        int format = AFMT_S16_LE;
        err_ret = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format);
        printf("set dsp to %s audio (%i/%i => %i)\n", "16bits signed", AFMT_S16_LE, format, err_ret);
    }
}

void clean_exit(int sig_num) {
    if (audio_fd >= 0) close(audio_fd);
}

#endif

int main(int argc, wchar* argv[]) {
    bcm_host_init();
    //if (argc==2) 
    //ndcid=atoi(argv[1]);

    printf("Personality: %08X\n", personality(0xFFFFFFFF));
    personality(~READ_IMPLIES_EXEC & personality(0xFFFFFFFF));
    printf("Updated personality: %08X\n", personality(0xFFFFFFFF));
    if (setup_curses() < 0) die("failed to setup curses!\n");
#if defined  TARGET_PANDORA || defined TARGET_RPI
    signal(SIGSEGV, clean_exit);
    signal(SIGKILL, clean_exit);

    init_sound();
#else
    void os_InitAudio();
    os_InitAudio();
#endif

#if defined(USES_HOMEDIR) && HOST_OS != OS_DARWIN
    string home = (string) getenv("HOME");
    if (home.c_str()) {
        home += "/.reicast";
        mkdir(home.c_str(), 0755); // create the directory if missing
        SetHomeDir(home);
    } else
        SetHomeDir(".");
#else
    SetHomeDir(".");
#endif

#if defined(SUPPORT_X11)
    x11_keymap[113] = DPad_Left;
    x11_keymap[114] = DPad_Right;

    x11_keymap[111] = DPad_Up;
    x11_keymap[116] = DPad_Down;

    x11_keymap[52] = Btn_Y;
    x11_keymap[53] = Btn_X;
    x11_keymap[54] = Btn_B;
    x11_keymap[55] = Btn_A;

    /*
            //TODO: Fix sliders
            x11_keymap[38] = DPad_Down;
            x11_keymap[39] = DPad_Down;
     */

    x11_keymap[36] = Btn_Start;
#endif

    printf("Home dir is: %s\n", GetPath("/").c_str());

    common_linux_setup();

    SetupInput();

    settings.profile.run_counts = 0;

    dc_init(argc, argv);

    dc_run();

#if defined  TARGET_PANDORA || defined TARGET_RPI
    clean_exit(0);
#endif

    return 0;
}

u32 alsa_Push(void* frame, u32 samples, bool wait);

u32 os_Push(void* frame, u32 samples, bool wait) {
#ifdef TARGET_PANDORA
    int audio_fd = -1;
#endif

    if (audio_fd > 0) {
        write(audio_fd, frame, samples * 4);
    } else {
        return alsa_Push(frame, samples, wait);
    }

    return 1;
}
#endif

int get_mic_data(u8* buffer) {
    return 0;
}

int push_vmu_screen(u8* buffer) {
    return 0;
}

void os_DebugBreak() {
    raise(SIGTRAP);
}
