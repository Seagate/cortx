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

/**
 * @page Layout-DB Layout DB DLD
 *
 * - @ref Layout-DB-ovw
 * - @ref Layout-DB-def
 * - @ref Layout-DB-req
 * - @ref Layout-DB-depends
 * - @ref Layout-DB-highlights
 * - @subpage Layout-DB-fspec "Functional Specification"
 * - @ref Layout-DB-lspec
 *    - @ref Layout-DB-lspec-comps
 *    - @ref Layout-DB-lspec-schema
 *    - @ref Layout-DB-lspec-state
 *    - @ref Layout-DB-lspec-thread
 *    - @ref Layout-DB-lspec-numa
 * - @ref Layout-DB-conformance
 * - @ref Layout-DB-ut
 * - @ref Layout-DB-st
 * - @ref Layout-DB-O
 * - @ref Layout-DB-ref
 *
 * <HR>
 * @section Layout-DB-ovw Overview
 * This document contains the detail level design for the Layout DB Module.
 *
 * Purpose of the Layout-DB DLD @n
 * The purpose of the Layout-DB Detailed Level Design (DLD) specification is to:
 * - Refine the higher level design
 * - To be verified by inspectors and architects
 * - To guide the coding phase
 *
 * <HR>
 * @section Layout-DB-def Definitions
 *   - COB: COB is component object and is defined at
 *   <a href="https://docs.google.com/a/seagate.com/spreadsheet/ccc?key=0AiZ-h3k
 uhu54dEtBOUFCUkxiNmJaWkRTQWwyWUltRnc&hl=en_US#gid=0">
 *    M0 Glossary</a>
 *
 * <HR>
 * @section Layout-DB-req Requirements
 * The specified requirements are as follows:
 * - R.LAYOUT.SCHEMA.Layid: Layout identifiers are unique globally in
 *   the system, and persistent in the life cycle.
 * - R.LAYOUT.SCHEMA.Types There are multiple layout types for
 *   different purposes: SNS, block map, local raid, de-dup, encryption,
 *   compression, etc.
 * - R.LAYOUT.SCHEMA.Formulae
 *    - Parameters: Layout may contain sub-map information. Layout may
 *      contain some formula, and its parameters and real mapping information
 *      should be calculated from the formula and its parameters.
 *    - Garbage Collection: If some objects are deleted from the system,
 *      their associated layout may still be left in the system, with zero
 *      user count. This layout can be re-used, or be garbage collected in
 *      some time.
 * - R.LAYOUT.SCHEMA.Sub-Layouts: Sub-layouts.
 *
 * <HR>
 * @section Layout-DB-depends Dependencies
 * - Layout is a managed resource and depends upon Resource Manager.
 * - Layout DB module depends upon the Layout module since the Layout module
 *   creates the layouts and uses/manages them.
 * - Layout DB module depends upon the DB5 interfaces exposed by
 *   Mero since the layouts are stored using the DB5 data-base.
 *
 * <HR>
 * @section Layout-DB-highlights Design Highlights
 * - Layout and layout-id are managed resources. @see @ref layout
 * - The Layout DB module provides support for storing layouts with multiple
 *   layout types.
 * - It provides support for storing composite layout maps.
 * - It is required that for adding a layout type or layout enumeration type,
 *   central layout.h should not require modifications.
 * - It is assumed that the problem of coruption is going to be attacked
 *   generically at the lower layers (db and fop) transparently, instead of
 *   adding magic numbers and check-sums in every module. Thus the input to
 *   Layout DB APIs which is either a layout or a FOP buffer in most of the
 *   cases is going to be tested for corruption by db or fop layer, as
 *   applicable.
 *
 * <HR>
 * @section Layout-DB-lspec Logical Specification
 * Layout DB makes use of the DB5 data-base to persistently store the layout
 * entries. This section describes how the Layout DB module works.
 *
 * - @ref Layout-DB-lspec-comps
 * - @ref Layout-DB-lspec-schema
 *    - @ref Layout-DB-lspec-ds1
 *    - @ref Layout-DB-lspec-sub1
 *    - @ref LayoutDBDFSInternal
 * - @ref Layout-DB-lspec-state
 * - @ref Layout-DB-lspec-thread
 * - @ref Layout-DB-lspec-numa
 *
 *
 * @subsection Layout-DB-lspec-comps Component Overview
 * The following diagram shows the internal components of the "Layout" module,
 * including the "Layout DB" component.
 *
 * @dot
 * digraph {
 *   node [style=box];
 *   label = "Layout Components and Interactions";
 *
 *   subgraph mero_client {
 *       label = "Client";
 *       cClient [label="Client"];
 *   }
 *
 *   subgraph mero_layout {
 *       label = "Layout";
 *
 *   cLDB [label="Layout DB", style="filled"];
 *   cFormula [label="Layout Formula", style="filled"];
 *   cLayout [label="Layout (Managed Resource)", style="filled"];
 *
 *       cLDB -> cFormula [label="build formula"];
 *       cFormula -> cLayout [label="build layout"];
 *    }
 *
 *   subgraph mero_server {
 *       label = "Server";
 *       cServer [label="Server"];
 *    }
 *
 *   cClient -> cFormula [label="substitute"];
 *   cServer -> cFormula [label="substitute"];
 *
 *   { rank=same; cClient cFormula cServer }
 *  }
 *  @enddot
 *
 * @subsection Layout-DB-lspec-schema Layout Schema Design
 * The layout schema for the Layout DB module consists of the following tables.
 * - @ref Layout-DB-lspec-schema-layouts
 * - @ref Layout-DB-lspec-schema-cob_lists
 * - @ref Layout-DB-lspec-schema-comp_layout_ext_map
 *
 * Key-Record structures for these tables are described below.
 *
 * @subsection Layout-DB-lspec-schema-layouts Table layouts
 * @verbatim
 * Table Name: layouts
 * Key: layout_id
 * Record:
 *    - layout_type_id
 *    - user_count
 *    - layout_type_specific_data (optional)
 *
 * @endverbatim
 *
 * layout_type_specific_data field is used to store layout type or layout enum
 * type specific data. Structure of this field varies accordingly. For example:
 * - In case of a layout with PDCLUST layout type, the structure
 *   m0_layout_pdclust_rec is used to store attributes like enumeration type
 *   id, N, K, P.
 * - In case of a layout with LIST enum type, an array of m0_fid
 *   structure with size LDB_MAX_INLINE_COB_ENTRIES is used to store a few COB
 *   entries inline into the layouts table itself.
 * - It is possible that some layouts do not need to store any layout type or
 *   layout enum type specific data in the layouts table. For example, a
 *   layout with COMPOSITE layout type.
 *
 * @subsection Layout-DB-lspec-schema-cob_lists Table cob_lists
 * @verbatim
 * Table Name: cob_lists
 * Key:
 *    - layout_id
 *    - cob_index
 * Record:
 *    - cob_id
 *
 *  @endverbatim
 *
 * This table contains multiple COB identifier entries for every PDCLUST type
 * of layout with LIST enumeration type.
 *
 * layout_id is a foreign key referring record, in the layouts table.
 *
 * cob_index for the first entry in this table will be the continuation of the
 * index from the array of m0_fid structures stored inline in the layouts
 * table.
 *
 * @subsection Layout-DB-lspec-schema-comp_layout_ext_map
 * Table comp_layout_ext_map
 *
 * @verbatim
 * Table Name: comp_layout_ext_map
 * Key
 *    - composite_layout_id
 *    - last_offset_of_segment
 * Record
 *    - start_offset_of_segment
 *    - layout_id
 *
 * @endverbatim
 *
 * composite_layout_id is the layout_id for the COMPOSITE type of layout,
 * stored as key in the layouts table.
 *
 * layout_id is a foreign key referring record, in the layouts table.
 *
 * Layout DB uses a single m0_emap instance to implement the composite layout
 * extent map viz. comp_layout_ext_map. This table stores the "layout segment
 * to sub-layout id mappings" for each compsite layout.
 *
 * Since prefix (an element of the key for m0_emap) is required to be 128 bit
 * in size, layout id (unit64_t) of the composite layout is used as a part of
 * the prefix (struct layout_prefix) to identify an extent map belonging to one
 * specific composite layout. The lower 64 bits are currently unused (fillers).
 *
 * An example:
 *
 * Suppose a layout L1 is of the type composite and constitutes of 3
 * sub-layouts say S1, S2, S3. These sub-layouts S1, S2 and S3 use
 * the layouts with layout id L11, L12 and L13 respectively.
 *
 * In this example, for the composite layout L1, the comp_layout_ext_map
 * table stores 3 layout segments viz. S1, S2 and S3. All these 3 segments
 * are stored in the form of ([A, B), V) where:
 * - A is the start offset from the layout L1
 * - B is the end offset from the layout L1
 * - V is the layout id for the layout used by the respective segment and
 *   is either of L11, L12 or L13 as applicable.
 *
 * @subsubsection Layout-DB-lspec-ds1 Subcomponent Data Structures
 * See @ref LayoutDBDFSInternal for internal data structures.
 *
 * @subsubsection Layout-DB-lspec-sub1 Subcomponent Subroutines
 * See @ref LayoutDBDFSInternal for internal subroutines.
 *
 * @subsection Layout-DB-lspec-state State Specification
 * This module does follow state machine ind of a design. Hence, this section
 * is not applicable.
 *
 * @subsection Layout-DB-lspec-thread Threading and Concurrency Model
 * See @ref layout-thread.
 *
 * @subsection Layout-DB-lspec-numa NUMA optimizations
 *
 * <HR>
 * @section Layout-DB-conformance Conformance
 * - I.LAYOUT.SCHEMA.Layid: Layout identifiers are unique globally in
 *   the system, and persistent in the life cycle. It is assumed that the
 *   layout identifiers are assigned by the Layout module and Layout DB module
 *   helps to store those persistently.
 * - I.LAYOUT.SCHEMA.Types: There are multiple layout types for different
 *   purposes: SNS, block map, local raid, de-dup, encryption, compression, etc.
 *   @n
 *   Layout DB module supports storing all kinds of layout types supported
 *   currently by the layout module viz. PDCLUST and COMPOSITE.
 *   The framework supports to add other layout types, as required in
 *   the future.
 * - I.LAYOUT.SCHEMA.Formulae:
 *    - Parameters:
 *       - In case of PDCLUST layout type using LINEAR enumeration,
 *         linear formula is stored by the Layout DB and substituting
 *         parameters in the stored formula derives the real mapping
 *         information that is the list of COB identifiers.
 *    - Garbage Collection:
 *       - A layout with 0 user count can stay in the DB unless it is deleted
	   explicitly from the DB.
 *       - An in-memory layout is deleted when its last reference is released
	   explicitly.
 * - I.LAYOUT.SCHEMA.Sub-Layouts: COMPOSITE type of layout is used to store
 *   sub-layouts.
 *
 * <HR>
 * @section Layout-DB-ut Unit Tests
 *
 * Following cases will be tested by unit tests:
 *
 * @test 1) Registering layout types including PDCLUST amd COMPOSITE types.
 *
 * @test 2) Unregistering layout types including PDCLUST amd COMPOSITE types.
 *
 * @test 3) Registering each of LIST and LINEAR enum types.
 *
 * @test 4) Unregistering each of LIST and LINEAR enum types.
 *
 * @test 5) Encode layout with each of layout type and enum types.
 *
 * @test 6) Decode layout with each of layout type and enum types.
 *
 * @test 7) Adding layouts with all the possible combinations of all the
 *          layout types and enumeration types.
 *
 * @test 8) Deleting layouts with all the possible combinations of all the
 *          layout types and enumeration types.
 *
 * @test 9) Updating layouts with all the possible combinations of all the
 *          layout types and enumeration types.
 *
 * @test 10) Reading a layout with all the possible combinations of all the
 *           layout types and enumeration types.
 *
 * @test 11) Checking DB persistence by comparing a layout with the layout read
 *           from the DB, for all the possible combinations of all the layout
 *           types and enumeration types.
 *
 * @test 12) Covering all the negative test cases.
 *
 * @test 13) Covering all the error cases.
 *
 * <HR>
 * @section Layout-DB-st System Tests
 *
 * System testing will include tests where multiple processes are writing
 * to and reading from the DB at the same time.
 *
 * <HR>
 * @section Layout-DB-O Analysis
 *
 * <HR>
 * @section Layout-DB-ref References
 * - <a href="https://docs.google.com/a/seagate.com/document/d/1KL6mEA0LH8JSBXR8
 KErtOe5jvtFcN-WcS7MdEPmHEOM/edit?hl=en_US">
 *    HLD of Layout Schema</a>
 * - <a href="https://docs.google.com/a/seagate.com/document/d/1YnXNBFyfH7-QXy5O
 1o4ddgwhhMbL6B0q15t0yl4N9-w/edit?hl=en_US#heading=h.gz7460ketfn1">Understanding
  LayoutSchema</a>
 *
 */

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/misc.h"  /* memset() */
#include "lib/vec.h"   /* M0_BUFVEC_INIT_BUF() */
#include "lib/finject.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"

