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
 * Original author: Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 * Original creation date: 02/07/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_COB
#include "lib/trace.h"
#include "lib/finject.h"

#include "ioservice/io_addb2.h"    /* M0_AVI_IOS_IO_ATTR_FOMCOB_FOP_TYPE */
#include "ioservice/cob_foms.h"    /* m0_fom_cob_create */
#include "ioservice/fid_convert.h" /* m0_fid_convert_cob2stob */
#include "ioservice/io_service.h"  /* m0_reqh_io_service */
#include "ioservice/storage_dev.h" /* m0_storage_dev_stob_find */
#include "mero/setup.h"            /* m0_cs_ctx_get */
#include "stob/domain.h"           /* m0_stob_domain_find_by_stob_id */

struct m0_poolmach;

/* Forward Declarations. */
static void cc_fom_fini(struct m0_fom *fom);
static int  cob_ops_fom_tick(struct m0_fom *fom);
static void cob_op_credit(struct m0_fom *fom, enum m0_cob_op opcode,
			  struct m0_be_tx_credit *accum);
static int  cc_cob_create(struct m0_fom            *fom,
			  struct m0_fom_cob_op     *cc,
			  const struct m0_cob_attr *attr);

static void cd_fom_fini(struct m0_fom *fom);
static int  cd_cob_delete(struct m0_fom            *fom,
			  struct m0_fom_cob_op     *cd,
			  const struct m0_cob_attr *attr);
static void ce_stob_destroy_credit(struct m0_fom_cob_op *cob_op,
				   struct m0_be_tx_credit *accum);
static int ce_stob_edit_credit(struct m0_fom *fom, struct m0_fom_cob_op *cc,
			       struct m0_be_tx_credit *accum, uint32_t cot);
static int ce_stob_edit(struct m0_fom *fom, struct m0_fom_cob_op *cd,
			uint32_t cot);
static int cob_fom_populate(struct m0_fom *fom);
static int    cob_op_fom_create(struct m0_fom **out);
static size_t cob_fom_locality_get(const struct m0_fom *fom);
static inline struct m0_fom_cob_op *cob_fom_get(const struct m0_fom *fom);
static int  cob_getattr_fom_tick(struct m0_fom *fom);
static void cob_getattr_fom_fini(struct m0_fom *fom);
static int  cob_getattr(struct m0_fom        *fom,
			struct m0_fom_cob_op *gop,
			struct m0_cob_attr   *attr);
static int  cob_setattr_fom_tick(struct m0_fom *fom);
static void cob_setattr_fom_fini(struct m0_fom *fom);
static int  cob_setattr(struct m0_fom        *fom,
			struct m0_fom_cob_op *gop,
			struct m0_cob_attr   *attr);
static int cob_locate(const struct m0_fom *fom, struct m0_cob **cob);
static int cob_attr_get(struct m0_cob      *cob,
			struct m0_cob_attr *attr);
static void cob_stob_create_credit(struct m0_fom *fom);
static int cob_stob_delete_credit(struct m0_fom *fom);
static struct m0_cob_domain *cdom_get(const struct m0_fom *fom);
static int cob_ops_stob_find(struct m0_fom_cob_op *co);

enum {
	CC_COB_VERSION_INIT	= 0,
	CC_COB_HARDLINK_NR	= 1,
	CD_FOM_STOBIO_LAST_REFS = 1,
};

struct m0_sm_state_descr cob_ops_phases[] = {
	[M0_FOPH_COB_OPS_PREPARE] = {
		.sd_name      = "COB OP Prepare",
		.sd_allowed   = M0_BITS(M0_FOPH_COB_OPS_EXECUTE,
					M0_FOPH_FAILURE)
	},
	[M0_FOPH_COB_OPS_EXECUTE] = {
		.sd_name      = "COB OP EXECUTE",
		.sd_allowed   = M0_BITS(M0_FOPH_SUCCESS,
					M0_FOPH_FAILURE)
	}
};

const struct m0_sm_conf cob_ops_conf = {
	.scf_name      = "COB create/delete/getattr",
	.scf_nr_states = ARRAY_SIZE(cob_ops_phases),
	.scf_state     = cob_ops_phases
};

/**
 * Common fom_type_ops for m0_fop_cob_create, m0_fop_cob_delete,
 * m0_fop_cob_getattr, and m0_fop_cob_setattr fops.
 */
const struct m0_fom_type_ops cob_fom_type_ops = {
	.fto_create = m0_cob_fom_create,
};

/** Cob create fom ops. */
static const struct m0_fom_ops cc_fom_ops = {
	.fo_fini	  = cc_fom_fini,
	.fo_tick	  = cob_ops_fom_tick,
	.fo_home_locality = cob_fom_locality_get
};

/** Cob delete fom ops. */
static const struct m0_fom_ops cd_fom_ops = {
	.fo_fini          = cd_fom_fini,
	.fo_tick          = cob_ops_fom_tick,
	.fo_home_locality = cob_fom_locality_get
};

/** Cob truncate fom ops. */
static const struct m0_fom_ops ct_fom_ops = {
	.fo_fini          = cd_fom_fini,
	.fo_tick          = cob_ops_fom_tick,
	.fo_home_locality = cob_fom_locality_get
};

/** Cob getattr fom ops. */
static const struct m0_fom_ops cob_getattr_fom_ops = {
	.fo_fini	  = cob_getattr_fom_fini,
	.fo_tick	  = cob_getattr_fom_tick,
	.fo_home_locality = cob_fom_locality_get
};

/** Cob setattr fom ops. */
static const struct m0_fom_ops cob_setattr_fom_ops = {
	.fo_fini	  = cob_setattr_fom_fini,
	.fo_tick	  = cob_setattr_fom_tick,
	.fo_home_locality = cob_fom_locality_get
};

static bool cob_is_md(const struct m0_fom_cob_op *cfom)
{
	return cfom->fco_cob_type == M0_COB_MD;
}

static void cob_fom_stob2fid_map(const struct m0_fom_cob_op *cfom,
				 struct m0_fid *out)
{
	if (cob_is_md(cfom))
		*out = cfom->fco_gfid;
	else
		m0_fid_convert_stob2cob(&cfom->fco_stob_id, out);
}

