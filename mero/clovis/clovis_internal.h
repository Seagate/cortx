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
 * Original author: Nikita Danilov  <nikita_danilov@seagate.com>
 * AuthorS:         Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                  James  Morse    <james.s.morse@seagate.com>
 *                  Sining Wu       <sining.wu@seagate.com>
 * Revision:        Pratik Shinde   <pratik.shinde@seagate.com>
 *                  Abhishek Saha   <abhishek.saha@seagate.com>
 * Original creation date: 14-Oct-2013
 */

#pragma once

#ifndef __MERO_CLOVIS_CLOVIS_INTERNAL_H__
#define __MERO_CLOVIS_CLOVIS_INTERNAL_H__

#ifdef __KERNEL__
#define M0_CLOVIS_THREAD_ENTER M0_THREAD_ENTER
#else
#define M0_CLOVIS_THREAD_ENTER
#endif

#define OP_OBJ2CODE(op_obj) op_obj->oo_oc.oc_op.op_code

#define CLOVIS_MOCK
#define CLOVIS_FOR_M0T1FS

#include "module/instance.h"
#include "mero/init.h"

#include "ioservice/io_fops.h"  /* m0_io_fop_{init,fini,release} */
#include "conf/schema.h"        /* m0_conf_service_type */
#include "conf/confc.h"         /* m0_confc */
#include "layout/pdclust.h"     /* struct m0_pdclust_attr */
#include "pool/pool.h"          /* struct m0_pool */
#include "reqh/reqh.h"          /* struct m0_reqh */
#include "rm/rm.h"              /* stuct m0_rm_owner */
#include "lib/refs.h"
#include "lib/hash.h"
#include "file/file.h"          /* struct m0_file */
#include "mero/ha.h"            /* m0_mero_ha */
#include "addb2/identifier.h"

/** @todo: remove this - its part of the test framework */
#include "be/ut/helper.h"       /* struct m0_be_ut_backend */

#include "clovis/clovis.h"      /* m0_clovis_* */
#include "clovis/clovis_idx.h"  /* m0_clovis_idx_* */
#include "clovis/pg.h"          /* nwxfer and friends */
#include "clovis/sync.h"        /* clovis_sync_request */
#include "fop/fop.h"

struct m0_clovis_idx_service_ctx;

#ifdef CLOVIS_FOR_M0T1FS
/**
 * Maximum length for an object's name.
 */
enum {
	M0_OBJ_NAME_MAX_LEN = 64
};
#endif
/**
 * Number of buckets for m0_clovis::m0c_rm_ctxs hash-table.
 */
enum {
	M0_CLOVIS_RM_HBUCKET_NR = 100
};

enum m0_clovis_entity_states {
	M0_CLOVIS_ES_INIT = 1,
	M0_CLOVIS_ES_CREATING,
	M0_CLOVIS_ES_DELETING,
	M0_CLOVIS_ES_OPENING,
	M0_CLOVIS_ES_OPEN,
	M0_CLOVIS_ES_CLOSING,
	M0_CLOVIS_ES_FAILED
};

/**
 * Parity buffers used for addressing an IO request.
 */
enum  m0_clovis_pbuf_type {
	/**
	 * Explicitly allocated buffers. This is done during:
	 * i.  Read operation in parity-verify mode (independent of the layout).
	 * ii. Write operation when the layout is not replicated.
	 */
	M0_CLOVIS_PBUF_DIR,
	/**
	 * Hold a pointer to data buffer. It's required for write IO on an
	 * object with the replicated layout.
	 */
	M0_CLOVIS_PBUF_IND,
	/**
	 * Parity units are not required. Used for read IO without parity
	 * verify mode.
	 */
	M0_CLOVIS_PBUF_NONE
};

M0_INTERNAL bool clovis_entity_invariant_full(struct m0_clovis_entity *ent);
M0_INTERNAL bool clovis_entity_invariant_locked(const struct
						m0_clovis_entity *ent);

void m0_clovis_op_fini(struct m0_clovis_op *op);

