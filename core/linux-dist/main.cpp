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
#include <sys/mman.h>
#include <sys/time.h>
#include "hw/sh4/dyna/blockmanager.h"
#include <unistd.h>




#if defined(SUPPORT_X11)
	#include <X11/Xlib.h>
	#include <X11/Xatom.h>
	#include <X11/Xutil.h>
	#include <X11/XKBlib.h>

	#if !defined(GLES)
		#include <GL/gl.h>
		#include <GL/glx.h>
	#endif
	
	#include <map>
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

void* x11_win=0,* x11_disp=0;
void* libPvr_GetRenderTarget() 
{ 
	return x11_win; 
}

void* libPvr_GetRenderSurface() 
{ 
	return x11_disp;
}

int msgboxf(const wchar* text,unsigned int type,...)
{
	va_list args;

	wchar temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);

	//printf(NULL,temp,VER_SHORTNAME,type | MB_TASKMODAL);
	puts(temp);
	return MBX_OK;
}

// Global variables storing the input info for 4 ports
// Used in external files as well
u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];


// Values of each possible input
enum DCPad {
	Btn_C		= 1,
	Btn_B		= 1<<1,
	Btn_A		= 1<<2,
	Btn_Start	= 1<<3,
	DPad_Up		= 1<<4,
	DPad_Down	= 1<<5, 
	DPad_Left	= 1<<6,
	DPad_Right	= 1<<7,
	Btn_Z		= 1<<8,
	Btn_Y		= 1<<9,
	Btn_X		= 1<<10,
	Btn_D		= 1<<11,
	DPad2_Up	= 1<<12,
	DPad2_Down	= 1<<13,
	DPad2_Left	= 1<<14,
	DPad2_Right	= 1<<15,

	Axis_LT= 0x10000,
	Axis_RT= 0x10001,
	Axis_X= 0x20000,
	Axis_Y= 0x20001,
};


#if defined(SUPPORT_X11) && !defined(TARGET_PANDORA)

/* Dreamcast controller structure
   represents the current state of each controller:
    - value of each axis
    - state of each button
*/
typedef struct{
	int buttons;
	s8 axis_x;
	s8 axis_y;
	u8 axis_lt;
	u8 axis_rt;
} dc_controller;


// Name of the sections in the configuration file 
// corresponding to each controller
char const * config_section_names_keyboard[4] = {
	"control1",
	"control2",
	"control3",
	"control4"
};


// Structure to store the mapping of each controller using a keyboard
typedef struct{
	// Analog simulation
   	int key_axis_up;
   	int key_axis_down;
   	int key_axis_left;
   	int key_axis_right;
   	int key_axis_lt;
   	int key_axis_rt;
	// Button mapping
	map<int, int> keymap;
} keyboard_map;

// Structure to store a joystick
typedef struct{
	int file_descriptor;
	int axis_count;
	int button_count;
	char name[128];
} joystick;

// Structure to store the mapping of each controller using a joystick
typedef struct{
	joystick js;
	// Axis mapping
   	int id_axis_x;
   	int id_axis_y;
   	int id_axis_dpad_x;
   	int id_axis_dpad_y;
   	int id_axis_lt;
   	int id_axis_rt;
	// Button mapping
	map<int, int> button_map;
} joystick_map;

// Global variables
// Dreamcast controllers
dc_controller dc_pads[4];
// Keyboard mappings
keyboard_map kb_maps[4];
// Joysticks
joystick js[4];
// Joystick mappings
joystick_map js_maps[4];


/* Initializes a dreamcast controller by setting all axis to 0 and
   releasing all buttons */
void setup_dc_controller(dc_controller & dc_pad)
{
	dc_pad.buttons = 0xFFFF;
	dc_pad.axis_x = 0;
	dc_pad.axis_y = 0;
	dc_pad.axis_lt = 0;
	dc_pad.axis_rt = 0;
}

