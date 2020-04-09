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

#include "layout/layout.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"          /* M0_LOG */

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "clovis/ut/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/pg.h"
#include "clovis/io.h"

#define DUMMY_PTR 0xdeafdead

#define UT_DEFAULT_BLOCK_SHIFT CLOVIS_DEFAULT_BUF_SHIFT
#define UT_DEFAULT_BLOCK_SIZE  (1ULL << CLOVIS_DEFAULT_BUF_SHIFT)

static const struct m0_bob_type layout_instance_bob = {
	.bt_name         = "layout_instance",
	.bt_magix_offset = offsetof(struct m0_layout_instance, li_magic),
	.bt_magix        = M0_LAYOUT_INSTANCE_MAGIC,
	.bt_check        = NULL
};
M0_BOB_DEFINE(static, &layout_instance_bob, m0_layout_instance);

static const struct m0_bob_type pdclust_instance_bob = {
	.bt_name         = "pd_instance",
	.bt_magix_offset = offsetof(struct m0_pdclust_instance, pi_magic),
	.bt_magix        = M0_LAYOUT_PDCLUST_INSTANCE_MAGIC,
	.bt_check        = NULL
};

M0_BOB_DEFINE(static, &pdclust_instance_bob, m0_pdclust_instance);

static const struct m0_bob_type pdclust_bob = {
	.bt_name         = "pdclust",
	.bt_magix_offset = offsetof(struct m0_pdclust_layout, pl_magic),
	.bt_magix        = M0_LAYOUT_PDCLUST_MAGIC,
	.bt_check        = NULL
};

M0_BOB_DEFINE(static, &pdclust_bob, m0_pdclust_layout);

M0_INTERNAL struct m0_clovis_obj *ut_clovis_dummy_obj_create(void)
{
	struct m0_clovis_obj *ret;
	M0_ALLOC_PTR(ret);
	ret->ob_attr.oa_bshift = UT_DEFAULT_BLOCK_SHIFT;
	return ret;
}

M0_INTERNAL void ut_clovis_dummy_obj_delete(struct m0_clovis_obj *obj)
{
	m0_free(obj);
}

M0_INTERNAL struct m0_pdclust_layout *
ut_clovis_dummy_pdclust_layout_create(struct m0_clovis *instance)
{
	struct m0_pdclust_layout *pl;

	M0_ALLOC_PTR(pl);
	m0_pdclust_layout_bob_init(pl);
	ut_clovis_striped_layout_init(&pl->pl_base,
				      &instance->m0c_reqh.rh_ldom);

	/* pointless but required by the compiler */
	m0_pdclust_layout_bob_check(pl);

	/*
	 * non-sense values, but they make it work
	 * invariant check is:
	 * pl->pl_C * (attr.pa_N + 2 * attr.pa_K) == pl->pl_L * attr.pa_P
	 */
	/* these values were extracted from m0t1fs */
	pl->pl_C = 1;
	pl->pl_L = 1;
	pl->pl_attr.pa_P = CLOVIS_M0T1FS_LAYOUT_P;
	pl->pl_attr.pa_N = CLOVIS_M0T1FS_LAYOUT_N;
	pl->pl_attr.pa_K = CLOVIS_M0T1FS_LAYOUT_K;
	pl->pl_attr.pa_unit_size = UT_DEFAULT_BLOCK_SIZE;

	return pl;
}

M0_INTERNAL void
ut_clovis_dummy_pdclust_layout_delete(struct m0_pdclust_layout *pl,
				      struct m0_clovis *instance)
{
	m0_pdclust_layout_bob_fini(pl);
	ut_clovis_striped_layout_fini(&pl->pl_base,
				      &instance->m0c_reqh.rh_ldom);
	m0_free(pl);
}

