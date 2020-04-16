/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 28-Apr-2016
 */

#pragma once

#ifndef __MERO_CLOVIS_M0CLOVIS_CLOVIS_INDEX_OP_H__
#define __MERO_CLOVIS_M0CLOVIS_CLOVIS_INDEX_OP_H__

/**
 * @defgroup clovis
 *
 * @{
 */
struct m0_clovis_realm;
struct m0_fid_arr;
struct m0_fid;
struct m0_bufvec;

int clovis_index_create(struct m0_clovis_realm *parent,
			struct m0_fid_arr *fids);
int clovis_index_drop(struct m0_clovis_realm *parent, struct m0_fid_arr *fids);
int clovis_index_list(struct m0_clovis_realm *parent,
		      struct m0_fid          *fid,
		      int                     cnt,
		      struct m0_bufvec       *keys);
int clovis_index_lookup(struct m0_clovis_realm *parent,
		        struct m0_fid_arr      *fids,
		        struct m0_bufvec       *rets);
int clovis_index_put(struct m0_clovis_realm *parent,
		     struct m0_fid_arr *fids,
		     struct m0_bufvec  *keys,
		     struct m0_bufvec  *vals);
int clovis_index_del(struct m0_clovis_realm *parent,
		     struct m0_fid_arr *fids,
		     struct m0_bufvec  *keys);
int clovis_index_get(struct m0_clovis_realm *parent,
		     struct m0_fid *fid,
		     struct m0_bufvec *keys,
		     struct m0_bufvec *vals);
int clovis_index_next(struct m0_clovis_realm *parent,
		      struct m0_fid *fid,
		      struct m0_bufvec *keys, int cnt,
		      struct m0_bufvec *vals);

/** @} end of clovis group */
#endif /* __MERO_CLOVIS_M0CLOVIS_CLOVIS_INDEX_OP_H__ */

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
