/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original author:  Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 07-July-2017
 */

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "clovis/ut/clovis.h"

#include "lib/finject.h"
/*
 * Including the c files so we can replace the M0_CLOVIS_PRE asserts
 * in order to test them.
 */
#include "clovis/clovis_layout.c"
#include "clovis/composite_layout.c"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"          /* M0_LOG */

struct m0_ut_suite ut_suite_clovis_layout;

static struct m0_clovis *dummy_instance;

static struct m0_clovis_op_layout* ut_clovis_op_layout_alloc()
{
	struct m0_clovis_op_layout *ol;

	M0_ALLOC_PTR(ol);
	M0_UT_ASSERT(ol != NULL);
	M0_SET0(ol);

	ol->ol_oc.oc_op.op_size = sizeof *ol;
	m0_clovis_op_layout_bob_init(ol);
	m0_clovis_ast_rc_bob_init(&ol->ol_ar);
	m0_clovis_op_common_bob_init(&ol->ol_oc);
	ol->ol_ops = (struct m0_clovis_op_layout_ops *)0x80000000;

	return ol;
}

static void ut_clovis_op_layout_free(struct m0_clovis_op_layout *ol)
{
	m0_free(ol);
}

static void ut_clovis_layout_op_invariant(void)
{
	bool                        rc;
	struct m0_clovis_op_layout *ol;

	/* Base cases. */
	ol = ut_clovis_op_layout_alloc();
	rc = clovis_layout_op_invariant(ol);
	M0_UT_ASSERT(rc == true);

	rc = clovis_layout_op_invariant(NULL);
	M0_UT_ASSERT(rc == false);

	ol->ol_oc.oc_op.op_size = sizeof *ol - 1;
	rc = clovis_layout_op_invariant(ol);
	M0_UT_ASSERT(rc == false);

	ut_clovis_op_layout_free(ol);
}

static void ut_clovis_layout_op_init(void)
{
	int                      rc = 0; /* required */
	struct m0_clovis_realm   realm;
	struct m0_clovis_obj     obj;
	struct m0_clovis_layout  layout;
	struct m0_clovis_op     *op = NULL;
	struct m0_clovis        *instance = NULL;
	struct m0_uint128        id;

	/* initialise */
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	obj.ob_entity.en_id = id;

	op = m0_alloc(sizeof(struct m0_clovis_op_layout));
	op->op_size = sizeof(struct m0_clovis_op_layout);

	/* ADD FI here */
	m0_fi_enable_once("m0_clovis_op_init", "fail_op_init");
	rc = clovis_layout_op_init(&obj, &layout, M0_CLOVIS_EO_LAYOUT_GET, op);
	M0_UT_ASSERT(rc != 0);

	layout.cl_type = M0_CLOVIS_LT_PDCLUST;
	rc = clovis_layout_op_init(&obj, &layout, M0_CLOVIS_EO_LAYOUT_GET, op);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op->op_code  == M0_CLOVIS_EO_LAYOUT_GET);
	m0_free(op);

	/* finalise */
	m0_clovis_entity_fini(&obj.ob_entity);
}

static int dummy_op_rc = 0;

static int op_layout_dummy_launch(struct m0_clovis_op_layout *ol)
{
	return dummy_op_rc;
}

static struct m0_clovis_op_layout_ops op_layout_dummy_ops = {
	.olo_launch        = op_layout_dummy_launch,
	.olo_copy_to_app   = NULL,
	.olo_copy_from_app = NULL,
};

