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
 * Original creation date: 08/13/2010
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_EXTMAP
#include "lib/trace.h"

#include "be/extmap.h"
#include "lib/vec.h"
#include "lib/errno.h"
#include "lib/arith.h"      /* M0_3WAY */
#include "lib/misc.h"
#include "lib/finject.h"
#include "format/format.h"  /* m0_format_header_pack */

/**
   @addtogroup extmap

   <b>Extent map implementation details.</b>

   @see extmap_internal.h

   Few notes:

   @li m0_be_emap_cursor::ec_seg is "external" representation of iteration
   state. m0_be_emap_cursor::ec_key and m0_be_emap_cursor::ec_rec are "internal";

   @li after internal state has changed it is "opened" by emap_it_open() which
   updates external stats to match changes;

   @li similarly when external state has changed, it is "packed" into internal
   one by emap_it_pack().

   @li m0_be_emap_cursor::ec_pair is descriptor of buffers for data-base
   operations. Buffers are aliased to m0_be_emap_cursor::ec_key and
   m0_be_emap_cursor::ec_rec by emap_it_init().

   @li be_emap_invariant() checks implementation invariant: an extent map is an
   collection of segments with non-empty extents forming the partition of the
   name-space and ordered by their starting offsets. This creates a separate
   cursor within the same transaction as the cursor it is called against.

   @li A segment ([A, B), V) is stored as a record (A, V) with a key (prefix,
   B). Note, that the _high_ extent end is used as a key. This way,
   m0_be_btree_cursor_get() can be used to position a cursor on a segment
   containing a
   given offset. Also note, that there is some redundancy in the persistent
   state: two consecutive segments ([A, B), V) and ([B, C), U) are stored as
   records (A, V) and (B, U) with keys (prefix, B) and (prefix, C)
   respectively. B is stored twice. Generally, starting offset of a segment
   extent can always be deduced from the key of previous segment (and for the
   first segment it's 0), so some slight economy of storage could be achieved at
   the expense of increased complexity and occasional extra storage traffic.

   @note be_emap_invariant() is potentially expensive. Consider turning it off
   conditionally.

   @{
 */

/*
static void key_print(const struct m0_be_emap_key *k)
{
	printf(U128X_F":%08lx", U128_P(&k->ek_prefix), k->ek_offset);
}
*/

static int be_emap_cmp(const void *key0, const void *key1);
static m0_bcount_t be_emap_ksize(const void* k);
static m0_bcount_t be_emap_vsize(const void* d);
static int emap_it_pack(struct m0_be_emap_cursor *it,
			void (*btree_func)(struct m0_be_btree *btree,
					   struct m0_be_tx    *tx,
					   struct m0_be_op    *op,
				     const struct m0_buf      *key,
				     const struct m0_buf      *val),
			struct m0_be_tx *tx);
static bool emap_it_prefix_ok(const struct m0_be_emap_cursor *it);
static int emap_it_open(struct m0_be_emap_cursor *it);
static void emap_it_init(struct m0_be_emap_cursor *it,
			 const struct m0_uint128  *prefix,
			 m0_bindex_t               offset,
			 struct m0_be_emap        *map);
static void be_emap_close(struct m0_be_emap_cursor *it);
static int emap_it_get(struct m0_be_emap_cursor *it);
static int be_emap_lookup(struct m0_be_emap        *map,
		    const struct m0_uint128        *prefix,
		          m0_bindex_t               offset,
			  struct m0_be_emap_cursor *it);
static int be_emap_next(struct m0_be_emap_cursor *it);
static int be_emap_prev(struct m0_be_emap_cursor *it);
static bool be_emap_invariant(struct m0_be_emap_cursor *it);
static int emap_extent_update(struct m0_be_emap_cursor *it,
			      struct m0_be_tx          *tx,
			const struct m0_be_emap_seg    *es);
static int be_emap_split(struct m0_be_emap_cursor *it,
			 struct m0_be_tx          *tx,
			 struct m0_indexvec       *vec,
			 m0_bindex_t               scan);
static bool be_emap_caret_invariant(const struct m0_be_emap_caret *car);

static const struct m0_be_btree_kv_ops be_emap_ops = {
	.ko_ksize   = be_emap_ksize,
	.ko_vsize   = be_emap_vsize,
	.ko_compare = be_emap_cmp
};

static struct m0_rwlock *emap_rwlock(struct m0_be_emap *emap)
{
	return &emap->em_lock.bl_u.rwlock;
}

static void emap_dump(struct m0_be_emap_cursor *it)
{
	struct m0_be_emap_cursor scan;
	struct m0_uint128       *prefix = &it->ec_key.ek_prefix;
	struct m0_be_emap_seg   *seg    = &scan.ec_seg;
	int                      i;
	int                      rc;

	m0_rwlock_read_lock(emap_rwlock(it->ec_map));
	rc = be_emap_lookup(it->ec_map, prefix, 0, &scan);
	M0_ASSERT(rc == 0);

	M0_LOG(M0_DEBUG, U128X_F":", U128_P(prefix));
	for (i = 0; ; ++i) {
		M0_LOG(M0_DEBUG, "\t%5.5i %16lx .. %16lx: %16lx %10lx", i,
		       (unsigned long)seg->ee_ext.e_start,
		       (unsigned long)seg->ee_ext.e_end,
		       (unsigned long)m0_ext_length(&seg->ee_ext),
		       (unsigned long)seg->ee_val);
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
		rc = be_emap_next(&scan);
		M0_ASSERT(rc == 0);
	}
	be_emap_close(&scan);
	m0_rwlock_read_unlock(emap_rwlock(it->ec_map));
}