static void addb2_add_cob_fom_attrs(const struct m0_fom_cob_op *cfom)
{
	const struct m0_fom *fom = &cfom->fco_fom;

	M0_ADDB2_ADD(M0_AVI_ATTR, m0_sm_id_get(&fom->fo_sm_phase),
		     M0_AVI_IOS_IO_ATTR_FOMCOB_FOP_TYPE,
		     cfom->fco_fop_type);
	M0_ADDB2_ADD(M0_AVI_ATTR, m0_sm_id_get(&fom->fo_sm_phase),
		     M0_AVI_IOS_IO_ATTR_FOMCOB_GFID_CONT,
		     cfom->fco_gfid.f_container);
	M0_ADDB2_ADD(M0_AVI_ATTR, m0_sm_id_get(&fom->fo_sm_phase),
		     M0_AVI_IOS_IO_ATTR_FOMCOB_GFID_KEY,
		     cfom->fco_gfid.f_key);
	M0_ADDB2_ADD(M0_AVI_ATTR, m0_sm_id_get(&fom->fo_sm_phase),
		     M0_AVI_IOS_IO_ATTR_FOMCOB_CFID_CONT,
		     cfom->fco_cfid.f_container);
	M0_ADDB2_ADD(M0_AVI_ATTR, m0_sm_id_get(&fom->fo_sm_phase),
		     M0_AVI_IOS_IO_ATTR_FOMCOB_CFID_KEY,
		     cfom->fco_cfid.f_key);
}

M0_INTERNAL int m0_cob_fom_create(struct m0_fop *fop, struct m0_fom **out,
				  struct m0_reqh *reqh)
{
	int			  rc;
	struct m0_fop            *rfop;
	struct m0_fom		 *fom;
	const struct m0_fom_ops  *fom_ops;
	struct m0_fom_cob_op     *cfom;
	struct m0_fop_type       *reptype;

	M0_PRE(fop != NULL);
	M0_PRE(fop->f_type != NULL);
	M0_PRE(out != NULL);
	M0_PRE(m0_is_cob_create_fop(fop)  || m0_is_cob_delete_fop(fop) ||
	       m0_is_cob_truncate_fop(fop) || m0_is_cob_getattr_fop(fop) ||
	       m0_is_cob_setattr_fop(fop) );

	rc = cob_op_fom_create(out);
	if (rc != 0) {
		return M0_RC(rc);
	}
	cfom = cob_fom_get(*out);
	fom = *out;
	M0_ASSERT(fom != NULL);

	if (m0_is_cob_create_fop(fop)) {
		fom_ops = &cc_fom_ops;
		reptype = &m0_fop_cob_op_reply_fopt;
	} else if (m0_is_cob_delete_fop(fop)) {
		fom_ops = &cd_fom_ops;
		reptype = &m0_fop_cob_op_reply_fopt;
	} else if (m0_is_cob_truncate_fop(fop)) {
		fom_ops = &ct_fom_ops;
		reptype = &m0_fop_cob_op_reply_fopt;
	} else if (m0_is_cob_getattr_fop(fop)) {
		fom_ops = &cob_getattr_fom_ops;
		reptype = &m0_fop_cob_getattr_reply_fopt;
	} else if (m0_is_cob_setattr_fop(fop)) {
		fom_ops = &cob_setattr_fom_ops;
		reptype = &m0_fop_cob_setattr_reply_fopt;
	} else
		M0_IMPOSSIBLE("Invalid fop type!");

	rfop = m0_fop_reply_alloc(fop, reptype);
	if (rfop == NULL) {
		m0_free(cfom);
		return M0_ERR(-ENOMEM);
	}

	m0_fom_init(fom, &fop->f_type->ft_fom_type, fom_ops, fop, rfop, reqh);
	rc = cob_fom_populate(fom);

	return M0_RC(rc);
}

static int cob_op_fom_create(struct m0_fom **out)
{
	struct m0_fom_cob_op *cfom;

	M0_PRE(out != NULL);

	M0_ALLOC_PTR(cfom);
	if (cfom == NULL)
		return M0_ERR(-ENOMEM);

	*out = &cfom->fco_fom;
	return 0;
}

static inline struct m0_fom_cob_op *cob_fom_get(const struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

	return container_of(fom, struct m0_fom_cob_op, fco_fom);
}

static void cc_fom_fini(struct m0_fom *fom)
{
	struct m0_fom_cob_op *cfom;

	M0_PRE(fom != NULL);

	cfom = cob_fom_get(fom);
	addb2_add_cob_fom_attrs(cfom);
	m0_fom_fini(fom);
	m0_free(cfom);
}

M0_INTERNAL size_t m0_cob_io_fom_locality(const struct m0_fid *fid)
{
	uint64_t hash = m0_fid_hash(fid);

	return m0_rnd(1 << 30, &hash) >> 1;
}

static size_t cob_fom_locality_get(const struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

	return m0_cob_io_fom_locality(&cob_fom_get(fom)->fco_cfid);
}

