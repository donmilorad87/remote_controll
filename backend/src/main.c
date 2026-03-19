#include "config.h"
#include "database.h"
#include "http_server.h"
#include "socket_server.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <curl/curl.h>

static volatile sig_atomic_t g_running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(void)
{
    /* Disable stdout/stderr buffering so docker logs work in real-time */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    fprintf(stdout, "=== RemoteControl Server ===\n");

    /* ─── Initialize libraries ────────────────────────────────────── */
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    curl_global_init(CURL_GLOBAL_ALL);

    /* ─── Load configuration ──────────────────────────────────────── */
    rc_config_t config;
    if (rc_config_load(&config) != 0) {
        fprintf(stderr, "[main] Failed to load configuration\n");
        return EXIT_FAILURE;
    }
    fprintf(stdout, "[main] Configuration loaded\n");

    /* ─── Initialize database pool ────────────────────────────────── */
    char connstr[512];
    if (rc_config_build_connstr(&config, connstr, sizeof(connstr)) != 0) {
        fprintf(stderr, "[main] Failed to build connection string\n");
        return EXIT_FAILURE;
    }

    rc_db_pool_t db_pool;
    if (rc_db_init(&db_pool, connstr) != 0) {
        fprintf(stderr, "[main] Failed to initialize database pool\n");
        return EXIT_FAILURE;
    }

    /* ─── Run migrations ──────────────────────────────────────────── */
    if (rc_db_run_migrations(&db_pool, config.migrations_path) != 0) {
        fprintf(stderr, "[main] Warning: migration errors occurred\n");
        /* Continue anyway — some migrations may have already been applied */
    }

    /* ─── Start HTTP server ───────────────────────────────────────── */
    rc_http_server_t http_server;
    memset(&http_server, 0, sizeof(http_server));

    if (rc_http_start(&http_server, &config, &db_pool) != 0) {
        fprintf(stderr, "[main] Failed to start HTTP server\n");
        rc_db_shutdown(&db_pool);
        return EXIT_FAILURE;
    }

    /* ─── Start UDP/DTLS socket server ────────────────────────────── */
    rc_socket_server_t socket_server;
    if (rc_socket_start(&socket_server, &config, &db_pool) != 0) {
        fprintf(stderr, "[main] Failed to start socket server\n");
        rc_http_stop(&http_server);
        rc_db_shutdown(&db_pool);
        return EXIT_FAILURE;
    }

    /* ─── Signal handling ─────────────────────────────────────────── */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fprintf(stdout, "[main] Server running. Press Ctrl+C to stop.\n");
    fprintf(stdout, "[main] HTTP API:  http://0.0.0.0:%u\n", config.api_port);
    fprintf(stdout, "[main] UDP Socket: 0.0.0.0:%u\n", config.socket_port);

    /* ─── Main loop (wait for shutdown signal) ────────────────────── */
    while (g_running) {
        sleep(1);
    }

    fprintf(stdout, "\n[main] Shutting down...\n");

    /* ─── Cleanup ─────────────────────────────────────────────────── */
    rc_socket_stop(&socket_server);
    rc_http_stop(&http_server);
    rc_db_shutdown(&db_pool);
    curl_global_cleanup();
    EVP_cleanup();
    ERR_free_strings();

    fprintf(stdout, "[main] Goodbye.\n");
    return EXIT_SUCCESS;
}
