#include "utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/rand.h>

int rc_env_get(const char *name, char *buf, size_t buf_size, const char *default_val)
{
    assert(name != NULL);
    assert(buf != NULL);
    assert(buf_size > 0);

    const char *val = getenv(name);
    if (val == NULL || val[0] == '\0') {
        if (default_val != NULL) {
            rc_strlcpy(buf, default_val, buf_size);
            return 0;
        }
        return -1;
    }
    rc_strlcpy(buf, val, buf_size);
    return 0;
}

int rc_env_get_uint16(const char *name, uint16_t *out, uint16_t default_val)
{
    assert(name != NULL);
    assert(out != NULL);

    const char *val = getenv(name);
    if (val == NULL || val[0] == '\0') {
        *out = default_val;
        return 0;
    }

    char *endptr = NULL;
    long num = strtol(val, &endptr, 10);
    if (endptr == val || *endptr != '\0' || num < 0 || num > 65535) {
        return -1;
    }
    *out = (uint16_t)num;
    return 0;
}

int rc_env_get_uint32(const char *name, uint32_t *out, uint32_t default_val)
{
    assert(name != NULL);
    assert(out != NULL);

    const char *val = getenv(name);
    if (val == NULL || val[0] == '\0') {
        *out = default_val;
        return 0;
    }

    char *endptr = NULL;
    long num = strtol(val, &endptr, 10);
    if (endptr == val || *endptr != '\0' || num < 0) {
        return -1;
    }
    *out = (uint32_t)num;
    return 0;
}

int rc_random_hex(char *out, size_t hex_len)
{
    assert(out != NULL);
    assert(hex_len > 0 && hex_len <= 512);

    size_t byte_len = hex_len / 2;
    unsigned char bytes[256];

    if (byte_len > sizeof(bytes)) {
        return -1;
    }

    if (RAND_bytes(bytes, (int)byte_len) != 1) {
        return -1;
    }

    for (size_t i = 0; i < byte_len; i++) {
        snprintf(out + (i * 2), 3, "%02x", bytes[i]);
    }
    out[hex_len] = '\0';
    return 0;
}

void rc_strlcpy(char *dst, const char *src, size_t dst_size)
{
    assert(dst != NULL);
    assert(dst_size > 0);

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dst_size - 1) ? src_len : (dst_size - 1);
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

char *rc_read_file(const char *path, size_t *out_len)
{
    assert(path != NULL);

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long len = ftell(f);
    if (len < 0 || len > 10 * 1024 * 1024) { /* 10 MB max */
        fclose(f);
        return NULL;
    }

    rewind(f);

    char *buf = malloc((size_t)len + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }

    size_t read_len = fread(buf, 1, (size_t)len, f);
    fclose(f);

    buf[read_len] = '\0';

    if (out_len != NULL) {
        *out_len = read_len;
    }

    return buf;
}
