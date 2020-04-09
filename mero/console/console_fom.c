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
 * Original author       : Dipak Dudhabhate <Dipak_Dudhabhate@xyratex.com>
 * Original creation date: 08/04/2011
 * Revision              : Manish Honap <Manish_Honap@xyratex.com>
 * Revision date         : 07/31/2012
 */

#include "lib/errno.h"		/* EINVAL */
#include "lib/memory.h"		/* M0_ALLOC_PTR */
#include "fop/fom_generic.h"    /* M0_FOPH_FAILURE */

#include "console/console_fom.h"
#include "console/console_fop.h"
#include "console/console_fop_xc.h"
#include "rpc/rpc.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONSOLE
#include "lib/trace.h"

/**
   @addtogroup console
   @{
*/

static int console_fom_create(struct m0_fop *fop, struct m0_reqh *reqh,
			      const struct m0_fom_ops *fom_ops,
			      struct m0_fom **m)
{
	struct m0_fom *fom;
	struct m0_fop *rep_fop;

        M0_PRE(fop != NULL);
        M0_PRE(m != NULL);

	/*
	 * XXX
	 * The proper way to do this is to do
	 * struct m0_cons_fom {
	 *         struct m0_fom cf_fom;
	 *         struct m0_fop cf_reply;
	 *         struct m0_cons_fop_reply cf_reply_data;
	 * };
	 * Then fom, reply fop and its data packet can be allocated at once,
	 * simplifying memory management.
	 */
        M0_ALLOC_PTR(fom);
        if (fom == NULL)
                return M0_ERR(-ENOMEM);
        rep_fop = m0_fop_reply_alloc(fop, &m0_cons_fop_reply_fopt);
	if (rep_fop == NULL) {
		m0_free(fom);
		return M0_ERR(-ENOMEM);
	}

	/**
	 * NOTE: Though service type is NOT set in the FOP_TYPE_INIT
	 * for console fops, we are setting it in the console UT,
	 * where the client thread creates the fop. So the assertion in
	 * m0_fom_init should pass
	 */
	m0_fom_init(fom, &fop->f_type->ft_fom_type,
		    fom_ops, fop, rep_fop, reqh);
        *m = fom;

	return 0;
}

static size_t console_fom_home_locality(const struct m0_fom *fom)
{
        M0_PRE(fom != NULL);

        return m0_fop_opcode(fom->fo_fop);
}

static void console_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(fom);
}

static int console_device_fom_create(struct m0_fop *fop, struct m0_fom **m,
				     struct m0_reqh *reqh)
{
        M0_PRE(fop != NULL);
        M0_PRE(m != NULL);
	M0_PRE(fop->f_type == &m0_cons_fop_device_fopt);

	return console_fom_create(fop, reqh, &m0_console_fom_device_ops, m);
}

static int console_fom_tick(struct m0_fom *fom)
{
        struct m0_cons_fop_reply *reply_fop;
        struct m0_rpc_item       *reply_item;
        struct m0_rpc_item       *req_item;
	struct m0_fop		 *fop = fom->fo_fop;
	struct m0_fop		 *rfop = fom->fo_rep_fop;

	M0_PRE(fom != NULL && fop != NULL && rfop != NULL);

	/* Reply fop */
        reply_fop = m0_fop_data(rfop);
	if (reply_fop == NULL) {
		m0_fom_phase_set(fom, M0_FOPH_FAILURE);
		return M0_FSO_AGAIN;
	}

	/* Request item */
        req_item = &fop->f_item;

	/* Set reply FOP */
	reply_fop->cons_notify_type = req_item->ri_type->rit_opcode;
        reply_fop->cons_return = 0;

	/* Reply item */
	reply_item = &rfop->f_item;
	m0_fom_phase_set(fom, M0_FOM_PHASE_FINISH);
	m0_rpc_reply_post(req_item, reply_item);
	return M0_FSO_WAIT;
}

const struct m0_fom_ops m0_console_fom_device_ops = {
	.fo_tick	  = console_fom_tick,
	.fo_fini	  = console_fom_fini,
	.fo_home_locality = console_fom_home_locality
};

const struct m0_fom_type_ops m0_console_fom_type_device_ops = {
	.fto_create = console_device_fom_create
};

static int console_test_fom_create(struct m0_fop  *fop,
				   struct m0_fom **m,
				   struct m0_reqh *reqh)
{
	M0_PRE(fop != NULL);
        M0_PRE(m != NULL);
	M0_PRE(fop->f_type == &m0_cons_fop_test_fopt);

	return console_fom_create(fop, reqh, &m0_console_fom_test_ops, m);
}

struct m0_fom_ops m0_console_fom_test_ops = {
	.fo_fini          = console_fom_fini,
	.fo_tick          = console_fom_tick,
	.fo_home_locality = console_fom_home_locality
};

const struct m0_fom_type_ops m0_console_fom_type_test_ops = {
	.fto_create = console_test_fom_create,
};

#undef M0_TRACE_SUBSYSTEM

/** @} end of console */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
