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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 07-Mar-2016
 */


/**
 * @addtogroup cas
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CAS

#include "ut/ut.h"
#include "lib/misc.h"                     /* M0_SET0 */
#include "lib/finject.h"
#include "lib/semaphore.h"
#include "lib/byteorder.h"
#include "fop/fop.h"
#include "reqh/reqh.h"
#include "reqh/reqh_service.h"
#include "be/ut/helper.h"                 /* m0_be_ut_backend */

#include "cas/cas.h"
#include "cas/cas_xc.h"
#include "rpc/at.h"
#include "fdmi/fdmi.h"
#include "rpc/rpc_machine.h"

#define IFID(x, y) M0_FID_TINIT('i', (x), (y))
#define TFID(x, y) M0_FID_TINIT('T', (x), (y))

enum { N = 4096 };

struct meta_rec {
	struct m0_cas_id cid;
	uint64_t         rc;
};

static struct m0_reqh          reqh;
static struct m0_be_ut_backend be;
static struct m0_be_seg       *seg0;
static struct m0_reqh_service *cas;
static struct m0_reqh_service *fdmi;
static struct m0_rpc_machine   rpc_machine;
static struct m0_cas_rep       rep;
static struct m0_cas_rec       repv[N];
static struct m0_fid           ifid = IFID(2, 3);
static bool                    mt;

extern void (*cas__ut_cb_done)(struct m0_fom *fom);
extern void (*cas__ut_cb_fini)(struct m0_fom *fom);

static void cb_done(struct m0_fom *fom);
static void cb_fini(struct m0_fom *fom);

static int cid_enc(struct m0_cas_id *cid, struct m0_rpc_at_buf *at_buf)
{
	int           rc;
	struct m0_buf buf;

	M0_PRE(cid != NULL);
	M0_PRE(at_buf != NULL);

	m0_rpc_at_init(at_buf);
	rc = m0_xcode_obj_enc_to_buf(&M0_XCODE_OBJ(m0_cas_id_xc, cid),
				     &buf.b_addr, &buf.b_nob);
	M0_UT_ASSERT(rc == 0);

	at_buf->ab_type  = M0_RPC_AT_INLINE;
	at_buf->u.ab_buf = buf;
	return rc;
}

static void rep_clear(void)
{
	int i;

	rep.cgr_rc  = -EINVAL;
	rep.cgr_rep.cr_nr  = 0;
	rep.cgr_rep.cr_rec = repv;
	for (i = 0; i < ARRAY_SIZE(repv); ++i) {
		m0_rpc_at_fini(&repv[i].cr_key);
		m0_rpc_at_fini(&repv[i].cr_val);
		repv[i].cr_rc = -EINVAL;
	}
}

static int at_inline_fill(struct m0_rpc_at_buf *dst, struct m0_rpc_at_buf *src)
{
	dst->ab_type = src->ab_type;
	dst->u.ab_buf = M0_BUF_INIT0;
	return m0_buf_copy(&dst->u.ab_buf, &src->u.ab_buf);
}

static void reqh_init(bool mkfs, bool use_small_credits)
{
	struct m0_be_domain_cfg cfg = {};
	int                     result;

	M0_SET0(&reqh);
	M0_SET0(&be);
	m0_fi_enable("cas_in_ut", "ut");
	seg0 = m0_be_domain_seg0_get(&be.but_dom);
	result = M0_REQH_INIT(&reqh,
			      .rhia_db      = seg0,
			      .rhia_mdstore = (void *)1,
			      .rhia_fid     = &g_process_fid);
	M0_UT_ASSERT(result == 0);
	be.but_dom_cfg.bc_engine.bec_reqh = &reqh;
	m0_be_ut_backend_cfg_default(&cfg);
	if (use_small_credits || m0_ut_small_credits())
		cfg.bc_engine.bec_tx_size_max =
			M0_BE_TX_CREDIT(6 << 10, 5 << 18);
	result = m0_be_ut_backend_init_cfg(&be, &cfg, mkfs);
	M0_ASSERT(result == 0);
}

