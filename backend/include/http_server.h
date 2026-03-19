#ifndef RC_HTTP_SERVER_H
#define RC_HTTP_SERVER_H

#include <microhttpd.h>
#include "config.h"
#include "database.h"

typedef struct {
    struct MHD_Daemon   *daemon;
    rc_config_t         *config;
    rc_db_pool_t        *db_pool;
} rc_http_server_t;

/* Start the HTTP server. Returns 0 on success. */
int rc_http_start(rc_http_server_t *server, rc_config_t *cfg, rc_db_pool_t *pool);

/* Stop the HTTP server. */
void rc_http_stop(rc_http_server_t *server);

#endif /* RC_HTTP_SERVER_H */