static void emap_key_init(struct m0_be_emap_key *key)
{
	m0_format_header_pack(&key->ek_header, &(struct m0_format_tag){
		.ot_version = M0_BE_EMAP_KEY_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_EMAP_KEY,
		.ot_footer_offset = offsetof(struct m0_be_emap_key, ek_footer)
	});
	m0_format_footer_update(key);
}

static void emap_rec_init(struct m0_be_emap_rec *rec)
{
	m0_format_header_pack(&rec->er_header, &(struct m0_format_tag){
		.ot_version = M0_BE_EMAP_REC_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_EMAP_REC,
		.ot_footer_offset = offsetof(struct m0_be_emap_rec, er_footer)
	});
	m0_format_footer_update(rec);
}

M0_INTERNAL int m0_be_emap_dump(struct m0_be_emap *map)
{
	struct m0_be_emap_cursor it;
	struct m0_be_emap_seg *seg;
	m0_bcount_t            nr_segs = 0;
	m0_bcount_t            nr_cobs = 0;
	int                    rc = 0;
	struct m0_uint128      prefix;

	m0_rwlock_read_lock(emap_rwlock(map));

	prefix = M0_UINT128(0, 0);
	rc = be_emap_lookup(map, &prefix, 0, &it);
	if (rc == -ESRCH) {
		prefix = it.ec_seg.ee_pre;
		rc = be_emap_lookup(map, &prefix, 0, &it);
	}
	if (rc != 0) {
		m0_rwlock_read_unlock(emap_rwlock(map));
		return M0_ERR(rc);
	}

	do {
		seg = m0_be_emap_seg_get(&it);
		M0_ASSERT(m0_ext_is_valid(&seg->ee_ext) &&
			  !m0_ext_is_empty(&seg->ee_ext));
		++nr_segs;
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			++nr_cobs;
		rc = be_emap_next(&it);
	} while (rc == 0 || rc == -ESRCH);

	be_emap_close(&it);

	m0_rwlock_read_unlock(emap_rwlock(map));

	M0_LOG(M0_DEBUG, "%p %"PRIx64" %u %lu", map,
	       seg->ee_pre.u_hi, (unsigned)nr_cobs, (unsigned long)nr_segs);

	if (rc == -ENOENT)
		return M0_RC(0);
	else
		return M0_ERR(rc);
}

static void delete_wrapper(struct m0_be_btree *btree, struct m0_be_tx *tx,
			   struct m0_be_op *op, const struct m0_buf *key,
			   const struct m0_buf *val)
{
	m0_be_btree_delete(btree, tx, op, key);
}

M0_INTERNAL void
m0_be_emap_init(struct m0_be_emap *map, struct m0_be_seg *db)
{
	m0_format_header_pack(&map->em_header, &(struct m0_format_tag){
		.ot_version = M0_BE_EMAP_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_EMAP,
		.ot_footer_offset = offsetof(struct m0_be_emap, em_footer)
	});
	m0_rwlock_init(emap_rwlock(map));
	m0_buf_init(&map->em_key_buf, &map->em_key, sizeof map->em_key);
	m0_buf_init(&map->em_val_buf, &map->em_rec, sizeof map->em_rec);
	emap_key_init(&map->em_key);
	emap_rec_init(&map->em_rec);
	m0_be_btree_init(&map->em_mapping, db, &be_emap_ops);
	map->em_seg = db;
	map->em_version = 0;
	m0_format_footer_update(map);
}

M0_INTERNAL void m0_be_emap_fini(struct m0_be_emap *map)
{
	map->em_version = 0;
	m0_be_btree_fini(&map->em_mapping);
	m0_rwlock_fini(emap_rwlock(map));
}

M0_INTERNAL void m0_be_emap_create(struct m0_be_emap *map,
				   struct m0_be_tx   *tx,
				   struct m0_be_op   *op)
{
	M0_PRE(map->em_seg != NULL);

