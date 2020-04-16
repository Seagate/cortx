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
 * Original author: Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 05/08/2011
 */

#include <stdio.h>     /* fprintf */
#include <sys/stat.h>  /* mkdir */
#include <unistd.h>    /* daemon */
#include <err.h>

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0D
#include "lib/trace.h"

#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/finject.h"    /* M0_FI_ENABLED */
#include "lib/string.h"     /* m0_strdup, m0_streq */
#include "lib/getopts.h"
#include "lib/memory.h"
#include "lib/misc.h"
#include "lib/locality.h"
#include "lib/uuid.h"       /* m0_uuid_generate */
#include "lib/fs.h"         /* m0_file_read */
#include "lib/thread.h"     /* m0_process */
#include "fid/fid.h"
#include "stob/ad.h"
#include "net/net.h"
#include "net/lnet/lnet.h"
#include "rpc/rpc.h"
#include "reqh/reqh.h"
#include "cob/cob.h"
#include "mdstore/mdstore.h"
#include "mero/setup.h"
#include "mero/setup_dix.h"     /* m0_cs_dix_setup */
#include "mero/setup_internal.h"
#include "mero/magic.h"
#include "mero/version.h"
#include "rpc/rpclib.h"
#include "rpc/rpc_internal.h"
#include "addb2/storage.h"
#include "addb2/net.h"
#include "module/instance.h"	/* m0_get */
#include "conf/obj.h"           /* M0_CONF_PROCESS_TYPE */
#include "conf/helpers.h"       /* m0_confc_args */
#include "conf/obj_ops.h"  	/* M0_CONF_DIRNEXT */
#include "be/ut/helper.h"
#include "ioservice/fid_convert.h" /* M0_AD_STOB_LINUX_DOM_KEY */
#include "ioservice/storage_dev.h"
#include "ioservice/io_service.h"  /* m0_ios_net_buffer_pool_size_set */
#include "stob/linux.h"
#include "conf/ha.h"            /* m0_conf_ha_process_event_post */

/**
   @addtogroup m0d
   @{
 */

extern struct m0_reqh_service_type m0_ss_svc_type;

/**
 * The space for M0_BAP_REPAIR zone in BE allocator is calculated based on
 * distributed index replication factor and total number of target disks. But
 * due to fragmentation or some other reasons it may be not sufficient, so
 * special "safety" coefficient is introduced to increase space in repair zone.
 * Safety coefficient is defined as "safety mul"/"safety div".
 */
enum {
	/** Multiplier of a repair zone safety coefficient. */
	M0_BC_REPAIR_ZONE_SAFETY_MUL = 3,
	/** Divider of a repair zone safety coefficient. */
	M0_BC_REPAIR_ZONE_SAFETY_DIV = 2
};

M0_TL_DESCR_DEFINE(cs_buffer_pools, "buffer pools in the mero context",
		   static, struct cs_buffer_pool, cs_bp_linkage, cs_bp_magic,
		   M0_CS_BUFFER_POOL_MAGIC, M0_CS_BUFFER_POOL_HEAD_MAGIC);
M0_TL_DEFINE(cs_buffer_pools, static, struct cs_buffer_pool);

M0_TL_DESCR_DEFINE(cs_eps, "cs endpoints", , struct cs_endpoint_and_xprt,
		   ex_linkage, ex_magix, M0_CS_ENDPOINT_AND_XPRT_MAGIC,
		   M0_CS_EPS_HEAD_MAGIC);

M0_TL_DEFINE(cs_eps, M0_INTERNAL, struct cs_endpoint_and_xprt);

static struct m0_bob_type cs_eps_bob;
M0_BOB_DEFINE(extern, &cs_eps_bob, cs_endpoint_and_xprt);

M0_INTERNAL const char *m0_cs_stypes[M0_STOB_TYPE_NR] = {
	[M0_LINUX_STOB] = "Linux",
	[M0_AD_STOB]    = "AD"
};

static bool reqh_context_check(const void *bob);

static struct m0_bob_type rhctx_bob = {
	.bt_name         = "m0_reqh_context",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_reqh_context, rc_magix),
	.bt_magix        = M0_CS_REQH_CTX_MAGIC,
	.bt_check        = reqh_context_check
};
M0_BOB_DEFINE(static, &rhctx_bob, m0_reqh_context);

M0_TL_DESCR_DEFINE(ndom, "network domains", static, struct m0_net_domain,
		   nd_app_linkage, nd_magix, M0_NET_DOMAIN_MAGIC,
		   M0_CS_NET_DOMAIN_HEAD_MAGIC);

M0_TL_DEFINE(ndom, static, struct m0_net_domain);

static struct m0_bob_type ndom_bob;
M0_BOB_DEFINE(static, &ndom_bob, m0_net_domain);

static const struct m0_bob_type cs_bob_type = {
	.bt_name         = "m0_mero",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_mero, cc_magic),
	.bt_magix        = M0_CS_MERO_MAGIC,
};
/* "inline" to silence the warning about unused m0_mero_bob_check() */
M0_BOB_DEFINE(static inline, &cs_bob_type, m0_mero);

static bool reqh_ctx_services_are_valid(const struct m0_reqh_context *rctx)
{
	struct m0_mero *cctx = container_of(rctx, struct m0_mero, cc_reqh_ctx);

	return _0C(ergo(rctx->rc_services[M0_CST_CONFD] != NULL &&
			m0_streq(rctx->rc_services[M0_CST_CONFD],
				 "M0_CST_CONFD"),
			rctx->rc_confdb != NULL && *rctx->rc_confdb != '\0')) &&
	       _0C(cctx->cc_no_conf ||
		   ergo(rctx->rc_services[M0_CST_CONFD] == NULL,
			cctx->cc_ha_addr != NULL &&
			*cctx->cc_ha_addr != '\0')) &&
	       _0C(ergo(rctx->rc_nr_services != 0, rctx->rc_services != NULL &&
			!cs_eps_tlist_is_empty(&rctx->rc_eps)));
}

static bool reqh_context_check(const void *bob)
{
	const struct m0_reqh_context *rctx = bob;
	return
		_0C(M0_IN(rctx->rc_state, (RC_UNINITIALISED,
					   RC_REQH_INITIALISED,
					   RC_INITIALISED))) &&
		_0C(M0_CHECK_EX(m0_tlist_invariant(&cs_eps_tl,
						   &rctx->rc_eps))) &&
		_0C(rctx->rc_mero != NULL) &&
		_0C(ergo(rctx->rc_state == RC_INITIALISED,
			 m0_reqh_invariant(&rctx->rc_reqh)));
}

static bool reqh_context_invariant(const struct m0_reqh_context *rctx)
{
	return m0_reqh_context_bob_check(rctx); /* calls reqh_context_check() */
}

static struct m0_reqh *mero2reqh(struct m0_mero *mero)
{
  	return &mero->cc_reqh_ctx.rc_reqh;
}

static struct m0_rconfc *mero2rconfc(struct m0_mero *mero)
{
	return &mero2reqh(mero)->rh_rconfc;
}

M0_INTERNAL struct m0_confc *m0_mero2confc(struct m0_mero *mero)
{
	return &mero2rconfc(mero)->rc_confc;
}

M0_INTERNAL struct m0_rpc_machine *m0_mero_to_rmach(struct m0_mero *mero)
{
	return m0_reqh_rpc_mach_tlist_head(&mero2reqh(mero)->rh_rpc_machines);
}

/**
   Looks up an xprt by the name.

   @param xprt_name Network transport name
   @param xprts Array of network transports supported in a mero environment
   @param xprts_nr Size of xprts array

   @pre xprt_name != NULL && xprts != NULL && xprts_nr > 0

 */
static struct m0_net_xprt *cs_xprt_lookup(const char *xprt_name,
					  struct m0_net_xprt **xprts,
					  size_t xprts_nr)
{
	size_t i;

	M0_PRE(xprt_name != NULL && xprts != NULL && xprts_nr > 0);

	for (i = 0; i < xprts_nr; ++i)
		if (m0_streq(xprt_name, xprts[i]->nx_name))
			return xprts[i];
	return NULL;
}

/** Lists supported network transports. */
static void cs_xprts_list(FILE *out, struct m0_net_xprt **xprts,
			  size_t xprts_nr)
{
	int i;

	M0_PRE(out != NULL && xprts != NULL);

	fprintf(out, "\nSupported transports:\n");
	for (i = 0; i < xprts_nr; ++i)
		fprintf(out, " %s\n", xprts[i]->nx_name);
}

/** Lists supported stob types. */
static void cs_stob_types_list(FILE *out)
{
	int i;

	M0_PRE(out != NULL);

	fprintf(out, "\nSupported stob types:\n");
	for (i = 0; i < ARRAY_SIZE(m0_cs_stypes); ++i)
		fprintf(out, " %s\n", m0_cs_stypes[i]);
}

/** Checks if the specified storage type is supported in a mero context. */
static bool stype_is_valid(const char *stype)
{
	M0_PRE(stype != NULL);

	return  m0_strcaseeq(stype, m0_cs_stypes[M0_AD_STOB]) ||
		m0_strcaseeq(stype, m0_cs_stypes[M0_LINUX_STOB]);
}

/**
   Checks if given network transport and network endpoint address are already
   in use in a request handler context.
 */
static bool cs_endpoint_is_duplicate(const struct m0_reqh_context *rctx,
				     const struct m0_net_xprt *xprt,
				     const char *ep)
{
	static int (*cmp[])(const char *s1, const char *s2) = {
		strcmp,
		m0_net_lnet_ep_addr_net_cmp
	};
	struct cs_endpoint_and_xprt *ex;
	bool                         seen = false;

	M0_PRE(reqh_context_invariant(rctx) && ep != NULL);

	m0_tl_for(cs_eps, &rctx->rc_eps, ex) {
		M0_ASSERT(cs_endpoint_and_xprt_bob_check(ex));
		if (cmp[!!m0_streq(xprt->nx_name, "lnet")](ex->ex_endpoint,
							   ep) == 0 &&
		    m0_streq(ex->ex_xprt, xprt->nx_name)) {
			if (seen)
				return true;
			else
				seen = true;
		}
	} m0_tl_endfor;

	return false;
}

/**
   Checks if given network endpoint address and network transport are valid
   and if they are already in use in given mero context.

   @param cctx Mero context
   @param ep Network endpoint address
   @param xprt_name Network transport name

   @pre cctx != NULL && ep != NULL && xprt_name != NULL

   @retval 0 On success
	-EINVAL If endpoint is invalid
	-EADDRINUSE If endpoint is already in use
*/
static int cs_endpoint_validate(struct m0_mero *cctx, const char *ep,
				const char *xprt_name)
{
	struct m0_net_xprt *xprt;

	M0_ENTRY();
	M0_PRE(cctx != NULL);

	if (ep == NULL || xprt_name == NULL)
		return M0_RC(-EINVAL);

	xprt = cs_xprt_lookup(xprt_name, cctx->cc_xprts, cctx->cc_xprts_nr);
	if (xprt == NULL)
		return M0_RC(-EINVAL);

	return M0_RC(cs_endpoint_is_duplicate(&cctx->cc_reqh_ctx, xprt, ep) ?
		     -EADDRINUSE : 0);
}

M0_INTERNAL int m0_ep_and_xprt_extract(struct cs_endpoint_and_xprt *epx,
				       const char *ep)
{
	char *sptr;
	char *endpoint;
	int   ep_len;

	M0_PRE(ep != NULL);

	epx->ex_cep = ep;
	ep_len = min32u(strlen(ep) + 1, CS_MAX_EP_ADDR_LEN);
	M0_ALLOC_ARR(epx->ex_scrbuf, ep_len);
	if (epx->ex_scrbuf == NULL)
		return M0_ERR(-ENOMEM);

	strncpy(epx->ex_scrbuf, ep, ep_len);
	epx->ex_scrbuf[ep_len - 1] = '\0';
	epx->ex_xprt = strtok_r(epx->ex_scrbuf, ":", &sptr);
	if (epx->ex_xprt == NULL)
		goto err;

	endpoint = strtok_r(NULL, "\0", &sptr);
	if (endpoint == NULL)
		goto err;

	epx->ex_endpoint = endpoint;
	cs_endpoint_and_xprt_bob_init(epx);
	cs_eps_tlink_init(epx);
	return 0;

err:
	m0_free(epx->ex_scrbuf);
	return M0_ERR(-EINVAL);
}

M0_INTERNAL void m0_ep_and_xprt_fini(struct cs_endpoint_and_xprt *epx)
{
	M0_PRE(cs_endpoint_and_xprt_bob_check(epx));
	M0_PRE(epx->ex_scrbuf != NULL);
	m0_free(epx->ex_scrbuf);
	cs_eps_tlink_fini(epx);
	cs_endpoint_and_xprt_bob_fini(epx);
}

/**
   Extracts network transport name and network endpoint address from given
   mero endpoint.
   Mero endpoint is of 2 parts network xprt:network endpoint.
 */
static int ep_and_xprt_append(struct m0_tl *head, const char *ep)
{
	struct cs_endpoint_and_xprt *epx;
	int                          rc;
	M0_PRE(ep != NULL);

	M0_ALLOC_PTR(epx);
	if (epx == NULL) {
		M0_LOG(M0_ERROR, "malloc failed");
		return M0_ERR(-ENOMEM);
	}

	rc = m0_ep_and_xprt_extract(epx, ep);
	if (rc != 0)
		goto err;

	cs_eps_tlist_add_tail(head, epx);
	return 0;
err:
	m0_free(epx);
	return M0_ERR(-EINVAL);
}

/**
   Checks if specified service has already a duplicate entry in given request
   handler context.
 */
static bool service_is_duplicate(const struct m0_reqh_context *rctx,
				 const char *sname)
{
	int n;
	int i;

	M0_PRE(reqh_context_invariant(rctx));

	for (i = 0, n = 0; i < rctx->rc_nr_services; ++i) {
		if (m0_strcaseeq(rctx->rc_services[i], sname))
			++n;
		if (n > 1)
			return true;
	}
	return false;
}

static int cs_reqh_ctx_init(struct m0_mero *cctx)
{
	struct m0_reqh_context *rctx = &cctx->cc_reqh_ctx;

	M0_ENTRY();

	*rctx = (struct m0_reqh_context) {
		.rc_mero = cctx
	};
	M0_ALLOC_ARR(rctx->rc_services,     M0_CST_NR);
	M0_ALLOC_ARR(rctx->rc_service_fids, M0_CST_NR);
	if (rctx->rc_services == NULL || rctx->rc_service_fids == NULL) {
		m0_free(rctx->rc_services);
		m0_free(rctx->rc_service_fids);
		return M0_ERR(-ENOMEM);
	}

	cs_eps_tlist_init(&rctx->rc_eps);
	m0_reqh_context_bob_init(rctx);

	rctx->rc_stob.s_sfile.sf_is_initialised = false;
	rctx->rc_stob.s_ad_disks_init = false;
	return M0_RC(0);
}

