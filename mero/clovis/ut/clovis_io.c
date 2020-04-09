/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 20-Oct-2014
 */
#define CLOVIS_UT

#include "lib/finject.h"
#include "layout/layout.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"        /* M0_LOG */
#include "lib/uuid.h"         /* m0_uuid_generate */

#include "ut/ut.h"            /* M0_UT_ASSERT */
#include "clovis/ut/clovis.h"

/*
 * Including the c files so we can replace the M0_CLOVIS_PRE asserts
 * in order to test them.
 */
#if defined(round_down)
#undef round_down
#endif
#if defined(round_up)
#undef round_up
#endif
#include "clovis/clovis_io.c"
#include "clovis/utils.c"

#include "layout/layout_internal.h" /* REMOVE ME */

static struct m0_clovis         *dummy_instance;
static struct m0_pdclust_layout *dummy_pdclust_layout;
struct m0_ut_suite               ut_suite_clovis_io;

#define DUMMY_PTR 0xdeafdead

#define UT_DEFAULT_BLOCK_SIZE (1ULL << CLOVIS_DEFAULT_BUF_SHIFT)

/**
 * Tests addr_is_network_aligned().
 */
static void ut_clovis_test_addr_is_network_aligned(void)
{
	/* The value of addr is irrelevant */
	uint64_t addr = (uint64_t)DUMMY_PTR;
	bool     aligned;

	/* Base cases. */
	addr |= CLOVIS_NETBUF_MASK;
	aligned = addr_is_network_aligned((void *)addr);
	M0_UT_ASSERT(aligned == false);

	addr &= ~CLOVIS_NETBUF_MASK;
	aligned = addr_is_network_aligned((void *)addr);
	M0_UT_ASSERT(aligned == true);
}

/*
 * Tests m0_clovis__page_size().
 */
static void ut_clovis_test_page_size(void)
{
	struct m0_clovis_op_io *ioo;
	struct m0_clovis_obj    obj;
	uint64_t                bs;

	/* Init. */
	M0_ALLOC_PTR(ioo);
	ioo->ioo_obj = &obj;

	/* Base cases. */
	obj.ob_attr.oa_bshift = 2;
	bs = m0_clovis__page_size(ioo);
	M0_UT_ASSERT(bs == 1ULL<<2);
	obj.ob_attr.oa_bshift = 3;
	bs = m0_clovis__page_size(ioo);
	M0_UT_ASSERT(bs == 1ULL<<3);
	obj.ob_attr.oa_bshift = 13;
	bs = m0_clovis__page_size(ioo);
	M0_UT_ASSERT(bs == 1ULL<<13);
	obj.ob_attr.oa_bshift = 0;
	bs = m0_clovis__page_size(ioo);
	M0_UT_ASSERT(bs == 1ULL);

	m0_free(ioo);
}

/*
 * Helper function to test page_nr(). ut_clovis_test_page_nr() may
 * invoke this function several times with different combinations of params.
 */
static void ut_clovis_helper_page_nr(m0_bcount_t size, m0_bcount_t bshift,
				        uint64_t exp_pg)
{
	struct m0_clovis_obj    obj;
	uint64_t                pg;

	obj.ob_attr.oa_bshift = bshift;
	pg = page_nr(size, &obj);
	M0_UT_ASSERT(pg == exp_pg);
}

/*
 * Tests page_nr().
 */
static void ut_clovis_test_page_nr(void)
{
	/* Base case. Assume CLOVIS_MIN_BUF_SHIFT == 9 */
	ut_clovis_helper_page_nr(511, CLOVIS_MIN_BUF_SHIFT, 0);
	ut_clovis_helper_page_nr(512, CLOVIS_MIN_BUF_SHIFT, 1);
	ut_clovis_helper_page_nr(513, CLOVIS_MIN_BUF_SHIFT, 1);
	ut_clovis_helper_page_nr(1024, CLOVIS_MIN_BUF_SHIFT, 2);
	ut_clovis_helper_page_nr(1536, CLOVIS_MIN_BUF_SHIFT, 3);
	/* CLOVIS_DEFAULT_BUF_SHIFT == 12 */
	ut_clovis_helper_page_nr(4096, CLOVIS_DEFAULT_BUF_SHIFT, 1);
	ut_clovis_helper_page_nr(8192, CLOVIS_DEFAULT_BUF_SHIFT, 2);
}

