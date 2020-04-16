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

#pragma once

#ifndef __MERO_LIB_TLIST_H__
#define __MERO_LIB_TLIST_H__

#include "lib/list.h"
#include "lib/list_xc.h"
#include "lib/types.h"                    /* uint64_t */

/**
   @defgroup tlist Typed lists.

   Typed list module provides a double-linked list implementation that
   eliminates some chores and sources of errors typical for the "raw" m0_list
   interface.

   Typed list is implemented on top of m0_list and adds the following features:

       - a "list descriptor" (m0_tl_descr) object holding information about this
         list type, including its human readable name;

       - "magic" numbers embedded in list header and list links and checked by
         the code to catch corruptions;

       - automatic conversion to and from list links and ambient objects they
         are embedded to, obviating the need in container_of() and
         m0_list_entry() calls. In fact, links (m0_tlink) are not mentioned in
         tlist interface at all;

       - gdb (7.0) pretty-printer for lists (not yet implemented).

   tlist is a safe and more convenient alternative to m0_list. As a general
   rule, m0_list should be used only when performance is critical or some
   flexibility beyond what tlist provides (e.g., a cyclic list without a head
   object) is necessary.

   Similarly to m0_list, tlist is a purely algorithmic module: it deals with
   neither concurrency nor liveness nor with any similar issues that its callers
   are supposed to handle.

   To describe a typical tlist usage pattern, suppose that one wants a list of
   objects of type foo hanging off every object of type bar.

   First, two things have to be done:

   - "list link" has to be embedded in foo:

     @code
     struct foo {
             ...
	     // linkage into a list of foo-s hanging off bar::b_list
	     struct m0_tlink f_linkage;
	     ...
     };
     @endcode

   - then, a "list head" has to be embedded in bar:

     @code
     struct bar {
             ...
	     // list of foo-s, linked through foo::f_linkage
	     struct m0_tl b_list;
	     ...
     };
     @endcode

   - now, define a tlist type:

     @code
     static const struct m0_tl_descr foobar_list = {
             .td_name        = "foo-s of bar",
	     .td_link_offset = offsetof(struct foo, f_linkage),
	     .td_head_magic  = 0x666f6f6261726865 // "foobarhe"
     };
     @endcode

   This defines the simplest form of tlist without magic checking in list links
   (the magic embedded in a list head is checked anyway). To add magic checking,
   place a magic field in foo:

   @code
   struct foo {
           ...
	   uint64_t f_magic;
	   ...
   };

   static const struct m0_tl_descr foobar_list = {
           ...
	   .td_link_magic_offset = offsetof(struct foo, f_magic),
	   .td_link_magic        = 0x666f6f6261726c69 // "foobarli"
   };
   @endcode

   Magic field can be shared by multiple tlist links embedded in the same object
   and can be used for other sanity checking. An "outermost" finaliser function
   must clear the magic as its last step to catch use-after-fini errors.

   Now, one can populate and manipulate foo-bar lists:

   @code
   struct bar  B;
   struct foo  F;
   struct foo *scan;

   m0_tlist_init(&B.b_list);
   m0_tlink_init(&F.f_linkage);

   m0_tlist_add(&foobar_list, &B.b_list, &F);
   M0_ASSERT(m0_tl_contains(&foobar_list, &B.b_list, &F));

   m0_tlist_for(&foobar_list, &B.b_list, scan)
           M0_ASSERT(scan == &F);
   m0_tlist_endfor;
   @endcode

   @note Differently from m0_list, tlist heads and links must be initialised
   before use, even when first usage overwrites the entity completely. This
   allows stronger checking in tlist manipulation functions.

   <b>Type-safe macros.</b>

   M0_TL_DESCR_DECLARE(), M0_TL_DECLARE(), M0_TL_DESCR_DEFINE() and
   M0_TL_DEFINE() macros generate versions of tlist interface tailored for a
   particular use case.

   4 separate macros are necessary for flexibility. They should be used in
   exactly one of the following ways for any given typed list:

       - static tlist, used in a single module only: M0_TL_DEFINE() and
         M0_TL_DESCR_DEFINE() with scope "static" in the module .c file;

       - tlist exported from a module: M0_TL_DEFINE() and M0_TL_DESCR_DEFINE()
         with scope "" in .c file and M0_TL_DESCR_DECLARE(), M0_TL_DECLARE()
         with scope "extern" in .h file;

       - tlist exported from a module as a collection of inline functions:
         M0_TL_DESCR_DEFINE() in .c file and M0_TL_DESCR_DECLARE() with scope
         "extern" followed by M0_TL_DEFINE() with scope "static inline" in .h
         file.

   Use m0_tl_for() and m0_tl_endfor() to iterate over lists generated by
   M0_TL_DECLARE() and M0_TL_DEFINE().

   @{
 */