#include "layout/layout_internal.h"
#include "layout/layout_db.h"

/**
 * @defgroup LayoutDBDFSInternal Layout DB Internals
 * @brief Detailed functional specification of the internals of the
 * Layout-DB module.
 *
 * This section covers the data structures and sub-routines used internally.
 *
 * @see @ref Layout-DB "Layout-DB DLD"
 * and @ref Layout-DB-lspec "Layout-DB Logical Specification".
 *
 * @{
 */

M0_INTERNAL void m0_layout_pair_set(struct m0_db_pair *pair, uint64_t *lid,
				    void *area, m0_bcount_t num_bytes)
{
	pair->dp_key.db_buf.b_addr = lid;
	pair->dp_key.db_buf.b_nob  = sizeof *lid;
	pair->dp_rec.db_buf.b_addr = area;
	pair->dp_rec.db_buf.b_nob  = num_bytes;
}

static int pair_init(struct m0_db_pair *pair,
		     struct m0_layout *l,
		     struct m0_db_tx *tx,
		     enum m0_layout_xcode_op op,
		     m0_bcount_t recsize)
{
	void                    *key_buf = pair->dp_key.db_buf.b_addr;
	void                    *rec_buf = pair->dp_rec.db_buf.b_addr;
	struct m0_bufvec         bv;
	struct m0_bufvec_cursor  rec_cur;
	int                      rc;

	M0_PRE(key_buf != NULL);
	M0_PRE(rec_buf != NULL);
	M0_PRE(pair->dp_key.db_buf.b_nob == sizeof l->l_id);
	M0_PRE(pair->dp_rec.db_buf.b_nob >= recsize);
	M0_PRE(M0_IN(op, (M0_LXO_DB_LOOKUP, M0_LXO_DB_ADD,
			  M0_LXO_DB_UPDATE, M0_LXO_DB_DELETE)));
	M0_PRE(recsize >= sizeof(struct m0_layout_rec));

	*(uint64_t *)key_buf = l->l_id;
	memset(pair->dp_rec.db_buf.b_addr, 0, pair->dp_rec.db_buf.b_nob);
	m0_db_pair_setup(pair, &l->l_dom->ld_layouts,
			 key_buf, sizeof l->l_id,
			 rec_buf, recsize);
	if (op == M0_LXO_DB_LOOKUP)
		rc = 0;
	else {
		bv = (struct m0_bufvec)M0_BUFVEC_INIT_BUF(&rec_buf,
						  &pair->dp_rec.db_buf.b_nob);
		m0_bufvec_cursor_init(&rec_cur, &bv);

		rc = m0_layout_encode(l, op, tx, &rec_cur);
		if (rc != 0) {
			m0_layout__log("pair_init", "m0_layout_encode() failed",
				       l->l_id, rc);
			m0_db_pair_fini(pair);
		}
	}
	return M0_RC(rc);
}

