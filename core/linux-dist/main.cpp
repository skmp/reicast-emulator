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

u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

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
// Structure to store the mapping of each controller using a keyboard
typedef struct{
	// Analog simulation
   	unsigned int XANA_UP;
	unsigned int XANA_DOWN;
	unsigned int XANA_LEFT;
	unsigned int XANA_RIGHT;
	unsigned int XANA_LT;
	unsigned int XANA_RT;
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
   	int ANA_X;
   	int ANA_Y;
   	int DPAD_X;
   	int DPAD_Y;
   	int TRIGGER_L;
   	int TRIGGER_R;
	// Button mapping
	map<int, int> btn_map;
	// Current joystick state
	int buttons;
	s8 ana_x;
	s8 ana_y;
	s8 trigger_left;
	s8 trigger_right;
} joystick_map;

// Name of the sections in the configuration file 
// corresponding to each controller
char * config_section_names_keyboard[4] = {
	"control1",
	"control2",
	"control3",
	"control4"
};

// Keyboard maps of all controllers
keyboard_map kb_maps[4];
// Joysticks
joystick js[4];
// Joystick map of all controllers
joystick_map js_maps[4];
// Variables to store actions of the current frame before being reported to the
// emulator
u8 temp_joyx[4] = {0, 0, 0, 0};
u8 temp_joyy[4] = {0, 0, 0, 0};
s8 temp_lt[4] = {0, 0, 0, 0};
s8 temp_rt[4] = {0, 0, 0, 0};

// Loads a keyboard map for a given port
void load_keyboard_map(int port)
{
	printf("Loading keyboard map for port %d\n", port);
	if (cfgOpen())
	{
		// Loading analog keys
		kb_maps[port].XANA_UP = cfgLoadInt(config_section_names_keyboard[port], "XANA_UP", 0);
		kb_maps[port].XANA_DOWN = cfgLoadInt(config_section_names_keyboard[port], "XANA_DOWN", 0);
		kb_maps[port].XANA_LEFT = cfgLoadInt(config_section_names_keyboard[port], "XANA_LEFT", 0);
		kb_maps[port].XANA_RIGHT = cfgLoadInt(config_section_names_keyboard[port], "XANA_RIGHT", 0);
		kb_maps[port].XANA_LT = cfgLoadInt(config_section_names_keyboard[port], "XANA_LT", 0);
		kb_maps[port].XANA_RT = cfgLoadInt(config_section_names_keyboard[port], "XANA_RT", 0);

		// Loading buttons keys
		unsigned int XDPAD_UP = cfgLoadInt(config_section_names_keyboard[port], "XDPAD_UP", 0);
		unsigned int XDPAD_DOWN = cfgLoadInt(config_section_names_keyboard[port], "XDPAD_DOWN", 0);
		unsigned int XDPAD_LEFT = cfgLoadInt(config_section_names_keyboard[port], "XDPAD_LEFT", 0);
		unsigned int XDPAD_RIGHT = cfgLoadInt(config_section_names_keyboard[port], "XDPAD_RIGHT", 0);
		unsigned int XBTN_Y = cfgLoadInt(config_section_names_keyboard[port], "XBTN_Y", 0);
		unsigned int XBTN_X = cfgLoadInt(config_section_names_keyboard[port], "XBTN_X", 0);
		unsigned int XBTN_B = cfgLoadInt(config_section_names_keyboard[port], "XBTN_B", 0);
		unsigned int XBTN_A = cfgLoadInt(config_section_names_keyboard[port], "XBTN_A", 0);
		unsigned int XBTN_START = cfgLoadInt(config_section_names_keyboard[port], "XBTN_START", 0);

		kb_maps[port].keymap[XDPAD_LEFT] = DPad_Left;
		kb_maps[port].keymap[XDPAD_RIGHT] = DPad_Right;
		kb_maps[port].keymap[XDPAD_UP] = DPad_Up;
		kb_maps[port].keymap[XDPAD_DOWN] = DPad_Down;
		kb_maps[port].keymap[XBTN_Y] = Btn_Y;
		kb_maps[port].keymap[XBTN_X] = Btn_X;
		kb_maps[port].keymap[XBTN_B] = Btn_B;
		kb_maps[port].keymap[XBTN_A] = Btn_A;
		kb_maps[port].keymap[XBTN_START] = Btn_Start;
	}
}

// Loads a joystick
void load_joystick(char * path, joystick & js)
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

