#ifndef RC_JWT_H
#define RC_JWT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define RC_JWT_MAX_LEN      2048
#define RC_JWT_ID_LEN       32

typedef struct {
    char    user_id[64];
    char    device_type[24];
    char    jwt_id[RC_JWT_ID_LEN + 1];
    int64_t issued_at;
    int64_t expires_at;
} rc_jwt_claims_t;

/* Generate a JWT token. Returns 0 on success.
   token_out must be at least RC_JWT_MAX_LEN bytes. */
int rc_jwt_create(const char *secret, const char *user_id,
                  const char *device_type, uint32_t expiry_seconds,
                  char *token_out, size_t token_out_size,
                  char *jwt_id_out, size_t jwt_id_out_size);

/* Verify and decode a JWT token. Returns 0 on success. */
int rc_jwt_verify(const char *secret, const char *token,
                  rc_jwt_claims_t *claims_out);

#endif /* RC_JWT_H */