	m0_be_op_active(op);
	M0_BE_OP_SYNC(local_op,
		      m0_be_btree_create(&map->em_mapping, tx, &local_op));
	op->bo_u.u_emap.e_rc = 0;
	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_emap_destroy(struct m0_be_emap *map,
				    struct m0_be_tx   *tx,
				    struct m0_be_op   *op)
{
	m0_be_op_active(op);
	op->bo_u.u_emap.e_rc = M0_BE_OP_SYNC_RET(
		local_op,
		m0_be_btree_destroy(&map->em_mapping, tx, &local_op),
		bo_u.u_btree.t_rc);
	m0_be_op_done(op);
}

M0_INTERNAL struct m0_be_emap_seg *
m0_be_emap_seg_get(struct m0_be_emap_cursor *it)
{
	return &it->ec_seg;
}

M0_INTERNAL bool m0_be_emap_ext_is_last(const struct m0_ext *ext)
{
	return ext->e_end == M0_BINDEX_MAX + 1;
}

M0_INTERNAL bool m0_be_emap_ext_is_first(const struct m0_ext *ext)
{
	return ext->e_start == 0;
}

M0_INTERNAL struct m0_be_op *m0_be_emap_op(struct m0_be_emap_cursor *it)
{
	return &it->ec_op;
}

M0_INTERNAL int m0_be_emap_op_rc(const struct m0_be_emap_cursor *it)
{
	return it->ec_op.bo_u.u_emap.e_rc;
}

M0_INTERNAL
struct m0_be_domain *m0_be_emap_seg_domain(const struct m0_be_emap *map)
{
	return map->em_seg->bs_domain;
}

M0_INTERNAL void m0_be_emap_lookup(struct m0_be_emap        *map,
				   const struct m0_uint128  *prefix,
				   m0_bindex_t               offset,
				   struct m0_be_emap_cursor *it)
{
	M0_PRE(offset <= M0_BINDEX_MAX);

	m0_be_op_active(&it->ec_op);
	m0_rwlock_read_lock(emap_rwlock(map));
	be_emap_lookup(map, prefix, offset, it);
	m0_rwlock_read_unlock(emap_rwlock(map));
	m0_be_op_done(&it->ec_op);

