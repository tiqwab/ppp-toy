#ifndef __IPCP_H__
#define __IPCP_H__

#include <stdbool.h>
#include <sys/types.h>

#include "id.h"

#define IPCP_BUF_SIZ 4096
#define IPCP_ADDRESS_LEN 32

extern char src_addr[IPCP_ADDRESS_LEN];
extern char dst_addr[IPCP_ADDRESS_LEN];
extern bool ipcp_negotiated;

/*
 * RFC 1172 5.1. IP-Addresses.
 *
 * This is deprecated in RFC 1332.
 */
struct ipcp_ip_addresses {
    u_int8_t type; // 1
    u_int8_t length;
    unsigned char src_address[4]; // stored in big-endian order
    unsigned char dst_address[4]; // stored in big-endian order
};

/*
 * RFC 1332 3.3. IP-Address.
 */
struct ipcp_ip_address {
    u_int8_t type; // 3
    u_int8_t length;
    unsigned char address[4]; // stored in big-endian order
};

char *generate_ip();

/*
 * Return 0 if success, otherwise -1.
 */
int process_ipcp(char *raw, size_t raw_len, int fd);

#endif /* __IPCP_H__ */