static void cs_reqh_ctx_fini(struct m0_reqh_context *rctx)
{
	struct cs_endpoint_and_xprt *ep;
	int                          i;

	m0_reqh_context_bob_fini(rctx);

	m0_tl_teardown(cs_eps, &rctx->rc_eps, ep) {
		m0_ep_and_xprt_fini(ep);
		m0_free(ep);
	};
	cs_eps_tlist_fini(&rctx->rc_eps);

	for (i = 0; i < M0_CST_NR; ++i)
		m0_free(rctx->rc_services[i]);
	m0_free(rctx->rc_services);
	m0_free(rctx->rc_service_fids);
	rctx->rc_stob.s_sfile.sf_is_initialised = false;
	rctx->rc_stob.s_ad_disks_init = false;
}

M0_INTERNAL struct m0_net_domain *
m0_cs_net_domain_locate(struct m0_mero *cctx, const char *xprt_name)
{
	struct m0_net_domain *ndom;

	M0_PRE(cctx != NULL && xprt_name != NULL);

	ndom = m0_tl_find(ndom, ndom, &cctx->cc_ndoms,
			  m0_streq(ndom->nd_xprt->nx_name, xprt_name));

	M0_ASSERT(ergo(ndom != NULL, m0_net_domain_bob_check(ndom)));

	return ndom;
}

static struct m0_net_buffer_pool *
cs_buffer_pool_get(struct m0_mero *cctx, struct m0_net_domain *ndom)
{
	struct cs_buffer_pool *cs_bp;

	M0_PRE(cctx != NULL);
	M0_PRE(ndom != NULL);

	cs_bp = m0_tl_find(cs_buffer_pools, cs_bp, &cctx->cc_buffer_pools,
			   cs_bp->cs_buffer_pool.nbp_ndom == ndom);
	return cs_bp == NULL ? NULL : &cs_bp->cs_buffer_pool;
}

/**
   Initialises rpc machine for the given endpoint address.
   Once the new rpc_machine is created it is added to list of rpc machines
   in given request handler.
   Request handler should be initialised before invoking this function.

   @param cctx Mero context
   @param xprt_name Network transport
   @param ep Network endpoint address
   @param tm_colour Unique colour to be assigned to each TM in a domain
   @param recv_queue_min_length Minimum number of buffers in TM receive queue
   @param max_rpc_msg_size Maximum RPC message size
   @param reqh Request handler to which the newly created
		rpc_machine belongs

   @pre cctx != NULL && xprt_name != NULL && ep != NULL && reqh != NULL
 */
static int cs_rpc_machine_init(struct m0_mero *cctx, const char *xprt_name,
			       const char *ep, const uint32_t tm_colour,
			       const uint32_t recv_queue_min_length,
			       const uint32_t max_rpc_msg_size,
			       struct m0_reqh *reqh)
{
	struct m0_rpc_machine        *rpcmach;
	struct m0_net_domain         *ndom;
	struct m0_net_buffer_pool    *buffer_pool;
	int                           rc;

	M0_PRE(cctx != NULL && xprt_name != NULL && ep != NULL && reqh != NULL);

	ndom = m0_cs_net_domain_locate(cctx, xprt_name);
	if (ndom == NULL)
		return M0_ERR(-EINVAL);
	if (max_rpc_msg_size > m0_net_domain_get_max_buffer_size(ndom))
		return M0_ERR(-EINVAL);

	M0_ALLOC_PTR(rpcmach);
	if (rpcmach == NULL)
		return M0_ERR(-ENOMEM);

	buffer_pool = cs_buffer_pool_get(cctx, ndom);
	rc = m0_rpc_machine_init(rpcmach, ndom, ep,
				 reqh, buffer_pool, tm_colour, max_rpc_msg_size,
				 recv_queue_min_length);
	if (rc != 0)
		m0_free(rpcmach);
	return M0_RC(rc);
}

static int cs_rpc_machines_init(struct m0_mero *cctx)
{
	struct m0_reqh_context      *rctx = &cctx->cc_reqh_ctx;
	struct cs_endpoint_and_xprt *ep;

	M0_ENTRY();
	M0_PRE(reqh_context_invariant(rctx));

	m0_tl_for(cs_eps, &rctx->rc_eps, ep) {
		int rc;

		M0_ASSERT(cs_endpoint_and_xprt_bob_check(ep));
		rc = cs_rpc_machine_init(cctx, ep->ex_xprt,
					 ep->ex_endpoint, ep->ex_tm_colour,
					 rctx->rc_recv_queue_min_length,
					 rctx->rc_max_rpc_msg_size,
					 &rctx->rc_reqh);
		if (rc != 0)
			return M0_RC(rc);
	} m0_tl_endfor;

	return M0_RC(0);
}

static void cs_rpc_machines_fini(struct m0_reqh *reqh)
{
	struct m0_rpc_machine *rpcmach;

	m0_tl_for(m0_reqh_rpc_mach, &reqh->rh_rpc_machines, rpcmach) {
		M0_ASSERT(m0_rpc_machine_bob_check(rpcmach));
		m0_rpc_machine_fini(rpcmach);
		m0_free(rpcmach);
	} m0_tl_endfor;
}

/**
 * Establishes rpc session to HA service. The session is set up to be used
 * globally across all mero modules.
 */
static int cs_ha_init(struct m0_mero *cctx)
{
	struct m0_mero_ha_cfg  mero_ha_cfg;
	const char            *ep;
	int                    rc;

	M0_ENTRY();
	if (cctx->cc_ha_addr == NULL && cctx->cc_reqh_ctx.rc_confdb != NULL) {
		ep = cs_eps_tlist_head(&cctx->cc_reqh_ctx.rc_eps)->ex_endpoint;
		cctx->cc_ha_addr = m0_strdup(ep);
		cctx->cc_no_all2all_connections = true;
	}

	mero_ha_cfg = (struct m0_mero_ha_cfg){
		.mhc_dispatcher_cfg = {
			.hdc_enable_note      = true,
			.hdc_enable_keepalive = true,
			.hdc_enable_fvec      = true,
		},
		.mhc_addr           = cctx->cc_ha_addr,
		.mhc_rpc_machine    = m0_mero_to_rmach(cctx),
		.mhc_reqh           = &cctx->cc_reqh_ctx.rc_reqh,
		.mhc_process_fid    = cctx->cc_reqh_ctx.rc_fid,
	};
	rc = m0_mero_ha_init(&cctx->cc_mero_ha, &mero_ha_cfg);
	M0_ASSERT(rc == 0);
	rc = m0_mero_ha_start(&cctx->cc_mero_ha);
	M0_ASSERT(rc == 0);
	cctx->cc_ha_is_started = true;
	return 0;
}

static bool bad_address(char *addr)
{
	return addr == NULL || *addr == '\0';
}

static void cs_ha_process_event(struct m0_mero                *cctx,
                                enum m0_conf_ha_process_event  event)
{
	enum m0_conf_ha_process_type type;

	type = cctx->cc_mkfs ? M0_CONF_HA_PROCESS_M0MKFS :
			       M0_CONF_HA_PROCESS_M0D;
	if (cctx->cc_ha_is_started && !cctx->cc_no_conf &&
	    cctx->cc_mero_ha.mh_link != NULL) {
		m0_conf_ha_process_event_post(&cctx->cc_mero_ha.mh_ha,
		                              cctx->cc_mero_ha.mh_link,
		                              &cctx->cc_reqh_ctx.rc_fid,
		                              m0_process(), event, type);
	}
}

/**
 * Clears global HA session info and terminates rpc session to HA service.
 */
static void cs_ha_stop(struct m0_mero *cctx)
{
	M0_ENTRY("client_ctx: %p", cctx);
	if (!cctx->cc_ha_is_started)
		return;
	cs_ha_process_event(cctx, M0_CONF_HA_PROCESS_STOPPED);
	cctx->cc_ha_is_started = false;
	if (bad_address(cctx->cc_ha_addr)) {
		M0_LOG(M0_ERROR, "invalid HA address");
		goto leave; /* session had no chance to be established */
	}
	if (!cctx->cc_no_conf)
		m0_mero_ha_disconnect(&cctx->cc_mero_ha);
	m0_mero_ha_stop(&cctx->cc_mero_ha);
leave:
	M0_LEAVE();
}

static void cs_ha_fini(struct m0_mero *cctx)
{
        M0_ENTRY("client_ctx: %p", cctx);
        m0_mero_ha_fini(&cctx->cc_mero_ha);
        M0_LEAVE();
}

static uint32_t
cs_domain_tms_nr(struct m0_reqh_context *rctx, struct m0_net_domain *dom)
{
	struct cs_endpoint_and_xprt *ep;
	uint32_t                     n = 0;

	m0_tl_for(cs_eps, &rctx->rc_eps, ep) {
		if (m0_streq(ep->ex_xprt, dom->nd_xprt->nx_name))
			ep->ex_tm_colour = n++;
	} m0_tl_endfor;

	M0_POST(n > 0);
	return n;
}

static uint32_t cs_dom_tm_min_recv_queue_total(struct m0_reqh_context *rctx,
					       struct m0_net_domain *dom)
{
	struct cs_endpoint_and_xprt *ep;
	uint32_t                     result = 0;

	M0_PRE(reqh_context_invariant(rctx));

	m0_tl_for(cs_eps, &rctx->rc_eps, ep) {
		if (m0_streq(ep->ex_xprt, dom->nd_xprt->nx_name))
			result += rctx->rc_recv_queue_min_length;
	} m0_tl_endfor;
	return result;
}

static void cs_buffer_pool_fini(struct m0_mero *cctx)
{
	struct cs_buffer_pool   *cs_bp;

	M0_PRE(cctx != NULL);

	m0_tl_for(cs_buffer_pools, &cctx->cc_buffer_pools, cs_bp) {
		cs_buffer_pools_tlink_del_fini(cs_bp);
		m0_net_buffer_pool_fini(&cs_bp->cs_buffer_pool);
		m0_free(cs_bp);
	} m0_tl_endfor;
}

static int cs_buffer_pool_setup(struct m0_mero *cctx)
{
	struct m0_reqh_context *rctx = &cctx->cc_reqh_ctx;
	struct m0_net_domain   *dom;
	struct cs_buffer_pool  *bp;
	uint32_t                tms_nr;
	uint32_t                max_recv_queue_len;
	int                     rc = 0;

	m0_tl_for(ndom, &cctx->cc_ndoms, dom) {
		max_recv_queue_len = cs_dom_tm_min_recv_queue_total(rctx, dom);
		tms_nr = cs_domain_tms_nr(rctx, dom);
		M0_ASSERT(max_recv_queue_len >= tms_nr);

		M0_ALLOC_PTR(bp);
		if (bp == NULL) {
			rc = -ENOMEM;
			break;
		}
		rc = m0_rpc_net_buffer_pool_setup(
			dom, &bp->cs_buffer_pool,
			m0_rpc_bufs_nr(max_recv_queue_len, tms_nr),
			tms_nr);
		if (rc != 0) {
			m0_free(bp);
			break;
		}
		cs_buffer_pools_tlink_init_at_tail(bp, &cctx->cc_buffer_pools);
	} m0_tl_endfor;

	if (rc != 0)
		cs_buffer_pool_fini(cctx);
	return M0_RC(rc);
}

static int stob_file_id_get(yaml_document_t *doc, yaml_node_t *node,
			    uint64_t *id)
{
	char             *endptr;
	const char       *key_str;
	yaml_node_pair_t *pair;

	for (pair = node->data.mapping.pairs.start;
	     pair < node->data.mapping.pairs.top; ++pair) {
		key_str = (const char *)yaml_document_get_node(doc,
					pair->key)->data.scalar.value;
		if (m0_strcaseeq(key_str, "id")) {
			*id = strtoll((const char *)yaml_document_get_node(doc,
				      pair->value)->data.scalar.value, &endptr,
				      10);
			return *endptr == '\0' ? M0_RC(0) : M0_ERR(-EINVAL);
		}
	}
	return M0_RC(-ENOENT);
}

static const char *stob_file_path_get(yaml_document_t *doc, yaml_node_t *node)
{
	const char       *key_str;
	yaml_node_pair_t *pair;

	for (pair = node->data.mapping.pairs.start;
	     pair < node->data.mapping.pairs.top; ++pair) {
		key_str = (const char *)yaml_document_get_node(doc,
					pair->key)->data.scalar.value;
		if (m0_strcaseeq(key_str, "filename"))
			return (const char *)yaml_document_get_node(doc,
					     pair->value)->data.scalar.value;
	}

	return NULL;
}

static int cs_stob_file_load(const char *dfile, struct cs_stobs *stob)
{
	FILE            *f;
	yaml_parser_t    parser;
	yaml_document_t *document;
	int              rc = -EINVAL;

	f = fopen(dfile, "r");
	if (f == NULL)
		return M0_ERR(-EINVAL);

	document = &stob->s_sfile.sf_document;
	rc = yaml_parser_initialize(&parser);
	if (rc != 1)
		goto end;

	yaml_parser_set_input_file(&parser, f);
	rc = yaml_parser_load(&parser, document);
	if (rc != 1)
		goto end;

	stob->s_sfile.sf_is_initialised = true;
	yaml_parser_delete(&parser);
	rc = 0;
end:
	fclose(f);
	return M0_RC(rc);
}

static void cs_storage_devs_fini(void)
{
	struct m0_storage_devs *devs = &m0_get()->i_storage_devs;

	m0_storage_devs_lock(devs);
	m0_storage_devs_detach_all(devs);
	m0_storage_devs_unlock(devs);
	m0_storage_devs_fini(devs);
}

/**
 * Initialise storage devices used by IO service.
 */