struct m0_clovis_ast_rc {
	struct m0_sm_ast        ar_ast;
	int                     ar_rc;
	uint64_t                ar_magic;

};

/*
 * Clovis has a number of nested structures, all of which are passed to the
 * application as a 'struct m0_clovis_op'. The application may in fact
 * allocate some of these structures, without knowing what they are, or how
 * big they should be. 'struct m0_clovis_op' is always the first member, and
 * contains all the fields the application is permitted to change.
 *
 * A 'struct m0_clovis_op' is always contained in a
 * 'struct m0_clovis_op_common'. This contains the fields that are common to
 * all operations, namely the launch/executed/finalise callbacks. The
 * application is not permitted to change (or even see) these.
 *
 * Operations then have a different 'super type' depending on whether they are
 * operating on an object, index or realm.
 *
 * Object operations always have a 'struct m0_clovis_op_obj', this represents
 * the namespace/metadata aspects of the object, such as its layout. Operations
 * such as create/delete will use this struct as the root 'type' of their work,
 * as they don't need IO buffers etc.
 *
 * 'struct m0_clovis_op_io' is the last (and biggest/highest) type, it contains
 * the databuf and paritybuf arrays for reading/writing object data.
 *
 *                   +---m0_clovis_op_common---+
 *                   |                          |
 *                   |    +-m0_clovis_op-+      |
 *                   |    |              |      |
 *                   |    +--------------+      |
 *                   |                          |
 *                   +--------------------------+
 *
 *               \/_                        _\/
 *
 * +m0_clovis_op_io---------------+     +m0_clovis_op_idx-------+
 * | +m0_clovis_op_obj---------+  |     |                       |
 * | |                         |  |     | [m0_clovis_op_common] |
 * | |  [m0_clovis_op_common]  |  |     |                       |
 * | |                         |  |     +-----------------------+
 * | +-------------------------+  |
 * |                              |
 * +------------------------------+
 *
 */
struct m0_clovis_op_common {
	struct m0_clovis_op    oc_op;
	uint64_t               oc_magic;

#ifdef CLOVIS_MOCK
	/* Timer used to move sm between states*/
	struct m0_sm_timer     oc_sm_timer;
#endif

	void                 (*oc_cb_launch)(struct m0_clovis_op_common *oc);
	void                 (*oc_cb_replied)(struct m0_clovis_op_common *oc);
	void                 (*oc_cb_cancel)(struct m0_clovis_op_common *oc);
	void                 (*oc_cb_fini)(struct m0_clovis_op_common *oc);
	void                 (*oc_cb_free)(struct m0_clovis_op_common *oc);

	/* Callback operations for states*/
	void                 (*oc_cb_executed)(void *args);
	void                 (*oc_cb_stable)(void *args);
	void                 (*oc_cb_failed)(void *args);
};

/**
 * An index operation.
 */
struct m0_clovis_op_idx {
	struct m0_clovis_op_common  oi_oc;
	uint64_t                    oi_magic;

	struct m0_clovis_idx       *oi_idx;

	/* K-V pairs */
	struct m0_bufvec           *oi_keys;
	struct m0_bufvec           *oi_vals;

	/* Hold per key-value query return code. */
	int32_t                    *oi_rcs;

	/* Number of queries sent to index*/
	int32_t                     oi_nr_queries;
	bool                        oi_query_rc;

	struct m0_sm_group         *oi_sm_grp;
	struct m0_clovis_ast_rc     oi_ar;

	/* A bit-mask of m0_clovis_op_idx_flags. */
	uint32_t                    oi_flags;
};

/**
 * Generic operation on a clovis object.
 */
struct m0_clovis_op_obj {
	struct m0_clovis_op_common  oo_oc;
	uint64_t                    oo_magic;

	struct m0_sm_group         *oo_sm_grp;
	struct m0_clovis_ast_rc     oo_ar;

