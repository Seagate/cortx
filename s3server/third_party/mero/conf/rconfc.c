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
 * Original author: Igor Vartanov
 * Original author: Egor Nikulenkov
 * Original creation date: 03-Mar-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "lib/tlist.h"
#include "lib/mutex.h"
#include "lib/memory.h"           /* M0_ALLOC_PTR, m0_free */
#include "lib/errno.h"
#include "lib/string.h"
#include "lib/buf.h"              /* m0_buf_strdup */
#include "lib/finject.h"          /* M0_FI_ENABLED */
#include "mero/magic.h"
#include "rm/rm.h"
#include "rm/rm_service.h"        /* m0_rm_svc_rwlock_get */
#include "rpc/conn.h"             /* m0_rpc_conn_sessions_cancel */
#include "rpc/rpclib.h"           /* m0_rpc_client_connect */
#include "conf/cache.h"
#include "conf/obj_ops.h"         /* m0_conf_obj_find */
#include "conf/confc.h"
#include "conf/helpers.h"         /* m0_conf_ha_state_update */
#include "conf/rconfc.h"
#include "conf/rconfc_internal.h" /* rlock_ctx, rconfc_link,
				   * ver_item, ver_accm */
#include "conf/rconfc_link_fom.h" /* rconfc_herd_link__on_death_cb */
#include "ha/entrypoint.h"        /* m0_ha_entrypoint_client */
#include "ha/ha.h"                /* m0_ha */
#include "module/instance.h"      /* m0_get */

/**
 * @page rconfc-lspec rconfc Internals
 *
 * - @ref rconfc-lspec-sm
 * - @ref rconfc-lspec-entrypoint
 * - @ref rconfc-lspec-rlock
 * - @ref rconfc-lspec-elect
 * - @ref rconfc-lspec-ha-notification
 * - @ref rconfc-lspec-gate
 *   - @ref rconfc-lspec-gate-check
 *   - @ref rconfc-lspec-gate-drain
 *   - @ref rconfc-lspec-gate-skip
 * - @ref rconfc-lspec-clean
 * - @ref rconfc_dlspec "Detailed Logical Specification"
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-lspec-sm Rconfc state machine
 *
 * @dot
 *  digraph rconfc_sm {
 *      node [fontsize=9];
 *      edge [fontsize=9];
 *      "M0_RCS_INIT"           [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_RCS_ENTRYPOINT_WAIT"    [shape=rect, style=filled, fillcolor=green];
 *      "M0_RCS_ENTRYPOINT_CONSUME" [shape=rect, style=filled, fillcolor=green];
 *      "M0_RCS_CREDITOR_SETUP"   [shape=rect, style=filled, fillcolor=green];
 *      "M0_RCS_GET_RLOCK"        [shape=rect, style=filled, fillcolor=green];
 *      "M0_RCS_VERSION_ELECT"    [shape=rect, style=filled, fillcolor=green];
 *      "M0_RCS_IDLE"             [shape=rect, style=filled, fillcolor=cyan];
 *      "M0_RCS_RLOCK_CONFLICT"   [shape=rect, style=filled, fillcolor=pink];
 *      "M0_RCS_CONDUCTOR_DRAIN"  [shape=rect, style=filled, fillcolor=pink];
 *      "M0_RCS_STOPPING"         [shape=rect, style=filled, fillcolor=dimgray];
 *      "M0_RCS_FAILURE"          [shape=rect, style=filled, fillcolor=tomato];
 *      "M0_RCS_FINAL"            [shape=rect, style=filled, fillcolor=red];
 *
 *      "M0_RCS_INIT" -> "M0_RCS_ENTRYPOINT_WAIT"
 *      "M0_RCS_INIT" -> "M0_RCS_STOPPING"
 *      "M0_RCS_ENTRYPOINT_WAIT" -> "M0_RCS_ENTRYPOINT_WAIT"
 *      "M0_RCS_ENTRYPOINT_WAIT" -> "M0_RCS_ENTRYPOINT_CONSUME"
 *      "M0_RCS_ENTRYPOINT_WAIT" -> "M0_RCS_FAILURE"
 *      "M0_RCS_ENTRYPOINT_CONSUME" -> "M0_RCS_CREDITOR_SETUP"
 *      "M0_RCS_ENTRYPOINT_CONSUME" -> "M0_RCS_ENTRYPOINT_WAIT"
 *      "M0_RCS_ENTRYPOINT_CONSUME" -> "M0_RCS_FAILURE"
 *      "M0_RCS_CREDITOR_SETUP" -> "M0_RCS_GET_RLOCK"
 *      "M0_RCS_CREDITOR_SETUP" -> "M0_RCS_ENTRYPOINT_WAIT"
 *      "M0_RCS_GET_RLOCK" -> "M0_RCS_VERSION_ELECT"
 *      "M0_RCS_GET_RLOCK" -> "M0_RCS_ENTRYPOINT_WAIT"
 *      "M0_RCS_GET_RLOCK" -> "M0_RCS_FAILURE"
 *      "M0_RCS_VERSION_ELECT" -> "M0_RCS_IDLE"
 *      "M0_RCS_VERSION_ELECT" -> "M0_RCS_ENTRYPOINT_WAIT"
 *      "M0_RCS_VERSION_ELECT" -> "M0_RCS_STOPPING"
 *      "M0_RCS_VERSION_ELECT" -> "M0_RCS_RLOCK_CONFLICT"
 *      "M0_RCS_VERSION_ELECT" -> "M0_RCS_FAILURE"
 *      "M0_RCS_IDLE" -> "M0_RCS_RLOCK_CONFLICT"
 *      "M0_RCS_IDLE" -> "M0_RCS_STOPPING"
 *      "M0_RCS_RLOCK_CONFLICT" -> "M0_RCS_CONDUCTOR_DRAIN"
 *      "M0_RCS_CONDUCTOR_DRAIN" -> "M0_RCS_CONDUCTOR_DISCONNECT"
 *      "M0_RCS_CONDUCTOR_DRAIN" -> "M0_RCS_FAILURE"
 *      "M0_RCS_CONDUCTOR_DRAIN" -> "M0_RCS_FINAL"
 *      "M0_RCS_CONDUCTOR_DISCONNECT" -> "M0_RCS_ENTRYPOINT_WAIT"
 *      "M0_RCS_CONDUCTOR_DISCONNECT" -> "M0_RCS_FAILURE"
 *      "M0_RCS_STOPPING" -> "M0_RCS_CONDUCTOR_DRAIN"
 *      "M0_RCS_FAILURE" -> "M0_RCS_STOPPING"
 *  }
 * @enddot
 *
 * Color agenda:                                          @n
 * @b green         - States during startup or reelection @n
 * @b pink          - Reelection-only states              @n
 * @b dark @b grey  - Stopping states                     @n
 *
 * After successful start rconfc is in M0_RCS_IDLE state, waiting for one of two
 * events: read lock conflict or user request for stopping. These two events
 * are handled only when rconfc is in M0_RCS_IDLE state. If rconfc was in other
 * state, then a fact of the happened event is stored, but its handling is
 * delayed until rconfc state is M0_RCS_IDLE.
 *
 * If failure is occurred that prevents rconfc from functioning properly, then
 * rconfc goes to M0_RCS_FAILURE state. SM in this state do nothing until user
 * requests for stopping.
 *
 * Rconfc internal state is protected by SM group lock. SM group is provided by
 * user on rconfc initialisation.
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-lspec-entrypoint Request cluster entry point from HA
 *
 * The first stage of rconfc startup is determining the entry point of mero
 * cluster, which configuration should be accessed. The entry point consists of
 * several components. All of them can be changed during cluster lifetime.
 *
 * Cluster entry point includes:
 * - List of confd servers fids along with RPC endpoints.
 * - Fid and RPC endpoint of active RM creditor that manages concurrent access
 *   to the cluster configuration database.
 * - Quorum value. Minimum number of confd servers running the same
 *   configuration version number necessary to elect this version.
 *
 * HA subsystem is responsible for serving queries for current cluster entry
 * point. Rconfc makes query to HA subsystem through a local HA agent.
 *
 * It may happen that rconfc is not able to succeed with version election for
 * some reason, (e.g. connection to active RM cannot be established, current set
 * of confds reported by HA does not yield the quorum, etc.)  In this case
 * rconfc repeats entry point request to HA and attempts to elect version with
 * the most recent entry point data set. There is no limit imposed on the number
 * of attempts.
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-lspec-rlock Read Lock Acquisition and Revocation
 *
 * During m0_rconfc_start() execution rconfc requesting read lock from Resource
 * Manager (RM) by calling rconfc_read_lock_get(). On request completion
 * rconfc_read_lock_complete() is called. Successful lock acquisition indicates
 * no configuration change is in progress and configuration reading is allowed.
 *
 * The read lock is retained by rconfc instance until finalisation. But the lock
 * can be revoked by RM in case a conflicting lock is requested. On the lock
 * revocation rconfc_read_lock_conflict() is called. The call installs
 * m0_confc_gate_ops::go_drain() callback to be notified when the last reading
 * context is detached from m0_rconfc::rc_confc instance. The callback ends in
 * calling rconfc_gate_drain() where rconfc starts conductor cache drain. In
 * rconfc_conductor_drained() rconfc eventually puts the read lock back to RM.
 *
 * Being informed about the conflict, rconfc disallows configuration reading
 * done via m0_rconfc::rc_confc until the next read lock acquisition is
 * complete. Besides, in rconfc_conductor_drain() the mentioned confc's cache is
 * drained to prevent consumer from reading cached-but-outdated configuration
 * values. However, the cache data remains untouched and readable to the very
 * moment when there is no cache object pinned anymore, and the last reading
 * context detaches from the confc being in use.
 *
 * When done with the cache, m0_rconfc::rc_confc is disconnected from confd
 * server to prevent unauthorized read operations. Then the conflicting lock is
 * returned back to RM complying with the conflict request.
 *
 * Immediately after revocation rconfc attempts to acquire read lock again. The
 * lock will be granted once the conflicting lock is released.
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-lspec-elect Version Election and Quorum
 *
 * In the course of rconfc_read_lock_complete() under condition of successful
 * read lock acquisition rconfc transits to M0_RCS_VERSION_ELECT state. It
 * initialises every confc instance of the m0_rconfc::rc_herd list, attaches
 * rconfc__cb_quorum_test() to its context and initiates asynchronous reading
 * from the corresponding confd server. When version quorum is either
 * reached or found impossible rconfc_version_elected() is called.
 *
 * On every reading event rconfc__cb_quorum_test() is called. In case the
 * reading context is not completed, the function returns zero value indicating
 * the process to go on. Otherwise rconfc_quorum_test() is called to see if
 * quorum is reached with the last reply. If quorum is reached or impossible,
 * then rconfc_version_elected() is called.
 *
 * Quorum is considered reached when the number of confd servers reported the
 * same version number is greater or equal to the value provided to
 * m0_rconfc_init(). In case zero value was provided, the required quorum number
 * is automatically calculated as a half of confd server count plus one.
 *
 * If quorum is reached, rconfc_conductor_engage() is called connecting
 * m0_rconfc::rc_confc with a confd server from active list. Starting from this
 * moment configuration reading is allowed until read lock is revoked.
 *
 * If quorum was not reached, rconfc repeats request to HA about entry point
 * information and starts new version election with the most recent entry point
 * data set.
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-lspec-ha-notification Processing HA notifications
 *
 * Rconfc is interested in the following notifications from HA:
 * - Permanent failure of active RM creditor.
 * - Permanent failure of one of confd servers from the herd.
 *
 * In order to receive these notifications rconfc creates phony confc
 * (m0_rconfc::rc_phony) and adds fake objects for RM creditor service and confd
 * services upon receiving cluster entry point. Using general non-phony confc
 * instance is not possible, because configuration version election isn't done
 * to that moment.
 *
 * Actions performed on RM creditor death:
 * - If read lock is not acquired yet, then rconfc restarts election process
 *   from requesting cluster entry point. HA subsystem is expected to return
 *   information about newly chosen active RM creditor or an error if it was
 *   unable to choose one. Please note that if HA subsystem constantly
 *   returns already dead RM creditor, then rconfc will go to infinite loop.
 *
 * - If read lock is held by rconfc, then RM creditor death is observed by
 *   checking local owner state in rconfc_read_lock_conflict(). The thing is
 *   that local owner also tracks changes in RM creditor HA state
 *   (see rlock_ctx_creditor_setup()). On RM creditor death owner goes to
 *   ROS_QUIESCE state and calls conflict callbacks for all held credits.
 *   Rconfc unsets local owner creditor and restarts election process in order
 *   to receive newly chosen creditor from HA.
 *
 * Actions performed on death of confd server from herd:
 * - Drop connection.
 * - Finalise internal confc.
 * - Mark herd link as CONFC_DEAD, so this confd doesn't participate in possible
 *   confd switch (see @ref rconfc-lspec-gate-skip).
 *
 * Death notification is basically handled by @ref rconfc_link::rl_fom that is
 * queued from rconfc_herd_link__on_death_cb(). The FOM is intended to safely
 * disconnect herd link from problematic confd when session and connection
 * termination may be timed out. The FOM prevents client's locality from being
 * blocked for a noticeably long time.

   @verbatim
                                       |  m0_fom_init()
         !m0_confc_is_inited() ||      |  m0_fom_queue()
         !m0_confc_is_online()         V
      +--------------------------- M0_RLF_INIT
      |                                |
      |                                |  wait for M0_RPC_SESSION_IDLE
      |                                V
      +---------------------- M0_RLF_SESS_WAIT_IDLE
      |                                |
      |                                |  m0_rpc_session_terminate()
      |                                V
      +---------------------- M0_RLF_SESS_TERMINATING
      |                                |  m0_rpc_session_fini()
      |                                |  m0_rpc_conn_terminate()
      |                                V
      +---------------------- M0_RLF_CONN_TERMINATING
      |                                |  m0_rpc_conn_fini()
      |                                V
      +--------------------------->M0_RLF_FINI
                                       |  m0_fom_fini()
                                       |  rconfc_herd_link_fini()
                                       V
   @endverbatim
 *
 *
 * @attention Currently HA notifications processing doesn't take "conductor"
 * confc into account. This confc instance is separated from those used in herd
 * and is not affected even if death of confd server it communicates with is
 * observed. It is assumed that RPC eventually will return an error and
 * "conductor" confc will be reconnected to another confd.
 *
 * @note There are no special HA notifications about the fact that confd servers
 * list has changed. In order to make rconfc logic correct in such case the
 * following behaviour is expected from HA:
 * - Confd server can't be excluded from list without prior HA notification
 *   about permanent failure of this confd server. Rconfc will receive
 *   this notification and will stop working with it.
 * - Confd server can't be added to the list without prior configuration
 *   database update, that adds this service to database. Rconfc will observe
 *   read lock conflict and eventually will restart election process, thus
 *   obtaining updated confd list.
 *
 * <hr> <!------------------------------------------------------------>
 * @section rconfc-lspec-gate Gating confc operations
 *
 * @subsection rconfc-lspec-gate-check Blocking confc context initialisation
 *
 * Rconfc performs gating read operations conducted through the confc instance
 * governed by the rconfc, i.e. m0_rconfc::rc_confc. When read lock is acquired
 * by rconfc, the reading is allowed. To be allowed to go on with reading,
 * m0_confc_ctx_init() performs checking by calling previously set callback
 * m0_confc::cc_gops::go_check(), that in fact is rconfc_gate_check().
 *
 * With the read lock revoked inside rconfc_gate_check() rconfc blocks any
 * m0_confc_ctx_init() calls done with this particular m0_rconfc::rc_confc. On
 * next successful read lock acquisition all the previously blocked contexts get
 * unblocked. Once being allowed to read, the context can be used as many times
 * as required.
 *

   @msc
   wordwraparcs="1", hscale="2.0";
   rm[ label = RM ],
   rc[ label = rconfc, linecolor="#0000ff", textcolor="#0000ff" ],
   c [ label = "rconfc->rc_confc", linecolor="#0000ff", textcolor="#0000ff" ],
   x [ label = confc_ctx ],
   m [ label = consumer ];

   ||| ;
   rm note rm [ label = "grant read lock", textcolor="#00aa00" ];
   rm=>>rc [ label = "rconfc_read_lock_complete(rc = 0)", textcolor="#00aa00"];
   rc=>rc  [ label = "state = M0_RCS_VERSION_ELECT" ];
   ---     [ label = "waiting until version is elected", linecolor="#0000ff",
   textcolor="#0000ff"];
   rc=>rc  [ label = "rconfc_conductor_engage" ];
   rc=>rc  [ label = "state = M0_RCS_IDLE" ];
   rc note rc [ label = "reading allowed" ];
   ... ;
   m=>x   [ label = "m0_confc_ctx_init"];
   x=>c   [ label = "get .go_check"];
   x<<c   [ label = ".go_check"];
   ---    [ label = "if (.go_check != NULL)" ];
   x=>>rc [ label = "rconfc_gate_check" ];
   rc>>x  [ label = "reading allowed" ];
   ---    [ label = "else // reading allowed by default" ];
   x>>m   [ label = "return from init" ];
   ... ;
   ||| ;
   @endmsc

 * <br/><center>
 * @b Diag.1: @b "Reading allowed at the moment of context initialisation"
 * </center><br/>

   @msc
   wordwraparcs="1", hscale="2.0";
   rm[ label = RM ],
   rc[ label = rconfc, linecolor="#0000ff", textcolor="#0000ff" ],
   c [ label = "rconfc->rc_confc", linecolor="#0000ff", textcolor="#0000ff" ],
   x [ label = confc_ctx ],
   m [ label = consumer ];

   ||| ;
   rm note rm [ label = "revoke read lock", textcolor="#cc0000" ];
   rm=>>rc [ label = "rconfc_read_lock_conflict", textcolor="#cc0000"];
   rc note rc [ label = "reading not allowed" ];
   rc=>rc  [ label = ".go_drain = rconfc_gate_drain" ];
   x<=m    [ label = "m0_confc_ctx_fini" ];
   rc<<=x  [ label = "rconfc_gate_drain" ];
   rc=>rc  [ label = "wait till all conf objects unpinned" ];
   rc=>c   [ label = "m0_confc_reconnect(NULL)" ];
   rm<=rc  [ label = "rconfc_read_lock_put" ];
   rm<=rc  [ label = "get read lock" ];
   rc=>rc  [ label = "state = M0_RCS_GET_RLOCK" ];
   ... ;
   m=>x   [ label = "m0_confc_ctx_init"];
   x=>c   [ label = "get .go_check"];
   x<<c   [ label = ".go_check"];
   ---    [ label = "if (.go_check != NULL)" ];
   x=>>rc [ label = "rconfc_gate_check"];
   rc=>rc [ label = "wait until rconfc in (M0_RCS_IDLE, M0_RCS_FAILURE)" ];
   ---    [ label = "waiting in rconfc_gate_check", linecolor="#0000ff",
   textcolor="#0000ff"];
   ... ;
   rm note rm [ label = "grant read lock", textcolor="#00aa00" ];
   rm=>>rc [ label = "rconfc_read_lock_complete(rc = 0)", textcolor="#00aa00"];
   rc=>rc  [ label = "state = M0_RCS_VERSION_ELECT" ];
   ---     [ label = "waiting until version is elected", linecolor="#00aa00",
   textcolor="#00aa00"];
   rc=>rc  [ label = "rconfc_conductor_engage" ];
   rc=>rc  [ label = "state = M0_RCS_IDLE" ];
   rc note rc [ label = "reading allowed" ];
   ---    [ label = "waking up in rconfc_gate_check", linecolor="#0000ff",
   textcolor="#0000ff"];
   rc>>x  [ label = "reading allowed" ];
   x>>m   [ label = "return from init" ];
   ... ;
   ||| ;
   @endmsc

 * <br/><center>
 * @b Diag.2: @b "Reading disallowed at the moment of context initialisation"
 * </center><br/>

   @msc
   wordwraparcs="1", hscale="2.0";
   rm[ label = RM ],
   rc[ label = rconfc, linecolor="#0000ff", textcolor="#0000ff" ],
   c [ label = "rconfc->rc_confc", linecolor="#0000ff", textcolor="#0000ff" ],
   x [ label = confc_ctx ],
   m [ label = consumer ];

   ||| ;
   rm note rm [ label = "revoke read lock", textcolor="#cc0000" ];
   rm=>>rc [ label = "rconfc_read_lock_conflict", textcolor="#cc0000"];
   rc note rc [ label = "reading not allowed" ];
   rc=>rc [ label = ".go_drain = rconfc_gate_drain" ];
   x<=m   [ label = "m0_confc_ctx_fini" ];
   rc<<=x [ label = "rconfc_gate_drain" ];
   rc=>rc [ label = "drain cache" ];
   rc=>c  [ label = "m0_confc_reconnect(NULL)" ];
   rm<=rc [ label = "rconfc_read_lock_put" ];
   rm<=rc [ label = "get read lock" ];
   rc=>rc [ label = "state = M0_RCS_GET_RLOCK" ];
   ... ;
   m=>x   [ label = "m0_confc_ctx_init"];
   x=>c   [ label = "get .go_check"];
   x<<c   [ label = ".go_check"];
   ---    [ label = "if (.go_check != NULL)" ];
   x=>>rc [ label = "rconfc_gate_check"];
   rc=>rc [ label = "wait until rconfc in (M0_RCS_IDLE, M0_RCS_FAILURE)" ];
   ---    [ label = "waiting in rconfc_gate_check", linecolor="#0000ff",
   textcolor="#0000ff"];
   ... ;
   rm note rc [ label = "communication failed", textcolor="#cc0000" ];
   rm=>>rc [ label = "rconfc_read_lock_complete(rc != 0)", textcolor="#cc0000"];
   rc=>rc  [ label = "state = M0_RCS_FAILURE"];
   rc note rc [ label = "reading not allowed" ];
   ---    [ label = "waking up in rconfc_gate_check", linecolor="#0000ff",
   textcolor="#0000ff"];
   rc>>x  [ label = "reading not allowed" ];
   x>>m   [ label = "return from init" ];
   ... ;
   ||| ;
   @endmsc

 * <br/><center>
 * @b Diag.3: @b "Reading remains disallowed because of
 *                RM communication failure"
 * </center><br/>
 *
 * @subsection rconfc-lspec-gate-drain Cleaning confc cache data
 *
 * When new configuration change is in progress, and therefore, read lock is
 * revoked, rconfc_read_lock_conflict() defers cache draining until there is no
 * reading context attached. It installs m0_confc::cc_gops::go_drain() callback,
 * that normally remains set to NULL and this way does not affect execution of
 * m0_confc_ctx_fini() anyhow. But with the callback set up, at the moment of
 * the very last detach m0_confc_ctx_fini() calls m0_confc::cc_gops::go_drain()
 * callback, that in fact is rconfc_gate_drain(), where cache cleanup is finally
 * invoked by setting M0_RCS_CONDUCTOR_DRAIN state. Rconfc SM remains in
 * M0_RCS_CONDUCTOR_DRAIN_CHECK state until all conf objects are unpinned. Once
 * there are no pinned objects, rconfc cleans cache, put read lock and starts
 * reelection process.
 *

   @msc
   wordwraparcs="1", hscale="2.0";
   rm[ label = RM ],
   rc[ label = rconfc, linecolor="#0000ff", textcolor="#0000ff" ],
   c [ label = "rconfc->rc_confc", linecolor="#0000ff", textcolor="#0000ff" ],
   h [ label = confc_cache ],
   x [ label = confc_ctx ],
   m [ label = consumer ];

   ||| ;
   rm note rm [ label = "revoke read lock", textcolor="#cc0000" ];
   rm=>>rc [ label = "rconfc_read_lock_conflict", textcolor="#cc0000"];
   rc=>c   [ label = "get number of contexts attached" ];
   rc<<c   [ label = " number of contexts attached" ];
   ---     [ label = " if no context attached ", linecolor="#0000ff",
   textcolor="#0000ff" ];
   rc=>rc  [ label = "rconfc_gate_drain" ];
   rc=>rc  [ label = "state = M0_RCS_CONDUCTOR_DRAIN" ];
   ---     [ label = " else if any context(s) attached ", linecolor="#0000ff",
   textcolor="#0000ff" ];
   rc=>rc  [ label = " .go_drain = rconfc_gate_drain" ];
   ... ;
   x<=m    [ label = "m0_confc_ctx_fini" ];
   x=>x    [ label = "M0_CNT_DEC(confc->cc_nr_ctx)" ];
   ---     [ label = " on no context attached " ];
   x=>c    [ label = "get .go_drain" ];
   c>>x    [ label = " .go_drain " ];
   rc<<=x  [ label = "rconfc_gate_drain" ];
   rc=>rc  [ label = " .go_drain = NULL" ];
   rc>>x   [ label = "return" ];
   rc=>rc  [ label = "state = M0_RCS_CONDUCTOR_DRAIN" ];
   ---     [ label = " loop until no pinned object remains ",
   linecolor="#00aaaa", textcolor="#00aaaa" ];
   rc=>h   [ label = "find first pinned object" ];
   h>>rc   [ label = "return m0_conf_obj" ];
   rc=>rc  [ label = "wait for object closure" ];
   ... ;
   m=>h    [ label = "m0_confc_close(waited conf object)" ];
   h->rc   [ label = "broadcast obj->co_chan" ];
   h>>m    [ label = "return" ];
   ---     [ label = "set M0_RCS_CONDUCTOR_DRAIN and search for a
   pinned object ", linecolor="#00aaaa", textcolor="#00aaaa" ];
   rc=>h   [ label = "m0_conf_cache_clean" ];
   h=>h    [ label = "delete all cached objects" ];
   h>>rc   [ label = "return" ];
   ... ;
   ||| ;

   @endmsc

 * <br/><center>
 * @b Diag.4: @b "Deferred Cache Cleanup"
 * </center><br/>
 *
 * @note Forced cache draining occurs when m0_confc_gate_ops::go_drain callback
 * is installed, which happens only when reading is not allowed. Normally the
 * callback is set to NULL, and therefore, confc cache remains unaffected during
 * m0_confc_ctx_fini().
 *
 * @subsection rconfc-lspec-gate-skip Reconnecting confc to another confd
 *
 * In case configuration reading fails because of network error, the confc
 * context requests the confc to skip its current connection to confd and switch
 * to some other confd server running the same version. This is done inside
 * state machine being in S_SKIP_CONFD state by calling callback function
 * m0_confc::cc_gops::go_skip() that in fact is rconfc_gate_skip(). The function
 * iterates through the m0_rconfc::rc_active list and returns on the first
 * successful connection established. In case of no success, the function
 * returns with -ENOENT making the state machine end in S_FAILURE state.
 *
 * @note As long as confc is switched to confd of the same version number, the
 * cache data remains valid and needs no special attendance.
 *
 * @subsection rconfc-lspec-clean Cleaning configuration cache during stopping.
 *
 * When rconfc is stopping, it scans configuration for pinned objects (i. e.
 * objects with m0_conf_obj::co_nrefs > 0). If such object is found then rconfc
 * waits until it will be unpinned by a configuration consumer. The consumer
 * must be subscribed to m0_reqh::rh_confc_cache_expired chan and put its pinned
 * objects in the callback registered with this chan. When all configuration
 * objects become unpinned, rconfc is able to clean configuration cache and go
 * to M0_RCS_FINAL state.
 */

