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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 10-Feb-2014
 */

#pragma once
#ifndef __MERO_BE_SEG0_H__
#define __MERO_BE_SEG0_H__

#include "lib/tlist.h"

/* import */
struct m0_buf;
struct m0_be_tx;
struct m0_be_domain;

/**
 * @defgroup be Meta-data back-end
 *
 * @{
 */

/** seg0 object type. */
struct m0_be_0type {

	/** unique prefix: "SEG", "LOG", "AD", "MD", etc. */
	const char  *b0_name;

	/**
	 * Initialisation call-back invoked by BE when seg0 is loaded. This
	 * call-back sets volatile BE-related parameters according to given
	 * @data, which stores options. These options describe BE-objects
	 * configuration. For example options may have an information that
	 * mdservice related trees are stored inside segment with #0102 id.
	 *
	 * @note after b0_init() call there's no need for concrete modules, like
	 * balloc or adstob to access segment dictionary.
	 *
	 * @param suffix distinguishes multiple instances of the same type,
	 * e.g., M0_BE:SEG1, M0_BE:SEG2.
	 *
	 * @param data is an opaque pointer and size to the options
	 *             this type needs to perform startup.
	 */
	int       (*b0_init)(struct m0_be_domain *dom, const char *suffix,
			     const struct m0_buf *data);

	void      (*b0_fini)(struct m0_be_domain *dom, const char *suffix,
			     const struct m0_buf *data);

	/** linkage in a list of 0types into m0_be_domain. */
	struct m0_tlink      b0_linkage;
	uint64_t             b0_magic;
};

/**
 * Registers new 0type.
 */
void m0_be_0type_register(struct m0_be_domain *dom, struct m0_be_0type *zt);

/**
 * Registers new 0type.
 */
void m0_be_0type_unregister(struct m0_be_domain *dom, struct m0_be_0type *zt);

/**
 * Calculates BE-credit for m0_be_0type_del().
 */
void m0_be_0type_del_credit(struct m0_be_domain *dom,
			    const struct m0_be_0type  *zt,
			    const char                *suffix,
			    struct m0_be_tx_credit    *credit);

/**
 * Calculates BE-credit for m0_be_0type_add().
 */
void m0_be_0type_add_credit(struct m0_be_domain *dom,
			    const struct m0_be_0type  *zt,
			    const char                *suffix,
			    const struct m0_buf       *data,
			    struct m0_be_tx_credit    *credit);

/**
 * Adds a record about 0type instance to the seg0 dictionary.
 */
int m0_be_0type_add(struct m0_be_0type  *zt,
		    struct m0_be_domain *dom,
		    struct m0_be_tx     *tx,
		    const char          *suffix,
		    const struct m0_buf *data);

/**
 * Deletes a record about 0type instance from the seg0 dictionary.
 */
int m0_be_0type_del(struct m0_be_0type  *zt,
		    struct m0_be_domain *dom,
		    struct m0_be_tx     *tx,
		    const char          *suffix);