static void _init(bool mkfs, bool use_small_credits)
{
	int result;

	/* Check validity of IFID definition. */
	M0_UT_ASSERT(m0_cas_index_fid_type.ft_id == 'i');
	reqh_init(mkfs, use_small_credits);

	m0_reqh_rpc_mach_tlink_init_at_tail(&rpc_machine,
					    &reqh.rh_rpc_machines);

	result = m0_reqh_service_allocate(&fdmi, &m0_fdmi_service_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(fdmi, &reqh, NULL);
	result = m0_reqh_service_start(fdmi);
	M0_UT_ASSERT(result == 0);

	result = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	m0_reqh_service_start(cas);
	m0_reqh_start(&reqh);
	cas__ut_cb_done = &cb_done;
	cas__ut_cb_fini = &cb_fini;
}

static void init(void)
{
	_init(true, false);
}

static void service_stop(void)
{
	m0_reqh_rpc_mach_tlist_pop(&reqh.rh_rpc_machines);
	m0_reqh_service_prepare_to_stop(fdmi);
	m0_reqh_idle_wait_for(&reqh, fdmi);
	m0_reqh_service_stop(fdmi);
	m0_reqh_service_fini(fdmi);

	m0_reqh_service_prepare_to_stop(cas);
	m0_reqh_idle_wait_for(&reqh, cas);
	m0_reqh_service_stop(cas);
	m0_reqh_service_fini(cas);
}

static void fini(void)
{
	service_stop();
	m0_be_ut_backend_fini(&be);
	m0_fi_disable("cas_in_ut", "ut");
	rep_clear();
	cas__ut_cb_done = NULL;
	cas__ut_cb_fini = NULL;
}

static void reinit_nomkfs(void)
{
	fini();
	_init(false, false);
}

/**
 * "init-fini" test: initialise and finalise a cas service.
 */
static void init_fini(void)
{
	init();
	fini();
}

/**
 * Different fails during service startup.
 */
static void init_fail(void)
{
	int rc;

	reqh_init(true, false);

	/* Failure to add meta-index to segment dictionary. */
	rc = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_fi_enable_once("m0_be_seg_dict_insert", "dict_insert_fail");
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	rc = m0_reqh_service_start(cas);
	M0_UT_ASSERT(rc == -ENOENT);
	m0_reqh_service_fini(cas);

	/* Failure to create meta-index. */
	rc = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_fi_enable_once("ctg_create", "ctg_create_failure");
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	rc = m0_reqh_service_start(cas);
	M0_UT_ASSERT(rc == -EFAULT);
	m0_reqh_service_fini(cas);

	/* Failure to add meta-index to itself. */
	rc = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_fi_enable_once("btree_save", "already_exists");
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	rc = m0_reqh_service_start(cas);
	M0_UT_ASSERT(rc == -EEXIST);
	m0_reqh_service_fini(cas);

	/* Failure to create catalogue-index index. */
	rc = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_fi_enable_off_n_on_m("ctg_create", "ctg_create_failure",
				1, 1);
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	rc = m0_reqh_service_start(cas);
	m0_fi_disable("ctg_create", "ctg_create_failure");
	M0_UT_ASSERT(rc == -EFAULT);
	m0_reqh_service_fini(cas);

	/* Failure to add catalogue-index index to meta-index. */
	rc = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_fi_enable_off_n_on_m("btree_save", "already_exists",
				1, 1);
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	rc = m0_reqh_service_start(cas);
	m0_fi_disable("btree_save", "already_exists");
	M0_UT_ASSERT(rc == -EEXIST);
	m0_reqh_service_fini(cas);

	/* Normal start (fdmi service is needed). */
	m0_reqh_rpc_mach_tlink_init_at_tail(&rpc_machine,
					    &reqh.rh_rpc_machines);

	rc = m0_reqh_service_allocate(&fdmi, &m0_fdmi_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(fdmi, &reqh, NULL);
	rc = m0_reqh_service_start(fdmi);
	M0_UT_ASSERT(rc == 0);

	rc = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	rc = m0_reqh_service_start(cas);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_start(&reqh);

	fini();
}

/**
 * Test CAS module re-initialisation.
 */
M0_INTERNAL int  m0_cas_module_init(void);
M0_INTERNAL void m0_cas_module_fini(void);

static void reinit(void)
{
	/*
	 * CAS module is already initialised as part of general mero
	 * initialisation in mero/init.c, see m0_cas_module_init().
	 * Finalise it first and then initialise again.
	 */
	m0_cas_module_fini();
	m0_cas_module_init();
}

/**
 * Test service re-start with existing meta-index.
 */
static void restart(void)
{
	int result;

	init();
	service_stop();

	m0_reqh_rpc_mach_tlink_init_at_tail(&rpc_machine,
					    &reqh.rh_rpc_machines);

	result = m0_reqh_service_allocate(&fdmi, &m0_fdmi_service_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(fdmi, &reqh, NULL);
	result = m0_reqh_service_start(fdmi);
	M0_UT_ASSERT(result == 0);

	result = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	m0_reqh_service_start(cas);
	fini();
}

static void fop_release(struct m0_ref *ref)
{
}

struct fopsem {
	struct m0_semaphore fs_end;
	struct m0_fop       fs_fop;
};

static void cb_done(struct m0_fom *fom)
{
	struct m0_cas_rep *reply = m0_fop_data(fom->fo_rep_fop);
	int                i;
	struct fopsem     *fs = M0_AMB(fs, fom->fo_fop, fs_fop);

	M0_UT_ASSERT(reply != NULL);
	M0_UT_ASSERT(reply->cgr_rep.cr_nr <= ARRAY_SIZE(repv));
	rep.cgr_rc         = reply->cgr_rc;
	rep.cgr_rep.cr_nr  = reply->cgr_rep.cr_nr;
	rep.cgr_rep.cr_rec = repv;
	for (i = 0; !mt && i < rep.cgr_rep.cr_nr; ++i) {
		struct m0_cas_rec *rec = &reply->cgr_rep.cr_rec[i];
		int                rc;

		repv[i].cr_hint = rec->cr_hint;
		repv[i].cr_rc   = rec->cr_rc;
		rc = at_inline_fill(&repv[i].cr_key, &rec->cr_key);
		M0_UT_ASSERT(rc == 0);
		rc = at_inline_fill(&repv[i].cr_val, &rec->cr_val);
		M0_UT_ASSERT(rc == 0);
	}
	m0_ref_put(&fom->fo_fop->f_ref);
	fom->fo_fop = NULL;
	m0_ref_put(&fom->fo_rep_fop->f_ref);
	m0_fop_release(&fom->fo_rep_fop->f_ref);
	fom->fo_rep_fop = NULL;
	{
		struct m0_tlink *link = &fom->fo_tx.tx_betx.t_engine_linkage;
		M0_ASSERT(ergo(link->t_link.ll_next != NULL,
			       !m0_list_link_is_in(&link->t_link)));
	}
	m0_semaphore_up(&fs->fs_end);
}

static void cb_fini(struct m0_fom *fom)
{
}

static void fop_submit(struct m0_fop_type *ft, const struct m0_fid *index,
		       struct m0_cas_rec *rec)
{
	int              result;
	struct fopsem    fs;
	struct m0_cas_op op = {
		.cg_id  = { .ci_fid = *index },
		.cg_rec = { .cr_rec = rec }
	};

	M0_UT_ASSERT(cas__ut_cb_done == &cb_done);
	M0_UT_ASSERT(cas__ut_cb_fini == &cb_fini);
	while (rec[op.cg_rec.cr_nr].cr_rc != ~0ULL)
		++ op.cg_rec.cr_nr;
	m0_fop_init(&fs.fs_fop, ft, &op, &fop_release);
	fs.fs_fop.f_item.ri_rmachine = (void *)1;
	m0_semaphore_init(&fs.fs_end, 0);
	rep_clear();
	result = m0_reqh_fop_handle(&reqh, &fs.fs_fop);
	M0_UT_ASSERT(result == 0);
	m0_semaphore_down(&fs.fs_end);
	/**
	 * @note There is no need to finalise the locally allocated fop: rpc was
	 * never used, so there are no resources to free.
	 */
	m0_semaphore_fini(&fs.fs_end);
}

enum {
	BSET   = true,
	BUNSET = false,
	BANY   = 2
};

enum {
	/**
	 * Record value in request is empty (has 0 bytes length). It is a valid
	 * value that is acceptable by CAS service.
	 */
	EMPTYVAL = 0,

	/**
	 * AT buffer with record value in request is unset (has M0_RPC_AT_EMPTY
	 * type).
	 */
	NOVAL    = (uint64_t)-1,
};

static void meta_fop_submit(struct m0_fop_type *fopt,
			    struct meta_rec *meta_recs,
			    int meta_recs_num)
{
	int                i;
	int                rc;
	struct m0_cas_rec *recs;

	M0_ALLOC_ARR(recs, meta_recs_num + 1);
	M0_UT_ASSERT(recs != NULL);
	for (i = 0; i < meta_recs_num; i++) {
		rc = cid_enc(&meta_recs[i].cid, &recs[i].cr_key);
		M0_UT_ASSERT(rc == 0);
		recs[i].cr_rc = meta_recs[i].rc;
	}
	recs[meta_recs_num] = (struct m0_cas_rec){ .cr_rc = ~0ULL };

	fop_submit(fopt, &m0_cas_meta_fid, recs);

	for (i = 0; i < meta_recs_num; i++)
		m0_rpc_at_fini(&recs[i].cr_key);
}

static bool rec_check(const struct m0_cas_rec *rec, int rc, int key, int val)
{
	return ergo(rc  != BANY, rc == rec->cr_rc) &&
	       ergo(key != BANY, m0_rpc_at_is_set(&rec->cr_key) == key) &&
	       ergo(val != BANY, m0_rpc_at_is_set(&rec->cr_val) == val);
}

static bool rep_check(int recno, uint64_t rc, int key, int val)
{
	return rec_check(&rep.cgr_rep.cr_rec[recno], rc, key, val);
}

static void meta_cid_submit(struct m0_fop_type *fopt,
			    struct m0_cas_id *cid)
{
	int                  rc;
	struct m0_rpc_at_buf at_buf;

	rc = cid_enc(cid, &at_buf);
	M0_UT_ASSERT(rc == 0);
	fop_submit(fopt, &m0_cas_meta_fid,
		   (struct m0_cas_rec[]) {
			   { .cr_key = at_buf },
			   { .cr_rc = ~0ULL } });

	M0_UT_ASSERT(ergo(!mt, rep.cgr_rc == 0));
	M0_UT_ASSERT(ergo(!mt, rep.cgr_rep.cr_nr == 1));
	m0_rpc_at_fini(&at_buf);
}

static void meta_fid_submit(struct m0_fop_type *fopt, struct m0_fid *fid)
{
	struct m0_cas_id cid = { .ci_fid = *fid };

	meta_cid_submit(fopt, &cid);
}

/**
 * Test meta-lookup of a non-existent index.
 */
static void meta_lookup_none(void)
{
	init();
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	fini();
}

/**
 * Test meta-lookup of 2 non-existent indices.
 */
static void meta_lookup_2none(void)
{
	struct m0_cas_id nonce0 = { .ci_fid = IFID(2, 3) };
	struct m0_cas_id nonce1 = { .ci_fid = IFID(2, 4) };

	init();
	meta_fop_submit(&cas_get_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 2);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	M0_UT_ASSERT(rep_check(1, -ENOENT, BUNSET, BUNSET));
	fini();
}

/**
 * Test meta-lookup of multiple non-existent indices.
 */
static void meta_lookup_Nnone(void)
{
	struct meta_rec nonce[N]  = {};
	int             i;

	for (i = 0; i < ARRAY_SIZE(nonce); ++i)
		nonce[i].cid.ci_fid = IFID(2, 3 + i);

	init();
	meta_fop_submit(&cas_get_fopt, nonce, N);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == N);
	M0_UT_ASSERT(m0_forall(i, N, rep_check(i, -ENOENT, BUNSET, BUNSET)));
	fini();
}

/**
 * Test index creation.
 */
static void create(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	fini();
}

/**
 * Test component catalogue creation.
 */
static void cctg_create(void)
{
	int                  rc;
	struct m0_cas_id     cid1 = { .ci_fid = TFID(1, 1) };
	struct m0_cas_id     cid2 = { .ci_fid = TFID(1, 2) };
	struct m0_dix_ldesc *desc = NULL;
	struct m0_ext        range[] = {
		{ .e_start = 1, .e_end = 3 },
		{ .e_start = 5, .e_end = 7 },
		{ .e_start = 9, .e_end = 11 },
	};

	init();
	cid1.ci_layout.dl_type = DIX_LTYPE_DESCR;
	cid2.ci_layout.dl_type = DIX_LTYPE_DESCR;
	/* Submit CID with empty imask ranges. */
	meta_cid_submit(&cas_put_fopt, &cid1);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	/* Submit CID with non-empty imask ranges. */
	desc = &cid2.ci_layout.u.dl_desc;
	rc = m0_dix_ldesc_init(desc, range, ARRAY_SIZE(range), HASH_FNC_CITY,
			       &M0_FID_INIT(10, 10));
	M0_UT_ASSERT(rc == 0);
	meta_cid_submit(&cas_put_fopt, &cid2);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	m0_dix_ldesc_fini(desc);
	fini();
}

/**
 * Test component catalogue creation and lookup.
 */
static void cctg_create_lookup(void)
{
	int                  rc;
	struct m0_cas_id     cid = { .ci_fid = TFID(1, 1) };
	struct m0_dix_ldesc *desc = NULL;
	struct m0_ext        range[] = {
		{ .e_start = 1, .e_end = 3 },
		{ .e_start = 5, .e_end = 7 },
		{ .e_start = 9, .e_end = 11 },
	};

	init();
	cid.ci_layout.dl_type = DIX_LTYPE_DESCR;
	desc = &cid.ci_layout.u.dl_desc;
	rc = m0_dix_ldesc_init(desc, range, ARRAY_SIZE(range), HASH_FNC_CITY,
			       &M0_FID_INIT(10, 10));
	M0_UT_ASSERT(rc == 0);
	meta_cid_submit(&cas_put_fopt, &cid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_cid_submit(&cas_get_fopt, &cid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	m0_dix_ldesc_fini(desc);
	fini();
}

/**
 * Test component catalogue creation and deletion.
 */
static void cctg_create_delete(void)
{
	int                  rc;
	struct m0_cas_id     cid = { .ci_fid = TFID(1, 1) };
	struct m0_dix_ldesc *desc = NULL;
	struct m0_ext        range[] = {
		{ .e_start = 1, .e_end = 3 },
		{ .e_start = 5, .e_end = 7 },
		{ .e_start = 9, .e_end = 11 },
	};

	init();
	cid.ci_layout.dl_type = DIX_LTYPE_DESCR;

	/* Test operations with empty imask ranges. */
	meta_cid_submit(&cas_put_fopt, &cid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_cid_submit(&cas_del_fopt, &cid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_cid_submit(&cas_get_fopt, &cid);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));

	/* Test operations with non-empty imask ranges. */
	desc = &cid.ci_layout.u.dl_desc;
	rc = m0_dix_ldesc_init(desc, range, ARRAY_SIZE(range), HASH_FNC_CITY,
			       &M0_FID_INIT(10, 10));
	M0_UT_ASSERT(rc == 0);
	meta_cid_submit(&cas_put_fopt, &cid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_cid_submit(&cas_del_fopt, &cid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_cid_submit(&cas_get_fopt, &cid);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	m0_dix_ldesc_fini(desc);
	fini();
}

/**
 * Test index creation and index lookup.
 */
static void create_lookup(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	fini();
}

/**
 * Test index creation of the same index again.
 */
static void create_create(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -EEXIST, BUNSET, BUNSET));
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	fini();
}

/**
 * Test index deletion.
 */
static void create_delete(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fid_submit(&cas_del_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	fini();
}

/**
 * Test index deletion and re-creation.
 */
static void recreate(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fid_submit(&cas_del_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	fini();
}

/**
 * Test that meta-cursor returns an existing index.
 */
static void meta_cur_1(void)
{
	struct m0_cas_id cid = { .ci_fid = ifid };

	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fop_submit(&cas_cur_fopt,
			(struct meta_rec[]) {
				{ .cid = cid, .rc = 1 } },
			1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 1, BSET, BUNSET));
	M0_UT_ASSERT(m0_fid_eq(repv[0].cr_key.u.ab_buf.b_addr, &ifid));
	fini();
}

/**
 * Test that meta-cursor detects end of the tree.
 */
static void meta_cur_eot(void)
{
	struct m0_cas_id cid = { .ci_fid = ifid };

	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fop_submit(&cas_cur_fopt,
			(struct meta_rec[]) {
				{ .cid = cid, .rc = 2 } },
			1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 2);
	M0_UT_ASSERT(rep_check(0, 1, BSET, BUNSET));
	M0_UT_ASSERT(rep_check(1, -ENOENT, BUNSET, BUNSET));
	M0_UT_ASSERT(m0_fid_eq(repv[0].cr_key.u.ab_buf.b_addr, &ifid));
	fini();
}

/**
 * Test meta-cursor empty iteration.
 */
static void meta_cur_0(void)
{
	struct m0_cas_id cid = { .ci_fid = ifid };

	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fop_submit(&cas_cur_fopt,
			(struct meta_rec[]) {
				{ .cid = cid, .rc = 0 } },
			1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 0);
	fini();
}

/**
 * Test meta-cursor on empty meta-index.
 */
static void meta_cur_empty(void)
{
	struct m0_cas_id cid = { .ci_fid = ifid };

	init();
	meta_fop_submit(&cas_cur_fopt,
			(struct meta_rec[]) {
				{ .cid = cid, .rc = 1 } },
			1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	fini();
}

/**
 * Test meta-cursor with non-existent starting point.
 */
static void meta_cur_none(void)
{
	struct m0_cas_id cid = { .ci_fid = IFID(8,9) };

	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fop_submit(&cas_cur_fopt,
			(struct meta_rec[]) {
				{ .cid = cid, .rc = 4 } },
			1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 4);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	M0_UT_ASSERT(rep_check(1, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep_check(2, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep_check(3, 0, BUNSET, BUNSET));
	fini();
}

/**
 * Test meta-cursor starting from meta-index.
 */
static void meta_cur_all(void)
{
	struct m0_fid fid = IFID(1, 0);
	struct m0_cas_id cid = { .ci_fid = m0_cas_meta_fid };

	init();
	meta_fid_submit(&cas_put_fopt, &fid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	meta_fop_submit(&cas_cur_fopt,
			(struct meta_rec[]) {
				{ .cid = cid , .rc = 5 } },
			1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 5);
	/* meta-index record */
	M0_UT_ASSERT(rep_check(0, 1, BSET, BUNSET));
	/* catalogue-index index record */
	M0_UT_ASSERT(rep_check(1, 2, BSET, BUNSET));
	/* moved meta record */
	M0_UT_ASSERT(rep_check(2, 3, BSET, BUNSET));
	/* newly inserted record */
	M0_UT_ASSERT(rep_check(3, 4, BSET, BUNSET));
	/* nonexistent record */
	M0_UT_ASSERT(rep_check(4, -ENOENT, BUNSET, BUNSET));
	M0_UT_ASSERT(m0_fid_eq(repv[0].cr_key.u.ab_buf.b_addr,
			       &m0_cas_meta_fid));
	M0_UT_ASSERT(m0_fid_eq(repv[1].cr_key.u.ab_buf.b_addr,
			       &m0_cas_ctidx_fid));
	M0_UT_ASSERT(m0_fid_eq(repv[2].cr_key.u.ab_buf.b_addr,
			       &m0_cas_dead_index_fid));
	M0_UT_ASSERT(m0_fid_eq(repv[3].cr_key.u.ab_buf.b_addr, &fid));
	fini();
}

static struct m0_fop_type *ft[] = {
	&cas_put_fopt,
	&cas_get_fopt,
	&cas_del_fopt,
	&cas_cur_fopt
};

/**
 * Test random meta-operations.
 */
static void meta_random(void)
{
	enum { K = 10 };
	struct m0_fid     fid;
	struct meta_rec   mrecs[K];
	int               i;
	int               j;
	int               total;
	uint64_t          seed  = time(NULL)*time(NULL);

	init();
	for (i = 0; i < 50; ++i) {
		struct m0_fop_type *type = ft[m0_rnd64(&seed) % ARRAY_SIZE(ft)];

		memset(mrecs, 0, sizeof(mrecs));
		total = 0;
		/*
		 * Keep number of operations in a fop small to avoid too large
		 * transactions.
		 */
		for (j = 0; j < K; ++j) {
			fid = M0_FID_TINIT(m0_cas_index_fid_type.ft_id,
					   2, m0_rnd64(&seed) % 5);
			mrecs[j].cid.ci_fid = fid;
			if (type == &cas_cur_fopt) {
				int n = m0_rnd64(&seed) % 1000;
				if (total + n < ARRAY_SIZE(repv))
					total += mrecs[j].rc = n;
			}
		}
		meta_fop_submit(type, mrecs, K);
		M0_UT_ASSERT(rep.cgr_rc == 0);
		if (type != &cas_cur_fopt) {
			M0_UT_ASSERT(rep.cgr_rep.cr_nr == K);
			M0_UT_ASSERT(m0_forall(i, K, rep_check(i, BANY, BUNSET,
							       BUNSET)));
		} else {
			M0_UT_ASSERT(rep.cgr_rep.cr_nr == total);
		}
	}
	fini();
}

/**
 * Test garbage meta-operations.
 */
static void meta_garbage(void)
{
	enum { M = 16*1024 };
	uint64_t          buf[M + M];
	struct m0_cas_rec op[N + 1] = {};
	int               i;
	int               j;
	uint64_t          seed = time(NULL)*time(NULL);

	init();
	for (i = 0; i < ARRAY_SIZE(buf); ++i)
		buf[i] = m0_rnd64(&seed);
	for (i = 0; i < 200; ++i) {
		for (j = 0; j < 10; ++j) {
			m0_bcount_t size = m0_rnd64(&seed) % M;
			op[j].cr_key = (struct m0_rpc_at_buf) {
						.ab_type  = 1,
						.u.ab_buf = M0_BUF_INIT(size,
						   buf + m0_rnd64(&seed) % M)
					};

		}
		op[j].cr_rc = ~0ULL;
		fop_submit(ft[m0_rnd64(&seed) % ARRAY_SIZE(ft)],
			   &m0_cas_meta_fid, op);
		M0_UT_ASSERT(rep.cgr_rc == -EPROTO);
	}
	fini();
}

static void index_op_rc(struct m0_fop_type *ft, struct m0_fid *index,
			uint64_t key, uint64_t val, uint64_t rc)
{
	struct m0_buf no = M0_BUF_INIT(0, NULL);

	fop_submit(ft, index,
		   (struct m0_cas_rec[]) {
		   { .cr_key.u.ab_buf = key != 0 ?
					M0_BUF_INIT(sizeof key, &key) : no,
		     .cr_key.ab_type = key != 0 ? M0_RPC_AT_INLINE :
						   M0_RPC_AT_EMPTY,
		     .cr_val.u.ab_buf = !M0_IN(val, (NOVAL, EMPTYVAL)) ?
					M0_BUF_INIT(sizeof val, &val) : no,
		     .cr_val.ab_type = val != NOVAL ? M0_RPC_AT_INLINE :
						      M0_RPC_AT_EMPTY,
		     .cr_rc  = rc },
		   { .cr_rc = ~0ULL } });
}

static void index_op(struct m0_fop_type *ft, struct m0_fid *index,
		     uint64_t key, uint64_t val)
{
	index_op_rc(ft, index, key, val, 0);
}

/**
 * Test insertion (in a non-meta index).
 */
static void insert(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	fini();
}

/**
 * Test insert+lookup.
 */
static void insert_lookup(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
	M0_UT_ASSERT(rep.cgr_rep.cr_rec[0].cr_val.u.ab_buf.b_nob
		     == sizeof (uint64_t));
	M0_UT_ASSERT(2 ==
		     *(uint64_t *)rep.cgr_rep.cr_rec[0].cr_val.u.ab_buf.b_addr);
	fini();
}

/**
 * Test insert+delete.
 */
static void insert_delete(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, 2);
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
	index_op(&cas_del_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	fini();
}

/**
 * Test lookup of a non-existing key
 */
static void lookup_none(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, 2);
	index_op(&cas_get_fopt, &ifid, 3, NOVAL);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	fini();
}

/**
 * Test insert, lookup, delete of the record with NULL value.
 */
static void empty_value(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, EMPTYVAL);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
	M0_UT_ASSERT(rep.cgr_rep.cr_rec[0].cr_val.u.ab_buf.b_nob == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_rec[0].cr_val.u.ab_buf.b_addr == NULL);
	index_op(&cas_del_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	fini();
}

/**
 * Test insert of an existing key
 */
static void insert_2(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep_check(0, -EEXIST, BUNSET, BUNSET));
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
	fini();
}

/**
 * Test delete of a non-existing key
 */
static void delete_2(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_del_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	fini();
}

enum {
	INSERTS = 1500,
	MULTI_INS = 15
};

#define CB(x) m0_byteorder_cpu_to_be64(x)
#define BC(x) m0_byteorder_be64_to_cpu(x)

static void insert_odd(struct m0_fid *index)
{
	int i;

	for (i = 1; i < INSERTS; i += 2) {
		/*
		 * Convert to big-endian to get predictable iteration order.
		 */
		index_op(&cas_put_fopt, index, CB(i), i*i);
		M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	}
}

static void lookup_all(struct m0_fid *index)
{
	int i;

	for (i = 1; i < INSERTS; ++i) {
		index_op(&cas_get_fopt, index, CB(i), NOVAL);
		if (i & 1) {
			M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
			M0_UT_ASSERT(repv[0].cr_val.u.ab_buf.b_nob ==
				     sizeof (uint64_t));
			M0_UT_ASSERT(i * i ==
				   *(uint64_t *)repv[0].cr_val.u.ab_buf.b_addr);
		} else {
			M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
		}
	}
}

/**
 * Test lookup of multiple values.
 */
static void lookup_N(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	insert_odd(&ifid);
	lookup_all(&ifid);
	fini();
}

/**
 * Test lookup after restart.
 */
static void lookup_restart(void)
{
	int           result;

	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	insert_odd(&ifid);
	lookup_all(&ifid);
	service_stop();

	m0_reqh_rpc_mach_tlink_init_at_tail(&rpc_machine,
					    &reqh.rh_rpc_machines);

	result = m0_reqh_service_allocate(&fdmi, &m0_fdmi_service_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(fdmi, &reqh, NULL);
	result = m0_reqh_service_start(fdmi);
	M0_UT_ASSERT(result == 0);

	result = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	m0_reqh_service_start(cas);
	lookup_all(&ifid);
	fini();
}

/**
 * Test iteration over multiple values (with restart).
 */
static void cur_N(void)
{
	int i;
	int result;

	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	insert_odd(&ifid);
	service_stop();

	m0_reqh_rpc_mach_tlink_init_at_tail(&rpc_machine,
					    &reqh.rh_rpc_machines);

	result = m0_reqh_service_allocate(&fdmi, &m0_fdmi_service_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(fdmi, &reqh, NULL);
	result = m0_reqh_service_start(fdmi);
	M0_UT_ASSERT(result == 0);

	result = m0_reqh_service_allocate(&cas, &m0_cas_service_type, NULL);
	M0_UT_ASSERT(result == 0);
	m0_reqh_service_init(cas, &reqh, NULL);
	m0_cas__ut_svc_be_set(cas, &be.but_dom);
	m0_reqh_service_start(cas);

	for (i = 1; i < INSERTS; ++i) {
		int j;
		int k;

		index_op_rc(&cas_cur_fopt, &ifid, CB(i), NOVAL, INSERTS);
		if (!(i & 1)) {
			M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
			continue;
		}
		for (j = i, k = 0; j < INSERTS; j += 2, ++k) {
			struct m0_cas_rec *r = &repv[k];
			struct m0_buf     *buf;

			buf = &r->cr_val.u.ab_buf;
			M0_UT_ASSERT(rep_check(k, k + 1, BSET, BSET));
			M0_UT_ASSERT(buf->b_nob == sizeof (uint64_t));
			M0_UT_ASSERT(*(uint64_t *)buf->b_addr == j * j);
			buf = &r->cr_key.u.ab_buf;
			M0_UT_ASSERT(buf->b_nob == sizeof (uint64_t));
			M0_UT_ASSERT(*(uint64_t *)buf->b_addr == CB(j));
		}
		M0_UT_ASSERT(rep_check(k, -ENOENT, BUNSET, BUNSET));
		M0_UT_ASSERT(rep.cgr_rep.cr_nr == INSERTS);
	}
	fini();
}

static struct m0_thread t[8];

static void meta_mt_thread(int idx)
{
	uint64_t seed = time(NULL) * (idx + 6);
	int      i;

	M0_UT_ASSERT(0 <= idx && idx < ARRAY_SIZE(t));

	for (i = 0; i < 20; ++i) {
		meta_fid_submit(ft[m0_rnd64(&seed) % ARRAY_SIZE(ft)],
			    &IFID(2, m0_rnd64(&seed) % 5));
		/*
		 * Cannot check anything: global rep and repv are corrupted.
		 */
	}
}

/**
 * Test multi-threaded meta-operations.
 */
static void meta_mt(void)
{
	int i;
	int result;

	init();
	mt = true;
	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		result = M0_THREAD_INIT(&t[i], int, NULL, &meta_mt_thread, i,
					"meta-mt-%i", i);
		M0_UT_ASSERT(result == 0);
	}
	for (i = 0; i < ARRAY_SIZE(t); ++i) {
		m0_thread_join(&t[i]);
		m0_thread_fini(&t[i]);
	}
	mt = false;
	fini();
}

static void meta_insert_fail(void)
{
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -ENOMEM, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep.cgr_rc == -ENOENT);
	/* Lookup process should return zero records. */
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	fini();
}

static void meta_lookup_fail(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Lookup process should return ENOMEM code */
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -ENOMEM, BUNSET, BUNSET));
	/* Lookup without ENOMEM returns record. */
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	fini();
}

static void meta_delete_fail(void)
{
	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	/* Delete record with fail. */
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	meta_fid_submit(&cas_del_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -ENOMEM, BUNSET, BUNSET));
	/* Lookup should return record. */
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	fini();
}

static void insert_fail(void)
{
	init();
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert key ENOMEM - fi. */
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep.cgr_rc == -ENOMEM);
	/* Search meta OK. */
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	/* Search key, ENOENT. */
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep.cgr_rep.cr_rec[0].cr_rc == -ENOENT);
	M0_UT_ASSERT(rep.cgr_rep.cr_rec[0].cr_val.u.ab_buf.b_addr == NULL);
	fini();
}

static void lookup_fail(void)
{
	init();
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert key OK. */
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Search meta OK. */
	meta_fid_submit(&cas_get_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	/* Search key, ENOMEM - fi. */
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep.cgr_rc == -ENOMEM);
	/* Secondary search OK. */
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(repv[0].cr_val.u.ab_buf.b_nob == sizeof (uint64_t));
	M0_UT_ASSERT(*(uint64_t *)repv[0].cr_val.u.ab_buf.b_addr == 2);
	fini();
}

static void delete_fail(void)
{
	init();
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert key OK. */
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Search key OK. */
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
	M0_UT_ASSERT(repv[0].cr_val.u.ab_buf.b_nob == sizeof (uint64_t));
	M0_UT_ASSERT(*(uint64_t *)repv[0].cr_val.u.ab_buf.b_addr == 2);
	/* Delete key, ENOMEM - fi. */
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	index_op(&cas_del_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep.cgr_rc == -ENOMEM);
	/* Search key OK. */
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(repv[0].cr_val.u.ab_buf.b_nob == sizeof (uint64_t));
	M0_UT_ASSERT(*(uint64_t *)repv[0].cr_val.u.ab_buf.b_addr == 2);
	/* Delete key OK. */
	index_op(&cas_del_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	/* Search key, ENOENT. */
	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep.cgr_rep.cr_rec[0].cr_rc == -ENOENT);
	M0_UT_ASSERT(rep.cgr_rep.cr_rec[0].cr_val.u.ab_buf.b_addr == NULL);
	fini();
}

struct record
{
	uint64_t key;
	uint64_t value;
};

static void multi_values_insert(struct record *recs, int recs_count)
{
	struct m0_cas_rec cas_recs[MULTI_INS];
	int               i;

	M0_UT_ASSERT(recs_count <= MULTI_INS);
	for (i = 0; i < recs_count - 1; i++) {
		cas_recs[i] = (struct m0_cas_rec) {
			.cr_key = (struct m0_rpc_at_buf) {
				.ab_type  = 1,
				.u.ab_buf = M0_BUF_INIT(sizeof recs[i].key,
							&recs[i].key)
				},
			.cr_val = (struct m0_rpc_at_buf) {
				.ab_type  = 1,
				.u.ab_buf = M0_BUF_INIT(sizeof recs[i].value,
							&recs[i].value)
				},
			.cr_rc = 0 };
	}
	cas_recs[recs_count - 1] = (struct m0_cas_rec) { .cr_rc = ~0ULL };
	fop_submit(&cas_put_fopt, &ifid, cas_recs);
}

static void cur_fail(void)
{
	struct record recs[MULTI_INS];
	int           i;

	init();
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert keys OK. */
	for (i = 0; i < MULTI_INS; i++) {
		recs[i].key = i+1;
		recs[i].value = i * i;
	}
	multi_values_insert(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				rep.cgr_rep.cr_rec[i].cr_rc == 0));
	/* Iterate from beginning, -ENOMEM. */
	m0_fi_enable_once("ctg_buf_get", "cas_alloc_fail");
	index_op_rc(&cas_cur_fopt, &ifid, 1, NOVAL, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == -ENOMEM);
	/*
	 * Iterate from begining, first - OK, second - fail.
	 * Service stops request processing after failure, all other reply
	 * records are empty.
	 */
	m0_fi_enable_off_n_on_m("m0_ctg_op_rc", "be-failure", 3, 2);
	index_op_rc(&cas_cur_fopt, &ifid, 1, NOVAL, MULTI_INS);
	m0_fi_disable("m0_ctg_op_rc", "be-failure");
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS);
	M0_UT_ASSERT(repv[0].cr_rc == 1);
	M0_UT_ASSERT(repv[1].cr_rc == -ENOMEM);
	for (i = 2; i < MULTI_INS - 1; i++)
		M0_UT_ASSERT(repv[i].cr_rc == 0);

	fini();
}

