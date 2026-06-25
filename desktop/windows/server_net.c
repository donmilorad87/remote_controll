#include "rc_desktop.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ws2tcpip.h>
#include <process.h>

static BOOL g_wsa_init = FALSE;
static int ensure_wsa(void) {
    if (!g_wsa_init) { WSADATA w; if (WSAStartup(MAKEWORD(2,2), &w) != 0) return -1; g_wsa_init = TRUE; }
    return 0;
}

int rc_detect_lan_ip(char *ip_out, size_t ip_out_size)
{
    assert(ip_out != NULL);
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) return -1;
    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) return -1;
    for (struct addrinfo *p = result; p; p = p->ai_next) {
        struct sockaddr_in *a = (struct sockaddr_in *)p->ai_addr;
        if ((ntohl(a->sin_addr.s_addr) & 0xFF000000) == 0x7F000000) continue;
        inet_ntop(AF_INET, &a->sin_addr, ip_out, (socklen_t)ip_out_size);
        freeaddrinfo(result);
        return 0;
    }
    freeaddrinfo(result);
    return -1;
}

void rc_load_name(char *name, size_t name_size)
{
    assert(name != NULL);
    FILE *f = fopen(RC_CONFIG_FILE, "r");
    if (f) { if (fgets(name, (int)name_size, f)) { size_t l = strlen(name); if (l > 0 && name[l-1] == '\n') name[l-1] = '\0'; } fclose(f); }
    if (name[0] == '\0') {
        DWORD sz = (DWORD)name_size;
        if (!GetComputerNameA(name, &sz)) snprintf(name, name_size, "Desktop");
    }
}

void rc_save_name(const char *name)
{
    FILE *f = fopen(RC_CONFIG_FILE, "w");
    if (f) { fprintf(f, "%s\n", name); fclose(f); }
}

/* Announce thread */
static unsigned __stdcall announce_func(void *arg)
{
    rc_desktop_t *d = (rc_desktop_t *)arg;
    struct sockaddr_in bcast = { .sin_family = AF_INET, .sin_port = htons(RC_ANNOUNCE_PORT) };
    bcast.sin_addr.s_addr = INADDR_BROADCAST;
    uint8_t buf[RC_MAX_EVENT_SIZE];

    while (d->running) {
        int len = rc_ser_announce(d->info.name, d->info.listen_port, buf, sizeof(buf));
        if (len > 0) sendto((SOCKET)d->announce_fd, (const char *)buf, len, 0, (struct sockaddr *)&bcast, sizeof(bcast));
        Sleep(RC_ANNOUNCE_INTERVAL_MS);
    }
    return 0;
}