	struct m0_fid               oo_fid;
#ifdef CLOVIS_FOR_M0T1FS
	struct m0_fid               oo_pfid;
	struct m0_buf               oo_name;
#endif
	struct m0_fid               oo_pver;     /* cob pool version */
	struct m0_layout_instance  *oo_layout_instance;

	/* MDS fop */
	struct m0_fop              *oo_mds_fop;
};

/**
 * An IO operation on a clovis object.
 */
struct m0_clovis_op_io {
	struct m0_clovis_op_obj           ioo_oo;
	uint64_t                          ioo_magic;

	struct m0_clovis_obj             *ioo_obj;
	struct m0_indexvec                ioo_ext;
	struct m0_bufvec                  ioo_data;
	struct m0_bufvec                  ioo_attr;
	uint64_t                          ioo_attr_mask;

	/* Object's pool version */
	struct m0_fid                     ioo_pver;

	/** @todo: remove this */
	uint32_t                          ioo_rc;

	/**
	 * Array of struct pargrp_iomap pointers.
	 * Each pargrp_iomap structure describes the part of parity group
	 * spanned by segments from ::ir_ivec.
	 */
	struct pargrp_iomap             **ioo_iomaps;

	/** Number of pargrp_iomap structures. */
	uint64_t                          ioo_iomap_nr;

	/** Indicates whether data buffers be replicated or not. */
	enum m0_clovis_pbuf_type          ioo_pbuf_type;
	/** Number of pages to read in RMW */
	uint64_t                          ioo_rmw_read_pages;

	/** State machine for this io operation */
	struct m0_sm                      ioo_sm;

	/** Operations for moving along state transitions */
	const struct m0_clovis_op_io_ops *ioo_ops;

	/**
	 * flock here is used to get DI details for a file. When a better way
	 * is found, remove it completely. See cob_init.
	 */
	struct m0_file                    ioo_flock;

	/** Network transfer request */
	struct nw_xfer_request            ioo_nwxfer;

	/**
	* State of SNS repair process with respect to
	* file_to_fid(io_request::ir_file).
	* There are only 2 states possible since Mero client IO path
	* involves a file-level distributed lock on global fid.
	*  - either SNS repair is still due on associated global fid.
	*  - or SNS repair has completed on associated global fid.
	*/
	enum sns_repair_state             ioo_sns_state;

	/**
	 * An array holding ids of failed sessions. The vacant entries are
	 * marked as ~(uint64_t)0.
	 * XXX This is a temporary solution. Sould be removed once
	 * MERO-899 lands into master.
	 */
	uint64_t                        *ioo_failed_session;

	/**
	* Total number of parity-maps associated with this request that are in
	* degraded mode.
	*/
	uint32_t                         ioo_dgmap_nr;
	bool                             ioo_dgmode_io_sent;

	/**
	 * Used by copy_{to,from}_application to indicate progress in
	 * log messages
	 */
	uint64_t                         ioo_copied_nr;

	/** Cached map index value from ioreq_iosm_handle_* functions */
	uint64_t                         ioo_map_idx;

	/** Ast for scheduling the 'next' callback */
	struct m0_sm_ast                 ioo_ast;

	/**
	 * Ast for moving state to READ/WRITE COMPLETE and to launch
	 * iosm_handle_executed.
	 */
	struct m0_sm_ast                 ioo_done_ast;

	/** Clink for waiting on another state machine */
	struct m0_clink                  ioo_clink;

	/** Channel to wait for this operation to be finalised */
	struct m0_chan                   ioo_completion;

	/**
	 * In case of a replicated layout indicates whether there is any
	 * corrupted parity group that needs to be rectified.
	 */
	bool                             ioo_rect_needed;

	/**
	 * XXX: get rid of this kludge!
	 * Relying on this to remove duplicate mapping for the same nxfer_req
	 */
	int                              ioo_addb2_mapped;
};

struct m0_clovis_io_args {
	struct m0_clovis_obj      *ia_obj;
	enum m0_clovis_obj_opcode  ia_opcode;
	struct m0_indexvec        *ia_ext;
	struct m0_bufvec          *ia_data;
	struct m0_bufvec          *ia_attr;
	uint64_t                   ia_mask;
};

