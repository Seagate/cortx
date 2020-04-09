/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 3-Nov-2014
 *
 * Original 'm0t1fs' author: Anand Vidwansa <anand_vidwansa@xyratex.com>
 */

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_addb.h"
#include "clovis/pg.h"
#include "clovis/io.h"

#include "lib/buf.h"             /* M0_BUF_INIT_PTR */
#include "lib/memory.h"          /* m0_alloc, m0_free */
#include "lib/errno.h"           /* ENOMEM */
#include "fid/fid.h"             /* m0_fid */
#include "rm/rm.h"               /* stuct m0_rm_owner */
#include "sns/parity_repair.h"   /* m0_sns_repair_spare_map*/
#include "fd/fd.h"               /* m0_fd_fwd_map m0_fd_bwd_map */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"           /* M0_LOG */

/**
 * Explantation of object block, parity group unit and page.
 *
 * An object is viewed as a stream of blocks (1-dimension). When an object
 * is stored in mero, it is transformed into a 2-dimenional parity group
 * matrix. The size of object block is set by Clovis applications and should
 * be >= (1 >> CLOVIS_MIN_BUF_SHIFT) and <= parity group unit size. So a
 * number of blocks may be assemblied into one parity data unit and stored
 * together.
 *
 * page is something Clovis inherits from m0t1fs as m0t1fs uses kernel pages.
 * It serves as the in-memory representation of an object block.
 */

/** BOB types for the assorted parts of io requests and nwxfer */
const struct m0_bob_type pgiomap_bobtype;
const struct m0_bob_type dtbuf_bobtype;

/** BOB definitions for the assorted parts of io requests and nwxfer */
M0_BOB_DEFINE(M0_INTERNAL, &pgiomap_bobtype, pargrp_iomap);
M0_BOB_DEFINE(M0_INTERNAL, &dtbuf_bobtype,   data_buf);

/** BOB initialisation for the assorted parts of io requests and nwxfer */
const struct m0_bob_type pgiomap_bobtype = {
	.bt_name         = "pargrp_iomap_bobtype",
	.bt_magix_offset = offsetof(struct pargrp_iomap, pi_magic),
	.bt_magix        = M0_CLOVIS_PGROUP_MAGIC,
	.bt_check        = NULL,
};

const struct m0_bob_type dtbuf_bobtype = {
	.bt_name         = "data_buf_bobtype",
	.bt_magix_offset = offsetof(struct data_buf, db_magic),
	.bt_magix        = M0_CLOVIS_DTBUF_MAGIC,
	.bt_check        = NULL,
};

/**
 * Finds the parity group associated with a given target offset.
 *
 * @param index   The target offset for intended IO.
 * @param req     IO-request holding information about IO.
 * @param tio_req The io-request for given target.
 * @param output  The parity group.
 */
static void pargrp_src_addr(m0_bindex_t                   index,
			    const struct m0_clovis_op_io *ioo,
			    const struct target_ioreq    *tio_req,
			    struct m0_pdclust_src_addr   *src)
{
	struct m0_pdclust_tgt_addr tgt;
	struct m0_pdclust_layout  *play;

	M0_PRE(ioo != NULL);
	M0_PRE(tio_req != NULL);
	M0_PRE(src != NULL);

	play = pdlayout_get(ioo);
	tgt.ta_obj = tio_req->ti_obj;
	tgt.ta_frame = index / layout_unit_size(play);
	m0_fd_bwd_map(pdlayout_instance(layout_instance(ioo)), &tgt, src);
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_id_find
 */
M0_INTERNAL uint64_t pargrp_id_find(m0_bindex_t	                  index,
				    const struct m0_clovis_op_io *ioo,
				    const struct ioreq_fop       *ir_fop)
{
	struct m0_pdclust_src_addr src;

	pargrp_src_addr(index, ioo, ir_fop->irf_tioreq, &src);
	return src.sa_group;
}

static inline m0_bindex_t gobj_offset(m0_bindex_t                 toff,
		                      struct pargrp_iomap        *map,
				      struct m0_pdclust_layout   *play,
				      struct m0_pdclust_src_addr *src)
{
	m0_bindex_t goff;

	M0_PRE(map  != NULL);
	M0_PRE(play != NULL);

	M0_ENTRY("grpid = %3"PRIu64", target_off = %3"PRIu64,
		  map->pi_grpid, toff);

	goff = map->pi_grpid * data_size(play) +
	       src->sa_unit * layout_unit_size(play) +
	       toff % layout_unit_size(play);

