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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 29-Nov-2016
 */

#pragma once

#ifndef __MERO_CONF_RCONFC_LINK_FOM_H__
#define __MERO_CONF_RCONFC_LINK_FOM_H__

#include "conf/rconfc_internal.h"

enum m0_rconfc_link_fom_states {
	/* Common */
	M0_RLF_INIT = M0_FOM_PHASE_INIT,
	M0_RLF_FINI = M0_FOM_PHASE_FINISH,
	/* Disconnect */
	M0_RLF_SESS_WAIT_IDLE,
	M0_RLF_SESS_TERMINATING,
	M0_RLF_CONN_TERMINATING,
};

/*
 * The fom type and ops below are exposed here for the sake of m0_fom_init()
 * done inside rconfc_herd_link__on_death_cb().
 */
extern struct m0_fom_type rconfc_link_fom_type;
extern const struct m0_fom_ops rconfc_link_fom_ops;

M0_INTERNAL int  m0_rconfc_mod_init(void);
M0_INTERNAL void m0_rconfc_mod_fini(void);

#endif /* __MERO_CONF_RCONFC_LINK_FOM_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
