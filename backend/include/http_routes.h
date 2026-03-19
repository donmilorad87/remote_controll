#ifndef RC_HTTP_ROUTES_H
#define RC_HTTP_ROUTES_H

#include <microhttpd.h>
#include "config.h"
#include "database.h"

/* POST /api/register */
enum MHD_Result rc_route_register(struct MHD_Connection *conn,
                                  rc_config_t *cfg, rc_db_pool_t *pool,
                                  const char *body, size_t body_len);

/* POST /api/login */
enum MHD_Result rc_route_login(struct MHD_Connection *conn,
                               rc_config_t *cfg, rc_db_pool_t *pool,
                               const char *body, size_t body_len);

/* GET /api/activate/:token */
enum MHD_Result rc_route_activate(struct MHD_Connection *conn,
                                  rc_db_pool_t *pool, const char *token);

/* POST /api/logout */
enum MHD_Result rc_route_logout(struct MHD_Connection *conn,
                                rc_config_t *cfg, rc_db_pool_t *pool,
                                const char *body, size_t body_len);

/* GET /health */
enum MHD_Result rc_route_health(struct MHD_Connection *conn);

/* GET /admin/... */
enum MHD_Result rc_route_admin(struct MHD_Connection *conn,
                               rc_db_pool_t *pool, const char *sub_path);

#endif /* RC_HTTP_ROUTES_H */
