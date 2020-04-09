/*
 * COPYRIGHT 2011, 2017 XYRATEX TECHNOLOGY LIMITED
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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"
#include "lib/assert.h"
#include "lib/list.h"

/** @addtogroup list @{ */

M0_INTERNAL void m0_list_init(struct m0_list *head)
{
	head->l_head = (struct m0_list_link *)head;
	head->l_tail = (struct m0_list_link *)head;
}
M0_EXPORTED(m0_list_init);

M0_INTERNAL void m0_list_fini(struct m0_list *head)
{
	M0_ASSERT(m0_list_is_empty(head));
}
M0_EXPORTED(m0_list_fini);

M0_INTERNAL bool m0_list_is_empty(const struct m0_list *head)
{
	return head->l_head == (void *)head;
}
M0_EXPORTED(m0_list_is_empty);

M0_INTERNAL bool m0_list_link_invariant(const struct m0_list_link *link)
{
	struct m0_list_link *scan;

	if ((link->ll_next == link) != (link->ll_prev == link)) {
		M0_LOG(M0_FATAL, "%p <- %p -> %p",
		       link->ll_prev, link, link->ll_next);
		return false;
	}

	for (scan = link->ll_next; scan != link; scan = scan->ll_next) {
		if (scan->ll_next->ll_prev != scan ||
		    scan->ll_prev->ll_next != scan) {
			M0_LOG(M0_FATAL, "%p -> %p <- %p -> %p <- %p",
			       scan->ll_prev->ll_next, scan->ll_prev,
			       scan, scan->ll_next, scan->ll_next->ll_prev);
			return false;
		}
	}
	return true;
}

M0_INTERNAL bool m0_list_invariant(const struct m0_list *head)
{
	return m0_list_link_invariant((void *)head);
}

M0_INTERNAL size_t m0_list_length(const struct m0_list *list)
{
	size_t               length;
	struct m0_list_link *scan;

	M0_ASSERT(m0_list_invariant(list));
	length = 0;
	for (scan = list->l_head; scan != (void *)list; scan = scan->ll_next)
		length++;
	return length;
}

M0_INTERNAL bool m0_list_contains(const struct m0_list *list,
				  const struct m0_list_link *link)
{
	struct m0_list_link *scan;

	M0_ASSERT(m0_list_invariant(list));
	for (scan = list->l_head; scan != (void *)list; scan = scan->ll_next)
		if (scan == link)
			return true;
	return false;
}

static inline void __m0_list_add(struct m0_list_link *prev,
				 struct m0_list_link *next,
				 struct m0_list_link *new)
{
	M0_ASSERT(prev->ll_next == next && next->ll_prev == prev);
	M0_ASSERT_EX(m0_list_link_invariant(next));
	new->ll_next = next;
	new->ll_prev = prev;

	next->ll_prev = new;
	prev->ll_next = new;
	M0_ASSERT_EX(m0_list_link_invariant(next));
}

M0_INTERNAL void m0_list_add(struct m0_list *head, struct m0_list_link *new)
{
	__m0_list_add((void *)head, head->l_head, new);
}
M0_EXPORTED(m0_list_add);

M0_INTERNAL void m0_list_add_tail(struct m0_list *head,
				  struct m0_list_link *new)
{
	__m0_list_add(head->l_tail, (void *)head, new);
}
M0_EXPORTED(m0_list_add_tail);

M0_INTERNAL void m0_list_add_after(struct m0_list_link *anchor,
				   struct m0_list_link *new)
{
	__m0_list_add(anchor, anchor->ll_next, new);
}
M0_EXPORTED(m0_list_add_after);

M0_INTERNAL void m0_list_add_before(struct m0_list_link *anchor,
				    struct m0_list_link *new)
{
	__m0_list_add(anchor->ll_prev, anchor, new);
}
M0_EXPORTED(m0_list_add_before);

static void __m0_list_del(struct m0_list_link *old)
{
	M0_ASSERT_EX(m0_list_link_invariant(old));
	old->ll_prev->ll_next = old->ll_next;
	old->ll_next->ll_prev = old->ll_prev;
}

M0_INTERNAL void m0_list_del(struct m0_list_link *old)
{
	__m0_list_del(old);
	m0_list_link_init(old);
}
M0_EXPORTED(m0_list_del);

M0_INTERNAL void m0_list_move(struct m0_list *head, struct m0_list_link *old)
{
	__m0_list_del(old);
	m0_list_add(head, old);
	M0_ASSERT(m0_list_invariant(head));
}

M0_INTERNAL void m0_list_move_tail(struct m0_list *head,
				   struct m0_list_link *old)
{
	__m0_list_del(old);
	m0_list_add_tail(head, old);
	M0_ASSERT(m0_list_invariant(head));
}

M0_INTERNAL void m0_list_link_init(struct m0_list_link *link)
{
	link->ll_prev = link;
	link->ll_next = link;
}
M0_EXPORTED(m0_list_link_init);

M0_INTERNAL void m0_list_link_fini(struct m0_list_link *link)
{
	M0_ASSERT(!m0_list_link_is_in(link));
}

M0_INTERNAL bool m0_list_link_is_in(const struct m0_list_link *link)
{
	return link->ll_prev != link;
}

M0_INTERNAL bool m0_list_link_is_last(const struct m0_list_link *link,
				      const struct m0_list *head)
{
	return link->ll_next == (void *)head;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of list group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
