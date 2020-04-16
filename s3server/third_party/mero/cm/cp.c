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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 *                  Anup Barve <anup_barve@xyratex.com>
 *                  Mandar Sawant <mandar_sawant@xyratex.com>
 * Original creation date: 02/22/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CM
#include "lib/trace.h"

#include "lib/misc.h"   /* m0_forall */
#include "lib/memory.h"
#include "lib/errno.h"

#include "mero/magic.h"
#include "reqh/reqh.h"
#include "net/buffer_pool.h"
#include "rpc/rpc_opcodes.h" /* M0_CM_CP_OPCODE */
#include "rpc/bulk.h"

#include "cm/cp.h"
#include "cm/ag.h"
#include "cm/proxy.h"
#include "cm/cm.h"

#include "fop/fop.h"
#include "fop/fom.h"

/**
 * @page CPDLD Copy Packet DLD
 *
 *   - @ref CPDLD-ovw
 *   - @ref CPDLD-def
 *   - @ref CPDLD-req
 *   - @ref CPDLD-depends
 *   - @ref CPDLD-highlights
 *   - @subpage CPDLD-fspec "Functional Specification" <!-- Note @subpage -->
 *   - @ref CPDLD-lspec
 *      - @ref CPDLD-lspec-comps
 *      - @ref CPDLD-lspec-state
 *      - @ref CPDLD-lspec-mn-xform
 *         - @ref CPDLD-lspec-mn-xform-out
 *         - @ref CPDLD-lspec-mn-xform-in
 *      - @ref CPDLD-lspec-thread
 *   - @ref CPDLD-conformance
 *   - @ref CPDLD-ut
 *   - @ref CPDLD-st
 *   - @ref CPDLD-ref
 *
 *   <hr>
 *   @section CPDLD-ovw Overview
 *
 *   Copy packet is the data structure used to describe the movement of a piece
 *   of re-structured data between various copy machine replica nodes and within
 *   the same replica. It is an entity which has data as well as operations.
 *   Copy packets are FOMs of special type, created when a data re-structuring
 *   request is posted to replica.
 *
 *   Copy packet processing logic is implemented in a non-blocking way.
 *   Packet has buffers to carry data and FOM for execution in context of
 *   request handler. It can perform different work which depends on its phase
 *   (i.e. FOM phase) in execution.
 *
 *   <hr>
 *   @section CPDLD-def Definitions
 *   - <b>Copy Packet:</b> A chunk of data traversing through the copy
 *   machine.
 *
 *   - <b>Copy packet acknowledgement:</b> Reply received, representing
 *   successful processing of the copy packet. With this acknowledgement, copy
 *   packet releases various resources and updates its internal state.
 *
 *   - <b>Next phase function:</b> Given a copy packet, this identifies the
 *   phase that has to be assigned to this copy packet. The next phase function
 *   (m0_cm_cp_ops::co_phase_next()) determines the routing and execution of
 *   copy packets through the copy machine.
 *
 *   <hr>
 *   @section CPDLD-req Requirements
 *
 *   - @b r.cm.cp Copy packet abstraction implemented such that it represents
 *	  the data to be transferred within replica.
 *
 *   - @b r.cm.cp.async Every read-write (receive-send) by replica should follow
 *	  the non-blocking processing model of Mero design.
 *
 *   - @b r.cm.buffer_pool Copy machine should provide a buffer pool, which is
 *	  efficiently used for copy packet data.
 *
 *   - @b r.cm.cp.bulk_transfer All data packets (except control packets) that
 *	  are sent over RPC should use bulk-interface for communication.
 *
 *   - @b r.cm.cp.fom.locality Copy packet FOMs should be efficiently assigned
 *        request handler locality without causing any deadlock or data
 *        corruption.
 *
 *   - @b r.cm.addb Copy packet should have its own addb context, (similar to
 *	  fom), although it uses different addb locations, this will trace the
 *	  entire path of the copy packet.
 *
 *   <hr>
 *   @section CPDLD-depends Dependencies
 *
 *   - @b r.cm.service Copy packet FOMs are executed in context of copy machine
 *	  replica.
 *
 *   - @b r.cm.ops Replica provides operations to create, configure and execute
 *     copy packet FOMs.
 *
 *   - @b r.layout Data restructuring needs layout info.
 *
 *   - @b r.layout.input-iterator Iterate over layout info to create packets
 *	  and forward it in replica.
 *
 *   - @b r.resource Resources like buffers, CPU cycles, network bandwidth,
 *	  storage bandwidth are needed by copy packet FOM during execution.
 *
 *   - @b r.confc Data from configuration will be used to initialise copy
 *	  packets.
 *
 *   <hr>
 *   @section CPDLD-highlights Design Highlights
 *
 *   - Copy packet is implemented as FOM, which inherently has non-blocking
 *     model of mero.
 *
 *   - Distributed sliding window algorithm is used to process copy packets
 *     within copy machine replica.
 *
 *   - Layout is updated periodically as the restructuring progresses.
 *
 *   <hr>
 *   @section CPDLD-lspec Logical Specification
 *
 *   - @ref CPDLD-lspec-comps
 *      - @ref DLDCPInternal  <!-- Note link -->
 *   - @ref CPDLD-lspec-state
 *   - @ref CPDLD-lspec-thread
 *
 *   @subsection CPDLD-lspec-comps Component Overview
 *
 *   <b>Copy packet functionality is split into two parts:</b>
 *
 *	- generic functionality, implemented by cm/cp.[hc] and
 *
 *      - copy packet type functionality which is based on copy machine type.
 *        (e.g. SNS, Replication, &c).
 *
 *   <b>Copy packet creation:</b>
 *   Given the size of the buffer pool, the replica calculates its initial
 *   sliding window (@see m0_cm_sw). Once the replica learns windows of every
 *   other replica, it can produce copy packets that replicas (including this
 *   one) are ready to process.
 *
 *   Copy packet is created when,
 *      - replica starts. It should be made sure that sliding window has enough
 *        packets for processing by creating them at start.
 *
 *      - has space. After completion of each copy packet, space in sliding
 *        window is checked. If space exists, then copy packets will be created.
 *
 *   <b>Copy Packet destruction:</b>
 *   Copy packet is destroyed by setting its phase to M0_CCP_FINI.
 *   Following are some cases where copy packet is finalised.
 *
 *	- On notification of copy packet data written to device/container.
 *
 *	- During transformation, packets that are no longer needed, are
 *	  finalised.
 *
 *	- On completion of copy packet transfer over the network.
 *
 *   <b>Copy packet cooperation within replica:</b>
 *   Copy packet needs resources (memory, processor, &c.) to do processing:
 *
 *	- Needs buffers to keep data during IO.
 *
 *	- Needs buffers to keep data until the transfer is finished.
 *
 *	- Needs buffers to keep intermediate checksum until all units of an
 *	  aggregation group have been received.
 *
 *   The copy packet (and its associated buffers) will go through various
 *   phases. In a particular scenario where data read from device creates a
 *   copy packet, then copy packet transitions to data transformation phase,
 *   which, after reconstructing the data, transitions to data write or send,
 *   which submits IO. On IO completion, the copy packet is destroyed.
 *
 *   Copy machine provides and manages resources required by the copy packet.
 *   e.g. In case of SNS Repair, copy machine creates 2 buffer pools, for
 *   incoming and outgoing copy packets. Based on the availability of buffers
 *   in these buffer pools, new copy packets are created. On finalisation of
 *   a copy packet, the corresponding buffers are released back to the respective
 *   buffer pool.
 *
 *   @subsection CPDLD-lspec-state State Specification
 *
 *   <b>Copy packet is a state machine that goes through following phases:</b>
 *
 *   - @b INIT        Copy packet gets initialised with input data. e.g In SNS,
 *		      extent, COB, &c gets initialised. Usually this is done with some
 *		      iterator over layout info.
 *		      (m0_cm_cp_phase::M0_CCP_INIT)
 *
 *   - @b READ        Reads data from its associated container or device according
 *		      to the input information, and places the data in a copy
 *		      packet data buffer. Before doing this, it needs to grab
 *		      necessary resources: memory, locks, permissions, CPU/disk
 *		      bandwidth, etc. Data/parity is encapsulated in copy packet,
 *		      and the copy packets are transfered to next phase.
 *		      (m0_cm_cp_phase::M0_CCP_READ)
 *
 *   - @b WRITE       Writes data from copy packet data buffer to the container
 *                    or device. Spare container and offset to write is
 *                    identified from layout information.
 *		      (m0_cm_cp_phase::M0_CCP_WRITE)
 *
 *   - @b XFORM       Data restructuring is done in this phase. This phase would
 *		      typically process a lot of local copy packets. E.g., for
 *		      SNS repair machine, a file typically has a component object
 *		      (cob) on each device in the pool, which means that a node
 *		      could (and should) calculate "partial parity" of all local
 *		      units, instead of sending each of them separately across
 *		      the network to a remote copy machine replica.
 *		      (m0_cm_cp_phase::M0_CCP_XFORM)
 *
 *   - @b IOWAIT      Waits for IO to complete. (m0_cm_cp_phase::M0_CCP_IO_WAIT)
 *
 *   - @b SW_CHECK    Checks if the copy packet is in sliding window.
 *                    If it is not, then waits in this phase till it fits in
 *                    the sliding window.
 *
 *   - @b SEND        Send copy packet over network. Control FOP and bulk
 *                    transfer are used for sending copy packet.
 *		      (m0_cm_cp_phase::M0_CCP_SEND)
 *
 *   - @b SEND_WAIT   Waits till the acknowledgement is received that copy
 *                    packet has been reached to the destination.
 *
 *   - @b BUF_ACQ     Acquire the buffers based on the control fop information.
 *
 *   - @b RECV_INIT   After acquiring required number of buffers, copy packet
 *                    FOM transitions to m0_cm_cp_phase::M0_CCP_RECV_INIT phase
 *                    and initiates zero copy using rpc_bulk.
 *
 *   - @b RECV_WAIT   Zero copy is completed. Any cleanup, if is done in this
 *                    phase.
 *
 *   - @b FINI        Finalises copy packet.
 *
 *   Specific copy packet can have phases in addition to these phases.
 *   Additional phases may be used to do processing for copy packet specific
 *   functionality. Handling of additional phases also can be done using next
 *   phase function, as implementation of next phase function is also specific
 *   to copy packet type.
 *
 *   Transition between standard phases is done by next phase function. It will
 *   produce the next phase according to the configuration of the copy machine
 *   and the copy packet itself.
 *
 *   <b>State diagram for copy packet:</b>
 *   @dot
 *   digraph {
 *	subgraph A1 {
 *	   start [shape=Mdiamond];
 *	   end [shape=doublecircle];
 *	}
 *	subgraph A2 {
 *	   size = "4,4"
 *	   node [shape=ellipse, fontsize=12]
 *	   start -> INIT
 *	   INIT  -> READ -> IOWAIT -> SW_CHECK -> SEND -> SEND_WAIT -> FINI
 *	   INIT  -> BUF_ACQ -> RECV_INIT -> RECV_WAIT -> XFORM
 *	   XFORM -> WRITE -> IOWAIT -> FINI
 *	   INIT  -> XFORM -> FINI
 *	   INIT  -> WRITE
 *	   INIT  -> SEND
 *	   IOWAIT -> XFORM ->SW_CHECK -> SEND -> SEND_WAIT -> FINI
 *	   FINI  -> end
 *	}
 *   }
 *   @enddot
 *
 *   @subsection CPDLD-lspec-mn-xform Transformation in multinode environment
 *
 *   When copy packet fom enters transformation phase, it calculates
 *   partial parity on that particular node. This calculation is based on the
 *   incoming copy packets for an aggregation group. In case of multinode data
 *   restructuring, transformation is executed locally i.e. along outgoing path
 *   as well as along the incoming path.
 *
 *   @subsubsection CPDLD-lspec-mn-xform-out Outgoing path
 *
 *   The transformed copy packet contains partial parity of the local copy
 *   packets belonging to a particular aggregation group. The transformed copy
 *   packet can either be written locally or can be sent to remote destination
 *   node.
 *
 *   @subsubsection CPDLD-lspec-mn-xform-in Incoming path
 *
 *   The transformed copy packet contains the partial parity of the copy packets
 *   which are received from other nodes as well as local copy packets. This
 *   is executed typically on the destination node (i.e. node on which spare
 *   units are allocated, in case of repair operation). Transformation phase
 *   function inherently waits for all the copy packets in an aggregation group
 *   to be transformed. For this to happen, transformation function has to do
 *   bookkeeping of following information:
 *   - number of copy packets that have been transformed for a particular
 *   aggregation group (m0_cm_aggr_group::cag_transformed_cp_nr).
 *   - indices of the copy packets in an aggregation group that have been
 *   transformed (this knowledge is required by parity recovery algorithm like
 *   Reed-Solomon) This is stored using a bitmap (m0_cm_cp::c_xform_cp_indices).
 *
 *   The index of the copy packet in an aggregation group is stored by the
 *   iterator in m0_cm_cp::c_ag_cp_idx. This index is used by the transformation
 *   function to populate the bitmap (m0_cm_cp::c_xform_cp_indices).
 *   Note: This index should be global index of a copy packet in an aggregation
 *   group.
 *
 *   For any aggregation group, transformation is marked as complete, iff all
 *   indices in the bitmap are set to true.
 *
 *   @subsection CPDLD-lspec-thread Threading and Concurrency Model
 *
 *   Copy packet is implemented as a FOM and thus do not have its own thread.
 *   It runs in the context of reqh threads. So FOM locality group lock
 *   (i.e m0_cm_cp:c_fom:fo_loc:fl_group:s_lock) is used to serialise access
 *   to m0_cm_cp and its operation.
 *
 *   <hr>
 *   @section CPDLD-conformance Conformance
 *
 *   - @b i.cm.cp Replicas communicate using copy packet structure.
 *
 *   - @b i.cm.cp.async Copy packet are implemented as FOM. FOM in request
 *	  handler infrastructure makes it non-blocking.
 *
 *   - @b i.cm.buffer_pool Buffer pools are managed by copy machine which
 *	  cater to the requirements of copy packet data.
 *
 *   - @b i.cm.cp.bulk_transfer All data packets (except control packets)
 *	  that are sent over RPC, use bulk-interface for communication.
 *
 *   - @b i.cm.cp.fom.locality Copy machine implements its type specific
 *        m0_cm_cp_ops::co_home_loc_helper().
 *
 *   - @b i.cm.cp.addb copy packet uses ADDB context of copy machine.
 *
 *   <hr>
 *   @section CPDLD-ut Unit Tests
 *   - Basic Test: Alloc, Init, fini and free.
 *   - Test storage phases (write, read and then verify).
 *   - Test transformation phase. Wait in the transformation phase till the
 *     bitmap in the transformed copy packet has all its bits set to true.
 *
 *   <hr>
 *   @section CPDLD-st System Tests
 *
 *   <hr>
 *   @section CPDLD-ref References
 *
 *   - <a href="https://docs.google.com/a/seagate.com/document/d/1Wvw8CTXOpH9ztF
 CDysXAXAgJ5lQoMcOkbBNBW9Nz9OM/edit#"> HLD of SNS Repair</a>
 *
 *   - <a href="https://docs.google.com/a/xyratex.com/document/d/1ZlkjayQoXVm-pr
 *   MxTkzxb1XncB6HU19I19kwrV-8eQc/edit#"> HLD of Copy machine and agents</a>
 *
 */

