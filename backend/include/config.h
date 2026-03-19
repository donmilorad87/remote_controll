#ifndef RC_CONFIG_H
#define RC_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RC_MAX_PATH         512
#define RC_MAX_URL          256
#define RC_MAX_EMAIL        256
#define RC_MAX_PASSWORD     128
#define RC_MAX_TOKEN        256
#define RC_MAX_JWT_SECRET   256
#define RC_MAX_DB_CONNSTR   512
#define RC_MAX_HOSTNAME     128

typedef struct {
    /* HTTP API */
    uint16_t    api_port;

    /* UDP/DTLS Socket */
    uint16_t    socket_port;

    /* Database (via PgBouncer) */
    char        db_host[RC_MAX_HOSTNAME];
    uint16_t    db_port;
    char        db_name[64];
    char        db_user[64];
    char        db_password[128];

    /* SMTP */
    char        smtp_host[RC_MAX_HOSTNAME];
    uint16_t    smtp_port;
    char        smtp_username[128];
    char        smtp_password[128];
    char        smtp_from[RC_MAX_EMAIL];
    char        smtp_from_name[64];

    /* JWT */
    char        jwt_secret[RC_MAX_JWT_SECRET];
    uint32_t    jwt_expiry_seconds;

    /* DTLS */
    char        dtls_cert_path[RC_MAX_PATH];
    char        dtls_key_path[RC_MAX_PATH];

    /* Domain */
    char        domain[RC_MAX_HOSTNAME];
    char        scheme[8];
    uint16_t    nginx_port;

    /* Migrations */
    char        migrations_path[RC_MAX_PATH];
} rc_config_t;

/* Load configuration from environment variables. Returns 0 on success. */
int rc_config_load(rc_config_t *cfg);

/* Build a PostgreSQL connection string from config. */
int rc_config_build_connstr(const rc_config_t *cfg, char *out, size_t out_size);

#endif /* RC_CONFIG_H */
