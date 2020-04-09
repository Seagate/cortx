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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 *                  Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 4-Jul-2013
 */


#pragma once

#ifndef __MERO_BE_IO_H__
#define __MERO_BE_IO_H__

#include "lib/chan.h"           /* m0_clink */
#include "lib/types.h"          /* m0_bcount_t */
#include "lib/atomic.h"         /* m0_atomic64 */
#include "lib/ext.h"            /* m0_ext */
#include "lib/tlist.h"          /* m0_tlink */
#include "lib/ext.h"            /* m0_ext */

#include "be/op.h"              /* m0_be_op */

#include "stob/io.h"            /* m0_stob_io */
#include "sm/sm.h"              /* m0_sm_ast */

/**
 * @defgroup be Meta-data back-end
 *
 * * Overview
 * m0_be_io is an abstraction on top of m0_stob_io. It makes all kinds
 * of stob I/O inside BE easier.
 *
 * * Tests
 * - test that I/O callback even for empty I/O is called from somewhere else;
 * @{
 */

struct m0_stob;
struct m0_be_io;
struct m0_be_io_sched;

struct m0_be_io_part {
	struct m0_stob_io bip_sio;
	/** clink signalled when @bio_sio is completed */
	struct m0_clink   bip_clink;
	struct m0_stob   *bip_stob;
	uint32_t          bip_bshift;
	struct m0_be_io  *bip_bio;
	int               bip_rc;
};

/**
 * @todo add bshift
 */
struct m0_be_io_credit {
	uint64_t    bic_reg_nr;
	m0_bcount_t bic_reg_size;
	uint64_t    bic_part_nr;
};

#define M0_BE_IO_CREDIT(reg_nr, reg_size, part_nr) (struct m0_be_io_credit){ \
	.bic_reg_nr = (reg_nr),                                              \
	.bic_reg_size = (reg_size),                                          \
	.bic_part_nr = (part_nr),                                            \
}

#define BE_IOCRED_F "(reg_nr=%"PRIu64" reg_size=%"PRIu64" part_nr=%"PRIu64")"
#define BE_IOCRED_P(iocred) \
	     (iocred)->bic_reg_nr, (iocred)->bic_reg_size, (iocred)->bic_part_nr

struct m0_be_io {
	/** Array of single stob I/Os. */
	struct m0_be_io_part   *bio_part;
	struct m0_bufvec        bio_bv_user;
	struct m0_indexvec      bio_iv_stob;
	/** Current index in m0_be_io::bio_bv_user and m0_be_io::bio_iv_stob */
	uint32_t                bio_vec_pos;
	struct m0_be_io_credit  bio_iocred;
	struct m0_be_io_credit  bio_used;
	/** Number of different stobs in current I/O */
	unsigned                bio_stob_nr;
	/**
	 * Number of finished I/Os.
	 *
	 * Failed I/O counts as finished.
	 */
	struct m0_atomic64      bio_stob_io_finished_nr;
	/**
	 * Operation passed by the user on which m0_be_io has to signal when io
	 * is completed.
	 */
	struct m0_be_op        *bio_op;
	/** @see m0_be_io_sync_enable */
	bool                    bio_sync;
	enum m0_stob_io_opcode  bio_opcode;
	struct m0_sm_ast        bio_ast;

	void                   *bio_user_data;

	/* m0_be_io_sched fields */
	struct m0_be_io_sched  *bio_sched;
	struct m0_tlink         bio_sched_link;
	uint64_t                bio_sched_magic;
	struct m0_be_op         bio_sched_op;
	struct m0_ext           bio_ext;
};

M0_INTERNAL int m0_be_io_init(struct m0_be_io *bio);
M0_INTERNAL void m0_be_io_fini(struct m0_be_io *bio);
M0_INTERNAL bool m0_be_io__invariant(struct m0_be_io *bio);

M0_INTERNAL int m0_be_io_allocate(struct m0_be_io        *bio,
				  struct m0_be_io_credit *iocred);
M0_INTERNAL void m0_be_io_deallocate(struct m0_be_io *bio);


M0_INTERNAL void m0_be_io_add(struct m0_be_io *bio,
			      struct m0_stob  *stob,
			      void            *ptr_user,
			      m0_bindex_t      offset_stob,
			      m0_bcount_t      size);

M0_INTERNAL void m0_be_io_add_nostob(struct m0_be_io *bio,
				     void            *ptr_user,
				     m0_bindex_t      offset_stob,
				     m0_bcount_t      size);
M0_INTERNAL void m0_be_io_stob_assign(struct m0_be_io *bio,
				      struct m0_stob  *stob,
				      m0_bindex_t      offset,
				      m0_bcount_t      size);
/**
 * Moves I/O vectors of the stob to the offset. Wraps them within window.
 * Works only for sequential stob I/O and unpacked I/O vectors.
 */
M0_INTERNAL void m0_be_io_stob_move(struct m0_be_io *bio,
				    struct m0_stob  *stob,
				    m0_bindex_t      offset,
				    m0_bindex_t      win_start,
				    m0_bcount_t      win_size);
/**
 * Packs I/O vectors.
 *
 * The bio must be filled through m0_be_io_add_nostob() and all parts must be
 * assigned with stobs.
 */
M0_INTERNAL void m0_be_io_vec_pack(struct m0_be_io *bio);
M0_INTERNAL m0_bcount_t m0_be_io_size(struct m0_be_io *bio);

/** call fdatasync() for linux stob after IO completion */
M0_INTERNAL void m0_be_io_sync_enable(struct m0_be_io *bio);
M0_INTERNAL bool m0_be_io_sync_is_enabled(struct m0_be_io *bio);

M0_INTERNAL enum m0_stob_io_opcode m0_be_io_opcode(struct m0_be_io *io);

M0_INTERNAL void m0_be_io_configure(struct m0_be_io        *bio,
				    enum m0_stob_io_opcode  opcode);

M0_INTERNAL void m0_be_io_launch(struct m0_be_io *bio, struct m0_be_op *op);

M0_INTERNAL bool m0_be_io_is_empty(struct m0_be_io *bio);
M0_INTERNAL void m0_be_io_reset(struct m0_be_io *bio);
M0_INTERNAL void m0_be_io_sort(struct m0_be_io *bio);

M0_INTERNAL void m0_be_io_user_data_set(struct m0_be_io *bio, void *data);
M0_INTERNAL void *m0_be_io_user_data(struct m0_be_io *bio);

M0_INTERNAL int m0_be_io_single(struct m0_stob         *stob,
				enum m0_stob_io_opcode  opcode,
				void                   *ptr_user,
				m0_bindex_t             offset_stob,
				m0_bcount_t             size);

M0_INTERNAL bool m0_be_io_intersect(const struct m0_be_io *bio1,
				    const struct m0_be_io *bio2);

M0_INTERNAL bool m0_be_io_ptr_user_is_eq(const struct m0_be_io *bio1,
					 const struct m0_be_io *bio2);
M0_INTERNAL bool m0_be_io_offset_stob_is_eq(const struct m0_be_io *bio1,
					    const struct m0_be_io *bio2);


/** iocred0 += iocred1 */
M0_INTERNAL void m0_be_io_credit_add(struct m0_be_io_credit       *iocred0,
				     const struct m0_be_io_credit *iocred1);

/** iocred0 <= iocred1 */
M0_INTERNAL bool m0_be_io_credit_le(const struct m0_be_io_credit *iocred0,
				    const struct m0_be_io_credit *iocred1);

/** @} end of be group */

#endif /* __MERO_BE_IO_H__ */


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
