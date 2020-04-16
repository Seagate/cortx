/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 12-Mar-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/errno.h"         /* ENOMEM */
#include "lib/memory.h"
#include "lib/trace.h"
#include "addb2/addb2.h"
#include "fol/fol.h"           /* m0_fol_frag */

#include "stob/io.h"
#include "stob/stob.h"         /* m0_stob_state_get */
#include "stob/domain.h"       /* m0_stob_domain_id_get */
#include "stob/addb2.h"

/**
 * @addtogroup stob
 *
 * @{
 */

static void m0_stob_io_private_fini(struct m0_stob_io *io)
{
	if (io->si_stob_private != NULL) {
		io->si_op->sio_fini(io);
		io->si_stob_private = NULL;
	}
}

M0_INTERNAL int m0_stob_io_private_setup(struct m0_stob_io *io,
					 struct m0_stob *obj)
{
	uint8_t type_id;
	int     result;

	type_id = m0_stob_domain__type_id(
			m0_stob_domain_id_get(m0_stob_dom_get(obj)));
	if (io->si_stob_magic != type_id) {
		m0_stob_io_private_fini(io);
		result = obj->so_ops->sop_io_init(obj, io);
		io->si_stob_magic = type_id;
	} else
		result = 0;
	return result;
}

static void stob_io_addb2_add_and_push(uint64_t           id,
				       struct m0_stob_io *io,
				       struct m0_stob    *obj)
{
	const struct m0_fid *fid = m0_stob_fid_get(obj);
	struct m0_indexvec  *iv  = &io->si_stob;
	struct m0_bufvec    *bv  = &io->si_user;
	m0_bcount_t          bvec_count;

	bvec_count = m0_vec_count(&bv->ov_vec);
	M0_ADDB2_ADD(id, FID_P(fid),
		     bvec_count,
		     bv->ov_vec.v_nr, iv->iv_vec.v_nr, iv->iv_index[0]);
	M0_ADDB2_PUSH(id, FID_P(fid),
		      bvec_count,
		      bv->ov_vec.v_nr, iv->iv_vec.v_nr, iv->iv_index[0]);

	M0_ADDB2_ADD(M0_AVI_ATTR, io->si_id,
		     M0_AVI_STOB_IO_ATTR_UVEC_NR,
		     io->si_user.ov_vec.v_nr);
	M0_ADDB2_ADD(M0_AVI_ATTR, io->si_id,
		     M0_AVI_STOB_IO_ATTR_UVEC_COUNT,
		     bvec_count);
	M0_ADDB2_ADD(M0_AVI_ATTR, io->si_id,
		     M0_AVI_STOB_IO_ATTR_UVEC_BYTES,
		     bvec_count << m0_stob_block_shift(obj));
}

static bool stob_io_invariant(struct m0_stob_io *io, struct m0_stob *obj,
			      enum m0_stob_io_state state)
{
	struct m0_indexvec  *iv  = &io->si_stob;
	struct m0_bufvec    *bv  = &io->si_user;

	return  _0C(m0_stob_state_get(obj) == CSS_EXISTS) &&
		_0C(m0_chan_has_waiters(&io->si_wait)) &&
		_0C(io->si_obj == NULL || ergo(state == SIS_PREPARED,
					    io->si_obj == obj)) &&
		_0C(io->si_state == state) &&
		_0C(io->si_opcode != SIO_INVALID) &&
		_0C(m0_vec_count(&bv->ov_vec) == m0_vec_count(&iv->iv_vec)) &&
		_0C(m0_stob_io_user_is_valid(bv)) &&
		_0C(m0_stob_io_stob_is_valid(iv));
}

M0_INTERNAL void m0_stob_io_init(struct m0_stob_io *io)
{
	M0_SET0(io);
	io->si_id = m0_dummy_id_generate();
	io->si_opcode = SIO_INVALID;
	io->si_state  = SIS_IDLE;
	m0_mutex_init(&io->si_mutex);
	m0_chan_init(&io->si_wait, &io->si_mutex);
	M0_POST(io->si_state == SIS_IDLE);
}

M0_INTERNAL void m0_stob_io_fini(struct m0_stob_io *io)
{
	M0_PRE(io->si_state == SIS_IDLE);
	m0_chan_fini_lock(&io->si_wait);
	m0_mutex_fini(&io->si_mutex);
	m0_stob_io_private_fini(io);
}

