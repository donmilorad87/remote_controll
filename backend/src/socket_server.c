#include "socket_server.h"
#include "session_manager.h"
#include "jwt.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/err.h>

/* Find or allocate a client slot */
static rc_socket_client_t *find_or_alloc_client(rc_socket_server_t *server,
                                                 struct sockaddr_in *addr)
{
    assert(server != NULL);
    assert(addr != NULL);

    /* Check for existing client by address */
    for (int i = 0; i < RC_MAX_CLIENTS; i++) {
        rc_socket_client_t *c = &server->clients[i];
        if (c->in_use &&
            c->addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            c->addr.sin_port == addr->sin_port) {
            return c;
        }
    }

    /* Allocate new slot */
    for (int i = 0; i < RC_MAX_CLIENTS; i++) {
        if (!server->clients[i].in_use) {
            memset(&server->clients[i], 0, sizeof(rc_socket_client_t));
            server->clients[i].in_use = true;
            server->clients[i].addr = *addr;
            server->clients[i].last_heartbeat = time(NULL);
            return &server->clients[i];
        }
    }

    return NULL; /* No slots available */
}

/* Handle AUTH event from a client */
static void handle_auth(rc_socket_server_t *server, rc_socket_client_t *client,
                        const rc_event_t *event)
{
    assert(server != NULL);
    assert(client != NULL);
    assert(event != NULL);

    rc_jwt_claims_t claims;
    if (rc_jwt_verify(server->config->jwt_secret,
                      event->data.auth.token, &claims) != 0) {
        fprintf(stderr, "[socket] Auth failed: invalid JWT\n");
        rc_session_remove_client(server, client);
        return;
    }

    /* Validate session in DB */
    rc_session_validate_t session;
    if (rc_db_validate_session(server->db_pool, claims.jwt_id, &session) != 0 ||
        session.error[0] != '\0') {
        fprintf(stderr, "[socket] Auth failed: invalid session\n");
        rc_session_remove_client(server, client);
        return;
    }

    /* If this is a desktop, kick any existing desktop for this user */
    if (strcmp(claims.device_type, "desktop") == 0) {
        rc_socket_client_t *existing = rc_session_find_client(
            server, claims.user_id, "desktop");
        if (existing != NULL && existing != client) {
            fprintf(stdout, "[socket] Kicking existing desktop for user %s\n",
                    claims.user_id);
            rc_session_remove_client(server, existing);
        }
    }

    /* Set client info */
    rc_strlcpy(client->user_id, claims.user_id, sizeof(client->user_id));
    rc_strlcpy(client->session_id, session.session_id, sizeof(client->session_id));
    rc_strlcpy(client->device_type, claims.device_type, sizeof(client->device_type));
    client->authenticated = true;

    /* Determine platform from device_type (simplified — real impl would get from client) */
    const char *platform = "unknown";
    if (strcmp(claims.device_type, "desktop") == 0) {
        platform = "linux"; /* Default; client should send platform info */
    } else {
        platform = "android";
    }
    rc_strlcpy(client->platform, platform, sizeof(client->platform));

    /* Register device connection in DB */
    rc_connect_device_t conn_result;
    if (rc_db_connect_device(server->db_pool, claims.user_id,
                             session.session_id, claims.device_type,
                             platform, &conn_result) == 0) {
        rc_strlcpy(client->connection_id, conn_result.connection_id,
                   sizeof(client->connection_id));
    }

    fprintf(stdout, "[socket] Client authenticated: user=%s type=%s\n",
            claims.user_id, claims.device_type);
}

