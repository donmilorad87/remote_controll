#define _GNU_SOURCE
#include "rc_desktop.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>

/* ─── Detect LAN IP ──────────────────────────────────────────────── */

int rc_detect_lan_ip(char *ip_out, size_t ip_out_size)
{
    assert(ip_out != NULL);
    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == -1) return -1;

    int found = -1;
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, ip_out, (socklen_t)ip_out_size);
        found = 0;
        break;
    }
    freeifaddrs(ifaddr);
    return found;
}

/* ─── Config: desktop name ───────────────────────────────────────── */

static void get_config_path(char *path, size_t path_size)
{
    const char *home = getenv("HOME");
    if (home != NULL) {
        snprintf(path, path_size, "%s/%s", home, RC_CONFIG_FILE);
    } else {
        snprintf(path, path_size, "%s", RC_CONFIG_FILE);
    }
}

void rc_load_name(char *name, size_t name_size)
{
    assert(name != NULL);
    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (f != NULL) {
        if (fgets(name, (int)name_size, f) != NULL) {
            /* Strip newline */
            size_t len = strlen(name);
            if (len > 0 && name[len - 1] == '\n') name[len - 1] = '\0';
        }
        fclose(f);
    }

    /* Fallback to hostname */
    if (name[0] == '\0') {
        if (gethostname(name, name_size) != 0) {
            snprintf(name, name_size, "Desktop");
        }
    }
}