M0_INTERNAL void m0_stob_io_credit(const struct m0_stob_io *io,
				   const struct m0_stob_domain *dom,
				   struct m0_be_tx_credit *accum)
{
	M0_PRE(io->si_opcode == SIO_WRITE);
	dom->sd_ops->sdo_stob_write_credit(dom, io, accum);
}

static void stob_io_fill(struct m0_stob_io    *io,
			 struct m0_stob       *obj,
			 struct m0_dtx        *tx,
			 struct m0_io_scope   *scope,
			 enum m0_stob_io_state state,
			 bool                  count_update)
{
	io->si_obj   = obj;
	io->si_tx    = tx;
	io->si_scope = scope;
	io->si_state = state;
	io->si_rc    = 0;
	if (count_update)
		io->si_count = 0;
	io->si_start = m0_time_now();
}

M0_INTERNAL int m0_stob_io_prepare(struct m0_stob_io *io, struct m0_stob *obj,
				   struct m0_dtx *tx, struct m0_io_scope *scope)
{
	const struct m0_fid *fid = m0_stob_fid_get(obj);
	uint8_t              type_id;
	int                  result;

	M0_PRE(stob_io_invariant(io, obj, SIS_IDLE));
	M0_ENTRY("stob=%p so_id="STOB_ID_F" si_opcode=%d io=%p tx=%p",
		 obj, STOB_ID_P(m0_stob_id_get(obj)), io->si_opcode, io, tx);
	//stob_io_addb2_add_and_push(M0_AVI_STOB_IO_PREPARE, io, obj);
	type_id = m0_stob_domain__type_id(
			m0_stob_domain_id_get(m0_stob_dom_get(obj)));
	if (io->si_stob_magic != type_id) {
		m0_stob_io_private_fini(io);
		result = obj->so_ops->sop_io_init(obj, io);
		io->si_stob_magic = type_id;
	} else
		result = 0;

	if (result == 0) {
		stob_io_fill(io, obj, tx, scope, SIS_PREPARED, true);

		result = io->si_op->sio_prepare == NULL ? 0 :
			io->si_op->sio_prepare(io);

		if (result != 0) {
			M0_LOG(M0_ERROR, "io=%p "FID_F" FAILED rc=%d",
					 io, FID_P(fid), result);
			io->si_state = SIS_IDLE;
		}
	}
	//m0_addb2_pop(M0_AVI_STOB_IO_PREPARE);
	M0_POST(ergo(result != 0, io->si_state == SIS_IDLE));
	return result;
}

M0_INTERNAL int m0_stob_io_launch(struct m0_stob_io *io, struct m0_stob *obj,
				  struct m0_dtx *tx, struct m0_io_scope *scope)
{
	const struct m0_fid *fid = m0_stob_fid_get(obj);
	int                  result;

	M0_PRE(stob_io_invariant(io, obj, SIS_PREPARED));
	M0_ENTRY("stob=%p so_id="STOB_ID_F" si_opcode=%d io=%p tx=%p",
		 obj, STOB_ID_P(m0_stob_id_get(obj)), io->si_opcode, io, tx);
	stob_io_addb2_add_and_push(M0_AVI_STOB_IO_LAUNCH, io, obj);
	result = m0_stob_io_private_setup(io, obj);

	if (result == 0) {
		stob_io_fill(io, obj, tx, scope, SIS_BUSY, false);

		result = io->si_op->sio_launch(io);
		if (result != 0) {
			M0_LOG(M0_ERROR, "io=%p "FID_F" FAILED rc=%d",
					 io, FID_P(fid), result);
			io->si_state = SIS_IDLE;
		}
	}
	m0_addb2_pop(M0_AVI_STOB_IO_LAUNCH);
	M0_POST(ergo(result != 0, io->si_state == SIS_IDLE));
	return result;
}

M0_INTERNAL int m0_stob_io_prepare_and_launch(struct m0_stob_io *io,
					      struct m0_stob *obj,
					      struct m0_dtx *tx,
					      struct m0_io_scope *scope)
{
	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_IO_LAUNCH);

	return m0_stob_io_prepare(io, obj, tx, scope) ?:
		m0_stob_io_launch(io, obj, tx, scope);
}

