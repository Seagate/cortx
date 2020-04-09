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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 05/19/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FOP
#include "lib/trace.h"

#include "lib/memory.h"
#include "lib/misc.h"            /* M0_SET0 */
#include "lib/errno.h"
#include "mero/magic.h"
#include "rpc/rpc_machine.h"     /* m0_rpc_machine, m0_rpc_machine_lock */
#include "rpc/addb2.h"
#include "fop/fop.h"
#include "fop/fop_xc.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "fop/fom_generic_xc.h"
#include "fop/fom_long_lock.h"   /* m0_fom_ll_global_init */
#include "addb2/identifier.h"
#include "reqh/reqh.h"

/**
   @addtogroup fop
   @{
 */

static struct m0_mutex fop_types_lock;
static struct m0_tl    fop_types_list;

M0_TL_DESCR_DEFINE(ft, "fop types", static, struct m0_fop_type,
		   ft_linkage,	ft_magix,
		   M0_FOP_TYPE_MAGIC, M0_FOP_TYPE_HEAD_MAGIC);

M0_TL_DEFINE(ft, static, struct m0_fop_type);

M0_INTERNAL const char *m0_fop_name(const struct m0_fop *fop)
{
	return fop->f_type != NULL ? fop->f_type->ft_name : "untyped";
}

static size_t fop_data_size(const struct m0_fop *fop)
{
	M0_PRE(fop->f_type->ft_xt != NULL);
	return fop->f_type->ft_xt->xct_sizeof;
}

M0_INTERNAL bool m0_fop_rpc_is_locked(struct m0_fop *fop)
{
	return m0_rpc_machine_is_locked(fop->f_item.ri_rmachine);
}

M0_INTERNAL int m0_fop_data_alloc(struct m0_fop *fop)
{
	M0_PRE(fop->f_data.fd_data == NULL && fop->f_type != NULL);

	fop->f_data.fd_data = m0_alloc(fop_data_size(fop));
	return fop->f_data.fd_data == NULL ? -ENOMEM : 0;
}

M0_INTERNAL void m0_fop_init(struct m0_fop *fop, struct m0_fop_type *fopt,
			     void *data, void (*fop_release)(struct m0_ref *))
{
	M0_ENTRY();
	M0_PRE(fop != NULL && fopt != NULL && fop_release != NULL);

	m0_ref_init(&fop->f_ref, 1, fop_release);
	fop->f_type = fopt;
	M0_SET0(&fop->f_item);
	m0_rpc_item_init(&fop->f_item, &fopt->ft_rpc_item_type);
	fop->f_data.fd_data = data;
	M0_LOG(M0_DEBUG, "fop: %p %s", fop, m0_fop_name(fop));

	M0_POST(m0_ref_read(&fop->f_ref) == 1);
	M0_LEAVE();
}

struct m0_fop *m0_fop_alloc(struct m0_fop_type *fopt, void *data,
			    struct m0_rpc_machine *mach)
{
	struct m0_fop *fop;

	M0_PRE(mach != NULL);

	M0_ALLOC_PTR(fop);
	if (fop == NULL)
		return NULL;

	m0_fop_init(fop, fopt, data, m0_fop_release);
	if (data == NULL) {
		int rc = m0_fop_data_alloc(fop);
		if (rc != 0) {
			m0_fop_fini(fop);
			m0_free(fop);
			return NULL;
		}
	}
	fop->f_item.ri_rmachine = mach;
	M0_POST(m0_ref_read(&fop->f_ref) == 1);
	return fop;
}
M0_EXPORTED(m0_fop_alloc);

struct m0_fop *m0_fop_alloc_at(struct m0_rpc_session *sess,
			       struct m0_fop_type *fopt)
{
	return m0_fop_alloc(fopt, NULL, m0_fop_session_machine(sess));
}
M0_EXPORTED(m0_fop_alloc_at);

struct m0_fop *m0_fop_reply_alloc(struct m0_fop *req,
				  struct m0_fop_type *rept)
{
	return m0_fop_alloc(rept, NULL, m0_fop_rpc_machine(req));
}
M0_EXPORTED(m0_fop_reply_alloc);

M0_INTERNAL void m0_fop_fini(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	M0_ENTRY("fop: %p %s", fop, m0_fop_name(fop));
	M0_PRE(M0_IN(m0_ref_read(&fop->f_ref), (0, 1)));

	m0_rpc_item_fini(&fop->f_item);
	if (fop->f_data.fd_data != NULL)
		m0_xcode_free_obj(&M0_FOP_XCODE_OBJ(fop));
	M0_LEAVE();
}