M0_INTERNAL struct m0_pdclust_instance *
ut_clovis_dummy_pdclust_instance_create(struct m0_pdclust_layout *pdl)
{
	int                         i;
	struct m0_pdclust_instance *pdi;

	M0_ALLOC_PTR(pdi);
	pdi->pi_base.li_l = &pdl->pl_base.sl_base;
	m0_pdclust_instance_bob_init(pdi);

	/* pointless but required by the compiler. */
	m0_pdclust_instance_bob_check(pdi);

	/* Init the layout_instance part. */
	m0_layout_instance_bob_init(&pdi->pi_base);
	/* Make the compiler quiet. */
	m0_layout_instance_bob_check(&pdi->pi_base);
	m0_fid_gob_make(&pdi->pi_base.li_gfid, 0, 1);
	pdi->pi_base.li_ops = (struct m0_layout_instance_ops *)DUMMY_PTR;

	/* tc */
	M0_ALLOC_ARR(pdi->pi_tile_cache.tc_lcode, pdl->pl_attr.pa_P);
	M0_ALLOC_ARR(pdi->pi_tile_cache.tc_permute, pdl->pl_attr.pa_P);
	M0_ALLOC_ARR(pdi->pi_tile_cache.tc_inverse, pdl->pl_attr.pa_P);

	for (i = 0; i < pdl->pl_attr.pa_P; i++) {
		/*
		 * These aren't valid values - but they keep the invariant
		 * check happy
		 */
		pdi->pi_tile_cache.tc_lcode[i] = 0;

 		/* tc->tc_permute[tc->tc_inverse[N]] = N */
		pdi->pi_tile_cache.tc_permute[i] = i;
		pdi->pi_tile_cache.tc_inverse[i] = i;
	}

	return pdi;
}

M0_INTERNAL void
ut_clovis_dummy_pdclust_instance_delete(struct m0_pdclust_instance *pdi)
{
	m0_free(pdi->pi_tile_cache.tc_lcode);
	m0_free(pdi->pi_tile_cache.tc_permute);
	m0_free(pdi->pi_tile_cache.tc_inverse);

	/* Fini the layout_instance part */
	m0_layout_instance_bob_fini(&pdi->pi_base);

	m0_pdclust_instance_bob_fini(pdi);
	m0_free(pdi);
}

M0_INTERNAL void ut_clovis_dummy_xfer_req_init(struct nw_xfer_request *xfer)
{
	nw_xfer_request_bob_init(xfer);
	xfer->nxr_state = NXS_UNINITIALIZED;
	/*M0_ALLOC_PTR(xfer->nxr_ops);*/
}

M0_INTERNAL struct nw_xfer_request *ut_clovis_dummy_xfer_req_create(void)
{
	struct nw_xfer_request *xfer;
	M0_ALLOC_PTR(xfer);
	M0_UT_ASSERT(xfer != NULL);
	ut_clovis_dummy_xfer_req_init(xfer);
	return xfer;
}

M0_INTERNAL void ut_clovis_dummy_xfer_req_fini(struct nw_xfer_request *xfer)
{
	/*m0_free((void *)xfer->nxr_ops);*/
	nw_xfer_request_bob_fini(xfer);
}

M0_INTERNAL void ut_clovis_dummy_xfer_req_delete(struct nw_xfer_request *xfer)
{
	ut_clovis_dummy_xfer_req_fini(xfer);
	m0_free(xfer);
}

M0_INTERNAL struct data_buf *
ut_clovis_dummy_data_buf_create(void)
{
	struct data_buf *ret;
	M0_ALLOC_PTR(ret);
	data_buf_bob_init(ret);
	ret->db_buf.b_addr = NULL;
	return ret;
}

M0_INTERNAL void ut_clovis_dummy_data_buf_delete(struct data_buf *db)
{
	data_buf_bob_fini(db);
	m0_free(db);
}

M0_INTERNAL void ut_clovis_dummy_data_buf_init(struct data_buf *db)
{
	void  *data;
	size_t data_len;

	M0_UT_ASSERT(db != NULL);

	data_len = UT_DEFAULT_BLOCK_SIZE;
	data = m0_alloc_aligned(data_len,
				UT_DEFAULT_BLOCK_SHIFT);
	M0_UT_ASSERT(data != NULL);

	data_buf_bob_init(db);
	m0_buf_init(&db->db_buf, data, data_len);
	db->db_flags = PA_NONE;
}

M0_INTERNAL void ut_clovis_dummy_data_buf_fini(struct data_buf *db)
{
	M0_UT_ASSERT(db != NULL);

	data_buf_bob_fini(db);

	/*
	 * We can't use m0_buf_free, because we allocated
	 * an aligned buffer...
	 */
	m0_free_aligned(db->db_buf.b_addr,
			UT_DEFAULT_BLOCK_SIZE,
			UT_DEFAULT_BLOCK_SHIFT);
}

