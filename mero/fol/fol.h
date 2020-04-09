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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 09/09/2010
 */

#pragma once

#ifndef __MERO_FOL_FOL_H__
#define __MERO_FOL_FOL_H__

/**
   @defgroup fol File operations log

   File operations log (fol) is a per-node collection of records, describing
   updates to file system state carried out on the node. See HLD for the
   description of requirements, usage patterns and constraints on fol,
   as well as important terminology (update, operation, etc.):
   https://docs.google.com/a/seagate.com/document/d/1Rca4BVw3EatIQ-wQ6XsB-xRBSlV
mN9wIcbuVKeZ8lD4/comment

   A fol is represented by an instance of struct m0_fol. A fol record
   is represented by the m0_fol_rec data type.

   A fol record contains the list of fol record fragments, belonging
   to fol record fragment types, added during updates. These fol
   record fragments provide flexibility for modules to participate
   in a transaction without global knowledge.

   @see m0_fol_frag : FOL record fragment.
   @see m0_fol_frag_type : FOL record fragment type.

   m0_fol_frag_ops structure contains operations for undo and redo of
   FOL record fragments.

   @see m0_fol_frag_init() : Initializes m0_fol_frag with
				 m0_fol_frag_type_ops.
   @see m0_fol_frag_fini() : Finalizes FOL record fragment.

   @see m0_fol_frag_type_register() : Registers FOL record fragment type.
   @see m0_fol_frag_type_deregister() : Deregisters FOL record fragment
                                            type.

   FOL record fragments list is kept in m0_fol_rec::fr_frags which is
   initialized in m0_fol_rec_init().

   m0_fol_rec_encode() is used to compose FOL record from FOL record descriptor
   and fragments. It encodes the FOL record fragments in the list
   m0_fol_rec:fr_frags in a buffer, which then will be added into the BE log.

   @see m0_fol_rec_encode()
   @see m0_fol_rec_decode()

   m0_fol_frag_type_init() and m0_fol_frag_type_fini() are added
   to initialize and finalize FOL fragment types.
   FOL record fragment types are registered in a global array of FOL record
   fragments using m0_fol_frag_type::rpt_index.

   After successful execution of updates on server side, in FOM
   generic phase using m0_fom_fol_rec_encode() FOL record fragments
   in the list are combined in a FOL record and is made persistent.
   Before this phase all FOL record fragments need to be added in
   the list after completing their updates.

   After retrieving FOL record from the storage, FOL record fragments
   are decoded based on fragment type using index and are used in
   undo or redo operations.

   @{
 */

/* export */
struct m0_fol;
struct m0_fol_rec;

/* import */
#include "lib/types.h"      /* uint64_t */
#include "lib/arith.h"      /* M0_IS_8ALIGNED */
#include "lib/mutex.h"
#include "lib/tlist.h"
#include "fid/fid.h"
#include "be/tx_credit.h"
#include "dtm/dtm_update.h" /* m0_update_id, m0_epoch_id */
#include "fid/fid_xc.h"
#include "dtm/dtm_update_xc.h"
#include "fdmi/src_rec.h"   /* m0_fdmi_src_rec */

struct m0_be_tx;
struct m0_epoch_id;

enum {
	/*
	 * The maximum possible length of fol record.
	 *
	 * We need to obtain sufficient BE credit before adding new record
	 * to the fol. Fol records are of variable length and the actual
	 * length is hard, if possible, to calculate at the moment of
	 * m0_fol_credit() call. We use the empirical value of maximum
	 * possible record length instead.
	 *
	 * XXX REVISEME: If the value is not sufficient, increase it.
	 * Alternative (proper?) solution is to calculate the size of fol
	 * record as a function of rpc opcode.
	 */
	/* FOL_REC_MAXSIZE = 1024 * 1024 */

	/* EN: Previous size is too big to fit into one RPC message */
	FOL_REC_MAXSIZE = 1024*50
};

