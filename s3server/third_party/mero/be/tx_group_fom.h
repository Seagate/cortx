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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 *                  Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 17-Jun-2013
 */

#pragma once
#ifndef __MERO_BE_TX_GROUP_FOM_H__
#define __MERO_BE_TX_GROUP_FOM_H__

#include "lib/types.h"          /* bool */
#include "lib/semaphore.h"      /* m0_semaphore */

#include "fop/fom.h"            /* m0_fom */
#include "sm/sm.h"              /* m0_sm_ast */

#include "be/op.h"              /* m0_be_op */

struct m0_be_tx_group;
struct m0_reqh;

/**
 * @defgroup be Meta-data back-end
 * @{
 */

struct m0_be_tx_group_fom {
	/** generic fom */
	struct m0_fom          tgf_gen;
	struct m0_reqh        *tgf_reqh;
	/** group to handle */
	struct m0_be_tx_group *tgf_group;
	/** m0_be_op for I/O operations */
	struct m0_be_op        tgf_op;
	/** m0_be_op for tx GC after recovery */
	struct m0_be_op        tgf_op_gc;
	/**
	 * True iff all transactions of the group have reached M0_BTS_DONE
	 * state.
	 */
	bool                   tgf_stable;
	bool                   tgf_stopping;
	struct m0_sm_ast       tgf_ast_handle;
	struct m0_sm_ast       tgf_ast_stable;
	struct m0_sm_ast       tgf_ast_stop;
	struct m0_semaphore    tgf_start_sem;
	struct m0_semaphore    tgf_finish_sem;
	bool                   tgf_recovery_mode;
};

/** @todo XXX TODO s/gf/m/ in function parameters */
M0_INTERNAL void m0_be_tx_group_fom_init(struct m0_be_tx_group_fom *m,
					 struct m0_be_tx_group     *gr,
					 struct m0_reqh            *reqh);
M0_INTERNAL void m0_be_tx_group_fom_fini(struct m0_be_tx_group_fom *m);
M0_INTERNAL void m0_be_tx_group_fom_reset(struct m0_be_tx_group_fom *m);

M0_INTERNAL int m0_be_tx_group_fom_start(struct m0_be_tx_group_fom *gf);
M0_INTERNAL void m0_be_tx_group_fom_stop(struct m0_be_tx_group_fom *gf);

M0_INTERNAL void m0_be_tx_group_fom_handle(struct m0_be_tx_group_fom *m);
M0_INTERNAL void m0_be_tx_group_fom_stable(struct m0_be_tx_group_fom *gf);

M0_INTERNAL struct m0_sm_group *
m0_be_tx_group_fom__sm_group(struct m0_be_tx_group_fom *m);

M0_INTERNAL void
m0_be_tx_group_fom_recovery_prepare(struct m0_be_tx_group_fom *m);

M0_INTERNAL void m0_be_tx_group_fom_mod_init(void);
M0_INTERNAL void m0_be_tx_group_fom_mod_fini(void);

/** @} end of be group */
#endif /* __MERO_BE_TX_GROUP_FOM_H__ */

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
