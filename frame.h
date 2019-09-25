#ifndef __FRAME_H__
#define __FRAME_H__

#include <sys/types.h>

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

#endif /* __FRAME_H__ */