static int cs_storage_devs_init(struct cs_stobs          *stob,
				enum m0_storage_dev_type  type,
				struct m0_be_seg         *seg,
				const char               *stob_path,
				bool                      force,
				bool                      disable_direct_io)
{
	int                     rc;
	int                     result;
	uint64_t                cid;
	uint64_t                stob_file_id;
	const char             *f_path;
	struct m0_storage_devs *devs = &m0_get()->i_storage_devs;
	struct m0_storage_dev  *dev;
	yaml_document_t        *doc;
	yaml_node_t            *node;
	yaml_node_t            *s_node;
	yaml_node_item_t       *item;
	m0_bcount_t             size = 0; /* Uses BALLOC_DEF_CONTAINER_SIZE; */

	struct m0_mero         *cctx;
	struct m0_reqh_context *rctx;
	struct m0_confc        *confc;
	struct m0_reqh         *reqh;
	struct m0_fid           sdev_fid;
	struct m0_conf_sdev    *conf_sdev;

	M0_ENTRY();
	M0_PRE(ergo(type == M0_STORAGE_DEV_TYPE_AD, stob->s_sdom != NULL));

	rctx = container_of(stob, struct m0_reqh_context, rc_stob);
	reqh = &rctx->rc_reqh;
	cctx = container_of(rctx, struct m0_mero, cc_reqh_ctx);
	confc = m0_mero2confc(cctx);
	rc = m0_storage_devs_init(devs, type, seg, stob->s_sdom, reqh);
	if (rc != 0)
		return M0_ERR(rc);
	m0_storage_devs_use_directio(devs, !disable_direct_io);

	if (stob->s_sfile.sf_is_initialised) {
		M0_LOG(M0_DEBUG, "yaml config");
		doc = &stob->s_sfile.sf_document;
		for (node = doc->nodes.start; node < doc->nodes.top; ++node) {
			for (item = (node)->data.sequence.items.start;
			     item < (node)->data.sequence.items.top; ++item) {
				s_node = yaml_document_get_node(doc, *item);
				result = stob_file_id_get(doc, s_node,
							  &stob_file_id);
				if (result != 0)
					continue;
				M0_ASSERT(stob_file_id > 0);
				cid = stob_file_id - 1;
				f_path = stob_file_path_get(doc, s_node);
				rc = m0_conf_device_cid_to_fid(confc, cid,
							       &sdev_fid);
				if (rc == 0) {
					rc = m0_conf_sdev_get(confc, &sdev_fid,
						              &conf_sdev);
					if (rc != 0) {
						M0_LOG(M0_ERROR,
						       "Cannot open sdev "FID_F,
						       FID_P(&sdev_fid));
						break;
					}
					M0_LOG(M0_DEBUG, "cid:0x%"PRIx64
					       " -> sdev_fid:"FID_F" idx:0x%x",
					       cid, FID_P(&sdev_fid),
					       conf_sdev->sd_dev_idx);
				} else
					/* Every storage device need not have a
					 * counterpart in configuration. */
					conf_sdev = NULL;
				if (M0_FI_ENABLED("spare_reserve"))
					/*
					 * We intend to fill disks faster, hence
					 * sizes reduced to 256 MB.
					 */
					size = 1024ULL *1024 * 256;
				rc = m0_storage_dev_new(devs, cid, f_path, size,
							conf_sdev, force, &dev);
				if (rc == 0) {
					/*
					 * There is no concurrent access to the
					 * devs on this stage. But we lock the
					 * structure to meet the precondition
					 * of m0_storage_dev_attach().
					 */
					m0_storage_devs_lock(devs);
					m0_storage_dev_attach(dev, devs);
					m0_storage_devs_unlock(devs);
				}
				if (conf_sdev != NULL)
					m0_confc_close(&conf_sdev->sd_obj);
				if (rc != 0) {
					M0_LOG(M0_ERROR, "Storage device failed"
					       " cid=%"PRIu64, cid);
					break;
				}
			}
		}
	} else if (stob->s_ad_disks_init || M0_FI_ENABLED("init_via_conf")) {
		M0_LOG(M0_DEBUG, "conf config");
		rc = cs_conf_storage_init(stob, devs, force);
	} else {
		/*
		 * This is special case for tests. We don't have configured
		 * storages. Therefore, create a default one.
		 * Unit tests with m0_rpc_server_start() and sss/st don't
		 * configure storages properly via conf.xc or YAML file and
		 * rely on these storages with specific IDs. Such tests should
		 * provide valid conf.xc or YAML file instead of using default
		 * storages with hardcoded IDs.
		 */
		M0_LOG(M0_DEBUG, "creating default storage");
		if (type == M0_STORAGE_DEV_TYPE_AD)
			rc = m0_storage_dev_new(devs,
						M0_AD_STOB_DOM_KEY_DEFAULT,
						NULL, size, NULL, force, &dev);
		else
			rc = m0_storage_dev_new(devs, M0_SDEV_CID_DEFAULT,
						stob_path, size, NULL,
						force, &dev);
		if (rc == 0) {
			/* see m0_storage_dev_attach() above */
			m0_storage_devs_lock(devs);
			m0_storage_dev_attach(dev, devs);
			m0_storage_devs_unlock(devs);
		}
	}
	if (rc != 0)
		cs_storage_devs_fini();
	return M0_RC(rc);
}

/** Generates linuxstob domain location which must be freed with m0_free(). */
static char *cs_storage_ldom_location_gen(const char *stob_path)
{
	char              *location;
	static const char  prefix[] = "linuxstob:";

	M0_ALLOC_ARR(location, strlen(stob_path) + ARRAY_SIZE(prefix));
	if (location != NULL)
		sprintf(location, "%s%s", prefix, stob_path);

	return location;
}

/**
 * Destroys linuxstob domain. This function is used when mkfs runs with the
 * force option. Therefore, domain may not exist.
 */
