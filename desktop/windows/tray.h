#ifndef RC_TRAY_H
#define RC_TRAY_H

#include <stdbool.h>
#include <windows.h>

typedef void (*rc_tray_quit_cb)(void);
typedef void (*rc_tray_reconnect_cb)(void);

/* Initialize system tray icon. Returns 0 on success. */
int rc_tray_init(HINSTANCE hInstance, rc_tray_quit_cb on_quit,
                 rc_tray_reconnect_cb on_reconnect);

/* Update tray tooltip text. */
void rc_tray_set_status(const char *status);

/* Run the tray message loop (blocks). */
void rc_tray_run(void);

/* Stop the tray and clean up. */
void rc_tray_shutdown(void);

#endif /* RC_TRAY_H */