/* Loads a keyboard map from a section in the configuration file */
void load_keyboard_map(keyboard_map & kb_map, char const * section_name)
{
	if (cfgOpen())
	{
		// Loading analog keys
		kb_map.key_axis_up = cfgLoadInt(section_name, "keyboard.axis_up", -1);
		kb_map.key_axis_down = cfgLoadInt(section_name, "keyboard.axis_down", -1);
		kb_map.key_axis_left = cfgLoadInt(section_name, "keyboard.axis_left", -1);
		kb_map.key_axis_right = cfgLoadInt(section_name, "keyboard.axis_right", -1);
		kb_map.key_axis_lt = cfgLoadInt(section_name, "keyboard.axis_lt", -1);
		kb_map.key_axis_rt = cfgLoadInt(section_name, "keyboard.axis_rt", -1);

		// Loading buttons keys
		unsigned int dpad_up = cfgLoadInt(section_name, "keyboard.dpad_up", -1);
		unsigned int dpad_down = cfgLoadInt(section_name, "keyboard.dpad_down", -1);
		unsigned int dpad_left = cfgLoadInt(section_name, "keyboard.dpad_left", -1);
		unsigned int dpad_right = cfgLoadInt(section_name, "keyboard.dpad_right", -1);
		unsigned int button_A = cfgLoadInt(section_name, "keyboard.A", -1);
		unsigned int button_B = cfgLoadInt(section_name, "keyboard.B", -1);
		unsigned int button_C = cfgLoadInt(section_name, "keyboard.C", -1);
		unsigned int button_X = cfgLoadInt(section_name, "keyboard.X", -1);
		unsigned int button_Y = cfgLoadInt(section_name, "keyboard.Y", -1);
		unsigned int button_Z = cfgLoadInt(section_name, "keyboard.Z", -1);
		unsigned int button_start = cfgLoadInt(section_name, "keyboard.start", -1);

		kb_map.keymap[dpad_up] = DPad_Up;
		kb_map.keymap[dpad_down] = DPad_Down;
		kb_map.keymap[dpad_left] = DPad_Left;
		kb_map.keymap[dpad_right] = DPad_Right;
		kb_map.keymap[button_A] = Btn_A;
		kb_map.keymap[button_B] = Btn_B;
		kb_map.keymap[button_C] = Btn_C;
		kb_map.keymap[button_X] = Btn_X;
		kb_map.keymap[button_Y] = Btn_Y;
		kb_map.keymap[button_Z] = Btn_Z;
		kb_map.keymap[button_start] = Btn_Start;

	}
}


/* Loads a joystick given a system path */
void load_joystick(joystick & js, char const * path)
{
	js.file_descriptor = open(path, O_RDONLY);
	js.axis_count = 0;
	js.button_count = 0;
	js.name[0] = '\0';
	if (js.file_descriptor >= 0)
	{
		fcntl(js.file_descriptor, F_SETFL, O_NONBLOCK);
		ioctl(js.file_descriptor,JSIOCGAXES,&js.axis_count);
		ioctl(js.file_descriptor,JSIOCGBUTTONS,&js.button_count);
		ioctl(js.file_descriptor,JSIOCGNAME(sizeof(js.name)),&js.name);
		printf("SDK: Found '%s' joystick with %d axis and %d buttons\n",js.name,js.axis_count,js.button_count);
	}
}

/* Loads a joystick map from a section in the configuration file */
void load_joystick_map(joystick_map & js_map, char const * section_name)
{
	// Initialize the file descriptor for the current joystick in case 
	// no joystick is found
	js_map.js.file_descriptor = -1;

	if (cfgOpen())
	{
		// Find the desired controller's name
		char target_name[128];
		cfgLoadStr(section_name, "controller.name", target_name, "None");
		printf("Using following controller: %s\n", target_name);
		// We find the corresponding controller
		for (int i = 0; i <= 3; ++i)
		{
			// We check if the names match
			if (strcmp(js[i].name, target_name) == 0)
			{
				js_map.js = js[i];
				// We erase the name so the same controller is not used twice
				js[i].name[0] = '\0';
				// And we stop looking for other controllers
				break;
			}
		}

		if (js_map.js.file_descriptor < 0)
		{
			// If no controller was found, no need to load the configuration
			return;
		}

		// Loading axis mapping
		int id_axis_x = cfgLoadInt(section_name, "js.axis_x", -1);
		int id_axis_y = cfgLoadInt(section_name, "js.axis_y", -1);
		int id_axis_lt = cfgLoadInt(section_name, "js.axis_lt", -1);
		int id_axis_rt = cfgLoadInt(section_name, "js.axis_rt", -1);
		int id_axis_dpad_x = cfgLoadInt(section_name, "js.axis_dpad_x", -1);
		int id_axis_dpad_y = cfgLoadInt(section_name, "js.axis_dpad_y", -1);
	
		js_map.id_axis_x = id_axis_x;
		js_map.id_axis_y = id_axis_y;
		js_map.id_axis_lt = id_axis_lt;
		js_map.id_axis_rt = id_axis_rt;
		js_map.id_axis_dpad_x = id_axis_dpad_x;
		js_map.id_axis_dpad_y = id_axis_dpad_y;

		// Loading buttons mapping
		int button_A = cfgLoadInt(section_name, "js.button_A", -1);
		int button_B = cfgLoadInt(section_name, "js.button_B", -1);
		int button_C = cfgLoadInt(section_name, "js.button_C", -1);
		int button_X = cfgLoadInt(section_name, "js.button_X", -1);
		int button_Y = cfgLoadInt(section_name, "js.button_Y", -1);
		int button_Z = cfgLoadInt(section_name, "js.button_Z", -1);
		int button_start = cfgLoadInt(section_name, "js.button_start", -1);

		js_map.button_map[button_A] = Btn_A;
		js_map.button_map[button_B] = Btn_B;
		js_map.button_map[button_C] = Btn_C;
		js_map.button_map[button_X] = Btn_X;
		js_map.button_map[button_Y] = Btn_Y;
		js_map.button_map[button_Z] = Btn_Z;
		js_map.button_map[button_start] = Btn_Start;
	}
}

