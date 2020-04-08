#include "win_common.h"

#include <windows.h>
 
#include "utils/glinit/wgl/wgl.h"
#include "hw/sh4/dyna/ngen.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/mem/_vmem.h"


#include "xinput_gamepad.h"
#include <xinput.h>
#pragma comment(lib, "XInput9_1_0.lib")

extern void ToggleFullscreen();
extern s32 mo_x_abs;
extern s32 mo_y_abs;
extern u32 mo_buttons;
extern f32 mo_x_delta;
extern f32 mo_y_delta;
extern f32 mo_wheel_delta;
// Keyboard
static Win32KeyboardDevice keyboard(0);

extern int screen_width, screen_height;

static LARGE_INTEGER qpf;
static double  qpfd;
void* window_win;
extern bool window_maximized ;
extern LRESULT CALLBACK WndProc2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern int screen_width, screen_height;
static std::shared_ptr<WinKbGamepadDevice> kb_gamepad;
static std::shared_ptr<WinMouseGamepadDevice> mouse_gamepad;

void os_SetupInput()
{
	XInputGamepadDevice::CreateDevices();
	kb_gamepad = std::make_shared<WinKbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
	mouse_gamepad = std::make_shared<WinMouseGamepadDevice>(0);
	GamepadDevice::Register(mouse_gamepad);
}

bool os_gl_init(void* hwnd, void* hdc)
{
	return wgl_Init(hwnd, hdc);
}

bool os_gl_swap()
{
	return wgl_Swap();
}

void os_gl_term()
{
	wgl_Term();
}

void os_SetWindowText(const char* text)
{
	if (GetWindowLong((HWND)libPvr_GetRenderTarget(), GWL_STYLE) & WS_BORDER)
	{
		SetWindowText((HWND)libPvr_GetRenderTarget(), text);
	}
}

void os_MakeExecutable(void* ptr, u32 sz)
{
	DWORD old;
	VirtualProtect(ptr, sz, PAGE_EXECUTE_READWRITE, &old);  // sizeof(sz) really?
}

//Helper functions
double os_GetSeconds()
{
	static bool initme = (QueryPerformanceFrequency(&qpf), qpfd = 1 / (double)qpf.QuadPart);
	LARGE_INTEGER time_now;

	QueryPerformanceCounter(&time_now);
	return time_now.QuadPart * qpfd;
}

void os_DebugBreak()
{
	__debugbreak();
}

//#include "plugins/plugin_manager.h"

void UpdateInputState(u32 port)
{
	/*
		 Disabled for now. Need new EMU_BTN_ANA_LEFT/RIGHT/.. virtual controller keys

	joyx[port]=joyy[port]=0;

	if (GetAsyncKeyState('J'))
		joyx[port]-=126;
	if (GetAsyncKeyState('L'))
		joyx[port]+=126;

	if (GetAsyncKeyState('I'))
		joyy[port]-=126;
	if (GetAsyncKeyState('K'))
		joyy[port]+=126;
	*/
	std::shared_ptr<XInputGamepadDevice> gamepad = XInputGamepadDevice::GetXInputDevice(port);
	if (gamepad != NULL)
		gamepad->ReadInput();
}