	M0_LEAVE("global file offset = %3"PRIu64, goff);
	return goff;
}

static inline bool is_page_read(struct data_buf *dbuf)
{
	return M0_RC(dbuf->db_flags & PA_READ &&
		dbuf->db_tioreq != NULL && dbuf->db_tioreq->ti_rc == 0);
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::data_buf_invariant
 */
M0_INTERNAL bool data_buf_invariant(const struct data_buf *db)
{
	return M0_RC(db != NULL &&
	       data_buf_bob_check(db) &&
	       ergo(db->db_buf.b_addr != NULL, db->db_buf.b_nob > 0));
}

/**
 * Checks all of the data_bufs in the parity group iomap pass the
 * invariant check.
 * This is heavily based on m0t1fs/linux_kernel/file.c::data_buf_invariant_nr
 *
 * @param map The set of data/parity data_bufs that should be checked.
 * @return true or false.
 */
static bool data_buf_invariant_nr(const struct pargrp_iomap *map)
{
	uint32_t                  row;
	uint32_t                  col;
	struct m0_clovis_obj     *obj;
	struct m0_pdclust_layout *play;

	M0_ENTRY();

	M0_PRE(map != NULL);

	obj = map->pi_ioo->ioo_obj;
	play = pdlayout_get(map->pi_ioo);

	for (row = 0; row < data_row_nr(play, obj); ++row) {
		M0_ASSERT(row < map->pi_max_row);
		for (col = 0; col < data_col_nr(play); ++col) {
			M0_ASSERT(col < map->pi_max_col);
			if (map->pi_databufs[row][col] != NULL &&
				!data_buf_invariant(map->pi_databufs[row][col]))
				return M0_RC(false);
		}
	}

	if (map->pi_paritybufs != NULL) {
		for (row = 0; row < parity_row_nr(play, obj); ++row) {
			M0_ASSERT(row < map->pi_max_row);
			for (col = 0; col < parity_col_nr(play); ++col) {
				M0_ASSERT(row < map->pi_max_row);
				if (map->pi_paritybufs[row][col] != NULL &&
					!data_buf_invariant(map->pi_paritybufs
					[row][col]))
					return M0_RC(false);
			}
		}
	}

	return M0_RC(true);
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_iomap_invariant
 */
M0_INTERNAL bool pargrp_iomap_invariant(const struct pargrp_iomap *map)
{
	M0_ENTRY();

	if (map == NULL || map->pi_ioo == NULL)
		return false;

	return M0_RC(pargrp_iomap_bob_check(map) &&
		     map->pi_ops != NULL &&
		     map->pi_rtype < PIR_NR &&
		     map->pi_databufs != NULL &&
		     map->pi_ioo != NULL &&
		     ergo(m0_vec_count(&map->pi_ivec.iv_vec) > 0 &&
		     map->pi_ivec.iv_vec.v_nr >= 2,
		     m0_forall(i, map->pi_ivec.iv_vec.v_nr - 1,
			       map->pi_ivec.iv_index[i] +
			       map->pi_ivec.iv_vec.v_count[i] <=
			       map->pi_ivec.iv_index[i+1])) &&
		     data_buf_invariant_nr(map));
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_iomap_invariant_nr
 */
M0_INTERNAL bool pargrp_iomap_invariant_nr(const struct m0_clovis_op_io *ioo)
{
	M0_ENTRY();

	M0_PRE(ioo != NULL);

	return M0_RC(m0_forall(i, ioo->ioo_iomap_nr,
			       pargrp_iomap_invariant(ioo->ioo_iomaps[i])));
}

/**
 * Initialises a data_buf descriptor.
 * This is heavily based on m0t1fs/linux_kernel/file.c::data_buf_init
 *
 * @param buf[out] The data_buf decsriptor to initialise.
 * @param addr The address to use for data.
 * @param addr_size The size of the 'addr' memory.
 * @param flags Whether this data/memory is for file-data or parity.
 */
static void data_buf_init(struct data_buf *buf,
			  void            *addr,
			  uint64_t         addr_size,
			  uint64_t         flags)
{
	M0_ENTRY();

	M0_PRE(buf  != NULL);
	M0_PRE(addr != NULL);
	M0_PRE(addr_is_network_aligned(addr));

	data_buf_bob_init(buf);
	buf->db_flags = flags;
	m0_buf_init(&buf->db_buf, addr, addr_size);
	buf->db_tioreq = NULL;
	buf->db_maj_ele = M0_KEY_VAL_NULL;

	M0_LEAVE();
}

/**
 * Finalises a data_buf descriptor.
 * This is heavily based on m0t1fs/linux_kernel/file.c::data_buf_fini
 *
 * @param buf The data_buf to finalise.
 */
static void data_buf_fini(struct data_buf *buf)
{
	M0_ENTRY();

	M0_PRE(buf != NULL);

	data_buf_bob_fini(buf);
	buf->db_flags = PA_NONE;

	M0_LEAVE();
}

/**
 * Finalises a data_buf descriptor and free the memory it describes.
 * This is heavily based on m0t1fs/linux_kernel/file.c::data_buf_dealloc_fini
 *
 * @param buf The data_buf to finalise.
 */
static void data_buf_dealloc_fini(struct data_buf *buf)
{
	M0_ENTRY("data_buf %p", buf);

	M0_PRE(data_buf_invariant(buf));

	if ((buf->db_flags & PA_APP_MEMORY) == 0) {
		m0_free_aligned(buf->db_buf.b_addr,
				buf->db_buf.b_nob,
				CLOVIS_NETBUF_SHIFT);

		m0_free_aligned(buf->db_auxbuf.b_addr,
				buf->db_auxbuf.b_nob,
				CLOVIS_NETBUF_SHIFT);
	}

	data_buf_fini(buf);
	m0_free(buf);

	M0_LEAVE();
}

/**
 * Initialises a data_buf descriptor, and allocate the memory.
 * This is heavily based on m0t1fs/linux_kernel/file.c::data_buf_alloc_init
 *
 * @param obj The object the memory should be allocated for, use to find the
 *            block size.
 * @param pattr Whether the memory should be used for data or parity.
 * @return The allocated data_buf, or NULL.
 */
static struct data_buf *data_buf_alloc_init(struct m0_clovis_obj *obj,
					    enum page_attr        pattr)
{
	struct m0_clovis *instance;
	struct data_buf  *buf;
	void             *addr;

	M0_ENTRY();

	M0_PRE(obj != NULL);
	instance = m0_clovis__obj_instance(obj);
	M0_PRE(instance != NULL);

	addr = m0_alloc_aligned(obj_buffer_size(obj), CLOVIS_NETBUF_SHIFT);
	if (addr == NULL) {
		M0_LOG(M0_ERROR, "Failed to get free page");
		return NULL;
	}

	M0_ALLOC_PTR(buf);
	if (buf == NULL) {
		m0_free_aligned(addr, obj_buffer_size(obj),
				CLOVIS_NETBUF_SHIFT);
		M0_LOG(M0_ERROR, "Failed to allocate data_buf");
		return NULL;
	}

	data_buf_init(buf, addr, obj_buffer_size(obj), pattr);

	M0_POST(data_buf_invariant(buf));
	M0_LEAVE();
	return buf;
}

static struct data_buf *data_buf_replicate_init(struct pargrp_iomap *map,
						int row, enum page_attr pattr)
{
	struct data_buf  *buf;
	void             *addr;
	size_t            size;

	M0_ENTRY();

	M0_ALLOC_PTR(buf);
	if (buf == NULL) {
		M0_LOG(M0_ERROR, "Failed to allocate data_buf");
		return NULL;
	}
	/*
	 * Column for data is always zero, as replication implies
	 * N == 1 in pdclust layout.
	 */
	addr = map->pi_databufs[row][0]->db_buf.b_addr;
	size = map->pi_databufs[row][0]->db_buf.b_nob;
	data_buf_init(buf, addr, size, pattr);

	M0_POST(data_buf_invariant(buf));
	M0_LEAVE();
	return buf;
}

/**
 * Counts the number of bytes in this vector needed to reach the next parity
 * group boundary.
 * This is heavily based on m0t1fs/linux_kernel/file.c::seg_collate
 *
 * @param map The Parity Group map.
 * @param cursor Where in the object we are currently.
 * @return The number of bytes to reach the next parity group boundary.
 */
static m0_bcount_t seg_collate(struct pargrp_iomap   *map,
			       struct m0_ivec_cursor *cursor)
{
	uint32_t                  seg;
	uint32_t                  cnt;
	m0_bindex_t               start;
	m0_bindex_t               grpend;
	m0_bcount_t               segcount;
	struct m0_pdclust_layout *play;

	M0_ENTRY();

	M0_PRE(cursor != NULL);
	M0_PRE_EX(pargrp_iomap_invariant(map));

	cnt    = 0;
	play   = pdlayout_get(map->pi_ioo);
	grpend = map->pi_grpid * data_size(play) + data_size(play);
	start  = m0_ivec_cursor_index(cursor);

	for (seg = cursor->ic_cur.vc_seg; start < grpend &&
	     seg < cursor->ic_cur.vc_vec->v_nr - 1; ++seg) {
		segcount = seg == cursor->ic_cur.vc_seg ?
			   m0_ivec_cursor_step(cursor) :
			   cursor->ic_cur.vc_vec->v_count[seg];

		if (start + segcount == map->pi_ioo->ioo_ext.iv_index[seg + 1]) {
			if (start + segcount >= grpend) {
				start = grpend;
				break;
			}
			start += segcount;
		} else
			break;

		++cnt;
	}

	if (cnt == 0)
		return 0;

	/* If this was last segment in vector, add its count too. */
	if (seg == cursor->ic_cur.vc_vec->v_nr - 1) {
		if (start + cursor->ic_cur.vc_vec->v_count[seg] >= grpend)
			start = grpend;
		else
			start += cursor->ic_cur.vc_vec->v_count[seg];
	}

	M0_LEAVE();
	return start - m0_ivec_cursor_index(cursor);
}

/**
 * Builds the iomap structure of row/columns for data/parity, one segment at
 * a time.
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_iomap_populate
 *
 * @param map[out] The map top populate.
 * @param ivec The extents of the global file we will operate on.
 * @param cursor Where in which extent we should start.
 * @return 0 for sucess, -errno otherwise.
 */
static int pargrp_iomap_populate(struct pargrp_iomap      *map,
				 const struct m0_indexvec *ivec,
				 struct m0_ivec_cursor    *cursor,
				 struct m0_bufvec_cursor   *buf_cursor)
{
	int                       rc;
	bool                      rmw = false;
	uint64_t                  seg;
	uint64_t                  size = 0;
	uint64_t                  grpsize;
	uint64_t                  pagesize;
	m0_bcount_t               count = 0;
	m0_bindex_t               endpos = 0;
	m0_bcount_t               segcount = 0;
	/* Number of pages _completely_ spanned by incoming io vector. */
	uint64_t                  nr = 0;
	/* Number of pages to be read + written for read-old approach. */
	uint64_t                  ro_page_nr;
	/* Number of pages to be read + written for read-rest approach. */
	uint64_t                  rr_page_nr;
	m0_bindex_t               grpstart;
	m0_bindex_t               grpend;
	m0_bindex_t               currindex;
	m0_bindex_t               startindex;
	struct m0_pdclust_layout *play;
	struct m0_clovis_op_io   *ioo;
	struct m0_clovis_op      *op;
	struct m0_clovis_obj     *obj;
	struct m0_clovis         *instance;

	M0_ENTRY("map %p, indexvec %p", map, ivec);

	M0_PRE(map  != NULL);
	M0_PRE(ivec != NULL);

	ioo = map->pi_ioo;
	obj = ioo->ioo_obj;
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);

	play     = pdlayout_get(ioo);
	grpsize  = data_size(play);
	grpstart = grpsize * map->pi_grpid;
	grpend   = grpstart + grpsize;
	pagesize = m0_clovis__page_size(ioo);

	/* For a write, if size of this map is less
	 * than parity group size, it is a read-modify-write.
	 *
	 * Note (Sining): as Clovis objects don't have attribute SIZE,
	 * rmw is true even when grpstart is larger than current object end.
	 * Does this cause any problem if returned 'read' pages are not
	 * zeroed?
	 */
	if (M0_IN(op->op_code, (M0_CLOVIS_OC_FREE, M0_CLOVIS_OC_WRITE))) {
		for (seg = cursor->ic_cur.vc_seg; seg < SEG_NR(ivec) &&
		     INDEX(ivec, seg) < grpend; ++seg) {
			currindex = seg == cursor->ic_cur.vc_seg ?
				    m0_ivec_cursor_index(cursor) :
				    INDEX(ivec, seg);
			size += min64u(seg_endpos(ivec, seg), grpend) -
				currindex;
		}

		if (size < grpsize)
			rmw = true;
	}
	if (op->op_code == M0_CLOVIS_OC_FREE && rmw)
		map->pi_trunc_partial = true;

	startindex = m0_ivec_cursor_index(cursor);
	M0_LOG(M0_INFO, "Group id %"PRIu64" is %s", map->pi_grpid,
	       rmw ? "rmw" : "aligned");

	for (seg = 0; !m0_ivec_cursor_move(cursor, count) &&
		      m0_ivec_cursor_index(cursor) < grpend;) {
		/*
		 * Skips the current segment if it is completely spanned by
		 * rounding up/down of earlier segment.
		 */
		if (map->pi_ops->pi_spans_seg(map,
			m0_ivec_cursor_index(cursor),
			m0_ivec_cursor_step(cursor)))
		{
			count = m0_ivec_cursor_step(cursor);
			continue;
		}

		INDEX(&map->pi_ivec, seg) = m0_ivec_cursor_index(cursor);
		endpos = min64u(grpend,
				m0_ivec_cursor_index(cursor)
				+ m0_ivec_cursor_step(cursor));
		segcount = seg_collate(map, cursor);
		if (segcount > 0)
			endpos = INDEX(&map->pi_ivec, seg) + segcount;
		COUNT(&map->pi_ivec, seg) = endpos - INDEX(&map->pi_ivec, seg);

		/*
		* If current segment is _partially_ spanned by previous
		* segment in pargrp_iomp::pi_ivec, start of segment is
		* rounded up to move to next page.
		*/
		/*
		 * FIXME: When indexvec is prepared with segments overlapping
		 * each other and either 1 or multiple segments span the
		 * boundary of the parity group, then the processing of indexvec
		 * fails to map them properly to map->pi_ivec.
		 * EOS-5083 is created to handle this.
		 */
		if (seg > 0 && INDEX(&map->pi_ivec, seg) <
			seg_endpos(&map->pi_ivec, seg - 1)) {
			m0_bindex_t newindex;

			newindex = m0_round_up(INDEX(&map->pi_ivec, seg) + 1,
					       pagesize);
			COUNT(&map->pi_ivec, seg) -=
				(newindex - INDEX(&map->pi_ivec, seg));
			INDEX(&map->pi_ivec, seg)  = newindex;
		}

		++map->pi_ivec.iv_vec.v_nr;
		M0_LOG(M0_DEBUG, "pre grpid = %"PRIu64" seg %"PRIu64
				 " = [%"PRIu64", +%"PRIu64")",
				 map->pi_grpid, seg,
				 INDEX(&map->pi_ivec, seg),
				 COUNT(&map->pi_ivec, seg));

		if (!(op->op_code == M0_CLOVIS_OC_READ &&
		      instance->m0c_config->cc_is_read_verify)) {
			/* if not in 'verify mode', ... */
			rc = map->pi_ops->pi_seg_process(map, seg, rmw,
					                 0, buf_cursor);
			if (rc != 0)
				return M0_ERR_INFO(rc, "seg_process failed");
		}

		INDEX(&map->pi_ivec, seg) =
			round_down(INDEX(&map->pi_ivec, seg), pagesize);
		COUNT(&map->pi_ivec, seg) = round_up(endpos, pagesize) -
				 INDEX(&map->pi_ivec, seg);
		M0_LOG(M0_DEBUG, "post grpid = %"PRIu64" seg %"PRIu64
				 " = [%"PRIu64", +%"PRIu64")",
				 map->pi_grpid, seg,
				 INDEX(&map->pi_ivec, seg),
				 COUNT(&map->pi_ivec, seg));

		count = endpos - m0_ivec_cursor_index(cursor);
		M0_LOG(M0_DEBUG, "cursor will advance +%"PRIu64" from %"PRIu64,
				 count, m0_ivec_cursor_index(cursor));
		++seg;
	}

	/* In 'verify mode', read all data units in this parity group */
	if (op->op_code == M0_CLOVIS_OC_READ &&
	    instance->m0c_config->cc_is_read_verify) {
		M0_LOG(M0_DEBUG, "change ivec to [%"PRIu64", +%"PRIu64") "
				 "for group id %"PRIu64,
				 grpstart, grpsize, map->pi_grpid);

		/*
		 * Full parity group. Note: Clovis doesn't have
		 * object size attribute).
		 */
		SEG_NR(&map->pi_ivec)   = 1;
		INDEX(&map->pi_ivec, 0) = grpstart;
		COUNT(&map->pi_ivec, 0) = grpsize;

		rc = map->pi_ops->pi_seg_process(map, 0, rmw, startindex,
						 buf_cursor);
		if (rc != 0)
			return M0_ERR_INFO(rc, "seg_process failed");
	}

	/*
	 * Decides whether to undertake read-old approach or read-rest for
	 * an rmw IO request.
	 * By default, the segments in index vector pargrp_iomap::pi_ivec
	 * are suitable for read-old approach.
	 * Hence the index vector is changed only if read-rest approach
	 * is selected.
	 *
	 * For full page modifications that do not span the entire
	 * unit of a parity group, sharing a pointer with respective
	 * parity unit pages serves the purpose (and it's identical
	 * with what a normal write does).
	 * Part page modifications are not currently supported by clovis.
	 */
	if (!m0_pdclust_is_replicated(play) && (rmw ||
	     map->pi_trunc_partial)) {
		nr = map->pi_ops->pi_fullpages_find(map);

		/*
		* Can use number of data_buf structures instead of using
		* indexvec_page_nr().
		*/
		ro_page_nr = /* Number of pages to be read. */
			iomap_page_nr(map) +
			parity_units_page_nr(play, obj) +
			/* Number of pages to be written. */
			iomap_page_nr(map) +
			parity_units_page_nr(play, obj);

		rr_page_nr = /* Number of pages to be read. */
			page_nr(grpend - grpstart, obj) - nr +
			/* Number of pages to be written. */
			iomap_page_nr(map) +
			parity_units_page_nr(play, obj);

		if (rr_page_nr < ro_page_nr ||
		    map->pi_trunc_partial) {
			M0_LOG(M0_DEBUG, "[%p] Read-rest approach selected",
			       ioo);
			map->pi_rtype = PIR_READREST;
			rc = map->pi_ops->pi_readrest(map);
			if (rc != 0)
				return M0_ERR(rc);
		} else {
			M0_LOG(M0_DEBUG, "[%p] Read-old approach selected",
			       ioo);
			map->pi_rtype = PIR_READOLD;
			rc = map->pi_ops->pi_readold_auxbuf_alloc(map);
		}
		if (rc != 0)
			return M0_ERR_INFO(rc, "[%p] failed", ioo);
	}

	if (map->pi_ioo->ioo_pbuf_type == M0_CLOVIS_PBUF_DIR)
		rc = map->pi_ops->pi_paritybufs_alloc(map);
	/*
	 * In case of write IO, whether it's a rmw or otherwise, parity buffers
	 * share a pointer with data buffer.
	 */
	else if (map->pi_ioo->ioo_pbuf_type == M0_CLOVIS_PBUF_IND)
		rc = map->pi_ops->pi_data_replicate(map);

	M0_POST_EX(ergo(rc == 0, pargrp_iomap_invariant(map)));
	return M0_RC(rc);
}

/** @todo: all clovis buffers should be PA_FULLPAGE_MODIFY - rip all this out */
/**
 * Counts the number of buffers in the parity group map that have the
 * PA_FULLPAGE_MODIFY flag set.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_fullpages_count
 *
 * @param map The parity group io map to inspect.
 * @return The number of buffers in the parity group that have
 * PA_FULLPAGE_MODIFY set.
 */
static uint64_t pargrp_iomap_fullpages_count(struct pargrp_iomap *map)
{
	uint32_t                  row;
	uint32_t                  col;
	uint64_t                  nr = 0;
	struct m0_pdclust_layout *play;
	struct m0_clovis_obj     *obj;
	struct m0_clovis_op_io   *ioo;

	M0_ENTRY("map %p", map);

	M0_PRE(map != NULL);
	ioo = map->pi_ioo;
	obj = ioo->ioo_obj;
	M0_PRE_EX(pargrp_iomap_invariant(map));

	play = pdlayout_get(ioo);

	for (row = 0; row < data_row_nr(play, obj); ++row) {
		M0_ASSERT(row <= map->pi_max_row);
		for (col = 0; col < data_col_nr(play); ++col) {
			M0_ASSERT(col <= map->pi_max_col);
			if (map->pi_databufs[row][col] &&
			    map->pi_databufs[row][col]->db_flags &
			    PA_FULLPAGE_MODIFY)
				++nr;
		}
	}

	M0_LEAVE();
	return nr; /* Not M0_RC, nr is a uint64_t */
}

/**
 * Determines if the count bytes from index are covered by a single segment in
 * the iomap's index-vector.
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_iomap_spans_seg
 *
 * @param map The map to inspect.
 * @param index The starting offset.
 * @param count The number of bytes that should be covered.
 * @return true or false.
 */
static bool pargrp_iomap_spans_seg(struct pargrp_iomap *map,
				   m0_bindex_t index, m0_bcount_t count)
{
	uint32_t seg;

	M0_ENTRY();

	M0_PRE_EX(pargrp_iomap_invariant(map));

	for (seg = 0; seg < map->pi_ivec.iv_vec.v_nr; ++seg) {
		if (index >= INDEX(&map->pi_ivec, seg) &&
			index + count <= seg_endpos(&map->pi_ivec, seg))
			return M0_RC(true);
	}
	return M0_RC(false);
}

/**
 * Allocates data_buf structures as needed and populates the buffer flags.
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_iomap_init.
 *
 * @param map[out] The parity group map to be processed.
 * @param seg Which segment to start processing.
 * @param rmw Whether to skip on allocation as memory will be allocated
 *            once old data has been read.
 * @return 0 for success, -errno otherwise.
 */
static int pargrp_iomap_seg_process(struct pargrp_iomap *map,
				    uint64_t seg, bool rmw,
				    uint64_t skip_buf_index,
				    struct m0_bufvec_cursor *buf_cursor)
{
	int                       rc;
	int                       flags;
	bool                      ret;
	uint32_t                  row;
	uint32_t                  col;
	uint64_t                  count = 0;
	uint64_t                  pagesize;
	m0_bindex_t               start;
	m0_bindex_t               end;
	struct m0_ivec_cursor     cur;
	struct m0_pdclust_layout *play;
	struct m0_clovis_op_io   *ioo;
	struct m0_clovis_obj     *obj;
	struct m0_clovis_op      *op;
	m0_bindex_t               grp_size;

	M0_ENTRY("map %p, seg %"PRIu64", %s", map, seg,
		 rmw ? "rmw" : "aligned");

	M0_PRE(map != NULL);
	ioo = map->pi_ioo;
	op = &ioo->ioo_oo.oo_oc.oc_op;
	obj = ioo->ioo_obj;
	play = pdlayout_get(ioo);
	pagesize = m0_clovis__page_size(ioo);
	grp_size = data_size(play) * map->pi_grpid;

	m0_ivec_cursor_init(&cur, &map->pi_ivec);
	ret = m0_ivec_cursor_move_to(&cur, INDEX(&map->pi_ivec, seg));
	M0_ASSERT(!ret);

	/* process a page at each iteration */
	while (!m0_ivec_cursor_move(&cur, count)) {
		start = m0_ivec_cursor_index(&cur);
		end   = min64u(m0_round_up(start + 1, pagesize),
			       start + m0_ivec_cursor_step(&cur));
		count = end - start;

		flags = 0;
		if (M0_IN(op->op_code, (M0_CLOVIS_OC_WRITE, M0_CLOVIS_OC_FREE))) {
			flags |= PA_WRITE;
			flags |= count == pagesize ?
				 PA_FULLPAGE_MODIFY : PA_PARTPAGE_MODIFY;
			/*
			 * Even if PA_PARTPAGE_MODIFY flag is set in
			 * this buffer, the auxiliary buffer can not be
			 * allocated until ::pi_rtype is selected.
			 */
			if (rmw && (flags & PA_PARTPAGE_MODIFY))
				flags |= PA_READ;
		} else {
			/*
			 * For read IO requests, file_aio_read() has already
			 * delimited the index vector to EOF boundary.
			 */
			flags |= PA_READ;
		}

		page_pos_get(map, start, grp_size, &row, &col);
		M0_ASSERT(col <= map->pi_max_col);
		M0_ASSERT(row <= map->pi_max_row);

		if (start < skip_buf_index) {
			rc = map->pi_ops->pi_databuf_alloc(map, row, col, NULL);
		} else {
			/*
			 * When setting with read_verify mode, it requires to
			 * read the whole parity group while clovis application
			 * may only ask to read fewer units and allocate less
			 * memory so not able to hold the whole parity group.
			 * In this case, clovis has to allocate the buffers
			 * internally. So set buf_cursor to NULL when the cursor
			 * reaches the end of application's buffer.
			 */
			if (buf_cursor && m0_bufvec_cursor_move(buf_cursor, 0))
				buf_cursor = NULL;
			rc = map->pi_ops->pi_databuf_alloc(map, row, col,
							   buf_cursor);
			if (rc == 0 && buf_cursor)
				m0_bufvec_cursor_move(buf_cursor, count);
		}
		M0_LOG(M0_DEBUG, "alloc start %8"PRIu64" count %4"PRIu64
			" pgid %3"PRIu64" row %u col %u f 0x%x addr %p",
			 start, count, map->pi_grpid, row, col, flags,
			 map->pi_databufs[row][col] != NULL?
			 map->pi_databufs[row][col]->db_buf.b_addr:NULL);
		if (rc != 0)
			goto err;

		map->pi_databufs[row][col]->db_flags |= flags;

	}

	return M0_RC(0);
err:
	for (row = 0; row < data_row_nr(play, obj); ++row) {
		for (col = 0; col < data_col_nr(play); ++col) {
			if (map->pi_databufs[row][col] != NULL) {
				data_buf_dealloc_fini(
						map->pi_databufs[row][col]);
				map->pi_databufs[row][col] = NULL;
			}
		}
	}

	return M0_ERR(rc);
}

/**
 * Allocates this entry in the column/row table of the iomap.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_databuf_alloc
 *
 * @param map The io map in question.
 * @param row The row to allocate the data_buf in.
 * @param col The column to allocate the data_buf in.
 * @return 0 for success, or -ENOMEM.
 */
static int pargrp_iomap_databuf_alloc(struct pargrp_iomap *map,
				      uint32_t             row,
				      uint32_t             col,
				      struct m0_bufvec_cursor *data)
{
	struct m0_clovis_obj *obj;
	struct data_buf      *buf;
	uint64_t              flags;
	void                 *addr = NULL; /* required */

	M0_ENTRY("row %u col %u", row, col);

	M0_PRE(map != NULL);
	M0_PRE(col <= map->pi_max_col);
	M0_PRE(row <= map->pi_max_row);
	M0_PRE(map->pi_databufs[row][col] == NULL);

	obj = map->pi_ioo->ioo_obj;

	M0_ALLOC_PTR(buf);
	if (buf == NULL) {
		M0_LOG(M0_ERROR, "Failed to allocate data_buf");
		return M0_ERR(-ENOMEM);
	}

	if (data)
		addr = m0_bufvec_cursor_addr(data);

	flags = PA_NONE | PA_APP_MEMORY;
	/* Fall back to allocate-copy route */
	if (!addr_is_network_aligned(addr) || addr == NULL) {
		addr = m0_alloc_aligned(obj_buffer_size(obj),
				        CLOVIS_NETBUF_SHIFT);
		flags = PA_NONE;
	}

	data_buf_init(buf, addr, obj_buffer_size(obj), flags);
	M0_POST_EX(data_buf_invariant(buf));
	map->pi_databufs[row][col] = buf;

	return M0_RC(map->pi_databufs[row][col] == NULL ?  -ENOMEM : 0);
}

/**
 * Allocates an auxillary buffer for this row/column in the parity group map.
 * This is used for read modify write, and possibly parity validation.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_auxbuf_alloc
 *
 * @param map The io map in question.
 * @param row The row to allocate the data_buf in.
 * @param col The column to allocate the data_buf in.
 * @return 0 for success, or -ENOMEM.
 */
static uint64_t pargrp_iomap_auxbuf_alloc(struct pargrp_iomap *map,
					  uint32_t	       row,
					  uint32_t	       col)
{
	uint64_t pagesize;

	M0_ENTRY();
	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(map->pi_rtype == PIR_READOLD);

	pagesize = m0_clovis__page_size(map->pi_ioo);
	map->pi_databufs[row][col]->db_auxbuf.b_addr = (void *)
		m0_alloc_aligned(pagesize, CLOVIS_NETBUF_SHIFT);

	if (map->pi_databufs[row][col]->db_auxbuf.b_addr == NULL)
		return M0_ERR(-ENOMEM);

	map->pi_databufs[row][col]->db_auxbuf.b_nob = pagesize;

	return M0_RC(0);
}

/**
 * Allocates auxillary buffers for read-old-mode for each buffer in the
 * parity group.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_readold_auxbuf_alloc
 *
 * @param map The io map in question.
 * @return 0 on success, -errno otherwise.
 */
static int pargrp_iomap_readold_auxbuf_alloc(struct pargrp_iomap *map)
{
	int                       rc = 0;
	uint64_t                  start;
	uint64_t                  end;
	uint64_t                  count = 0;
	uint32_t                  row = 0;
	uint32_t                  col = 0;
	struct m0_ivec_cursor     cur;
	m0_bindex_t               grp_size;
	struct m0_pdclust_layout *play;
	uint64_t                  pagesize;

	M0_ENTRY("map %p", map);

	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(map->pi_rtype == PIR_READOLD);
	play     = pdlayout_get(map->pi_ioo);
	grp_size = data_size(play) * map->pi_grpid;
	pagesize = m0_clovis__page_size(map->pi_ioo);
	m0_ivec_cursor_init(&cur, &map->pi_ivec);

	while (!m0_ivec_cursor_move(&cur, count)) {
		start = m0_ivec_cursor_index(&cur);
		end   = min64u(m0_round_up(start + 1, pagesize),
			       start + m0_ivec_cursor_step(&cur));
		count = end - start;
		page_pos_get(map, start, grp_size, &row, &col);

		if (map->pi_databufs[row][col] != NULL) {
			/*
			 * In Readold approach, all valid pages have to
			 * be read regardless of whether they are fully
			 * occupied or partially occupied.
			 * This is needed in order to calculate correct
			 * parity in differential manner.
			 * Also, read flag should be set only for pages
			 * which lie within end-of-file boundary.
			 */
			/*
			 * XXX: Clovis doesn't have object size attribute at
			 * this  moment. How to handle an op which writes
			 * beyond the 'end' of an object (with holes)?
			 */
			/* XXX: just send the write - the ioserver should
			 * allocate space. It looks like this code is
			 * switching to rmw if the write extends the file. */
			//if (end < inode->i_size ||
			//    (inode->i_size > 0 &&
			//     page_id(end - 1) == page_id(inode->i_size - 1)))
			//	map->pi_databufs[row][col]->db_flags |=
			//		PA_READ;

			map->pi_databufs[row][col]->db_flags |= PA_READ;

			rc = pargrp_iomap_auxbuf_alloc(map, row, col);
			if (rc != 0)
				return M0_ERR(rc);
		}
	}

	return M0_RC(rc);
}

/**
 * Reads the remainder of a parity group row so that parity can be generated
 * in a read-modify-write scenario.
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_iomap_readrest
 *
 * A read request from rmw IO request can lead to either:
 *
 * read_old - Read the old data for the extent spanned by current
 * IO request, along with the old parity unit. This approach needs
 * to calculate new parity in _iterative_ manner. This approach is
 * selected only if current IO extent lies within file size.
 *
 * read_rest - Read rest of the parity group, which is _not_ spanned
 * by current IO request, so that data for whole parity group can
 * be availble for parity calculation.
 * This approach reads the extent from start of parity group to the
 * point where a page is completely spanned by incoming IO request.
 *
 * Typically, the approach which leads to least size of data to be
 * read and written from server is selected.
 *
 *   N = 5, P = 1, K = 1, unit_size = 4k
 *   F => Fully occupied
 *   P' => Partially occupied
 *   # => Parity unit
 *   * => Spare unit
 *   x => Start of actual file extent.
 *   y => End of actual file extent.
 *   a => Rounded down value of x.
 *   b => Rounded up value of y.
 *
 *  Read-rest approach (XXX Sining: these diagrams don't seem right)
 *
 *   a    x
 *   +---+---+---+---+---+---+---+
 *   |   | P'| F | F | F | # | * |  PG#0
 *   +---+---+---+---+---+---+---+
 *   | F | F | F | F | F | # | * |  PG#1
 *   +---+---+---+---+---+---+---+
 *   | F | F | F | P'|   | # | * |  PG#2
 *   +---+---+---+---+---+---+---+
 *     N   N   N   N   N   K   P
 *                 y     b
 *
 *  Read-old approach
 *
 *   a     x
 *   +---+---+---+---+---+---+---+
 *   |   |   |   | P'| F | # | * |  PG#0
 *   +---+---+---+---+---+---+---+
 *   | F | F | F | F | F | # | * |  PG#1
 *   +---+---+---+---+---+---+---+
 *   | F | P'|   |   |   | # | * |  PG#2
 *   +---+---+---+---+---+---+---+
 *     N   N   N   N   N   K   P
 *                 y     b
 *
 */
static int pargrp_iomap_readrest(struct pargrp_iomap *map)
{
	int                       rc;
	uint32_t                  row = 0;
	uint32_t                  col = 0;
	uint32_t                  seg;
	uint32_t                  seg_nr;
	m0_bindex_t               grpstart;
	m0_bindex_t               grpend;
	m0_bindex_t               start;
	m0_bindex_t               end;
	m0_bcount_t               count = 0;
	struct m0_indexvec       *ivec;
	struct m0_ivec_cursor     cur;
	struct m0_pdclust_layout *play;
	struct m0_clovis_op_io   *ioo;
	uint64_t                  pagesize;

	M0_ENTRY("map %p", map);

	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(map->pi_rtype == PIR_READREST);

	ioo      = map->pi_ioo;
	play     = pdlayout_get(map->pi_ioo);
	ivec     = &map->pi_ivec;
	seg_nr   = map->pi_ivec.iv_vec.v_nr;
	grpstart = data_size(play) * map->pi_grpid;
	grpend   = grpstart + data_size(play);
	pagesize = m0_clovis__page_size(ioo);

	/* Extends first segment to align with start of parity group. */
	COUNT(ivec, 0) += (INDEX(ivec, 0) - grpstart);
	INDEX(ivec, 0)	= grpstart;

	/* Extends last segment to align with end of parity group. */
	COUNT(ivec, seg_nr - 1) = grpend - INDEX(ivec, seg_nr - 1);

	/*
	 * All io extents _not_ spanned by pargrp_iomap::pi_ivec
	 * need to be included so that _all_ pages from parity group
	 * are available to do IO.
	 */
	for (seg = 1; seg_nr > 2 && seg <= seg_nr - 2; ++seg) {
		if (seg_endpos(ivec, seg) < INDEX(ivec, seg + 1))
			COUNT(ivec, seg) += INDEX(ivec, seg + 1) -
					    seg_endpos(ivec, seg);
	}

	m0_ivec_cursor_init(&cur, &map->pi_ivec);

	while (!m0_ivec_cursor_move(&cur, count)) {

		start = m0_ivec_cursor_index(&cur);
		end   = min64u(m0_round_up(start + 1, pagesize),
			       start + m0_ivec_cursor_step(&cur));
		count = end - start;
		page_pos_get(map, start, grpstart, &row, &col);

		if (map->pi_databufs[row][col] == NULL) {
			rc = pargrp_iomap_databuf_alloc(map, row, col, NULL);
			if (rc != 0)
				return M0_ERR(rc);

			/* see comments in readold_xxx above */
			map->pi_databufs[row][col]->db_flags |= PA_READ;
		}
	}

	return M0_RC(0);
}

/**
 * Calculates parity of data buffers.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_parity_recalc
 *
 * @param map The parity group to calculate the parity for.
 */
static int pargrp_iomap_parity_recalc(struct pargrp_iomap *map)
{
	int                       rc;
	uint32_t                  row;
	uint32_t                  col;
	struct m0_buf            *dbufs;
	struct m0_buf            *pbufs;
	struct m0_pdclust_layout *play;
	struct m0_clovis         *instance;
	struct m0_clovis_op      *op;
	struct m0_clovis_op_io   *ioo;
	struct m0_clovis_obj     *obj;
	uint64_t                  pagesize;

	M0_ENTRY("map = %p", map);

	M0_PRE_EX(pargrp_iomap_invariant(map));

	ioo = map->pi_ioo;
	M0_PRE_EX(m0_clovis_op_io_invariant(ioo));
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	M0_PRE(instance != NULL);
	obj = ioo->ioo_obj;
	/*XXX (sining): pargrp_iomap_invariant has checked obj != NULL*/
	M0_PRE(obj != NULL);

	/* Allocate memory */
	play = pdlayout_get(map->pi_ioo);
	M0_ALLOC_ARR(dbufs, layout_n(play));
	M0_ALLOC_ARR(pbufs, layout_k(play));

	if (dbufs == NULL || pbufs == NULL) {
		rc = -ENOMEM;
		goto last;
	}
	pagesize = m0_clovis__page_size(ioo);

	if ((op->op_code == M0_CLOVIS_OC_WRITE && map->pi_rtype == PIR_NONE)
	    || map->pi_rtype == PIR_READREST) {
		void *zpage;

		zpage = m0_alloc_aligned(1ULL<<obj->ob_attr.oa_bshift,
					  CLOVIS_NETBUF_SHIFT);
		if (zpage == 0) {
			rc = -ENOMEM;
			goto last;
		}

		for (row = 0; row < data_row_nr(play, ioo->ioo_obj); ++row) {
			for (col = 0; col < data_col_nr(play); ++col)
				if (map->pi_databufs[row][col] != NULL) {
					dbufs[col] =
					    map->pi_databufs[row][col]->db_buf;
				} else {
					dbufs[col].b_addr = (void *)zpage;
					dbufs[col].b_nob  = pagesize;
				}

			for (col = 0; col < layout_k(play); ++col)
				pbufs[col] = map->pi_paritybufs[row][col]->
					     db_buf;

			m0_parity_math_calculate(parity_math(map->pi_ioo),
						 dbufs, pbufs);
		}

		rc = 0;
		m0_free_aligned(zpage, 1ULL<<obj->ob_attr.oa_bshift,
				CLOVIS_NETBUF_SHIFT);
		M0_LOG(M0_DEBUG, "Parity recalculated for %s",
		       map->pi_rtype == PIR_READREST ? "read-rest" :
		       "aligned write");

	} else {
		struct m0_buf *old;

		M0_ALLOC_ARR(old, layout_n(play));
		if (old == NULL) {
			rc = -ENOMEM;
			goto last;
		}

		for (row = 0; row < data_row_nr(play, ioo->ioo_obj); ++row) {
			for (col = 0; col < layout_k(play); ++col)
				pbufs[col] = map->pi_paritybufs[row][col]->
					db_buf;

			for (col = 0; col < data_col_nr(play); ++col) {
				/*
				 * During rmw-IO request with read-old approach
				 * we allocate primary and auxiliary buffers
				 * for those units from a parity group, that
				 * are spanned by input rmw-IO request. If
				 * these units belong to failed devices then
				 * during the degraded reading, primary buffers
				 * are allocated for rest of the units from the
				 * parity group in order to recover the failed
				 * units. Thus if a parity group is in dgmode,
				 * then every unit will have a primary buffer,
				 * but may not have an auxiliary buffer.
				 */
				if (map->pi_databufs[row][col] == NULL ||
				    map->pi_databufs[row][col]->
				     db_auxbuf.b_addr == NULL)
					continue;

				dbufs[col] = map->pi_databufs[row][col]->db_buf;
				old[col] = map->pi_databufs[row][col]->db_auxbuf;
				m0_parity_math_diff(parity_math(map->pi_ioo),
						    old, dbufs, pbufs, col);
			}

		}
		m0_free(old);
		rc = 0;
	}
last:
	m0_free(dbufs);
	m0_free(pbufs);

	return M0_RC(rc);
}

/**
 * Allocates parity buffers for this iomap.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_paritybufs_alloc
 *
 * @param map The parity group iomap in question.
 * @return 0 for success, -errno otherwise.
 */
static int pargrp_iomap_paritybufs_alloc(struct pargrp_iomap *map)
{
	uint32_t                  row;
	uint32_t                  col;
	struct m0_pdclust_layout *play;
	struct m0_clovis         *instance;
	struct m0_clovis_obj     *obj;
	struct m0_clovis_op      *op;
	unsigned int              op_code;
	struct data_buf          *dbuf;

	M0_ENTRY("[%p] map %p", map->pi_ioo, map);

	M0_PRE_EX(pargrp_iomap_invariant(map));
	obj = map->pi_ioo->ioo_obj;
	op = &map->pi_ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	op_code = op->op_code;

	play = pdlayout_get(map->pi_ioo);
	for (row = 0; row < parity_row_nr(play, obj); ++row) {
		for (col = 0; col < parity_col_nr(play); ++col) {
			map->pi_paritybufs[row][col] =
				data_buf_alloc_init(obj, PA_NONE);
			if (map->pi_paritybufs[row][col] == NULL)
				goto err;

			dbuf = map->pi_paritybufs[row][col];
			if (M0_IN(op_code, (M0_CLOVIS_OC_WRITE,
					    M0_CLOVIS_OC_FREE)))
				dbuf->db_flags |= PA_WRITE;

			if (map->pi_rtype == PIR_READOLD ||
			    (op_code == M0_CLOVIS_OC_READ &&
			     instance->m0c_config->cc_is_read_verify))
				dbuf->db_flags |= PA_READ;
		}
	}

	return M0_RC(0);
err:
	for (row = 0; row < parity_row_nr(play, obj); ++row) {
		for (col = 0; col < parity_col_nr(play); ++col)
			m0_free0(&map->pi_paritybufs[row][col]);
	}

	return M0_ERR(-ENOMEM);
}

static int pargrp_iomap_databuf_replicate(struct pargrp_iomap *map)
{
	int                       row;
	int                       col;
	struct m0_pdclust_layout *play;
	struct m0_clovis_obj     *obj;
	struct m0_clovis_op      *op;
	struct data_buf          *dbuf;

	M0_ENTRY("[%p] map %p", map->pi_ioo, map);

	M0_PRE_EX(pargrp_iomap_invariant(map));

	obj = map->pi_ioo->ioo_obj;
	op = &map->pi_ioo->ioo_oo.oo_oc.oc_op;

	M0_PRE(m0_clovis__is_update_op(op));

	play = pdlayout_get(map->pi_ioo);
	for (row = 0; row < parity_row_nr(play, obj); ++row) {
		for (col = 0; col < parity_col_nr(play); ++col) {
			if (map->pi_databufs[row][0] == NULL) {
				map->pi_paritybufs[row][col] =
					data_buf_alloc_init(obj, PA_NONE);
				if (map->pi_paritybufs[row][col] == NULL) {
					goto err;
				}
				continue;
			} else {
				map->pi_paritybufs[row][col] =
					data_buf_replicate_init(map, row,
								PA_NONE);
				if (map->pi_paritybufs[row][col] == NULL) {
					goto err;
				}
			}
			dbuf = map->pi_paritybufs[row][col];
			if (op->op_code == M0_CLOVIS_OC_WRITE)
				dbuf->db_flags |= PA_WRITE;
		}
	}

	return M0_RC(0);
err:
	for (; row > -1; --row) {
		for (col = 0; col < parity_col_nr(play); ++col) {
			if (map->pi_databufs[row][0] == NULL) {
				data_buf_dealloc_fini(map->
					pi_paritybufs[row][col]);
				map->pi_paritybufs[row][col] = NULL;
			} else
				m0_free0(&map->pi_paritybufs[row][col]);
		}
	}

	return M0_ERR(-ENOMEM);

}

/**
 * Marks [data | parity]units of a parity group to PA_READ_FAILED if any of
 * the units of same type 'type' is PA_READ_FAILED.
 *
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_pages_mark.
 *
 * @param map The parity map in question.
 * @param type The type of units in a group.
 * @return 0 for success, -errno otherwise.
 */
static int pargrp_iomap_pages_mark_as_failed(struct pargrp_iomap       *map,
					     enum m0_pdclust_unit_type  type)
{
	int                         rc = 0;
	uint32_t                    row;
	uint32_t                    row_nr;
	uint32_t                    col;
	uint32_t                    col_nr;
	struct data_buf          ***bufs;
	struct m0_pdclust_layout   *play;
	struct m0_clovis_op_io     *ioo;

	M0_PRE(map != NULL);
	M0_PRE(M0_IN(type, (M0_PUT_DATA, M0_PUT_PARITY)));
	M0_ENTRY("[%p] map %p", map->pi_ioo, map);

	ioo = map->pi_ioo;
	play = pdlayout_get(ioo);

	if (type == M0_PUT_DATA) {
		M0_ASSERT(map->pi_databufs != NULL);
		row_nr = data_row_nr(play, ioo->ioo_obj);
		col_nr = data_col_nr(play);
		bufs   = map->pi_databufs;
	} else {
		row_nr = parity_row_nr(play, ioo->ioo_obj);
		col_nr = parity_col_nr(play);
		bufs   = map->pi_paritybufs;
	}

	/*
	 * Allocates data_buf structures from either ::pi_databufs
	 * or ::pi_paritybufs array.
	 * The loop traverses the matrix, column (unit) by column (unit).
	 */
	for (col = 0; col < col_nr; ++col) {
		for (row = 0; row < row_nr; ++row) {
			/*
			 * If the page is marked as PA_READ_FAILED, all
			 * other pages belonging to the unit same as
			 * the failed one, are also marked as PA_READ_FAILED,
			 * hence the loop breaks from here.
			 */
			if (bufs[row][col] != NULL &&
			    bufs[row][col]->db_flags & PA_READ_FAILED)
				break;
		}

		if (row == row_nr)
			continue;

		for (row = 0; row < row_nr; ++row) {
			if (bufs[row][col] == NULL) {
				bufs[row][col] =
				    data_buf_alloc_init(ioo->ioo_obj, PA_NONE);
				if (bufs[row][col] == NULL) {
					rc = -ENOMEM;
					break;
				}
			}
			bufs[row][col]->db_flags |= PA_READ_FAILED;
		}
	}
	return M0_RC(rc);
}

/**
 * Gets the state of node where the unit(src) is.
 *
 * This is heavily based on m0t1fs/linux_kernel/file.c::unit_state.
 *
 * @param src The unit in question.
 * @param ioo The io operation.
 * @param state The pointer to the returned state.
 * @return 0 for success, -errno otherwise.
 */
static int unit_state(const struct m0_pdclust_src_addr *src,
		      struct m0_clovis_op_io *ioo,
		      enum m0_pool_nd_state *state)
{
	int			    rc;
	struct m0_pdclust_instance *play_instance;
	struct m0_pdclust_tgt_addr  tgt;
	struct m0_poolmach         *pm;

	M0_ENTRY();

	M0_PRE(src   != NULL);
	M0_PRE(ioo   != NULL);
	M0_PRE(state != NULL);

	play_instance = pdlayout_instance(layout_instance(ioo));
	m0_fd_fwd_map(play_instance, src, &tgt);

	pm = clovis_ioo_to_poolmach(ioo);
	M0_ASSERT(pm != NULL);
	rc = m0_poolmach_device_state(pm, tgt.ta_obj, state);
	if (rc != 0)
		return M0_ERR(rc);

	return M0_RC(rc);
}

/**
 * Gets the spare unit (used as new data unit) if SNS repair is done.
 *
 * This is heavily based on m0t1fs/linux_kernel/file.c::io_spare_map.
 *
 * @param map The parity map in question.
 * @param src The unit in question.
 * @param spare_slot Current spare unit.
 * @param spare_slot_prev Previous spare unit.
 * @param eff_state Returned state of current spare unit.
 * @return 0 for success, -errno otherwise.
 */
static int io_spare_map(const struct pargrp_iomap *map,
			const struct m0_pdclust_src_addr *src,
			uint32_t *spare_slot, uint32_t *spare_slot_prev,
			enum m0_pool_nd_state *eff_state)
{

	int			    rc;
	const struct m0_fid	   *gfid;
	struct m0_pdclust_layout   *play;
	struct m0_pdclust_instance *play_instance;
	struct m0_pdclust_src_addr  spare;
	struct m0_clovis_op_io     *ioo;
	struct m0_poolmach         *pm;

	M0_PRE(map != NULL);
	M0_PRE(map->pi_ioo != NULL);
	M0_PRE(src != NULL);
	M0_PRE(spare_slot != NULL);
	M0_PRE(spare_slot_prev != NULL);
	M0_PRE(eff_state != NULL);

	ioo = map->pi_ioo;
	play = pdlayout_get(ioo);
	play_instance = pdlayout_instance(layout_instance(ioo));
	gfid = &ioo->ioo_oo.oo_fid;
	pm = clovis_ioo_to_poolmach(ioo);
	M0_ASSERT(pm != NULL);
	rc = m0_sns_repair_spare_map(pm, gfid, play, play_instance,
				     src->sa_group, src->sa_unit,
				     spare_slot, spare_slot_prev);
	if (rc != 0)
		return M0_ERR(rc);

	/* Check if there is an effective failure of unit. */
	spare.sa_group = src->sa_group;
	spare.sa_unit = *spare_slot_prev;
	rc = unit_state(&spare, ioo, eff_state);
	return M0_RC(rc);
}

/**
 * Updates the page attributes in a degraded mode IO.
 *
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::mark_page_as_read_failed.
 *
 * @param map The parity map in question.
 * @param row The 'row' of this 'page'(unit)  in a parity group.
 * @param col The 'col' of this 'page'(unit)  in a parity group.
 * @return null
 */
static void mark_page_as_read_failed(struct pargrp_iomap *map, uint32_t row,
				     uint32_t col, enum page_attr page_type)
{
	struct m0_pdclust_layout  *play;
	struct m0_pdclust_src_addr src;
	enum m0_pool_nd_state      state;
	uint32_t		   spare_slot;
	uint32_t		   spare_prev;
	int			   rc;

	M0_PRE(M0_IN(page_type,(PA_DATA, PA_PARITY)));
	M0_PRE(map != NULL);
	M0_PRE(ergo(page_type == PA_DATA,
		    map->pi_databufs[row][col] != NULL));
	M0_PRE(ergo(page_type == PA_PARITY,
		    map->pi_paritybufs[row][col] != NULL));

	M0_ENTRY("pid=%"PRIu64", row = %u, col=%u, type=0x%x",
		 map->pi_grpid, row, col, page_type);

	play = pdlayout_get(map->pi_ioo);
	src.sa_group = map->pi_grpid;
	if (page_type == PA_DATA)
		src.sa_unit = col;
	else
		src.sa_unit = col + layout_n(play);

	rc = unit_state(&src, map->pi_ioo, &state);
	M0_ASSERT(rc == 0);
	if (state == M0_PNDS_SNS_REPAIRED) {
		/* gets the state of corresponding spare unit */
		rc = io_spare_map(map, &src, &spare_slot, &spare_prev,
				  &state);
		M0_ASSERT(rc == 0);
	}

	/*
	 * Checking state M0_PNDS_SNS_REBALANCING allows concurrent read during
	 * sns rebalancing in oostore mode, this works similar to M0_PNDS_FAILED.
	 * To handle concurrent i/o in non-oostore mode, some more changes are
	 * required to write data to live unit (on earlier failed device) if the
	 * device state is M0_PNDS_SNS_REBALANCING.
	 */
	if (M0_IN(state, (M0_PNDS_FAILED, M0_PNDS_OFFLINE,
			  M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REBALANCING))) {
		if (page_type == PA_DATA)
			map->pi_databufs[row][col]->db_flags |=
				PA_READ_FAILED;
		else
			map->pi_paritybufs[row][col]->db_flags |=
				PA_READ_FAILED;
	}

	M0_LEAVE();
}

/**
 * Mark those units on 'bad' devices and allocated neccessary resources
 * for later degraded IO. (Note: special treatment for device with
 * M0_PNDS_SNS_REPAIRED state as new IO requests will be issued accoding to
 * new layout.)
 *
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_dgmode_process.
 *
 * @param map   The parity map in question.
 * @param tio   The target IO request in question.
 * @param index The starting offset.
 * @param count  (not used at all. ???).
 * @return 0 for success, -errno otherwise.
 *
 * Code related to inode->i_size has been removed. (sining)
 */
static int pargrp_iomap_dgmode_process(struct pargrp_iomap *map,
		                       struct target_ioreq *tio,
		                       m0_bindex_t         *index,
			               uint32_t             count)
{
	int                         rc = 0;
	uint32_t                    row;
	uint32_t                    col;
	uint32_t		    spare_slot;
	uint32_t		    spare_slot_prev;
	m0_bindex_t                 goff;
	struct m0_pdclust_layout   *play;
	struct m0_pdclust_src_addr  src;
	struct m0_poolmach         *pm;
	enum m0_pool_nd_state	    dev_state;
	struct m0_clovis_op_io     *ioo;
	m0_bindex_t                 grp_size;

	M0_PRE_EX(map != NULL && pargrp_iomap_invariant(map));
	M0_PRE(tio   != NULL);
	M0_PRE(index != NULL);
	/*M0_PRE(count >  0);*/
	M0_ENTRY("grpid = %3"PRIu64", count = %u\n", map->pi_grpid, count);

	ioo = map->pi_ioo;
	play = pdlayout_get(ioo);
	pargrp_src_addr(index[0], ioo, tio, &src);
	M0_ASSERT(src.sa_group == map->pi_grpid);
	M0_ASSERT(src.sa_unit  <  layout_n(play) + layout_k(play));
	grp_size = data_size(play) * map->pi_grpid;

	/* Retrieve device state */
	pm = clovis_ioo_to_poolmach(ioo);
	M0_ASSERT(pm != NULL);
	rc = m0_poolmach_device_state(pm, tio->ti_obj, &dev_state);
	if (dev_state == M0_PNDS_SNS_REPAIRED) {
		/*
		 * If a device has just been repaired, a different layout
		 * (using spare units) is used and new requests need to be sent.
		 * But it's necessary to check whether the spare to which the
		 * original unit has been mapped during repair is alive. In case
		 * it's not online the degraded mode is invoked.
		 */
		rc = io_spare_map(map, &src, &spare_slot, &spare_slot_prev,
			          &dev_state);
		M0_ASSERT(rc == 0);
		if (dev_state == M0_PNDS_SNS_REPAIRED)
			return M0_RC(0);
	}

	map->pi_state = PI_DEGRADED;
	++map->pi_ioo->ioo_dgmap_nr;

	/* Failed segment belongs to a data unit. */
	if (src.sa_unit < layout_n(play)) {
		goff = gobj_offset(index[0], map, play, &src);
		page_pos_get(map, goff, grp_size, &row, &col);
		M0_ASSERT(map->pi_databufs[row][col] != NULL);
		map->pi_databufs[row][col]->db_flags |= PA_READ_FAILED;
	} else {
		/* Failed segment belongs to a parity unit. */
		row = page_nr(index[0], ioo->ioo_obj)
		    % page_nr(layout_unit_size(play), ioo->ioo_obj);
		col = src.sa_unit - layout_n(play);
		M0_ASSERT(map->pi_paritybufs[row][col] != NULL);
		map->pi_paritybufs[row][col]->db_flags |= PA_READ_FAILED;
	}

	/*
	 * Since m0_parity_math_recover() API will recover one or more
	 * _whole_ units, all pages from a failed unit can be marked as
	 * PA_READ_FAILED. These pages need not be read again.
	 */
	rc = pargrp_iomap_pages_mark_as_failed(map, M0_PUT_DATA);
	if (rc != 0)
		return M0_ERR(rc);

	/*
	 * If parity buffers are not allocated, they should be allocated
	 * since they are needed for recovering lost data.
	 */
	if (map->pi_paritybufs == NULL) {
		M0_ALLOC_ARR(map->pi_paritybufs,
			     parity_row_nr(play, ioo->ioo_obj));
		if (map->pi_paritybufs == NULL)
			return M0_ERR(-ENOMEM);

		for (row = 0; row < parity_row_nr(play, ioo->ioo_obj); ++row) {
			M0_ALLOC_ARR(map->pi_paritybufs[row],
				     parity_col_nr(play));
			if (map->pi_paritybufs[row] == NULL) {
				rc = -ENOMEM;
				goto par_fail;
			}
		}
	}
	rc = pargrp_iomap_pages_mark_as_failed(map, M0_PUT_PARITY);
	return M0_RC(rc);

par_fail:
	M0_ASSERT(rc != 0);
	for (row = 0; row < parity_row_nr(play, ioo->ioo_obj); ++row)
		m0_free0(&map->pi_paritybufs[row]);
	m0_free0(&map->pi_paritybufs);

	return M0_ERR(rc);
}

/**
 * Re-organises and allocates buffers for data and parity matrices.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_dgmode_recover.
 *
 * @param map The parity map in question.
 * @return 0 for success, -errno otherwise.
 *
 * Code related to inode->i_size has been removed. (sining)
 */
static int pargrp_iomap_dgmode_postprocess(struct pargrp_iomap *map)
{
	int                       rc = 0;
	uint32_t                  row;
	uint32_t                  col;
	struct data_buf          *dbuf;
	struct m0_pdclust_layout *play;
	struct m0_clovis         *instance;
	struct m0_clovis_op_io   *ioo;
	struct m0_clovis_obj     *obj;

	/*
	 * read_old: Reads unavailable data subject to condition that
	 *           data lies within file size. Parity is already read.
	 * read_rest: Reads parity units. Data for parity group is already
	 *            read.
	 * simple_read: Reads unavailable data subject to condition that
	 *              data lies within file size. Parity also has to be read.
	 */
	M0_PRE_EX(map != NULL && pargrp_iomap_invariant(map));
	M0_ENTRY("parity group id %3"PRIu64", map state = %d",
		 map->pi_grpid, map->pi_state);

	ioo = map->pi_ioo;
	play = pdlayout_get(ioo);
	obj = ioo->ioo_obj;
	instance = m0_clovis__op_instance(&ioo->ioo_oo.oo_oc.oc_op);

	/*
	 * Data matrix from parity group.
	 * The loop traverses column by column to be in sync with
	 * increasing file offset.
	 * This is necessary in order to generate correct index vector.
	 */
	for (col = 0; col < data_col_nr(play); ++col) {
		for (row = 0; row < data_row_nr(play, obj); ++row) {

			if (map->pi_databufs[row][col] != NULL &&
			    map->pi_databufs[row][col]->db_flags &
			    PA_READ_FAILED) {
					continue;
			} else {
				/*
				 * If current parity group map is degraded,
				 * then recovery is needed and a new
				 * data buffer needs to be allocated subject to
				 * limitation of file size.
				 */
				if (map->pi_state == PI_DEGRADED) {
					map->pi_databufs[row][col] =
					    data_buf_alloc_init(obj, PA_NONE);
					if (map->pi_databufs[row][col] ==
					    NULL) {
						rc = -ENOMEM;
						break;
					}
					mark_page_as_read_failed(map, row, col,
								 PA_DATA);
				}
				if (map->pi_state == PI_HEALTHY)
					continue;
			}
			dbuf = map->pi_databufs[row][col];

			if (dbuf->db_flags & PA_READ_FAILED
			    || is_page_read(dbuf)) {
				continue;
			}
			dbuf->db_flags |= PA_DGMODE_READ;
		}
	}

	if (rc != 0)
		goto err;
	/* If parity group is healthy, there is no need to read parity. */
	if (map->pi_state != PI_DEGRADED &&
	    !instance->m0c_config->cc_is_read_verify)
		return M0_RC(0);

	/*
	 * Populates the index vector if original read IO request did not
	 * span it. Since recovery is needed using parity algorithms,
	 * whole parity group needs to be read subject to file size limitation.
	 * Ergo, parity group index vector contains only one segment
	 * worth the parity group in size.
	 */
	INDEX(&map->pi_ivec, 0) = map->pi_grpid * data_size(play);
	COUNT(&map->pi_ivec, 0) = data_size(play);

	/*
	 * m0_0vec requires all members except the last one to have data count
	 * multiple of 4K.
	 */
	COUNT(&map->pi_ivec, 0) = round_up(COUNT(&map->pi_ivec, 0),
					   m0_clovis__page_size(ioo));
	SEG_NR(&map->pi_ivec)   = 1;
	/*indexvec_dump(&map->pi_ivec);*/

	/* parity matrix from parity group. */
	for (row = 0; row < parity_row_nr(play, obj); ++row) {
		for (col = 0; col < parity_col_nr(play); ++col) {

			if (map->pi_paritybufs[row][col] == NULL) {
				map->pi_paritybufs[row][col] =
					data_buf_alloc_init(obj, PA_NONE);
				if (map->pi_paritybufs[row][col] == NULL) {
					rc = -ENOMEM;
					break;
				}
			}
			dbuf = map->pi_paritybufs[row][col];
			mark_page_as_read_failed(map, row, col, PA_PARITY);
			/* Skips the page if it is marked as PA_READ_FAILED. */
			if (dbuf->db_flags & PA_READ_FAILED ||
			    is_page_read(dbuf)) {
				continue;
			}
			dbuf->db_flags |= PA_DGMODE_READ;
		}
	}
	if (rc != 0)
		goto err;
	return M0_RC(rc);
err:
	return M0_ERR(rc);
}

static uint32_t iomap_dgmode_recov_prepare(struct pargrp_iomap *map,
					   uint8_t *failed)
{
	struct m0_pdclust_layout *play;
	uint32_t                  col;
	uint32_t                  K = 0;

	play = pdlayout_get(map->pi_ioo);
	for (col = 0; col < data_col_nr(play); ++col) {
		if (map->pi_databufs[0][col] != NULL &&
		    map->pi_databufs[0][col]->db_flags &
		    PA_READ_FAILED) {
			failed[col] = 1;
			++K;
		}

	}
	for (col = 0; col < parity_col_nr(play); ++col) {
		M0_ASSERT(map->pi_paritybufs[0][col] != NULL);
		if (map->pi_paritybufs[0][col]->db_flags &
		    PA_READ_FAILED) {
			failed[col + layout_n(play)] = 1;
			++K;
		}
	}
	return K;
}

/**
 * Reconstructs the missing units of a parity group.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::pargrp_iomap_dgmode_recover.
 *
 * @param map The parity map in question.
 * @return 0 for success, -errno otherwise.
 */
static int pargrp_iomap_dgmode_recover(struct pargrp_iomap *map)
{
	int                       rc = 0;
	uint32_t                  row;
	uint32_t                  col;
	uint32_t                  K;
	uint64_t                  pagesize;
	void                     *zpage;
	struct m0_buf            *data;
	struct m0_buf            *parity;
	struct m0_buf             failed;
	struct m0_pdclust_layout *play;
	struct m0_clovis_op_io   *ioo;

	M0_ENTRY();
	M0_PRE_EX(map != NULL && pargrp_iomap_invariant(map));
	M0_PRE(map->pi_state == PI_DEGRADED);

	ioo = map->pi_ioo;
	play = pdlayout_get(ioo);
	pagesize = m0_clovis__page_size(ioo);

	M0_ALLOC_ARR(data, layout_n(play));
	if (data == NULL)
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory"
			       " for data buf");

	M0_ALLOC_ARR(parity, layout_k(play));
	if (parity == NULL) {
		m0_free(data);
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory"
			       " for parity buf");
	}

	zpage = m0_alloc_aligned(pagesize, CLOVIS_NETBUF_SHIFT);
	if (zpage == 0) {
		m0_free(data);
		m0_free(parity);
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate page.");
	}

	failed.b_nob = layout_n(play) + layout_k(play);

	failed.b_addr = m0_alloc(failed.b_nob);
	if (failed.b_addr == NULL) {
		m0_free(data);
		m0_free(parity);
		m0_free_aligned(zpage, pagesize, CLOVIS_NETBUF_SHIFT);
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory "
					    "for m0_buf");
	}

