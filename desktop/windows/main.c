#include "rc_desktop.h"
#include "rc_protocol.h"
#include "input_win32.h"
#include "volume_win32.h"
#include "tray.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static rc_desktop_t g_desktop;

void rc_handle_event(const rc_event_t *event)
{
    assert(event != NULL);
    switch (event->type) {
    case RC_EVT_CURSOR_MOVE:
        rc_input_move_cursor(event->data.cursor_move.direction, event->data.cursor_move.speed);
        break;
    case RC_EVT_TOUCHPAD:
        rc_input_move_cursor_relative(event->data.touchpad.dx, event->data.touchpad.dy);
        break;
    case RC_EVT_MOUSE_CLICK:
        rc_input_mouse_click(event->data.mouse_click.button);
        break;
    case RC_EVT_MOUSE_SCROLL:
        rc_input_mouse_scroll(event->data.mouse_scroll.direction);
        break;
    case RC_EVT_KEY_PRESS:
        rc_input_key_press(event->data.key.keycode, event->data.key.modifiers);
        break;
    case RC_EVT_KEY_RELEASE:
        rc_input_key_release(event->data.key.keycode, event->data.key.modifiers);
        break;
    case RC_EVT_VOLUME:
        rc_volume_handle(event->data.volume.action);
        break;
    default: break;
    }
}

static void on_quit(void) { rc_desktop_stop(&g_desktop); }
static void on_reconnect(void) { /* auto-discovery — nothing to do */ }

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR cmdLine, int nShow)
{
    (void)hPrev; (void)cmdLine; (void)nShow;

    rc_input_init();
    rc_volume_init();

    if (rc_desktop_start(&g_desktop) != 0) {
        MessageBoxA(NULL, "Failed to start network.", "RemoteControl", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (rc_tray_init(hInstance, on_quit, on_reconnect) == 0) {
        char status[128];
        snprintf(status, sizeof(status), "%s (%s)", g_desktop.info.name, g_desktop.info.lan_ip);
        rc_tray_set_status(status);
        rc_tray_run();
    }

    rc_desktop_stop(&g_desktop);
    rc_tray_shutdown();
    rc_input_shutdown();
    rc_volume_shutdown();
    return 0;
}
