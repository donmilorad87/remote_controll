/* Shared protocol header for all desktop clients.
   Must match the server's protocol.h exactly. */

#ifndef RC_CLIENT_PROTOCOL_H
#define RC_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Event types */
#define RC_EVT_CURSOR_MOVE      0x01
#define RC_EVT_MOUSE_CLICK      0x02
#define RC_EVT_MOUSE_SCROLL     0x03
#define RC_EVT_KEY_PRESS        0x04
#define RC_EVT_KEY_RELEASE      0x05
#define RC_EVT_VOLUME           0x06
#define RC_EVT_AUTH             0xFE
#define RC_EVT_HEARTBEAT        0xFF

/* Mouse buttons */
#define RC_MOUSE_LEFT           0
#define RC_MOUSE_MIDDLE         1
#define RC_MOUSE_RIGHT          2

/* Volume actions */
#define RC_VOL_DOWN             0
#define RC_VOL_UP               1
#define RC_VOL_MUTE             2

/* Cursor directions */
#define RC_DIR_UP               0
#define RC_DIR_DOWN             1
#define RC_DIR_LEFT             2
#define RC_DIR_RIGHT            3
#define RC_DIR_UP_LEFT          4
#define RC_DIR_UP_RIGHT         5
#define RC_DIR_DOWN_LEFT        6
#define RC_DIR_DOWN_RIGHT       7
#define RC_DIR_STOP             8

/* Max sizes */
#define RC_MAX_EVENT_SIZE       2048
#define RC_MAX_AUTH_TOKEN_LEN   2048

typedef struct {
    uint8_t type;
    union {
        struct { uint8_t direction; uint8_t speed; } cursor_move;
        struct { uint8_t button; } mouse_click;
        struct { int8_t direction; } mouse_scroll;
        struct { uint16_t keycode; uint8_t modifiers; } key;
        struct { uint8_t action; } volume;
        struct { uint16_t token_len; char token[RC_MAX_AUTH_TOKEN_LEN]; } auth;
    } data;
} rc_event_t;

/* Deserialize a received packet into an event. Returns 0 on success. */
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
        evt->data.key.keycode = (uint16_t)((buf[off] << 8) | buf[off+1]);
        off += 2;
        evt->data.key.modifiers = buf[off++];
        break;
    case RC_EVT_VOLUME:
        if (len < 2) return -1;
        evt->data.volume.action = buf[off++];
        break;
    case RC_EVT_HEARTBEAT:
        break;
    default:
        return -1;
    }
    return 0;
}

/* Serialize an auth event. Returns bytes written. */
static inline int rc_event_serialize_auth(const char *token, uint8_t *buf, size_t buf_size)
{
    uint16_t tlen = (uint16_t)strlen(token);
    size_t needed = 3 + tlen;
    if (buf_size < needed) return -1;
    buf[0] = RC_EVT_AUTH;
    buf[1] = (uint8_t)(tlen >> 8);
    buf[2] = (uint8_t)(tlen & 0xFF);
    memcpy(buf + 3, token, tlen);
    return (int)needed;
}

/* Serialize a heartbeat. Returns bytes written. */
static inline int rc_event_serialize_heartbeat(uint8_t *buf, size_t buf_size)
{
    if (buf_size < 1) return -1;
    buf[0] = RC_EVT_HEARTBEAT;
    return 1;
}

#endif /* RC_CLIENT_PROTOCOL_H */