	K = iomap_dgmode_recov_prepare(map, (uint8_t *)failed.b_addr);
	if (K > layout_k(play)) {
		M0_LOG(M0_ERROR, "More failures in group %d",
				(int)map->pi_grpid);
		rc = -EIO;
		goto end;
	}
	if (parity_math(map->pi_ioo)->pmi_parity_algo ==
	    M0_PARITY_CAL_ALGO_REED_SOLOMON) {
		rc = m0_parity_recov_mat_gen(parity_math(map->pi_ioo),
				(uint8_t *)failed.b_addr);
		if (rc != 0)
			goto end;
	}

	/* Populates data and failed buffers. */
	for (row = 0; row < data_row_nr(play, ioo->ioo_obj); ++row) {
		for (col = 0; col < data_col_nr(play); ++col) {
			data[col].b_nob = pagesize;

			if (map->pi_databufs[row][col] == NULL) {
				data[col].b_addr = (void *)zpage;
				continue;
			}
			data[col].b_addr = map->pi_databufs[row][col]->
					   db_buf.b_addr;
		}

		for (col = 0; col < parity_col_nr(play); ++col) {
			M0_ASSERT(map->pi_paritybufs[row][col] != NULL);
			parity[col].b_addr =
			    map->pi_paritybufs[row][col]->db_buf.b_addr;
			parity[col].b_nob  = pagesize;
		}

		m0_parity_math_recover(parity_math(ioo), data,
				       parity, &failed, M0_LA_INVERSE);
	}

