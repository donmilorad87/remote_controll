#ifndef RC_AUTH_H
#define RC_AUTH_H

#include <stdbool.h>
#include <stddef.h>

#define RC_BCRYPT_HASH_LEN  64
#define RC_SALT_LEN         16

/* Hash a password using bcrypt-like hashing (SHA-256 + salt via OpenSSL).
   Output buffer must be at least RC_BCRYPT_HASH_LEN bytes. */
int rc_auth_hash_password(const char *password, char *hash_out, size_t hash_out_size);

/* Verify a password against a stored hash. Returns true if match. */
bool rc_auth_verify_password(const char *password, const char *stored_hash);

/* Validate email format. Returns true if valid. */
bool rc_auth_validate_email(const char *email);

/* Validate password strength. Returns true if acceptable.
   Requirements: >= 8 chars. */
bool rc_auth_validate_password(const char *password);

#endif /* RC_AUTH_H */
