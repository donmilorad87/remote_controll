#include "jwt.h"
#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <jansson.h>

/* Base64URL encoding (no padding) */
static int base64url_encode(const unsigned char *input, size_t input_len,
                            char *output, size_t output_size)
{
    assert(input != NULL);
    assert(output != NULL);

    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    if (b64 == NULL || mem == NULL) {
        BIO_free(b64);
        BIO_free(mem);
        return -1;
    }

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, mem);

    BIO_write(b64, input, (int)input_len);
    BIO_flush(b64);

    BUF_MEM *buf_ptr = NULL;
    BIO_get_mem_ptr(b64, &buf_ptr);

    if ((size_t)buf_ptr->length >= output_size) {
        BIO_free_all(b64);
        return -1;
    }

    /* Convert base64 to base64url: +→-, /→_, remove = padding */
    size_t j = 0;
    for (size_t i = 0; i < (size_t)buf_ptr->length && j < output_size - 1; i++) {
        char c = buf_ptr->data[i];
        if (c == '+') {
            output[j++] = '-';
        } else if (c == '/') {
            output[j++] = '_';
        } else if (c != '=') {
            output[j++] = c;
        }
    }
    output[j] = '\0';

    BIO_free_all(b64);
    return 0;
}

static int base64url_decode(const char *input, unsigned char *output,
                            size_t output_size, size_t *decoded_len)
{
    assert(input != NULL);
    assert(output != NULL);

    size_t input_len = strlen(input);
    /* Convert base64url back to base64 */
    size_t padded_len = input_len + (4 - (input_len % 4)) % 4;
    char b64_buf[4096];

    if (padded_len >= sizeof(b64_buf)) {
        return -1;
    }

    for (size_t i = 0; i < input_len; i++) {
        if (input[i] == '-') {
            b64_buf[i] = '+';
        } else if (input[i] == '_') {
            b64_buf[i] = '/';
        } else {
            b64_buf[i] = input[i];
        }
    }
    for (size_t i = input_len; i < padded_len; i++) {
        b64_buf[i] = '=';
    }
    b64_buf[padded_len] = '\0';

    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new_mem_buf(b64_buf, (int)padded_len);
    if (b64 == NULL || mem == NULL) {
        BIO_free(b64);
        BIO_free(mem);
        return -1;
    }

    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    b64 = BIO_push(b64, mem);

    int read_len = BIO_read(b64, output, (int)output_size);
    BIO_free_all(b64);

    if (read_len < 0) {
        return -1;
    }

    if (decoded_len != NULL) {
        *decoded_len = (size_t)read_len;
    }
    return 0;
}

int rc_jwt_create(const char *secret, const char *user_id,
                  const char *device_type, uint32_t expiry_seconds,
                  char *token_out, size_t token_out_size,
                  char *jwt_id_out, size_t jwt_id_out_size)
{
    assert(secret != NULL && secret[0] != '\0');
    assert(user_id != NULL);
    assert(device_type != NULL);
    assert(token_out != NULL);
    assert(jwt_id_out != NULL);

    if (jwt_id_out_size <= RC_JWT_ID_LEN) {
        return -1;
    }

    time_t now = time(NULL);

    /* Generate JWT ID */
    if (rc_random_hex(jwt_id_out, RC_JWT_ID_LEN) != 0) {
        return -1;
    }

    /* Build header */
    const char *header_json = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    char header_b64[256];
    if (base64url_encode((const unsigned char *)header_json,
                         strlen(header_json), header_b64, sizeof(header_b64)) != 0) {
        return -1;
    }

    /* Build payload */
    json_t *payload = json_object();
    if (payload == NULL) {
        return -1;
    }
    json_object_set_new(payload, "sub", json_string(user_id));
    json_object_set_new(payload, "dev", json_string(device_type));
    json_object_set_new(payload, "jti", json_string(jwt_id_out));
    json_object_set_new(payload, "iat", json_integer(now));
    json_object_set_new(payload, "exp", json_integer(now + expiry_seconds));

    char *payload_str = json_dumps(payload, JSON_COMPACT);
    json_decref(payload);
    if (payload_str == NULL) {
        return -1;
    }

    char payload_b64[1024];
    int enc_ret = base64url_encode((const unsigned char *)payload_str,
                                   strlen(payload_str), payload_b64, sizeof(payload_b64));
    free(payload_str);
    if (enc_ret != 0) {
        return -1;
    }

    /* Create signing input: header.payload */
    char signing_input[2048];
    int si_len = snprintf(signing_input, sizeof(signing_input), "%s.%s",
                          header_b64, payload_b64);
    if (si_len < 0 || (size_t)si_len >= sizeof(signing_input)) {
        return -1;
    }

    /* HMAC-SHA256 signature */
    unsigned char hmac_result[32];
    unsigned int hmac_len = 0;
    HMAC(EVP_sha256(), secret, (int)strlen(secret),
         (const unsigned char *)signing_input, (size_t)si_len,
         hmac_result, &hmac_len);

    char sig_b64[128];
    if (base64url_encode(hmac_result, hmac_len, sig_b64, sizeof(sig_b64)) != 0) {
        return -1;
    }

    /* Final token: header.payload.signature */
    int token_len = snprintf(token_out, token_out_size, "%s.%s.%s",
                             header_b64, payload_b64, sig_b64);
    if (token_len < 0 || (size_t)token_len >= token_out_size) {
        return -1;
    }

    return 0;
}