/*
 * Tests layout_n().
 */
static void ut_clovis_test_layout_n(void)
{
	struct m0_pdclust_layout        play;
	uint32_t                        n;

	/* Base case. */
	play.pl_attr.pa_N = 777;
	n = layout_n(&play);
	M0_UT_ASSERT(n == 777);
	play.pl_attr.pa_N = 0;
	n = layout_n(&play);
	M0_UT_ASSERT(n == 0);
}

/*
 * Tests layout_k().
 */
static void ut_clovis_test_layout_k(void)
{
	struct m0_pdclust_layout        play;
	uint32_t                        k;

	/* Base case. */
	play.pl_attr.pa_K = 777;
	k = layout_k(&play);
	M0_UT_ASSERT(k == 777);
	play.pl_attr.pa_K = 0;
	k = layout_k(&play);
	M0_UT_ASSERT(k == 0);
}

/*
 * Helper function for ut_clovis_test_page_id().
 */
static void ut_clovis_helper_page_id(m0_bindex_t offset, m0_bcount_t bshift,
				        uint64_t exp_pg)
{
	struct m0_clovis_obj    obj;
	uint64_t                pg;

	obj.ob_attr.oa_bshift = bshift;
	pg = page_id(offset, &obj);
	M0_UT_ASSERT(pg == exp_pg);
}

/*
 * Tests page_id().
 */
static void ut_clovis_test_page_id(void)
{
	/* Base case. Assume CLOVIS_MIN_BUF_SHIFT == 9 */
	ut_clovis_helper_page_id(511, CLOVIS_MIN_BUF_SHIFT, 0);
	ut_clovis_helper_page_id(512, CLOVIS_MIN_BUF_SHIFT, 1);
	ut_clovis_helper_page_id(513, CLOVIS_MIN_BUF_SHIFT, 1);
	ut_clovis_helper_page_id(1024, CLOVIS_MIN_BUF_SHIFT, 2);
	ut_clovis_helper_page_id(1536, CLOVIS_MIN_BUF_SHIFT, 3);
	/* CLOVIS_DEFAULT_BUF_SHIFT == 12 */
	ut_clovis_helper_page_id(4096, CLOVIS_DEFAULT_BUF_SHIFT, 1);
	ut_clovis_helper_page_id(8192, CLOVIS_DEFAULT_BUF_SHIFT, 2);
}

/*
 * Tests layout_unit_size().
 */
static void ut_clovis_test_layout_unit_size(void)
{
	struct m0_pdclust_layout        play;
	uint64_t                        size;

	/* Base case. */
	play.pl_attr.pa_unit_size = 777;
	size = layout_unit_size(&play);
	M0_UT_ASSERT(size == 777);
	play.pl_attr.pa_unit_size = 0;
	size = layout_unit_size(&play);
	M0_UT_ASSERT(size == 0);
}

/*
 * Helper function for ut_clovis_test_data_row_nr().
 */
static void ut_clovis_helper_data_row_nr(uint64_t size, m0_bcount_t bshift,
					    uint64_t exp_row_nr)
{
	struct m0_pdclust_layout        play;
	struct m0_clovis_obj            obj;
	uint32_t                        row_nr;

	play.pl_attr.pa_unit_size = size;
	obj.ob_attr.oa_bshift = bshift;
	row_nr = data_row_nr(&play, &obj);
	M0_UT_ASSERT(row_nr == exp_row_nr);
}

/*
 * Tests data_row_nr().
 */
