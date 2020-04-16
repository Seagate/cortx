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
 * Original author: Trupti Patil <trupti_patil@xyratex.com>
 * Original creation date: 11/16/2011
 */

/**
 * @addtogroup list_enum
 *
 * A layout with list enumeration type contains list of component
 * object identifiers in itself.
 * @{
 */

#include "lib/errno.h"
#include "lib/vec.h"
#include "lib/memory.h"
#include "lib/misc.h" /* m0_forall() */
#include "lib/bob.h"
#include "lib/finject.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LAYOUT
#include "lib/trace.h"

#include "mero/magic.h"
#include "fid/fid.h"  /* m0_fid_is_valid() */
#include "layout/layout_internal.h"
#include "layout/list_enum.h"

static const struct m0_bob_type list_bob = {
	.bt_name         = "list_enum",
	.bt_magix_offset = offsetof(struct m0_layout_list_enum, lle_magic),
	.bt_magix        = M0_LAYOUT_LIST_ENUM_MAGIC,
	.bt_check        = NULL
};

M0_BOB_DEFINE(static, &list_bob, m0_layout_list_enum);

struct list_schema_data {
	/** Table to store COB lists for all the layouts with LIST enum type. */
	struct m0_table  lsd_cob_lists;
};

/**
 * cob_lists table.
 *
 * @note This structure needs to be maintained as 8 bytes aligned.
 */
struct cob_lists_key {
	/** Layout id, value obtained from m0_layout::l_id. */
	uint64_t  clk_lid;

	/** Index for the COB from the layout it is part of. */
	uint32_t  clk_cob_index;

	/** Padding to make the structure 8 bytes aligned. */
	uint32_t  clk_pad;
};

struct cob_lists_rec {
	/** COB identifier. */
	struct m0_fid  clr_cob_id;
};

/**
 * Compare cob_lists table keys.
 * This is a 3WAY comparison.
 */
static int lcl_key_cmp(struct m0_table *table,
		       const void *key0, const void *key1)
{
	const struct cob_lists_key *k0 = key0;
	const struct cob_lists_key *k1 = key1;

	return M0_3WAY(k0->clk_lid, k1->clk_lid) ?:
                M0_3WAY(k0->clk_cob_index, k1->clk_cob_index);
}

/** table_ops for cob_lists table. */
static const struct m0_table_ops cob_lists_table_ops = {
	.to = {
		[TO_KEY] = {
			.max_size = sizeof(struct cob_lists_key)
		},
		[TO_REC] = {
			.max_size = sizeof(struct cob_lists_rec)
		}
	},
	.key_cmp = lcl_key_cmp
};

static bool list_allocated_invariant(const struct m0_layout_list_enum *le)
{
	return
		m0_layout_list_enum_bob_check(le) &&
		le->lle_nr == 0 &&
		le->lle_list_of_cobs == NULL;
}

static bool list_invariant(const struct m0_layout_list_enum *le)
{
	return
		m0_layout_list_enum_bob_check(le) &&
		le->lle_nr != 0 &&
		le->lle_list_of_cobs != NULL &&
		m0_forall(i, le->lle_nr,
			  m0_fid_is_valid(&le->lle_list_of_cobs[i])) &&
		m0_layout__enum_invariant(&le->lle_base);
}

static const struct m0_layout_enum_ops list_enum_ops;

/** Implementation of leto_allocate for LIST enumeration type. */
static int list_allocate(struct m0_layout_domain *dom,
			 struct m0_layout_enum **out)
{
	struct m0_layout_list_enum *list_enum;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(out != NULL);

	M0_ENTRY();

