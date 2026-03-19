#include "config.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int rc_config_load(rc_config_t *cfg)
{
    assert(cfg != NULL);
    memset(cfg, 0, sizeof(*cfg));

    int ret = 0;

    /* HTTP API port */
    ret |= rc_env_get_uint16("RC_API_PORT", &cfg->api_port, 7481);

    /* Socket port */
    ret |= rc_env_get_uint16("RC_SOCKET_PORT", &cfg->socket_port, 7482);

    /* Database */
    ret |= rc_env_get("RC_DB_HOST", cfg->db_host, sizeof(cfg->db_host), "localhost");
    ret |= rc_env_get_uint16("RC_DB_PORT", &cfg->db_port, 7484);
    ret |= rc_env_get("RC_DB_NAME", cfg->db_name, sizeof(cfg->db_name), "remotecontrol");
    ret |= rc_env_get("RC_DB_USER", cfg->db_user, sizeof(cfg->db_user), "rcadmin");
    ret |= rc_env_get("RC_DB_PASSWORD", cfg->db_password, sizeof(cfg->db_password), "");

    /* SMTP */
    ret |= rc_env_get("MAIL_HOST", cfg->smtp_host, sizeof(cfg->smtp_host), "localhost");
    ret |= rc_env_get_uint16("MAIL_PORT", &cfg->smtp_port, 2525);
    ret |= rc_env_get("MAIL_USERNAME", cfg->smtp_username, sizeof(cfg->smtp_username), "");
    ret |= rc_env_get("MAIL_PASSWORD", cfg->smtp_password, sizeof(cfg->smtp_password), "");
    ret |= rc_env_get("MAIL_FROM_EMAIL", cfg->smtp_from, sizeof(cfg->smtp_from),
                       "noreply@remotecontrol.rs");
    ret |= rc_env_get("MAIL_FROM_NAME", cfg->smtp_from_name, sizeof(cfg->smtp_from_name),
                       "RemoteControl");

    /* JWT */
    ret |= rc_env_get("JWT_SECRET", cfg->jwt_secret, sizeof(cfg->jwt_secret), "");
    ret |= rc_env_get_uint32("JWT_EXPIRY_SECONDS", &cfg->jwt_expiry_seconds, 86400);

    /* DTLS */
    ret |= rc_env_get("DTLS_CERT_PATH", cfg->dtls_cert_path, sizeof(cfg->dtls_cert_path),
                       "/etc/ssl/certs/dtls-cert.pem");
    ret |= rc_env_get("DTLS_KEY_PATH", cfg->dtls_key_path, sizeof(cfg->dtls_key_path),
                       "/etc/ssl/private/dtls-key.pem");

    /* Domain */
    ret |= rc_env_get("RC_DOMAIN", cfg->domain, sizeof(cfg->domain), "local.remotecontrol.rs");
    ret |= rc_env_get("RC_SCHEME", cfg->scheme, sizeof(cfg->scheme), "http");
    ret |= rc_env_get_uint16("RC_NGINX_PORT", &cfg->nginx_port, 7480);

    /* Migrations */
    ret |= rc_env_get("RC_MIGRATIONS_PATH", cfg->migrations_path,
                       sizeof(cfg->migrations_path), "/app/migrations");

    /* Validate required fields */
    if (cfg->jwt_secret[0] == '\0') {
        fprintf(stderr, "[config] JWT_SECRET is required\n");
        return -1;
    }

    if (cfg->db_password[0] == '\0') {
        fprintf(stderr, "[config] RC_DB_PASSWORD is required\n");
        return -1;
    }

    return ret;
}

int rc_config_build_connstr(const rc_config_t *cfg, char *out, size_t out_size)
{
    assert(cfg != NULL);
    assert(out != NULL);

    int written = snprintf(out, out_size,
        "host=%s port=%u dbname=%s user=%s password=%s",
        cfg->db_host, cfg->db_port, cfg->db_name,
        cfg->db_user, cfg->db_password);

    if (written < 0 || (size_t)written >= out_size) {
        return -1;
    }
    return 0;
}