	if (parity_math(map->pi_ioo)->pmi_parity_algo ==
	    M0_PARITY_CAL_ALGO_REED_SOLOMON)
		m0_parity_recov_mat_destroy(parity_math(map->pi_ioo));
end:
	m0_free(data);
	m0_free(parity);
	m0_free(failed.b_addr);
	m0_free_aligned(zpage, pagesize, CLOVIS_NETBUF_SHIFT);

	return rc == 0 ?
		M0_RC(rc) :
		M0_ERR_INFO(-EIO,
			    "Number of failed units"
			    "in parity group exceeds the"
			    "total number of parity units"
			    "in a parity group %d.", (int)map->pi_grpid);
}

static bool crc_cmp(const struct m0_buf *val1, const struct m0_buf *val2)
{
	return *(uint32_t *)val1->b_addr == *(uint32_t *)val2->b_addr;
}

static void db_crc_set(struct data_buf *dbuf, uint32_t key,
		       struct m0_key_val *kv)
{
	m0_crc32(dbuf->db_buf.b_addr, dbuf->db_buf.b_nob,
		 (void *)&dbuf->db_crc);
	dbuf->db_key = key;
	m0_key_val_init(kv, &M0_BUF_INIT_PTR(&dbuf->db_key),
			&M0_BUF_INIT_PTR(&dbuf->db_crc));
}

