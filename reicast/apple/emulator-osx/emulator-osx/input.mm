/*
 This file is part of reicast-osx
 */
#include "license/bsd"

#import <Carbon/Carbon.h>
#import <AppKit/AppKit.h>
#import <GameController/GameController.h>
#import <sys/stat.h>

#import "libswirl.h"
#import "types.h"
#import "hw/maple/maple_cfg.h"
#import "gui/gui.h"
#import "OSXKeyboardDevice.hpp"
#import "OSXGamepadDevice.hpp"

class OSXGameControllerGamepadDevice : public GamepadDevice {
public:
    static constexpr int MIN_ANALOG_VALUE = -32768;
    static constexpr int MAX_ANALOG_VALUE = 32768;
    
    OSXGameControllerGamepadDevice(int maple_port, GCExtendedGamepad *gamepad) : GamepadDevice(maple_port, "OSX"), gamepad(gamepad), is_paused(false) {
        _name = gamepad.controller.vendorName.UTF8String;
        {
            char controller_id[48];
            snprintf(controller_id, sizeof(controller_id), "osx_%s", gamepad.controller.vendorName.UTF8String);
            _unique_id = std::string(controller_id);
            
        }
        
        if (!find_mapping()) {
            input_mapper = new GameControllerInputMapping();
            save_mapping();
        }
        
    }
    
    virtual void load_axis_min_max(u32 axis) override
    {
        axis_min_values[axis] = MIN_ANALOG_VALUE;
        axis_ranges[axis] = -MIN_ANALOG_VALUE + MAX_ANALOG_VALUE;
    }
    
    bool is_paused; //used for macOS earlier than 10.15
    GCExtendedGamepad __strong *gamepad;
};

OSXKeyboardDevice keyboard(0);
static std::shared_ptr<OSXKbGamepadDevice> kb_gamepad(0);
static std::shared_ptr<OSXMouseGamepadDevice> mouse_gamepad(0);
static std::shared_ptr<OSXGameControllerGamepadDevice> g_controller_gamepads[64];
static int g_num_controller_gamepads = 0;