/**
 * <hr> <!------------------------------------------------------------>
 * @section seg0-metadata Meta-segment (seg0), systematic BE storage startup.
 * <b>Overview.</b>
 *
 * Seg0 stores an information to bring all BE subsystems up. It may include
 * different hierarchies of objects which have to be brought up and initialized
 * systematically. So the goal of metadata is to store an information which is
 * used to specify an order and some options for BE subsystem initialization
 * and startup.
 *
 * It's supposed that BE objects like segment or different structures stored
 * inside it, have no mutual dependencies, so the initialization order and
 * metadata may have trivial format.
 *
 * It's assumed that different BE objects of the same type can be brought up
 * independently. This assumption makes initialization routines straightforward.
 *
 * Concrete BE-object initialization depends on a tier of BE-objects which have
 * to be started before it. Type of this object has to be registered after types
 * from which initialization of this object depends on.
 *
 * The example hierarchiy below has the following startup ordering:
 * M0_BE:SEG, M0_BE:COB, M0_BE:MDS, M0_BE:AD, M0_BE:BALLOC.
 *
 * @dot
 *  digraph conf_fetch_phase {
 *      node [fontsize=9];
 *      edge [fontsize=9];
 *      "M0_BE:SEG0"    [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:SEG1"    [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:SEG2"    [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:SEGN"    [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:COB1"    [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:COB0"    [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:MDS"     [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:AD0"     [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:AD1"     [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:BALLOC0" [shape=rect, style=filled, fillcolor=lightgrey];
 *      "M0_BE:BALLOC1" [shape=rect, style=filled, fillcolor=lightgrey];
 *
 *      "M0_BE:SEG0" -> "M0_BE:SEG1"
 *      "M0_BE:SEG0" -> "M0_BE:SEG2"
 *      "M0_BE:SEG0" -> "M0_BE:SEGN"
 *      "M0_BE:SEG1" -> "M0_BE:COB0"
 *      "M0_BE:SEG2" -> "M0_BE:COB0"
 *      "M0_BE:SEG2" -> "M0_BE:COB1"
 *      "M0_BE:COB1" -> "M0_BE:MDS"
 *      "M0_BE:COB0" -> "M0_BE:MDS"
 *      "M0_BE:SEGN" -> "M0_BE:AD0"
 *      "M0_BE:SEGN" -> "M0_BE:AD1"
 *      "M0_BE:AD1"  -> "M0_BE:BALLOC0"
 *      "M0_BE:AD0"  -> "M0_BE:BALLOC1"
 *  }
 * @enddot
 *
 * <b> Highlights </b>
 *
 * - While BE initialization, seg0 dictionary is scanned for a special records
 *   started with "M0_BE:". Values of these records are structures which have a
 *   special m0_be_0type stored inside, which is extracted and
 *   m0_be_0type::b0_init() is called on every such type. Metadata associated
 *   with m0_be_0type is not somehow related to confd but it can be node- or
 *   segment- related.
 *
 * - m0_be_0type::b0_init() initializes BE-internal objects like all segments
 *   and log and the sets volatile-only variables inside other subsystems for which
 *   m0_be_0types are defined.
 *
 * - In cases when subsystem needs to change one of its configuration parameters
 *   it calls  m0_be_0type_add() to peform changes in seg0 dictionary.
 *
 * <b>BE domain initialization</b>
 *
 * mkfs.be uses m0_be_0type_add() to populate the seg0 dictionary.
 *
 * <b>BE-internal objects: segments and log</b>
 *
 * Segments and log configuration options are stored inside seg0. Initialization
 * of these BE objects are more than just their volatile-only fields assignment and
 * it includes corresponding m0_stob lookup and steps needed to bring on-disk
 * and in-memory parts of these objects up.
 *
 * To perform such startup sequences the following functions are used for all
 * segments: m0_be_seg_init(), m0_be_seg_open(); and for log: m0_be_log_init(),
 * m0_be_log_open().  Accoringly to given interfaces, each segment has to have
 * the following set of metadata options: underlying stob options, domain
 * options, segment options; and log should have the following: underlying stob
 * options.
 *
 * <b> Metadata format </b>
 * Related metadata is stored inside seg0 dictionary and retrieved from it by
 * iteration of all records in dictionary with predefined prefix in its
 * keys (@todo dictionary has to be updated a little bit to do this).
 * Format of the key is the following:
 * "{ prefix } { separator } { 0type } { objname }"
 * Example: "M0_BE:COB1"
 *
 * The value of such dictionary record contains struct m0_buf representing a
 * pointer and size of an object which is being brought up. It has to be casted
 * inside concrete ->b0_init() and used as options for concrete m0_be_0type to
 * perform initialization.
 *
 *
 * <b>BE-object startup pseudocode (top-level).</b>
 *
 * @code
 * struct m0_be_domain      *dom = ...;
 *
 * struct m0_buf      *opt;
 * struct m0_be_0type *objtype;
 *
 *  m0_tlist_for(&be_0type_list, &dom->bd_0types, objtype) {
 *      for (opt = segobj_opt_begin(dict, objtype);
 *           opt != NULL && rc != 0;
 *           opt = segobj_opt_next(dict, opt, objtype))
 *              rc = be_obj_up(dom, opt);
 * } m0_tlist_endfor;
 * @endcode
 *
 * <b>Iterator-like interface used while startup.</b>
 * @code
 * const struct m0_buf *segobj_opt_begin(const struct m0_be_seg   *dict,
 *                                       const struct m0_be_0type *objtype);
 *
 * const struct m0_buf *segobj_opt_next(const struct m0_be_seg    *dict,
 *                                      const struct m0_be_buf    *opt,
 *                                      const struct m0_be_0type  *objtype);
 * @endcode
 *
 * <b>Pseudocode bringing BE-object up.</b>
 * @code
 * int be_obj_up(struct m0_be_domain *dom, struct m0_be_buf *opt)
 * {
 *      char suffix[] = ...;
 *
 *      return type->b0_init(dom, suffix, opt);
 * }
 * @endcode
 *
 * <hr> <!------------------------------------------------------------>
 */

/** @} end of be group */
#endif /* __MERO_BE_SEG0_H__ */

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
