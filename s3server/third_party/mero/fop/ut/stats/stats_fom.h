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
 * Original author: Andriy Tkachuk <andriy_tkachuk@xyratex.com>
 * Original creation date: 04/04/2013
 */

#pragma once

#ifndef __MERO_FOP_UT_STATS_FOM_H__
#define __MERO_FOP_UT_STATS_FOM_H__

#include "fop/fom.h"
#include "fop/fom_generic.h"


enum fom_stats_phase {
	PH_INIT = M0_FOM_PHASE_INIT,  /*< fom has been initialised. */
	PH_FINISH = M0_FOM_PHASE_FINISH,  /*< terminal phase. */
	PH_RUN
};

/**
 * Object encompassing FOM for stats
 * operation and necessary context data
 */
struct fom_stats {
	/** Generic m0_fom object. */
	struct m0_fom fs_gen;
};

#endif /* __MERO_FOP_UT_STATS_FOM_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