static int cob_fom_populate(struct m0_fom *fom)
{
	struct m0_fom_cob_op     *cfom;
	struct m0_fop_cob_common *common;
	struct m0_fop            *fop;
	int                       rc;
	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_fop != NULL);

	fop = fom->fo_fop;
	common = m0_cobfop_common_get(fom->fo_fop);
	cfom = cob_fom_get(fom);
	cfom->fco_gfid = common->c_gobfid;
	cfom->fco_cfid = common->c_cobfid;
	m0_fid_convert_cob2stob(&cfom->fco_cfid, &cfom->fco_stob_id);
	cfom->fco_cob_idx = common->c_cob_idx;
	cfom->fco_cob_type = common->c_cob_type;
	cfom->fco_flags = common->c_flags;
	cfom->fco_fop_type = m0_is_cob_create_fop(fop) ? M0_COB_OP_CREATE :
				m0_is_cob_delete_fop(fop) ?
				M0_COB_OP_DELETE : M0_COB_OP_TRUNCATE;
	cfom->fco_recreate = false;
	cfom->fco_is_done = false;
	M0_LOG(M0_DEBUG, "Cob %s operation for "FID_F"/%x "FID_F" for %s",
			  m0_fop_name(fop), FID_P(&cfom->fco_cfid),
			  cfom->fco_cob_idx, FID_P(&cfom->fco_gfid),
			  cob_is_md(cfom) ? "MD" : "IO");

	if (M0_IN(cfom->fco_fop_type,(M0_COB_OP_DELETE, M0_COB_OP_TRUNCATE))) {
		rc = m0_indexvec_alloc(&cfom->fco_range, 1);
		if (rc != 0)
			return M0_ERR(-ENOMEM);
		rc = m0_indexvec_alloc(&cfom->fco_want, 1);
		if (rc != 0) {
			m0_indexvec_free(&cfom->fco_range);
			return M0_ERR(-ENOMEM);
		}
		rc = m0_indexvec_alloc(&cfom->fco_got, 1);
		if (rc != 0) {
			m0_indexvec_free(&cfom->fco_range);
			m0_indexvec_free(&cfom->fco_want);
			return M0_ERR(-ENOMEM);
		}
		cfom->fco_range.iv_index[0] = 0;
		cfom->fco_range.iv_vec.v_count[0] = M0_BINDEX_MAX + 1;
		cfom->fco_range_idx = 0;
	}
	return M0_RC(0);
}

static int cob_ops_stob_find(struct m0_fom_cob_op *co)
{
	struct m0_storage_devs *devs = m0_cs_storage_devs_get();
	int                     rc;

	rc = m0_storage_dev_stob_find(devs, &co->fco_stob_id, &co->fco_stob);
	if (rc == 0 && m0_stob_state_get(co->fco_stob) == CSS_NOENT) {
		m0_storage_dev_stob_put(devs, co->fco_stob);
		rc = -ENOENT;
	}
	return M0_RC(rc);
}

static void cob_tick_tail(struct m0_fom *fom,
			  struct m0_fop_cob_op_rep_common *r_common)
{
	M0_PRE(fom != NULL);

	if (M0_IN(m0_fom_phase(fom), (M0_FOPH_SUCCESS, M0_FOPH_FAILURE)))
		/* Piggyback some information about the transaction */
		m0_fom_mod_rep_fill(&r_common->cor_mod_rep, fom);
}

static int cob_getattr_fom_tick(struct m0_fom *fom)
{
	struct m0_cob_attr               attr = { { 0, } };
	int                              rc = 0;
	struct m0_fom_cob_op            *cob_op;
	const char                      *ops;
	struct m0_fop                   *fop;
	struct m0_fop_cob_op_rep_common *r_common;
	struct m0_fop_cob_getattr_reply *reply;

	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_ops != NULL);
	M0_PRE(fom->fo_type != NULL);

	cob_op = cob_fom_get(fom);
	M0_ENTRY("cob_getattr for "FID_F", phase %s", FID_P(&cob_op->fco_gfid),
		 m0_fom_phase_name(fom, m0_fom_phase(fom)));

	fop = fom->fo_fop;
	reply = m0_fop_data(fom->fo_rep_fop);
	r_common = &reply->cgr_common;

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		rc = m0_fom_tick_generic(fom);
		return M0_RC(rc);
	}

	ops = m0_fop_name(fop);

	switch (m0_fom_phase(fom)) {
	case M0_FOPH_COB_OPS_PREPARE:
		M0_LOG(M0_DEBUG, "Cob %s operation prepare", ops);
		m0_fom_phase_set(fom, M0_FOPH_COB_OPS_EXECUTE);
		reply->cgr_rc = 0;
		return M0_FSO_AGAIN;
	case M0_FOPH_COB_OPS_EXECUTE:
		M0_LOG(M0_DEBUG, "Cob %s operation started for "FID_F,
		       ops, FID_P(&cob_op->fco_gfid));
		rc = cob_getattr(fom, cob_op, &attr);
		m0_md_cob_mem2wire(&reply->cgr_body, &attr);
		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	        M0_LOG(M0_DEBUG, "Cob %s operation for "FID_F" finished with "
		       "%d", ops, FID_P(&cob_op->fco_gfid), rc);
		break;
	default:
		M0_IMPOSSIBLE("Invalid phase for cob getattr fom.");
		rc = -EINVAL;
		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	}

	if (rc != 0)
		reply->cgr_rc = rc;
	cob_tick_tail(fom, r_common);
	return M0_RC(M0_FSO_AGAIN);
}

static int cob_setattr_fom_tick(struct m0_fom *fom)
{
	struct m0_cob_attr               attr = { { 0, } };
	int                              rc = 0;
	struct m0_fom_cob_op            *cob_op;
	const char                      *ops;
	struct m0_fop                   *fop;
	struct m0_fop_cob_common        *cs_common;
	struct m0_fop_cob_op_rep_common *r_common;
	struct m0_fop_cob_setattr_reply *reply;
	struct m0_be_tx_credit          *tx_cred;

	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_ops != NULL);
	M0_PRE(fom->fo_type != NULL);

	cob_op = cob_fom_get(fom);
	M0_ENTRY("cob_setattr for "FID_F", phase %s", FID_P(&cob_op->fco_gfid),
		 m0_fom_phase_name(fom, m0_fom_phase(fom)));

	fop = fom->fo_fop;
	cs_common = m0_cobfop_common_get(fop);
	reply = m0_fop_data(fom->fo_rep_fop);
	r_common = &reply->csr_common;
	ops = m0_fop_name(fop);

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		switch(m0_fom_phase(fom)) {
		case M0_FOPH_TXN_OPEN:
			tx_cred = m0_fom_tx_credit(fom);
			cob_op_credit(fom, M0_COB_OP_UPDATE, tx_cred);
			if (cob_op->fco_flags & M0_IO_FLAG_CROW)
				cob_stob_create_credit(fom);
			break;
		}
		rc = m0_fom_tick_generic(fom);
		return M0_RC(rc);
	}

	switch (m0_fom_phase(fom)) {
	case M0_FOPH_COB_OPS_PREPARE:
		M0_LOG(M0_DEBUG, "Cob %s operation prepare", ops);
		m0_fom_phase_set(fom, M0_FOPH_COB_OPS_EXECUTE);
		reply->csr_rc = 0;
		return M0_FSO_AGAIN;
	case M0_FOPH_COB_OPS_EXECUTE:
		M0_LOG(M0_DEBUG, "Cob %s operation started for "FID_F,
		       ops, FID_P(&cob_op->fco_gfid));
		m0_md_cob_wire2mem(&attr, &cs_common->c_body);
		m0_dump_cob_attr(&attr);
		rc = cob_setattr(fom, cob_op, &attr);
		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	        M0_LOG(M0_DEBUG, "Cob %s operation finished with %d", ops, rc);
		break;
	default:
		M0_IMPOSSIBLE("Invalid phase for cob setattr fom.");
		rc = -EINVAL;
		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	}

	if (rc != 0)
		reply->csr_rc = rc;
	cob_tick_tail(fom, r_common);
	return M0_RC(M0_FSO_AGAIN);
}