static size_t offset_get(struct pargrp_iomap *map)
{
	struct m0_pdclust_layout *play;
	struct m0_clovis_op_io   *ioo;

	ioo  = map->pi_ioo;
	play = pdlayout_get(ioo);

	return m0_pdclust_unit_size(play) * map->pi_grpid;
}

static int pargrp_iomap_replica_elect(struct pargrp_iomap *map)
{
	int                       rc = 0;
	uint32_t                  row;
	uint32_t                  col;
	uint32_t                  crc_idx;
	uint32_t                  vote_nr;
	size_t                    offset;
	struct m0_pdclust_layout *play;
	struct m0_clovis         *instance;
	struct m0_clovis_op_io   *ioo;
	struct m0_clovis_op      *op;
	struct m0_key_val        *crc_arr;
	struct data_buf          *dbuf;
	struct data_buf          *pbuf;
	struct m0_key_val        *mjr;
	uint32_t                  unit_id;

	M0_ENTRY("map = %p", map);
	M0_PRE_EX(map != NULL && pargrp_iomap_invariant(map));

	ioo = map->pi_ioo;
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);

	if (map->pi_state != PI_DEGRADED &&
	    !(op->op_code == M0_CLOVIS_OC_READ &&
	      instance->m0c_config->cc_is_read_verify))
		return M0_RC(0);

	play = pdlayout_get(ioo);
	M0_ALLOC_ARR(crc_arr, layout_n(play) + layout_k(play));

	if (crc_arr == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto last;
	}

	/*
	 * Parity comparison is done only in the case of a replicated layout.
	 * Assume that for a 1 + K replicated layout K = 3. Then we have the
	 * following layout of pages for a single parity group.
	 *
	 * \ col
	 *row   +---+---+---+---+
	 *      | D | P | P | P |
	 *      +---+---+---+---+
	 *      | D | P | P | P |
	 *      +---+---+---+---+
	 *      | D | P | P | P |
	 *      +---+---+---+---+
	 * Each block represents a page sized data. All members of a row shall
	 * be identical in ideal case (due to replication). We calculate CRC
	 * for each page and check if all members of a row have an identical
	 * CRC. The page corresponding to the value that holds majority in
	 * a row shall be returned to user.
	 */
	for (row = 0, crc_idx = 0; row < data_row_nr(play, ioo->ioo_obj);
	     ++row, crc_idx = 0) {
		dbuf = map->pi_databufs[row][0];
		/* Degraded pages won't contend the election. */
		if (dbuf->db_flags & PA_READ_FAILED)
			dbuf->db_crc = 0;
		else {
			unit_id = 0;
			db_crc_set(dbuf, unit_id, &crc_arr[crc_idx++]);
		}
		for (col = 0; col < layout_k(play); ++col) {
			pbuf = map->pi_paritybufs[row][col];
			if (pbuf->db_flags & PA_READ_FAILED)
				pbuf->db_crc = 0;
			/*
			 * Only if a page is not degraded it contends
			 * the election.
			 */
			else {
				/* Shift by one to count for the data unit. */
				unit_id = col + 1;
				db_crc_set(pbuf, unit_id, &crc_arr[crc_idx++]);
			}
		}
		vote_nr = 0;
		mjr = m0_vote_majority_get(crc_arr, crc_idx, crc_cmp,
					   &vote_nr);
		if (mjr == NULL) {
			M0_LOG(M0_ERROR, "[%p] parity verification "
			       "failed for %llu [%u:%u], rc %d",
			       map->pi_ioo, (unsigned long long)map->pi_grpid,
			       row, col, -EIO);
			rc = M0_ERR(-EIO);
			goto last;
		}

		M0_ASSERT(vote_nr >= crc_idx/2 + 1);
		if (vote_nr < crc_idx) {
			/** TODO: initiate a write on affected page. */
			offset = offset_get(map);
			M0_LOG(M0_WARN, "Discrepancy observed at offset %d",
			       (int)offset);
			map->pi_is_corrupted = true;
			ioo->ioo_rect_needed = true;
		}
		/**
		 * Store the index of majority element with rest of the
		 * members.
		 */
		map->pi_databufs[row][0]->db_maj_ele = *mjr;
		for (col = 0; col < layout_k(play); ++col) {
			map->pi_paritybufs[row][col]->db_maj_ele = *mjr;
		}
	}