	if (M0_FI_ENABLED("mem_err")) { list_enum = NULL; goto err1_injected; }
	M0_ALLOC_PTR(list_enum);
err1_injected:
	if (list_enum == NULL) {
		m0_layout__log("list_allocate", "M0_ALLOC_PTR() failed",
			       LID_NONE, -ENOMEM);
		return M0_ERR(-ENOMEM);
	}
	m0_layout__enum_init(dom, &list_enum->lle_base,
			     &m0_list_enum_type, &list_enum_ops);
	m0_layout_list_enum_bob_init(list_enum);
	M0_POST(list_allocated_invariant(list_enum));
	*out = &list_enum->lle_base;
	M0_LEAVE("list enum pointer %p", list_enum);
	return 0;
}

/** Implementation of leo_delete for LIST enumeration type. */
static void list_delete(struct m0_layout_enum *e)
{
	struct m0_layout_list_enum *list_enum;

	list_enum = bob_of(e, struct m0_layout_list_enum,
			   lle_base, &list_bob);
	M0_PRE(list_allocated_invariant(list_enum));

	M0_ENTRY("enum_pointer %p", e);
	m0_layout_list_enum_bob_fini(list_enum);
	m0_layout__enum_fini(&list_enum->lle_base);
	m0_free(list_enum);
	M0_LEAVE();
}

/* Populates the allocated list enum object using the supplied arguemnts. */
static int list_populate(struct m0_layout_list_enum *list_enum,
			 struct m0_fid *cob_list, uint32_t nr)
{
	M0_PRE(list_allocated_invariant(list_enum));
	M0_PRE(cob_list != NULL);

	if (nr == 0) {
		M0_LOG(M0_ERROR,
			"list_enum %p, Invalid attributes (nr = 0), rc %d",
		       list_enum, -EPROTO);
		return M0_ERR(-EPROTO);
	}
	list_enum->lle_nr = nr;
	list_enum->lle_list_of_cobs = cob_list;
	M0_POST(list_invariant(list_enum));
	return 0;
}

M0_INTERNAL int m0_list_enum_build(struct m0_layout_domain *dom,
				   struct m0_fid *cob_list, uint32_t nr,
				   struct m0_layout_list_enum **out)
{
	struct m0_layout_enum      *e;
	struct m0_layout_list_enum *list_enum;
	uint32_t                    i;
	int                         rc;

	M0_PRE(out != NULL);

	M0_ENTRY("domain %p", dom);
	if (M0_FI_ENABLED("fid_invalid_err")) { goto err1_injected; }
	for (i = 0; i < nr; ++i) {
		if (!m0_fid_is_valid(&cob_list[i])) {
err1_injected:
			m0_layout__log("m0_list_enum_build", "fid invalid",
				       LID_NONE, -EPROTO);
			return M0_ERR(-EPROTO);
		}
	}

	rc = list_allocate(dom, &e);
	if (rc == 0) {
		list_enum = bob_of(e, struct m0_layout_list_enum,
				   lle_base, &list_bob);
		rc = list_populate(list_enum, cob_list, nr);
		if (rc == 0)
			*out = list_enum;
		else
			list_delete(e);
	}
	M0_POST(ergo(rc == 0, list_invariant(*out)));
	M0_LEAVE("domain %p, rc %d", dom, rc);
	return M0_RC(rc);
}

static struct m0_layout_list_enum
*enum_to_list_enum(const struct m0_layout_enum *e)
{
	struct m0_layout_list_enum *list_enum;

	list_enum = bob_of(e, struct m0_layout_list_enum,
			   lle_base, &list_bob);
	M0_ASSERT(list_invariant(list_enum));
	return list_enum;
}

/** Implementation of leo_fini for LIST enumeration type. */
static void list_fini(struct m0_layout_enum *e)
{
	struct m0_layout_list_enum *list_enum;
	uint64_t                    lid;

	M0_PRE(m0_layout__enum_invariant(e));

	lid = e->le_sl_is_set ? e->le_sl->sl_base.l_id : 0;
	M0_ENTRY("lid %llu, enum_pointer %p", (unsigned long long)lid, e);
	list_enum = enum_to_list_enum(e);
	m0_layout_list_enum_bob_fini(list_enum);
	m0_layout__enum_fini(&list_enum->lle_base);
	m0_free(list_enum->lle_list_of_cobs);
	m0_free(list_enum);
	M0_LEAVE();
}