/**
 * @defgroup DLDCPInternal Copy packet internal
 * @ingroup CP
 *
 * @see @ref CPDLD and @ref CPDLD-lspec
 *
 * @{
 */

M0_TL_DESCR_DEFINE(cp_data_buf, "copy packet data buffers", M0_INTERNAL,
		   struct m0_net_buffer, nb_extern_linkage, nb_magic,
		   M0_NET_BUFFER_LINK_MAGIC, CM_CP_DATA_BUF_HEAD_MAGIX);
M0_TL_DEFINE(cp_data_buf, M0_INTERNAL, struct m0_net_buffer);

static const struct m0_bob_type cp_bob = {
	.bt_name = "copy packet",
	.bt_magix_offset = M0_MAGIX_OFFSET(struct m0_cm_cp, c_magix),
	.bt_magix = CM_CP_MAGIX,
	.bt_check = NULL
};

M0_TL_DESCR_DEFINE(proxy_cps, "copy packets in proxy", M0_INTERNAL,
		   struct m0_cm_cp, c_cm_proxy_linkage, c_magix,
		   CM_CP_MAGIX, CM_PROXY_CP_HEAD_MAGIX);
M0_TL_DEFINE(proxy_cps, M0_INTERNAL, struct m0_cm_cp);


M0_BOB_DEFINE(static, &cp_bob, m0_cm_cp);

