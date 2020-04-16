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

/**
 * @addtogroup BE
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "lib/string.h" 	/* m0_streq */
#include "be/active_record.h"
#include "mero/magic.h"
#include "be/op.h"
#include "be/seg.h"
#include "be/seg0.h"
#include "be/domain.h"
#include "reqh/reqh.h"
#include "module/instance.h"

M0_BE_LIST_DESCR_DEFINE(ard, "list of ar_domain_subsystem in ar_domain",
			static, struct m0_be_active_record_domain_subsystem,
			rds_link, rds_magic,
			M0_BE_ACT_REC_DOM_MAGIC, M0_BE_ACT_REC_DOM_MAGIC);
M0_BE_LIST_DEFINE(ard, static, struct m0_be_active_record_domain_subsystem);

M0_BE_LIST_DESCR_DEFINE(rds, "list of active_record in ar_domain_subsystem",
			static, struct m0_be_active_record,
			ar_link, ar_magic,
			M0_BE_ACT_REC_DOM_SUB_MAGIC, M0_BE_ACT_REC_DOM_SUB_MAGIC);
M0_BE_LIST_DEFINE(rds, static, struct m0_be_active_record);


/* ----------------------------------------------------------------------
 * m0_be_active_record_domain
 * ---------------------------------------------------------------------- */

static int active_record0_init(struct m0_be_domain *dom, const char *suffix,
			       const struct m0_buf *data);
static void active_record0_fini(struct m0_be_domain *dom, const char *suffix,
				const struct m0_buf *data);

#define BE_ACTIVE_RECORD_ID "0000"
struct m0_be_0type m0_be_active_record0 = {
	.b0_name = "M0_BE:ACTIVE_RECORD",
	.b0_init = active_record0_init,
	.b0_fini = active_record0_fini,
};

static int active_record0_init(struct m0_be_domain *dom, const char *suffix,
			       const struct m0_buf *data)
{
	struct m0_be_active_record_domain
		*adom =	*(struct m0_be_active_record_domain**)data->b_addr;
	struct m0_reqh
		*reqh = dom->bd_cfg.bc_engine.bec_reqh;
	unsigned               key;

	M0_ENTRY("suffix: %s, data: %p, adom: %p", suffix, data->b_addr, adom);

	key = m0_get()->i_actrec_dom_key;

	if (m0_reqh_lockers_get(reqh, key) == NULL)
		m0_reqh_lockers_set(reqh, key, adom);

	return M0_RC(0);
}

static void active_record0_fini(struct m0_be_domain *dom, const char *suffix,
				const struct m0_buf *data)
{
	M0_ENTRY();
	M0_LEAVE();
}

M0_INTERNAL void
m0_be_active_record_domain_init(struct m0_be_active_record_domain *dom,
				struct m0_be_seg *seg)
{
	struct m0_be_active_record_domain_subsystem *sub;

	dom->ard_seg = seg;

	sub = ard_be_list_head(&dom->ard_list);
	if (sub == NULL)
		return;

	for (;;) {
		M0_ASSERT(ard_be_list_is_empty(&sub->rds_list));
		m0_mutex_init(&sub->rds_lock);
		m0_chan_init(&sub->rds_chan, &sub->rds_lock);

		sub = ard_be_list_next(&dom->ard_list, sub);
		if (sub == NULL)
			break;
	}
}

M0_INTERNAL void
m0_be_active_record_domain_fini(struct m0_be_active_record_domain *dom)
{
	struct m0_be_active_record_domain_subsystem *sub;

	sub = ard_be_list_head(&dom->ard_list);
	if (sub == NULL)
		return;

	for (;;) {
		M0_ASSERT(rds_be_list_is_empty(&sub->rds_list));
		m0_chan_fini_lock(&sub->rds_chan);
		m0_mutex_fini(&sub->rds_lock);

		sub = ard_be_list_next(&dom->ard_list, sub);
		if (sub == NULL)
			break;
	}
}

M0_INTERNAL bool
m0_be_active_record_domain__invariant(struct m0_be_active_record_domain *dom)
{
	/* XXX: update later */
	return dom->ard_seg != NULL;
}

