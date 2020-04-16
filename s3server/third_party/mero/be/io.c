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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/io.h"

#include <unistd.h>              /* fdatasync */

#include "lib/memory.h"          /* m0_alloc */
#include "lib/errno.h"           /* ENOMEM */
#include "lib/ext.h"             /* m0_ext_are_overlapping */
#include "lib/locality.h"        /* m0_locality0_get */

#include "stob/io.h"             /* m0_stob_iovec_sort */
#include "stob/stob.h"           /* m0_stob_fd */

#include "be/op.h"               /* m0_be_op_active */
#include "be/ha.h"               /* m0_be_io_err_send */

/**
 * @addtogroup be
 *
 * @{
 */

static bool be_io_cb(struct m0_clink *link);

static m0_bindex_t be_io_stob_offset_pack(m0_bindex_t offset, uint32_t bshift)
{
	return (m0_bindex_t)m0_stob_addr_pack((void *)offset, bshift);
}

static m0_bcount_t be_io_size_pack(m0_bcount_t size, uint32_t bshift)
{
	return (m0_bcount_t)m0_stob_addr_pack((void *)size, bshift);
}

static m0_bcount_t be_io_size_unpack(m0_bcount_t size, uint32_t bshift)
{
	return (m0_bcount_t)m0_stob_addr_open((void *)size, bshift);
}

static void be_io_free(struct m0_be_io *bio)
{
	m0_free(bio->bio_bv_user.ov_vec.v_count);
	m0_free(bio->bio_bv_user.ov_buf);
	m0_free(bio->bio_iv_stob.iv_vec.v_count);
	m0_free(bio->bio_iv_stob.iv_index);
	m0_free(bio->bio_part);
}

static void be_io_part_init(struct m0_be_io_part *bip,
			    struct m0_be_io      *bio)
{
	bip->bip_bio = bio;
	m0_stob_io_init(&bip->bip_sio);
	m0_clink_init(&bip->bip_clink, be_io_cb);
}

static void be_io_part_fini(struct m0_be_io_part *bip)
{
	m0_clink_fini(&bip->bip_clink);
	m0_stob_io_fini(&bip->bip_sio);
}

static bool be_io_part_invariant(struct m0_be_io_part *bip)
{
	struct m0_stob_io *sio = &bip->bip_sio;

	return _0C(bip->bip_bio != NULL) &&
	       _0C(sio->si_user.ov_vec.v_nr == sio->si_stob.iv_vec.v_nr) &&
	       _0C(m0_vec_count(&sio->si_user.ov_vec) ==
		   m0_vec_count(&sio->si_stob.iv_vec));
}

static int be_io_part_launch(struct m0_be_io_part *bip)
{
	struct m0_stob_io *sio = &bip->bip_sio;
	int                rc;

	m0_clink_add_lock(&sio->si_wait, &bip->bip_clink);

	M0_ENTRY("sio=%p stob=%p stob_fid="FID_F" "
		 "si_user=(count=%"PRIu32" size=%llu) "
		 "si_stob=(count=%"PRIu32" size=%llu)",
		 sio, bip->bip_stob, FID_P(m0_stob_fid_get(bip->bip_stob)),
		 sio->si_user.ov_vec.v_nr,
		 (1ULL << bip->bip_bshift) *
		 (unsigned long long)m0_vec_count(&sio->si_user.ov_vec),
		 sio->si_stob.iv_vec.v_nr,
		 (1ULL << bip->bip_bshift) *
		 (unsigned long long)m0_vec_count(&sio->si_stob.iv_vec));

	rc = m0_stob_io_prepare_and_launch(sio, bip->bip_stob, NULL, NULL);
	if (rc != 0)
		m0_clink_del_lock(&bip->bip_clink);
	return M0_RC(rc);
}

