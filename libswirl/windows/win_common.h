#ifndef _win_common_h_
#define _win_common_h_

#include "license/bsd"

#define NOMINMAX 1

#include <windows.h>
#include <windowsx.h>

#include "rend/TexCache.h"

#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "stdclass.h"
#include "cfg/cfg.h"

#include "win_keyboard.h"

#include "hw/maple/maple_cfg.h"

#include <process.h>

#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/mem/_vmem.h"

#include "hw/pvr/Renderer_if.h"

#include "utils/glwrap/gl3w.h"


#include "libswirl.h"
#include "hw/pvr/Renderer_if.h"

#include "utils/glinit/wgl/wgl.h"

#include "gui/gui.h"
#include "gui/gui_renderer.h"


// Windows class name to register
#define WINDOW_CLASS "nilDC"

// Width and height of the window
#define DEFAULT_WINDOW_WIDTH  1280
#define DEFAULT_WINDOW_HEIGHT 720
#undef ARRAY_SIZE	// macros are evil


#endif
