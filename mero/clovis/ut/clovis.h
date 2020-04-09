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
#pragma once

#ifndef __MERO_CLOVIS_UT_CLOVIS_H__
#define __MERO_CLOVIS_UT_CLOVIS_H__

#include "ut/ut.h"              /* M0_UT_ASSERT */
#include "clovis/clovis.h"
#include "layout/layout.h"      /* m0_layout */

extern struct m0_clovis_config clovis_default_config;
#define CLOVIS_DEFAULT_EP          "0@lo:12345:45:101"
#define CLOVIS_DEFAULT_HA_ADDR     "0@lo:12345:66:1"
#define CLOVIS_DEFAULT_PROFILE     "<0x7000000000000001:0>"
#define CLOVIS_DEFAULT_PROC_FID    "<0x7200000000000000:0>"
#define CLOVIS_SET_DEFAULT_CONFIG() \
	do { \
		struct m0_clovis_config *confp = &clovis_default_config; \
									 \
		confp->cc_is_oostore            = false; \
		confp->cc_is_read_verify        = false; \
		confp->cc_layout_id             = 1; \
		confp->cc_local_addr            = CLOVIS_DEFAULT_EP;\
		confp->cc_ha_addr               = CLOVIS_DEFAULT_HA_ADDR;\
		confp->cc_profile               = CLOVIS_DEFAULT_PROFILE; \
		confp->cc_process_fid           = CLOVIS_DEFAULT_PROC_FID; \
		confp->cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;\
		confp->cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE; \
	} while(0);

static inline int do_clovis_init(struct m0_clovis **instance)
{
	CLOVIS_SET_DEFAULT_CONFIG();
	return m0_clovis_init(instance, &clovis_default_config, false);
}

#define CLOVIS_INIT(instance) do_clovis_init(instance)

/* These values were extracted from m0t1fs */
#define CLOVIS_M0T1FS_LAYOUT_P 4
#define CLOVIS_M0T1FS_LAYOUT_N 2
#define CLOVIS_M0T1FS_LAYOUT_K 1

/*Some dummy values to help tests */
#define DUMMY_PTR 0xdeafdead
#define UT_DEFAULT_BLOCK_SIZE (1ULL << CLOVIS_DEFAULT_BUF_SHIFT)

/* for layout and instance*/
extern struct m0_layout_enum ut_clovis_layout_enum;
extern struct m0_layout_instance_ops ut_clovis_layout_instance_ops;

/**
 * Initialises the clovis UT environment.
 */
M0_INTERNAL int ut_clovis_init(void);


/**
 * Finalises the clovis UT environment.
 */
M0_INTERNAL int ut_clovis_fini(void);

/** Fake setup for a realm and entity. */
M0_INTERNAL void ut_clovis_realm_entity_setup(struct m0_clovis_realm *realm,
					      struct m0_clovis_entity *ent,
					      struct m0_clovis *cinst);

/**
 * A version of m0_clovis_init for use in unit tests.
 * This will initialise clovis as far as we can in this environment.
 *
 * @param instance A pointer to where the instance should be stored.
 * @return The value of m0_clovis_init.
 */
M0_INTERNAL int ut_m0_clovis_init(struct m0_clovis **instance);

/**
 * A version of m0_clovis_fini for use in unit tests.
 * This will finalise whatever was done in ut_m0_clovis_init.
 *
 * @param instance A pointer to where the instance should be stored.
 */
M0_INTERNAL void ut_m0_clovis_fini(struct m0_clovis **instance);

/**
 * A trick to force the UTs to run in random order every time. This allows the
 * tester to discover hidden dependencies among tests (bonus score!).
 */
M0_INTERNAL void ut_clovis_shuffle_test_order(struct m0_ut_suite *suite);