M0_INTERNAL void m0_fop_release(struct m0_ref *ref)
{
	struct m0_fop *fop;

	M0_ENTRY();
	M0_PRE(ref != NULL);

	fop = container_of(ref, struct m0_fop, f_ref);
	m0_fop_fini(fop);
	m0_free(fop);

	M0_LEAVE();
}

struct m0_fop *m0_fop_get(struct m0_fop *fop)
{
	uint64_t count = m0_ref_read(&fop->f_ref);

	M0_ENTRY("fop: %p %s [%llu -> %llu]", fop, m0_fop_name(fop),
		 (unsigned long long)count, (unsigned long long)count + 1);
	M0_PRE(count > 0);

	m0_ref_get(&fop->f_ref);

	M0_LEAVE();
	return fop;
}
M0_EXPORTED(m0_fop_get);

void m0_fop_put(struct m0_fop *fop)
{
	uint64_t count = m0_ref_read(&fop->f_ref);

	M0_ENTRY("fop: %p %s [%llu -> %llu]", fop, m0_fop_name(fop),
		 (unsigned long long)count, (unsigned long long)count - 1);
	M0_PRE(m0_fop_rpc_is_locked(fop));
	M0_PRE(count > 0);

	m0_ref_put(&fop->f_ref);

	M0_LEAVE();
}
M0_EXPORTED(m0_fop_put);

void m0_fop_put0(struct m0_fop *fop)
{
	if (fop != NULL)
		m0_fop_put(fop);
}
M0_EXPORTED(m0_fop_put0);

void m0_fop_put_lock(struct m0_fop *fop)
{
	struct m0_rpc_machine *mach;

	M0_PRE(m0_ref_read(&fop->f_ref) > 0);

	mach = m0_fop_rpc_machine(fop);
	M0_ASSERT(mach != NULL);
	m0_rpc_machine_lock(mach);
	m0_fop_put(fop);
	m0_rpc_machine_unlock(mach);
}
M0_EXPORTED(m0_fop_put_lock);

void m0_fop_put0_lock(struct m0_fop *fop)
{
	if (fop != NULL)
		m0_fop_put_lock(fop);
}
M0_EXPORTED(m0_fop_put0_lock);

void *m0_fop_data(const struct m0_fop *fop)
{
	return fop->f_data.fd_data;
}
M0_EXPORTED(m0_fop_data);

uint32_t m0_fop_opcode(const struct m0_fop *fop)
{
	return fop->f_type->ft_rpc_item_type.rit_opcode;
}
M0_EXPORTED(m0_fop_opcode);

void m0_fop_type_fini(struct m0_fop_type *fopt)
{
	M0_ENTRY("name=%s opcode=%"PRIu32" rpc_flags=%"PRIu64,
		 fopt->ft_name, fopt->ft_rpc_item_type.rit_opcode,
		 fopt->ft_rpc_item_type.rit_flags);
	m0_mutex_lock(&fop_types_lock);
	if (fopt->ft_magix == M0_FOP_TYPE_MAGIC) {
		m0_rpc_item_type_deregister(&fopt->ft_rpc_item_type);
		ft_tlink_del_fini(fopt);
		fopt->ft_magix = 0;
	}
	m0_mutex_unlock(&fop_types_lock);
}
M0_EXPORTED(m0_fop_type_fini);

void m0_fop_type_init(struct m0_fop_type *ft,
		      const struct __m0_fop_type_init_args *args)
{
	struct m0_rpc_item_type    *rpc_type;
	const struct m0_xcode_type *xt = args->xt;

	M0_ENTRY("name=%s opcode=%"PRIu32" rpc_flags=%"PRIu64,
		 args->name, args->opcode, args->rpc_flags);
	M0_PRE(ft->ft_magix == 0);
	M0_PRE(ergo(args->rpc_flags & M0_RPC_ITEM_TYPE_REPLY,
		    xt->xct_nr > 0 && xt->xct_child[0].xf_type == &M0_XT_U32));
	M0_PRE_EX(xt == NULL ||
		  m0_xcode_type_flags((struct m0_xcode_type*)xt,
				      M0_XCODE_TYPE_FLAG_DOM_RPC, 0,
				      M0_BITS(M0_XA_ATOM)));

	rpc_type = &ft->ft_rpc_item_type;

	ft->ft_name = args->name;
	ft->ft_xt   = xt;
	ft->ft_ops  = args->fop_ops;

	rpc_type->rit_opcode = args->opcode;
	rpc_type->rit_flags  = args->rpc_flags;
	rpc_type->rit_ops    = args->rpc_ops ?: &m0_fop_default_item_type_ops;

	m0_fom_type_init(&ft->ft_fom_type, args->opcode,
			 args->fom_ops, args->svc_type, args->sm);
	m0_rpc_item_type_register(rpc_type);
	m0_mutex_lock(&fop_types_lock);
	ft_tlink_init_at(ft, &fop_types_list);
	m0_mutex_unlock(&fop_types_lock);
}
M0_EXPORTED(m0_fop_type_init);