/**
 * @defgroup rconfc_dlspec rconfc Internals
 *
 * @{
 */

static void rconfc_start(struct m0_rconfc *rconfc);
static void rconfc_stop_internal(struct m0_rconfc *rconfc);
static inline uint32_t rconfc_state(const struct m0_rconfc *rconfc);

static bool rconfc_gate_check(struct m0_confc *confc);
static int  rconfc_gate_skip(struct m0_confc *confc);
static bool rconfc_gate_drain(struct m0_clink *clink);
static bool ha_clink_cb(struct m0_clink *clink);

struct m0_confc_gate_ops m0_rconfc_gate_ops = {
	.go_check = rconfc_gate_check,
	.go_skip  = rconfc_gate_skip,
	.go_drain = rconfc_gate_drain,
};

static void rconfc_read_lock_get(struct m0_rconfc *rconfc);
static void rconfc_read_lock_complete(struct m0_rm_incoming *in, int32_t rc);
static void rconfc_read_lock_conflict(struct m0_rm_incoming *in);

struct m0_rm_incoming_ops m0_rconfc_ri_ops = {
	.rio_complete = rconfc_read_lock_complete,
	.rio_conflict = rconfc_read_lock_conflict,
};

static struct m0_sm_state_descr rconfc_states[] = {
	[M0_RCS_INIT] = {
		.sd_flags     = M0_SDF_INITIAL,
		.sd_name      = "M0_RCS_INIT",
		.sd_allowed   = M0_BITS(M0_RCS_FAILURE,
					M0_RCS_ENTRYPOINT_WAIT,
					M0_RCS_STOPPING)
	},
	[M0_RCS_ENTRYPOINT_WAIT] = {
		.sd_name      = "M0_RCS_ENTRYPOINT_WAIT",
		.sd_allowed   = M0_BITS(M0_RCS_ENTRYPOINT_WAIT,
					M0_RCS_ENTRYPOINT_CONSUME,
					M0_RCS_FAILURE),
	},
	[M0_RCS_ENTRYPOINT_CONSUME] = {
		.sd_name      = "M0_RCS_ENTRYPOINT_CONSUME",
		.sd_allowed   = M0_BITS(M0_RCS_ENTRYPOINT_WAIT,
					M0_RCS_CREDITOR_SETUP, M0_RCS_FAILURE),
	},
	[M0_RCS_CREDITOR_SETUP] = {
		.sd_name      = "M0_RCS_CREDITOR_SETUP",
		.sd_allowed   = M0_BITS(M0_RCS_GET_RLOCK,
					M0_RCS_ENTRYPOINT_WAIT)
	},
	[M0_RCS_GET_RLOCK] = {
		.sd_name      = "M0_RCS_GET_RLOCK",
		.sd_allowed   = M0_BITS(M0_RCS_VERSION_ELECT,
					M0_RCS_ENTRYPOINT_WAIT, M0_RCS_FAILURE),
	},
	[M0_RCS_VERSION_ELECT] = {
		.sd_name      = "M0_RCS_VERSION_ELECT",
		.sd_allowed   = M0_BITS(M0_RCS_IDLE, M0_RCS_ENTRYPOINT_WAIT,
					M0_RCS_STOPPING, M0_RCS_RLOCK_CONFLICT,
					M0_RCS_FAILURE),
	},
	[M0_RCS_IDLE] = {
		.sd_name      = "M0_RCS_IDLE",
		.sd_allowed   = M0_BITS(M0_RCS_STOPPING, M0_RCS_RLOCK_CONFLICT),
	},
	[M0_RCS_RLOCK_CONFLICT] = {
		.sd_name      = "M0_RCS_RLOCK_CONFLICT",
		.sd_allowed   = M0_BITS(M0_RCS_CONDUCTOR_DRAIN),
	},
	[M0_RCS_CONDUCTOR_DRAIN] = {
		.sd_name      = "M0_RCS_CONDUCTOR_DRAIN",
		.sd_allowed   = M0_BITS(M0_RCS_CONDUCTOR_DISCONNECT,
					M0_RCS_FAILURE, M0_RCS_FINAL),
	},
	[M0_RCS_CONDUCTOR_DISCONNECT] = {
		.sd_name      = "M0_RCS_CONDUCTOR_DISCONNECT",
		.sd_allowed   = M0_BITS(M0_RCS_ENTRYPOINT_WAIT, M0_RCS_FAILURE),
	},
	[M0_RCS_STOPPING] = {
		.sd_name      = "M0_RCS_STOPPING",
		.sd_allowed   = M0_BITS(M0_RCS_CONDUCTOR_DRAIN),
	},
	[M0_RCS_FAILURE] = {
		.sd_flags     = M0_SDF_FAILURE,
		.sd_name      = "M0_RCS_FAILURE",
		.sd_allowed   = M0_BITS(M0_RCS_STOPPING)
	},
	[M0_RCS_FINAL] = {
		.sd_flags     = M0_SDF_TERMINAL,
		.sd_name      = "M0_RCS_FINAL"
	}
};