struct m0_clovis_op_md {
	struct m0_clovis_op_common mdo_oc;
	struct m0_bufvec           mdo_key;
	struct m0_bufvec           mdo_val;
	struct m0_bufvec           mdo_chk;
};

bool m0_clovis_op_md_invariant(const struct m0_clovis_op_md *mop);

union m0_clovis_max_size_op {
	struct m0_clovis_op_io io;
	struct m0_clovis_op_md md;
};

/**
 * SYNC operation and related data structures.
 */
struct m0_clovis_op_sync {
	struct m0_clovis_op_common  os_oc;
	uint64_t                    os_magic;

	struct m0_sm_group         *os_sm_grp;
	struct m0_clovis_ast_rc     os_ar;

	struct clovis_sync_request *os_req;

	/**
	 * Mode to set the fsync fop (m0_fop_fsync::ff_fsync_mode).
	 * mdservice/fsync_fops.h defines 2 modes: M0_FSYNC_MODE_ACTIVE and
	 * M0_FSYNC_MODE_PASSIVE. In passive mode the fsync fom merely
	 * waits for the transactions to become committed, in active mode it
	 * uses m0_be_tx_force(), to cause the transactions to make progress
	 * more quickly than they otherwise would.
	 */
	int32_t                     os_mode;
};

/**
 * SM states of component object (COB) request.
 */
enum m0_clovis_cob_req_states {
	CLOVIS_COB_REQ_ACTIVE,
	CLOVIS_COB_REQ_SENDING,
	CLOVIS_COB_REQ_DONE,
} M0_XCA_ENUM;

/**
 * Request to ioservice component object (COB).
 */
struct m0_clovis_ios_cob_req {
	struct m0_clovis_op_obj *icr_oo;
	uint32_t                 icr_index;
	struct m0_clovis_ast_rc  icr_ar;
	uint64_t                 icr_magic;
};

struct m0_clovis_layout_ops {
	int  (*lo_alloc) (struct m0_clovis_layout **);
	int  (*lo_get) (struct m0_clovis_layout *);
	void (*lo_put) (struct m0_clovis_layout *);
	/** Function to construct IO for an object. */
	int  (*lo_io_build)(struct m0_clovis_io_args *io_args, struct m0_clovis_op **op);
};

/** miscallaneous constants */
enum {
	/*  4K, typical linux/intel page size */
	CLOVIS_DEFAULT_BUF_SHIFT        = 12,
	/* 512, typical disk sector */
	CLOVIS_MIN_BUF_SHIFT            = 9,

	/* RPC */
	CLOVIS_RPC_TIMEOUT              = 60, /* Seconds */
	CLOVIS_RPC_MAX_RETRIES          = 60,
	CLOVIS_RPC_RESEND_INTERVAL      = M0_MKTIME(CLOVIS_RPC_TIMEOUT, 0) /
					  CLOVIS_RPC_MAX_RETRIES,
	CLOVIS_MAX_NR_RPC_IN_FLIGHT     = 100,

	CLOVIS_AST_THREAD_TIMEOUT       = 10,
	CLOVIS_MAX_NR_CONTAINERS        = 1024,

	CLOVIS_MAX_NR_IOS               = 128,
	CLOVIS_MD_REDUNDANCY            = 3,

	/*
	 * These constants are used to create buffers acceptable to the
	 * network code.
	 */
	CLOVIS_NETBUF_MASK              = 4096 - 1,
	CLOVIS_NETBUF_SHIFT             = 12,
};

/**
 * The initlift state machine moves in one of these two directions.
 */
enum clovis_initlift_direction {
	CLOVIS_SHUTDOWN = -1,
	CLOVIS_STARTUP = 1,
};

/**
 * m0_clovis represents a clovis 'instance', a connection to a mero cluster.
 * It is initalised by m0_clovis_init, and finalised with m0_clovis_fini.
 * Any operation to open a realm requires the clovis instance to be specified,
 * allowing an application to work with multiple mero clusters.
 *
 * The prefix m0c is used over 'ci', to avoid confusion with colibri inode.
 */
