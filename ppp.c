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

typedef int8_t identifier;

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
#define LCP_LENGTH(name, r) \
    ((*(((u_int8_t *) r) + offsetof(name, pad_len)) << 8) \
    | (*((((u_int8_t *) r) + offsetof(name, pad_len)) + 1)))

struct configure_request {
    u_int8_t code; // 1
    identifier id;
    u_int16_t pad_len;
    char *options;
};

struct configure_ack {
    u_int8_t code; // 2
    identifier id;
    u_int16_t pad_len;
    char *options;
};

struct terminate_request {
    u_int8_t code; // 5
    identifier id;
    u_int16_t pad_len;
    char *data;
};

identifier generate_id() {
    static identifier id = 0x0;
    return ++id;
}

struct echo_request {
    u_int8_t code; // 9
    identifier id;
    u_int16_t pad_len;
    u_int32_t magic;
    char *data;
};

struct echo_reply {
    u_int8_t code; // 10
    identifier id;
    u_int16_t pad_len;
    u_int32_t magic;
    char *data;
};

#define IS_FRAME_BYTE(frame, index, expected) \
    (((unsigned char) frame[index]) == expected)
#define ISNOT_FRAME_BYTE(frame, index, expected) \
    (((unsigned char) frame[index]) != expected)

// FIXME
// magic_number should be zero at first.
// This is filled after magic-number configuration negotiation is finished.
static unsigned char magic_number[4] = {0x0a, 0x0b, 0x0c, 0x0d};

/*
 * Send a LCP packet.
 *
 * Generate a new identifier if id < 0.
 */
void send_lcp_packet(u_int8_t code, identifier id, u_int16_t len, char *data, size_t data_len, int fd) {
    char raw_buf[BUF_SIZ], encoded_buf[BUF_SIZ];
    size_t encoded_len;
    int idx = 0;

    // Check id
    if (id < 0) {
        id = generate_id();
    }

    // Create a frame

    raw_buf[idx++] = 0x7e; // Flag at start
    raw_buf[idx++] = 0xff; // Address
    raw_buf[idx++] = 0x03; // Control
    raw_buf[idx++] = 0xc0; // Protocol
    raw_buf[idx++] = 0x21; // Protocol
    raw_buf[idx++] = code;
    raw_buf[idx++] = id;
    raw_buf[idx++] = len >> 8;
    raw_buf[idx++] = (len & 0xff);
    memcpy(&raw_buf[idx], data, data_len);
    idx += data_len;
    idx += 2; // FCS
    raw_buf[idx++] = 0x7e; // Flag at end
    calc_fcs((unsigned char *) raw_buf, idx);

    // Encode a frame (escape)
    encode_frame((unsigned char *) encoded_buf, &encoded_len, (unsigned char *) raw_buf, idx);

    write(fd, encoded_buf, encoded_len);
}

void print_frame(FILE *file, char *frame, size_t len) {
    fprintf(file, "frame: ");
    for (size_t i = 0; i < len; i++) {
        fprintf(file, "%02x ", (unsigned char) frame[i]);
    }
    fprintf(file, "\n");
}

/*
 * ack->options must be large enough (same size with req->options at least).
 */
void create_configure_ack(struct configure_ack *ack, struct configure_request *req) {
    size_t options_len;

    ack->code = 2;
    ack->id = req->id;
    ack->pad_len = req->pad_len;
    options_len = LCP_LENGTH(struct configure_request, req) - 4;
    strncpy(ack->options, req->options, options_len);
}

/*
 * Check accepted config options and send a response.
 *
 * Return 0 if success, otherwise -1.
 */
int process_lcp_configure_request(struct configure_request *req, int fd) {
    struct configure_ack ack;
    char ack_options[BUF_SIZ];
    size_t packet_len, options_len;

    fprintf(stdout, "This is Configure-Request. code=%d, id=%d, length=%d\n",
            req->code, req->id, LCP_LENGTH(struct configure_request, req));

    ack.options = ack_options;
    create_configure_ack(&ack, req);
    packet_len = LCP_LENGTH(struct configure_ack, &ack);
    options_len = packet_len - 4;
    send_lcp_packet(ack.code, ack.id, packet_len, ack.options, options_len, fd);
    return 0;
}

/*
 * Return 0 if success, otherwise -1.
 */
int process_lcp_terminate_request(struct terminate_request *req) {
    fprintf(stdout, "This is Terminate-Request. code=%d, id=%d, length=%d\n",
            req->code, req->id, LCP_LENGTH(struct terminate_request, req));
    return 0;
}

/*
 * Return 0 if success, otherwise -1.
 */
int process_lcp_echo_request(struct echo_request *req, int fd) {
    fprintf(stdout, "This is Echo-Request. code=%d, id=%d, length=%d\n",
            req->code, req->id, LCP_LENGTH(struct echo_request, req));
    return 0;
}

/*
 * Return 0 if success, otherwise -1.
 */
int process_lcp(char *raw, size_t raw_len, int fd) {
    struct ppp_frame frame;
    struct configure_request conf_req;
    struct terminate_request term_req;
    struct echo_request echo_req;
    u_int8_t code;

    frame.address = raw[1];
    frame.control = raw[2];
    frame.protocol = (((u_int16_t) raw[4]) << 8) | ((u_int16_t) raw[3]);
    frame.information = &raw[5];

    code = (u_int8_t) frame.information[0];
    switch (code) {
        case 0x01:
            conf_req.code = code;
            conf_req.id = frame.information[1];
            conf_req.pad_len = *((u_int16_t *) &frame.information[2]);
            conf_req.options = ((char *) frame.information) + 4;
            process_lcp_configure_request(&conf_req, fd);
            break;
        case 0x05:
            term_req.code = code;
            term_req.id = frame.information[1];
            term_req.pad_len = *((u_int16_t *) &frame.information[2]);
            term_req.data = ((char *) frame.information) + 4;
            process_lcp_terminate_request(&term_req);
            break;
        case 0x09:
            echo_req.code = code;
            echo_req.id = frame.information[1];
            echo_req.pad_len = *((u_int16_t *) &frame.information[2]);
            echo_req.magic = *((u_int32_t *) &frame.information[4]);
            echo_req.data = ((char *) frame.information) + 8;
            process_lcp_echo_request(&echo_req, fd);
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

    // fixed magic number (0x0a, 0x0b, 0x0c, 0x0d) for now
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
    }

teardown:
    if (close(fd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