/**
   In-memory representation of a fol.

   <b>Liveness rules and concurrency control.</b>

   m0_fol liveness is managed by the user (the structure is not reference
   counted) which is responsible for guaranteeing that m0_fol_fini() is the last
   call against a given instance.

   FOL code manages concurrency internally: multiple threads can call m0_fol_*()
   functions against the same fol (except for m0_fol_init() and m0_fol_fini()).
 */
struct m0_fol {
	/** Lock, serializing fol access. */
	struct m0_mutex    f_lock;
};

/**
   Initialise in-memory fol structure.
 */
M0_INTERNAL void m0_fol_init(struct m0_fol *fol);
M0_INTERNAL void m0_fol_fini(struct m0_fol *fol);

/**
   Fixed part of a fol record.

   @see m0_fol_rec
 */
struct m0_fol_rec_header {
	/** Number of record fragments added to the record. */
	uint32_t            rh_frags_nr;
	/**
	 * Length of the remaining operation type specific data in bytes.
	 *
	 * @note XXX Currently this is the length of encoded record.
	 */
	uint32_t            rh_data_len;
	/**
	 * Identifier of this update.
	 *
	 * @note The update might be for a different node.
	 */
	struct m0_update_id rh_self;
	uint64_t            rh_magic;
} M0_XCA_RECORD;

M0_BASSERT(M0_IS_8ALIGNED(sizeof(struct m0_fol_rec_header)));

/**
   In-memory representation of a fol record.

   m0_fol_rec is bound to a particular fol and remembers its
   location in the log.
 */
struct m0_fol_rec {
	struct m0_fol            *fr_fol;
	uint64_t                  fr_tid;
	struct m0_fol_rec_header  fr_header;
	/** A DTM epoch this update is a part of. */
	struct m0_epoch_id       *fr_epoch;
	/** Identifiers of sibling updates. */
	struct m0_fol_update_ref *fr_sibling;
	/**
	   A list of all FOL record fragments in a record.
	   Fragments are linked through m0_fol_frag:rp_link to this list.
	 */
	struct m0_tl              fr_frags;
	/** FDMI Source Record entry. */
	struct m0_fdmi_src_rec    fr_fdmi_rec;
};

/**
   Initializes fol record fragments list.

   The user must call m0_fol_rec_fini() when finished dealing with
   the record.
 */
M0_INTERNAL void m0_fol_rec_init(struct m0_fol_rec *rec, struct m0_fol *fol);

/** Finalizes fol record fragments list. */
M0_INTERNAL void m0_fol_rec_fini(struct m0_fol_rec *rec);

/**
   Encodes the fol record @rec at the specified buffer @at.

   @see m0_fol_rec_put()
 */
M0_INTERNAL int m0_fol_rec_encode(struct m0_fol_rec *rec, struct m0_buf *at);

/**
   Decodes a record into @rec from the specified buffer @at.

   @at is m0_be_tx::t_payload.

   @rec must be initialized with m0_fol_rec_init() beforehand.
   The user must call m0_fol_rec_fini() when finished dealing with
   the record.
 */
M0_INTERNAL int m0_fol_rec_decode(struct m0_fol_rec *rec, struct m0_buf *at);

int m0_fol_rec_to_str(struct m0_fol_rec *rec, char *str, int str_len);

M0_INTERNAL bool m0_fol_rec_invariant(const struct m0_fol_rec *drec);

M0_INTERNAL int m0_fols_init(void);
M0_INTERNAL void m0_fols_fini(void);

/** Represents a fragment of FOL record. */
struct m0_fol_frag {
	const struct m0_fol_frag_ops    *rp_ops;
	/**
	    Pointer to the data where FOL record fragment is serialised or
	    will be de-serialised.
	 */
	void                            *rp_data;
	/** Linkage into a fol record fragments. */
	struct m0_tlink                  rp_link;
	/** Magic for fol record fragments list. */
	uint64_t                         rp_magic;
	/**
	 * As rp_data points to the in-memory record fragment during encoding,
	 * rp_data is freed only when rp_flag is equals to M0_XCODE_DECODE.
	 */
	enum m0_xcode_what               rp_flag;
};