last:

	m0_free(crc_arr);
	M0_LOG(M0_DEBUG, "parity verified for %"PRIu64" rc=%d",
			 map->pi_grpid, rc);
	return M0_RC(rc);
}

static int pargrp_iomap_parity_verify(struct pargrp_iomap *map)
{
	int			  rc;
	uint32_t		  row;
	uint32_t		  col;
	uint64_t                  pagesize;
	struct m0_buf		 *dbufs;
	struct m0_buf		 *pbufs;
	struct m0_buf		 *old_pbuf;
	struct m0_pdclust_layout *play;
	struct page              *page;
	struct m0_clovis	 *instance;
	struct m0_clovis_op_io   *ioo;
	struct m0_clovis_op      *op;
	void                     *zpage;

	M0_ENTRY("map = %p", map);
	M0_PRE_EX(map != NULL && pargrp_iomap_invariant(map));

	ioo = map->pi_ioo;
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	pagesize = m0_clovis__page_size(ioo);

	if (!(op->op_code == M0_CLOVIS_OC_READ &&
	      instance->m0c_config->cc_is_read_verify))
		return M0_RC(0);

	play = pdlayout_get(ioo);
	M0_ALLOC_ARR(dbufs, layout_n(play));
	M0_ALLOC_ARR(pbufs, layout_k(play));
	zpage = m0_alloc_aligned(pagesize, CLOVIS_NETBUF_SHIFT);

	if (dbufs == NULL || pbufs == NULL || zpage == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto last;
	}

	/* temprary buf to hold parity */
	for (col = 0; col < layout_k(play); ++col) {
		page = m0_alloc_aligned(pagesize, CLOVIS_NETBUF_SHIFT);
		if (page == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto last;
		}

		pbufs[col].b_addr = (void *)page;
		pbufs[col].b_nob = pagesize;
	}

	for (row = 0; row < data_row_nr(play, ioo->ioo_obj); ++row) {
		/* data */
		for (col = 0; col < data_col_nr(play); ++col) {
			if (map->pi_databufs[row][col] != NULL)	{
				dbufs[col] =
					map->pi_databufs[row][col]->db_buf;
			} else {
				dbufs[col].b_addr = zpage;
				dbufs[col].b_nob  = pagesize;
			}
		}

		/* generate parity into new buf */
		m0_parity_math_calculate(parity_math(map->pi_ioo),
					 dbufs, pbufs);

		/* verify the parity */
		for (col = 0; col < layout_k(play); ++col) {
			old_pbuf = &map->pi_paritybufs[row][col]->db_buf;
			if (memcmp(pbufs[col].b_addr, old_pbuf->b_addr,
				   pagesize)) {
				M0_LOG(M0_ERROR, "[%p] parity verification "
				       "failed for %llu [%u:%u], rc %d",
				       map->pi_ioo,
				       (unsigned long long)map->pi_grpid, row,
				        col, -EIO);
				rc = M0_ERR(-EIO);
				goto last;
			}
			M0_LOG(M0_DEBUG,
			       "parity verified for %"PRIu64" [%u:%u]",
			       map->pi_grpid, row, col);
		}
	}

	rc = 0;
last:
	if (pbufs != NULL) {
		for (col = 0; col < layout_k(play); ++col) {
			if(pbufs[col].b_addr == NULL) continue;

			m0_free_aligned(pbufs[col].b_addr,
					pagesize, CLOVIS_NETBUF_SHIFT);
		}
	}
	m0_free(dbufs);
	m0_free(pbufs);
	m0_free_aligned(zpage, pagesize, CLOVIS_NETBUF_SHIFT);
	M0_LOG(M0_DEBUG, "parity verified for %"PRIu64" rc=%d",
			 map->pi_grpid, rc);
	return M0_RC(rc);
}