static void be_io_part_reset(struct m0_be_io_part *bip)
{
	struct m0_stob_io *sio = &bip->bip_sio;

	sio->si_user.ov_vec.v_nr = 0;
	sio->si_stob.iv_vec.v_nr = 0;
	sio->si_obj = NULL;

	bip->bip_stob   = NULL;
	bip->bip_bshift = 0;
	bip->bip_rc     = 0;
}

static bool be_io_part_add(struct m0_be_io_part *bip,
			   void                 *ptr_user,
			   m0_bindex_t           offset_stob,
			   m0_bcount_t           size)
{
	struct m0_stob_io *sio    = &bip->bip_sio;
	uint32_t           bshift = bip->bip_bshift;
	void             **u_buf  = sio->si_user.ov_buf;
	struct m0_vec     *u_vec  = &sio->si_user.ov_vec;
	m0_bindex_t       *s_offs = sio->si_stob.iv_index;
	struct m0_vec     *s_vec  = &sio->si_stob.iv_vec;
	uint32_t           nr     = u_vec->v_nr;
	void              *ptr_user_packed;
	m0_bindex_t        offset_stob_packed;
	m0_bcount_t        size_packed;
	bool               added;

	M0_PRE(u_vec->v_nr == s_vec->v_nr);

	ptr_user_packed    = m0_stob_addr_pack(ptr_user, bshift);
	offset_stob_packed = be_io_stob_offset_pack(offset_stob, bshift);
	size_packed        = be_io_size_pack(size, bshift);

	if (nr > 0 &&
	    u_buf [nr - 1] + u_vec->v_count[nr - 1] == ptr_user_packed &&
	    s_offs[nr - 1] + s_vec->v_count[nr - 1] == offset_stob_packed) {
		/* optimization for sequential regions */
		u_vec->v_count[nr - 1] += size_packed;
		s_vec->v_count[nr - 1] += size_packed;
		added = false;
	} else {
		u_buf[nr]          = ptr_user_packed;
		u_vec->v_count[nr] = size_packed;
		s_offs[nr]         = offset_stob_packed;
		s_vec->v_count[nr] = size_packed;

		++u_vec->v_nr;
		++s_vec->v_nr;
		added = true;
	}
	return added;
}

M0_INTERNAL int m0_be_io_init(struct m0_be_io *bio)
{
	return 0;
}

M0_INTERNAL int m0_be_io_allocate(struct m0_be_io        *bio,
				  struct m0_be_io_credit *iocred)
{
	struct m0_bufvec   *bv = &bio->bio_bv_user;
	struct m0_indexvec *iv = &bio->bio_iv_stob;
	int                 rc;
	size_t              i;

	M0_ENTRY("bio=%p iocred="BE_IOCRED_F, bio, BE_IOCRED_P(iocred));

	bio->bio_iocred = *iocred;

	M0_ALLOC_ARR(bv->ov_vec.v_count, bio->bio_iocred.bic_reg_nr);
	M0_ALLOC_ARR(bv->ov_buf,         bio->bio_iocred.bic_reg_nr);
	M0_ALLOC_ARR(iv->iv_vec.v_count, bio->bio_iocred.bic_reg_nr);
	M0_ALLOC_ARR(iv->iv_index,       bio->bio_iocred.bic_reg_nr);
	M0_ALLOC_ARR(bio->bio_part,      bio->bio_iocred.bic_part_nr);

