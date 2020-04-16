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
 * Original author: Anand Vidwansa <anand_vidwansa@xyratex.com>
 * Original creation date: 12/17/2012
 */

#include "lib/bob.h"		/* m0_bob_type */
#include "lib/memory.h"		/* M0_ALLOC_ARR */
#include "lib/misc.h"		/* m0_forall */
#include "lib/errno.h"		/* Includes appropriate errno header. */
#include "lib/types.h"		/* Includes appropriate types header. */
#include "lib/string.h"		/* strcmp() */
#include "lib/finject.h"	/* M0_FI_ENABLED() */
#include "lib/varr.h"		/* m0_varr */
#include "lib/varr_private.h"	/* m0_varr_buf_alloc(), m0_varr_buf_free */
#ifndef __KERNEL__
#include <limits.h>		/* CHAR_BIT */
#else
#include <linux/pagemap.h>	/* PAGE_SIZE */
#include <linux/limits.h>
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

M0_INTERNAL const struct m0_bob_type varr_bobtype;

M0_BOB_DEFINE(M0_INTERNAL, &varr_bobtype, m0_varr);

M0_INTERNAL const struct m0_bob_type varr_bobtype = {
	.bt_name         = "virtual_array",
	.bt_magix_offset = offsetof(struct m0_varr, va_magic),
	.bt_magix        = M0_LIB_GENARRAY_MAGIC,
	.bt_check        = NULL,
};

M0_INTERNAL bool varr_invariant(const struct m0_varr *arr);
/** Constructs a tree to hold buffers. */
M0_INTERNAL int varr_buffers_alloc(struct m0_varr *arr);
/** Frees a tree holding buffers. */
M0_INTERNAL void varr_buffers_dealloc(struct m0_varr *arr);
/**
 * Evaluates the height of the tree based upon total number of leaf-level
 * buffers to be allocated.
 */
M0_INTERNAL uint32_t depth_find(const struct m0_varr *arr, uint64_t buff_nr);
/**
 * Returns index within a buffer at given depth, for a given target_index
 * in an array.
 */
M0_INTERNAL uint32_t index_within_level(const struct m0_varr *arr,
					uint64_t target_idx, uint32_t depth);
/** Returns total number of children for a node at given level in a tree. */
M0_INTERNAL uint32_t children_of_level(const struct m0_varr *arr,
				       uint32_t level);
/**
 * Fetches address of a buffer in which object with index 'index' resides.
 * Returns NULL in case address of the buffer is not present in cache.
 */
M0_INTERNAL void *cache_fetch(const struct m0_varr *arr, uint64_t index);
/**
 * Updates the cache with address of a buffer and range of indices residing
 * in it.
 */
M0_INTERNAL void cache_update(struct m0_varr *arr, void *holder,
			      uint64_t start_index);
/** Returns number of objects that can fit in a single buffer. */
M0_INTERNAL unsigned long varr_obj_nr_in_buff(const struct m0_varr *arr);
/** Computes number of buffers required at the leaf-level. */
M0_INTERNAL uint64_t total_leaf_buffers(unsigned long nr,
					unsigned long obj_nr_in_1_cont,
					uint8_t obj_nr_shift);
/** Returns index of the highest numbered node at given depth. */
M0_INTERNAL uint64_t max_idx_within_level(const struct m0_varr_cursor *cursor,
					  uint32_t depth);
M0_INTERNAL uint32_t inc_to_idx_xlate(const struct m0_varr_cursor *cursor,
				      uint64_t carry, uint32_t depth);
M0_INTERNAL uint64_t inc_for_next_level(const struct m0_varr_cursor *cursor,
					uint64_t carry, uint32_t depth);

M0_INTERNAL uint8_t log_radix(const struct m0_varr *arr, uint32_t level);
/** Returns the ceiling of logarithm to the base two. */
M0_INTERNAL uint8_t nearest_power_of_two(size_t num);
/** Returns a 64-bit number whose last 'n' bits are set, and rest are zero. */
M0_INTERNAL uint64_t last_nbits_set(uint8_t n);
/** Increments buffer based upon its level in a tree */
M0_INTERNAL void *buff_incr(const struct m0_varr *arr, uint32_t depth,
			    void *buff, uint32_t inc);
