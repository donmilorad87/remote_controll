#include "database.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int rc_db_init(rc_db_pool_t *pool, const char *connstr)
{
    assert(pool != NULL);
    assert(connstr != NULL);

    memset(pool, 0, sizeof(*pool));
    rc_strlcpy(pool->connstr, connstr, sizeof(pool->connstr));
    pthread_mutex_init(&pool->mutex, NULL);

    for (int i = 0; i < RC_DB_POOL_SIZE; i++) {
        pool->connections[i] = PQconnectdb(connstr);
        if (PQstatus(pool->connections[i]) != CONNECTION_OK) {
            fprintf(stderr, "[db] Connection %d failed: %s\n",
                    i, PQerrorMessage(pool->connections[i]));
            /* Clean up already-created connections */
            for (int j = 0; j < i; j++) {
                PQfinish(pool->connections[j]);
                pool->connections[j] = NULL;
            }
            PQfinish(pool->connections[i]);
            pool->connections[i] = NULL;
            return -1;
        }
        pool->in_use[i] = false;
        pool->count++;
    }

    fprintf(stdout, "[db] Pool initialized with %d connections\n", pool->count);
    return 0;
}

PGconn *rc_db_acquire(rc_db_pool_t *pool)
{
    assert(pool != NULL);

    pthread_mutex_lock(&pool->mutex);

    for (int i = 0; i < pool->count; i++) {
        if (!pool->in_use[i]) {
            /* Verify connection is still alive */
            if (PQstatus(pool->connections[i]) != CONNECTION_OK) {
                PQreset(pool->connections[i]);
                if (PQstatus(pool->connections[i]) != CONNECTION_OK) {
                    fprintf(stderr, "[db] Failed to reset connection %d\n", i);
                    continue;
                }
            }
            pool->in_use[i] = true;
            pthread_mutex_unlock(&pool->mutex);
            return pool->connections[i];
        }
    }

    pthread_mutex_unlock(&pool->mutex);
    fprintf(stderr, "[db] No available connections in pool\n");
    return NULL;
}

void rc_db_release(rc_db_pool_t *pool, PGconn *conn)
{
    assert(pool != NULL);
    assert(conn != NULL);

    pthread_mutex_lock(&pool->mutex);

    for (int i = 0; i < pool->count; i++) {
        if (pool->connections[i] == conn) {
            pool->in_use[i] = false;
            pthread_mutex_unlock(&pool->mutex);
            return;
        }
    }

    pthread_mutex_unlock(&pool->mutex);
    fprintf(stderr, "[db] Released unknown connection\n");
}

void rc_db_shutdown(rc_db_pool_t *pool)
{
    assert(pool != NULL);

    pthread_mutex_lock(&pool->mutex);
    for (int i = 0; i < pool->count; i++) {
        if (pool->connections[i] != NULL) {
            PQfinish(pool->connections[i]);
            pool->connections[i] = NULL;
        }
    }
    pool->count = 0;
    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);

    fprintf(stdout, "[db] Pool shut down\n");
}

/* ─── Stored Procedure Wrappers ──────────────────────────────────── */

