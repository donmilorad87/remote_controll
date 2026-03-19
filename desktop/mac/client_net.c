#include "rc_client.h"
#include "rc_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <curl/curl.h>
#include <jansson.h>

/* Event handler callback — set by main.c */
extern void rc_handle_event(const rc_event_t *event);

/* ─── .env file parser ───────────────────────────────────────────── */

int rc_client_load_config(rc_client_config_t *cfg, const char *env_path)
{
    assert(cfg != NULL);
    assert(env_path != NULL);

    memset(cfg, 0, sizeof(*cfg));

    /* Defaults */
    strcpy(cfg->server_host, "local.remotecontrol.rs");
    cfg->server_api_port = 7480;
    cfg->server_socket_port = 7482;

    FILE *f = fopen(env_path, "r");
    if (f == NULL) {
        fprintf(stderr, "[config] Cannot open %s, using defaults\n", env_path);
        snprintf(cfg->api_base_url, sizeof(cfg->api_base_url),
                 "http://%s:%u", cfg->server_host, cfg->server_api_port);
        return 0;
    }

    char scheme[8] = "https";
    char line[512];
    while (fgets(line, sizeof(line), f) != NULL) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') continue;

        char key[128], value[384];
        if (sscanf(line, "%127[^=]=%383[^\n]", key, value) != 2) continue;

        if (strcmp(key, "RC_SERVER_HOST") == 0) {
            strncpy(cfg->server_host, value, sizeof(cfg->server_host) - 1);
        } else if (strcmp(key, "RC_SERVER_API_PORT") == 0) {
            cfg->server_api_port = (uint16_t)atoi(value);
        } else if (strcmp(key, "RC_SERVER_SOCKET_PORT") == 0) {
            cfg->server_socket_port = (uint16_t)atoi(value);
        } else if (strcmp(key, "RC_SERVER_SCHEME") == 0) {
            strncpy(scheme, value, sizeof(scheme) - 1);
        }
    }
    fclose(f);

    snprintf(cfg->api_base_url, sizeof(cfg->api_base_url),
             "%s://%s:%u", scheme, cfg->server_host, cfg->server_api_port);

    fprintf(stdout, "[config] Server: %s, Socket port: %u\n",
            cfg->api_base_url, cfg->server_socket_port);
    return 0;
}

/* ─── CURL response buffer ───────────────────────────────────────── */

typedef struct {
    char   *data;
    size_t  len;
    size_t  cap;
} curl_buf_t;

static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    curl_buf_t *buf = (curl_buf_t *)userdata;
    size_t bytes = size * nmemb;

    assert(buf != NULL);

    if (buf->len + bytes + 1 > buf->cap) {
        size_t new_cap = buf->cap + bytes + 1024;
        char *new_data = realloc(buf->data, new_cap);
        if (new_data == NULL) return 0;
        buf->data = new_data;
        buf->cap = new_cap;
    }

    memcpy(buf->data + buf->len, ptr, bytes);
    buf->len += bytes;
    buf->data[buf->len] = '\0';
    return bytes;
}

/* ─── HTTP login ─────────────────────────────────────────────────── */

int rc_client_login(rc_client_t *client, const char *email, const char *password)
{
    assert(client != NULL);
    assert(email != NULL);
    assert(password != NULL);

    CURL *curl = curl_easy_init();
    if (curl == NULL) return -1;

    char url[512];
    snprintf(url, sizeof(url), "%s/api/login", client->config.api_base_url);

    /* Build JSON body */
    json_t *body = json_object();
    json_object_set_new(body, "email", json_string(email));
    json_object_set_new(body, "password", json_string(password));
    json_object_set_new(body, "device_type", json_string("desktop"));

    char *body_str = json_dumps(body, JSON_COMPACT);
    json_decref(body);

    curl_buf_t resp_buf = { .data = malloc(1024), .len = 0, .cap = 1024 };
    if (resp_buf.data == NULL) {
        free(body_str);
        curl_easy_cleanup(curl);
        return -1;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    free(body_str);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code != 200) {
        fprintf(stderr, "[auth] Login failed: HTTP %ld\n", http_code);
        if (resp_buf.data != NULL) {
            fprintf(stderr, "[auth] Response: %s\n", resp_buf.data);
        }
        free(resp_buf.data);
        return -1;
    }

    /* Parse response */
    json_error_t err;
    json_t *resp = json_loads(resp_buf.data, 0, &err);
    free(resp_buf.data);

    if (resp == NULL) return -1;

    const char *token = json_string_value(json_object_get(resp, "token"));
    const char *user_id = json_string_value(json_object_get(resp, "user_id"));
    const char *session_id = json_string_value(json_object_get(resp, "session_id"));

    if (token == NULL || user_id == NULL) {
        json_decref(resp);
        return -1;
    }

    strncpy(client->auth.token, token, sizeof(client->auth.token) - 1);
    strncpy(client->auth.user_id, user_id, sizeof(client->auth.user_id) - 1);
    if (session_id != NULL) {
        strncpy(client->auth.session_id, session_id,
                sizeof(client->auth.session_id) - 1);
    }
    client->auth.authenticated = true;

    json_decref(resp);
    fprintf(stdout, "[auth] Logged in as %s\n", email);
    return 0;
}

