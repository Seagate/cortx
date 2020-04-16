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
 * Original creation date: 28-Apr-2016
 */

#pragma once

#ifndef __MERO_HA_ENTRYPOINT_H__
#define __MERO_HA_ENTRYPOINT_H__

/**
 * @defgroup ha
 *
 * client
 * - INIT FINI AVAILABLE CANCEL WAIT UNAVAILABLE FILL SEND
 * server
 * - stateless
 *
 * constraints
 * - only 1 outgoing entrypoint request in progress;
 * - unlimited number of incoming entrypoints requests at the same time;
 * @{
 */

#include "lib/types.h"          /* uint32_t */
#include "lib/buf.h"            /* m0_buf */
#include "lib/chan.h"           /* m0_chan */
#include "lib/semaphore.h"      /* m0_semaphore */
#include "lib/tlist.h"          /* m0_tl */
#include "lib/mutex.h"          /* m0_mutex */

#include "sm/sm.h"              /* m0_sm */
#include "fid/fid.h"            /* m0_fid */
#include "xcode/xcode_attr.h"   /* M0_XCA_RECORD */
#include "fop/fom.h"            /* M0_FOM_PHASE_INIT */
#include "fop/fop.h"            /* m0_fop */
#include "ha/entrypoint_fops.h" /* m0_ha_entrypoint_req */
#include "rpc/link.h"           /* m0_rpc_link */


struct m0_ha_entrypoint_server;
struct m0_rpc_session;
struct m0_rpc_item;

enum m0_ha_entrypoint_server_fom_state {
	M0_HES_INIT = M0_FOM_PHASE_INIT,
	M0_HES_FINI = M0_FOM_PHASE_FINISH,
	M0_HES_REPLY_WAIT,
};

struct m0_ha_entrypoint_server_cfg {
	struct m0_reqh         *hesc_reqh;
	struct m0_rpc_machine  *hesc_rpc_machine;
	void                  (*hesc_request_received)
		(struct m0_ha_entrypoint_server    *hes,
		 const struct m0_ha_entrypoint_req *req,
		 const struct m0_uint128           *req_id);
};

struct m0_ha_entrypoint_server {
	struct m0_ha_entrypoint_server_cfg  hes_cfg;
	struct m0_reqh_service             *hes_he_service;
	struct m0_tl                        hes_requests;
	struct m0_uint128                   hes_next_id;
	/** protects hes_requests */
	struct m0_mutex                     hes_lock;
};

enum m0_ha_entrypoint_client_state {
	M0_HEC_INIT,
	M0_HEC_STOPPED,
	M0_HEC_UNAVAILABLE,
	M0_HEC_CONNECT,
	M0_HEC_CONNECT_WAIT,
	M0_HEC_FILL,
	M0_HEC_SEND,
	M0_HEC_SEND_WAIT,
	M0_HEC_DISCONNECT,
	M0_HEC_DISCONNECT_WAIT,
	M0_HEC_AVAILABLE,
	M0_HEC_FINI,
};

struct m0_ha_entrypoint_client_cfg {
	struct m0_reqh        *hecc_reqh;
	struct m0_rpc_machine *hecc_rpc_machine;
	struct m0_fid          hecc_process_fid;
};

struct m0_ha_entrypoint_client {
	struct m0_ha_entrypoint_client_cfg  ecl_cfg;
	struct m0_rpc_link                  ecl_rlink;
	struct m0_clink                     ecl_rlink_wait;
	struct m0_ha_entrypoint_req         ecl_req;
	struct m0_fop                       ecl_req_fop;
	struct m0_ha_entrypoint_req_fop     ecl_req_fop_data;
	struct m0_ha_entrypoint_rep         ecl_rep;
	struct m0_fom                       ecl_fom;
	/** must be accessed under ecl_sm_group lock */
	bool                                ecl_fom_running;
	struct m0_mutex                     ecl_fom_running_lock;
	/** is true when ecl stops; avoids infinite wait for a dead HA */
	bool                                ecl_stopping;
	struct m0_sm                        ecl_sm;
	struct m0_sm_group                  ecl_sm_group;
	struct m0_rpc_item                 *ecl_reply;
	bool                                ecl_send_error;
	/** subscribes to ecl_sm state transitions  */
	struct m0_clink                     ecl_clink;
};

M0_INTERNAL int
m0_ha_entrypoint_server_init(struct m0_ha_entrypoint_server     *hes,
                             struct m0_ha_entrypoint_server_cfg *hes_cfg);
M0_INTERNAL void
m0_ha_entrypoint_server_fini(struct m0_ha_entrypoint_server *hes);
/* asynchronous */
M0_INTERNAL void
m0_ha_entrypoint_server_start(struct m0_ha_entrypoint_server *hes);
/* synchronous */
M0_INTERNAL void
m0_ha_entrypoint_server_stop(struct m0_ha_entrypoint_server *hes);
M0_INTERNAL void
m0_ha_entrypoint_server_reply(struct m0_ha_entrypoint_server    *hes,
                              const struct m0_uint128           *req_id,
                              const struct m0_ha_entrypoint_rep *rep);
M0_INTERNAL const struct m0_ha_entrypoint_req *
m0_ha_entrypoint_server_request_find(struct m0_ha_entrypoint_server *hes,
                                     const struct m0_uint128        *req_id);


M0_INTERNAL int
m0_ha_entrypoint_client_init(struct m0_ha_entrypoint_client     *ecl,
			     const char                         *ep,
                             struct m0_ha_entrypoint_client_cfg *ecl_cfg);
M0_INTERNAL void
m0_ha_entrypoint_client_fini(struct m0_ha_entrypoint_client *ecl);
M0_INTERNAL void
m0_ha_entrypoint_client_start(struct m0_ha_entrypoint_client *ecl);
M0_INTERNAL void
m0_ha_entrypoint_client_start_sync(struct m0_ha_entrypoint_client *ecl);
M0_INTERNAL void
m0_ha_entrypoint_client_stop(struct m0_ha_entrypoint_client *ecl);
M0_INTERNAL void
m0_ha_entrypoint_client_request(struct m0_ha_entrypoint_client *ecl);
M0_INTERNAL struct m0_chan *
m0_ha_entrypoint_client_chan(struct m0_ha_entrypoint_client *ecl);

/**
 * @pre m0_sm_group_is_locked(&ecl->ecl_sm_group)
 *
 * @note Use m0_sm_state_name() to convert m0_ha_entrypoint_client_state
 *       to string.
 * @code
 *         state = m0_ha_entrypoint_client_state_get(ecl);
 *         M0_LOG(M0_DEBUG, "state=%s", m0_sm_state_name(&ecl->ecl_sm, state));
 * @endcode
 */
M0_INTERNAL enum m0_ha_entrypoint_client_state
m0_ha_entrypoint_client_state_get(struct m0_ha_entrypoint_client *ecl);

M0_INTERNAL int  m0_ha_entrypoint_mod_init(void);
M0_INTERNAL void m0_ha_entrypoint_mod_fini(void);

extern const struct m0_fom_type_ops m0_ha_entrypoint_fom_type_ops;
extern struct m0_reqh_service_type m0_ha_entrypoint_service_type;
extern struct m0_sm_conf m0_ha_entrypoint_server_fom_states_conf;

/** @} end of ha group */
#endif /* __MERO_HA_ENTRYPOINT_H__ */

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