static void multi_values_lookup(struct record *recs, int recs_count)
{
	struct m0_cas_rec cas_recs[MULTI_INS];
	int               i;

	M0_UT_ASSERT(recs_count <= MULTI_INS);
	for (i = 0; i < MULTI_INS - 1; i++) {
		cas_recs[i] = (struct m0_cas_rec){
			.cr_key = (struct m0_rpc_at_buf) {
				.ab_type  = 1,
				.u.ab_buf = M0_BUF_INIT(sizeof recs[i].key,
							&recs[i].key)
				},
			.cr_val = (struct m0_rpc_at_buf) {
				.ab_type  = 0,
				.u.ab_buf = M0_BUF_INIT(0, NULL)
				},
			.cr_rc = 0 };
	}
	cas_recs[MULTI_INS - 1] = (struct m0_cas_rec) { .cr_rc = ~0ULL };
	fop_submit(&cas_get_fopt, &ifid, cas_recs);
}

static void multi_values_delete(struct record *recs, int recs_count)
{
	struct m0_cas_rec cas_recs[MULTI_INS];
	int               i;

	M0_UT_ASSERT(recs_count <= MULTI_INS);
	for (i = 0; i < MULTI_INS - 1; i++) {
		cas_recs[i] = (struct m0_cas_rec){
			.cr_key = (struct m0_rpc_at_buf) {
				.ab_type  = 1,
				.u.ab_buf = M0_BUF_INIT(sizeof recs[i].key,
							&recs[i].key)
				},
			.cr_val = (struct m0_rpc_at_buf) {
				.ab_type  = 0,
				.u.ab_buf = M0_BUF_INIT(0, NULL)
				},
			.cr_rc = 0 };
	}
	cas_recs[MULTI_INS - 1] = (struct m0_cas_rec) { .cr_rc = ~0ULL };
	fop_submit(&cas_del_fopt, &ifid, cas_recs);
}

