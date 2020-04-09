/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 15-Sep-2014
 */

#include "lib/memory.h"               /* m0_alloc, m0_free */
#include "lib/errno.h"                /* ENOMEM */
#include "lib/uuid.h"                 /* m0_uuid_generate */
#include "lib/finject.h"              /* M0_FI_ENABLED */
#include "lib/arith.h"                /* M0_CNT_INC */
#include "lib/mutex.h"                /* m0_mutex_lock */
#include "addb2/global.h"
#include "addb2/sys.h"
#include "fid/fid.h"                  /* m0_fid */
#include "conf/ha.h"                  /* m0_conf_ha_process_event_post */
#include "conf/helpers.h"             /* m0_confc_root_open */
#include "conf/confc.h"               /* m0_confc_state */
#include "rpc/rpclib.h"               /* m0_rpc_client_connect */
#include "pool/pool.h"                /* m0_pool_init */
#include "rm/rm_service.h"            /* m0_rms_type */
#include  "ha/epoch.h"                 /* m0_ha_client_{add, del}*/
#include "net/lnet/lnet_core_types.h" /* M0_NET_LNET_NIDSTR_SIZE */
#include "net/lnet/lnet.h"            /* m0_net_lnet_xprt */

#include "clovis/io.h"                /* io_sm_conf */
#include "clovis/clovis.h"
#include "clovis/clovis_addb.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_layout.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"                /* M0_LOG */

/* BOB type for m0_clovis */
static const struct m0_bob_type m0c_bobtype;
M0_BOB_DEFINE(static, &m0c_bobtype,  m0_clovis);

static const struct m0_bob_type m0c_bobtype = {
	.bt_name         = "m0c_bobtype",
	.bt_magix_offset = offsetof(struct m0_clovis, m0c_magic),
	.bt_magix        = M0_CLOVIS_M0C_MAGIC,
	.bt_check        = NULL,
};

/**
 * Pointer to the root configuration object that a Clovis client
 * attaches to. It is closed after Clovis initilisation is done as it
 * may become invalid at some point of time.
 */
struct m0_conf_root *clovis_conf_root;

/**
 * The Initialisation code in clovis is fiddly, lots of different parts of mero
 * need to be initialised in the correct order, if something fails, all the
 * parts that have been initialised need to be finalised and an error returned.
 *
 * This typically means that all the initialisation code needs to know how to
 * finalise clovis too, while not being the finalisation code. A rats nest of
 * implicit states need to be tracked by clovis_*_init functions, pinned down
 * by a rack of gotos, so that init can 'fini' what it has done so far.
 *
 * This gives us difficult to read duplicated code, which in turns gives us
 * really-difficult-to-test code paths when one component fails to initialise.
 *
 * Why not use a state machine?
 */
/**
 * Clovis initialises these components, in this order. If something fails
 * it automatically reverses and moves back to CLOVIS_IL_UNINITIALISED.
 *
 * Rule of thumb: if you need a goto, you are probably tracking some kind of
 *                state that should be added here instead.
 */
enum clovis_initlift_states {
	CLOVIS_IL_UNINITIALISED = 0,
	CLOVIS_IL_NET, /* TODO: break this out */
	CLOVIS_IL_RPC, /* TODO: break this out */
	CLOVIS_IL_AST_THREAD,
	CLOVIS_IL_HA,
	CLOVIS_IL_CONFC,    /* Creates confc and stashes in m0c */
	CLOVIS_IL_POOLS,
	CLOVIS_IL_POOL_VERSION,
	CLOVIS_IL_RESOURCE_MANAGER,
	CLOVIS_IL_LAYOUT_DB,
	CLOVIS_IL_IDX_SERVICE,
	CLOVIS_IL_ROOT_FID, /* TODO: remove this m0t1fs ism */
	CLOVIS_IL_ADDB2,
	CLOVIS_IL_INITIALISED,
	CLOVIS_IL_FAILED,
};

/** Forward declarations for state callbacks */
static int clovis_initlift_uninitialised(struct m0_sm *mach);
static int clovis_initlift_net(struct m0_sm *mach);
static int clovis_initlift_rpc(struct m0_sm *mach);
static int clovis_initlift_ast_thread(struct m0_sm *mach);
static int clovis_initlift_ha(struct m0_sm *mach);
static int clovis_initlift_confc(struct m0_sm *mach);
static int clovis_initlift_pools(struct m0_sm *mach);
static int clovis_initlift_pool_version(struct m0_sm *mach);
static int clovis_initlift_resource_manager(struct m0_sm *mach);
static int clovis_initlift_layouts(struct m0_sm *mach);
static int clovis_initlift_idx_service(struct m0_sm *mach);
static int clovis_initlift_rootfid(struct m0_sm *mach);
static int clovis_initlift_addb2(struct m0_sm *mach);

/**
 * State machine phases for clovis operations.
 */
struct m0_sm_state_descr clovis_initlift_phases[] = {
	[CLOVIS_IL_UNINITIALISED] = {
		.sd_flags = M0_SDF_INITIAL | M0_SDF_FINAL,
		.sd_name = "clovis-uninitialised",
		.sd_allowed = M0_BITS(CLOVIS_IL_NET, CLOVIS_IL_FAILED),
		.sd_in = clovis_initlift_uninitialised,
	},
	[CLOVIS_IL_NET] = {
		.sd_name = "init/fini-net",
		.sd_allowed = M0_BITS(CLOVIS_IL_RPC, CLOVIS_IL_UNINITIALISED),
		.sd_in = clovis_initlift_net,
	},
	[CLOVIS_IL_RPC] = {
		.sd_name = "init/fini-rpc",
		.sd_allowed = M0_BITS(CLOVIS_IL_AST_THREAD, CLOVIS_IL_NET),
		.sd_in = clovis_initlift_rpc,
	},
	[CLOVIS_IL_AST_THREAD] = {
		.sd_name = "init/fini-ast-thread",
		.sd_allowed = M0_BITS(CLOVIS_IL_HA, CLOVIS_IL_RPC),
		.sd_in = clovis_initlift_ast_thread,
	},
	[CLOVIS_IL_HA] = {
		.sd_name = "init/fini-ha",
		.sd_allowed = M0_BITS(CLOVIS_IL_CONFC,
				      CLOVIS_IL_AST_THREAD),
		.sd_in = clovis_initlift_ha,
	},
	[CLOVIS_IL_CONFC] = {
		.sd_name = "init/fini-confc",
		.sd_allowed = M0_BITS(CLOVIS_IL_POOLS,
				      CLOVIS_IL_HA),
		.sd_in = clovis_initlift_confc,
	},
	[CLOVIS_IL_POOLS] = {
		.sd_name = "init/fini-pools",
		.sd_allowed = M0_BITS(CLOVIS_IL_POOL_VERSION,
				      CLOVIS_IL_CONFC),
		.sd_in = clovis_initlift_pools,
	},
	[CLOVIS_IL_POOL_VERSION] = {
		.sd_name = "init/fini-pool-version",
		.sd_allowed = M0_BITS(CLOVIS_IL_RESOURCE_MANAGER,
				      CLOVIS_IL_POOLS),
		.sd_in = clovis_initlift_pool_version,
	},
	[CLOVIS_IL_RESOURCE_MANAGER] = {
		.sd_name = "init/fini-resource-manager",
		.sd_allowed = M0_BITS(CLOVIS_IL_LAYOUT_DB,
				      CLOVIS_IL_POOL_VERSION),
		.sd_in = clovis_initlift_resource_manager,
	},
	[CLOVIS_IL_LAYOUT_DB] = {
		.sd_name = "init/fini-layout-database",
		.sd_allowed = M0_BITS(CLOVIS_IL_IDX_SERVICE,
				      CLOVIS_IL_RESOURCE_MANAGER),
		.sd_in = clovis_initlift_layouts,
	},
	[CLOVIS_IL_IDX_SERVICE] = {
		.sd_name = "init/fini-resource-manager",
		.sd_allowed = M0_BITS(CLOVIS_IL_ROOT_FID,
				      CLOVIS_IL_LAYOUT_DB),
		.sd_in = clovis_initlift_idx_service,
	},
	[CLOVIS_IL_ROOT_FID] = {
		.sd_name = "retrieve-root-fid",
		.sd_allowed = M0_BITS(CLOVIS_IL_ADDB2,
				      CLOVIS_IL_IDX_SERVICE),
		.sd_in = clovis_initlift_rootfid,
	},
	[CLOVIS_IL_ADDB2] = {
		.sd_name = "init/fini-addb2",
		.sd_allowed = M0_BITS(CLOVIS_IL_INITIALISED,
				      CLOVIS_IL_ROOT_FID),
		.sd_in = clovis_initlift_addb2,
	},
	[CLOVIS_IL_INITIALISED] = {
		.sd_name = "clovis-initialised",
		.sd_allowed = M0_BITS(CLOVIS_IL_ADDB2),
	},
	[CLOVIS_IL_FAILED] = {
		.sd_name = "failed",
		.sd_flags = M0_SDF_FAILURE | M0_SDF_FINAL,
	}
};