/** Implementation of leto_register for LIST enumeration type. */
static int list_register(struct m0_layout_domain *dom,
			 const struct m0_layout_enum_type *et)
{
	struct list_schema_data *lsd;
	int                      rc;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(et != NULL);
	M0_PRE(IS_IN_ARRAY(et->let_id, dom->ld_enum));
	M0_PRE(dom->ld_type_data[et->let_id] == NULL);

	M0_ENTRY("Enum_type_id %lu", (unsigned long)et->let_id);

	if (M0_FI_ENABLED("mem_err")) { lsd = NULL; goto err1_injected; }
	M0_ALLOC_PTR(lsd);
err1_injected:
	if (lsd == NULL) {
		m0_layout__log("list_register", "M0_ALLOC_PTR() failed",
			       LID_NONE, -ENOMEM);
		return M0_ERR(-ENOMEM);
	}

	if (M0_FI_ENABLED("table_init_err"))
		{ rc = -EEXIST; goto err2_injected; }
	rc = m0_table_init(&lsd->lsd_cob_lists, dom->ld_dbenv,
			   "cob_lists", DEFAULT_DB_FLAG, &cob_lists_table_ops);
err2_injected:
	if (rc == 0)
		dom->ld_type_data[et->let_id] = lsd;
	else {
		m0_layout__log("list_register", "m0_table_init() failed",
			       LID_NONE, rc);
		m0_free(lsd);
	}
	M0_LEAVE("Enum_type_id %lu, rc %d", (unsigned long)et->let_id, rc);
	return M0_RC(rc);
}

/** Implementation of leto_unregister for LIST enumeration type. */
static void list_unregister(struct m0_layout_domain *dom,
			    const struct m0_layout_enum_type *et)
{
	struct list_schema_data *lsd;

	M0_PRE(m0_layout__domain_invariant(dom));
	M0_PRE(et != NULL);

	M0_ENTRY("Enum_type_id %lu", (unsigned long)et->let_id);
	lsd = dom->ld_type_data[et->let_id];
	m0_table_fini(&lsd->lsd_cob_lists);
	dom->ld_type_data[et->let_id] = NULL;
	m0_free(lsd);
	M0_LEAVE("Enum_type_id %lu", (unsigned long)et->let_id);
}

/** Implementation of leto_max_recsize() for LIST enumeration type. */
static m0_bcount_t list_max_recsize(void)
{
	return sizeof(struct cob_entries_header) +
		LDB_MAX_INLINE_COB_ENTRIES *sizeof(struct m0_fid);
}

