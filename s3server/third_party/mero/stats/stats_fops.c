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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@xyratex.com>
 * Original creation date: 07/24/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STATS
#include "lib/trace.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "rpc/item.h"
#include "fop/fop_item_type.h"
#include "rpc/rpc_opcodes.h"
#include "fop/fop.h"
#include "stats/stats_srv.h"
#include "stats/stats_fops_xc.h"

extern struct m0_sm_state_descr stats_update_phases[];
extern const struct m0_sm_conf stats_update_fom_sm_conf;
extern struct m0_sm_state_descr stats_query_phases[];
extern const struct m0_sm_conf stats_query_fom_sm_conf;
extern const struct m0_fom_type_ops stats_update_fom_type_ops;
extern const struct m0_fom_type_ops stats_query_fom_type_ops;

struct m0_fop_type m0_fop_stats_update_fopt;
struct m0_fop_type m0_fop_stats_query_fopt;
struct m0_fop_type m0_fop_stats_query_rep_fopt;

static const struct m0_rpc_item_type_ops stats_update_item_type_ops = {
	M0_FOP_DEFAULT_ITEM_TYPE_OPS,
};

static const struct m0_rpc_item_type_ops stats_query_item_type_ops = {
	M0_FOP_DEFAULT_ITEM_TYPE_OPS,
};

const struct m0_fop_type_ops stats_update_fop_ops;
const struct m0_fop_type_ops stats_query_fop_ops;

M0_INTERNAL int m0_stats_fops_init(void)
{
#ifndef __KERNEL__
	m0_sm_conf_extend(m0_generic_conf.scf_state, stats_query_phases,
			  m0_generic_conf.scf_nr_states);
#endif
	M0_FOP_TYPE_INIT(&m0_fop_stats_update_fopt,
			 .name      = "Stats update fop",
			 .opcode    = M0_STATS_UPDATE_FOP_OPCODE,
			 .xt	    = m0_stats_update_fop_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_ONEWAY,
			 .fop_ops   = &stats_update_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &stats_update_fom_type_ops,
			 .sm	    = &stats_update_fom_sm_conf,
			 .svc_type  = &m0_stats_svc_type,
#endif
			 .rpc_ops   = &stats_update_item_type_ops);
	M0_FOP_TYPE_INIT(&m0_fop_stats_query_fopt,
			 .name      = "Stats query fop",
			 .opcode    = M0_STATS_QUERY_FOP_OPCODE,
			 .xt	    = m0_stats_query_fop_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &stats_query_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &stats_query_fom_type_ops,
			 .sm	    = &stats_query_fom_sm_conf,
			 .svc_type  = &m0_stats_svc_type,
#endif
			 .rpc_ops   = &stats_query_item_type_ops);
	M0_FOP_TYPE_INIT(&m0_fop_stats_query_rep_fopt,
			 .name      = "Stats query rep fop",
			 .opcode    = M0_STATS_QUERY_REP_FOP_OPCODE,
			 .xt	    = m0_stats_query_rep_fop_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	return 0;
}

M0_INTERNAL void m0_stats_fops_fini(void)
{
	m0_fop_type_fini(&m0_fop_stats_update_fopt);
	m0_fop_type_fini(&m0_fop_stats_query_fopt);
	m0_fop_type_fini(&m0_fop_stats_query_rep_fopt);
}

M0_INTERNAL struct m0_stats_update_fop *
m0_stats_update_fop_get(struct m0_fop *fop)
{
	return (struct m0_stats_update_fop *)m0_fop_data(fop);
}

M0_INTERNAL struct m0_stats_query_fop *
m0_stats_query_fop_get(struct m0_fop *fop)
{
	return (struct m0_stats_query_fop *)m0_fop_data(fop);
}

M0_INTERNAL struct m0_stats_query_rep_fop *
m0_stats_query_rep_fop_get(struct m0_fop *fop)
{
	return (struct m0_stats_query_rep_fop *)m0_fop_data(fop);
}

M0_INTERNAL void m0_stats_query_fop_release(struct m0_ref *ref)
{
        struct m0_fop *fop = container_of(ref, struct m0_fop, f_ref);

        M0_PRE(fop != NULL);
        m0_fop_fini(fop);
        m0_free(fop);
}

#undef M0_TRACE_SUBSYSTEM
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