static void multi_insert(void)
{
	struct record recs[MULTI_INS];

	init();
	/* Fill array with pair: [key, value]. */
	m0_forall(i, MULTI_INS, (recs[i].key = i, recs[i].value = i * i, true));
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert several keys and values OK. */
	multi_values_insert(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				rep.cgr_rep.cr_rec[i].cr_rc == 0));
	fini();
}

static void multi_lookup(void)
{
	struct record recs[MULTI_INS];

	init();
	/* Fill array with pair: [key, value]. */
	m0_forall(i, MULTI_INS, (recs[i].key = i, recs[i].value = i * i, true));
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert several keys and values OK. */
	multi_values_insert(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
			rep.cgr_rep.cr_rec[i].cr_rc == 0));
	/* Lookup values. */
	multi_values_lookup(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
			rep.cgr_rep.cr_rec[i].cr_rc == 0));
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
			*(uint64_t *)repv[i].cr_val.u.ab_buf.b_addr == i * i));
	fini();
}

static void multi_delete(void)
{
	struct record recs[MULTI_INS];

	init();
	/* Fill array with pair: [key, value]. */
	m0_forall(i, MULTI_INS, (recs[i].key = i, recs[i].value = i*i, true));
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert several keys and values OK. */
	multi_values_insert(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				rep.cgr_rep.cr_rec[i].cr_rc == 0));
	/* Delete all values. */
	multi_values_delete(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				rep.cgr_rep.cr_rec[i].cr_rc == 0));
	/* Lookup values: ENOENT. */
	multi_values_lookup(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				rep.cgr_rep.cr_rec[i].cr_rc == -ENOENT));
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				repv[i].cr_val.u.ab_buf.b_addr == NULL));
	fini();
}

