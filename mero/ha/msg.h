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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 26-Apr-2016
 */

#pragma once

#ifndef __MERO_HA_MSG_H__
#define __MERO_HA_MSG_H__

/**
 * @defgroup ha
 *
 * @{
 */

#include "lib/types.h"          /* UINT64_MAX */
#include "lib/time.h"           /* m0_time_t */
#include "lib/cookie.h"         /* m0_cookie */

#include "fid/fid.h"            /* m0_fid */
#include "stob/ioq_error.h"     /* m0_stob_ioq_error */
#include "ha/note.h"            /* m0_ha_msg_nvec */
#include "ha/failvec.h"         /* m0_ha_msg_failure_vec_req */
#include "mero/keepalive.h"     /* m0_ha_msg_keepalive_req */
#include "conf/ha.h"            /* m0_conf_ha_process */
#include "rpc/ha.h"             /* m0_rpc_ha_msg */
#include "be/ha.h"              /* m0_be_io_err */
#include "sns/cm/ha.h"		/* m0_ha_sns_err */

/*
 * XXX The generator of *_xc.h files cannot handle dependencies, so we have
 * to specify them manually.
 */
#include "stob/ioq_error_xc.h"  /* workaround */
#include "lib/types_xc.h"       /* m0_uint128_xc */
#include "lib/cookie_xc.h"      /* m0_cookie_xc */
#include "ha/note_xc.h"         /* m0_ha_msg_nvec_xc */
#include "ha/failvec_xc.h"      /* m0_ha_msg_failure_vec_req */
#include "mero/keepalive_xc.h"  /* m0_ha_msg_keepalive_req_xc */
#include "conf/ha_xc.h"         /* m0_conf_ha_process_xc */
#include "rpc/ha_xc.h"          /* m0_rpc_ha_msg_xc */
#include "be/ha_xc.h"           /* m0_be_io_err_xc */
#include "sns/cm/ha_xc.h"	/* m0_ha_sns_err_xc */

enum {
	M0_HA_MSG_FAILURE_VEC_LIMIT = 1024,
};

enum {
	M0_HA_MSG_TAG_UNKNOWN = 0,
	M0_HA_MSG_TAG_INVALID = UINT64_MAX,
};

enum m0_ha_msg_type {
	M0_HA_MSG_INVALID,
	M0_HA_MSG_STOB_IOQ,
	M0_HA_MSG_NVEC,
	M0_HA_MSG_FAILURE_VEC_REQ,
	M0_HA_MSG_FAILURE_VEC_REP,
	M0_HA_MSG_KEEPALIVE_REQ,
	M0_HA_MSG_KEEPALIVE_REP,
	M0_HA_MSG_EVENT_PROCESS,
	M0_HA_MSG_EVENT_SERVICE,
	M0_HA_MSG_EVENT_RPC,
	M0_HA_MSG_BE_IO_ERR,
	M0_HA_MSG_SNS_ERR,
};

struct m0_ha_msg_data {
	uint64_t hed_type;           /**< m0_ha_msg_type */
	union {
		struct m0_stob_ioq_error hed_stob_ioq
					M0_XCA_TAG("M0_HA_MSG_STOB_IOQ");
		struct m0_ha_msg_nvec    hed_nvec
					M0_XCA_TAG("M0_HA_MSG_NVEC");
		struct m0_ha_msg_failure_vec_req hed_fvec_req
					M0_XCA_TAG("M0_HA_MSG_FAILURE_VEC_REQ");
		struct m0_ha_msg_failure_vec_rep hed_fvec_rep
					M0_XCA_TAG("M0_HA_MSG_FAILURE_VEC_REP");
		struct m0_ha_msg_keepalive_req   hed_keepalive_req
					M0_XCA_TAG("M0_HA_MSG_KEEPALIVE_REQ");
		struct m0_ha_msg_keepalive_rep   hed_keepalive_rep
					M0_XCA_TAG("M0_HA_MSG_KEEPALIVE_REP");
		struct m0_conf_ha_process        hed_event_process
					M0_XCA_TAG("M0_HA_MSG_EVENT_PROCESS");
		struct m0_conf_ha_service        hed_event_service
					M0_XCA_TAG("M0_HA_MSG_EVENT_SERVICE");
		struct m0_ha_msg_rpc             hed_event_rpc
					M0_XCA_TAG("M0_HA_MSG_EVENT_RPC");
		struct m0_be_io_err              hed_be_io_err
					M0_XCA_TAG("M0_HA_MSG_BE_IO_ERR");
		struct m0_ha_sns_err		 hed_ha_sns_err
					M0_XCA_TAG("M0_HA_MSG_SNS_ERR");
	} u;
} M0_XCA_UNION M0_XCA_DOMAIN(rpc);

struct m0_ha_msg {
/* public fields */
	struct m0_fid          hm_fid;            /**< conf object fid */
	struct m0_fid          hm_source_process; /**< source process fid */
	struct m0_fid          hm_source_service; /**< source service fid */
	m0_time_t              hm_time;           /**< event timestamp */
	uint64_t               hm_epoch;          /**< message epoch */
	struct m0_ha_msg_data  hm_data;           /**< user data */
/* private fields */
	/** unique across a pair of m0_ha_link tag */
	uint64_t               hm_tag;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Returns m0_ha_msg tag. */
M0_INTERNAL uint64_t m0_ha_msg_tag(const struct m0_ha_msg *msg);
/** Returns m0_ha_msg type */
M0_INTERNAL enum m0_ha_msg_type m0_ha_msg_type_get(const struct m0_ha_msg *msg);

/** Compares 2 m0_ha_msg. It's useful in tests. */
M0_INTERNAL bool m0_ha_msg_eq(const struct m0_ha_msg *msg1,
                              const struct m0_ha_msg *msg2);

/** Prints message content. Useful for debugging. */
M0_INTERNAL void m0_ha_msg_debug_print(const struct m0_ha_msg *msg,
                                       const char             *prefix);

/** @} end of ha group */
#endif /* __MERO_HA_MSG_H__ */

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