static const struct m0_sm_conf rconfc_sm_conf = {
	.scf_name      = "Rconfc",
	.scf_nr_states = ARRAY_SIZE(rconfc_states),
	.scf_state     = rconfc_states
};

/***************************************
 * List Definitions
 ***************************************/

M0_TL_DESCR_DEFINE(rcnf_herd, "rconfc's working  confc list", M0_INTERNAL,
		   struct rconfc_link, rl_herd, rl_magic,
		   M0_RCONFC_LINK_MAGIC, M0_RCONFC_HERD_HEAD_MAGIC
	);
M0_TL_DEFINE(rcnf_herd, M0_INTERNAL, struct rconfc_link);

M0_TL_DESCR_DEFINE(rcnf_active, "rconfc's active confc list", M0_INTERNAL,
		   struct rconfc_link, rl_active, rl_magic,
		   M0_RCONFC_LINK_MAGIC, M0_RCONFC_ACTIVE_HEAD_MAGIC
	);
M0_TL_DEFINE(rcnf_active, M0_INTERNAL, struct rconfc_link);

/* -------------- Thread for async full conf loading -------------- */

static void rconfc_load_ast_thread(struct rconfc_load_ctx *rx)
{
	while (rx->rx_ast.run) {
		m0_chan_wait(&rx->rx_grp.s_clink);
		m0_sm_group_lock(&rx->rx_grp);
		m0_sm_asts_run(&rx->rx_grp);
		m0_sm_group_unlock(&rx->rx_grp);
	}
}

static int rconfc_load_ast_thread_init(struct rconfc_load_ctx *rx)
{
	M0_SET0(&rx->rx_grp);
	M0_SET0(&rx->rx_ast);
	m0_sm_group_init(&rx->rx_grp);
	rx->rx_ast.run = true;
	return M0_THREAD_INIT(&rx->rx_ast.thread, struct rconfc_load_ctx *,
			      NULL, &rconfc_load_ast_thread, rx, "rx_ast_thr");
}

static void rconfc_load_ast_thread_fini(struct rconfc_load_ctx *rx)
{
	rx->rx_ast.run = false;
	m0_clink_signal(&rx->rx_grp.s_clink);
	m0_thread_join(&rx->rx_ast.thread);
	m0_sm_group_fini(&rx->rx_grp);
}

static void rconfc_idle(struct m0_rconfc *rconfc);
static void rconfc_fail(struct m0_rconfc *rconfc, int rc);

static void rconfc_conf_load_fini(struct m0_sm_group *grp,
				  struct m0_sm_ast   *ast)
{
	struct m0_rconfc *rconfc = ast->sa_datum;

	M0_ENTRY("rconfc = %p", rconfc);
	m0_confc_gate_ops_set(&rconfc->rc_confc, &rconfc->rc_gops);
	if (rconfc->rc_rx.rx_rc == 0) {
		rconfc_idle(rconfc); /* unlock reading gate */
	} else {
		rconfc_fail(rconfc, rconfc->rc_rx.rx_rc);
	}
	rconfc_load_ast_thread_fini(&rconfc->rc_rx);
	M0_LEAVE();
}

static void rconfc_conf_full_load(struct m0_sm_group *grp,
				  struct m0_sm_ast   *ast)
{
	int rc;
	struct m0_conf_root   *root = NULL;
	struct m0_rconfc      *rconfc = ast->sa_datum;
	struct m0_confc       *confc = &rconfc->rc_confc;
	struct m0_conf_cache  *cache = &confc->cc_cache;

	M0_ENTRY("rconfc = %p", rconfc);
	rc = m0_conf_obj_find_lock(cache, &M0_CONF_ROOT_FID, &confc->cc_root) ?:
	     m0_confc_root_open(confc, &root) ?:
	     m0_conf_full_load(root);
	if (root != NULL)
		m0_confc_close(&root->rt_obj);
	/*
	 * The configuration might be loaded. Now we need to invoke
	 * rconfc_ha_restore() to re-associate the kept clinks with their
	 * objects in cache.
	 */
	rconfc->rc_rx.rx_rc = rc;
	rconfc->rc_load_fini_ast.sa_datum = rconfc;
	rconfc->rc_load_fini_ast.sa_cb = rconfc_conf_load_fini;
	/* The call to execute in context of group rconfc initialised with */
	m0_sm_ast_post(rconfc->rc_sm.sm_grp, &rconfc->rc_load_fini_ast);
	M0_LEAVE();
}

/***************************************
 * Helpers
 ***************************************/

/* -------------- Confc Helpers -------------- */

static uint64_t _confc_ver_read(const struct m0_confc *confc)
{
	return confc->cc_cache.ca_ver;
}

/** Safe remote confd address reading */
static const char *_confc_remote_addr_read(const struct m0_confc *confc)
{
	return m0_confc_is_online(confc) ?
		m0_rpc_conn_addr(m0_confc2conn((struct m0_confc *)confc)) :
		NULL;
}

static int _confc_cache_clean(struct m0_confc *confc)
{
	struct m0_conf_cache *cache = &confc->cc_cache;

	M0_ENTRY();
	m0_conf_cache_clean(cache, NULL);
	/* Clear version to prevent version mismatch error after reelection */
	cache->ca_ver = M0_CONF_VER_UNKNOWN;
	/**
	 * @todo Confc root pointer is not valid anymore after cache cleanup, so
	 * it should be reinitialised. The easiest way would be to reinitialise
	 * confc completely, but user can create confc contexts during
	 * reelection, so let's reinitialise root object the hackish way.
	 */
	return M0_RC(m0_conf_obj_find(cache, &M0_CONF_ROOT_FID,
				      &confc->cc_root));
}

static int _confc_cache_clean_lock(struct m0_confc *confc)
{
	int rc;

	M0_ENTRY();
	m0_conf_cache_lock(&confc->cc_cache);
	rc = _confc_cache_clean(confc);
	m0_conf_cache_unlock(&confc->cc_cache);
	return M0_RC(rc);
}

/* ------------------- Phony confc ------------------ */

/**
 * Phony confc initialisation. Done with no confd address, and cache being fed
 * with minimal formal local cache string containing root object only. Once
 * initialised, it is expected to append proper object FIDs, obtained from HA
 * and watched for death notifications during entire rconfc instance life cycle.
 */
static int _confc_phony_init(struct m0_confc *confc)
{
	void *fake_ptr = (void *)1;
	int   rc;

	M0_ENTRY();
	/*
	 * The confc never to do real conf reading, so no real group is
	 * required, as well as no real RPC machine to be used because no RPC
	 * connection to be established. Fake pointers are just to prevent
	 * assertions from firing in m0_confc_init().
	 */
	rc = m0_confc_init(confc, fake_ptr, NULL, fake_ptr, NULL);
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_ha_client_add(confc);
	if (rc != 0)
		m0_confc_fini(confc);
	return M0_RC(rc);
}

/**
 * Finalisation of the phony confc. Indicates that no HA notification is
 * expected anymore, so the instance must be removed from HA client list and
 * drained safely.
 */
static void _confc_phony_fini(struct m0_confc *phony)
{
	M0_ENTRY();
	/* Cache contains only root object */
	M0_PRE(m0_conf_cache_tlist_length(
				&phony->cc_cache.ca_registry) == 1);
	m0_ha_client_del(phony);
	m0_confc_fini(phony);
	M0_SET0(phony);
	M0_LEAVE();
}

/**
 * Appends fake object to phony cache if it's not there yet.
 */
static int _confc_phony_cache_append(struct m0_confc      *confc,
				     const struct m0_fid  *fid)
{
	struct m0_conf_cache *cache = &confc->cc_cache;
	struct m0_conf_obj   *obj;
	int                   rc = 0;

	M0_ENTRY();
	m0_conf_cache_lock(cache);
	obj = m0_conf_cache_lookup(cache, fid);
	if (obj != NULL)
		goto out;
	rc = -ENOMEM;
	obj = m0_conf_obj_create(fid, cache);
	if (obj == NULL)
		goto out;
	/*
	 * fake it be an already normally read object to comply with HA
	 * subscription mechanisms (see ha_state_accept())...
	 */
	obj->co_status = M0_CS_READY;
	/*
	 * ...and fake it be pinned, as further discharging will rely on
	 * that pin while iterating the cache
	 */
	obj->co_nrefs  = 1;
	rc = m0_conf_cache_add(cache, obj);
	if (rc != 0) {
		obj->co_nrefs  = 0;
		m0_conf_obj_delete(obj);
	}
out:
	m0_conf_cache_unlock(cache);
	return M0_RC(rc);
}

/**
 * Removes fake conf object with respective fid from phony cache. Object status
 * needs to be forced to M0_CS_MISSING to safely pass m0_conf_cache_del().
 */
static void _confc_phony_cache_remove(struct m0_confc     *confc,
				      const struct m0_fid *fid)
{
	struct m0_conf_obj   *obj;
	struct m0_conf_cache *cache = &confc->cc_cache;

	M0_ENTRY();
	m0_conf_cache_lock(cache);
	obj = m0_conf_cache_lookup(&confc->cc_cache, fid);
	M0_ASSERT(!m0_conf_obj_is_stub(obj));
	obj->co_status = M0_CS_MISSING;
	obj->co_nrefs  = 0;
	m0_conf_cache_del(cache, obj);
	m0_conf_cache_unlock(cache);
	M0_LEAVE();
}

/* -------------- Read lock context ----------------- */

static int rlock_ctx_read_domain_init(struct rlock_ctx *rlx)
{
	return m0_rwlockable_domain_type_init(&rlx->rlc_dom, &rlx->rlc_rt);
}

static void rlock_ctx_read_domain_fini(struct rlock_ctx *rlx)
{
	m0_rwlockable_domain_type_fini(&rlx->rlc_dom, &rlx->rlc_rt);
}

static struct m0_rconfc *rlock_ctx_incoming_to_rconfc(struct m0_rm_incoming *in)
{
	struct rlock_ctx *rlx;

	rlx = container_of(in, struct rlock_ctx, rlc_req);
	return rlx->rlc_parent;
}

static bool rlock_ctx_is_online(struct rlock_ctx *rlx)
{
	return rlx->rlc_rm_addr != NULL;
}

static void rlock_ctx_disconnect(struct rlock_ctx *rlx)
{
	int rc;

	M0_ENTRY("rconfc = %p, rlx = %p", rlx->rlc_parent, rlx);
	M0_PRE(rlock_ctx_is_online(rlx));
	rc = m0_rpc_session_destroy(&rlx->rlc_sess, m0_rpc__down_timeout());
	if (rc != 0)
		M0_LOG(M0_ERROR, "Failed to destroy rlock session");
	rc = m0_rpc_conn_destroy(&rlx->rlc_conn, m0_rpc__down_timeout());
	if (rc != 0)
		M0_LOG(M0_ERROR, "Failed to destroy rlock connection");
	m0_free(rlx->rlc_rm_addr);
	rlx->rlc_rm_addr = NULL;
	M0_LEAVE();
}

static int rlock_ctx_connect(struct rlock_ctx *rlx, const char *ep)
{
	enum {
		MAX_RPCS_IN_FLIGHT        = 15,
		RLOCK_CTX_TIMEOUT_DEFAULT = 3ULL * M0_TIME_ONE_SECOND,
	};

	int       rc;
	m0_time_t deadline = rlx->rlc_timeout == 0 ?
		m0_time_from_now(0, RLOCK_CTX_TIMEOUT_DEFAULT) :
		m0_time_from_now(0, rlx->rlc_timeout);

	M0_ENTRY("rconfc = %p, rlx = %p, ep = %s", rlx->rlc_parent, rlx, ep);
	M0_PRE(!rlock_ctx_is_online(rlx));
	M0_PRE(ep != NULL);
	rlx->rlc_rm_addr = m0_strdup(ep);
	if (rlx->rlc_rm_addr == NULL)
		return M0_ERR(-ENOMEM);
	if (M0_FI_ENABLED("rm_conn_failed"))
		rc = M0_ERR(-ECONNREFUSED);
	else
		rc = m0_rpc_client_connect(&rlx->rlc_conn, &rlx->rlc_sess,
					   rlx->rlc_rmach, rlx->rlc_rm_addr,
					   NULL, MAX_RPCS_IN_FLIGHT, deadline);
	if (rc != 0) {
		m0_free(rlx->rlc_rm_addr);
		rlx->rlc_rm_addr = NULL;
	}
	rlx->rlc_online = (rc == 0);
	/*
	 * It might happen that rconfc was put to M0_RCS_FAILURE state before
	 * connection was established, due to reasons unrelated to RM.
	 */
	return rconfc_state(rlx->rlc_parent) == M0_RCS_FAILURE ?
		M0_ERR(-ESTALE) : M0_RC(rc);
}

static int rlock_ctx_create(struct m0_rconfc       *parent,
			    struct m0_rpc_machine  *rmach,
			    struct rlock_ctx      **out)
{
	struct rlock_ctx *rlx;
	int               rc;

	M0_ENTRY("rconfc = %p, out = %p", parent, out);
	M0_PRE(out != NULL);
	M0_PRE(rmach != NULL);

	M0_ALLOC_PTR(rlx);
	if (rlx == NULL)
		return M0_ERR(-ENOMEM);

	rlx->rlc_parent = parent;
	rlx->rlc_rmach = rmach;
	rc = rlock_ctx_read_domain_init(rlx);
	if (rc != 0)
		return M0_ERR(rc);
	m0_rw_lockable_init(&rlx->rlc_rwlock, &M0_RWLOCK_FID, &rlx->rlc_dom);
	m0_fid_tgenerate(&rlx->rlc_owner_fid, M0_RM_OWNER_FT);
	m0_rm_rwlock_owner_init(&rlx->rlc_owner, &rlx->rlc_owner_fid,
				&rlx->rlc_rwlock, NULL);
	rlx->rlc_rm_addr = NULL;
	*out = rlx;
	return M0_RC(0);
}

static void rlock_ctx_destroy(struct rlock_ctx *rlx)
{
	M0_ENTRY("rconfc = %p, rlx = %p", rlx->rlc_parent, rlx);
	M0_PRE(!rlock_ctx_is_online(rlx));
	m0_rw_lockable_fini(&rlx->rlc_rwlock);
	rlock_ctx_read_domain_fini(rlx);
	m0_free(rlx);
	M0_LEAVE();
}

static enum m0_rm_owner_state
rlock_ctx_creditor_state(struct rlock_ctx *rlx)
{
	return rlx->rlc_owner.ro_sm.sm_state;
}

static int rlock_ctx_creditor_setup(struct rlock_ctx *rlx,
				    const char       *ep)
{
	struct m0_rm_owner  *owner = &rlx->rlc_owner;
	struct m0_rm_remote *creditor = &rlx->rlc_creditor;
	struct m0_fid       *fid = &rlx->rlc_rm_fid;
	struct m0_conf_obj  *obj;
	int                  rc;

	M0_ENTRY("rconfc = %p, rlx = %p, ep = %s", rlx->rlc_parent, rlx, ep);
	M0_PRE(M0_IN(rlock_ctx_creditor_state(rlx),
		     (ROS_ACTIVE, ROS_DEAD_CREDITOR)));
	rc = rlock_ctx_connect(rlx, ep);
	if (rc != 0) {
		_confc_phony_cache_remove(&rlx->rlc_parent->rc_phony, fid);
		return M0_ERR(rc);
	}
	m0_rm_remote_init(creditor, owner->ro_resource);
	creditor->rem_session = &rlx->rlc_sess;
	m0_rm_owner_creditor_reset(owner, creditor);
	rlx->rlc_rm_fid       = *fid;
	/*
	 * Subscribe for HA state changes of creditor.
	 * Unsubscription is automatically done in m0_rm_remote_fini().
	 */
	obj = m0_conf_cache_lookup(&rlx->rlc_parent->rc_phony.cc_cache, fid);
	m0_clink_add_lock(&obj->co_ha_chan, &creditor->rem_tracker.rht_clink);

	return M0_RC(0);
}

