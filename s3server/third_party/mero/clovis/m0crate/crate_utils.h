/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Ivan Alekhin <ivan.alekhin@seagate.com>
 * Original creation date: 30-May-2017
 */

#pragma once

#ifndef __MERO_CLOVIS_M0CRATE_CRATE_UTILS_H__
#define __MERO_CLOVIS_M0CRATE_CRATE_UTILS_H__

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>


/**
 * @defgroup crate_utils
 *
 * @{
 */

typedef unsigned long long bcnt_t;
unsigned long long getnum(const char *str, const char *msg);
void init_rand_generator(unsigned long long seed);
int generate_fid(int seed, unsigned long *low, unsigned long *high);
unsigned char *calc_md5sum (char *buffer, int blocksize);
void timeval_norm(struct timeval *t);
void timeval_diff(const struct timeval *start, const struct timeval *end,
                         struct timeval *diff);
void timeval_add(struct timeval *sum, struct timeval *term);
void timeval_sub(struct timeval *end, struct timeval *start);
double tsec(const struct timeval *tval);
double rate(bcnt_t items, const struct timeval *tval, int scale);
unsigned long long genrand64_int64(void);

/** @} end of crate_utils group */
#endif /* __MERO_CLOVIS_M0CRATE_CRATE_UTILS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