/**
 * Textual descriptions for the valid state machine transitions.
 */
struct m0_sm_trans_descr clovis_initlift_trans[] = {
	/* INIT section*/
	{"initialising-network",       CLOVIS_IL_UNINITIALISED, CLOVIS_IL_NET},
	{"initialising-rpc-layer",     CLOVIS_IL_NET, CLOVIS_IL_RPC},
	{"initialising-ast-thread",    CLOVIS_IL_RPC,
				       CLOVIS_IL_AST_THREAD},
	{"connecting-to-ha",           CLOVIS_IL_AST_THREAD,
				       CLOVIS_IL_HA},
	{"connecting-to-confd",        CLOVIS_IL_HA,
				       CLOVIS_IL_CONFC},
	{"setting-up-pools",           CLOVIS_IL_CONFC,
				       CLOVIS_IL_POOLS},
	{"initialising-pool-version",  CLOVIS_IL_POOLS,
				       CLOVIS_IL_POOL_VERSION},
	{"initialising-resource-manager",
				       CLOVIS_IL_POOL_VERSION,
				       CLOVIS_IL_RESOURCE_MANAGER},
	{"initialising-layout-database",
				       CLOVIS_IL_RESOURCE_MANAGER,
				       CLOVIS_IL_LAYOUT_DB},
	{"initialising-index-service",
				       CLOVIS_IL_LAYOUT_DB,
				       CLOVIS_IL_IDX_SERVICE},
	{"retrieving-root-fid",        CLOVIS_IL_IDX_SERVICE,
				       CLOVIS_IL_ROOT_FID},
	{"initialising-addb2",         CLOVIS_IL_ROOT_FID,
				       CLOVIS_IL_ADDB2},
	{"clovis-initialised",         CLOVIS_IL_ADDB2,
				       CLOVIS_IL_INITIALISED},

	/* FINI section*/
	{"clovis-shutting-down",       CLOVIS_IL_INITIALISED,
				       CLOVIS_IL_ADDB2},
	{"finalising-addb2",           CLOVIS_IL_ADDB2,
				       CLOVIS_IL_ROOT_FID},
	{"finalising-root-fid",        CLOVIS_IL_ROOT_FID,
				       CLOVIS_IL_IDX_SERVICE},
	{"finalising-index-service",   CLOVIS_IL_IDX_SERVICE,
				       CLOVIS_IL_LAYOUT_DB},
	{"finalising-layout-database", CLOVIS_IL_LAYOUT_DB,
				       CLOVIS_IL_RESOURCE_MANAGER},
	{"finalising-resource-manager",
				       CLOVIS_IL_RESOURCE_MANAGER,
				       CLOVIS_IL_POOL_VERSION},
	{"finalising-pool-version",    CLOVIS_IL_POOL_VERSION,
				       CLOVIS_IL_POOLS},
	{"finalising-pools",           CLOVIS_IL_POOLS,
                                       CLOVIS_IL_CONFC},
	{"closing-stale-confc",        CLOVIS_IL_CONFC,
				       CLOVIS_IL_HA},
	{"closing-ha-session",         CLOVIS_IL_HA,
				       CLOVIS_IL_AST_THREAD},
	{"finalising-ast-thread",      CLOVIS_IL_AST_THREAD, CLOVIS_IL_RPC},
	{"finalising-rpc-layer",       CLOVIS_IL_RPC, CLOVIS_IL_NET},
	{"finalising-network",         CLOVIS_IL_NET, CLOVIS_IL_UNINITIALISED},
	{"clovis-initialisation-failed",
				       CLOVIS_IL_UNINITIALISED,
				       CLOVIS_IL_FAILED},
};

/**
 * Configuration structure for the clovis init state machine.
 */
struct m0_sm_conf clovis_initlift_conf = {
	.scf_name = "clovis-initlift-conf",
	.scf_nr_states = ARRAY_SIZE(clovis_initlift_phases),
	.scf_state = clovis_initlift_phases,
	.scf_trans = clovis_initlift_trans,
	.scf_trans_nr = ARRAY_SIZE(clovis_initlift_trans),
};

/**
 * Checks a m0_clovis struct is correct.
 *
 * @param m0c The clovis instance to check.
 * @return true or false.
 */
static bool clovis_m0c_invariant(struct m0_clovis *m0c)
{
	return M0_RC(m0c != NULL && m0_clovis_bob_check(m0c));
}

static struct m0_rconfc *clovis_rconfc(struct m0_clovis *m0c)
{
	return &m0c->m0c_reqh.rh_rconfc;
}

M0_INTERNAL struct m0_confc *m0_clovis_confc(struct m0_clovis *m0c)
{
	return &clovis_rconfc(m0c)->rc_confc;
}

static int clovis_fid_sscanf(const char    *s,
			     struct m0_fid *fid,
			     const char    *descr)
{
	int rc = m0_fid_sscanf(s, fid);

	if (rc != 0)
		return M0_ERR_INFO(rc, "can't m0_fid_sscanf() %s %s", descr, s);
	return M0_RC(0);
}

/**
 * Helper function to get the value of the next floor in the
 * direction of travel.
 *
 * @param m0c the clovis instance we are working with.
 * @return the next state/floor to transition to.
 */
static int clovis_initlift_get_next_floor(struct m0_clovis *m0c)
{
	int rc;

	M0_PRE(m0c != NULL);
	M0_PRE(M0_IN(m0c->m0c_initlift_direction,
		     (CLOVIS_STARTUP, CLOVIS_SHUTDOWN)));

	if (M0_FI_ENABLED("clovis_ut"))
		/* no op, -1 means don't move */
		return M0_RC(-1);

	rc = m0c->m0c_initlift_sm.sm_state + m0c->m0c_initlift_direction;
	M0_POST(rc >= CLOVIS_IL_UNINITIALISED);
	M0_POST(rc <= CLOVIS_IL_INITIALISED);

	return M0_RC(rc);
}

