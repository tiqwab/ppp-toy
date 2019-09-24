#include "lcp.h"

unsigned char magic_number[4] = {0x0a, 0x0b, 0x0c, 0x0d};

identifier generate_id() {
    static identifier id = 0x0;
    return ++id;
}

void send_lcp_packet(u_int8_t code, identifier id, u_int16_t len, char *data, size_t data_len, int fd) {
    char raw_buf[LCP_BUF_SIZ], encoded_buf[LCP_BUF_SIZ];
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
    if (data != NULL && data_len > 0) {
        memcpy(&raw_buf[idx], data, data_len);
        idx += data_len;
    }
    idx += 2; // FCS
    raw_buf[idx++] = 0x7e; // Flag at end
    calc_fcs((unsigned char *) raw_buf, idx);

    // Encode a frame (escape)
    encode_frame((unsigned char *) encoded_buf, &encoded_len, (unsigned char *) raw_buf, idx);

    write(fd, encoded_buf, encoded_len);
}

void create_configure_ack(struct configure_ack *ack, struct configure_request *req) {
    size_t options_len;

    ack->code = 2;
    ack->id = req->id;
    ack->pad_len = req->pad_len;
    options_len = LCP_LENGTH(struct configure_request, req) - 4;
    strncpy(ack->options, req->options, options_len);
}

int process_lcp_configure_request(struct configure_request *req, int fd) {
    struct configure_ack ack;
    char ack_options[LCP_BUF_SIZ];
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

int process_lcp_terminate_request(struct terminate_request *req) {
    fprintf(stdout, "This is Terminate-Request. code=%d, id=%d, length=%d\n",
            req->code, req->id, LCP_LENGTH(struct terminate_request, req));
    return 0;
}

void create_echo_reply(struct echo_reply *reply, struct echo_request *req) {
    size_t options_len;

    reply->code = 10;
    reply->id = req->id;
    reply->pad_len = req->pad_len;
    memcpy(&reply->magic, magic_number, sizeof(magic_number));
}

int process_lcp_echo_request(struct echo_request *req, int fd) {
    struct echo_reply reply;
    size_t packet_len;

    fprintf(stdout, "This is Echo-Request. code=%d, id=%d, length=%d\n",
            req->code, req->id, LCP_LENGTH(struct echo_request, req));

    create_echo_reply(&reply, req);
    packet_len = LCP_LENGTH(struct echo_reply, &reply);
    send_lcp_packet(reply.code, reply.id, packet_len, (char *) &reply.magic, 4, fd);
    return 0;
}

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
            // echo_req.data = ((char *) frame.information) + 8;
            process_lcp_echo_request(&echo_req, fd);
            break;
        default:
            fprintf(stderr, "Not yet implemented for code: %d\n", code);
            return -1;
    }

    return 0;
}