M0_INTERNAL int
m0_be_active_record_domain__create(struct m0_be_active_record_domain **dom,
				   struct m0_be_tx                    *tx,
				   struct m0_be_seg                   *seg,
				   const struct m0_buf                *path)
{
	struct m0_be_domain *bedom = seg->bs_domain;
	struct m0_buf        data = {};
	int		     rc;
	unsigned             i;

	M0_BE_ALLOC_PTR_SYNC(*dom, seg, tx);
	ard_be_list_create(&(*dom)->ard_list, tx);

	for (i = 0; !m0_buf_eq(&path[i], &M0_BUF_INIT0); ++i) {
		struct m0_be_active_record_domain_subsystem *sub;

		M0_BE_ALLOC_PTR_SYNC(sub, seg, tx);
		strncpy(sub->rds_name, path[i].b_addr, path[i].b_nob);
		rds_be_list_create(&sub->rds_list, tx);
		ard_be_tlink_create(sub, tx);
		ard_be_list_add(&(*dom)->ard_list, tx, sub);

		M0_BE_TX_CAPTURE_PTR(seg, tx, sub);
	}

	M0_BE_TX_CAPTURE_PTR(seg, tx, *dom);

	data = M0_BUF_INIT_PTR(dom);
	rc = m0_be_0type_add(&m0_be_active_record0, bedom, tx,
			     BE_ACTIVE_RECORD_ID, &data);
	M0_ASSERT(rc == 0);

	return M0_RC(0);
}

M0_INTERNAL int
m0_be_active_record_domain_destroy(struct m0_be_active_record_domain *dom,
				   struct m0_be_tx *tx)
{
	struct m0_be_active_record_domain_subsystem *sub;
	struct m0_be_seg			    *seg = dom->ard_seg;
	struct m0_be_domain			    *bedom = seg->bs_domain;
	int					     rc;

	rc = m0_be_0type_del(&m0_be_active_record0, bedom, tx,
			     BE_ACTIVE_RECORD_ID);
	M0_ASSERT(rc == 0);

	for (;;) {
		sub = ard_be_list_tail(&dom->ard_list);
		if (sub == NULL)
			break;

		/* no unrecovered records should be in lists */
		M0_ASSERT(rds_be_list_is_empty(&sub->rds_list));

		ard_be_list_del(&dom->ard_list, tx, sub);
		rds_be_list_destroy(&sub->rds_list, tx);
		ard_be_tlink_destroy(sub, tx);
		M0_BE_FREE_PTR_SYNC(sub, seg, tx);
	}

	ard_be_list_destroy(&dom->ard_list, tx);
	M0_BE_FREE_PTR_SYNC(dom, seg, tx);

	return M0_RC(0);
}

M0_INTERNAL void
m0_be_active_record_domain_credit(struct m0_be_active_record_domain *dom,
				  enum m0_be_active_record_domain_op op,
				  uint8_t                            subsys_nr,
				  struct m0_be_tx_credit            *accum)
{
	struct m0_be_active_record_domain_subsystem  sub;
	struct m0_be_seg			    *seg   = dom->ard_seg;
	struct m0_be_domain			    *bedom = seg->bs_domain;
	struct m0_buf                                data  = {};


	M0_ASSERT_INFO(M0_IN(op, (RDO_CREATE, RDO_DESTROY)), "op=%d", op);
	switch(op) {
	case RDO_CREATE:
		m0_be_0type_add_credit(bedom, &m0_be_active_record0,
				       BE_ACTIVE_RECORD_ID, &data, accum);
		M0_BE_ALLOC_CREDIT_PTR(dom, dom->ard_seg, accum);
		M0_BE_ALLOC_CREDIT_ARR(&sub, subsys_nr, dom->ard_seg, accum);
		ard_be_list_credit(M0_BLO_CREATE, 1, accum);
		rds_be_list_credit(M0_BLO_CREATE, subsys_nr, accum);
		ard_be_list_credit(M0_BLO_ADD, subsys_nr, accum);
		m0_be_tx_credit_mac(accum, &M0_BE_TX_CREDIT_TYPE(sub),
				    subsys_nr);
		m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT_TYPE(dom));
		break;
	case RDO_DESTROY:
		m0_be_0type_del_credit(bedom, &m0_be_active_record0,
				       BE_ACTIVE_RECORD_ID, accum);
		M0_BE_FREE_CREDIT_PTR(dom, dom->ard_seg, accum);
		ard_be_list_credit(M0_BLO_DESTROY, 1, accum);