/** @} end group LayoutDBDFSInternal */

/**
 * @addtogroup LayoutDBDFS
 * @{
 */

M0_INTERNAL int m0_layout_lookup(struct m0_layout_domain *dom,
				 uint64_t lid,
				 struct m0_layout_type *lt,
				 struct m0_db_tx *tx,
				 struct m0_db_pair *pair,
				 struct m0_layout **out)
{
	int                      rc;
	struct m0_bufvec         bv;
	struct m0_bufvec_cursor  cur;
	m0_bcount_t              max_recsize;
	m0_bcount_t              recsize;
	struct m0_layout        *l;
	struct m0_layout        *ghost;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(lid > 0);
	M0_PRE(lt != NULL);
	M0_PRE(tx != NULL);
	M0_PRE(pair != NULL);
	M0_PRE(out != NULL);

	M0_ENTRY("lid %llu", (unsigned long long)lid);
	if (dom->ld_type[lt->lt_id] != lt) {
		m0_layout__log("m0_layout_lookup", "Unregistered layout type",
			       lid, -EPROTO);
		return M0_ERR(-EPROTO);
	}

	m0_mutex_lock(&dom->ld_lock);
	l = m0_layout__list_lookup(dom, lid, true);
	m0_mutex_unlock(&dom->ld_lock);
	if (l != NULL) {
		/*
		 * Layout object exists in memory and m0_layout__list_lookup()
		 * has now acquired a reference on it.
		 */
		*out = l;
		M0_POST(m0_layout__invariant(*out));
		M0_LEAVE("lid %llu, rc %d", (unsigned long long)lid, 0);
		return 0;
	}