static const struct pargrp_iomap_ops iomap_ops = {
	.pi_populate               = pargrp_iomap_populate,
	.pi_spans_seg              = pargrp_iomap_spans_seg,
	.pi_fullpages_find         = pargrp_iomap_fullpages_count,
	.pi_seg_process            = pargrp_iomap_seg_process,
	.pi_databuf_alloc          = pargrp_iomap_databuf_alloc,
	.pi_readrest               = pargrp_iomap_readrest,
	.pi_readold_auxbuf_alloc   = pargrp_iomap_readold_auxbuf_alloc,
	.pi_parity_recalc          = pargrp_iomap_parity_recalc,
	.pi_parity_verify          = pargrp_iomap_parity_verify,
	.pi_parity_replica_verify  = pargrp_iomap_replica_elect,
	.pi_data_replicate         = pargrp_iomap_databuf_replicate,
	.pi_paritybufs_alloc       = pargrp_iomap_paritybufs_alloc,
	.pi_dgmode_process         = pargrp_iomap_dgmode_process,
	.pi_dgmode_postprocess     = pargrp_iomap_dgmode_postprocess,
	.pi_dgmode_recover         = pargrp_iomap_dgmode_recover,
	.pi_replica_recover        = pargrp_iomap_replica_elect,
};

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_iomap_init
 */