static int cs_storage_ldom_destroy(const char *stob_path,
				   const char *str_cfg_init)
{
	struct m0_stob_domain *dom;
	char                  *location;
	int                    rc;

	M0_ENTRY();
	M0_PRE(stob_path != NULL);

	location = cs_storage_ldom_location_gen(stob_path);
	if (location == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_stob_domain_init(location, str_cfg_init, &dom);
	if (rc == 0)
		rc = m0_stob_domain_destroy(dom);
	else
		rc = m0_stob_domain_destroy_location(location);
	/* Don't fail if domain doesn't exist. */
	if (rc == -ENOENT)
		rc = 0;
	m0_free(location);
	return M0_RC(rc);
}

static int cs_storage_bstore_prepare(const char             *stob_path,
				     const char             *str_cfg_init,
				     uint64_t                dom_key,
				     bool                    mkfs,
				     struct m0_stob_domain **out)
{
	int   rc;
	int   rc1 = 0;
	char *location;

	M0_ENTRY();
	M0_PRE(stob_path != NULL);

	location = cs_storage_ldom_location_gen(stob_path);
	if (location == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_stob_domain_init(location, str_cfg_init, out);
	if (mkfs) {
		/* Found existing stob domain, kill it. */
		if (!M0_IN(rc, (0, -ENOENT))) {
			rc1 = rc == 0 ? m0_stob_domain_destroy(*out) :
					m0_stob_domain_destroy_location
						(location);
			if (rc1 != 0)
				goto out;
		}
		if (rc != 0) {
			rc = m0_stob_domain_create(location, str_cfg_init,
						   dom_key, NULL, out);
			if (rc != 0)
				M0_LOG(M0_ERROR,
				       "m0_stob_domain_create: rc=%d", rc);
		} else {
			M0_LOG(M0_INFO, "Found alive filesystem, do nothing.");
		}
	} else if (rc != 0)
		M0_LOG(M0_ERROR, "Cannot init stob domain rc=%d", rc);
out:
	m0_free(location);
	return M0_RC(rc1 == 0 ? rc : rc1);
}

/**
   Initialises storage including database environment and stob domain of given
   type (e.g. linux or ad). There is a stob domain and a database environment
   created per request handler context.

   @todo Use generic mechanism to generate stob ids
 */
static int cs_storage_init(const char *stob_type,
			   const char *stob_path,
			   uint64_t dom_key,
			   struct cs_stobs *stob,
			   struct m0_be_seg *seg,
			   bool mkfs, bool force,
			   bool disable_direct_io)
{
	const char *ldom_cfg_init;
	bool        linux_stob;
	bool        fake_storage;
	int         rc = 0;

	M0_ENTRY();
	M0_PRE(stob_type != NULL);
	M0_PRE(stob != NULL);
	M0_PRE(stob->s_sdom == NULL);
	M0_PRE(stype_is_valid(stob_type));

	/* XXX `-F` (force) doesn't work for linuxstob storage devices. */

	ldom_cfg_init = disable_direct_io ? "directio=false" : "directio=true";

	linux_stob = m0_strcaseeq(stob_type, m0_cs_stypes[M0_LINUX_STOB]);
	m0_get()->i_reqh_uses_ad_stob = !linux_stob;
	m0_get()->i_storage_is_fake   = fake_storage =
		linux_stob && stob_path != NULL && !stob->s_ad_disks_init &&
		!stob->s_sfile.sf_is_initialised;


	if (mkfs && force && (fake_storage || !linux_stob))
		rc = cs_storage_ldom_destroy(stob_path, NULL);
	if (linux_stob) {
		rc = rc ?: cs_storage_devs_init(stob, M0_STORAGE_DEV_TYPE_LINUX,
						NULL, stob_path, force,
						disable_direct_io);
	} else {
		rc = rc ?: cs_storage_bstore_prepare(stob_path, ldom_cfg_init,
						     dom_key, mkfs,
						     &stob->s_sdom);
		if (rc != 0)
			stob->s_sdom = NULL;
		rc = rc ?: cs_storage_devs_init(stob, M0_STORAGE_DEV_TYPE_AD,
						seg, stob_path, false,
						disable_direct_io);
	}
	if (rc != 0 && stob->s_sdom != NULL)
		m0_stob_domain_fini(stob->s_sdom);

	return M0_RC(rc);
}

/**
   Finalises storage for a request handler in a mero context.
 */
static void cs_storage_fini(struct cs_stobs *stob)
{
	cs_storage_devs_fini();
	if (stob->s_sdom != NULL)
		m0_stob_domain_fini(stob->s_sdom);
	if (stob->s_sfile.sf_is_initialised)
		yaml_document_delete(&stob->s_sfile.sf_document);
}

/**
   Initialises and starts a particular service.

   Once the service is initialised, it is started and registered with the
   appropriate request handler.
 */
M0_INTERNAL int cs_service_init(const char *name, struct m0_reqh_context *rctx,
				struct m0_reqh *reqh, struct m0_fid *fid)
{
	struct m0_reqh_service_type *stype;
	struct m0_reqh_service      *service;
	int                          rc = 0;

	M0_ENTRY("name=`%s'", name);
	M0_PRE(name != NULL && *name != '\0' && reqh != NULL);

	stype = m0_reqh_service_type_find(name);
	if (stype == NULL) {
		M0_LOG(M0_ERROR, "Service name %s is not found.", name);
		return M0_ERR_INFO(-EINVAL, "Unknown reqh service type: %s",
				   name);
	}

	service = m0_reqh_service_find(stype, reqh);
	if (service == NULL) {
		rc = m0_reqh_service_setup(&service, stype, reqh, rctx, fid);
		M0_POST(ergo(rc == 0, m0_reqh_service_invariant(service)));
	} else {
		M0_LOG(M0_WARN, "Service %s ("FID_F") is already registered "
		       "for reqh %p", name, FID_P(fid), reqh);
	}
	return M0_RC(rc);
}

static int reqh_context_services_init(struct m0_reqh_context *rctx,
                                      struct m0_mero         *cctx)
{
	uint32_t i;
	int      rc = 0;

	M0_ENTRY();
	M0_PRE(reqh_context_invariant(rctx));

	for (i = 0; i < M0_CST_NR && rc == 0; ++i) {
		if (rctx->rc_services[i] == NULL ||
		    M0_IN(i, (M0_CST_HA, M0_CST_SSS)))
			continue;
		if (i == M0_CST_FIS) {
			if (!rctx->rc_fis_enabled)
				/*
				 * Even in case FIS is present in conf, the
				 * service must stay down being not enabled by
				 * command line parameter '-j'.
				 */
				continue;
			else
				M0_LOG(M0_DEBUG, "FIS enabled by command opt.");
		}
		rc = cs_service_init(rctx->rc_services[i], rctx, &rctx->rc_reqh,
				     &rctx->rc_service_fids[i]);
		M0_LOG(M0_DEBUG, "service: %s" FID_F " cs_service_init: %d",
		       rctx->rc_services[i], FID_P(&rctx->rc_service_fids[i]),
		       rc);
	}
	return M0_RC(rc);
}

M0_INTERNAL void cs_service_fini(struct m0_reqh_service *service)
{
	struct m0_reqh *reqh = service->rs_reqh;
	/*
	 * XXX: The following lines are going to be refactored. Currently it's
	 * not so easy to extract proper interface from reqh/reqh.c.
	 */
	M0_ASSERT(m0_reqh_service_invariant(service));
	if (M0_IN(m0_reqh_service_state_get(service),
		  (M0_RST_STARTED, M0_RST_STOPPING))) {
		m0_reqh_service_prepare_to_stop(service);
		m0_reqh_idle_wait_for(reqh, service);
		m0_reqh_service_stop(service);
	}
	M0_LOG(M0_DEBUG, "service=%s", service->rs_type->rst_name);
	m0_reqh_service_fini(service);
}

static void reqh_context_services_fini(struct m0_reqh_context *rctx,
				       struct m0_mero         *cctx)
{
	struct m0_reqh_service *service;
	const char *name;
	uint32_t    i;
	int         rc = 0;

	M0_ENTRY();

	for (i = 0; i < M0_CST_NR && rc == 0; ++i) {
		if (rctx->rc_services[i] == NULL ||
		    M0_IN(i, (M0_CST_HA, M0_CST_SSS)))
			continue;
		name = rctx->rc_services[i];
		M0_LOG(M0_DEBUG, "service: %s" FID_F, name,
			FID_P(&rctx->rc_service_fids[i]));
		service = m0_reqh_service_lookup(&rctx->rc_reqh,
						 &rctx->rc_service_fids[i]);
		if (service != NULL)
			cs_service_fini(service);
	}

	M0_LEAVE();
}

static int reqh_services_start(struct m0_reqh_context *rctx,
			       struct m0_mero         *cctx)
{
	struct m0_reqh         *reqh = &rctx->rc_reqh;
	struct m0_reqh_service *ss_service;
	int                     rc;

	M0_ENTRY();

	/**
	 * @todo XXX Handle errors properly.
	 * See http://es-gerrit.xyus.xyratex.com:8080/#/c/2612/7..9/mero/setup.c
	 * for the discussion.
	 */
	rc = m0_reqh_service_setup(&ss_service, &m0_ss_svc_type,
				   reqh, NULL, NULL) ?:
		cs_service_init("simple-fom-service", NULL, reqh, NULL) ?:
		cs_service_init("M0_CST_FDMI", NULL, reqh, NULL) ?:
		reqh_context_services_init(rctx, cctx);

	if (rc == 0)
		m0_reqh_start(reqh);

	return M0_RC(rc);
}

static int
cs_net_domain_init(struct cs_endpoint_and_xprt *ep, struct m0_mero *cctx)
{
	struct m0_net_xprt   *xprt;
	struct m0_net_domain *ndom = NULL;
	int                   rc;

	M0_PRE(cs_endpoint_and_xprt_bob_check(ep));

	xprt = cs_xprt_lookup(ep->ex_xprt, cctx->cc_xprts, cctx->cc_xprts_nr);
	if (xprt == NULL)
		return M0_ERR(-EINVAL);

	ndom = m0_cs_net_domain_locate(cctx, ep->ex_xprt);
	if (ndom != NULL)
		return 0; /* pass */

	M0_ALLOC_PTR(ndom);
	if (ndom == NULL) {
		rc = -ENOMEM;
		goto err;
	}

	rc = m0_net_domain_init(ndom, xprt);
	if (rc != 0)
		goto err;

	m0_net_domain_bob_init(ndom);
	ndom_tlink_init_at_tail(ndom, &cctx->cc_ndoms);
	return 0;
err:
	m0_free(ndom); /* freeing NULL does not hurt */
	return M0_RC(rc);
}

/**
   Initialises network domains per given distinct xport:endpoint pair in a
   mero context.
 */
static int cs_net_domains_init(struct m0_mero *cctx)
{
	struct m0_reqh_context      *rctx = &cctx->cc_reqh_ctx;
	struct cs_endpoint_and_xprt *ep;

	M0_ENTRY();
	M0_PRE(reqh_context_invariant(rctx));

	m0_tl_for(cs_eps, &rctx->rc_eps, ep) {
		int rc = cs_net_domain_init(ep, cctx);
		if (rc != 0)
			return M0_RC(rc);
	} m0_tl_endfor;
	return M0_RC(0);
}

/**
   Finalises all the network domains within a mero context.

   @param cctx Mero context to which the network domains belong
 */
static void cs_net_domains_fini(struct m0_mero *cctx)
{
	struct m0_net_domain *ndom;

	m0_tl_for(ndom, &cctx->cc_ndoms, ndom) {
		M0_ASSERT(m0_net_domain_bob_check(ndom));
		m0_net_domain_fini(ndom);
		ndom_tlink_del_fini(ndom);
		m0_net_domain_bob_fini(ndom);
		m0_free(ndom);
	} m0_tl_endfor;
}

static int cs_storage_prepare(struct m0_reqh_context *rctx, bool erase)
{
	struct m0_sm_group   *grp   = m0_locality0_get()->lo_grp;
	struct m0_be_domain  *bedom = rctx->rc_beseg->bs_domain;
	struct m0_cob_domain *dom;
	struct m0_dtx         tx = {};
	int                   rc = 0;
	int                   rc2;

	m0_sm_group_lock(grp);

	if (erase)
		rc = m0_mdstore_destroy(&rctx->rc_mdstore, grp, bedom);

	rc = rc ?: m0_mdstore_create(&rctx->rc_mdstore, grp, &rctx->rc_cdom_id,
				     bedom, rctx->rc_beseg);
	if (rc != 0)
		goto end;
	dom = rctx->rc_mdstore.md_dom;

	m0_dtx_init(&tx, bedom, grp);
	m0_cob_tx_credit(dom, M0_COB_OP_DOMAIN_MKFS, &tx.tx_betx_cred);

	rc = m0_dtx_open_sync(&tx);
	if (rc == 0) {
		rc = m0_cob_domain_mkfs(dom, &M0_MDSERVICE_SLASH_FID, &tx.tx_betx);
		m0_dtx_done_sync(&tx);
	}
	m0_dtx_fini(&tx);

	if (rc != 0) {
		/* Note, m0_mdstore_destroy() creates exclusive tx. */
		rc2 = m0_mdstore_destroy(&rctx->rc_mdstore, grp, bedom);
		if (rc2 != 0)
			M0_LOG(M0_ERROR, "Ignoring rc2=%d from "
					 "m0_mdstore_destroy()", rc2);
	}
end:
	m0_sm_group_unlock(grp);
	return M0_RC(rc);
}

static void be_seg_init(struct m0_be_ut_backend *be,
			m0_bcount_t		 size,
			bool		 	 preallocate,
			bool		 	 format,
			const char		*stob_create_cfg,
			struct m0_be_seg       **out)
{
	struct m0_be_seg *seg;

	seg = m0_be_domain_seg_first(&be->but_dom);
	if (seg != NULL && format) {
		m0_be_ut_backend_seg_del(be, seg);
		seg = NULL;
	}
	if (seg == NULL) {
		if (size == 0)
			size = M0_BE_SEG_SIZE_DEFAULT;
		m0_be_ut_backend_seg_add2(be, size, preallocate,
					  stob_create_cfg, &seg);
	}
	*out = seg;
}

static bool pver_is_actual(const struct m0_conf_obj *obj)
{
	/**
	 * @todo XXX filter only actual pool versions till formulaic
	 *           pool version creation in place.
	 */
	return m0_conf_obj_type(obj) == &M0_CONF_PVER_TYPE &&
		M0_CONF_CAST(obj, m0_conf_pver)->pv_kind == M0_CONF_PVER_ACTUAL;
}

/**
 * Read configuration and calculate percent for M0_BAP_REPAIR zone.
 */
static int be_repair_zone_pcnt_get(struct m0_reqh *reqh,
				   uint32_t       *repair_zone_pcnt)
{
	struct m0_confc           *confc;
	struct m0_conf_root       *croot = NULL;
	struct m0_conf_diter       it;
	struct m0_conf_pver       *pver_obj;
	int                        rc;

	*repair_zone_pcnt = 0;
	rc = m0_confc_root_open(m0_reqh2confc(reqh), &croot);
	M0_LOG(M0_DEBUG, "m0_confc_root_open: rc=%d", rc);
	if (rc == 0) {
		confc = m0_confc_from_obj(&croot->rt_obj);
		rc = m0_conf_diter_init(&it, confc, &croot->rt_obj,
					M0_CONF_ROOT_POOLS_FID,
					M0_CONF_POOL_PVERS_FID);
		M0_LOG(M0_DEBUG, "m0_conf_diter_init rc %d", rc);
	}
	while (rc == 0 &&
	       m0_conf_diter_next_sync(&it, pver_is_actual) ==
	       M0_CONF_DIRNEXT) {
		pver_obj = M0_CONF_CAST(m0_conf_diter_result(&it),
					m0_conf_pver);
		if (!m0_fid_eq(&pver_obj->pv_obj.co_id,
			       &croot->rt_imeta_pver)) {
			continue;
		}
		*repair_zone_pcnt = pver_obj->pv_u.subtree.pvs_attr.pa_K *
			100 /
			pver_obj->pv_u.subtree.pvs_attr.pa_P *
			M0_BC_REPAIR_ZONE_SAFETY_MUL /
			M0_BC_REPAIR_ZONE_SAFETY_DIV;

		M0_LOG(M0_DEBUG, "pver_obj %p K %d P %d percent %d",
		       pver_obj,
		       pver_obj->pv_u.subtree.pvs_attr.pa_K,
		       pver_obj->pv_u.subtree.pvs_attr.pa_P,
		       *repair_zone_pcnt);
	}
	if (rc == 0)
		m0_conf_diter_fini(&it);
	if (croot != NULL)
		m0_confc_close(&croot->rt_obj);
	M0_LOG(M0_DEBUG, "spare percent %d", *repair_zone_pcnt);
	return rc;
}

static int cs_be_dom_cfg_zone_pcnt_fill(struct m0_reqh          *reqh,
					struct m0_be_domain_cfg *dom_cfg)
{
	uint32_t *zone_pcnt = dom_cfg->bc_zone_pcnt;
	uint32_t  repair_zone_pcnt;
	int       rc;

	rc = be_repair_zone_pcnt_get(reqh, &repair_zone_pcnt);

	if (rc == 0) {
		zone_pcnt[M0_BAP_REPAIR] = repair_zone_pcnt;
		zone_pcnt[M0_BAP_NORMAL] = 100 - zone_pcnt[M0_BAP_REPAIR];
	}

	return rc;
}

static int cs_be_init(struct m0_reqh_context *rctx,
		      struct m0_be_ut_backend *be,
		      const char              *name,
		      bool                     preallocate,
		      bool                     format,
		      struct m0_be_seg       **out)
{
	enum { len = 1024 };
	char **loc = &be->but_stob_domain_location;
	int    rc;

	*loc = m0_alloc(len);
	if (*loc == NULL)
		return M0_ERR(-ENOMEM);
	snprintf(*loc, len, "linuxstob:%s%s", name[0] == '/' ? "" : "./", name);

	m0_be_ut_backend_cfg_default(&be->but_dom_cfg);
	be->but_dom_cfg.bc_log.lc_store_cfg.lsc_stob_dont_zero = false;
	be->but_dom_cfg.bc_log.lc_store_cfg.lsc_stob_create_cfg =
		rctx->rc_be_log_path;
	be->but_dom_cfg.bc_seg0_cfg.bsc_stob_create_cfg = rctx->rc_be_seg0_path;
	if (!m0_is_po2(rctx->rc_be_log_size))
		return M0_ERR(-EINVAL);
	if (rctx->rc_be_log_size > 0) {
		be->but_dom_cfg.bc_log.lc_store_cfg.lsc_size =
			rctx->rc_be_log_size;
	}
	if (rctx->rc_be_tx_group_tx_nr_max > 0) {
		be->but_dom_cfg.bc_engine.bec_group_cfg.tgc_tx_nr_max =
			rctx->rc_be_tx_group_tx_nr_max;
	}
	if (rctx->rc_be_tx_group_tx_nr_max > 0) {
		be->but_dom_cfg.bc_engine.bec_group_cfg.tgc_tx_nr_max =
			rctx->rc_be_tx_group_tx_nr_max;
	}
	if (!equi(rctx->rc_be_tx_group_reg_nr_max > 0,
	          rctx->rc_be_tx_group_reg_size_max > 0))
		return M0_ERR(-EINVAL);
	if (rctx->rc_be_tx_group_reg_nr_max > 0 &&
	    rctx->rc_be_tx_group_reg_size_max > 0) {
		be->but_dom_cfg.bc_engine.bec_group_cfg.tgc_size_max =
			M0_BE_TX_CREDIT(rctx->rc_be_tx_group_reg_nr_max,
			                rctx->rc_be_tx_group_reg_size_max);
	}
	if (rctx->rc_be_tx_group_payload_size_max > 0) {
		be->but_dom_cfg.bc_engine.bec_group_cfg.tgc_payload_max =
			rctx->rc_be_tx_group_payload_size_max;
	}
	if (rctx->rc_be_tx_reg_nr_max > 0 &&
	    rctx->rc_be_tx_reg_size_max > 0) {
		be->but_dom_cfg.bc_engine.bec_tx_size_max =
			M0_BE_TX_CREDIT(rctx->rc_be_tx_reg_nr_max,
			                rctx->rc_be_tx_reg_size_max);
	}
	if (rctx->rc_be_tx_payload_size_max > 0) {
		be->but_dom_cfg.bc_engine.bec_tx_payload_max =
			rctx->rc_be_tx_payload_size_max;
	}
	if (rctx->rc_be_tx_group_freeze_timeout_min > 0 &&
	    rctx->rc_be_tx_group_freeze_timeout_max > 0) {
		be->but_dom_cfg.bc_engine.bec_group_freeze_timeout_min =
			rctx->rc_be_tx_group_freeze_timeout_min;
		be->but_dom_cfg.bc_engine.bec_group_freeze_timeout_max =
			rctx->rc_be_tx_group_freeze_timeout_max;
	}
	rc = cs_be_dom_cfg_zone_pcnt_fill(&rctx->rc_reqh, &be->but_dom_cfg);
	if (rc != 0)
		goto err;
	rc = m0_be_ut_backend_init_cfg(be, &be->but_dom_cfg, format);
	if (rc != 0)
		goto err;

	be_seg_init(be, rctx->rc_be_seg_size, preallocate, format,
		    rctx->rc_be_seg_path, out);
	if (*out != NULL)
		return 0;
	M0_LOG(M0_ERROR, "cs_be_init: failed to init segment");
	rc = M0_ERR(-ENOMEM);
err:
	m0_free0(loc);
	return M0_ERR(rc);
}

M0_INTERNAL void cs_be_fini(struct m0_be_ut_backend *be)
{
	m0_be_ut_backend_fini(be);
	m0_free(be->but_stob_domain_location);
}

/**
   Initialises a request handler context.
   A request handler context consists of the storage domain, database,
   cob domain, fol and request handler instance to be initialised.
   The request handler context is allocated and initialised per request handler
   in a mero process per node. So, there can exist multiple request handlers
   and thus multiple request handler contexts in a mero context.

   @param rctx Request handler context to be initialised
 */
static int cs_reqh_start(struct m0_reqh_context *rctx)
{
	int rc;

	M0_ENTRY();
	M0_PRE(reqh_context_invariant(rctx));

	rc = M0_REQH_INIT(&rctx->rc_reqh,
			  .rhia_mdstore = &rctx->rc_mdstore,
			  .rhia_pc = &rctx->rc_mero->cc_pools_common,
			  .rhia_fid = &rctx->rc_fid);
	rctx->rc_state = RC_REQH_INITIALISED;
	return M0_RC(rc);
}

static int cs_storage_setup(struct m0_mero *cctx)
{
	/**
	 * @todo Have a generic mechanism to generate unique cob domain id.
	 * Handle error messages properly.
	 */
	static int              cdom_id = M0_MDS_COB_ID_START;
	struct m0_reqh_context *rctx = &cctx->cc_reqh_ctx;
	bool                    mkfs = cctx->cc_mkfs;
	bool                    force = cctx->cc_force;
	int                     rc;

	M0_ENTRY();
	M0_PRE(reqh_context_invariant(rctx));

	if (cctx->cc_no_storage)
		return M0_RC(0);

	rctx->rc_be.but_dom_cfg.bc_engine.bec_reqh = &rctx->rc_reqh;

	rc = cs_be_init(rctx, &rctx->rc_be, rctx->rc_bepath,
			rctx->rc_be_seg_preallocate,
			(mkfs && force), &rctx->rc_beseg);
	if (rc != 0)
		return M0_ERR_INFO(rc, "cs_be_init");

	rc = m0_reqh_be_init(&rctx->rc_reqh, rctx->rc_beseg);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "m0_reqh_be_init: rc=%d", rc);
		goto be_fini;
	}

	if (!rctx->rc_stob.s_ad_disks_init && rctx->rc_dfilepath != NULL) {
		rc = cs_stob_file_load(rctx->rc_dfilepath, &rctx->rc_stob);
		if (rc != 0) {
			M0_LOG(M0_ERROR,
			       "Failed to load device configuration file");
			goto reqh_be_fini;
		}
	}

	rc = cs_storage_init(rctx->rc_stype, rctx->rc_stpath,
			     M0_AD_STOB_LINUX_DOM_KEY,
			     &rctx->rc_stob, rctx->rc_beseg,
			     mkfs, force, rctx->rc_disable_direct_io);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "cs_storage_init: rc=%d", rc);
		/* XXX who should call yaml_document_delete()? */
		goto reqh_be_fini;
	}

	rc = m0_reqh_addb2_init(&rctx->rc_reqh, rctx->rc_addb_stlocation,
				M0_ADDB2_STOB_DOM_KEY, mkfs, force);
	if (rc != 0)
		goto cleanup_stob;

	rctx->rc_cdom_id.id = ++cdom_id;

	/*
	  This MUST be initialized before m0_mdstore_init() is called.
	  Otherwise it returns -ENOENT, which is used for detecting if
	  fs is alive and should be preserved.
	 */
	rctx->rc_mdstore.md_dom = m0_reqh_lockers_get(&rctx->rc_reqh,
						      m0_get()->i_mds_cdom_key);

	if (mkfs) {
		/*
		 * Init mdstore without root cob first. Now we can use it
		 * for mkfs.
		 */
		rc = m0_mdstore_init(&rctx->rc_mdstore, rctx->rc_beseg, false);
		if (rc != 0 && rc != -ENOENT) {
			M0_LOG(M0_ERROR, "m0_mdstore_init: rc=%d", rc);
			goto cleanup_addb2;
		}

		/* Prepare new metadata structure, erase old one if exists. */
		if ((rc == 0 && force) || rc == -ENOENT)
			rc = cs_storage_prepare(rctx, (rc == 0 && force));
		m0_mdstore_fini(&rctx->rc_mdstore);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "cs_storage_prepare: rc=%d", rc);
			goto cleanup_addb2;
		}
	}

	M0_ASSERT(rctx->rc_mdstore.md_dom != NULL);
	/* Init mdstore and root cob as it should be created by mkfs. */
	rc = m0_mdstore_init(&rctx->rc_mdstore, rctx->rc_beseg, true);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Failed to initialize mdstore. %s",
		       !mkfs ? "Did you run mkfs?" : "Mkfs failed?");
		goto cleanup_addb2;
	}

	rctx->rc_state = RC_INITIALISED;
	return M0_RC(rc);