static int noninline_read(struct m0_fid *cob_list,
			  struct m0_striped_layout *stl,
			  struct m0_db_tx *tx,
			  uint32_t idx_start,
			  uint32_t idx_end)
{
	struct list_schema_data *lsd;
	struct cob_lists_key     key;
	struct cob_lists_rec     rec;
	struct m0_db_pair        pair;
	struct m0_db_cursor      cursor;
	uint32_t                 i;
	int                      rc;

	M0_ENTRY("lid %llu, idx_start %lu, idx_end %lu",
		 (unsigned long long)stl->sl_base.l_id,
		 (unsigned long)idx_start,
		 (unsigned long)idx_end);
	lsd = stl->sl_base.l_dom->ld_type_data[m0_list_enum_type.let_id];
	M0_ASSERT(lsd != NULL);

	if (M0_FI_ENABLED("cursor_init_err"))
		{ rc = -ENOENT; goto err1_injected; }
	rc = m0_db_cursor_init(&cursor, &lsd->lsd_cob_lists, tx, 0);
err1_injected:
	if (rc != 0) {
		m0_layout__log("noninline_read",
			       "m0_db_cursor_init() failed",
			       stl->sl_base.l_id, rc);
		return M0_RC(rc);
	}

	key.clk_lid       = stl->sl_base.l_id;
	key.clk_cob_index = idx_start;
	m0_db_pair_setup(&pair, &lsd->lsd_cob_lists,
			 &key, sizeof key, &rec, sizeof rec);
	for (i = idx_start; i < idx_end; ++i) {
		if (M0_FI_ENABLED("cursor_get_err"))
			{ rc = -ENOMEM; goto err2_injected; }
		if (i == idx_start)
			rc = m0_db_cursor_get(&cursor, &pair);
		else
			rc = m0_db_cursor_next(&cursor, &pair);
err2_injected:
		if (rc != 0) {
			m0_layout__log("noninline_read",
				       "m0_db_cursor_get() failed",
				       key.clk_lid, rc);
			goto out;
		}

		if (M0_FI_ENABLED("invalid_fid_err")) { goto err3_injected; }
		if (!m0_fid_is_valid(&rec.clr_cob_id)) {
err3_injected:
			rc = -EPROTO;
			m0_layout__log("noninline_read",
				       "fid invalid", key.clk_lid, rc);
			goto out;
		}
		cob_list[i] = rec.clr_cob_id;
	}
out:
	m0_db_pair_fini(&pair);
	m0_db_cursor_fini(&cursor);
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)stl->sl_base.l_id, rc);
	return M0_RC(rc);
}

/** Implementation of leo_decode() for LIST enumeration type. */
static int list_decode(struct m0_layout_enum *e,
		       struct m0_bufvec_cursor *cur,
		       enum m0_layout_xcode_op op,
		       struct m0_db_tx *tx,
		       struct m0_striped_layout *stl)
{
	uint64_t                    lid;
	struct m0_layout_list_enum *list_enum;
	struct cob_entries_header  *ce_header;
	/* Number of cobs to be read from the buffer. */
	uint32_t                    num_inline;
	struct m0_fid              *cob_id;
	struct m0_fid              *cob_list;
	uint32_t                    i;
	int                         rc;

	M0_PRE(e != NULL);
	M0_PRE(cur != NULL);
	M0_PRE(m0_bufvec_cursor_step(cur) >= sizeof *ce_header);
	M0_PRE(M0_IN(op, (M0_LXO_DB_LOOKUP, M0_LXO_BUFFER_OP)));
	M0_PRE(ergo(op == M0_LXO_DB_LOOKUP, tx != NULL));
	M0_PRE(m0_layout__striped_allocated_invariant(stl));

	lid = stl->sl_base.l_id;
	ce_header = m0_bufvec_cursor_addr(cur);
	m0_bufvec_cursor_move(cur, sizeof *ce_header);
	M0_ENTRY("lid %llu, nr %lu", (unsigned long long)lid,
		 (unsigned long)ce_header->ces_nr);
	list_enum = bob_of(e, struct m0_layout_list_enum,
			   lle_base, &list_bob);
	M0_ASSERT(list_allocated_invariant(list_enum));

	if (M0_FI_ENABLED("mem_err")) { cob_list = NULL; goto err1_injected; }
	M0_ALLOC_ARR(cob_list, ce_header->ces_nr);
err1_injected:
	if (cob_list == NULL) {
		rc = -ENOMEM;
		m0_layout__log("list_decode", "M0_ALLOC_ARR() failed",
			       lid, rc);
		goto out;
	}
	rc = 0;
	num_inline = op == M0_LXO_BUFFER_OP ? ce_header->ces_nr :
		min_check(ce_header->ces_nr,
			  (uint32_t)LDB_MAX_INLINE_COB_ENTRIES);
	M0_ASSERT(m0_bufvec_cursor_step(cur) >= num_inline * sizeof *cob_id);