static bool cob_pool_version_mismatch(const struct m0_fom *fom)
{
	int                       rc;
	struct m0_cob            *cob;
	struct m0_fop_cob_common *common;

	common = m0_cobfop_common_get(fom->fo_fop);
	rc = cob_locate(fom, &cob);
	if (rc == 0 && cob != NULL) {
		M0_LOG(M0_DEBUG, "cob pver"FID_F", common pver"FID_F,
				FID_P(&cob->co_nsrec.cnr_pver),
				FID_P(&common->c_body.b_pver));
		return !m0_fid_eq(&cob->co_nsrec.cnr_pver,
				  &common->c_body.b_pver);
	}
	return false;
}

static void cob_stob_create_credit(struct m0_fom *fom)
{
	struct m0_fom_cob_op   *cob_op;
	struct m0_be_tx_credit *tx_cred;

	cob_op = cob_fom_get(fom);
	tx_cred = m0_fom_tx_credit(fom);
	if (!cob_is_md(cob_op))
		m0_cc_stob_cr_credit(&cob_op->fco_stob_id, tx_cred);
	cob_op_credit(fom, M0_COB_OP_CREATE, tx_cred);
	cob_op->fco_is_done = true;
}

static int cob_stob_create(struct m0_fom *fom, struct m0_cob_attr *attr)
{
	int                   rc = 0;
	struct m0_fom_cob_op *cob_op;

	cob_op = cob_fom_get(fom);
	if (!cob_is_md(cob_op))
		rc = m0_cc_stob_create(fom, &cob_op->fco_stob_id);
	return rc ?: cc_cob_create(fom, cob_op, attr);
}

static int cob_stob_delete_credit(struct m0_fom *fom)
{
	int                     rc = 0;
	struct m0_fom_cob_op   *cob_op;
	uint32_t                fop_type;
	struct m0_be_tx_credit *tx_cred;
	struct m0_be_tx_credit  cob_op_tx_credit = {};

	cob_op = cob_fom_get(fom);
	tx_cred = m0_fom_tx_credit(fom);
	fop_type = cob_op->fco_fop_type;
	if (cob_is_md(cob_op)) {
		cob_op_credit(fom, M0_COB_OP_DELETE, tx_cred);
		if (cob_op->fco_recreate)
			cob_op_credit(fom, M0_COB_OP_CREATE, tx_cred);
		cob_op->fco_is_done = true;
		return M0_RC(rc);
	}
	rc = ce_stob_edit_credit(fom, cob_op, tx_cred, fop_type);
	if (rc == 0) {
		M0_SET0(&cob_op_tx_credit);
		cob_op_credit(fom, fop_type, &cob_op_tx_credit);
		if (cob_op->fco_recreate)
			cob_stob_create_credit(fom);
		if (fop_type == M0_COB_OP_DELETE &&
		    !m0_be_should_break(m0_fom_tx(fom)->t_engine, tx_cred,
					&cob_op_tx_credit)) {
			m0_be_tx_credit_add(tx_cred, &cob_op_tx_credit);
			ce_stob_destroy_credit(cob_op, tx_cred);
			m0_be_tx_credit_add(tx_cred, &cob_op_tx_credit);
			cob_op->fco_is_done = true;
		}
	}
	return M0_RC(rc);
}
static int cob_stob_ref_drop_wait(struct m0_fom *fom)
{
	struct m0_stob *stob = cob_fom_get(fom)->fco_stob;

	M0_PRE(cob_fom_get(fom)->fco_fop_type == M0_COB_OP_DELETE);
	M0_PRE(m0_stob_state_get(stob) == CSS_EXISTS);
	M0_PRE(!m0_chan_has_waiters(&stob->so_ref_chan));

	m0_chan_lock(&stob->so_ref_chan);
	m0_fom_wait_on(fom, &stob->so_ref_chan, &fom->fo_cb);
	m0_chan_unlock(&stob->so_ref_chan);
	M0_LOG(M0_DEBUG, "fom %p, stob %p, stob->so_ref = %"PRIu64
	       ", waiting for ref to drop to 1", fom, stob, stob->so_ref);

	return M0_RC(M0_FSO_WAIT);
}