struct m0_clovis {
	uint64_t                                m0c_magic;

	/** Mero instance */
	struct m0                              *m0c_mero;

	/** State machine group used for all operations and entities. */
	struct m0_sm_group                      m0c_sm_group;

	/** Request handler for the instance*/
	struct m0_reqh                          m0c_reqh;

	struct m0_clink                         m0c_conf_exp;
	struct m0_clink                         m0c_conf_ready;
	struct m0_clink                         m0c_conf_ready_async;

	struct m0_fid                           m0c_process_fid;
	struct m0_fid                           m0c_profile_fid;

	/**
	 * The following fields picture the pools in mero.
	 * m0c_pools_common: details about all pools in mero.
	 * m0c_pool: current pool used by this clovis instance
	 */
	struct m0_pools_common                  m0c_pools_common;

	/** HA service context. */
	struct m0_reqh_service_ctx             *m0c_ha_rsctx;

	struct m0_mero_ha                       m0c_mero_ha;

	/** Index service context. */
	struct m0_clovis_idx_service_ctx        m0c_idx_svc_ctx;

	/**
	 * Instantaneous count of pending io requests.
	 * Every io request increments this value while initializing
	 * and decrements it while finalizing.
	 */
	struct m0_atomic64                      m0c_pending_io_nr;

	/** Indicates the state of confc.  */
	struct m0_confc_update_state            m0c_confc_state;

	/** Channel on which mero internal data structures refreshed
	 *  as per new configurtion event brodcast.
	 */
	struct m0_chan                          m0c_conf_ready_chan;

	/**
	 * Reference counter of this configuration instance users.
	 * When it drops to zero, all data structures using configuration
	 * cache can be refreshed.
	 * clovis_[idx_op|obj_io]_cb_launch get ref using m0_clovis__io_ref_get
	 * and clovis_idx_op_complete & ioreq_iosm_handle_executed put
	 * ref using m0_clovis__io_ref_put.
	 */
	struct m0_ref                            m0c_ongoing_io;

	/** Special thread which runs ASTs from io requests. */
	/* Also required for confc to connect! */
	struct m0_thread                        m0c_astthread;

	/** flag used to make the ast thread exit */
	bool                                    m0c_astthread_active;

	/** Channel on which io waiters can wait. */
	struct m0_chan                          m0c_io_wait;

#ifdef CLOVIS_FOR_M0T1FS
	/** Root fid, retrieved from mdservice in mount time. */
	struct m0_fid                           m0c_root_fid;

	/** Maximal allowed namelen (retrived from mdservice) */
	int                                     m0c_namelen;
#endif
	/** local endpoint address module parameter */
	char                                   *m0c_laddr;
	struct m0_net_xprt                     *m0c_xprt;
	struct m0_net_domain                    m0c_ndom;
	struct m0_net_buffer_pool               m0c_buffer_pool;
	struct m0_rpc_machine                   m0c_rpc_machine;

	/** Clovis configuration, it takes place of m0t1fs mount options*/
	struct m0_clovis_config                *m0c_config;

	/**
	 * m0c_initlift_xxx fields control the progress of init/fini a
	 * clovis instance.
	 *  - sm: the state machine for initialising this clovis instance
	 *  - direction: up or down (init/fini)
	 *  - rc: the first failure value when aborting initialisation.
	 */
	struct m0_sm                            m0c_initlift_sm;
	enum clovis_initlift_direction          m0c_initlift_direction;
	int                                     m0c_initlift_rc;

#ifdef CLOVIS_MOCK
	struct m0_htable                        m0c_mock_entities;
#endif

	struct m0_htable                        m0c_rm_ctxs;
};

/** CPUs semaphore - to control CPUs usage by parity calcs. */
extern struct m0_semaphore clovis_cpus_sem;

/**
 * Represents the context needed for the RM to lock and unlock
 * object resources (here mero objects).
 */
