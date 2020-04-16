/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 04/19/2011
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib/ub.h"
#include "lib/memory.h"
#include "lib/errno.h"             /* ENOENT */
#include "lib/misc.h"              /* M0_SET0 */
#include "lib/bitstring.h"
#include "lib/processor.h"
#include "fop/fop.h"
#include "cob/cob.h"
#include "reqh/reqh.h"
#include "mdstore/mdstore.h"
#include "net/net.h"
#include "ut/ut.h"
#include "ut/be.h"
#include "ut/stob.h"		/* m0_ut_stob_linux_get_by_key */
#include "be/ut/helper.h"
#include "mdservice/fsync_foms.c"

#include "mdservice/ut/mdstore.h"
#include "mdservice/ut/lustre.h"

static struct m0_cob_domain_id id = { 42 };

static struct m0_sm_group	*grp;
static struct m0_be_ut_backend	 ut_be;
static struct m0_be_seg		*be_seg;

static struct m0_mdstore           md;
static struct m0_reqh              reqh;
static struct m0_reqh_service     *mdservice;
extern struct m0_reqh_service_type m0_mds_type;
struct m0_rpc_machine              machine;

int m0_md_lustre_fop_alloc(struct m0_fop **fop, void *data,
			   struct m0_rpc_machine *mach);

static struct m0_semaphore inflight;
static int error = 0;

static void (*orig_fom_fini)(struct m0_fom *fom) = NULL;
static int (*orig_fom_create)(struct m0_fop *fop, struct m0_fom **m,
			      struct m0_reqh *reqh);

static void fom_fini(struct m0_fom *fom)
{
	M0_UT_ASSERT(orig_fom_fini != NULL);

	if (error == 0)
		error = m0_fom_rc(fom);
	orig_fom_fini(fom);
	m0_semaphore_up(&inflight);
}

static int fom_create(struct m0_fop *fop, struct m0_fom **m,
		       struct m0_reqh *reqh)
{
	int rc;

	rc = orig_fom_create(fop, m, reqh);
	if (rc == 0) {
		M0_UT_ASSERT(*m != NULL);
		(*m)->fo_local = true;
	}
	return rc;
}

static void test_mkfs(void)
{
	struct m0_md_lustre_fid	 testroot;
	struct m0_fid		 rootfid;
	struct m0_be_tx		 tx;
	struct m0_be_tx_credit	 cred = {};
	int			 fd;
	int			 rc;

	M0_SET0(&ut_be);
	be_seg = m0_be_domain_seg0_get(&ut_be.but_dom);
	rc = M0_REQH_INIT(&reqh,
			  .rhia_db = be_seg,
			  .rhia_mdstore = &md,
			  .rhia_fid     = &g_process_fid,
		);
	M0_UT_ASSERT(rc == 0);
	ut_be.but_dom_cfg.bc_engine.bec_reqh = &reqh;
	m0_be_ut_backend_init(&ut_be);

	grp = m0_be_ut_backend_sm_group_lookup(&ut_be);

	fd = open(M0_MDSTORE_OPS_DUMP_PATH, O_RDONLY);
	M0_UT_ASSERT(fd > 0);

	rc = read(fd, &testroot, sizeof(testroot));
	M0_UT_ASSERT(rc == sizeof(testroot));
	close(fd);

	md.md_dom = NULL;
	rc = m0_mdstore_init(&md, be_seg, false);
	M0_UT_ASSERT(rc == -ENOENT);
	rc = m0_mdstore_create(&md, grp, &id, &ut_be.but_dom, be_seg);
	M0_UT_ASSERT(rc == 0);

	/* Create root and other structures */
	m0_cob_tx_credit(md.md_dom, M0_COB_OP_DOMAIN_MKFS, &cred);
	m0_ut_be_tx_begin(&tx, &ut_be, &cred);
	m0_fid_set(&rootfid, testroot.f_seq, testroot.f_oid);
	rc = m0_cob_domain_mkfs(md.md_dom, &rootfid, &tx);
	M0_UT_ASSERT(rc == 0);
	m0_ut_be_tx_end(&tx);

	/* Fini old mdstore */
	m0_mdstore_fini(&md);

	/* Init mdstore with root init flag set to 1 */
	rc = m0_mdstore_init(&md, be_seg, true);
	M0_UT_ASSERT(rc == 0);

	/* Fini everything */
	m0_mdstore_fini(&md);
}

extern void (*m0_md_req_fom_fini_func)(struct m0_fom *fom);
extern struct m0_fom_type_ops m0_md_fom_ops;

static void test_init(void)
{
	int rc;

	/* Patch md fom operations vector to overwrite finaliser. */
	orig_fom_fini = m0_md_req_fom_fini_func;
	m0_md_req_fom_fini_func = fom_fini;
	/* Patch fom creation routine. */
	orig_fom_create = m0_md_fom_ops.fto_create;
	m0_md_fom_ops.fto_create = fom_create;
	rc = m0_mdstore_init(&md, be_seg, true);
	M0_UT_ASSERT(rc == 0);

	rc = m0_reqh_service_allocate(&mdservice, &m0_mds_type, NULL);
	M0_UT_ASSERT(rc == 0);
	m0_reqh_service_init(mdservice, &reqh, NULL);
	m0_reqh_service_start(mdservice);

	m0_reqh_start(&reqh);
}

