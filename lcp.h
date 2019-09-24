#ifndef __LCP_H__
#define __LCP_H__

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "code.h"
#include "fcs.h"

#define LCP_BUF_SIZ 4096

typedef int8_t identifier;

// Length of code, id, length, and data
#define LCP_LENGTH(name, r) \
    ((*(((u_int8_t *) r) + offsetof(name, pad_len)) << 8) \
    | (*((((u_int8_t *) r) + offsetof(name, pad_len)) + 1)))

// FIXME
// magic_number should be zero at first.
// This is filled after magic-number configuration negotiation is finished.
extern unsigned char magic_number[4];

/*
 * Representation of PPP frame.
 * Some fields are omitted such as flag, padding, and FCS.
 *
 * FIXME: Should it be defined here?
 */
struct ppp_frame {
    u_int8_t address;
    u_int8_t control;
    u_int16_t protocol;
    char *information;
};

#define CONF_REQ(frame) ((struct configure_request *) (frame->information))

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

struct echo_request {
    u_int8_t code; // 9
    identifier id;
    u_int16_t pad_len;
    u_int32_t magic;
    // char *data;
};

struct echo_reply {
    u_int8_t code; // 10
    identifier id;
    u_int16_t pad_len;
    u_int32_t magic;
    // char *data;
};

identifier generate_id();

/*
 * Send a LCP packet.
 *
 * Generate a new identifier if id < 0.
 */
void send_lcp_packet(u_int8_t code, identifier id, u_int16_t len, char *data, size_t data_len, int fd);

/*
 * ack->options must be large enough (same size with req->options at least).
 */
void create_configure_ack(struct configure_ack *ack, struct configure_request *req);

/*
 * Check accepted config options and send a response.
 *
 * Return 0 if success, otherwise -1.
 */
int process_lcp_configure_request(struct configure_request *req, int fd);

/*
 * Return 0 if success, otherwise -1.
 */
int process_lcp_terminate_request(struct terminate_request *req);

void create_echo_reply(struct echo_reply *reply, struct echo_request *req);

/*
 * Check an accepted Echo-Request and send Echo-Reply.
 *
 * Return 0 if success, otherwise -1.
 */
int process_lcp_echo_request(struct echo_request *req, int fd);

/*
 * Return 0 if success, otherwise -1.
 */
int process_lcp(char *raw, size_t raw_len, int fd);

#endif /* __LCP_H__ */
