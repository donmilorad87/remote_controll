#ifndef RC_PROTOCOL_H
#define RC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ─── Event types ────────────────────────────────────────────────── */
#define RC_EVT_CURSOR_MOVE      0x01
#define RC_EVT_MOUSE_CLICK      0x02
#define RC_EVT_MOUSE_SCROLL     0x03
#define RC_EVT_KEY_PRESS        0x04
#define RC_EVT_KEY_RELEASE      0x05
#define RC_EVT_VOLUME           0x06
#define RC_EVT_AUTH             0xFE
#define RC_EVT_HEARTBEAT        0xFF

/* ─── Mouse buttons ──────────────────────────────────────────────── */
#define RC_MOUSE_LEFT           0
#define RC_MOUSE_MIDDLE         1
#define RC_MOUSE_RIGHT          2

/* ─── Volume actions ─────────────────────────────────────────────── */
#define RC_VOL_DOWN             0
#define RC_VOL_UP               1
#define RC_VOL_MUTE             2

/* ─── Cursor directions (discrete) ───────────────────────────────── */
#define RC_DIR_UP               0
#define RC_DIR_DOWN             1
#define RC_DIR_LEFT             2
#define RC_DIR_RIGHT            3
#define RC_DIR_UP_LEFT          4
#define RC_DIR_UP_RIGHT         5
#define RC_DIR_DOWN_LEFT        6
#define RC_DIR_DOWN_RIGHT       7
#define RC_DIR_STOP             8

/* ─── Event structure ────────────────────────────────────────────── */
#define RC_MAX_EVENT_SIZE       2048
#define RC_MAX_AUTH_TOKEN_LEN   2048

typedef struct {
    uint8_t type;
    union {
        struct {
            uint8_t direction;      /* RC_DIR_* constant */
            uint8_t speed;          /* 1-10 movement speed multiplier */
        } cursor_move;

        struct {
            uint8_t button;         /* RC_MOUSE_LEFT/MIDDLE/RIGHT */
        } mouse_click;

        struct {
            int8_t direction;       /* -1 = down, 1 = up */
        } mouse_scroll;

        struct {
            uint16_t keycode;       /* Platform-independent keycode */
            uint8_t  modifiers;     /* Bitmask: 1=shift, 2=ctrl, 4=alt, 8=meta */
        } key;

        struct {
            uint8_t action;         /* RC_VOL_DOWN/UP/MUTE */
        } volume;

        struct {
            uint16_t token_len;
            char     token[RC_MAX_AUTH_TOKEN_LEN];
        } auth;
    } data;
} rc_event_t;

/* Serialize an event to a byte buffer. Returns bytes written, or -1 on error. */
int rc_protocol_serialize(const rc_event_t *event, uint8_t *buf, size_t buf_size);

/* Deserialize a byte buffer to an event. Returns 0 on success, -1 on error. */
int rc_protocol_deserialize(const uint8_t *buf, size_t buf_len, rc_event_t *event);

#endif /* RC_PROTOCOL_H */
