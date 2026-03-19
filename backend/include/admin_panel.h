#ifndef RC_ADMIN_PANEL_H
#define RC_ADMIN_PANEL_H

#include <microhttpd.h>
#include "database.h"

/* Render the admin dashboard page. Caller must free returned string. */
char *rc_admin_render_dashboard(rc_db_pool_t *pool);

/* Render the users list page. Caller must free returned string. */
char *rc_admin_render_users(rc_db_pool_t *pool, int page);

/* Render the active connections page. Caller must free returned string. */
char *rc_admin_render_connections(rc_db_pool_t *pool);

#endif /* RC_ADMIN_PANEL_H */