/**
 * Fills the layout_domain of a m0_clovis instance, so it contains only
 * a layout's id.
 * This only guarantees m0_layout_find(M0_DEFAULT_LAYOUT_ID) returns something.
 *
 * @param layout layout to be added to the domain.
 * @param cinst clovis instance.
 * @remark This might be seen as a hack to reduce dependencies with other mero
 * components.
 * @remark cinst must have been successfully initialised at least until the
 * CLOVIS_IL_HA_STATE level.
 */
void ut_clovis_layout_domain_fill(struct m0_clovis *cinst);
/**
 * Empties a layout domain that has been filled via
 * ut_clovis_layout_domain_fill().
 *
 * @param cinst clovis instance.
 */
void ut_clovis_layout_domain_empty(struct m0_clovis *cinst);
//XXX doxygen
M0_INTERNAL void
ut_clovis_striped_layout_fini(struct m0_striped_layout *stl,
			      struct m0_layout_domain *dom);

M0_INTERNAL void
ut_clovis_striped_layout_init(struct m0_striped_layout *stl,
				 struct m0_layout_domain *dom);

/* Helper functions for clovis unit tests*/

#include "clovis/pg.h"

extern const struct pargrp_iomap_ops mock_iomap_ops;

/**
 * Creates a UT dummy m0_clovis_obj.
 * This allows passing some invariants() forced by the lower layers of Mero.
 */
M0_INTERNAL struct m0_clovis_obj *ut_clovis_dummy_obj_create(void);

/**
 * Deletes a UT dummy m0_clovis_obj.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_clovis_dummy_obj_delete(struct m0_clovis_obj *obj);

/**
 * Creates a UT dummy m0_pdclust_layout.
 * This allows passing some invariants() forced by the lower layers of Mero.
 */
M0_INTERNAL struct m0_pdclust_layout *
ut_clovis_dummy_pdclust_layout_create(struct m0_clovis *instance);

/**
 * Deletes a UT dummy pdclust_layout.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void
ut_clovis_dummy_pdclust_layout_delete(struct m0_pdclust_layout *pl,
				      struct m0_clovis *instance);

/**
 * Creates a UT dummy m0_pdclust_instance.
 * This allows passing some invariants() forced by the lower layers of Mero.
 */
//M0_INTERNAL struct m0_layout_instance *
M0_INTERNAL struct m0_pdclust_instance *
ut_clovis_dummy_pdclust_instance_create(struct m0_pdclust_layout *pdl);

/**
 * Deletes a UT dummy m0_pdclust_instance.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void
ut_clovis_dummy_pdclust_instance_delete(struct m0_pdclust_instance *pdi);
//ut_clovis_dummy_pdclust_instance_delete(struct m0_layout_instance *layout_inst);

/**
 * Initialises a UT dummy nw_xfer_request.
 */
M0_INTERNAL void ut_clovis_dummy_xfer_req_init(struct nw_xfer_request *xfer);

/**
 * Creates a UT dummy nw_xfer_request.
 * This allows passing some invariants() forced by the lower layers of Mero.
 */
M0_INTERNAL struct nw_xfer_request *ut_clovis_dummy_xfer_req_create(void);

/**
 * Finalises a UT dummy nw_xfer_request.
 */
M0_INTERNAL void ut_clovis_dummy_xfer_req_fini(struct nw_xfer_request *xfer);

/**
 * Deletes a UT dummy nw_xfer_request.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_clovis_dummy_xfer_req_delete(struct nw_xfer_request *xfer);

/**
 * Creates a UT dummy data_buf.
 * This allows passing some invariants() forced by the lower layers of Mero.
 */
M0_INTERNAL struct data_buf *
ut_clovis_dummy_data_buf_create(void);

/**
 * Deletes a UT dummy data_buf.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_clovis_dummy_data_buf_delete(struct data_buf *db);

/**
 * Initialises a UT dummy data_buf.
 */
M0_INTERNAL void ut_clovis_dummy_data_buf_init(struct data_buf *db);

/**
 * Finalises a UT dummy data_buf.
 */
