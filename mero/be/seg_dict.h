/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 *		    Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 24-Aug-2013
 */

#pragma once

#ifndef __MERO_BE_SEG_DICT_H__
#define __MERO_BE_SEG_DICT_H__

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

/*
 * Design highlights:
 * - bh_dict (BE list) contains entries sorted by keys. This fact is used in
 *   the iterator.
 * - XXX Seg_dict is not thread-safe. For users that access seg_dict during
 *   mkfs/init process, it is not a problem, because they're serialised by
 *   serial mero/setup. But we should be aware of the next use-cases:
 *     1. Multiple instances of cm/ag_store may modify/lookup seg_dict
 *        concurrently.
 *     2. Ad stob domains may be created/destroyed concurrently with
 *        cm/ag_store. See m0_storage_dev.
 * - XXX Seg dict iterator returns pointers to potentially unpinned memory. It
 *   may cause problems with `this_key` strings.
 */

struct m0_be_tx;
struct m0_be_seg;
struct m0_be_tx_credit;

M0_INTERNAL void m0_be_seg_dict_init(struct m0_be_seg *seg);
M0_INTERNAL void m0_be_seg_dict_fini(struct m0_be_seg *seg);
M0_INTERNAL int m0_be_seg_dict_lookup(struct m0_be_seg  *seg,
				      const char        *name,
				      void             **out);
M0_INTERNAL int m0_be_seg_dict_begin(struct m0_be_seg  *seg,
				     const char        *start_key,
				     const char       **this_key,
				     void             **this_rec);
M0_INTERNAL int m0_be_seg_dict_next(struct m0_be_seg  *seg,
				    const char        *prefix,
				    const char        *start_key,
				    const char       **this_key,
				    void             **this_rec);

/* tx based dictionary interface */
M0_INTERNAL int m0_be_seg_dict_insert(struct m0_be_seg *seg,
				      struct m0_be_tx  *tx,
				      const char       *name,
				      void             *value);
M0_INTERNAL int m0_be_seg_dict_delete(struct m0_be_seg *seg,
				      struct m0_be_tx  *tx,
				      const char       *name);
M0_INTERNAL void m0_be_seg_dict_create(struct m0_be_seg *seg,
				       struct m0_be_tx  *tx);
M0_INTERNAL void m0_be_seg_dict_destroy(struct m0_be_seg *seg,
					struct m0_be_tx  *tx);

M0_INTERNAL void m0_be_seg_dict_create_credit(struct m0_be_seg       *seg,
					      struct m0_be_tx_credit *accum);
M0_INTERNAL void m0_be_seg_dict_destroy_credit(struct m0_be_seg       *seg,
					       struct m0_be_tx_credit *accum);
M0_INTERNAL void m0_be_seg_dict_insert_credit(struct m0_be_seg       *seg,
					      const char             *name,
					      struct m0_be_tx_credit *accum);
M0_INTERNAL void m0_be_seg_dict_delete_credit(struct m0_be_seg       *seg,
					      const char             *name,
					      struct m0_be_tx_credit *accum);

/** @} end of be group */
#endif /* __MERO_BE_SEG_DICT_H__ */

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
