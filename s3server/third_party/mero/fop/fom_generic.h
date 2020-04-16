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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>,
 * Original creation date: 09-Aug-2012
 */

#pragma once

#ifndef __MERO_FOP_FOM_GENERIC_H__
#define __MERO_FOP_FOM_GENERIC_H__

#include "fop/fom.h"
#include "lib/types.h"
#include "xcode/xcode_attr.h"
#include "lib/string.h"
#include "lib/string_xc.h"
#include "be/tx.h"
#include "be/tx_xc.h"

struct m0_rpc_item;

/**
 * @addtogroup fom
 */

#define FOM_PHASE_STATS_DATA_SZ(nr)	(M0_FOM_STATS_CNTR_DATA * (nr))

/**
 * "Phases" through which fom execution typically passes.
 *
 * This enumerates standard phases, handled by the generic code independent of
 * fom type.
 *
 * @see https://docs.google.com/a/xyratex.com/Doc?docid=0ATg1HFjUZcaZZGNkNXg4cXpfMjA2Zmc0N3I3Z2Y
 * @see m0_fom_tick_generic()
 */
enum m0_fom_standard_phase {
	M0_FOPH_INIT = M0_FOM_PHASE_INIT,  /*< fom has been initialised. */
	M0_FOPH_FINISH = M0_FOM_PHASE_FINISH,  /*< terminal phase. */
	M0_FOPH_AUTHENTICATE,        /*< authentication loop is in progress. */
	M0_FOPH_AUTHENTICATE_WAIT,   /*< waiting for key cache miss. */
	M0_FOPH_RESOURCE_LOCAL,      /*< local resource reservation loop is in
	                                 progress. */
	M0_FOPH_RESOURCE_LOCAL_WAIT, /*< waiting for a local resource. */
	M0_FOPH_RESOURCE_DISTRIBUTED,/*< distributed resource reservation loop
	                                 is in progress. */
	M0_FOPH_RESOURCE_DISTRIBUTED_WAIT, /*< waiting for a distributed
	                                       resource. */
	M0_FOPH_OBJECT_CHECK,       /*< object checking loop is in progress. */
	M0_FOPH_OBJECT_CHECK_WAIT,  /*< waiting for object cache miss. */
	M0_FOPH_AUTHORISATION,      /*< authorisation loop is in progress. */
	M0_FOPH_AUTHORISATION_WAIT, /*< waiting for userdb cache miss. */
	M0_FOPH_TXN_INIT,           /*< init local transactional context. */
	M0_FOPH_TXN_OPEN,           /*< open local transactional context. */
	M0_FOPH_TXN_WAIT,           /*< waiting for log space. */
	M0_FOPH_SUCCESS,            /*< fom execution completed successfully. */
	M0_FOPH_FOL_REC_ADD,        /*< add a FOL transaction record. */
	M0_FOPH_TXN_COMMIT,         /*< commit local transaction context. */
	M0_FOPH_QUEUE_REPLY,        /*< queuing fop reply.  */
	M0_FOPH_QUEUE_REPLY_WAIT,   /*< waiting for fop cache space. */
	M0_FOPH_TXN_COMMIT_WAIT,    /*< waiting to commit local transaction
	                                context. */
	M0_FOPH_TIMEOUT,            /*< fom timed out. */
	M0_FOPH_FAILURE,            /*< fom execution failed. */
	M0_FOPH_NR,                  /*< number of standard phases. fom type
	                                specific phases have numbers larger than
	                                this. */
	M0_FOPH_TYPE_SPECIFIC        /*< used when only single specific phase
					 present in FOM. */
};

