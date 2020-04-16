/*
 * DERIVED FROM the stringencoders library's modp_numtoa
 *
 * Copyright ; 2007, Nick Galbreath -- nickg [at] client9 [dot] com
 * All rights reserved.
 * http://code.google.com/p/stringencoders/
 * Released under the MIT license.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "evhtp-internal.h"
#include "evhtp_numtoa.h"

static inline void
strreverse(char * begin, char * end) {
    char aux;

    while (end > begin) {
        aux = *end, *end-- = *begin, *begin++ = aux;
    }
}

size_t
evhtp_modp_u64toa(uint64_t value, char * str) {
    char * wstr = str;

    /* Conversion. Number is reversed. */
    do {
        *wstr++ = (char)(48 + (value % 10));
    } while (value /= 10);

    *wstr = '\0';

    /* Reverse string */
    strreverse(str, wstr - 1);

    return (size_t)(wstr - str);
}

size_t
evhtp_modp_u32toa(uint32_t value, char * str) {
    char * wstr = str;

    /* Conversion. Number is reversed. */
    do {
        *wstr++ = (char)(48 + (value % 10));
    } while (value /= 10);

    *wstr = '\0';

    /* Reverse string */
    strreverse(str, wstr - 1);

    return (size_t)(wstr - str);
}

inline size_t
evhtp_modp_sizetoa(size_t value, char * str) {
#if EVHTP_SYS_ARCH == 64
    return evhtp_modp_u64toa(value, str);
#elif EVHTP_SYS_ARCH == 32
    return evhtp_modp_u32toa(value, str);
#else
#warn "UNKNOWN ARCH"
#endif
}

