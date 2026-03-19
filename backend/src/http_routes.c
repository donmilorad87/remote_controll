#include "http_routes.h"
#include "auth.h"
#include "jwt.h"
#include "email.h"
#include "admin_panel.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <jansson.h>

/* ─── Helper: send JSON response ─────────────────────────────────── */

static enum MHD_Result send_json(struct MHD_Connection *conn,
                                 unsigned int status, const char *json)
{
    assert(conn != NULL);
    assert(json != NULL);

    struct MHD_Response *resp = MHD_create_response_from_buffer(
        strlen(json), (void *)json, MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) return MHD_NO;

    MHD_add_response_header(resp, "Content-Type", "application/json");
    MHD_add_response_header(resp, "Access-Control-Allow-Origin", "*");

    enum MHD_Result ret = MHD_queue_response(conn, status, resp);
    MHD_destroy_response(resp);
    return ret;
}

static enum MHD_Result send_html(struct MHD_Connection *conn,
                                 unsigned int status, const char *html)
{
    assert(conn != NULL);
    assert(html != NULL);

    struct MHD_Response *resp = MHD_create_response_from_buffer(
        strlen(html), (void *)html, MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) return MHD_NO;

    MHD_add_response_header(resp, "Content-Type", "text/html; charset=utf-8");

    enum MHD_Result ret = MHD_queue_response(conn, status, resp);
    MHD_destroy_response(resp);
    return ret;
}

/* ─── POST /api/register ─────────────────────────────────────────── */

enum MHD_Result rc_route_register(struct MHD_Connection *conn,
                                  rc_config_t *cfg, rc_db_pool_t *pool,
                                  const char *body, size_t body_len)
{
    assert(conn != NULL);
    assert(cfg != NULL);
    assert(pool != NULL);

    if (body == NULL || body_len == 0) {
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"missing_body\"}");
    }

    /* Parse JSON body */
    json_error_t err;
    json_t *root = json_loadb(body, body_len, 0, &err);
    if (root == NULL) {
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"invalid_json\"}");
    }

    const char *email = json_string_value(json_object_get(root, "email"));
    const char *password = json_string_value(json_object_get(root, "password"));

    if (email == NULL || password == NULL) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"email_and_password_required\"}");
    }

    /* Validate */
    if (!rc_auth_validate_email(email)) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"invalid_email\"}");
    }

    if (!rc_auth_validate_password(password)) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"password_too_short\",\"message\":\"Minimum 8 characters\"}");
    }

    /* Hash password */
    char hash[256];
    if (rc_auth_hash_password(password, hash, sizeof(hash)) != 0) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                         "{\"error\":\"hash_failed\"}");
    }

    /* Register in database */
    rc_register_result_t result;
    if (rc_db_register_user(pool, email, hash, &result) != 0) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                         "{\"error\":\"database_error\"}");
    }

    if (result.error[0] != '\0') {
        json_decref(root);
        char resp[256];
        snprintf(resp, sizeof(resp), "{\"error\":\"%s\"}", result.error);
        return send_json(conn, MHD_HTTP_CONFLICT, resp);
    }

    /* Build activation URL (omit port for 443/80) */
    char activation_url[512];
    if (cfg->nginx_port == 443 || cfg->nginx_port == 80) {
        snprintf(activation_url, sizeof(activation_url),
                 "%s://%s/activate/%s",
                 cfg->scheme, cfg->domain, result.token);
    } else {
        snprintf(activation_url, sizeof(activation_url),
                 "%s://%s:%u/activate/%s",
                 cfg->scheme, cfg->domain, cfg->nginx_port, result.token);
    }

    /* Send activation email via SMTP */
    rc_email_send_activation(cfg, email, activation_url);

    json_decref(root);

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"message\":\"registration_successful\","
             "\"user_id\":\"%s\"}",
             result.user_id);
    return send_json(conn, MHD_HTTP_CREATED, resp);
}

/* ─── POST /api/login ────────────────────────────────────────────── */

enum MHD_Result rc_route_login(struct MHD_Connection *conn,
                               rc_config_t *cfg, rc_db_pool_t *pool,
                               const char *body, size_t body_len)
{
    assert(conn != NULL);
    assert(cfg != NULL);
    assert(pool != NULL);

    if (body == NULL || body_len == 0) {
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"missing_body\"}");
    }

    json_error_t err;
    json_t *root = json_loadb(body, body_len, 0, &err);
    if (root == NULL) {
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"invalid_json\"}");
    }

    const char *email = json_string_value(json_object_get(root, "email"));
    const char *password = json_string_value(json_object_get(root, "password"));
    const char *device_type = json_string_value(json_object_get(root, "device_type"));

    if (email == NULL || password == NULL || device_type == NULL) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"email_password_device_type_required\"}");
    }

    /* Validate device_type */
    if (strcmp(device_type, "desktop") != 0 && strcmp(device_type, "mobile") != 0) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"invalid_device_type\"}");
    }

    /* Look up user */
    rc_user_lookup_t user;
    if (rc_db_get_user_by_email(pool, email, &user) != 0) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                         "{\"error\":\"database_error\"}");
    }

    if (user.error[0] != '\0') {
        json_decref(root);
        return send_json(conn, MHD_HTTP_UNAUTHORIZED,
                         "{\"error\":\"invalid_credentials\"}");
    }

    /* Verify password */
    if (!rc_auth_verify_password(password, user.password_hash)) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_UNAUTHORIZED,
                         "{\"error\":\"invalid_credentials\"}");
    }

    /* Check activation */
    if (!user.is_activated) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_FORBIDDEN,
                         "{\"error\":\"account_not_activated\"}");
    }

    /* Create JWT */
    char token[RC_JWT_MAX_LEN];
    char jwt_id[RC_JWT_ID_LEN + 1];
    if (rc_jwt_create(cfg->jwt_secret, user.user_id, device_type,
                      cfg->jwt_expiry_seconds, token, sizeof(token),
                      jwt_id, sizeof(jwt_id)) != 0) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                         "{\"error\":\"token_creation_failed\"}");
    }

    /* Get client IP for session record */
    const union MHD_ConnectionInfo *info =
        MHD_get_connection_info(conn, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    const char *ip = (info != NULL) ? "0.0.0.0" : "0.0.0.0";
    (void)info; /* simplified for now */

    /* Create session in DB (this will kick existing desktop if device_type=desktop) */
    rc_session_create_t session;
    if (rc_db_create_session(pool, user.user_id, device_type, jwt_id,
                             ip, "RemoteControl Client",
                             cfg->jwt_expiry_seconds, &session) != 0) {
        json_decref(root);
        return send_json(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                         "{\"error\":\"session_creation_failed\"}");
    }

    json_decref(root);

    /* Build response */
    char resp[RC_JWT_MAX_LEN + 256];
    snprintf(resp, sizeof(resp),
             "{\"token\":\"%s\","
             "\"user_id\":\"%s\","
             "\"session_id\":\"%s\","
             "\"expires_in\":%u}",
             token, user.user_id, session.session_id,
             cfg->jwt_expiry_seconds);
    return send_json(conn, MHD_HTTP_OK, resp);
}

