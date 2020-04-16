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
 * Original author: Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 07/07/2011
 */

#include "rpc/it/ping_fom.h"
#include "rpc/it/ping_fop.h"
#include "rpc/it/ping_fop_xc.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "rpc/rpc.h"
#include "fop/fom_generic.h"

static int ping_fop_fom_create(struct m0_fop *fop, struct m0_fom **m,
			       struct m0_reqh *reqh);

/** Generic ops object for ping */
struct m0_fom_ops m0_fom_ping_ops = {
	.fo_fini          = m0_fop_ping_fom_fini,
	.fo_tick          = m0_fom_ping_state,
	.fo_home_locality = m0_fom_ping_home_locality
};

/** FOM type specific functions for ping FOP. */
const struct m0_fom_type_ops m0_fom_ping_type_ops = {
	.fto_create = ping_fop_fom_create
};

M0_INTERNAL size_t m0_fom_ping_home_locality(const struct m0_fom *fom)
{
	static unsigned loc = 0;

	M0_PRE(fom != NULL);

	return loc++;
}

/**
 * State function for ping request
 */
M0_INTERNAL int m0_fom_ping_state(struct m0_fom *fom)
{
	struct m0_fop_ping_rep *ping_fop_rep;
	struct m0_rpc_item     *item;
	struct m0_fom_ping     *fom_obj;
	struct m0_fop          *fop;

	fom_obj = container_of(fom, struct m0_fom_ping, fp_gen);
	fop = m0_fop_reply_alloc(fom->fo_fop, &m0_fop_ping_rep_fopt);
	M0_ASSERT(fop != NULL);
	ping_fop_rep = m0_fop_data(fop);
	ping_fop_rep->fpr_rc = 0;
	item = m0_fop_to_rpc_item(fop);
	m0_rpc_reply_post(&fom_obj->fp_fop->f_item, item);
	m0_fom_phase_set(fom, M0_FOPH_FINISH);
	return M0_FSO_WAIT;
}

/* Init for ping */
static int ping_fop_fom_create(struct m0_fop *fop, struct m0_fom **m,
			       struct m0_reqh *reqh)
{
	struct m0_fom      *fom;
	struct m0_fom_ping *fom_obj;

	M0_PRE(fop != NULL);
	M0_PRE(m != NULL);

	fom_obj= m0_alloc(sizeof(struct m0_fom_ping));
	if (fom_obj == NULL)
		return -ENOMEM;
	fom = &fom_obj->fp_gen;
	m0_fom_init(fom, &fop->f_type->ft_fom_type, &m0_fom_ping_ops, fop,
		    NULL, reqh);
	fom_obj->fp_fop = fop;
	*m = fom;
	return 0;
}

M0_INTERNAL void m0_fop_ping_fom_fini(struct m0_fom *fom)
{
	struct m0_fom_ping *fom_obj;

	fom_obj = container_of(fom, struct m0_fom_ping, fp_gen);
	m0_fom_fini(fom);
	m0_free(fom_obj);
}

/** @} end of io_foms */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
