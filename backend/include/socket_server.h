#ifndef RC_SOCKET_SERVER_H
#define RC_SOCKET_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <netinet/in.h>
#include "config.h"
#include "database.h"
#include "protocol.h"

#define RC_MAX_CLIENTS          256
#define RC_HEARTBEAT_INTERVAL   10
#define RC_CLIENT_TIMEOUT       30

typedef struct {
    bool                in_use;
    char                user_id[64];
    char                session_id[64];
    char                connection_id[64];
    char                device_type[24];
    char                platform[24];
    struct sockaddr_in  addr;
    SSL                 *ssl;
    BIO                 *bio;
    time_t              last_heartbeat;
    bool                authenticated;
} rc_socket_client_t;

typedef struct {
    int                 udp_fd;
    SSL_CTX             *ssl_ctx;
    rc_socket_client_t  clients[RC_MAX_CLIENTS];
    pthread_mutex_t     clients_mutex;
    rc_config_t         *config;
    rc_db_pool_t        *db_pool;
    volatile bool       running;
    pthread_t           recv_thread;
    pthread_t           heartbeat_thread;
} rc_socket_server_t;

/* Start the UDP/DTLS socket server. Returns 0 on success. */
int rc_socket_start(rc_socket_server_t *server, rc_config_t *cfg,
                    rc_db_pool_t *pool);

/* Stop the socket server. */
void rc_socket_stop(rc_socket_server_t *server);

/* Route an event from a source client to its paired device. */
int rc_socket_route_event(rc_socket_server_t *server,
                          rc_socket_client_t *source,
                          const rc_event_t *event);

/* Notify a client that it was kicked (another desktop logged in). */
void rc_socket_kick_client(rc_socket_server_t *server,
                           const char *session_id);

#endif /* RC_SOCKET_SERVER_H */