M0_INTERNAL void m0_cm_cp_fom_fini(struct m0_fom *fom)
{
	struct m0_cm_cp *cp = bob_of(fom, struct m0_cm_cp, c_fom,
				     &cp_bob);
	M0_ENTRY();

	m0_cm_cp_fini(cp);
	cp->c_ops->co_free(cp);

	M0_LEAVE();
}

static uint64_t cp_fom_locality(const struct m0_fom *fom)
{
	struct m0_cm_cp *cp = bob_of(fom, struct m0_cm_cp, c_fom, &cp_bob);

	return cp->c_ops->co_home_loc_helper(cp);
}

static int cp_fom_tick(struct m0_fom *fom)
{
	struct m0_cm_cp *cp = bob_of(fom, struct m0_cm_cp, c_fom, &cp_bob);
	int		 phase = m0_fom_phase(fom);
	int              rc;

	M0_PRE(phase < cp->c_ops->co_action_nr);
	M0_LOG(M0_DEBUG, "fom phase = %d", phase);

	rc = cp->c_ops->co_action[phase](cp);
	/** @todo Revise it. */
	if (m0_fom_phase(fom) == M0_CCP_FINI)
		cp->c_ops->co_action[M0_CCP_FINI](cp);
	return M0_RC(rc);
}