/*
 * @param do_alloc: a flag to control whether we allocate and initialise
 *                  for data buf structure.
 */
M0_INTERNAL void ut_clovis_dummy_paritybufs_create(struct pargrp_iomap *map,
						   bool do_alloc)
{
	int i;
	int j;

	/*
	 * Why put the following code in dummy_ioo_xxx? Not all unit tests
	 * need pre-allocated parity buffers as paritybufs_alloc
	 */
	M0_ALLOC_ARR(map->pi_paritybufs, map->pi_max_row);
	M0_UT_ASSERT(map->pi_paritybufs != NULL);
	for (i = 0; i < map->pi_max_row; i++) {
		M0_ALLOC_ARR(map->pi_paritybufs[i], CLOVIS_M0T1FS_LAYOUT_K);
		M0_UT_ASSERT(map->pi_paritybufs[i] != NULL);
	}

	if (do_alloc == false)
		return;

	for (i = 0; i < map->pi_max_row; i++) {
		for (j = 0; j < CLOVIS_M0T1FS_LAYOUT_K; j++) {
			M0_ALLOC_PTR(map->pi_paritybufs[i][j]);
			M0_UT_ASSERT(map->pi_paritybufs[i][j] != NULL);

			ut_clovis_dummy_data_buf_init(map->pi_paritybufs[i][j]);
		}
	}
}

M0_INTERNAL void ut_clovis_dummy_paritybufs_delete(struct pargrp_iomap *map,
						   bool do_free)
{
	int i;
	int j;

	for (i = 0; i < map->pi_max_row; i++) {
		for (j = 0; j < CLOVIS_M0T1FS_LAYOUT_K && do_free == true; j++) {
			ut_clovis_dummy_data_buf_fini(map->pi_paritybufs[i][j]);

			m0_free(map->pi_paritybufs[i][j]);

		}
		m0_free(map->pi_paritybufs[i]);
	}
	m0_free(map->pi_paritybufs);

}

M0_INTERNAL struct pargrp_iomap *
ut_clovis_dummy_pargrp_iomap_create(struct m0_clovis *instance, int num_blocks)
{
	int                  r, c;
	int                  rc;
	struct pargrp_iomap *ret;

	M0_UT_ASSERT(num_blocks <= CLOVIS_M0T1FS_LAYOUT_N);

	M0_ALLOC_PTR(ret);
	M0_UT_ASSERT(ret != NULL);
	pargrp_iomap_bob_init(ret);
	ret->pi_ops = (struct pargrp_iomap_ops *)DUMMY_PTR;

	M0_ALLOC_ARR(ret->pi_databufs, CLOVIS_M0T1FS_LAYOUT_N);
	ret->pi_max_row = 1;
	ret->pi_max_col = CLOVIS_M0T1FS_LAYOUT_N;

	for (r = 0; r < ret->pi_max_row; r++) {
		M0_ALLOC_ARR(ret->pi_databufs[r], CLOVIS_M0T1FS_LAYOUT_N);
		M0_UT_ASSERT(ret->pi_databufs[r] != NULL);

		for (c = 0; c < ret->pi_max_col; c++) {
			M0_ALLOC_PTR(ret->pi_databufs[r][c]);
			M0_UT_ASSERT(ret->pi_databufs[r][c] != NULL);

			ut_clovis_dummy_data_buf_init(ret->pi_databufs[r][c]);
		}
	}

	ret->pi_ioo = (struct m0_clovis_op_io *)DUMMY_PTR;

	rc = m0_indexvec_alloc(&ret->pi_ivec, 1);
	M0_UT_ASSERT(rc == 0);
	ret->pi_ivec.iv_vec.v_count[0] = num_blocks * UT_DEFAULT_BLOCK_SIZE;

	return ret;
}

M0_INTERNAL void
ut_clovis_dummy_pargrp_iomap_delete(struct pargrp_iomap *map,
				    struct m0_clovis *instance)
{
	int r, c;

	m0_indexvec_free(&map->pi_ivec);

	for (r = 0; r < map->pi_max_row; r++) {
		for (c = 0; c < map->pi_max_col; c++) {
			ut_clovis_dummy_data_buf_fini(map->pi_databufs[r][c]);

			m0_free(map->pi_databufs[r][c]);

		}
		m0_free(map->pi_databufs[r]);
	}
	m0_free(map->pi_databufs);