M0_INTERNAL void ut_clovis_dummy_data_buf_fini(struct data_buf *db);

/*
 * Creates dummy buf's for parity units
 *
 * @param map The parity group
 * @param do_alloc A flag to control whether allocate and initialise
 *                 data buf's.
 */
M0_INTERNAL void ut_clovis_dummy_paritybufs_create(struct pargrp_iomap *map,
						   bool do_alloc);
/*
 * Frees dummy buf's for parity units
 *
 * @param map The parity group
 * @param do_free A flag to control whether data buf structures are freed.
 */
M0_INTERNAL void ut_clovis_dummy_paritybufs_delete(struct pargrp_iomap *map,
						   bool do_free);

/**
 * Allocate an iomap structure, to be freed by
 * ut_clovis_dummy_pargrp_iomap_delete.
 *
 * @param instance The clovis instance to use.
 * @param num_blocks The number of 4K blocks you will read/write with this map.
 *                   This value must be <= CLOVIS_M0T1FS_LAYOUT_N;
 * @return the allocated structure
 * @remark Need to set the pargrp_iomap's pi_ioo to point to a real ioo if
 * invariant(pargrp_iomap) has to pass.
 */
M0_INTERNAL struct pargrp_iomap *
ut_clovis_dummy_pargrp_iomap_create(struct m0_clovis *instance, int num_blocks);

/**
 * Deletes a UT dummy pargrp_iomap.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void
ut_clovis_dummy_pargrp_iomap_delete(struct pargrp_iomap *map,
				    struct m0_clovis *instance);

/**
 * Creates a UT dummy m0_clovis_op_io.
 * This allows passing some invariants() forced by the lower layers of Mero.
 */
M0_INTERNAL struct m0_clovis_op_io *
ut_clovis_dummy_ioo_create(struct m0_clovis *instance, int num_io_maps);

/**
 * Deletes a UT dummy clovis_op_io.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_clovis_dummy_ioo_delete(struct m0_clovis_op_io *ioo,
				       struct m0_clovis *instance);

/**
 * Returns the pdclust_layout of an ioo.
 */
M0_INTERNAL struct m0_pdclust_layout*
ut_get_pdclust_layout_from_ioo(struct m0_clovis_op_io *ioo);

/**
 * Callback for a ioreq_fop.
 * Executed when an rpc reply is received.
 */
void dummy_ioreq_fop_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast);

/**
 * Creates a UT dummy ioreq_fop.
 * This allows passing some invariants() forced by the lower layers of Mero.
 */
M0_INTERNAL struct ioreq_fop *ut_clovis_dummy_ioreq_fop_create(void);

/**
 * Deletes a UT dummy ioreq_fop.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_clovis_dummy_ioreq_fop_delete(struct ioreq_fop *fop);

/**
 * Deletes a UT dummy ioreq_fop.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_clovis_dummy_ioreq_fop_delete(struct ioreq_fop *fop);


/**
 * Creates a UT dummy target_ioreq.
 * This allows passing some invariants() forced by the lower layers of Mero.
 */
M0_INTERNAL struct target_ioreq *ut_clovis_dummy_target_ioreq_create(void);

/**
 * Deletes a UT dummy target_ioreq.
 * Call this function once for each dummy created.
 */
M0_INTERNAL void ut_clovis_dummy_target_ioreq_delete(struct target_ioreq *ti);

/**
 * Creates a UT dummy pool machine.
 */
M0_INTERNAL int ut_clovis_dummy_poolmach_create(struct m0_pool_version *pv);

/**
 * Deletes a UT dummy pool machine.
 */
M0_INTERNAL void ut_clovis_dummy_poolmach_delete(struct m0_pool_version *pv);

M0_INTERNAL void ut_clovis_set_device_state(struct m0_poolmach *pm, int dev,
					    enum m0_pool_nd_state state);
#endif /* __MERO_CLOVIS_UT_CLOVIS_H__ */

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
