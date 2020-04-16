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
 * Original author: Rohan Puri <Rohan_Puri@xyratex.com>,
 *                  Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 04/15/2011
 */

#pragma once

#ifndef __MERO_RPC_SESSION_FOM_H__
#define __MERO_RPC_SESSION_FOM_H__

#include "fop/fop.h"
#include "rpc/session_fops.h"
#include "rpc/session_fops_xc.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"     /* M0_FOPH_NR */

/**
   @addtogroup rpc_session

   @{

   This file contains, fom declarations for
   [conn|session]_[establish|terminate].
 */

/*
 * FOM to execute "RPC connection create" request
 */

enum m0_rpc_fom_conn_establish_phase {
	M0_FOPH_CONN_ESTABLISHING = M0_FOPH_NR + 1
};

extern struct m0_fom_type m0_rpc_fom_conn_establish_type;
extern const struct m0_fom_ops m0_rpc_fom_conn_establish_ops;

M0_INTERNAL size_t m0_rpc_session_default_home_locality(const struct m0_fom
							*fom);
M0_INTERNAL int m0_rpc_fom_conn_establish_tick(struct m0_fom *fom);
M0_INTERNAL void m0_rpc_fom_conn_establish_fini(struct m0_fom *fom);

/*
 * FOM to execute "Session Create" request
 */

enum m0_rpc_fom_session_establish_phase {
	M0_FOPH_SESSION_ESTABLISHING = M0_FOPH_NR + 1
};

extern struct m0_fom_type m0_rpc_fom_session_establish_type;
extern const struct m0_fom_ops m0_rpc_fom_session_establish_ops;

M0_INTERNAL int m0_rpc_fom_session_establish_tick(struct m0_fom *fom);
M0_INTERNAL void m0_rpc_fom_session_establish_fini(struct m0_fom *fom);

/*
 * FOM to execute session terminate request
 */

enum m0_rpc_fom_session_terminate_phase {
	M0_FOPH_SESSION_TERMINATING = M0_FOPH_NR + 1
};

extern struct m0_fom_type m0_rpc_fom_session_terminate_type;
extern const struct m0_fom_ops m0_rpc_fom_session_terminate_ops;

M0_INTERNAL int m0_rpc_fom_session_terminate_tick(struct m0_fom *fom);
M0_INTERNAL void m0_rpc_fom_session_terminate_fini(struct m0_fom *fom);

/*
 * FOM to execute RPC connection terminate request
 */

enum m0_rpc_fom_conn_terminate_phase {
	M0_FOPH_CONN_TERMINATING = M0_FOPH_NR + 1
};

extern struct m0_fom_type m0_rpc_fom_conn_terminate_type;
extern const struct m0_fom_ops m0_rpc_fom_conn_terminate_ops;

M0_INTERNAL int m0_rpc_fom_conn_terminate_tick(struct m0_fom *fom);
M0_INTERNAL void m0_rpc_fom_conn_terminate_fini(struct m0_fom *fom);


/*
 * Context fom used store fom data between its processing phases.
 */
struct m0_rpc_connection_session_specific_fom {
	/**
	   Genreric fom
	 */
	struct m0_fom ssf_fom_generic;

	/**
	   session pointer, during termination phase it has to be stored
	   between FOM phases
	 */
	void    *ssf_term_session;
};

#endif

/** @} end of rpc_session group */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