int rc_db_register_user(rc_db_pool_t *pool, const char *email,
                        const char *password_hash, rc_register_result_t *result)
{
    assert(pool != NULL);
    assert(email != NULL);
    assert(result != NULL);

    memset(result, 0, sizeof(*result));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[2] = { email, password_hash };
    PGresult *res = PQexecParams(conn,
        "SELECT out_user_id, out_token, out_error FROM sp_register_user($1, $2)",
        2, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[db] sp_register_user failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        if (!PQgetisnull(res, 0, 0))
            rc_strlcpy(result->user_id, PQgetvalue(res, 0, 0), sizeof(result->user_id));
        if (!PQgetisnull(res, 0, 1))
            rc_strlcpy(result->token, PQgetvalue(res, 0, 1), sizeof(result->token));
        if (!PQgetisnull(res, 0, 2))
            rc_strlcpy(result->error, PQgetvalue(res, 0, 2), sizeof(result->error));
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}

int rc_db_activate_user(rc_db_pool_t *pool, const char *token,
                        rc_activate_result_t *result)
{
    assert(pool != NULL);
    assert(token != NULL);
    assert(result != NULL);

    memset(result, 0, sizeof(*result));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[1] = { token };
    PGresult *res = PQexecParams(conn,
        "SELECT out_user_id, out_error FROM sp_activate_user($1)",
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[db] sp_activate_user failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        if (!PQgetisnull(res, 0, 0))
            rc_strlcpy(result->user_id, PQgetvalue(res, 0, 0), sizeof(result->user_id));
        if (!PQgetisnull(res, 0, 1))
            rc_strlcpy(result->error, PQgetvalue(res, 0, 1), sizeof(result->error));
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}

int rc_db_get_user_by_email(rc_db_pool_t *pool, const char *email,
                            rc_user_lookup_t *result)
{
    assert(pool != NULL);
    assert(email != NULL);
    assert(result != NULL);

    memset(result, 0, sizeof(*result));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[1] = { email };
    PGresult *res = PQexecParams(conn,
        "SELECT out_user_id, out_password_hash, out_is_activated, out_error "
        "FROM sp_get_user_by_email($1)",
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[db] sp_get_user_by_email failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        if (!PQgetisnull(res, 0, 0))
            rc_strlcpy(result->user_id, PQgetvalue(res, 0, 0), sizeof(result->user_id));
        if (!PQgetisnull(res, 0, 1))
            rc_strlcpy(result->password_hash, PQgetvalue(res, 0, 1),
                       sizeof(result->password_hash));
        if (!PQgetisnull(res, 0, 2))
            result->is_activated = (strcmp(PQgetvalue(res, 0, 2), "t") == 0);
        if (!PQgetisnull(res, 0, 3))
            rc_strlcpy(result->error, PQgetvalue(res, 0, 3), sizeof(result->error));
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}

int rc_db_create_session(rc_db_pool_t *pool, const char *user_id,
                         const char *device_type, const char *jwt_id,
                         const char *ip_address, const char *user_agent,
                         uint32_t expiry_seconds, rc_session_create_t *result)
{
    assert(pool != NULL);
    assert(user_id != NULL);
    assert(result != NULL);

    memset(result, 0, sizeof(*result));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    char expiry_str[32];
    snprintf(expiry_str, sizeof(expiry_str),
             "NOW() + INTERVAL '%u seconds'", expiry_seconds);

    /* Build the query with the interval inline since parameterized intervals are tricky */
    char query[1024];
    snprintf(query, sizeof(query),
        "SELECT out_session_id, out_kicked_session, out_error "
        "FROM sp_create_session($1, $2, $3, $4, $5, NOW() + INTERVAL '%u seconds')",
        expiry_seconds);

    const char *params[5] = { user_id, device_type, jwt_id, ip_address, user_agent };
    PGresult *res = PQexecParams(conn, query, 5, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[db] sp_create_session failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        if (!PQgetisnull(res, 0, 0))
            rc_strlcpy(result->session_id, PQgetvalue(res, 0, 0),
                       sizeof(result->session_id));
        if (!PQgetisnull(res, 0, 1))
            rc_strlcpy(result->kicked_session_id, PQgetvalue(res, 0, 1),
                       sizeof(result->kicked_session_id));
        if (!PQgetisnull(res, 0, 2))
            rc_strlcpy(result->error, PQgetvalue(res, 0, 2), sizeof(result->error));
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}

int rc_db_validate_session(rc_db_pool_t *pool, const char *jwt_id,
                           rc_session_validate_t *result)
{
    assert(pool != NULL);
    assert(jwt_id != NULL);
    assert(result != NULL);

    memset(result, 0, sizeof(*result));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[1] = { jwt_id };
    PGresult *res = PQexecParams(conn,
        "SELECT out_session_id, out_user_id, out_device_type, out_error "
        "FROM sp_validate_session($1)",
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[db] sp_validate_session failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        if (!PQgetisnull(res, 0, 0))
            rc_strlcpy(result->session_id, PQgetvalue(res, 0, 0),
                       sizeof(result->session_id));
        if (!PQgetisnull(res, 0, 1))
            rc_strlcpy(result->user_id, PQgetvalue(res, 0, 1),
                       sizeof(result->user_id));
        if (!PQgetisnull(res, 0, 2))
            rc_strlcpy(result->device_type, PQgetvalue(res, 0, 2),
                       sizeof(result->device_type));
        if (!PQgetisnull(res, 0, 3))
            rc_strlcpy(result->error, PQgetvalue(res, 0, 3), sizeof(result->error));
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}

int rc_db_revoke_session(rc_db_pool_t *pool, const char *jwt_id)
{
    assert(pool != NULL);
    assert(jwt_id != NULL);

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[1] = { jwt_id };
    PGresult *res = PQexecParams(conn,
        "SELECT out_success, out_error FROM sp_revoke_session($1)",
        1, NULL, params, NULL, NULL, 0);

    int ret = (PQresultStatus(res) == PGRES_TUPLES_OK) ? 0 : -1;

    PQclear(res);
    rc_db_release(pool, conn);
    return ret;
}

int rc_db_connect_device(rc_db_pool_t *pool, const char *user_id,
                         const char *session_id, const char *device_type,
                         const char *platform, rc_connect_device_t *result)
{
    assert(pool != NULL);
    assert(user_id != NULL);
    assert(result != NULL);

    memset(result, 0, sizeof(*result));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[4] = { user_id, session_id, device_type, platform };
    PGresult *res = PQexecParams(conn,
        "SELECT out_connection_id, out_error FROM sp_connect_device($1, $2, $3, $4)",
        4, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "[db] sp_connect_device failed: %s\n", PQerrorMessage(conn));
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        if (!PQgetisnull(res, 0, 0))
            rc_strlcpy(result->connection_id, PQgetvalue(res, 0, 0),
                       sizeof(result->connection_id));
        if (!PQgetisnull(res, 0, 1))
            rc_strlcpy(result->error, PQgetvalue(res, 0, 1), sizeof(result->error));
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}

int rc_db_disconnect_device(rc_db_pool_t *pool, const char *connection_id)
{
    assert(pool != NULL);
    assert(connection_id != NULL);

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[1] = { connection_id };
    PGresult *res = PQexecParams(conn,
        "SELECT sp_disconnect_device($1)",
        1, NULL, params, NULL, NULL, 0);

    int ret = (PQresultStatus(res) == PGRES_TUPLES_OK) ? 0 : -1;

    PQclear(res);
    rc_db_release(pool, conn);
    return ret;
}

int rc_db_get_paired_desktop(rc_db_pool_t *pool, const char *user_id,
                             rc_paired_device_t *result)
{
    assert(pool != NULL);
    assert(user_id != NULL);
    assert(result != NULL);

    memset(result, 0, sizeof(*result));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[1] = { user_id };
    PGresult *res = PQexecParams(conn,
        "SELECT out_connection_id, out_session_id, out_platform "
        "FROM sp_get_paired_desktop($1)",
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        result->found = true;
        rc_strlcpy(result->connection_id, PQgetvalue(res, 0, 0),
                   sizeof(result->connection_id));
        rc_strlcpy(result->session_id, PQgetvalue(res, 0, 1),
                   sizeof(result->session_id));
        rc_strlcpy(result->platform, PQgetvalue(res, 0, 2),
                   sizeof(result->platform));
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}

int rc_db_get_paired_mobile(rc_db_pool_t *pool, const char *user_id,
                            rc_paired_device_t *result)
{
    assert(pool != NULL);
    assert(user_id != NULL);
    assert(result != NULL);

    memset(result, 0, sizeof(*result));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    const char *params[1] = { user_id };
    PGresult *res = PQexecParams(conn,
        "SELECT out_connection_id, out_session_id, out_platform "
        "FROM sp_get_paired_mobile($1)",
        1, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        result->found = true;
        rc_strlcpy(result->connection_id, PQgetvalue(res, 0, 0),
                   sizeof(result->connection_id));
        rc_strlcpy(result->session_id, PQgetvalue(res, 0, 1),
                   sizeof(result->session_id));
        rc_strlcpy(result->platform, PQgetvalue(res, 0, 2),
                   sizeof(result->platform));
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}

int rc_db_admin_stats(rc_db_pool_t *pool, rc_admin_stats_t *stats)
{
    assert(pool != NULL);
    assert(stats != NULL);

    memset(stats, 0, sizeof(*stats));

    PGconn *conn = rc_db_acquire(pool);
    if (conn == NULL) return -1;

    PGresult *res = PQexec(conn,
        "SELECT out_total_users, out_activated_users, "
        "out_active_sessions, out_active_connections "
        "FROM sp_admin_dashboard_stats()");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        rc_db_release(pool, conn);
        return -1;
    }

    if (PQntuples(res) > 0) {
        stats->total_users = strtoll(PQgetvalue(res, 0, 0), NULL, 10);
        stats->activated_users = strtoll(PQgetvalue(res, 0, 1), NULL, 10);
        stats->active_sessions = strtoll(PQgetvalue(res, 0, 2), NULL, 10);
        stats->active_connections = strtoll(PQgetvalue(res, 0, 3), NULL, 10);
    }

    PQclear(res);
    rc_db_release(pool, conn);
    return 0;
}