/** Copy packet FOM operations */
static const struct m0_fom_ops cp_fom_ops = {
	.fo_fini          = m0_cm_cp_fom_fini,
	.fo_tick          = cp_fom_tick,
	.fo_home_locality = cp_fom_locality
};

M0_INTERNAL int m0_cm_cp_fom_create(struct m0_fop *fop, struct m0_fop *r_fop,
				    struct m0_fom **m, struct m0_reqh *reqh)
{
	struct m0_cm_cp        *cp;
	struct m0_cm           *cm;
	struct m0_reqh_service *service;

	M0_PRE(fop != NULL);
	M0_PRE(m != NULL);

	service = m0_reqh_service_find(fop->f_type->ft_fom_type.ft_rstype,
				       reqh);
	M0_PRE(service != NULL);
	cm = container_of(service, struct m0_cm, cm_service);
	M0_PRE(cm != NULL);
	cp = cm->cm_ops->cmo_cp_alloc(cm);
	if (cp == NULL)
		return M0_ERR(-ENOMEM);

	m0_cm_cp_fom_init(cm, cp, fop, r_fop);
	*m = &cp->c_fom;
	return 0;
}

/** @} end internal */

/**
   @addtogroup CP
   @{
 */

static struct m0_sm_state_descr m0_cm_cp_state_descr[] = {
	[M0_CCP_INIT] = {
		.sd_flags       = M0_SDF_INITIAL,
		.sd_name        = "Init",
		.sd_allowed     = M0_BITS(M0_CCP_READ, M0_CCP_WRITE,
					  M0_CCP_XFORM, M0_CCP_SEND,
					  M0_CCP_SW_CHECK, M0_CCP_FAIL,
					  M0_CCP_FINI)
	},
	[M0_CCP_READ] = {
		.sd_flags       = 0,
		.sd_name        = "Read",
		.sd_allowed     = M0_BITS(M0_CCP_IO_WAIT, M0_CCP_FAIL,
					  M0_CCP_FINI)
	},
	[M0_CCP_WRITE_PRE] = {
		.sd_flags       = 0,
		.sd_name        = "Write-pre",
		.sd_allowed     = M0_BITS(M0_CCP_TX_OPEN, M0_CCP_WRITE,
					  M0_CCP_FAIL)
	},
	[M0_CCP_TX_OPEN] = {
		.sd_flags       = 0,
		.sd_name        = "TX Open",
		.sd_allowed     = M0_BITS(M0_CCP_WRITE, M0_CCP_FAIL)
	},
	[M0_CCP_WRITE] = {
		.sd_flags       = 0,
		.sd_name        = "Write",
		.sd_allowed     = M0_BITS(M0_CCP_TX_DONE, M0_CCP_IO_WAIT,
					  M0_CCP_FAIL, M0_CCP_FINI)
	},
	[M0_CCP_TX_DONE] = {
		.sd_flags       = 0,
		.sd_name        = "TX Done",
		.sd_allowed     = M0_BITS(M0_CCP_IO_WAIT, M0_CCP_FAIL)
	},
	[M0_CCP_IO_WAIT] = {
		.sd_flags       = 0,
		.sd_name        = "IO Wait",
		.sd_allowed     = M0_BITS(M0_CCP_XFORM, M0_CCP_SEND,
					  M0_CCP_FAIL, M0_CCP_FINI)
	},
	[M0_CCP_XFORM] = {
		.sd_flags       = 0,
		.sd_name        = "Xform",
		.sd_allowed     = M0_BITS(M0_CCP_FAIL, M0_CCP_FINI,
					  M0_CCP_WRITE_PRE, M0_CCP_WRITE,
					  M0_CCP_SEND, M0_CCP_SW_CHECK)
	},
	[M0_CCP_SW_CHECK] = {
		.sd_flags       = 0,
		.sd_name        = "Sliding window check",
		.sd_allowed     = M0_BITS(M0_CCP_SEND, M0_CCP_FINI, M0_CCP_FAIL)
	},
	[M0_CCP_SEND] = {
		.sd_flags       = 0,
		.sd_name        = "Send",
		.sd_allowed     = M0_BITS(M0_CCP_FINI, M0_CCP_RECV_INIT,
					  M0_CCP_SEND_WAIT, M0_CCP_FAIL)
	},
	[M0_CCP_SEND_WAIT] = {
		.sd_flags       = 0,
		.sd_name        = "Send Wait",
		.sd_allowed     = M0_BITS(M0_CCP_FINI, M0_CCP_FAIL)
	},
	[M0_CCP_RECV_INIT] = {
		.sd_flags       = 0,
		.sd_name        = "Recv Init",
		.sd_allowed     = M0_BITS(M0_CCP_RECV_WAIT, M0_CCP_FINI)
	},
	[M0_CCP_RECV_WAIT] = {
		.sd_flags       = 0,
		.sd_name        = "Recv Wait",
		.sd_allowed     = M0_BITS(M0_CCP_XFORM, M0_CCP_RECV_INIT,
					  M0_CCP_FAIL)
	},
	[M0_CCP_FAIL] = {
		.sd_flags       = M0_SDF_FAILURE,
		.sd_name        = "Failure",
		.sd_allowed     = M0_BITS(M0_CCP_FINI)
	},
	[M0_CCP_FINI] = {
		.sd_flags       = M0_SDF_TERMINAL,
		.sd_name        = "Fini",
		.sd_allowed     = 0
	},
};