u16 kcode[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

u64 hashU64(u64 key)  {
    key ^= (key >> 33);
    key *= 0xff51afd7ed558ccd;
    key ^= (key >> 33);

    return key;
}
static void register_device_for_gamepad(GCExtendedGamepad *gamepad, std::shared_ptr<OSXGameControllerGamepadDevice> &device)  {
    if (g_num_controller_gamepads == ARRAY_SIZE(g_controller_gamepads)) {
        return;
    }
    u64 key = hashU64((u64)gamepad) % ARRAY_SIZE(g_controller_gamepads);
    int i = 0;
    do {
        if (!g_controller_gamepads[key]) {
            g_controller_gamepads[key] = device;
            g_num_controller_gamepads++;
            GamepadDevice::Register(device);
            return;
        }
        key++;
        if (key >= ARRAY_SIZE(g_controller_gamepads)) key = 0;
        i++;
    } while (i < ARRAY_SIZE(g_controller_gamepads));
}

static void unregister_device_for_gamepad(GCExtendedGamepad *gamepad) {
    if (g_num_controller_gamepads == 0) {
        return;
    }
    u64 key = hashU64((u64)gamepad) % ARRAY_SIZE(g_controller_gamepads);
    int i = 0;
    do {
        if (g_controller_gamepads[key] && g_controller_gamepads[key]->gamepad == gamepad) {
            auto &device = g_controller_gamepads[key];
            GamepadDevice::Unregister(device);
            device.reset();
            g_num_controller_gamepads--;
            return;
        }
        key++;
        if (key >= ARRAY_SIZE(g_controller_gamepads)) key = 0;
        i++;
    } while (i < ARRAY_SIZE(g_controller_gamepads));
}

void os_SetupInput() {
    kb_gamepad = std::make_shared<OSXKbGamepadDevice>(0);
    GamepadDevice::Register(kb_gamepad);
    mouse_gamepad = std::make_shared<OSXMouseGamepadDevice>(0);
    GamepadDevice::Register(mouse_gamepad);
}

extern "C" {
void emu_key_input(UInt16 keyCode, bool pressed, UInt modifierFlags) {
    keyboard.keyboard_input(keyCode, pressed, keyboard.convert_modifier_keys(modifierFlags));
    if ((modifierFlags
         & (NSEventModifierFlagShift | NSEventModifierFlagControl | NSEventModifierFlagOption | NSEventModifierFlagCommand)) == 0)
        kb_gamepad->gamepad_btn_input(keyCode, pressed);
}

void emu_character_input(const char *characters) {
    if (characters != NULL)
        while (*characters != '\0')
            keyboard.keyboard_character(*characters++);
}

void emu_mouse_buttons(int button, bool pressed)
{
    mouse_gamepad->gamepad_btn_input(button, pressed);
}

void connect_controller(GCExtendedGamepad *gamepad) {
    if (g_num_controller_gamepads >= ARRAY_SIZE(g_controller_gamepads)) {
        NSLog(@"Number of controllers connected exceeds %d", g_num_controller_gamepads);
        return;
    }
    auto dev = std::make_shared<OSXGameControllerGamepadDevice>(g_num_controller_gamepads, gamepad);
    gamepad.buttonA.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
        dev->gamepad_btn_input(OSX_BTN_A, pressed);
    };
    gamepad.buttonB.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
        dev->gamepad_btn_input(OSX_BTN_B, pressed);
    };
    gamepad.buttonX.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
        dev->gamepad_btn_input(OSX_BTN_X, pressed);
    };
    gamepad.buttonX.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
        dev->gamepad_btn_input(OSX_BTN_Y, pressed);
    };
    
    if (@available(macOS 10.15, *)) {
        gamepad.buttonMenu.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            dev->gamepad_btn_input(OSX_BTN_START, pressed);
        };
    } else {
        gamepad.controller.controllerPausedHandler = ^(GCController * _Nonnull controller) {
            dev->is_paused = !dev->is_paused;
            dev->gamepad_btn_input(OSX_BTN_START, dev->is_paused);
        };
    }
    if (@available(macOS 10.15, *)) {
        gamepad.buttonOptions.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed) {
            dev->gamepad_btn_input(OSX_BTN_SELECT, pressed);
        };
    } else {
        //FIXME: Select is not usable on versions of macOS older than Catalina
    }
    gamepad.dpad.up.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
        dev->gamepad_btn_input(OSX_DPAD_UP, pressed);
    };
    gamepad.dpad.down.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
        dev->gamepad_btn_input(OSX_DPAD_DOWN, pressed);
    };
    gamepad.dpad.left.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
        dev->gamepad_btn_input(OSX_DPAD_LEFT, pressed);
    };
    gamepad.dpad.right.valueChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
        dev->gamepad_btn_input(OSX_DPAD_RIGHT, pressed);
    };
    
    gamepad.leftThumbstick.valueChangedHandler = ^(GCControllerDirectionPad * _Nonnull dpad, float xValue, float yValue) {
        dev->gamepad_axis_input(OSX_AXIS_LS_X, (int)(xValue * OSXGameControllerGamepadDevice::MAX_ANALOG_VALUE));
        dev->gamepad_axis_input(OSX_AXIS_LS_Y, (int)(-yValue * OSXGameControllerGamepadDevice::MAX_ANALOG_VALUE));
    };
    
    gamepad.leftTrigger.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed) {
        dev->gamepad_axis_input(OSX_AXIS_LT, (int)(value * OSXGameControllerGamepadDevice::MAX_ANALOG_VALUE));
    };
    
    gamepad.rightTrigger.valueChangedHandler = ^(GCControllerButtonInput * _Nonnull button, float value, BOOL pressed) {
        dev->gamepad_axis_input(OSX_AXIS_RT, (int)(value * OSXGameControllerGamepadDevice::MAX_ANALOG_VALUE));
    };
    
    register_device_for_gamepad(gamepad, dev);
}

void disconnect_controller(GCExtendedGamepad *gamepad) {
    unregister_device_for_gamepad(gamepad);
}

}