cleanup_addb2:
	m0_reqh_addb2_fini(&rctx->rc_reqh);
cleanup_stob:
	cs_storage_fini(&rctx->rc_stob);
reqh_be_fini:
	m0_reqh_be_fini(&rctx->rc_reqh);
be_fini:
	cs_be_fini(&rctx->rc_be);
	return M0_ERR(rc);
}

static void cs_reqh_shutdown(struct m0_reqh_context *rctx)
{
	struct m0_reqh *reqh = &rctx->rc_reqh;

	M0_ENTRY();
	M0_PRE(reqh_context_invariant(rctx));

	if (m0_reqh_state_get(reqh) == M0_REQH_ST_NORMAL) {
		/*
		 * In case incomplete contexts exist, those need to be offlined
		 * early to unblock fom domains and let reqh shut down smooth.
		 */
		if (!rctx->rc_mero->cc_no_conf)
			m0_reqh_service_ctxs_shutdown_prepare(reqh);
		m0_reqh_shutdown_wait(reqh);
	}

	M0_LEAVE();
}

/**
   Finalises a request handler context.
   Sets m0_reqh::rh_shutdown true, and checks if the request handler can be
   shutdown by invoking m0_reqh_can_shutdown().
   This waits until m0_reqh_can_shutdown() returns true and then proceeds for
   further cleanup.

   @param rctx Request handler context to be finalised

   @pre reqh_context_invariant()
 */
static void cs_reqh_stop(struct m0_reqh_context *rctx)
{
	struct m0_reqh *reqh = &rctx->rc_reqh;

	M0_ENTRY();
	M0_PRE(reqh_context_invariant(rctx));

	if (M0_IN(m0_reqh_state_get(reqh), (M0_REQH_ST_DRAIN, M0_REQH_ST_INIT)))
		m0_reqh_pre_storage_fini_svcs_stop(reqh);

	M0_POST(m0_reqh_state_get(reqh) == M0_REQH_ST_STOPPED);
	M0_LEAVE();
}

static void cs_reqh_storage_fini(struct m0_reqh_context *rctx)
{
	struct m0_reqh *reqh = &rctx->rc_reqh;

	m0_reqh_be_fini(reqh);
	m0_mdstore_fini(&rctx->rc_mdstore);
	m0_reqh_addb2_fini(reqh);
	cs_be_fini(&rctx->rc_be);
	m0_reqh_post_storage_fini_svcs_stop(reqh);
	m0_reqh_fini(reqh);
	rctx->rc_state = RC_UNINITIALISED;
	M0_LEAVE();
}

struct m0_reqh *m0_cs_reqh_get(struct m0_mero *cctx)
{
	struct m0_reqh_context *rctx = &cctx->cc_reqh_ctx;

	m0_rwlock_read_lock(&cctx->cc_rwlock);
	M0_ASSERT(reqh_context_invariant(rctx));
	m0_rwlock_read_unlock(&cctx->cc_rwlock);

	return &rctx->rc_reqh;
}

M0_INTERNAL struct m0_reqh_context *m0_cs_reqh_context(struct m0_reqh *reqh)
{
	return bob_of(reqh, struct m0_reqh_context, rc_reqh, &rhctx_bob);
}

M0_INTERNAL struct m0_mero *m0_cs_ctx_get(struct m0_reqh *reqh)
{
	return m0_cs_reqh_context(reqh)->rc_mero;
}

M0_INTERNAL struct m0_storage_devs *m0_cs_storage_devs_get(void)
{
	return &m0_get()->i_storage_devs;
}

static void cs_mero_init(struct m0_mero *cctx)
{
	ndom_tlist_init(&cctx->cc_ndoms);
	m0_bob_type_tlist_init(&ndom_bob, &ndom_tl);
	cs_buffer_pools_tlist_init(&cctx->cc_buffer_pools);

	m0_bob_type_tlist_init(&cs_eps_bob, &cs_eps_tl);

	m0_rwlock_init(&cctx->cc_rwlock);

	cs_eps_tlist_init(&cctx->cc_ios_eps);
	cs_eps_tlist_init(&cctx->cc_mds_eps);
	M0_SET0(&cctx->cc_stats_svc_epx);
	cctx->cc_args.ca_argc = 0;
	cctx->cc_args.ca_argc_max = 0;
	cctx->cc_args.ca_argv = NULL;
	cctx->cc_ha_addr = NULL;
}

static void cs_mero_fini(struct m0_mero *cctx)
{
	struct cs_endpoint_and_xprt *ep;

	m0_free(cctx->cc_ha_addr);

	m0_tl_teardown(cs_eps, &cctx->cc_ios_eps, ep) {
		m0_ep_and_xprt_fini(ep);
		m0_free(ep);
	};
	cs_eps_tlist_fini(&cctx->cc_ios_eps);

	m0_tl_teardown(cs_eps, &cctx->cc_mds_eps, ep) {
		m0_ep_and_xprt_fini(ep);
		m0_free(ep);
	};
	cs_eps_tlist_fini(&cctx->cc_mds_eps);

	if (cctx->cc_stats_svc_epx.ex_endpoint != NULL)
		m0_ep_and_xprt_fini(&cctx->cc_stats_svc_epx);
	cs_buffer_pools_tlist_fini(&cctx->cc_buffer_pools);
	ndom_tlist_fini(&cctx->cc_ndoms);
	m0_rwlock_fini(&cctx->cc_rwlock);
	if (cctx->cc_enable_finj)
		m0_fi_fini();
	while (cctx->cc_args.ca_argc > 0)
		m0_free(cctx->cc_args.ca_argv[--cctx->cc_args.ca_argc]);
	m0_free(cctx->cc_args.ca_argv);
}

static void cs_usage(FILE *out, const char *progname)
{
	M0_PRE(out != NULL);
	M0_PRE(progname != NULL);

	fprintf(out,
"Usage: %s [-h] [-x] [-l]\n"
"    or %s <global options> <reqh>+\n"
"\n"
"Type `%s -h' for help.\n", progname, progname, progname);
}

static void cs_help(FILE *out, const char *progname)
{
	M0_PRE(out != NULL);

	cs_usage(out, progname);
	fprintf(out, "\n"
"Queries:\n"
"  -h   Display this help.\n"
"  -v   Show version information and exit.\n"
"  -x   List supported network transports.\n"
"  -l   List supported services.\n"
"\n"
"Global options:\n"
"  -F       Force mkfs to override found filesystem.\n"
"  -Q num   Minimum length of TM receive queue.\n"
"  -M num   Maximum RPC message size.\n"
"  -H addr  Endpoint address of HA service.\n"
"  -G addr  Endpoint address of MD service.\n"
"  -R addr  Endpoint address of stats service.\n"
"  -i addr  Add new entry to the list of IOS endpoint addresses.\n"
"  -w num   Pool width.\n"
"  -Z       Run as daemon.\n"
"  -E num   Number of net buffers used by IOS.\n"
"  -J num   Number of net buffers used by SNS.\n"
"  -o str   Enable fault injection point with given name.\n"
"  -g       Disable ADDB storage.\n"
"\n"
"Request handler options:\n"
"  -D str   BE stob domain file path (used by UT only).\n"
"  -L str   BE log file path.\n"
"  -b str   BE seg0 file path.\n"
"  -B str   BE primary segment file path.\n"
"  -z num   BE primary segment size in bytes (used by m0mkfs only).\n"
"  -V num   BE log size.\n"
"  -n num   BE tx group tx nr max.\n"
"  -k num   BE tx group reg nr max.\n"
"  -K num   BE tx group reg size max.\n"
"  -p num   BE tx group payload size max.\n"
"  -C num   BE tx reg nr max.\n"
"  -N num   BE tx reg size max.\n"
"  -s num   BE tx payload size max.\n"
"  -y num   BE tx group freeze timeout min, ms.\n"
"  -Y num   BE tx group freeze timeout max, ms.\n"
"  -a       Preallocate BE segment.\n"
"  -c str   [optional] Path to the configuration database."
"           Mandatory for confd service.\n"
"  -T str   Type of storage. Supported types: linux, ad.\n"
"  -S str   Stob file path.\n"
"  -A str   ADDB stob file path.\n"
"  -d str   [optional] Path to device configuration file.\n"
"           Device configuration file should contain device id and the\n"
"           corresponding device path.\n"
"           if -U option is specified, disks.conf file is not used.\n"
"           E.g. id: 0,\n"
"                filename: /dev/sda\n"
"           Note that only AD type stob domain can be configured over device.\n"
"  -U       Use confc API instead of `-d` to obtain device configuration.\n"
"  -q num   [optional] Minimum length of TM receive queue.\n"
"           Defaults to the value set with '-Q' option.\n"
"  -m num   [optional] Maximum RPC message size.\n"
"           Defaults to the value set with '-M' option.\n"
"  -e addr  Network layer endpoint of a service.\n"
"           Format: <transport>:<address>.\n"
"           Currently supported transport is lnet.\n"
"           .\n"
"           lnet takes 4-tuple endpoint address in the form\n"
"               NID : PID : PortalNumber : TransferMachineIdentifier\n"
"           e.g. lnet:172.18.50.40@o2ib1:12345:34:1\n"
"           .\n"
"           If multiple '-e' options are provided, network transport\n"
"           will have several endpoints, distinguished by transfer machine id\n"
"           (the 4th component of 4-tuple endpoint address in lnet).\n"
"  -f fid   Process fid (mandatory for m0d).\n"
"  -I       Disable direct I/O for data.\n"
"  -j       Enable fault injection service (FIS).\n"
"\n"
"Example:\n"
"    %s -Q 4 -M 4096 -T linux -D bepath -S stobfile \\\n"
"        -e lnet:172.18.50.40@o2ib1:12345:34:1 \\\n"
"        -q 8 -m 65536 \\\n"
"        -f '<0x7200000000000001:1>'\n",
		progname);
}

static int cs_reqh_ctx_validate(struct m0_mero *cctx)
{
	struct m0_reqh_context      *rctx = &cctx->cc_reqh_ctx;
	struct cs_endpoint_and_xprt *ep;
	M0_ENTRY();

	if (!cctx->cc_no_storage) {
		if (rctx->rc_stype == NULL)
			return M0_ERR_INFO(-EINVAL, "rc_stype is not set");
		if (rctx->rc_stpath == NULL)
			return M0_ERR_INFO(-EINVAL, "rc_stpath is not set");
		if (rctx->rc_bepath == NULL)
			return M0_ERR_INFO(-EINVAL, "rc_bepath is not set");
	}
	cctx->cc_recv_queue_min_length = max64(cctx->cc_recv_queue_min_length,
					       M0_NET_TM_RECV_QUEUE_DEF_LEN);
	rctx->rc_recv_queue_min_length = max64(rctx->rc_recv_queue_min_length,
					       M0_NET_TM_RECV_QUEUE_DEF_LEN);

	if (rctx->rc_max_rpc_msg_size == 0)
		rctx->rc_max_rpc_msg_size = cctx->cc_max_rpc_msg_size;

	if (cctx->cc_no_storage)
		return M0_RC(0);

	if (!stype_is_valid(rctx->rc_stype)) {
		cs_stob_types_list(cctx->cc_outfile);
		return M0_ERR_INFO(-EINVAL, "Invalid service type");
	}

	if (cs_eps_tlist_is_empty(&rctx->rc_eps) && rctx->rc_nr_services == 0)
		return M0_RC(0);

	if (cs_eps_tlist_is_empty(&rctx->rc_eps))
		return M0_ERR_INFO(-EINVAL, "Endpoint is missing");

	m0_tl_for(cs_eps, &rctx->rc_eps, ep) {
		int rc;

		M0_ASSERT(cs_endpoint_and_xprt_bob_check(ep));
		rc = cs_endpoint_validate(cctx, ep->ex_endpoint, ep->ex_xprt);
		if (rc != 0)
			return M0_ERR_INFO(rc, "Invalid endpoint: %s",
				      ep->ex_endpoint);
	} m0_tl_endfor;

	return M0_RC(0);
}

