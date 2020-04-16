#ifndef __EVHTP_NUMTOA_H__
#define __EVHTP_NUMTOA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "evhtp-config.h"

/**
 * @brief based on the system architecture, convert a size_t
 *        number to a string.
 *
 * @param value the input value
 * @param str The output buffer, should be 24 chars or more.
 *
 * @return
 */
EVHTP_EXPORT size_t evhtp_modp_sizetoa(size_t value, char * str);

/**
 * @brief converts uint32_t value to string
 *
 * @param value input value
 * @param str output buffer, should be 16 chars or more
 *
 * @return
 */
EVHTP_EXPORT size_t evhtp_modp_u32toa(uint32_t value, char * str);


/**
 * @brief convert uint64_t value to a string
 *
 * @param value input value
 * @param str output buffer, should be 24 chars or more
 *
 * @return
 */
EVHTP_EXPORT size_t evhtp_modp_u64toa(uint64_t value, char * str);

#define evhtp_modp_uchartoa(_val) (unsigned char)('0' + _val)

#ifdef __cplusplus
}
#endif

#endif