static void ut_clovis_test_data_row_nr(void)
{
	struct m0_pdclust_layout        play;
	struct m0_clovis_obj            obj;

	/* Keep gcc quiet during debug build */
	M0_SET0(&obj);
	M0_SET0(&play);

	/* Base case. */
	ut_clovis_helper_data_row_nr(511, CLOVIS_MIN_BUF_SHIFT, 0);
	ut_clovis_helper_data_row_nr(512, CLOVIS_MIN_BUF_SHIFT, 1);
	ut_clovis_helper_data_row_nr(513, CLOVIS_MIN_BUF_SHIFT, 1);
	ut_clovis_helper_data_row_nr(777, CLOVIS_MIN_BUF_SHIFT, 1);
	ut_clovis_helper_data_row_nr(1023, CLOVIS_MIN_BUF_SHIFT, 1);
	ut_clovis_helper_data_row_nr(1024, CLOVIS_MIN_BUF_SHIFT, 2);
}

/*
 * Tests data_col_nr().
 */
static void ut_clovis_test_data_col_nr(void)
{
	struct m0_pdclust_layout        play;
	uint32_t                        col_nr;

	/* Base case. */
	play.pl_attr.pa_N = 777;
	col_nr = data_col_nr(&play);
	M0_UT_ASSERT(col_nr == 777);
	play.pl_attr.pa_N = 0;
	col_nr = data_col_nr(&play);
	M0_UT_ASSERT(col_nr == 0);
}

/*
 * Tests parity_col_nr().
 */
static void ut_clovis_test_parity_col_nr(void)
{
	struct m0_pdclust_layout        play;
	uint32_t                        col_nr;

	/* Base case. */
	play.pl_attr.pa_K = 777;
	col_nr = parity_col_nr(&play);
	M0_UT_ASSERT(col_nr == 777);
	play.pl_attr.pa_K = 0;
	col_nr = parity_col_nr(&play);
	M0_UT_ASSERT(col_nr == 0);
}

/**
 * Tests parity_row_nr().
 * @see ut_clovis_test_data_row_nr().
 */
static void ut_clovis_test_parity_row_nr(void)
{
	struct m0_pdclust_layout        play;
	struct m0_clovis_obj            obj;
	uint32_t                        row_nr;

	/* Base case. */
	play.pl_attr.pa_unit_size = 511;
	obj.ob_attr.oa_bshift = CLOVIS_MIN_BUF_SHIFT;
	row_nr = parity_row_nr(&play, &obj);
	M0_UT_ASSERT(row_nr == 0);
}

/**
 * Tests data_size().
 */
static void ut_clovis_test_data_size(void)
{
	struct m0_pdclust_layout        play;
	uint64_t                        ds;

	/* Base case */
	play.pl_attr.pa_N = 2;
	play.pl_attr.pa_unit_size = 7;
	ds = data_size(&play);
	M0_UT_ASSERT(ds == 14);
}

/**
 * Tests pdlayout_instance().
 */
static void ut_clovis_test_pdlayout_instance(void)
{
	struct m0_pdclust_layout   *pdl;
	struct m0_pdclust_instance *pdi;

	/* Base case */
	pdl = dummy_pdclust_layout;
	pdi = ut_clovis_dummy_pdclust_instance_create(pdl);
	pdlayout_instance(&pdi->pi_base);
}

/**
 * Tests layout_instance().
 */
static void ut_clovis_test_layout_instance(void)
{
	struct m0_clovis_op_io          ioo;
	struct m0_layout_instance       linstance;
	struct m0_layout_instance      *lins;

	/* Base case. */
	ioo.ioo_oo.oo_layout_instance = &linstance;
	lins = layout_instance(&ioo);
	M0_UT_ASSERT(lins == &linstance);
}

/**
 * Tests the pre-conditions of target_fid().
 */
static void ut_clovis_test_target_fid(void)
{
}

/**
 * Tests target_offset().
 */