static int cob_ops_fom_tick(struct m0_fom *fom)
{
	struct m0_fom_cob_op            *cob_op;
	struct m0_fop_cob_common        *common;
	struct m0_cob_attr               attr = { { 0, } };
	int                              rc = 0;
	uint32_t                         fop_type;
	const char                      *ops;
	struct m0_fop                   *fop;
	struct m0_fop_cob_op_rep_common *r_common;
	struct m0_fop_cob_op_reply      *reply;
	struct m0_stob                  *stob = NULL;

	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_ops != NULL);
	M0_PRE(fom->fo_type != NULL);

	fop = fom->fo_fop;
	common = m0_cobfop_common_get(fop);
	reply = m0_fop_data(fom->fo_rep_fop);
	r_common = &reply->cor_common;
	cob_op = cob_fom_get(fom);
	fop_type = cob_op->fco_fop_type;

	M0_ENTRY("fom %p, fop %p, item %p[%u], phase %s, "FID_F" stob %p",
                 fom, fom->fo_fop, m0_fop_to_rpc_item(fom->fo_fop),
                 m0_fop_opcode(fom->fo_fop),
                 m0_fom_phase_name(fom, m0_fom_phase(fom)),
                 FID_P(&cob_op->fco_cfid), cob_op->fco_stob);
	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		switch (m0_fom_phase(fom)) {
		case M0_FOPH_INIT:
			/*
			 * If M0_FOPH_INIT phase is being entered the second
			 * time or beyond, specifically by the cob-delete fom,
			 * after waiting for so_ref to drop to 1, then there is
			 * nothing more to be done in this phase.
			 */
			if (!cob_is_md(cob_op) &&
			    fop_type == M0_COB_OP_DELETE &&
			    cob_op->fco_stob != NULL) {
				stob = cob_op->fco_stob;
				if (stob->so_ref > 1)
					return M0_RC(
						cob_stob_ref_drop_wait(fom));
				M0_LOG(M0_DEBUG, "fom %p, fom_type %d, "
				       "stob %p, Finished waiting for "
				       "ref to drop to 1", fom,
				       cob_op->fco_fop_type, stob);
				/* Mark the stob state as CSS_DELETE */
				m0_stob_delete_mark(cob_op->fco_stob);
				break;
			}

			/* Check if cob with different pool version exists. */
			if (fop_type == M0_COB_OP_CREATE &&
			    cob_pool_version_mismatch(fom)) {
				M0_CNT_DEC(common->c_body.b_nlink);
				fop_type = cob_op->fco_fop_type =
					M0_COB_OP_DELETE;
				cob_op->fco_recreate = true;
			}

			if (fop_type == M0_COB_OP_CREATE)
				break;
			/*
			 * Find the stob and handle non-existing stob error
			 * earlier, before initialising the transaction.
			 * This avoids complications in handling transaction
			 * cleanup.
			 */
			rc = cob_is_md(cob_op) ? 0 : cob_ops_stob_find(cob_op);
			if (rc != 0) {
				if (rc == -ENOENT &&
				    cob_op->fco_flags & M0_IO_FLAG_CROW) {
					/* nothing to delete or truncate */
					M0_ASSERT(M0_IN(fop_type,
							(M0_COB_OP_DELETE,
							 M0_COB_OP_TRUNCATE)));
					rc = 0;
					m0_fom_phase_move(fom, rc,
							  M0_FOPH_SUCCESS);
				} else {
					m0_fom_phase_move(fom, rc,
							  M0_FOPH_FAILURE);
				}
				cob_op->fco_is_done = true;
				goto tail;
			}

			/*
			 * TODO Optimise this code with the similar code above
			 * for entering the M0_FOPH_INIT state for the
			 * second time and beyond.
			 */
			if (!cob_is_md(cob_op) &&
			    fop_type == M0_COB_OP_DELETE) {
				if (cob_op->fco_stob->so_ref > 1)
					return M0_RC(
						cob_stob_ref_drop_wait(fom));
				else
					/* Mark the stob state as CSS_DELETE */
					m0_stob_delete_mark(cob_op->fco_stob);
			}

			if (m0_is_cob_truncate_fop(fop)) {
				struct m0_fop_cob_truncate *trunc =
							m0_fop_data(fop);
				if (trunc->ct_io_ivec.ci_nr > 0) {
					uint32_t bshift;

					bshift = m0_stob_block_shift(
							cob_op->fco_stob);
					m0_indexvec_wire2mem(
						&trunc->ct_io_ivec,
						trunc->ct_io_ivec.ci_nr,
						bshift, &cob_op->fco_range);
				}
				M0_LOG(M0_DEBUG, "trunc count%"PRIu64,
						trunc->ct_size);
			}
			break;
		case M0_FOPH_TXN_OPEN:
			switch (fop_type) {
			case M0_COB_OP_CREATE:
				cob_stob_create_credit(fom);
				break;
			case M0_COB_OP_DELETE:
			case M0_COB_OP_TRUNCATE:
				rc = cob_stob_delete_credit(fom);
				if (rc == -EAGAIN)
					rc = 0;
				if (rc != 0)
					goto tail;
				break;
			default:
				M0_IMPOSSIBLE("Invalid fop type!");
				break;
			}
			break;
		case M0_FOPH_QUEUE_REPLY:
			/*
			 * When an operation can't be done in a single
			 * transaction due to insufficient credits, it is split
			 * into multiple trasactions.
			 * As the the operation is incomplete, skip the sending
			 * of reply and reinitialise the trasaction.
			 * i.e move fom phase to M0_FOPH_TXN_COMMIT_WAIT and
			 * then to M0_FOPH_TXN_INIT.
			 */
			if (!cob_op->fco_is_done && m0_fom_rc(fom) == 0) {
				m0_fom_phase_set(fom, M0_FOPH_TXN_COMMIT_WAIT);
				return M0_FSO_AGAIN;
			}
			break;
		case M0_FOPH_TXN_COMMIT_WAIT:
			if (!cob_op->fco_is_done && m0_fom_rc(fom) == 0) {
				rc = m0_fom_tx_commit_wait(fom);
				if (rc == M0_FSO_AGAIN) {
					M0_SET0(m0_fom_tx(fom));
					m0_fom_phase_set(fom, M0_FOPH_TXN_INIT);
				} else
					return M0_RC(rc);
			}
			break;
		}
		rc = m0_fom_tick_generic(fom);
		return M0_RC(rc);
	}
	ops = m0_fop_name(fop);

	switch (m0_fom_phase(fom)) {
	case M0_FOPH_COB_OPS_PREPARE:
		m0_fom_phase_set(fom, M0_FOPH_COB_OPS_EXECUTE);
		reply->cor_rc = 0;
		return M0_RC(M0_FSO_AGAIN);
	case M0_FOPH_COB_OPS_EXECUTE:
		fop_type = cob_op->fco_fop_type;
		M0_LOG(M0_DEBUG, "Cob %s operation for "FID_F"/%x "FID_F" for %s",
				ops, FID_P(&cob_op->fco_cfid),
				cob_op->fco_cob_idx,
				FID_P(&cob_op->fco_gfid),
				cob_is_md(cob_op) ? "MD" : "IO");
		m0_md_cob_wire2mem(&attr, &common->c_body);
		if (fop_type == M0_COB_OP_CREATE) {
			rc = cob_stob_create(fom, &attr);
		} else if (fop_type == M0_COB_OP_DELETE) {
			if (cob_op->fco_is_done) {
				rc = cob_is_md(cob_op) ? 0 :
					ce_stob_edit(fom, cob_op,
						     M0_COB_OP_DELETE);
				rc = rc ?: cd_cob_delete(fom, cob_op, &attr);
				if (rc == 0 && cob_op->fco_recreate) {
					cob_op->fco_fop_type = M0_COB_OP_CREATE;
					M0_CNT_INC(attr.ca_nlink);
					rc = cob_stob_create(fom, &attr);
				}
			} else
				rc = ce_stob_edit(fom, cob_op,
						  M0_COB_OP_TRUNCATE);
		} else {
			rc = ce_stob_edit(fom, cob_op, M0_COB_OP_TRUNCATE);
			if (cob_op->fco_is_done)
				m0_storage_dev_stob_put(m0_cs_storage_devs_get(),
							cob_op->fco_stob);
		}

		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	        M0_LOG(M0_DEBUG, "Cob %s operation finished with %d", ops, rc);
		break;
	default:
		M0_IMPOSSIBLE("Invalid phase for cob create/delete fom.");
		rc = -EINVAL;
		m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	}