/**
 * Helper function to move the initlift onto its next state in the
 * direction of travel.
 *
 * @param m0c the clovis instance we are working with.
 */
static void clovis_initlift_move_next_floor(struct m0_clovis *m0c)
{
	M0_PRE(m0c != NULL);

	if (M0_FI_ENABLED("immediate_ret"))
		/*
		 * no-op: this is used by m0_clovis_init/fini to kick the init
		 * process into action - for the unit tests, we don't want to do
		 * anything.
		 */
		m0c->m0c_initlift_rc = 0;
	else if (M0_FI_ENABLED("failure"))
		m0c->m0c_initlift_rc = -EPROTO;
	else
		m0_sm_move(&m0c->m0c_initlift_sm, 0,
			   clovis_initlift_get_next_floor(m0c));
}

/**
 * Helper function to fail initialisation, move the lift into reverse
 * and log the failing rc.
 *
 * @param rc the failing rc.
 * @param m0c the clovis instance we are working with.
 */
static void clovis_initlift_fail(int rc, struct m0_clovis *m0c)
{
	M0_PRE(rc != 0);
	M0_PRE(m0c != NULL);
	M0_PRE(m0c->m0c_initlift_rc == 0);

	m0c->m0c_initlift_rc = rc;
	m0c->m0c_initlift_direction = CLOVIS_SHUTDOWN;
}

static int clovis_initlift_uninitialised(struct m0_sm *mach)
{
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));

	if (m0c->m0c_initlift_direction == CLOVIS_SHUTDOWN) {
		if (m0c->m0c_initlift_rc != 0)
			m0_sm_fail(&m0c->m0c_initlift_sm, CLOVIS_IL_FAILED,
				   m0c->m0c_initlift_rc);
	}

	return M0_RC(-1);
}

/**
 * This is heavily based on
 *  m0t1fs/linux_kernel/super.c::m0t1fs_net_fini
 *
 * @param m0c the clovis instance we are working with.
 */
static void clovis_net_fini(struct m0_clovis *m0c)
{
	M0_ENTRY();

	M0_PRE(m0c != NULL);

	m0_net_domain_fini(&m0c->m0c_ndom);
	m0c->m0c_xprt = NULL;
	m0_free(m0c->m0c_laddr);
	m0c->m0c_laddr = NULL;

	M0_LEAVE();
}

/**
 * This is heavily based on m0t1fs/linux_kernel/super.c::m0t1fs_net_init.
 * m0t1fs is a kernel model and it sets the local addr and other arguments when
 * installing the kernel model. For Clovis, as it supports both user and kernel
 * modes, it is not feasible to pass arguments as m0t1fs does, instead Clovis
 * asks an applition to input local endpoint and others explictly. (comments on
 * commit d56b031c)
 *
 * @param m0c the clovis instance we are working with.
 * @return 0 for success, or an -errno code.
 */
static int clovis_net_init(struct m0_clovis *m0c)
{
	struct m0_net_xprt   *xprt;
	struct m0_net_domain *ndom;
	int                   rc;
	char                 *laddr;
	size_t                laddr_len;

	M0_ENTRY();

	M0_PRE(m0c != NULL);
	M0_PRE(m0c->m0c_config != NULL);
	M0_PRE(m0c->m0c_config->cc_local_addr != NULL);

	laddr_len = strlen(m0c->m0c_config->cc_local_addr) + 1;
	laddr = m0_alloc(laddr_len);
	if (laddr == NULL)
		return M0_ERR(-ENOMEM);
	strncpy(laddr, m0c->m0c_config->cc_local_addr, laddr_len);
	m0c->m0c_laddr = laddr;

	m0c->m0c_xprt = &m0_net_lnet_xprt;
	xprt =  m0c->m0c_xprt;
	ndom = &m0c->m0c_ndom;

	rc = m0_net_domain_init(ndom, xprt);
	if (rc != 0) {
		m0c->m0c_laddr = NULL;
		m0_free(laddr);
	}

	return M0_RC(rc);
}

static int clovis_initlift_net(struct m0_sm *mach)
{
	int               rc;
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		rc = clovis_net_init(m0c);
		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else
		clovis_net_fini(m0c);

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

/**
 * This is heavily based on
 * m0t1fs/linux_kernel/super.c::m0t1fs_rpc_fini
 *
 * @param m0c the clovis instance we are working with.
 */
static void clovis_rpc_fini(struct m0_clovis *m0c)
{
	M0_ENTRY();

	M0_PRE(m0c != NULL);

	m0_rpc_machine_fini(&m0c->m0c_rpc_machine);
	M0_SET0(&m0c->m0c_rpc_machine);
	if (m0_reqh_state_get(&m0c->m0c_reqh) != M0_REQH_ST_STOPPED)
		m0_reqh_services_terminate(&m0c->m0c_reqh);
	m0_reqh_fini(&m0c->m0c_reqh);
	M0_SET0(&m0c->m0c_reqh);
	m0_rpc_net_buffer_pool_cleanup(&m0c->m0c_buffer_pool);

	M0_LEAVE();
}

/**
 * This is heavily based on
 *  m0t1fs/linux_kernel/super.c::m0t1fs_rpc_init
 *
 * @param m0c the clovis instance we are working with.
 * @return 0 for success, or an -errno code.
 */
static int clovis_rpc_init(struct m0_clovis *m0c)
{
	struct m0_rpc_machine     *rpc_machine;
	struct m0_reqh            *reqh;
	struct m0_net_domain      *ndom;
	const char                *laddr;
	struct m0_net_buffer_pool *buffer_pool;
	struct m0_net_transfer_mc *tm;
	int                        rc;
	uint32_t                   bufs_nr;
	uint32_t                   tms_nr;

	M0_ENTRY();
	M0_PRE(m0c != NULL);

	rpc_machine = &m0c->m0c_rpc_machine;
	reqh        = &m0c->m0c_reqh;
	ndom        = &m0c->m0c_ndom;
	laddr       =  m0c->m0c_laddr;
	buffer_pool = &m0c->m0c_buffer_pool;

	tms_nr  = 1;
	bufs_nr = m0_rpc_bufs_nr(
			m0c->m0c_config->cc_tm_recv_queue_min_len, tms_nr);

	rc = m0_rpc_net_buffer_pool_setup(ndom, buffer_pool,
					  bufs_nr, tms_nr);
	if (rc != 0)
		goto exit;

	rc = M0_REQH_INIT(reqh,
			  .rhia_dtm = (void*)1,
			  .rhia_db = (void*)1,
			  .rhia_mdstore = (void*)1,
			  .rhia_pc = &m0c->m0c_pools_common,
			  .rhia_fid = &m0c->m0c_process_fid);
	if (rc != 0)
		goto pool_fini;

	rc = m0_rpc_machine_init(rpc_machine, ndom, laddr, reqh,
				 buffer_pool, M0_BUFFER_ANY_COLOUR,
				 m0c->m0c_config->cc_max_rpc_msg_size,
				 m0c->m0c_config->cc_tm_recv_queue_min_len);
	if (rc != 0)
		goto reqh_fini;

	m0_reqh_start(reqh);
	tm = &rpc_machine->rm_tm;
	M0_ASSERT(tm->ntm_recv_pool == buffer_pool);
	return M0_RC(rc);

reqh_fini:
	m0_reqh_fini(reqh);
pool_fini:
	m0_rpc_net_buffer_pool_cleanup(buffer_pool);

exit:
	M0_ASSERT(rc != 0);
	clovis_initlift_fail(rc, m0c);

	return M0_RC(rc);
}

static int clovis_initlift_rpc(struct m0_sm *mach)
{
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP)
		clovis_rpc_init(m0c);
	else
		clovis_rpc_fini(m0c);

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

/**
 * This is heavily based on
 * m0t1fs/linux_kernel/super.c::ast_thread
 */
static void clovis_ast_thread(struct m0_clovis *m0c)
{
	M0_PRE(m0c != NULL);

	while (1) {
		/*
		 * See commit 19b74c444 for details on why it is changed
		 * to m0_chan_wait
		 */
		m0_chan_wait(&m0c->m0c_sm_group.s_clink);
		m0_sm_group_lock(&m0c->m0c_sm_group);
		m0_sm_asts_run(&m0c->m0c_sm_group);
		m0_sm_group_unlock(&m0c->m0c_sm_group);
		if (!m0c->m0c_astthread_active &&
		    m0_atomic64_get(&m0c->m0c_pending_io_nr) == 0) {
			m0_chan_signal_lock(&m0c->m0c_io_wait);
			return;
		}
	}
}

static int clovis_initlift_ast_thread(struct m0_sm *mach)
{
	int               rc;
	struct m0_clovis *m0c;
	struct m0_clink   w;

	M0_ENTRY();
	M0_PRE(mach != NULL);
	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));
	M0_PRE(m0_sm_group_is_locked(&m0c->m0c_sm_group));

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		m0c->m0c_astthread_active = true;
		rc = M0_THREAD_INIT(&m0c->m0c_astthread, struct m0_clovis *,
				    NULL, &clovis_ast_thread, m0c,
				    "clovis:ast");

		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else {
		/* Release the lock so that m0_clink_add_lock can use it */
		m0_sm_group_unlock(&m0c->m0c_sm_group);

		m0_clink_init(&w, NULL);
		m0_clink_add_lock(&m0c->m0c_io_wait, &w);

		m0c->m0c_astthread_active = false;
		m0_chan_signal_lock(&m0c->m0c_sm_group.s_chan);
		m0_chan_wait(&w);
		m0_thread_join(&m0c->m0c_astthread);

		m0_clink_del_lock(&w);
		m0_clink_fini(&w);

		/* Re-acquire the lock */
		m0_sm_group_lock(&m0c->m0c_sm_group);

		M0_SET0(&m0c->m0c_astthread);
	}

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

