#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

static void print_error(char *cmd, char *msg) {
    fprintf(stderr, "%s. ", msg);
    perror(cmd);
}

/*
 * Setup a ppp network interface.
 * Return 0 if success, otherwise -1.
 */
int setup_ppp_if(char *device_name, char *src_addr, char *dst_addr) {
    int socket_device;
    struct ifreq req;
    struct sockaddr sa;
    struct in_addr addr;

    // (1) Open socket
    if ((socket_device = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
        print_error("socket", "Failed to open socket");
        return -1;
    }

    strncpy(req.ifr_name, device_name, IFNAMSIZ);

    // Set MTU
    // strncpy(req.ifr_name, device_name, IFNAMSIZ);
    // req.ifr_mtu = 1500;
    // if (ioctl(socket_device, SIOCSIFMTU, &req)) {
    //     print_error("ioctl", "Failed in ioctl with SIOCSIFMTU");
    //     return -1;
    // }

    // (2) Set the source address of the peer-to-peer interface (only AF_INET (IPv4))
    ((struct sockaddr_in *) &sa)->sin_family = AF_INET;
    ((struct sockaddr_in *) &sa)->sin_port = htons(0);
    addr.s_addr = inet_addr(src_addr);
    ((struct sockaddr_in *) &sa)->sin_addr = addr;
    req.ifr_addr = (struct sockaddr) sa;

    if (ioctl(socket_device, SIOCSIFADDR, &req) < 0) {
        print_error("ioctl", "Failed in ioctl with SIOCSIFADDR");
        return -1;
    }

    // (3) Set the destination address of peer-to-peer device (only AF_INET (IPv4))
    ((struct sockaddr_in *) &sa)->sin_family = AF_INET;
    ((struct sockaddr_in *) &sa)->sin_port = htons(0);
    addr.s_addr = inet_addr(dst_addr);
    ((struct sockaddr_in *) &sa)->sin_addr = addr;
    req.ifr_dstaddr = (struct sockaddr) sa;

    if (ioctl(socket_device, SIOCSIFDSTADDR, &req) < 0) {
        print_error("ioctl", "Failed in ioctl with SIOCSIFDSTADDR");
        return -1;
    }

    // Set netmask
    // ((struct sockaddr_in *) &sa)->sin_family = AF_INET;
    // ((struct sockaddr_in *) &sa)->sin_port = htons(0);
    // addr.s_addr = inet_addr("255.255.255.255");
    // ((struct sockaddr_in *) &sa)->sin_addr = addr;
    // req.ifr_netmask = (struct sockaddr) sa;

    // if (ioctl(socket_device, SIOCSIFNETMASK, &req) < 0) {
    //     print_error("ioctl", "Failed in ioctl with SIOCSIFNETMASK");
    //     return -1;
    // }

    // (4) Get and set active flags
    if (ioctl(socket_device, SIOCGIFFLAGS, &req) < 0) {
        print_error("ioctl", "Failed in ioctl with SIOCGIFFLAGS");
        return -1;
    }

    req.ifr_flags |= IFF_UP;

    if (ioctl(socket_device, SIOCSIFFLAGS, &req) < 0) {
        print_error("ioctl", "Failed in ioctl with SIOCSIFFLAGS");
        return -1;
    }

    return 0;
}
