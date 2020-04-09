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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 11/21/2012
 */

#pragma once

#ifndef __MERO_COB_NS_ITER_H__
#define __MERO_COB_NS_ITER_H__

#include "cob/cob.h"

/**
 * @defgroup cob_fid_ns_iter Cob-fid namespace iterator
 *
 * The cob on data server has cob nskey = <gob_fid, cob_index>,
 * where,
 * gob_fid   : global file identifier corresponding to which the cob is
 *             being created.
 * cob_index : unique index of the cob in the pool.
 *
 * @see m0_cob_nskey
 *
 * The cob-fid iterator uniquely iterates over gob_fids, thus skipping entries
 * with same gob_fids but different cob_index.
 *
 * This iterator is used in SNS repair iterator. @see m0_sns_repair_iter
 *
 * @{
 */

struct m0_cob_fid_ns_iter {
	/** Cob domain. */
	struct m0_cob_domain *cni_cdom;

	struct m0_be_btree_cursor  cni_it;
	/** Last fid value returned. */
	struct m0_fid         cni_last_fid;
};

/**
 * Initialises the namespace iterator.
 * @param iter - Cob fid namespace iterator that is to be initialised.
 * @param gfid - Initial gob-fid with which iterator is initialised.
 * @param dbenv - DB environment from which the records should be extracted.
 * @param cdom - Cob domain.
 */
M0_INTERNAL int m0_cob_ns_iter_init(struct m0_cob_fid_ns_iter *iter,
				    struct m0_fid *gfid,
				    struct m0_cob_domain *cdom);

/**
 * Iterates over namespace to point to unique gob fid in the namespace.
 * @param iter - Pointer to the namespace iterator.
 * @param tx - Database transaction used for DB operations by iterator.
 * @param gfid - Next unique gob-fid in the iterator. This is output variable.
 */
M0_INTERNAL int m0_cob_ns_iter_next(struct m0_cob_fid_ns_iter *iter,
				    struct m0_fid *gfid,
				    struct m0_cob_nsrec **nsrec);

M0_INTERNAL int m0_cob_ns_rec_of(struct m0_be_btree *cob_namespace,
				 const struct m0_fid *key_gfid,
				 struct m0_fid *next_gfid,
				 struct m0_cob_nsrec **nsrec);

/**
 * Finalises the namespace iterator.
 * @param iter - Namespace iterator that is to be finalised.
 */
M0_INTERNAL void m0_cob_ns_iter_fini(struct m0_cob_fid_ns_iter *iter);

/** @} end group cob_fid_ns_iter */

#endif    /* __MERO_COB_NS_ITER_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