tail:
	if (rc != 0)
		reply->cor_rc = rc;
	cob_tick_tail(fom, r_common);
	return M0_RC(M0_FSO_AGAIN);
}

M0_INTERNAL int m0_cc_stob_cr_credit(struct m0_stob_id *sid,
				     struct m0_be_tx_credit *accum)
{
	struct m0_stob_domain *sdom;

	M0_ENTRY("stob_fid="FID_F, FID_P(&sid->si_fid));

	sdom = m0_stob_domain_find_by_stob_id(sid);
	if (sdom == NULL) {
		return M0_ERR(-EINVAL);
	}

	m0_stob_create_credit(sdom, accum);

	return M0_RC(0);
}

M0_INTERNAL int m0_cc_stob_create(struct m0_fom *fom, struct m0_stob_id *sid)
{
	int rc;

	M0_ENTRY("stob create fid="FID_F, FID_P(&sid->si_fid));
	rc = m0_storage_dev_stob_create(m0_cs_storage_devs_get(),
					sid, &fom->fo_tx);
	return M0_RC(rc);
}

static struct m0_cob_domain *cdom_get(const struct m0_fom *fom)
{
	struct m0_reqh_io_service *ios;

	M0_PRE(fom != NULL);

	ios = container_of(fom->fo_service, struct m0_reqh_io_service,
			   rios_gen);

	return ios->rios_cdom;
}

M0_INTERNAL int m0_cc_cob_nskey_make(struct m0_cob_nskey **nskey,
				     const struct m0_fid *gfid,
				     uint32_t cob_idx)
{
	char     nskey_name[M0_FID_STR_LEN] = { 0 };
	uint32_t nskey_name_len;

	M0_PRE(m0_fid_is_set(gfid));

	nskey_name_len = sprintf(nskey_name, "%u", cob_idx);

	return m0_cob_nskey_make(nskey, gfid, nskey_name, nskey_name_len);
}

static int cc_md_cob_nskey_make(struct m0_cob_nskey **nskey,
				const struct m0_fid *gfid)
{
	M0_PRE(m0_fid_is_set(gfid));

	return m0_cob_nskey_make(nskey, gfid, (const char*)gfid, sizeof *gfid);
}

static void cob_op_credit(struct m0_fom *fom, enum m0_cob_op opcode,
			  struct m0_be_tx_credit *accum)
{
	struct m0_cob_domain *cdom;

	M0_PRE(fom != NULL);
	cdom = cdom_get(fom);
	M0_ASSERT(cdom != NULL);
	m0_cob_tx_credit(cdom, opcode, accum);
}

enum cob_attr_operation {
	COB_ATTR_GET,
	COB_ATTR_SET
};

static int cob_attr_get(struct m0_cob        *cob,
			struct m0_cob_attr   *attr)
{
	/* copy attr from cob to @attr */
	M0_SET0(attr);
	attr->ca_valid = 0;
	attr->ca_tfid = cob->co_nsrec.cnr_fid;
	attr->ca_pfid = cob->co_nskey->cnk_pfid;

	/*
	 * Copy permissions and owner info into rep.
	 */
	if (cob->co_flags & M0_CA_OMGREC) {
		attr->ca_valid |= M0_COB_UID | M0_COB_GID | M0_COB_MODE;
		attr->ca_uid  = cob->co_omgrec.cor_uid;
		attr->ca_gid  = cob->co_omgrec.cor_gid;
		attr->ca_mode = cob->co_omgrec.cor_mode;
	}

	/*
	 * Copy nsrec fields into response.
	 */
	if (cob->co_flags & M0_CA_NSREC) {
		attr->ca_valid |= M0_COB_ATIME | M0_COB_CTIME   | M0_COB_MTIME |
				  M0_COB_SIZE  | M0_COB_BLKSIZE | M0_COB_BLOCKS|
				  M0_COB_LID | M0_COB_PVER;
		attr->ca_atime   = cob->co_nsrec.cnr_atime;
		attr->ca_ctime   = cob->co_nsrec.cnr_ctime;
		attr->ca_mtime   = cob->co_nsrec.cnr_mtime;
		attr->ca_blksize = cob->co_nsrec.cnr_blksize;
		attr->ca_blocks  = cob->co_nsrec.cnr_blocks;
		attr->ca_nlink   = cob->co_nsrec.cnr_nlink;
		attr->ca_size    = cob->co_nsrec.cnr_size;
		attr->ca_lid     = cob->co_nsrec.cnr_lid;
		attr->ca_pver    = cob->co_nsrec.cnr_pver;
	}
	return 0;
}