/** HA service connectivity.*/
static void clovis_ha_process_event(struct m0_clovis              *m0c,
                                    enum m0_conf_ha_process_event  event)
{
	if (M0_FI_ENABLED("no-link"))
		return;

	/*
 	 * Don't post an event if HA link is not connect.
 	 * HA link can be NULL in UT as clovis UT doesn't run through
 	 * all steps of initialisation (for example, those setps involving
 	 * interactions with services such as HA initialisation).
 	 */
	if (m0c->m0c_mero_ha.mh_link == NULL)
		return;

	m0_conf_ha_process_event_post(&m0c->m0c_mero_ha.mh_ha,
	                              m0c->m0c_mero_ha.mh_link,
	                              &m0c->m0c_process_fid,
				      m0_process(), event,
#ifdef __KERNEL__
				      M0_CONF_HA_PROCESS_KERNEL);
#else
				      M0_CONF_HA_PROCESS_OTHER);
#endif
}

/**
 * Establishes rpc session to HA service. The session is set up to be used
 * globally.
 */
static struct m0_mero_ha_cfg  mero_ha_cfg;
static int clovis_ha_init(struct m0_clovis *m0c)
{
	int                    rc;
	struct m0_mero_ha     *mero_ha = &m0c->m0c_mero_ha;

	M0_ENTRY();

	if (M0_FI_ENABLED("skip-ha-init"))
		return 0;

	mero_ha_cfg = (struct m0_mero_ha_cfg){
		.mhc_dispatcher_cfg = {
			.hdc_enable_note      = true,
			.hdc_enable_keepalive = true,
			.hdc_enable_fvec      = true,
		},
		.mhc_addr        = m0c->m0c_config->cc_ha_addr,
		.mhc_rpc_machine = &m0c->m0c_rpc_machine,
		.mhc_reqh        = &m0c->m0c_reqh,
		.mhc_process_fid = m0c->m0c_process_fid,
	};
	rc = m0_mero_ha_init(mero_ha, &mero_ha_cfg);
	M0_ASSERT(rc == 0);

	rc = m0_mero_ha_start(mero_ha);
	M0_ASSERT(rc == 0);

	m0_mero_ha_connect(mero_ha);
	clovis_ha_process_event(m0c, M0_CONF_HA_PROCESS_STARTING);

	return M0_RC(0);
}

/**
 * Clears global HA session info and terminates rpc session to HA service.
 */
static void clovis_ha_fini(struct m0_clovis *m0c)
{
	M0_ENTRY();

	if (M0_FI_ENABLED("skip-ha-fini"))
		return;

	/*
	 * Where is the best place to process HA_STOPPING event? We don't
	 * have umount-like operation as in m0t1fs.
	 */
	clovis_ha_process_event(m0c, M0_CONF_HA_PROCESS_STOPPED);
	m0_mero_ha_disconnect(&m0c->m0c_mero_ha);
	m0_mero_ha_stop(&m0c->m0c_mero_ha);
	m0_mero_ha_fini(&m0c->m0c_mero_ha);
	M0_LEAVE();
}

