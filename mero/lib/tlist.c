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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 08/26/2011
 */

#include "lib/tlist.h"

/**
   @addtogroup tlist
   @{
 */

/**
   Returns the address of a link embedded in an ambient object.
 */
static struct m0_list_link *__link(const struct m0_tl_descr *d,
				   const void *obj);

/**
   Returns the value of the magic field in an ambient object
 */
static uint64_t magic(const struct m0_tl_descr *d, const void *obj);

/**
   Casts a link to its ambient object.
 */
static void *amb(const struct m0_tl_descr *d, struct m0_list_link *link);

M0_INTERNAL void m0_tlist_init(const struct m0_tl_descr *d, struct m0_tl *list)
{
	list->t_magic = d->td_head_magic;
	m0_list_init(&list->t_head);
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));
}

M0_INTERNAL void m0_tlist_fini(const struct m0_tl_descr *d, struct m0_tl *list)
{
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));
	m0_list_fini(&list->t_head);
	/*
	 * We don't unset the magic field (list->t_magic), because it can be
	 * shared by multiple tlinks embedded in the same ambient object.
	 */
}

M0_INTERNAL void m0_tlink_init(const struct m0_tl_descr *d, void *obj)
{
	m0_list_link_init(__link(d, obj));
	if (d->td_link_magic != 0)
		*(uint64_t *)(obj + d->td_link_magic_offset) = d->td_link_magic;
	M0_INVARIANT_EX(m0_tlink_invariant(d, obj));
}

M0_INTERNAL void m0_tlink_init_at(const struct m0_tl_descr *d, void *obj,
				  struct m0_tl *list)
{
	m0_tlink_init(d, obj);
	m0_tlist_add(d, list, obj);
}

M0_INTERNAL void m0_tlink_init_at_tail(const struct m0_tl_descr *d, void *obj,
				       struct m0_tl *list)
{
	m0_tlink_init(d, obj);
	m0_tlist_add_tail(d, list, obj);
}

M0_INTERNAL void m0_tlink_fini(const struct m0_tl_descr *d, void *obj)
{
	m0_list_link_fini(__link(d, obj));
}

M0_INTERNAL void m0_tlink_del_fini(const struct m0_tl_descr *d, void *obj)
{
	m0_tlist_del(d, obj);
	m0_tlink_fini(d, obj);
}

M0_INTERNAL bool m0_tlist_is_empty(const struct m0_tl_descr *d,
				   const struct m0_tl *list)
{
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));
	return m0_list_is_empty(&list->t_head);
}

M0_INTERNAL bool m0_tlink_is_in(const struct m0_tl_descr *d, const void *obj)
{
	M0_INVARIANT_EX(m0_tlink_invariant(d, obj));
	return m0_list_link_is_in(__link(d, obj));
}

M0_INTERNAL bool m0_tlist_contains(const struct m0_tl_descr *d,
				   const struct m0_tl *list, const void *obj)
{
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));
	M0_INVARIANT_EX(m0_tlink_invariant(d, obj));
	return m0_list_contains(&list->t_head, __link(d, obj));
}

M0_INTERNAL size_t m0_tlist_length(const struct m0_tl_descr *d,
				   const struct m0_tl *list)
{
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));
	return m0_list_length(&list->t_head);
}
M0_EXPORTED(m0_tlist_length);

M0_INTERNAL void m0_tlist_add(const struct m0_tl_descr *d, struct m0_tl *list,
			      void *obj)
{
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));
	M0_PRE(!m0_tlink_is_in(d, obj));
	m0_list_add(&list->t_head, __link(d, obj));
}

M0_INTERNAL void m0_tlist_add_tail(const struct m0_tl_descr *d,
				   struct m0_tl *list, void *obj)
{
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));
	M0_PRE(!m0_tlink_is_in(d, obj));
	m0_list_add_tail(&list->t_head, __link(d, obj));
}

M0_INTERNAL void m0_tlist_add_after(const struct m0_tl_descr *d, void *obj,
				    void *new)
{
	M0_PRE(m0_tlink_is_in(d, obj));
	M0_PRE(!m0_tlink_is_in(d, new));
	m0_list_add_after(__link(d, obj), __link(d, new));
}

M0_INTERNAL void m0_tlist_add_before(const struct m0_tl_descr *d, void *obj,
				     void *new)
{
	M0_PRE(m0_tlink_is_in(d, obj));
	M0_PRE(!m0_tlink_is_in(d, new));
	m0_list_add_before(__link(d, obj), __link(d, new));
}