M0_INTERNAL void m0_fop_type_init_nr(const struct m0_fop_type_batch *batch)
{
	for (; batch->tb_type != NULL; ++batch)
		m0_fop_type_init(batch->tb_type, &batch->tb_args);
}

M0_INTERNAL void m0_fop_type_fini_nr(const struct m0_fop_type_batch *batch)
{
	for (; batch->tb_type != NULL; ++batch) {
		if (batch->tb_type->ft_magix != 0)
			m0_fop_type_fini(batch->tb_type);
	}
}

M0_INTERNAL struct m0_fop_type *m0_fop_type_next(struct m0_fop_type *ftype)
{
	struct m0_fop_type *rtype;

	m0_mutex_lock(&fop_types_lock);
	if (ftype == NULL) {
		/* Returns head of fop_types_list */
		rtype = ft_tlist_head(&fop_types_list);
	} else {
		/* Returns Next from fop_types_list */
		rtype = ft_tlist_next(&fop_types_list, ftype);
	}
	m0_mutex_unlock(&fop_types_lock);
	return rtype;
}


M0_FOL_FRAG_TYPE_DECLARE(m0_fop_fol_frag, , NULL, NULL, NULL, NULL);
M0_EXTERN struct m0_sm_conf fom_states_conf;
M0_INTERNAL int m0_fops_init(void)
{
	m0_sm_conf_init(&fom_states_conf);
	ft_tlist_init(&fop_types_list);
	m0_mutex_init(&fop_types_lock);
	m0_fom_ll_global_init();

	m0_fop_fol_frag_type.rpt_xt  = m0_fop_fol_frag_xc;
	m0_fop_fol_frag_type.rpt_ops = NULL;
	M0_FOL_FRAG_TYPE_INIT(m0_fop_fol_frag,
				  "fop generic record frag");
	return m0_fol_frag_type_register(&m0_fop_fol_frag_type);
}

M0_INTERNAL void m0_fops_fini(void)
{
	m0_mutex_fini(&fop_types_lock);
	/* Do not finalise fop_types_list, it can be validly non-empty. */
	m0_fol_frag_type_deregister(&m0_fop_fol_frag_type);

	m0_sm_conf_fini(&fom_states_conf);
}

struct m0_rpc_item *m0_fop_to_rpc_item(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);

	return (struct m0_rpc_item *)&fop->f_item;
}
M0_EXPORTED(m0_fop_to_rpc_item);

struct m0_fop *m0_rpc_item_to_fop(const struct m0_rpc_item *item)
{
	M0_PRE(item != NULL);
	return container_of(item, struct m0_fop, f_item);
}

void m0_fop_rpc_machine_set(struct m0_fop *fop, struct m0_rpc_machine *mach)
{
	M0_PRE(fop != NULL);
	M0_PRE(mach != NULL);

	fop->f_item.ri_rmachine = mach;
}

struct m0_rpc_machine *m0_fop_rpc_machine(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);

	return fop->f_item.ri_rmachine;
}
M0_EXPORTED(m0_fop_rpc_machine);

M0_INTERNAL struct m0_fop_type *m0_item_type_to_fop_type
    (const struct m0_rpc_item_type *item_type) {
	M0_PRE(item_type != NULL);

	return container_of(item_type, struct m0_fop_type, ft_rpc_item_type);
}

M0_INTERNAL int m0_fop_encdec(struct m0_fop           *fop,
			      struct m0_bufvec_cursor *cur,
			      enum m0_xcode_what       what)
{
	int                 result;
	struct m0_xcode_obj xo = M0_FOP_XCODE_OBJ(fop);

	result = m0_xcode_encdec(&xo, cur, what);
	if (result == 0 && m0_fop_data(fop) == NULL)
		fop->f_data.fd_data = xo.xo_ptr;
	return result;
}

