#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "code.h"
#include "fcs.h"

#define ESCAPE_SEQ 0x7d
#define BUF_SIZ 4096

typedef u_int8_t identifier;

/*
 * Representation of PPP frame.
 * Some fields are omitted such as flag, padding, and FCS.
 */
struct ppp_frame {
    u_int8_t address;
    u_int8_t control;
    u_int16_t protocol;
    char *information;
};

#define CONF_REQ(frame) ((struct configure_request *) (frame->information))
// Length of code, id, length, and data
#define LCP_LENGTH(r) \
    ((*(((u_int8_t *) r) + offsetof(struct configure_request, pad_len)) << 8) \
    | (*((((u_int8_t *) r) + offsetof(struct configure_request, pad_len)) + 1)))

struct configure_request {
    u_int8_t code;
    identifier id;
    u_int16_t pad_len;
    char *options;
};

struct terminate_request {
    u_int8_t code;
    identifier id;
    u_int16_t pad_len;
    char *data;
};

// identifier generate_id() {
//     static identifier id = 0x0;
//     return ++id;
// }
// 
// struct echo_request {
//     u_int8_t code;
//     identifier id;
//     u_int16_t length;
//     u_int32_t magic;
//     char data[4];
// };
// 
// void create_echo_request(struct echo_request *req) {
//     req->code = 0x09;
//     req->id = generate_id();
//     req->length = 4;
//     req->magic = 0x0000;
//     strncpy(req->data, "test", 4);
// }

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
int process_lcp_configure_request(struct configure_request *req) {
    fprintf(stdout, "This is Configure-Request. code=%d, id=%d, length=%d\n",
            req->code, req->id, LCP_LENGTH(req));
    return 0;
}

/*
 * Return 0 if success, otherwise -1.
 */
int process_lcp_terminate_request(struct terminate_request *req) {
    fprintf(stdout, "This is Terminate-Request. code=%d, id=%d, length=%d\n",
            req->code, req->id, LCP_LENGTH(req));
    return 0;
}

/*
 * Return 0 if success, otherwise -1.
 */
int process_lcp(char *raw, size_t raw_len) {
    struct ppp_frame frame;
    struct configure_request *conf_req;
    struct terminate_request *term_req;
    u_int8_t code;

    frame.address = raw[1];
    frame.control = raw[2];
    frame.protocol = (((u_int16_t) raw[4]) << 8) | ((u_int16_t) raw[3]);
    frame.information = &raw[5];

    code = (u_int8_t) frame.information[0];
    switch (code) {
        case 0x01:
            conf_req = (struct configure_request *) frame.information;
            process_lcp_configure_request(conf_req);
            break;
        case 0x05:
            term_req = (struct terminate_request *) frame.information;
            process_lcp_terminate_request(term_req);
            break;
        default:
            fprintf(stderr, "Not yet implemented for code: %d\n", code);
            return -1;
    }

    return 0;
}

/*
 * Return 0 if success, otherwise -1.
 */
int process_frame(char *orig_frame, size_t len) {
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
        return process_lcp(decoded_frame, decoded_len);
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
        if (process_frame(buf, frame_len) < 0) {
            fprintf(stderr, "Ignore a frame. ");
            print_frame(stderr, buf, frame_len);
        }
        buf_pos = buf;
    }

teardown:
    if (close(fd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