static void multi_insert_fail(void)
{
	struct record recs[MULTI_INS];

	init();
	/* Fill array with pair: [key, value]. */
	m0_forall(i, MULTI_INS, (recs[i].key = i, recs[i].value = i * i, true));
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert several keys and values OK. */
	m0_fi_enable_off_n_on_m("ctg_buf_get", "cas_alloc_fail", 1, 1);
	multi_values_insert(recs, MULTI_INS);
	m0_fi_disable("ctg_buf_get", "cas_alloc_fail");
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				i % 2 ?
				rep.cgr_rep.cr_rec[i].cr_rc == 0 :
				rep.cgr_rep.cr_rec[i].cr_rc == -ENOMEM));
	fini();
}

static void multi_lookup_fail(void)
{
	struct record recs[MULTI_INS];

	init();
	/* Fill array with pair: [key, value]. */
	m0_forall(i, MULTI_INS, (recs[i].key = i, recs[i].value = i*i, true));
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert several keys and values OK. */
	multi_values_insert(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				rep.cgr_rep.cr_rec[i].cr_rc == 0));
	/* Lookup values. */
	m0_fi_enable_off_n_on_m("ctg_buf_get", "cas_alloc_fail", 1, 1);
	multi_values_lookup(recs, MULTI_INS);
	m0_fi_disable("ctg_buf_get", "cas_alloc_fail");
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
				i % 2 ?
				rep.cgr_rep.cr_rec[i].cr_rc == 0 :
				rep.cgr_rep.cr_rec[i].cr_rc == -ENOMEM));
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
			i % 2 ?
			*(uint64_t *)repv[i].cr_val.u.ab_buf.b_addr == i*i :
			repv[i].cr_val.u.ab_buf.b_addr == NULL));
	fini();
}