struct m0_clovis_rm_lock_ctx {
	/**
	 * Locking mechanism provided by RM, rmc_file::fi_fid contains
	 * fid of gob.
	 */
	struct m0_file          rmc_file;
	/** An owner for maintaining file locks. */
	struct m0_rm_owner      rmc_owner;
	/** Remote portal for requesting resource from creditor. */
	struct m0_rm_remote     rmc_creditor;
	/** Key for the hash-table */
	struct m0_fid           rmc_key;
	/**
	 * Reference counter to book keep how many operations are using
	 * this rm_ctx.
	 */
	struct m0_ref           rmc_ref;
	/** back pointer to hash-table where this rm_ctx will be stored. */
	struct m0_htable       *rmc_htable;
	/** A linkage in hash-table for storing RM lock contexts. */
	struct m0_hlink         rmc_hlink;
	uint64_t                rmc_magic;
	/** A generation count for cookie associated with this ctx. */
	uint64_t                rmc_gen;
};

/** Methods for hash-table holding rm_ctx for RM locks */
M0_HT_DECLARE(rm_ctx, M0_INTERNAL, struct m0_clovis_rm_lock_ctx, struct m0_fid);

/**
 * A wrapper structure over m0_rm_incoming.
 * It represents a request to borrow/sublet resource
 * form remote RM creditor and lock/unlock the object.
 */
struct m0_clovis_rm_lock_req {
	struct m0_rm_incoming rlr_in;
	struct m0_mutex       rlr_mutex;
	int32_t               rlr_rc;
	struct m0_chan        rlr_chan;
};

/**
 * Bob's for shared data structures in files
 */
extern const struct m0_bob_type oc_bobtype;
extern const struct m0_bob_type oo_bobtype;
extern const struct m0_bob_type op_bobtype;
extern const struct m0_bob_type ar_bobtype;

M0_BOB_DECLARE(M0_INTERNAL, m0_clovis_op_common);
M0_BOB_DECLARE(M0_INTERNAL, m0_clovis_op_obj);
M0_BOB_DECLARE(M0_INTERNAL, m0_clovis_op);
M0_BOB_DECLARE(M0_INTERNAL, m0_clovis_ast_rc);

/** global init/fini, used by mero/init.c */
M0_INTERNAL int m0_clovis_global_init(void);
M0_INTERNAL void m0_clovis_global_fini(void);

/**
 * Gets the confc from clovis instance.
 *
 * @param m0c clovis instance.
 * @return the confc used by this clovis instance.
 */
M0_INTERNAL struct m0_confc* m0_clovis_confc(struct m0_clovis *m0c);

M0_INTERNAL int m0_clovis_op_executed(struct m0_clovis_op *op);
M0_INTERNAL int m0_clovis_op_stable(struct m0_clovis_op *op);
M0_INTERNAL int m0_clovis_op_failed(struct m0_clovis_op *op);
M0_INTERNAL int m0_clovis_op_get(struct m0_clovis_op **op, size_t size);

/**
 * Returns the m0_clovis clovis instance, found from the provided operation.
 *
 * @param op The Operation to find the instance for.
 * @return A pointer to the m0_clovis instance.
 */
M0_INTERNAL struct m0_clovis *
m0_clovis__entity_instance(const struct m0_clovis_entity *entity);

/**
 * Returns the m0_clovis clovis instance, found from the provided operation.
 *
 * @param op The Operation to find the instance for.
 * @return A pointer to the m0_clovis instance.
 */
M0_INTERNAL struct m0_clovis *
m0_clovis__op_instance(const struct m0_clovis_op *op);

/**
 * Returns generic clovis op from io op.
 */
M0_INTERNAL struct m0_clovis_op *
m0_clovis__ioo_to_op(struct m0_clovis_op_io *ioo);

/**
 * Returns the m0_clovis clovis instance, found from the provided object.
 *
 * @param obj The object to find the instance for.
 * @return A pointer to the m0_clovis instance.
 */
