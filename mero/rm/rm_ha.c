/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 1-Oct-2015
 */


#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RM
#include "lib/trace.h"

#include "lib/finject.h"    /* M0_FI_ENABLED */
#include "lib/errno.h"
#include "lib/memory.h"     /* m0_free */
#include "lib/misc.h"
#include "lib/string.h"     /* m0_strdup */
#include "sm/sm.h"
#include "conf/confc.h"
#include "conf/helpers.h"   /* m0_confc2reqh */
#include "conf/diter.h"
#include "conf/obj_ops.h"   /* m0_conf_obj_get_lock */
#include "reqh/reqh.h"      /* m0_reqh */
#include "rm/rm_ha.h"

/**
 * @addtogroup rm-ha
 *
 * @{
 */

static struct m0_sm_state_descr rm_ha_subscriber_states[] = {
	[RM_HA_SBSCR_INIT] = {
		.sd_flags   = M0_SDF_INITIAL,
		.sd_name    = "Initial",
		.sd_allowed = M0_BITS(RM_HA_SBSCR_FS_OPEN)
	},
	[RM_HA_SBSCR_FS_OPEN] =  {
		.sd_name    = "Open_configuration",
		.sd_allowed = M0_BITS(RM_HA_SBSCR_FAILURE,
				      RM_HA_SBSCR_DITER_SEARCH)
	},
	[RM_HA_SBSCR_DITER_SEARCH] = {
		.sd_name    = "Diter_search",
		.sd_allowed = M0_BITS(RM_HA_SBSCR_FAILURE, RM_HA_SBSCR_FINAL)
	},
	[RM_HA_SBSCR_FAILURE] = {
		.sd_flags   = M0_SDF_FAILURE | M0_SDF_FINAL,
		.sd_name    = "Failure",
		.sd_allowed = 0
	},
	[RM_HA_SBSCR_FINAL] = {
		.sd_flags   = M0_SDF_TERMINAL,
		.sd_name    = "Final",
		.sd_allowed = 0
	}
};

static const struct m0_sm_conf rm_ha_sbscr_sm_conf = {
	.scf_name      = "rm_ha_subscriber",
	.scf_nr_states = ARRAY_SIZE(rm_ha_subscriber_states),
	.scf_state     = rm_ha_subscriber_states
};

static void rm_ha_sbscr_state_set(struct m0_rm_ha_subscriber *sbscr, int state)
{
        M0_LOG(M0_DEBUG, "sbscr: %p, state change:[%s -> %s]",
	       sbscr, m0_sm_state_name(&sbscr->rhs_sm, sbscr->rhs_sm.sm_state),
	       m0_sm_state_name(&sbscr->rhs_sm, state));
	m0_sm_state_set(&sbscr->rhs_sm, state);
}

static void rm_ha_sbscr_fail(struct m0_rm_ha_subscriber *sbscr, int rc)
{
        M0_LOG(M0_DEBUG, "sbscr: %p, state %s failed with %d", sbscr,
	       m0_sm_state_name(&sbscr->rhs_sm, sbscr->rhs_sm.sm_state), rc);
	m0_sm_fail(&sbscr->rhs_sm, RM_HA_SBSCR_FAILURE, rc);
}

static void rm_ha_sbscr_ast_post(struct m0_rm_ha_subscriber *sbscr,
				 void (*cb)(struct m0_sm_group *,
					    struct m0_sm_ast *))
{
	sbscr->rhs_ast.sa_cb    = cb;
	sbscr->rhs_ast.sa_datum = sbscr;
	m0_sm_ast_post(sbscr->rhs_sm.sm_grp, &sbscr->rhs_ast);
}

static bool rm_ha_rms_is_located(struct m0_conf_obj         *next,
				 struct m0_rm_ha_subscriber *sbscr)
{
	struct m0_conf_service *svc;
	const char            **ep;

	/* looking for rmservice having the given endpoint */
	M0_ASSERT(m0_conf_obj_type(next) == &M0_CONF_SERVICE_TYPE);
	svc = M0_CONF_CAST(next, m0_conf_service);
	if (svc->cs_type == M0_CST_RMS) {
		ep = svc->cs_endpoints;
		while (*ep != NULL) {
			if (m0_streq(*ep, sbscr->rhs_tracker->rht_ep)) {
				return true;
			}
			++ep;
		}
	}
	return false;
}