	if (bv->ov_vec.v_count == NULL ||
	    bv->ov_buf         == NULL ||
	    iv->iv_vec.v_count == NULL ||
	    iv->iv_index       == NULL ||
	    bio->bio_part      == NULL) {
		be_io_free(bio);
		rc = -ENOMEM;
	} else {
		for (i = 0; i < bio->bio_iocred.bic_part_nr; ++i)
			be_io_part_init(&bio->bio_part[i], bio);
		m0_be_io_reset(bio);
		rc = 0;
	}
	M0_POST(ergo(rc == 0, m0_be_io__invariant(bio)));
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_io_deallocate(struct m0_be_io *bio)
{
	unsigned i;

	for (i = 0; i < bio->bio_iocred.bic_part_nr; ++i)
		be_io_part_fini(&bio->bio_part[i]);
	be_io_free(bio);
}

M0_INTERNAL void m0_be_io_fini(struct m0_be_io *bio)
{
}

M0_INTERNAL bool m0_be_io__invariant(struct m0_be_io *bio)
{
	struct m0_be_io_part *bip;
	struct m0_stob_io    *sio;
	struct m0_bufvec     *bv = &bio->bio_bv_user;
	struct m0_indexvec   *iv = &bio->bio_iv_stob;
	m0_bcount_t           count_total = 0;
	uint32_t              nr_total    = 0;
	unsigned              i;
	uint32_t              pos = 0;

	for (i = 0; i < bio->bio_stob_nr; ++i) {
		bip          = &bio->bio_part[i];
		sio          = &bip->bip_sio;
		nr_total    += sio->si_user.ov_vec.v_nr;
		count_total += m0_vec_count(&sio->si_user.ov_vec);
		if (be_io_part_invariant(bip) &&
		    _0C(sio->si_user.ov_vec.v_count ==
			&bv->ov_vec.v_count[pos]) &&
		    _0C(sio->si_user.ov_buf == &bv->ov_buf[pos]) &&
		    _0C(sio->si_stob.iv_vec.v_count ==
			&iv->iv_vec.v_count[pos]) &&
		    _0C(sio->si_stob.iv_index == &iv->iv_index[pos])) {

			pos = nr_total;
			continue;
		}
		return false;
	}

	return _0C(bio != NULL) &&
	       _0C(m0_be_io_credit_le(&bio->bio_used, &bio->bio_iocred)) &&
	       _0C(m0_atomic64_get(&bio->bio_stob_io_finished_nr) <=
		   bio->bio_stob_nr) &&
	       _0C(nr_total    <= bio->bio_used.bic_reg_nr) &&
	       _0C(count_total <= bio->bio_used.bic_reg_size);
}

static void be_io_vec_cut(struct m0_be_io      *bio,
			  struct m0_be_io_part *bip)
{
	struct m0_stob_io  *sio = &bip->bip_sio;
	struct m0_bufvec   *bv  = &bio->bio_bv_user;
	struct m0_indexvec *iv  = &bio->bio_iv_stob;
	uint32_t            pos = bio->bio_vec_pos;

