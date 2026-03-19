#ifndef RC_LOGIN_WINDOW_H
#define RC_LOGIN_WINDOW_H

#include "rc_client.h"

typedef struct {
    char email[256];
    char password[128];
    int  action;  /* 0=cancel, 1=login, 2=register */
} rc_login_result_t;

/* Show GTK login dialog. Blocks until user submits or cancels.
   Returns 0 on success, -1 if cancelled. */
int rc_login_window_show(rc_login_result_t *result);

#endif /* RC_LOGIN_WINDOW_H */
