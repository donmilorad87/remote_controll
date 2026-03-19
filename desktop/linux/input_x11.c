#include "input_x11.h"
#include "rc_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

static Display *g_display = NULL;

/* Cursor movement step sizes per speed level (pixels) */
static const int SPEED_TABLE[11] = {
    0, 2, 5, 8, 12, 18, 25, 35, 50, 70, 100
};

/* Direction deltas: [dx, dy] for each RC_DIR_* */
static const int DIR_DX[9] = { 0,  0, -1, 1, -1, 1, -1, 1, 0 };
static const int DIR_DY[9] = { -1, 1,  0, 0, -1, -1, 1, 1, 0 };

int rc_input_init(void)
{
    g_display = XOpenDisplay(NULL);
    if (g_display == NULL) {
        fprintf(stderr, "[input] Cannot open X11 display\n");
        return -1;
    }

    /* Verify XTest extension is available */
    int event_base, error_base, major, minor;
    if (!XTestQueryExtension(g_display, &event_base, &error_base,
                             &major, &minor)) {
        fprintf(stderr, "[input] XTest extension not available\n");
        XCloseDisplay(g_display);
        g_display = NULL;
        return -1;
    }

    fprintf(stdout, "[input] X11 input initialized (XTest %d.%d)\n", major, minor);
    return 0;
}

void rc_input_shutdown(void)
{
    if (g_display != NULL) {
        XCloseDisplay(g_display);
        g_display = NULL;
    }
}

void rc_input_move_cursor(uint8_t direction, uint8_t speed)
{
    assert(g_display != NULL);

    if (direction > RC_DIR_STOP || speed > 10) return;
    if (direction == RC_DIR_STOP) return;

    int step = SPEED_TABLE[speed];
    int dx = DIR_DX[direction] * step;
    int dy = DIR_DY[direction] * step;

    XTestFakeRelativeMotionEvent(g_display, dx, dy, 0);
    XFlush(g_display);
}

void rc_input_mouse_click(uint8_t button)
{
    assert(g_display != NULL);

    unsigned int xbutton;
    switch (button) {
    case RC_MOUSE_LEFT:   xbutton = 1; break;
    case RC_MOUSE_MIDDLE: xbutton = 2; break;
    case RC_MOUSE_RIGHT:  xbutton = 3; break;
    default: return;
    }

    XTestFakeButtonEvent(g_display, xbutton, True, 0);
    XTestFakeButtonEvent(g_display, xbutton, False, 50);
    XFlush(g_display);
}

void rc_input_mouse_scroll(int8_t direction)
{
    assert(g_display != NULL);

    /* X11 scroll: button 4 = up, button 5 = down */
    unsigned int button = (direction > 0) ? 4 : 5;

    XTestFakeButtonEvent(g_display, button, True, 0);
    XTestFakeButtonEvent(g_display, button, False, 0);
    XFlush(g_display);
}

void rc_input_key_press(uint16_t keycode, uint8_t modifiers)
{
    assert(g_display != NULL);

    /* Press modifier keys first */
    if (modifiers & 0x01) /* Shift */
        XTestFakeKeyEvent(g_display, XKeysymToKeycode(g_display, XK_Shift_L), True, 0);
    if (modifiers & 0x02) /* Ctrl */
        XTestFakeKeyEvent(g_display, XKeysymToKeycode(g_display, XK_Control_L), True, 0);
    if (modifiers & 0x04) /* Alt */
        XTestFakeKeyEvent(g_display, XKeysymToKeycode(g_display, XK_Alt_L), True, 0);
    if (modifiers & 0x08) /* Meta/Super */
        XTestFakeKeyEvent(g_display, XKeysymToKeycode(g_display, XK_Super_L), True, 0);

    /* Press the key */
    KeyCode kc = XKeysymToKeycode(g_display, (KeySym)keycode);
    if (kc != 0) {
        XTestFakeKeyEvent(g_display, kc, True, 0);
    }

    XFlush(g_display);
}

void rc_input_key_release(uint16_t keycode, uint8_t modifiers)
{
    assert(g_display != NULL);

    /* Release the key */
    KeyCode kc = XKeysymToKeycode(g_display, (KeySym)keycode);
    if (kc != 0) {
        XTestFakeKeyEvent(g_display, kc, False, 0);
    }

    /* Release modifier keys */
    if (modifiers & 0x08)
        XTestFakeKeyEvent(g_display, XKeysymToKeycode(g_display, XK_Super_L), False, 0);
    if (modifiers & 0x04)
        XTestFakeKeyEvent(g_display, XKeysymToKeycode(g_display, XK_Alt_L), False, 0);
    if (modifiers & 0x02)
        XTestFakeKeyEvent(g_display, XKeysymToKeycode(g_display, XK_Control_L), False, 0);
    if (modifiers & 0x01)
        XTestFakeKeyEvent(g_display, XKeysymToKeycode(g_display, XK_Shift_L), False, 0);

    XFlush(g_display);
}