	sio->si_user.ov_vec.v_count = &bv->ov_vec.v_count[pos];
	sio->si_user.ov_buf         = &bv->ov_buf[pos];
	sio->si_stob.iv_vec.v_count = &iv->iv_vec.v_count[pos];
	sio->si_stob.iv_index       = &iv->iv_index[pos];
}

M0_INTERNAL void m0_be_io_add(struct m0_be_io *bio,
			      struct m0_stob  *stob,
			      void            *ptr_user,
			      m0_bindex_t      offset_stob,
			      m0_bcount_t      size)
{
	struct m0_be_io_part *bip;
	bool                  added;

	M0_PRE_EX(m0_be_io__invariant(bio));
	M0_PRE(bio->bio_used.bic_reg_size + size <=
	       bio->bio_iocred.bic_reg_size);
	M0_PRE(bio->bio_used.bic_reg_nr + 1 <= bio->bio_iocred.bic_reg_nr);

	if (bio->bio_stob_nr == 0 ||
	    bio->bio_part[bio->bio_stob_nr - 1].bip_stob != stob) {
		M0_ASSERT(bio->bio_stob_nr < bio->bio_iocred.bic_part_nr);
		++bio->bio_stob_nr;
		bip = &bio->bio_part[bio->bio_stob_nr - 1];
		bip->bip_stob   = stob;
		bip->bip_bshift = stob == NULL ? 0 : m0_stob_block_shift(stob);
		be_io_vec_cut(bio, bip);
		m0_be_io_credit_add(&bio->bio_used, &M0_BE_IO_CREDIT(0, 0, 1));
	}
	bip = &bio->bio_part[bio->bio_stob_nr - 1];
	added = be_io_part_add(bip, ptr_user, offset_stob, size);
	/* m0_be_io::bio_used is calculated for the worst-case */
	m0_be_io_credit_add(&bio->bio_used, &M0_BE_IO_CREDIT(1, size, 0));
	bio->bio_vec_pos += added;

	M0_POST_EX(m0_be_io__invariant(bio));
}

M0_INTERNAL void m0_be_io_add_nostob(struct m0_be_io *bio,
				     void            *ptr_user,
				     m0_bindex_t      offset_stob,
				     m0_bcount_t      size)
{
	m0_be_io_add(bio, NULL, ptr_user, offset_stob, size);
}

/*
 * Inserts new element into m0_be_io's vectors at index+1 position.
 * Space must be preallocated. Caller must fill new element with proper values.
 */
static void be_io_vec_fork(struct m0_be_io *bio, unsigned index)
{
	void                **ov       = bio->bio_bv_user.ov_buf;
	m0_bindex_t          *iv       = bio->bio_iv_stob.iv_index;
	m0_bcount_t          *ov_count = bio->bio_bv_user.ov_vec.v_count;
	m0_bcount_t          *iv_count = bio->bio_iv_stob.iv_vec.v_count;
	m0_bindex_t          *index_ptr;
	struct m0_be_io_part *bip;
	uint32_t              nr;
	unsigned              i;

	M0_PRE(index < bio->bio_vec_pos);

	m0_be_io_credit_add(&bio->bio_used, &M0_BE_IO_CREDIT(1, 0, 0));
	M0_ASSERT(m0_be_io_credit_le(&bio->bio_used, &bio->bio_iocred));
	index_ptr = &iv[index];

	/* Shift elements to the right by 1 position to make a window for new
	 * element.
	 */
	for (i = bio->bio_vec_pos; i > index; --i) {
		ov[i] = ov[i - 1];
		iv[i] = iv[i - 1];
		ov_count[i] = ov_count[i - 1];
		iv_count[i] = iv_count[i - 1];
	}

	/* Individual m0_be_io_part uses parts of m0_be_io's vectors. So shift
	 * pointers inside m0_be_io_part if respective parts are shifted
	 * previously.
	 * Also, we need to increase number of elements in the m0_be_io_part
	 * where new element is added.
	 */
	for (i = 0; i < bio->bio_stob_nr; ++i) {
		bip = &bio->bio_part[i];
		nr  = bip->bip_sio.si_stob.iv_vec.v_nr;
		if (nr > 0 && index_ptr >= bip->bip_sio.si_stob.iv_index &&
		    index_ptr <= &bip->bip_sio.si_stob.iv_index[nr - 1]) {
			++bip->bip_sio.si_user.ov_vec.v_nr;
			++bip->bip_sio.si_stob.iv_vec.v_nr;
		}
		if (bip->bip_sio.si_stob.iv_index > index_ptr) {
			++bip->bip_sio.si_user.ov_buf;
			++bip->bip_sio.si_user.ov_vec.v_count;
			++bip->bip_sio.si_stob.iv_index;
			++bip->bip_sio.si_stob.iv_vec.v_count;
		}
	}
	++bio->bio_vec_pos;
}

static unsigned be_io_vec_index_by_ptr(struct m0_be_io *bio,
				       m0_bindex_t     *iv_ptr)
{
	m0_bindex_t *iv = bio->bio_iv_stob.iv_index;
	unsigned     i;

	for (i = 0; i < bio->bio_vec_pos; ++i)
		if (&iv[i] == iv_ptr)
			break;
	M0_ASSERT(i < bio->bio_vec_pos);
	return i;
}

M0_INTERNAL void m0_be_io_stob_assign(struct m0_be_io *bio,
				      struct m0_stob  *stob,
				      m0_bindex_t      offset,
				      m0_bcount_t      size)
{
	struct m0_be_io_part *bip;
	m0_bcount_t           total = 0;
	int                   i;

	/*
	 * Current implementation supports only assignment of 1 stob for
	 * the whole m0_be_io. Must be fixed when multiple stobs support
	 * is added to BE log.
	 */
	M0_PRE(_0C(bio->bio_stob_nr == 1) &&
	       _0C(bio->bio_part[0].bip_stob == NULL) &&
	       _0C(bio->bio_part[0].bip_sio.si_stob.iv_vec.v_nr > 0) &&
	       _0C(bio->bio_part[0].bip_sio.si_stob.iv_index[0] == offset));
	for (i = 0; i < bio->bio_part[0].bip_sio.si_stob.iv_vec.v_nr; ++i)
		total += bio->bio_part[0].bip_sio.si_stob.iv_vec.v_count[i];
	M0_PRE(total == size);

	bip = &bio->bio_part[0];
	bip->bip_stob   = stob;
	bip->bip_bshift = m0_stob_block_shift(stob);
}

M0_INTERNAL void m0_be_io_stob_move(struct m0_be_io *bio,
				    struct m0_stob  *stob,
				    m0_bindex_t      offset,
				    m0_bindex_t      win_start,
				    m0_bcount_t      win_size)
{
	struct m0_be_io_part *bip;
	m0_bindex_t          *iv;
	void                **ov;
	m0_bcount_t          *iv_count;
	m0_bcount_t          *ov_count;
	m0_bindex_t           win_end = win_start + win_size;
	m0_bindex_t           end;
	uint32_t              nr;
	unsigned              i;

	M0_PRE(win_start < win_end && offset >= win_start &&
	       offset < win_end);

	for (i = 0; i < bio->bio_stob_nr; ++i)
		if (bio->bio_part[i].bip_stob == stob)
			break;
	M0_ASSERT(i != bio->bio_stob_nr);
	bip = &bio->bio_part[i];
	nr  = bip->bip_sio.si_stob.iv_vec.v_nr;
	iv  = bip->bip_sio.si_stob.iv_index;
	ov  = bip->bip_sio.si_user.ov_buf;
	iv_count = bip->bip_sio.si_stob.iv_vec.v_count;
	ov_count = bip->bip_sio.si_user.ov_vec.v_count;

	M0_ASSERT(ergo(nr > 0, iv[0] == 0));

	i = 0;
	while (i < nr) {
		iv[i] = offset;
		end   = iv[i] + iv_count[i];
		if (end > win_end) {
			be_io_vec_fork(bio,
				       be_io_vec_index_by_ptr(bio, &iv[i]));
			iv_count[i] -= end - win_end;
			ov_count[i]  = iv_count[i];
			iv_count[i + 1] = end - win_end;
			ov_count[i + 1] = iv_count[i + 1];
			iv[i + 1] = win_start;
			ov[i + 1] = (char*)ov[i] + ov_count[i];
			++i;
			++nr;
		}
		offset = end >= win_end ? end - win_end + win_start : end;
		++i;
	}
	M0_POST(m0_be_io__invariant(bio));
}

M0_INTERNAL void m0_be_io_vec_pack(struct m0_be_io *bio)
{
	struct m0_be_io_part *bip;
	m0_bcount_t          *iv_count;
	m0_bcount_t          *ov_count;
	m0_bindex_t          *iv;
	void                **ov;
	uint32_t              nr;
	uint32_t              bshift;
	int                   part;
	int                   i;

	for (part = 0; part < bio->bio_stob_nr; ++part) {
		bip = &bio->bio_part[part];
		M0_ASSERT(bip->bip_stob != NULL);
		nr  = bip->bip_sio.si_stob.iv_vec.v_nr;
		iv  = bip->bip_sio.si_stob.iv_index;
		ov  = bip->bip_sio.si_user.ov_buf;
		iv_count = bip->bip_sio.si_stob.iv_vec.v_count;
		ov_count = bip->bip_sio.si_user.ov_vec.v_count;
		bshift   = bip->bip_bshift;
		for (i = 0; i < nr; ++i) {
			ov[i] = m0_stob_addr_pack(ov[i], bshift);
			iv[i] = be_io_stob_offset_pack(iv[i], bshift);
			iv_count[i] = be_io_size_pack(iv_count[i], bshift);
			ov_count[i] = be_io_size_pack(ov_count[i], bshift);
		}
	}
}

M0_INTERNAL m0_bcount_t m0_be_io_size(struct m0_be_io *bio)
{
	struct m0_be_io_part *bip;
	m0_bcount_t           size;
	m0_bcount_t           size_total = 0;
	int                   i;

	for (i = 0; i < bio->bio_stob_nr; ++i) {
		bip = &bio->bio_part[i];
		size = m0_vec_count(&bip->bip_sio.si_stob.iv_vec);
		size = be_io_size_unpack(size, bip->bip_bshift);
		size_total += size;
	}
	return size_total;
}

M0_INTERNAL void m0_be_io_configure(struct m0_be_io        *bio,
				    enum m0_stob_io_opcode  opcode)
{
	struct m0_stob_io *sio;
	unsigned           i;

