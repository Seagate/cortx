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

#pragma once

#ifndef __MERO_LIB_VIRTUAL_ARRAY_H__
#define __MERO_LIB_VIRTUAL_ARRAY_H__

#include "lib/assert.h"
#include "lib/misc.h"

/**
 * @defgroup varr Virtual array
 * A virtual array represents a data structure which can be used at places
 * where big contiguous memory allocations are not possible.
 *
 * Virtual array can be used for big arrays or pointers which would potentially
 * hold large memory areas. Virtual array library provides interface for
 * read, write, and iterate over an array.
 *
 * Using such structures can be especially helpful in kernel where contiguity
 * of pages is not guaranteed, and any memory allocation growing beyond page
 * size can essentially fail. Current implementation supports both user-space
 * as well as kernel versions.
 *
 * The structure of virtual array is kept something similar to a block map
 * from an on-disk inode with multiple indirections.
 *
 * Using pointer arithmetic on virtual array is strongly discouraged since
 * it does not guarantee contiguity of buffers.
 *
 * A virtual array uses a radix-tree like structure whose height is
 * dependent on number of objects to be accommodated in the array.
 *
 * Following example illustrates the usage of various interfaces
 * that are provided by virtual array library.
 *
 * Consider an array consisting of OBJ_NR number of objects, of the type
 * unsigned long. Library leaves it to its user what shall be the size of a
 * buffer holding objects. Following example uses M0_0VEC_ALIGN as buffer size.
 *
 * @code
 *
 * struct m0_varr varr;
 *
 * enum {
 *         OBJ_NR = 10203040,
 * };
 *
 * rc = m0_varr_init(&varr, OBJ_NR, sizeof(unsigned long), M0_0VEC_ALIGN);
 *
 * m0_varr_iter(&varr, unsigned long, id, obj, 0, m0_varr_size(&varr), 1) {
 *	*obj = id;
 * } m0_varr_enditer;
 *
 * m0_varr_iter(&varr, unsigned long, id, obj, 0, m0_varr_size(&varr), 1) {
 *         ptr = m0_varr_ele_get(&varr, id);
 *         M0_ASSERT(*ptr == *obj);
 * } m0_varr_enditer;
 *
 * m0_varr_fini(&varr);
 *
 * @endcode
 * @{
 */

enum m0_varr_tree_char {
	/** Number of nodes which originate from root of radix tree. */
	M0_VA_TNODE_NR	     = 64,
	/** Size of pointer to a tree node. */
	M0_VA_TNODEPTR_SIZE  = sizeof(void *),
	/** Log (M0_VA_TNODE_NR) to base 2. */
	M0_VA_TNODE_NR_SHIFT = 6,
	/** Maximum allowable depth of a tree. */
	M0_VA_DEPTH_MAX	     = 16,
};
M0_BASSERT(M0_VA_TNODE_NR == M0_BITS(M0_VA_TNODE_NR_SHIFT));

/**
 * An object that holds address of a node, and its index within a buffer of
 * width vp_width.
 */
struct m0_varr_path_element {
	uint32_t vp_idx;
	uint32_t vp_width;
	void	*vp_buf;
};

struct m0_varr_cursor {
	struct m0_varr		   *vc_arr;
	uint32_t		    vc_depth;
	/** Number of leaf level nodes behind cursor's current position. */
	uint64_t		    vc_done;
	/**
	 * Holds addresses of those nodes which form a path between the root
	 * node and current cursor position. Address of a node at level 'i'
	 * on a path is stored in vc_path[i].
	 */
	struct m0_varr_path_element vc_path[M0_VA_DEPTH_MAX];
};

struct m0_varr {
	/** Number of elements in array. */
	uint64_t           va_nr;
	/** Number of leaf buffers. */
	uint64_t	   va_buff_nr;
	size_t		   va_obj_size;
	/** Log of object-size to the base two. */
	uint8_t            va_obj_shift;
	/**
	 * Size of buffer which is used to store objects and buffer-pointers
	 * from a tree.
	 */
	size_t		   va_bufsize;
	/** Log of va_bufsize to the base two. */
	uint8_t		   va_buf_shift;

	/** Depth of tree proportional to number of objects stored. */
	uint32_t	   va_depth;

	/**
	 * Number of pointers that can be accommodated in one
	 * meta buffer. This number is easy to calculate and need not be stored
	 * as a member of structure. However, during tree traversal, this
	 * number is calculated multiple times in each trail, owing to
	 * significant and _exactly same_ compute operations which can be
	 * easily avoided by maintaining it as a member.
	 */
	uint64_t	   va_bufptr_nr;
	uint8_t		   va_bufptr_nr_shift;

