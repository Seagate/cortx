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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>
 * Original creation date: 08/08/2016
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIXCM
#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/trace.h"
#include "lib/chan.h"

#include "conf/helpers.h"     /* m0_conf_service_get */
#include "fop/fom.h"
#include "reqh/reqh.h"
#include "rpc/rpc_machine.h"
#include "pool/pool.h"
#include "cm/proxy.h"

#include "dix/fid_convert.h"
#include "dix/cm/cp.h"
#include "dix/cm/cm.h"
#include "dix/cm/iter.h"

/**
  @page DIXCMDLD DIX copy machine DLD
  - @ref DIXCMDLD-ovw
  - @ref DIXCMDLD-def
  - @ref DIXCMDLD-req
  - @ref DIXCMDLD-depends
  - @ref DIXCMDLD-highlights
  - @subpage DIXCMDLD-fspec
  - @ref DIXCMDLD-lspec
     - @ref DIXCMDLD-lspec-cm-setup
     - @ref DIXCMDLD-lspec-cm-prepare
     - @ref DIXCMDLD-lspec-cm-start
     - @ref DIXCMDLD-lspec-cm-data-next
     - @ref DIXCMDLD-lspec-cm-sliding-window
     - @ref DIXCMDLD-lspec-cm-stop
  - @ref DIXCMDLD-conformance
  - @ref DIXCMDLD-ut
  - @ref DIXCMDLD-st
  - @ref DIXCMDLD-O
  - @ref DIXCMDLD-ref

  <hr>
  @section DIXCMDLD-ovw Overview
  This module implements DIX copy machine using generic copy machine
  infrastructure. DIX copy machine is built upon the request handler service.
  The same DIX copy machine can be configured to perform multiple tasks,
  repair and rebalance using parity de-clustering layout.
  DIX copy machine is typically started during Mero process startup, although
  it can also be started later.

  <hr>
  @section DIXCMDLD-def Definitions
    Please refer to "Definitions" section in "HLD of copy machine and agents"
    and @ref DIXCMDLD-ref

  <hr>
  @section DIXCMDLD-req Requirements
  - @b r.dix.cm.aggregation.group Aggregation groups should be implemented
    taking into account that it should lead to minimal changes into common
    framework.

  - @b r.dix.cm.data.next The implementation should efficiently select next
    data to be processed without causing any deadlock or bottle neck.

  - @b r.dix.cm.report.progress The implementation should efficiently report
    overall progress of data restructuring and update corresponding layout
    information for restructured objects.

  - @b r.dix.cm.repair.trigger For repair, DIX copy machine should respond to
    triggers caused by various kinds of failures.

  - @b r.dix.cm.repair.iter For repair, DIX copy machine iterator should iterate
    over parity group units on the survived component catalogues and accordingly
    write the lost data to spare units of the corresponding parity group.

  - @b r.dix.cm.rebalance.iter For rebalance, DIX copy machine iterator should
    iterate over the spare units of the repaired parity groups and copy the data
    from corresponding spare units to the target unit on the new device.

  - @b r.dix.cm.be.btree The implementation should work with BE btree directly
    without calling routines of CAS service.

  <hr>
  @section DIXCMDLD-depends Dependencies
  - @b r.dix.cm.resources.manage It must be possible to efficiently manage and
    throttle resources.

    Please refer to "Dependencies" section in "HLD of copy machine and agents"
    and "HLD of SNS Repair" in @ref DIXCMDLD-ref

  <hr>
  @section DIXCMDLD-highlights Design Highlights
  - DIX copy machine uses request handler service infrastructure.
  - DIX copy machine specific data structure embeds generic copy machine and
    other DIX repair specific objects.
  - DIX copy machine defines its specific aggregation group data structure which
    embeds generic aggregation group.
  - Once initialised DIX copy machine remains idle until failure is reported.
  - DIX copy machine creates copy packets and allocates buffers dynamically
    without using of any pools.
  - Failure triggers DIX copy machine to start repair operation.
  - For multiple nodes, DIX copy machine maintains a local proxy of every other
    remote replica in the cluster.
  - For multiple nodes, DIX copy machine sets infinite bounds for its sliding
    window and communicates it to other replicas identified by the local proxies
    through READY FOPs.
  - Every node that serves some unit (from the same parity group) that contains
    data (as spare units can contain data as well) has enough information to
    restore parity by sending of served unit. To make this process more
    deterministic the following rule can be used: if some node serves unit with
    the lowest number (in scope of parity group) that contains data - this node
    is responsible for restoring of parity.
  - No data transformation is needed by DIX repair/re-balance processes.
  - Aggregation groups are not really needed by DIX repair/re-balance proccesses
    and can be implemented rudimentary. Such solution allows to use the code of
    generic copy machine without significant modification.
    @todo It would be nice to find the solution where implementation of any kind
    of aggregation groups is not necessary.
  - For rebalance, the same rule to determine responsible node for data
    reconstruction can be applied. DIX copy machine uses the same layout as
    used during DIX repair to map a unit to the target unit on new device.
    The newly added device may have a new UUID, but will have the same index in
    the pool and the component catalogues identifiers of the failed device and
    the replacement device will also be the same.

  <hr>
  @section DIXCMDLD-lspec Logical specification
  - @ref DIXCMDLD-lspec-cm-setup
  - @ref DIXCMDLD-lspec-cm-prepare
  - @ref DIXCMDLD-lspec-cm-start
  - @ref DIXCMDLD-lspec-cm-data-next
  - @ref DIXCMDLD-lspec-cm-sliding-window
  - @ref DIXCMDLD-lspec-cm-stop

  @subsection DIXCMDLD-lspec-comps Component overview
  The focus of DIX copy machine is to efficiently restructure (repair or
  re-balance) data in case of failures, viz. device, node, etc. The
  restructuring operation is split into various copy packet phases.

  @subsection DIXCMDLD-lspec-cm-setup Copy machine setup
  DIX copy machine service allocates and initialises the corresponding copy
  machine.
  After cm_setup() is successfully called, the copy machine transitions to
  M0_CMS_IDLE state and waits until failure happens. As mentioned in the HLD,
  failure information is a broadcast to all the replicas in the cluster using
  TRIGGER FOP. The FOM corresponding to the TRIGGER FOP activates the DIX copy
  machine to start repair operation by invoking m0_cm_start(), this invokes DIX
  copy machine specific start routine which initialises specific data
  structures.

  Once the repair operation is complete the same copy machine is used to perform
  re-balance operation. In re-balance operation the data from the containing
  unit with the lowest index in the repaired parity group is copied to the new
  device using the layout.

  @subsection DIXCMDLD-lspec-cm-prepare Copy machine ready
  Once copy machine is initialised it is ready to start repair/re-balance
  process.

  @subsection DIXCMDLD-lspec-cm-start Copy machine startup
  Starts and initialises DIX copy machine data iterator.
  @see m0_dix_cm_iter_start()

  @subsection DIXCMDLD-lspec-cm-data-next Copy machine data iterator
  DIX copy machine implements an iterator to efficiently select next data to
  process. This is done by implementing the copy machine specific operation,
  m0_cm_ops::cmo_data_next(). The following pseudo code illustrates the DIX data
  iterator for repair as well as re-balance operation,

  @code
   - for each component catalogue C in ctidx
     - fetch layout L for C
     // proceed in local key order (keys belong to C).
     - for each local key I belonging to C
       // determine whether group S that I belongs to needs reconstruction.
       - if no device id is in the failure set continue to the next key
       // group has to be reconstructed, check whether the unit that contains
       // data and has the lowest number in group S is served locally
       - for each data, parity and spare unit U in S (0 <= U < N + 2K)
         - if U is local && U contains data && U number is min
           - determine destination
           - create copy packet
  @endcode
  The above iterator iterates through each component catalogue in
  catalogue-index catalogue in record key order and determines whether
  corresponding parity group needs reconstruction, if yes then checks whether it
  serves the unit that contains data and has the lowest index in scope of parity
  group, if so then this node is responsible for data reconstruction. After that
  the destination is determined and copy packet is created.

  @subsection DIXCMDLD-lspec-cm-sliding-window Copy machine sliding window
  DIX copy machine supports only infinite sliding window that does not
  need to be maintained.
  It is caused by the following reasons:
  - DIX repair/rebalance processes have no data transformation, so it is not
  needed to regulate data reconstruction process using sliding window
  - Aggregation groups IDs can not be determined and ordered for distributed
  indices to apply sliding window
  Some mandatory callbacks can be implemented as stubs.

  @subsection DIXCMDLD-lspec-cm-stop Copy machine stop
  Once all the component objects corresponding to the distibuted indices
  belonging to the failure set are re-structured (repair or re-balance) by every
  replica in the cluster successfully, the re-structuring operation is marked
  complete.

  @subsection DIXCMDLD-lspec-thread Threading and Concurrency Model
  DIX copy machine is implemented as a request handler service, thus it shares
  the request handler threading model and does not create its own threads. All
  the copy machine operations are performed in context of request handler
  threads.

  DIX copy machine uses generic copy machine infrastructure, which implements
  copy machine state machine using generic Mero state machine infrastructure.
  @ref State machine <!-- sm/sm.h -->

  Locking
  All the updates to members of copy machine are done with m0_cm_lock() held.

  @subsection DIXCMDLD-lspec-numa NUMA optimizations
  N/A

  <hr>
  @section DIXCMDLD-conformance Conformance
  - @b i.dix.cm.aggregation.group Aggregation groups should be implemented
    taking into account that it should lead to minimal changes into common
    framework.

  - @b i.dix.cm.data.next DIX copy machine implements a next function using
    catalogue-index catalogue iterator and pdclust layout infrastructure to
    select the next data to be repaired from the failure set. This is done in
    component catalogue fid order.

  - @b i.dix.cm.report.progress The implementation should efficiently report
    overall progress of data restructuring and update corresponding layout
    information for restructured objects.

  - @b i.dix.cm.repair.trigger Various failures are reported through TRIGGER
    FOP, which create corresponding FOMs. FOMs invoke dix specific copy machine
    operations through generic copy machine interfaces which cause copy machine
    state transitions.

  - @b i.dix.cm.repair.iter For repair, DIX copy machine iterator iterates over
    record keys, determines whether servived data containing unit with the
    lowest index in scope of parity group is served locally to use it for lost
    data reconstruction.

  - @b i.dix.cm.rebalance.iter For rebalance, DIX copy machine acts like the
    repair iterator to copy the data to the corresponding target units on the
    new device.

  - @b i.dix.cm.be.btree DIX copy machine calls BE btree interfaces without
    calling of routines of CAS service.

  <hr>
  @section DIXCMDLD-ut Unit tests
  N/A

  <hr>
  @section DIXCMDLD-st System tests
  N/A

  <hr>
  @section DIXCMDLD-O Analysis
  N/A

  <hr>
  @section DIXCMDLD-ref References
   Following are the references to the documents from which the design is
   derived,
   - <a href="https://docs.google.com/a/seagate.com/document/d/1IPlMzMZZ7686iCpv
t1LyMzglfd9KAkKKhSAlu2Q7N_I/edit">Copy Machine redesign.</a>
   - <a href="https://docs.google.com/a/xyratex.com/document/d/1ZlkjayQoXVm-prMx
   Tkzxb1XncB6HU19I19kwrV-8eQc/edit?hl=en_US">HLD of copy machine and agents.</a
   >
   - <a href="https://docs.google.com/a/xyratex.com/Doc?docid=0ATg1HFjUZcaZZGNkN
   Xg4cXpfMTc5ZjYybjg4Y3Q&hl=en_US">HLD of SNS Repair.</a>
*/