	pargrp_iomap_bob_fini(map);
	m0_free(map);
}

#ifndef __KERNEL__
#include <stdlib.h>
#endif
M0_INTERNAL struct m0_clovis_op_io *
ut_clovis_dummy_ioo_create(struct m0_clovis *instance, int num_io_maps)
{
	int                         i;
	int                         rc;
	uint64_t                    layout_id;
	struct m0_layout           *layout;
	struct m0_pdclust_layout   *pl;
	struct m0_pdclust_instance *pdi;
	struct m0_clovis_op_io     *ioo;
	struct m0_pool_version     *pv;

	M0_ALLOC_PTR(ioo);
	M0_UT_ASSERT(ioo != NULL);

	m0_clovis_op_io_bob_init(ioo);
	m0_clovis_op_obj_bob_init(&ioo->ioo_oo);
	m0_clovis_op_common_bob_init(&ioo->ioo_oo.oo_oc);
	m0_clovis_op_bob_init(&ioo->ioo_oo.oo_oc.oc_op);

	ioo->ioo_oo.oo_oc.oc_op.op_size = sizeof(*ioo);
	ioo->ioo_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;

	/* Create parity layout and instance */
	pv = instance->m0c_pools_common.pc_cur_pver;
	layout_id = m0_pool_version2layout_id(&pv->pv_id, M0_DEFAULT_LAYOUT_ID);
	layout = m0_layout_find(&instance->m0c_reqh.rh_ldom, layout_id);
	M0_UT_ASSERT(layout != NULL);
	pl = m0_layout_to_pdl(layout);
	pdi = ut_clovis_dummy_pdclust_instance_create(pl);
	pdi->pi_base.li_l = &pl->pl_base.sl_base;
	pdi->pi_base.li_l->l_pver = instance->m0c_pools_common.pc_cur_pver;
	ioo->ioo_oo.oo_layout_instance = &pdi->pi_base;

	/* Set entity */
	ioo->ioo_obj = ut_clovis_dummy_obj_create();
	ioo->ioo_oo.oo_oc.oc_op.op_entity = &ioo->ioo_obj->ob_entity;

	/* IO extends */
	rc = m0_indexvec_alloc(&ioo->ioo_ext, 1);
	M0_UT_ASSERT(rc == 0);
	ioo->ioo_ext.iv_index[0] = 0;
	ioo->ioo_ext.iv_vec.v_count[0] = UT_DEFAULT_BLOCK_SIZE;

	ioo->ioo_data.ov_vec.v_nr = 1;
	M0_ALLOC_ARR(ioo->ioo_data.ov_vec.v_count, 1);
	ioo->ioo_data.ov_vec.v_count[0] = UT_DEFAULT_BLOCK_SIZE;
	M0_ALLOC_ARR(ioo->ioo_data.ov_buf, 1);
	M0_ALLOC_ARR(ioo->ioo_data.ov_buf[0], 1);

	/* failed sessions*/
	M0_ALLOC_ARR(ioo->ioo_failed_session, 1);
	ioo->ioo_failed_session[0] = ~(uint64_t)0;

	/* fid */
	//m0_fid_set(&ioo->ioo_oo.oo_fid, 0, 1);
	m0_fid_gob_make(&ioo->ioo_oo.oo_fid, 0, 1);

	m0_sm_init(&ioo->ioo_sm, &io_sm_conf, IRS_INITIALIZED, &instance->m0c_sm_group);

	ut_clovis_dummy_xfer_req_init(&ioo->ioo_nwxfer);

	/* IO parity group map */
	M0_ALLOC_ARR(ioo->ioo_iomaps, num_io_maps);
	M0_UT_ASSERT(ioo->ioo_iomaps != NULL);
	ioo->ioo_iomap_nr = num_io_maps;
	for (i = 0; i < ioo->ioo_iomap_nr; i++) {
		ioo->ioo_iomaps[i] =
			ut_clovis_dummy_pargrp_iomap_create(instance,
							    CLOVIS_M0T1FS_LAYOUT_N);
		ioo->ioo_iomaps[i]->pi_grpid = i;
		ioo->ioo_iomaps[i]->pi_ioo = ioo;
	}

	/* SM group */
	M0_ALLOC_PTR(ioo->ioo_oo.oo_sm_grp);
	m0_sm_group_init(ioo->ioo_oo.oo_sm_grp);

	return ioo;
}

