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

#undef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER

#include "lib/trace.h"
#include "fis/fi_command.h"
#include "fis/fi_command_fops.h"  /* m0_fi_command_req_fopt */
#include "fop/fop.h"              /* m0_fop */
#include "rpc/rpc.h"              /* m0_rpc_session */
#include "rpc/rpclib.h"           /* m0_rpc_post_sync */

/**
 * @page fis-lspec-command Fault Injection Command.
 *
 * - @subpage fis-lspec-command-fops
 * - @subpage fis-lspec-command-fom
 *
 * Fault Injection Command may be sent from mero code by calling
 * m0_fi_command_post_sync(). The command is executed synchronously providing
 * m0_fi_command_rep::fcp_rc as a return code.
 *
 * @see fis/ut/fis_ut.c
 */

/**
 * @addtogroup fis-dlspec
 *
 * @{
 */
static bool fi_str_not_empty(const char *s)
{
	return s != NULL && *s != '\0';
}

M0_INTERNAL int m0_fi_command_post_sync(struct m0_rpc_session *sess,
					const char            *func,
					const char            *tag,
					enum m0_fi_disp        disp,
					uint32_t               num1,
					uint32_t               num2)
{
	struct m0_fop            *fop;
	struct m0_fi_command_rep *rep;
	struct m0_fi_command_req  req = {
		.fcr_func = M0_BUF_INITS((char *)func),
		.fcr_tag  = M0_BUF_INITS((char *)tag),
		.fcr_disp = disp,
		.fcr_num1 = num1,
		.fcr_num2 = num2,
	};
	int rc;

	M0_ENTRY("func=%s, tag=%s, disp=%d", func, tag, disp);
	M0_PRE(m0_rpc_session_validate(sess) == 0);
	M0_PRE(_0C(fi_str_not_empty(func)) &&
	       _0C(fi_str_not_empty(tag)));

	fop = m0_fop_alloc(&m0_fi_command_req_fopt, &req,
			   session_machine(sess));
	if (fop == NULL)
		return M0_ERR(-ENOMEM);
	rc = m0_rpc_post_sync(fop, sess, NULL, 0);
	if (rc == 0) {
		rep = m0_fop_data(m0_rpc_item_to_fop(fop->f_item.ri_reply));
		rc = rep->fcp_rc;
	}
	fop->f_data.fd_data = NULL; /* protect local data from freeing */
	m0_fop_put_lock(fop);
	return M0_RC(rc);
}

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