	M0_ASSERT_EX(be_emap_invariant(it));
}

M0_INTERNAL void m0_be_emap_close(struct m0_be_emap_cursor *it)
{
	M0_INVARIANT_EX(be_emap_invariant(it));
	be_emap_close(it);
}

static bool be_emap_changed(struct m0_be_emap_cursor *it, m0_bindex_t off)
{
	if (it->ec_version != it->ec_map->em_version || M0_FI_ENABLED("yes")) {
		M0_LOG(M0_DEBUG, "versions mismatch: %d != %d",
		       (int)it->ec_version, (int)it->ec_map->em_version);
		be_emap_lookup(it->ec_map, &it->ec_key.ek_prefix, off, it);
		return true;
	} else
		return false;
}

M0_INTERNAL void m0_be_emap_next(struct m0_be_emap_cursor *it)
{
	M0_PRE(!m0_be_emap_ext_is_last(&it->ec_seg.ee_ext));

	m0_be_op_active(&it->ec_op);

	m0_rwlock_read_lock(emap_rwlock(it->ec_map));
	if (!be_emap_changed(it, it->ec_key.ek_offset))
		be_emap_next(it);
	m0_rwlock_read_unlock(emap_rwlock(it->ec_map));

	m0_be_op_done(&it->ec_op);
}

M0_INTERNAL void m0_be_emap_prev(struct m0_be_emap_cursor *it)
{
	M0_PRE(!m0_be_emap_ext_is_first(&it->ec_seg.ee_ext));

	m0_be_op_active(&it->ec_op);

	m0_rwlock_read_lock(emap_rwlock(it->ec_map));
	if (!be_emap_changed(it, it->ec_rec.er_start - 1))
		be_emap_prev(it);
	m0_rwlock_read_unlock(emap_rwlock(it->ec_map));

	m0_be_op_done(&it->ec_op);
}

M0_INTERNAL void m0_be_emap_extent_update(struct m0_be_emap_cursor *it,
					  struct m0_be_tx          *tx,
				    const struct m0_be_emap_seg    *es)
{
	m0_be_op_active(&it->ec_op);
	emap_extent_update(it, tx, es);
	m0_be_op_done(&it->ec_op);
}

static int update_next_segment(struct m0_be_emap_cursor *it,
			       struct m0_be_tx          *tx,
			       m0_bindex_t               delta,
			       bool                      get_next)
{
	int rc = 0;

	if (get_next)
		rc = be_emap_next(it);

	if (rc == 0) {
		it->ec_seg.ee_ext.e_start -= delta;
		rc = emap_extent_update(it, tx, &it->ec_seg);
	}

	return M0_RC(rc);
}

M0_INTERNAL void m0_be_emap_merge(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  m0_bindex_t               delta)
{
	bool inserted = false;
	int  rc;

	M0_PRE(!m0_be_emap_ext_is_last(&it->ec_seg.ee_ext));
	M0_PRE(delta <= m0_ext_length(&it->ec_seg.ee_ext));
	M0_INVARIANT_EX(be_emap_invariant(it));

	m0_be_op_active(&it->ec_op);

	m0_rwlock_write_lock(emap_rwlock(it->ec_map));
	rc = emap_it_pack(it, delete_wrapper, tx);

	if (rc == 0 && delta < m0_ext_length(&it->ec_seg.ee_ext)) {
		it->ec_seg.ee_ext.e_end -= delta;
		rc = emap_it_pack(it, m0_be_btree_insert, tx);
		inserted = true;
	}

	if (rc == 0)
		rc = emap_it_get(it) /* re-initialise cursor position */ ?:
			update_next_segment(it, tx, delta, inserted);
	m0_rwlock_write_unlock(emap_rwlock(it->ec_map));

	M0_ASSERT_EX(ergo(rc == 0, be_emap_invariant(it)));
	it->ec_op.bo_u.u_emap.e_rc = rc;
	m0_be_op_done(&it->ec_op);
}

M0_INTERNAL void m0_be_emap_split(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  struct m0_indexvec       *vec)
{
	M0_PRE(m0_vec_count(&vec->iv_vec) == m0_ext_length(&it->ec_seg.ee_ext));
	M0_INVARIANT_EX(be_emap_invariant(it));

	m0_be_op_active(&it->ec_op);
	m0_rwlock_write_lock(emap_rwlock(it->ec_map));
	be_emap_split(it, tx, vec, it->ec_seg.ee_ext.e_start);
	m0_rwlock_write_unlock(emap_rwlock(it->ec_map));
	m0_be_op_done(&it->ec_op);

	M0_ASSERT_EX(be_emap_invariant(it));
}

M0_INTERNAL void m0_be_emap_paste(struct m0_be_emap_cursor *it,
				  struct m0_be_tx          *tx,
				  struct m0_ext            *ext,
				  uint64_t                  val,
	void (*del)(struct m0_be_emap_seg*),
	void (*cut_left)(struct m0_be_emap_seg*, struct m0_ext*, uint64_t),
	void (*cut_right)(struct m0_be_emap_seg*, struct m0_ext*, uint64_t))
{
	struct m0_be_emap_seg *seg      = &it->ec_seg;
	struct m0_ext         *chunk    = &seg->ee_ext;
	const struct m0_ext    ext0     = *ext;
	struct m0_ext          clip;
	m0_bcount_t            length[3];
	typeof(val)            bstart[3] = {};
	m0_bcount_t            consumed;
	uint64_t               val_orig;
	struct m0_indexvec     vec = {
		.iv_vec = {
			.v_nr    = ARRAY_SIZE(length),
			.v_count = length
		},
		.iv_index = bstart
	};
	int rc = 0;

	M0_PRE(m0_ext_is_in(chunk, ext->e_start));
	M0_INVARIANT_EX(be_emap_invariant(it));

	M0_BE_CREDIT_DEC(M0_BE_CU_EMAP_PASTE, tx);

	m0_be_op_active(&it->ec_op);

	/*
	 * Iterate over existing segments overlapping with the new one,
	 * calculating for each, what parts have to be deleted and what remains.
	 *
	 * In the worst case, an existing segment can split into three
	 * parts. Generally, some of these parts can be empty.
	 *
	 * Cutting and deleting segments is handled uniformly by
	 * be_emap_split(), thanks to the latter skipping empty segments.
	 *
	 * Note that the _whole_ new segment is inserted on the last iteration
	 * of the loop below (see length[1] assignment), thus violating the map
	 * invariant until the loop exits (the map is "porous" during that
	 * time).
	 */

	m0_rwlock_write_lock(emap_rwlock(it->ec_map));
	while (!m0_ext_is_empty(ext)) {
		m0_ext_intersection(ext, chunk, &clip);
		M0_LOG(M0_DEBUG, "ext="EXT_F" chunk="EXT_F" clip="EXT_F,
			EXT_P(ext), EXT_P(chunk), EXT_P(&clip));
		consumed = m0_ext_length(&clip);
		M0_ASSERT(consumed > 0);

		length[0] = clip.e_start - chunk->e_start;
		length[1] = clip.e_end == ext->e_end ? m0_ext_length(&ext0) : 0;
		length[2] = chunk->e_end - clip.e_end;
		M0_LOG(M0_DEBUG, "len123=%lx:%lx:%lx", (unsigned long)length[0],
			(unsigned long)length[1], (unsigned long)length[2]);

		bstart[1] = val;
		val_orig  = seg->ee_val;

		if (length[0] > 0) {
			if (cut_left)
				cut_left(seg, &clip, val_orig);
			bstart[0] = seg->ee_val;
		}
		if (length[2] > 0) {
			if (cut_right)
				cut_right(seg, &clip, val_orig);
			bstart[2] = seg->ee_val;
		}
		if (length[0] == 0 && length[2] == 0 && del)
			del(seg);

		rc = be_emap_split(it, tx, &vec, length[0] > 0 ?
						chunk->e_start : ext0.e_start);
		if (rc != 0)
			break;

		ext->e_start += consumed;
		M0_ASSERT(ext->e_start <= ext->e_end);

		M0_LOG(M0_DEBUG, "left %llu",
				(unsigned long long)m0_ext_length(ext));

		if (m0_ext_is_empty(ext))
			break;
		/*
		 * If vec is empty, be_emap_split() just deletes
		 * the current extent and puts iterator to the next
		 * position automatically.
		 */
		if (!m0_vec_is_empty(&vec.iv_vec)) {
			M0_ASSERT(!m0_be_emap_ext_is_last(&seg->ee_ext));
			if (be_emap_next(it) != 0)
				break;
		}
	}
	m0_rwlock_write_unlock(emap_rwlock(it->ec_map));

	/* emap_dump(it); */ /* expensive - use for debug only */

	M0_ASSERT_EX(ergo(rc == 0, be_emap_invariant(it)));

	it->ec_op.bo_u.u_emap.e_rc = rc;

	m0_be_op_done(&it->ec_op);

	/*
	 * A tale of two keys.
	 *
	 * Primordial version of this function inserted the whole new extent (as
	 * specified by @ext) at the first iteration of the loop. From time to
	 * time the (clip.e_start == ext->e_start) assertion got violated for no
	 * apparent reason. Eventually, after a lot of tracing (by Anatoliy),
	 * the following sequence was tracked down:
	 *
	 * - on entry to m0_be_emap_paste():
	 *
	 *   map: *[0, 512) [512, 1024) [1024, 2048) [2048, ...)
	 *   ext:   [0, 1024)
	 *
	 *   (where current cursor position is starred).
	 *
	 * - at the end of the first iteration, instead of expected
	 *
	 *   map: [0, 1024) *[512, 1024) [1024, 2048) [2048, ...)
	 *
	 *   the map was
	 *
	 *   map: [0, 1024) *[1024, 2048) [2048, ...)
	 *
	 * - that is, the call to be_emap_split():
	 *
	 *   - deleted [0, 512) (as expected),
	 *   - inserted [0, 1024) (as expected),
	 *   - deleted [512, 1024) ?!
	 *
	 * The later is seemingly impossible, because the call deletes exactly
	 * one segment. The surprising explanation is that segment ([L, H), V)
	 * is stored as a record (L, V) with H as a key (this is documented at
	 * the top of this file) and the [0, 1024) segment has the same key as
	 * already existing [512, 1024) one, with the former forever masking the
	 * latter.
	 *
	 * The solution is to insert the new extent as the last step, but the
	 * more important moral of this melancholy story is
	 *
	 *         Thou shalt wit thine abstraction levels.
	 *
	 * In the present case, be_emap_split() operates on the level of
	 * records and keys which turns out to be subtly different from the
	 * level of segments and maps.
	 */
}

M0_INTERNAL int m0_be_emap_count(struct m0_be_emap_cursor *it,
				 m0_bcount_t *segs)
{
	struct m0_be_emap_seg *seg;
	struct m0_be_op       *op;
	m0_bcount_t            nr_segs = 0;
	int                    rc = 0;

	M0_INVARIANT_EX(be_emap_invariant(it));

	op = m0_be_emap_op(it);
	do {
		seg = m0_be_emap_seg_get(it);
		M0_ASSERT(m0_ext_is_valid(&seg->ee_ext) &&
			  !m0_ext_is_empty(&seg->ee_ext));
		++nr_segs;
		if (m0_be_emap_ext_is_last(&seg->ee_ext))
			break;
		M0_SET0(op);
		m0_be_op_init(op);
		m0_be_emap_next(it);
		m0_be_op_wait(op);
		rc = m0_be_emap_op_rc(it);
		m0_be_op_fini(op);
	} while (rc == 0);

	*segs = nr_segs;

	return M0_RC(rc);
}

M0_INTERNAL void m0_be_emap_obj_insert(struct m0_be_emap *map,
				       struct m0_be_tx   *tx,
				       struct m0_be_op   *op,
			         const struct m0_uint128 *prefix,
				       uint64_t           val)
{
	m0_be_op_active(op);

	m0_rwlock_write_lock(emap_rwlock(map));
	map->em_key.ek_prefix = *prefix;
	map->em_key.ek_offset = M0_BINDEX_MAX + 1;
	m0_format_footer_update(&map->em_key);
	map->em_rec.er_start = 0;
	map->em_rec.er_value = val;
	m0_format_footer_update(&map->em_rec);

	++map->em_version;
	op->bo_u.u_emap.e_rc = M0_BE_OP_SYNC_RET(
		local_op,
		m0_be_btree_insert(&map->em_mapping, tx, &local_op,
				   &map->em_key_buf, &map->em_val_buf),
		bo_u.u_btree.t_rc);
	m0_rwlock_write_unlock(emap_rwlock(map));

	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_emap_obj_delete(struct m0_be_emap *map,
				       struct m0_be_tx   *tx,
				       struct m0_be_op   *op,
				 const struct m0_uint128 *prefix)
{
	struct m0_be_emap_cursor it;
	int                      rc;

	m0_be_op_active(op);

	m0_rwlock_write_lock(emap_rwlock(map));
	rc = be_emap_lookup(map, prefix, 0, &it);
	if (rc == 0) {
		M0_ASSERT(m0_be_emap_ext_is_first(&it.ec_seg.ee_ext) &&
			  m0_be_emap_ext_is_last(&it.ec_seg.ee_ext));
		rc = emap_it_pack(&it, delete_wrapper, tx);
		be_emap_close(&it);
	}
	op->bo_u.u_emap.e_rc = rc;
	m0_rwlock_write_unlock(emap_rwlock(map));

	m0_be_op_done(op);
}

M0_INTERNAL void m0_be_emap_caret_init(struct m0_be_emap_caret  *car,
				       struct m0_be_emap_cursor *it,
				       m0_bindex_t               index)
{
	M0_PRE(index <= M0_BINDEX_MAX);
	M0_PRE(m0_ext_is_in(&it->ec_seg.ee_ext, index));
	car->ct_it    = it;
	car->ct_index = index;
	M0_ASSERT(be_emap_caret_invariant(car));
}

M0_INTERNAL void m0_be_emap_caret_fini(struct m0_be_emap_caret *car)
{
	M0_ASSERT(be_emap_caret_invariant(car));
}

M0_INTERNAL m0_bcount_t
m0_be_emap_caret_step(const struct m0_be_emap_caret *car)
{
	M0_ASSERT(be_emap_caret_invariant(car));
	return car->ct_it->ec_seg.ee_ext.e_end - car->ct_index;
}

M0_INTERNAL int m0_be_emap_caret_move(struct m0_be_emap_caret *car,
				      m0_bcount_t              count)
{
	int rc = 0;

	m0_be_op_active(&car->ct_it->ec_op);

	m0_rwlock_read_lock(emap_rwlock(car->ct_it->ec_map));
	M0_ASSERT(be_emap_caret_invariant(car));
	while (count > 0 && car->ct_index < M0_BINDEX_MAX + 1) {
		m0_bcount_t step;

		step = m0_be_emap_caret_step(car);
		if (count >= step) {
			struct m0_be_emap_cursor *it = car->ct_it;
			if (be_emap_changed(it, car->ct_index))
				rc = it->ec_op.bo_u.u_emap.e_rc;
			rc = rc ?: be_emap_next(it);
			if (rc < 0)
				break;
		} else
			step = count;
		car->ct_index += step;
		count -= step;
	}
	M0_ASSERT(be_emap_caret_invariant(car));
	m0_rwlock_read_unlock(emap_rwlock(car->ct_it->ec_map));

	m0_be_op_done(&car->ct_it->ec_op);
	return rc < 0 ? rc : car->ct_index == M0_BINDEX_MAX + 1;
}

M0_INTERNAL int m0_be_emap_caret_move_sync(struct m0_be_emap_caret *car,
				           m0_bcount_t              count)
{
	int rc = 0;

	M0_SET0(&car->ct_it->ec_op);
	m0_be_op_init(&car->ct_it->ec_op);
	rc = m0_be_emap_caret_move(car, count);
	if (rc == 0)
		m0_be_op_wait(&car->ct_it->ec_op);

	return rc;
}

M0_INTERNAL void m0_be_emap_credit(struct m0_be_emap      *map,
				   enum m0_be_emap_optype  optype,
				   m0_bcount_t             nr,
				   struct m0_be_tx_credit *accum)
{
	M0_PRE(M0_IN(optype, (M0_BEO_CREATE, M0_BEO_DESTROY, M0_BEO_INSERT,
			      M0_BEO_DELETE, M0_BEO_UPDATE,
			      M0_BEO_MERGE, M0_BEO_SPLIT, M0_BEO_PASTE)));

	switch (optype) {
	case M0_BEO_CREATE:
		m0_be_btree_create_credit(&map->em_mapping, nr, accum);
		break;
	case M0_BEO_DESTROY:
		M0_ASSERT(nr == 1);
		m0_be_btree_destroy_credit(&map->em_mapping, accum);
		break;
	case M0_BEO_INSERT:
		m0_be_btree_insert_credit(&map->em_mapping, nr,
			sizeof map->em_key, sizeof map->em_rec, accum);
		break;
	case M0_BEO_DELETE:
		m0_be_btree_delete_credit(&map->em_mapping, nr,
			sizeof map->em_key, sizeof map->em_rec, accum);
		break;
	case M0_BEO_UPDATE:
		m0_be_btree_update_credit(&map->em_mapping, nr,
			sizeof map->em_rec, accum);
		break;
	case M0_BEO_MERGE:
		m0_be_btree_delete_credit(&map->em_mapping, nr,
			sizeof map->em_key, sizeof map->em_rec, accum);
		m0_be_btree_insert_credit(&map->em_mapping, nr,
			sizeof map->em_key, sizeof map->em_rec, accum);
		m0_be_btree_update_credit(&map->em_mapping, nr,
			sizeof map->em_rec, accum);
		break;
	case M0_BEO_SPLIT:
		m0_be_btree_delete_credit(&map->em_mapping, 1,
			sizeof map->em_key, sizeof map->em_rec, accum);
		m0_be_btree_insert_credit(&map->em_mapping, nr,
			sizeof map->em_key, sizeof map->em_rec, accum);
		m0_be_btree_update_credit(&map->em_mapping, 1,
			sizeof map->em_rec, accum);
		M0_BE_CREDIT_INC(nr, M0_BE_CU_EMAP_SPLIT, accum);
		break;
	case M0_BEO_PASTE:
		m0_forall(i, nr, m0_be_emap_credit(map, M0_BEO_SPLIT, 3, accum),
			  true);
		M0_BE_CREDIT_INC(nr, M0_BE_CU_EMAP_PASTE, accum);
		break;
	default:
		M0_IMPOSSIBLE("invalid emap operation");
	}
}

static int
be_emap_cmp(const void *key0, const void *key1)
{
	const struct m0_be_emap_key *a0 = key0;
	const struct m0_be_emap_key *a1 = key1;

	return m0_uint128_cmp(&a0->ek_prefix, &a1->ek_prefix) ?:
		M0_3WAY(a0->ek_offset, a1->ek_offset);
}

static m0_bcount_t
be_emap_ksize(const void* k)
{
	return sizeof(struct m0_be_emap_key);
}

static m0_bcount_t
be_emap_vsize(const void* d)
{
	return sizeof(struct m0_be_emap_rec);
}

static int
emap_it_pack(struct m0_be_emap_cursor *it,
	     void (*btree_func)(struct m0_be_btree *btree,
				struct m0_be_tx    *tx,
				struct m0_be_op    *op,
			  const struct m0_buf      *key,
			  const struct m0_buf      *val),
	     struct m0_be_tx *tx)
{
	const struct m0_be_emap_seg *ext = &it->ec_seg;
	struct m0_be_emap_key       *key = &it->ec_key;
	struct m0_be_emap_rec       *rec = &it->ec_rec;

	key->ek_prefix = ext->ee_pre;
	key->ek_offset = ext->ee_ext.e_end;
	m0_format_footer_update(key);
	rec->er_start  = ext->ee_ext.e_start;
	rec->er_value  = ext->ee_val;
	m0_format_footer_update(rec);
	++it->ec_map->em_version;
	it->ec_op.bo_u.u_emap.e_rc = M0_BE_OP_SYNC_RET(
		op,
		btree_func(&it->ec_map->em_mapping, tx, &op, &it->ec_keybuf,
			   &it->ec_recbuf),
		bo_u.u_btree.t_rc);

	return it->ec_op.bo_u.u_emap.e_rc;
}

static bool emap_it_prefix_ok(const struct m0_be_emap_cursor *it)
{
	return m0_uint128_eq(&it->ec_seg.ee_pre, &it->ec_prefix);
}

static int emap_it_open(struct m0_be_emap_cursor *it)
{
	struct m0_be_emap_key *key;
	struct m0_be_emap_rec *rec;
	struct m0_buf          keybuf;
	struct m0_buf          recbuf;
	struct m0_be_emap_seg *ext = &it->ec_seg;
	struct m0_be_op       *op  = &it->ec_cursor.bc_op;
	int                    rc;

	M0_PRE(m0_be_op_is_done(op));

	rc = op->bo_u.u_btree.t_rc;
	if (rc == 0) {
		m0_be_btree_cursor_kv_get(&it->ec_cursor, &keybuf, &recbuf);
		key = keybuf.b_addr;
		rec = recbuf.b_addr;
		it->ec_key = *key;
		it->ec_rec = *rec;
		emap_key_init(&it->ec_key);
		emap_rec_init(&it->ec_rec);
		ext->ee_pre         = key->ek_prefix;
		ext->ee_ext.e_start = rec->er_start;
		ext->ee_ext.e_end   = key->ek_offset;
		m0_ext_init(&ext->ee_ext);
		ext->ee_val         = rec->er_value;
		if (!emap_it_prefix_ok(it))
			rc = -ESRCH;
	}
	it->ec_op.bo_u.u_emap.e_rc = rc;

	return rc;
}

static void emap_it_init(struct m0_be_emap_cursor *it,
			 const struct m0_uint128  *prefix,
			 m0_bindex_t               offset,
			 struct m0_be_emap        *map)
{
	m0_buf_init(&it->ec_keybuf, &it->ec_key, sizeof it->ec_key);
	m0_buf_init(&it->ec_recbuf, &it->ec_rec, sizeof it->ec_rec);

	emap_key_init(&it->ec_key);
	it->ec_key.ek_prefix = it->ec_prefix = *prefix;
	it->ec_key.ek_offset = offset + 1;
	emap_rec_init(&it->ec_rec);

	it->ec_map = map;
	it->ec_version = map->em_version;
	m0_be_btree_cursor_init(&it->ec_cursor, &map->em_mapping);
}

static void be_emap_close(struct m0_be_emap_cursor *it)
{
	m0_be_btree_cursor_fini(&it->ec_cursor);
}

static int emap_it_get(struct m0_be_emap_cursor *it)
{
	struct m0_be_op *op = &it->ec_cursor.bc_op;
	int              rc;

	M0_SET0(op);
	m0_be_op_init(op);
	m0_be_btree_cursor_get(&it->ec_cursor, &it->ec_keybuf, true);
	m0_be_op_wait(op);
	rc = emap_it_open(it);
	m0_be_op_fini(op);

	return rc;
}

static int be_emap_lookup(struct m0_be_emap        *map,
			  const struct m0_uint128  *prefix,
			  m0_bindex_t               offset,
			  struct m0_be_emap_cursor *it)
{
	int rc;

	emap_it_init(it, prefix, offset, map);
	rc = emap_it_get(it);
	if (rc != 0)
		be_emap_close(it);

	M0_POST(ergo(rc == 0, m0_ext_is_in(&it->ec_seg.ee_ext, offset)));

	return M0_RC(rc);
}

static int be_emap_next(struct m0_be_emap_cursor *it)
{
	struct m0_be_op *op = &it->ec_cursor.bc_op;
	int              rc;

	M0_SET0(op);
	m0_be_op_init(op);
	m0_be_btree_cursor_next(&it->ec_cursor);
	m0_be_op_wait(op);
	rc = emap_it_open(it);
	m0_be_op_fini(op);

	return rc;
}

static int
be_emap_prev(struct m0_be_emap_cursor *it)
{
	struct m0_be_op *op = &it->ec_cursor.bc_op;
	int              rc;

	M0_SET0(op);
	m0_be_op_init(op);
	m0_be_btree_cursor_prev(&it->ec_cursor);
	m0_be_op_wait(op);
	rc = emap_it_open(it);
	m0_be_op_fini(op);

	return rc;
}

#if 1
static bool
be_emap_invariant_check(struct m0_be_emap_cursor *it)
{
	int                   rc;
	m0_bindex_t           reached	= 0;
	m0_bcount_t           total	= 0;

	if (!_0C(m0_be_emap_ext_is_first(&it->ec_seg.ee_ext)))
		return false;
	while (1) {
		if (!_0C(it->ec_seg.ee_ext.e_start == reached))
			return false;
		if (!_0C(it->ec_seg.ee_ext.e_end > reached))
			return false;
		if (!_0C(m0_format_footer_verify(&it->ec_key) == 0))
			return false;
		if (!_0C(m0_format_footer_verify(&it->ec_rec) == 0))
			return false;
		reached = it->ec_seg.ee_ext.e_end;
		total += m0_ext_length(&it->ec_seg.ee_ext);
		if (m0_be_emap_ext_is_last(&it->ec_seg.ee_ext))
			break;
		rc = be_emap_next(it);
		if (rc != 0)
			break;
	}
	if (!_0C(total == M0_BCOUNT_MAX))
		return false;
	if (!_0C(reached == M0_BINDEX_MAX + 1))
		return false;
	return true;
}

static bool
be_emap_invariant(struct m0_be_emap_cursor *it)
{
	struct m0_be_emap_cursor scan;
	int                      rc;
	bool                     is_good = true;

	m0_rwlock_read_lock(emap_rwlock(it->ec_map));
	rc = be_emap_lookup(it->ec_map, &it->ec_key.ek_prefix, 0, &scan);
	if (rc == 0) {
		is_good = be_emap_invariant_check(&scan);
		be_emap_close(&scan);
	}
	m0_rwlock_read_unlock(emap_rwlock(it->ec_map));

	if (!is_good)
		emap_dump(it);

	return is_good;
}

#else

static bool
be_emap_invariant(struct m0_be_emap_cursor *it)
{
	return true;
}
#endif

static int
emap_extent_update(struct m0_be_emap_cursor *it,
		   struct m0_be_tx          *tx,
	     const struct m0_be_emap_seg    *es)
{
	M0_PRE(it != NULL);
	M0_PRE(es != NULL);
	M0_PRE(m0_uint128_eq(&it->ec_seg.ee_pre, &es->ee_pre));
	M0_PRE(it->ec_seg.ee_ext.e_end == es->ee_ext.e_end);

	it->ec_seg.ee_ext.e_start = es->ee_ext.e_start;
	it->ec_seg.ee_val = es->ee_val;
	return emap_it_pack(it, m0_be_btree_update, tx);
}

static int
be_emap_split(struct m0_be_emap_cursor *it,
	      struct m0_be_tx          *tx,
	      struct m0_indexvec       *vec,
	      m0_bindex_t               scan)
{
	int rc = 0;
	m0_bcount_t count;
	m0_bindex_t seg_end = it->ec_seg.ee_ext.e_end;
	uint32_t    i;

	for (i = 0; i < vec->iv_vec.v_nr; ++i) {
		count = vec->iv_vec.v_count[i];
		if (count == 0)
			continue;
		M0_BE_CREDIT_DEC(M0_BE_CU_EMAP_SPLIT, tx);
		it->ec_seg.ee_ext.e_start = scan;
		it->ec_seg.ee_ext.e_end   = scan + count;
		it->ec_seg.ee_val         = vec->iv_index[i];
		if (it->ec_seg.ee_ext.e_end == seg_end)
			/* The end of original segment is reached:
			 * just update it instead of deleting and
			 * inserting again - it is cheaper.
			 * Note: the segment key in underlying btree
			 *       is the end offset of its extent. */
			rc = emap_it_pack(it, m0_be_btree_update, tx);
		else
			rc = emap_it_pack(it, m0_be_btree_insert, tx);
		if (rc != 0)
			break;
		scan += count;
	}

	/* If the vector is empty or the segment end was not reached:
	 * just delete the segment - that is used by m0_be_emap_paste(). */
	if (rc == 0 && (m0_vec_is_empty(&vec->iv_vec) ||
			it->ec_seg.ee_ext.e_end != seg_end)) {
		m0_bindex_t last_end = it->ec_seg.ee_ext.e_end;
		it->ec_seg.ee_ext.e_end = seg_end;
		rc = emap_it_pack(it, delete_wrapper, tx);
		it->ec_key.ek_offset = last_end;
		m0_format_footer_update(&it->ec_key);
	}

	if (rc == 0)
		/* Re-initialize cursor position. */
		rc = emap_it_get(it);

	it->ec_op.bo_u.u_emap.e_rc = rc;
	return M0_RC(rc);
}

static bool
be_emap_caret_invariant(const struct m0_be_emap_caret *car)
{
	return _0C(m0_ext_is_in(&car->ct_it->ec_seg.ee_ext, car->ct_index) ||
		   (m0_be_emap_ext_is_last(&car->ct_it->ec_seg.ee_ext) &&
		    car->ct_index == M0_BINDEX_MAX + 1));
}

/** @} end group extmap */
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
