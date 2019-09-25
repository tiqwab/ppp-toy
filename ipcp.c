#include <stdio.h>
#include <string.h>

#include "frame.h"
#include "ipcp.h"

/*
 * Return 0 if success, otherwise -1.
 */
int process_ipcp_ip_addresses(struct ipcp_ip_addresses *req, struct ipcp_packet *packet, int fd) {
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
int process_ipcp_ip_address(struct ipcp_ip_address *req, struct ipcp_packet *packet, int fd) {
    fprintf(stdout,
            "This is IPCP IP-Address. type=%d, length=%d, "
            "address=%d.%d.%d.%d\n",
            req->type, req->length, req->address[0],
            req->address[1], req->address[2], req->address[3]);

    // Do nothing for now.

    return 0;
}

int process_ipcp(char *raw, size_t raw_len, int fd) {
    struct ppp_frame frame;
    struct ipcp_packet packet;
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
            packet.code = code;
            packet.id = frame.information[1];
            packet.pad_len = *((u_int16_t *) &frame.information[2]);

            type = frame.information[4];
            switch (type) {
                case 0x01:
                    // IP-Addresses
                    packet.s.ip_addresses.type = type;
                    packet.s.ip_addresses.length = frame.information[5];
                    memcpy(&packet.s.ip_addresses.src_address,
                            &frame.information[6], 4);
                    memcpy(&packet.s.ip_addresses.dst_address,
                            &frame.information[10], 4);
                    process_ipcp_ip_addresses(&packet.s.ip_addresses,
                            &packet, fd);
                    break;
                case 0x03:
                    // IP-Address
                    packet.s.ip_address.type = type;
                    packet.s.ip_address.length = frame.information[5];
                    memcpy(&packet.s.ip_address.address,
                            &frame.information[6], 4);
                    process_ipcp_ip_address(&packet.s.ip_address,
                            &packet, fd);
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