/* Reports the state of a controller (axis / buttons) to the global variables */
inline void report_controller_state(int port)
{
	joyx[port] = dc_pads[port].axis_x;
	joyy[port] = dc_pads[port].axis_y;
	lt[port] = dc_pads[port].axis_lt;
	rt[port] = dc_pads[port].axis_rt;
	kcode[port] = dc_pads[port].buttons;
}
#endif


void emit_WriteCodeCache();

static int kbfd = -1; 
#ifdef TARGET_PANDORA
static int audio_fd = -1;
#endif


#define MAP_SIZE 32


void SetupInput()
{
	for (int port=0;port<4;port++)
	{
		kcode[port]=0xFFFF;
		rt[port]=0;
		lt[port]=0;
	}

#if HOST_OS != OS_DARWIN
	if (true) {
		#ifdef TARGET_PANDORA
		const char* device = "/dev/input/event4";
		#else
		const char* device = "/dev/event2";
		#endif
		char name[256]= "Unknown";

		if ((kbfd = open(device, O_RDONLY)) > 0) {
			fcntl(kbfd,F_SETFL,O_NONBLOCK);
			if(ioctl(kbfd, EVIOCGNAME(sizeof(name)), name) < 0) {
				perror("evdev ioctl");
			}

			printf("The device on %s says its name is %s\n",device, name);

		}
		else
			perror("evdev open");
	}
#endif
	
#if HOST_OS != OS_DARWIN


	#if defined(SUPPORT_X11) && !defined(TARGET_PANDORA)		
	for (int port=0;port<4;port++)
	{
		// Setup the dreamcast controllers
		setup_dc_controller(dc_pads[port]);

		// Load keyboard mappings
		load_keyboard_map(kb_maps[port], config_section_names_keyboard[port]);
	}
	#endif

	// Open joystick devices
	load_joystick(js[0], "/dev/input/js0");
	load_joystick(js[1], "/dev/input/js1");
	load_joystick(js[2], "/dev/input/js2");
	load_joystick(js[3], "/dev/input/js3");

	// Load joystick mappings
	for (int port=0;port<4;port++)
	{
		load_joystick_map(js_maps[port], config_section_names_keyboard[port]);
	}
#endif
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
	while(read(kbfd,&ie,sizeof(ie))==sizeof(ie)) {
		//printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
		if (ie.type=EV_KEY)
		switch (ie.code) {
			case KEY_SELECT:	keys[9]=ie.value; break;
			case KEY_UP:	keys[1]=ie.value; break;
			case KEY_DOWN:	keys[2]=ie.value; break;
			case KEY_LEFT:	keys[3]=ie.value; break;
			case KEY_RIGHT:	keys[4]=ie.value; break;
			case KEY_Y:keys[5]=ie.value; break;
			case KEY_B:keys[6]=ie.value; break;
			case KEY_A:	keys[7]=ie.value; break;
			case KEY_X:	keys[8]=ie.value; break;
			case KEY_START:		keys[12]=ie.value; break;
		}
	}
	if (keys[6]) { kcode[port] &= ~Btn_A; }
	if (keys[7]) { kcode[port] &= ~Btn_B; }
	if (keys[5]) { kcode[port] &= ~Btn_Y; }
	if (keys[8]) { kcode[port] &= ~Btn_X; }
	if (keys[1]) { kcode[port] &= ~DPad_Up;    }
	if (keys[2]) { kcode[port] &= ~DPad_Down;  }
	if (keys[3]) { kcode[port] &= ~DPad_Left;  }
	if (keys[4]) { kcode[port] &= ~DPad_Right; }
	if (keys[12]){ kcode[port] &= ~Btn_Start; }
	if (keys[9]){ die("death by escape key"); } 
	if (keys[10]) rt[port]=255;
	if (keys[11]) lt[port]=255;
	
	return true;

	#elif defined(TARGET_PANDORA)
	static int keys[13];
	while(read(kbfd,&ie,sizeof(ie))==sizeof(ie)) {
		if (ie.type=EV_KEY)
		//printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
		switch (ie.code) {
			case KEY_SPACE: keys[0]=ie.value; break;
			case KEY_UP:	keys[1]=ie.value; break;
			case KEY_DOWN:	keys[2]=ie.value; break;
			case KEY_LEFT:	keys[3]=ie.value; break;
			case KEY_RIGHT:	keys[4]=ie.value; break;
			case KEY_PAGEUP:keys[5]=ie.value; break;
			case KEY_PAGEDOWN:keys[6]=ie.value; break;
			case KEY_END:	keys[7]=ie.value; break;
			case KEY_HOME:	keys[8]=ie.value; break;
			case KEY_MENU:		keys[9]=ie.value; break;
			case KEY_RIGHTSHIFT:	keys[10]=ie.value; break;
			case KEY_RIGHTCTRL:	keys[11]=ie.value; break;
			case KEY_LEFTALT:		keys[12]=ie.value; break;
		}
	}
			
	if (keys[0]) { kcode[port] &= ~Btn_C; }
	if (keys[6]) { kcode[port] &= ~Btn_A; }
	if (keys[7]) { kcode[port] &= ~Btn_B; }
	if (keys[5]) { kcode[port] &= ~Btn_Y; }
	if (keys[8]) { kcode[port] &= ~Btn_X; }
	if (keys[1]) { kcode[port] &= ~DPad_Up;    }
	if (keys[2]) { kcode[port] &= ~DPad_Down;  }
	if (keys[3]) { kcode[port] &= ~DPad_Left;  }
	if (keys[4]) { kcode[port] &= ~DPad_Right; }
	if (keys[12]){ kcode[port] &= ~Btn_Start; }
	if (keys[9]){ die("death by escape key"); } 
	if (keys[10]) rt[port]=255;
	if (keys[11]) lt[port]=255;
	
	return true;
	#else
  	while(read(kbfd,&ie,sizeof(ie))==sizeof(ie)) {
		printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
	}
	#endif
#endif
return true;
}

