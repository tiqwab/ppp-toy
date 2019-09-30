#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "code.h"
#include "fcs.h"
#include "if.h"
#include "ipcp.h"
#include "lcp.h"
#include "tty.h"

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

    if (IS_FRAME_BYTE(decoded_frame, 3, 0xc0)
            && IS_FRAME_BYTE(decoded_frame, 4, 0x21)) {
        // This is LCP.
        return process_lcp(decoded_frame, decoded_len, fd);
    } else if (IS_FRAME_BYTE(decoded_frame, 3, 0x80)
            && IS_FRAME_BYTE(decoded_frame, 4, 0x21)) {
        // This is IPCP.
        return process_ipcp(decoded_frame, decoded_len, fd);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    char *device;
    int fd, n, frame_len;
    char buf[BUF_SIZ];
    bool ppp_device_setup = false;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s DEV\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    device = argv[1];

    if ((fd = setup_tty_and_ppp_if(device)) < 0) {
        exit(EXIT_FAILURE);
    }

    // Send magic number.
    char magic_number_option[6];
    magic_number_option[0] = 0x05;
    magic_number_option[1] = 0x06;
    memcpy(&magic_number_option[2], magic_number, 4);
    send_lcp_packet(1, -1, 10, magic_number_option, 6, fd);

    while (1) {
        if ((n = read(fd, buf, BUF_SIZ)) < 0) {
            if (errno == EINTR) {
                break;
            }
            perror("read");
            exit(EXIT_FAILURE);
        } else if (n == 0) {
            break;
        }

        frame_len = n;
        if (process_frame(buf, frame_len, fd) < 0) {
            fprintf(stderr, "Ignore a frame. ");
            print_frame(stderr, buf, frame_len);
        }

        if (ipcp_negotiated && !ppp_device_setup) {
            if (setup_ppp_if(ppp_device_name, src_addr, dst_addr) < 0) {
                fprintf(stderr, "Failed in setup_ppp_if\n");
                exit(EXIT_FAILURE);
            }
            ppp_device_setup = true;
        }
    }

    if (close(fd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
