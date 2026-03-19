#include "session_manager.h"
#include "utils.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

rc_socket_client_t *rc_session_find_client(rc_socket_server_t *server,
                                           const char *user_id,
                                           const char *device_type)
{
    assert(server != NULL);
    assert(user_id != NULL);
    assert(device_type != NULL);

    for (int i = 0; i < RC_MAX_CLIENTS; i++) {
        rc_socket_client_t *c = &server->clients[i];
        if (c->in_use && c->authenticated &&
            strcmp(c->user_id, user_id) == 0 &&
            strcmp(c->device_type, device_type) == 0) {
            return c;
        }
    }
    return NULL;
}

rc_socket_client_t *rc_session_find_by_session(rc_socket_server_t *server,
                                               const char *session_id)
{
    assert(server != NULL);
    assert(session_id != NULL);

    for (int i = 0; i < RC_MAX_CLIENTS; i++) {
        rc_socket_client_t *c = &server->clients[i];
        if (c->in_use && strcmp(c->session_id, session_id) == 0) {
            return c;
        }
    }
    return NULL;
}

rc_socket_client_t *rc_session_find_pair(rc_socket_server_t *server,
                                         rc_socket_client_t *client)
{
    assert(server != NULL);
    assert(client != NULL);

    const char *pair_type;
    if (strcmp(client->device_type, "mobile") == 0) {
        pair_type = "desktop";
    } else if (strcmp(client->device_type, "desktop") == 0) {
        pair_type = "mobile";
    } else {
        return NULL;
    }

    return rc_session_find_client(server, client->user_id, pair_type);
}

void rc_session_remove_client(rc_socket_server_t *server,
                              rc_socket_client_t *client)
{
    assert(server != NULL);
    assert(client != NULL);

    if (client->connection_id[0] != '\0') {
        rc_db_disconnect_device(server->db_pool, client->connection_id);
    }

    if (client->ssl != NULL) {
        SSL_shutdown(client->ssl);
        SSL_free(client->ssl);
        client->ssl = NULL;
    }

    if (client->bio != NULL) {
        /* BIO is freed with SSL_free if attached */
        client->bio = NULL;
    }

    fprintf(stdout, "[session] Client disconnected: user=%s type=%s\n",
            client->user_id, client->device_type);

    memset(client, 0, sizeof(*client));
}