static void ut_clovis_layout_op_cb_launch(void)
{
	int                         op_code;
	struct m0_clovis           *instance = NULL;
	struct m0_clovis_realm      realm;
	struct m0_sm_group         *op_grp;
	struct m0_sm_group         *en_grp;
	struct m0_sm_group          locality_grp;
	struct m0_clovis_entity     ent;
	struct m0_clovis_op_layout *ol;

	/* Initialise clovis */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	ol = ut_clovis_op_layout_alloc();
	ol->ol_oc.oc_op.op_entity = &ent;
	ol->ol_oc.oc_op.op_code = M0_CLOVIS_EO_LAYOUT_GET;
	m0_clovis_op_bob_init(&ol->ol_oc.oc_op);

	op_grp = &ol->ol_oc.oc_op.op_sm_group;
	en_grp = &ent.en_sm_group;
	m0_sm_group_init(op_grp);
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&locality_grp);
	ol->ol_sm_grp = &locality_grp;
	ol->ol_ops = &op_layout_dummy_ops;

	for (op_code = M0_CLOVIS_EO_LAYOUT_GET;
	     op_code <= M0_CLOVIS_EO_LAYOUT_SET; op_code++) {
		dummy_op_rc = 0;
		ol->ol_oc.oc_op.op_code = op_code;
		m0_sm_init(&ol->ol_oc.oc_op.op_sm, &clovis_op_conf,
			   M0_CLOVIS_OS_INITIALISED, op_grp);

		m0_sm_group_lock(op_grp);
		clovis_layout_op_cb_launch(&ol->ol_oc);
		m0_sm_move(&ol->ol_oc.oc_op.op_sm, 0, M0_CLOVIS_OS_EXECUTED);
		m0_sm_move(&ol->ol_oc.oc_op.op_sm, 0, M0_CLOVIS_OS_STABLE);
		m0_sm_fini(&ol->ol_oc.oc_op.op_sm);
		m0_sm_group_unlock(op_grp);
	}

	/* finalise */
	m0_sm_group_fini(&locality_grp);
	m0_sm_group_fini(op_grp);

	m0_clovis_entity_fini(&ent);
	ut_clovis_op_layout_free(ol);
}

static void ut_clovis_layout_op_cb_free(void)
{
	struct m0_clovis_op_layout *ol;
	struct m0_clovis_realm      realm;
	struct m0_clovis_entity     ent;
	struct m0_clovis           *instance = NULL;

	/* init */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	/* base cases */
	ol = ut_clovis_op_layout_alloc();
	ol->ol_oc.oc_op.op_entity = &ent;
	clovis_layout_op_cb_free(&ol->ol_oc);

	/* fini */
	m0_clovis_entity_fini(&ent);
}

static void ut_clovis_layout_op_cb_fini(void)
{
	struct m0_clovis_op_layout *ol;
	struct m0_clovis_realm      realm;
	struct m0_clovis_entity     ent;
	struct m0_clovis           *instance = NULL;

	/* init */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	/* base cases */
	ol = ut_clovis_op_layout_alloc();
	ol->ol_oc.oc_op.op_entity = &ent;
	clovis_layout_op_cb_fini(&ol->ol_oc);

	/* fini */
	m0_clovis_entity_fini(&ent);
	ut_clovis_op_layout_free(ol);
}

static void ut_m0_clovis_layout_op(void)
{
	int                      rc = 0;
	struct m0_clovis_op     *op = NULL;
	struct m0_clovis_obj     obj;
	struct m0_clovis_layout *layout;
	struct m0_uint128        id;
	struct m0_clovis        *instance = NULL;
	struct m0_clovis_realm   realm;

	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	obj.ob_entity.en_id = id;

	/* Base case: GET op. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_PDCLUST);
	M0_UT_ASSERT(layout != NULL);
	rc = m0_clovis_layout_op(&obj, M0_CLOVIS_EO_LAYOUT_GET, layout, &op);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op != NULL);
	m0_free(op);
	op = NULL;

	/* Base case: SET op. */
	rc = m0_clovis_layout_op(&obj, M0_CLOVIS_EO_LAYOUT_SET, layout, &op);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op != NULL);
	m0_free(op);

	m0_clovis_entity_fini(&obj.ob_entity);
}

static void ut_m0_clovis_layout_alloc(void)
{
	struct m0_clovis_layout *layout;

	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_PDCLUST);
	M0_UT_ASSERT(layout != NULL);
	M0_UT_ASSERT(layout->cl_type == M0_CLOVIS_LT_PDCLUST);
	M0_UT_ASSERT(layout->cl_ops == &clovis_layout_pdclust_ops);
	m0_clovis_layout_free(layout);

	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_CAPTURE);
	M0_UT_ASSERT(layout != NULL);
	M0_UT_ASSERT(layout->cl_type == M0_CLOVIS_LT_CAPTURE);
	M0_UT_ASSERT(layout->cl_ops == &clovis_layout_capture_ops);
	m0_clovis_layout_free(layout);

	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	M0_UT_ASSERT(layout != NULL);
	M0_UT_ASSERT(layout->cl_type == M0_CLOVIS_LT_COMPOSITE);
	M0_UT_ASSERT(layout->cl_ops == &clovis_layout_composite_ops);
	m0_clovis_layout_free(layout);
}