static int cs_reqh_ctx_services_validate(struct m0_mero *cctx)
{
	struct m0_reqh_context *rctx = &cctx->cc_reqh_ctx;
	int                     i;

	M0_ENTRY();

	M0_PRE(reqh_ctx_services_are_valid(rctx));

	for (i = 0; i < M0_CST_NR && rctx->rc_services[i] != NULL;
	     ++i) {
		const char *sname = rctx->rc_services[i];

		if (!m0_reqh_service_is_registered(sname))
			return M0_ERR_INFO(-ENOENT,
					   "Service is not registered: %s",
					   sname);

		if (service_is_duplicate(rctx, sname))
			return M0_ERR_INFO(-EEXIST, "Service is not unique: %s",
				           sname);
	}

	return M0_RC(0);
}

/**
   Causes the process to run as a daemon if appropriate context flag is set.
   This involves forking, detaching from the keyboard if any, and ensuring
   SIGHUP will not affect the process.
   @note Must be called before any long-lived threads are created (i.e. at the
   time of calling, only the main thread should exist, although it is acceptable
   if threads are created and destroyed before going into daemon mode).  There
   is no Linux API to enforce this requirement.
   @note A trace log file opened before this function is called has a different
   process ID in the name than the process that continues to write to the file.
 */
static int cs_daemonize(struct m0_mero *cctx)
{
	if (cctx->cc_daemon) {
		struct sigaction hup_act = { .sa_handler = SIG_IGN };
		return daemon(1, 0) ?: sigaction(SIGHUP, &hup_act, NULL);
	}
	return 0;
}

static int process_fid_parse(const char *str, struct m0_fid *fid)
{
	M0_PRE(str != NULL);
	M0_PRE(fid != NULL);
	return m0_fid_sscanf(str, fid) ?:
	       m0_fid_tget(fid) == M0_CONF_PROCESS_TYPE.cot_ftype.ft_id &&
	       m0_fid_is_valid(fid) ?
	       0 : M0_ERR(-EINVAL);
}

/* With this, utilities like m0mkfs will generate process FID on the fly */
static void process_fid_generate_conditional(struct m0_reqh_context *rctx)
{
	if (!m0_fid_is_set(&rctx->rc_fid)) {
		m0_uuid_generate((struct m0_uint128*)&rctx->rc_fid);
		m0_fid_tassume(&rctx->rc_fid, &M0_CONF_PROCESS_TYPE.cot_ftype);
	}
}

/** Parses CLI arguments, filling m0_mero structure. */
static int _args_parse(struct m0_mero *cctx, int argc, char **argv)
{
	struct m0_reqh_context *rctx = &cctx->cc_reqh_ctx;
	int                     rc_getops;
	int                     rc = 0;

	M0_ENTRY();

	if (argc <= 1) {
		cs_usage(cctx->cc_outfile, argv[0]);
		return M0_RC(-EINVAL);
	}

	rc_getops = M0_GETOPTS(argv[0], argc, argv,
			/* -------------------------------------------
			 * Queries
			 */
			M0_VOIDARG('h', "Usage help",
				LAMBDA(void, (void)
				{
					cs_help(cctx->cc_outfile, argv[0]);
					rc = 1;
				})),
			M0_VOIDARG('v', "Show version information and exit",
				LAMBDA(void, (void)
				{
					m0_build_info_print();
					rc = 1;
				})),
			M0_VOIDARG('x', "List supported network transports",
				LAMBDA(void, (void)
				{
					cs_xprts_list(cctx->cc_outfile,
						      cctx->cc_xprts,
						      cctx->cc_xprts_nr);
					rc = 1;
				})),
			M0_VOIDARG('l', "List supported services",
				LAMBDA(void, (void)
				{
					printf("Supported services:\n");
					m0_reqh_service_list_print();
					rc = 1;
				})),
			/* -------------------------------------------
			 * Global options
			 */
			M0_VOIDARG('F',
				   "Force mkfs to override found filesystem",
				LAMBDA(void, (void)
				{
					cctx->cc_force = true;
				})),
			M0_FORMATARG('Q', "Minimum length of TM receive queue",
				     "%i", &cctx->cc_recv_queue_min_length),
			M0_FORMATARG('M', "Maximum RPC message size", "%i",
				     &cctx->cc_max_rpc_msg_size),
			M0_STRINGARG('H', "Endpoint address of HA service",
				LAMBDA(void, (const char *s)
				{
					cctx->cc_ha_addr = m0_strdup(s);
					M0_ASSERT(cctx->cc_ha_addr != NULL);
				})),
			M0_STRINGARG('G', "Endpoint address of MD service",
				LAMBDA(void, (const char *s)
				{
					rc = ep_and_xprt_append(
						&cctx->cc_mds_eps, s);
					M0_LOG(M0_DEBUG, "adding %s to md ep "
					       "list %d", s, rc);
				})),
			M0_STRINGARG('R', "Endpoint address of stats service",
				LAMBDA(void, (const char *s)
				{
					rc = m0_ep_and_xprt_extract(
						&cctx->cc_stats_svc_epx, s);
				})),
			M0_STRINGARG('i', "Add new entry to the list of IOS"
				     " endpoint addresses",
				LAMBDA(void, (const char *s)
				{
					rc = ep_and_xprt_append(
						&cctx->cc_ios_eps, s);
					M0_LOG(M0_DEBUG, "adding %s to ios ep "
					       "list %d", s, rc);
				})),
			M0_FORMATARG('w', "Pool width", "%i",
				     &cctx->cc_pool_width),
			M0_VOIDARG('Z', "Run as daemon",
				LAMBDA(void, (void)
				{
					cctx->cc_daemon = true;
				})),
			M0_NUMBERARG('E', "Number of net buffers used by IOS",
				LAMBDA(void, (int64_t n)
				{
					m0_ios_net_buffer_pool_size_set(n);
				})),
			M0_NUMBERARG('J', "Number of net buffers used by SNS",
				LAMBDA(void, (int64_t n)
				{
					cctx->cc_sns_buf_nr = n;
				})),
			M0_STRINGARG('o', "Enable fault injection point"
				     " with given name",
				LAMBDA(void, (const char *s)
				{
				      cctx->cc_enable_finj = true;
				      m0_fi_init();
				      rc = m0_fi_enable_fault_point(s);
				})),
			M0_VOIDARG('g', "Disable ADDB storage",
				LAMBDA(void, (void)
				{
					m0_get()->i_disable_addb2_storage =
								true;
				})),
			M0_STRINGARG('u', "Node UUID",
				LAMBDA(void, (const char *s)
				{
					/* not used here, it's a placeholder */
				})),
			/* -------------------------------------------
			 * Request handler options
			 */
			M0_STRINGARG('D', "BE stob domain file path",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_bepath = s;
				})),
			M0_STRINGARG('L', "BE log file path",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_be_log_path = s;
				})),
			M0_STRINGARG('b', "BE seg0 file path",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_be_seg0_path = s;
				})),
			M0_STRINGARG('B', "BE primary segment file path",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_be_seg_path = s;
				})),
			M0_NUMBERARG('z', "BE primary segment size",
				LAMBDA(void, (int64_t size)
				{
					rctx->rc_be_seg_size = size;
				})),
			M0_NUMBERARG('V', "BE log size",
				LAMBDA(void, (int64_t size)
				{
					rctx->rc_be_log_size = size;
				})),
			M0_NUMBERARG('n', "BE tx group tx nr max",
				LAMBDA(void, (int64_t nr)
				{
					rctx->rc_be_tx_group_tx_nr_max = nr;
				})),
			M0_NUMBERARG('k', "BE tx group reg nr max",
				LAMBDA(void, (int64_t nr)
				{
					rctx->rc_be_tx_group_reg_nr_max = nr;
				})),
			M0_NUMBERARG('K', "BE tx group reg size max",
				LAMBDA(void, (int64_t size)
				{
					rctx->rc_be_tx_group_reg_size_max =
								size;
				})),
			M0_NUMBERARG('p', "BE tx group payload size max",
				LAMBDA(void, (int64_t size)
				{
					rctx->rc_be_tx_group_payload_size_max =
								size;
				})),
			M0_NUMBERARG('C', "BE tx reg nr max",
				LAMBDA(void, (int64_t nr)
				{
					rctx->rc_be_tx_reg_nr_max = nr;
				})),
			M0_NUMBERARG('N', "BE tx reg size max",
				LAMBDA(void, (int64_t size)
				{
					rctx->rc_be_tx_reg_size_max = size;
				})),
			M0_NUMBERARG('s', "BE tx payload size max",
				LAMBDA(void, (int64_t size)
				{
					rctx->rc_be_tx_payload_size_max = size;
				})),
			M0_NUMBERARG('y', "BE tx group freeze timeout min, ms",
				LAMBDA(void, (int64_t t)
				{
				       rctx->rc_be_tx_group_freeze_timeout_min =
						t * M0_TIME_ONE_MSEC;
				})),
			M0_NUMBERARG('Y', "BE tx group freeze timeout max, ms",
				LAMBDA(void, (int64_t t)
				{
				       rctx->rc_be_tx_group_freeze_timeout_max =
						t * M0_TIME_ONE_MSEC;
				})),
			M0_VOIDARG('a', "Preallocate BE segment",
				LAMBDA(void, (void)
				{
					rctx->rc_be_seg_preallocate = true;
				})),
			M0_STRINGARG('c', "Path to the configuration database",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_confdb = s;
				})),
			M0_STRINGARG('T', "Storage domain type",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_stype = s;
				})),
			M0_STRINGARG('S', "Data storage filename",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_stpath = s;
				})),
			M0_STRINGARG('A', "ADDB storage domain location",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_addb_stlocation = s;
				})),
			M0_STRINGARG('d', "Device configuration file",
				LAMBDA(void, (const char *s)
				{
					rctx->rc_dfilepath = s;
				})),
			M0_VOIDARG('U', "Use confc API instead of `-d`"
				   " to obtain device configuration",
				LAMBDA(void, (void)
				{
					rctx->rc_stob.s_ad_disks_init = true;
				})),
			M0_NUMBERARG('q', "Minimum TM recv queue length",
				LAMBDA(void, (int64_t length)
				{
					rctx->rc_recv_queue_min_length = length;
				})),
			M0_NUMBERARG('m', "Maximum RPC message size",
				LAMBDA(void, (int64_t size)
				{
					rctx->rc_max_rpc_msg_size = size;
				})),
			/*
			 * XXX TODO Test the following use case: endpoints are
			 * specified both via `-e' CLI option and via
			 * configuration.
			 */
			M0_STRINGARG('e', "Network endpoint,"
				     " e.g. transport:address",
				LAMBDA(void, (const char *s)
				{
				      rc = ep_and_xprt_append(&rctx->rc_eps, s);
				})),
			M0_STRINGARG('f', "Process fid (mandatory for m0d)",
				LAMBDA(void, (const char *s)
				{
				      rc = process_fid_parse(s, &rctx->rc_fid);
				})),
			M0_VOIDARG('I', "Disable direct I/O for data",
				LAMBDA(void, (void)
				{
					rctx->rc_disable_direct_io = true;
				})),
			M0_VOIDARG('j', "Enable fault injection service (FIS)",
				LAMBDA(void, (void)
				{
					rctx->rc_fis_enabled = true;
				})),
			);
	/* generate reqh fid in case it is all-zero */
	process_fid_generate_conditional(rctx);

	return M0_RC(rc_getops ?: rc);
}

static int cs_args_parse(struct m0_mero *cctx, int argc, char **argv)
{
	M0_ENTRY();
	return _args_parse(cctx, argc, argv);
}

static void cs_ha_connect(struct m0_mero *cctx)
{
	m0_mero_ha_connect(&cctx->cc_mero_ha);
	cs_ha_process_event(cctx, M0_CONF_HA_PROCESS_STARTING);
}

static int cs_conf_setup(struct m0_mero *cctx)
{
	struct m0_reqh      *reqh = mero2reqh(cctx);
	struct m0_confc_args conf_args = {
		.ca_profile = "0:0",
		.ca_rmach   = m0_mero_to_rmach(cctx),
		.ca_group   = m0_locality0_get()->lo_grp
	};
	int                  rc;

	if (cctx->cc_reqh_ctx.rc_confdb != NULL) {
		rc = m0_file_read(cctx->cc_reqh_ctx.rc_confdb,
				  &conf_args.ca_confstr);
		if (rc != 0)
			return M0_ERR(rc);
	}

	rc = m0_reqh_conf_setup(reqh, &conf_args);
	/* confstr is not needed after m0_reqh_conf_setup() */
	m0_free0(&conf_args.ca_confstr);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_rconfc_start_sync(mero2rconfc(cctx));
	if (rc != 0) {
		m0_rconfc_stop_sync(mero2rconfc(cctx));
		m0_rconfc_fini(mero2rconfc(cctx));
	}
	return M0_RC(rc);
}

static void cs_conf_fini(struct m0_mero *cctx)
{
	if (cctx->cc_pools_common.pc_confc != NULL)
		m0_pools_common_fini(&cctx->cc_pools_common);

	if (m0_confc_is_inited(m0_mero2confc(cctx))) {
		m0_rconfc_stop_sync(mero2rconfc(cctx));
		m0_rconfc_fini(mero2rconfc(cctx));
	}
}

static int cs_reqh_mdpool_layouts_setup(struct m0_mero *cctx)
{
	return M0_RC(m0_reqh_mdpool_layout_build(&cctx->cc_reqh_ctx.rc_reqh));
}

/**
 * The callback is installed to m0_rconfc::rc_fatal_cb intended to catch rconfc
 * failure state, and because of that shut down m0d nicely. With rconfc failed
 * mero instance becomes non-functional, as long as conf reading is impossible.
 *
 * The callback sends SIGINT to itself resulting eventually in m0_cs_fini() call.
 */
static void cs_rconfc_fatal_cb(struct m0_rconfc *rconfc)
{
	int signum = M0_FI_ENABLED("ut_signal") ? SIGUSR2 : SIGINT;

	M0_ENTRY("signum=%d", signum);
	if (kill(getpid(), signum) != 0)
		M0_LOG(M0_ERROR, "kill() failed with errno=%d", errno);
	M0_LEAVE();
}

volatile sig_atomic_t gotsignal;