M0_INTERNAL struct m0_clovis *
m0_clovis__obj_instance(const struct m0_clovis_obj *obj);

/**
 * Returns the clovis instance associated to an object operation.
 *
 * @param oo object operation pointing to the instance.
 * @return a pointer to the clovis instance associated to the entity.
 */
M0_INTERNAL struct m0_clovis*
m0_clovis__oo_instance(struct m0_clovis_op_obj *oo);

/**
 * Returns if clovis instance is operating under oostore mode.
 */
M0_INTERNAL bool m0_clovis__is_oostore(struct m0_clovis *instance);

/* sm conf that needs registering by m0_clovis_init */
extern struct m0_sm_conf clovis_op_conf;
extern struct m0_sm_conf clovis_entity_conf;

/* used by the entity code to create operations */
M0_INTERNAL int m0_clovis_op_alloc(struct m0_clovis_op **op, size_t op_size);

M0_INTERNAL int m0_clovis_op_init(struct m0_clovis_op *op,
				  const struct m0_sm_conf *conf,
				  struct m0_clovis_entity *entity);

/* XXX juan: add doxygen */
M0_INTERNAL void m0_clovis_init_io_op(void);

/**
 * Checks the data struct holding the AST information  is not malformed
 * or corrupted.
 *
 * @param ar The pointer to AST information.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
M0_INTERNAL bool m0_clovis_op_obj_ast_rc_invariant(struct m0_clovis_ast_rc *ar);

/**
 * Checks an object operation is not malformed or corrupted.
 *
 * @param oo object operation to be checked.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
M0_INTERNAL bool m0_clovis_op_obj_invariant(struct m0_clovis_op_obj *oo);

/**
 * Checks an object's IO operation is not malformed or corrupted.
 *
 * @param iop object's IO operation to be checked.
 * @return true if the operation is not malformed or false if some error
 * was detected.
 */
M0_INTERNAL bool m0_clovis_op_io_invariant(const struct m0_clovis_op_io *iop);

/**
 * Retrieves the ios session corresponding to a container_id. The ioservice
 * for an object is calculated from the container id.
 *
 * @param cinst clovis instance.
 * @param container_id container ID.
 * @return the session associated to the
 * @remark container_id == 0 is not valid.
 */
M0_INTERNAL struct m0_rpc_session *
m0_clovis_obj_container_id_to_session(struct m0_pool_version *pv,
				      uint64_t container_id);

/**
 * Selects a locality for an operation.
 *
 * @param m0c The clovis instance we are working with.
 * @return the pointer to assigned locality for success, NULL otherwise.
 */
M0_INTERNAL struct m0_locality *
m0_clovis__locality_pick(struct m0_clovis *cinst);

/**
 * Checks object's cached pool version is valid.
 *
 * @param obj The object to be checked.
 * @return true for valid pool version, false otherwise.
 */
M0_INTERNAL bool
m0_clovis__obj_poolversion_is_valid(const struct m0_clovis_obj *obj);

/**
 * Sends COB fops to mdservices or ioservices depending on COB operation's
 * protocol.
 *
 * @param oo object operation being processed.
 * @return 0 if success or an error code otherwise.
 */
M0_INTERNAL int m0_clovis__obj_namei_send(struct m0_clovis_op_obj *oo);

/**
 * Cancels fops sent during namei launch operation
 *
 * @param op operation to be cancelled
 * @return 0 if success ot an error code otherwise
 */
M0_INTERNAL int m0_clovis__obj_namei_cancel(struct m0_clovis_op *op);

/**
 * Get object's attributes from services synchronously.
 *
 * @param obj object to be queried for.
 * @return 0 if success or an error code otherwise.
 */
M0_INTERNAL int m0_clovis__obj_attr_get_sync(struct m0_clovis_obj *obj);

/**
 * Reads the specified layout from the mds.
 *
 * @param m0c The clovis instance we are working with, contains the layout db.
 * @param lid The layout identifier to read.
 * @param l_out Where to store the resultant layout.
 * @return 0 for success, an error code otherwise.
 */
