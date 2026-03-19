#ifndef RC_VOLUME_MACOS_H
#define RC_VOLUME_MACOS_H

#include <stdint.h>

int rc_volume_init(void);
void rc_volume_shutdown(void);
void rc_volume_handle(uint8_t action);

#endif /* RC_VOLUME_MACOS_H */
