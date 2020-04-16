/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Abrarahmed Momin  <abrar.habib@seagate.com>
 * Original creation date: 23th-June-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_FI_COMMON_H__
#define __S3_SERVER_S3_FI_COMMON_H__

#ifdef __cplusplus
#define EXTERN_C_BLOCK_BEGIN extern "C" {
#define EXTERN_C_BLOCK_END }
#define EXTERN_C_FUNC extern "C"
#else
#define EXTERN_C_BLOCK_BEGIN
#define EXTERN_C_BLOCK_END
#define EXTERN_C_FUNC
#endif

EXTERN_C_BLOCK_BEGIN
#include "lib/finject.h"

#ifdef HAVE_CONFIG_H
#include "config.h" /* ENABLE_FAULT_INJECTION */
#endif

typedef struct m0_fi_fault_point s3_fp;
#define S3_FI_FUNC_NAME "s3_fi_is_enabled"
#define S3_MODULE_NAME "UNKNOWN"

#ifdef ENABLE_FAULT_INJECTION
#include "lib/finject_internal.h"

/**
 * Creates and checks fault point in code, identified by "tag"
 *
 * @param tag  FP tag, which can be activated using s3_fi_enable* routines
 *
 * eg: s3_fi_is_enabled("write_fail");
 *
 */
int s3_fi_is_enabled(const char *tag);

/**
 * Enables fault point, which identified by "tag"
 *
 * @param tag  FP tag, which was specified as a parameter to s3_fi_is_enabled()
 *
 * eg: s3_fi_enable("write_fail");
 *
 */
void s3_fi_enable(const char *tag);

/**
 * Enables fault point, which identified by "tag".
 * Fault point is triggered only once.
 *
 * @param tag  FP tag, which was specified as a parameter to s3_fi_is_enabled()
 *
 * eg: s3_fi_enable("write_fail");
 *
 */
void s3_fi_enable_once(const char *tag);

/**
 * Enables fault point, which identified by "tag"
 *
 * @param tag  FP tag, which was specified as a parameter to s3_fi_is_enabled()
 * @param p    Integer number in range [1..100], which means a probability in
 *             percents, with which FP should be triggered on each hit
 * eg: s3_fi_enable("write_fail", 10);
 *
 */
void s3_fi_enable_random(const char *tag, uint32_t p);

/**
 * Enables fault point, which identified by "tag"
 *
 * @param tag  FP tag, which was specified as a parameter to s3_fi_is_enabled()
 * @param n    A "frequency" with which FP is triggered
 * eg: s3_fi_enable("write_fail", 5);
 * Here fault injection is triggered first 5 times.
 *
 */
void s3_fi_enable_each_nth_time(const char *tag, uint32_t n);

/**
 * Enables fault point, which identified by "tag"
 *
 * @param tag  FP tag, which was specified as a parameter to s3_fi_is_enabled()
 * @param n    Integer values, means 'skip triggering N times in a row and then
 *trigger
 *             M times in a row
 * @param m    Integer values, means M times in a row to trigger FP, after
 *skipping it
 *             N times before
 * eg: s3_fi_enable("write_fail", 5, 20);
 * Here FI is not trigerred first 5 times then FI is triggered next 20 times,
 * this cycle is repeated.
 */
void s3_fi_enable_off_n_on_m(const char *tag, uint32_t n, uint32_t m);

/**
 * Enables fault point, which identified by "tag"
 *
 * @param tag  FP tag, which was specified as a parameter to s3_fi_is_enabled()

 * eg: s3_fi_enable("write_fail", 10, 200);
 *
 */
void s3_fi_disable(const char *fp_tag);

#else /* ENABLE_FAULT_INJECTION */

inline int s3_fi_is_enabled(const char *tag) { return false; }

inline void s3_fi_enable(const char *tag) {}

inline void s3_fi_enable_once(const char *tag) {}

inline void s3_fi_enable_random(const char *tag, uint32_t p) {}

inline void s3_fi_enable_each_nth_time(const char *tag, uint32_t n) {}

inline void s3_fi_enable_off_n_on_m(const char *tag, uint32_t n, uint32_t m) {}

inline void s3_fi_disable(const char *fp_tag) {}

#endif /* ENABLE_FAULT_INJECTION */

EXTERN_C_BLOCK_END

#endif /*  __S3_SERVER_S3_FI_COMMON_H__ */