struct m0_tl_descr;
struct m0_tl;
struct m0_tlink;

/**
   An instance of this type must be defined for each "tlist type", specifically
   for each link embedded in an ambient type.

   @verbatim
			      ambient object
                          +  +-----------+  +
     td_link_magic_offset |  |           |  |
                          v  |           |  |
                             |-----------|  |
                             |link magic |  | td_link_offset
                             |-----------|  |
                             |           |  |
        head                 |           |  |
    +->+----------+          |           |  |
    |  |head magic|          |           |  v
    |  |----------|          |-----------|
    |  |        +----------->|link     +------------> . . . ---+
    |  +----------+          |-----------|                     |
    |                        |           |                     |
    |                        |           |                     |
    |                        |           |                     |
    |                        |           |                     |
    |                        +-----------+                     |
    |                                                          |
    +----------------------------------------------------------+

   @endverbatim
 */
struct m0_tl_descr {
	/** Human-readable list name, used for error messages. */
	const char *td_name;
	/** Offset of list link (m0_tlink) in the ambient object. */
	int         td_link_offset;
	/**
	    Offset of magic field in the ambient object.
	    This is used only when link magic checking is on.

	    @see m0_tl_descr::td_link_magic
	 */
	int         td_link_magic_offset;
	/**
	    Magic stored in an ambient object.

	    If this field is 0, link magic checking is disabled.
	 */
	uint64_t    td_link_magic;
	/**
	    Magic stored in m0_tl::t_magic and checked on all tlist
	    operations.
	 */
	uint64_t    td_head_magic;
	/** Size of the ambient object. */
	size_t      td_container_size;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

#define M0_TL_DESCR(name, ambient_type, link_field, link_magic_field,	\
                    link_magic, head_magic)				\
{									\
	.td_name              = name,					\
	.td_link_offset       = offsetof(ambient_type, link_field),	\
	.td_link_magic_offset = offsetof(ambient_type, link_magic_field), \
	.td_link_magic        = link_magic,				\
	.td_head_magic        = head_magic,				\
	.td_container_size    = sizeof(ambient_type)			\
};									\
									\
M0_BASSERT(M0_HAS_TYPE(M0_FIELD_VALUE(ambient_type, link_field),	\
		       struct m0_tlink));				\
M0_BASSERT(M0_HAS_TYPE(M0_FIELD_VALUE(ambient_type, link_magic_field),	\
		       uint64_t))


/**
   tlist head.
 */