static const struct m0_sm_conf m0_cm_cp_sm_conf = {
	.scf_name = "sm:cp conf",
	.scf_nr_states = ARRAY_SIZE(m0_cm_cp_state_descr),
	.scf_state = m0_cm_cp_state_descr
};

M0_INTERNAL void m0_cm_cp_init(struct m0_cm_type *cmtype,
			       const struct m0_fom_type_ops *ft_ops)
{
	m0_fom_type_init(&cmtype->ct_fomt, cmtype->ct_fom_id, ft_ops,
			 &cmtype->ct_stype, &m0_cm_cp_sm_conf);
}

M0_INTERNAL bool m0_cm_cp_invariant(const struct m0_cm_cp *cp)
{
	const struct m0_cm_cp_ops *ops = cp->c_ops;

	return m0_cm_cp_bob_check(cp) && ops != NULL &&
	       cp->c_ag != NULL &&
	       m0_fom_phase(&cp->c_fom) < ops->co_action_nr &&
	       cp->c_ops->co_invariant(cp);
}

M0_INTERNAL void m0_cm_cp_only_init(struct m0_cm *cm, struct m0_cm_cp *cp)
{
	m0_cm_cp_bob_init(cp);
	cp_data_buf_tlist_init(&cp->c_buffers);
	m0_rpc_bulk_init(&cp->c_bulk);
	m0_mutex_init(&cp->c_reply_wait_mutex);
	m0_chan_init(&cp->c_reply_wait, &cp->c_reply_wait_mutex);
	proxy_cp_tlink_init(cp);

	/* copy packet epoch is derived from its cm */
	cp->c_epoch = cm->cm_epoch;
}

