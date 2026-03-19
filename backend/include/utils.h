#ifndef RC_UTILS_H
#define RC_UTILS_H

#include <stddef.h>
#include <stdint.h>

/* Read an environment variable with a default fallback.
   Copies into buf. Returns 0 on success. */
int rc_env_get(const char *name, char *buf, size_t buf_size, const char *default_val);

/* Read an environment variable as uint16_t. Returns default_val if not set. */
int rc_env_get_uint16(const char *name, uint16_t *out, uint16_t default_val);

/* Read an environment variable as uint32_t. Returns default_val if not set. */
int rc_env_get_uint32(const char *name, uint32_t *out, uint32_t default_val);

/* Generate random hex string. out must be at least (len*2 + 1) bytes. */
int rc_random_hex(char *out, size_t hex_len);

/* Safe string copy with null termination. */
void rc_strlcpy(char *dst, const char *src, size_t dst_size);

/* Read entire file into a malloc'd buffer. Caller must free.
   Returns NULL on error. Sets *out_len if non-NULL. */
char *rc_read_file(const char *path, size_t *out_len);

#endif /* RC_UTILS_H */