static bool rm_ha_svc_filter(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE;
}

static void rm_ha_sbscr_diter_next(struct m0_rm_ha_subscriber *sbscr)
{
	struct m0_conf_obj *next;
	int                 rc;
	bool                found = false;

	while ((rc = m0_conf_diter_next(&sbscr->rhs_diter, rm_ha_svc_filter)) ==
			M0_CONF_DIRNEXT) {
		next = m0_conf_diter_result(&sbscr->rhs_diter);
		if (rm_ha_rms_is_located(next, sbscr) ||
		    M0_FI_ENABLED("subscribe")) {
			m0_clink_add_lock(
				     &m0_conf_obj2reqh(next)->rh_conf_cache_exp,
				     &sbscr->rhs_tracker->rht_conf_exp);
			/**
			 * @todo What if obj is already in M0_NC_FAILED state?
			 * We should check it somewhere.
			 */
			m0_conf_cache_lock(next->co_cache);
			m0_clink_add(&next->co_ha_chan,
					  &sbscr->rhs_tracker->rht_clink);
			m0_conf_obj_get(next);
			m0_conf_cache_unlock(next->co_cache);
			found = true;
			break;
		}
        }

	if (rc == M0_CONF_DIREND) {
		M0_ASSERT(!found);
		/*
		 * No M0_ERR() to not flood the log, because as for now
		 * RM services on clients (m0t1fs) are not added to conf.
		 */
		rc = -ENOENT;
	}

	if (rc < 0 || found) {
		m0_clink_del_lock(&sbscr->rhs_clink);
		m0_clink_fini(&sbscr->rhs_clink);
		m0_confc_close(sbscr->rhs_dir_root);
		m0_conf_diter_fini(&sbscr->rhs_diter);
		if (found)
			rm_ha_sbscr_state_set(sbscr, RM_HA_SBSCR_FINAL);
		else
			rm_ha_sbscr_fail(sbscr, rc);
	} else {
		/*
		 * Directory entry is absent in conf cache. Once it is retrieved
		 * from confd, rm_ha_diter_cb() will be called.
		 */
		M0_ASSERT(rc == M0_CONF_DIRMISS);
	}
}

static void rm_ha_sbscr_diter_next_ast(struct m0_sm_group *grp,
				     struct m0_sm_ast   *ast)
{
	struct m0_rm_ha_subscriber *sbscr = ast->sa_datum;

	rm_ha_sbscr_diter_next(sbscr);
}

static bool rm_ha_diter_cb(struct m0_clink *clink)
{
	struct m0_rm_ha_subscriber *sbscr;

	sbscr = container_of(clink, struct m0_rm_ha_subscriber, rhs_clink);
	rm_ha_sbscr_ast_post(sbscr, rm_ha_sbscr_diter_next_ast);
	return true;
}

static void rm_ha_sbscr_fs_opened(struct m0_sm_group *grp,
				  struct m0_sm_ast   *ast)
{
	struct m0_rm_ha_subscriber *sbscr = ast->sa_datum;
	struct m0_confc_ctx        *cctx = &sbscr->rhs_cctx;
	int                         rc;

	rc = m0_confc_ctx_error_lock(cctx);
	if (rc == 0) {
		sbscr->rhs_dir_root = m0_confc_ctx_result(&sbscr->rhs_cctx);
		rc = m0_conf_diter_init(&sbscr->rhs_diter, sbscr->rhs_confc,
					sbscr->rhs_dir_root,
					M0_CONF_ROOT_NODES_FID,
					M0_CONF_NODE_PROCESSES_FID,
					M0_CONF_PROCESS_SERVICES_FID);
		if (rc == 0) {
			rm_ha_sbscr_state_set(sbscr, RM_HA_SBSCR_DITER_SEARCH);
			m0_clink_init(&sbscr->rhs_clink, rm_ha_diter_cb);
			m0_conf_diter_wait_arm(&sbscr->rhs_diter,
					       &sbscr->rhs_clink);
			rm_ha_sbscr_diter_next(sbscr);
		} else {
			m0_confc_close(sbscr->rhs_dir_root);
		}
	}
	m0_confc_ctx_fini(cctx);

