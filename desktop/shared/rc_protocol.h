/* RemoteControl — binary protocol for direct LAN communication.
   Desktop is UDP server. Mobile connects directly — no pairing needed.
   Desktop announces itself on LAN. Mobile discovers and connects. */

#ifndef RC_PROTOCOL_H
#define RC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ─── Event types ────────────────────────────────────────────────── */
#define RC_EVT_CURSOR_MOVE      0x01  /* D-pad discrete direction */
#define RC_EVT_MOUSE_CLICK      0x02
#define RC_EVT_MOUSE_SCROLL     0x03
#define RC_EVT_KEY_PRESS        0x04
#define RC_EVT_KEY_RELEASE      0x05
#define RC_EVT_VOLUME           0x06
#define RC_EVT_TOUCHPAD         0x07  /* Relative dx/dy from touch drag */
#define RC_EVT_ANNOUNCE         0xA0  /* Desktop broadcasts: "I exist" */
#define RC_EVT_CONNECT          0xA1  /* Mobile sends: "I want to control you" */
#define RC_EVT_CONNECTED        0xA2  /* Desktop replies: "OK, connected" */
#define RC_EVT_HEARTBEAT        0xFF

/* ─── Mouse buttons ──────────────────────────────────────────────── */
#define RC_MOUSE_LEFT           0
#define RC_MOUSE_MIDDLE         1
#define RC_MOUSE_RIGHT          2

/* ─── Volume actions ─────────────────────────────────────────────── */
#define RC_VOL_DOWN             0
#define RC_VOL_UP               1
#define RC_VOL_MUTE             2

/* ─── Cursor directions (discrete D-pad) ─────────────────────────── */
#define RC_DIR_UP               0
#define RC_DIR_DOWN             1
#define RC_DIR_LEFT             2
#define RC_DIR_RIGHT            3
#define RC_DIR_STOP             8

/* ─── Sizes ──────────────────────────────────────────────────────── */
#define RC_MAX_EVENT_SIZE       256
#define RC_MAX_NAME_LEN         32
#define RC_PORT                 7482
#define RC_ANNOUNCE_PORT        7483
#define RC_ANNOUNCE_INTERVAL_MS 2000

/* ─── Event structure ────────────────────────────────────────────── */
typedef struct {
    uint8_t type;
    union {
        struct { uint8_t direction; uint8_t speed; } cursor_move;
        struct { int16_t dx; int16_t dy; } touchpad;
        struct { uint8_t button; } mouse_click;
        struct { int8_t direction; } mouse_scroll;
        struct { uint16_t keycode; uint8_t modifiers; } key;
        struct { uint8_t action; } volume;
        struct {
            char     name[RC_MAX_NAME_LEN + 1];
            uint16_t port;
        } announce;
    } data;
} rc_event_t;

/* ─── Deserialize ────────────────────────────────────────────────── */
static inline int rc_event_deserialize(const uint8_t *buf, size_t len, rc_event_t *evt)
{
    if (len < 1 || evt == NULL) return -1;
    memset(evt, 0, sizeof(*evt));
    evt->type = buf[0];
    size_t off = 1;

    switch (evt->type) {
    case RC_EVT_CURSOR_MOVE:
        if (len < 3) return -1;
        evt->data.cursor_move.direction = buf[off++];
        evt->data.cursor_move.speed = buf[off++];
        break;
    case RC_EVT_TOUCHPAD:
        if (len < 5) return -1;
        evt->data.touchpad.dx = (int16_t)((buf[off] << 8) | buf[off + 1]); off += 2;
        evt->data.touchpad.dy = (int16_t)((buf[off] << 8) | buf[off + 1]); off += 2;
        break;
    case RC_EVT_MOUSE_CLICK:
        if (len < 2) return -1;
        evt->data.mouse_click.button = buf[off++];
        break;
    case RC_EVT_MOUSE_SCROLL:
        if (len < 2) return -1;
        evt->data.mouse_scroll.direction = (int8_t)buf[off++];
        break;
    case RC_EVT_KEY_PRESS:
    case RC_EVT_KEY_RELEASE:
        if (len < 4) return -1;
        evt->data.key.keycode = (uint16_t)((buf[off] << 8) | buf[off + 1]); off += 2;
        evt->data.key.modifiers = buf[off++];
        break;
    case RC_EVT_VOLUME:
        if (len < 2) return -1;
        evt->data.volume.action = buf[off++];
        break;
    case RC_EVT_ANNOUNCE: {
        if (len < 4) return -1;
        uint8_t name_len = buf[off++];
        if (name_len > RC_MAX_NAME_LEN || len < off + name_len + 2) return -1;
        memcpy(evt->data.announce.name, buf + off, name_len);
        evt->data.announce.name[name_len] = '\0';
        off += name_len;
        evt->data.announce.port = (uint16_t)((buf[off] << 8) | buf[off + 1]);
        break;
    }
    case RC_EVT_CONNECT:
    case RC_EVT_CONNECTED:
    case RC_EVT_HEARTBEAT:
        break;
    default:
        return -1;
    }
    return 0;
}