/** Shifts a given number to left/right by taking into account sizeof(number) */
#define safe_bitshift(num, shift, operator)				 \
({									 \
	uint8_t     __shift = (shift);					 \
	typeof(num) __num   = (num);					 \
	M0_ASSERT(__shift < CHAR_BIT * sizeof __num);			 \
	__num operator __shift;						 \
})

M0_INTERNAL int m0_varr_init(struct m0_varr *arr, uint64_t nr, size_t size,
			     size_t bufsize)
{
	int	 rc = 0;

	M0_PRE(arr != NULL);
	M0_PRE(nr > 0 && size > 0 && bufsize > 0);
	M0_PRE(size <= bufsize);
#ifdef __KERNEL__
	M0_PRE(bufsize <= PAGE_SIZE);
#endif

	arr->va_nr              = nr;
	/* Can result into padding if object and buffer sizes are not integer
	 * powers of two. */
	arr->va_obj_shift       = nearest_power_of_two(size);
	arr->va_obj_size	= safe_bitshift((size_t) 1, arr->va_obj_shift,
						<<);
	arr->va_buf_shift       = nearest_power_of_two(bufsize);
	arr->va_bufsize		= safe_bitshift((size_t) 1, arr->va_buf_shift,
						<<);
	arr->va_bufptr_nr_shift = arr->va_buf_shift -
		nearest_power_of_two(M0_VA_TNODEPTR_SIZE);
	arr->va_bufptr_nr       = safe_bitshift((uint64_t) 1,
						arr->va_bufptr_nr_shift, <<);
	m0_varr_bob_init(arr);
	arr->va_failure_depth   = 0;
	M0_ALLOC_PTR(arr->va_cache);
	if (arr->va_cache != NULL) {
		arr->va_buff_nr = total_leaf_buffers(arr->va_nr,
						     varr_obj_nr_in_buff(arr),
						     arr->va_buf_shift -
						     arr->va_obj_shift);
		arr->va_depth = depth_find(arr, arr->va_buff_nr);
		rc = varr_buffers_alloc(arr);
	} else
		rc = -ENOMEM;
	if (rc != 0)
		m0_varr_fini(arr);
	M0_POST_EX(ergo(rc == 0, varr_invariant(arr)));
	return M0_RC(rc);
}

M0_INTERNAL uint8_t nearest_power_of_two(size_t num)
{
	size_t  aligned_num  = 1;
	uint8_t aligned_shift = 0;

	M0_PRE(num > 0);

	while (num > aligned_num) {
		aligned_num = safe_bitshift(aligned_num, 1, <<);
		++aligned_shift;
	}
	return aligned_shift;
}

M0_INTERNAL unsigned long varr_obj_nr_in_buff(const struct m0_varr *arr)
{
	M0_PRE(arr != NULL);
	return	safe_bitshift((unsigned long) 1,
			      (arr->va_buf_shift - arr->va_obj_shift), <<);
}

M0_INTERNAL uint64_t total_leaf_buffers(unsigned long nr,
					unsigned long obj_nr_in_1_cont,
					uint8_t obj_nr_shift)
{
	uint64_t buff_nr;
	M0_PRE(obj_nr_in_1_cont > 0);

	buff_nr  = safe_bitshift(nr, obj_nr_shift, >>);
	buff_nr += (nr & (obj_nr_in_1_cont - 1)) == 0 ? 0 : 1;
	return buff_nr;
}

/* All trees that hold objects will have same depth. This depth is a many to
 * one function of total number of objects to be stored in the array.
 * For example, suppose one buffer can hold k objects, then an array of k
 * objects can fit into a single leaf node of a tree. Then in order to store an
 * array with k + 1 objects, instead of using a tree with depth 2, we use two
 * trees each having depth one. Thus, if total number of available trees is
 * M0_VA_TNODE_NR then for *all* arrays with total objects less than or equal to
 * k * M0_VA_TNODE_NR, depth of trees holding object(s) will be one.
 * When total objects in an array exceed k * M0_VA_TNODE_NR, we increase
 * depth by one. If buf_size represents size of a buffer, * ptr_size represents
 * size of a pointer and obj_size represents size of an object, then following
 * table summarizes mapping between total number of objects and depth of trees
 * holding objects.
 * @verbatim
  _______________________________________________________________________
 | Max. number of objects                                     | Depth   |
 |____________________________________________________________|_________|
 | M0_VA_TNODE_NR * (bufsize/obj_size)                        |   1     |
 |____________________________________________________________|_________|
 | M0_VA_TNODE_NR * (buffsize/ptr_size)  * (buf_size/obj_size)|   2     |
 |____________________________________________________________|_________|
 | M0_VA_TNODE_NR * (bufsize/ptr_size)^2 * (buf_size/obj_size)|   3     |
 |____________________________________________________________|_________|
 * @endverbatim
 * The current implementation treats the structure virtual array not as a
 * collection of trees with same depth, but as a single tree encompassing
 * entire data-structure. Following function returns depth of this tree. For
 * each case in the table above, this tree has depth one more than the one
 * mentioned in the table above.
 */
