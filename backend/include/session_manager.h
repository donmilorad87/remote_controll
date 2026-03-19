#ifndef RC_SESSION_MANAGER_H
#define RC_SESSION_MANAGER_H

#include "socket_server.h"

/* Find a client by user_id and device_type.
   Returns pointer to client or NULL if not found. */
rc_socket_client_t *rc_session_find_client(rc_socket_server_t *server,
                                           const char *user_id,
                                           const char *device_type);

/* Find a client by session_id.
   Returns pointer to client or NULL if not found. */
rc_socket_client_t *rc_session_find_by_session(rc_socket_server_t *server,
                                               const char *session_id);

/* Find the paired device for a given client.
   If client is mobile, find desktop. If desktop, find mobile.
   Returns pointer to paired client or NULL. */
rc_socket_client_t *rc_session_find_pair(rc_socket_server_t *server,
                                         rc_socket_client_t *client);

/* Remove a client from the active client list and clean up. */
void rc_session_remove_client(rc_socket_server_t *server,
                              rc_socket_client_t *client);

#endif /* RC_SESSION_MANAGER_H */