	if (rc != 0)
		rm_ha_sbscr_fail(sbscr, rc);
}

static bool rm_ha_sbscr_fs_open_cb(struct m0_clink *link)
{
	struct m0_rm_ha_subscriber *sbscr;
	bool                        confc_is_ready;

	sbscr = container_of(link, struct m0_rm_ha_subscriber, rhs_clink);
	confc_is_ready = m0_confc_ctx_is_completed(&sbscr->rhs_cctx);
	M0_LOG(M0_DEBUG, "subscriber=%p confc_ready=%d",
	                  sbscr, !!confc_is_ready);
	if (confc_is_ready) {
		m0_clink_del(&sbscr->rhs_clink);
		m0_clink_fini(&sbscr->rhs_clink);
		rm_ha_sbscr_ast_post(sbscr, rm_ha_sbscr_fs_opened);
	}
	return true;
}

static void rm_ha_conf_open(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_rm_ha_subscriber *sbscr = ast->sa_datum;
	struct m0_confc            *confc = sbscr->rhs_confc;
	struct m0_confc_ctx        *cctx = &sbscr->rhs_cctx;
	int                         rc;

	rc = m0_confc_ctx_init(cctx, confc);
	if (rc == 0) {
		rm_ha_sbscr_state_set(sbscr, RM_HA_SBSCR_FS_OPEN);
		m0_clink_init(&sbscr->rhs_clink, rm_ha_sbscr_fs_open_cb);
		m0_clink_add_lock(&cctx->fc_mach.sm_chan, &sbscr->rhs_clink);
		m0_confc_open(cctx, confc->cc_root, M0_FID0);
	} else {
		rm_ha_sbscr_fail(sbscr, M0_ERR(rc));
	}

	M0_RC_INFO(rc, "subscriber=%p", sbscr);
}

/**
 * Helper function to locate RMS conf object with the given endpoint.
 */
static int rm_remote_ep_to_rms_obj(struct m0_confc     *confc,
				   const char          *rem_ep,
				   struct m0_conf_obj **obj)
{
	struct m0_conf_obj     *next;
	struct m0_conf_obj     *root;
	struct m0_conf_diter    it;
	struct m0_conf_service *svc;
	const char            **ep;
	bool                    found = false;
	int                     rc;

	M0_PRE(confc != NULL);
	M0_PRE(rem_ep != NULL);

	rc = m0_confc_open_sync(&root, confc->cc_root, M0_FID0);
	if (rc != 0)
		return M0_RC(rc);

	M0_ASSERT(root != NULL && root->co_status == M0_CS_READY);

	rc = m0_conf_diter_init(&it, confc, root,
				M0_CONF_ROOT_NODES_FID,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0)
		return M0_ERR(rc);

	while (!found) {
		rc = m0_conf_diter_next_sync(&it, rm_ha_svc_filter);
		if(rc != M0_CONF_DIRNEXT)
			break;
		next = m0_conf_diter_result(&it);
		svc = M0_CONF_CAST(next, m0_conf_service);
		if (svc->cs_type !=  M0_CST_RMS)
			continue;
		ep = svc->cs_endpoints;
		while (*ep != NULL) {
			if (m0_streq(*ep, rem_ep)) {
				/*
				 * we need the object guaranteed
				 * to be pinned on return, because it is used
				 * outside of function.
				 */
				m0_conf_obj_get_lock(next);
				*obj = next;
				found = true;
				break;
			}
			++ep;
		}
	}
	m0_conf_diter_fini(&it);
	m0_confc_close(root);

	return found ? 0 : M0_RC(rc) ?: M0_ERR(-ENOENT);
}

static bool rm_ha_conf_expired_cb(struct m0_clink *cl)
{
	struct m0_rm_ha_tracker *tracker = M0_AMB(tracker, cl, rht_conf_exp);

	M0_ENTRY("tracker %p", tracker);
	m0_rm_ha_unsubscribe_lock(tracker);
	M0_LEAVE();
	return true;
}