static void ut_clovis_test_target_offset(void)
{
	struct m0_pdclust_layout        play;
	uint64_t                        offset;

	/* Base cases. */
	play.pl_attr.pa_unit_size = 10;
	offset = target_offset(2, &play, 0);
	M0_UT_ASSERT(offset == 20);
	offset = target_offset(2, &play, 3);
	M0_UT_ASSERT(offset == 23);
	offset = target_offset(2, &play, 14);
	M0_UT_ASSERT(offset == 24);
}

/**
 * Tests group_id().
 */
static void ut_clovis_test_group_id(void)
{
	uint64_t id;

	/* Base cases. */
	id = group_id(8, 3);
	M0_UT_ASSERT(id == 2);
	id = group_id(9, 3);
	M0_UT_ASSERT(id == 3);
	id = group_id(0, 3);
	M0_UT_ASSERT(id == 0);
}

/**
 * Tests seg_endpos().
 */
static void ut_clovis_test_seg_endpos(void)
{
	struct m0_indexvec ivec;
	m0_bcount_t        pos;

	/* Base case. */
	M0_ALLOC_ARR(ivec.iv_index, 1);
	M0_ALLOC_ARR(ivec.iv_vec.v_count, 1);
	ivec.iv_index[0] = 7;
	ivec.iv_vec.v_count[0] = 13;
	pos = seg_endpos(&ivec, 0);
	M0_UT_ASSERT(pos == 20);

	m0_free(ivec.iv_index);
	m0_free(ivec.iv_vec.v_count);
}

static void ut_clovis_test_indexvec_page_nr(void)
{
}

static void ut_clovis_test_iomap_page_nr(void)
{
}

/**
 * Tests parity_units_page_nr().
 */
static void ut_clovis_test_parity_units_page_nr(void)
{
	struct m0_pdclust_layout        play;
	struct m0_clovis_obj            obj;
	uint64_t                        pn;

	/* Base case. */
	play.pl_attr.pa_unit_size = 2 * 512;
	play.pl_attr.pa_K = 3;
	obj.ob_attr.oa_bshift = CLOVIS_MIN_BUF_SHIFT;
	pn = parity_units_page_nr(&play, &obj);
	M0_UT_ASSERT(pn == 6);
}

/**
 * Tests round_down().
 */
static void ut_clovis_test_round_down(void)
{
	uint64_t ret;

	/* Base case. */
	ret = round_down(777, 64);
	M0_UT_ASSERT(ret == 768);
	ret = round_down(12345, 512);
	M0_UT_ASSERT(ret == 12288);
	ret = round_down(12288, 512);
	M0_UT_ASSERT(ret == 12288);
}

/**
 * Tests round_up().
 */
static void ut_clovis_test_round_up(void)
{
	uint64_t ret;

	/* Base case. */
	ret = round_up(777, 64);
	M0_UT_ASSERT(ret == 832);
	ret = round_up(12345, 512);
	M0_UT_ASSERT(ret == 12800);
	ret = round_up(12800, 512);
	M0_UT_ASSERT(ret == 12800);
}

/**
 * Tests pdlayout_get().
 */