M0_INTERNAL struct m0_fop_type *m0_fop_type_find(uint32_t opcode)
{
	struct m0_fop_type *ftype = NULL;

	while ((ftype = m0_fop_type_next(ftype)) != NULL) {
		if(ftype->ft_rpc_item_type.rit_opcode == opcode)
			break;
	}
	return ftype;
}

static int fop_xc_type(uint32_t opcode, const struct m0_xcode_type **out)
{
	struct m0_fop_type *ftype;

	ftype = m0_fop_type_find(opcode);
	if (ftype == NULL)
		return M0_ERR(-EINVAL);

	*out = ftype->ft_xt;
	return 0;
}

M0_INTERNAL int m0_fop_xc_type(const struct m0_xcode_obj   *par,
			       const struct m0_xcode_type **out)
{
	struct m0_fop_fol_frag *rp = par->xo_ptr;

	return fop_xc_type(rp->ffrp_fop_code, out);
}

M0_INTERNAL int m0_fop_rep_xc_type(const struct m0_xcode_obj   *par,
				   const struct m0_xcode_type **out)
{
	struct m0_fop_fol_frag *rp = par->xo_ptr;

	return fop_xc_type(rp->ffrp_rep_code, out);
}

M0_INTERNAL int m0_fop_fol_add(struct m0_fop *fop, struct m0_fop *rep,
			       struct m0_dtx *dtx)
{
	struct m0_fol_frag     *frag;
	struct m0_fop_fol_frag *rp;

	M0_ALLOC_PTR(frag);
	if (frag == NULL)
		return M0_ERR(-ENOMEM);

	M0_ALLOC_PTR(rp);
	if (rp == NULL) {
		m0_free(frag);
		return M0_ERR(-ENOMEM);
	}

	rp->ffrp_fop_code = fop->f_type->ft_rpc_item_type.rit_opcode;
	rp->ffrp_rep_code = rep->f_type->ft_rpc_item_type.rit_opcode;
	rp->ffrp_fop = m0_fop_data(fop);
	rp->ffrp_rep = m0_fop_data(rep);

	m0_fol_frag_init(frag, rp, &m0_fop_fol_frag_type);
	m0_fol_frag_add(&dtx->tx_fol_rec, frag);
	return 0;
}

struct m0_rpc_machine *m0_fop_session_machine(const struct m0_rpc_session *s)
{
	M0_PRE(s != NULL && s->s_conn != NULL);

	return s->s_conn->c_rpc_machine;
}
M0_EXPORTED(m0_fop_session_machine);

int m0_fop_type_addb2_instrument(struct m0_fop_type *type)
{
	struct m0_fom_type      *ft   = &type->ft_fom_type;
	struct m0_rpc_item_type *rt   = &type->ft_rpc_item_type;
	uint64_t                 mask = ((uint64_t)rt->rit_opcode) << 12;

	mask |= M0_AVI_FOP_TYPES_RANGE_START;
	return (ft->ft_conf.scf_name != NULL ?
		m0_sm_addb2_init(&ft->ft_conf, M0_AVI_PHASE,
				 mask | (M0_AFC_PHASE << 8)) : 0) ?:
		m0_sm_addb2_init(&ft->ft_state_conf, M0_AVI_STATE,
				 mask | (M0_AFC_STATE << 8)) ?:
		m0_sm_addb2_init(&rt->rit_outgoing_conf, M0_AVI_RPC_OUT_PHASE,
				 mask | (M0_AFC_RPC_OUT << 8)) ?:
		m0_sm_addb2_init(&rt->rit_incoming_conf, M0_AVI_RPC_IN_PHASE,
				 mask | (M0_AFC_RPC_IN << 8));
}

struct m0_net_transfer_mc *m0_fop_tm_get(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);

	return &m0_fop_rpc_machine(fop)->rm_tm;
}

struct m0_net_domain *m0_fop_domain_get(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);

	return m0_fop_tm_get(fop)->ntm_dom;
}

void m0_fop_type_addb2_deinstrument(struct m0_fop_type *type)
{
	struct m0_fom_type      *ft = &type->ft_fom_type;
	struct m0_rpc_item_type *rt = &type->ft_rpc_item_type;

	m0_sm_addb2_fini(&ft->ft_conf);
	m0_sm_addb2_fini(&ft->ft_state_conf);
	m0_sm_addb2_fini(&rt->rit_outgoing_conf);
	m0_sm_addb2_fini(&rt->rit_incoming_conf);
}

/** @} end of fop group */
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