M0_INTERNAL uint32_t depth_find(const struct m0_varr *arr,
				uint64_t total_leaves)
{
	uint32_t level;

	M0_PRE(arr != NULL);
	M0_PRE(total_leaves > 0);

	for (level = 1;; ++level)
		if (total_leaves <= safe_bitshift((uint64_t) 1,
						  arr->va_bufptr_nr_shift *
						  (level - 1) +
						  M0_VA_TNODE_NR_SHIFT, <<))
			break;
	return level + 1;
}

M0_INTERNAL int varr_buffers_alloc(struct m0_varr *arr)
{
	struct m0_varr_cursor cursor;
	int		      rc = 0;
	void		     *holder;
	uint32_t	      i;

	for (i = 1; i < arr->va_depth; ++i) {
		rc = m0_varr_cursor_init(&cursor, arr, i);
		if (rc != 0)
			goto end;
		do {
			holder = m0_varr_buf_alloc(arr->va_bufsize);
			if (holder == NULL) {
				rc = -ENOMEM;
				arr->va_failure_depth = cursor.vc_done == 0 ?
				 cursor.vc_depth : cursor.vc_depth + 1;
				goto end;
			}
			*(void **)m0_varr_cursor_get(&cursor) = holder;
		} while (m0_varr_cursor_next(&cursor));
	}
end:
	return M0_RC(rc);
}

M0_INTERNAL int m0_varr_cursor_init(struct m0_varr_cursor *cursor,
				    const struct m0_varr *arr,
				    uint32_t depth)
{
	struct m0_varr_path_element *pe;
	void			    *buf;
	void			    *root;

	M0_PRE(cursor != NULL);
	M0_PRE(arr != NULL);
	M0_PRE(depth <= arr->va_depth);

	cursor->vc_arr	 = (struct m0_varr *)arr;
	cursor->vc_depth = 0;
	cursor->vc_done	 = 0;
	pe		 = &cursor->vc_path[0];
	pe->vp_idx	 = 0;
	root		 = (void *)arr->va_tree;
	/* Note that we will never dereference pe->vp_buf at depth == 0 outside
	 * the scope of this function */
	pe->vp_buf	 = (void *)&root;
	pe->vp_width	 = 1;

	while (cursor->vc_depth < depth) {
		buf = pe->vp_buf;
		if (buf != NULL) {
			++pe;
			++cursor->vc_depth;
			pe->vp_buf   = *(void **)buf;
			pe->vp_idx   = 0;
			pe->vp_width = children_of_level(arr,
							 cursor->vc_depth);
		} else
			return M0_ERR(-EINVAL);
	}
	return 0;
}

M0_INTERNAL uint32_t children_of_level(const struct m0_varr *arr,
				       uint32_t level)
{
	M0_PRE(arr != NULL);
	M0_PRE(level <= arr->va_depth);

	if (level <= 1)
		return level == 1 ? M0_VA_TNODE_NR : 1;
	else
		return level == arr->va_depth ?
			safe_bitshift((uint32_t) 1,
				      arr->va_buf_shift - arr->va_obj_shift,
				      <<) : arr->va_bufptr_nr;
}

M0_INTERNAL void* m0_varr_cursor_get(struct m0_varr_cursor *cursor)
{
	M0_PRE(cursor != NULL);
	return cursor->vc_path[cursor->vc_depth].vp_buf;
}

M0_INTERNAL int m0_varr_cursor_next(struct m0_varr_cursor *cursor)
{
	return m0_varr_cursor_move(cursor, 1);
}