static void rlock_ctx_creditor_unset(struct rlock_ctx *rlx)
{
	M0_ENTRY("rconfc = %p, rlx = %p", rlx->rlc_parent, rlx);
	rlock_ctx_disconnect(rlx);
	m0_rm_remote_fini(&rlx->rlc_creditor);
	M0_SET0(&rlx->rlc_creditor);
	rlx->rlc_owner.ro_creditor = NULL;
	_confc_phony_cache_remove(&rlx->rlc_parent->rc_phony, &rlx->rlc_rm_fid);
	M0_LEAVE();
}

static void rlock_ctx_owner_windup(struct rlock_ctx *rlx)
{
	int rc;

	M0_ENTRY("rconfc = %p, rlx = %p", rlx->rlc_parent, rlx);
	m0_rm_owner_windup(&rlx->rlc_owner);
	rc = m0_rm_owner_timedwait(&rlx->rlc_owner,
				   M0_BITS(ROS_FINAL, ROS_INSOLVENT),
				   M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	M0_LEAVE();
}

/* -------------- Quorum calculation context ----------------- */

static void ver_accm_init(struct ver_accm *va, int total)
{
	M0_ENTRY();
	M0_PRE(total <= VERSION_ITEMS_TOTAL_MAX);
	M0_SET0(va);
	va->va_total = total;
	M0_LEAVE();
}

/* -------------- Rconfc Helpers -------------- */

static inline uint32_t rconfc_state(const struct m0_rconfc *rconfc)
{
	return rconfc->rc_sm.sm_state;
}

static uint32_t rconfc_confd_count(const char **confd_addr)
{
	uint32_t count;

	M0_PRE(confd_addr != NULL);
	count = 0;
	while (*confd_addr++ != NULL)
		++count;
	return count;
}

/**
 * Reports confd addresses uniqueness. Tested along with fids uniqueness during
 * rconfc_herd_update().
 */
static bool rconfc_confd_addr_are_all_unique(const char **confd_addr)
{
	uint32_t count = rconfc_confd_count(confd_addr);
	return m0_forall(i, count,
			 m0_forall(j, count,
				   i == j ? true :
				   !m0_streq(confd_addr[i],
					     confd_addr[j])));
}

/***************************************
 * Rconfc private part
 ***************************************/

static bool rconfc_is_locked(struct m0_rconfc *rconfc)
{
	return m0_sm_group_is_locked(rconfc->rc_sm.sm_grp);
}

/** Read Lock cancellation */
static void rconfc_read_lock_put(struct m0_rconfc *rconfc)
{
	struct rlock_ctx *rlx;

	M0_ENTRY("rconfc = %p", rconfc);
	rlx = rconfc->rc_rlock_ctx;
	if (!M0_IS0(&rlx->rlc_req)) {
	    if (!M0_IN(rlx->rlc_req.rin_sm.sm_state,
		       (RI_INITIALISED, RI_FAILURE, RI_RELEASED)))
			m0_rm_credit_put(&rlx->rlc_req);
		m0_rm_rwlock_req_fini(&rlx->rlc_req);
	}
	M0_SET0(&rlx->rlc_req);
	M0_LEAVE();
}

static void rconfc__ast_post(struct m0_rconfc  *rconfc,
			     void              *datum,
			     void             (*cb)(struct m0_sm_group *,
						    struct m0_sm_ast *))
{
	struct m0_sm_ast *ast = &rconfc->rc_ast;

	ast->sa_cb = cb;
	ast->sa_datum = datum;
	m0_sm_ast_post(rconfc->rc_sm.sm_grp, ast);
}

static void rconfc_ast_post(struct m0_rconfc  *rconfc,
			    void             (*cb)(struct m0_sm_group *,
						   struct m0_sm_ast *))
{
	rconfc__ast_post(rconfc, rconfc, cb);
}

static void rconfc_state_set(struct m0_rconfc *rconfc, int state)
{
	M0_LOG(M0_DEBUG, "rconfc: %p, %s -> %s", rconfc,
	       m0_sm_state_name(&rconfc->rc_sm, rconfc_state(rconfc)),
	       m0_sm_state_name(&rconfc->rc_sm, state));

	m0_sm_state_set(&rconfc->rc_sm, state);
}

static void rconfc_fail(struct m0_rconfc *rconfc, int rc)
{
	M0_ENTRY("rconfc = %p, rc = %d", rconfc, rc);
	M0_PRE(rconfc_is_locked(rconfc));
	M0_LOG(M0_ERROR, "rconfc: %p, state %s failed with %d", rconfc,
	       m0_sm_state_name(&rconfc->rc_sm, rconfc_state(rconfc)), rc);
	/*
	 * Put read lock on failure, because this rconfc can prevent remote
	 * write lock requests from completion.
	 */
	rconfc_read_lock_put(rconfc);
	rconfc->rc_sm_state_on_abort = rconfc_state(rconfc);
	m0_sm_fail(&rconfc->rc_sm, M0_RCS_FAILURE, rc);
	if (rconfc->rc_stopping)
		rconfc_stop_internal(rconfc);
	if (rconfc->rc_fatal_cb != NULL)
		rconfc->rc_fatal_cb(rconfc);
	M0_LEAVE();
}

static void _failure_ast_cb(struct m0_sm_group *grp M0_UNUSED,
			    struct m0_sm_ast   *ast)
{
	struct m0_rconfc *rconfc = ast->sa_datum;

	rconfc_fail(rconfc, rconfc->rc_datum);
}

static void rconfc_fail_ast(struct m0_rconfc *rconfc, int rc)
{
	rconfc->rc_datum = rc;
	rconfc_ast_post(rconfc, _failure_ast_cb);
}

M0_INTERNAL bool m0_rconfc_reading_is_allowed(const struct m0_rconfc *rconfc)
{
	M0_PRE(rconfc != NULL);
	return rconfc_state(rconfc) == M0_RCS_IDLE;
}

static bool rconfc_quorum_is_reached(struct m0_rconfc *rconfc)
{
	M0_PRE(rconfc_is_locked(rconfc));
	return rconfc->rc_ver != M0_CONF_VER_UNKNOWN;
}

static void rconfc_active_all_unlink(struct m0_rconfc *rconfc)
{
	struct rconfc_link *lnk;

	/* unlink all active entries */
	m0_tl_teardown(rcnf_active, &rconfc->rc_active, lnk) {
		rcnf_active_tlink_fini(lnk);
	}
}

/**
 * Performs subscription to HA notifications regarding confd death.
 */
static void rconfc_herd_link_subscribe(struct rconfc_link *lnk)
{
	struct m0_conf_obj   *obj;
	struct m0_conf_cache *cache = &lnk->rl_rconfc->rc_phony.cc_cache;

	M0_PRE(lnk->rl_state == CONFC_IDLE);
	obj = m0_conf_cache_lookup(cache, &lnk->rl_confd_fid);
	M0_ASSERT(obj != NULL);
	m0_clink_add_lock(&obj->co_ha_chan, &lnk->rl_ha_clink);
}

/**
 * Dismisses subscription to HA notifications in case it was done previously.
 */
static void rconfc_herd_link_unsubscribe(struct rconfc_link *lnk)
{
	if (lnk->rl_ha_clink.cl_chan != NULL)
		m0_clink_del_lock(&lnk->rl_ha_clink);
}

static inline struct m0_reqh *rconfc_link2reqh(struct rconfc_link *lnk)
{
	return lnk->rl_rconfc->rc_rmach->rm_reqh;
}

/**
 * The callback is called when corresponding HA notification arrives to phony
 * confc. Death notification results in disabling respective link and putting it
 * to dead state. Any other sort of notification is ignored.
 */
static bool rconfc_herd_link__on_death_cb(struct m0_clink *clink)
{
	struct rconfc_link *lnk = M0_AMB(lnk, clink, rl_ha_clink);
	struct m0_conf_obj *obj = M0_AMB(obj, clink->cl_chan, co_ha_chan);

	M0_ENTRY("lnk=%p", lnk);
	M0_ASSERT(m0_fid_eq(&lnk->rl_confd_fid, &obj->co_id));
	if (obj->co_ha_state != M0_NC_FAILED) {
		M0_LEAVE("co_ha_state = %d, return true", obj->co_ha_state);
		return true;
	}
	m0_rconfc_lock(lnk->rl_rconfc);
	m0_mutex_lock(&lnk->rl_rconfc->rc_herd_lock);
	if (lnk->rl_state != CONFC_DEAD && !lnk->rl_fom_queued) {
		if (lnk->rl_on_state_cb != NULL)  /* For UT only */
			lnk->rl_on_state_cb(lnk);
		lnk->rl_fom_queued = true;
		m0_fom_init(&lnk->rl_fom, &rconfc_link_fom_type,
			    &rconfc_link_fom_ops, NULL, NULL,
			    rconfc_link2reqh(lnk));
		m0_fom_queue(&lnk->rl_fom);
		/**
		 * @todo The dead confd fid is going to be removed from phony
		 * cache, and the link object to be finalised in
		 * rconfc_link_fom_fini(). Therefore, no way remains to listen
		 * for the confd state changes in future. You may be tempted by
		 * an idea of reviving the link in case the confd is announced
		 * M0_NC_ONLINE later, but consider the following analysis:
		 *
		 * Merits:
		 *
		 * The only merit of getting the link online is this may bring
		 * an additional active list entry into the effect, in case the
		 * revived confd runs version that was elected previously.
		 *
		 * The merit is diminished by the fact that rconfc performs full
		 * conf load every time it obtains read lock, so using an
		 * alternative active link may be needed only during the conf
		 * load, and never happens after that until the moment of next
		 * version reelection.
		 *
		 * Demerits:
		 *
		 * a) The phony object must remain cached, and the herd link
		 * object must remain in the list during all the rconfc life.
		 *
		 * b) Herd link revival is not an instant action. It requires
		 * the confc to connect, and invoke conf reading in case of
		 * success. If reading succeeds, the conf version must be
		 * qualified, and in case it matches with the currently elected
		 * one, the entry must be finally added to active list. The
		 * entire routine seems to require an additional state machine
		 * to be added to herd link. The machine must be reconciled with
		 * rconfc state machine. This is ultimately to additionally
		 * complicate the existing rconfc state machine, and do that
		 * rather seriously. Otherwise, controlling rconfc state while
		 * having the revival in background may result in unpredictable
		 * races.
		 *
		 * c) There may be several concurrent M0_NC_ONLINE events, or
		 * state jitters, that in combination with async processing make
		 * the whole logic be really non-trivial compared to on-death
		 * processing.
		 *
		 * So the merit of having an additional active entry is going to
		 * be achieved at the cost of over-complicating the existing
		 * rconfc design and an impact on rconfc code maintainability
		 * and extensibility. At the same time a simple version
		 * reelection may achieve the same result at sufficiently lower
		 * cost, as all the logic of entrypoint re-querying is already
		 * implemented in the context of MERO-2113,2150.
		 */
	} else {
		M0_LOG(M0_INFO, "Link to "FID_F" known to be CONFC_DEAD",
		       FID_P(&lnk->rl_confd_fid));
	}
	m0_mutex_unlock(&lnk->rl_rconfc->rc_herd_lock);
	m0_rconfc_unlock(lnk->rl_rconfc);
	M0_LEAVE("lnk=%p co_ha_state = M0_NC_FAILED, return false", lnk);
	return false;
}

static void rconfc_herd_link_init(struct rconfc_link *lnk)
{
	enum { HERD_LINK_TIMEOUT_DEFAULT = 1ULL * M0_TIME_ONE_SECOND };

	struct m0_rconfc *rconfc = lnk->rl_rconfc;

	M0_ENTRY("lnk %p", lnk);
	M0_PRE(rconfc != NULL);
	m0_clink_init(&lnk->rl_ha_clink, rconfc_herd_link__on_death_cb);
	if (M0_FI_ENABLED("confc_init"))
		lnk->rl_rc = -EIO;
	else
		lnk->rl_rc =
			m0_confc_init_wait(&lnk->rl_confc, rconfc->rc_sm.sm_grp,
					   lnk->rl_confd_addr, rconfc->rc_rmach,
					   NULL, HERD_LINK_TIMEOUT_DEFAULT);
	if (lnk->rl_rc == 0)
		lnk->rl_state = CONFC_IDLE;
	else
		m0_clink_fini(&lnk->rl_ha_clink);
	M0_LEAVE();
}

/*
 * M0_INTERNAL here is used for the sake of rconfc_herd_link_die() in
 * conf/rconfc_link_fom.c
 */
M0_INTERNAL void rconfc_herd_link_fini(struct rconfc_link *lnk)
{
	M0_ENTRY("lnk %p", lnk);
	M0_PRE(rconfc_is_locked(lnk->rl_rconfc));
	M0_PRE(lnk->rl_fom_clink.cl_chan == NULL);
	M0_PRE(lnk->rl_fom_queued || M0_IS0(&lnk->rl_fom));
	/* dead confc has no internals to fini */
	if (lnk->rl_state != CONFC_DEAD) {
		rconfc_herd_link_unsubscribe(lnk);
		_confc_phony_cache_remove(&lnk->rl_rconfc->rc_phony,
					  &lnk->rl_confd_fid);
		m0_clink_fini(&lnk->rl_ha_clink);
		if (m0_confc_is_inited(&lnk->rl_confc))
			m0_confc_fini(&lnk->rl_confc);
	}
	M0_LEAVE();
}

static void rconfc_herd_ast(struct m0_sm_group *grp,
			    struct m0_sm_ast   *ast)
{
	M0_LOG(M0_DEBUG, "Re-trying to stop...");
	rconfc_stop_internal(ast->sa_datum);
}

static bool rconfc_herd_fini_cb(struct m0_clink *link)
{
	struct m0_rconfc *rconfc = M0_AMB(rconfc, link, rc_herd_cl);
	M0_ENTRY("rconfc %p", rconfc);
	m0_clink_del(link);
	rconfc_ast_post(rconfc, rconfc_herd_ast);
	return true;
}

static void rconfc_ha_update_ast(struct m0_sm_group *grp,
				 struct m0_sm_ast   *ast)
{
	struct m0_rconfc            *rconfc = ast->sa_datum;
	struct rlock_ctx            *rlx = rconfc->rc_rlock_ctx;
	struct m0_ha_entrypoint_rep *hep = &rconfc->rc_ha_entrypoint_rep;
	char                        *rm_addr = hep->hae_active_rm_ep;
	int                          rc = 0;

	m0_free(rconfc->rc_nvec.nv_note);
	rconfc_state_set(rconfc, M0_RCS_CREDITOR_SETUP);

	if (rlx->rlc_rm_addr == NULL || !m0_streq(rlx->rlc_rm_addr, rm_addr)) {
		if (rlock_ctx_is_online(rlx))
			rlock_ctx_creditor_unset(rlx);
		rc = rlock_ctx_creditor_setup(rlx, rm_addr);
	}
	if (rc == 0) {
		rconfc_read_lock_get(rconfc);
	} else {
		rconfc_start(rconfc);
		M0_CNT_INC(rconfc->rc_ha_entrypoint_retries);
	}

	m0_ha_entrypoint_rep_free(&rconfc->rc_ha_entrypoint_rep);
}

static bool rconfc_ha_update_cb(struct m0_clink *link)
{
	struct m0_rconfc *rconfc = M0_AMB(rconfc, link, rc_ha_update_cl);
	M0_ENTRY("rconfc %p", rconfc);
	m0_clink_del(link);
	rconfc_ast_post(rconfc, rconfc_ha_update_ast);
	return true;
}

static int rconfc_herd_fini(struct m0_rconfc *rconfc)
{
	struct rconfc_link *lnk;

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc_is_locked(rconfc));
	m0_mutex_lock(&rconfc->rc_herd_lock);
	m0_tl_for(rcnf_herd, &rconfc->rc_herd, lnk) {
		if (lnk->rl_cctx.fc_confc == NULL) {
			M0_LOG(M0_DEBUG, "lnk %p is finalised", lnk);
		} else if (m0_confc_ctx_is_completed(&lnk->rl_cctx)) {
			m0_confc_ctx_fini_locked(&lnk->rl_cctx);
		} else {
			/*
			 * Found link which conf reading context is still
			 * active. We need to cancel context operations and wait
			 * until the context is completed.
			 */
			M0_LOG(M0_DEBUG, "Stop re-trying required (cctx)");
			m0_clink_add(&lnk->rl_cctx.fc_mach.sm_chan,
				     &rconfc->rc_herd_cl);
			m0_rpc_conn_sessions_cancel(
				m0_confc2conn(&lnk->rl_confc));
			m0_mutex_unlock(&rconfc->rc_herd_lock);
			return M0_RC(1);
		}
		if (lnk->rl_fom_queued) {
			/*
			 * Found link already queued for finalisation. We need
			 * to wait until FOM finalisation is announced on the
			 * channel m0_rconfc::rc_herd_chan.
			 */
			M0_LOG(M0_DEBUG, "Stop re-trying required (link FOM)");
			m0_clink_add(&rconfc->rc_herd_chan,
				     &rconfc->rc_herd_cl);
			m0_mutex_unlock(&rconfc->rc_herd_lock);
			return M0_RC(1);
		}
	} m0_tl_endfor;
	m0_mutex_unlock(&rconfc->rc_herd_lock);
	m0_tl_for(rcnf_herd, &rconfc->rc_herd, lnk) {
		rconfc_herd_link_fini(lnk);
	} m0_tl_endfor;
	return M0_RC(0);
}