/**
 * For the sake of UT framework. Possible signal value sent due to rconfc fell
 * into failed state in the course of UT passage needs to be cleaned up.
 */
M0_INTERNAL void m0_cs_gotsignal_reset(void)
{
	/* get rid of remnants of cs_rconfc_fatal_cb() if called */
	gotsignal = 0;
}

/**
 * mero/setup initialisation levels.
 *
 * Future improvements:
 * - fix CS_LEVEL_BUFFER_POOL_AND_NET_DOMAINS_FINI_HACK;
 * - define m0_rconfc and m0_ha init order and call cs_ha_connect() only once;
 * - m0_ha_client_del() returns int and the return code is not checked. If it is
 *   checked rpcping ST fails;
 * - finalise subsystems at the same level they were initialised. It would
 *   ensure the right finalisation order for the subsystems that depend on
 *   each other;
 * - make m0_cs_init()/m0_cs_setup_env()/m0_cs_start() a single function;
 * - group the relevant subsystem initialisers and finalisers into separate
 *   m0_modules. It's not possible to do this before the init/fini are not
 *   matched in the existing "single module for entire mero/setup" scheme;
 * - fix double m0_confc_root_open()/m0_confc_close() for the filesystem conf obj;
 * - fix "if (cctx->cc_no_conf) return M0_RC(0)" copy-paste one way or another;
 * - pass parameters to m0_cs_init()/m0_cs_setup_env()/m0_cs_start() using
 *   m0_conf wherever possible;
 * - make some steps (like storage) optional through configuration;
 * - make platform-dependent code to a new abstraction or hide it under
 *   #ifdef __KERNEL__:
 *   - signal handling and kernel module load/unload handling;
 *   - setrlimit (take from m0d code);
 *   - process restart (currently implemented in mero/m0d.c and it's not used);
 * - move m0t1fs, clovis and m0_halon_interface init/fini code to the new
 *   mero/setup;
 * - reconsider m0_reqh/m0_rpc_machine/m0_net/m0_net_buffer_pool init/fini
 *   sequence.
 */
enum cs_level {
	CS_LEVEL_MERO_INIT,
	/*
	 * XXX Added as a hack to match original implementation.
	 * I don't know why it works now.
	 *
	 * The proper way would be to wait in m0_net_buffer_pool_fini()
	 * until all buffers are not in use, then run the finalisation code.
	 */
	CS_LEVEL_BUFFER_POOL_AND_NET_DOMAINS_FINI_HACK,
	CS_LEVEL_REQH_CTX_INIT,
	CS_LEVEL_CS_INIT,
	CS_LEVEL_RWLOCK_LOCK,
	CS_LEVEL_ARGS_PARSE,
	CS_LEVEL_REQH_CTX_VALIDATE,
	CS_LEVEL_DAEMONIZE,
	CS_LEVEL_NET_DOMAINS_INIT,
	CS_LEVEL_BUFFER_POOL_SETUP,
	CS_LEVEL_REQH_START,
	CS_LEVEL_RPC_MACHINES_INIT,
	CS_LEVEL_HA_INIT,
	CS_LEVEL_HA_CONNECT_ATTEMPT1,
	CS_LEVEL_REQH_STOP_HACK,
	CS_LEVEL_RCONFC_INIT_START,
	CS_LEVEL_HA_CLIENT_ADD,
	CS_LEVEL_CONF_GET,
	CS_LEVEL_CONF_FULL_LOAD,
	CS_LEVEL_CONF_SERVICES_INIT,
	CS_LEVEL_CONF_TO_ARGS,
	CS_LEVEL_CONF_ARGS_PARSE,
	CS_LEVEL_HA_CONNECT_ATTEMPT2,
	CS_LEVEL_POOLS_COMMON_INIT,
	CS_LEVEL_POOLS_SETUP,
	CS_LEVEL_SET_OOSTORE,
	CS_LEVEL_REQH_CTX_SERVICES_VALIDATE,
	CS_LEVEL_CONF_FS_CONFC_CLOSE,
	CS_LEVEL_STORAGE_SETUP,
	CS_LEVEL_RWLOCK_UNLOCK,
	CS_LEVEL_STARTED_EVENT_FOR_MKFS,
	CS_LEVEL_RCONFC_FATAL_CALLBACK,
	CS_LEVEL_DIX_SETUP,
	CS_LEVEL_SETUP_ENV,
	CS_LEVEL_CHECK_CONFIG,
	CS_LEVEL_CONF_GET2,
	CS_LEVEL_POOLS_SERVICE_CTX_CREATE,
	CS_LEVEL_POOL_VERSIONS_SETUP,
	CS_LEVEL_REQH_SERVICES_START,
	CS_LEVEL_REQH_MDPOOL_LAYOUTS_SETUP,
	CS_LEVEL_CONF_CONFC_HA_UPDATE,
	CS_LEVEL_CONF_FS_CONFC_CLOSE2,
	CS_LEVEL_STARTED_EVENT_FOR_M0D,
	CS_LEVEL_START,
};

static const struct m0_modlev cs_module_levels[];

static struct m0_mero *cs_module2mero(struct m0_module *module)
{
	return bob_of(module, struct m0_mero, cc_module, &cs_bob_type);
}

static int cs_level_enter(struct m0_module *module)
{
	struct m0_mero         *cctx = cs_module2mero(module);
	struct m0_reqh_context *rctx = &cctx->cc_reqh_ctx;
	struct m0_reqh         *reqh = mero2reqh(cctx);
	int                     level = module->m_cur + 1;
	const char             *level_name = cs_module_levels[level].ml_name;
	int                     rc;

	M0_ENTRY("cctx=%p level=%d level_name=%s", cctx, level, level_name);
	if (gotsignal)
		return M0_ERR(-EINTR);
	switch (level) {
	case CS_LEVEL_MERO_INIT:
		cs_mero_init(cctx);
		return M0_RC(0);
	case CS_LEVEL_BUFFER_POOL_AND_NET_DOMAINS_FINI_HACK:
		return M0_RC(0);
	case CS_LEVEL_REQH_CTX_INIT:
		return M0_RC(cs_reqh_ctx_init(cctx));
	case CS_LEVEL_CS_INIT:
		return M0_RC(0);
	case CS_LEVEL_RWLOCK_LOCK:
		m0_rwlock_write_lock(&cctx->cc_rwlock);
		return M0_RC(0);
	case CS_LEVEL_ARGS_PARSE:
		return M0_RC(cs_args_parse(cctx,
					   cctx->cc_setup_env_argc,
					   cctx->cc_setup_env_argv));
	case CS_LEVEL_REQH_CTX_VALIDATE:
		return M0_RC(cs_reqh_ctx_validate(cctx));
	case CS_LEVEL_DAEMONIZE:
		return M0_RC(cs_daemonize(cctx));
	case CS_LEVEL_NET_DOMAINS_INIT:
		return M0_RC(cs_net_domains_init(cctx));
	case CS_LEVEL_BUFFER_POOL_SETUP:
		return M0_RC(cs_buffer_pool_setup(cctx));
	case CS_LEVEL_REQH_START:
		return M0_RC(cs_reqh_start(&cctx->cc_reqh_ctx));
	case CS_LEVEL_RPC_MACHINES_INIT:
		return M0_RC(cs_rpc_machines_init(cctx));
	case CS_LEVEL_HA_INIT:
		return M0_RC(cs_ha_init(cctx));
	case CS_LEVEL_HA_CONNECT_ATTEMPT1:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		if (cctx->cc_reqh_ctx.rc_confdb == NULL)
			cs_ha_connect(cctx);
		return M0_RC(0);
	case CS_LEVEL_REQH_STOP_HACK:
		return M0_RC(0);
	case CS_LEVEL_RCONFC_INIT_START:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		return M0_RC(cs_conf_setup(cctx));
	case CS_LEVEL_HA_CLIENT_ADD:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		return M0_RC(m0_ha_client_add(m0_mero2confc(cctx)));
	case CS_LEVEL_CONF_GET:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		rc = m0_confc_root_open(m0_reqh2confc(reqh),
					&cctx->cc_conf_root);
		if (rc != 0)
			cctx->cc_conf_root = NULL;
		return M0_RC(rc);
	case CS_LEVEL_CONF_FULL_LOAD:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		return M0_RC(m0_conf_full_load(cctx->cc_conf_root));
	case CS_LEVEL_CONF_SERVICES_INIT:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		return M0_RC(cs_conf_services_init(cctx));
	case CS_LEVEL_CONF_TO_ARGS:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		return M0_RC(cs_conf_to_args(&cctx->cc_args,
					     cctx->cc_conf_root));
	case CS_LEVEL_CONF_ARGS_PARSE:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		return M0_RC(_args_parse(cctx, cctx->cc_args.ca_argc,
		                         cctx->cc_args.ca_argv));
	case CS_LEVEL_HA_CONNECT_ATTEMPT2:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		if (cctx->cc_reqh_ctx.rc_confdb != NULL)
			cs_ha_connect(cctx);
		return M0_RC(0);
	case CS_LEVEL_POOLS_COMMON_INIT:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		return M0_RC(m0_pools_common_init(&cctx->cc_pools_common,
						  m0_mero_to_rmach(cctx)));
	case CS_LEVEL_POOLS_SETUP:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		return M0_RC(m0_pools_setup(&cctx->cc_pools_common,
					    NULL, NULL, NULL));
	case CS_LEVEL_SET_OOSTORE:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		if (cctx->cc_pools_common.pc_md_redundancy > 0)
			mero2reqh(cctx)->rh_oostore = true;
		return M0_RC(0);
	case CS_LEVEL_REQH_CTX_SERVICES_VALIDATE:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		if (!cctx->cc_mkfs)
			return M0_RC(cs_reqh_ctx_services_validate(cctx));
		return M0_RC(0);
	case CS_LEVEL_CONF_FS_CONFC_CLOSE:
		if (cctx->cc_no_conf)
			return M0_RC(0);
		m0_confc_close(&cctx->cc_conf_root->rt_obj);
		cctx->cc_conf_root = NULL;
		return M0_RC(0);
	case CS_LEVEL_STORAGE_SETUP:
		return M0_RC(cs_storage_setup(cctx));
	case CS_LEVEL_RWLOCK_UNLOCK:
		m0_rwlock_write_unlock(&cctx->cc_rwlock);
		return M0_RC(0);
	case CS_LEVEL_STARTED_EVENT_FOR_MKFS:
		/*
		 * m0_cs_start() is not called in mkfs mode.
		 *
		 * Halon expects STARTED to be able to continue the bootstrap,
		 * even in mkfs case.
		 *
		 * XXX STARTED expected even in the error case.
		 * It should be fixed either here or in Halon.
		 */
		if (cctx->cc_mkfs)
			cs_ha_process_event(cctx, M0_CONF_HA_PROCESS_STARTED);
		return M0_RC(0);
	case CS_LEVEL_RCONFC_FATAL_CALLBACK:
		if (!cctx->cc_no_conf) { /* otherwise rconfc did not start */
			/*
			 * So far rconfc has no fatal callback installed
			 * to distinguish rconfc failure occurred during
			 * m0_rconfc_start_wait() from failure state reached
			 * after successful rconfc start. To catch the
			 * after-start failure, fatal callback is installed
			 * here.
			 */
			m0_rconfc_lock(mero2rconfc(cctx));
			m0_rconfc_fatal_cb_set(mero2rconfc(cctx),
					       cs_rconfc_fatal_cb);
			m0_rconfc_unlock(mero2rconfc(cctx));
		}
		return M0_RC(0);
	case CS_LEVEL_DIX_SETUP:
		if (cctx->cc_mkfs)
			return M0_RC(m0_cs_dix_setup(cctx));
		else
			return M0_RC(0);
	case CS_LEVEL_SETUP_ENV:
		return M0_RC(0);
	case CS_LEVEL_CHECK_CONFIG:
		cctx->cc_skip_pools_and_ha_update = cctx->cc_no_conf ||
					     bad_address(cctx->cc_ha_addr) ||
					     cctx->cc_no_all2all_connections;
		return M0_RC(0);
	case CS_LEVEL_CONF_GET2:
		if (cctx->cc_skip_pools_and_ha_update)
			return M0_RC(0);
		rc = m0_confc_root_open(m0_reqh2confc(reqh),
					&cctx->cc_conf_root);
		if (rc != 0)
			cctx->cc_conf_root = NULL;
		return M0_RC(rc);
	case CS_LEVEL_POOLS_SERVICE_CTX_CREATE:
		if (cctx->cc_skip_pools_and_ha_update)
			return M0_RC(0);
		return M0_RC(m0_pools_service_ctx_create(&cctx->cc_pools_common));
	case CS_LEVEL_POOL_VERSIONS_SETUP:
		if (cctx->cc_skip_pools_and_ha_update)
			return M0_RC(0);
		return M0_RC(m0_pool_versions_setup(&cctx->cc_pools_common));
	case CS_LEVEL_REQH_SERVICES_START:
		return M0_RC(reqh_services_start(rctx, cctx));
	case CS_LEVEL_REQH_MDPOOL_LAYOUTS_SETUP:
		if (cctx->cc_skip_pools_and_ha_update)
			return M0_RC(0);
		if (M0_FI_ENABLED("pools_cleanup"))
			return M0_RC(-EINVAL);
		return M0_RC(cs_reqh_mdpool_layouts_setup(cctx));
	case CS_LEVEL_CONF_CONFC_HA_UPDATE:
		if (cctx->cc_skip_pools_and_ha_update)
			return M0_RC(0);
		return M0_RC(m0_conf_confc_ha_update(m0_reqh2confc(reqh)));
	case CS_LEVEL_CONF_FS_CONFC_CLOSE2:
		m0_confc_close(&cctx->cc_conf_root->rt_obj);
		cctx->cc_conf_root = NULL;
		return M0_RC(0);
	case CS_LEVEL_STARTED_EVENT_FOR_M0D:
		cs_ha_process_event(cctx, M0_CONF_HA_PROCESS_STARTED);
		return M0_RC(0);
	case CS_LEVEL_START:
		return M0_RC(0);
	default:
		return M0_ERR_INFO(-ENOSYS, "cctx=%p level=%d", cctx, level);
	}
}

