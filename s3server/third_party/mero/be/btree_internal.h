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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 8-Jun-2017
 */

#pragma once

#ifndef __MERO_BE_BTREE_INTERNAL_H__
#define __MERO_BE_BTREE_INTERNAL_H__

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

#include "format/format.h" /* m0_format_header */
#include "be/btree.h"      /* BTREE_FAN_OUT */

/* btree constants */
enum {
	KV_NR = 2 * BTREE_FAN_OUT - 1,
};

struct bt_key_val {
	void *key;
	void *val;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/* WARNING!: fields position is paramount, see node_update() */
struct m0_be_bnode {
	struct m0_format_header b_header;
	struct m0_be_bnode     *b_next;
	unsigned int            b_nr_active; /**< Number of active keys. */
	unsigned int            b_level;     /**< Level in the B-Tree. */
	bool                    b_leaf;      /**< Leaf node? */
	char                    b_pad[7];
	struct bt_key_val       b_key_vals[KV_NR];
	struct m0_be_bnode     *b_children[KV_NR + 1];
	struct m0_format_footer b_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);
M0_BASSERT(sizeof(bool) == 1);

/** @} end of be group */
#endif /* __MERO_BE_BTREE_INTERNAL_H__ */

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