bool HandleJoystick(u32 port)
{


#if HOST_OS != OS_DARWIN
  // Joystick must be connected
  if (js_maps[port].js.file_descriptor < 0) return false;
  struct js_event JE;
  while(read(js_maps[port].js.file_descriptor,&JE,sizeof(JE))==sizeof(JE))
  {
  	if ((JE.type & ~JS_EVENT_INIT) == JE.type)
  	{
  		// Index of the axis / button
  		int id = JE.number;
  		// Value of the axis
  		int value = JE.value;
  		// Signed value in [-128, 127] (for direction axis)
  		s8 signed_value = (s8)(value / 256);
  		// Unsigned value in [0, 255] (for trigger axis)
  		u8 unsigned_value = signed_value + 127;
  		switch(JE.type)
  		{
  			// Axis handling
  			case JS_EVENT_AXIS:
  				// Direction stick
  				if(id == js_maps[port].id_axis_x)
  				{
  					dc_pads[port].axis_x = signed_value;
				}
				else if (id == js_maps[port].id_axis_y)
				{
					dc_pads[port].axis_y = signed_value;
				}

				// DPad (Handled as an axis by computer, but as buttons by the Dreamcast)
  				else if (id == js_maps[port].id_axis_dpad_x)
  				{
  					if (signed_value < 0)
  					{
  						dc_pads[port].buttons &= ~DPad_Left;
  					}
  					else if (signed_value > 0)
  					{
  						dc_pads[port].buttons &= ~DPad_Right;
  					}
  					else
  					{
  					 	dc_pads[port].buttons |= DPad_Left;
  					 	dc_pads[port].buttons |= DPad_Right;
  					}
  				}
  				else if (id == js_maps[port].id_axis_dpad_y)
  				{
  					if (signed_value < 0)
  					{
  						dc_pads[port].buttons &= ~DPad_Up;
  					}
  					else if (signed_value > 0)
  					{
  						dc_pads[port].buttons &= ~DPad_Down;
  					}
  					else
  					{
  					 	dc_pads[port].buttons |= DPad_Up;
  					 	dc_pads[port].buttons |= DPad_Down;
  					}
  				}

  				// Triggers
				else if (id == js_maps[port].id_axis_lt)
				{
  					dc_pads[port].axis_lt = unsigned_value;
  				}
  				else if (id == js_maps[port].id_axis_rt)
  				{
  					dc_pads[port].axis_rt = unsigned_value;
  				}
  				break;

  			// Button handling
  			case JS_EVENT_BUTTON:
  				dc_pads[port].buttons ^= js_maps[port].button_map[id];
  				break;
  			default:
  				break;
  		}
  	}
  }

  return true;
#endif
  // OS == DARWIN, no joystick support
  return false;

}

