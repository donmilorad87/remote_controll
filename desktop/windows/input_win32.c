#include "input_win32.h"
#include "rc_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <windows.h>

static const int SPEED_TABLE[11] = {
    0, 2, 5, 8, 12, 18, 25, 35, 50, 70, 100
};

static const int DIR_DX[9] = { 0,  0, -1, 1, -1, 1, -1, 1, 0 };
static const int DIR_DY[9] = { -1, 1,  0, 0, -1, -1, 1, 1, 0 };

int rc_input_init(void)
{
    fprintf(stdout, "[input] Win32 input initialized\n");
    return 0;
}

void rc_input_shutdown(void)
{
    /* Nothing to clean up on Windows */
}

void rc_input_move_cursor(uint8_t direction, uint8_t speed)
{
    if (direction > RC_DIR_STOP || speed > 10) return;
    if (direction == RC_DIR_STOP) return;

    int step = SPEED_TABLE[speed];
    int dx = DIR_DX[direction] * step;
    int dy = DIR_DY[direction] * step;

    INPUT input;
    memset(&input, 0, sizeof(INPUT));
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;

    SendInput(1, &input, sizeof(INPUT));
}

void rc_input_mouse_click(uint8_t button)
{
    INPUT inputs[2];
    memset(inputs, 0, sizeof(inputs));

    inputs[0].type = INPUT_MOUSE;
    inputs[1].type = INPUT_MOUSE;

    switch (button) {
    case RC_MOUSE_LEFT:
        inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        break;
    case RC_MOUSE_MIDDLE:
        inputs[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        break;
    case RC_MOUSE_RIGHT:
        inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        break;
    default:
        return;
    }

    SendInput(2, inputs, sizeof(INPUT));
}

void rc_input_mouse_scroll(int8_t direction)
{
    INPUT input;
    memset(&input, 0, sizeof(INPUT));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = (direction > 0) ? WHEEL_DELTA : (DWORD)(-WHEEL_DELTA);

    SendInput(1, &input, sizeof(INPUT));
}

void rc_input_key_press(uint16_t keycode, uint8_t modifiers)
{
    INPUT inputs[5]; /* max: 4 modifiers + 1 key */
    int count = 0;
    memset(inputs, 0, sizeof(inputs));

    /* Press modifiers */
    if (modifiers & 0x01) { /* Shift */
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_SHIFT;
        count++;
    }
    if (modifiers & 0x02) { /* Ctrl */
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_CONTROL;
        count++;
    }
    if (modifiers & 0x04) { /* Alt */
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_MENU;
        count++;
    }
    if (modifiers & 0x08) { /* Meta/Win */
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_LWIN;
        count++;
    }

    /* Press key */
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = (WORD)keycode;
    count++;

    SendInput(count, inputs, sizeof(INPUT));
}

void rc_input_key_release(uint16_t keycode, uint8_t modifiers)
{
    INPUT inputs[5];
    int count = 0;
    memset(inputs, 0, sizeof(inputs));

    /* Release key */
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wVk = (WORD)keycode;
    inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
    count++;

    /* Release modifiers */
    if (modifiers & 0x08) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_LWIN;
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    }
    if (modifiers & 0x04) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_MENU;
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    }
    if (modifiers & 0x02) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_CONTROL;
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    }
    if (modifiers & 0x01) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_SHIFT;
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    }

    SendInput(count, inputs, sizeof(INPUT));
}
