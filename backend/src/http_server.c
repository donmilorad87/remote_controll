#include "http_server.h"
#include "http_routes.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_POST_BODY   (64 * 1024)  /* 64 KB max request body */

/* Per-connection context for accumulating POST body */
typedef struct {
    char    *body;
    size_t   body_len;
    size_t   body_cap;
} rc_http_conn_ctx_t;

/* Send a simple JSON error response */
static enum MHD_Result send_json_response(struct MHD_Connection *conn,
                                          unsigned int status_code,
                                          const char *json_body)
{
    assert(conn != NULL);
    assert(json_body != NULL);

    struct MHD_Response *resp = MHD_create_response_from_buffer(
        strlen(json_body), (void *)json_body, MHD_RESPMEM_MUST_COPY);

    if (resp == NULL) return MHD_NO;

    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(resp, "Access-Control-Allow-Methods",
                            "GET, POST, OPTIONS");
    MHD_add_response_header(resp, "Access-Control-Allow-Headers",
                            "Content-Type, Authorization");

    enum MHD_Result ret = MHD_queue_response(conn, status_code, resp);
    MHD_destroy_response(resp);
    return ret;
}

static enum MHD_Result request_handler(void *cls,
                                       struct MHD_Connection *conn,
                                       const char *url,
                                       const char *method,
                                       const char *version,
                                       const char *upload_data,
                                       size_t *upload_data_size,
                                       void **con_cls)
{
    rc_http_server_t *server = (rc_http_server_t *)cls;
    (void)version;

    assert(server != NULL);
    assert(url != NULL);

    /* Handle CORS preflight */
    if (strcmp(method, "OPTIONS") == 0) {
        return send_json_response(conn, MHD_HTTP_OK, "{}");
    }

    /* First call: allocate connection context */
    if (*con_cls == NULL) {
        rc_http_conn_ctx_t *ctx = calloc(1, sizeof(rc_http_conn_ctx_t));
        if (ctx == NULL) {
            return send_json_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                                      "{\"error\":\"out_of_memory\"}");
        }
        *con_cls = ctx;
        return MHD_YES;
    }

    rc_http_conn_ctx_t *ctx = (rc_http_conn_ctx_t *)*con_cls;

    /* Accumulate POST body */
    if (*upload_data_size > 0) {
        size_t new_len = ctx->body_len + *upload_data_size;
        if (new_len > MAX_POST_BODY) {
            return send_json_response(conn, MHD_HTTP_CONTENT_TOO_LARGE,
                                      "{\"error\":\"body_too_large\"}");
        }

        if (new_len > ctx->body_cap) {
            size_t new_cap = new_len + 1024;
            char *new_body = realloc(ctx->body, new_cap);
            if (new_body == NULL) {
                return send_json_response(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                                          "{\"error\":\"out_of_memory\"}");
            }
            ctx->body = new_body;
            ctx->body_cap = new_cap;
        }

        memcpy(ctx->body + ctx->body_len, upload_data, *upload_data_size);
        ctx->body_len = new_len;
        ctx->body[ctx->body_len] = '\0';
        *upload_data_size = 0;
        return MHD_YES;
    }

    /* ─── Route matching ─────────────────────────────────────────── */

    /* GET /health */
    if (strcmp(url, "/health") == 0 && strcmp(method, "GET") == 0) {
        return rc_route_health(conn);
    }

    /* POST /api/register */
    if (strcmp(url, "/api/register") == 0 && strcmp(method, "POST") == 0) {
        return rc_route_register(conn, server->config, server->db_pool,
                                 ctx->body, ctx->body_len);
    }

    /* POST /api/login */
    if (strcmp(url, "/api/login") == 0 && strcmp(method, "POST") == 0) {
        return rc_route_login(conn, server->config, server->db_pool,
                              ctx->body, ctx->body_len);
    }

    /* GET /api/activate/:token */
    if (strncmp(url, "/api/activate/", 14) == 0 && strcmp(method, "GET") == 0) {
        const char *token = url + 14;
        return rc_route_activate(conn, server->db_pool, token);
    }

    /* POST /api/logout */
    if (strcmp(url, "/api/logout") == 0 && strcmp(method, "POST") == 0) {
        return rc_route_logout(conn, server->config, server->db_pool,
                               ctx->body, ctx->body_len);
    }

    /* GET /admin/... */
    if (strncmp(url, "/admin", 6) == 0 && strcmp(method, "GET") == 0) {
        const char *sub_path = (strlen(url) > 6) ? url + 6 : "/";
        return rc_route_admin(conn, server->db_pool, sub_path);
    }

    /* 404 */
    return send_json_response(conn, MHD_HTTP_NOT_FOUND,
                              "{\"error\":\"not_found\"}");
}

static void request_completed(void *cls, struct MHD_Connection *conn,
                              void **con_cls,
                              enum MHD_RequestTerminationCode toe)
{
    (void)cls;
    (void)conn;
    (void)toe;

    if (*con_cls != NULL) {
        rc_http_conn_ctx_t *ctx = (rc_http_conn_ctx_t *)*con_cls;
        free(ctx->body);
        free(ctx);
        *con_cls = NULL;
    }
}

int rc_http_start(rc_http_server_t *server, rc_config_t *cfg, rc_db_pool_t *pool)
{
    assert(server != NULL);
    assert(cfg != NULL);
    assert(pool != NULL);

    server->config = cfg;
    server->db_pool = pool;

    server->daemon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG,
        cfg->api_port,
        NULL, NULL,
        &request_handler, server,
        MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL,
        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)30,
        MHD_OPTION_THREAD_POOL_SIZE, (unsigned int)4,
        MHD_OPTION_END);

    if (server->daemon == NULL) {
        fprintf(stderr, "[http] Failed to start HTTP server on port %u\n",
                cfg->api_port);
        return -1;
    }

    fprintf(stdout, "[http] Server listening on port %u\n", cfg->api_port);
    return 0;
}

void rc_http_stop(rc_http_server_t *server)
{
    assert(server != NULL);

    if (server->daemon != NULL) {
        MHD_stop_daemon(server->daemon);
        server->daemon = NULL;
    }

    fprintf(stdout, "[http] Server stopped\n");
}
