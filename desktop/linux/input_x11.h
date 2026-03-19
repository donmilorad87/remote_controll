#ifndef RC_INPUT_X11_H
#define RC_INPUT_X11_H

#include <stdbool.h>
#include <stdint.h>

/* Initialize X11 input subsystem. Call once at startup. Returns 0 on success. */
int rc_input_init(void);

/* Shutdown X11 input subsystem. */
void rc_input_shutdown(void);

/* Move cursor by relative delta. direction is RC_DIR_* constant, speed 1-10. */
void rc_input_move_cursor(uint8_t direction, uint8_t speed);

/* Simulate mouse button click.
   button: RC_MOUSE_LEFT, RC_MOUSE_MIDDLE, RC_MOUSE_RIGHT */
void rc_input_mouse_click(uint8_t button);

/* Simulate mouse scroll. direction: 1=up, -1=down */
void rc_input_mouse_scroll(int8_t direction);

/* Simulate key press. keycode is X11 keysym, modifiers bitmask. */
void rc_input_key_press(uint16_t keycode, uint8_t modifiers);

/* Simulate key release. */
void rc_input_key_release(uint16_t keycode, uint8_t modifiers);

#endif /* RC_INPUT_X11_H */