static void rconfc_herd_link_destroy(struct rconfc_link *lnk)
{
	rcnf_herd_tlink_fini(lnk);
	m0_free(lnk->rl_confd_addr);
	m0_free(lnk);
}

static void rconfc_herd_prune(struct m0_rconfc *rconfc)
{
	struct rconfc_link *lnk;

	M0_ENTRY("rconfc = %p", rconfc);
	m0_tl_teardown(rcnf_herd, &rconfc->rc_herd, lnk)
		rconfc_herd_link_destroy(lnk);
	M0_LEAVE();
}

static int rconfc_herd_destroy(struct m0_rconfc *rconfc)
{
	int rc;

	M0_ENTRY();

	rconfc_active_all_unlink(rconfc);
	rc = rconfc_herd_fini(rconfc);
	if (rc == 0)
		rconfc_herd_prune(rconfc);

	return M0_RC(rc);
}

M0_INTERNAL struct rconfc_link *rconfc_herd_find(struct m0_rconfc *rconfc,
						 const char       *addr)
{
	M0_PRE(addr != NULL);
	return m0_tl_find(rcnf_herd, lnk, &rconfc->rc_herd,
				m0_streq(lnk->rl_confd_addr, addr));
}

/**
 * Allocates a herd of confc instances in accordance with the number of
 * addresses of confd servers to be in touch with.
 */
static int rconfc_herd_update(struct m0_rconfc   *rconfc,
			      const char        **confd_addr,
			      struct m0_fid_arr  *confd_fids)
{
	struct rconfc_link *lnk;
	uint32_t            count = rconfc_confd_count(confd_addr);
	uint32_t            idx;

	M0_ENTRY("rconfc = %p, confd_addr[] = %p, [confd_addr] = %u",
		 rconfc, confd_addr, count);
	M0_PRE(confd_addr != NULL);
	M0_PRE(count > 0);
	M0_PRE(count == confd_fids->af_count);
	M0_PRE(rconfc_confd_addr_are_all_unique(confd_addr));
	M0_PRE(m0_fid_arr_all_unique(confd_fids));

	for (idx = 0; *confd_addr != NULL; confd_addr++, idx++) {
		lnk = rconfc_herd_find(rconfc, *confd_addr);
		if (lnk == NULL) {
			M0_ALLOC_PTR(lnk);
			if (lnk == NULL)
				goto no_mem;
			/* add the allocated element to herd */
			lnk->rl_rconfc     = rconfc;
			lnk->rl_confd_fid  = confd_fids->af_elems[idx];
			lnk->rl_confd_addr = m0_strdup(*confd_addr);
			if (lnk->rl_confd_addr == NULL)
				goto no_mem;
			lnk->rl_state      = CONFC_DEAD;
			rcnf_herd_tlink_init_at_tail(lnk, &rconfc->rc_herd);
		/*
		 * } XXX: should we update the dead links here? It seems
		 *        they are not updated anywhere currently, so the
		 *        dead links get stuck dead forever even though
		 *        the confds may become Online already.
		 *
		 *        Beware also that HAlon returns in Entry Point
		 *        replies all the cluster confds regardless of their
		 *        states, i.e. the dead ones might be there as well.
		 * if (lnk->rl_state == CONFC_DEAD) {
		 */
			/*
			 * XXX:  rconfc_herd_link_init() can block on waiting
			 *       and this function is called from an AST:
			 *       rconfc_conductor_disconnected_ast() ->
			 *         rconfc_conductor_disconnected() ->
			 *           rconfc_start() ->
			 *               or:
			 *       rlock_owner_clink_cb() ->
			 *         rconfc_owner_creditor_reset() ->
			 *           rconfc_start() ->
			 *               or:
			 *         rconfc_start_ast_cb() ->
			 *           rconfc_start() ->
			 *             rconfc_start_internal() ->
			 *               rconfc_entrypoint_consume() ->
			 *                 rconfc_herd_update()
			 */
			rconfc_herd_link_init(lnk);
			/*
			 * only successfully connected @ref rconfc_link gets
			 * subscribed to HA notifications, and therefore, its
			 * confd fid is to be added to phony cache
			 */
			if (lnk->rl_state == CONFC_IDLE) {
				_confc_phony_cache_append(
					&rconfc->rc_phony,
					&lnk->rl_confd_fid);
				rconfc_herd_link_subscribe(lnk);
			} else {
				M0_ASSERT(lnk->rl_state == CONFC_DEAD);
			}
		}
		lnk->rl_preserve = true;
	}
	ver_accm_init(rconfc->rc_qctx, count);
	m0_tl_for(rcnf_herd, &rconfc->rc_herd, lnk) {
		if (!lnk->rl_preserve) {
			rconfc_herd_link_fini(lnk);
			rcnf_herd_tlist_del(lnk);
			rconfc_herd_link_destroy(lnk);
		} else {
			/*
			 * Clean confc cache, so version will be actually
			 * queried from confd, not extracted from cache.
			 */
			if (lnk->rl_state != CONFC_DEAD)
				_confc_cache_clean_lock(&lnk->rl_confc);
			lnk->rl_preserve = false;
		}
	} m0_tl_endfor;
	return m0_tl_exists(rcnf_herd, lnk, &rconfc->rc_herd,
			    lnk->rl_state != CONFC_DEAD) ? M0_RC(0) :
							   M0_ERR(-ENOENT);
no_mem:
	rconfc_herd_prune(rconfc);
	return M0_ERR(-ENOMEM);
}

static void rconfc_active_add(struct m0_rconfc *rconfc, struct rconfc_link *lnk)
{
	if (lnk->rl_state == CONFC_OPEN &&
	    _confc_ver_read(&lnk->rl_confc) == rconfc->rc_ver)
		rcnf_active_tlink_init_at_tail(lnk, &rconfc->rc_active);
}

/**
 * Re-populates the active list based on the herd items current
 * status. Population starts when quorum version is found.
 */
static void rconfc_active_populate(struct m0_rconfc *rconfc)
{
	struct rconfc_link *lnk;

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc->rc_ver != M0_CONF_VER_UNKNOWN);
	rconfc_active_all_unlink(rconfc);
	/* re-populate active list */
	m0_tl_for(rcnf_herd, &rconfc->rc_herd, lnk) {
		rconfc_active_add(rconfc, lnk);
	} m0_tl_endfor;
	M0_LEAVE();
}

/**
 * Connects "conductor" confc to the confd server identified by the provided
 * link. Initialises the confc in case it was not done before.
 */
static int rconfc_conductor_connect(struct m0_rconfc   *rconfc,
				    struct rconfc_link *lnk)
{
	/*
	 * As the conductor is expected to operate on rcnf_active list entries,
	 * i.e. previously known to be connectable alright, there is no need in
	 * any lengthy timeout here.
	 *
	 * In case active list iteration brings no success, rconfc is just to
	 * repeat version election starting with ENTRYPOINT request.
	 */
	enum { CONDUCTOR_TIMEOUT_DEFAULT = 200ULL * M0_TIME_ONE_MSEC };

	M0_ENTRY("rconfc = %p, lnk = %p, confd_addr = %s", rconfc, lnk,
		 lnk != NULL ? lnk->rl_confd_addr : "(null)");
	M0_PRE(rconfc != NULL);
	M0_PRE(lnk    != NULL);
	if (!m0_confc_is_inited(&rconfc->rc_confc)) {
		int rc;
		/* first use, initialization required */
		M0_PRE(_confc_ver_read(&rconfc->rc_confc) ==
		       M0_CONF_VER_UNKNOWN);
		rc = m0_confc_init_wait(&rconfc->rc_confc, rconfc->rc_sm.sm_grp,
					lnk->rl_confd_addr, rconfc->rc_rmach,
					rconfc->rc_local_conf,
					CONDUCTOR_TIMEOUT_DEFAULT);
		if (rc != 0)
			return M0_ERR(rc);
		m0_confc_gate_ops_set(&rconfc->rc_confc, &rconfc->rc_gops);
	}
	return M0_RC(m0_confc_reconnect(&rconfc->rc_confc, rconfc->rc_rmach,
					lnk->rl_confd_addr));
}

/**
 * Iterates through active list entries and tries to connect next confd
 * address. Finishes with either connection succeeded or list exhausted.
 *
 * The intended effect is that all spare confc items are going to be marked
 * CONFC_IDLE with the following exceptions:
 * - the newly connected item is marked CONFC_OPEN
 * - the previously connected item, i.e. the one which failure led to the
 * iteration, is marked CONFC_FAILED
 * - every item found non-responsive during the iteration is marked
 * CONFC_FAILED
 *
 * @note All CONFC_FAILED items are going to be re-set to CONFC_IDLE, and due to
 * this, re-tried when next iteration starts.
 */
static int rconfc_conductor_iterate(struct m0_rconfc *rconfc)
{
	struct rconfc_link *next;
	struct rconfc_link *prev;
	const char         *confd_addr;
	int                 rc;

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc != NULL);
	M0_PRE(rconfc->rc_ver != M0_CONF_VER_UNKNOWN);
	confd_addr = _confc_remote_addr_read(&rconfc->rc_confc) ?: "";
	M0_PRE((prev = m0_tl_find(rcnf_active, item, &rconfc->rc_active,
				  m0_streq(confd_addr, item->rl_confd_addr)))
	       == NULL || prev->rl_state == CONFC_OPEN);
	/* mark items idle except the failed one */
	m0_tl_for(rcnf_active, &rconfc->rc_active, next) {
		M0_PRE(next->rl_state != CONFC_DEAD);
		next->rl_state = m0_streq(confd_addr, next->rl_confd_addr) ?
			CONFC_FAILED : CONFC_IDLE;
	} m0_tl_endfor;
	/* start from the last connected item, or from list tail otherwise */
	prev = m0_tl_find(rcnf_active, item, &rconfc->rc_active,
			  m0_streq(confd_addr, item->rl_confd_addr)) ?:
		rcnf_active_tlist_tail(&rconfc->rc_active);
	while (1) {
		/*
		 * loop through the list until successful connect or no more
		 * idle items to try
		 */
		next = rcnf_active_tlist_next(&rconfc->rc_active, prev) ?:
			rcnf_active_tlist_head(&rconfc->rc_active);
		if (next->rl_state == CONFC_FAILED)
			/* this is to start version reelection */
			return M0_ERR(-ENOENT);
		rc = rconfc_conductor_connect(rconfc, next);
		if (rc == 0 && !M0_FI_ENABLED("conductor_conn_fail")) {
			next->rl_state = CONFC_OPEN;
			return M0_RC(rc);
		}
		M0_LOG(M0_ERROR, "Failed to connect to confd_addr = %s rc = %d",
		       next->rl_confd_addr, rc);
		next->rl_state = CONFC_FAILED;
		prev = next;
	}
	/**
	 * @note The iteration procedure above performs synchronous connection
	 * in rconfc_conductor_connect() with every remote confd in rcnf_active
	 * list until the connection succeeds.  Every failed case is going to
	 * take some time until CONDUCTOR_TIMEOUT_DEFAULT expires. Theoretically
	 * this may keep rconfc AST thread busy for some time while conductor is
	 * still trying to get to connected state.
	 *
	 * It is worth mentioning that the iteration is done with empty
	 * conductor's cache, so there are no consumers trying to access conf
	 * objects from other threads while rconfc remains locked.  This may
	 * diminish the influence of possible blocking effect of the synchronous
	 * connection approach.
	 *
	 * Nevertheless, this synchronism may be a subject for future rconfc
	 * redesign in case any negative effects get revealed.
	 *
	 * New design is to partition the rconfc_conductor_iterate() logic. The
	 * first part preceding the while() above is to prepare iteration and
	 * start iterator FOM that is to re-implement the logic of the while()
	 * block. rconfc_version_elected() is going to be affected as well.  New
	 * design is going to bring extra complexity into existent rconfc
	 * design.
	 *
	 * However, the redesign must take into consideration the fact that
	 * rconfc_gate_skip() re-uses the rconfc_conductor_iterate() logic and
	 * currently expects it to be performed exactly synchronous way. So, the
	 * redesign must be done not only in regard to post-election phase
	 * (rconfc_version_elected() partitioning), but asynchronous confc
	 * gating must be designed as well.
	 *
	 * Again, it is worth mentioning that with current logic of full conf
	 * loading before rconfc announces the conf ready, the confd skipping
	 * may occur only during conf loading, when it is guaranteed to have no
	 * conf consumers accessing objects in cache at the time.
	 */
}

static void rconfc_read_lock_get(struct m0_rconfc *rconfc)
{
	struct rlock_ctx      *rlx;
	struct m0_rm_incoming *req;

	M0_ENTRY("rconfc = %p", rconfc);
	rconfc_state_set(rconfc, M0_RCS_GET_RLOCK);
	rlx = rconfc->rc_rlock_ctx;
	req = &rlx->rlc_req;
	m0_rm_rwlock_req_init(req, &rlx->rlc_owner, &m0_rconfc_ri_ops,
			      RIF_MAY_BORROW | RIF_MAY_REVOKE, RM_RWLOCK_READ);
	m0_rm_credit_get(req);
	M0_LEAVE();
}

static void
rconfc_entrypoint_debug_print(struct m0_ha_entrypoint_rep *entrypoint)
{
	int i;

	M0_LOG(M0_DEBUG, "hbp_quorum=%"PRIu32" confd_nr=%"PRIu32,
	       entrypoint->hae_quorum, entrypoint->hae_confd_fids.af_count);
	M0_LOG(M0_DEBUG, "hbp_active_rm_fid="FID_F" hbp_active_rm_ep=%s",
	       FID_P(&entrypoint->hae_active_rm_fid),
	       entrypoint->hae_active_rm_ep);
	for (i = 0; entrypoint->hae_confd_eps[i] != NULL; ++i) {
		M0_LOG(M0_DEBUG, "hbp_confd_fids[%d]="FID_F
		       " hbp_confd_eps[%d]=%s",
		       i, FID_P(&entrypoint->hae_confd_fids.af_elems[i]),
		       i, entrypoint->hae_confd_eps[i]);
	}
}