static void multi_delete_fail(void)
{
	struct record recs[MULTI_INS];

	init();
	/* Fill array with pair: [key, value]. */
	m0_forall(i, MULTI_INS, (recs[i].key = i, recs[i].value = i*i, true));
	/* Insert meta OK. */
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	/* Insert several keys and values OK. */
	multi_values_insert(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
			rep.cgr_rep.cr_rec[i].cr_rc == 0));
	/* Delete several recs. */
	m0_fi_enable_off_n_on_m("ctg_buf_get", "cas_alloc_fail", 1, 1);
	multi_values_delete(recs, MULTI_INS);
	m0_fi_disable("ctg_buf_get", "cas_alloc_fail");
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
			i % 2 ?
			rep.cgr_rep.cr_rec[i].cr_rc == 0 :
			rep.cgr_rep.cr_rec[i].cr_rc == -ENOMEM));
	/* Lookup values. */
	multi_values_lookup(recs, MULTI_INS);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == MULTI_INS - 1);
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
			i % 2 ?
			rep.cgr_rep.cr_rec[i].cr_rc == -ENOENT :
			rep.cgr_rep.cr_rec[i].cr_rc == 0));
	M0_UT_ASSERT(m0_forall(i, MULTI_INS - 1,
			i % 2 ?
			repv[i].cr_val.u.ab_buf.b_addr == NULL :
			*(uint64_t *)repv[i].cr_val.u.ab_buf.b_addr == i * i));
	fini();
}

