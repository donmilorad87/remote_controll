#ifndef RC_DATABASE_H
#define RC_DATABASE_H

#include <libpq-fe.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#define RC_DB_POOL_SIZE 10

typedef struct {
    PGconn      *connections[RC_DB_POOL_SIZE];
    bool         in_use[RC_DB_POOL_SIZE];
    pthread_mutex_t mutex;
    int          count;
    char         connstr[512];
} rc_db_pool_t;

/* Initialize the database connection pool. */
int rc_db_init(rc_db_pool_t *pool, const char *connstr);

/* Acquire a connection from the pool. Returns NULL if none available. */
PGconn *rc_db_acquire(rc_db_pool_t *pool);

/* Release a connection back to the pool. */
void rc_db_release(rc_db_pool_t *pool, PGconn *conn);

/* Run migrations from the given directory. */
int rc_db_run_migrations(rc_db_pool_t *pool, const char *migrations_path);

/* Shutdown and close all connections. */
void rc_db_shutdown(rc_db_pool_t *pool);

/* ─── Stored procedure wrappers ─────────────────────────────────── */

typedef struct {
    char user_id[64];
    char token[256];
    char error[128];
} rc_register_result_t;

typedef struct {
    char user_id[64];
    char error[128];
} rc_activate_result_t;

typedef struct {
    char user_id[64];
    char password_hash[256];
    bool is_activated;
    char error[128];
} rc_user_lookup_t;

typedef struct {
    char session_id[64];
    char kicked_session_id[64];
    char error[128];
} rc_session_create_t;

typedef struct {
    char session_id[64];
    char user_id[64];
    char device_type[24];
    char error[128];
} rc_session_validate_t;

typedef struct {
    char connection_id[64];
    char error[128];
} rc_connect_device_t;

typedef struct {
    char connection_id[64];
    char session_id[64];
    char platform[24];
    bool found;
} rc_paired_device_t;

typedef struct {
    int64_t total_users;
    int64_t activated_users;
    int64_t active_sessions;
    int64_t active_connections;
} rc_admin_stats_t;

int rc_db_register_user(rc_db_pool_t *pool, const char *email,
                        const char *password_hash, rc_register_result_t *result);

int rc_db_activate_user(rc_db_pool_t *pool, const char *token,
                        rc_activate_result_t *result);

int rc_db_get_user_by_email(rc_db_pool_t *pool, const char *email,
                            rc_user_lookup_t *result);

int rc_db_create_session(rc_db_pool_t *pool, const char *user_id,
                         const char *device_type, const char *jwt_id,
                         const char *ip_address, const char *user_agent,
                         uint32_t expiry_seconds, rc_session_create_t *result);

int rc_db_validate_session(rc_db_pool_t *pool, const char *jwt_id,
                           rc_session_validate_t *result);

int rc_db_revoke_session(rc_db_pool_t *pool, const char *jwt_id);

int rc_db_connect_device(rc_db_pool_t *pool, const char *user_id,
                         const char *session_id, const char *device_type,
                         const char *platform, rc_connect_device_t *result);

int rc_db_disconnect_device(rc_db_pool_t *pool, const char *connection_id);

int rc_db_get_paired_desktop(rc_db_pool_t *pool, const char *user_id,
                             rc_paired_device_t *result);

int rc_db_get_paired_mobile(rc_db_pool_t *pool, const char *user_id,
                            rc_paired_device_t *result);

int rc_db_admin_stats(rc_db_pool_t *pool, rc_admin_stats_t *stats);

#endif /* RC_DATABASE_H */
