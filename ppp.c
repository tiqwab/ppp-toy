#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "code.h"
#include "fcs.h"
#include "lcp.h"

#define ESCAPE_SEQ 0x7d
#define BUF_SIZ 4096

#define IS_FRAME_BYTE(frame, index, expected) \
    (((unsigned char) frame[index]) == expected)
#define ISNOT_FRAME_BYTE(frame, index, expected) \
    (((unsigned char) frame[index]) != expected)

void print_frame(FILE *file, char *frame, size_t len) {
    fprintf(file, "frame: ");
    for (size_t i = 0; i < len; i++) {
        fprintf(file, "%02x ", (unsigned char) frame[i]);
    }
    fprintf(file, "\n");
}

/*
 * Return 0 if success, otherwise -1.
 */
int process_frame(char *orig_frame, size_t len, int fd) {
    char decoded_frame[BUF_SIZ];
    size_t decoded_len;

    // Decode a frame
    decode_frame((unsigned char *) decoded_frame, &decoded_len,
            (unsigned char *) orig_frame, len);
    printf("decoded frame length: %ld\n", decoded_len);
    print_frame(stdout, decoded_frame, decoded_len);

    // Check some fields before processing it
    if (ISNOT_FRAME_BYTE(decoded_frame, 0, 0x7e) // flag
            || ISNOT_FRAME_BYTE(decoded_frame, decoded_len-1, 0x7e) // flag
            || ISNOT_FRAME_BYTE(decoded_frame, 1, 0xff) // address
            || ISNOT_FRAME_BYTE(decoded_frame, 2, 0x03) // control
            ) {
        fprintf(stderr, "A frame contains some illegal fields.\n");
        return -1;
    }

    // Check FCS
    if (check_fcs(((unsigned char *) decoded_frame), decoded_len) < 0) {
        fprintf(stderr, "FCS error\n");
        return -1;
    }

    if (IS_FRAME_BYTE(decoded_frame, 3, 0xc0) && IS_FRAME_BYTE(decoded_frame, 4, 0x21)) {
        // This is LCP.
        return process_lcp(decoded_frame, decoded_len, fd);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    char *device;
    int fd, n, frame_len;
    char buf[BUF_SIZ];
    char *buf_pos;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s DEV\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    device = argv[1];

    if ((fd = open(device, O_RDWR)) < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    buf_pos = buf;

    // Send magic number.
    char magic_number_option[6];
    magic_number_option[0] = 0x05;
    magic_number_option[1] = 0x06;
    memcpy(&magic_number_option[2], magic_number, 4);
    send_lcp_packet(1, -1, 10, magic_number_option, 6, fd);

    while (1) {
        char prev_ch = -1, current_ch = -1;
        char *ptr;

        while (1) {
            if ((n = read(fd, buf_pos, 1)) < 0) {
                perror("read");
                exit(EXIT_FAILURE);
            } else if (n == 0) {
                goto teardown;
            }

            if ((current_ch = *buf_pos) != 0x7e) {
                prev_ch = current_ch;
                continue;
            }
            if (prev_ch == ESCAPE_SEQ) {
                prev_ch = current_ch;
                continue;
            }

            *buf_pos++ = current_ch; // 0x7e
            break;
        }

test:

        while (1) {
            if (read(fd, buf_pos, 1) < 0) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (*buf_pos == 0x7e) {
                ptr = buf_pos - 1;
                if (*ptr != ESCAPE_SEQ) {
                    break;
                }
            }
            buf_pos++;
        }

        frame_len = buf_pos - buf + 1;
        if (process_frame(buf, frame_len, fd) < 0) {
            fprintf(stderr, "Ignore a frame. ");
            print_frame(stderr, buf, frame_len);
        }
        buf_pos = buf;

        // Assume that frames are consecutive if the next byte is not 0x7e.
        if ((n = read(fd, buf_pos, 1)) < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        } else if (n == 0) {
            goto teardown;
        }
        if (*buf_pos != 0x7e) {
            buf_pos[1] = *buf_pos;
            *buf_pos = 0x7e;
            buf_pos += 2;
        } else {
            buf_pos++;
        }
        goto test;
    }

teardown:
    if (close(fd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
