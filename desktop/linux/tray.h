#ifndef RC_TRAY_H
#define RC_TRAY_H

#include <stdbool.h>

/* Callback types for tray menu actions */
typedef void (*rc_tray_quit_cb)(void);
typedef void (*rc_tray_reconnect_cb)(void);

/* Initialize system tray icon. Returns 0 on success. */
int rc_tray_init(rc_tray_quit_cb on_quit, rc_tray_reconnect_cb on_reconnect);

/* Update tray status text. */
void rc_tray_set_status(const char *status);

/* Run the tray main loop (blocks). */
void rc_tray_run(void);

/* Stop the tray. */
void rc_tray_shutdown(void);

#endif /* RC_TRAY_H */