struct m0_fol_frag_type {
	uint32_t                           rpt_index;
	const char                        *rpt_name;
	/**
	    Xcode type representing the FOL record fragment.
	    Used to encode, decode or calculate the length of
	    FOL record fragments using xcode operations.
	 */
	const struct m0_xcode_type        *rpt_xt;
	const struct m0_fol_frag_type_ops *rpt_ops;
};

struct m0_fol_frag_type_ops {
	/**  Sets the record fragment operations vector. */
	void (*rpto_rec_frag_init)(struct m0_fol_frag *frag);
};

/**
    FOL record fragments are decoded from FOL record and then undo or
    redo operations are performed on these fragments.
 */
struct m0_fol_frag_ops {
	const struct m0_fol_frag_type *rpo_type;
	int (*rpo_undo)(struct m0_fol_frag *frag, struct m0_be_tx *tx);
	int (*rpo_redo)(struct m0_fol_frag *frag, struct m0_be_tx *tx);
	void (*rpo_undo_credit)(const struct m0_fol_frag *frag,
				struct m0_be_tx_credit *accum);
	void (*rpo_redo_credit)(const struct m0_fol_frag *frag,
				struct m0_be_tx_credit *accum);
};

struct m0_fol_frag_header {
	uint32_t rph_index;
	uint64_t rph_magic;
} M0_XCA_RECORD;

/**
   During encoding of FOL record data points to the in-memory FOL record
   fragment object allocated by the calling function.
   In case if decoding data should be NULL, as it is allocated by xcode.
   @pre frag != NULL
   @pre type != NULL
 */
M0_INTERNAL void
m0_fol_frag_init(struct m0_fol_frag *frag, void *data,
		     const struct m0_fol_frag_type *type);

M0_INTERNAL void m0_fol_frag_fini(struct m0_fol_frag *frag);

/** Register a new fol record fragment type. */
M0_INTERNAL int
m0_fol_frag_type_register(struct m0_fol_frag_type *type);

M0_INTERNAL void
m0_fol_frag_type_deregister(struct m0_fol_frag_type *type);

/** Descriptor for the tlist of fol record fragments. */
M0_TL_DESCR_DECLARE(m0_rec_frag, M0_EXTERN);
M0_TL_DECLARE(m0_rec_frag, M0_INTERNAL, struct m0_fol_frag);

M0_INTERNAL void m0_fol_frag_add(struct m0_fol_rec *rec,
				     struct m0_fol_frag *frag);

#define M0_FOL_FRAG_TYPE_DECLARE(frag, scope, undo, redo,	\
				     undo_cred, redo_cred)	\
scope struct m0_fol_frag_type frag ## _type;			\
static const struct m0_fol_frag_ops frag ## _ops = {		\
	.rpo_type = &frag ## _type,				\
	.rpo_undo = undo,					\
	.rpo_redo = redo,					\
	.rpo_undo_credit = undo_cred,				\
	.rpo_redo_credit = redo_cred,				\
};								\
static void frag ## _ops_init(struct m0_fol_frag *frag)	\
{								\
	frag->rp_ops = &frag ## _ops;				\
}								\
static const struct m0_fol_frag_type_ops frag ## _type_ops = {	\
	.rpto_rec_frag_init = frag ##_ops_init			\
};

#define M0_FOL_FRAG_TYPE_XC_OPS(name, frag_xc, frag_type_ops)	\
(struct m0_fol_frag_type) {					\
	.rpt_name = name,					\
	.rpt_xt   = (frag_xc),					\
	.rpt_ops  = (frag_type_ops)				\
};

#define M0_FOL_FRAG_TYPE_INIT(frag, name)		        \
frag ## _type = M0_FOL_FRAG_TYPE_XC_OPS(name, frag ## _xc,	\
				            &frag ## _type_ops)

/** @} end of fol group */
#endif /* __MERO_FOL_FOL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