/**
 * Returns the pdclust_layout of an ioo.
 */
M0_INTERNAL struct m0_pdclust_layout*
ut_get_pdclust_layout_from_ioo(struct m0_clovis_op_io *ioo)
{
	return bob_of(ioo->ioo_oo.oo_layout_instance->li_l,
		      struct m0_pdclust_layout,
		      pl_base.sl_base, &pdclust_bob);
}

M0_INTERNAL void ut_clovis_dummy_ioo_delete(struct m0_clovis_op_io *ioo,
				       struct m0_clovis *instance)
{
	int                           i;
	/*struct m0_pdclust_layout     *pl;*/
	struct m0_layout_instance    *li;
	struct m0_pdclust_instance   *pdi;

	m0_sm_group_fini(ioo->ioo_oo.oo_sm_grp);
	m0_free(ioo->ioo_oo.oo_sm_grp);

	if (ioo->ioo_iomaps != NULL) {
		for (i = 0; i < ioo->ioo_iomap_nr; i++) {
			ut_clovis_dummy_pargrp_iomap_delete(
				ioo->ioo_iomaps[i], instance);
		}
		m0_free0(&ioo->ioo_iomaps);
	}

	ut_clovis_dummy_xfer_req_fini(&ioo->ioo_nwxfer);

	m0_sm_group_lock(&instance->m0c_sm_group);
	m0_sm_move(&ioo->ioo_sm, 0, IRS_REQ_COMPLETE);
	m0_sm_fini(&ioo->ioo_sm);
	m0_sm_group_unlock(&instance->m0c_sm_group);

	m0_free(ioo->ioo_failed_session);
	m0_free(ioo->ioo_data.ov_buf[0]);
	m0_free(ioo->ioo_data.ov_buf);
	m0_free(ioo->ioo_data.ov_vec.v_count);
	m0_indexvec_free(&ioo->ioo_ext);
	ut_clovis_dummy_obj_delete(ioo->ioo_obj);

	li = ioo->ioo_oo.oo_layout_instance;
	pdi = bob_of(li, struct m0_pdclust_instance, pi_base,
		     &pdclust_instance_bob);
	ut_clovis_dummy_pdclust_instance_delete(pdi);
#if 0
	pl = ut_get_pdclust_layout_from_ioo(ioo);
	ut_clovis_dummy_pdclust_layout_delete(pl, instance);
#endif
	m0_free(ioo);
}

void dummy_ioreq_fop_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
}

M0_INTERNAL struct ioreq_fop *ut_clovis_dummy_ioreq_fop_create(void)
{
	struct ioreq_fop *ret;

	M0_ALLOC_PTR(ret);
	m0_bob_type_tlist_init(&iofop_bobtype, &iofops_tl);
	ioreq_fop_bob_init(ret);
	ret->irf_tioreq = (struct target_ioreq *)DUMMY_PTR;
	ret->irf_ast.sa_cb = dummy_ioreq_fop_cb;
	ret->irf_ast.sa_mach = (struct m0_sm *)DUMMY_PTR;

	return ret;
}

M0_INTERNAL void ut_clovis_dummy_ioreq_fop_delete(struct ioreq_fop *fop)
{
	ioreq_fop_bob_fini(fop);
	m0_free(fop);
}

M0_INTERNAL struct target_ioreq *ut_clovis_dummy_target_ioreq_create(void)
{
	struct target_ioreq *ti;

	M0_ALLOC_PTR(ti);
	target_ioreq_bob_init(ti);
	M0_ALLOC_PTR(ti->ti_session);
	ti->ti_session->s_session_id = ~(uint64_t)0;
	ti->ti_nwxfer = (struct nw_xfer_request *)DUMMY_PTR;
	ti->ti_bufvec.ov_buf = (void **)DUMMY_PTR;
	ti->ti_auxbufvec.ov_buf = (void **)DUMMY_PTR;
	m0_fid_set(&ti->ti_fid, 0, 1);
	iofops_tlist_init(&ti->ti_iofops);
	tioreqht_tlink_init(ti);

	return ti;
}