M0_INTERNAL void m0_cm_cp_fom_init(struct m0_cm *cm, struct m0_cm_cp *cp,
				   struct m0_fop *fop, struct m0_fop *r_fop)
{
	struct m0_reqh_service *service;

	M0_PRE(cm != NULL && cp != NULL && cp->c_ops != NULL);

	m0_cm_cp_only_init(cm, cp);
	service = &cm->cm_service;
	m0_fom_init(&cp->c_fom, &cm->cm_type->ct_fomt, &cp_fom_ops, fop, r_fop,
		    service->rs_reqh);
}

M0_TL_DECLARE(rpcbulk, M0_INTERNAL, struct m0_rpc_bulk_buf);
M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);

M0_INTERNAL void m0_cm_cp_only_fini(struct m0_cm_cp *cp)
{
	struct m0_rpc_bulk *rbulk;
	/*
	 * If the copy packet is not an accumulator copy packet, it will get
	 * finalised after transformation. For such copy packets, finalise the
	 * bitmap.
	 */
	if (cp->c_xform_cp_indices.b_nr > 0)
		m0_bitmap_fini(&cp->c_xform_cp_indices);
	m0_chan_fini_lock(&cp->c_reply_wait);
	m0_mutex_fini(&cp->c_reply_wait_mutex);

	/*
	 * Release the net buffers if rpc bulk object is still dirty.
	 * And wait on channel till all net buffers are deleted from
	 * transfer machine.
	 */
	rbulk = &cp->c_bulk;
	m0_mutex_lock(&rbulk->rb_mutex);
	if (!rpcbulk_tlist_is_empty(&rbulk->rb_buflist)) {
		struct m0_clink clink;
		size_t          buf_nr;
		size_t          non_queued_buf_nr;

		m0_clink_init(&clink, NULL);
		m0_clink_add(&rbulk->rb_chan, &clink);
		buf_nr = rpcbulk_tlist_length(&rbulk->rb_buflist);
		non_queued_buf_nr = m0_rpc_bulk_store_del_unqueued(rbulk);
		m0_mutex_unlock(&rbulk->rb_mutex);

		m0_rpc_bulk_store_del(rbulk);
		M0_LOG(M0_DEBUG, "bulk %p, buf_nr %llu, non_queued_buf_nr %llu",
					   rbulk,
		       (unsigned long long)buf_nr,
		       (unsigned long long)non_queued_buf_nr);
		/*
		 * If there were some queued net bufs which had to be deleted,
		 * then it is required to wait for their callbacks.
		 */
		if (buf_nr > non_queued_buf_nr) {
			m0_chan_wait(&clink);
		}
		m0_clink_del_lock(&clink);
		m0_clink_fini(&clink);
	} else {
		m0_mutex_unlock(&rbulk->rb_mutex);
	}
	M0_ASSERT(m0_rpc_bulk_is_empty(rbulk));

	m0_rpc_bulk_buflist_empty(&cp->c_bulk);
	m0_rpc_bulk_fini(&cp->c_bulk);
	proxy_cp_tlink_fini(cp);
	m0_cm_cp_bob_fini(cp);
}

