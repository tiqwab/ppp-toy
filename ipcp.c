#include <stdio.h>
#include <string.h>

#include "frame.h"
#include "ipcp.h"
#include "lcp.h"

char src_addr[IPCP_ADDRESS_LEN];
char dst_addr[IPCP_ADDRESS_LEN];
bool ipcp_negotiated = false;

char *generate_ip() {
    // Return the same IP for now.
    static char ip[4] = {192, 168, 11, 4};
    return ip;
}

/*
 * Almost same as send_lcp_packet except for Protocol.
 */
void send_ipcp_packet(u_int8_t code, identifier id, u_int16_t len,
        char *data, size_t data_len, int fd) {
    char raw_buf[IPCP_BUF_SIZ], encoded_buf[IPCP_BUF_SIZ];
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
    raw_buf[idx++] = 0x80; // Protocol
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

/*
 * Return 0 if success, otherwise -1.
 */
int process_ipcp_ip_addresses(struct ipcp_ip_addresses *req, struct configure_request *conf_req, int fd) {
    fprintf(stdout,
            "This is IPCP IP-Address. type=%d, length=%d, "
            "src_address=%d.%d.%d.%d, dst_address=%d.%d.%d.%d\n",
            req->type, req->length,
            req->src_address[0], req->src_address[1],
            req->src_address[2], req->src_address[3],
            req->dst_address[0], req->dst_address[1],
            req->dst_address[2], req->dst_address[3]);

    // Do nothing for now.

    return 0;
}

/*
 * Return 0 if success, otherwise -1.
 */
int process_ipcp_ip_address(struct ipcp_ip_address *req, struct configure_request *conf_req_received, int fd) {
    struct configure_ack ack;
    char options[IPCP_BUF_SIZ];
    char *addr;
    size_t packet_len, options_len;

    snprintf(dst_addr, IPCP_ADDRESS_LEN, "%d.%d.%d.%d",
            (unsigned char) req->address[0], (unsigned char) req->address[1],
            (unsigned char) req->address[2], (unsigned char) req->address[3]);

    fprintf(stdout,
            "This is IPCP IP-Address. type=%d, length=%d, "
            "address=%s\n",
            req->type, req->length, dst_addr);

    // Set source IP
    addr = generate_ip();
    snprintf(src_addr, IPCP_ADDRESS_LEN, "%d.%d.%d.%d",
            (unsigned char) addr[0], (unsigned char) addr[1],
            (unsigned char) addr[2], (unsigned char) addr[3]);

    // Send my IP to peer.
    options[0] = 3;
    options[1] = 6;
    memcpy(&options[2], addr, 4);
    packet_len = 10;
    options_len = 6;
    send_ipcp_packet(1, -1, packet_len, options, options_len, fd);

    // Send ack.
    ack.options = options;
    create_configure_ack(&ack, conf_req_received);
    packet_len = LCP_LENGTH(struct configure_ack, &ack);
    options_len = packet_len - 4;
    send_ipcp_packet(ack.code, ack.id, packet_len, options,
            options_len, fd);

    ipcp_negotiated = true;

    return 0;
}

int process_ipcp(char *raw, size_t raw_len, int fd) {
    struct ppp_frame frame;
    struct configure_request conf_req;
    struct ipcp_ip_addresses conf_addresses;
    struct ipcp_ip_address conf_address;
    u_int8_t code, type;

    frame.address = raw[1];
    frame.control = raw[2];
    frame.protocol = (((u_int16_t) raw[4]) << 8) | ((u_int16_t) raw[3]);
    frame.information = &raw[5];

    code = (u_int8_t) frame.information[0];
    switch (code) {
        case 0x01:
            // Configure-Request
            conf_req.code = code;
            conf_req.id = frame.information[1];
            conf_req.pad_len = *((u_int16_t *) &frame.information[2]);
            conf_req.options = (char *) &frame.information[4];

            type = frame.information[4];
            switch (type) {
                case 0x01:
                    // IP-Addresses
                    conf_addresses.type = type;
                    conf_addresses.length = frame.information[5];
                    memcpy(&conf_addresses.src_address,
                            &frame.information[6], 4);
                    memcpy(&conf_addresses.dst_address,
                            &frame.information[10], 4);
                    process_ipcp_ip_addresses(&conf_addresses,
                            &conf_req, fd);
                    break;
                case 0x03:
                    // IP-Address
                    conf_address.type = type;
                    conf_address.length = frame.information[5];
                    memcpy(&conf_address.address,
                            &frame.information[6], 4);
                    process_ipcp_ip_address(&conf_address,
                            &conf_req, fd);
                    break;
                default:
                    fprintf(stderr,
                            "Not yet implemented for IPCP type: %d\n", type);
            }
            break;
        default:
            fprintf(stderr,
                    "Not yet implemented for IPCP code: %d\n", code);
            return -1;
    }

    return 0;
}