static int rconfc_entrypoint_consume(struct m0_rconfc *rconfc)
{
	struct rlock_ctx               *rlx = rconfc->rc_rlock_ctx;
	struct m0_confc                *phony = &rlx->rlc_parent->rc_phony;
	struct m0_ha_entrypoint_rep    *hep = &rconfc->rc_ha_entrypoint_rep;
	int                             rc;

	M0_ENTRY();
	rconfc_state_set(rconfc, M0_RCS_ENTRYPOINT_CONSUME);
	if (hep->hae_control == M0_HA_ENTRYPOINT_QUIT)
		/* HA commanded stop querying. No operation permitted. */
		return M0_ERR(-EPERM);
	if (!m0_fid_is_set(&hep->hae_active_rm_fid))
		/*
		 * The situation when active RM fid is unset cannot happen
		 * during regular cluster operation. Whichever reason actually
		 * caused the fid to be zero, it must be considered an
		 * unrecoverable issue preventing rconfc from further running.
		 */
		return M0_ERR(-ENOKEY);
	rconfc->rc_quorum = hep->hae_quorum;
	rlx->rlc_rm_fid = hep->hae_active_rm_fid;
	M0_LOG(M0_DEBUG, "rm_fid="FID_F, FID_P(&rlx->rlc_rm_fid));
	if (hep->hae_active_rm_ep == NULL || hep->hae_active_rm_ep[0] == '\0')
		return M0_ERR(-ENOENT);
	rconfc_entrypoint_debug_print(hep);
	_confc_phony_cache_append(phony, &rlx->rlc_rm_fid);

	/*
	 * rconfc SM channel is used for the convenience here:
	 * 1) No additional channel and mutex structures are required.
	 * 2) No additional locks are required here since the code is always
	 *    run under the same SM group lock.
	 * The drawback of this approach is that the threads waiting
	 * for the SM state change (like at m0_rconfc_start_wait()) will be
	 * awaken needlessly. But this seems to be harmless.
	 */
	m0_clink_add(&rconfc->rc_sm.sm_chan, &rconfc->rc_ha_update_cl);

	rc = rconfc_herd_update(rconfc, hep->hae_confd_eps,
				&hep->hae_confd_fids) ?:
	     m0_conf_confc_ha_update_async(&rconfc->rc_phony,
					   &rconfc->rc_nvec,
					   &rconfc->rc_sm.sm_chan);

	if (rc != 0) {
		m0_clink_del(&rconfc->rc_ha_update_cl);
		return M0_ERR(rc);
	}

	return M0_RC(rc);
}

static int rconfc_start_internal(struct m0_rconfc *rconfc)
{
	int               rc;

	M0_ENTRY();

	rc = rconfc_entrypoint_consume(rconfc);
	if (rc != 0) {
		if (M0_IN(rc, (-ENOKEY, -EPERM)))
			/* HA requested rconfc to stop querying entrypoint */
			rconfc_fail(rconfc, rc);
		else
			/* rconfc is to keep trying with next entrypoint */
			rconfc_herd_destroy(rconfc);
		return M0_ERR(rc);
	}
	return M0_RC(rc);
}

static void rconfc_start(struct m0_rconfc *rconfc)
{
	struct m0_ha                   *ha  = m0_get()->i_ha;
	struct m0_ha_entrypoint_client *ecl = &ha->h_entrypoint_client;
	int                             rc  = 0;

	M0_PRE(rconfc->rc_local_conf == NULL);

	M0_ENTRY();

	while (rconfc_state(rconfc) != M0_RCS_FAILURE) {
		rconfc_state_set(rconfc, M0_RCS_ENTRYPOINT_WAIT);

		if (rconfc->rc_ha_entrypoint_rep.hae_control == M0_HA_ENTRYPOINT_QUERY) {
			M0_LOG(M0_DEBUG, "Querying ENTRYPOINT...");
			m0_ha_entrypoint_client_request(ecl);
			break;
		}

		M0_LOG(M0_DEBUG, "ENTRYPOINT ready...");
		rc = rconfc_start_internal(rconfc);
		rconfc->rc_ha_entrypoint_rep.hae_control = M0_HA_ENTRYPOINT_QUERY;
		if (rc == 0)
			break;
		m0_ha_entrypoint_rep_free(&rconfc->rc_ha_entrypoint_rep);
		M0_LOG(M0_DEBUG, "Try reconnecting after rc=%d", rc);
		M0_CNT_INC(rconfc->rc_ha_entrypoint_retries);
	}

	M0_LEAVE("rc=%d hae_control=%d is_failed=%s entrypoint_retries=%"PRIu32,
		 rc, rconfc->rc_ha_entrypoint_rep.hae_control,
		 m0_bool_to_str(rconfc_state(rconfc) == M0_RCS_FAILURE),
		 rconfc->rc_ha_entrypoint_retries);
}

static void rconfc_start_ast_cb(struct m0_sm_group *grp M0_UNUSED,
				struct m0_sm_ast   *ast)
{
	struct m0_rconfc *rconfc = ast->sa_datum;

	M0_ENTRY();
	rconfc_start(rconfc);
	M0_LEAVE();
}

static void rconfc_owner_creditor_reset(struct m0_sm_group *grp M0_UNUSED,
					struct m0_sm_ast   *ast)
{
	struct m0_rconfc *rconfc = ast->sa_datum;
	struct rlock_ctx *rlx    = rconfc->rc_rlock_ctx;

	M0_ENTRY("rconfc = %p", rconfc);
	rlock_ctx_creditor_unset(rlx);
	/*
	 * Start conf reelection.
	 * RM creditor is likely to be changed by HA.
	 */
	rconfc_start(rconfc);

	M0_LEAVE();
}

static bool rlock_owner_clink_cb(struct m0_clink *cl)
{
	struct rlock_ctx *rlx = container_of(cl, struct rlock_ctx, rlc_clink);
	uint32_t          state = rlx->rlc_owner.ro_sm.sm_state;
	struct m0_rconfc *rconfc = rlx->rlc_parent;

	M0_ASSERT(state != ROS_INSOLVENT);
	if (state == ROS_DEAD_CREDITOR) {
		rconfc_ast_post(rconfc, rconfc_owner_creditor_reset);
		m0_clink_del(cl);
		m0_clink_fini(cl);
	}
	return true;
}

static void rconfc_creditor_death_handle(struct m0_rconfc *rconfc)
{
	struct rlock_ctx   *rlx = rconfc->rc_rlock_ctx;
	struct m0_rm_owner *owner = &rlx->rlc_owner;

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rlock_ctx_creditor_state(rlx) != ROS_ACTIVE);
	if (owner->ro_sm.sm_state == ROS_DEAD_CREDITOR) {
		rconfc_ast_post(rconfc, rconfc_owner_creditor_reset);
	} else {
		/* Wait until owner is in ROS_DEAD_CREDITOR state. */
		m0_clink_init(&rlx->rlc_clink, rlock_owner_clink_cb);
		m0_clink_add(&owner->ro_sm.sm_chan, &rlx->rlc_clink);
	}
	M0_LEAVE();
}

static void rconfc_conductor_disconnected(struct m0_rconfc *rconfc)
{
	struct rlock_ctx   *rlx   = rconfc->rc_rlock_ctx;
	struct m0_rm_owner *owner = &rlx->rlc_owner;

	M0_ENTRY("rconfc = %p", rconfc);
	/* return read lock back to RM */
	rconfc_read_lock_put(rconfc);
	/* prepare for version election */
	rconfc_active_all_unlink(rconfc);
	rconfc->rc_ver = M0_CONF_VER_UNKNOWN;
	if (rlock_ctx_creditor_state(rlx) != ROS_ACTIVE) {
		m0_rm_owner_lock(owner);
		/* Creditor is considered dead by HA */
		rconfc_creditor_death_handle(rconfc);
		m0_rm_owner_unlock(owner);
	} else {
		/*
		 * Start process of conf reelection.
		 * List of confd or active RM could possibly have changed.
		 */
		rconfc_start(rconfc);
	}
	M0_LEAVE();
}

static void rconfc_conductor_disconnected_ast(struct m0_sm_group *grp,
					      struct m0_sm_ast   *ast)
{
	struct m0_rconfc *rconfc = ast->sa_datum;

	m0_clink_cleanup(&rconfc->rc_conductor_clink);
	m0_rpc_link_fini(&rconfc->rc_confc.cc_rlink);
	rconfc_conductor_disconnected(rconfc);
}

static bool rconfc_conductor_disconnect_cb(struct m0_clink *clink)
{
	struct m0_rconfc *rconfc = M0_AMB(rconfc, clink, rc_conductor_clink);

	M0_ENTRY("rconfc = %p", rconfc);
	rconfc_ast_post(rconfc, rconfc_conductor_disconnected_ast);
	M0_LEAVE();
	return true;
}

static void rconfc_conductor_drained(struct m0_rconfc *rconfc)
{
	M0_ENTRY("rconfc = %p", rconfc);
	/* disconnect confc until read lock being granted */
	rconfc_state_set(rconfc, M0_RCS_CONDUCTOR_DISCONNECT);
	m0_clink_init(&rconfc->rc_conductor_clink,
		      rconfc_conductor_disconnect_cb);
	rconfc->rc_conductor_clink.cl_is_oneshot = true;
	m0_rpc_link_disconnect_async(&rconfc->rc_confc.cc_rlink,
				     m0_rpc__down_timeout(),
				     &rconfc->rc_conductor_clink);
	M0_LEAVE();
}

/**
 * Drain confc cache because of read lock conflict.
 *
 * Waits for cache objects being entirely unpinned. This is done by just waiting
 * on a first pinned object met in cache. When the one appears unpinned, the
 * checking for other objects is repeated until no more pinned object
 * remains. Finally makes the cache empty.
 */
static void rconfc_conductor_drain(struct m0_sm_group *grp,
				   struct m0_sm_ast   *ast)
{
	struct m0_rconfc     *rconfc = ast->sa_datum;
	struct m0_conf_cache *cache = &rconfc->rc_confc.cc_cache;
	struct m0_conf_obj   *obj;
	int                   rc = 1;

	M0_ENTRY("rconfc = %p", rconfc);
	m0_conf_cache_lock(cache);
	if ((obj = m0_conf_cache_pinned(cache)) != NULL) {
		M0_LOG(M0_DEBUG, "* pinned (%"PRIu64") obj "FID_F", "
		       "waiters %d, ha %d", obj->co_nrefs, FID_P(&obj->co_id),
		       obj->co_chan.ch_waiters, obj->co_ha_chan.ch_waiters);
		m0_clink_add(&obj->co_chan, &rconfc->rc_unpinned_cl);
	} else {
		rc = _confc_cache_clean(&rconfc->rc_confc);
		if (rc != 0)
			rconfc_fail(rconfc, rc);
	}
	m0_conf_cache_unlock(cache);

	if (rc == 0)
		rconfc->rc_stopping ? rconfc_state_set(rconfc, M0_RCS_FINAL) :
				      rconfc_conductor_drained(rconfc);
	M0_LEAVE("rc=%d, stopping=%d", rc, rconfc->rc_stopping);
}

static bool rconfc_unpinned_cb(struct m0_clink *link)
{
	struct m0_rconfc *rconfc = container_of(
				link, struct m0_rconfc, rc_unpinned_cl);

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc_state(rconfc) == M0_RCS_CONDUCTOR_DRAIN);
	m0_clink_del(link);
	rconfc_ast_post(rconfc, rconfc_conductor_drain);
	M0_LEAVE();
	return false;
}

/**
 * Called during m0_confc_ctx_init(). Confc context initialisation appears
 * blocked until rconfc allows read operations.
 *
 * @note Caller is blocked until rconfc reelection is finished. Caller
 * (e.g. ios start sm) might be in the same sm group with rconfc sm group.
 * So, m0_rconfc_lock() and m0_rconfc_unlock() will use the recursive version
 * of sm group lock/unlock respectively.
 *
 * @see m0_confc_gate_ops::go_check
 */
static bool rconfc_gate_check(struct m0_confc *confc)
{
	struct m0_rconfc *rconfc;
	bool              result;

	M0_ENTRY("confc = %p", confc);
	M0_PRE(confc != NULL);
	M0_PRE(m0_mutex_is_locked(&confc->cc_lock));

	rconfc = container_of(confc, struct m0_rconfc, rc_confc);
	if (!m0_rconfc_reading_is_allowed(rconfc)) {
		m0_mutex_unlock(&confc->cc_lock);
		m0_rconfc_lock(rconfc);
		m0_sm_timedwait(&rconfc->rc_sm,
				M0_BITS(M0_RCS_IDLE, M0_RCS_FAILURE),
				M0_TIME_NEVER);
		m0_rconfc_unlock(rconfc);
		m0_mutex_lock(&confc->cc_lock);
	}
	result = m0_rconfc_reading_is_allowed(rconfc);
	M0_LEAVE("result=%s", result ? "true" : "false");
	return result;
}

/**
 * Called from configuration reading state machine being in S_SKIP_CONFD state.
 *
 * @see m0_confc_gate_ops::go_skip
 */
static int rconfc_gate_skip(struct m0_confc *confc)
{
	struct m0_rconfc *rconfc;

	M0_ENTRY("confc = %p", confc);
	M0_PRE(confc != NULL);
	rconfc = container_of(confc, struct m0_rconfc, rc_confc);
	return M0_RC(rconfc_conductor_iterate(rconfc));
}

/**
 * Called when all confc contexts are detached from conductor confc,
 * and therefore its cache can be cleaned.
 *
 * @see m0_confc_gate_ops::go_drain
 */
static bool rconfc_gate_drain(struct m0_clink *clink)
{
	struct m0_rconfc *rconfc;
	struct m0_confc  *confc;

	M0_ENTRY("clink = %p", clink);
	confc = container_of(clink, struct m0_confc, cc_drain);
	rconfc = container_of(confc, struct m0_rconfc, rc_confc);
	rconfc->rc_gops.go_drain = NULL;
	m0_confc_gate_ops_set(&rconfc->rc_confc, &rconfc->rc_gops);
	/*
	 * Conductor confc has no active confc contexts at this moment,
	 * so it's safe to call rconfc_ast_drain() in this sm group.
	 *
	 * If several rconfc operate in one sm group, then processing
	 * 'foreign' confc contexts is stopped for some time, but let's live
	 * with that for now, because no dead-locks are possible.
	 */
	rconfc_state_set(rconfc, M0_RCS_CONDUCTOR_DRAIN);
	rconfc_ast_post(rconfc, rconfc_conductor_drain);
	M0_LEAVE();
	return false; /* leave this event "unconsumed" */
}

static void rlock_conflict_handle(struct m0_sm_group *grp,
				  struct m0_sm_ast   *ast)
{
	struct m0_rconfc *rconfc = ast->sa_datum;

	M0_ENTRY("rconfc = %p, expired_cb = %p, ready_cb = %p", rconfc,
		 rconfc->rc_expired_cb, rconfc->rc_ready_cb);
	M0_PRE(rconfc_is_locked(rconfc));
	/* prepare for emptying conductor's cache */
	if (rconfc->rc_expired_cb != NULL)
		rconfc->rc_expired_cb(rconfc);
	/*
	 * if no context attached, call it directly, otherwise it is
	 * going to be called during the very last context finalisation
	 */
	m0_mutex_lock(&rconfc->rc_confc.cc_lock);
	if (rconfc->rc_confc.cc_nr_ctx == 0) {
		rconfc_state_set(rconfc, M0_RCS_CONDUCTOR_DRAIN);
		rconfc_ast_post(rconfc, rconfc_conductor_drain);
	} else {
		rconfc->rc_gops.go_drain = m0_rconfc_gate_ops.go_drain;
		m0_confc_gate_ops_set(&rconfc->rc_confc,
				      &rconfc->rc_gops);
	}
	m0_mutex_unlock(&rconfc->rc_confc.cc_lock);
	M0_LEAVE();
}

static void rconfc_rlock_windup(struct m0_rconfc *rconfc)
{
	struct rlock_ctx *rlx = rconfc->rc_rlock_ctx;

	M0_ENTRY("rconfc = %p", rconfc);
	/**
	 * Release sm group lock to prevent dead-lock with
	 * rconfc_read_lock_conflict. In worst case rconfc_rlock_windup()
	 * acquires locks in "rconfc sm group lock"->"rm owner lock" order and
	 * rconfc_read_lock_conflict in reverse order.
	 */
	m0_rconfc_unlock(rconfc);
	rconfc_read_lock_put(rconfc);
	m0_rconfc_lock(rconfc);
	if (rlock_ctx_creditor_state(rlx) == ROS_ACTIVE)
		rlock_ctx_owner_windup(rlx);
	m0_rm_rwlock_owner_fini(&rlx->rlc_owner);
	M0_LEAVE();
}