struct m0_tl {
	/**
	   Head magic. This is set to m0_tl::td_head_magic and verified by the
	   list invariant.
	 */
	uint64_t       t_magic;
	/** Underlying m0_list. */
	struct m0_list t_head;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
   tlist link.
 */
struct m0_tlink {
	/** Underlying m0_list link. */
	struct m0_list_link t_link;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

M0_INTERNAL void m0_tlist_init(const struct m0_tl_descr *d, struct m0_tl *list);
M0_INTERNAL void m0_tlist_fini(const struct m0_tl_descr *d, struct m0_tl *list);

M0_INTERNAL void m0_tlink_init(const struct m0_tl_descr *d, void *obj);
M0_INTERNAL void m0_tlink_fini(const struct m0_tl_descr *d, void *obj);
M0_INTERNAL void m0_tlink_init_at(const struct m0_tl_descr *d,
				  void *obj, struct m0_tl *list);
M0_INTERNAL void m0_tlink_init_at_tail(const struct m0_tl_descr *d,
				       void *obj, struct m0_tl *list);
M0_INTERNAL void m0_tlink_del_fini(const struct m0_tl_descr *d, void *obj);

M0_INTERNAL bool m0_tlist_invariant(const struct m0_tl_descr *d,
				    const struct m0_tl *list);
M0_INTERNAL bool m0_tlist_invariant_ext(const struct m0_tl_descr *d,
					const struct m0_tl *list,
					bool (*check)(const void *, void *),
					void *datum);
M0_INTERNAL bool m0_tlink_invariant(const struct m0_tl_descr *d,
				    const void *obj);

M0_INTERNAL bool m0_tlist_is_empty(const struct m0_tl_descr *d,
				   const struct m0_tl *list);
M0_INTERNAL bool m0_tlink_is_in(const struct m0_tl_descr *d, const void *obj);

M0_INTERNAL bool m0_tlist_contains(const struct m0_tl_descr *d,
				   const struct m0_tl *list, const void *obj);
M0_INTERNAL size_t m0_tlist_length(const struct m0_tl_descr *d,
				   const struct m0_tl *list);

/**
   Adds an element to the beginning of a list.

   @pre !m0_tlink_is_in(d, obj)
   @post m0_tlink_is_in(d, obj)
 */
M0_INTERNAL void m0_tlist_add(const struct m0_tl_descr *d, struct m0_tl *list,
			      void *obj);

/**
   Adds an element to the end of a list.

   @pre !m0_tlink_is_in(d, obj)
   @post m0_tlink_is_in(d, obj)
 */
M0_INTERNAL void m0_tlist_add_tail(const struct m0_tl_descr *d,
				   struct m0_tl *list, void *obj);

/**
   Adds an element after another element of the list.

   @pre !m0_tlink_is_in(d, next)
   @post m0_tlink_is_in(d, next)
 */
M0_INTERNAL void m0_tlist_add_after(const struct m0_tl_descr *d, void *obj,
				    void *next);

/**
   Adds an element before another element of the list.

   @pre !m0_tlink_is_in(d, next)
   @post m0_tlink_is_in(d, next)
 */
M0_INTERNAL void m0_tlist_add_before(const struct m0_tl_descr *d, void *obj,
				     void *next);

/**
   Deletes an element from the list.

   @pre   m0_tlink_is_in(d, obj)
   @post !m0_tlink_is_in(d, obj)
 */
M0_INTERNAL void m0_tlist_del(const struct m0_tl_descr *d, void *obj);

/**
   Deletes at element from the list, if it was there.

   @post !m0_tlink_is_in(d, obj)
 */
M0_INTERNAL void m0_tlist_remove(const struct m0_tl_descr *d, void *obj);

/**
   Moves an element from a list to the head of (possibly the same) list.

   @pre  m0_tlink_is_in(d, obj)
   @post m0_tlink_is_in(d, obj)
 */
M0_INTERNAL void m0_tlist_move(const struct m0_tl_descr *d, struct m0_tl *list,
			       void *obj);

/**
   Moves an element from a list to the tail of (possibly the same) list.

   @pre  m0_tlink_is_in(d, obj)
   @post m0_tlink_is_in(d, obj)
 */
M0_INTERNAL void m0_tlist_move_tail(const struct m0_tl_descr *d,
				    struct m0_tl *list, void *obj);
/**
   Returns the first element of a list or NULL if the list is empty.
 */
void *m0_tlist_head(const struct m0_tl_descr *d, const struct m0_tl *list);

/**
   Removes and returns the head the list or NULL.
 */
M0_INTERNAL void *m0_tlist_pop(const struct m0_tl_descr *d,
			       const struct m0_tl *list);

/**
   Returns the last element of a list or NULL if the list is empty.
 */
M0_INTERNAL void *m0_tlist_tail(const struct m0_tl_descr *d,
				const struct m0_tl *list);

/**
   Returns the next element of a list or NULL if @obj is the last element.

   @pre m0_tlist_contains(d, list, obj)
 */
void *m0_tlist_next(const struct m0_tl_descr *d,
		    const struct m0_tl *list, const void *obj);

/**
   Returns the previous element of a list or NULL if @obj is the first element.

   @pre m0_tlist_contains(d, list, obj)
 */
M0_INTERNAL void *m0_tlist_prev(const struct m0_tl_descr *d,
				const struct m0_tl *list, const void *obj);

/**
   Iterates over elements of list @head of type @descr, assigning them in order
   (from head to tail) to @obj.

   It is safe to delete the "current" object in the body of the loop or modify
   the portion of the list preceding the current element. It is *not* safe to
   modify the list after the current point.

   @code
   m0_tlist_for(&foobar_list, &B.b_list, foo)
           sum += foo->f_value;
   m0_tlist_endfor;

   m0_tlist_for(&foobar_list, &B.b_list, foo) {
           if (foo->f_value % sum == 0)
	           m0_tlist_del(&foobar_list, foo);
   } m0_tlist_endfor;
   @endcode

   m0_tlist_for() macro has a few points of technical interest:

       - it introduces a scope to declare a temporary variable to hold the
         pointer to a "next" list element. The undesirable result of this is
         that the loop has to be terminated by the matching m0_tlist_endfor
         macro, closing the hidden scope. An alternative would be to use C99
         syntax for iterative statement, which allows a declaration in the
         for-loop header. Unfortunately, even though C99 mode can be enforced
         for compilation of linux kernel modules (by means of CFLAGS_MODULE),
         the kernel doesn't compile correctly in this mode;

       - "inventive" use of comma expression in the loop condition allows to
         calculate next element only once and only when the current element is
         not NULL.

   @see m0_tlist_endfor
 */
#define m0_tlist_for(descr, head, obj)					\
do {									\
	void *__tl;							\
	const struct m0_tl *__head = (head);				\
									\
	for (obj = m0_tlist_head(descr, __head);			\
	     obj != NULL &&						\
	     ((void)(__tl = m0_tlist_next(descr, __head, obj)), true);	\
	     obj = __tl)

/**
   Terminates m0_tlist_for() loop.
 */
#define m0_tlist_endfor ;(void)__tl; } while (0)

/**
 * Returns a conjunction (logical AND) of an expression evaluated for each list
 * element.
 *
 * @note This is not the macro you are interested in. Look at m0_tl_forall().
 *
 * @see m0_forall(), m0_tl_forall(), m0_list_forall(), m0_list_entry_forall().
 */
#define m0_tlist_forall(descr, var, head, ...)	\
({						\
	void *var;				\
						\
	m0_tlist_for(descr, head, var) {	\
		if (!({ __VA_ARGS__ ; }))	\
			break;			\
	} m0_tlist_endfor;			\
	var == NULL;				\
})

#define M0_TL_DESCR_DECLARE(name, scope)	\
scope const struct m0_tl_descr name ## _tl

/**
   Declares a version of tlist interface with definitions adjusted to take
   parameters of a specified ambient type (rather than void) and to hide
   m0_tl_descr from signatures.

   @code
   M0_TL_DECLARE(foo, static, struct foo);
   @endcode

   declares

   @code
   static void foo_tlist_init(struct m0_tl *head);
   static void foo_tlink_init(struct foo *amb);
   static void foo_tlist_move(struct m0_tl *list, struct foo *amb);
   static struct foo *foo_tlist_head(const struct m0_tl *list);
   @endcode

   &c.

   @see M0_TL_DEFINE()
   @see M0_TL_DESCR_DEFINE()
 */
#define M0_TL_DECLARE(name, scope, amb_type)				\
									\
scope void name ## _tlist_init(struct m0_tl *head);			\
scope void name ## _tlist_fini(struct m0_tl *head);			\
scope void name ## _tlink_init(amb_type *amb);				\
scope bool name ## _tlist_invariant(const struct m0_tl *head);		\
scope bool name ## _tlist_invariant_ext(const struct m0_tl *head,       \
                                        bool (*check)(const amb_type *, \
                                        void *), void *);		\
scope bool   name ## _tlist_is_empty(const struct m0_tl *list);		\
 scope void name ## _tlink_init_at(amb_type *amb, struct m0_tl *head);	\
 scope void name ## _tlink_init_at_tail(amb_type *amb, struct m0_tl *head);\
scope void name ## _tlink_fini(amb_type *amb);				\
scope void name ## _tlink_del_fini(amb_type *amb);			\
scope bool   name ## _tlist_is_empty(const struct m0_tl *list);		\
scope bool   name ## _tlink_is_in   (const amb_type *amb);		\
scope bool   name ## _tlist_contains(const struct m0_tl *list,		\
				     const amb_type *amb);		\
scope size_t name ## _tlist_length(const struct m0_tl *list);		\
scope void   name ## _tlist_add(struct m0_tl *list, amb_type *amb);	\
scope void   name ## _tlist_add_tail(struct m0_tl *list, amb_type *amb); \
scope void   name ## _tlist_add_after(amb_type *amb, amb_type *next);	\
scope void   name ## _tlist_add_before(amb_type *amb, amb_type *next);	\
scope void   name ## _tlist_del(amb_type *amb);				\
scope void   name ## _tlist_remove(amb_type *amb);			\
scope void   name ## _tlist_move(struct m0_tl *list, amb_type *amb);	\
scope void   name ## _tlist_move_tail(struct m0_tl *list, amb_type *amb); \
scope amb_type *name ## _tlist_head(const struct m0_tl *list);		\
scope amb_type *name ## _tlist_pop(const struct m0_tl *list);		\
scope amb_type *name ## _tlist_tail(const struct m0_tl *list);		\
scope amb_type *name ## _tlist_next(const struct m0_tl *list,           \
				    const amb_type *amb);	        \