M0_INTERNAL int m0_varr_cursor_move(struct m0_varr_cursor *cursor,
				    uint64_t inc)
{
	void			    *buf;
	struct m0_varr_path_element *pe;
	uint32_t		     d = cursor->vc_depth;
	uint64_t		     target_idx;
	uint64_t		     max_idx_in_level;
	uint64_t		     idx_in_level;

	M0_PRE(cursor != NULL);
	M0_PRE(d <= cursor->vc_arr->va_depth);

	pe = &cursor->vc_path[d];
	max_idx_in_level = max_idx_within_level(cursor, d);
	target_idx = cursor->vc_done + inc;
	if (target_idx > max_idx_in_level)
		goto end;
	else if (target_idx == cursor->vc_done)
		goto next;
	idx_in_level = pe->vp_idx + inc;
	while (d > 0 && idx_in_level >= pe->vp_width) {
		inc	   = inc_for_next_level(cursor, idx_in_level,
						  d);
		pe->vp_idx = inc_to_idx_xlate(cursor, idx_in_level, d);
		--pe;
		--d;
		idx_in_level  = pe->vp_idx + inc;
	}
	pe->vp_buf = buff_incr(cursor->vc_arr, d, pe->vp_buf,
			       inc);
	pe->vp_idx = idx_in_level;
	while (d < cursor->vc_depth) {
		buf = pe->vp_buf;
		++pe;
		++d;
		pe->vp_buf = *(void **)buf;
		pe->vp_buf = buff_incr(cursor->vc_arr, d, pe->vp_buf,
				       pe->vp_idx);
	}
	cursor->vc_done = target_idx;
	goto next;
next:
	return 1;
end:
	return 0;

}

M0_INTERNAL uint64_t max_idx_within_level(const struct m0_varr_cursor *cursor,
					  uint32_t depth)
{
	uint64_t shift;

	shift = depth == cursor->vc_arr->va_depth ? 0 :
		cursor->vc_arr->va_buf_shift - cursor->vc_arr->va_obj_shift +
		(cursor->vc_arr->va_depth - depth - 1) *
		cursor->vc_arr->va_bufptr_nr_shift;
	return safe_bitshift(cursor->vc_arr->va_nr - 1, shift, >>);
}

M0_INTERNAL uint32_t inc_to_idx_xlate(const struct m0_varr_cursor *cursor,
					uint64_t carry, uint32_t depth)
{
	M0_PRE(cursor != NULL);
	M0_PRE(depth <= cursor->vc_arr->va_depth);
	return carry & (cursor->vc_path[depth].vp_width - 1);
}

M0_INTERNAL uint64_t inc_for_next_level(const struct m0_varr_cursor *cursor,
					  uint64_t carry, uint32_t depth)
{
	M0_PRE(cursor != NULL);
	M0_PRE(depth <= cursor->vc_arr->va_depth);
	return safe_bitshift(carry, log_radix(cursor->vc_arr, depth), >>);
}

M0_INTERNAL uint8_t log_radix(const struct m0_varr *arr, uint32_t level)
{
	M0_PRE(arr != NULL);
	M0_PRE(level <= arr->va_depth);

	if (level <= 1)
		return level == 1 ? M0_VA_TNODE_NR_SHIFT : 0;
	else
		return level == arr->va_depth ?
			arr->va_buf_shift - arr->va_obj_shift :
			arr->va_bufptr_nr_shift;
}

M0_INTERNAL uint32_t index_within_level(const struct m0_varr *arr,
					uint64_t target_idx, uint32_t depth)
{
	uint64_t shift;
	uint64_t mask_bits;

	M0_PRE(arr != NULL);
	M0_PRE(depth <= arr->va_depth);

	shift = depth == arr->va_depth ? 0 :
		arr->va_buf_shift - arr->va_obj_shift +
		(arr->va_depth - depth - 1) * arr->va_bufptr_nr_shift;
	mask_bits = depth == arr->va_depth ?
		arr->va_buf_shift - arr->va_obj_shift :
		depth == 1 ? M0_VA_TNODE_NR_SHIFT : arr->va_bufptr_nr_shift;
	target_idx  = safe_bitshift(target_idx, shift, >>);
	target_idx &= last_nbits_set(mask_bits);
	return target_idx;
}

