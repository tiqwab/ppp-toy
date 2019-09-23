#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "code.h"

int encode_frame(unsigned char *dst, size_t *dst_len,
        unsigned char *src, size_t src_len) {
    fprintf(stderr, "encode_frame is not yet implemented.\n");
    return -1;
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