scope amb_type *name ## _tlist_prev(const struct m0_tl *list,           \
				    const amb_type *amb)

#define __AUN __attribute__((unused))

/**
   Defines a tlist descriptor (m0_tl_descr) for a particular ambient type.
 */
#define M0_TL_DESCR_DEFINE(name, hname, scope, amb_type, amb_link_field, \
		     amb_magic_field, amb_magic, head_magic)		\
scope const struct m0_tl_descr name ## _tl = M0_TL_DESCR(hname,		\
							 amb_type,	\
							 amb_link_field, \
							 amb_magic_field, \
							 amb_magic,	\
							 head_magic)

/**
   Defines functions declared by M0_TL_DECLARE().

   The definitions generated assume that tlist descriptor, defined by
   M0_TL_DESC_DEFINED() is in scope.
 */
#define M0_TL_DEFINE(name, scope, amb_type)				\
									\
scope __AUN void name ## _tlist_init(struct m0_tl *head)		\
{									\
	m0_tlist_init(&name ## _tl, head);				\
}									\
									\
scope __AUN void name ## _tlist_fini(struct m0_tl *head)		\
{									\
	m0_tlist_fini(&name ## _tl, head);				\
}									\
									\
scope __AUN void name ## _tlink_init(amb_type *amb)			\
{									\
	m0_tlink_init(&name ## _tl, amb);				\
}									\
									\
scope __AUN void name ## _tlink_init_at(amb_type *amb, struct m0_tl *head) \
{									\
	m0_tlink_init_at(&name ## _tl, amb, head);			\
}									\
									\
scope __AUN void name ## _tlink_init_at_tail(amb_type *amb, struct m0_tl *head) \
{									\
	m0_tlink_init_at_tail(&name ## _tl, amb, head);			\
}									\
									\
scope __AUN void name ## _tlink_fini(amb_type *amb)			\
{									\
	m0_tlink_fini(&name ## _tl, amb);				\
}									\
									\
scope __AUN void name ## _tlink_del_fini(amb_type *amb)			\
{									\
	m0_tlink_del_fini(&name ## _tl, amb);				\
}									\
									\
scope __AUN bool name ## _tlist_invariant(const struct m0_tl *list)	\
{									\
	return m0_tlist_invariant(&name ## _tl, list);			\
}									\
									\
scope __AUN bool name ## _tlist_invariant_ext(const struct m0_tl *list, \
					      bool (*check)(const amb_type *,\
					      void *), void *datum)		\
{									\
	return m0_tlist_invariant_ext(&name ## _tl, list,               \
			 (bool (*)(const void *, void *))check, datum);	\
}									\
									\
scope __AUN bool   name ## _tlist_is_empty(const struct m0_tl *list)	\
{									\
	return m0_tlist_is_empty(&name ## _tl, list);			\
}									\
									\
scope __AUN bool   name ## _tlink_is_in   (const amb_type *amb)		\
{									\
	return m0_tlink_is_in(&name ## _tl, amb);			\
}									\
									\
scope __AUN bool   name ## _tlist_contains(const struct m0_tl *list,	\
				     const amb_type *amb)		\
{									\
	return m0_tlist_contains(&name ## _tl, list, amb);		\
}									\
									\
scope __AUN size_t name ## _tlist_length(const struct m0_tl *list)	\
{									\
	return m0_tlist_length(&name ## _tl, list);			\
}									\
									\
scope __AUN void   name ## _tlist_add(struct m0_tl *list, amb_type *amb) \
{									\
	m0_tlist_add(&name ## _tl, list, amb);				\
}									\
									\
scope __AUN void   name ## _tlist_add_tail(struct m0_tl *list, amb_type *amb) \
{									\
	m0_tlist_add_tail(&name ## _tl, list, amb);			\
}									\
									\
scope __AUN void   name ## _tlist_add_after(amb_type *amb, amb_type *next) \
{									\
	m0_tlist_add_after(&name ## _tl, amb, next);			\
}									\
									\
scope __AUN void   name ## _tlist_add_before(amb_type *amb, amb_type *next) \
{									\
	m0_tlist_add_before(&name ## _tl, amb, next);			\
}									\
									\
scope __AUN void   name ## _tlist_del(amb_type *amb)			\
{									\
	m0_tlist_del(&name ## _tl, amb);				\
}									\
									\
scope __AUN void   name ## _tlist_remove(amb_type *amb)			\
{									\
	m0_tlist_remove(&name ## _tl, amb);				\
}									\
									\
scope __AUN void   name ## _tlist_move(struct m0_tl *list, amb_type *amb) \
{									\
	m0_tlist_move(&name ## _tl, list, amb);				\
}									\
									\
scope __AUN void   name ## _tlist_move_tail(struct m0_tl *list, amb_type *amb) \
{									\
	m0_tlist_move_tail(&name ## _tl, list, amb);			\
}									\
									\
scope __AUN amb_type *name ## _tlist_head(const struct m0_tl *list)	\
{									\
	return (amb_type *)m0_tlist_head(&name ## _tl, list);		\
}									\
									\
scope __AUN amb_type *name ## _tlist_pop(const struct m0_tl *list)	\
{									\
	return (amb_type *)m0_tlist_pop(&name ## _tl, list);		\
}									\
									\
scope __AUN amb_type *name ## _tlist_tail(const struct m0_tl *list)	\
{									\
	return (amb_type *)m0_tlist_tail(&name ## _tl, list);		\
}									\
									\
scope __AUN amb_type *name ## _tlist_next(const struct m0_tl *list,     \
					  const amb_type *amb)		\
{									\
	return (amb_type *)m0_tlist_next(&name ## _tl, list, amb);	\
}									\
									\
scope __AUN amb_type *name ## _tlist_prev(const struct m0_tl *list,     \
				     const amb_type *amb)               \
{									\
	return (amb_type *)m0_tlist_prev(&name ## _tl, list, amb);	\
}									\
									\
struct __ ## name ## _terminate_me_with_a_semicolon { ; }

/**
 * A version of m0_tlist_for() to use with tailored lists.
 *
 * m0_tl_for() loop is terminated with m0_tl_endfor().
 */
#define m0_tl_for(name, head, obj) m0_tlist_for(& name ## _tl, head, obj)

/**
 * Terminates m0_tl_for() loop.
 */
#define m0_tl_endfor m0_tlist_endfor

/**
 * Empties the list, by taking the list head, assigning it to "obj" and removing
 * it from the list, until the list is empty.
 *
 * @note this doesn't require terminating macro.
 */
#define m0_tl_teardown(name, head, obj) \
	while (((obj) = name ## _tlist_pop(head)) != NULL)


/**
 * Returns a conjunction (logical AND) of an expression evaluated for each list
 * element.
 *
 * Declares a variable named "var" of list ambient object type in a new scope
 * and evaluates user-supplied expression (the last argument) with "var"
 * iterated over successive list elements, while this expression returns
 * true. Returns true iff the whole list was iterated over.
 *
 * The list can be modified by the user-supplied expression.
 *
 * This function is useful for invariant checking.
 *
 * @code
 * bool foo_invariant(const struct foo *f)
 * {
 *         return m0_tl_forall(bar, b, &f->f_bars,
 *                             b->b_count > 0 && b->b_parent == f);
 * }
 * @endcode
 *
 * @see m0_forall(), m0_tlist_forall(), m0_list_forall(), m0_list_entry_forall().
 */
#define m0_tl_forall(name, var, head, ...)		\
({							\
	typeof (name ## _tlist_head(NULL)) var;		\
							\
	m0_tl_for(name, head, var) {			\
		if (!({ __VA_ARGS__ ; }))		\
			break;				\
	} m0_tlist_endfor;				\
	var == NULL;					\
})

/**
 * Returns the list element that satisfies given condition.
 * Returns NULL if no such element is found.
 *
 * Example:
 * @code
 * return m0_tl_find(seg, seg, &dom->bd_segs, seg->bs_addr == addr);
 * @endcode
 *
 * @see m0_tl_forall()
 */
#define m0_tl_find(name, var, head, ...)		\
({							\
	typeof (name ## _tlist_head(NULL)) var;		\
							\
	m0_tl_for(name, head, var) {			\
		if (({ __VA_ARGS__ ; }))		\
			break;				\
	} m0_tlist_endfor;				\
	var;						\
})

/**
 * Returns a disjunction (logical OR) of an expression, evaluated for each list
 * element.
 *
 * @see m0_tl_forall()
 */
#define m0_tl_exists(name, var, head, ...)			\
	(!m0_tl_forall(name, var, head, !({ __VA_ARGS__ ; })))

/**
 * Reduces ("aggregates") given expression over a list.
 *
 * @see http://en.wikipedia.org/wiki/Fold_(higher-order_function)
 *
 * Example uses
 *
 * @code
 * sum = m0_tl_reduce(foo, scan, &foos, 0, + scan->f_delta);
 * product = m0_tl_reduce(foo, scan, &foos, 1, * scan->f_factor);
 * @encode
 *
 * @see m0_tl_fold(), m0_reduce()
 */
#define m0_tl_reduce(name, var, head, init, exp)	\
({							\
	typeof (name ## _tlist_head(NULL)) var;	\
	typeof(init) __accum = (init);			\
							\
	m0_tl_for(name, head, var) {			\
		__accum = __accum exp;			\
	} m0_tlist_endfor;				\
	__accum;					\
})

/**
 * Folds given expression over a list.
 *
 * This is a generalised version of m0_tl_reduce().
 *
 * @see http://en.wikipedia.org/wiki/Fold_(higher-order_function)
 *
 * Example uses
 *
 * @code
 * total = m0_tl_fold(foo, scan, sum, &foos, 0, sum + scan->f_delta);
 * max = m0_tl_fold(foo, scan, m, &foos, INT_MIN, max_t(int, m, scan->f_val));
 * @encode
 *
 * @see m0_tl_reduce(), m0_fold()
 */
#define m0_tl_fold(name, var, accum, head, init, exp)	\
({							\
	typeof (name ## _tlist_head(NULL)) var;	\
	typeof(init) accum = (init);			\
							\
	m0_tl_for(name, head, var) {			\
		accum = exp;				\
	} m0_tlist_endfor;				\
	accum;						\
})

/** @} end of tlist group */

/* __MERO_LIB_TLIST_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
