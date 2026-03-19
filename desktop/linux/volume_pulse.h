#ifndef RC_VOLUME_PULSE_H
#define RC_VOLUME_PULSE_H

#include <stdint.h>

/* Initialize PulseAudio volume control. Returns 0 on success. */
int rc_volume_init(void);

/* Shutdown volume control. */
void rc_volume_shutdown(void);

/* Handle volume event. action: RC_VOL_UP, RC_VOL_DOWN, RC_VOL_MUTE */
void rc_volume_handle(uint8_t action);

#endif /* RC_VOLUME_PULSE_H */
