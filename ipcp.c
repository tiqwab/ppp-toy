#include <stdio.h>
#include <string.h>

#include "frame.h"
#include "ipcp.h"
#include "lcp.h"

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
int process_ipcp_ip_address(struct ipcp_ip_address *req, struct configure_request *conf_req, int fd) {
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
            conf_req.options = (char *) &frame.information[5];

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