/* ─── Serialize helpers ──────────────────────────────────────────── */

static inline int rc_ser_cursor(uint8_t dir, uint8_t speed, uint8_t *b, size_t s)
{
    if (s < 3) return -1;
    b[0] = RC_EVT_CURSOR_MOVE; b[1] = dir; b[2] = speed; return 3;
}

static inline int rc_ser_touchpad(int16_t dx, int16_t dy, uint8_t *b, size_t s)
{
    if (s < 5) return -1;
    b[0] = RC_EVT_TOUCHPAD;
    b[1] = (uint8_t)(dx >> 8); b[2] = (uint8_t)(dx & 0xFF);
    b[3] = (uint8_t)(dy >> 8); b[4] = (uint8_t)(dy & 0xFF);
    return 5;
}

static inline int rc_ser_click(uint8_t btn, uint8_t *b, size_t s)
{
    if (s < 2) return -1;
    b[0] = RC_EVT_MOUSE_CLICK;
    b[1] = btn;
    return 2;
}

static inline int rc_ser_scroll(int8_t dir, uint8_t *b, size_t s)
{
    if (s < 2) return -1;
    b[0] = RC_EVT_MOUSE_SCROLL;
    b[1] = (uint8_t)dir;
    return 2;
}

static inline int rc_ser_key(uint8_t type, uint16_t kc, uint8_t mod, uint8_t *b, size_t s)
{
    if (s < 4) return -1;
    b[0] = type;
    b[1] = (uint8_t)(kc >> 8);
    b[2] = (uint8_t)(kc & 0xFF);
    b[3] = mod;
    return 4;
}

static inline int rc_ser_volume(uint8_t act, uint8_t *b, size_t s)
{
    if (s < 2) return -1;
    b[0] = RC_EVT_VOLUME;
    b[1] = act;
    return 2;
}

static inline int rc_ser_announce(const char *name, uint16_t port, uint8_t *b, size_t s)
{
    uint8_t nlen = (uint8_t)strlen(name);
    if (nlen > RC_MAX_NAME_LEN) nlen = RC_MAX_NAME_LEN;
    size_t total = 1 + 1 + nlen + 2;
    if (s < total) return -1;
    b[0] = RC_EVT_ANNOUNCE;
    b[1] = nlen;
    memcpy(b + 2, name, nlen);
    b[2 + nlen] = (uint8_t)(port >> 8);
    b[3 + nlen] = (uint8_t)(port & 0xFF);
    return (int)total;
}

static inline int rc_ser_connect(uint8_t *b, size_t s)
{
    if (s < 1) return -1;
    b[0] = RC_EVT_CONNECT;
    return 1;
}

static inline int rc_ser_connected(uint8_t *b, size_t s)
{
    if (s < 1) return -1;
    b[0] = RC_EVT_CONNECTED;
    return 1;
}

static inline int rc_ser_heartbeat(uint8_t *b, size_t s)
{
    if (s < 1) return -1;
    b[0] = RC_EVT_HEARTBEAT;
    return 1;
}

#endif /* RC_PROTOCOL_H */
