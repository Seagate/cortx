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
 * Original author: Alexey Lyashkov <Alexey_Lyashkov@xyratex.com>,
 *                  Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 04/17/2010
 */

#pragma once

#ifndef __MERO_LIB_LIST_H__
#define __MERO_LIB_LIST_H__

#include "lib/misc.h"  /* offsetof */

/**
   @defgroup list Double-linked list.

   @{
 */

/**
   List entry.
 */
struct m0_list_link {
	/**
	 * Next entry in the list
	 */
	struct m0_list_link *ll_next;
	/**
	 * Previous entry in the list
	 */
	struct m0_list_link *ll_prev;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
 initialize list link entry

 It is not necessary to call this function if the first operation on the link is
 any of m0_list_add*() functions.

 @param link - pointer to link enty
*/
M0_INTERNAL void m0_list_link_init(struct m0_list_link *link);

/**
 free resources associated with link entry

 @param link - pointer to link enty
*/
M0_INTERNAL void m0_list_link_fini(struct m0_list_link *link);

M0_INTERNAL bool m0_list_link_invariant(const struct m0_list_link *link);

/**
   List head.
 */
struct m0_list {
	/**
	 * Pointer to the first entry in the list.
	 */
	struct m0_list_link *l_head;
	/**
	 * Pointer to the last entry in the list.
	 */
	struct m0_list_link *l_tail;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/*
   It is necessary that m0_list and m0_list_link structures have exactly the
   same layout as.
 */

M0_BASSERT(offsetof(struct m0_list, l_head) ==
	   offsetof(struct m0_list_link, ll_next));
M0_BASSERT(offsetof(struct m0_list, l_tail) ==
	   offsetof(struct m0_list_link, ll_prev));

/**
   Initializes list head.
 */
M0_INTERNAL void m0_list_init(struct m0_list *head);

/**
   Finalizes the list.
 */
M0_INTERNAL void m0_list_fini(struct m0_list *head);

/**
 check list is empty

 @param head pointer to list head
 */
M0_INTERNAL bool m0_list_is_empty(const struct m0_list *head);

/**
   Returns true iff @link is in @list.
 */
M0_INTERNAL bool m0_list_contains(const struct m0_list *list,
				  const struct m0_list_link *link);

/**
 This function iterate over the argument list checking that double-linked
 list invariant holds (x->ll_prev->ll_next == x && x->ll_next->ll_prev == x).

 @return true iff @list isn't corrupted
*/
M0_INTERNAL bool m0_list_invariant(const struct m0_list *list);

M0_INTERNAL size_t m0_list_length(const struct m0_list *list);

/**
 add list to top on the list

 This function can be called on an uninitialised @next link. All @next fields are
 overwritten.

 @param head pointer to list head
 @param next  pointer to list entry

 */
M0_INTERNAL void m0_list_add(struct m0_list *head, struct m0_list_link *next);

/**
 add list to tail on the list

 This function can be called on an uninitialised @next link. All @next fields are
 overwritten.

 @param head pointer to list head
 @param next  pointer to list entry
 */
M0_INTERNAL void m0_list_add_tail(struct m0_list *head,
				  struct m0_list_link *next);

/**
   Adds an element to the list right after the specified element.

   This function can be called on an uninitialised @next link. All @next fields
   are overwritten.
 */
M0_INTERNAL void m0_list_add_after(struct m0_list_link *anchor,
				   struct m0_list_link *next);

/**
   Adds an element to the list right before the specified element.

   This function can be called on an uninitialised @next link. All @next fields
   are overwritten.
 */
M0_INTERNAL void m0_list_add_before(struct m0_list_link *anchor,
				    struct m0_list_link *next);

/**
   Deletes an entry from the list and re-initializes the entry.
 */
M0_INTERNAL void m0_list_del(struct m0_list_link *old);

/**
   Moves an entry to head of the list.
 */
M0_INTERNAL void m0_list_move(struct m0_list *head, struct m0_list_link *next);

/**
   Moves an entry to tail of the list.
 */
M0_INTERNAL void m0_list_move_tail(struct m0_list *head,
				   struct m0_list_link *next);

/**
 * return first entry from the list
 *
 * @param head pointer to list head
 *
 * @return pointer to first list entry or NULL if list empty
 */
static inline struct m0_list_link *m0_list_first(const struct m0_list *head)
{
	return head->l_head !=
		(struct m0_list_link *)head ?
		head->l_head : (struct m0_list_link *)NULL;
}


/**
 is link entry connected to the list

 @param link - pointer to link entry

 @retval true - entry connected to a list
 @retval false - entry disconnected from a list
*/
M0_INTERNAL bool m0_list_link_is_in(const struct m0_list_link *link);

M0_INTERNAL bool m0_list_link_is_last(const struct m0_list_link *link,
				      const struct m0_list *head);

M0_INTERNAL size_t m0_list_length(const struct m0_list *list);

/**
 * get pointer to object from pointer to list link entry
 */
#define m0_list_entry(link, type, member) \
	container_of(link, type, member)

/**
 * Iterates over a list
 *
 * @param head	the head of list.
 * @param pos	the pointer to list_link to use as a loop counter.
 */
#define m0_list_for_each(head, pos) \
	for (pos = (head)->l_head; pos != (void *)(head); \
	     pos = (pos)->ll_next)

/**
   Read-only iterates over a "typed" list.

   The loop body is not allowed to modify the list.
 */
#define m0_list_for_each_entry(head, pos, type, member) \
	for (pos = m0_list_entry((head)->l_head, type, member); \
	     &(pos->member) != (void *)head; \
	     pos = m0_list_entry((pos)->member.ll_next, type, member))

/**
   Iterates over a "typed" list safely: the loop body is allowed to remove the
   current element.

   @param head	the head of list.
   @param pos	the pointer to list_link to use as a loop counter.
 */
#define m0_list_for_each_entry_safe(head, pos, next, type, member)	\
	for (pos = m0_list_entry((head)->l_head, type, member),		\
	     next = m0_list_entry((pos)->member.ll_next, type, member); \
	     &(pos)->member != (void *)head;				\
	     pos = next,						\
	     next = m0_list_entry((next)->member.ll_next, type, member))

/**
 * Returns a conjunction (logical AND) of an expression evaluated for each list
 * element.
 *
 * Declares a struct m0_list_link pointer variable named "var" in a new scope
 * and evaluates user-supplied expression (the last argument) with "var"
 * iterated over successive list elements, while this expression returns
 * true. Returns true iff the whole list was iterated over.
 *
 * The list can not be modified by the user-supplied expression.
 *
 * This function is useful for invariant checking.
 *
 * @code
 * bool foo_invariant(const struct foo *f)
 * {
 *         return m0_list_forall(bar, &f->f_bars,
 *                               bar_is_valid(container_of(bar, struct bar,
 *                                            b_linkage)));
 * }
 * @endcode
 *
 * @see m0_tlist_forall(), m0_tl_forall(),
 * @see m0_forall(), m0_list_entry_forall().
 */
#define m0_list_forall(var, head, ...)		\
({						\
	struct m0_list_link *var;		\
						\
	m0_list_for_each(head, var) {		\
		if (!({ __VA_ARGS__ ; }))	\
		    break;			\
	}					\
	var == (void *)head;			\
})

/**
 * Returns a conjunction (logical AND) of an expression evaluated for each list
 * element.
 *
 * Declares a pointer variable named "var", in a new scope and evaluates
 * user-supplied expression (the last argument) with "var" iterated over
 * successive list elements, while this expression returns true. Returns true
 * iff the whole list was iterated over.
 *
 * The list can be modified by the user-supplied expression.
 *
 * This function is useful for invariant checking.
 *
 * @code
 * bool foo_invariant(const struct foo *f)
 * {
 *         return m0_list_entry_forall(b, &f->f_bars, struct bar, b_linkage,
 *                                     b->b_count > 0 && b->b_parent == f);
 * }
 * @endcode
 *
 * @see m0_tlist_forall(), m0_tl_forall(), m0_list_forall(), m0_forall().
 */
#define m0_list_entry_forall(var, head, type, member, ...)		\
({									\
	type *var;							\
	type *next;							\
									\
	m0_list_for_each_entry_safe(head, var, next, type, member) {	\
		if (!({ __VA_ARGS__ ; }))				\
		    break;						\
	}								\
	&var->member == (void *)head;					\
})

/** @} end of list group */
#endif /* __MERO_LIB_LIST_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