static void ut_m0_clovis_layout_capture(void)
{
	int                              rc = 0;
	struct m0_clovis_obj             obj;
	struct m0_clovis_layout         *layout;
	struct m0_clovis_layout         *orig_layout;
	struct m0_clovis_pdclust_layout *pdlayout;
	struct m0_clovis_capture_layout *cap_layout;
	struct m0_uint128                id;
	struct m0_clovis                *instance = NULL;
	struct m0_clovis_realm           realm;

	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	obj.ob_entity.en_id = id;
	obj.ob_attr.oa_layout_id = 12;
	obj.ob_attr.oa_pver = (struct m0_fid){.f_container = 24, .f_key = 36};

	orig_layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_PDCLUST);
	M0_UT_ASSERT(orig_layout != NULL);
	pdlayout = M0_AMB(pdlayout, orig_layout, pl_layout);
	pdlayout->pl_pver = obj.ob_attr.oa_pver;
	pdlayout->pl_lid = obj.ob_attr.oa_layout_id;

	rc = m0_clovis_layout_capture(orig_layout, &obj, &layout);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(layout != NULL);
	cap_layout = M0_AMB(cap_layout, layout, cl_layout);
	M0_UT_ASSERT(m0_fid_eq(&cap_layout->cl_pver, &pdlayout->pl_pver));
	M0_UT_ASSERT(cap_layout->cl_lid == pdlayout->pl_lid);
	M0_UT_ASSERT(m0_uint128_eq(&cap_layout->cl_orig_id, &obj.ob_entity.en_id));

	m0_clovis_layout_free(orig_layout);
	m0_clovis_layout_free(layout);
	m0_clovis_entity_fini(&obj.ob_entity);
}

/**----------------------------------------------------------------------------*
 *                       Clovis COMPOSITE LAYOUT Tests                         *
 *-----------------------------------------------------------------------------*/
static struct m0_clovis_op_composite_io* ut_composite_io_op_alloc()
{
	int                               i;
	int                               rc;
	int                               nr_subobjs;
	struct m0_clovis_op              *op;
	struct m0_clovis_op_composite_io *oci;
	struct composite_sub_io          *sio_arr;
	struct composite_sub_io_ext      *sio_ext;
	struct m0_clovis_obj              obj;
	struct m0_uint128                 id;
	struct m0_clovis                 *instance = dummy_instance;
	struct m0_clovis_realm            realm;

	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	obj.ob_entity.en_id = id;
	obj.ob_attr.oa_pver = (struct m0_fid){.f_container = 24, .f_key = 36};

	M0_ALLOC_PTR(oci);
	M0_UT_ASSERT(oci != NULL);
	M0_SET0(oci);
	op = &oci->oci_oo.oo_oc.oc_op;

	oci->oci_oo.oo_oc.oc_op.op_size = sizeof *oci;
	m0_clovis_op_common_bob_init(&oci->oci_oo.oo_oc);
	m0_clovis_op_obj_bob_init(&oci->oci_oo);
	m0_clovis_op_composite_io_bob_init(oci);
	oci->oci_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;

	m0_sm_group_init(&op->op_sm_group);
	m0_sm_init(&op->op_sm, &clovis_op_conf,
		   M0_CLOVIS_OS_INITIALISED, &op->op_sm_group);

	nr_subobjs = 3;
	M0_ALLOC_ARR(sio_arr, nr_subobjs);
        for (i = 0; i < nr_subobjs; i++) {
		id.u_lo++;
                sio_arr[i].si_id = id;
                sio_arr[i].si_lid = 1;
		sio_arr[i].si_nr_exts = 1;
		sio_ext_tlist_init(&sio_arr[i].si_exts);

		M0_ALLOC_PTR(sio_ext);
		M0_UT_ASSERT(sio_ext != NULL);
		sio_ext->sie_off = i * 4096;
		sio_ext->sie_len = 4096;
		sio_ext_tlink_init_at(sio_ext, &sio_arr[i].si_exts);
        }

	m0_fi_enable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");
	m0_fi_enable("m0_clovis_obj_op", "fake_op");
	rc = composite_sub_io_ops_build(&obj, op, sio_arr, nr_subobjs);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(oci->oci_sub_ops != NULL);
	M0_UT_ASSERT(oci->oci_nr_sub_ops == nr_subobjs);
	m0_fi_disable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");
	m0_fi_disable("m0_clovis_obj_op", "fake_op");

	/* Free sio array. */
	composite_sub_io_destroy(sio_arr, nr_subobjs);

	return oci;
}

static void ut_composite_io_op_free(struct m0_clovis_op_composite_io *oci)
{
	m0_sm_group_fini(&oci->oci_oo.oo_oc.oc_op.op_sm_group);
	m0_free(oci->oci_sub_ops);
	m0_free(oci);
}

