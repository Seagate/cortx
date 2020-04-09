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
 * Original author: Yuriy Umanets <yuriy.umanets@seagate.com>
 * Original creation date: 1-Jun-2017
 */

#pragma once
#ifndef __MERO_FDMI_MODULE_H__
#define __MERO_FDMI_MODULE_H__

#include "module/module.h"
#include "rpc/conn_pool.h"
#include "fdmi/fol_fdmi_src.h"

/** Levels of m0_fdmi_module::fdm_module. */
enum {
	M0_LEVEL_FDMI
};

struct m0_fdmi_module_source {
	/** FOL source internal context. */
	struct m0_fol_fdmi_src_ctx   fdms_ffs_ctx;
	/**
	 * List of all locked transactions.
	 * Entries are linked by m0_be_tx::t_fdmi_linkage.
	 */
	struct m0_tl                 fdms_ffs_locked_tx_list;
	struct m0_mutex              fdms_ffs_locked_tx_lock;
};

struct m0_fdmi_module_plugin {
	struct m0_tl                 fdmp_fdmi_filters;
	struct m0_mutex              fdmp_fdmi_filters_lock;

	struct m0_tl                 fdmp_fdmi_recs;
	struct m0_mutex              fdmp_fdmi_recs_lock;

	bool                         fdmp_dock_inited;
	/**
	 * Connection pool stores connects for
	 * posting release request to the source.
	 */
	struct m0_rpc_conn_pool      fdmp_conn_pool;
};

struct m0_fdmi_module {
	struct m0_module             fdm_module;
	struct m0_fdmi_module_source fdm_s;
	struct m0_fdmi_module_plugin fdm_p;
};

M0_INTERNAL struct m0_fdmi_module *m0_fdmi_module__get(void);

#endif /* __MERO_FDMI_MODULE_H__ */

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
