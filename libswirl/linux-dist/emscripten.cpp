#include <types.h>

#if defined(TARGET_EMSCRIPTEN)
#include <emscripten.h>
#include <emscripten/html5.h>
#include "emscripten.h"
#include "input/keyboard_device.h"
#include "input/gamepad_device.h"

class emKbInputMapping : public InputMapping
{
public:
	emKbInputMapping()
	{
		name = "EM Keyboard";
		set_button(DC_BTN_A, 88);
		set_button(DC_BTN_B, 67);
		set_button(DC_BTN_X, 83);
		set_button(DC_BTN_Y, 68);
		set_button(DC_DPAD_UP, 38);
		set_button(DC_DPAD_DOWN, 40);
		set_button(DC_DPAD_LEFT, 37);
		set_button(DC_DPAD_RIGHT, 39);
		set_button(DC_BTN_START, 13);
		set_button(EMU_BTN_TRIGGER_LEFT, 70);
		set_button(EMU_BTN_TRIGGER_RIGHT, 86);
		set_button(EMU_BTN_MENU, 9);

		dirty = false;
	}
};

class emKbGamepadDevice : public GamepadDevice
{
public:
	emKbGamepadDevice(int maple_port) : GamepadDevice(maple_port, "EM")
	{
		_name = "Keyboard";
		_unique_id = "em_keyboard";
		if (!find_mapping())
			input_mapper = new emKbInputMapping();
	}
};

static std::shared_ptr<emKbGamepadDevice> kb_gamepad;

extern int screen_width, screen_height;
extern u32 mo_buttons;
extern f32 mo_x_delta;
extern f32 mo_y_delta;
extern f32 mo_wheel_delta;
extern s32 mo_x_abs;
extern s32 mo_y_abs;

static EM_BOOL mouse_callback(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
	//printf("%s, screen: (%ld,%ld), client: (%ld,%ld),%s%s%s%s button: %hu, buttons: %hu, movement: (%ld,%ld), canvas: (%ld,%ld)\n",
	//	   "mouse", e->screenX, e->screenY, e->clientX, e->clientY,
	//	   e->ctrlKey ? " CTRL" : "", e->shiftKey ? " SHIFT" : "", e->altKey ? " ALT" : "", e->metaKey ? " META" : "",
	//	   e->button, e->buttons, e->movementX, e->movementY, e->canvasX, e->canvasY);

	auto clX = e->clientX;
	auto clY = e->clientY;

	mo_x_abs = (clX - (screen_width - screen_height * 640 / 480) / 2) * 480 / screen_height;
	mo_y_abs = clY * 480 / screen_height;

	mo_buttons = e->buttons ? 0 : (1 << 2);
	return 0;
}

static EM_BOOL key_callback(int eventType, const EmscriptenKeyboardEvent* e, void* userData)
{
	//printf("%s, key: \"%s\", code: \"%s\", location: %lu,%s%s%s%s repeat: %d, locale: \"%s\", char: \"%s\", charCode: %lu, keyCode: %lu, which: %lu\n",
	//	"keyEvent", e->key, e->code, e->location,
	//	e->ctrlKey ? " CTRL" : "", e->shiftKey ? " SHIFT" : "", e->altKey ? " ALT" : "", e->metaKey ? " META" : "",
	//	e->repeat, e->locale, e->charValue, e->charCode, e->keyCode, e->which);

	kb_gamepad->gamepad_btn_input(e->which, eventType == EMSCRIPTEN_EVENT_KEYDOWN);

	if (eventType == EMSCRIPTEN_EVENT_KEYPRESS && (!strcmp(e->key, "f") || e->which == 102)) {
		EmscriptenFullscreenChangeEvent fsce;
		EMSCRIPTEN_RESULT ret = emscripten_get_fullscreen_status(&fsce);

		if (!fsce.isFullscreen) {
			printf("Requesting fullscreen..\n");
			ret = emscripten_request_fullscreen(0, 1);

		}
		else {
			printf("Exiting fullscreen..\n");
			ret = emscripten_exit_fullscreen();

			ret = emscripten_get_fullscreen_status(&fsce);

			if (fsce.isFullscreen) {
				fprintf(stderr, "Fullscreen exit did not work!\n");
			}
		}
	}
	if (eventType == EMSCRIPTEN_EVENT_KEYPRESS && (!strcmp(e->key, "p") || e->which == 112)) {
		EmscriptenPointerlockChangeEvent plce;
		EMSCRIPTEN_RESULT ret = emscripten_get_pointerlock_status(&plce);

		if (!plce.isActive) {
			printf("Requesting pointer lock..\n");
			ret = emscripten_request_pointerlock(0, 1);
		}
		else {
			printf("Exiting pointer lock..\n");
			ret = emscripten_exit_pointerlock();
			ret = emscripten_get_pointerlock_status(&plce);
			if (plce.isActive) {
				fprintf(stderr, "Pointer lock exit did not work!\n");
			}
		}
	}
	return 0;
}


void input_emscripten_init() {
	emscripten_set_mousedown_callback("#canvas", 0, 1, mouse_callback);
	emscripten_set_mouseup_callback("#canvas", 0, 1, mouse_callback);
	emscripten_set_mousemove_callback("#canvas", 0, 1, mouse_callback);

	emscripten_set_keydown_callback("#canvas", 0, 1, key_callback);
	emscripten_set_keyup_callback("#canvas", 0, 1, key_callback);

	kb_gamepad = std::make_shared<emKbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
}

int os_MessageBox(const char* text, unsigned int type)
{
	return 1;
}
#endif
