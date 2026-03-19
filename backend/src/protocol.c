#include "protocol.h"

#include <assert.h>
#include <string.h>

/* Serialize event to wire format.
   Returns number of bytes written, or -1 on error. */
int rc_protocol_serialize(const rc_event_t *event, uint8_t *buf, size_t buf_size)
{
    assert(event != NULL);
    assert(buf != NULL);

    if (buf_size < 1) return -1;

    buf[0] = event->type;
    size_t offset = 1;

    switch (event->type) {
    case RC_EVT_CURSOR_MOVE:
        if (buf_size < 3) return -1;
        buf[offset++] = event->data.cursor_move.direction;
        buf[offset++] = event->data.cursor_move.speed;
        break;

    case RC_EVT_MOUSE_CLICK:
        if (buf_size < 2) return -1;
        buf[offset++] = event->data.mouse_click.button;
        break;

    case RC_EVT_MOUSE_SCROLL:
        if (buf_size < 2) return -1;
        buf[offset++] = (uint8_t)event->data.mouse_scroll.direction;
        break;

    case RC_EVT_KEY_PRESS:
    case RC_EVT_KEY_RELEASE:
        if (buf_size < 4) return -1;
        buf[offset++] = (uint8_t)(event->data.key.keycode >> 8);
        buf[offset++] = (uint8_t)(event->data.key.keycode & 0xFF);
        buf[offset++] = event->data.key.modifiers;
        break;

    case RC_EVT_VOLUME:
        if (buf_size < 2) return -1;
        buf[offset++] = event->data.volume.action;
        break;

    case RC_EVT_AUTH: {
        uint16_t tlen = event->data.auth.token_len;
        if (buf_size < (size_t)(3 + tlen)) return -1;
        buf[offset++] = (uint8_t)(tlen >> 8);
        buf[offset++] = (uint8_t)(tlen & 0xFF);
        memcpy(buf + offset, event->data.auth.token, tlen);
        offset += tlen;
        break;
    }

    case RC_EVT_HEARTBEAT:
        /* No payload */
        break;

    default:
        return -1;
    }

    return (int)offset;
}

/* Deserialize wire format to event.
   Returns 0 on success, -1 on error. */
int rc_protocol_deserialize(const uint8_t *buf, size_t buf_len, rc_event_t *event)
{
    assert(buf != NULL);
    assert(event != NULL);

    if (buf_len < 1) return -1;

    memset(event, 0, sizeof(*event));
    event->type = buf[0];
    size_t offset = 1;

    switch (event->type) {
    case RC_EVT_CURSOR_MOVE:
        if (buf_len < 3) return -1;
        event->data.cursor_move.direction = buf[offset++];
        event->data.cursor_move.speed = buf[offset++];
        if (event->data.cursor_move.direction > RC_DIR_STOP) return -1;
        if (event->data.cursor_move.speed > 10) return -1;
        break;

    case RC_EVT_MOUSE_CLICK:
        if (buf_len < 2) return -1;
        event->data.mouse_click.button = buf[offset++];
        if (event->data.mouse_click.button > RC_MOUSE_RIGHT) return -1;
        break;

    case RC_EVT_MOUSE_SCROLL:
        if (buf_len < 2) return -1;
        event->data.mouse_scroll.direction = (int8_t)buf[offset++];
        if (event->data.mouse_scroll.direction != -1 &&
            event->data.mouse_scroll.direction != 1) return -1;
        break;

    case RC_EVT_KEY_PRESS:
    case RC_EVT_KEY_RELEASE:
        if (buf_len < 4) return -1;
        event->data.key.keycode = (uint16_t)((buf[offset] << 8) | buf[offset + 1]);
        offset += 2;
        event->data.key.modifiers = buf[offset++];
        break;

    case RC_EVT_VOLUME:
        if (buf_len < 2) return -1;
        event->data.volume.action = buf[offset++];
        if (event->data.volume.action > RC_VOL_MUTE) return -1;
        break;

    case RC_EVT_AUTH: {
        if (buf_len < 3) return -1;
        uint16_t tlen = (uint16_t)((buf[offset] << 8) | buf[offset + 1]);
        offset += 2;
        if (tlen == 0 || tlen > RC_MAX_AUTH_TOKEN_LEN) return -1;
        if (buf_len < offset + tlen) return -1;
        event->data.auth.token_len = tlen;
        memcpy(event->data.auth.token, buf + offset, tlen);
        event->data.auth.token[tlen] = '\0';
        break;
    }

    case RC_EVT_HEARTBEAT:
        break;

    default:
        return -1;
    }

    return 0;
}