/* ─── HTTP register ──────────────────────────────────────────────── */

int rc_client_register(rc_client_t *client, const char *email, const char *password)
{
    assert(client != NULL);
    assert(email != NULL);
    assert(password != NULL);

    CURL *curl = curl_easy_init();
    if (curl == NULL) return -1;

    char url[512];
    snprintf(url, sizeof(url), "%s/api/register", client->config.api_base_url);

    json_t *body = json_object();
    json_object_set_new(body, "email", json_string(email));
    json_object_set_new(body, "password", json_string(password));
    char *body_str = json_dumps(body, JSON_COMPACT);
    json_decref(body);

    curl_buf_t resp_buf = { .data = malloc(1024), .len = 0, .cap = 1024 };

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    free(body_str);
    curl_easy_cleanup(curl);

    int ret = (res == CURLE_OK && (http_code == 201 || http_code == 200)) ? 0 : -1;

    if (ret != 0) {
        fprintf(stderr, "[auth] Registration failed: HTTP %ld\n", http_code);
        if (resp_buf.data != NULL) {
            fprintf(stderr, "[auth] Response: %s\n", resp_buf.data);
        }
    } else {
        fprintf(stdout, "[auth] Registered! Check email for activation link.\n");
    }

    free(resp_buf.data);
    return ret;
}

/* ─── UDP receive thread ─────────────────────────────────────────── */

static void *udp_recv_thread(void *arg)
{
    rc_client_t *client = (rc_client_t *)arg;
    assert(client != NULL);

    uint8_t buf[RC_MAX_EVENT_SIZE];

    while (client->running) {
        ssize_t n = recv(client->udp_fd, buf, sizeof(buf), 0);
        if (n <= 0) {
            if (errno == EINTR) continue;
            if (!client->running) break;
            continue;
        }

        rc_event_t event;
        if (rc_event_deserialize(buf, (size_t)n, &event) == 0) {
            rc_handle_event(&event);
        }
    }

    return NULL;
}

/* ─── Heartbeat thread ───────────────────────────────────────────── */

static void *heartbeat_thread(void *arg)
{
    rc_client_t *client = (rc_client_t *)arg;
    assert(client != NULL);

    uint8_t buf[4];

    while (client->running) {
        sleep(8);
        if (!client->running) break;

        int len = rc_event_serialize_heartbeat(buf, sizeof(buf));
        if (len > 0) {
            send(client->udp_fd, buf, (size_t)len, 0);
        }
    }

    return NULL;
}

/* ─── Connect to socket server ───────────────────────────────────── */

int rc_client_connect(rc_client_t *client)
{
    assert(client != NULL);
    assert(client->auth.authenticated);

    /* Resolve server address */
    struct hostent *he = gethostbyname(client->config.server_host);
    if (he == NULL) {
        fprintf(stderr, "[net] Cannot resolve %s\n", client->config.server_host);
        return -1;
    }

    /* Create UDP socket */
    client->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->udp_fd < 0) {
        fprintf(stderr, "[net] Socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    server_addr.sin_port = htons(client->config.server_socket_port);

    /* Connect UDP socket (so we can use send/recv instead of sendto/recvfrom) */
    if (connect(client->udp_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        fprintf(stderr, "[net] UDP connect failed: %s\n", strerror(errno));
        close(client->udp_fd);
        return -1;
    }

    /* Send AUTH packet */
    uint8_t auth_buf[RC_MAX_EVENT_SIZE];
    int auth_len = rc_event_serialize_auth(client->auth.token, auth_buf,
                                           sizeof(auth_buf));
    if (auth_len <= 0) {
        close(client->udp_fd);
        return -1;
    }

    if (send(client->udp_fd, auth_buf, (size_t)auth_len, 0) < 0) {
        fprintf(stderr, "[net] Auth send failed: %s\n", strerror(errno));
        close(client->udp_fd);
        return -1;
    }

    client->connected = true;
    client->running = true;

    /* Start receive thread */
    pthread_create(&client->recv_thread, NULL, udp_recv_thread, client);
    pthread_create(&client->heartbeat_thread, NULL, heartbeat_thread, client);

    fprintf(stdout, "[net] Connected to socket server\n");
    return 0;
}

void rc_client_disconnect(rc_client_t *client)
{
    assert(client != NULL);

    client->running = false;
    client->connected = false;

    if (client->udp_fd >= 0) {
        close(client->udp_fd);
        client->udp_fd = -1;
    }

    pthread_join(client->recv_thread, NULL);
    pthread_join(client->heartbeat_thread, NULL);
}

int rc_client_logout(rc_client_t *client)
{
    assert(client != NULL);

    if (!client->auth.authenticated) return 0;

    CURL *curl = curl_easy_init();
    if (curl == NULL) return -1;

    char url[512];
    snprintf(url, sizeof(url), "%s/api/logout", client->config.api_base_url);

    char auth_header[RC_MAX_TOKEN_LEN + 16];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
             client->auth.token);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    memset(&client->auth, 0, sizeof(client->auth));
    return 0;
}