static void ut_composite_io_op_cb_launch(void)
{
	struct m0_clovis_realm            realm;
	struct m0_clovis                 *instance = NULL;
	struct m0_sm_group               *op_grp;
	struct m0_sm_group               *en_grp;
	struct m0_sm_group                locality_grp;
	struct m0_clovis_op_composite_io *oci;
	struct m0_clovis_entity           ent;

	/* Initialise clovis */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	m0_fi_enable("m0_clovis_op_failed", "skip_ongoing_io_ref");
	oci = ut_composite_io_op_alloc();
	m0_fi_disable("m0_clovis_op_failed", "skip_ongoing_io_ref");
	oci->oci_oo.oo_oc.oc_op.op_entity = &ent;
	oci->oci_oo.oo_oc.oc_op.op_code = M0_CLOVIS_OC_READ;
	m0_clovis_op_bob_init(&oci->oci_oo.oo_oc.oc_op);

	en_grp = &ent.en_sm_group;
	m0_sm_group_init(en_grp);
	m0_sm_group_init(&locality_grp);
	oci->oci_oo.oo_sm_grp = &locality_grp;

	m0_fi_enable("composite_io_op_cb_launch", "no_subobj_ops_launched");
	op_grp = &oci->oci_oo.oo_oc.oc_op.op_sm_group;
	m0_fi_enable_once("m0_clovis_op_stable", "skip_ongoing_io_ref");
	m0_sm_group_lock(op_grp);
	composite_io_op_cb_launch(&oci->oci_oo.oo_oc);
	m0_sm_move(&oci->oci_oo.oo_oc.oc_op.op_sm, 0, M0_CLOVIS_OS_EXECUTED);
	m0_sm_move(&oci->oci_oo.oo_oc.oc_op.op_sm, 0, M0_CLOVIS_OS_STABLE);
	m0_sm_fini(&oci->oci_oo.oo_oc.oc_op.op_sm);
	m0_sm_group_unlock(op_grp);
	m0_fi_disable("composite_io_op_cb_launch", "no_subobj_ops_launched");
	/*
	 * XXX This "once" injection is not triggered. It may be redundant
	 * or the test expects call of the function and it doesn't happen.
	 */
	m0_fi_disable("m0_clovis_op_stable", "skip_ongoing_io_ref");

	/* finalise */
	m0_sm_group_fini(&locality_grp);

	m0_clovis_entity_fini(&ent);
	ut_composite_io_op_free(oci);
}

static void ut_composite_io_op_cb_free(void)
{
	struct m0_clovis_op_composite_io *oci;
	struct m0_clovis_realm            realm;
	struct m0_clovis_entity           ent;
	struct m0_clovis                 *instance;

	/* init */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	/* base cases */
	m0_fi_enable("m0_clovis_op_failed", "skip_ongoing_io_ref");
	oci = ut_composite_io_op_alloc();
	m0_fi_disable("m0_clovis_op_failed", "skip_ongoing_io_ref");
	oci->oci_oo.oo_oc.oc_op.op_entity = &ent;
	m0_fi_enable("composite_io_op_cb_free", "skip_free_sub_io_op");
	composite_io_op_cb_free(&oci->oci_oo.oo_oc);
	m0_fi_disable("composite_io_op_cb_free", "skip_free_sub_io_op");

	/* fini */
	m0_clovis_entity_fini(&ent);
}

static void ut_composite_io_op_cb_fini(void)
{
	struct m0_clovis_op_composite_io *oci;
	struct m0_clovis_realm            realm;
	struct m0_clovis_entity           ent;
	struct m0_clovis                 *instance;

	/* init */
	M0_SET0(&ent);
	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &ent, instance);

	/* base cases */
	m0_fi_enable("m0_clovis_op_failed", "skip_ongoing_io_ref");
	oci = ut_composite_io_op_alloc();
	m0_fi_disable("m0_clovis_op_failed", "skip_ongoing_io_ref");
	oci->oci_oo.oo_oc.oc_op.op_entity = &ent;
	m0_fi_enable("composite_io_op_cb_fini", "skip_fini_sub_io_op");
	composite_io_op_cb_fini(&oci->oci_oo.oo_oc);
	m0_fi_disable("composite_io_op_cb_fini", "skip_fini_sub_io_op");

	/* fini */
	m0_clovis_entity_fini(&ent);
	ut_composite_io_op_free(oci);
}