extern bool KillTex;

#ifdef TARGET_PANDORA
static Cursor CreateNullCursor(Display *display, Window root)
{
	Pixmap cursormask; 
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;
	
	cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
	xgc.function = GXclear;
	gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
	XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	cursor = XCreatePixmapCursor(display, cursormask, cursormask,
	&dummycolour,&dummycolour, 0,0);
	XFreePixmap(display,cursormask);
	XFreeGC(display,gc);
	return cursor;
}
#endif

int x11_dc_buttons[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

void UpdateInputState(u32 port)
{
	static char key = 0;

	kcode[port]= x11_dc_buttons[port];
	rt[port]=0;
	lt[port]=0;
	
#if defined(TARGET_GCW0) || defined(TARGET_PANDORA)
	HandleJoystick(port);
	HandleKb(port);
return;
#elif defined(SUPPORT_X11)
	// kcode[port] = 0xFFFF;
	// Attempt to get input from a controller
	if (!HandleJoystick(port))
	{
		// Fallback to keyboard if no controller found
		HandleKb(port);
	}
	// Report the current buttons to the emulator
  	report_controller_state(port);
	return;
#endif
	for(;;)
	{
		key = 0;
		read(STDIN_FILENO, &key, 1);

		if (0  == key || EOF == key) break;
		if ('k' == key) KillTex=true;

#ifdef TARGET_PANDORA
		if (' ' == key) { kcode[port] &= ~Btn_C; }
		if ('6' == key) { kcode[port] &= ~Btn_A; }
		if ('O' == key) { kcode[port] &= ~Btn_B; }
		if ('5' == key) { kcode[port] &= ~Btn_Y; }
		if ('H' == key) { kcode[port] &= ~Btn_X; }
		if ('A' == key) { kcode[port] &= ~DPad_Up;    }
		if ('B' == key) { kcode[port] &= ~DPad_Down;  }
		if ('D' == key) { kcode[port] &= ~DPad_Left;  }
		if ('C' == key) { kcode[port] &= ~DPad_Right; }
#else
		if ('b' == key) { kcode[port] &= ~Btn_C; }
		if ('v' == key) { kcode[port] &= ~Btn_A; }
		if ('c' == key) { kcode[port] &= ~Btn_B; }
		if ('x' == key) { kcode[port] &= ~Btn_Y; }
		if ('z' == key) { kcode[port] &= ~Btn_X; }
		if ('i' == key) { kcode[port] &= ~DPad_Up;    }
		if ('k' == key) { kcode[port] &= ~DPad_Down;  }
		if ('j' == key) { kcode[port] &= ~DPad_Left;  }
		if ('l' == key) { kcode[port] &= ~DPad_Right; }
#endif
		if (0x0A== key) { kcode[port] &= ~Btn_Start;  }
#ifdef TARGET_PANDORA
		if ('q' == key){ die("death by escape key"); } 
#endif
		//if (0x1b == key){ die("death by escape key"); } //this actually quits when i press left for some reason

		if ('a' == key) rt[port]=255;
		if ('s' == key) lt[port]=255;
#if !defined(HOST_NO_REC)
		if ('b' == key)	emit_WriteCodeCache();
		if ('n' == key)	bm_Reset();
		if ('m' == key)	bm_Sort();
		if (',' == key)	{ emit_WriteCodeCache(); bm_Sort(); }
#endif
	}
}

void os_DoEvents()
{

	#if defined(SUPPORT_X11)
	static bool ana_up[4] = {false, false, false, false};
	static bool ana_down[4] = {false, false, false, false};
	static bool ana_left[4] = {false, false, false, false};
	static bool ana_right[4] = {false, false, false, false};

	if (x11_win) {
		//Handle X11
		XEvent e;
		if(XCheckWindowEvent((Display*)x11_disp, (Window)x11_win, KeyPressMask | KeyReleaseMask, &e)){
			switch(e.type){

				case KeyPress:
				case KeyRelease:
				{
                    
					for (int port = 0; port <= 3; ++port)
					{
						if (js_maps[port].js.file_descriptor > 0)
						{
							continue;
						}
                    	//Detect up press
                    	if(e.xkey.keycode == kb_maps[port].key_axis_up)
                    	{
	                        if(e.type == KeyPress)
                        	{    
	                	        ana_up[port] = true;
                        	}
                        	else if(e.type == KeyRelease)
                        	{
	                            ana_up[port] = false;
                        	}
                        	else
                        	{
                        	}
                    	} else {

                    	}
                    
                    	//Detect down Press
                    	if(e.xkey.keycode == kb_maps[port].key_axis_down)
                    	{
	                        if(e.type == KeyPress)
                        	{    
	                            ana_down[port] = true;
                        	}
                        	else if(e.type == KeyRelease)
                        	{
	                            ana_down[port] = false;
                        	}
                        	else
                        	{
                        	}
                    	}
  
                    	//Detect left press
                    	if(e.xkey.keycode == kb_maps[port].key_axis_left)
                    	{
	                        if(e.type == KeyPress)
                        	{    
	                            ana_left[port] = true;
                        	}
                        	else if(e.type == KeyRelease)
                        	{
	                            ana_left[port] = false;
                        	}
                        	else
                        	{
                        	}
                    	} 
                        
                    	//Detect right Press
                    	if(e.xkey.keycode == kb_maps[port].key_axis_right)
                    	{
	                        if(e.type == KeyPress)
                        	{    
	                            ana_right[port] = true;
                        	}
                        	else if(e.type == KeyRelease)
                        	{
	                            ana_right[port] = false;
                        	}
                        	else
                        	{
                        	}
                    	}
                        
                    	//detect LT press
                    	if (e.xkey.keycode == kb_maps[port].key_axis_lt)
                    	{
	                        if (e.type == KeyPress)
                        	{    
	                            dc_pads[port].axis_lt = 255;
                        	}
                        	else if (e.type == KeyRelease)
                        	{
	                            dc_pads[port].axis_lt = 0;
                        	}
                        	else
                        	{
                        	}
                    	}
                        
                    	//detect RT press
                    	if (e.xkey.keycode == kb_maps[port].key_axis_rt)
                    	{
	                        if (e.type == KeyPress)
                        	{    
	                            dc_pads[port].axis_rt = 255;
                        	}
                        	else if (e.type == KeyRelease)
                        	{
	                            dc_pads[port].axis_rt = 0;
                        	}
                        	else
                        	{
                        	}
                    	}
   
						int dc_key = kb_maps[port].keymap[e.xkey.keycode];

						if (e.type == KeyPress)
							dc_pads[port].buttons &= ~dc_key;
						else
							dc_pads[port].buttons |= dc_key;
					}

				}
				break;

				default:
				{
					printf("KEYRELEASE\n");
				}
				break;

			}
        }
            
        /* Check analogue control states (up/down) */
        for (int port = 0; port <= 3; ++port)
		{
			if (js_maps[port].js.file_descriptor > 0)
			{
				continue;
			}

			// X axis
	        if((ana_up[port] == true) && (ana_down[port] == false))
        	{
	            dc_pads[port].axis_y = -127;
        	}
        	else if((ana_up[port] == false) && (ana_down[port] == true))
        	{
	            dc_pads[port].axis_y = 127;
        	}
        	else
        	{
	            /* Either both pressed simultaniously or neither pressed */
            	dc_pads[port].axis_y = 0;
        	}
	            
        	// Y axis
        	if((ana_left[port] == true) && (ana_right[port] == false))
        	{
	            dc_pads[port].axis_x = -127;
        	}
        	else if((ana_left[port] == false) && (ana_right[port] == true))
        	{
	            dc_pads[port].axis_x = 127;
        	}
        	else
        	{
	            /* Either both pressed simultaniously or neither pressed */
            	dc_pads[port].axis_x = 0;
        	}       
        }
	}
    #endif
}

void os_SetWindowText(const char * text)
{
	if (0==x11_win || 0==x11_disp || 1)
		printf("%s\n",text);
#if defined(SUPPORT_X11)
	else if (x11_win) {
		XChangeProperty((Display*)x11_disp, (Window)x11_win,
			XInternAtom((Display*)x11_disp, "WM_NAME",		False),		//WM_NAME,
			XInternAtom((Display*)x11_disp, "UTF8_STRING",	False),		//UTF8_STRING,
			8, PropModeReplace, (const unsigned char *)text, strlen(text));
	}
#endif
}


void* x11_glc;

int ndcid=0;
void os_CreateWindow()
{
#if defined(SUPPORT_X11)

	// Variables regarding keyboard auto-repeat
	Bool ar_set, ar_supp = false;

	if (cfgLoadInt("pvr","nox11",0)==0)
		{
			XInitThreads();
			// X11 variables
			Window				x11Window	= 0;
			Display*			x11Display	= 0;
			long				x11Screen	= 0;
			XVisualInfo*		x11Visual	= 0;
			Colormap			x11Colormap	= 0;

			/*
			Step 0 - Create a NativeWindowType that we can use it for OpenGL ES output
			*/
			Window					sRootWindow;
			XSetWindowAttributes	sWA;
			unsigned int			ui32Mask;
			int						i32Depth;

			// Initializes the display and screen
			x11Display = XOpenDisplay( 0 );
			if (!x11Display && !(x11Display = XOpenDisplay( ":0" )))
			{
				printf("Error: Unable to open X display\n");
				return;
			}
			x11Screen = XDefaultScreen( x11Display );

			// Gets the window parameters
			sRootWindow = RootWindow(x11Display, x11Screen);
			
			int depth = CopyFromParent;

			#if !defined(GLES)
				// Get a matching FB config
				static int visual_attribs[] =
				{
					GLX_X_RENDERABLE    , True,
					GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
					GLX_RENDER_TYPE     , GLX_RGBA_BIT,
					GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
					GLX_RED_SIZE        , 8,
					GLX_GREEN_SIZE      , 8,
					GLX_BLUE_SIZE       , 8,
					GLX_ALPHA_SIZE      , 8,
					GLX_DEPTH_SIZE      , 24,
					GLX_STENCIL_SIZE    , 8,
					GLX_DOUBLEBUFFER    , True,
					//GLX_SAMPLE_BUFFERS  , 1,
					//GLX_SAMPLES         , 4,
					None
				};

				int glx_major, glx_minor;

				// FBConfigs were added in GLX version 1.3.
				if ( !glXQueryVersion( x11Display, &glx_major, &glx_minor ) || 
				( ( glx_major == 1 ) && ( glx_minor < 3 ) ) || ( glx_major < 1 ) )
				{
					printf("Invalid GLX version");
					exit(1);
				}

				int fbcount;
				GLXFBConfig* fbc = glXChooseFBConfig(x11Display, x11Screen, visual_attribs, &fbcount);
				if (!fbc)
				{
					printf( "Failed to retrieve a framebuffer config\n" );
					exit(1);
				}
				printf( "Found %d matching FB configs.\n", fbcount );

				GLXFBConfig bestFbc = fbc[ 0 ];
				XFree( fbc );
 
				// Get a visual
				XVisualInfo *vi = glXGetVisualFromFBConfig( x11Display, bestFbc );
				printf( "Chosen visual ID = 0x%x\n", vi->visualid );


				depth = vi->depth;
				x11Visual = vi;

				x11Colormap = XCreateColormap(x11Display, RootWindow(x11Display, x11Screen), vi->visual, AllocNone);
			#else
				i32Depth = DefaultDepth(x11Display, x11Screen);
				x11Visual = new XVisualInfo;
				XMatchVisualInfo( x11Display, x11Screen, i32Depth, TrueColor, x11Visual);
				if (!x11Visual)
				{
					printf("Error: Unable to acquire visual\n");
					return;
				}
				x11Colormap = XCreateColormap( x11Display, sRootWindow, x11Visual->visual, AllocNone );
			#endif

			sWA.colormap = x11Colormap;

			// Add to these for handling other events
			sWA.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
			ui32Mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

			#ifdef TARGET_PANDORA
			int width=800;
			int height=480;
			#else
			int width=cfgLoadInt("x11","width", WINDOW_WIDTH);
			int height=cfgLoadInt("x11","height", WINDOW_HEIGHT);
			#endif

			if (width==-1)
			{
				width=XDisplayWidth(x11Display,x11Screen);
				height=XDisplayHeight(x11Display,x11Screen);
			}

			// Creates the X11 window
			x11Window = XCreateWindow( x11Display, RootWindow(x11Display, x11Screen), (ndcid%3)*640, (ndcid/3)*480, width, height,
				0, depth, InputOutput, x11Visual->visual, ui32Mask, &sWA);
			#ifdef TARGET_PANDORA
				// fullscreen
				Atom wmState = XInternAtom(x11Display, "_NET_WM_STATE", False);
				Atom wmFullscreen = XInternAtom(x11Display, "_NET_WM_STATE_FULLSCREEN", False);
				XChangeProperty(x11Display, x11Window, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)&wmFullscreen, 1);
				
				XMapRaised(x11Display, x11Window);
			#else
				XMapWindow(x11Display, x11Window);

				#if !defined(GLES)

					#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
					#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
					typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
 
 					glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
  					glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
           			glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

           			verify( glXCreateContextAttribsARB != 0 );

           			int context_attribs[] = { 
           				GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
           				GLX_CONTEXT_MINOR_VERSION_ARB, 1, 
           				GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
						GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
						None
					};

					x11_glc = glXCreateContextAttribsARB( x11Display, bestFbc, 0, True, context_attribs);
					XSync( x11Display, False );

					if (!x11_glc) {
						die("Failed to create GL3.1 context\n");
					}
				#endif
			#endif
			XFlush(x11Display);

			

			//(EGLNativeDisplayType)x11Display;
			x11_disp=(void*)x11Display;
			x11_win=(void*)x11Window;

			// Turn off autorepeat so that keyboard controls do not simulate rapid on/off
			ar_set = XkbSetDetectableAutoRepeat(x11Display, True, &ar_supp);
            printf("XkbSetDetectableAutoRepeat returns %u, supported = %u\n",ar_set, ar_supp);
		}
		else
			printf("Not creating X11 window ..\n");
#endif
}

