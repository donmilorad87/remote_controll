#include "auth.h"
#include "utils.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/* Password hash format: $rc$<32-hex-salt>$<64-hex-sha256-hash>
   Total: 4 + 32 + 1 + 64 + 1 = ~102 chars */

#define HASH_PREFIX     "$rc$"
#define HASH_PREFIX_LEN 4
#define SALT_HEX_LEN    32
#define DIGEST_HEX_LEN  64
#define ITERATIONS       100000

static int compute_hash(const char *password, const unsigned char *salt,
                        size_t salt_len, char *hex_out)
{
    assert(password != NULL);
    assert(salt != NULL);

    unsigned char digest[32]; /* SHA-256 = 32 bytes */
    int ret = PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
                                 salt, (int)salt_len,
                                 ITERATIONS, EVP_sha256(),
                                 sizeof(digest), digest);
    if (ret != 1) {
        return -1;
    }

    for (int i = 0; i < 32; i++) {
        snprintf(hex_out + (i * 2), 3, "%02x", digest[i]);
    }
    hex_out[DIGEST_HEX_LEN] = '\0';
    return 0;
}

int rc_auth_hash_password(const char *password, char *hash_out, size_t hash_out_size)
{
    assert(password != NULL);
    assert(hash_out != NULL);

    /* Need: "$rc$" + 32 hex salt + "$" + 64 hex digest + '\0' = 102 */
    if (hash_out_size < 102) {
        return -1;
    }

    unsigned char salt_bytes[16];
    if (RAND_bytes(salt_bytes, sizeof(salt_bytes)) != 1) {
        return -1;
    }

    char salt_hex[SALT_HEX_LEN + 1];
    for (int i = 0; i < 16; i++) {
        snprintf(salt_hex + (i * 2), 3, "%02x", salt_bytes[i]);
    }
    salt_hex[SALT_HEX_LEN] = '\0';

    char digest_hex[DIGEST_HEX_LEN + 1];
    if (compute_hash(password, salt_bytes, sizeof(salt_bytes), digest_hex) != 0) {
        return -1;
    }

    snprintf(hash_out, hash_out_size, "%s%s$%s", HASH_PREFIX, salt_hex, digest_hex);
    return 0;
}

bool rc_auth_verify_password(const char *password, const char *stored_hash)
{
    assert(password != NULL);
    assert(stored_hash != NULL);

    /* Verify format: "$rc$<32-hex>$<64-hex>" */
    if (strncmp(stored_hash, HASH_PREFIX, HASH_PREFIX_LEN) != 0) {
        return false;
    }

    size_t hash_len = strlen(stored_hash);
    /* Expected: 4 + 32 + 1 + 64 = 101 */
    if (hash_len != 101) {
        return false;
    }

    if (stored_hash[HASH_PREFIX_LEN + SALT_HEX_LEN] != '$') {
        return false;
    }

    /* Extract salt bytes from hex */
    char salt_hex[SALT_HEX_LEN + 1];
    memcpy(salt_hex, stored_hash + HASH_PREFIX_LEN, SALT_HEX_LEN);
    salt_hex[SALT_HEX_LEN] = '\0';

    unsigned char salt_bytes[16];
    for (int i = 0; i < 16; i++) {
        unsigned int byte_val = 0;
        if (sscanf(salt_hex + (i * 2), "%2x", &byte_val) != 1) {
            return false;
        }
        salt_bytes[i] = (unsigned char)byte_val;
    }

    /* Compute hash with extracted salt */
    char computed_hex[DIGEST_HEX_LEN + 1];
    if (compute_hash(password, salt_bytes, sizeof(salt_bytes), computed_hex) != 0) {
        return false;
    }

    /* Compare digest portion (constant-time comparison) */
    const char *stored_digest = stored_hash + HASH_PREFIX_LEN + SALT_HEX_LEN + 1;
    int diff = 0;
    for (int i = 0; i < DIGEST_HEX_LEN; i++) {
        diff |= (computed_hex[i] ^ stored_digest[i]);
    }
    return (diff == 0);
}

bool rc_auth_validate_email(const char *email)
{
    assert(email != NULL);

    size_t len = strlen(email);
    if (len < 5 || len > 254) {
        return false;
    }

    /* Find @ symbol */
    const char *at = strchr(email, '@');
    if (at == NULL) {
        return false;
    }

    /* Must have chars before and after @ */
    if (at == email || at[1] == '\0') {
        return false;
    }

    /* Must have a dot after @ */
    const char *dot = strchr(at + 1, '.');
    if (dot == NULL || dot[1] == '\0') {
        return false;
    }

    return true;
}

bool rc_auth_validate_password(const char *password)
{
    assert(password != NULL);

    size_t len = strlen(password);
    return (len >= 8 && len <= 128);
}
