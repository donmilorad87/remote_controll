#include "rc_desktop.h"
#include "rc_protocol.h"
#include "input_x11.h"
#include "volume_pulse.h"
#include "tray.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static rc_desktop_t g_desktop;
static volatile sig_atomic_t g_running = 1;

void rc_handle_event(const rc_event_t *event)
{
    assert(event != NULL);

    switch (event->type) {
    case RC_EVT_CURSOR_MOVE:
        rc_input_move_cursor(event->data.cursor_move.direction,
                             event->data.cursor_move.speed);
        break;
    case RC_EVT_TOUCHPAD:
        rc_input_move_cursor_relative(event->data.touchpad.dx,
                                      event->data.touchpad.dy);
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
    default:
        break;
    }
}

static void on_quit(void) { g_running = 0; }
static void on_reconnect(void) { /* No-op for auto-discovery */ }
static void signal_handler(int sig) { (void)sig; g_running = 0; }

int main(void)
{
    fprintf(stdout, "=== RemoteControl Desktop (Linux) ===\n");

    if (rc_input_init() != 0) {
        fprintf(stderr, "[main] X11 input init failed\n");
        return EXIT_FAILURE;
    }

    if (rc_volume_init() != 0) {
        fprintf(stderr, "[main] PulseAudio init failed (volume disabled)\n");
    }

    if (rc_desktop_start(&g_desktop) != 0) {
        fprintf(stderr, "[main] Network start failed\n");
        rc_input_shutdown();
        rc_volume_shutdown();
        return EXIT_FAILURE;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fprintf(stdout, "[main] Ready. Announcing as \"%s\" on %s\n",
            g_desktop.info.name, g_desktop.info.lan_ip);

    if (rc_tray_init(on_quit, on_reconnect) == 0) {
        char status[128];
        snprintf(status, sizeof(status), "%s (%s)",
                 g_desktop.info.name, g_desktop.info.lan_ip);
        rc_tray_set_status(status);
        rc_tray_run();
    } else {
        fprintf(stdout, "[main] No tray. Press Ctrl+C to quit.\n");
        while (g_running) { sleep(1); }
    }

    rc_desktop_stop(&g_desktop);
    rc_tray_shutdown();
    rc_input_shutdown();
    rc_volume_shutdown();
    fprintf(stdout, "[main] Goodbye.\n");
    return EXIT_SUCCESS;
}