	bio->bio_opcode = opcode;
	for (i = 0; i < bio->bio_stob_nr; ++i) {
		sio = &bio->bio_part[i].bip_sio;
		sio->si_opcode   = opcode;
	}
}

static void be_io_finished(struct m0_be_io *bio)
{
	struct m0_be_op      *op = bio->bio_op;
	uint64_t              finished_nr;
	unsigned              i;
	int                   rc;

	finished_nr = bio->bio_stob_nr == 0 ? 0 :
		      m0_atomic64_add_return(&bio->bio_stob_io_finished_nr, 1);
	/*
	 * Next `if' body will be executed only in the last finished stob I/O
	 * callback.
	 */
	if (finished_nr == bio->bio_stob_nr) {
		rc = 0;
		for (i = 0; i < bio->bio_stob_nr; ++i) {
			rc = bio->bio_part[i].bip_rc ?: rc;
			if (rc != 0) {
				M0_LOG(M0_INFO,
				       "failed I/O part number: %u, rc = %d",
				       i, rc);
				break;
			}
		}
		M0_ASSERT_INFO(rc == 0, "m0_be_op can't fail, rc = %d", rc);
		m0_be_op_rc_set(op, rc);
		m0_be_op_done(op);
	}
}

static bool be_io_cb(struct m0_clink *link)
{
	struct m0_be_io_part *bip = container_of(link,
						 struct m0_be_io_part,
						 bip_clink);
	struct m0_be_io      *bio = bip->bip_bio;
	struct m0_stob_io    *sio = &bip->bip_sio;
	int                   fd;
	int                   rc;

	m0_clink_del(&bip->bip_clink);
	/* XXX Temporary hack. I/O error should be handled gracefully. */
	M0_ASSERT_INFO(sio->si_rc == 0, "stob I/O operation failed: "
		       "bio = %p, sio = %p, sio->si_rc = %d",
		       bio, sio, sio->si_rc);
	rc = sio->si_rc;

	/* XXX temporary hack:
	 * - sync() should be implemented on stob level or at least
	 * - sync() shoudn't be called from linux stob worker thread as it is
	 *   now.
	 */
	if (rc == 0 && bio->bio_sync) {
		fd = m0_stob_fd(bip->bip_sio.si_obj);
		rc = fdatasync(fd);
		rc = rc == 0 ? 0 : M0_ERR_INFO(-errno, "fd=%d", fd);
		M0_ASSERT_INFO(rc == 0, "fdatasync() failed: rc=%d", rc);
	}
	bip->bip_rc = rc;
	be_io_finished(bio);
	return rc == 0;
}

static void be_io_empty_ast(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct m0_be_io *bio = ast->sa_datum;

	M0_PRE(m0_be_io_is_empty(bio));
	be_io_finished(bio);
}

M0_INTERNAL void m0_be_io_launch(struct m0_be_io *bio, struct m0_be_op *op)
{
	unsigned i;
	int      rc;

	M0_ENTRY("bio=%p op=%p sync=%d opcode=%d "
	         "m0_be_io_size(bio)=%"PRIu64,
	         bio, op, !!bio->bio_sync, bio->bio_opcode, m0_be_io_size(bio));

	M0_PRE(m0_be_io__invariant(bio));

	bio->bio_op = op;
	m0_be_op_active(op);

	if (m0_be_io_is_empty(bio)) {
		bio->bio_ast.sa_cb    = &be_io_empty_ast;
		bio->bio_ast.sa_datum = bio;
		m0_sm_ast_post(m0_locality0_get()->lo_grp, &bio->bio_ast);
		M0_LEAVE();
		return;
	}

	rc = 0;
	for (i = 0; i < bio->bio_stob_nr; ++i) {
		if (rc == 0)
			rc = be_io_part_launch(&bio->bio_part[i]);
		if (rc != 0)
			be_io_finished(bio);
	}
	M0_LEAVE("rc=%d", rc);
}

M0_INTERNAL void m0_be_io_sync_enable(struct m0_be_io *bio)
{
	bio->bio_sync = true;
}

M0_INTERNAL bool m0_be_io_sync_is_enabled(struct m0_be_io *bio)
{
	return bio->bio_sync;
}

M0_INTERNAL enum m0_stob_io_opcode m0_be_io_opcode(struct m0_be_io *io)
{
	return io->bio_opcode;
}

M0_INTERNAL bool m0_be_io_is_empty(struct m0_be_io *bio)
{
	return bio->bio_stob_nr == 0;
}

M0_INTERNAL void m0_be_io_reset(struct m0_be_io *bio)
{
	unsigned i;

	for (i = 0; i < bio->bio_stob_nr; ++i)
		be_io_part_reset(&bio->bio_part[i]);
	m0_atomic64_set(&bio->bio_stob_io_finished_nr, 0);
	bio->bio_vec_pos = 0;
	bio->bio_used    = M0_BE_IO_CREDIT(0, 0, 0);
	bio->bio_stob_nr = 0;
	bio->bio_sync    = false;
}

M0_INTERNAL void m0_be_io_sort(struct m0_be_io *bio)
{
	unsigned i;

	for (i = 0; i < bio->bio_stob_nr; ++i)
		m0_stob_iovec_sort(&bio->bio_part[i].bip_sio);
}

M0_INTERNAL void m0_be_io_user_data_set(struct m0_be_io *bio, void *data)
{
	bio->bio_user_data = data;
}

M0_INTERNAL void *m0_be_io_user_data(struct m0_be_io *bio)
{
	return bio->bio_user_data;
}

M0_INTERNAL int m0_be_io_single(struct m0_stob         *stob,
				enum m0_stob_io_opcode  opcode,
				void                   *ptr_user,
				m0_bindex_t             offset_stob,
				m0_bcount_t             size)
{
	struct m0_be_io bio = {};
	int             rc;

	rc = m0_be_io_init(&bio);
	if (rc != 0)
		goto out;
	rc = m0_be_io_allocate(&bio, &M0_BE_IO_CREDIT(1, size, 1));
	if (rc == 0) {
		m0_be_io_add(&bio, stob, ptr_user, offset_stob, size);
		m0_be_io_configure(&bio, opcode);
		rc = M0_BE_OP_SYNC_RC(op, m0_be_io_launch(&bio, &op));
		m0_be_io_deallocate(&bio);
	}
	m0_be_io_fini(&bio);
out:
	if (rc != 0)
		m0_be_io_err_send(-rc, M0_BE_LOC_NONE, opcode);
	return M0_RC(rc);
}

static bool be_io_part_intersect(const struct m0_be_io_part *bip1,
				 const struct m0_be_io_part *bip2)
{
	const struct m0_indexvec *iv1 = &bip1->bip_sio.si_stob;
	const struct m0_indexvec *iv2 = &bip2->bip_sio.si_stob;
	struct m0_ext             e1;
	struct m0_ext             e2;
	uint32_t                  i;
	uint32_t                  j;

#define EXT(start, end) (struct m0_ext){ .e_start = (start), .e_end = (end) }
	for (i = 0; i < iv1->iv_vec.v_nr; ++i)
		for (j = 0; j < iv2->iv_vec.v_nr; ++j) {
			e1 = EXT(iv1->iv_index[i],
				 iv1->iv_index[i] + iv1->iv_vec.v_count[i]);
			e2 = EXT(iv2->iv_index[j],
				 iv2->iv_index[j] + iv2->iv_vec.v_count[j]);
			if (m0_ext_are_overlapping(&e1, &e2))
				return true;
		}
#undef EXT
	return false;
}

M0_INTERNAL bool m0_be_io_intersect(const struct m0_be_io *bio1,
				    const struct m0_be_io *bio2)
{
	int i;
	int j;

	for (i = 0; i < bio1->bio_stob_nr; ++i)
		for (j = 0; j < bio2->bio_stob_nr; ++j) {
			if (be_io_part_intersect(&bio1->bio_part[i],
						 &bio2->bio_part[j]))
				return true;
		}
	return false;
}

/** @note only one part and only one buffer in bufvec is supported atm. */
M0_INTERNAL bool m0_be_io_ptr_user_is_eq(const struct m0_be_io *bio1,
					 const struct m0_be_io *bio2)
{
	struct m0_stob_io *sio1 = &bio1->bio_part[0].bip_sio;
	struct m0_stob_io *sio2 = &bio2->bio_part[0].bip_sio;
	struct m0_bufvec *bv1 = &sio1->si_user;
	struct m0_bufvec *bv2 = &sio2->si_user;

	return bio1->bio_stob_nr == bio2->bio_stob_nr &&
	       bio2->bio_stob_nr == 1 && /* XXX */
	       bv1->ov_vec.v_nr == bv2->ov_vec.v_nr &&
	       bv2->ov_vec.v_nr == 1 && /* XXX */
	       bv1->ov_buf[0] == bv2->ov_buf[0];
}

/** @note only one part and only one buffer in indexvec is supported atm. */
M0_INTERNAL bool m0_be_io_offset_stob_is_eq(const struct m0_be_io *bio1,
					    const struct m0_be_io *bio2)
{
	struct m0_stob_io *sio1 = &bio1->bio_part[0].bip_sio;
	struct m0_stob_io *sio2 = &bio2->bio_part[0].bip_sio;
	struct m0_indexvec *iv1 = &sio1->si_stob;
	struct m0_indexvec *iv2 = &sio2->si_stob;

	return bio1->bio_stob_nr == bio2->bio_stob_nr &&
	       bio2->bio_stob_nr == 1 && /* XXX */
	       iv1->iv_vec.v_nr == iv2->iv_vec.v_nr &&
	       iv2->iv_vec.v_nr == 1 && /* XXX */
	       iv1->iv_index[0] == iv2->iv_index[0];
}

M0_INTERNAL void m0_be_io_credit_add(struct m0_be_io_credit       *iocred0,
				     const struct m0_be_io_credit *iocred1)
{
	iocred0->bic_reg_nr   += iocred1->bic_reg_nr;
	iocred0->bic_reg_size += iocred1->bic_reg_size;
	iocred0->bic_part_nr  += iocred1->bic_part_nr;
}

M0_INTERNAL bool m0_be_io_credit_le(const struct m0_be_io_credit *iocred0,
				    const struct m0_be_io_credit *iocred1)
{
	return iocred0->bic_reg_nr   <= iocred1->bic_reg_nr &&
	       iocred0->bic_reg_size <= iocred1->bic_reg_size &&
	       iocred0->bic_part_nr  <= iocred1->bic_part_nr;
}

/** @} end of be group */


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