	/* Allocate outside of the domain lock to improve concurrency. */
	rc = lt->lt_ops->lto_allocate(dom, lid, &l);
	if (rc != 0) {
		m0_layout__log("m0_layout_lookup", "lto_allocate() failed",
			       lid, rc);
		return M0_RC(rc);
	}
	/* Here, lto_allocate() has locked l->l_lock. */

	if (M0_FI_ENABLED("ghost_creation")) {}

	/* Re-check for possible concurrent layout creation. */
	m0_mutex_lock(&dom->ld_lock);
	ghost = m0_layout__list_lookup(dom, lid, true);
	if (ghost != NULL) {
		/*
		 * Another instance of the layout with the same layout id
		 * "ghost" was created while the domain lock was released.
		 * Use it. m0_layout__list_lookup() has now acquired a
		 * reference on "ghost".
		 */
		m0_mutex_unlock(&dom->ld_lock);
		l->l_ops->lo_delete(l);

		/* Wait for possible decoding completion. */
		m0_mutex_lock(&ghost->l_lock);
		m0_mutex_unlock(&ghost->l_lock);

		*out = ghost;
		M0_POST(m0_layout__invariant(*out));
		M0_POST(m0_ref_read(&(*out)->l_ref) > 1);
		M0_LEAVE("lid %llu, ghost found, rc %d",
			 (unsigned long long)lid, 0);
		return 0;
	}
	m0_mutex_unlock(&dom->ld_lock);

