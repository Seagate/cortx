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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 12/10/2011
 */

#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/chan.h"
#include "lib/finject.h"
#include "lib/time.h"
#include "lib/misc.h"           /* M0_IN() */
#include "fop/fop.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"

#include "rpc/rpc.h"
#include "rpc/rpclib.h"
#include "fop/fop_item_type.h"

#include "ut/cs_fop.h"
#include "ut/cs_fop_xc.h"
#include "rpc/rpc_opcodes.h"

static void cs_ut_rpc_item_reply_cb(struct m0_rpc_item *item);

/*
  RPC item operations structures.
 */
const struct m0_rpc_item_ops cs_ds_req_fop_rpc_item_ops = {
        .rio_replied = cs_ut_rpc_item_reply_cb,
};

struct m0_fop_type cs_ds1_req_fop_fopt;
struct m0_fop_type cs_ds1_rep_fop_fopt;
struct m0_fop_type cs_ds2_req_fop_fopt;
struct m0_fop_type cs_ds2_rep_fop_fopt;

/*
  Fom specific routines for corresponding fops.
 */
static int cs_req_fop_fom_tick(struct m0_fom *fom);
static int cs_ds1_req_fop_fom_create(struct m0_fop *fop, struct m0_fom **out,
				     struct m0_reqh *reqh);
static int cs_ds2_req_fop_fom_create(struct m0_fop *fop, struct m0_fom **out,
				     struct m0_reqh *reqh);
static void cs_ut_fom_fini(struct m0_fom *fom);
static size_t cs_ut_find_fom_home_locality(const struct m0_fom *fom);

/*
  Operation structures for ds1 service foms.
 */
static const struct m0_fom_ops cs_ds1_req_fop_fom_ops = {
	.fo_fini = cs_ut_fom_fini,
	.fo_tick = cs_req_fop_fom_tick,
	.fo_home_locality = cs_ut_find_fom_home_locality
};

/*
  Operation structures for ds2 service foms.
 */
static const struct m0_fom_ops cs_ds2_req_fop_fom_ops = {
	.fo_fini = cs_ut_fom_fini,
	.fo_tick = cs_req_fop_fom_tick,
	.fo_home_locality = cs_ut_find_fom_home_locality
};

extern struct m0_reqh_service_type ds1_service_type;
extern struct m0_reqh_service_type ds2_service_type;

enum ds_phases {
	M0_FOPH_DS1_REQ = M0_FOPH_NR + 1,
	M0_FOPH_DS2_REQ = M0_FOPH_NR + 1,
};

/*
  Fom type operations for ds1 service foms.
 */
static const struct m0_fom_type_ops cs_ds1_req_fop_fom_type_ops = {
        .fto_create = cs_ds1_req_fop_fom_create,
};

/*
  Fom type operations for ds2 service foms.
 */
static const struct m0_fom_type_ops cs_ds2_req_fop_fom_type_ops = {
        .fto_create = cs_ds2_req_fop_fom_create,
};

static void cs_ut_rpc_item_reply_cb(struct m0_rpc_item *item)
{
	struct m0_fop *reply;

        M0_PRE(item != NULL);
	M0_PRE(M0_IN(m0_fop_opcode(m0_rpc_item_to_fop(item)),
		     (M0_CS_DS1_REQ_OPCODE, M0_CS_DS2_REQ_OPCODE)));

	if (m0_rpc_item_error(item) == 0) {
		reply = m0_rpc_item_to_fop(item->ri_reply);
		M0_ASSERT(M0_IN(m0_fop_opcode(reply), (M0_CS_DS1_REP_OPCODE,
						       M0_CS_DS2_REP_OPCODE)));
	}
}

void m0_cs_ut_ds1_fop_fini(void)
{
	m0_fop_type_fini(&cs_ds1_req_fop_fopt);
	m0_fop_type_fini(&cs_ds1_rep_fop_fopt);
	m0_xc_ut_cs_fop_fini();
}

int m0_cs_ut_ds1_fop_init(void)
{
	/*
	 * As we are finalising and initialising fop types multiple times per
	 * service for various m0d commands, So reinitialise fop_type_format for
	 * each corresponding service fop types.
	 */
	m0_xc_ut_cs_fop_init();
	M0_FOP_TYPE_INIT(&cs_ds1_req_fop_fopt,
			 .name      = "ds1 request",
			 .opcode    = M0_CS_DS1_REQ_OPCODE,
			 .xt        = cs_ds1_req_fop_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fom_ops   = &cs_ds1_req_fop_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &ds1_service_type);
	M0_FOP_TYPE_INIT(&cs_ds1_rep_fop_fopt,
			 .name      = "ds1 reply",
			 .opcode    = M0_CS_DS1_REP_OPCODE,
			 .xt        = cs_ds1_rep_fop_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .fom_ops   = &cs_ds1_req_fop_fom_type_ops);
	return 0;
}

void m0_cs_ut_ds2_fop_fini(void)
{
	m0_fop_type_fini(&cs_ds2_rep_fop_fopt);
	m0_fop_type_fini(&cs_ds2_req_fop_fopt);
	m0_xc_ut_cs_fop_fini();
}