	/**
	 * Array of radix tree nodes, each of which represents an abstraction
	 * of buffer containing multitude of objects.
	 * The arrangement is such that there could be n levels within any
	 * tree node before a leaf node is reached.
	 */
	void		  *va_tree[M0_VA_TNODE_NR];
	/** Holds address of a buffer holding recently accessed object. */
	struct varr_cache *va_cache;
	/** Holds the cursor depth in case of a failure. */
	uint32_t	   va_failure_depth;
	/** Magic field to cross check sanity of structure. */
	uint64_t	   va_magic;
};

/**
 * Initialises a virtual array.
 * @param  nr[in]      Length of array.
 * @param  size[in]    Size of object to be stored in array.
 * @param  bufsize[in] Size of each buffer which stores the objects.
 * @retval 0	       On success.
 * @retval -ENOMEM     On failure.
 * @pre   arr != NULL && nr > 0.
 * @post  varr_invariant(arr).
 */
M0_INTERNAL int m0_varr_init(struct m0_varr *arr, uint64_t nr, size_t size,
			     size_t bufsize);

/**
 * Finalises a virtual array.
 * @pre varr_invariant(arr)
 */
M0_INTERNAL void m0_varr_fini(struct m0_varr *arr);

/**
 * Returns address of an object having index as 'index'. Updates an internal
 * cache if required. Since concurrent access to the cache may result in
 * spurious outcome, calls to m0_varr_ele_get() should be protected under a
 * lock.
 * @pre  arr != NULL && index < arr->va_nr.
 * @post varr_invariant(arr).
 */
M0_INTERNAL void * m0_varr_ele_get(struct m0_varr *arr, uint64_t index);

/** Returns the number of elements stored in an array. */
M0_INTERNAL uint64_t m0_varr_size(const struct m0_varr *arr);

/**
 * Initializes a cursor to the address of the first node at given depth.
 * @param arr    [in]    An array to which a cursor gets associated.
 * @param depth  [in]    Depth to which cursor is initialized.
 * @param cursor [out]
 * @retval	  0      On success.
 * @retval	 -EINVAL On failure.
 * @pre	arr != NULL
 * @pre depth <= arr->va_depth
 */
M0_INTERNAL int m0_varr_cursor_init(struct m0_varr_cursor *cursor,
				    const struct m0_varr *arr, uint32_t depth);
/**
 * Returns a pointer corresponding to the current location of a cursor.
 * @pre	m0_varr_cursor_init()
 */
M0_INTERNAL void *m0_varr_cursor_get(struct m0_varr_cursor *cursor);
/**
 * Moves cursor to the next node at the same level as m0_varr_cursor::vc_depth.
 * @retval 1 On success.
 * @retval 0 On completion of all nodes at level m0_varr_cursor::vc_depth.
 */
M0_INTERNAL int m0_varr_cursor_next(struct m0_varr_cursor *cursor);
/**
 * Moves the cursor location by 'inc', along the same level as
 * m0_varr_cursor::vc_depth.
 * @retval 1 On success.
 * @retval 0 On completion of all nodes at level m0_varr_cursor::vc_depth.
 * @pre	m0_varr_cursor_init()
 */
M0_INTERNAL int m0_varr_cursor_move(struct m0_varr_cursor *cursor,
				    uint64_t inc);
/**
 * Iterates over an arbitrary arithmetic progression of indices over
 * the range [start, end).
 */
#define m0_varr_iter(arr, type, idx, obj, start, end, inc)	       \
({								       \
	uint64_t	      idx   = (start);			       \
	uint64_t	      __end = (end);			       \
	uint64_t	      __inc = (inc);			       \
	struct m0_varr	     *__arr = (arr);			       \
	type		     *obj;				       \
	int		      __rc;				       \
	struct m0_varr_cursor __cursor;				       \
								       \
	M0_PRE(idx < __arr->va_nr && __end <= __arr->va_nr);	       \
	M0_PRE(ergo(__arr->va_obj_shift > 0,			       \
		    sizeof *obj > M0_BITS(__arr->va_obj_shift - 1) &&  \
	            sizeof *obj <= M0_BITS(__arr->va_obj_shift)));     \
								       \
        __rc = m0_varr_cursor_init(&__cursor, __arr, __arr->va_depth); \
	M0_ASSERT(__rc == 0);					       \
	m0_varr_cursor_move(&__cursor, idx);			       \
	for (obj = m0_varr_cursor_get(&__cursor); idx < __end;	       \
		idx += __inc, m0_varr_cursor_move(&__cursor, __inc),   \
		obj = m0_varr_cursor_get(&__cursor)) {		       \

#define m0_varr_enditer } } )

/** Iterates over whole virtual array. */
#define m0_varr_for(arr, type, idx, obj)				    \
({									    \
	struct m0_varr *__arr__ = (arr);				    \
	m0_varr_iter(__arr__, type, idx, obj, 0, m0_varr_size(__arr__), 1)

#define m0_varr_endfor m0_varr_enditer; })

/** @} end of varr group */
#endif /* __MERO_LIB_VIRTUAL_ARRAY_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