/**
  @addtogroup DIXCM

  DIX copy machine implements a copy machine in-order to re-structure data
  efficiently in an event of a failure. It uses DIX (distributed index)
  and component catalogue infrastructure with parity de-clustering layout.

  @{
*/

M0_INTERNAL struct m0_dix_cm *cm2dix(struct m0_cm *cm)
{
	return container_of(cm, struct m0_dix_cm, dcm_base);
}

M0_INTERNAL int m0_dix_cm_type_register(void)
{
	int rc;

	rc = m0_cm_type_register(&dix_repair_cmt) ?:
	     m0_cm_type_register(&dix_rebalance_cmt);
	if (rc == 0) {
		m0_dix_cm_iter_type_register(&dix_repair_dcmt);
		m0_dix_cm_iter_type_register(&dix_rebalance_dcmt);
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_dix_cm_type_deregister(void)
{
	m0_cm_type_deregister(&dix_repair_cmt);
	m0_cm_type_deregister(&dix_rebalance_cmt);
}

M0_INTERNAL int m0_dix_cm_setup(struct m0_cm *cm)
{
	return M0_RC(0);
}

M0_INTERNAL int m0_dix_cm_prepare(struct m0_cm *cm)
{
	struct m0_dix_cm *dcm = cm2dix(cm);

	dcm->dcm_stats_key = -1;

	return M0_RC(0);
}

static bool dix_cm_proxies_completed_cb(struct m0_clink *cl)
{
	struct m0_dix_cm        *dcm = M0_AMB(dcm, cl, dcm_proxies_completed);
	struct m0_cm            *cm  = &dcm->dcm_base;
	struct m0_cm_aggr_group *end_mark;
	bool                     completed;

	M0_PRE(m0_cm_is_locked(cm));
	completed  = cm->cm_proxy_active_nr == 0;
	if (completed || cm->cm_abort) {
		end_mark = m0_cm_aggr_group_locate(cm, &GRP_END_MARK_ID, true);
		M0_ASSERT(end_mark != NULL);
		m0_cm_aggr_group_fini(end_mark);
		m0_clink_del(cl);
		m0_clink_fini(cl);
	}

	return false;
}

M0_INTERNAL int m0_dix_cm_start(struct m0_cm *cm)
{
	struct m0_dix_cm        *dcm  = cm2dix(cm);
	struct m0_cm_aggr_group *ag   = NULL;
	struct m0_reqh          *reqh = cm->cm_service.rs_reqh;
	int                      rc;

	M0_PRE(m0_cm_is_locked(cm));

	if (!proxy_tlist_is_empty(&cm->cm_proxies)) {
		/* Create end-marker ag and add it into ag incoming list. */
		rc = m0_cm_aggr_group_alloc(cm, &GRP_END_MARK_ID, true, &ag);
		if (rc != 0)
			return M0_ERR(rc);
		m0_clink_init(&dcm->dcm_proxies_completed,
			      dix_cm_proxies_completed_cb);
		m0_clink_add_lock(&dcm->dcm_base.cm_wait,
				  &dcm->dcm_proxies_completed);
	}

	dcm->dcm_stats_key = m0_locality_data_alloc(
		sizeof(struct m0_dix_cm_stats),
		NULL, NULL, NULL);
	if (dcm->dcm_stats_key < 0)
		return M0_ERR(dcm->dcm_stats_key);

	/*
	 * Unlock CM since FOM locality lock is taken during waiting for
	 * iterator to start. It prevents usual locality->CM locking order that
	 * can lead to deadlock.
	 */
	m0_cm_unlock(cm);
	rc = m0_dix_cm_iter_start(&dcm->dcm_it, dcm->dcm_type, reqh,
				  m0_cm_rpc_machine_find(reqh)->rm_bulk_cutoff);
	m0_cm_lock(cm);
	if (rc != 0) {
		if (ag != NULL) {
			m0_cm_aggr_group_fini(ag);
			m0_free(ag);
		}
		m0_locality_data_free(dcm->dcm_stats_key);
	}
	dcm->dcm_start_time = m0_time_now();
	return M0_RC(rc);
}

static void dix_cm_tstats_calc(int loc_idx, void *loc_stats, void *total_stats)
{
	M0_PRE(loc_stats != NULL);
	M0_PRE(total_stats != NULL);

	((struct m0_dix_cm_stats *)total_stats)->dcs_read_size +=
		((struct m0_dix_cm_stats *)loc_stats)->dcs_read_size;
	((struct m0_dix_cm_stats *)total_stats)->dcs_write_size +=
		((struct m0_dix_cm_stats *)loc_stats)->dcs_write_size;
}

M0_INTERNAL void m0_dix_cm_stop(struct m0_cm *cm)
{
	struct m0_dix_cm        *dcm = cm2dix(cm);
	struct m0_cm_aggr_group *end_mark;
	struct m0_dix_cm_stats   total_stats = {};
	M0_PRE(m0_cm_is_locked(cm));

	if (!cm->cm_done && !proxy_tlist_is_empty(&cm->cm_proxies)) {
		end_mark = m0_cm_aggr_group_locate(cm, &GRP_END_MARK_ID, true);
		M0_ASSERT(end_mark != NULL);
		m0_cm_aggr_group_fini(end_mark);
		m0_clink_del_lock(&dcm->dcm_proxies_completed);
		m0_clink_fini(&dcm->dcm_proxies_completed);
	}
	/* release the cm lock, because m0_dix_cm_iter_stop() may block. */
	m0_cm_unlock(cm);
	m0_dix_cm_iter_stop(&dcm->dcm_it);
	m0_cm_lock(cm);
	M0_SET0(&dcm->dcm_it);
	dcm->dcm_stop_time = m0_time_now();

	if (dcm->dcm_stats_key >= 0) {
		m0_locality_data_iterate(dcm->dcm_stats_key,
					 &dix_cm_tstats_calc,
					 &total_stats);

		M0_LOG(M0_DEBUG, "Time: %llu Read Size: %llu Write size: %llu",
		       (unsigned long long)m0_time_sub(dcm->dcm_stop_time,
						       dcm->dcm_start_time),
		       (unsigned long long)total_stats.dcs_read_size,
		       (unsigned long long)total_stats.dcs_write_size);

		m0_locality_data_free(dcm->dcm_stats_key);
	}
}

M0_INTERNAL void m0_dix_cm_fini(struct m0_cm *cm)
{
}

M0_INTERNAL int m0_dix_get_space_for(struct m0_cm             *cm,
				     const struct m0_cm_ag_id *id,
				     size_t                   *count)
{
	*count = 1;
	return M0_RC(0);
}

M0_INTERNAL int m0_dix_cm_ag_next(struct m0_cm *cm,
				  const struct m0_cm_ag_id *id_curr,
				  struct m0_cm_ag_id *id_next)
{
	M0_ENTRY();
	/*
	 * Incoming aggregation group list is not populated in case of DIX
	 * repair/re-balance, because local CM doesn't know which copy packets
	 * it will receive.
	 */
	return M0_RC(-ENODATA);
}

M0_INTERNAL struct m0_reqh *m0_dix_cm2reqh(const struct m0_dix_cm *dcm)
{
	return dcm->dcm_base.cm_service.rs_reqh;
}

static struct m0_cm_proxy *dix_cm_sdev2proxy(struct m0_dix_cm *dcm,
					     uint32_t          sdev_id)
{
	struct m0_reqh             *reqh;
	struct m0_confc            *confc;
	struct m0_fid               svc_fid;
	struct m0_conf_service     *svc = NULL;
	int                         rc;

	reqh = m0_dix_cm2reqh(dcm);
	M0_ASSERT(reqh != NULL);
	confc = m0_reqh2confc(reqh);
	M0_ASSERT(confc != NULL);
	svc_fid = reqh->rh_pools->pc_dev2svc[sdev_id].pds_ctx->sc_fid;
	rc = m0_conf_service_get(confc, &svc_fid, &svc);
	M0_ASSERT(rc == 0);
	M0_ASSERT(svc->cs_type == M0_CST_CAS);
	M0_ASSERT(svc->cs_endpoints[0] != NULL);

	return m0_cm_proxy_locate(&dcm->dcm_base, svc->cs_endpoints[0]);
}

static int dix_cm_ag_setup(struct m0_cm    *cm,
			   struct m0_cm_cp *cp,
			   struct m0_fid   *cctg_fid,
			   uint64_t         recs_nr)
{
	struct m0_cm_ag_id       ag_id;
	struct m0_cm_aggr_group *ag;
	int                      rc;

	M0_ENTRY("cm %p", cp);
	/* Build aggregation group id. */
	ag_id.ai_hi.u_hi = cctg_fid->f_container;
	ag_id.ai_hi.u_lo = cctg_fid->f_key;
	ag_id.ai_lo.u_hi = 0;
	ag_id.ai_lo.u_hi = recs_nr;
	rc = m0_cm_aggr_group_alloc(cm, &ag_id, false, &ag);
	if (rc == 0)
		m0_cm_ag_cp_add(ag, cp);

	return M0_RC(rc);
}

M0_INTERNAL int m0_dix_cm_data_next(struct m0_cm *cm, struct m0_cm_cp *cp)
{
	struct m0_dix_cm      *dcm  = cm2dix(cm);
	struct m0_dix_cm_iter *iter = &dcm->dcm_it;
	struct m0_fom         *pfom = &cm->cm_cp_pump.p_fom;
	struct m0_dix_cm_cp   *dix_cp;
	struct m0_buf          key = {};
	struct m0_buf          val = {};
	struct m0_cm_proxy    *proxy;
	struct m0_fid          local_cctg_fid = {};
	struct m0_fid          remote_cctg_fid = {};
	struct m0_fid          dix_fid = {};
	uint64_t               processed_recs_nr;
	uint32_t               sdev_id = (uint32_t)-1;
	int                    rc;

	/* Inc progress counter. */
	dcm->dcm_processed_nr++;

	if (cm->cm_quiesce || cm->cm_abort) {
		M0_LOG(M0_WARN, "%lu: Got %s cmd: returning -ENODATA",
				 cm->cm_id,
				 cm->cm_quiesce ? "QUIESCE" : "ABORT");
		return M0_RC(-ENODATA);
	}

	if (!dcm->dcm_iter_inprogress) {
		if (!dcm->dcm_cp_in_progress) {
			m0_chan_lock(&iter->di_completed);
			m0_fom_wait_on(pfom, &iter->di_completed, &pfom->fo_cb);
			m0_chan_unlock(&iter->di_completed);
			m0_dix_cm_iter_next(iter);
			dcm->dcm_iter_inprogress = true;
		}
		return M0_FSO_WAIT;
	} else {
		dcm->dcm_iter_inprogress = false;
		/* Set proxy for copy packet and fill key/value AT buffers. */
		rc = m0_dix_cm_iter_get(iter, &key, &val, &sdev_id);
		if (rc != 0) {
			if (rc == -ENODATA)
				cm->cm_last_out_hi = GRP_END_MARK_ID;
			return M0_ERR(rc);
		}
		/* Setup aggregation group. */
		m0_dix_cm_iter_cur_pos(iter, &local_cctg_fid,
				       &processed_recs_nr);
		rc = dix_cm_ag_setup(cm, cp, &local_cctg_fid,
				     processed_recs_nr);
		if (rc == 0) {
			dix_cp = M0_AMB(dix_cp, cp, dc_base);
			M0_ASSERT(dix_cp != NULL);
			proxy = dix_cm_sdev2proxy(dcm, sdev_id);
			M0_ASSERT(proxy != NULL);
			cp->c_cm_proxy = proxy;
			dix_cp->dc_key = key;
			dix_cp->dc_val = val;

			/*
			 * Convert FID of local component catalogue to FID of
			 * remote component catalogue,
			 */
			m0_dix_fid_convert_cctg2dix(&local_cctg_fid, &dix_fid);
			m0_dix_fid_convert_dix2cctg(&dix_fid, &remote_cctg_fid,
						    sdev_id);

			dix_cp->dc_ctg_fid       = remote_cctg_fid;
			dix_cp->dc_ctg_op_flags |= COF_CREATE;
			dix_cp->dc_is_local      = true;
			dcm->dcm_cp_in_progress  = true;
			rc = M0_FSO_AGAIN;
		} else {
			/*
			 * Key/value buffers are not needed anymore in error
			 * case. In case rc == 0 we will destroy these key and
			 * val in dix_cp_fop_release().
			 */
			m0_buf_free(&key);
			m0_buf_free(&val);
		}
		return rc;
	}
}

M0_INTERNAL bool m0_dix_is_peer(struct m0_cm               *cm,
				struct m0_reqh_service_ctx *ctx)
{
	return ctx->sc_type == M0_CST_CAS;
}

#undef M0_TRACE_SUBSYSTEM

/** @} DIXCM */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