static int clovis_initlift_ha(struct m0_sm *mach)
{
	int               rc;
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		rc = clovis_ha_init(m0c);
		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else
		clovis_ha_fini(m0c);

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

static bool clovis_rconfc_expired_cb(struct m0_clink *clink)
{
	struct m0_clovis *m0c = M0_AMB(m0c, clink, m0c_conf_exp);
	struct m0_confc  *confc = m0_clovis_confc(m0c);

	M0_PRE(confc != NULL);

	M0_ENTRY();
	if (m0c->m0c_reqh.rh_rconfc.rc_stopping)
		return true;

	m0_mutex_lock(&m0c->m0c_confc_state.cus_lock);
	m0c->m0c_confc_state.cus_state = M0_CC_REVOKED;
	m0_mutex_unlock(&m0c->m0c_confc_state.cus_lock);

	M0_LEAVE();
	return true;
}

static bool clovis_rconfc_ready_cb(struct m0_clink *clink)
{
	struct m0_clovis *m0c = M0_AMB(m0c, clink, m0c_conf_ready);

	M0_ENTRY();
	m0_mutex_lock(&m0c->m0c_confc_state.cus_lock);
	m0c->m0c_confc_state.cus_state = M0_CC_GETTING_READY;
	m0_mutex_unlock(&m0c->m0c_confc_state.cus_lock);
	M0_LEAVE();
	return true;
}

static void clovis_rconfc_fatal_cb(struct m0_rconfc *rconfc)
{
	struct m0_reqh   *reqh = M0_AMB(reqh, rconfc, rh_rconfc);
	struct m0_clovis *m0c = M0_AMB(m0c, reqh, m0c_reqh);

	M0_ENTRY("rconfc %p", rconfc);
	M0_LOG(M0_ERROR, "rconfc encounters fatal error and can't be started");
	m0_mutex_lock(&m0c->m0c_confc_state.cus_lock);
	m0c->m0c_confc_state.cus_state = M0_CC_FAILED;
	m0_mutex_unlock(&m0c->m0c_confc_state.cus_lock);
	M0_LEAVE();
}

static void clovis_io_ref_cb(struct m0_ref *ref)
{
	struct m0_clovis *m0c = M0_AMB(m0c, ref, m0c_ongoing_io);
	struct m0_clink  *pc_clink = &m0c->m0c_pools_common.pc_conf_ready_async;

	M0_ENTRY("clovis %p", m0c);

	m0_mutex_lock(&m0c->m0c_confc_state.cus_lock);
	if (m0_ref_read(ref) == 0 &&
	    m0c->m0c_confc_state.cus_state == M0_CC_GETTING_READY) {
		m0_pool_versions_stale_mark(&m0c->m0c_pools_common,
					    &m0c->m0c_confc_state);
		if (m0c->m0c_pools_common.pc_confc != NULL)
			m0_pools_common_conf_ready_async_cb(pc_clink);

		m0c->m0c_confc_state.cus_state = M0_CC_READY;
	}
	m0_mutex_unlock(&m0c->m0c_confc_state.cus_lock);

	m0_chan_lock(&m0c->m0c_conf_ready_chan);
	m0_chan_broadcast(&m0c->m0c_conf_ready_chan);
	m0_chan_unlock(&m0c->m0c_conf_ready_chan);
}

static bool clovis_confc_ready_async_cb(struct m0_clink *clink)
{
	struct m0_clovis *m0c = M0_AMB(m0c, clink, m0c_conf_ready_async);

	if (m0_ref_read(&m0c->m0c_ongoing_io) == 0 &&
	    m0c->m0c_confc_state.cus_state == M0_CC_GETTING_READY)
		clovis_io_ref_cb(&m0c->m0c_ongoing_io);
	return true;
}

M0_INTERNAL int m0_clovis__io_ref_get(struct m0_clovis *m0c)
{
	struct m0_clink  clink;
	struct m0_confc *confc = m0_clovis_confc(m0c);

	M0_PRE(confc != NULL);
	M0_ENTRY("m0c=%p", m0c);
	while (1) {
		m0_mutex_lock(&m0c->m0c_confc_state.cus_lock);
		if (M0_IN(m0c->m0c_confc_state.cus_state,
			  (M0_CC_READY, M0_CC_FAILED)))
			break;
		m0_clink_init(&clink, NULL);
		m0_clink_add(&m0c->m0c_conf_ready_chan, &clink);

		m0_mutex_unlock(&m0c->m0c_confc_state.cus_lock);
		/* Wait till configuration is updated. */
		m0_chan_wait(&clink);

		m0_clink_del_lock(&clink);
		m0_clink_fini(&clink);
	}
	if (m0c->m0c_confc_state.cus_state == M0_CC_READY)
		m0_ref_get(&m0c->m0c_ongoing_io);
	m0_mutex_unlock(&m0c->m0c_confc_state.cus_lock);
	return M0_RC(m0c->m0c_confc_state.cus_state == M0_CC_READY ?
			0 : -ESTALE);
}

M0_INTERNAL void m0_clovis__io_ref_put(struct m0_clovis *m0c)
{
	struct m0_confc  *confc = m0_clovis_confc(m0c);
	M0_PRE(confc != NULL);

	m0_ref_put(&m0c->m0c_ongoing_io);
	M0_POST(m0_ref_read(&m0c->m0c_ongoing_io) >= 0);
}

static int clovis_confc_init(struct m0_clovis *m0c)
{
	int                   rc;
	struct m0_reqh       *reqh;
	struct m0_confc_args *confc_args;
	struct m0_rconfc     *rconfc;

	confc_args = &(struct m0_confc_args){
		.ca_profile = m0c->m0c_config->cc_profile,
		.ca_rmach   = &m0c->m0c_rpc_machine,
		.ca_group   = &m0c->m0c_sm_group,
	};
	reqh = &m0c->m0c_reqh;
	rconfc = clovis_rconfc(m0c);

	/*
	 * confc needs the ast thread to make progress, and we block
	 * it with this lock.
	 */
	m0_sm_group_unlock(&m0c->m0c_sm_group);

	rc = m0_reqh_conf_setup(reqh, confc_args);
	if (rc != 0)
		goto err_exit;

	m0_clink_init(&m0c->m0c_conf_exp, clovis_rconfc_expired_cb);
	m0_clink_init(&m0c->m0c_conf_ready, clovis_rconfc_ready_cb);
	m0_clink_init(&m0c->m0c_conf_ready_async, clovis_confc_ready_async_cb);
	m0_clink_add_lock(&reqh->rh_conf_cache_exp, &m0c->m0c_conf_exp);
	m0_clink_add_lock(&reqh->rh_conf_cache_ready, &m0c->m0c_conf_ready);
	m0_clink_add_lock(&reqh->rh_conf_cache_ready_async,
			  &m0c->m0c_conf_ready_async);
	m0_ref_init(&m0c->m0c_ongoing_io, 0, clovis_io_ref_cb);

	rc = m0_rconfc_start_sync(rconfc);
	if (rc != 0)
		goto err_rconfc_stop;

	rc = m0_ha_client_add(m0_reqh2confc(reqh));
	if (rc != 0)
		goto err_rconfc_stop;

	rc = m0_confc_root_open(m0_reqh2confc(reqh), &clovis_conf_root);
	if (rc != 0)
		goto err_ha_client_del;

	rc = m0_conf_full_load(clovis_conf_root);
	if (rc != 0)
		goto err_conf_close;

	rc = m0_conf_confc_ha_update(m0_reqh2confc(reqh));
	if (rc != 0)
		goto err_conf_close;

	/* Set clovis_rconfc_fatal_cb */
	m0_rconfc_lock(rconfc);
	m0_rconfc_fatal_cb_set(rconfc, clovis_rconfc_fatal_cb);
	m0_rconfc_unlock(rconfc);

	/* re-acquire the lock */
	m0_sm_group_lock(&m0c->m0c_sm_group);
	return M0_RC(0);

err_conf_close:
	m0_confc_close(&clovis_conf_root->rt_obj);

err_ha_client_del:
	m0_ha_client_del(m0_reqh2confc(reqh));

err_rconfc_stop:
	m0_rconfc_stop_sync(rconfc);
	m0_rconfc_fini(rconfc);
	m0_clink_del_lock(&m0c->m0c_conf_exp);
	m0_clink_del_lock(&m0c->m0c_conf_ready);
	m0_clink_fini(&m0c->m0c_conf_exp);
	m0_clink_fini(&m0c->m0c_conf_ready);
	m0_clink_fini(&m0c->m0c_conf_ready_async);

err_exit:
	/* re-acquire the lock */
	m0_sm_group_lock(&m0c->m0c_sm_group);

	return M0_RC(rc);
}

static void clovis_confc_fini(struct m0_clovis *m0c)
{
	/*
	 * Mero-1580:
	 * clovis_conf_root has to be closed in the shutdown path.
	 * This is needed if clovis init fails for some reason.
	 */
	if (clovis_conf_root) {
		m0_confc_close(&clovis_conf_root->rt_obj);
		clovis_conf_root = NULL;
	}

	m0_sm_group_unlock(&m0c->m0c_sm_group);
	m0_ha_client_del(m0_reqh2confc(&m0c->m0c_reqh));
	m0_rconfc_stop_sync(clovis_rconfc(m0c));
	m0_rconfc_fini(clovis_rconfc(m0c));
	m0_clink_del_lock(&m0c->m0c_conf_exp);
	m0_clink_del_lock(&m0c->m0c_conf_ready);
	m0_clink_del_lock(&m0c->m0c_conf_ready_async);
	m0_clink_fini(&m0c->m0c_conf_exp);
	m0_clink_fini(&m0c->m0c_conf_ready);
	m0_sm_group_lock(&m0c->m0c_sm_group);
	m0_clink_fini(&m0c->m0c_conf_ready_async);
}

static int clovis_initlift_confc(struct m0_sm *mach)
{
	int               rc;
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_PRE(mach != NULL);
	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));
	M0_PRE(m0_sm_group_is_locked(&m0c->m0c_sm_group));

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		rc = clovis_confc_init(m0c);
		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else {
		clovis_confc_fini(m0c);
	}

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