	max_recsize = m0_layout_max_recsize(dom);
	recsize = pair->dp_rec.db_buf.b_nob <= max_recsize ?
		  pair->dp_rec.db_buf.b_nob : max_recsize;
	rc = pair_init(pair, l, tx, M0_LXO_DB_LOOKUP, recsize);
	M0_ASSERT(rc == 0);
	rc = m0_table_lookup(tx, pair);
	if (rc != 0) {
		/* Error covered in UT. */
		l->l_ops->lo_delete(l);
		m0_layout__log("m0_layout_lookup", "m0_table_lookup() failed",
			       lid, rc);
		goto out;
	}

	bv = (struct m0_bufvec)M0_BUFVEC_INIT_BUF(&pair->dp_rec.db_buf.b_addr,
						  &recsize);
	m0_bufvec_cursor_init(&cur, &bv);
	rc = m0_layout_decode(l, &cur, M0_LXO_DB_LOOKUP, tx);
	if (rc != 0) {
		/* Error covered in UT. */
		l->l_ops->lo_delete(l);
		m0_layout__log("m0_layout_lookup", "m0_layout_decode() failed",
			       lid, rc);
		goto out;
	}
	*out = l;
	M0_POST(m0_layout__invariant(*out) && m0_ref_read(&l->l_ref) > 0);
	m0_mutex_unlock(&l->l_lock);
out:
	m0_db_pair_fini(pair);
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)lid, rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_layout_add(struct m0_layout *l,
			      struct m0_db_tx *tx, struct m0_db_pair *pair)
{
	m0_bcount_t recsize;
	int         rc;

