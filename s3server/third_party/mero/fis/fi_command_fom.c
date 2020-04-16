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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 10-Feb-2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER

#include "lib/trace.h"
#include "lib/memory.h"           /* M0_ALLOC_PTR, m0_free */
#include "lib/buf.h"              /* m0_buf_strdup */
#include "lib/finject.h"          /* M0_FI_ENABLED */

#include "fis/fi_command.h"
#include "fis/fi_command_fops.h"
#include "fop/fom_generic.h"
#include "fop/fop.h"
#include "rpc/rpc.h"
#include "sm/sm.h"

/**
 * @page fis-lspec-command-fom Fault Injection Command FOM.
 *
 * Fault Injection Command FOM is based on generic FOM functionality. The
 * arrived @ref m0_fi_command_req::fcr_disp is checked for supported types. In
 * case the command not supported, -EINVAL is replied immediately to caller
 * side.  Otherwise, the corresponding function from m0_fi_xxx family is called
 * with the provided command parameters.
 */

/**
 * @addtogroup fis-dlspec
 *
 * @{
 */
struct m0_fi_command_fom {
	/** Generic m0_fom object. */
        struct m0_fom  fcf_gen;
	/** FOP associated with this FOM. */
        struct m0_fop *fcf_fop;
};

extern const struct m0_fom_ops fi_command_fom_ops;

static int fi_command_fom_create(struct m0_fop *fop, struct m0_fom **out,
				 struct m0_reqh *reqh)
{
	struct m0_fom            *fom;
	struct m0_fi_command_fom *fom_obj;
	struct m0_fop            *fop_rep;

	M0_ENTRY();
	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);

	fop_rep = M0_FI_ENABLED("no_mem") ? NULL :
		m0_fop_reply_alloc(fop, &m0_fi_command_rep_fopt);
	M0_ALLOC_PTR(fom_obj);
	if (fop_rep == NULL || fom_obj == NULL)
		goto no_mem;
	fom = &fom_obj->fcf_gen;
	m0_fom_init(fom, &fop->f_type->ft_fom_type, &fi_command_fom_ops, fop,
		    fop_rep, reqh);
	fom_obj->fcf_fop = fop;
	*out = fom;
	return M0_RC(0);
no_mem:
	m0_free(fop_rep);
	m0_free(fom_obj);
	return M0_ERR(-ENOMEM);
}

static void fi_command_fom_fini(struct m0_fom *fom)
{
	struct m0_fi_command_fom *fom_obj = M0_AMB(fom_obj, fom, fcf_gen);

	m0_fom_fini(fom);
	m0_free(fom_obj);
}

static int fi_command_execute(const struct m0_fi_command_req *req)
{
	char *func = m0_buf_strdup(&req->fcr_func);
	char *tag  = m0_buf_strdup(&req->fcr_tag);
	int   rc   = 0;

	M0_ENTRY("func=%s tag=%s disp=%d", func, tag, req->fcr_disp);
	if (func == NULL || tag == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	switch (req->fcr_disp) {
	case M0_FI_DISP_ENABLE:
		m0_fi_enable(func, tag);
		break;
	case M0_FI_DISP_DISABLE:
		m0_fi_disable(func, tag);
		break;
	case M0_FI_DISP_ENABLE_ONCE:
		m0_fi_enable_once(func, tag);
		break;
	case M0_FI_DISP_RANDOMIZE:
		m0_fi_enable_random(func, tag, req->fcr_num1);
		break;
	case M0_FI_DISP_DO_OFF_N_ON_M:
		m0_fi_enable_off_n_on_m(func, tag, req->fcr_num1,
					req->fcr_num2);
		break;
	default:
		rc = -EINVAL;
	}
out:
	m0_free(func);
	m0_free(tag);
	return rc == 0 ? M0_RC(rc) : M0_ERR(rc);
}

static int fi_command_fom_tick(struct m0_fom *fom)
{
	struct m0_fi_command_fom *fcf = M0_AMB(fcf, fom, fcf_gen);
	struct m0_fi_command_req *req = m0_fop_data(fcf->fcf_fop);
	struct m0_fop            *fop = fom->fo_rep_fop;
	struct m0_fi_command_rep *rep = m0_fop_data(fop);

	rep->fcp_rc = fi_command_execute(req);
	m0_rpc_reply_post(&fcf->fcf_fop->f_item, m0_fop_to_rpc_item(fop));
	m0_fom_phase_set(fom, M0_FOPH_FINISH);
	return M0_FSO_WAIT;
}

static size_t fi_command_fom_locality(const struct m0_fom *fom)
{
	static size_t locality = 0;

	return locality++;
}

const struct m0_fom_type_ops m0_fi_command_fom_type_ops = {
	.fto_create = fi_command_fom_create
};

const struct m0_fom_ops fi_command_fom_ops = {
	.fo_fini          = fi_command_fom_fini,
	.fo_tick          = fi_command_fom_tick,
	.fo_home_locality = fi_command_fom_locality
};

/** @} end fis-dlspec */
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
