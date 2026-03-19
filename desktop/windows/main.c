#include "rc_client.h"
#include "rc_protocol.h"
#include "input_win32.h"
#include "volume_win32.h"
#include "login_window.h"
#include "tray.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <curl/curl.h>

static rc_client_t g_client;

/* Called by recv thread in client_net.c */
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
    rc_client_disconnect(&g_client);
    rc_client_logout(&g_client);
}

static void on_reconnect(void)
{
    if (g_client.connected) rc_client_disconnect(&g_client);
    if (g_client.auth.authenticated) rc_client_connect(&g_client);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR cmdLine, int nShow)
{
    (void)hPrev;
    (void)cmdLine;
    (void)nShow;

    curl_global_init(CURL_GLOBAL_ALL);

    memset(&g_client, 0, sizeof(g_client));
    g_client.udp_fd = -1;

    rc_client_load_config(&g_client.config, ".env");
    rc_input_init();
    rc_volume_init();

    /* Show login window */
    rc_login_result_t login_result;
    if (rc_login_window_show(&login_result) != 0) {
        rc_input_shutdown();
        rc_volume_shutdown();
        curl_global_cleanup();
        return 0;
    }

    /* Handle register */
    if (login_result.action == 2) {
        rc_client_register(&g_client, login_result.email, login_result.password);
        MessageBoxA(NULL, "Check your email for activation link.",
                    "RemoteControl", MB_OK | MB_ICONINFORMATION);
        rc_input_shutdown();
        rc_volume_shutdown();
        curl_global_cleanup();
        return 0;
    }

    /* Login */
    if (rc_client_login(&g_client, login_result.email,
                        login_result.password) != 0) {
        MessageBoxA(NULL, "Login failed.", "RemoteControl", MB_OK | MB_ICONERROR);
        rc_input_shutdown();
        rc_volume_shutdown();
        curl_global_cleanup();
        return 1;
    }

    /* Connect to socket server */
    if (rc_client_connect(&g_client) != 0) {
        MessageBoxA(NULL, "Connection failed.", "RemoteControl",
                    MB_OK | MB_ICONERROR);
        rc_client_logout(&g_client);
        rc_input_shutdown();
        rc_volume_shutdown();
        curl_global_cleanup();
        return 1;
    }

    /* Initialize and run system tray */
    if (rc_tray_init(hInstance, on_quit, on_reconnect) == 0) {
        rc_tray_set_status("Connected");
        rc_tray_run(); /* Blocks until quit */
    }

    /* Cleanup */
    rc_client_disconnect(&g_client);
    rc_client_logout(&g_client);
    rc_tray_shutdown();
    rc_input_shutdown();
    rc_volume_shutdown();
    curl_global_cleanup();

    return 0;
}