static int cob_locate(const struct m0_fom *fom, struct m0_cob **cob_out)
{
	struct m0_cob_oikey   oikey;
	struct m0_cob_domain *cdom;
	struct m0_fid         fid;
	struct m0_fom_cob_op *cob_op;
	struct m0_cob        *cob;
	int                   rc;

	cob_op = cob_fom_get(fom);
	M0_ASSERT(cob_op != NULL);
	cdom = cdom_get(fom);
	M0_ASSERT(cdom != NULL);
	cob_fom_stob2fid_map(cob_op, &fid);
	m0_cob_oikey_make(&oikey, &fid, 0);
	rc = m0_cob_locate(cdom, &oikey, 0, &cob);
	if (rc == 0)
		*cob_out = cob;
	return rc;
}

static int cob_attr_op(struct m0_fom          *fom,
		       struct m0_fom_cob_op   *gop,
		       struct m0_cob_attr     *attr,
		       enum cob_attr_operation op)
{
	int              rc;
	struct m0_cob   *cob;
	struct m0_be_tx *tx;
	uint32_t valid = attr->ca_valid;

	M0_PRE(fom != NULL);
	M0_PRE(gop != NULL);
	M0_PRE(op == COB_ATTR_GET || op == COB_ATTR_SET);

	M0_LOG(M0_DEBUG, "cob attr for "FID_F"/%x "FID_F" %s",
			 FID_P(&gop->fco_cfid), gop->fco_cob_idx,
			 FID_P(&gop->fco_gfid),
			 cob_is_md(gop) ? "MD" : "IO");

	rc = cob_locate(fom, &cob);
	if (rc != 0) {
		if (valid & M0_COB_NLINK)
			M0_LOG(M0_DEBUG, "nlink = %u", attr->ca_nlink);
		/*
		 * CROW setattr must have non-zero nlink set
		 * to avoid creation of invalid cobs.
		 */
		if (rc != -ENOENT || !(gop->fco_flags & M0_IO_FLAG_CROW) ||
		    !(valid & M0_COB_NLINK) || attr->ca_nlink == 0)
			return M0_RC(rc);
		M0_ASSERT(op == COB_ATTR_SET);
		rc = cob_stob_create(fom, attr) ?: cob_locate(fom, &cob);
		if (rc != 0)
			return M0_RC(rc);
	}

	M0_ASSERT(cob != NULL);
	M0_ASSERT(cob->co_nsrec.cnr_nlink != 0);
	switch (op) {
	case COB_ATTR_GET:
		rc = cob_attr_get(cob, attr);
		M0_ASSERT(ergo(attr->ca_valid & M0_COB_PVER,
			       m0_fid_is_set(&attr->ca_pver) &&
			       m0_fid_is_valid(&attr->ca_pver)));
		break;
	case COB_ATTR_SET:
		tx = m0_fom_tx(fom);
		rc = m0_cob_setattr(cob, attr, tx);
		break;
	}
	m0_cob_put(cob);

	M0_LOG(M0_DEBUG, "Cob attr: %d rc: %d", op, rc);
	return M0_RC(rc);
}

static int cob_getattr(struct m0_fom        *fom,
		       struct m0_fom_cob_op *gop,
		       struct m0_cob_attr   *attr)
{
	return cob_attr_op(fom, gop, attr, COB_ATTR_GET);
}

static int cob_setattr(struct m0_fom        *fom,
		       struct m0_fom_cob_op *gop,
		       struct m0_cob_attr   *attr)
{
	return cob_attr_op(fom, gop, attr, COB_ATTR_SET);
}

static int cc_cob_create(struct m0_fom            *fom,
			 struct m0_fom_cob_op     *cc,
			 const struct m0_cob_attr *attr)
{
	struct m0_cob_domain *cdom;
	struct m0_be_tx	     *tx;
	int                   rc;

	M0_PRE(fom != NULL);
	M0_PRE(cc != NULL);

	cdom = cdom_get(fom);
	M0_ASSERT(cdom != NULL);
	tx = m0_fom_tx(fom);
	rc = m0_cc_cob_setup(cc, cdom, attr, tx);

	return M0_RC(rc);
}

M0_INTERNAL int m0_cc_cob_setup(struct m0_fom_cob_op     *cc,
				struct m0_cob_domain     *cdom,
				const struct m0_cob_attr *attr,
				struct m0_be_tx	         *ctx)
{
	int		      rc;
	struct m0_cob	     *cob;
	struct m0_cob_nskey  *nskey = NULL;
	struct m0_cob_nsrec   nsrec = {};

	M0_PRE(cc != NULL);
	M0_PRE(cdom != NULL);

	rc = m0_cob_alloc(cdom, &cob);
	if (rc != 0)
		return M0_RC(rc);

	rc = cob_is_md(cc) ?
		cc_md_cob_nskey_make(&nskey, &cc->fco_gfid) :
		m0_cc_cob_nskey_make(&nskey, &cc->fco_gfid, cc->fco_cob_idx);
	if (rc != 0) {
		m0_cob_put(cob);
		return M0_RC(rc);
	}

	cob_fom_stob2fid_map(cc, &nsrec.cnr_fid);
	m0_cob_nsrec_init(&nsrec);
	nsrec.cnr_nlink   = attr->ca_nlink;
	nsrec.cnr_size    = attr->ca_size;
	nsrec.cnr_blksize = attr->ca_blksize;
	nsrec.cnr_blocks  = attr->ca_blocks;
	nsrec.cnr_atime   = attr->ca_atime;
	nsrec.cnr_mtime   = attr->ca_mtime;
	nsrec.cnr_ctime   = attr->ca_ctime;
	nsrec.cnr_lid     = attr->ca_lid;
	nsrec.cnr_pver    = attr->ca_pver;

	rc = m0_cob_create(cob, nskey, &nsrec, NULL, NULL, ctx);
	if (rc != 0) {
	        /*
	         * Cob does not free nskey and fab rec on errors. We need to do
		 * so ourself. In case cob created successfully, it frees things
		 * on last put.
	         */
		m0_free(nskey);
	}
	m0_cob_put(cob);

