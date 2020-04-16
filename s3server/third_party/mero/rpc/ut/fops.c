/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 18-Dec-2012
 */

#include "lib/memory.h"
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"
#include "rpc/rpc_opcodes.h"

#include "rpc/ut/fops_xc.h"

extern struct m0_reqh_service_type m0_rpc_service_type;

static int arrow_fom_create(struct m0_fop *fop, struct m0_fom **m,
			    struct m0_reqh *reqh);
static void arrow_fom_fini(struct m0_fom *fom);
static int arrow_fom_tick(struct m0_fom *fom);
static size_t arrow_fom_home_locality(const struct m0_fom *fom);

struct m0_fop_type m0_rpc_arrow_fopt;

struct m0_semaphore arrow_hit;
struct m0_semaphore arrow_destroyed;

static const struct m0_fom_type_ops arrow_fom_type_ops = {
	.fto_create = arrow_fom_create,
};

static const struct m0_fom_ops arrow_fom_ops = {
	.fo_fini          = arrow_fom_fini,
	.fo_tick          = arrow_fom_tick,
	.fo_home_locality = arrow_fom_home_locality,
};

M0_INTERNAL void m0_rpc_test_fops_init(void)
{
	m0_xc_rpc_ut_fops_init();
	M0_FOP_TYPE_INIT(&m0_rpc_arrow_fopt,
		.name      = "RPC_arrow",
		.opcode    = M0_RPC_ARROW_OPCODE,
		.xt        = arrow_xc,
		.rpc_flags = M0_RPC_ITEM_TYPE_ONEWAY,
		.fom_ops   = &arrow_fom_type_ops,
		.sm        = &m0_generic_conf,
		.svc_type  = &m0_rpc_service_type);
	m0_semaphore_init(&arrow_hit, 0);
	m0_semaphore_init(&arrow_destroyed, 0);
}

M0_INTERNAL void m0_rpc_test_fops_fini(void)
{
	m0_semaphore_fini(&arrow_destroyed);
	m0_semaphore_fini(&arrow_hit);
	m0_fop_type_fini(&m0_rpc_arrow_fopt);
	m0_xc_rpc_ut_fops_fini();
}


static int arrow_fom_create(struct m0_fop *fop, struct m0_fom **m,
			    struct m0_reqh *reqh)
{
	struct m0_fom *fom;

	M0_ALLOC_PTR(fom);
	M0_ASSERT(fom != NULL);

	m0_fom_init(fom, &m0_rpc_arrow_fopt.ft_fom_type, &arrow_fom_ops,
		    fop, NULL, reqh);
	*m = fom;
	return 0;
}

static void arrow_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(fom);
	m0_semaphore_up(&arrow_destroyed);
}

static int arrow_fom_tick(struct m0_fom *fom)
{
	m0_semaphore_up(&arrow_hit);
	m0_fom_phase_set(fom, M0_FOPH_FINISH);
	return M0_FSO_WAIT;
}

static size_t arrow_fom_home_locality(const struct m0_fom *fom)
{
	return 0;
}

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
