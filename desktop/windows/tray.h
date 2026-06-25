#ifndef RC_TRAY_H
#define RC_TRAY_H

#include <stdbool.h>
#include <windows.h>

typedef void (*rc_tray_quit_cb)(void);
typedef void (*rc_tray_reconnect_cb)(void);

int rc_tray_init(HINSTANCE hInstance, rc_tray_quit_cb on_quit,
                 rc_tray_reconnect_cb on_reconnect);
void rc_tray_set_status(const char *status);
void rc_tray_run(void);
void rc_tray_shutdown(void);

#endif
