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

void rc_input_move_cursor_relative(int16_t dx, int16_t dy)
{
    INPUT input;
    memset(&input, 0, sizeof(INPUT));
    input.type = INPUT_MOUSE;
    input.mi.dx = (LONG)dx;
    input.mi.dy = (LONG)dy;
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

/* Map X11 keysym (0xFF00+ range) to Windows VK code.
   Returns 0 if not a special key (should use Unicode instead). */
static WORD special_key_to_vk(uint16_t keysym)
{
    switch (keysym) {
    case 0xFF08: return VK_BACK;       /* Backspace */
    case 0xFF09: return VK_TAB;        /* Tab */
    case 0xFF0D: return VK_RETURN;     /* Enter */
    case 0xFF1B: return VK_ESCAPE;     /* Escape */
    case 0xFF50: return VK_HOME;
    case 0xFF51: return VK_LEFT;
    case 0xFF52: return VK_UP;
    case 0xFF53: return VK_RIGHT;
    case 0xFF54: return VK_DOWN;
    case 0xFF55: return VK_PRIOR;      /* Page Up */
    case 0xFF56: return VK_NEXT;       /* Page Down */
    case 0xFF57: return VK_END;
    case 0xFF63: return VK_INSERT;
    case 0xFFFF: return VK_DELETE;
    case 0xFFBE: return VK_F1;
    case 0xFFBF: return VK_F2;
    case 0xFFC0: return VK_F3;
    case 0xFFC1: return VK_F4;
    case 0xFFC2: return VK_F5;
    case 0xFFC3: return VK_F6;
    case 0xFFC4: return VK_F7;
    case 0xFFC5: return VK_F8;
    case 0xFFC6: return VK_F9;
    case 0xFFC7: return VK_F10;
    case 0xFFC8: return VK_F11;
    case 0xFFC9: return VK_F12;
    default:     return 0;
    }
}

void rc_input_key_press(uint16_t keycode, uint8_t modifiers)
{
    INPUT inputs[6];
    int count = 0;
    memset(inputs, 0, sizeof(inputs));

    /* Press modifiers first */
    if (modifiers & 0x01) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_SHIFT;
        count++;
    }
    if (modifiers & 0x02) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_CONTROL;
        count++;
    }
    if (modifiers & 0x04) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_MENU;
        count++;
    }
    if (modifiers & 0x08) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = VK_LWIN;
        count++;
    }

    /* Check if this is a special key (0xFF00+ range) */
    WORD vk = special_key_to_vk(keycode);
    if (vk != 0) {
        /* Special key — use VK code */
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = vk;
        count++;
    } else if (keycode >= 0xFE00 && keycode <= 0xFEFF) {
        /* Modifier combo range — extract VK code (0xFExx → VK 0xx) */
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = (WORD)(keycode & 0xFF);
        count++;
    } else if (keycode >= 0x0020) {
        /* Printable character — send as Unicode directly */
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = 0;
        inputs[count].ki.wScan = (WORD)keycode;
        inputs[count].ki.dwFlags = KEYEVENTF_UNICODE;
        count++;
    }

    if (count > 0) {
        SendInput(count, inputs, sizeof(INPUT));
    }
}

void rc_input_key_release(uint16_t keycode, uint8_t modifiers)
{
    INPUT inputs[6];
    int count = 0;
    memset(inputs, 0, sizeof(inputs));

    /* Release the key */
    WORD vk = special_key_to_vk(keycode);
    if (vk != 0) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = vk;
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    } else if (keycode >= 0xFE00 && keycode <= 0xFEFF) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = (WORD)(keycode & 0xFF);
        inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        count++;
    } else if (keycode >= 0x0020) {
        inputs[count].type = INPUT_KEYBOARD;
        inputs[count].ki.wVk = 0;
        inputs[count].ki.wScan = (WORD)keycode;
        inputs[count].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        count++;
    }

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

    if (count > 0) {
        SendInput(count, inputs, sizeof(INPUT));
    }
}
