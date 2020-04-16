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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 27-Jan-2013
 */


#pragma once

#ifndef __MERO_DTM_OPERATION_H__
#define __MERO_DTM_OPERATION_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* import */
#include "dtm/nucleus.h"
#include "dtm/update.h"
#include "dtm/update_xc.h"
struct m0_dtm_remote;
struct m0_dtm;
struct m0_tl;

/* export */
struct m0_dtm_oper;

struct m0_dtm_oper {
	struct m0_dtm_op oprt_op;
	struct m0_tl     oprt_uu;
	uint64_t         oprt_flags;
};
M0_INTERNAL bool m0_dtm_oper_invariant(const struct m0_dtm_oper *oper);

enum m0_dtm_oper_flags {
	M0_DOF_CLOSED = 1 << 0,
	M0_DOF_LAST   = 1 << 1,
	M0_DOF_SENT   = 1 << 2
};

struct m0_dtm_oper_updates {
	uint32_t                    ou_nr;
	struct m0_dtm_update_descr *ou_update;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_dtm_oper_descr {
	struct m0_dtm_oper_updates od_updates;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL void m0_dtm_oper_init(struct m0_dtm_oper *oper, struct m0_dtm *dtm,
				  struct m0_tl *uu);
M0_INTERNAL void m0_dtm_oper_fini(struct m0_dtm_oper *oper);
M0_INTERNAL void m0_dtm_oper_close(struct m0_dtm_oper *oper);
M0_INTERNAL void m0_dtm_oper_prepared(const struct m0_dtm_oper *oper,
				      const struct m0_dtm_remote *rem);
M0_INTERNAL void m0_dtm_oper_done(const struct m0_dtm_oper *oper,
				  const struct m0_dtm_remote *rem);
M0_INTERNAL void m0_dtm_oper_pack(struct m0_dtm_oper *oper,
				  const struct m0_dtm_remote *rem,
				  struct m0_dtm_oper_descr *ode);
M0_INTERNAL void m0_dtm_oper_unpack(struct m0_dtm_oper *oper,
				    const struct m0_dtm_oper_descr *ode);
M0_INTERNAL int  m0_dtm_oper_build(struct m0_dtm_oper *oper, struct m0_tl *uu,
				   const struct m0_dtm_oper_descr *ode);
M0_INTERNAL void m0_dtm_reply_pack(const struct m0_dtm_oper *oper,
				   const struct m0_dtm_oper_descr *request,
				   struct m0_dtm_oper_descr *reply);
M0_INTERNAL void m0_dtm_reply_unpack(struct m0_dtm_oper *oper,
				     const struct m0_dtm_oper_descr *reply);

M0_INTERNAL struct m0_dtm_update *m0_dtm_oper_get(const struct m0_dtm_oper *oper,
						  uint32_t label);

/** @} end of dtm group */

#endif /* __MERO_DTM_OPERATION_H__ */


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