static int clovis_pools_init(struct m0_clovis *m0c)
{
	int                     rc;
	struct m0_pools_common *pools = &m0c->m0c_pools_common;

	m0_sm_group_unlock(&m0c->m0c_sm_group);

	rc = m0_pools_common_init(pools, &m0c->m0c_rpc_machine);
	if (rc != 0)
		goto err_exit;
	M0_ASSERT(ergo(m0c->m0c_config->cc_is_oostore,
		       pools->pc_md_redundancy > 0));

	rc = m0_pools_setup(pools, &m0c->m0c_profile_fid, NULL, NULL);
	if (rc != 0)
		goto err_pools_common_fini;

	rc = m0_pools_service_ctx_create(pools);
	if (rc != 0)
		goto err_pools_destroy;

	m0_pools_common_service_ctx_connect_sync(pools);

	m0_sm_group_lock(&m0c->m0c_sm_group);
	return M0_RC(0);

err_pools_destroy:
	m0_pools_destroy(pools);
err_pools_common_fini:
	m0_pools_common_fini(pools);
err_exit:
	m0_sm_group_lock(&m0c->m0c_sm_group);
	return M0_RC(rc);
}

static void clovis_pools_fini(struct m0_clovis *m0c)
{
	struct m0_pools_common *pools = &m0c->m0c_pools_common;

	m0_pools_service_ctx_destroy(pools);
	m0_pools_destroy(pools);
	m0_pools_common_fini(pools);
}

static int clovis_initlift_pools(struct m0_sm *mach)
{
	int               rc;
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		rc = clovis_pools_init(m0c);
		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else
		clovis_pools_fini(m0c);

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

/* Finds the pool and pool version to use. */
static int clovis_initlift_pool_version(struct m0_sm *mach)
{
	int                     rc;
	struct m0_clovis       *m0c;
	struct m0_pools_common *pools;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));
	pools = &m0c->m0c_pools_common;

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		/* Confc needs the lock to proceed. */
		m0_sm_group_unlock(&m0c->m0c_sm_group);

		rc = m0_pool_versions_setup(pools);
		if (rc != 0) {
			m0_sm_group_lock(&m0c->m0c_sm_group);
			clovis_initlift_fail(rc, m0c);
			goto exit;
		}

		m0_sm_group_lock(&m0c->m0c_sm_group);
	} else /* CLOVIS_SHUTDOWN */
		m0_pool_versions_destroy(pools);

exit:
	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

static int clovis_service_start(struct m0_reqh *reqh, struct m0_fid *sfid,
				struct m0_reqh_service_type *stype,
				struct m0_reqh_service **service)
{
	return m0_reqh_service_setup(service, stype, reqh, NULL, sfid);
}

static int clovis_initlift_resource_manager(struct m0_sm *mach)
{
	int                     rc = 0;
	struct m0_clovis       *m0c;
	struct m0_reqh_service *service;
	struct m0_reqh         *reqh;
	struct m0_fid           sfid;
	struct m0_fid           fake_pfid
		= M0_FID_TINIT(M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 0, 0);

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));
	reqh = &m0c->m0c_reqh;

	/*
	 * @todo Use fake process fid for init script based cluster setup.
	 *       Init script based setup not adding clovis process
	 *       to configuration and also not passing clovis process
	 *       FID to mount.
	 */
	if (!m0_fid_eq(&reqh->rh_fid, &fake_pfid)) {

		/* Confc needs the lock to proceed. */
		m0_sm_group_unlock(&m0c->m0c_sm_group);
		rc = m0_conf_process2service_get(&reqh->rh_rconfc.rc_confc,
						 &reqh->rh_fid, M0_CST_RMS,
						 &sfid);
		m0_sm_group_lock(&m0c->m0c_sm_group);
		if (rc != 0) {
			clovis_initlift_fail(rc, m0c);
			goto exit;
		}
	} else {
		m0_uuid_generate((struct m0_uint128 *)&sfid);
		m0_fid_tassume(&sfid, &M0_CONF_SERVICE_TYPE.cot_ftype);
	}

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		rc = clovis_service_start(reqh, &sfid, &m0_rms_type, &service);
		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else {
		/* reqh_services_terminate is handled by rpc_fini.
		 * Reqh services are terminated not in reverse order because
		 * m0_reqh_services_terminate() terminates all services
		 * including rpc_service. Rpc_service starts in
		 * clovis_rpc_init() implicitly.
		 */
	}

exit:
	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

static int clovis_initlift_layouts(struct m0_sm *mach)
{
	int               rc;
	struct m0_clovis *m0c;
	struct m0_reqh   *reqh;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));
	reqh = &m0c->m0c_reqh;

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		rc = m0_reqh_mdpool_layout_build(reqh);
		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else  {
		m0_reqh_layouts_cleanup(reqh);
	}

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

static int clovis_initlift_idx_service(struct m0_sm *mach)
{
	int                               rc = 0;
	struct m0_clovis                 *m0c;
	struct m0_clovis_idx_service     *service;
	struct m0_clovis_idx_service_ctx *ctx;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));
	ctx     = &m0c->m0c_idx_svc_ctx;

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		m0_clovis_idx_service_config(m0c,
			m0c->m0c_config->cc_idx_service_id,
			m0c->m0c_config->cc_idx_service_conf);
		service = ctx->isc_service;
		M0_ASSERT(service != NULL &&
			  service->is_svc_ops != NULL &&
			  service->is_svc_ops->iso_init != NULL);

		/* Confc needs the lock to proceed. */
		m0_sm_group_unlock(&m0c->m0c_sm_group);
		rc = service->is_svc_ops->iso_init((void *)ctx);
		m0_sm_group_lock(&m0c->m0c_sm_group);

		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else {
		service = ctx->isc_service;
		M0_ASSERT(service != NULL &&
			  service->is_svc_ops != NULL &&
			  service->is_svc_ops->iso_fini != NULL);
		service->is_svc_ops->iso_fini((void *)ctx);
	}

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

/* Default root fid and name_len for oostore mode */
#ifdef CLOVIS_FOR_M0T1FS

M0_INTERNAL const struct m0_fid M0_CLOVIS_ROOT_FID = {
	.f_container = 1ULL,
	.f_key       = 1ULL
};

#define M0_CLOVIS_M0T1FS_NAME_LEN (256)

#endif

/**
 * Lookup the fid of the root filesystem. This is a m0t1fsism.
 *
 * @param m0c The clovis instance we are working with.
 * @return 0 for success, an error code otherwise.
 */
