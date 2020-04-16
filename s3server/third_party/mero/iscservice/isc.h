/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * https://www.seagate.com/contacts
 *
 * Original author: Jean-Philippe Bernardy <jean-philippe.bernardy@tweag.io>
 * Original creation date: 15 Feb 2016
 * Modifications: Nachiket Sahasrabudhe <nachiket.sahasrabuddhe@seagate.com>
 * Date of modification: 27 Nov 2017
 */

#pragma once

#ifndef __MERO_ISC_H__
#define __MERO_ISC_H__

#include "iscservice/isc_fops.h"
#include "fop/fom_generic.h"

/**
   @defgroup iscservice ISC Service Operations
   @see @ref reqh

   @section iscservice-highlights Design Highlights

   The in-storage-compute service (iscs) provides a platform to perform
   computations on data stored with Mero. This enables Mero users to perform
   concurrent computation on stored data along with minimal network
   communication. The computations on data can range from linear algebraic
   operations or Fast Fourier Transform, to searching of a string (distributed
   grep) across Mero nodes. ISC service facilitates these computations
   by providing two types of interfaces:
	- Low level interface
	- Sandboxing

   Low-level interface is designed for computations "trusted" by Mero. It can be
   thought of as a system call which after being invoked triggers a
   preregistered computation on Mero side. The Mero trusted applications can
   use the internal Mero infrastructure (fom scheduler/stob I/O etc.) and hence
   the ISC service does not perform any task on behalf of the computation.

   Sandboxing is for applications not having a direct access to Mero
   internals. They can request data from Mero and can then apply the
   appropriate computation.

   @section iscservice-low-level Low Level Interface

   Low level interface can be used in two ways:
	- m0_isc_comp_req_exec: this API supports a local invocation of a
	  registered function. It's an asynchronous call which executes
	  a registered callback on completion.
	- Using fop/fom mechanism: supports remote invocation. Arguments and
	  results to be returned are communicated over a network.

  Computations registered with the service are stored in an in-memory hash
  table. It's worth noting that the service remains oblivious to the details of
  the function, including IO or network access. Since Mero uses a non-preemptive
  request handler, a service fom is expected to yield the cpu core whenever it's
  waiting on an external event. In order to be aligned with this semantics, a
  computation is expected to convey its state (see m0_fom_state) to its caller
  fom.

  A typical use-case of isc service is described below:
  -All computations registered with ISC service low-level framework shall
   confine to the following signature:
  @verbatim
	int comp(struct m0_buf *args, struct m0_buf *out,
		 struct m0_isc_comp_private *comp_data, int *rc)
  @endverbatim
  - "out" holds the result of a computation while "rc" captures an error code
    if any. The returned value of computation shall be either of M0_FSO_AGAIN
    and M0_FSO_WAIT. In case the computation needs to be re-invoked, "rc" shall
    hold "-EAGAIN". A detailed semantics of return value and error code is
    available in "ISC Service user guide" (please see below for the link).
  - While using external libraries it's necessary to prepare a wrapper over
    the library APIs to convert them to the above mentioned signature.
  - A library is expected to have a function "mero_lib_init()". When library
    is linked (via m0_spiel_process_lib_load()), this function will be called
    which can then register relevant library APIs with Mero.
  - An API is registered with Mero using m0_isc_comp_register(). A unique
    identifier (m0_fid) associated with the API is used to store it in the
    internal hash table.
  - A user of the computation can invoke it either using m0_comp_req_exec()
    or by sending m0_fop_isc. In both cases a fid of the computation
    is provided to invoke the same. In the later case the results are
    communicated using RPC adaptive transmission buffers (m0_rpc_at_buf).

  -ISC Service user guide:
    @see
    https://docs.google.com/document/d/1a8qK32HaEzxSlfqpJ8IATNcQaOJIl9-ujX0J7ZdXMSc/edit#

  @section iscservice-sandboxing Sandboxing

  Sandboxing is used by applications that don't have an access to Mero
  internals. Computations involved in sandboxing will communicate with
  Mero instance over a shared memory segment (or POSIX message queues).

  @section iscservice-layout_plan Layout Plans

  File layout maps logical offset from a file to logical offset from available
  device(s). In order to schedule computations over data it's necessary to know
  the interdependency of operations, and this is where the layout access plan
  is required. Kindly refer "layout/plan.h" for detailed documentation.
@{
 */


/**
 * Holds the information private to a computation. This data is useful when a
 * halted computation is resumed.
 */
struct m0_isc_comp_private {
	/**
	 * The fom associated with the instance of a computation. A computation
	 * can register a fom callback on some channel, when it's going to
	 * return M0_FSO_WAIT.
	 * A typical use-case can be of a computation returning M0_FSO_WAIT to
	 * a calling fom, while waiting for disk or network i/o. In this case
	 * the associated fom is made to wait on a channel, internal to a
	 * computation, and it re-launches the computation when i/o completion
	 * is signalled.
	 */
	struct m0_fom *icp_fom;

	/**
	 * The private data for a computation. A computation can save its
	 * current state before passing control to calling fom. It's a
	 * responsibility of the computation to manage the memory associated
	 * with icp_data.
	 */
	void          *icp_data;
};