	M0_LOG(M0_DEBUG, "lid %llu, nr %lu, Start reading inline entries",
	       (unsigned long long)lid, (unsigned long)ce_header->ces_nr);
	for (i = 0; i < num_inline; ++i) {
		cob_id = m0_bufvec_cursor_addr(cur);
		m0_bufvec_cursor_move(cur, sizeof *cob_id);

		if (M0_FI_ENABLED("fid_invalid_err")) { goto err2_injected; }
		if (!m0_fid_is_valid(cob_id)) {
err2_injected:
			rc = -EPROTO;
			M0_LOG(M0_WARN, "fid invalid, i %lu", (unsigned long)i);
			goto out;
		}
		cob_list[i] = *cob_id;
	}

	if (ce_header->ces_nr > num_inline) {
		M0_ASSERT(op == M0_LXO_DB_LOOKUP);
		M0_LOG(M0_DEBUG,
			"lid %llu, nr %lu, Start reading noninline entries",
		       (unsigned long long)lid,
		       (unsigned long)ce_header->ces_nr);
		rc = noninline_read(cob_list, stl, tx, i, ce_header->ces_nr);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "noninline_read() failed");
			goto out;
		}
	}

	if (M0_FI_ENABLED("attr_err")) { ce_header->ces_nr = 0; }
	rc = list_populate(list_enum, cob_list, ce_header->ces_nr);
	if (rc != 0)
		M0_LOG(M0_ERROR, "list_populate() failed");
out:
	if (rc != 0)
		m0_free(cob_list);
	M0_POST(ergo(rc == 0, list_invariant(list_enum)));
	M0_POST(ergo(rc != 0, list_allocated_invariant(list_enum)));
	M0_LEAVE("lid %llu, rc %d", (unsigned long long)lid, rc);
	return M0_RC(rc);
}

static int noninline_write(const struct m0_layout_enum *e,
			   struct m0_db_tx *tx,
			   enum m0_layout_xcode_op op,
			   uint32_t idx_start)
{
	struct m0_layout_list_enum *list_enum;
	struct m0_fid              *cob_list;
	struct list_schema_data    *lsd;
	struct m0_db_cursor         cursor;
	struct cob_lists_key        key;
	struct cob_lists_rec        rec;
	struct m0_db_pair           pair;
	uint32_t                    i;
	int                         rc;

	M0_PRE(M0_IN(op, (M0_LXO_DB_ADD, M0_LXO_DB_DELETE)));

	list_enum = enum_to_list_enum(e);
	M0_ENTRY("lid %llu, idx_start %lu, idx_end %lu",
		 (unsigned long long)e->le_sl->sl_base.l_id,
		 (unsigned long)idx_start,
		 (unsigned long)list_enum->lle_nr);
	cob_list = list_enum->lle_list_of_cobs;
	lsd = e->le_sl->sl_base.l_dom->ld_type_data[m0_list_enum_type.let_id];
	M0_ASSERT(lsd != NULL);

	if (M0_FI_ENABLED("cursor_init_err"))
		{ rc = -ENOENT; goto err1_injected; }
	rc = m0_db_cursor_init(&cursor, &lsd->lsd_cob_lists, tx,
			       M0_DB_CURSOR_RMW);
err1_injected:
	if (rc != 0) {
		m0_layout__log("noninline_write",
			       "m0_db_cursor_init() failed",
			       (unsigned long long)e->le_sl->sl_base.l_id, rc);
		return M0_RC(rc);
	}

	key.clk_lid = e->le_sl->sl_base.l_id;
	for (i = idx_start; i < list_enum->lle_nr; ++i) {
		M0_ASSERT(m0_fid_is_valid(&cob_list[i]));
		key.clk_cob_index = i;
		m0_db_pair_setup(&pair, &lsd->lsd_cob_lists,
				 &key, sizeof key, &rec, sizeof rec);

		if (op == M0_LXO_DB_ADD) {
			rec.clr_cob_id = cob_list[i];

			if (M0_FI_ENABLED("cursor_add_err"))
				{ rc = -ENOENT; goto err2_injected; }
			rc = m0_db_cursor_add(&cursor, &pair);
err2_injected:
			if (rc != 0) {
				m0_layout__log("noninline_write",
					       "m0_db_cursor_add() failed",
					       key.clk_lid, rc);
				goto out;
			}
		} else if (op == M0_LXO_DB_DELETE) {
			if (M0_FI_ENABLED("cursor_get_err"))
				{ rc = -ENOENT; goto err3_injected; }
			if (i == idx_start)
				rc = m0_db_cursor_get(&cursor, &pair);
			else
				rc = m0_db_cursor_next(&cursor, &pair);
err3_injected:
			if (rc != 0) {
				m0_layout__log("noninline_write",
					       "m0_db_cursor_get() failed",
					       key.clk_lid, rc);
				goto out;
			}
			M0_ASSERT(m0_fid_eq(&rec.clr_cob_id, &cob_list[i]));

			if (M0_FI_ENABLED("cursor_del_err"))
				{ rc = -ENOMEM; goto err4_injected; }
			rc = m0_db_cursor_del(&cursor);
err4_injected:
			if (rc != 0) {
				m0_layout__log("noninline_write",
					       "m0_db_cursor_del() failed",
					       key.clk_lid, rc);
				goto out;
			}
		}
	}
out:
	m0_db_pair_fini(&pair);
	m0_db_cursor_fini(&cursor);
	M0_LEAVE("lid %llu, rc %d",
		 (unsigned long long)e->le_sl->sl_base.l_id, rc);
	return M0_RC(rc);
}

