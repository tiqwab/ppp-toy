#ifndef __FCS_H__
#define __FCS_H__

#include <sys/types.h>

#include "fcstable.h"

/*
 * Return 0 if success, otherwise -1.
 * ref. RFC 1662.
 */
int calc_fcs(unsigned char *frame, size_t len);

/*
 * Return 0 if success, otherwise -1.
 * ref. RFC 1662.
 */
int check_fcs(unsigned char *frame, size_t len);

#endif /* __FCS_H__ */
