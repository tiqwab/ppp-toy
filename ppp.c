#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define ESCAPE_SEQ 0x7d
#define BUF_SIZ 1024

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

#define CONF_REQ(frame) ((struct config_request *) (frame->information))

struct config_request {
    u_int8_t code;
    identifier id;
    u_int16_t length;
    char *options;
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

void process_frame(char *frame, size_t len) {
    // just print in hexdecimal format for now
    size_t i;

    printf("frame: ");
    for (i = 0; i < len; i++) {
        printf("%x ", (unsigned char) frame[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    char *device;
    int fd, n;
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

        process_frame(buf, buf_pos - buf + 1);
        buf_pos = buf;
    }

teardown:
    if (close(fd) < 0) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
