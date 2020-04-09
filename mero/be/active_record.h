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
 * Original author: Anatoliy Bilenko <Anatoliy.Bilenko@seagate.com>
 * Original creation date: 31-Jan-2017
 */

#pragma once

#ifndef __MERO_BE_ACT_RECORD_H__
#define __MERO_BE_ACT_RECORD_H__

/**
 * @defgroup BE
 *
 * @{
 */

#include "be/list.h"
#include "lib/buf.h"
#include "lib/chan.h"
#include "lib/mutex.h"

enum m0_be_active_record_type {
	ART_OPEN,
	ART_CLOSE,
	ART_NORM
};

struct m0_be_active_record {
	struct m0_be_list_link			ar_link;
	uint64_t				ar_tx_id;
	enum m0_be_active_record_type		ar_rec_type;
	struct m0_be_active_record_domain      *ar_dom;
	/**
	 * Copy of tx payload inside log today used as a hack to
	 * access this payload during recovery and without need to
	 * scan log fully
	 */
	struct m0_buf				ar_payload;
	uint64_t				ar_magic;
};

struct m0_be_active_record_domain_subsystem {
	char                   rds_name[32];
	struct m0_be_list      rds_list;
	/* link into m0_be_active_record_domain::ard_list */
	struct m0_be_list_link rds_link;
	uint64_t               rds_magic;

	/* volatile fields */
	struct m0_mutex        rds_lock;
	struct m0_chan         rds_chan;
};

struct m0_be_active_record_domain {
	struct m0_be_list  ard_list;
	struct m0_be_seg  *ard_seg;
};

enum m0_be_active_record_domain_op {
	RDO_CREATE,
	RDO_DESTROY
};

enum m0_be_active_record_op {
	ARO_CREATE,
	ARO_DESTROY,
	ARO_DEL,
	ARO_ADD
};

/* ----------------------------------------------------------------------
 * struct m0_be_active_record_domain
 * ---------------------------------------------------------------------- */

M0_INTERNAL void
m0_be_active_record_domain_init(struct m0_be_active_record_domain *dom,
				struct m0_be_seg *seg);
M0_INTERNAL void
m0_be_active_record_domain_fini(struct m0_be_active_record_domain *dom);
M0_INTERNAL bool
m0_be_active_record_domain__invariant(struct m0_be_active_record_domain *dom);

#define m0_be_active_record_domain_create(dom, tx, seg, ...)		\
	m0_be_active_record_domain__create((dom), (tx), (seg),		\
					   (const struct m0_buf []){	\
					   __VA_ARGS__, M0_BUF_INIT0 })

/* @pre m0_be_active_record_domain_init() is called */
M0_INTERNAL int
m0_be_active_record_domain__create(struct m0_be_active_record_domain **dom,
				   struct m0_be_tx                    *tx,
				   struct m0_be_seg                   *seg,
				   const struct m0_buf                *path);
M0_INTERNAL int
m0_be_active_record_domain_destroy(struct m0_be_active_record_domain *dom,
				   struct m0_be_tx *tx);

M0_INTERNAL void
m0_be_active_record_domain_credit(struct m0_be_active_record_domain *dom,
				  enum m0_be_active_record_domain_op op,
				  uint8_t                            subsys_nr,
				  struct m0_be_tx_credit            *accum);

/* ----------------------------------------------------------------------
 * struct m0_be_active_record
 * ---------------------------------------------------------------------- */

M0_INTERNAL void
m0_be_active_record_init(struct m0_be_active_record        *rec,
			 struct m0_be_active_record_domain *ar_dom);
M0_INTERNAL void
m0_be_active_record_fini(struct m0_be_active_record *rec);
M0_INTERNAL bool
m0_be_active_record__invariant(struct m0_be_active_record *rec);

M0_INTERNAL int
m0_be_active_record_create(struct m0_be_active_record	    **rec,
			   struct m0_be_tx	             *tx,
			   struct m0_be_active_record_domain *ar_dom);
M0_INTERNAL int
m0_be_active_record_destroy(struct m0_be_active_record *rec,
			    struct m0_be_tx            *tx);

M0_INTERNAL void
m0_be_active_record_credit(struct m0_be_active_record  *rec,
			   enum m0_be_active_record_op  op,
			   struct m0_be_tx_credit      *accum);

M0_INTERNAL int
m0_be_active_record_add(const char		   *subsys,
			struct m0_be_active_record *rec,
			struct m0_be_tx            *tx);

M0_INTERNAL int
m0_be_active_record_del(const char		   *subsys,
			struct m0_be_active_record *rec,
			struct m0_be_tx            *tx);


/** @} end of BE group */
#endif /* __MERO_BE_ACT_RECORD_H__ */

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
