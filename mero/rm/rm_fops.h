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
 * Original author: Rajesh Bhalerao <Rajesh_Bhalerao@xyratex.com>
 * Original creation date: 07/07/2011
 */

#pragma once

#ifndef __MERO_RM_FOPS_H__
#define __MERO_RM_FOPS_H__

#include "lib/buf.h"
#include "lib/buf_xc.h"
#include "lib/cookie.h"
#include "lib/cookie_xc.h"

#include "fop/fom_generic.h"
#include "fop/fom_generic_xc.h"
#include "fop/fop.h"
#include "xcode/xcode_attr.h"

/**
 * @defgroup rmfop Resource Manager FOP Description
 * @{
 */

/**
 *
 * This file defines RM-fops needed for RM-generic layer. All the layers using
 * RM will have to define their own FOPs to fetch resource data. RM-generic FOPs
 * provide a facility for resource credits management and to fetch small
 * resource data.
 *
 * <b>RM fop formats</b>
 *
 * Various RM data-structures have to be located based on information stored in
 * fops:
 *
 *     - resource type: identified by 64-bit identifier
 *       (m0_rm_resource_type::rt_id),
 *
 *     - resource: resource information is never passed separately, but only to
 *       identify a resource owner,
 *
 *     - owner: when a first request to a remote resource owner is made, the
 *       owner is identified by the resource
 *       (m0_rm_resource_type_ops::rto_encode()) it is a responsibility of the
 *       remote RM to locate the owner. In the subsequent fops for the same
 *       owner, it is identified by a 128-bit cookie (m0_rm_cookie),
 *
 *     - credit: identified by an opaque byte array
 *       (m0_rm_resource_ops::rop_credit_decode(),
 *       m0_rm_credit_ops::rro_encode()). A 0-sized array in REVOKE and CANCEL
 *       fops is interpreted to mean "whole credit previously granted",
 *
 *     - loan: identified by a 128-bit cookie (m0_rm_cookie).
 *
 */

struct m0_rm_fop_owner {
	struct m0_cookie ow_cookie;
	struct m0_buf    ow_resource;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rm_fop_loan {
	struct m0_cookie lo_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rm_fop_credit {
	struct m0_buf cr_opaque;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rm_fop_req {
	/* Could either be debtor or creditor */
	struct m0_rm_fop_owner  rrq_owner;
	struct m0_rm_fop_credit rrq_credit;
	uint64_t                rrq_policy;
	uint64_t                rrq_flags;
	/* Next three fields determine request reserve priority.
	 * @see m0_rm_incoming, m0_rm_reserve_prio
	 */
	struct m0_fid           rrq_orig_owner;
	m0_time_t               rrq_orig_time;
	uint64_t                rrq_orig_seq;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rm_fop_borrow {
	struct m0_rm_fop_req   bo_base;
	struct m0_rm_fop_owner bo_creditor;
	struct m0_uint128      bo_group_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rm_fop_borrow_rep {
	int32_t                     br_rc;
	struct m0_rm_fop_loan       br_loan;
	struct m0_rm_fop_credit     br_credit;
	struct m0_buf               br_lvb;
	/**
	 * Whenever a borrow request is made with creditor cookie NULL,
	 * A new owner is created on creditor side and this creditor is
	 * assigned a new cookie. Next time borrower wants to make a new
	 * request for resource, this cookie will be used.
	 */
	struct m0_cookie            br_creditor_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rm_fop_revoke {
	struct m0_rm_fop_req  fr_base;
	struct m0_rm_fop_loan fr_loan;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rm_fop_revoke_rep {
	int32_t                     rr_rc;
	/**
	 * Debtor sends its own cookie.
	 * This is useful for the creditor to identify the debtor.
	 * It's necessary to identify the debtor when a loan has been
	 * given to many debtors belonging to the same group.
	 */
	struct m0_cookie            rr_debtor_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_rm_fop_cancel {
	struct m0_rm_fop_loan fc_loan;
	struct m0_cookie      fc_creditor_cookie;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Externs
 */
extern struct m0_fop_type m0_rm_fop_borrow_fopt;
extern struct m0_fop_type m0_rm_fop_borrow_rep_fopt;
extern struct m0_fop_type m0_rm_fop_revoke_fopt;
extern struct m0_fop_type m0_rm_fop_revoke_rep_fopt;
extern struct m0_fop_type m0_rm_fop_cancel_fopt;
extern struct m0_fop_type m0_fop_generic_reply_fopt;

/**
 * FOP init() and fini() functions.
 */
M0_INTERNAL int m0_rm_fop_init(void);
M0_INTERNAL void m0_rm_fop_fini(void);

/** @} end of Resource manager FOP description */

/* __MERO_RM_FOPS_H__ */
#endif

/**
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