static int clovis_rootfid_lookup(struct m0_clovis  *m0c)
{
#ifdef CLOVIS_FOR_M0T1FS
	int                         rc;
	struct m0_fop              *fop;
	struct m0_rpc_session      *session;
	struct m0_fop_statfs_rep   *statfs_rep;
	struct m0_fop_statfs       *statfs;
	struct m0_fop              *rep_fop = NULL;
	struct m0_reqh_service_ctx *ctx;
	struct m0_pools_common     *pc;

	M0_ENTRY();
	M0_PRE(m0c != NULL);
	M0_PRE(m0_sm_group_is_locked(&m0c->m0c_sm_group));

	/*
	 * As the root fid in oostore mode is always set to be M0_ROOT_FID,
	 * no neeed to send a request to mds to query it.
	 */
	if (m0c->m0c_config->cc_is_oostore) {
		m0c->m0c_namelen = M0_CLOVIS_M0T1FS_NAME_LEN;
		m0c->m0c_root_fid = M0_CLOVIS_ROOT_FID;
		return M0_RC(0);
	}

	/* Layout data is stored on mds0 */
	pc = &m0c->m0c_pools_common;
	ctx = pc->pc_mds_map[0];
	M0_ASSERT(ctx != NULL);

	/* Wait till the connection is ready. */
	m0_reqh_service_connect_wait(ctx);
	session = &ctx->sc_rlink.rlk_sess;
	M0_ASSERT(session != NULL);

	/* Allocate a fop */
	fop = m0_fop_alloc_at(session, &m0_fop_statfs_fopt);
	if (fop == NULL) {
		rc = -ENOMEM;
		M0_LOG(M0_ERROR, "m0_fop_alloc() failed with %d", rc);
		return M0_RC(rc);
	}

	/* populate the request values */
	statfs = m0_fop_data(fop);
	statfs->f_flags = 0;

	rc = m0_rpc_post_sync(fop, session, NULL, 0 /* deadline */);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "m0_rpc_post_sync(%x) failed with %d",
		       m0_fop_opcode(fop), rc);
		goto out;
	}

	/* Retrieve and unpack the reply */
	rep_fop = m0_rpc_item_to_fop(fop->f_item.ri_reply);
	m0_fop_get(rep_fop);
	statfs_rep = m0_fop_data(rep_fop);
	rc = statfs_rep->f_rc;
	if (rc != 0) {
		M0_LOG(M0_ERROR, "remote statfs fop (%x) failed with %d",
		       m0_fop_opcode(fop), rc);
		goto out;
	}

	/* Unpack the reply */
	m0c->m0c_namelen = statfs_rep->f_namelen;
	M0_LOG(M0_DEBUG, "Got mdservice root "FID_F,
	       FID_P(&statfs_rep->f_root));

	m0c->m0c_root_fid = statfs_rep->f_root;

out:
	/* We are done with the reply */
	if (rep_fop != NULL)
		m0_fop_put0_lock(rep_fop);

	/* We are done with the request */
	m0_fop_put0_lock(fop);

	return M0_RC(rc);
#endif
}

