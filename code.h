#ifndef __CODE_H__
#define __CODE_H__

#include <sys/types.h>

/*
 * Encode (escaping) a frame orig to dst.
 * dst is expected to be large enough (src_len * 2 would be enough).
 * The actual length of dst is stored in dst_len if not null.
 *
 * Return 0 if success, otherwise return -1.
 */
int encode_frame(unsigned char *dst, size_t *dst_len,
        unsigned char *src, size_t src_len);

/*
 * Decode (remove escaping) a frame orig to dst.
 * dst is expected to be large enough (src_len would be enough).
 * The actual length of dst is stored in dst_len if not null.
 *
 * Return 0 if success, otherwise return -1.
 */
int decode_frame(unsigned char *dst, size_t *dst_len,
        unsigned char *src, size_t src_len);

#endif /* __CODE_H__ */