static void test_fini(void)
{
	int rc;

	rc = m0_mdstore_destroy(&md, grp, &ut_be.but_dom);
	M0_UT_ASSERT(rc == 0);

	m0_reqh_service_prepare_to_stop(mdservice);
	m0_reqh_service_stop(mdservice);
	m0_reqh_service_fini(mdservice);

	if (m0_reqh_state_get(&reqh) == M0_REQH_ST_NORMAL)
		m0_reqh_shutdown_wait(&reqh);

	if (M0_IN(m0_reqh_state_get(&reqh), (M0_REQH_ST_DRAIN,
					     M0_REQH_ST_INIT)))
		m0_reqh_pre_storage_fini_svcs_stop(&reqh);

	m0_md_fom_ops.fto_create = orig_fom_create;
	m0_md_req_fom_fini_func = orig_fom_fini;

	m0_be_ut_backend_fini(&ut_be);
	m0_reqh_post_storage_fini_svcs_stop(&reqh);
	m0_reqh_fini(&reqh);
}

enum { REC_NR = 128 };

static void test_mdops(void)
{
	struct m0_md_lustre_logrec *rec;
	struct m0_md_lustre_fid     root;
	int                         fd;
	int                         result;
	int                         size;
	struct m0_fop              *fop;
	int                         i;

	fd = open(M0_MDSTORE_OPS_DUMP_PATH, O_RDONLY);
	M0_UT_ASSERT(fd > 0);

	result = read(fd, &root, sizeof(root));
	M0_UT_ASSERT(result == sizeof(root));
	error = 0;

	m0_sm_group_init(&machine.rm_sm_grp);
	for (i = 0; i < REC_NR; ++i) {
		fop = NULL;
		do {
			result = read(fd, &size, sizeof(size));
			if (result < sizeof(size))
				goto end;
			rec = m0_alloc(size);
			M0_UT_ASSERT(rec != NULL);
			result = read(fd, rec, size);
			M0_UT_ASSERT(result == size);
			result = m0_md_lustre_fop_alloc(&fop, rec, &machine);
			m0_free(rec);
			/* Let's get second part of rename fop. */
		} while (result == -EAGAIN);
		if (result == -EOPNOTSUPP)
			continue;
		m0_reqh_fop_handle(&reqh, fop);
		m0_fop_put_lock(fop);

		/* Process fops one by one sequentially. */
		m0_semaphore_down(&inflight);
	}
 end:
	close(fd);
	/* Make sure that all fops are handled. */
	m0_reqh_idle_wait_for(&reqh, m0_reqh_service_find(&m0_mds_type, &reqh));
	M0_UT_ASSERT(error == 0);
	m0_sm_group_fini(&machine.rm_sm_grp);
}

static void ut_fsync_create_fom(void)
{
	struct m0_fop_fsync    *ffop;
	struct m0_fop          *fop;
	struct m0_fop          *rep;
	struct m0_fom          *fom;
	int                     rc;

	/* make up an rpc machine to use */
	m0_sm_group_init(&machine.rm_sm_grp);

	fop = m0_fop_alloc(&m0_fop_fsync_mds_fopt, NULL, &machine);
	M0_UT_ASSERT(fop != NULL);

	ffop = m0_fop_data(fop);
	M0_UT_ASSERT(ffop != NULL);
	ffop->ff_be_remid.tri_txid = 666;
	ffop->ff_be_remid.tri_locality = 1;

	/* the type for the request is right */
	M0_UT_ASSERT(fop->f_type == &m0_fop_fsync_mds_fopt);

	/* check reqh has is associated with the mds service */
	fop->f_type->ft_fom_type.ft_rstype = &m0_mds_type;
	M0_ASSERT(m0_reqh_service_find(fop->f_type->ft_fom_type.ft_rstype , &reqh) != NULL);

	/* create a fom for the fsync fop request */
	m0_sm_group_lock(&machine.rm_sm_grp);
	rc = m0_fsync_req_fom_create(fop, &fom, &reqh);
	m0_sm_group_unlock(&machine.rm_sm_grp);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(fom != NULL);

	/* check the fom returned */
	rep = fom->fo_rep_fop;
	M0_UT_ASSERT(rep != NULL);
	M0_UT_ASSERT(fom->fo_fop == fop);
	/* check the types of the fop request and reply */
	M0_UT_ASSERT(fom->fo_fop->f_type == &m0_fop_fsync_mds_fopt);
	M0_UT_ASSERT(fom->fo_ops == &fsync_fom_ops);
	M0_UT_ASSERT(rep->f_type == &m0_fop_fsync_rep_fopt);

	/* test fom's ops */
	M0_UT_ASSERT(fsync_fom_locality_get(fom) == 1);

	/* release (the kraken!) */
	m0_free(fop);
}

struct m0_ut_suite mdservice_ut = {
	.ts_name = "mdservice-ut",
	.ts_tests = {
		{ "mdservice-mkfs", test_mkfs },
		{ "mdservice-init", test_init },
		{ "mdservice-ops",  test_mdops },
		{ "mdservice-fsync", ut_fsync_create_fom},
		{ "mdservice-fini", test_fini },
		{ NULL, NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