M0_INTERNAL uint64_t last_nbits_set(uint8_t n)
{
	M0_PRE(n <= 64);
	return n < 64 ? ~safe_bitshift(~(uint64_t) 0, n, <<) :
		~(uint64_t) 0;
}

M0_INTERNAL void *buff_incr(const struct m0_varr *arr, uint32_t depth,
			    void *buff, uint32_t inc)
{
	size_t inc_unit;

	M0_PRE(arr != NULL && buff != NULL);

	if (depth == arr->va_depth)
		inc_unit = arr->va_obj_size;
	else
		inc_unit = M0_VA_TNODEPTR_SIZE;
	buff += inc*inc_unit;
	return buff;
}

M0_INTERNAL void varr_buffers_dealloc(struct m0_varr *arr)
{
	struct m0_varr_cursor cursor;
	int		      rc;
	void		     *holder;
	int32_t		      i;
	uint32_t	      depth;

	depth = arr->va_failure_depth == 0 ? arr->va_depth :
		arr->va_failure_depth;

	for (i = depth - 1; i > 0; --i) {
		rc = m0_varr_cursor_init(&cursor, arr, i);
		M0_ASSERT(rc == 0);
		do {
			holder = *(void **)m0_varr_cursor_get(&cursor);
			/* This condition will fail when varr_buffers_alloc()
			 * has got terminated intermittently. */
			if ((void *)holder != NULL) {
				m0_varr_buf_free(holder, arr->va_bufsize);
			} else
				break;

		} while (m0_varr_cursor_next(&cursor));
	}
}

M0_INTERNAL void m0_varr_fini(struct m0_varr *arr)
{
	M0_PRE(arr != NULL);
	M0_PRE_EX(varr_invariant(arr));

	varr_buffers_dealloc(arr);
	m0_free(arr->va_cache);
	m0_varr_bob_fini(arr);
	arr->va_nr     = arr->va_bufsize = 0;
	arr->va_depth  = 0;
}

M0_INTERNAL bool varr_invariant(const struct m0_varr *arr)
{
	return  m0_varr_bob_check(arr) &&
		arr->va_nr > 0         &&
		arr->va_buf_shift >= arr->va_obj_shift;
}

M0_INTERNAL void *m0_varr_ele_get(struct m0_varr *arr, uint64_t index)
{
	uint32_t  level;
	void	 *holder;

	M0_PRE(arr != NULL);
	M0_PRE(index < arr->va_nr);

	holder = cache_fetch(arr, index);
	if (holder != NULL)
		goto end;
	holder = (void *)arr->va_tree;
	for (level = 1; level < arr->va_depth; ++level) {

		holder =  buff_incr(arr, level, holder,
				    index_within_level((const struct m0_varr *)
						       arr, index, level));
		/* Dereferences the buffer pointer at given offset. */
		holder = *(void **)holder;
		M0_ASSERT(holder != NULL);
	}
	cache_update(arr, holder, index);
	M0_POST_EX(varr_invariant(arr));
end:
	/* Adds to holder the index of required object within a buffer */
	return buff_incr(arr, arr->va_depth, holder,
			 index_within_level((const struct m0_varr *) arr,
					    index, arr->va_depth));
}

M0_INTERNAL void *cache_fetch(const struct m0_varr *arr, uint64_t index)
{
	struct varr_cache *cache;

	M0_PRE(arr != NULL);
	cache = arr->va_cache;
	return cache->vc_buff != NULL		&&
		cache->vc_first_index <= index	&&
		index <= cache->vc_last_index	?
		cache->vc_buff : NULL;
}

M0_INTERNAL void cache_update(struct m0_varr *arr, void *holder,
			      uint64_t index)
{
	uint64_t	   index_in_level;
	struct varr_cache *cache;

	M0_PRE(arr != NULL);
	M0_PRE(index < arr->va_nr);

	cache = arr->va_cache;
	index_in_level = index_within_level((const struct m0_varr *)arr,
					    index, arr->va_depth);
	cache->vc_buff = holder;
	cache->vc_first_index = index - index_in_level;
	cache->vc_last_index = min64u(cache->vc_first_index +
				      varr_obj_nr_in_buff((const struct
							   m0_varr *) arr) -
				      1, arr->va_nr - 1);
}

M0_INTERNAL uint64_t m0_varr_size(const struct m0_varr *arr)
{
	M0_PRE(arr != NULL);
	return arr->va_nr;
}

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