	return M0_RC(rc);
}

static void cd_fom_fini(struct m0_fom *fom)
{
	struct m0_fom_cob_op *cfom;

	M0_PRE(fom != NULL);

	cfom = cob_fom_get(fom);
	addb2_add_cob_fom_attrs(cfom);
	m0_indexvec_free(&cfom->fco_range);
	m0_indexvec_free(&cfom->fco_want);
	m0_indexvec_free(&cfom->fco_got);
	m0_fom_fini(fom);
	m0_free(cfom);
}

static void cob_getattr_fom_fini(struct m0_fom *fom)
{
	struct m0_fom_cob_op *cfom;
	M0_PRE(fom != NULL);

	cfom = cob_fom_get(fom);
	m0_fom_fini(fom);
	m0_free(cfom);
}

static void cob_setattr_fom_fini(struct m0_fom *fom)
{
	struct m0_fom_cob_op *cfom;
	M0_PRE(fom != NULL);

	cfom = cob_fom_get(fom);
	m0_fom_fini(fom);
	m0_free(cfom);
}

static int cd_cob_delete(struct m0_fom            *fom,
			 struct m0_fom_cob_op     *cd,
			 const struct m0_cob_attr *attr)
{
	int                   rc;
	struct m0_cob        *cob;

	M0_PRE(fom != NULL);
	M0_PRE(cd != NULL);

        M0_LOG(M0_DEBUG, "Deleting cob for "FID_F"/%x",
	       FID_P(&cd->fco_cfid), cd->fco_cob_idx);

	rc = cob_locate(fom, &cob);
	if (rc != 0)
		return M0_RC(rc);

	M0_ASSERT(cob != NULL);
	M0_CNT_DEC(cob->co_nsrec.cnr_nlink);
	M0_ASSERT(attr->ca_nlink == 0);
	M0_ASSERT(cob->co_nsrec.cnr_nlink == 0);

	rc = m0_cob_delete(cob, m0_fom_tx(fom));
	if (rc == 0)
		M0_LOG(M0_DEBUG, "Cob deleted successfully.");

	return M0_RC(rc);
}

static void ce_stob_destroy_credit(struct m0_fom_cob_op *cc,
				   struct m0_be_tx_credit *accum)
{
	m0_stob_destroy_credit(cc->fco_stob, accum);
}

static int ce_stob_edit_credit(struct m0_fom *fom, struct m0_fom_cob_op *cc,
			       struct m0_be_tx_credit *accum, uint32_t cot)
{
	struct m0_stob *stob;
	int             rc;
	uint32_t        idx = cc->fco_range_idx;

	M0_PRE(M0_IN(cot, (M0_COB_OP_DELETE, M0_COB_OP_TRUNCATE)));
	stob = cc->fco_stob;
	M0_ASSERT(stob != NULL);
	M0_ASSERT(M0_IN(m0_stob_state_get(stob), (CSS_EXISTS, CSS_DELETE)));

	cc->fco_want.iv_index[0] = cc->fco_range.iv_index[idx];
	cc->fco_want.iv_vec.v_count[0] = cc->fco_range.iv_vec.v_count[idx];
	rc = m0_stob_punch_credit(stob, &cc->fco_want, &cc->fco_got, accum);
	if (rc != 0)
		return M0_RC(rc);

	if (cot == M0_COB_OP_TRUNCATE) {
		cc->fco_range.iv_index[idx] += cc->fco_got.iv_vec.v_count[0];
		cc->fco_range.iv_vec.v_count[idx] -=
						cc->fco_got.iv_vec.v_count[0];
		if (cc->fco_range.iv_vec.v_count[idx] == 0) {
			cc->fco_range_idx++;
			if (cc->fco_range_idx == cc->fco_range.iv_vec.v_nr)
				cc->fco_is_done = true;
		}
	} else if (cc->fco_got.iv_vec.v_count[0] != M0_BINDEX_MAX + 1)
			rc = -EAGAIN;

	/* To update the cob size post truncate. */
	if (rc == 0)
		cob_op_credit(fom, M0_COB_OP_UPDATE, accum);

	return M0_RC(rc);
}

static int ce_stob_edit(struct m0_fom *fom, struct m0_fom_cob_op *cd,
			uint32_t cot)
{
	struct m0_storage_devs *devs = m0_cs_storage_devs_get();
	struct m0_stob         *stob = cd->fco_stob;
	int                     rc;

	M0_PRE(M0_IN(cot, (M0_COB_OP_DELETE, M0_COB_OP_TRUNCATE)));
	M0_PRE(!cob_is_md(cd));

	M0_PRE(stob != NULL);
	M0_PRE(ergo((cd->fco_fop_type == M0_COB_OP_DELETE &&
		     cot == M0_COB_OP_DELETE),
		    (m0_stob_state_get(stob) == CSS_DELETE &&
		     stob->so_ref == 1)));
	M0_PRE(ergo((cd->fco_fop_type == M0_COB_OP_DELETE &&
		     cot == M0_COB_OP_TRUNCATE &&
		     stob->so_ref > 1),
		    m0_stob_state_get(stob) == CSS_EXISTS));
	M0_PRE(ergo((cd->fco_fop_type == M0_COB_OP_DELETE &&
		     cot == M0_COB_OP_TRUNCATE &&
		     stob->so_ref == 1),
		    m0_stob_state_get(stob) == CSS_DELETE));
	M0_PRE(ergo(cd->fco_fop_type == M0_COB_OP_TRUNCATE,
		    (cot == M0_COB_OP_TRUNCATE &&
		     m0_stob_state_get(stob) == CSS_EXISTS)));

	M0_ENTRY("fom %p, fom_type %d, cot %d, stob %p, "FID_F,
		 fom, cd->fco_fop_type, cot, stob, FID_P(&stob->so_id.si_fid));

	rc = m0_stob_punch(stob, &cd->fco_got, &fom->fo_tx);
	if (rc != 0)
		return M0_ERR(rc);

	if (cot == M0_COB_OP_DELETE)
		rc = m0_storage_dev_stob_destroy(devs, stob, &fom->fo_tx);

	return M0_RC(rc);
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
