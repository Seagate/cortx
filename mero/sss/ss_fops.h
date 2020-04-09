/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@xyratex.com>
 * Original creation date: 03-Jun-2014
 */

#pragma once

#ifndef __MERO_SSS_SS_FOPS_H__
#define __MERO_SSS_SS_FOPS_H__

#include "conf/onwire.h"
#include "lib/types_xc.h"
#include "lib/buf_xc.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"

/**
 * @defgroup ss_fop Start stop FOP
 * @{
 */

struct m0_ref;
extern struct m0_fop_type m0_fop_ss_fopt;
extern struct m0_fop_type m0_fop_ss_rep_fopt;
extern struct m0_fop_type m0_fop_ss_svc_list_fopt;
extern struct m0_fop_type m0_fop_ss_svc_list_rep_fopt;

/** Service commands. */
enum m0_sss_req_cmd {
	M0_SERVICE_START,
	M0_SERVICE_STOP,
	M0_SERVICE_STATUS,
	M0_SERVICE_QUIESCE,
	M0_SERVICE_INIT,
	M0_SERVICE_HEALTH,
};

/** Request to start/stop a service. */
struct m0_sss_req {
	/**
	 * Command to execute.
	 * @see enum m0_sss_req_cmd
	 */
	uint32_t      ss_cmd;
	/**
	 * Name of service type. Mandatory only for M0_SERVICE_INIT command.
	 * @see m0_reqh_service_type::rst_name
	 */
	struct m0_buf ss_name;
	/**
	 * Identifier of the service being started.
	 * fid type should set to M0_CONF_SERVICE_TYPE.cot_ftype
	 */
	struct m0_fid ss_id;
	/** Opaque parameter. */
	struct m0_buf ss_param;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Response to m0_sss_req. */
struct m0_sss_rep {
	/**
	 * Result of service operation
	 */
	int32_t  ssr_rc;
	/**
	 * Service status. Undefined if ssr_rc < 0.
	 * @see enum m0_reqh_service_state
	 */
	uint32_t ssr_state;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL int m0_ss_fops_init(void);
M0_INTERNAL void m0_ss_fops_fini(void);
M0_INTERNAL void m0_ss_fop_release(struct m0_ref *ref);

/** @} ss_fop */
#endif /* __MERO_SSS_SS_FOPS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