LRESULT CALLBACK WndProc2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		/*
		Here we are handling 2 system messages: screen saving and monitor power.
		They are especially relevant on mobile devices.
		*/
	case WM_SYSCOMMAND:
	{
		switch (wParam)
		{
		case SC_SCREENSAVE:   // Screensaver trying to start ?
		case SC_MONITORPOWER: // Monitor trying to enter powersave ?
			return 0;         // Prevent this from happening
		}
		break;
	}
	// Handles the close message when a user clicks the quit icon of the window
	case WM_CLOSE:
		PostQuitMessage(0);
		return 1;

	case WM_SIZE:
		screen_width = LOWORD(lParam);
		screen_height = HIWORD(lParam);
		window_maximized = (wParam & SIZE_MAXIMIZED) != 0;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		switch (message)
		{
		case WM_LBUTTONDOWN:
			mouse_gamepad->gamepad_btn_input(0, true);
			break;
		case WM_LBUTTONUP:
			mouse_gamepad->gamepad_btn_input(0, false);
			break;
		case WM_MBUTTONDOWN:
			mouse_gamepad->gamepad_btn_input(1, true);
			break;
		case WM_MBUTTONUP:
			mouse_gamepad->gamepad_btn_input(1, false);
			break;
		case WM_RBUTTONDOWN:
			mouse_gamepad->gamepad_btn_input(2, true);
			break;
		case WM_RBUTTONUP:
			mouse_gamepad->gamepad_btn_input(2, false);
			break;
		}
		/* no break */
	case WM_MOUSEMOVE:
	{
		static int prev_x = -1;
		static int prev_y = -1;
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		mo_x_abs = (xPos - (screen_width - 640 * screen_height / 480) / 2) * 480 / screen_height;
		mo_y_abs = yPos * 480 / screen_height;
		mo_buttons = 0xffffffff;
		if (wParam & MK_LBUTTON)
			mo_buttons &= ~(1 << 2);
		if (wParam & MK_MBUTTON)
			mo_buttons &= ~(1 << 3);
		if (wParam & MK_RBUTTON)
			mo_buttons &= ~(1 << 1);
		if (prev_x != -1)
		{
			mo_x_delta += (f32)(xPos - prev_x) * settings.input.MouseSensitivity / 100.f;
			mo_y_delta += (f32)(yPos - prev_y) * settings.input.MouseSensitivity / 100.f;
		}
		prev_x = xPos;
		prev_y = yPos;
	}
	if (message != WM_MOUSEMOVE)
		return 0;
	break;
	case WM_MOUSEWHEEL:
		mo_wheel_delta -= (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA * 16;
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		u8 keycode;
		// bit 24 indicates whether the key is an extended key, such as the right-hand ALT and CTRL keys that appear on an enhanced 101- or 102-key keyboard.
		// (It also distinguishes between the main Return key and the numeric keypad Enter key)
		// The value is 1 if it is an extended key; otherwise, it is 0.
		if (wParam == VK_RETURN && ((lParam & (1 << 24)) != 0))
			keycode = VK_NUMPAD_RETURN;
		else
			keycode = wParam & 0xff;
		kb_gamepad->gamepad_btn_input(keycode, message == WM_KEYDOWN);
		keyboard.keyboard_input(keycode, message == WM_KEYDOWN);
	}
	break;

	case WM_SYSKEYDOWN:
		if (wParam == VK_RETURN)
			if ((HIWORD(lParam) & KF_ALTDOWN))
				ToggleFullscreen();

		break;

	case WM_CHAR:
		keyboard.keyboard_character((char)wParam);
		return 0;

	default:
		break;
	}

	// Calls the default window procedure for messages we did not handle
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void os_DoEvents()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		// If the message is WM_QUIT, exit the while loop
		if (msg.message == WM_QUIT)
		{
			if (virtualDreamcast && sh4_cpu->IsRunning()) {
				virtualDreamcast->Stop([] {
					g_GUIRenderer->Stop();
					});
			}
			else
			{
				g_GUIRenderer->Stop();
			}
		}

		// Translate the message and dispatch it to WindowProc()
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void os_LaunchFromURL(const string& url)
{
	ShellExecuteA((HWND)window_win, "open", url.c_str(), nullptr, nullptr, SW_SHOW);
}

void os_CreateWindow()
{
	WNDCLASS sWC;
	sWC.style = CS_HREDRAW | CS_VREDRAW;
	sWC.lpfnWndProc = WndProc2;
	sWC.cbClsExtra = 0;
	sWC.cbWndExtra = 0;
	sWC.hInstance = (HINSTANCE)GetModuleHandle(0);
	sWC.hIcon = 0;
	sWC.hCursor = LoadCursor(NULL, IDC_ARROW);
	sWC.lpszMenuName = 0;
	sWC.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	sWC.lpszClassName = WINDOW_CLASS;
	screen_width = cfgLoadInt("windows", "width", DEFAULT_WINDOW_WIDTH);
	screen_height = cfgLoadInt("windows", "height", DEFAULT_WINDOW_HEIGHT);
	window_maximized = cfgLoadBool("windows", "maximized", false);

	ATOM registerClass = RegisterClass(&sWC);
	if (!registerClass)
	{
		MessageBox(0, ("Failed to register the window class"), ("Error"), MB_OK | MB_ICONEXCLAMATION);
	}

	// Create the eglWindow
	RECT sRect;
	SetRect(&sRect, 0, 0, screen_width, screen_height);
	AdjustWindowRectEx(&sRect, WS_OVERLAPPEDWINDOW, false, 0);

	HWND hWnd = CreateWindow(WINDOW_CLASS, VER_FULLNAME, WS_VISIBLE | WS_OVERLAPPEDWINDOW | (window_maximized ? WS_MAXIMIZE : 0),
		0, 0, sRect.right - sRect.left, sRect.bottom - sRect.top, NULL, NULL, sWC.hInstance, NULL);

	window_win = hWnd;
}


void* libPvr_GetRenderTarget()
{
	return window_win;
}

void* libPvr_GetRenderSurface()
{
	return GetDC((HWND)window_win);
}