M0_INTERNAL int pargrp_iomap_init(struct pargrp_iomap    *map,
				  struct m0_clovis_op_io *ioo,
				  uint64_t                grpid)
{
	int                       rc;
	int                       row;
	struct m0_pdclust_layout *play;
	struct m0_clovis         *instance;
	struct m0_clovis_op      *op;

	M0_ENTRY("map = %p, op_io = %p, grpid = %"PRIu64, map, ioo, grpid);

	M0_PRE(map != NULL);
	M0_PRE(ioo != NULL);
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	M0_PRE(instance != NULL);

	pargrp_iomap_bob_init(map);
	play                  = pdlayout_get(ioo);
	map->pi_ops           = &iomap_ops;
	map->pi_rtype         = PIR_NONE;
	map->pi_grpid         = grpid;
	map->pi_ioo           = ioo;
	map->pi_state         = PI_HEALTHY;
	map->pi_paritybufs    = NULL;
	map->pi_trunc_partial = false;

	rc = m0_indexvec_alloc(
		&map->pi_ivec, page_nr(data_size(play), ioo->ioo_obj));
	if (rc != 0)
		goto fail;

	/*
	 * This number is incremented only when a valid segment
	 * is added to the index vector.
	 */
	map->pi_ivec.iv_vec.v_nr = 0;

	map->pi_max_row = data_row_nr(play, ioo->ioo_obj);
	map->pi_max_col = layout_n(play);

	M0_ALLOC_ARR(map->pi_databufs, data_row_nr(play, ioo->ioo_obj));
	if (map->pi_databufs == NULL)
		goto fail;

	for (row = 0; row < data_row_nr(play, ioo->ioo_obj); ++row) {
		M0_ALLOC_ARR(map->pi_databufs[row], layout_n(play));
		if (map->pi_databufs[row] == NULL)
			goto fail;
	}

	/*
	 * Whether direct or indirect parity allocation, meta level buffers
	 * are always allocated. Allocation of buffers holding actual parity
	 * is governed by M0_CLOVIS_PBUF_DIR/IND.
	 */
	if (M0_IN(ioo->ioo_pbuf_type,
		  (M0_CLOVIS_PBUF_DIR, M0_CLOVIS_PBUF_IND))) {
		M0_ALLOC_ARR(map->pi_paritybufs,
			     parity_row_nr(play, ioo->ioo_obj));
		if (map->pi_paritybufs == NULL)
			goto fail;

		for (row = 0; row < parity_row_nr(play, ioo->ioo_obj); ++row) {
			M0_ALLOC_ARR(map->pi_paritybufs[row],
				     parity_col_nr(play));
			if (map->pi_paritybufs[row] == NULL)
				goto fail;
		}
	}

	M0_POST_EX(pargrp_iomap_invariant(map));
	return M0_RC(0);

fail:
	m0_indexvec_free(&map->pi_ivec);

	if (map->pi_databufs != NULL) {
		for (row = 0; row < data_row_nr(play, ioo->ioo_obj); ++row)
			m0_free(&map->pi_databufs[row]);
		m0_free(map->pi_databufs);
	}
	if (map->pi_paritybufs != NULL) {
		for (row = 0; row < parity_row_nr(play, ioo->ioo_obj); ++row)
			m0_free(&map->pi_paritybufs[row]);
		m0_free(map->pi_paritybufs);
	}

	return M0_ERR(-ENOMEM);
}

static bool are_pbufs_allocated(struct m0_clovis_op_io *ioo)
{
	return ioo->ioo_pbuf_type == M0_CLOVIS_PBUF_DIR;
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::pargrp_iomap_fini
 */
/** TODO: arguments here can be reduced, map+obj are in ioo */
M0_INTERNAL void pargrp_iomap_fini(struct pargrp_iomap *map,
				   struct m0_clovis_obj *obj)
{
	uint32_t                  row;
	uint32_t                  col;
	uint32_t                  col_r; /* num of col in replicated layout */
	struct data_buf          *buf;
	struct m0_pdclust_layout *play;

	M0_ENTRY("map %p", map);

	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(obj != NULL);

	play          = pdlayout_get(map->pi_ioo);
	map->pi_ops   = NULL;
	map->pi_rtype = PIR_NONE;
	map->pi_state = PI_NONE;

	pargrp_iomap_bob_fini(map);
	m0_indexvec_free(&map->pi_ivec);

	for (row = 0; row < data_row_nr(play, obj); ++row) {
		if (map->pi_ioo->ioo_pbuf_type == M0_CLOVIS_PBUF_IND &&
		    map->pi_databufs[row][0] == NULL) {
			for (col_r = 0; col_r < parity_col_nr(play); ++col_r) {
				data_buf_dealloc_fini(map->
					pi_paritybufs[row][col_r]);
				map->pi_paritybufs[row][col_r] = NULL;
			}
		}

		for (col = 0; col < data_col_nr(play); ++col) {
			if (map->pi_databufs[row][col] != NULL) {
				data_buf_dealloc_fini(map->
					pi_databufs[row][col]);
				map->pi_databufs[row][col] = NULL;
			}
		}

		m0_free0(&map->pi_databufs[row]);
	}

	if (map->pi_paritybufs != NULL) {
		for (row = 0; row < parity_row_nr(play, obj); ++row) {
			for (col = 0; col < parity_col_nr(play); ++col) {
				buf = map->pi_paritybufs[row][col];
				if (buf != NULL) {
					if (are_pbufs_allocated(map->pi_ioo)) {
						data_buf_dealloc_fini(buf);
					} else {
						data_buf_fini(buf);
						m0_free(buf);
					}
					map->pi_paritybufs[row][col] = NULL;
				}
			}
			m0_free0(&map->pi_paritybufs[row]);
		}
	}

	m0_free0(&map->pi_databufs);
	m0_free0(&map->pi_paritybufs);
	map->pi_ioo = NULL;

	M0_LEAVE();
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