static void rconfc_stop_internal(struct m0_rconfc *rconfc)
{
	struct rlock_ctx *rlx = rconfc->rc_rlock_ctx;
	int               rc;

	M0_ENTRY("rconfc = %p", rconfc);
	if (rconfc->rc_sm.sm_state == M0_RCS_FINAL)
		goto exit;
	/*
	 * This function may be called several times from rconfc_herd_fini_cb().
	 * It is possible that rconfc already in M0_RCS_STOPPING state.
	 */
	if (rconfc->rc_sm.sm_state != M0_RCS_STOPPING)
		rconfc_state_set(rconfc, M0_RCS_STOPPING);
	rc = rconfc_herd_fini(rconfc);
	if (rc != 0)
		goto exit;
	rconfc_rlock_windup(rconfc);
	if (rlock_ctx_is_online(rlx))
		rlock_ctx_creditor_unset(rlx);
	rconfc_state_set(rconfc, M0_RCS_CONDUCTOR_DRAIN);
	if (!m0_confc_is_inited(&rconfc->rc_confc)) {
		rconfc_state_set(rconfc, M0_RCS_FINAL);
		goto exit;
	}
	rconfc_ast_post(rconfc, rconfc_conductor_drain);
exit:
	M0_LEAVE();
}

static void rconfc_stop_ast_cb(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_rconfc *rconfc = ast->sa_datum;

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc_is_locked(rconfc));
	rconfc->rc_stopping = true;
	if (rconfc->rc_expired_cb != NULL)
		rconfc->rc_expired_cb(rconfc);
	rconfc_stop_internal(rconfc);
	M0_LEAVE();
}

/**
 * Called when a conflicting lock acquisition is initiated somewhere in the
 * cluster. All conflicting read locks get revoked as the result firing
 * m0_rm_incoming_ops::rio_conflict() events for resource borrowers. Deprived of
 * read lock, rconfc disallows any configuration reading. As well, the cached
 * data appears outdated, and has to be dropped.
 */
static void rconfc_read_lock_conflict(struct m0_rm_incoming *in)
{
	struct m0_rconfc *rconfc;

	M0_ENTRY("in = %p", in);
	rconfc = rlock_ctx_incoming_to_rconfc(in);
	m0_rconfc_lock(rconfc);
	if (rconfc_state(rconfc) == M0_RCS_IDLE) {
		rconfc_state_set(rconfc, M0_RCS_RLOCK_CONFLICT);
		rconfc_ast_post(rconfc, rlock_conflict_handle);
	} else {
		rconfc->rc_rlock_conflict = true;
	}
	m0_rconfc_unlock(rconfc);
	M0_LEAVE();
}

static void rconfc_idle(struct m0_rconfc *rconfc)
{
	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc_is_locked(rconfc));
	if (rconfc->rc_stopping) {
		rconfc_stop_internal(rconfc);
		M0_LEAVE("Stopped internally");
		return;
	}
	if (rconfc->rc_rlock_conflict) {
		rconfc->rc_rlock_conflict = false;
		rconfc_state_set(rconfc, M0_RCS_RLOCK_CONFLICT);
		rconfc_ast_post(rconfc, rlock_conflict_handle);
		M0_LEAVE("Conflict to be handled...");
		return;
	}
	/*
	 * If both rc_stopping and rc_rlock_conflict flags aren't set, then
	 * SM will be idle until read lock conflict is observed provoking
	 * reelection or user requests stopping rconfc.
	 */
	rconfc_state_set(rconfc, M0_RCS_IDLE);
	if (rconfc->rc_ready_cb != NULL)
		rconfc->rc_ready_cb(rconfc);
	M0_LEAVE("Idle");
}

static bool rconfc_quorum_is_possible(struct m0_rconfc *rconfc)
{
	struct ver_accm *va = rconfc->rc_qctx;
	int              ver_count_max;
	int              armed_count;

	M0_PRE(rconfc_is_locked(rconfc));
	armed_count = m0_tl_reduce(rcnf_herd, lnk, &rconfc->rc_herd, 0,
				   + (lnk->rl_state == CONFC_ARMED));
	ver_count_max = m0_fold(idx, acc, va->va_count, 0,
				max_type(int, acc, va->va_items[idx].vi_count));
	if (ver_count_max + armed_count < rconfc->rc_quorum) {
		M0_LOG(M0_WARN, "No chance left to reach the quorum");
		rconfc->rc_ver = M0_CONF_VER_UNKNOWN;
		/* Notify consumer about conf expired */
		if (rconfc->rc_expired_cb != NULL)
			rconfc->rc_expired_cb(rconfc);
		return false;
	}
	return true;
}

/**
 * Function tests if quorum reached to the moment.
 */
static bool rconfc_quorum_test(struct m0_rconfc *rconfc,
			       struct m0_confc *confc)
{
	struct ver_accm *va = rconfc->rc_qctx;
	struct ver_item *vi = NULL;
	uint64_t         ver;
	int              idx;
	bool             quorum_reached = false;

	M0_ENTRY("rconfc = %p, confc = %p", rconfc, confc);
	M0_PRE(va != NULL);
	ver = _confc_ver_read(confc);
	M0_ASSERT(ver != M0_CONF_VER_UNKNOWN);
	for (idx = 0; idx < va->va_count; idx++) {
		if (va->va_items[idx].vi_ver == ver) {
			vi = va->va_items + idx;
			break;
		}
	}
	if (vi == NULL) {
		/* new version appeared */
		M0_ASSERT(va->va_count < va->va_total);
		vi = va->va_items + va->va_count;
		M0_PRE(vi->vi_ver == 0);
		M0_PRE(vi->vi_count == 0);
		vi->vi_ver = ver;
		++va->va_count;
	}
	++vi->vi_count;

	/* Walk along the herd and see if quorum of any version is reached. */
	for (idx = 0; idx < va->va_count; idx++) {
		if (va->va_items[idx].vi_count >= rconfc->rc_quorum) {
			/* remember the winner */
			rconfc->rc_ver = va->va_items[idx].vi_ver;
			quorum_reached = true;
			break;
		}
	}
	M0_LEAVE("quorum %sreached", quorum_reached ? "" : "not ");
	return quorum_reached;
}

/**
 * Puts "conductor" confc in effect. In case the confc is not initialised yet,
 * the initialisation happens first inside rconfc_conductor_connect(). Then
 * connection to the very first responsive confd from active list is
 * established. Ultimately, consumer is notified about configuration expiration
 * by calling callback provided during m0_rconfc_init().
 */
static int rconfc_conductor_engage(struct m0_rconfc *rconfc)
{
	int rc = 0;

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc != NULL);
	M0_PRE(rconfc->rc_ver != M0_CONF_VER_UNKNOWN);
	/*
	 * See if the confc not initialized yet, or having different
	 * version compared to the newly elected one
	 */
	if (!m0_confc_is_inited(&rconfc->rc_confc) ||
	    _confc_ver_read(&rconfc->rc_confc) != rconfc->rc_ver) {
		/* need to connect conductor to a new version confd */
		rc = rconfc_conductor_iterate(rconfc);
	}
	return M0_RC(rc);
}

/**
 * Finalises one completed confc context.
 * Scheduled from rconfc__cb_quorum_test().
 */
static void rconfc_cctx_fini(struct m0_sm_group *grp,
			     struct m0_sm_ast   *ast)
{
	struct rconfc_link *lnk = ast->sa_datum;

	M0_ASSERT(lnk->rl_state != CONFC_IDLE);
	/*
	 * The context might become complete just at the time
	 * of rconfc_version_elected() execution and be
	 * finalised from rconfc_herd_cctxs_fini() already.
	 */
	if (m0_clink_is_armed(&lnk->rl_clink)) {
		m0_clink_del(&lnk->rl_clink);
		m0_clink_fini(&lnk->rl_clink);
		m0_confc_ctx_fini_locked(&lnk->rl_cctx);
	}
}

/**
 * Finalises all completed confc contexts from the herd.
 */
static void rconfc_herd_cctxs_fini(struct m0_rconfc *rconfc)
{
	struct rconfc_link *lnk;

	m0_tl_for(rcnf_herd, &rconfc->rc_herd, lnk) {
		if (lnk->rl_state == CONFC_DEAD)
			/*
			 * Even with version elected some links may remain dead
			 * in the herd and require no finalisation. Dead link is
			 * a link that failed to connect to the respective confd
			 * during version election.
			 */
			continue;
		M0_ASSERT(lnk->rl_state != CONFC_IDLE);
		/*
		 * rconfc_cctx_fini() might be called already for some
		 * of the contexts (because ASTs are not always executed
		 * in the same order they were scheduled).
		 */
		if (m0_clink_is_armed(&lnk->rl_clink) &&
		    m0_confc_ctx_is_completed(&lnk->rl_cctx)) {
			m0_clink_del(&lnk->rl_clink);
			m0_clink_fini(&lnk->rl_clink);
			m0_confc_ctx_fini_locked(&lnk->rl_cctx);
		}
	} m0_tl_endfor;
}

static void rconfc_version_elected(struct m0_sm_group *grp,
				   struct m0_sm_ast   *ast)
{
	struct m0_rconfc   *rconfc = ast->sa_datum;
	int                 rc;

	M0_PRE(rconfc_is_locked(rconfc));
	M0_ENTRY("rconfc = %p", rconfc);

	rconfc_herd_cctxs_fini(rconfc);

	rc = rconfc_quorum_is_reached(rconfc) ?
		rconfc_conductor_engage(rconfc) : -EPROTO;
	if (rc != 0) {
		M0_ERR_INFO(rc, "re-election started");
		rc = rconfc_herd_destroy(rconfc);
		if (rc != 0) {
			M0_ERR_INFO(rc, "herd_destroy() failed");
			goto out;
		}
		/*
		 * Start re-election. As version was not elected, the conductor
		 * never was engaged, and therefore needs no draining...
		 */
		if (m0_confc_is_inited(&rconfc->rc_confc))
			m0_confc_fini(&rconfc->rc_confc);
		/* ... and no disconnection. */
		rconfc_conductor_disconnected(rconfc);
		M0_CNT_INC(rconfc->rc_ha_entrypoint_retries);
	} else {
		M0_SET0(&rconfc->rc_rx);
		M0_SET0(&rconfc->rc_load_ast);
		M0_SET0(&rconfc->rc_load_fini_ast);
		/*
		 * disable gating operations to let rc_confc read configuration
		 * before rconfc gets to M0_RCS_IDLE state
		 */
		m0_confc_gate_ops_set(&rconfc->rc_confc, NULL);
		/* conf load kick-off */
		rconfc->rc_load_ast.sa_datum = rconfc;
		rconfc->rc_load_ast.sa_cb    = rconfc_conf_full_load;
		/*
		 * make conf reading occur in a thread other than standard group
		 * context rconfc was initialised with
		 *
		 * Note: this trick is absolutely temporary, and needs to be
		 * eliminated later when conf clients learn to handle conf
		 * updates on their own
		 */
		rconfc_load_ast_thread_init(&rconfc->rc_rx);
		m0_sm_ast_post(&rconfc->rc_rx.rx_grp, &rconfc->rc_load_ast);
	}
out:
	M0_LEAVE();
}

/**
 * Callback attached to confc context clink. Fires when reading from one of
 * confd instances is done, and therefore, the confd version is known to the
 * moment of context completion. When context is complete, the entire herd is
 * tested for quorum.
 *
 * @note Even after initialisation completion late confc replies yet still
 * possible and to be done in background filling in the active list.
 *
 * @note Callback is executed in context of rconfc sm group, because
 * corresponding confc was inited with rconfc sm group. And this group
 * is used for all confc contexts attached to confc later.
 */
static bool rconfc__cb_quorum_test(struct m0_clink *clink)
{
	struct rconfc_link *lnk;
	struct m0_rconfc   *rconfc;
	bool                quorum_was;
	bool                quorum_is = false;

	M0_ENTRY("clink = %p", clink);
	M0_PRE(clink != NULL);
	lnk = container_of(clink, struct rconfc_link, rl_clink);
	M0_ASSERT(lnk->rl_state == CONFC_ARMED);
	rconfc = lnk->rl_rconfc;
	M0_ASSERT(rconfc_is_locked(rconfc));

	if (m0_confc_ctx_is_completed(&lnk->rl_cctx)) {
		lnk->rl_rc = m0_confc_ctx_error(&lnk->rl_cctx);
		if (M0_FI_ENABLED("read_ver_failed")) {
			lnk->rl_confc.cc_cache.ca_ver = M0_CONF_VER_UNKNOWN;
			lnk->rl_rc = -ENODATA;
		}
		lnk->rl_state = lnk->rl_rc == 0 ? CONFC_OPEN : CONFC_FAILED;

		/*
		 * The code may be called after quorum was already
		 * reached, so we need to see if it was
		 */
		quorum_was = rconfc_quorum_is_reached(rconfc);

		if (lnk->rl_state == CONFC_FAILED)
			M0_LOG(M0_DEBUG, "Lnk failed, rc = %d, ep = %s",
			       lnk->rl_rc, lnk->rl_confd_addr);
		else if (!quorum_was)
			quorum_is = rconfc_quorum_test(rconfc, &lnk->rl_confc);

		if (quorum_was)
			rconfc_active_add(rconfc, lnk);
		else if (quorum_is)
			rconfc_active_populate(rconfc);

		if (quorum_was)
			rconfc__ast_post(rconfc, lnk, rconfc_cctx_fini);
		else if (quorum_is || !rconfc_quorum_is_possible(rconfc))
			rconfc_ast_post(rconfc, rconfc_version_elected);
	}
	M0_LEAVE();
	return true;
}

/**
 * Version election start. Iterates through confc herd and makes every entry to
 * start asynchronous reading from corresponding confd.
 */
static void rconfc_version_elect(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_rconfc   *rconfc = ast->sa_datum;
	struct ver_accm    *va;
	struct rconfc_link *lnk;

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc != NULL && rconfc->rc_qctx != NULL);

	rconfc_state_set(rconfc, M0_RCS_VERSION_ELECT);
	va = rconfc->rc_qctx;
	M0_PRE(va->va_total != 0);
	va->va_count = 0;
	m0_forall(idx, va->va_total, M0_SET0(&va->va_items[idx]));
	/* query confd instances */
	m0_tl_for(rcnf_herd, &rconfc->rc_herd, lnk) {
		if (lnk->rl_state != CONFC_DEAD && lnk->rl_rc == 0) {
			m0_confc_ctx_init(&lnk->rl_cctx, &lnk->rl_confc);

			m0_clink_init(&lnk->rl_clink, rconfc__cb_quorum_test);
			m0_clink_add(&lnk->rl_cctx.fc_mach.sm_chan,
				     &lnk->rl_clink);
			lnk->rl_state = CONFC_ARMED;
			m0_confc_open(&lnk->rl_cctx, NULL,
				      M0_CONF_ROOT_PROFILES_FID);
		}
	} m0_tl_endfor;
	M0_LEAVE();
}

/**
 * Called when read lock request completes.
 *
 * @param in -- read request object
 * @param rc -- read request result code indicating success (rc == 0) or failure
 */
static void rconfc_read_lock_complete(struct m0_rm_incoming *in, int32_t rc)
{
	struct m0_rconfc *rconfc;
	struct rlock_ctx *rlx;

	M0_ENTRY("in = %p, rc = %d", in, rc);
	rconfc = rlock_ctx_incoming_to_rconfc(in);
	M0_ASSERT(rconfc_state(rconfc) == M0_RCS_GET_RLOCK);
	rlx = rconfc->rc_rlock_ctx;
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Read lock request failed with rc = %d", rc);
		/*
		 * RPC connection to RM may be lost, or there is no remote RM
		 * service anymore to respond to read lock request, so need to
		 * handle this to prevent read lock context from further
		 * communication attempts
		 */
		M0_ASSERT(rlock_ctx_is_online(rlx));
		rlock_ctx_creditor_unset(rlx);
	}

	if (M0_FI_ENABLED("rlock_req_failed"))
		rc = M0_ERR(-ESRCH);
	if (rc == 0)
		rconfc_ast_post(rconfc, rconfc_version_elect);
	else if (rlock_ctx_creditor_state(rlx) == ROS_ACTIVE)
		rconfc_fail_ast(rconfc, rc);
	else
		/* Creditor is considered dead by HA */
		rconfc_creditor_death_handle(rconfc);
	M0_LEAVE();
}