/**
 * Tests different operations after server re-start.
 * Server restart is emulated by re-initialisation of request handler along with
 * BE subsystem. No mkfs is performed for BE, so on-disk data between restarts
 * is not modified.
 */
static void server_restart_nomkfs(void)
{
	struct m0_cas_id cid = { .ci_fid = ifid };

	init();
	meta_fid_submit(&cas_put_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));

	/* Check that index and record are present. */
	reinit_nomkfs();
	meta_fop_submit(&cas_cur_fopt,
			(struct meta_rec[]) {
				{ .cid = cid, .rc = 1 } },
			1);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 1, BSET, BUNSET));
	M0_UT_ASSERT(m0_fid_eq(repv[0].cr_key.u.ab_buf.b_addr, &ifid));

	index_op(&cas_get_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BSET));
	M0_UT_ASSERT(rep.cgr_rep.cr_rec[0].cr_val.u.ab_buf.b_nob
		     == sizeof (uint64_t));
	M0_UT_ASSERT(2 ==
		     *(uint64_t *)rep.cgr_rep.cr_rec[0].cr_val.u.ab_buf.b_addr);

	/* Check that the same record can't be inserted. */
	reinit_nomkfs();
	index_op(&cas_put_fopt, &ifid, 1, 2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
	M0_UT_ASSERT(rep_check(0, -EEXIST, BUNSET, BUNSET));

	/* Check that record can be deleted. */
	reinit_nomkfs();
	index_op(&cas_del_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));

	/* Check that record was really deleted. */
	reinit_nomkfs();
	index_op(&cas_del_fopt, &ifid, 1, NOVAL);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));

	/* Check that index can deleted. */
	reinit_nomkfs();
	meta_fid_submit(&cas_del_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));

	/* Check that index was deleted. */
	reinit_nomkfs();
	meta_fid_submit(&cas_del_fopt, &ifid);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));

	fini();
}