/**
   Standard fom phase transition function.

   This function handles standard fom phases from enum m0_fom_standard_phase.

   First do "standard actions":

   - authenticity checks: reqh verifies that protected state in the fop is
     authentic. Various bits of information in M0 are protected by cryptographic
     signatures made by a node that issued this information: object identifiers
     (including container identifiers and fids), capabilities, locks, layout
     identifiers, other resources identifiers, etc. reqh verifies authenticity
     of such information by fetching corresponding node keys, re-computing the
     signature locally and checking it with one in the fop;

   - resource limits: reqh estimates local resources (memory, cpu cycles,
     storage and network bandwidths) necessary for operation execution. The
     execution of operation is delayed if it would overload the server or
     exhaust resource quotas associated with operation source (client, group of
     clients, user, group of users, job, etc.);

   - resource usage and conflict resolution: reqh determines what distributed
     resources will be consumed by the operation execution and call resource
     management infrastructure to request the resources and deal with resource
     usage conflicts (by calling RM if necessary);

   - object existence: reqh extracts identities of file system objects affected
     by the fop and requests appropriate stores to load object representations
     together with their basic attributes;

   - authorization control: reqh extracts the identity of a user (or users) on
     whose behalf the operation is executed. reqh then uses enterprise user data
     base to map user identities into internal form. Resulting internal user
     identifiers are matched against protection and authorization information
     stored in the file system objects (loaded on the previous step);

   - distributed transactions: for operations mutating file system state, reqh
     sets up local transaction context where the rest of the operation is
     executed.

   Once the standard actions are performed successfully, request handler
   delegates the rest of operation execution to the fom type specific state
   transition function.

   Fom execution proceeds as follows:

   @verbatim

	fop
	 |
	 v                fom->fo_state = FOS_READY
     m0_reqh_fop_handle()-------------->FOM
					 | fom->fo_state = FOS_RUNNING
					 v
				     FOPH_INIT
					 |
			failed		 v         fom->fo_state = FOS_WAITING
		     +<-----------FOPH_AUTHETICATE------------->+
		     |			 |           FOPH_AUTHENTICATE_WAIT
		     |			 v<---------------------+
		     +<----------FOPH_RESOURCE_LOCAL----------->+
		     |			 |           FOPH_RESOURCE_LOCAL_WAIT
		     |			 v<---------------------+
		     +<-------FOPH_RESOURCE_DISTRIBUTED-------->+
		     |			 |	  FOPH_RESOURCE_DISTRIBUTED_WAIT
		     |			 v<---------------------+
		     +<---------FOPH_OBJECT_CHECK-------------->+
		     |                   |              FOPH_OBJECT_CHECK
		     |		         v<---------------------+
		     +<---------FOPH_AUTHORISATION------------->+
		     |			 |            FOPH_AUTHORISATION
	             |	                 v<---------------------+
		     +<---------FOPH_TXN_CONTEXT--------------->+
		     |			 |            FOPH_TXN_CONTEXT_WAIT
		     |			 v<---------------------+
		     +<-------------FOPH_NR_+_1---------------->+
		     |			 |            FOPH_NR_+_1_WAIT
		     v			 v<---------------------+
		 FOPH_FAILED        FOPH_SUCCESS
		     |			 |
		     |			 v
		     +------------FOPH_TXN_COMMIT-------------->+
					 |            FOPH_TXN_COMMIT_WAIT
			    send reply	 v<---------------------+
				FOPH_QUEUE_REPLY------------->+
					 |            FOPH_QUEUE_REPLY_WAIT
					 v<---------------------+
				   FOPH_FINISH ---> m0_fom_fini()

   @endverbatim

   If a generic phase handler function fails while executing a fom, then
   it just sets the m0_fom::fo_rc to the result of the operation and returns
   M0_FSO_WAIT.  m0_fom_tick_generic() then sets the m0_fom::fo_phase to
   M0_FOPH_FAILED, logs an ADDB event, and returns, later the fom execution
   proceeds as mentioned in above diagram.

   If fom fails while executing fop specific operation, the m0_fom::fo_phase
   is set to M0_FOPH_FAILED already by the fop specific operation handler, and
   the m0_fom::fo_rc set to the result of the operation.

   @see m0_fom_phase
   @see m0_fom_phase_outcome

   @param fom, fom under execution

   @retval M0_FSO_AGAIN, if fom operation is successful, transition to next
	   phase, M0_FSO_WAIT, if fom execution blocks and fom goes into
	   corresponding wait phase, or if fom execution is complete, i.e
	   success or failure

   @todo standard fom phases implementation, depends on the support routines for
	handling various standard operations on fop as mentioned above
 */
int m0_fom_tick_generic(struct m0_fom *fom);

M0_INTERNAL void m0_fom_generic_fini(void);
M0_INTERNAL int m0_fom_generic_init(void);

enum {
	M0_FOM_GENERIC_TRANS_NR = 47,
};

extern struct m0_sm_trans_descr
m0_generic_phases_trans[M0_FOM_GENERIC_TRANS_NR];
extern const struct m0_sm_conf m0_generic_conf;

/**
   Generic reply.

   RPC operations that return nothing but error code to sender can use
   this generic reply fop. Request handler uses this type of fop to
   report operation failure in generic fom phases.
 */
struct m0_fop_generic_reply {
	int32_t           gr_rc;
	struct m0_fop_str gr_msg;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

extern struct m0_fop_type m0_fop_generic_reply_fopt;

bool m0_rpc_item_is_generic_reply_fop(const struct m0_rpc_item *item);

/**
 * m0_fop_mod_rep contains common reply values for an UPDATE fop.
 */
struct m0_fop_mod_rep {
	/** Remote ID assigned to this UPDATE operation */
	struct m0_be_tx_remid fmr_remid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL void m0_fom_mod_rep_fill(struct m0_fop_mod_rep *rep,
				     struct m0_fom *fom);

/**
   If item is of type m0_fop_generic_reply then m0_rpc_item_generic_reply_rc()
   extracts and returns error code contained in the fop; otherwise it
   returns 0.
 */
int32_t m0_rpc_item_generic_reply_rc(const struct m0_rpc_item *item);

M0_INTERNAL int m0_fom_tx_commit_wait(struct m0_fom *fom);

/** @} end of fom group */

/* __MERO_FOP_FOM_GENERIC_H__ */
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