M0_INTERNAL void m0_tlist_del(const struct m0_tl_descr *d, void *obj)
{
	M0_PRE(m0_tlink_is_in(d, obj));
	m0_tlist_remove(d, obj);
}

M0_INTERNAL void m0_tlist_remove(const struct m0_tl_descr *d, void *obj)
{
	M0_INVARIANT_EX(m0_tlink_invariant(d, obj));
	m0_list_del(__link(d, obj));
	M0_PRE(!m0_tlink_is_in(d, obj));
}

M0_INTERNAL void m0_tlist_move(const struct m0_tl_descr *d, struct m0_tl *list,
			       void *obj)
{
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));
	M0_PRE(m0_tlink_is_in(d, obj));

	m0_list_move(&list->t_head, __link(d, obj));
}

M0_INTERNAL void m0_tlist_move_tail(const struct m0_tl_descr *d,
				    struct m0_tl *list, void *obj)
{
	M0_INVARIANT_EX(m0_tlist_invariant(d, list));

	m0_list_move_tail(&list->t_head, __link(d, obj));
}

void *m0_tlist_head(const struct m0_tl_descr *d, const struct m0_tl *list)
{
	const struct m0_list *head;

	M0_INVARIANT_EX(m0_tlist_invariant(d, list));

	head = &list->t_head;
	return m0_list_is_empty(head) ? NULL : amb(d, head->l_head);
}

M0_INTERNAL void *m0_tlist_pop(const struct m0_tl_descr *d,
			       const struct m0_tl *list)
{
	void *head = m0_tlist_head(d, list);

	if (head != NULL)
		m0_tlist_del(d, head);
	return head;
}

M0_INTERNAL void *m0_tlist_tail(const struct m0_tl_descr *d,
				const struct m0_tl *list)
{
	const struct m0_list *head;

	M0_INVARIANT_EX(m0_tlist_invariant(d, list));

	head = &list->t_head;
	return head->l_tail != (void *)head ? amb(d, head->l_tail) : NULL;
}

void *m0_tlist_next(const struct m0_tl_descr *d,
		    const struct m0_tl *list, const void *obj)
{
	struct m0_list_link *next;

	next = __link(d, obj)->ll_next;
	return (void *)next != &list->t_head ? amb(d, next) : NULL;
}

M0_INTERNAL void *m0_tlist_prev(const struct m0_tl_descr *d,
				const struct m0_tl *list, const void *obj)
{
	struct m0_list_link *prev;

	prev = __link(d, obj)->ll_prev;
	return (void *)prev != &list->t_head ? amb(d, prev) : NULL;
}

M0_INTERNAL bool m0_tlist_invariant(const struct m0_tl_descr *d,
				    const struct m0_tl *list)
{
	const struct m0_list_link *head;
	struct m0_list_link       *scan;

	head = (void *)&list->t_head;

	if (list->t_magic != d->td_head_magic)
		return false;
	if ((list->t_head.l_head == head) != (list->t_head.l_tail == head))
		return false;

	for (scan = list->t_head.l_head; scan != head; scan = scan->ll_next) {
		if (scan->ll_next->ll_prev != scan ||
		    scan->ll_prev->ll_next != scan)
			return false;
		if (!M0_CHECK_EX(m0_tlink_invariant(d, amb(d, scan))))
			return false;
	}
	return true;
}

M0_INTERNAL bool m0_tlist_invariant_ext(const struct m0_tl_descr *d,
					const struct m0_tl *list,
					bool (*check)(const void *, void *),
					void *datum)
{
	if (m0_tlist_invariant(d, list)) {
		void *obj;

		m0_tlist_for(d, (struct m0_tl *)list, obj) {
			if (!check(obj, datum))
				return false;
		} m0_tlist_endfor;
	}
	return true;
}

M0_INTERNAL bool m0_tlink_invariant(const struct m0_tl_descr *d,
				    const void *obj)
{
	return d->td_link_magic == 0 || magic(d, obj) == d->td_link_magic;
}

static struct m0_list_link *__link(const struct m0_tl_descr *d, const void *obj)
{
	return &((struct m0_tlink *)(obj + d->td_link_offset))->t_link;
}

static uint64_t magic(const struct m0_tl_descr *d, const void *obj)
{
	return *(uint64_t *)(obj + d->td_link_magic_offset);
}

static void *amb(const struct m0_tl_descr *d, struct m0_list_link *link)
{
	return (void *)container_of(link, struct m0_tlink,
				    t_link) - d->td_link_offset;
}

/** @} end of tlist group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
