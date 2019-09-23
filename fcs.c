#include <assert.h>
#include <stdio.h>

#include "fcs.h"

#define PPPINITFCS16 0xffff /* Initial FCS value */
#define PPPGOODFCS16 0xf0b8 /* Good final FCS value */

/*
 * Calculate a new fcs given the current fcs and the new data.
 */
u16 pppfcs16(u16 fcs, unsigned char *cp, int len) {
    assert(sizeof(u16) == 2);
    assert(((u16) -1) > 0);
    while (len--) {
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
    }
    return fcs;
}

int calc_fcs(unsigned char *frame, size_t len) {
    u16 trialfcs;
    unsigned char *cp;

    // FCS is calculated for Address, Control, Protocol, Information, and Padding
    cp = frame + 1;
    len -= 4; // 2 * flag (1 byte) + fcs (2 bytes)

    /* add on output */
    trialfcs = pppfcs16(PPPINITFCS16, cp, len);
    trialfcs ^= 0xffff; // complement

    cp[len] = (trialfcs & 0x00ff); // least significant byte first
    cp[len+1] = ((trialfcs >> 8) & 0x00ff);

    // check on input
    trialfcs = pppfcs16(PPPINITFCS16, cp, len + 2);
    if (trialfcs != PPPGOODFCS16) {
        fprintf(stderr, "Something wrong with calculate FCS\n");
        return -1;
    }

    return 0;
}

int check_fcs(unsigned char *frame, size_t len) {
    u16 trialfcs;
    unsigned char *cp;

    // FCS is calculated for Address, Control, Protocol, Information, and Padding
    cp = frame + 1;
    len -= 4; // 2 * flag (1 byte) + fcs (2 bytes)

    /* add on output */
    trialfcs = pppfcs16(PPPINITFCS16, cp, len);
    trialfcs ^= 0xffff; // complement

    printf("%02x, %02x\n", cp[len], cp[len+1]);
    printf("%02x, %02x\n", (trialfcs & 0x00ff), ((trialfcs >> 8) & 0x00ff));
    // cp[len] = (trialfcs & 0x00ff); // least significant byte first
    // cp[len+1] = ((trialfcs >> 8) & 0x00ff);

    // check on input
    trialfcs = pppfcs16(PPPINITFCS16, cp, len + 2);
    if (trialfcs != PPPGOODFCS16) {
        fprintf(stderr, "Something wrong with calculate FCS\n");
        return -1;
    }

    return 0;
}