termios tios, orig_tios;

int setup_curses()
{
    //initscr();
    //cbreak();
    //noecho();


    /* Get current terminal settings */
    if (tcgetattr(STDIN_FILENO, &orig_tios)) {
        printf("Error getting current terminal settings\n");
        return -1;
    }

    memcpy(&tios, &orig_tios, sizeof(struct termios));
    tios.c_lflag &= ~ICANON;    //(ECHO|ICANON);&= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);

    tios.c_cc[VTIME] = 0;
    tios.c_cc[VMIN]  = 0;

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
int dc_init(int argc,wchar* argv[]);
void dc_run();

#ifdef TARGET_PANDORA
void gl_term();

void clean_exit(int sig_num) {
	void *array[10];
	size_t size;
	
	// close files
	for (int id_joystick = 0; id_joystick <= 3; ++id_joystick)
		if (js[id_joystick].file_descriptor >= 0) close(js[id_joystick].file_descriptor);
	if (kbfd>=0) close(kbfd);
	if(audio_fd>=0) close(audio_fd);

	// Close EGL context ???
	if (sig_num!=0)
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
	if (sig_num!=0) {
		write(2, "\nSignal received\n", sizeof("\nSignal received\n"));
	
		size = backtrace(array, 10);
		backtrace_symbols_fd(array, size, STDERR_FILENO);
		exit(1);
	}
}

void init_sound()
{
    if((audio_fd=open("/dev/dsp",O_WRONLY))<0) {
		printf("Couldn't open /dev/dsp.\n");
    }
    else
	{
	  printf("sound enabled, dsp openned for write\n");
	  int tmp=44100;
	  int err_ret;
	  err_ret=ioctl(audio_fd,SNDCTL_DSP_SPEED,&tmp);
	  printf("set Frequency to %i, return %i (rate=%i)\n", 44100, err_ret, tmp);
	  int channels=2;
	  err_ret=ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &channels);	  
	  printf("set dsp to stereo (%i => %i)\n", channels, err_ret);
	  int format=AFMT_S16_LE;
	  err_ret=ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format);
	  printf("set dsp to %s audio (%i/%i => %i)\n", "16bits signed" ,AFMT_S16_LE, format, err_ret);
	}
}
#endif

