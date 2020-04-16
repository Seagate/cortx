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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 09/09/2011
 */

#pragma once

#ifndef __MERO_CONSOLE_MESG_H__
#define __MERO_CONSOLE_MESG_H__

#include "fop/fop.h"
#include "rpc/rpc.h"

/**
 *  Prints name and opcode of FOP.
 *  It can be used to print more info if required.
 */
M0_INTERNAL void m0_cons_fop_name_print(const struct m0_fop_type *ftype);

/**
 * @brief Builds and send FOP using rpc_post and waits for reply.
 *
 * @param fop	   FOP to be send.
 * @param session  RPC connection session.
 * @param resend_interval
 * @param nr_sent_max
 *                 Attempt to send fop at most nr_sent_max number of
 *                 times after each resend_interval; timeout fop if reply
 *                 is not received after nr_sent_max attempts.
 */
M0_INTERNAL int m0_cons_fop_send(struct m0_fop *fop,
				 struct m0_rpc_session *session,
				 m0_time_t resend_interval,
				 uint64_t nr_sent_max);

/**
 *  @brief Iterate over FOP fields and print names.
 */
M0_INTERNAL int m0_cons_fop_show(struct m0_fop_type *fopt);

/**
 * @brief Helper function to print list of FOPs.
 */
M0_INTERNAL void m0_cons_fop_list_show(void);

/* __MERO_CONSOLE_MESG_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