M0_INTERNAL void ut_clovis_dummy_target_ioreq_delete(struct target_ioreq *ti)
{
	M0_UT_ASSERT(iofops_tlist_is_empty(&ti->ti_iofops));

	m0_free(ti->ti_session);
	tioreqht_tlink_fini(ti);
	iofops_tlist_fini(&ti->ti_iofops);
	target_ioreq_bob_fini(ti);
	m0_free(ti);
}

M0_INTERNAL int ut_clovis_dummy_poolmach_create(struct m0_pool_version *pv)
{
	uint32_t                  i;
	int                       rc = 0;
	struct m0_poolmach       *pm;
	struct m0_poolmach_state *state;

	M0_UT_ASSERT(pv != NULL);

	pm = &pv->pv_mach;
	M0_SET0(pm);
	m0_rwlock_init(&pm->pm_lock);

	/* This is On client. */
	M0_ALLOC_PTR(state);
	if (state == NULL)
		return -ENOMEM;

	state->pst_nr_nodes            = 5; //nr_nodes;
	/* nr_devices io devices and 1 md device. md uses container 0 */
	state->pst_nr_devices          = 5; //nr_devices + 1;
	state->pst_max_node_failures   = 1;
	state->pst_max_device_failures = 1;

	M0_ALLOC_ARR(state->pst_nodes_array, state->pst_nr_nodes);
	M0_ALLOC_ARR(state->pst_devices_array,
		     state->pst_nr_devices);
	M0_ALLOC_ARR(state->pst_spare_usage_array,
		     state->pst_max_device_failures);
	if (state->pst_nodes_array == NULL ||
	    state->pst_devices_array == NULL ||
	    state->pst_spare_usage_array == NULL) {
		/* m0_free(NULL) is valid */
		m0_free(state->pst_nodes_array);
		m0_free(state->pst_devices_array);
		m0_free(state->pst_spare_usage_array);
		m0_free(state);
		return -ENOMEM;
	}

	for (i = 0; i < state->pst_nr_nodes; i++) {
		state->pst_nodes_array[i].pn_state = M0_PNDS_ONLINE;
		M0_SET0(&state->pst_nodes_array[i].pn_id);
	}

	for (i = 0; i < state->pst_nr_devices; i++) {
		state->pst_devices_array[i].pd_state = M0_PNDS_ONLINE;
		M0_SET0(&state->pst_nodes_array[i].pn_id);
		state->pst_devices_array[i].pd_node  = NULL;
	}

	for (i = 0; i < state->pst_max_device_failures; i++) {
		/* -1 means that this spare slot is not used */
		state->pst_spare_usage_array[i].psu_device_index =
					POOL_PM_SPARE_SLOT_UNUSED;
	}
	//poolmach_events_tlist_init(&state->pst_events_list);
	pm->pm_state = state;

	pm->pm_is_initialised = true;

	return rc;
}

M0_INTERNAL void ut_clovis_dummy_poolmach_delete(struct m0_pool_version *pv)
{
	struct m0_poolmach       *pm;
	struct m0_poolmach_state *state;

	if (pv == NULL || pv->pv_mach.pm_state == NULL)
		return;

	pm = &pv->pv_mach;
	state = pm->pm_state;

	m0_free(state->pst_spare_usage_array);
	m0_free(state->pst_devices_array);
	m0_free(state->pst_nodes_array);
	m0_free0(&pm->pm_state);

	pm->pm_is_initialised = false;
	m0_rwlock_fini(&pm->pm_lock);
}

/**
 * Mock operation for Calculating parity.
 * @param map The parity group to calculate the parity for.
 */
static int mock_pargrp_iomap_parity_recalc(struct pargrp_iomap *map)
{
	return 0;
}

static int mock_pargrp_iomap_dgmode_process(struct pargrp_iomap *map,
					    struct target_ioreq *tio,
					    m0_bindex_t         *index,
					    uint32_t             count)
{
	return 0;
}

static int mock_pargrp_iomap_dgmode_recover(struct pargrp_iomap *map)
{
	return 0;
}

const struct pargrp_iomap_ops mock_iomap_ops = {
	.pi_parity_recalc        = mock_pargrp_iomap_parity_recalc,
	.pi_dgmode_process       = mock_pargrp_iomap_dgmode_process,
	.pi_dgmode_recover       = mock_pargrp_iomap_dgmode_recover,
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
