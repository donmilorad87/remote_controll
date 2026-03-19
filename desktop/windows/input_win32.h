#ifndef RC_INPUT_WIN32_H
#define RC_INPUT_WIN32_H

#include <stdint.h>

int rc_input_init(void);
void rc_input_shutdown(void);
void rc_input_move_cursor(uint8_t direction, uint8_t speed);
void rc_input_mouse_click(uint8_t button);
void rc_input_mouse_scroll(int8_t direction);
void rc_input_key_press(uint16_t keycode, uint8_t modifiers);
void rc_input_key_release(uint16_t keycode, uint8_t modifiers);

#endif /* RC_INPUT_WIN32_H */