		M0_BE_FREE_CREDIT_ARR(&sub, subsys_nr, dom->ard_seg, accum);
		rds_be_list_credit(M0_BLO_DESTROY, subsys_nr, accum);
		ard_be_list_credit(M0_BLO_DEL, subsys_nr, accum);
		break;
	}
}

/* ----------------------------------------------------------------------
 * m0_be_active_record
 * ---------------------------------------------------------------------- */

M0_INTERNAL void
m0_be_active_record_init(struct m0_be_active_record        *rec,
			 struct m0_be_active_record_domain *ar_dom)
{
	rec->ar_dom = ar_dom;
}

M0_INTERNAL void
m0_be_active_record_fini(struct m0_be_active_record *rec)
{
}

M0_INTERNAL bool
m0_be_active_record__invariant(struct m0_be_active_record *rec)
{
	return rec->ar_dom != NULL;
}

M0_INTERNAL int
m0_be_active_record_create(struct m0_be_active_record	    **rec,
			   struct m0_be_tx	             *tx,
			   struct m0_be_active_record_domain *ar_dom)
{
	M0_BE_ALLOC_PTR_SYNC(*rec, ar_dom->ard_seg, tx);
	*(*rec) = (struct m0_be_active_record) {
		.ar_tx_id    = 0,
		.ar_rec_type = ART_NORM,
		.ar_dom      = ar_dom,
	};

	rds_be_tlink_create(*rec, tx);
	M0_BE_TX_CAPTURE_PTR(ar_dom->ard_seg, tx, *rec);

	return M0_RC(0);
}

M0_INTERNAL int
m0_be_active_record_destroy(struct m0_be_active_record *rec,
			    struct m0_be_tx            *tx)
{
	rds_be_tlink_destroy(rec, tx);
	M0_BE_FREE_PTR_SYNC(rec, rec->ar_dom->ard_seg, tx);

	return M0_RC(0);
}

M0_INTERNAL void
m0_be_active_record_credit(struct m0_be_active_record  *rec,
			   enum m0_be_active_record_op  op,
			   struct m0_be_tx_credit      *accum)
{
	struct m0_be_active_record_domain_subsystem sub;

	M0_ASSERT_INFO(M0_IN(op, (ARO_CREATE, ARO_DESTROY, ARO_DEL,
				  ARO_ADD)), "op=%d", op);
	switch(op) {
	case ARO_CREATE:
		M0_BE_ALLOC_CREDIT_PTR(rec, rec->ar_dom->ard_seg, accum);
		m0_be_tx_credit_add(accum, &M0_BE_TX_CREDIT_TYPE(sub));
		break;
	case ARO_DESTROY:
		M0_BE_FREE_CREDIT_PTR(rec, rec->ar_dom->ard_seg, accum);
		break;
	case ARO_DEL:
		rds_be_list_credit(M0_BLO_DEL, 1, accum);
		break;
	case ARO_ADD:
		rds_be_list_credit(M0_BLO_ADD, 1, accum);
		break;
	}
}

static struct m0_be_active_record_domain_subsystem *
be_active_record__subsystem_lookup(struct m0_be_active_record_domain *dom,
				   const char *subsys)
{
	struct m0_be_active_record_domain_subsystem *sub;

	sub = ard_be_list_head(&dom->ard_list);
	if (sub == NULL || m0_streq(subsys, sub->rds_name))
		return sub;

	for (;;) {
		sub = ard_be_list_next(&dom->ard_list, sub);
		if (sub == NULL || m0_streq(subsys, sub->rds_name))
			break;
	}

	return sub;
}

M0_INTERNAL int
m0_be_active_record_add(const char		   *subsys,
			struct m0_be_active_record *rec,
			struct m0_be_tx            *tx)
{
	struct m0_be_active_record_domain_subsystem *sub =
		be_active_record__subsystem_lookup(rec->ar_dom, subsys);

	if (sub == NULL)
		return M0_RC(-ENOENT);

	rds_be_list_add(&sub->rds_list, tx, rec);

	return M0_RC(0);
}

M0_INTERNAL int
m0_be_active_record_del(const char		   *subsys,
			struct m0_be_active_record *rec,
			struct m0_be_tx            *tx)
{
	struct m0_be_active_record_domain_subsystem *sub =
		be_active_record__subsystem_lookup(rec->ar_dom, subsys);

	if (sub == NULL)
		return M0_RC(-ENOENT);

	rds_be_list_del(&sub->rds_list, tx, rec);

	return M0_RC(0);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of BE group */

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