int m0_cs_ut_ds2_fop_init(void)
{
	/*
	  As we are finalising and initialising fop types multiple times
	  per service for various m0d commands, So reinitialise
	  fop_type_format for each corresponding service fop types.
	*/
	m0_xc_ut_cs_fop_init();
	M0_FOP_TYPE_INIT(&cs_ds2_req_fop_fopt,
			 .name      = "ds2 request",
			 .opcode    = M0_CS_DS2_REQ_OPCODE,
			 .xt        = cs_ds2_req_fop_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fom_ops   = &cs_ds2_req_fop_fom_type_ops,
			 .sm        = &m0_generic_conf,
			 .svc_type  = &ds2_service_type);
	M0_FOP_TYPE_INIT(&cs_ds2_rep_fop_fopt,
			 .name      = "ds2 reply",
			 .opcode    = M0_CS_DS2_REP_OPCODE,
			 .xt        = cs_ds2_rep_fop_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY,
			 .fom_ops   = &cs_ds2_req_fop_fom_type_ops);
	return 0;
}

void m0_ut_fom_phase_set(struct m0_fom *fom, int phase)
{
	switch (m0_fom_phase(fom)) {
	case M0_FOPH_SUCCESS:
		m0_fom_phase_set(fom, M0_FOPH_FOL_REC_ADD);
		/* fall through */
	case M0_FOPH_FAILURE:
		m0_fom_phase_set(fom, M0_FOPH_TXN_COMMIT);
		m0_fom_phase_set(fom, M0_FOPH_QUEUE_REPLY);
		/* fall through */
	default:
		if (m0_fom_phase(fom) != phase)
			m0_fom_phase_set(fom, phase);
	}
}

/*
  Allocates and initialises a fom.
 */
static int cs_ds_req_fop_fom_create(struct m0_fop *fop,
				    const struct m0_fom_ops *ops,
				    struct m0_fom **out, struct m0_reqh *reqh)
{
        struct m0_fom         *fom;
	struct m0_fop         *rfop;

	M0_PRE(fop != NULL);
	M0_PRE(ops != NULL);
        M0_PRE(out != NULL);
	M0_PRE(M0_IN(m0_fop_opcode(fop), (M0_CS_DS1_REQ_OPCODE,
					  M0_CS_DS2_REQ_OPCODE)));

        M0_ALLOC_PTR(fom);
        if (fom == NULL)
                return -ENOMEM;

	rfop = m0_fop_opcode(fop) == M0_CS_DS1_REQ_OPCODE ?
		m0_fop_reply_alloc(fop, &cs_ds1_rep_fop_fopt) :
		m0_fop_reply_alloc(fop, &cs_ds2_rep_fop_fopt);
	if (rfop == NULL) {
		m0_free(fom);
		return -ENOMEM;
	}
	m0_fom_init(fom, &fop->f_type->ft_fom_type, ops, fop, rfop, reqh);

        *out = fom;
        return 0;
}

static int cs_ds1_req_fop_fom_create(struct m0_fop *fop, struct m0_fom **out,
				     struct m0_reqh *reqh)
{
	return cs_ds_req_fop_fom_create(fop, &cs_ds1_req_fop_fom_ops, out,
					reqh);
}

static int cs_ds2_req_fop_fom_create(struct m0_fop *fop, struct m0_fom **out,
				     struct m0_reqh *reqh)
{
	return cs_ds_req_fop_fom_create(fop, &cs_ds2_req_fop_fom_ops, out,
					reqh);
}

/*
  Finalises a fom.
 */
static void cs_ut_fom_fini(struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

        m0_fom_fini(fom);
        m0_free(fom);
}

/*
  Returns an index value base on fom parameters to locate fom's
  home locality to execute a fom.
 */
static size_t cs_ut_find_fom_home_locality(const struct m0_fom *fom)
{
	M0_PRE(fom != NULL);

	return m0_fop_opcode(fom->fo_fop);
}

/*
  Transitions fom through its generic phases and also
  performs corresponding fop specific execution.
 */
static int cs_req_fop_fom_tick(struct m0_fom *fom)
{
	int                    rc;
	struct cs_ds1_req_fop *ds1_reqfop;
	struct cs_ds1_rep_fop *ds1_repfop;
	struct cs_ds2_req_fop *ds2_reqfop;
	struct cs_ds2_rep_fop *ds2_repfop;
	uint64_t               opcode;

	M0_PRE(M0_IN(m0_fop_opcode(fom->fo_fop), (M0_CS_DS1_REQ_OPCODE,
						  M0_CS_DS2_REQ_OPCODE)));
	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		rc = m0_fom_tick_generic(fom);
	} else {
		opcode = m0_fop_opcode(fom->fo_fop);
		switch (opcode) {
		case M0_CS_DS1_REQ_OPCODE:
			ds1_reqfop = m0_fop_data(fom->fo_fop);
			ds1_repfop = m0_fop_data(fom->fo_rep_fop);
			ds1_repfop->csr_rc = ds1_reqfop->csr_value;
			m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
			rc = M0_FSO_AGAIN;
			break;
		case M0_CS_DS2_REQ_OPCODE:
			ds2_reqfop = m0_fop_data(fom->fo_fop);
			ds2_repfop = m0_fop_data(fom->fo_rep_fop);
			ds2_repfop->csr_rc = ds2_reqfop->csr_value;
			m0_fom_phase_set(fom, M0_FOPH_SUCCESS);
			rc = M0_FSO_AGAIN;
			break;
		default:
			M0_ASSERT("Invalid fop" == 0);
			rc = 0;
			break;
		}
	}
	if (M0_FI_ENABLED("inject_delay"))
		m0_nanosleep(m0_time(M0_RPC_ITEM_RESEND_INTERVAL +1, 0), NULL);

	return rc;
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