static void multi_create_drop(void)
{
	struct m0_cas_id nonce0 = { .ci_fid = IFID(2, 3) };
	struct m0_cas_id nonce1 = { .ci_fid = IFID(2, 4) };

	init();
	meta_fop_submit(&cas_put_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 2);

	meta_fop_submit(&cas_get_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep_check(1, 0, BUNSET, BUNSET));

	meta_fop_submit(&cas_del_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 2);

	meta_fop_submit(&cas_get_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	M0_UT_ASSERT(rep_check(1, -ENOENT, BUNSET, BUNSET));

	meta_fop_submit(&cas_put_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 2);

	meta_fop_submit(&cas_get_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	M0_UT_ASSERT(rep_check(1, 0, BUNSET, BUNSET));

	fini();
}

enum {
	/*
	 * Better have more rows, for 3-levels b-tree and multiple transactions,
	 * but only 7000 can fit into typical 1Mb file.
	 * 2000 is enough to test multiple transactions if decrease transaction
	 * size limit by using -c switch.
	 */
	BIG_ROWS_NUMBER = 2000,
	/*
	 * Number of rows for 2-level btree.
	 */
	SMALL_ROWS_NUMBER = 257
};

/**
 * Test for index drop GC.
 *
 * To test dididing tree clear by transactions run:
 * sudo ./utils/m0run m0ut -- -t cas-service:create-insert-drop -c
 */
static void create_insert_drop(void)
{
	struct m0_cas_id nonce0 = { .ci_fid = IFID(2, 3) };
	struct m0_cas_id nonce1 = { .ci_fid = IFID(2, 4) };
	int              i;

	/*
	 * Use small credits in order to split big transaction into smaller
	 * ones. BE performs quadratic number of checks inside invariants
	 * comparing to number of capture operations.
	 */
	_init(true, true);
	/*
	 * Create 2 catalogs.
	 */
	meta_fop_submit(&cas_put_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 2);

	for (i = 0 ; i < BIG_ROWS_NUMBER ; ++i) {
		index_op(&cas_put_fopt, &nonce0.ci_fid, i+1, i+2);
		M0_UT_ASSERT(rep.cgr_rc == 0);
		M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
		M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	}
	for (i = 0 ; i < SMALL_ROWS_NUMBER ; ++i) {
		index_op(&cas_put_fopt, &nonce1.ci_fid, i+1, i+2);
		M0_UT_ASSERT(rep.cgr_rc == 0);
		M0_UT_ASSERT(rep.cgr_rep.cr_nr == 1);
		M0_UT_ASSERT(rep_check(0, 0, BUNSET, BUNSET));
	}

	/*
	 * Drop 2 catalogs.
	 */
	meta_fop_submit(&cas_del_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep.cgr_rc == 0);
	M0_UT_ASSERT(rep.cgr_rep.cr_nr == 2);

	/*
	 * Check that both catalogs are dropped.
	 */
	meta_fop_submit(&cas_get_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 },
				{ .cid = nonce1 } },
			2);
	M0_UT_ASSERT(rep_check(0, -ENOENT, BUNSET, BUNSET));
	M0_UT_ASSERT(rep_check(1, -ENOENT, BUNSET, BUNSET));

	/*
	 * Wait for GC complete.
	 */
	meta_fop_submit(&cas_gc_fopt,
			(struct meta_rec[]) {
				{ .cid = nonce0 }},
			1);
	fini();
}

struct m0_ut_suite cas_service_ut = {
	.ts_name   = "cas-service",
	.ts_owners = "Nikita",
	.ts_init   = NULL,
	.ts_fini   = NULL,
	.ts_tests  = {
		{ "init-fini",               &init_fini,             "Nikita" },
		{ "init-fail",               &init_fail,             "Leonid" },
		{ "re-init",                 &reinit,                "Egor"   },
		{ "re-start",                &restart,               "Nikita" },
		{ "meta-lookup-none",        &meta_lookup_none,      "Nikita" },
		{ "meta-lookup-2-none",      &meta_lookup_2none,     "Nikita" },
		{ "meta-lookup-N-none",      &meta_lookup_Nnone,     "Nikita" },
		{ "create",                  &create,                "Nikita" },
		{ "create-lookup",           &create_lookup,         "Nikita" },
		{ "create-create",           &create_create,         "Nikita" },
		{ "create-delete",           &create_delete,         "Nikita" },
		{ "recreate",                &recreate,              "Nikita" },
		{ "meta-cur-1",              &meta_cur_1,            "Nikita" },
		{ "meta-cur-0",              &meta_cur_0,            "Nikita" },
		{ "meta-cur-eot",            &meta_cur_eot,          "Nikita" },
		{ "meta-cur-empty",          &meta_cur_empty,        "Nikita" },
		{ "meta-cur-none",           &meta_cur_none,         "Nikita" },
		{ "meta-cur-all",            &meta_cur_all,          "Leonid" },
		{ "meta-random",             &meta_random,           "Nikita" },
		{ "meta-garbage",            &meta_garbage,          "Nikita" },
		{ "insert",                  &insert,                "Nikita" },
		{ "insert-lookup",           &insert_lookup,         "Nikita" },
		{ "insert-delete",           &insert_delete,         "Nikita" },
		{ "lookup-none",             &lookup_none,           "Nikita" },
		{ "empty-value",             &empty_value,           "Egor"   },
		{ "insert-2",                &insert_2,              "Nikita" },
		{ "delete-2",                &delete_2,              "Nikita" },
		{ "lookup-N",                &lookup_N,              "Nikita" },
		{ "lookup-restart",          &lookup_restart,        "Nikita" },
		{ "cur-N",                   &cur_N,                 "Nikita" },
		{ "meta-mt",                 &meta_mt,               "Nikita" },
		{ "meta-insert-fail",        &meta_insert_fail,      "Leonid" },
		{ "meta-lookup-fail",        &meta_lookup_fail,      "Leonid" },
		{ "meta-delete-fail",        &meta_delete_fail,      "Leonid" },
		{ "insert-fail",             &insert_fail,           "Leonid" },
		{ "lookup-fail",             &lookup_fail,           "Leonid" },
		{ "delete-fail",             &delete_fail,           "Leonid" },
		{ "cur-fail",                &cur_fail,              "Egor"   },
		{ "multi-insert",            &multi_insert,          "Leonid" },
		{ "multi-lookup",            &multi_lookup,          "Leonid" },
		{ "multi-delete",            &multi_delete,          "Leonid" },
		{ "multi-insert-fail",       &multi_insert_fail,     "Leonid" },
		{ "multi-lookup-fail",       &multi_lookup_fail,     "Leonid" },
		{ "multi-delete-fail",       &multi_delete_fail,     "Leonid" },
		{ "multi-create-drop",       &multi_create_drop,     "Eugene" },
		{ "create-insert-drop",      &create_insert_drop,    "Eugene" },
		{ "cctg-create",             &cctg_create,           "Sergey" },
		{ "cctg-create-lookup",      &cctg_create_lookup,    "Sergey" },
		{ "cctg-create-delete",      &cctg_create_delete,    "Sergey" },
		{ "server-restart-nomkfs",   &server_restart_nomkfs, "Egor"   },
		{ NULL, NULL }
	}
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of cas group */

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