static int clovis_initlift_rootfid(struct m0_sm *mach)
{
	int               rc;
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
		rc = clovis_rootfid_lookup(m0c);
		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else {
		m0c->m0c_namelen = 0;
		M0_SET0(&m0c->m0c_root_fid);
	}

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

static int clovis_initlift_addb2(struct m0_sm *mach)
{
	int                  rc = 0;
	struct m0_clovis    *m0c;
	struct m0_addb2_sys *sys = m0_addb2_global_get();

	M0_ENTRY();
	M0_PRE(mach != NULL);

	m0c = bob_of(mach, struct m0_clovis, m0c_initlift_sm, &m0c_bobtype);
	M0_ASSERT(clovis_m0c_invariant(m0c));

	if (M0_FI_ENABLED("no-addb2"))
		return M0_RC(clovis_initlift_get_next_floor(m0c));

	if (m0c->m0c_initlift_direction == CLOVIS_STARTUP) {
#ifndef __KERNEL__
		/*
		 * When reaching here, m0t1fs has started ADDB 'sys' if clovis
		 * is in kernel mode. TODO: clovis is the 'only' interface
		 * mero and m0t1fs is built on top of clovis in the future. We
		 * will sort this out later.
		 */
		m0_addb2_sys_sm_start(m0_addb2_global_get());
		rc = m0_addb2_sys_net_start(m0_addb2_global_get());
		if (rc != 0) {
			clovis_initlift_fail(rc, m0c);
			return M0_RC(clovis_initlift_get_next_floor(m0c));
		}
#endif

		rc = m0_addb2_sys_net_start_with(
			sys, &m0c->m0c_pools_common.pc_svc_ctxs);

		if (rc != 0)
			clovis_initlift_fail(rc, m0c);
	} else {
#ifndef __KERNEL__
		m0_addb2_sys_net_stop(m0_addb2_global_get());
		m0_addb2_sys_sm_stop(m0_addb2_global_get());
#endif
	}

	return M0_RC(clovis_initlift_get_next_floor(m0c));
}

M0_INTERNAL void m0_clovis_global_fini(void)
{
	M0_ENTRY();

	m0_sm_addb2_fini(&io_sm_conf);
	m0_sm_addb2_fini(&clovis_op_conf);

	m0_sm_conf_fini(&io_sm_conf);
	m0_sm_conf_fini(&clovis_initlift_conf);
	m0_sm_conf_fini(&clovis_op_conf);
	m0_sm_conf_fini(&clovis_entity_conf);
	m0_semaphore_fini(&clovis_cpus_sem);

	M0_LEAVE();
}

/**
 * Retrieves the number of online CPUs
 */
#ifndef __KERNEL__
#include <unistd.h>
#endif
static int clovis_get_online_cpus(void)
{
	int cpus;

#ifdef __KERNEL__
	cpus = num_online_cpus();
#else
	cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif

	return (cpus / 2) ?: 1;
}

M0_INTERNAL int m0_clovis_global_init(void)
{
	int cpus;
	int rc;

	M0_ENTRY();

	m0_sm_conf_init(&clovis_initlift_conf);
	m0_sm_conf_init(&clovis_op_conf);
	m0_sm_conf_init(&clovis_entity_conf);
	m0_sm_conf_init(&io_sm_conf);

	/*
	 * Limit the number of concurrent parity calculations
	 * to avoid starving other threads (especially LNet) out.
	 *
	 * Note: the exact threshold number may come from configuration
	 * database later where it can be specified per-node.
	 */
	cpus = clovis_get_online_cpus();
	M0_LOG(M0_INFO, "mero: max CPUs for parity calcs: %d\n", cpus);
	m0_semaphore_init(&clovis_cpus_sem, cpus);

	m0_clovis_idx_services_register();

	rc = m0_sm_addb2_init(&clovis_op_conf,
			      M0_AVI_CLOVIS_SM_OP,
			      M0_AVI_CLOVIS_SM_OP_COUNTER) ?:
		m0_sm_addb2_init(&io_sm_conf,
				 M0_AVI_CLOVIS_IOO_REQ,
				 M0_AVI_CLOVIS_IOO_REQ_COUNTER);

	return M0_RC(rc);
}

#define NOT_EMPTY(x) (x != NULL && *x != '\0')

static struct m0 m0_clovis_mero_instance;
int m0_clovis_init(struct m0_clovis **m0c_p,
		   struct m0_clovis_config *conf, bool init_m0)
{
	int               rc;
	struct m0_clovis *m0c;

	M0_PRE(m0c_p != NULL);
	M0_PRE(*m0c_p == NULL);
	M0_PRE(conf != NULL);
	M0_PRE(NOT_EMPTY(conf->cc_local_addr));
	M0_PRE(NOT_EMPTY(conf->cc_ha_addr));
	M0_PRE(NOT_EMPTY(conf->cc_profile));
	M0_PRE(NOT_EMPTY(conf->cc_process_fid));
	M0_PRE(conf->cc_tm_recv_queue_min_len != 0);
	M0_PRE(conf->cc_max_rpc_msg_size != 0);

	/* Initialise Mero if needed at the very beginning. */
#ifndef __KERNEL__
	if (init_m0) {
		M0_SET0(&m0_clovis_mero_instance);
		rc = m0_init(&m0_clovis_mero_instance);
		if (rc != 0)
			return M0_RC(rc);
	}
#endif

	/* Can't call M0_ENTRY before m0 is initialised. */
	M0_ENTRY();

	/* initialise this m0_clovis instance */
	M0_ALLOC_PTR(m0c);
	if (m0c == NULL)
		return M0_ERR(-ENOMEM);
	m0_clovis_bob_init(m0c);

	/* Set configuration parameters */
	m0c->m0c_config = conf;

	/* Parse some parameters in m0c_config for future uses. */
	rc = clovis_fid_sscanf(conf->cc_process_fid, &m0c->m0c_process_fid,
	                       "process fid");
	if (rc != 0)
		goto err_exit;

	rc = clovis_fid_sscanf(conf->cc_profile, &m0c->m0c_profile_fid,
	                       "profile");
	if (rc != 0)
		goto err_exit;

	/* Set mero instance. */
	m0c->m0c_mero = &m0_clovis_mero_instance;

	/* Initialise configuration state lock */
	m0_mutex_init(&m0c->m0c_confc_state.cus_lock);
	m0_chan_init(&m0c->m0c_conf_ready_chan, &m0c->m0c_confc_state.cus_lock);

	/*
	 * Initialise those types specific to IO operations, this is needed
	 * to get rootfid in the Clovis initlift trip.
	 */
	m0_clovis_init_io_op();

	/* Initialise state machine group */
	m0_sm_group_init(&m0c->m0c_sm_group);
	m0_chan_init(&m0c->m0c_io_wait, &m0c->m0c_sm_group.s_lock);

	/* Move the initlift in its direction of travel */
	m0_sm_group_lock(&m0c->m0c_sm_group);

	m0_sm_init(&m0c->m0c_initlift_sm, &clovis_initlift_conf,
		   CLOVIS_IL_UNINITIALISED, &m0c->m0c_sm_group);
	m0c->m0c_initlift_direction = CLOVIS_STARTUP;
	clovis_initlift_move_next_floor(m0c);

	/*
	 * If Clovis initialisation successes, close the configuration fs
	 * object(cached) now as it may become invalid at some point of time.
	 */
	rc = m0c->m0c_initlift_rc;
	if (rc == 0) {
		m0_confc_close(&clovis_conf_root->rt_obj);
		clovis_conf_root = NULL;

		/*
		 * m0t1fs posts the `STARTED` event during `mount`, clovis
		 * doesn't have corresponding `mount` operation, instead
		 * clovis posts this event when all initialisation steps
		 * are completed successfully.
		 */
		clovis_ha_process_event(m0c, M0_CONF_HA_PROCESS_STARTED);
	}

	m0_sm_group_unlock(&m0c->m0c_sm_group);

	/* Check the stashed first failure (may be 0) */
	if (rc != 0)
		goto err_exit;

	/* Do post checks if all initialisation steps success. */
	M0_POST(m0_sm_conf_is_initialized(&clovis_op_conf));
	M0_POST(m0_sm_conf_is_initialized(&clovis_entity_conf));
	/* This is a sanity test, not a side-effect of m0_clovis_init */
	M0_POST(m0_uint128_cmp(&M0_CLOVIS_UBER_REALM,
			       &M0_CLOVIS_ID_APP) < 0);

	/* Extra initialisation work for composite layout. */
	m0_clovis__composite_container_init(m0c);

	/* Init the hash-table for RM contexts */
	rm_ctx_htable_init(&m0c->m0c_rm_ctxs, M0_CLOVIS_RM_HBUCKET_NR);

	if (conf->cc_is_addb_init) {
		char buf[64];
		sprintf(buf, "linuxstob:./clovis_addb_%d", (int)m0_pid());
		rc = m0_reqh_addb2_init(&m0c->m0c_reqh, buf,
					0xaddbf11e, true, true);
	}
	/* publish the allocated clovis instance */
	*m0c_p = m0c;
	return M0_RC(rc);

err_exit:
	m0_free(m0c);
#ifndef __KERNEL__
	if (init_m0)
		m0_fini();
#endif
	return M0_RC(rc);

}
M0_EXPORTED(m0_clovis_init);

void m0_clovis_fini(struct m0_clovis *m0c, bool fini_m0)
{
	M0_ENTRY();

	M0_PRE(m0_sm_conf_is_initialized(&clovis_op_conf));
	M0_PRE(m0_sm_conf_is_initialized(&clovis_entity_conf));
	M0_PRE(m0c != NULL);

	if (m0c->m0c_config->cc_is_addb_init) {
		m0_addb2_force_all();
		m0_reqh_addb2_fini(&m0c->m0c_reqh);
	}

	/* Finalize hash-table for RM contexts */
	rm_ctx_htable_fini(&m0c->m0c_rm_ctxs);

	/* shut down this clovis instance */
	m0_sm_group_lock(&m0c->m0c_sm_group);

	/*
	 * As explained in m0_clovis_init(), clovis doesn't have `umount`
	 * so it posts the `STOPPING` event here.
	 */
	clovis_ha_process_event(m0c, M0_CONF_HA_PROCESS_STOPPING);

	m0c->m0c_initlift_direction = CLOVIS_SHUTDOWN;
	if (m0c->m0c_initlift_sm.sm_state == CLOVIS_IL_INITIALISED)
		/* Kick the initlift in its direction of travel */
		clovis_initlift_move_next_floor(m0c);

	m0_sm_group_unlock(&m0c->m0c_sm_group);
	m0_chan_fini_lock(&m0c->m0c_io_wait);

	m0_chan_fini_lock(&m0c->m0c_conf_ready_chan);
	m0_mutex_fini(&m0c->m0c_confc_state.cus_lock);


	/* finalise the m0_clovis instance */
	m0_sm_group_fini(&m0c->m0c_sm_group);

	m0_clovis_bob_fini(m0c);
#ifndef __KERNEL__
	if (fini_m0)
		m0_fini();
#endif
	m0_free(m0c);

	M0_LEAVE();
}
M0_EXPORTED(m0_clovis_fini);

void m0_clovis_process_fid(const struct m0_clovis *m0c,
			   struct m0_fid *proc_fid)
{
	M0_PRE(m0c != NULL && proc_fid != NULL);

	/* TODO: check if the process fid is a valid one for clovis. */
	*proc_fid = m0c->m0c_process_fid;
}
M0_EXPORTED(m0_clovis_process_fid);

#ifdef REALM_SUPPORTED
/* for now, this only lets you open the uber realm for an instance */
void m0_clovis_realm_open(struct m0_clovis_realm   *realm,
			  uint64_t wcount, uint64_t rcount,
			  struct m0_clovis_op **op)
{
	/* TODO: how to set the id of the realm to open?
	 *       if not, why does the uber realm have one? */
}
#endif

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