/* ─── GET /api/activate/:token ───────────────────────────────────── */

enum MHD_Result rc_route_activate(struct MHD_Connection *conn,
                                  rc_db_pool_t *pool, const char *token)
{
    assert(conn != NULL);
    assert(pool != NULL);

    if (token == NULL || token[0] == '\0') {
        return send_html(conn, MHD_HTTP_BAD_REQUEST,
            "<html><body><h2>Invalid activation link</h2></body></html>");
    }

    rc_activate_result_t result;
    if (rc_db_activate_user(pool, token, &result) != 0) {
        return send_html(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
            "<html><body><h2>Server error</h2></body></html>");
    }

    if (result.error[0] != '\0') {
        char html[512];
        snprintf(html, sizeof(html),
            "<html><body style=\"font-family:Arial,sans-serif;text-align:center;"
            "padding:50px;\"><h2 style=\"color:#dc2626;\">Activation Failed</h2>"
            "<p>%s</p></body></html>",
            result.error);
        return send_html(conn, MHD_HTTP_BAD_REQUEST, html);
    }

    return send_html(conn, MHD_HTTP_OK,
        "<html><body style=\"font-family:Arial,sans-serif;text-align:center;"
        "padding:50px;\"><h2 style=\"color:#16a34a;\">Account Activated!</h2>"
        "<p>You can now log in from your desktop and mobile apps.</p>"
        "</body></html>");
}

/* ─── POST /api/logout ───────────────────────────────────────────── */

enum MHD_Result rc_route_logout(struct MHD_Connection *conn,
                                rc_config_t *cfg, rc_db_pool_t *pool,
                                const char *body, size_t body_len)
{
    assert(conn != NULL);
    assert(cfg != NULL);
    assert(pool != NULL);

    /* Get Authorization header */
    const char *auth_header = MHD_lookup_connection_value(
        conn, MHD_HEADER_KIND, "Authorization");

    if (auth_header == NULL || strncmp(auth_header, "Bearer ", 7) != 0) {
        return send_json(conn, MHD_HTTP_UNAUTHORIZED,
                         "{\"error\":\"missing_token\"}");
    }

    const char *token = auth_header + 7;
    (void)body;
    (void)body_len;

    /* Verify JWT to get jwt_id */
    rc_jwt_claims_t claims;
    if (rc_jwt_verify(cfg->jwt_secret, token, &claims) != 0) {
        return send_json(conn, MHD_HTTP_UNAUTHORIZED,
                         "{\"error\":\"invalid_token\"}");
    }

    /* Revoke session */
    rc_db_revoke_session(pool, claims.jwt_id);

    return send_json(conn, MHD_HTTP_OK,
                     "{\"message\":\"logged_out\"}");
}

/* ─── GET /health ────────────────────────────────────────────────── */

enum MHD_Result rc_route_health(struct MHD_Connection *conn)
{
    assert(conn != NULL);
    return send_json(conn, MHD_HTTP_OK, "{\"status\":\"ok\"}");
}

/* ─── GET /admin/... ──────────────────────────────────────────────── */

enum MHD_Result rc_route_admin(struct MHD_Connection *conn,
                               rc_db_pool_t *pool, const char *sub_path)
{
    assert(conn != NULL);
    assert(pool != NULL);

    char *html = NULL;

    if (sub_path == NULL || strcmp(sub_path, "/") == 0 ||
        strcmp(sub_path, "") == 0) {
        html = rc_admin_render_dashboard(pool);
    } else if (strncmp(sub_path, "/users", 6) == 0) {
        /* Parse page parameter */
        const char *page_str = MHD_lookup_connection_value(
            conn, MHD_GET_ARGUMENT_KIND, "page");
        int page = (page_str != NULL) ? atoi(page_str) : 1;
        if (page < 1) page = 1;
        html = rc_admin_render_users(pool, page);
    } else if (strcmp(sub_path, "/connections") == 0) {
        html = rc_admin_render_connections(pool);
    }

    if (html == NULL) {
        return send_html(conn, MHD_HTTP_NOT_FOUND,
            "<html><body><h2>Page not found</h2></body></html>");
    }

    enum MHD_Result ret = send_html(conn, MHD_HTTP_OK, html);
    free(html);
    return ret;
}