// Loads a keyboard map for a given port
void load_joystick_map(int port)
{
	// Initialize the file descriptor for the current joystick in case 
	// no joystick is found
	js_maps[port].js.file_descriptor = -1;
	printf("Loading joystick map for port %d\n", port);
	if (cfgOpen())
	{
		// Finding proper file descriptor
		char target_name[128];
		cfgLoadStr(config_section_names_keyboard[port], "controller.name", target_name, "\0");
		printf("Using following controller: %s\n", target_name);
		// We find the corresponding controller
		for (int i = 0; i <= 3; ++i)
		{
			// We check if the names match
			if (strcmp(js[i].name, target_name) == 0)
			{
				js_maps[port].js = js[i];
				// We erase the name so the same controller is not used twice
				js[i].name[0] = "\0";
				// And we stop looking for other controllers
				break;
			}
		}


		// Loading axis mapping
		int XANA_X = cfgLoadInt(config_section_names_keyboard[port], "JS_ANA_X", 0);
		int XANA_Y = cfgLoadInt(config_section_names_keyboard[port], "JS_ANA_Y", 0);
		int XANA_LT = cfgLoadInt(config_section_names_keyboard[port], "JS_ANA_LT", 0);
		int XANA_RT = cfgLoadInt(config_section_names_keyboard[port], "JS_ANA_RT", 0);
		int XDPAD_X = cfgLoadInt(config_section_names_keyboard[port], "JS_DPAD_X", 0);
		int XDPAD_Y = cfgLoadInt(config_section_names_keyboard[port], "JS_DPAD_Y", 0);
	

		js_maps[port].ANA_X = XANA_X;
		js_maps[port].ANA_Y = XANA_Y;
		js_maps[port].TRIGGER_L = XANA_LT;
		js_maps[port].TRIGGER_R = XANA_RT;
		js_maps[port].DPAD_X = XDPAD_X;
		js_maps[port].DPAD_Y = XDPAD_Y;

		printf("ANA_X: %d\n", js_maps[port].ANA_X);
		printf("ANA_Y: %d\n", js_maps[port].ANA_Y);
		printf("TRIGGER_L: %d\n", js_maps[port].TRIGGER_L);
		printf("TRIGGER_R: %d\n", js_maps[port].TRIGGER_R);
		printf("DPAD_X: %d\n", js_maps[port].DPAD_X);
		printf("DPAD_Y: %d\n", js_maps[port].DPAD_Y);

		// Loading buttons mapping
		int XBTN_Y = cfgLoadInt(config_section_names_keyboard[port], "JS_BTN_Y", 0);
		int XBTN_X = cfgLoadInt(config_section_names_keyboard[port], "JS_BTN_X", 0);
		int XBTN_B = cfgLoadInt(config_section_names_keyboard[port], "JS_BTN_B", 0);
		int XBTN_A = cfgLoadInt(config_section_names_keyboard[port], "JS_BTN_A", 0);
		int XBTN_START = cfgLoadInt(config_section_names_keyboard[port], "JS_BTN_START", 0);

		js_maps[port].btn_map[XBTN_Y] = Btn_Y;
		js_maps[port].btn_map[XBTN_X] = Btn_X;
		js_maps[port].btn_map[XBTN_B] = Btn_B;
		js_maps[port].btn_map[XBTN_A] = Btn_A;
		js_maps[port].btn_map[XBTN_START] = Btn_Start;

		// Initialization of buttons as not pressed
		js_maps[port].buttons = 0xFFFF;
		// Initialization of all axis to 0
		js_maps[port].ana_x = 0;
		js_maps[port].ana_y = 0;
		js_maps[port].trigger_left = 0;
		js_maps[port].trigger_right = 0;


	}
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

		#if defined(SUPPORT_X11) && !defined(TARGET_PANDORA)		
		load_keyboard_map(port);
		#endif
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
	// Open joystick devices
	load_joystick("/dev/input/js0", js[0]);
	load_joystick("/dev/input/js1", js[1]);
	load_joystick("/dev/input/js2", js[2]);
	load_joystick("/dev/input/js3", js[3]);

	// Load joystick mappings
	load_joystick_map(0);
	load_joystick_map(1);
	load_joystick_map(2);
	load_joystick_map(3);
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

  // Joystick must be connected
  if (js_maps[port].js.file_descriptor < 0) return false;

#if HOST_OS != OS_DARWIN
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
  				if(id == js_maps[port].ANA_X)
  				{
  					js_maps[port].ana_x = signed_value;
				}
				else if (id == js_maps[port].ANA_Y)
				{
					js_maps[port].ana_y = signed_value;
				}

				// DPad (Handled as an axis by computer, but as buttons by the Dreamcast)
  				else if (id == js_maps[port].DPAD_X)
  				{
  					if (signed_value < 0)
  					{
  						js_maps[port].buttons &= ~DPad_Left;
  					}
  					else if (signed_value > 0)
  					{
  						js_maps[port].buttons &= ~DPad_Right;
  					}
  					else
  					{
  					 	js_maps[port].buttons |= DPad_Left;
  					 	js_maps[port].buttons |= DPad_Right;
  					}
  				}
  				else if (id == js_maps[port].DPAD_Y)
  				{
  					if (signed_value < 0)
  					{
  						js_maps[port].buttons &= ~DPad_Up;
  					}
  					else if (signed_value > 0)
  					{
  						js_maps[port].buttons &= ~DPad_Down;
  					}
  					else
  					{
  					 	js_maps[port].buttons |= DPad_Up;
  					 	js_maps[port].buttons |= DPad_Down;
  					}
  				}

  				// Triggers
				else if (id == js_maps[port].TRIGGER_L)
				{
  					js_maps[port].trigger_left = unsigned_value;
  				}
  				else if (id == js_maps[port].TRIGGER_R)
  				{
  					js_maps[port].trigger_right = unsigned_value;
  				}
  				break;

  			// Button handling
  			case JS_EVENT_BUTTON:
  				js_maps[port].buttons ^= js_maps[port].btn_map[id];
  				break;
  			default:
  				break;
  		}
  	}
  }

  // Report the current buttons to the emulator
  joyx[port] = js_maps[port].ana_x;
  joyy[port] = js_maps[port].ana_y;
  lt[port] = js_maps[port].trigger_left;
  rt[port] = js_maps[port].trigger_right;
  kcode[port]= js_maps[port].buttons;

