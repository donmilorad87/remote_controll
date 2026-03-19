/* Shared client networking header for all desktop apps. */

#ifndef RC_CLIENT_H
#define RC_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#define RC_MAX_TOKEN_LEN    2048
#define RC_MAX_HOST_LEN     256

typedef struct {
    char        server_host[RC_MAX_HOST_LEN];
    uint16_t    server_api_port;
    uint16_t    server_socket_port;
    char        api_base_url[512];
} rc_client_config_t;

typedef struct {
    char    token[RC_MAX_TOKEN_LEN];
    char    user_id[64];
    char    session_id[64];
    bool    authenticated;
} rc_client_auth_t;

typedef struct {
    int                 udp_fd;
    rc_client_config_t  config;
    rc_client_auth_t    auth;
    volatile bool       connected;
    volatile bool       running;
#ifdef _WIN32
    HANDLE              recv_thread;
    HANDLE              heartbeat_thread;
#else
    pthread_t           recv_thread;
    pthread_t           heartbeat_thread;
#endif
} rc_client_t;

/* Load client config from .env file. Returns 0 on success. */
int rc_client_load_config(rc_client_config_t *cfg, const char *env_path);

/* HTTP login. Sends POST to /api/login and stores JWT. Returns 0 on success. */
int rc_client_login(rc_client_t *client, const char *email,
                    const char *password);

/* HTTP register. Sends POST to /api/register. Returns 0 on success. */
int rc_client_register(rc_client_t *client, const char *email,
                       const char *password);

/* Connect to the UDP socket server. Returns 0 on success. */
int rc_client_connect(rc_client_t *client);

/* Disconnect from the socket server. */
void rc_client_disconnect(rc_client_t *client);

/* HTTP logout. */
int rc_client_logout(rc_client_t *client);

#endif /* RC_CLIENT_H */
