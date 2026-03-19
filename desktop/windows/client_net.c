#include "rc_client.h"
#include "rc_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ws2tcpip.h>
#include <process.h>
#include <curl/curl.h>
#include <jansson.h>

/* Event handler callback — set by main.c */
extern void rc_handle_event(const rc_event_t *event);

static BOOL g_wsa_initialized = FALSE;

static int ensure_wsa(void)
{
    if (!g_wsa_initialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            fprintf(stderr, "[net] WSAStartup failed: %d\n", WSAGetLastError());
            return -1;
        }
        g_wsa_initialized = TRUE;
    }
    return 0;
}

/* ─── .env file parser ───────────────────────────────────────────── */

int rc_client_load_config(rc_client_config_t *cfg, const char *env_path)
{
    assert(cfg != NULL);
    assert(env_path != NULL);

    memset(cfg, 0, sizeof(*cfg));

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
        if (line[0] == '#' || line[0] == '\n') continue;

        char key[128], value[384];
        if (sscanf(line, "%127[^=]=%383[^\n]", key, value) != 2) continue;

        if (strcmp(key, "RC_SERVER_HOST") == 0) {
            snprintf(cfg->server_host, sizeof(cfg->server_host), "%s", value);
        } else if (strcmp(key, "RC_SERVER_API_PORT") == 0) {
            cfg->server_api_port = (uint16_t)atoi(value);
        } else if (strcmp(key, "RC_SERVER_SOCKET_PORT") == 0) {
            cfg->server_socket_port = (uint16_t)atoi(value);
        } else if (strcmp(key, "RC_SERVER_SCHEME") == 0) {
            snprintf(scheme, sizeof(scheme), "%s", value);
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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

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

/* ─── Winsock UDP receive thread ─────────────────────────────────── */

static unsigned __stdcall udp_recv_thread(void *arg)
{
    rc_client_t *client = (rc_client_t *)arg;
    assert(client != NULL);

    uint8_t buf[RC_MAX_EVENT_SIZE];

    while (client->running) {
        int n = recv((SOCKET)client->udp_fd, (char *)buf, sizeof(buf), 0);
        if (n <= 0) {
            int err = WSAGetLastError();
            if (!client->running) break;
            if (err == WSAEINTR) continue;
            fprintf(stderr, "[net] recv error: %d\n", err);
            continue;
        }

        rc_event_t event;
        if (rc_event_deserialize(buf, (size_t)n, &event) == 0) {
            rc_handle_event(&event);
        }
    }

    return 0;
}

/* ─── Heartbeat thread ───────────────────────────────────────────── */

static unsigned __stdcall heartbeat_thread(void *arg)
{
    rc_client_t *client = (rc_client_t *)arg;
    assert(client != NULL);

    uint8_t buf[4];

    while (client->running) {
        Sleep(8000);
        if (!client->running) break;

        int len = rc_event_serialize_heartbeat(buf, sizeof(buf));
        if (len > 0) {
            send((SOCKET)client->udp_fd, (const char *)buf, len, 0);
        }
    }

    return 0;
}

/* ─── Connect to socket server ───────────────────────────────────── */

int rc_client_connect(rc_client_t *client)
{
    assert(client != NULL);
    assert(client->auth.authenticated);

    if (ensure_wsa() != 0) return -1;

    /* Resolve server address */
    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", client->config.server_socket_port);

    if (getaddrinfo(client->config.server_host, port_str, &hints, &result) != 0) {
        fprintf(stderr, "[net] Cannot resolve %s\n", client->config.server_host);
        return -1;
    }

    /* Create UDP socket */
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "[net] Socket creation failed: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        return -1;
    }

    /* Connect UDP socket */
    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "[net] UDP connect failed: %d\n", WSAGetLastError());
        closesocket(sock);
        freeaddrinfo(result);
        return -1;
    }
    freeaddrinfo(result);

    client->udp_fd = (int)sock;

    /* Send AUTH packet */
    uint8_t auth_buf[RC_MAX_EVENT_SIZE];
    int auth_len = rc_event_serialize_auth(client->auth.token, auth_buf,
                                           sizeof(auth_buf));
    if (auth_len <= 0) {
        closesocket(sock);
        return -1;
    }

    if (send(sock, (const char *)auth_buf, auth_len, 0) == SOCKET_ERROR) {
        fprintf(stderr, "[net] Auth send failed: %d\n", WSAGetLastError());
        closesocket(sock);
        return -1;
    }

    client->connected = true;
    client->running = true;

    /* Start threads using _beginthreadex (Windows) */
    client->recv_thread = (HANDLE)_beginthreadex(NULL, 0, udp_recv_thread, client, 0, NULL);
    client->heartbeat_thread = (HANDLE)_beginthreadex(NULL, 0, heartbeat_thread, client, 0, NULL);

    fprintf(stdout, "[net] Connected to socket server\n");
    return 0;
}

void rc_client_disconnect(rc_client_t *client)
{
    assert(client != NULL);

    client->running = false;
    client->connected = false;

    if (client->udp_fd >= 0) {
        closesocket((SOCKET)client->udp_fd);
        client->udp_fd = -1;
    }

    if (client->recv_thread != NULL) {
        WaitForSingleObject(client->recv_thread, 5000);
        CloseHandle(client->recv_thread);
        client->recv_thread = NULL;
    }
    if (client->heartbeat_thread != NULL) {
        WaitForSingleObject(client->heartbeat_thread, 5000);
        CloseHandle(client->heartbeat_thread);
        client->heartbeat_thread = NULL;
    }
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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    memset(&client->auth, 0, sizeof(client->auth));
    return 0;
}
