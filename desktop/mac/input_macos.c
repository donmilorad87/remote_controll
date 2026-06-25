#include "input_macos.h"
#include "rc_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Carbon/Carbon.h>

static const int SPEED_TABLE[11] = {
    0, 2, 5, 8, 12, 18, 25, 35, 50, 70, 100
};

static const int DIR_DX[9] = { 0,  0, -1, 1, -1, 1, -1, 1, 0 };
static const int DIR_DY[9] = { -1, 1,  0, 0, -1, -1, 1, 1, 0 };

int rc_input_init(void)
{
    /* CGEvent doesn't require initialization, but we need Accessibility permissions */
    if (!CGRequestPostEvent(NULL)) {
        fprintf(stderr, "[input] Note: App may need Accessibility permissions\n");
    }

    fprintf(stdout, "[input] macOS CGEvent input initialized\n");
    return 0;
}

void rc_input_shutdown(void)
{
    /* Nothing to clean up */
}

void rc_input_move_cursor(uint8_t direction, uint8_t speed)
{
    if (direction > RC_DIR_STOP || speed > 10) return;
    if (direction == RC_DIR_STOP) return;

    int step = SPEED_TABLE[speed];
    int dx = DIR_DX[direction] * step;
    int dy = DIR_DY[direction] * step;

    /* Get current mouse position */
    CGEventRef get_pos = CGEventCreate(NULL);
    CGPoint pos = CGEventGetLocation(get_pos);
    CFRelease(get_pos);

    pos.x += dx;
    pos.y += dy;

    CGEventRef move = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved,
                                               pos, kCGMouseButtonLeft);
    if (move != NULL) {
        CGEventPost(kCGHIDEventTap, move);
        CFRelease(move);
    }
}

void rc_input_move_cursor_relative(int16_t dx, int16_t dy)
{
    CGEventRef get_pos = CGEventCreate(NULL);
    CGPoint pos = CGEventGetLocation(get_pos);
    CFRelease(get_pos);

    pos.x += dx;
    pos.y += dy;

    CGEventRef move = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved,
                                               pos, kCGMouseButtonLeft);
    if (move != NULL) {
        CGEventPost(kCGHIDEventTap, move);
        CFRelease(move);
    }
}

void rc_input_mouse_click(uint8_t button)
{
    CGEventRef get_pos = CGEventCreate(NULL);
    CGPoint pos = CGEventGetLocation(get_pos);
    CFRelease(get_pos);

    CGEventType down_type, up_type;
    CGMouseButton cg_button;

    switch (button) {
    case RC_MOUSE_LEFT:
        down_type = kCGEventLeftMouseDown;
        up_type = kCGEventLeftMouseUp;
        cg_button = kCGMouseButtonLeft;
        break;
    case RC_MOUSE_MIDDLE:
        down_type = kCGEventOtherMouseDown;
        up_type = kCGEventOtherMouseUp;
        cg_button = kCGMouseButtonCenter;
        break;
    case RC_MOUSE_RIGHT:
        down_type = kCGEventRightMouseDown;
        up_type = kCGEventRightMouseUp;
        cg_button = kCGMouseButtonRight;
        break;
    default:
        return;
    }

    CGEventRef down = CGEventCreateMouseEvent(NULL, down_type, pos, cg_button);
    CGEventRef up = CGEventCreateMouseEvent(NULL, up_type, pos, cg_button);

    if (down != NULL && up != NULL) {
        CGEventPost(kCGHIDEventTap, down);
        CGEventPost(kCGHIDEventTap, up);
    }

    if (down != NULL) CFRelease(down);
    if (up != NULL) CFRelease(up);
}

void rc_input_mouse_scroll(int8_t direction)
{
    /* Positive = up, negative = down. macOS uses inverted convention. */
    int32_t delta = (direction > 0) ? 5 : -5;

    CGEventRef scroll = CGEventCreateScrollWheelEvent(NULL,
        kCGScrollEventUnitLine, 1, delta);

    if (scroll != NULL) {
        CGEventPost(kCGHIDEventTap, scroll);
        CFRelease(scroll);
    }
}

void rc_input_key_press(uint16_t keycode, uint8_t modifiers)
{
    CGEventRef event = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)keycode, true);
    if (event == NULL) return;

    CGEventFlags flags = 0;
    if (modifiers & 0x01) flags |= kCGEventFlagMaskShift;
    if (modifiers & 0x02) flags |= kCGEventFlagMaskControl;
    if (modifiers & 0x04) flags |= kCGEventFlagMaskAlternate;
    if (modifiers & 0x08) flags |= kCGEventFlagMaskCommand;

    CGEventSetFlags(event, flags);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}

void rc_input_key_release(uint16_t keycode, uint8_t modifiers)
{
    CGEventRef event = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)keycode, false);
    if (event == NULL) return;

    CGEventFlags flags = 0;
    if (modifiers & 0x01) flags |= kCGEventFlagMaskShift;
    if (modifiers & 0x02) flags |= kCGEventFlagMaskControl;
    if (modifiers & 0x04) flags |= kCGEventFlagMaskAlternate;
    if (modifiers & 0x08) flags |= kCGEventFlagMaskCommand;

    CGEventSetFlags(event, flags);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
}