	M0_PRE(m0_layout__invariant(l));
	M0_PRE(tx != NULL);
	M0_PRE(pair != NULL);

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	m0_mutex_lock(&l->l_lock);
	recsize = l->l_ops->lo_recsize(l);
	rc = pair_init(pair, l, tx, M0_LXO_DB_ADD, recsize);
	if (rc == 0) {
		rc = m0_table_insert(tx, pair);
		if (rc != 0)
			m0_layout__log("m0_layout_add",
				       "m0_table_insert() failed", l->l_id, rc);
		m0_db_pair_fini(pair);
	} else
		m0_layout__log("m0_layout_add", "pair_init() failed",
			       l->l_id, rc);
	m0_mutex_unlock(&l->l_lock);
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)l->l_id, rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_layout_update(struct m0_layout *l,
				 struct m0_db_tx *tx, struct m0_db_pair *pair)
{
	m0_bcount_t recsize;
	int         rc;

	M0_PRE(m0_layout__invariant(l));
	M0_PRE(tx != NULL);
	M0_PRE(pair != NULL);

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	m0_mutex_lock(&l->l_lock);
	recsize = l->l_ops->lo_recsize(l);
	rc = pair_init(pair, l, tx, M0_LXO_DB_UPDATE, recsize);
	if (rc == 0) {
		if (M0_FI_ENABLED("table_update_err"))
			{ rc = L_TABLE_UPDATE_ERR; goto err1_injected; }
		rc = m0_table_update(tx, pair);
err1_injected:
		if (rc != 0)
			m0_layout__log("m0_layout_update",
				       "m0_table_update() failed", l->l_id, rc);
		m0_db_pair_fini(pair);
	} else
		m0_layout__log("m0_layout_update",
			       "pair_init() failed", l->l_id, rc);
	m0_mutex_unlock(&l->l_lock);
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)l->l_id, rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_layout_delete(struct m0_layout *l,
				 struct m0_db_tx *tx, struct m0_db_pair *pair)
{
	m0_bcount_t recsize;
	int         rc;

	M0_PRE(m0_layout__invariant(l));
	M0_PRE(tx != NULL);
	M0_PRE(pair != NULL);

	M0_ENTRY("lid %llu", (unsigned long long)l->l_id);
	m0_mutex_lock(&l->l_lock);
	if (l->l_user_count > 0) {
		M0_LOG(M0_ERROR, "lid %llu, user_count %lu, Invalid "
		       "user_count, rc %d", (unsigned long long)l->l_id,
		       (unsigned long)l->l_user_count, -EPROTO);
		m0_mutex_unlock(&l->l_lock);
		return M0_ERR(-EPROTO);
	}

	recsize = l->l_ops->lo_recsize(l);
	rc = pair_init(pair, l, tx, M0_LXO_DB_DELETE, recsize);
	if (rc == 0) {
		rc = m0_table_delete(tx, pair);
		if (rc != 0)
			m0_layout__log("m0_layout_delete",
				       "m0_table_delete() failed", l->l_id, rc);
		m0_db_pair_fini(pair);
	} else
		m0_layout__log("m0_layout_delete",
			       "pair_init() failed", l->l_id, rc);
	m0_mutex_unlock(&l->l_lock);
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)l->l_id, rc);
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end group LayoutDBDFS */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