M0_INTERNAL bool m0_stob_io_user_is_valid(const struct m0_bufvec *user)
{
	return true;
}

M0_INTERNAL bool m0_stob_io_stob_is_valid(const struct m0_indexvec *stob)
{
	uint32_t    i;
	m0_bindex_t reached;

	for (reached = 0, i = 0; i < stob->iv_vec.v_nr; ++i) {
		if (stob->iv_index[i] < reached)
			return false;
		reached = stob->iv_index[i] + stob->iv_vec.v_count[i];
	}
	return true;
}

M0_INTERNAL int m0_stob_io_bufvec_launch(struct m0_stob   *stob,
					 struct m0_bufvec *bufvec,
					 int               op_code,
					 m0_bindex_t       offset)
{
	int                 rc;
	struct m0_stob_io   io;
	struct m0_clink     clink;
	m0_bcount_t         count;
	m0_bindex_t         offset_idx = offset;

	M0_PRE(stob != NULL);
	M0_PRE(bufvec != NULL);
	M0_PRE(M0_IN(op_code, (SIO_READ, SIO_WRITE)));

	count = m0_vec_count(&bufvec->ov_vec);
	m0_stob_io_init(&io);

	io.si_opcode = op_code;
	io.si_flags  = 0;
	io.si_user.ov_vec.v_nr = bufvec->ov_vec.v_nr;
	io.si_user.ov_vec.v_count = bufvec->ov_vec.v_count;
	io.si_user.ov_buf = bufvec->ov_buf;
	io.si_stob = (struct m0_indexvec) {
		.iv_vec = { .v_nr = 1, .v_count = &count },
		.iv_index = &offset_idx
	};

	m0_clink_init(&clink, NULL);
	m0_clink_add_lock(&io.si_wait, &clink);

	rc = m0_stob_io_prepare_and_launch(&io, stob, NULL, NULL);
	if (rc == 0) {
		m0_chan_wait(&clink);
		rc = io.si_rc;
	}

	m0_clink_del_lock(&clink);
	m0_clink_fini(&clink);

	m0_stob_io_fini(&io);

	return M0_RC(rc);
}

M0_INTERNAL void *m0_stob_addr_pack(const void *buf, uint32_t shift)
{
	uint64_t addr = (uint64_t)buf;

	M0_ASSERT_INFO(((addr >> shift) << shift) == addr,
	               "addr=%"PRIx64" shift=%"PRIu32, addr, shift);
	return (void *)(addr >> shift);
}

M0_INTERNAL void *m0_stob_addr_open(const void *buf, uint32_t shift)
{
	uint64_t addr = (uint64_t)buf;

	M0_ASSERT_INFO(((addr << shift) >> shift) == addr,
	               "addr=%"PRIx64" shift=%"PRIu32, addr, shift);
	return (void *)(addr << shift);
}

M0_INTERNAL void m0_stob_iovec_sort(struct m0_stob_io *stob)
{
	struct m0_indexvec *ivec = &stob->si_stob;
	struct m0_bufvec   *bvec = &stob->si_user;
	int                 i;
	bool                exchanged;
	bool                different_count;

#define SWAP_NEXT(arr, idx)			\
({						\
	int               _idx = (idx);		\
	typeof(&arr[idx]) _arr = (arr);		\
	typeof(arr[idx])  _tmp;			\
						\
	_tmp           = _arr[_idx];		\
	_arr[_idx]     = _arr[_idx + 1];	\
	_arr[_idx + 1] = _tmp;			\
})

	different_count = ivec->iv_vec.v_count != bvec->ov_vec.v_count;

	/*
	 * Bubble sort the index vectores.
	 * It also move bufvecs while sorting.
	 */
	do {
		exchanged = false;
		for (i = 0; i < ivec->iv_vec.v_nr - 1; i++) {
			if (ivec->iv_index[i] > ivec->iv_index[i + 1]) {

				SWAP_NEXT(ivec->iv_index, i);
				SWAP_NEXT(ivec->iv_vec.v_count, i);
				SWAP_NEXT(bvec->ov_buf, i);
				if (different_count)
					SWAP_NEXT(bvec->ov_vec.v_count, i);
				exchanged = true;
			}
		}
	} while (exchanged);

#undef SWAP_NEXT
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of stob group */

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
