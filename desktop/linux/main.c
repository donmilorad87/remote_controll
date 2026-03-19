#include "rc_client.h"
#include "rc_protocol.h"
#include "input_x11.h"
#include "volume_pulse.h"
#include "login_window.h"
#include "tray.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

static rc_client_t g_client;
static volatile sig_atomic_t g_running = 1;

/* Called by client_net.c recv thread when an event arrives from the server */
void rc_handle_event(const rc_event_t *event)
{
    assert(event != NULL);

    switch (event->type) {
    case RC_EVT_CURSOR_MOVE:
        rc_input_move_cursor(event->data.cursor_move.direction,
                             event->data.cursor_move.speed);
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

static void on_quit(void)
{
    g_running = 0;
}

static void on_reconnect(void)
{
    if (g_client.connected) {
        rc_client_disconnect(&g_client);
    }
    if (g_client.auth.authenticated) {
        if (rc_client_connect(&g_client) == 0) {
            rc_tray_set_status("Connected");
        } else {
            rc_tray_set_status("Reconnect failed");
        }
    }
}

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(void)
{
    fprintf(stdout, "=== RemoteControl Desktop (Linux) ===\n");

    curl_global_init(CURL_GLOBAL_ALL);

    /* Load config */
    memset(&g_client, 0, sizeof(g_client));
    g_client.udp_fd = -1;

    if (rc_client_load_config(&g_client.config, ".env") != 0) {
        fprintf(stderr, "[main] Failed to load config\n");
        return EXIT_FAILURE;
    }

    /* Initialize X11 input */
    if (rc_input_init() != 0) {
        fprintf(stderr, "[main] Failed to init X11 input\n");
        return EXIT_FAILURE;
    }

    /* Initialize PulseAudio volume */
    if (rc_volume_init() != 0) {
        fprintf(stderr, "[main] Warning: PulseAudio init failed, volume control disabled\n");
    }

    /* Show login window */
    rc_login_result_t login_result;
    if (rc_login_window_show(&login_result) != 0) {
        fprintf(stdout, "[main] Login cancelled\n");
        rc_input_shutdown();
        rc_volume_shutdown();
        return EXIT_SUCCESS;
    }

    /* Handle register or login */
    if (login_result.action == 2) {
        /* Register */
        if (rc_client_register(&g_client, login_result.email,
                               login_result.password) != 0) {
            fprintf(stderr, "[main] Registration failed\n");
            rc_input_shutdown();
            rc_volume_shutdown();
            return EXIT_FAILURE;
        }
        fprintf(stdout, "[main] Check your email for activation, then restart.\n");
        rc_input_shutdown();
        rc_volume_shutdown();
        return EXIT_SUCCESS;
    }

    /* Login */
    if (rc_client_login(&g_client, login_result.email,
                        login_result.password) != 0) {
        fprintf(stderr, "[main] Login failed\n");
        rc_input_shutdown();
        rc_volume_shutdown();
        return EXIT_FAILURE;
    }

    /* Connect to socket server */
    if (rc_client_connect(&g_client) != 0) {
        fprintf(stderr, "[main] Socket connection failed\n");
        rc_client_logout(&g_client);
        rc_input_shutdown();
        rc_volume_shutdown();
        return EXIT_FAILURE;
    }

    /* Signal handling */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize and run system tray */
    if (rc_tray_init(on_quit, on_reconnect) == 0) {
        rc_tray_set_status("Connected");
        rc_tray_run(); /* Blocks until quit */
    } else {
        /* Fallback: no tray, just wait */
        fprintf(stdout, "[main] Running without tray. Press Ctrl+C to quit.\n");
        while (g_running) {
            sleep(1);
        }
    }

    /* Cleanup */
    fprintf(stdout, "\n[main] Shutting down...\n");
    rc_client_disconnect(&g_client);
    rc_client_logout(&g_client);
    rc_tray_shutdown();
    rc_input_shutdown();
    rc_volume_shutdown();
    curl_global_cleanup();

    fprintf(stdout, "[main] Goodbye.\n");
    return EXIT_SUCCESS;
}