int main(int argc, wchar* argv[])
{
	//if (argc==2) 
		//ndcid=atoi(argv[1]);

	if (setup_curses() < 0) die("failed to setup curses!\n");
#ifdef TARGET_PANDORA
	signal(SIGSEGV, clean_exit);
	signal(SIGKILL, clean_exit);
	
	init_sound();
#else
	void os_InitAudio();
	os_InitAudio();
#endif

#if defined(USES_HOMEDIR) && HOST_OS != OS_DARWIN
	string home = (string)getenv("HOME");
	if(home.c_str())
	{
		home += "/.reicast";
		mkdir(home.c_str(), 0755); // create the directory if missing
		SetHomeDir(home);
	}
	else
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

	printf("Home dir is: %s\n",GetPath("/").c_str());

	common_linux_setup();

	SetupInput();
	
	settings.profile.run_counts=0;
		
	dc_init(argc,argv);

	dc_run();
	
#ifdef TARGET_PANDORA
	clean_exit(0);
#endif

	return 0;
}

u32 alsa_Push(void* frame, u32 samples, bool wait);
u32 os_Push(void* frame, u32 samples, bool wait)
{
	#ifndef TARGET_PANDORA
		int audio_fd = -1;
	#endif

	if (audio_fd > 0) {
		write(audio_fd, frame, samples*4);
	} else {
		return alsa_Push(frame, samples, wait);
	}

	return 1;
}
#endif

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }


void os_DebugBreak()
{
    raise(SIGTRAP);
}