M0_INTERNAL int m0_rm_ha_subscriber_init(struct m0_rm_ha_subscriber *sbscr,
					 struct m0_sm_group         *grp,
					 struct m0_confc            *confc,
					 const char                 *rem_ep,
					 struct m0_rm_ha_tracker    *tracker)
{
	tracker->rht_ep = m0_strdup(rem_ep);
	if (tracker->rht_ep == NULL)
		return M0_ERR(-ENOMEM);
	sbscr->rhs_tracker = tracker;
	sbscr->rhs_confc = confc;
	m0_sm_init(&sbscr->rhs_sm, &rm_ha_sbscr_sm_conf, RM_HA_SBSCR_INIT, grp);
	return M0_RC(0);
}

M0_INTERNAL void m0_rm_ha_subscribe(struct m0_rm_ha_subscriber *sbscr)
{
	rm_ha_sbscr_ast_post(sbscr, rm_ha_conf_open);
}

M0_INTERNAL int m0_rm_ha_subscribe_sync(struct m0_confc         *confc,
					const char              *rem_ep,
					struct m0_rm_ha_tracker *tracker)
{
	struct m0_conf_obj *obj;
	int                 rc;

	M0_ASSERT(confc != NULL);
	M0_ASSERT(rem_ep != NULL);

	/**
	 * @todo Ideally this function should be implemented through
	 * asynchronous m0_rm_ha_subscribe(). The problem is that
	 * m0_rm_ha_subscriber internally locks confc sm group. For global confc
	 * it is locality0 sm group. So this group can't be provided to
	 * m0_rm_ha_subscriber_init(). Usually users requesting synchronous
	 * operation don't have another option, except locality0 sm group.
	 */
	tracker->rht_ep = m0_strdup(rem_ep);
	if (tracker->rht_ep == NULL)
		return M0_ERR(-ENOMEM);
	rc = rm_remote_ep_to_rms_obj(confc, rem_ep, &obj);
	if (rc == 0) {
		m0_clink_add_lock(&m0_conf_obj2reqh(obj)->rh_conf_cache_exp,
				  &tracker->rht_conf_exp);
		/**
		 * @todo What if remote is already in M0_NC_FAILED state?
		 * We should check it somewhere.
		 */
		m0_clink_add_lock(&obj->co_ha_chan, &tracker->rht_clink);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_rm_ha_subscriber_fini(struct m0_rm_ha_subscriber *sbscr)
{
	m0_sm_fini(&sbscr->rhs_sm);
}

M0_INTERNAL void m0_rm_ha_tracker_init(struct m0_rm_ha_tracker *tracker,
				       m0_chan_cb_t             cb)
{
	m0_clink_init(&tracker->rht_clink, cb);
	m0_clink_init(&tracker->rht_conf_exp, rm_ha_conf_expired_cb);
	tracker->rht_ep = NULL;
	tracker->rht_state = M0_NC_UNKNOWN;
}

M0_INTERNAL void m0_rm_ha_tracker_fini(struct m0_rm_ha_tracker *tracker)
{
	m0_clink_fini(&tracker->rht_clink);
	m0_clink_fini(&tracker->rht_conf_exp);
	m0_free(tracker->rht_ep);
}

M0_INTERNAL void m0_rm_ha_unsubscribe(struct m0_rm_ha_tracker *tracker)
{
	struct m0_conf_obj *obj;

	if (m0_clink_is_armed(&tracker->rht_conf_exp))
		m0_clink_cleanup_locked(&tracker->rht_conf_exp);
	if (m0_clink_is_armed(&tracker->rht_clink)) {
		obj = M0_AMB(obj, tracker->rht_clink.cl_chan, co_ha_chan);
		m0_clink_cleanup_locked(&tracker->rht_clink);
		/**
		 * The object may be a fake one. Need to put only "real"
		 * configuration object @see _confc_phony_cache_append()
		 */
		if (obj->co_parent != NULL)
			m0_conf_obj_put(obj);
	}
	tracker->rht_clink.cl_chan = NULL;
}

M0_INTERNAL void m0_rm_ha_unsubscribe_lock(struct m0_rm_ha_tracker *tracker)
{
	struct m0_chan *ch = tracker->rht_clink.cl_chan;

	if (ch != NULL) {
		m0_chan_lock(ch);
		m0_rm_ha_unsubscribe(tracker);
		m0_chan_unlock(ch);
	}
}

/** @} end of rm-ha group */
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