/* Route event to the paired device */
int rc_socket_route_event(rc_socket_server_t *server,
                          rc_socket_client_t *source,
                          const rc_event_t *event)
{
    assert(server != NULL);
    assert(source != NULL);
    assert(event != NULL);

    rc_socket_client_t *target = rc_session_find_pair(server, source);
    if (target == NULL) {
        return -1; /* No paired device */
    }

    /* Serialize the event */
    uint8_t buf[RC_MAX_EVENT_SIZE];
    int len = rc_protocol_serialize(event, buf, sizeof(buf));
    if (len <= 0) {
        return -1;
    }

    /* Send via UDP to the target's address */
    ssize_t sent = sendto(server->udp_fd, buf, (size_t)len, 0,
                          (struct sockaddr *)&target->addr,
                          sizeof(target->addr));
    if (sent < 0) {
        fprintf(stderr, "[socket] Send failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

void rc_socket_kick_client(rc_socket_server_t *server, const char *session_id)
{
    assert(server != NULL);
    assert(session_id != NULL);

    pthread_mutex_lock(&server->clients_mutex);
    rc_socket_client_t *client = rc_session_find_by_session(server, session_id);
    if (client != NULL) {
        rc_session_remove_client(server, client);
    }
    pthread_mutex_unlock(&server->clients_mutex);
}

/* Receive thread: reads UDP packets and dispatches */
static void *recv_thread_func(void *arg)
{
    rc_socket_server_t *server = (rc_socket_server_t *)arg;

    assert(server != NULL);

    uint8_t buf[RC_MAX_EVENT_SIZE];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    while (server->running) {
        ssize_t n = recvfrom(server->udp_fd, buf, sizeof(buf), 0,
                             (struct sockaddr *)&sender_addr, &addr_len);

        if (n <= 0) {
            if (errno == EINTR) continue;
            if (!server->running) break;
            fprintf(stderr, "[socket] recvfrom error: %s\n", strerror(errno));
            continue;
        }

        pthread_mutex_lock(&server->clients_mutex);

        /* Find or create client */
        rc_socket_client_t *client = find_or_alloc_client(server, &sender_addr);
        if (client == NULL) {
            pthread_mutex_unlock(&server->clients_mutex);
            fprintf(stderr, "[socket] Max clients reached, dropping packet\n");
            continue;
        }

        /* Deserialize event */
        rc_event_t event;
        if (rc_protocol_deserialize(buf, (size_t)n, &event) != 0) {
            pthread_mutex_unlock(&server->clients_mutex);
            continue;
        }

        /* Handle by event type */
        switch (event.type) {
        case RC_EVT_AUTH:
            handle_auth(server, client, &event);
            break;

        case RC_EVT_HEARTBEAT:
            client->last_heartbeat = time(NULL);
            break;

        default:
            if (!client->authenticated) {
                /* Ignore events from unauthenticated clients */
                pthread_mutex_unlock(&server->clients_mutex);
                continue;
            }
            client->last_heartbeat = time(NULL);
            rc_socket_route_event(server, client, &event);
            break;
        }

        pthread_mutex_unlock(&server->clients_mutex);
    }

    return NULL;
}

/* Heartbeat thread: checks for timed-out clients */
static void *heartbeat_thread_func(void *arg)
{
    rc_socket_server_t *server = (rc_socket_server_t *)arg;

    assert(server != NULL);

    while (server->running) {
        sleep(RC_HEARTBEAT_INTERVAL);

        if (!server->running) break;

        time_t now = time(NULL);

        pthread_mutex_lock(&server->clients_mutex);
        for (int i = 0; i < RC_MAX_CLIENTS; i++) {
            rc_socket_client_t *c = &server->clients[i];
            if (c->in_use && (now - c->last_heartbeat) > RC_CLIENT_TIMEOUT) {
                fprintf(stdout, "[socket] Client timed out: user=%s\n", c->user_id);
                rc_session_remove_client(server, c);
            }
        }
        pthread_mutex_unlock(&server->clients_mutex);
    }

    return NULL;
}

int rc_socket_start(rc_socket_server_t *server, rc_config_t *cfg,
                    rc_db_pool_t *pool)
{
    assert(server != NULL);
    assert(cfg != NULL);
    assert(pool != NULL);

    memset(server, 0, sizeof(*server));
    server->config = cfg;
    server->db_pool = pool;
    server->running = true;
    pthread_mutex_init(&server->clients_mutex, NULL);

    /* Create UDP socket */
    server->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server->udp_fd < 0) {
        fprintf(stderr, "[socket] Failed to create UDP socket: %s\n",
                strerror(errno));
        return -1;
    }

    /* Allow address reuse */
    int optval = 1;
    setsockopt(server->udp_fd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    /* Bind */
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(cfg->socket_port);

    if (bind(server->udp_fd, (struct sockaddr *)&bind_addr,
             sizeof(bind_addr)) < 0) {
        fprintf(stderr, "[socket] Failed to bind UDP port %u: %s\n",
                cfg->socket_port, strerror(errno));
        close(server->udp_fd);
        return -1;
    }

    /* Note: DTLS would be initialized here with SSL_CTX_new(DTLS_server_method())
       For initial development, we use plain UDP. DTLS can be layered on top. */

    /* Start threads */
    if (pthread_create(&server->recv_thread, NULL, recv_thread_func, server) != 0) {
        fprintf(stderr, "[socket] Failed to start recv thread\n");
        close(server->udp_fd);
        return -1;
    }

    if (pthread_create(&server->heartbeat_thread, NULL,
                       heartbeat_thread_func, server) != 0) {
        fprintf(stderr, "[socket] Failed to start heartbeat thread\n");
        server->running = false;
        close(server->udp_fd);
        return -1;
    }

    fprintf(stdout, "[socket] UDP server listening on port %u\n", cfg->socket_port);
    return 0;
}

void rc_socket_stop(rc_socket_server_t *server)
{
    assert(server != NULL);

    server->running = false;

    /* Close socket to unblock recvfrom */
    if (server->udp_fd >= 0) {
        close(server->udp_fd);
        server->udp_fd = -1;
    }

    pthread_join(server->recv_thread, NULL);
    pthread_join(server->heartbeat_thread, NULL);

    /* Clean up all clients */
    pthread_mutex_lock(&server->clients_mutex);
    for (int i = 0; i < RC_MAX_CLIENTS; i++) {
        if (server->clients[i].in_use) {
            rc_session_remove_client(server, &server->clients[i]);
        }
    }
    pthread_mutex_unlock(&server->clients_mutex);
    pthread_mutex_destroy(&server->clients_mutex);

    fprintf(stdout, "[socket] Server stopped\n");
}
