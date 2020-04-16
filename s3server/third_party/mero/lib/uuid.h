/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Carl Braganza <carl_braganza@xyratex.com>
 * Original creation date: 10/04/2012
 */

#pragma once

#ifndef __MERO_LIB_UUID_H__
#define __MERO_LIB_UUID_H__

#include "lib/types.h" /* struct m0_uint128 */

/**
   @defgroup uuid UUID support
   @{
 */

enum {
	M0_UUID_STRLEN = 36
};

/**
   Parse the 8-4-4-4-12 hexadecimal string representation of a UUID
   and convert to numerical form.
   See <a href="http://en.wikipedia.org/wiki/Universally_unique_identifier">
   Universally unique identifier</a> for more details.
 */
M0_INTERNAL int m0_uuid_parse(const char *str, struct m0_uint128 *val);

/**
   Produce the 8-4-4-4-12 hexadecimal string representation of a UUID
   from its numerical form.
   See <a href="http://en.wikipedia.org/wiki/Universally_unique_identifier">
   Universally unique identifier</a> for more details.
   @param val The numerical UUID.
   @param buf String buffer.
   @param len Length of the buffer.
              It must be at least M0_UUID_STRLEN+1 bytes long.
 */
M0_INTERNAL void m0_uuid_format(const struct m0_uint128 *val,
				char *buf, size_t len);

M0_INTERNAL void m0_uuid_generate(struct m0_uint128 *u);

/**
 * The UUID of a Mero node.
 */
M0_EXTERN struct m0_uint128 m0_node_uuid;

void m0_kmod_uuid_file_set(const char *path);
void m0_node_uuid_string_set(const char *uuid);
int  m0_node_uuid_string_get(char buf[M0_UUID_STRLEN + 1]);

/** @} end uuid group */

#endif /* __MERO_LIB_UUID_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