static void ut_clovis_test_pdlayout_get(void)
{
	struct m0_clovis               *instance = NULL;
	struct m0_clovis_op_io         *ioo;
	struct m0_pdclust_layout       *pl;
	struct m0_pdclust_layout       *aux;

	/* init */
	instance = dummy_instance;
	ioo = ut_clovis_dummy_ioo_create(instance, 1);

	/* Base case. */
	pl = ut_get_pdclust_layout_from_ioo(ioo);
	aux = pdlayout_get(ioo);
	M0_UT_ASSERT(aux == pl);

	/* fini */
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

/**
 * Tests page_pos_get().
 */
static void ut_clovis_test_page_pos_get(void)
{
}

/**
 * Tests pre-conditions for io_desc_size().
 */
static void ut_clovis_test_io_desc_size(void)
{
}

/**
 * Tests io_seg_size().
 */
static void ut_clovis_test_io_seg_size(void)
{
	uint32_t size;

	/* Base case. */
	size = io_seg_size();
	M0_UT_ASSERT(size == sizeof(struct m0_ioseg));
}

static struct m0_reqh_service *ut_service; /* discarded */
/**
 * Enables the access to the resource manager service. This can be considered
 * a hack, necessary just because clovis' initialisation lift does not reach
 * levels above the confd setup.
 */
static void ut_clovis_enable_resource_manager(struct m0_clovis *instance)
{
	struct m0_uint128       uuid;
	int                     rc;

	m0_uuid_generate(&uuid);
	/* now force it be a service fid */
	m0_fid_tassume((struct m0_fid *)&uuid, &M0_CONF_SERVICE_TYPE.cot_ftype);
	rc = m0_reqh_service_setup(&ut_service, &m0_rms_type, &instance->m0c_reqh,
				   NULL, (struct m0_fid *) &uuid);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_PTR(instance->m0c_pools_common.pc_rm_ctx);
}

/**
 * Releases memory allocated by enable().
 */
static void ut_clovis_disable_resource_manager(struct m0_clovis *instance)
{
	m0_reqh_service_quit(ut_service);
	m0_free(instance->m0c_pools_common.pc_rm_ctx);
}

/**
 * Tests clovis_rm_domain_get().
 */
static void ut_clovis_test_clovis_rm_domain_get(void)
{
	struct m0_rm_domain *dom;
	struct m0_clovis    *instance = NULL;

	/* Init. */
	instance = dummy_instance;

	/* Base case. */
	dom = clovis_rm_domain_get(instance);
	M0_UT_ASSERT(dom != NULL);
}

/**
 * Helper callback for ut_clovis_test_clovis_obj_io_cb_launch().
 */
static int ut_clovis_mock_io_launch_prepare(struct m0_clovis_op_io *ioo)
{
	return 0;
}

/**
 * Helper callback for ut_clovis_test_clovis_obj_io_cb_launch().
 */
static int
ut_clovis_mock_io_launch_distribute(struct nw_xfer_request  *xfer)
{
	return 0;  // -1; /* if we don't want clovis_obj_io_cb_launch do the ast */
}

static void ut_clovis_mock_iosm_handle_launch(struct m0_sm_group *grp,
					      struct m0_sm_ast *ast)
{
	return;
}

/**
 * Tests clovis_obj_io_cb_launch().
 */
static void ut_clovis_test_clovis_obj_io_cb_launch(void)
{
	struct m0_clovis_op_common     *oc;
	struct m0_clovis_op_io         *ioo;
	struct m0_clovis               *instance = NULL;
	struct m0_clovis_realm          realm;
	struct nw_xfer_ops             *nxr_ops;
	struct m0_clovis_op_io_ops     *op_io_ops;

	/* Initialise. */
	instance = dummy_instance;

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ut_clovis_realm_entity_setup(&realm, ioo->ioo_oo.oo_oc.oc_op.op_entity,
				     instance);
	oc = &ioo->ioo_oo.oo_oc;

	/* Base case. Set different callbacks. */
	M0_ALLOC_PTR(op_io_ops);
	op_io_ops->iro_iomaps_prepare =
		&ut_clovis_mock_io_launch_prepare;
	op_io_ops->iro_iosm_handle_launch =
		ut_clovis_mock_iosm_handle_launch;
	ioo->ioo_ops = op_io_ops;

	M0_ALLOC_PTR(nxr_ops);
	nxr_ops->nxo_distribute = &ut_clovis_mock_io_launch_distribute;
	ioo->ioo_nwxfer.nxr_ops = nxr_ops;

	clovis_obj_io_cb_launch(oc);

	/* The CB should be enqueued by now. Force its execution. */
	m0_sm_group_lock(ioo->ioo_oo.oo_sm_grp);
	m0_sm_group_unlock(ioo->ioo_oo.oo_sm_grp);

	m0_free(nxr_ops);
	m0_free(op_io_ops);
	m0_clovis_entity_fini(ioo->ioo_oo.oo_oc.oc_op.op_entity);
	ut_clovis_dummy_ioo_delete(ioo, instance);
}

static void ut_clovis_test_clovis_obj_io_ast_fini(void)
{
}

static void ut_clovis_test_clovis_obj_io_cb_fini(void)
{
}

/**
 * Tests m0_clovis_op_io_invariant().
 */
static void ut_clovis_test_m0_clovis_op_io_invariant(void)
{
	struct m0_clovis_op_io *ioo;
	struct m0_clovis       *instance = NULL;
	bool                    ret;

	/* initialise clovis */
	instance = dummy_instance;
	//ut_clovis_layout_domain_fill(instance);

	/* Base case. */
	ioo = ut_clovis_dummy_ioo_create(instance, 1);
	ret = m0_clovis_op_io_invariant(ioo);
	M0_UT_ASSERT(ret == true);
	ut_clovis_dummy_ioo_delete(ioo, instance);

	/* Finalise clovis. */
	//ut_clovis_layout_domain_empty(instance);
}

/**
 * Helper function: Initialises an indexvec with a specific sequence of numbers.
 * @param ivec Vector being initialised.
 * @param arr Sequence of numbers to initialise the vector with.
 * @param len Number of numbers in the provided sequence.
 */
static void ut_clovis_helper_sort_init(struct m0_indexvec *ivec, uint32_t *arr,
				       uint32_t len)
{
	uint32_t i;
	for (i = 0; i < len; ++i) {
		INDEX(ivec, i) = arr[i];
		COUNT(ivec, i) = arr[i];
	}
}

/**
 * Helper function: Checks if the indices of an indexvec are sorted from lowest
 * to highest.
 * @param ivec Vector being checked.
 * @return true if the indices are sorted or false otherwise.
 */
static bool ut_clovis_helper_sort_is_sorted(struct m0_indexvec *ivec)
{
	uint32_t        i;
	m0_bindex_t     prev_ind = 0;

	for (i = 0; i < SEG_NR(ivec); ++i) {
		if (prev_ind > INDEX(ivec, i))
			return false;
		prev_ind = INDEX(ivec, i);
	}
	return true;
}

/**
 * Helper function for ut_clovis_test_indexvec_sort().
 * Initialises an indexvec and checks it is correctly sorted by
 * indexvec_sort().
 * @param ivec Index vector that will be sorted.
 * @param arr Sequence of numbers to fill the vector with before sorting.
 * @param len Length of the array of numbers.
 */
static void ut_clovis_helper_sort(struct m0_indexvec *ivec, uint32_t *arr,
				       uint32_t len)
{
	ut_clovis_helper_sort_init(ivec, arr, len);
	indexvec_sort(ivec);
	M0_UT_ASSERT(ut_clovis_helper_sort_is_sorted(ivec));
}

/**
 * Tests indexvec_sort().
 */
static void ut_clovis_test_indexvec_sort(void)
{
	struct m0_indexvec      ext;
	int                     rc;

	uint32_t vecs[3][10] = { {1, 9, 8, 7, 2, 3, 4, 6, 5, 10},
				 {10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
				 {1, 400, 34, 399, 2, 33, 13, 14, 1, 10}};

	/* Base case. */
	rc = m0_indexvec_alloc(&ext, 10);
	M0_UT_ASSERT(rc == 0);
	ut_clovis_helper_sort(&ext, vecs[0], 10);
	ut_clovis_helper_sort(&ext, vecs[1], 10);
	ut_clovis_helper_sort(&ext, vecs[2], 10);
	m0_indexvec_free(&ext);
}

/**
 * Tests clovis_obj_io_cb_free().
 */
static void ut_clovis_test_clovis_obj_io_cb_free(void)
{
	struct m0_clovis_op_io         *ioo;

	/* Base case. */
	M0_ALLOC_PTR(ioo);
	ioo->ioo_oo.oo_oc.oc_op.op_size = sizeof *ioo;
	clovis_obj_io_cb_free(&ioo->ioo_oo.oo_oc);
}

/**
 * Tests m0_clovis_obj_op, covering READ.
 */
static void ut_clovis_test_m0_clovis_obj_op(void)
{
	int                     rc;
	struct m0_clovis_obj    obj;
	struct m0_clovis_realm  realm;
	struct m0_indexvec      ext;
	struct m0_bufvec        data;
	struct m0_bufvec        attr;
	struct m0_clovis_op    *op;
	struct m0_clovis       *instance = NULL;
	struct m0_clovis_op_io *ioop;

	/* Keep gcc quiet during debug build */
	M0_SET0(&op);

	/* auxiliary structs */
	rc = m0_indexvec_alloc(&ext, 1);
	M0_UT_ASSERT(rc == 0);
	ext.iv_vec.v_count[0] = 512;
	rc = m0_bufvec_alloc(&data, 1, 512);
	M0_UT_ASSERT(rc == 0);
	rc = m0_bufvec_alloc(&attr, 1, 1);
	M0_UT_ASSERT(rc == 0);

	/* initialise clovis */
	instance = dummy_instance;
	M0_SET0(&obj);
	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	obj.ob_attr.oa_bshift = CLOVIS_MIN_BUF_SHIFT;
	obj.ob_attr.oa_pver   = instance->m0c_pools_common.pc_cur_pver->pv_id;

	m0_fi_enable_once("m0_clovis__obj_layout_id_get", "fake_obj_layout_id");
	m0_fi_enable_once("tolerance_of_level", "fake_tolerance_of_level");

	/* Base case: no assert for READ */
	op = NULL;
	rc = m0_clovis_obj_op(&obj, M0_CLOVIS_OC_READ, &ext,
			      &data, &attr, 0, &op);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op->op_size >= sizeof *ioop);

	/* XXX container_of || bob_of */
	ioop = (struct m0_clovis_op_io *)op;
	m0_clovis_op_io_bob_init(ioop);
	m0_clovis_op_obj_bob_init(&ioop->ioo_oo);
	m0_clovis_op_common_bob_init(&ioop->ioo_oo.oo_oc);
	m0_clovis_op_bob_init(&ioop->ioo_oo.oo_oc.oc_op);

	M0_UT_ASSERT(op->op_code == M0_CLOVIS_OC_READ);
	M0_UT_ASSERT(ioop->ioo_oo.oo_oc.oc_cb_launch ==
		     clovis_obj_io_cb_launch);
	M0_UT_ASSERT(ioop->ioo_oo.oo_oc.oc_cb_fini == clovis_obj_io_cb_fini);
	M0_UT_ASSERT(ioop->ioo_oo.oo_oc.oc_cb_free == clovis_obj_io_cb_free);
	M0_UT_ASSERT(ioop->ioo_obj == &obj);
	M0_UT_ASSERT(ioop->ioo_attr_mask == 0);

	m0_clovis_op_fini(op);
	m0_clovis_op_free(op);
	op = NULL;

	/* Free the {index,buf}vec that we have used for these tests. */
	m0_bufvec_free(&attr);
	m0_bufvec_free(&data);
	m0_indexvec_free(&ext);

	m0_clovis_entity_fini(&obj.ob_entity);
}

M0_INTERNAL int m0_clovis_io_ut_init(void)
{
	int rc;

#ifndef __KERNEL__
	ut_clovis_shuffle_test_order(&ut_suite_clovis_io);
#endif

	m0_clovis_init_io_op();

	rc = ut_m0_clovis_init(&dummy_instance);
	M0_UT_ASSERT(rc == 0);

	ut_clovis_layout_domain_fill(dummy_instance);
	dummy_pdclust_layout =
		ut_clovis_dummy_pdclust_layout_create(dummy_instance);
	M0_UT_ASSERT(dummy_pdclust_layout != NULL);

	ut_clovis_enable_resource_manager(dummy_instance);

	return 0;
}

M0_INTERNAL int m0_clovis_io_ut_fini(void)
{
	ut_clovis_disable_resource_manager(dummy_instance);
	ut_clovis_layout_domain_empty(dummy_instance);
	ut_m0_clovis_fini(&dummy_instance);

	return 0;
}

struct m0_ut_suite ut_suite_clovis_io = {
	.ts_name = "clovis-io-ut",
	.ts_init = m0_clovis_io_ut_init,
	.ts_fini = m0_clovis_io_ut_fini,
	.ts_tests = {

		{ "addr_is_network_aligned",
				    &ut_clovis_test_addr_is_network_aligned},
		{ "page_size",
				    &ut_clovis_test_page_size},
		{ "test_page_nr",
				    &ut_clovis_test_page_nr},
		{ "layout_n",
				    &ut_clovis_test_layout_n},
		{ "layout_k",
				    &ut_clovis_test_layout_k},
		{ "page_id",
				    &ut_clovis_test_page_id},
		{ "layout_unit_size",
				    &ut_clovis_test_layout_unit_size},
		{ "data_row_nr",
				    &ut_clovis_test_data_row_nr},
		{ "data_col_nr",
				    &ut_clovis_test_data_col_nr},
		{ "parity_col_nr",
				    &ut_clovis_test_parity_col_nr},
		{ "parity_row_nr",
				    &ut_clovis_test_parity_row_nr},
		{ "data_size",
				    &ut_clovis_test_data_size},
		{ "pdlayout_instance",
				    &ut_clovis_test_pdlayout_instance},
		{ "layout_instance",
				    &ut_clovis_test_layout_instance},
		{ "target_fid",
				    &ut_clovis_test_target_fid},
		{ "target_offset",
				    &ut_clovis_test_target_offset},
		{ "group_id",
				    &ut_clovis_test_group_id},
		{ "seg_endpos",
				    &ut_clovis_test_seg_endpos},
		{ "indexvec_page_nr",
				    &ut_clovis_test_indexvec_page_nr},
		{ "iomap_page_nr",
				    &ut_clovis_test_iomap_page_nr},
		{ "parity_units_page_nr",
				    &ut_clovis_test_parity_units_page_nr},
		{ "round_down",
				    &ut_clovis_test_round_down},
		{ "round_up",
				    &ut_clovis_test_round_up},
		{ "pdlayout_get",
				    &ut_clovis_test_pdlayout_get},
		{ "page_pos_get",
				    &ut_clovis_test_page_pos_get},
		{ "io_desc_size",
				    &ut_clovis_test_io_desc_size},
		{ "io_seg_size",
				    &ut_clovis_test_io_seg_size},
		{ "clovis_rm_domain_get",
				    &ut_clovis_test_clovis_rm_domain_get},
		{ "clovis_obj_io_cb_launch",
				    &ut_clovis_test_clovis_obj_io_cb_launch},
		{ "clovis_obj_io_ast_fini",
				    &ut_clovis_test_clovis_obj_io_ast_fini},
		{ "clovis_obj_io_cb_fini",
				    &ut_clovis_test_clovis_obj_io_cb_fini},
		{ "m0_clovis_op_io_invariant",
				    &ut_clovis_test_m0_clovis_op_io_invariant},
		{ "indexvec_sort",
				    &ut_clovis_test_indexvec_sort},
		{ "clovis_obj_io_cb_free",
				    &ut_clovis_test_clovis_obj_io_cb_free},
		{ "m0_clovis_obj_op",
				    &ut_clovis_test_m0_clovis_obj_op},
		{ NULL, NULL },
	}
};

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