/* Find or add mobile */
static rc_mobile_slot_t *find_or_add(rc_desktop_t *d, struct sockaddr_in *addr)
{
    for (int i = 0; i < RC_MAX_MOBILES; i++) {
        if (d->mobiles[i].active && d->mobiles[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr && d->mobiles[i].addr.sin_port == addr->sin_port) {
            d->mobiles[i].last_seen = time(NULL); return &d->mobiles[i];
        }
    }
    for (int i = 0; i < RC_MAX_MOBILES; i++) {
        if (!d->mobiles[i].active) { d->mobiles[i].active = true; d->mobiles[i].addr = *addr; d->mobiles[i].last_seen = time(NULL); return &d->mobiles[i]; }
    }
    return NULL;
}

/* Recv thread */
static unsigned __stdcall recv_func(void *arg)
{
    rc_desktop_t *d = (rc_desktop_t *)arg;
    uint8_t buf[RC_MAX_EVENT_SIZE];
    struct sockaddr_in sender; int slen = sizeof(sender);

    while (d->running) {
        int n = recvfrom((SOCKET)d->udp_fd, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&sender, &slen);
        if (n <= 0) { if (!d->running) break; continue; }
        rc_event_t evt;
        if (rc_event_deserialize(buf, (size_t)n, &evt) != 0) continue;

        if (evt.type == RC_EVT_CONNECT) {
            find_or_add(d, &sender);
            /* Reply with CONNECTED */
            uint8_t r[4]; int rl = rc_ser_connected(r, sizeof(r));
            if (rl > 0) sendto((SOCKET)d->udp_fd, (const char *)r, rl, 0, (struct sockaddr *)&sender, slen);
            /* Also reply with ANNOUNCE so mobile learns our name */
            uint8_t ann[RC_MAX_EVENT_SIZE];
            int alen = rc_ser_announce(d->info.name, d->info.listen_port, ann, sizeof(ann));
            if (alen > 0) sendto((SOCKET)d->udp_fd, (const char *)ann, alen, 0, (struct sockaddr *)&sender, slen);
        } else if (evt.type == RC_EVT_HEARTBEAT) {
            for (int i = 0; i < RC_MAX_MOBILES; i++) {
                if (d->mobiles[i].active && d->mobiles[i].addr.sin_addr.s_addr == sender.sin_addr.s_addr && d->mobiles[i].addr.sin_port == sender.sin_port)
                    d->mobiles[i].last_seen = time(NULL);
            }
        } else {
            bool ok = false;
            for (int i = 0; i < RC_MAX_MOBILES; i++) {
                if (d->mobiles[i].active && d->mobiles[i].addr.sin_addr.s_addr == sender.sin_addr.s_addr && d->mobiles[i].addr.sin_port == sender.sin_port) {
                    ok = true; d->mobiles[i].last_seen = time(NULL); break;
                }
            }
            if (ok) rc_handle_event(&evt);
        }

        time_t now = time(NULL);
        for (int i = 0; i < RC_MAX_MOBILES; i++) {
            if (d->mobiles[i].active && (now - d->mobiles[i].last_seen) > 30) d->mobiles[i].active = false;
        }
    }
    return 0;
}

int rc_desktop_start(rc_desktop_t *desktop)
{
    assert(desktop != NULL);
    memset(desktop, 0, sizeof(*desktop));
    desktop->running = true;
    desktop->info.listen_port = RC_PORT;
    if (ensure_wsa() != 0) return -1;
    rc_load_name(desktop->info.name, sizeof(desktop->info.name));
    if (rc_detect_lan_ip(desktop->info.lan_ip, sizeof(desktop->info.lan_ip)) != 0) strcpy(desktop->info.lan_ip, "0.0.0.0");

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return -1;
    BOOL opt = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    struct sockaddr_in ba = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(RC_PORT) };
    if (bind(s, (struct sockaddr *)&ba, sizeof(ba)) == SOCKET_ERROR) { closesocket(s); return -1; }
    desktop->udp_fd = (int)s;

    SOCKET as = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (as == INVALID_SOCKET) { closesocket(s); return -1; }
    setsockopt(as, SOL_SOCKET, SO_BROADCAST, (const char *)&opt, sizeof(opt));
    desktop->announce_fd = (int)as;

    desktop->recv_thread = (HANDLE)_beginthreadex(NULL, 0, recv_func, desktop, 0, NULL);
    desktop->announce_thread = (HANDLE)_beginthreadex(NULL, 0, announce_func, desktop, 0, NULL);
    return 0;
}

void rc_desktop_stop(rc_desktop_t *desktop)
{
    assert(desktop != NULL);
    desktop->running = false;
    if (desktop->udp_fd >= 0) { closesocket((SOCKET)desktop->udp_fd); desktop->udp_fd = -1; }
    if (desktop->announce_fd >= 0) { closesocket((SOCKET)desktop->announce_fd); desktop->announce_fd = -1; }
    if (desktop->recv_thread) { WaitForSingleObject(desktop->recv_thread, 3000); CloseHandle(desktop->recv_thread); }
    if (desktop->announce_thread) { WaitForSingleObject(desktop->announce_thread, 3000); CloseHandle(desktop->announce_thread); }
}
