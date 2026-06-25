/* RemoteControl — desktop app header.
   Desktop is UDP server. Auto-announces on LAN. No pairing needed. */

#ifndef RC_DESKTOP_H
#define RC_DESKTOP_H

#include <stdbool.h>
#include <stdint.h>
#include "rc_protocol.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <pthread.h>
    #include <netinet/in.h>
#endif

#define RC_MAX_MOBILES  8
#define RC_CONFIG_FILE  ".remotecontrol.conf"

typedef struct {
    bool                active;
    struct sockaddr_in  addr;
    time_t              last_seen;
} rc_mobile_slot_t;

typedef struct {
    char                name[RC_MAX_NAME_LEN + 1];
    char                lan_ip[46];
    uint16_t            listen_port;
} rc_desktop_info_t;

typedef struct {
    int                 udp_fd;         /* Main UDP socket for events */
    int                 announce_fd;    /* UDP socket for broadcast announce */
    rc_mobile_slot_t    mobiles[RC_MAX_MOBILES];
    rc_desktop_info_t   info;
    volatile bool       running;
#ifdef _WIN32
    HANDLE              recv_thread;
    HANDLE              announce_thread;
#else
    pthread_t           recv_thread;
    pthread_t           announce_thread;
#endif
} rc_desktop_t;

/* Detect the LAN IP address of this machine. */
int rc_detect_lan_ip(char *ip_out, size_t ip_out_size);

/* Load desktop name from config file. Falls back to hostname. */
void rc_load_name(char *name, size_t name_size);

/* Save desktop name to config file. */
void rc_save_name(const char *name);

/* Start the desktop server (UDP listener + announcer). Returns 0 on success. */
int rc_desktop_start(rc_desktop_t *desktop);

/* Stop the desktop server. */
void rc_desktop_stop(rc_desktop_t *desktop);

/* Event handler — implemented per platform (calls input/volume). */
extern void rc_handle_event(const rc_event_t *event);

#endif /* RC_DESKTOP_H */
