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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#pragma once

#ifndef __MERO_FDMI_FDMI_PLUGIN_DOCK_INTERNAL_H__
#define __MERO_FDMI_FDMI_PLUGIN_DOCK_INTERNAL_H__

#include "fop/fom.h"
#include "fdmi/plugin_dock.h"

/**
   @defgroup fdmi_pd_int FDMI Plugin Dock internals
   @ingroup fdmi_main

   @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
   @{
*/

/**
   Plugin dock initialisation.
   - Should be called once during FDMI service start
 */

M0_INTERNAL int  m0_fdmi__plugin_dock_init(void);

/**
 * Start plugin dock
 */
M0_INTERNAL int m0_fdmi__plugin_dock_start(struct m0_reqh *reqh);

/**
 * Stop plugin dock
 */
M0_INTERNAL void m0_fdmi__plugin_dock_stop(void);

/**
   Plugin dock de-initialisation.
   - Should be called once during FDMI service shutdown
 */

M0_INTERNAL void m0_fdmi__plugin_dock_fini(void);

/**
   Plugin dock FOM registration.
   - Should be called once during FDMI service start
 */

M0_INTERNAL int  m0_fdmi__plugin_dock_fom_init(void);

/**
   Incoming FDMI record registration in plugin dock communication context.
 */

M0_INTERNAL struct
m0_fdmi_record_reg *m0_fdmi__pdock_fdmi_record_register(struct m0_fop *fop);

/**
   Plugin dock FOM context
 */
struct pdock_fom {
	/** FOM based on record notification FOP */
	struct m0_fom              pf_fom;
	/** FDMI record notification body */
	struct m0_fop_fdmi_record *pf_rec;
	/** Current position in filter ids array the FOM iterates on */
	uint32_t                   pf_pos;
	/** custom FOM finalisation routine, currently intended for use in UT */
	void (*pf_custom_fom_fini)(struct m0_fom *fom);
};

/**
   Helper function, lookup for FDMI filter registration by filter id
   - NOTE: used in UT as well
 */
struct m0_fdmi_filter_reg *
m0_fdmi__pdock_filter_reg_find(const struct m0_fid *fid);

/**
   Helper function, lookup for FDMI record registration by record id
   - NOTE: used in UT as well
 */
struct m0_fdmi_record_reg *
m0_fdmi__pdock_record_reg_find(const struct m0_uint128 *rid);

const struct m0_fom_type_ops *m0_fdmi__pdock_fom_type_ops_get(void);

struct m0_rpc_machine *m0_fdmi__pdock_conn_pool_rpc_machine(void);

/** @} end of fdmi_pd_int group */

#endif

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