#endif
	return true;

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
	kcode[port] = 0xFFFF;
	if (!HandleJoystick(port))
	{
		HandleKb(port);
		kcode[port]= x11_dc_buttons[port];
		joyx[port] = temp_joyx[port];
	    joyy[port] = temp_joyy[port];
	    lt[port] = temp_lt[port];
	    rt[port] = temp_rt[port];	
	} else {

	}
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
                    	//Detect up press
                    	if(e.xkey.keycode == kb_maps[port].XANA_UP)
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
                    	if(e.xkey.keycode == kb_maps[port].XANA_DOWN)
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
                    	if(e.xkey.keycode == kb_maps[port].XANA_LEFT)
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
                    	if(e.xkey.keycode == kb_maps[port].XANA_RIGHT)
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
                    	if (e.xkey.keycode == kb_maps[port].XANA_LT)
                    	{
	                        if (e.type == KeyPress)
                        	{    
	                            temp_lt[port] = 255;
                        	}
                        	else if (e.type == KeyRelease)
                        	{
	                            temp_lt[port] = 0;
                        	}
                        	else
                        	{
                        	}
                    	}
                        
                    	//detect RT press
                    	if (e.xkey.keycode == kb_maps[port].XANA_RT)
                    	{
	                        if (e.type == KeyPress)
                        	{    
	                            temp_rt[port] = 255;
                        	}
                        	else if (e.type == KeyRelease)
                        	{
	                            temp_rt[port] = 0;
                        	}
                        	else
                        	{
                        	}
                    	}
   
						int dc_key = kb_maps[port].keymap[e.xkey.keycode];

						if (e.type == KeyPress)
							x11_dc_buttons[port] &= ~dc_key;
						else
							x11_dc_buttons[port] |= dc_key;
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
	        if((ana_up[port] == true) && (ana_down[port] == false))
        	{
	            temp_joyy[port] = -127;
        	}
        	else if((ana_up[port] == false) && (ana_down[port] == true))
        	{
	            temp_joyy[port] = 127;
        	}
        	else
        	{
	            /* Either both pressed simultaniously or neither pressed */
            	temp_joyy[port] = 0;
        	}
	            
        	/* Check analogue control states (left/right) */
        	if((ana_left[port] == true) && (ana_right[port] == false))
        	{
	            temp_joyx[port] = -127;
        	}
        	else if((ana_left[port] == false) && (ana_right[port] == true))
        	{
	            temp_joyx[port] = 127;
        	}
        	else
        	{
	            /* Either both pressed simultaniously or neither pressed */
            	temp_joyx[port] = 0;
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