M0_INTERNAL int m0_clovis_layout_mds_lookup(struct m0_clovis  *m0c,
					    uint64_t           lid,
					    struct m0_layout **l_out);

/**
 * Initialises an entity.
 *
 * @param entity Entity to be initialised.
 * @param parent Parent realm of the entity.
 * @param id Identifier of the entity.
 * @param type Type of the entity.
 */
M0_INTERNAL void m0_clovis_entity_init(struct m0_clovis_entity *entity,
				       struct m0_clovis_realm  *parent,
				       const struct m0_uint128 *id,
				       const enum m0_clovis_entity_type type);
/**
 * Gets current valid pool version from clovis instance.
 *
 * @param instance The clovis instance containing information of pool and pool
 *                 versions.
 * @param pv The returned pool version.
 */
M0_INTERNAL int
m0_clovis__obj_pool_version_get(struct m0_clovis_obj *obj,
				struct m0_pool_version **pv);

/**
 * Gets the default layout identifier from confd.
 *
 * @param instance The clovis instance containing information of confd.
 * @return Default layout id.
 */
M0_INTERNAL uint64_t
m0_clovis__obj_layout_id_get(struct m0_clovis_op_obj *oo);

/**
 * Builds a layout instance using the supplied layout.
 *
 * @param cinst clovis instance.
 * @param layout_id ID of the layout.
 * @param fid (global) fid of the object this instance is associated to.
 * @param[out] linst new layout instance.
 * @return 0 if the operation succeeds or an error code (<0) otherwise.
 * @remark This function might trigger network traffic.
 */
M0_INTERNAL int
m0_clovis__obj_layout_instance_build(struct m0_clovis *cinst,
				     const uint64_t layout_id,
				     const struct m0_fid *fid,
				     struct m0_layout_instance **linst);

/**
 * Fetches the pool version of supplied object and stores as an object
 * attribute.
 *
 * @param obj object whose pool version needs to be found.
 * @return 0 if the operation succeeds or an error code (<0) otherwise.
 */
M0_INTERNAL int m0_clovis__cob_poolversion_get(struct m0_clovis_obj *obj);

M0_INTERNAL int clovis_obj_fid_make_name(char *name, size_t name_len,
					 const struct m0_fid *fid);
/**
 * TODO: doxygen
 */
M0_INTERNAL struct m0_clovis_obj*
m0_clovis__obj_entity(struct m0_clovis_entity *entity);
M0_INTERNAL uint64_t m0_clovis__obj_lid(struct m0_clovis_obj *obj);
M0_INTERNAL enum m0_clovis_layout_type
m0_clovis__obj_layout_type(struct m0_clovis_obj *obj);
M0_INTERNAL struct m0_fid m0_clovis__obj_pver(struct m0_clovis_obj *obj);
M0_INTERNAL void m0_clovis__obj_attr_set(struct m0_clovis_obj *obj,
					 struct m0_fid         pver,
					 uint64_t              lid);
M0_INTERNAL bool
m0_clovis__obj_pool_version_is_valid(const struct m0_clovis_obj *obj);
M0_INTERNAL int m0_clovis__obj_io_build(struct m0_clovis_io_args *args,
					struct m0_clovis_op **op);
M0_INTERNAL void m0_clovis__obj_op_done(struct m0_clovis_op *op);

M0_INTERNAL bool m0_clovis__is_read_op(struct m0_clovis_op *op);
M0_INTERNAL bool m0_clovis__is_update_op(struct m0_clovis_op *op);

M0_INTERNAL int m0_clovis__io_ref_get(struct m0_clovis *m0c);
M0_INTERNAL void m0_clovis__io_ref_put(struct m0_clovis *m0c);
M0_INTERNAL struct m0_file *m0_clovis_fop_to_file(struct m0_fop *fop);
M0_INTERNAL bool clovis_entity_id_is_valid(const struct m0_uint128 *id);

/** @} end of clovis group */

#endif /* __MERO_CLOVIS_CLOVIS_INTERNAL_H__ */

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
