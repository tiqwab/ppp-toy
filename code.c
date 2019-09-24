#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "code.h"

int encode_frame(unsigned char *dst, size_t *dst_len,
        unsigned char *src, size_t src_len) {
    size_t i = 0, j = 0;

    // Flag at the beginning.
    dst[j++] = src[i++];

    // Skip flags at the beginning and the end of the frame.
    for ( ; i < src_len - 1; i++) {
        if (src[i] == 0x7e || src[i] == 0x7d || src[i] < 0x1f) {
            dst[j++] = 0x7d;
            dst[j++] = src[i] ^ 0x20;
        } else {
            dst[j++] = src[i];
        }
    }

    // Flag at the end.
    dst[j++] = src[i++];

    *dst_len = j;
    return 0;
}

int decode_frame(unsigned char *dst, size_t *dst_len,
        unsigned char *src, size_t src_len) {
    size_t i, j = 0;
    bool is_escaped = false;
    unsigned char c;

    for (i = 0; i < src_len; i++) {
        if (is_escaped) {
            c = src[i] ^ 0x20;
            is_escaped = false;
        } else {
            if (src[i] == 0x7d) {
                is_escaped = true;
                continue;
            }
            c = src[i];
        }
        dst[j++] = c;
    }

    if (dst_len != NULL) {
        *dst_len = j;
    }

    return 0;
}