static int rconfc_local_load(struct m0_rconfc *rconfc)
{
	struct m0_conf_root *root = NULL;
	int                  rc;

	M0_ENTRY("rconfc %p, local_conf = '%s'", rconfc, rconfc->rc_local_conf);
	M0_PRE(rconfc->rc_local_conf != NULL && *rconfc->rc_local_conf != '\0');
	M0_PRE(!rconfc_is_locked(rconfc));
	rc = m0_confc_init(&rconfc->rc_confc, rconfc->rc_sm.sm_grp,
			   NULL, rconfc->rc_rmach, rconfc->rc_local_conf) ?:
		m0_confc_root_open(&rconfc->rc_confc, &root);
	m0_rconfc_lock(rconfc);
	if (rc != 0) {
		rconfc->rc_sm_state_on_abort = rconfc_state(rconfc);
		rconfc_fail(rconfc, rc);
	} else {
		rconfc->rc_ver = root->rt_verno;
		m0_confc_close(&root->rt_obj);
		if (rconfc->rc_ready_cb != NULL)
			rconfc->rc_ready_cb(rconfc);
	}
	m0_rconfc_unlock(rconfc);
	M0_LEAVE("rc=%d", rc);
	return M0_RC(rc);
}

/**************************************
 * Rconfc public interface
 **************************************/

M0_INTERNAL void m0_rconfc_lock(struct m0_rconfc *rconfc)
{
	/*
	 * Don't use m0_sm_group_lock() to ensure that ASTs posted via
	 * rconfc_ast_post() are executed in well-known context.
	 * m0_sm_group_lock() has side effect of running ASTs in current context
	 * after acquiring the mutex.
	 *
	 * We use the recursive version of sm group lock, because it might
	 * be called from ios start sm group AST, which uses the same sm group.
	 */
	m0_sm_group_lock_rec(rconfc->rc_sm.sm_grp, false);
}

M0_INTERNAL void m0_rconfc_unlock(struct m0_rconfc *rconfc)
{
	m0_sm_group_unlock_rec(rconfc->rc_sm.sm_grp, false);
}

M0_INTERNAL int m0_rconfc_init(struct m0_rconfc      *rconfc,
			       const struct m0_fid   *profile,
			       struct m0_sm_group    *sm_group,
			       struct m0_rpc_machine *rmach,
			       m0_rconfc_cb_t         expired_cb,
			       m0_rconfc_cb_t         ready_cb)
{
	int               rc;
	struct rlock_ctx *rlock_ctx;
	struct ver_accm  *va;
	struct m0_ha     *ha;


	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc != NULL);
	M0_PRE(rmach != NULL);
	M0_PRE(sm_group != NULL);

	M0_SET0(rconfc);

	M0_ALLOC_PTR(va);
	if (va == NULL)
		return M0_ERR(-ENOMEM);
	rc = rlock_ctx_create(rconfc, rmach, &rlock_ctx);
	if (rc != 0)
		goto rlock_err;
	rc = _confc_phony_init(&rconfc->rc_phony);
	if (rc != 0)
		goto confc_err;
	rconfc->rc_profile = *profile;
	rconfc->rc_rmach   = rmach;
	rconfc->rc_qctx    = va;
	rconfc->rc_ver     = M0_CONF_VER_UNKNOWN;
	rconfc->rc_gops = (struct m0_confc_gate_ops) {
		.go_check = m0_rconfc_gate_ops.go_check,
		.go_skip  = m0_rconfc_gate_ops.go_skip,
		.go_drain = NULL,
	};
	rconfc->rc_expired_cb = expired_cb;
	rconfc->rc_ready_cb   = ready_cb;
	rconfc->rc_rlock_ctx  = rlock_ctx;

	rcnf_herd_tlist_init(&rconfc->rc_herd);
	rcnf_active_tlist_init(&rconfc->rc_active);
	m0_clink_init(&rconfc->rc_unpinned_cl, rconfc_unpinned_cb);
	m0_clink_init(&rconfc->rc_herd_cl, rconfc_herd_fini_cb);
	m0_mutex_init(&rconfc->rc_herd_lock);
	m0_chan_init(&rconfc->rc_herd_chan, &rconfc->rc_herd_lock);
	m0_sm_init(&rconfc->rc_sm, &rconfc_sm_conf, M0_RCS_INIT, sm_group);

	/* Subscribe on ha entrypoint callbacks */
	ha = m0_get()->i_ha;
	m0_clink_init(&rconfc->rc_ha_entrypoint_cl, ha_clink_cb);
	m0_clink_add_lock(m0_ha_entrypoint_client_chan(&ha->h_entrypoint_client),
			  &rconfc->rc_ha_entrypoint_cl);
	rconfc->rc_ha_entrypoint_rep.hae_control = M0_HA_ENTRYPOINT_QUERY;

	m0_clink_init(&rconfc->rc_ha_update_cl, rconfc_ha_update_cb);

	return M0_RC(0);
confc_err:
	rlock_ctx_destroy(rlock_ctx);
rlock_err:
	m0_free(va);
	return M0_ERR(rc);
}

M0_INTERNAL int m0_rconfc_start(struct m0_rconfc *rconfc)
{
	M0_ENTRY("rconfc = %p, profile = "FID_F, rconfc,
		 FID_P(&rconfc->rc_profile));
	M0_PRE(rconfc->rc_fatal_cb == NULL);
	if (rconfc->rc_local_conf != NULL)
		return M0_RC(rconfc_local_load(rconfc));
	rconfc_ast_post(rconfc, rconfc_start_ast_cb);
	return M0_RC(0);
}

M0_INTERNAL int m0_rconfc_start_wait(struct m0_rconfc *rconfc,
				     uint64_t          timeout_ns)
{
	struct rlock_ctx *rlx = M0_MEMBER(rconfc, rc_rlock_ctx);
	int               rc;

	M0_ENTRY("rconfc = %p, profile = "FID_F, rconfc,
		 FID_P(&rconfc->rc_profile));
	if (timeout_ns != M0_TIME_NEVER)
		rlx->rlc_timeout = timeout_ns;
	rc = m0_rconfc_start(rconfc);
	if (rc != 0)
		return M0_ERR(rc);
	m0_rconfc_lock(rconfc);
	rc = rconfc->rc_sm.sm_rc;
	if (rc == 0 && !m0_rconfc_is_preloaded(rconfc)) {
		m0_time_t deadline = timeout_ns == M0_TIME_NEVER ?
			M0_TIME_NEVER : m0_time_from_now(0, timeout_ns);

		if (m0_sm_timedwait(&rconfc->rc_sm,
				    M0_BITS(M0_RCS_IDLE, M0_RCS_FAILURE),
				    deadline) == -ETIMEDOUT) {
			rconfc_fail(rconfc, M0_ERR(-ETIMEDOUT));
		}
		/*
		 * Wait mat result in some error (failed to take lock,
		 * etc). Let's have this under control.
		 */
		rc = rconfc->rc_sm.sm_rc;
	}
	m0_rconfc_unlock(rconfc);
	return M0_RC(rc);
}

M0_INTERNAL void m0_rconfc_stop(struct m0_rconfc *rconfc)
{
	M0_ENTRY("rconfc %p", rconfc);
	m0_rconfc_lock(rconfc);
	m0_sm_timedwait(&rconfc->rc_sm, M0_BITS(M0_RCS_INIT, M0_RCS_IDLE,
						M0_RCS_FAILURE), M0_TIME_NEVER);
	m0_rconfc_unlock(rconfc);
	/*
	 * Can't use rconfc_ast_post() here, because this AST can be already
	 * posted (after observing read lock conflict, for example).
	 */
	rconfc->rc_stop_ast.sa_cb    = rconfc_stop_ast_cb;
	rconfc->rc_stop_ast.sa_datum = rconfc;
	m0_sm_ast_post(rconfc->rc_sm.sm_grp, &rconfc->rc_stop_ast);
	M0_LEAVE();
}

M0_INTERNAL void m0_rconfc_stop_sync(struct m0_rconfc *rconfc)
{
	M0_ENTRY("rconfc = %p", rconfc);
	if (rconfc_state(rconfc) == M0_RCS_INIT &&
	    !m0_confc_is_inited(&rconfc->rc_confc))
		goto leave;
	m0_rconfc_stop(rconfc);
	m0_rconfc_lock(rconfc);
	m0_sm_timedwait(&rconfc->rc_sm, M0_BITS(M0_RCS_FINAL), M0_TIME_NEVER);
	m0_rconfc_unlock(rconfc);
leave:
	M0_LEAVE();
}

M0_INTERNAL void m0_rconfc_fini(struct m0_rconfc *rconfc)
{
	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc != NULL);
	M0_PRE(rconfc->rc_rlock_ctx != NULL);
	M0_PRE(m0_confc_is_inited(&rconfc->rc_phony));

	m0_clink_del_lock(&rconfc->rc_ha_entrypoint_cl);
	m0_clink_fini(&rconfc->rc_ha_entrypoint_cl);

	m0_rconfc_lock(rconfc);
	if (rconfc_state(rconfc) == M0_RCS_INIT)
		/*
		 * looks like this rconfc instance never was started, so do
		 * internal cleanup prior to read lock context destruction and
		 * finalising state machine
		 */
		rconfc_stop_internal(rconfc);
	m0_rconfc_unlock(rconfc);
	m0_free(rconfc->rc_local_conf);
	m0_free(rconfc->rc_qctx);
	rlock_ctx_destroy(rconfc->rc_rlock_ctx);
	if (m0_confc_is_inited(&rconfc->rc_confc))
		m0_confc_fini(&rconfc->rc_confc);
	rconfc_active_all_unlink(rconfc);
	rcnf_active_tlist_fini(&rconfc->rc_active);
	rconfc_herd_prune(rconfc);
	rcnf_herd_tlist_fini(&rconfc->rc_herd);
	m0_mutex_lock(&rconfc->rc_herd_lock);
	m0_chan_fini(&rconfc->rc_herd_chan);
	m0_mutex_unlock(&rconfc->rc_herd_lock);
	m0_mutex_fini(&rconfc->rc_herd_lock);
	m0_clink_fini(&rconfc->rc_herd_cl);
	_confc_phony_fini(&rconfc->rc_phony);
	m0_rconfc_lock(rconfc);
	m0_sm_fini(&rconfc->rc_sm);
	m0_rconfc_unlock(rconfc);

	M0_LEAVE();
}

M0_INTERNAL uint64_t m0_rconfc_ver_max_read(struct m0_rconfc *rconfc)
{
	uint64_t ver_max;

	M0_ENTRY("rconfc = %p", rconfc);
	M0_PRE(rconfc_state(rconfc) != M0_RCS_INIT);
	m0_rconfc_lock(rconfc);
	ver_max = m0_tl_fold(rcnf_herd, lnk, acc, &rconfc->rc_herd,
			     M0_CONF_VER_UNKNOWN,
			     max64(acc, _confc_ver_read(&lnk->rl_confc)));
	m0_rconfc_unlock(rconfc);
	M0_LEAVE("ver_max = %"PRIu64, ver_max);
	return ver_max;
}

M0_INTERNAL void m0_rconfc_fatal_cb_set(struct m0_rconfc *rconfc,
					m0_rconfc_cb_t    cb)
{
	M0_PRE(rconfc_is_locked(rconfc));
	M0_PRE(rconfc->rc_fatal_cb == NULL);
	rconfc->rc_fatal_cb = cb;
	if (rconfc_state(rconfc) == M0_RCS_FAILURE)
		/*
		 * Failure might occur between successful rconfc start and this
		 * callback setup. If occurred, callback is fired right away.
		 */
		cb(rconfc);
}

M0_INTERNAL int m0_rconfc_confd_endpoints(struct m0_rconfc   *rconfc,
					  const char       ***eps)
{

	struct rconfc_link  *lnk;
	size_t               confd_eps_length;
	int                  i = 0;

	M0_PRE(rconfc_state(rconfc) != M0_RCS_INIT);
	M0_PRE(*eps == NULL);
	confd_eps_length = m0_tlist_length(&rcnf_herd_tl, &rconfc->rc_herd);
	M0_ALLOC_ARR(*eps, confd_eps_length + 1);
	m0_tl_for(rcnf_herd, &rconfc->rc_herd, lnk) {
		(*eps)[i] = m0_strdup(lnk->rl_confd_addr);
		if ((*eps)[i] == NULL)
			goto fail;
		M0_CNT_INC(i);
	} m0_tl_endfor;
	(*eps)[i] = NULL;
	return i;
fail:
	m0_strings_free(*eps);
	return M0_ERR(-ENOMEM);
}

M0_INTERNAL int m0_rconfc_rm_endpoint(struct m0_rconfc *rconfc, char **ep)
{
	struct rlock_ctx *rlx;

	M0_PRE(rconfc_state(rconfc) != M0_RCS_INIT);
	M0_PRE(_0C(rconfc != NULL) && _0C(rconfc->rc_rlock_ctx != NULL) &&
	       rlock_ctx_is_online(rconfc->rc_rlock_ctx));
	M0_PRE(*ep == NULL);

	rlx = rconfc->rc_rlock_ctx;
	*ep = m0_strdup(rlx->rlc_rm_addr);
	if (*ep == NULL)
		return M0_ERR(-ENOMEM);
	return M0_RC(0);
}

M0_INTERNAL void m0_rconfc_rm_fid(struct m0_rconfc *rconfc, struct m0_fid *out)
{
	struct rlock_ctx *rlx;

	M0_PRE(rconfc_state(rconfc) != M0_RCS_INIT);
	M0_PRE(_0C(rconfc != NULL) && _0C(rconfc->rc_rlock_ctx != NULL) &&
	       rlock_ctx_is_online(rconfc->rc_rlock_ctx));
	M0_PRE(m0_fid_eq(out, &M0_FID0));

	rlx = rconfc->rc_rlock_ctx;
	*out = rlx->rlc_rm_fid;
}

M0_INTERNAL bool m0_rconfc_is_preloaded(struct m0_rconfc *rconfc)
{
	return rconfc_state(rconfc) == M0_RCS_INIT &&
		m0_confc_is_inited(&rconfc->rc_confc) &&
		rconfc->rc_local_conf != NULL;
}

static bool ha_clink_cb(struct m0_clink *clink)
{
        struct m0_rconfc                   *rconfc = container_of(clink,
                                                          struct m0_rconfc,
                                                          rc_ha_entrypoint_cl);
        struct m0_ha                       *ha  = m0_get()->i_ha;
        struct m0_ha_entrypoint_client     *ecl = &ha->h_entrypoint_client;
        enum m0_ha_entrypoint_client_state  state;
	int rc;

        state = m0_ha_entrypoint_client_state_get(ecl);
        M0_ENTRY("ha_entrypoint_client_state=%s",
		 m0_sm_state_name(&ecl->ecl_sm, state));

	if (rconfc->rc_local_conf != NULL)
		return true;

        if (state == M0_HEC_AVAILABLE &&
	    ecl->ecl_rep.hae_control != M0_HA_ENTRYPOINT_QUERY) {
                m0_rconfc_lock(rconfc);
		rc = m0_ha_entrypoint_rep_copy(&rconfc->rc_ha_entrypoint_rep,
					       &ecl->ecl_rep);
		M0_ASSERT(rc == 0 || rc == -EINVAL);

		if (rc == -EINVAL)
			rconfc_fail(rconfc, rc);
		else if (rconfc_state(rconfc) == M0_RCS_ENTRYPOINT_WAIT)
			rconfc_ast_post(rconfc, rconfc_start_ast_cb);

                m0_rconfc_unlock(rconfc);
        }

        M0_LEAVE();
        return true;
}

/** @} rconfc_dlspec */
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