/** Implementation of leo_encode() for LIST enumeration type. */
static int list_encode(const struct m0_layout_enum *e,
		       enum m0_layout_xcode_op op,
		       struct m0_db_tx *tx,
		       struct m0_bufvec_cursor *out)
{
	struct m0_layout_list_enum *list_enum;
	/* Number of cobs to be written to the buffer. */
	uint32_t                    num_inline;
	struct cob_entries_header   ce_header;
	m0_bcount_t                 nbytes;
	uint64_t                    lid;
	uint32_t                    i;
	int                         rc;

	M0_PRE(e != NULL);
	M0_PRE(M0_IN(op, (M0_LXO_DB_ADD, M0_LXO_DB_UPDATE,
			  M0_LXO_DB_DELETE, M0_LXO_BUFFER_OP)));
	M0_PRE(ergo(op != M0_LXO_BUFFER_OP, tx != NULL));
	M0_PRE(out != NULL);
	M0_PRE(m0_bufvec_cursor_step(out) >= sizeof ce_header);

	list_enum = enum_to_list_enum(e);
	lid = e->le_sl->sl_base.l_id;
	M0_ENTRY("lid %llu, nr %lu", (unsigned long long)lid,
		 (unsigned long)list_enum->lle_nr);

	ce_header.ces_nr = list_enum->lle_nr;
	nbytes = m0_bufvec_cursor_copyto(out, &ce_header, sizeof ce_header);
	M0_ASSERT(nbytes == sizeof ce_header);

	num_inline = op == M0_LXO_BUFFER_OP ? list_enum->lle_nr :
		min_check(list_enum->lle_nr,
			  (uint32_t)LDB_MAX_INLINE_COB_ENTRIES);

	M0_ASSERT(m0_bufvec_cursor_step(out) >= num_inline *
					sizeof list_enum->lle_list_of_cobs[0]);

	M0_LOG(M0_DEBUG, "lid %llu, nr %lu, Start accepting inline entries",
	       (unsigned long long)lid, (unsigned long)list_enum->lle_nr);
	for (i = 0; i < num_inline; ++i) {
		nbytes = m0_bufvec_cursor_copyto(out,
					&list_enum->lle_list_of_cobs[i],
					sizeof list_enum->lle_list_of_cobs[i]);
		M0_ASSERT(nbytes == sizeof list_enum->lle_list_of_cobs[i]);
	}