M0_INTERNAL void m0_cm_cp_fini(struct m0_cm_cp *cp)
{
	m0_cm_cp_only_fini(cp);
	m0_fom_fini(&cp->c_fom);
}

M0_INTERNAL int m0_cm_cp_enqueue(struct m0_cm *cm, struct m0_cm_cp *cp)
{
	struct m0_fom  *fom = &cp->c_fom;
	struct m0_reqh *reqh = cm->cm_service.rs_reqh;

	M0_PRE(reqh != NULL);
	M0_PRE(m0_cm_cp_invariant(cp));

	if (m0_reqh_state_get(reqh) >= M0_REQH_ST_DRAIN)
		return M0_ERR(-ESHUTDOWN);

	m0_fom_queue(fom);
	return M0_RC(0);
}

M0_INTERNAL void m0_cm_cp_buf_add(struct m0_cm_cp *cp, struct m0_net_buffer *nb)
{
	M0_PRE(cp != NULL);
	M0_PRE(nb != NULL);

	cp_data_buf_tlink_init(nb);
	cp_data_buf_tlist_add_tail(&cp->c_buffers, nb);
	M0_CNT_INC(cp->c_buf_nr);
}

M0_INTERNAL void m0_cm_cp_buf_release(struct m0_cm_cp *cp)
{
	struct m0_net_buffer_pool *nbp;
	struct m0_net_buffer      *nbuf;
	struct m0_net_buffer      *nbuf_head;
	uint64_t                   colour;

	nbuf_head = cp_data_buf_tlist_head(&cp->c_buffers);
	if (nbuf_head != NULL) {
		nbp = nbuf_head->nb_pool;
		M0_ASSERT(nbp != NULL);
		m0_net_buffer_pool_lock(nbp);
		m0_tl_for(cp_data_buf, &cp->c_buffers, nbuf) {
			colour = cp->c_ops->co_home_loc_helper(cp) %
						nbp->nbp_colours_nr;
			cp_data_buf_tlink_del_fini(nbuf);
			m0_cm_buffer_put(nbp, nbuf, colour);
			M0_CNT_DEC(cp->c_buf_nr);
		} m0_tl_endfor;
		nbp->nbp_ops->nbpo_not_empty(nbp);
		m0_net_buffer_pool_unlock(nbp);
	}
	cp_data_buf_tlist_fini(&cp->c_buffers);
}

M0_INTERNAL uint64_t m0_cm_cp_nr(struct m0_cm_cp *cp)
{
	struct m0_bitmap *bm = &cp->c_xform_cp_indices;
	int               i;
	uint64_t          cnt = 0;

	for (i = 0; i < bm->b_nr; ++i) {
		if (m0_bitmap_get(bm, i))
			M0_CNT_INC(cnt);
	}

	return cnt;
}

M0_INTERNAL int m0_cm_cp_bufvec_merge(struct m0_cm_cp *cp)
{
	struct m0_net_buffer    *nbuf;
	struct m0_net_buffer    *nbuf_head;
	int                      rc;

	nbuf_head = cp_data_buf_tlist_head(&cp->c_buffers);
	nbuf = nbuf_head;
	while ((nbuf = cp_data_buf_tlist_next(&cp->c_buffers, nbuf)) != NULL) {
		rc = m0_bufvec_merge(&nbuf_head->nb_buffer, &nbuf->nb_buffer);
		if (rc != 0)
			return M0_RC(rc);
	}
	return 0;
}

