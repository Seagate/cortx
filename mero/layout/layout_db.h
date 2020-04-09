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
 * Original author: Trupti Patil <Trupti_Patil@xyratex.com>
 * Original creation date: 10/18/2011
 */

#pragma once

#ifndef __MERO_LAYOUT_LAYOUT_DB_H__
#define __MERO_LAYOUT_LAYOUT_DB_H__

/**
 * @page Layout-DB-fspec Layout DB Functional Specification
 * Layout DB Module is used by the Layout module to make persistent records for
 * the layout entries created and used.
 *
 * This section describes the data structures exposed and the external
 * interfaces of the Layout DB module and it briefly identifies the users of
 * these interfaces so as to explain how to use this module.
 *
 * - @ref Layout-DB-fspec-ds
 * - @ref Layout-DB-fspec-sub
 * - @ref Layout-DB-fspec-usecases
 * - @ref LayoutDBDFS "Detailed Functional Specification"
 *
 * @section Layout-DB-fspec-ds Data Structures
 * - struct m0_layout_rec
 *
 * @section Layout-DB-fspec-sub Subroutines
 * - int m0_layout_lookup(struct m0_layout_domain *dom, uint64_t lid, struct m0_db_tx *tx, struct m0_db_pair *pair, struct m0_layout **out);
 * - int m0_layout_add(struct m0_layout *l, struct m0_db_tx *tx, struct m0_db_pair *pair);
 * - int m0_layout_update(struct m0_layout *l, struct m0_db_tx *tx, struct m0_db_pair *pair);
int m0_layout_delete(struct m0_layout *l, struct m0_db_tx *tx, struct m0_db_pair *pair);
 *
 * @subsection Layout-DB-fspec-sub-acc Accessors and Invariants
 *
 * @section Layout-DB-fspec-usecases Recipes
 * A file layout is used by the client to perform IO against that file. For
 * example, layout for a file may contain COB identifiers for all the COBs
 * associated with that file. For example, the COB identifiers may be stored
 * by the layout either in the form of a list or as a linear formula.
 *
 * Example use case of reading a file:
 * - Reading a file involves reading basic file attributes from the basic file
 *   attributes table).
 * - The layout id is obtained from the basic file attributes.
 * - A query is sent to the Layout module to obtain layout for this layout id
 *   using the API m0_layout_lookup()
 *   - m0_layout_lookup() returns the layout object if it is cached.
 *   - If the layout is not cached, m0_layout_lookup() return it by reading
 *     it from the layout DB and by keeping it in the cache.
 * - Reading a layout record from the layout DB involves the following for
 *   example:
 *    - If the layout record is with the LINEAR enumeration, then the
 *      linear formula is obtained from the DB. Once the formula is available,
 *      the user can substitute the required parameters into the formula so as
 *      to obtain the list of COB identifiers to operate upon.
 *    - If the layout record is with the LIST enumeration, then the
 *      the list of COB identifiers is obtained from the layout DB itself.
 *    - If the layout record is of the COMPOSITE layout type, it means it
 *      constitutes of multiple sub-layouts. In this case, the sub-layouts are
 *      read from the layout DB. Those sub-layout records in turn could be of
 *      other layout types and with LINEAR or LIST enumeration for example.
 *      The sub-layout records are then read accordingly until the time the
 *      final list of all the COB identifiers is obtained.
 *
 *  @see @ref LayoutDBDFS "Layout DB Detailed Functional Specification"
 */

/* import */
#include "layout/layout.h"

/**
 * @defgroup LayoutDBDFS Layout DB
 * @brief Detailed functional specification for Layout DB.
 *
 * Detailed functional specification provides documentation of all the data
 * structures and interfaces (internal and external).
 *
 * @see @ref Layout-DB "Layout DB DLD" and its @ref Layout-DB-fspec
 * "Layout DB Functional Specification".
 *
 * @{
 */