	rc = 0;
	/*
	 * The auxiliary table viz. cob_lists is not to be modified for an
	 * update operation.
	 */
	if (list_enum->lle_nr > num_inline && op != M0_LXO_DB_UPDATE) {
		M0_ASSERT(op == M0_LXO_DB_ADD || op == M0_LXO_DB_DELETE);
		M0_LOG(M0_DEBUG,
			"lid %llu, nr %lu, Start writing noninline entries",
		       (unsigned long long)lid,
		       (unsigned long)list_enum->lle_nr);
		rc = noninline_write(e, tx, op, i);
		if (rc != 0)
			M0_LOG(M0_ERROR, "noninline_write() failed");
	}

	M0_LEAVE("lid %llu, rc %d", (unsigned long long)lid, rc);
	return M0_RC(rc);
}

/** Implementation of leo_nr for LIST enumeration. */
static uint32_t list_nr(const struct m0_layout_enum *e)
{
	struct m0_layout_list_enum *list_enum;

	M0_PRE(e != NULL);

	M0_ENTRY("lid %llu, enum_pointer %p",
		 (unsigned long long)e->le_sl->sl_base.l_id, e);
	list_enum = enum_to_list_enum(e);
	M0_LEAVE("lid %llu, enum_pointer %p, nr %lu",
		 (unsigned long long)e->le_sl->sl_base.l_id, e,
		 (unsigned long)list_enum->lle_nr);
	return list_enum->lle_nr;
}

/** Implementation of leo_get for LIST enumeration. */
static void list_get(const struct m0_layout_enum *e, uint32_t idx,
		     const struct m0_fid *gfid, struct m0_fid *out)
{
	struct m0_layout_list_enum *list_enum;

	M0_PRE(e != NULL);
	/* gfid is ignored for the list enumeration type. */
	M0_PRE(out != NULL);

	M0_ENTRY("lid %llu, enum_pointer %p",
		 (unsigned long long)e->le_sl->sl_base.l_id, e);
	list_enum = enum_to_list_enum(e);
	M0_PRE(idx < list_enum->lle_nr);
	M0_ASSERT(m0_fid_is_valid(&list_enum->lle_list_of_cobs[idx]));
	*out = list_enum->lle_list_of_cobs[idx];
	M0_LEAVE("lid %llu, enum_pointer %p, fid_pointer %p",
		 (unsigned long long)e->le_sl->sl_base.l_id, e, out);
}

/** Implementation of leo_recsize() for list enumeration type. */
static m0_bcount_t list_recsize(struct m0_layout_enum *e)
{
	struct m0_layout_list_enum *list_enum;

	M0_PRE(e != NULL);

	list_enum = enum_to_list_enum(e);
	return sizeof(struct cob_entries_header) +
		min_check((uint32_t)LDB_MAX_INLINE_COB_ENTRIES,
			  list_enum->lle_nr) *
		sizeof(struct m0_fid);
}

static const struct m0_layout_enum_ops list_enum_ops = {
	.leo_nr      = list_nr,
	.leo_get     = list_get,
	.leo_recsize = list_recsize,
	.leo_fini    = list_fini,
	.leo_delete  = list_delete,
	.leo_decode  = list_decode,
	.leo_encode  = list_encode
};

static const struct m0_layout_enum_type_ops list_type_ops = {
	.leto_register    = list_register,
	.leto_unregister  = list_unregister,
	.leto_max_recsize = list_max_recsize,
	.leto_allocate    = list_allocate
};

struct m0_layout_enum_type m0_list_enum_type = {
	.let_name      = "list",
	.let_id        = 0,
	.let_ref_count = 0,
	.let_ops       = &list_type_ops
};

#undef M0_TRACE_SUBSYSTEM

/** @} end group list_enum */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