M0_INTERNAL int m0_cm_cp_bufvec_split(struct m0_cm_cp *cp)
{
	struct m0_net_buffer       *nbuf_head;
	struct m0_bufvec           *bufvec;
	uint32_t                    new_v_nr;
	m0_bcount_t                *new_v_count;
	uint32_t                    i;

	nbuf_head = cp_data_buf_tlist_head(&cp->c_buffers);
	new_v_nr = nbuf_head->nb_pool->nbp_seg_nr;
	M0_ALLOC_ARR(new_v_count, new_v_nr);
	if (new_v_count == NULL)
		return M0_ERR(-ENOMEM);

	bufvec = &nbuf_head->nb_buffer;
	for (i = 0; i < new_v_nr; ++i)
		new_v_count[i] = bufvec->ov_vec.v_count[i];

	m0_free(bufvec->ov_vec.v_count);
	bufvec->ov_vec.v_nr = new_v_nr;
	bufvec->ov_vec.v_count = new_v_count;

	return 0;
}

M0_INTERNAL void m0_cm_cp_buf_move(struct m0_cm_cp *src, struct m0_cm_cp *dest)
{
	struct m0_net_buffer *nbuf;
	M0_PRE(src->c_data_seg_nr == dest->c_data_seg_nr);

	m0_tl_for(cp_data_buf, &src->c_buffers, nbuf) {
		cp_data_buf_tlink_del_fini(nbuf);
		M0_CNT_DEC(src->c_buf_nr);
		m0_cm_cp_buf_add(dest, nbuf);
	} m0_tl_endfor;
}

M0_INTERNAL int m0_cm_cp_dup(struct m0_cm_cp *src, struct m0_cm_cp **dest)
{
	struct m0_cm         *cm;
	struct m0_cm_cp      *cp;

	cm = src->c_ag->cag_cm;
	cp = cm->cm_ops->cmo_cp_alloc(cm);
	if (cp == NULL)
		return -ENOMEM;
	m0_cm_ag_cp_add_locked(src->c_ag, cp);
	cp->c_ag_cp_idx = src->c_ag_cp_idx;
	cp->c_data_seg_nr = src->c_data_seg_nr;
	m0_cm_cp_fom_init(cm, cp, NULL, NULL);
	m0_bitmap_init(&cp->c_xform_cp_indices,
		       src->c_xform_cp_indices.b_nr);
	m0_cm_cp_buf_move(src, cp);
	if (src->c_xform_cp_indices.b_nr > 0)
		m0_bitmap_copy(&cp->c_xform_cp_indices, &src->c_xform_cp_indices);
	*dest = cp;

	return M0_RC(0);
}

M0_INTERNAL void m0_cm_cp_data_copy(struct m0_cm_cp *src, struct m0_cm_cp *dst)
{
	struct m0_net_buffer      *src_nbuf;
	struct m0_net_buffer      *dst_nbuf;
	struct m0_net_buffer_pool *nbp;
	m0_bcount_t                bytes_copied = 0;
	uint64_t                   buf_size = 0;
	uint64_t                   total_data_seg_nr;

	M0_PRE(!cp_data_buf_tlist_is_empty(&src->c_buffers));
	M0_PRE(!cp_data_buf_tlist_is_empty(&dst->c_buffers));
	M0_PRE(src->c_buf_nr == dst->c_buf_nr);

	total_data_seg_nr = src->c_data_seg_nr;
	for (src_nbuf = cp_data_buf_tlist_head(&src->c_buffers),
	     dst_nbuf = cp_data_buf_tlist_head(&dst->c_buffers);
	     src_nbuf != NULL && dst_nbuf != NULL;
	     src_nbuf = cp_data_buf_tlist_next(&src->c_buffers, src_nbuf),
	     dst_nbuf = cp_data_buf_tlist_next(&dst->c_buffers, dst_nbuf)) {
		nbp = src_nbuf->nb_pool;
		if (total_data_seg_nr < nbp->nbp_seg_nr)
			buf_size = total_data_seg_nr * nbp->nbp_seg_size;
		else {
			total_data_seg_nr -= nbp->nbp_seg_nr;
			buf_size = nbp->nbp_seg_nr * nbp->nbp_seg_size;
		}
		bytes_copied = m0_bufvec_copy(&dst_nbuf->nb_buffer,
					      &src_nbuf->nb_buffer, buf_size);
		M0_ASSERT(bytes_copied == buf_size);
	}
}

/** @} end-of-CPDLD */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