static void ut_m0_clovis_composite_layer_add(void)
{
	int                                rc;
	struct m0_clovis_layout           *layout;
	struct m0_clovis_composite_layout *clayout;
	struct m0_clovis_obj               obj;
	struct m0_clovis_composite_layer  *found;
	struct m0_uint128                  id;
	struct m0_clovis_realm             realm;
	struct m0_clovis                  *instance;

	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	obj.ob_entity.en_id = id;
	obj.ob_attr.oa_layout_id = 12;
	obj.ob_attr.oa_pver = (struct m0_fid){.f_container = 24, .f_key = 36};

	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	M0_UT_ASSERT(layout != NULL);
	clayout = M0_AMB(clayout, layout, ccl_layout);

	/* Add a layer. */
	m0_fi_enable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");
	rc = m0_clovis_composite_layer_add(layout, &obj, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(clayout->ccl_nr_layers == 1);
        found = m0_tl_find(clayer, found, &clayout->ccl_layers,
                           found->ccr_priority == 0);
	M0_UT_ASSERT(found != NULL);	
	m0_fi_disable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");

	/* Add again. */
	rc = m0_clovis_composite_layer_add(layout, &obj, 0);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(clayout->ccl_nr_layers == 1);
}

static void ut_m0_clovis_composite_layer_del(void)
{
	int                                rc;
	struct m0_clovis_layout           *layout;
	struct m0_clovis_composite_layout *clayout;
	struct m0_clovis_obj               obj;
	struct m0_clovis_composite_layer  *found;
	struct m0_uint128                  id;
	struct m0_clovis_realm             realm;
	struct m0_clovis                  *instance;

	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	obj.ob_entity.en_id = id;
	obj.ob_attr.oa_layout_id = 12;
	obj.ob_attr.oa_pver = (struct m0_fid){.f_container = 24, .f_key = 36};

	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	M0_UT_ASSERT(layout != NULL);
	clayout = M0_AMB(clayout, layout, ccl_layout);

	/* Add layers. */
	m0_fi_enable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");
	rc = m0_clovis_composite_layer_add(layout, &obj, 0);
	M0_UT_ASSERT(rc == 0);
	obj.ob_entity.en_id.u_lo++;
	rc = m0_clovis_composite_layer_add(layout, &obj, 1);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(clayout->ccl_nr_layers == 2);
	m0_fi_disable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");

	/* Delete an existing layer. */
	m0_clovis_composite_layer_del(layout, obj.ob_entity.en_id);
	M0_UT_ASSERT(clayout->ccl_nr_layers == 1);
        found = m0_tl_find(clayer, found, &clayout->ccl_layers,
                           m0_uint128_eq(&found->ccr_subobj,
					 &obj.ob_entity.en_id));
	M0_UT_ASSERT(found == NULL);

	m0_clovis_composite_layer_del(layout, obj.ob_entity.en_id);
	M0_UT_ASSERT(clayout->ccl_nr_layers == 1);
}

static void ut_composite_sub_io_ops_build(void)
{
	int                               i;
	int                               rc = 0;
	int                               nr_subobjs;
	struct composite_sub_io          *sio_arr;
	struct composite_sub_io_ext      *sio_ext;
	struct m0_clovis_obj              obj;
	struct m0_uint128                 id;
	struct m0_clovis                 *instance;
	struct m0_clovis_realm            realm;
	struct m0_clovis_op              *op = NULL;
	struct m0_clovis_op_common       *oc;
	struct m0_clovis_op_obj          *oo;
	struct m0_clovis_op_composite_io *oci;

	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	id = M0_CLOVIS_ID_APP;
	id.u_lo++;
	obj.ob_entity.en_id = id;

	rc = m0_clovis_op_get(&op, sizeof(struct m0_clovis_op_composite_io));
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(op != NULL);
	op->op_code = M0_CLOVIS_OC_READ;
        oc = M0_AMB(oc, op, oc_op);
        oo = M0_AMB(oo, oc, oo_oc);
        oci = M0_AMB(oci, oo, oci_oo);

	nr_subobjs = 3;
	M0_ALLOC_ARR(sio_arr, nr_subobjs);
        for (i = 0; i < nr_subobjs; i++) {
		id.u_lo++;
                sio_arr[i].si_id = id;
                sio_arr[i].si_lid = 1;
		sio_arr[i].si_nr_exts = 1;
		sio_ext_tlist_init(&sio_arr[i].si_exts);

		M0_ALLOC_PTR(sio_ext);
		M0_UT_ASSERT(sio_ext != NULL);
		sio_ext->sie_off = i * 4096;
		sio_ext->sie_len = 4096;
		sio_ext_tlink_init_at(sio_ext, &sio_arr[i].si_exts);
        }

	m0_fi_enable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");
	m0_fi_enable("m0_clovis_obj_op", "fake_op");
	m0_fi_enable("m0_clovis_op_failed", "skip_ongoing_io_ref");
	rc = composite_sub_io_ops_build(&obj, op, sio_arr, nr_subobjs);
	m0_fi_disable("m0_clovis_op_failed", "skip_ongoing_io_ref");
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(oci->oci_sub_ops != NULL);
	M0_UT_ASSERT(oci->oci_nr_sub_ops == nr_subobjs);
	/*
	 * if layout is not found in m0_clovis__obj_layout_instance_build(),
	 * then the op is set to NULL
	 */
	for (i = 0; i < oci->oci_nr_sub_ops; i++) {
		M0_UT_ASSERT(oci->oci_sub_ops[i] == NULL);
	}
	m0_fi_disable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");
	m0_fi_disable("m0_clovis_obj_op", "fake_op");

	m0_free(op);
	m0_free(sio_arr);
	m0_clovis_entity_fini(&obj.ob_entity);
}

static int composite_layout_add_layers(struct m0_clovis_layout *layout,
				       int nr_layers,
				       struct m0_uint128 *layer_ids)
{
	int                     i;
	int                     j;
	int                     rc = 0;
	struct m0_clovis_obj    obj;
	struct m0_uint128       id;
	struct m0_clovis_realm  realm;
	struct m0_clovis       *instance;

	instance = dummy_instance;
	ut_clovis_realm_entity_setup(&realm, &obj.ob_entity, instance);
	id = M0_CLOVIS_ID_APP;

	m0_fi_enable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");
	for (i = 0; i < nr_layers; i++) {
		id.u_lo++;
		obj.ob_entity.en_id = id;
		obj.ob_attr.oa_layout_id = 12;
		obj.ob_attr.oa_pver = (struct m0_fid){.f_container = 24, .f_key = 36};
		rc = m0_clovis_composite_layer_add(layout, &obj, i);
		layer_ids[i] = id;
	}
	if (rc != 0) {
		for (j = 0; j < i; j++)
			m0_clovis_composite_layer_del(layout, layer_ids[j]);
	}
	m0_fi_disable("m0_clovis__obj_attr_get_sync", "obj_attr_get_ok");

	return rc;
}

struct io_seg {
	m0_bindex_t is_off;
	m0_bcount_t is_len;
};

static int do_composite_io_divide(struct m0_clovis_composite_layout *clayout,
				  int nr_io_segs, struct io_seg *io_segs,
				  struct composite_sub_io **sio_arr,
				  int *nr_subobjs)
{
	int                i;
	int                rc;
	struct m0_indexvec ext;
	struct m0_bufvec   data;

	/* Prepare bufvec and indexvec. */
	rc = m0_bufvec_empty_alloc(&data, nr_io_segs);
	if (rc != 0)
		return rc;

	for (i = 0; i < nr_io_segs; i++) {
		data.ov_vec.v_count[i] = io_segs[i].is_len;
		data.ov_buf[i] = m0_alloc(io_segs[i].is_len);
		if (data.ov_buf[i] == NULL)
			goto free;
	}

	rc = m0_indexvec_alloc(&ext, nr_io_segs);
	if (rc != 0)
		goto free;
	for (i = 0; i < nr_io_segs; i++) {
		ext.iv_index[i] = io_segs[i].is_off;
		ext.iv_vec.v_count[i] = io_segs[i].is_len;
	}

	rc = composite_io_divide(clayout, M0_CLOVIS_OC_READ,
				 &ext, &data, sio_arr, nr_subobjs);

free:
	m0_bufvec_free(&data);
	m0_indexvec_free(&ext);
	return rc;
}

static void ut_composite_io_divide(void)
{
	int                                i = 0;
	int                                rc = 0;
	int                                unit_size = 4096;
	int                                nr_layers;
	int                                nr_io_segs;
	struct m0_uint128                 *layer_ids;
	m0_bindex_t                        off;
	m0_bcount_t                        len;
	struct m0_clovis_layout           *layout;
	struct m0_clovis_composite_layout *clayout;
	struct m0_clovis_composite_layer  *layer;
	struct m0_clovis_composite_extent *ext;
        struct m0_tl                      *ext_list = NULL;
        struct composite_sub_io           *sio;
        struct composite_sub_io           *sio_arr;
        struct composite_sub_io_ext       *sio_ext;
        int                                nr_sios = 0;
	struct io_seg                     *io_segs;

	/* Create a composite layout with layers. */
	layout = m0_clovis_layout_alloc(M0_CLOVIS_LT_COMPOSITE);
	M0_UT_ASSERT(layout != NULL);
	clayout = M0_AMB(clayout, layout, ccl_layout);
	M0_UT_ASSERT(clayout != NULL);

	nr_layers = 3;
	M0_ALLOC_ARR(layer_ids, nr_layers);
	M0_UT_ASSERT(layer_ids != NULL);
	rc = composite_layout_add_layers(layout, nr_layers, layer_ids);
	M0_UT_ASSERT(rc == 0);

	/* Case 1: multiple layers, non-overlapping extents. */
	i = 0;
	off = 0;
	m0_tl_for(clayer, &clayout->ccl_layers, layer) {
		/* Set key and value. */
		off = i * 16 * unit_size;
		len = 16 * unit_size;
		M0_ALLOC_PTR(ext);
		M0_UT_ASSERT(ext != NULL);
		ext->ce_id = layer_ids[i];
		ext->ce_off = off;
		ext->ce_len = len;
		ext_list = &layer->ccr_rd_exts;
                cext_tlink_init_at_tail(ext, ext_list);
		i++;
	} m0_tl_endfor;

	/* Write to the newly created object above.*/
	nr_io_segs= 3;
	M0_ALLOC_ARR(io_segs, nr_io_segs);
	M0_UT_ASSERT(io_segs != NULL);
	for (i = 0; i < nr_io_segs; i++) {
		io_segs[i].is_off = i * 16 * unit_size;
		io_segs[i].is_len = 16 * unit_size;
	}
	rc = do_composite_io_divide(clayout, nr_io_segs, io_segs,
				      &sio_arr, &nr_sios);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(nr_sios == nr_layers);

	/* Verify. */
	off = 0;
	for (i = 0; i < nr_sios; i++) {
		off = i * 16 * unit_size;
		len = 16 * unit_size;

		sio = &sio_arr[i];
		M0_UT_ASSERT(sio->si_nr_exts == 1);
                sio_ext = sio_ext_tlist_head(&sio->si_exts);
		M0_UT_ASSERT(sio_ext->sie_off == off);
		M0_UT_ASSERT(sio_ext->sie_len == len);
	}

	/* Finalise and free. */
	m0_tl_teardown(clayer, &clayout->ccl_layers, layer) {
                m0_tl_teardown(cext, &layer->ccr_rd_exts, ext)
                        m0_free(ext);
                m0_tl_teardown(cext, &layer->ccr_wr_exts, ext)
                        m0_free(ext);
                m0_free0(&layer);
        }
	m0_clovis_layout_free(layout);
	m0_free(layer_ids);
	m0_free(io_segs);
}

static void ut_composite_layer_idx_extents_extract(void)
{
	int                                      i;
	int                                      rc;
	int                                     *rcs;
	int                                      nr_kvp;
	struct m0_tl                            *ext_list;
	struct m0_bufvec                          keys;
	struct m0_bufvec                          vals;
	struct m0_clovis_composite_layer          layer;
	struct m0_clovis_composite_layer_idx_key  max_key;
	struct m0_clovis_composite_layer_idx_key  key;
	struct m0_clovis_composite_layer_idx_val  val;

	layer.ccr_subobj = M0_CLOVIS_ID_APP;
	layer.ccr_subobj.u_lo++;
	cext_tlist_init(&layer.ccr_rd_exts);
	ext_list = &layer.ccr_rd_exts;

	/* Allocate bufvec's for keys and vals. */
	nr_kvp = M0_CLOVIS_COMPOSITE_EXTENT_SCAN_BATCH;
	rc = m0_bufvec_empty_alloc(&keys, nr_kvp)?:
	     m0_bufvec_empty_alloc(&vals, nr_kvp);
	M0_UT_ASSERT(rc == 0);
	M0_ALLOC_ARR(rcs, nr_kvp);
	M0_UT_ASSERT(rcs != NULL);

	/* Case 1: no key/value pairs returned. */
	rc = composite_layer_idx_extents_extract(&layer, &keys, &vals, rcs,
						 ext_list, &max_key);
	M0_UT_ASSERT(rc == 0);

	/* Case 2: every key/value pair in bufvec. */
	for (i = 0; i < nr_kvp; i++) {
		/* Set key and value. */
		key.cek_layer_id = layer.ccr_subobj;
		key.cek_off = i * 4096;
		val.cev_len = 4096;
		rc = m0_clovis_composite_layer_idx_key_to_buf(
			&key, &keys.ov_buf[i], &keys.ov_vec.v_count[i])?:
		     m0_clovis_composite_layer_idx_val_to_buf(
			&val, &vals.ov_buf[i], &vals.ov_vec.v_count[i]);
		M0_UT_ASSERT(rc == 0);
	}

	rc = composite_layer_idx_extents_extract(&layer, &keys, &vals, rcs,
						 ext_list, &max_key);
	M0_UT_ASSERT(rc == nr_kvp);
	M0_UT_ASSERT(max_key.cek_off == (i - 1) * 4096);

	for (i = 0; i < nr_kvp; i++) {
		m0_free(keys.ov_buf[i]);
		keys.ov_buf[i] = NULL;
		keys.ov_vec.v_count[i] = 0;
		m0_free(vals.ov_buf[i]);
		vals.ov_buf[i] = NULL;
		vals.ov_vec.v_count[i] = 0;
	}

	/* Case 3: less than nr_kvp key/value pairs returned*/
	for (i = 0; i < nr_kvp; i++) {
		/* Set key and value. */
		key.cek_layer_id = layer.ccr_subobj;
		if (i >= 2)
			key.cek_layer_id.u_lo++;
		key.cek_off = i * 4096;
		val.cev_len = 4096;
		rc = m0_clovis_composite_layer_idx_key_to_buf(
			&key, &keys.ov_buf[i], &keys.ov_vec.v_count[i])?:
		     m0_clovis_composite_layer_idx_val_to_buf(
			&val, &vals.ov_buf[i], &vals.ov_vec.v_count[i]);
		M0_UT_ASSERT(rc == 0);
	}

	rc = composite_layer_idx_extents_extract(&layer, &keys, &vals, rcs,
						 ext_list, &max_key);
	M0_UT_ASSERT(rc == 2);
	M0_UT_ASSERT(max_key.cek_off == 4096);
}

static void ut_composite_layer_idx_scan(void)
{

}

M0_INTERNAL int ut_suite_clovis_layout_init(void)
{
	int rc;

	rc = ut_m0_clovis_init(&dummy_instance);
	M0_UT_ASSERT(rc == 0);

	return 0;
}

M0_INTERNAL int ut_suite_clovis_layout_fini(void)
{
	ut_m0_clovis_fini(&dummy_instance);

	return 0;
}

struct m0_ut_suite ut_suite_clovis_layout = {
	.ts_name = "clovis-layout-ut",
	.ts_init = ut_suite_clovis_layout_init,
	.ts_fini = ut_suite_clovis_layout_fini,
	.ts_tests = {
		{ "clovis_layout_op_invariant",
			&ut_clovis_layout_op_invariant},
		{ "clovis_layout_op_init",
			&ut_clovis_layout_op_init},
		{ "clovis_layout_op_cb_launch",
			&ut_clovis_layout_op_cb_launch},
		{ "clovis_layout_op_cb_free",
			&ut_clovis_layout_op_cb_free},
		{ "clovis_layout_op_cb_fini",
			&ut_clovis_layout_op_cb_fini},
		{ "m0_clovis_layout_op",
			&ut_m0_clovis_layout_op},
		{ "m0_clovis_layout_alloc",
			&ut_m0_clovis_layout_alloc},
		{ "m0_clovis_layout_capture",
			&ut_m0_clovis_layout_capture},
		{ "composite_io_op_cb_launch",
			&ut_composite_io_op_cb_launch},
		{ "composite_io_op_cb_free",
			&ut_composite_io_op_cb_free},
		{ "composite_io_op_cb_fini",
			&ut_composite_io_op_cb_fini},
		{ "m0_clovis_composite_layer_add",
			&ut_m0_clovis_composite_layer_add},
		{ "m0_clovis_composite_layer_del",
			&ut_m0_clovis_composite_layer_del},
		{ "composite_sub_io_ops_build",
			&ut_composite_sub_io_ops_build},
		{ "composite_io_divide",
			&ut_composite_io_divide},
		{ "composite_layer_idx_extents_extract",
			&ut_composite_layer_idx_extents_extract},
		{ "composite_layer_idx_scan",
			&ut_composite_layer_idx_scan},
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