int rc_jwt_verify(const char *secret, const char *token, rc_jwt_claims_t *claims_out)
{
    assert(secret != NULL);
    assert(token != NULL);
    assert(claims_out != NULL);

    memset(claims_out, 0, sizeof(*claims_out));

    /* Split token into 3 parts: header.payload.signature */
    char token_copy[RC_JWT_MAX_LEN];
    rc_strlcpy(token_copy, token, sizeof(token_copy));

    char *dot1 = strchr(token_copy, '.');
    if (dot1 == NULL) return -1;
    *dot1 = '\0';
    char *payload_b64 = dot1 + 1;

    char *dot2 = strchr(payload_b64, '.');
    if (dot2 == NULL) return -1;
    *dot2 = '\0';
    char *sig_b64 = dot2 + 1;

    char *header_b64 = token_copy;

    /* Verify signature */
    char signing_input[2048];
    int si_len = snprintf(signing_input, sizeof(signing_input), "%s.%s",
                          header_b64, payload_b64);
    if (si_len < 0 || (size_t)si_len >= sizeof(signing_input)) {
        return -1;
    }

    unsigned char expected_hmac[32];
    unsigned int hmac_len = 0;
    HMAC(EVP_sha256(), secret, (int)strlen(secret),
         (const unsigned char *)signing_input, (size_t)si_len,
         expected_hmac, &hmac_len);

    char expected_sig_b64[128];
    if (base64url_encode(expected_hmac, hmac_len, expected_sig_b64,
                         sizeof(expected_sig_b64)) != 0) {
        return -1;
    }

    /* Constant-time comparison */
    size_t sig_len = strlen(sig_b64);
    size_t exp_len = strlen(expected_sig_b64);
    if (sig_len != exp_len) {
        return -1;
    }
    int diff = 0;
    for (size_t i = 0; i < sig_len; i++) {
        diff |= (sig_b64[i] ^ expected_sig_b64[i]);
    }
    if (diff != 0) {
        return -1;
    }

    /* Decode payload */
    unsigned char payload_raw[2048];
    size_t payload_len = 0;
    if (base64url_decode(payload_b64, payload_raw, sizeof(payload_raw) - 1,
                         &payload_len) != 0) {
        return -1;
    }
    payload_raw[payload_len] = '\0';

    /* Parse JSON payload */
    json_error_t err;
    json_t *payload = json_loads((const char *)payload_raw, 0, &err);
    if (payload == NULL) {
        return -1;
    }

    const char *sub = json_string_value(json_object_get(payload, "sub"));
    const char *dev = json_string_value(json_object_get(payload, "dev"));
    const char *jti = json_string_value(json_object_get(payload, "jti"));
    json_int_t iat = json_integer_value(json_object_get(payload, "iat"));
    json_int_t exp = json_integer_value(json_object_get(payload, "exp"));

    if (sub == NULL || dev == NULL || jti == NULL || exp == 0) {
        json_decref(payload);
        return -1;
    }

    /* Check expiration */
    time_t now = time(NULL);
    if (now > (time_t)exp) {
        json_decref(payload);
        return -1;
    }

    rc_strlcpy(claims_out->user_id, sub, sizeof(claims_out->user_id));
    rc_strlcpy(claims_out->device_type, dev, sizeof(claims_out->device_type));
    rc_strlcpy(claims_out->jwt_id, jti, sizeof(claims_out->jwt_id));
    claims_out->issued_at = (int64_t)iat;
    claims_out->expires_at = (int64_t)exp;

    json_decref(payload);
    return 0;
}
