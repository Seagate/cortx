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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 16-Aug-2016
 */

#pragma once

#ifndef __MERO_CM_REPREB_TRIGGER_FOM_H__
#define __MERO_CM_REPREB_TRIGGER_FOM_H__

/**
 * @defgroup CM
 *
 * @{
 */
#include "cm/repreb/cm.h"
#include "lib/types.h"
#include "fop/fom_generic.h"   /* M0_FOPH_NR */

struct m0_fom;

struct m0_fom_trigger_ops {
	struct m0_fop_type* (*fto_type)(uint32_t op);
	uint64_t (*fto_progress)(struct m0_fom *fom, bool reinit_counter);
	void (*fto_prepare)(struct m0_fom *fom);
};

struct m0_trigger_fom {
	const struct m0_fom_trigger_ops *tf_ops;
	struct m0_fom                    tf_fom;
};

#ifndef __KERNEL__

enum m0_trigger_phases {
	M0_TPH_PREPARE = M0_FOPH_NR + 1,
	M0_TPH_READY,
	M0_TPH_START,
	M0_TPH_FINI = M0_FOM_PHASE_FINISH
};

#endif

M0_INTERNAL int m0_trigger_fom_create(struct m0_trigger_fom  *tfom,
				      struct m0_fop          *fop,
				      struct m0_reqh         *reqh);
/** @} end of CM group */
#endif /* __MERO_CM_REPREB_TRIGGER_FOM_H__ */

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