/**
 * Looks for an in-memory layout object with the given identifier in the list
 * of layout objects maintained in the layout domain.
 * If not found there, looks up for a persistent layout record in the layout DB
 * with the given identifier. If present in the layout DB, prepares an
 * in-memory layout object with the information from the DB and adds the
 * layout object to the list of layout objects in the layout domain.
 *
 * All operations are performed in the context of the caller-supplied
 * transaction.
 *
 * @param pair A m0_db_pair sent by the caller along with having set
 * pair->dp_key.db_buf and pair->dp_rec.db_buf. This is to leave the buffer
 * allocation with the caller.
 *
 * Regarding the size of the pair->dp_rec.db_buf:
 * The buffer size should be large enough to contain the data that is to be
 * read specifically from the layouts table. It means it needs to be at the
 * most the size returned by m0_layout_max_recsize(). It is no harm if it is
 * bigger than that.
 *
 * @post
 * - Returns a layout object with the given identifier if it exists in the
 *   list of the layout objects maintained in the layout domain.
 * - If it does not exist in that list, then it read from the database. If its
 *   entry exists in the database, then using it, a new layout object is built
 *   internally (along with enumeration object being built if applicable).
 * - In case of successful return, an additional reference is acquired on
 *   the layout object returned in the variable "out".
 */
M0_INTERNAL int m0_layout_lookup(struct m0_layout_domain *dom,
				 uint64_t lid,
				 struct m0_layout_type *lt,
				 struct m0_db_tx *tx,
				 struct m0_db_pair *pair,
				 struct m0_layout **out);

/**
 * Adds a new layout record entry into the layouts table.
 * If applicable, adds layout type and enum type specific entries into the
 * relevant tables.
 *
 * @param pair A m0_db_pair sent by the caller along with having set
 * pair->dp_key.db_buf and pair->dp_rec.db_buf. This is to leave the buffer
 * allocation with the caller.
 *
 * Regarding the size of the pair->dp_rec.db_buf:
 * The buffer size should be large enough to contain the data that is to be
 * written specifically to the layouts table. It means it needs to be at the
 * most the size returned by m0_layout_max_recsize().
 */
M0_INTERNAL int m0_layout_add(struct m0_layout *l,
			      struct m0_db_tx *tx, struct m0_db_pair *pair);

/**
 * Updates a layout record from the DB. The only field that can be updated for
 * an existing layout in the layout DB is its user count. It is not prevented
 * through the implementation to update any other fields since it was found to
 * be costly through performance perspective.
 *
 * @param pair A m0_db_pair sent by the caller along with having set
 * pair->dp_key.db_buf and pair->dp_rec.db_buf. This is to leave the buffer
 * allocation with the caller.
 *
 * Regarding the size of the pair->dp_rec.db_buf:
 * The buffer size should be large enough to contain the data that is to be
 * written specifically to the layouts table. It means it needs to be at the
 * most the size returned by m0_layout_max_recsize().
 *
 * @note Even a non-existing record can be written to the database using
 * the database update operation. In other words, not using m0_layout_add()
 * and directly using m0_layout_update() results into the layout record being
 * written to the DB.
 */
M0_INTERNAL int m0_layout_update(struct m0_layout *l,
				 struct m0_db_tx *tx, struct m0_db_pair *pair);

/**
 * Deletes a layout record with given layout id and its related information
 * from the relevant tables.
 *
 * @param pair A m0_db_pair sent by the caller along with having set
 * pair->dp_key.db_buf and pair->dp_rec.db_buf. This is to leave the buffer
 * allocation with the caller.
 *
 * Regarding the size of the pair->dp_rec.db_buf:
 * The buffer size should be large enough to contain the data that is to be
 * written specifically to the layouts table. It means it needs to be at the
 * most the size returned by m0_layout_max_recsize().
 */
M0_INTERNAL int m0_layout_delete(struct m0_layout *l,
				 struct m0_db_tx *tx, struct m0_db_pair *pair);

M0_INTERNAL void m0_layout_pair_set(struct m0_db_pair *pair, uint64_t *lid,
				    void *area, m0_bcount_t num_bytes);
/** @} end group LayoutDBDFS */

#endif /*  __MERO_LAYOUT_LAYOUT_DB_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