static void cs_level_leave(struct m0_module *module)
{
	struct m0_mero         *cctx = cs_module2mero(module);
	struct m0_reqh_context *rctx = &cctx->cc_reqh_ctx;
	int                     level = module->m_cur;
	const char             *level_name = cs_module_levels[level].ml_name;

	M0_ENTRY("cctx=%p level=%d level_name=%s", cctx, level, level_name);
	switch (level) {
	case CS_LEVEL_MERO_INIT:
		cs_mero_fini(cctx);
		break;
	case CS_LEVEL_BUFFER_POOL_AND_NET_DOMAINS_FINI_HACK:
		cs_buffer_pool_fini(cctx);
		cs_net_domains_fini(cctx);
		break;
	case CS_LEVEL_REQH_CTX_INIT:
		cs_reqh_ctx_fini(rctx);
		break;
	case CS_LEVEL_CS_INIT:
	case CS_LEVEL_RWLOCK_LOCK:
	case CS_LEVEL_ARGS_PARSE:
	case CS_LEVEL_REQH_CTX_VALIDATE:
	case CS_LEVEL_DAEMONIZE:
	case CS_LEVEL_NET_DOMAINS_INIT:
		/* see CS_LEVEL_BUFFER_FINI_HACK */
	case CS_LEVEL_BUFFER_POOL_SETUP:
		/* see CS_LEVEL_BUFFER_FINI_HACK */
		break;
	case CS_LEVEL_REQH_START:
		if (rctx->rc_state == RC_INITIALISED)
			cs_reqh_storage_fini(rctx);
		break;
	case CS_LEVEL_RPC_MACHINES_INIT:
		cs_rpc_machines_fini(&rctx->rc_reqh);
		break;
	case CS_LEVEL_HA_INIT:
	case CS_LEVEL_HA_CONNECT_ATTEMPT1:
		break;
	case CS_LEVEL_REQH_STOP_HACK:
		if (cctx->cc_ha_is_started)
			cctx->cc_ha_was_started = true;
		/*
		 * XXX There is a race during HA finalisation. Addb2 is still
		 * up at this point and may cause I/O to storage. If the I/O
		 * fails ioq_io_error() uses i_ha and i_ha_link to send an HA
		 * message. This can be done along with the HA finalisation.
		 */
		if (cctx->cc_ha_was_started &&
		    rctx->rc_state >= RC_REQH_INITIALISED)
			cs_ha_fini(cctx);
		if (rctx->rc_state >= RC_REQH_INITIALISED)
			cs_reqh_stop(rctx);
		break;
	case CS_LEVEL_RCONFC_INIT_START:
		if (cctx->cc_conf_root != NULL) {
			m0_confc_close(&cctx->cc_conf_root->rt_obj);
			cctx->cc_conf_root = NULL;
		}
		/*
		 * Need to shut rconfc down prior to stopping REQH
		 * services, as rconfc needs RPC be up and running
		 * to safely stop and fini.
		 */
		cs_conf_fini(cctx);
		break;
	case CS_LEVEL_HA_CLIENT_ADD:
		m0_ha_client_del(m0_mero2confc(cctx));
		break;
	case CS_LEVEL_CONF_GET:
		break;
	case CS_LEVEL_CONF_FULL_LOAD:
		break;
	case CS_LEVEL_CONF_SERVICES_INIT:
		reqh_context_services_fini(&cctx->cc_reqh_ctx, cctx);
		break;
	case CS_LEVEL_CONF_TO_ARGS:
		break;
	case CS_LEVEL_CONF_ARGS_PARSE:
		break;
	case CS_LEVEL_HA_CONNECT_ATTEMPT2:
		if (cctx->cc_mkfs)
			cs_ha_process_event(cctx, M0_CONF_HA_PROCESS_STOPPED);
		if (cctx->cc_ha_was_started)
			cs_ha_stop(cctx);
		break;
	case CS_LEVEL_POOLS_COMMON_INIT:
		if (cctx->cc_pools_common.pc_confc != NULL)
			m0_pools_common_fini(&cctx->cc_pools_common);
		break;
	case CS_LEVEL_POOLS_SETUP:
		if (cctx->cc_pools_common.pc_confc != NULL)
			m0_pools_destroy(&cctx->cc_pools_common);
		break;
	case CS_LEVEL_SET_OOSTORE:
		break;
	case CS_LEVEL_REQH_CTX_SERVICES_VALIDATE:
		break;
	case CS_LEVEL_CONF_FS_CONFC_CLOSE:
		break;
	case CS_LEVEL_STORAGE_SETUP:
		if (rctx->rc_state == RC_INITIALISED) {
			cs_storage_fini(&rctx->rc_stob);
			rctx->rc_reqh.rh_pools = NULL;
		}
		break;
	case CS_LEVEL_RWLOCK_UNLOCK:
		break;
	case CS_LEVEL_STARTED_EVENT_FOR_MKFS:
		if (cctx->cc_mkfs)
			cs_ha_process_event(cctx, M0_CONF_HA_PROCESS_STOPPING);
		break;
	case CS_LEVEL_RCONFC_FATAL_CALLBACK:
		break;
	case CS_LEVEL_DIX_SETUP:
		break;
	case CS_LEVEL_SETUP_ENV:
		break;
	case CS_LEVEL_CHECK_CONFIG:
		break;
	case CS_LEVEL_CONF_GET2:
		if (cctx->cc_conf_root != NULL) {
			m0_confc_close(&cctx->cc_conf_root->rt_obj);
			cctx->cc_conf_root = NULL;
		}
		break;
	case CS_LEVEL_POOLS_SERVICE_CTX_CREATE:
		if (cctx->cc_pools_common.pc_confc != NULL)
			m0_pools_service_ctx_destroy(&cctx->cc_pools_common);
		break;
	case CS_LEVEL_POOL_VERSIONS_SETUP:
		if (rctx->rc_state == RC_INITIALISED)
			m0_reqh_layouts_cleanup(&rctx->rc_reqh);
		if (cctx->cc_pools_common.pc_confc != NULL)
			m0_pool_versions_destroy(&cctx->cc_pools_common);
		break;
	case CS_LEVEL_REQH_SERVICES_START:
		if (rctx->rc_state >= RC_REQH_INITIALISED) {
			cs_reqh_shutdown(rctx);
			/*
			 * cctx->cc_ha_is_started is being changed,
			 * we need the old one
			 */
			cctx->cc_ha_was_started = cctx->cc_ha_is_started;
		}
		break;
	case CS_LEVEL_REQH_MDPOOL_LAYOUTS_SETUP:
		break;
	case CS_LEVEL_CONF_CONFC_HA_UPDATE:
		break;
	case CS_LEVEL_CONF_FS_CONFC_CLOSE2:
		break;
	case CS_LEVEL_STARTED_EVENT_FOR_M0D:
		cs_ha_process_event(cctx, M0_CONF_HA_PROCESS_STOPPING);
		break;
	case CS_LEVEL_START:
		break;
	default:
		M0_IMPOSSIBLE("Can't be here: level=%d", level);
	}
	M0_LEAVE();
}

#define CS_MODULE_LEVEL(level) [level] = {      \
		.ml_name  = #level,             \
		.ml_enter = cs_level_enter,     \
		.ml_leave = cs_level_leave,     \
	}

static const struct m0_modlev cs_module_levels[] = {
	CS_MODULE_LEVEL(CS_LEVEL_MERO_INIT),
	CS_MODULE_LEVEL(CS_LEVEL_BUFFER_POOL_AND_NET_DOMAINS_FINI_HACK),
	CS_MODULE_LEVEL(CS_LEVEL_REQH_CTX_INIT),
	CS_MODULE_LEVEL(CS_LEVEL_CS_INIT),
	CS_MODULE_LEVEL(CS_LEVEL_RWLOCK_LOCK),
	CS_MODULE_LEVEL(CS_LEVEL_ARGS_PARSE),
	CS_MODULE_LEVEL(CS_LEVEL_REQH_CTX_VALIDATE),
	CS_MODULE_LEVEL(CS_LEVEL_DAEMONIZE),
	CS_MODULE_LEVEL(CS_LEVEL_NET_DOMAINS_INIT),
	CS_MODULE_LEVEL(CS_LEVEL_BUFFER_POOL_SETUP),
	CS_MODULE_LEVEL(CS_LEVEL_REQH_START),
	CS_MODULE_LEVEL(CS_LEVEL_RPC_MACHINES_INIT),
	CS_MODULE_LEVEL(CS_LEVEL_HA_INIT),
	CS_MODULE_LEVEL(CS_LEVEL_HA_CONNECT_ATTEMPT1),
	CS_MODULE_LEVEL(CS_LEVEL_REQH_STOP_HACK),
	CS_MODULE_LEVEL(CS_LEVEL_RCONFC_INIT_START),
	CS_MODULE_LEVEL(CS_LEVEL_HA_CLIENT_ADD),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_GET),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_FULL_LOAD),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_SERVICES_INIT),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_TO_ARGS),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_ARGS_PARSE),
	CS_MODULE_LEVEL(CS_LEVEL_HA_CONNECT_ATTEMPT2),
	CS_MODULE_LEVEL(CS_LEVEL_POOLS_COMMON_INIT),
	CS_MODULE_LEVEL(CS_LEVEL_POOLS_SETUP),
	CS_MODULE_LEVEL(CS_LEVEL_SET_OOSTORE),
	CS_MODULE_LEVEL(CS_LEVEL_REQH_CTX_SERVICES_VALIDATE),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_FS_CONFC_CLOSE),
	CS_MODULE_LEVEL(CS_LEVEL_STORAGE_SETUP),
	CS_MODULE_LEVEL(CS_LEVEL_RWLOCK_UNLOCK),
	CS_MODULE_LEVEL(CS_LEVEL_STARTED_EVENT_FOR_MKFS),
	CS_MODULE_LEVEL(CS_LEVEL_RCONFC_FATAL_CALLBACK),
	CS_MODULE_LEVEL(CS_LEVEL_DIX_SETUP),
	CS_MODULE_LEVEL(CS_LEVEL_SETUP_ENV),
	CS_MODULE_LEVEL(CS_LEVEL_CHECK_CONFIG),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_GET2),
	CS_MODULE_LEVEL(CS_LEVEL_POOLS_SERVICE_CTX_CREATE),
	CS_MODULE_LEVEL(CS_LEVEL_POOL_VERSIONS_SETUP),
	CS_MODULE_LEVEL(CS_LEVEL_REQH_SERVICES_START),
	CS_MODULE_LEVEL(CS_LEVEL_REQH_MDPOOL_LAYOUTS_SETUP),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_CONFC_HA_UPDATE),
	CS_MODULE_LEVEL(CS_LEVEL_CONF_FS_CONFC_CLOSE2),
	CS_MODULE_LEVEL(CS_LEVEL_STARTED_EVENT_FOR_M0D),
	CS_MODULE_LEVEL(CS_LEVEL_START),
};
#undef CS_MODULE_LEVEL

int m0_cs_setup_env(struct m0_mero *cctx, int argc, char **argv)
{
	int rc;

	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EINVAL);

	cctx->cc_setup_env_argc = argc;
	cctx->cc_setup_env_argv = argv;
	rc = m0_module_init(&cctx->cc_module, CS_LEVEL_SETUP_ENV);
	if (rc != 0)
		m0_module_fini(&cctx->cc_module, CS_LEVEL_CS_INIT);
	return M0_RC(rc);
}

int m0_cs_start(struct m0_mero *cctx)
{
	int rc;

	M0_ENTRY();

	rc = m0_module_init(&cctx->cc_module, CS_LEVEL_START);
	if (rc != 0)
		m0_module_fini(&cctx->cc_module, CS_LEVEL_SETUP_ENV);
	return rc == 0 ? M0_RC(0) : M0_ERR(rc);
}

int m0_cs_init(struct m0_mero *cctx, struct m0_net_xprt **xprts,
	       size_t xprts_nr, FILE *out, bool mkfs)
{
	int rc;

	M0_ENTRY();
	M0_PRE(xprts != NULL && xprts_nr > 0 && out != NULL);

	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EINVAL);

	cctx->cc_xprts    = xprts;
	cctx->cc_xprts_nr = xprts_nr;
	cctx->cc_outfile  = out;
	cctx->cc_mkfs     = mkfs;
	cctx->cc_force    = false;
	cctx->cc_no_all2all_connections = false;
	cctx->cc_sns_buf_nr = 1 << 6;

	m0_module_setup(&cctx->cc_module, "mero/setup", cs_module_levels,
	                ARRAY_SIZE(cs_module_levels), m0_get());
	m0_mero_bob_init(cctx);
	rc = m0_module_init(&cctx->cc_module, CS_LEVEL_REQH_CTX_INIT);
	if (rc != 0) {
		m0_module_fini(&cctx->cc_module, M0_MODLEV_NONE);
		m0_mero_bob_fini(cctx);
	}
	return M0_RC(rc);
}

void m0_cs_fini(struct m0_mero *cctx)
{
	M0_ENTRY();

	m0_module_fini(&cctx->cc_module, M0_MODLEV_NONE);
	m0_mero_bob_fini(cctx);

	M0_LEAVE();
}

/**
 * Extract the path of the provided dev_id from the config file, create stob id
 * for it and call m0_stob_linux_reopen() to reopen the stob.
 */
M0_INTERNAL int m0_mero_stob_reopen(struct m0_reqh *reqh,
				    struct m0_poolmach *pm,
				    uint32_t dev_id)
{
	struct m0_stob_id       stob_id;
	struct m0_reqh_context *rctx;
	struct cs_stobs        *stob;
	yaml_document_t        *doc;
	yaml_node_t            *node;
	yaml_node_t            *s_node;
	yaml_node_item_t       *item;
	const char             *f_path;
	uint64_t                cid;
	int                     rc = 0;
	int                     result;

	rctx = m0_cs_reqh_context(reqh);
	stob = &rctx->rc_stob;
	doc = &stob->s_sfile.sf_document;
	if (rctx->rc_stob.s_ad_disks_init)
		return M0_RC(cs_conf_device_reopen(pm, stob, dev_id));
	if (!stob->s_sfile.sf_is_initialised)
		return M0_ERR(-EINVAL);
	for (node = doc->nodes.start; node < doc->nodes.top; ++node) {
		for (item = (node)->data.sequence.items.start;
		     item < (node)->data.sequence.items.top; ++item) {
			s_node = yaml_document_get_node(doc, *item);
			result = stob_file_id_get(doc, s_node, &cid);
			if (result != 0)
				continue;
			if (cid == dev_id) {
				f_path = stob_file_path_get(doc, s_node);
				m0_stob_id_make(0, cid, &stob->s_sdom->sd_id,
						&stob_id);
				rc = m0_stob_linux_reopen(&stob_id, f_path);
				if (rc != 0)
					return M0_ERR(rc);
			}
		}
	}
	return M0_RC(rc);
}

/** @} endgroup m0d */
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