enum m0_isc_comp_req_type {
	M0_ICRT_LOCAL,
	M0_ICRT_REMOTE,
};

/**
 * A request for a computation holds all the relevant data required to
 * refer and execute a particular computation.
 */
struct m0_isc_comp_req {
/* Public fields: */

	/** Arguments for a computation. */
	struct m0_buf              icr_args;
	/**
	 * Output of the computation. It's allocated by
	 * the computation and caller of the computation
	 * is responsible for the deallocation. In case of
	 * a remote invocation, m0_rpc_at_fini() finalizes it.
	 */
	struct m0_buf              icr_result;
	/** Unique identifier of a computation.  */
	struct m0_fid              icr_comp_fid;
	/** Cookie associated with a computation. */
	struct m0_cookie           icr_cookie;
	/** * Return value of fom execution.  */
	int                        icr_rc;
/* Private fields: */
	/**
	 * A request can be either local or remote.
	 * See m0_isc_comp_req_type.
	 */
	enum m0_isc_comp_req_type  icr_req_type;
	/**
	 * Does request engage into bulkio.
	 */
	bool                       icr_req_bulk;
	/**
	 * State of a computation privately maintained
	 * by itself.
	 */
	struct m0_isc_comp_private icr_comp_data;
	struct m0_mutex            icr_guard;
	/** Channel to announce completion of computation. */
	struct m0_chan             icr_chan;
	/** Fom associated with the request. */
	struct m0_fom              icr_fom;
	struct m0_reqh            *icr_reqh;
};

/**
 * Initializes the request for computation.
 * @param[in] comp_req      Request to be initialized.
 * @param[in] comp_args     Arguments for the computation. This buffer is
 *			    copied internally.
 * @param[in] comp_fid      Unique identifier for the computation.
 * @param[in] comp_cookie   Cookie for the computation.
 * @param[in] comp_req_type Indicates if the request is local or remote.
 * @param[in] reqh          Request handler.
 */
M0_INTERNAL void m0_isc_comp_req_init(struct m0_isc_comp_req *comp_req,
				      const struct m0_buf *comp_args,
				      const struct m0_fid *comp_fid,
				      const struct m0_cookie *comp_cookie,
				      enum m0_isc_comp_req_type comp_req_type,
				      struct m0_reqh *reqh);

M0_INTERNAL void m0_isc_comp_req_fini(struct m0_isc_comp_req *comp_req);

/**
 * Invokes the function locally. This is an asynchronous API, which on
 * completion invokes the registered callback(s) from the channel embedded in
 * m0_isc_comp_req.
 * @param[in]  comp_req A request for computation.
 * @retval     0        On success.
 * @retval    -EINVAL   If isc-service is not present with the Mero instance.
 */
M0_INTERNAL int m0_isc_comp_req_exec(struct m0_isc_comp_req *comp_req);

/**
 * Synchronous version of m0_isc_comp_req_exec(). The return value indicates
 * error if any, associated with the launching of a computation, and actual
 * result of computation shall be reflected by comp_req->icr_rc.
 * @param[in]  comp_req A request for computation.
 * @retval     0        On success.
 * @retval    -EINVAL   If isc-service is not present with the Mero instance.
 */
M0_INTERNAL int m0_isc_comp_req_exec_sync(struct m0_isc_comp_req *comp_req);

/**
 * Registers a "trusted" computation with the service. A prospective
 * caller of this API could be invoked via m0_spiel_process_lib_load().
 * For more details @see sss/process_foms.c::ss_process_fom_tick.
 * @param[in] ftn     An external computation. All computations
 *                    are expected to follow the same signature.
 * @param[in] f_name  Human readable name of the computation.
 * @param[in] ftn_fid A unique identifier associated with the computation.
 * @retval    0       On success.
 * @retval   -EEXIST  When the fid provided already exists.
 */
M0_INTERNAL int m0_isc_comp_register(int (*ftn)(struct m0_buf *arg_in,
						struct m0_buf *args_out,
						struct m0_isc_comp_private
						*comp_data, int *rc),
				     const char *f_name,
				     const struct m0_fid *ftn_fid);

/**
 * Removes the function from service registry. It need not immediately
 * remove the function from in-memory hash table, as there might be
 * ongoing instances of it. But it ensures that no new execution request
 * is accepted, as well as once all the ongoing instances are completed,
 * the function is removed from the hash table.
 * @param[in] ftn_fid An identifier for a registered computation.
 */
M0_INTERNAL void m0_isc_comp_unregister(const struct m0_fid *fid);

/**
 * Probes the state of a computation, with respect to its registration
 * with service.
 * @param[in] fid                 Unique identifier of a computation.
 * @retval    M0_ICS_REGISTERED   If computation is registered.
 * @retval    M0_ICS_UNREGISTERED If computation is present but marked
 *                                unregistered.
 * @retval    -ENOENT             If computation is not present.
 */
M0_INTERNAL int m0_isc_comp_state_probe(const struct m0_fid *fid);

/** @} end of iscservice */
/* __MERO_ISC_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