void rc_save_name(const char *name)
{
    assert(name != NULL);
    char path[512];
    get_config_path(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (f != NULL) {
        fprintf(f, "%s\n", name);
        fclose(f);
    }
}

/* ─── Announce thread: broadcast presence every 2s ───────────────── */

static void *announce_thread_func(void *arg)
{
    rc_desktop_t *d = (rc_desktop_t *)arg;
    assert(d != NULL);

    struct sockaddr_in bcast_addr;
    memset(&bcast_addr, 0, sizeof(bcast_addr));
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_addr.s_addr = INADDR_BROADCAST;
    bcast_addr.sin_port = htons(RC_ANNOUNCE_PORT);

    uint8_t buf[RC_MAX_EVENT_SIZE];

    while (d->running) {
        int len = rc_ser_announce(d->info.name, d->info.listen_port, buf, sizeof(buf));
        if (len > 0) {
            sendto(d->announce_fd, buf, (size_t)len, 0,
                   (struct sockaddr *)&bcast_addr, sizeof(bcast_addr));
        }
        usleep(RC_ANNOUNCE_INTERVAL_MS * 1000);
    }
    return NULL;
}

/* ─── Find or add mobile slot ────────────────────────────────────── */

static rc_mobile_slot_t *find_or_add_mobile(rc_desktop_t *d, struct sockaddr_in *addr)
{
    /* Check existing */
    for (int i = 0; i < RC_MAX_MOBILES; i++) {
        if (d->mobiles[i].active &&
            d->mobiles[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            d->mobiles[i].addr.sin_port == addr->sin_port) {
            d->mobiles[i].last_seen = time(NULL);
            return &d->mobiles[i];
        }
    }
    /* Add new */
    for (int i = 0; i < RC_MAX_MOBILES; i++) {
        if (!d->mobiles[i].active) {
            d->mobiles[i].active = true;
            d->mobiles[i].addr = *addr;
            d->mobiles[i].last_seen = time(NULL);
            fprintf(stdout, "[net] Mobile connected from %s:%d\n",
                    inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
            return &d->mobiles[i];
        }
    }
    return NULL;
}

/* ─── Main receive thread ────────────────────────────────────────── */

static void *recv_thread_func(void *arg)
{
    rc_desktop_t *d = (rc_desktop_t *)arg;
    assert(d != NULL);

    uint8_t buf[RC_MAX_EVENT_SIZE];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);

    while (d->running) {
        ssize_t n = recvfrom(d->udp_fd, buf, sizeof(buf), 0,
                             (struct sockaddr *)&sender, &sender_len);
        if (n <= 0) {
            if (!d->running) break;
            continue;
        }

        rc_event_t evt;
        if (rc_event_deserialize(buf, (size_t)n, &evt) != 0) continue;

        switch (evt.type) {
        case RC_EVT_CONNECT: {
            find_or_add_mobile(d, &sender);
            /* Reply with CONNECTED */
            uint8_t reply[4];
            int rlen = rc_ser_connected(reply, sizeof(reply));
            if (rlen > 0) {
                sendto(d->udp_fd, reply, (size_t)rlen, 0,
                       (struct sockaddr *)&sender, sender_len);
            }
            /* Also reply with ANNOUNCE so mobile learns our name */
            uint8_t ann[RC_MAX_EVENT_SIZE];
            int alen = rc_ser_announce(d->info.name, d->info.listen_port,
                                       ann, sizeof(ann));
            if (alen > 0) {
                sendto(d->udp_fd, ann, (size_t)alen, 0,
                       (struct sockaddr *)&sender, sender_len);
            }
            break;
        }
        case RC_EVT_HEARTBEAT: {
            /* Update last_seen for known mobile */
            for (int i = 0; i < RC_MAX_MOBILES; i++) {
                if (d->mobiles[i].active &&
                    d->mobiles[i].addr.sin_addr.s_addr == sender.sin_addr.s_addr &&
                    d->mobiles[i].addr.sin_port == sender.sin_port) {
                    d->mobiles[i].last_seen = time(NULL);
                    break;
                }
            }
            break;
        }
        default: {
            /* Accept events from any known mobile */
            bool known = false;
            for (int i = 0; i < RC_MAX_MOBILES; i++) {
                if (d->mobiles[i].active &&
                    d->mobiles[i].addr.sin_addr.s_addr == sender.sin_addr.s_addr &&
                    d->mobiles[i].addr.sin_port == sender.sin_port) {
                    known = true;
                    d->mobiles[i].last_seen = time(NULL);
                    break;
                }
            }
            if (known) {
                rc_handle_event(&evt);
            }
            break;
        }
        }

        /* Cleanup stale mobiles (no heartbeat for 30s) */
        time_t now = time(NULL);
        for (int i = 0; i < RC_MAX_MOBILES; i++) {
            if (d->mobiles[i].active && (now - d->mobiles[i].last_seen) > 30) {
                fprintf(stdout, "[net] Mobile timed out\n");
                d->mobiles[i].active = false;
            }
        }
    }
    return NULL;
}

/* ─── Start ──────────────────────────────────────────────────────── */

int rc_desktop_start(rc_desktop_t *desktop)
{
    assert(desktop != NULL);
    memset(desktop, 0, sizeof(*desktop));
    desktop->running = true;
    desktop->info.listen_port = RC_PORT;

    rc_load_name(desktop->info.name, sizeof(desktop->info.name));

    if (rc_detect_lan_ip(desktop->info.lan_ip, sizeof(desktop->info.lan_ip)) != 0) {
        strcpy(desktop->info.lan_ip, "0.0.0.0");
    }

    fprintf(stdout, "[net] Name: %s | IP: %s | Port: %d\n",
            desktop->info.name, desktop->info.lan_ip, desktop->info.listen_port);

    /* Main UDP socket */
    desktop->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (desktop->udp_fd < 0) return -1;

    int optval = 1;
    setsockopt(desktop->udp_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in bind_addr = { .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(RC_PORT) };
    if (bind(desktop->udp_fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        fprintf(stderr, "[net] Bind %d failed: %s\n", RC_PORT, strerror(errno));
        close(desktop->udp_fd);
        return -1;
    }

    /* Announce UDP socket (broadcast) */
    desktop->announce_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (desktop->announce_fd < 0) { close(desktop->udp_fd); return -1; }

    setsockopt(desktop->announce_fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));

    /* Start threads */
    pthread_create(&desktop->recv_thread, NULL, recv_thread_func, desktop);
    pthread_create(&desktop->announce_thread, NULL, announce_thread_func, desktop);

    fprintf(stdout, "[net] Listening on :%d, announcing on :%d\n", RC_PORT, RC_ANNOUNCE_PORT);
    return 0;
}

/* ─── Stop ───────────────────────────────────────────────────────── */

void rc_desktop_stop(rc_desktop_t *desktop)
{
    assert(desktop != NULL);
    desktop->running = false;
    if (desktop->udp_fd >= 0) { close(desktop->udp_fd); desktop->udp_fd = -1; }
    if (desktop->announce_fd >= 0) { close(desktop->announce_fd); desktop->announce_fd = -1; }
    pthread_join(desktop->recv_thread, NULL);
    pthread_join(desktop->announce_thread, NULL);
}
