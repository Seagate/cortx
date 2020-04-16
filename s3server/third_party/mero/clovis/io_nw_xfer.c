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

#include "lib/memory.h"          /* m0_alloc, m0_free */
#include "lib/errno.h"           /* ENOMEM */
#include "lib/finject.h"         /* M0_FI_ */
#include "fid/fid.h"             /* m0_fid */
#include "rpc/rpclib.h"          /* m0_rpc_ */
#include "lib/ext.h"             /* struct m0_ext */
#include "fop/fom_generic.h"     /* m0_rpc_item_generic_reply_rc */
#include "sns/parity_repair.h"   /* m0_sns_repair_spare_map*/
#include "fd/fd.h"               /* m0_fd_fwd_map m0_fd_bwd_map */
#include "clovis/clovis_addb.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"           /* M0_LOG */

/** BOB types for the assorted parts of io requests and nwxfer */
const struct m0_bob_type nwxfer_bobtype;
const struct m0_bob_type tioreq_bobtype;

/** BOB definitions for the assorted parts of io requests and nwxfer */
M0_BOB_DEFINE(M0_INTERNAL, &nwxfer_bobtype,  nw_xfer_request);
M0_BOB_DEFINE(M0_INTERNAL, &tioreq_bobtype,  target_ioreq);

/** BOB initialisation for the assorted parts of io requests and nwxfer */
const struct m0_bob_type nwxfer_bobtype = {
	.bt_name         = "nw_xfer_request_bobtype",
	.bt_magix_offset = offsetof(struct nw_xfer_request, nxr_magic),
	.bt_magix        = M0_CLOVIS_NWREQ_MAGIC,
	.bt_check        = NULL,
};

const struct m0_bob_type tioreq_bobtype = {
	.bt_name         = "target_ioreq",
	.bt_magix_offset = offsetof(struct target_ioreq, ti_magic),
	.bt_magix        = M0_CLOVIS_TIOREQ_MAGIC,
	.bt_check        = NULL,
};

static void clovis_to_op_io_map(const struct m0_clovis_op *op,
				struct m0_clovis_op_io *ioo)
{
	uint64_t oid  = m0_sm_id_get(&op->op_sm);
	uint64_t ioid = m0_sm_id_get(&ioo->ioo_sm);

	if (ioo->ioo_addb2_mapped++ == 0)
		M0_ADDB2_ADD(M0_AVI_CLOVIS_TO_IOO, oid, ioid);
}

static void clovis_op_io_to_rpc_map(const struct m0_clovis_op_io *ioo,
				    const struct m0_rpc_item     *item)
{
	uint64_t rid  = m0_sm_id_get(&item->ri_sm);
	uint64_t ioid = m0_sm_id_get(&ioo->ioo_sm);
	M0_ADDB2_ADD(M0_AVI_CLOVIS_IOO_TO_RPC, ioid, rid);
}

/**
 * Calculate the size needed for per-segment on-wire data integrity.
 * Note: Clovis leaves its applications to decide how to use locks on
 * objects, so it doesn't manage any lock. But file lock is needed
 * to calculate di size, a file lock is faked here to get di details.
 * Clearly, a more reliable way to get di size is needed.
 *
 * @param ioo The IO operation, to find the clovis instance.
 * @return the size of data integrity data.
 */
static uint32_t io_di_size(struct m0_clovis_op_io *ioo)
{
	uint32_t                rc = 0;
	const struct m0_fid    *fid;
	const struct m0_di_ops *di_ops;
	struct m0_file         *file;

	M0_PRE(ioo != NULL);

	#ifndef ENABLE_DATA_INTEGRITY
		return M0_RC(rc);
	#endif
	/* Get di details (hack!) by setting the dom be NULL*/
	file = &ioo->ioo_flock;
	fid = &ioo->ioo_oo.oo_fid;
	m0_file_init(file, fid, NULL, M0_DI_DEFAULT_TYPE);
	di_ops = file->fi_di_ops;

	if (di_ops->do_out_shift(file) == 0)
		return M0_RC(0);

	rc = di_ops->do_out_shift(file) * M0_DI_ELEMENT_SIZE;

	return M0_RC(rc);
}

static void parity_page_pos_get(struct pargrp_iomap *map,
				m0_bindex_t          index,
				uint32_t            *row,
				uint32_t            *col)
{
	uint64_t		  pg_id;
	struct m0_pdclust_layout *play;

	M0_PRE(map != NULL);
	M0_PRE(row != NULL);
	M0_PRE(col != NULL);

	play = pdlayout_get(map->pi_ioo);

	pg_id = page_id(index, map->pi_ioo->ioo_obj);
	*row  = pg_id % parity_row_nr(play, map->pi_ioo->ioo_obj);
	*col  = pg_id / parity_row_nr(play, map->pi_ioo->ioo_obj);
}

/**
 * Allocates an index and buffer vector(in structure dgmode_rwvec) for a
 * degraded mode IO.
 * This is heavily based on m0t1fs/linux_kernel/file.c::dgmode_rwvec_alloc_init
 *
 * @param ti The target_ioreq fop asking for the allocation.
 * @return 0 for success, or -errno.
 */
static int dgmode_rwvec_alloc_init(struct target_ioreq *ti)
{
	int                       rc;
	uint64_t                  cnt;
	struct dgmode_rwvec      *dg;
	struct m0_pdclust_layout *play;
	struct m0_clovis_op_io   *ioo;

	M0_ENTRY();
	M0_PRE(ti != NULL);
	M0_PRE(ti->ti_dgvec == NULL);

	ioo = bob_of(ti->ti_nwxfer, struct m0_clovis_op_io, ioo_nwxfer,
		     &ioo_bobtype);

	M0_ALLOC_PTR(dg);
	if (dg == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	play = pdlayout_get(ioo);
	dg->dr_tioreq = ti;

	cnt = page_nr(ioo->ioo_iomap_nr
		      * layout_unit_size(play)
		      * (layout_n(play) + layout_k(play)),
		      ioo->ioo_obj);
	rc  = m0_indexvec_alloc(&dg->dr_ivec, cnt);
	if (rc != 0)
		goto failed;

	M0_ALLOC_ARR(dg->dr_bufvec.ov_buf, cnt);
	if (dg->dr_bufvec.ov_buf == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	M0_ALLOC_ARR(dg->dr_bufvec.ov_vec.v_count, cnt);
	if (dg->dr_bufvec.ov_vec.v_count == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	M0_ALLOC_ARR(dg->dr_auxbufvec.ov_buf, cnt);
	if (dg->dr_auxbufvec.ov_buf == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	M0_ALLOC_ARR(dg->dr_auxbufvec.ov_vec.v_count, cnt);
	if (dg->dr_auxbufvec.ov_vec.v_count == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	M0_ALLOC_ARR(dg->dr_pageattrs, cnt);
	if (dg->dr_pageattrs == NULL) {
		rc = -ENOMEM;
		goto failed;
	}

	/*
	 * This value is incremented every time a new segment is added
	 * to this index vector.
	 */
	dg->dr_ivec.iv_vec.v_nr = 0;

	ti->ti_dgvec = dg;
	return M0_RC(0);
failed:
	ti->ti_dgvec = NULL;
	if (dg->dr_bufvec.ov_buf != NULL)
		m0_free(dg->dr_bufvec.ov_buf);
	if (dg->dr_bufvec.ov_vec.v_count != NULL)
		m0_free(dg->dr_bufvec.ov_vec.v_count);
	if (dg->dr_auxbufvec.ov_buf != NULL)
		m0_free(dg->dr_auxbufvec.ov_buf);
	if (dg->dr_auxbufvec.ov_vec.v_count != NULL)
		m0_free(dg->dr_auxbufvec.ov_vec.v_count);
	m0_free(dg);
	return M0_ERR(rc);
}

/**
 * Free index and buffer vector stored in structure dgmode_rwvec for a
 * degraded mode IO.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::dgmode_rwvec_dealloc_fini
 *
 * @param dg The dgmode_rwvec to be finalised.
 * @return NULL
 */
static void dgmode_rwvec_dealloc_fini(struct dgmode_rwvec *dg)
{
	M0_ENTRY();

	M0_PRE(dg != NULL);

	dg->dr_tioreq = NULL;
	/*
	 * Will need to go through array of parity groups to find out
	 * exact number of segments allocated for the index vector.
	 * Instead, a fixed number of segments is enough to avoid
	 * triggering the assert from m0_indexvec_free().
	 * The memory allocator knows the size of memory area held by
	 * dg->dr_ivec.iv_index and dg->dr_ivec.iv_vec.v_count.
	 */
	if (dg->dr_ivec.iv_vec.v_nr == 0)
		++dg->dr_ivec.iv_vec.v_nr;

	m0_indexvec_free(&dg->dr_ivec);
	m0_free(dg->dr_bufvec.ov_buf);
	m0_free(dg->dr_bufvec.ov_vec.v_count);
	m0_free(dg->dr_auxbufvec.ov_buf);
	m0_free(dg->dr_auxbufvec.ov_vec.v_count);
	m0_free(dg->dr_pageattrs);
	m0_free(dg);
}

/**
 * Generates a hash for a target-io request.
 * This is heavily based on m0t1fs/linux_kernel/file.c::tioreqs_hash_func.
 *
 * @param htable The hash table in use.
 * @param k Pointer to the key of the entry to hash.
 * @return the hash key.
 */
static uint64_t tioreqs_hash_func(const struct m0_htable *htable, const void *k)
{
	const uint64_t *key;
	M0_PRE(htable != NULL);
	M0_PRE(htable->h_bucket_nr > 0);
	M0_PRE(k != NULL);

	key = (uint64_t *)k;

	return *key % htable->h_bucket_nr;
}

/**
 * Compares keys for target-io requets.
 * This is heavily based on m0t1fs/linux_kernel/file.c::tioreq_key_eq.
 *
 * @param key1 The key of the first target-io request.
 * @param key2 The key of the second target-io request.
 * @return true or false.
 */
static bool tioreq_key_eq(const void *key1, const void *key2)
{
	const uint64_t *k1 = (uint64_t *)key1;
	const uint64_t *k2 = (uint64_t *)key2;

	M0_PRE(k1 != NULL);
	M0_PRE(k2 != NULL);

	return *k1 == *k2;
}

M0_HT_DESCR_DEFINE(tioreqht, "Hash of target_ioreq objects", M0_INTERNAL,
		   struct target_ioreq, ti_link, ti_magic,
		   M0_CLOVIS_TIOREQ_MAGIC, M0_CLOVIS_TLIST_HEAD_MAGIC,
		   ti_fid.f_container, tioreqs_hash_func, tioreq_key_eq);

M0_HT_DEFINE(tioreqht, M0_INTERNAL, struct target_ioreq, uint64_t);

/**
 * Checks a target_ioreq struct is correct.
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_invariant
 *
 * @param ti The target_ioreq fop to check.
 * @return true or false.
 */
static bool target_ioreq_invariant(const struct target_ioreq *ti)
{
	return M0_RC(ti != NULL &&
		     _0C(target_ioreq_bob_check(ti)) &&
		     _0C(ti->ti_session       != NULL) &&
		     _0C(ti->ti_nwxfer        != NULL) &&
		     _0C(ti->ti_bufvec.ov_buf != NULL) &&
		     _0C(ti->ti_auxbufvec.ov_buf != NULL) &&
		     _0C(m0_fid_is_valid(&ti->ti_fid)) &&
		     m0_tl_forall(iofops, iofop, &ti->ti_iofops,
			          ioreq_fop_invariant(iofop)));
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_request_invariant
 */
M0_INTERNAL bool nw_xfer_request_invariant(const struct nw_xfer_request *xfer)
{
	return M0_RC(xfer != NULL &&
		     _0C(nw_xfer_request_bob_check(xfer)) &&
		     _0C(xfer->nxr_state < NXS_STATE_NR) &&
		     _0C(ergo(xfer->nxr_state == NXS_INITIALIZED,
			     (xfer->nxr_rc == xfer->nxr_bytes) ==
			     (m0_atomic64_get(&xfer->nxr_iofop_nr) == 0))) &&
		     _0C(ergo(xfer->nxr_state == NXS_INFLIGHT,
			      !tioreqht_htable_is_empty(
			      &xfer->nxr_tioreqs_hash)))&&
		     _0C(ergo(xfer->nxr_state == NXS_COMPLETE,
			      m0_atomic64_get(&xfer->nxr_iofop_nr) == 0 &&
			      m0_atomic64_get(&xfer->nxr_rdbulk_nr) == 0)) &&
		     m0_htable_forall(tioreqht, tioreq, &xfer->nxr_tioreqs_hash,
				      target_ioreq_invariant(tioreq)));
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_fini
 */
void target_ioreq_fini(struct target_ioreq *ti)
{
	struct m0_clovis_op_io *ioo;
	unsigned int            opcode;

	M0_ENTRY("target_ioreq %p", ti);

	M0_PRE(target_ioreq_invariant(ti));
	M0_PRE(iofops_tlist_is_empty(&ti->ti_iofops));

	ioo = bob_of(ti->ti_nwxfer, struct m0_clovis_op_io,
		     ioo_nwxfer, &ioo_bobtype);
	opcode = ioo->ioo_oo.oo_oc.oc_op.op_code;
	target_ioreq_bob_fini(ti);
	tioreqht_tlink_fini(ti);
	iofops_tlist_fini(&ti->ti_iofops);
	ti->ti_ops     = NULL;
	ti->ti_session = NULL;
	ti->ti_nwxfer  = NULL;

	/* Resets the number of segments in vector. */
	if (ti->ti_ivec.iv_vec.v_nr == 0)
		ti->ti_ivec.iv_vec.v_nr = ti->ti_bufvec.ov_vec.v_nr;

	m0_indexvec_free(&ti->ti_ivec);
	if (opcode == M0_CLOVIS_OC_FREE)
		m0_indexvec_free(&ti->ti_trunc_ivec);
	m0_free0(&ti->ti_bufvec.ov_buf);
	m0_free0(&ti->ti_bufvec.ov_vec.v_count);
	m0_free0(&ti->ti_auxbufvec.ov_buf);
	m0_free0(&ti->ti_auxbufvec.ov_vec.v_count);
	m0_free0(&ti->ti_pageattrs);

	if (ti->ti_dgvec != NULL)
		dgmode_rwvec_dealloc_fini(ti->ti_dgvec);

	m0_free(ti);
	M0_LEAVE();
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_locate
 */
static struct target_ioreq *target_ioreq_locate(struct nw_xfer_request *xfer,
						struct m0_fid          *fid)
{
	struct target_ioreq *ti;

	M0_ENTRY("nw_xfer_request %p, fid %p", xfer, fid);

	M0_PRE(nw_xfer_request_invariant(xfer));
	M0_PRE(fid != NULL);

	ti = tioreqht_htable_lookup(&xfer->nxr_tioreqs_hash, &fid->f_container);
	/* WARN: Searches only with the container but compares the whole fid. */
	M0_ASSERT(ergo(ti != NULL, m0_fid_cmp(fid, &ti->ti_fid) == 0));

	M0_LEAVE();
	return ti;
}

/*
 * For partially parity groups only data units present in the truncate range
 * will be truncated. For fully spanned parity group both data and parity
 * units will be truncated.
 */
static bool should_unit_be_truncated(bool                      partial,
				     enum m0_pdclust_unit_type unit_type,
				     enum page_attr            flags)
{
	return (!partial || unit_type == M0_PUT_DATA) &&
	       (flags & PA_WRITE);
}

/**
 * Adds an io segment to index vector and buffer vector in
 * target_ioreq structure.
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_seg_add
 *
 * @param ti The target io request.
 * @param src Where in the global file the io occurs.
 * @param tgt Where in the target servers 'tile' the io occurs.
 * @param gob_offset Offset in the global file.
 * @param count Number of bytes in this operation.
 * @param map Map of data/parity buffers, used to tie ti to the corresponding
 *            buffers.
 */
static void target_ioreq_seg_add(struct target_ioreq              *ti,
				 const struct m0_pdclust_src_addr *src,
				 const struct m0_pdclust_tgt_addr *tgt,
				 m0_bindex_t                       gob_offset,
				 m0_bcount_t                       count,
				 struct pargrp_iomap              *map)
{
	uint32_t                   seg;
	uint32_t                   tseg;
	m0_bindex_t                toff;
	m0_bindex_t                goff;
	m0_bindex_t                pgstart;
	m0_bindex_t                pgend;
	struct data_buf           *buf;
	struct m0_clovis_op_io    *ioo;
	struct m0_pdclust_layout  *play;
	uint64_t                   frame;
	uint64_t                   unit;
	struct m0_indexvec        *ivec;
	struct m0_indexvec        *trunc_ivec = NULL;
	struct m0_bufvec          *bvec;
	struct m0_bufvec          *auxbvec;
	enum m0_pdclust_unit_type  unit_type;
	enum page_attr            *pattr;
	uint64_t                   cnt;
	unsigned int               opcode;
	m0_bcount_t                grp_size;
	uint64_t                   page_size;

	M0_PRE(tgt != NULL);
	frame = tgt->ta_frame;
	M0_PRE(src != NULL);
	unit  = src->sa_unit;
	M0_ENTRY("tio req %p, gob_offset %"PRIu64", count %"PRIu64
		 " frame %"PRIu64" unit %"PRIu64,
		 ti, gob_offset, count, frame, unit);

	M0_PRE(ti != NULL);
	M0_PRE(map != NULL);
	M0_PRE(target_ioreq_invariant(ti));

	ioo = bob_of(ti->ti_nwxfer, struct m0_clovis_op_io,
		     ioo_nwxfer, &ioo_bobtype);
	opcode = ioo->ioo_oo.oo_oc.oc_op.op_code;
	play = pdlayout_get(ioo);

	page_size = m0_clovis__page_size(ioo);
	grp_size = data_size(play) * map->pi_grpid;
	unit_type = m0_pdclust_unit_classify(play, unit);
	M0_ASSERT(M0_IN(unit_type, (M0_PUT_DATA, M0_PUT_PARITY)));

	toff    = target_offset(frame, play, gob_offset);
	pgstart = toff;
	goff    = unit_type == M0_PUT_DATA ? gob_offset : 0;

	M0_LOG(M0_DEBUG,
	       "[gpos %"PRIu64", count %"PRIu64"] [%"PRIu64", %"PRIu64"]"
	       "->[%"PRIu64",%"PRIu64"] %c", gob_offset, count, src->sa_group,
	       src->sa_unit, tgt->ta_frame, tgt->ta_obj,
	       unit_type == M0_PUT_DATA ? 'D' : 'P');

	/* Use ti_dgvec as long as it is dgmode-read/write. */
	if (ioreq_sm_state(ioo) == IRS_DEGRADED_READING ||
	    ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING)  {
		M0_ASSERT(ti->ti_dgvec != NULL);
		ivec  = &ti->ti_dgvec->dr_ivec;
		bvec  = &ti->ti_dgvec->dr_bufvec;
		auxbvec  = &ti->ti_dgvec->dr_auxbufvec;
		pattr = ti->ti_dgvec->dr_pageattrs;
		cnt = page_nr(ioo->ioo_iomap_nr * layout_unit_size(play) *
		      (layout_n(play) + layout_k(play)), ioo->ioo_obj);
		M0_LOG(M0_DEBUG, "map_nr=%"PRIu64" req state=%u cnt=%"PRIu64,
				 ioo->ioo_iomap_nr, ioreq_sm_state(ioo), cnt);
	} else {
		ivec  = &ti->ti_ivec;
		trunc_ivec  = &ti->ti_trunc_ivec;
		bvec  = &ti->ti_bufvec;
		auxbvec = &ti->ti_auxbufvec;
		pattr = ti->ti_pageattrs;
		cnt = page_nr(ioo->ioo_iomap_nr * layout_unit_size(play) *
			      layout_n(play), ioo->ioo_obj);
		M0_LOG(M0_DEBUG, "map_nr=%"PRIu64" req state=%u cnt=%"PRIu64,
				 ioo->ioo_iomap_nr, ioreq_sm_state(ioo), cnt);
	}

	while (pgstart < toff + count) {
		pgend = min64u(pgstart + page_size,
			       toff + count);
		seg   = SEG_NR(ivec);

		INDEX(ivec, seg) = pgstart;
		COUNT(ivec, seg) = pgend - pgstart;

		if (unit_type == M0_PUT_DATA) {
			uint32_t row = map->pi_max_row;
			uint32_t col = map->pi_max_col;

			page_pos_get(map, goff, grp_size, &row, &col);
			M0_ASSERT(row <= map->pi_max_row);
			M0_ASSERT(col <= map->pi_max_col);
			buf = map->pi_databufs[row][col];

			pattr[seg] |= PA_DATA;
			M0_LOG(M0_DEBUG, "Data seg %u added", seg);
		} else {
			buf = map->pi_paritybufs[page_id(goff, ioo->ioo_obj)]
			[unit - data_col_nr(play)];
			pattr[seg] |= PA_PARITY;
			M0_LOG(M0_DEBUG, "Parity seg %u added", seg);
		}
		buf->db_tioreq = ti;
		if (buf->db_flags & PA_WRITE)
			ti->ti_req_type = TI_READ_WRITE;

		if (opcode == M0_CLOVIS_OC_FREE &&
		    should_unit_be_truncated(map->pi_trunc_partial,
					     unit_type, buf->db_flags)) {
			tseg   = SEG_NR(trunc_ivec);
			INDEX(trunc_ivec, tseg) = pgstart;
			COUNT(trunc_ivec, tseg) = pgend - pgstart;
			++trunc_ivec->iv_vec.v_nr;
			M0_LOG(M0_DEBUG, "Seg id %d [%"PRIu64", %"PRIu64"]"
					 "added to target ioreq with "FID_F,
					 tseg, INDEX(trunc_ivec, tseg),
					 COUNT(trunc_ivec, tseg),
					 FID_P(&ti->ti_fid));
		}

		if (opcode == M0_CLOVIS_OC_FREE && !map->pi_trunc_partial)
			pattr[seg] |= PA_TRUNC;

		M0_ASSERT(addr_is_network_aligned(buf->db_buf.b_addr));
		bvec->ov_buf[seg] = buf->db_buf.b_addr;
		bvec->ov_vec.v_count[seg] = COUNT(ivec, seg);
		if (map->pi_rtype == PIR_READOLD &&
		    unit_type == M0_PUT_DATA) {
			M0_ASSERT(buf->db_auxbuf.b_addr != NULL);
			auxbvec->ov_buf[seg] = buf->db_auxbuf.b_addr;
			auxbvec->ov_vec.v_count[seg] = page_size;
		}
		pattr[seg] |= buf->db_flags;
		M0_LOG(M0_DEBUG, "pageaddr=%p, auxpage=%p,"
				 " index=%6"PRIu64", size=%4"PRIu64
				 " grpid=%3"PRIu64" flags=%4x for "FID_F,
		                 bvec->ov_buf[seg], auxbvec->ov_buf[seg],
				 INDEX(ivec, seg), COUNT(ivec, seg),
				 map->pi_grpid, pattr[seg],
				 FID_P(&ti->ti_fid));
		M0_LOG(M0_DEBUG, "Seg id %d [%"PRIu64", %"PRIu64
				 "] added to target_ioreq with "FID_F
				 " with flags 0x%x: ", seg,
				 INDEX(ivec, seg), COUNT(ivec, seg),
				 FID_P(&ti->ti_fid), pattr[seg]);

		goff += COUNT(ivec, seg);
		++ivec->iv_vec.v_nr;
		pgstart = pgend;
	}
	M0_LEAVE();
}

/**
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_fid().
 */
M0_INTERNAL struct m0_fid target_fid(struct m0_clovis_op_io *ioo,
				     struct m0_pdclust_tgt_addr *tgt)
{
	struct m0_fid fid;

	m0_poolmach_gob2cob(clovis_ioo_to_poolmach(ioo),
			    &ioo->ioo_oo.oo_fid, tgt->ta_obj,
			    &fid);
	return fid;
}

/**
 * Finds the rpc session to use to contact the server hosting a particular
 * target:fid (cob).
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_session
 *
 * @param ioo The IO operation.
 * @param tfid The cob fid to look for.
 * @param a pointer to the rpc_session to use to contact this target.
 */
static inline struct m0_rpc_session *
target_session(struct m0_clovis_op_io *ioo, struct m0_fid tfid)
{
	struct m0_clovis_op    *op;
	struct m0_pool_version *pv;
	struct m0_clovis       *instance;

	M0_PRE(ioo != NULL);
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	pv = m0_pool_version_find(&instance->m0c_pools_common, &ioo->ioo_pver);
	M0_ASSERT(pv != NULL);

	return m0_clovis_obj_container_id_to_session(
			pv, m0_fid_cob_device_id(&tfid));
}

/**
 * Pair data/parity buffers with the io fop rpc.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::target_ioreq_iofops_prepare
 *
 * @param irfop The io fop that needs bulk buffers adding.
 * @param dom The network domain this rpc will be sent in.
 * @param rbuf[out] The rpc bulk buffer that contains the target-iorequest's
 *                  extents.
 * @param delta[out] The extra space in the fop needed for metadata.
 * @param maxsize Caller provided limit.
 * @return 0 for success, -errno otherwise.
 */
static int bulk_buffer_add(struct ioreq_fop        *irfop,
			   struct m0_net_domain    *dom,
			   struct m0_rpc_bulk_buf **rbuf,
			   uint32_t                *delta,
			   uint32_t                 maxsize)
{
	int                      rc;
	int                      seg_nr;
	struct m0_clovis_op_io  *ioo;
	struct m0_indexvec      *ivec;

	M0_PRE(irfop  != NULL);
	M0_PRE(dom    != NULL);
	M0_PRE(rbuf   != NULL);
	M0_PRE(delta  != NULL);
	M0_PRE(maxsize > 0);
	M0_ENTRY("ioreq_fop %p net_domain %p delta_size %d",
		 irfop, dom, *delta);

	ioo     = bob_of(irfop->irf_tioreq->ti_nwxfer, struct m0_clovis_op_io,
			 ioo_nwxfer, &ioo_bobtype);
	ivec    = M0_IN(ioreq_sm_state(ioo), (IRS_READING, IRS_WRITING)) ?
			&irfop->irf_tioreq->ti_ivec :
			&irfop->irf_tioreq->ti_dgvec->dr_ivec;
	seg_nr  = min32(m0_net_domain_get_max_buffer_segments(dom),
			SEG_NR(ivec));
	*delta += io_desc_size(dom);

	if (m0_io_fop_size_get(&irfop->irf_iofop.if_fop) + *delta < maxsize) {
		rc = m0_rpc_bulk_buf_add(&irfop->irf_iofop.if_rbulk, seg_nr,
					 0, dom, NULL, rbuf);
		if (rc != 0) {
			*delta -= io_desc_size(dom);
			return M0_ERR(rc);
		}
	} else {
		rc      = -ENOSPC;
		*delta -= io_desc_size(dom);
	}

	M0_POST(ergo(rc == 0, *rbuf != NULL));
	return M0_RC(rc);
}

/**
 * Finalises an io request fop, (releases bulk buffers).
 * This is heavily based on m0t1fs/linux_kernel/file.c::irfop_fini
 *
 * @param irfop The io request fop to finalise.
 */
static void irfop_fini(struct ioreq_fop *irfop)
{
	M0_ENTRY("ioreq_fop %p", irfop);

	M0_PRE(irfop != NULL);

	m0_rpc_bulk_buflist_empty(&irfop->irf_iofop.if_rbulk);
	ioreq_fop_fini(irfop);
	m0_free(irfop);

	M0_LEAVE();
}

/**
 * Assembles io fops for the specified target server.
 * This is heavily based on
 * m0t1fs/linux_kernel/file.c::target_ioreq_iofops_prepare
 *
 * @param ti The target io request whose data/parity fops should be assembled.
 * @param filter Whether to restrict the set of fops to prepare.
 * @return 0 for success, -errno otherwise.
 */
static int target_ioreq_iofops_prepare(struct target_ioreq *ti,
				       enum page_attr       filter)
{
	int                          rc = 0;
	uint32_t                     seg = 0;
	/* Number of segments in one m0_rpc_bulk_buf structure. */
	uint32_t                     bbsegs;
	uint32_t                     maxsize;
	uint32_t                     delta;
	enum page_attr               rw;
	enum page_attr              *pattr;
	struct m0_bufvec            *bvec;
	struct m0_bufvec            *auxbvec;
	struct m0_clovis_op_io      *ioo;
	struct m0_clovis_obj_attr   *io_attr;
	struct m0_indexvec          *ivec;
	struct ioreq_fop            *irfop;
	struct m0_net_domain        *ndom;
	struct m0_rpc_bulk_buf      *rbuf;
	struct m0_io_fop            *iofop;
	struct m0_fop_cob_rw        *rw_fop;
	struct nw_xfer_request      *xfer;
	/* Is it in the READ phase of WRITE request. */
	bool                         read_in_write = false;
	void                        *buf;

	M0_ENTRY("prepare io fops for target ioreq %p filter 0x%x, tfid "FID_F,
		 ti, filter, FID_P(&ti->ti_fid));

	M0_PRE(target_ioreq_invariant(ti));
	M0_PRE(M0_IN(filter, (PA_DATA, PA_PARITY)));

	rc = m0_rpc_session_validate(ti->ti_session);
	if (rc != 0 && rc != -ECANCELED)
		return M0_ERR(rc);

	xfer = ti->ti_nwxfer;
	ioo = bob_of(xfer, struct m0_clovis_op_io, ioo_nwxfer, &ioo_bobtype);
	M0_ASSERT(M0_IN(ioreq_sm_state(ioo),
			(IRS_READING, IRS_DEGRADED_READING,
			 IRS_WRITING, IRS_DEGRADED_WRITING)));

	if (ioo->ioo_oo.oo_oc.oc_op.op_code == M0_CLOVIS_OC_WRITE &&
	    M0_IN(ioreq_sm_state(ioo), (IRS_READING, IRS_DEGRADED_READING)))
		read_in_write = true;

	if (M0_IN(ioreq_sm_state(ioo), (IRS_READING, IRS_WRITING))) {
		ivec    = &ti->ti_ivec;
		bvec    = &ti->ti_bufvec;
		auxbvec = &ti->ti_auxbufvec;
		pattr   = ti->ti_pageattrs;
	} else {
		if (ti->ti_dgvec == NULL) {
			return M0_RC(0);
		}
		ivec    = &ti->ti_dgvec->dr_ivec;
		bvec    = &ti->ti_dgvec->dr_bufvec;
		auxbvec = &ti->ti_dgvec->dr_auxbufvec;
		pattr   = ti->ti_dgvec->dr_pageattrs;
	}

	ndom = ti->ti_session->s_conn->c_rpc_machine->rm_tm.ntm_dom;
	rw = ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING ? PA_DGMODE_WRITE :
	     ioreq_sm_state(ioo) == IRS_WRITING ? PA_WRITE :
	     ioreq_sm_state(ioo) == IRS_DEGRADED_READING ? PA_DGMODE_READ :
	     PA_READ;
	maxsize = m0_rpc_session_get_max_item_payload_size(ti->ti_session);

	while (seg < SEG_NR(ivec)) {
		delta  = 0;
		bbsegs = 0;

		M0_LOG(M0_DEBUG, "pageattr = %u, filter = %u, rw = %u",
		       pattr[seg], filter, rw);

		if (!(pattr[seg] & filter) || !(pattr[seg] & rw) ||
		     (pattr[seg] & PA_TRUNC)) {
			++seg;
			continue;
		}

		M0_ALLOC_PTR(irfop);
		if (irfop == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto err;
		}
		rc = ioreq_fop_init(irfop, ti, filter);
		if (rc != 0) {
			m0_free(irfop);
			goto err;
		}

		iofop = &irfop->irf_iofop;
		rw_fop = io_rw_get(&iofop->if_fop);

		rc = bulk_buffer_add(irfop, ndom, &rbuf, &delta, maxsize);
		if (rc != 0) {
			ioreq_fop_fini(irfop);
			m0_free(irfop);
			goto err;
		}
		delta += io_seg_size();

		/*
		* Adds io segments and io descriptor only if it fits within
		* permitted size.
		*/
		/* TODO: can this loop become a function call?
		 * -- too many levels of indentation */
		while (seg < SEG_NR(ivec) &&
			m0_io_fop_size_get(&iofop->if_fop) + delta < maxsize) {

			/*
			* Adds a page to rpc bulk buffer only if it passes
			* through the filter.
			*/
			if (pattr[seg] & rw && pattr[seg] & filter &&
			    !(pattr[seg] & PA_TRUNC)) {
				delta += io_seg_size() + io_di_size(ioo);

				if (filter == PA_DATA &&
				    read_in_write &&
				    auxbvec != NULL &&
				    auxbvec->ov_buf[seg] != NULL)
					buf = auxbvec->ov_buf[seg];
				else
					buf = bvec->ov_buf[seg];

				rc = m0_rpc_bulk_buf_databuf_add(rbuf,
					buf, COUNT(ivec, seg),
					INDEX(ivec, seg), ndom);

				if (rc == -EMSGSIZE) {
					/*
					 * Fix the number of segments in
					 * current m0_rpc_bulk_buf structure.
					 */
					rbuf->bb_nbuf->nb_buffer.ov_vec.v_nr =
						bbsegs;
					rbuf->bb_zerovec.z_bvec.ov_vec.v_nr =
						bbsegs;
					bbsegs = 0;

					delta -= io_seg_size() - io_di_size(ioo);

					/*
					 * Buffer must be 4k aligned to be
					 * used by network hw
					 */
					M0_ASSERT(addr_is_network_aligned(buf));
					rc     = bulk_buffer_add(irfop, ndom,
							&rbuf, &delta, maxsize);
					if (rc == -ENOSPC)
						break;
					else if (rc != 0)
						goto fini_fop;

					/*
					 * Since current bulk buffer is full,
					 * new bulk buffer is added and
					 * existing segment is attempted to
					 * be added to new bulk buffer.
					 */
					continue;
				} else if (rc == 0)
					++bbsegs;
			}

			++seg;
		}

		if (m0_io_fop_byte_count(iofop) == 0) {
			irfop_fini(irfop);
			continue;
		}

		rbuf->bb_nbuf->nb_buffer.ov_vec.v_nr = bbsegs;
		rbuf->bb_zerovec.z_bvec.ov_vec.v_nr = bbsegs;

		rw_fop->crw_fid = ti->ti_fid;
		rw_fop->crw_pver = ioo->ioo_pver;
		rw_fop->crw_index = ti->ti_obj;
		io_attr = m0_clovis_io_attr(ioo);
		rw_fop->crw_lid = io_attr->oa_layout_id;

		/*
		 * XXX(Sining): This is a bit tricky: m0_io_fop_prepare in
		 * ioservice/io_fops.c calls io_fop_di_prepare which has only
		 * file system in mind and uses super block and file related
		 * information to do something (it returns 0 directly for user
		 * space). This is not the case for Clovis kernel mode!!
		 *
		 * Simply return 0 just like it does for user space at this
		 * moment.
		 */
		rc = m0_io_fop_prepare(&iofop->if_fop);
		if (rc != 0)
			goto fini_fop;

		if (m0_is_read_fop(&iofop->if_fop))
			m0_atomic64_add(&xfer->nxr_rdbulk_nr,
					m0_rpc_bulk_buf_length(
					&iofop->if_rbulk));

		m0_atomic64_inc(&ti->ti_nwxfer->nxr_iofop_nr);
		iofops_tlist_add(&ti->ti_iofops, irfop);

		M0_LOG(M0_DEBUG,
		       "fop=%p bulk=%p (%s) @"FID_F" io fops = %"PRIu64
		       " read bulks = %"PRIu64", list_len=%d",
		       &iofop->if_fop, &iofop->if_rbulk,
		       m0_is_read_fop(&iofop->if_fop) ? "r" : "w",
		       FID_P(&ti->ti_fid),
		       m0_atomic64_get(&xfer->nxr_iofop_nr),
		       m0_atomic64_get(&xfer->nxr_rdbulk_nr),
		       (int)iofops_tlist_length(&ti->ti_iofops));
	}

	return M0_RC(0);

fini_fop:
	irfop_fini(irfop);
err:
	m0_tl_teardown(iofops, &ti->ti_iofops, irfop) {
		irfop_fini(irfop);
	}

	return M0_ERR(rc);
}
static int target_cob_fop_prepare(struct target_ioreq *ti);
static const struct target_ioreq_ops tioreq_ops = {
	.tio_seg_add         = target_ioreq_seg_add,
	.tio_iofops_prepare  = target_ioreq_iofops_prepare,
	.tio_cc_fops_prepare = target_cob_fop_prepare,
};

static int target_cob_fop_prepare(struct target_ioreq *ti)
{
	M0_PRE(M0_IN(ti->ti_req_type, (TI_COB_CREATE, TI_COB_TRUNCATE)));

	return ioreq_cc_fop_init(ti);
}

/**
 * Initialises a target io request.
 * This is heavily based on m0t1fs/linux_kernel/file.c::target_ioreq_init
 *
 * @param ti[out] The target io request to initialise.
 * @param xfer The corresponding network transfer request.
 * @param cobfid The fid of the cob this request will act on.
 * @param ta_obj Which object in the global layout the cobfid corresponds to.
 * @param session The rpc session that should be used to send this request.
 * @param size The size of the request in bytes.
 * @return 0 for success, -errno otherwise.
 */
static int target_ioreq_init(struct target_ioreq    *ti,
			     struct nw_xfer_request *xfer,
			     const struct m0_fid    *cobfid,
			     uint64_t                ta_obj,
			     struct m0_rpc_session  *session,
			     uint64_t                size)
{
	int                     rc;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis_op    *op;
	struct m0_clovis       *instance;
	uint32_t                nr;

	M0_PRE(cobfid  != NULL);
	M0_ENTRY("target_ioreq %p, nw_xfer_request %p, "FID_F,
		 ti, xfer, FID_P(cobfid));

	M0_PRE(ti      != NULL);
	M0_PRE(xfer    != NULL);
	M0_PRE(session != NULL);
	M0_PRE(size    >  0);

	ti->ti_rc        = 0;
	ti->ti_ops       = &tioreq_ops;
	ti->ti_fid       = *cobfid;
	ti->ti_nwxfer    = xfer;
	ti->ti_dgvec     = NULL;
	ti->ti_req_type  = TI_NONE;
	M0_SET0(&ti->ti_cc_fop);

	/*
	 * Target object is usually in ONLINE state unless explicitly
	 * told otherwise.
	 */
	ti->ti_state     = M0_PNDS_ONLINE;
	ti->ti_session   = session;
	ti->ti_parbytes  = 0;
	ti->ti_databytes = 0;

	ioo      = bob_of(xfer, struct m0_clovis_op_io, ioo_nwxfer,
			  &ioo_bobtype);
	op       = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	M0_PRE(instance != NULL);

	ti->ti_obj = ta_obj;

	iofops_tlist_init(&ti->ti_iofops);
	tioreqht_tlink_init(ti);
	target_ioreq_bob_init(ti);

	nr = page_nr(size, ioo->ioo_obj);
	rc = m0_indexvec_alloc(&ti->ti_ivec, nr);
	if (rc != 0)
		goto out;
	if (op->op_code == M0_CLOVIS_OC_FREE)
		rc = m0_indexvec_alloc(&ti->ti_trunc_ivec, nr);
		if (rc != 0)
			goto out;

	ti->ti_bufvec.ov_vec.v_nr = nr;
	M0_ALLOC_ARR(ti->ti_bufvec.ov_vec.v_count, nr);
	if (ti->ti_bufvec.ov_vec.v_count == NULL)
		goto fail;

	M0_ALLOC_ARR(ti->ti_bufvec.ov_buf, nr);
	if (ti->ti_bufvec.ov_buf == NULL)
		goto fail;

	/*
	 * For READOLD method, an extra bufvec is needed to remember
	 * the addresses of auxillary buffers so those auxillary
	 * buffers can be used in rpc bulk transfer to avoid polluting
	 * real data buffers which are the application's memory for IO
	 * in case zero copy method is in use.
	 */
	ti->ti_auxbufvec.ov_vec.v_nr = nr;
	M0_ALLOC_ARR(ti->ti_auxbufvec.ov_vec.v_count, nr);
	if (ti->ti_auxbufvec.ov_vec.v_count == NULL)
		goto fail;

	M0_ALLOC_ARR(ti->ti_auxbufvec.ov_buf, nr);
	if (ti->ti_auxbufvec.ov_buf == NULL)
		goto fail;

	M0_ALLOC_ARR(ti->ti_pageattrs, nr);
	if (ti->ti_pageattrs == NULL)
		goto fail;

	/*
	 * This value is incremented when new segments are added to the
	 * index vector in target_ioreq_seg_add().
	 */
	ti->ti_ivec.iv_vec.v_nr = 0;
	ti->ti_trunc_ivec.iv_vec.v_nr = 0;

	M0_POST_EX(target_ioreq_invariant(ti));
	return M0_RC(0);
fail:
	m0_indexvec_free(&ti->ti_ivec);
	if (op->op_code == M0_CLOVIS_OC_FREE)
		m0_indexvec_free(&ti->ti_trunc_ivec);
	m0_free(ti->ti_bufvec.ov_vec.v_count);
	m0_free(ti->ti_bufvec.ov_buf);
	m0_free(ti->ti_auxbufvec.ov_vec.v_count);
	m0_free(ti->ti_auxbufvec.ov_buf);

out:
	return M0_ERR(-ENOMEM);
}

/**
 * Retrieves (possibly allocating and initialising) a target io request for the
 * provided network transfer requests.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_tioreq_get
 *
 * @param xfer The network transfer.
 * @param fid The cob fid that the request will operate on.
 * @param ta_obj Which object in the global layout the cobfid corresponds to.
 * @param session The session the request will be sent on.
 * @param size The size of the request.
 * @param out[out] The discovered (or allocated) target io request.
 */
static int nw_xfer_tioreq_get(struct nw_xfer_request *xfer,
			      struct m0_fid          *fid,
			      uint64_t                ta_obj,
			      struct m0_rpc_session  *session,
			      uint64_t                size,
			      struct target_ioreq   **out)
{
	int                     rc = 0;
	struct target_ioreq    *ti;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis_op    *op;
	struct m0_clovis       *instance;

	M0_PRE(fid != NULL);
	M0_ENTRY("nw_xfer_request %p, "FID_F, xfer, FID_P(fid));

	M0_PRE(session != NULL);
	M0_PRE(out != NULL);
	M0_PRE(nw_xfer_request_invariant(xfer));

	ioo = bob_of(xfer, struct m0_clovis_op_io, ioo_nwxfer, &ioo_bobtype);
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	M0_PRE(instance != NULL);

	ti = target_ioreq_locate(xfer, fid);
	if (ti == NULL) {
		M0_ALLOC_PTR(ti);
		if (ti == NULL)
			return M0_ERR(-ENOMEM);

		rc = target_ioreq_init(ti, xfer, fid, ta_obj, session, size);
		if (rc == 0) {
			tioreqht_htable_add(&xfer->nxr_tioreqs_hash, ti);
			M0_LOG(M0_INFO, "New target_ioreq added for "FID_F,
			       FID_P(fid));
		}
		else
			m0_free(ti);
	}

	if (ti->ti_dgvec == NULL && M0_IN(ioreq_sm_state(ioo),
		(IRS_DEGRADED_READING, IRS_DEGRADED_WRITING)))
		rc = dgmode_rwvec_alloc_init(ti);

	*out = ti;

	return M0_RC(rc);
}

static void bitmap_reset(struct m0_bitmap *bitmap)
{
	int i;

	for (i = 0; i < bitmap->b_nr; ++i)
		m0_bitmap_set(bitmap, i, false);
}
/**
 * Distributes file data into target_ioreq objects as required and populates
 * target_ioreq::ti_ivec and target_ioreq::ti_bufvec.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_io_distribute
 *
 * @param xfer The network transfer request.
 * @return 0 for success, -errno otherwise.
 */
static int nw_xfer_io_distribute(struct nw_xfer_request *xfer)
{
	int                         rc;
	unsigned int                op_code;
	uint64_t                    map;
	uint64_t                    unit;
	uint64_t                    unit_size;
	uint64_t                    count;
	uint64_t                    pgstart;
	struct m0_clovis_op        *op;
	/* Extent representing a data unit. */
	struct m0_ext               u_ext;
	/* Extent representing resultant extent. */
	struct m0_ext               r_ext;
	/* Extent representing a segment from index vector. */
	struct m0_ext               v_ext;
	struct m0_clovis_op_io     *ioo;
	struct target_ioreq        *ti;
	struct m0_ivec_cursor       cursor;
	struct m0_pdclust_layout   *play;
	enum m0_pdclust_unit_type   unit_type;
	struct m0_pdclust_src_addr  src;
	struct m0_pdclust_tgt_addr  tgt;
	struct m0_bitmap            units_spanned;
	uint32_t                    row_start;
	uint32_t                    row_end;
	uint32_t                    row;
	uint32_t                    col;
	struct data_buf            *dbuf;
	struct pargrp_iomap        *iomap;
	struct m0_clovis           *instance;
	m0_bcount_t                 grp_size;

	M0_ENTRY("nw_xfer_request %p", xfer);

	M0_PRE(nw_xfer_request_invariant(xfer));

	ioo       = bob_of(xfer, struct m0_clovis_op_io, ioo_nwxfer,
			   &ioo_bobtype);
	op        = &ioo->ioo_oo.oo_oc.oc_op;
	op_code   = op->op_code,
	play      = pdlayout_get(ioo);
	unit_size = layout_unit_size(play);
	instance  = m0_clovis__op_instance(op);
	rc = m0_bitmap_init(&units_spanned, m0_pdclust_size(play));

	for (map = 0; map < ioo->ioo_iomap_nr; ++map) {
		count        = 0;
		iomap        = ioo->ioo_iomaps[map];
		pgstart      = data_size(play) * iomap->pi_grpid;
		src.sa_group = iomap->pi_grpid;

		M0_LOG(M0_DEBUG, "xfer %p map %p [grpid = %"PRIu64" state=%u]",
				 xfer, iomap, iomap->pi_grpid, iomap->pi_state);

		/* Cursor for pargrp_iomap::pi_ivec. */
		m0_ivec_cursor_init(&cursor, &iomap->pi_ivec);
		bitmap_reset(&units_spanned);
		grp_size = data_size(play) * iomap->pi_grpid;
		while (!m0_ivec_cursor_move(&cursor, count)) {
			unit = (m0_ivec_cursor_index(&cursor) - pgstart) /
				unit_size;

			u_ext.e_start = pgstart + unit * unit_size;
			u_ext.e_end   = u_ext.e_start + unit_size;
			m0_ext_init(&u_ext);

			v_ext.e_start  = m0_ivec_cursor_index(&cursor);
			v_ext.e_end    = v_ext.e_start +
				m0_ivec_cursor_step(&cursor);
			m0_ext_init(&v_ext);

			m0_ext_intersection(&u_ext, &v_ext, &r_ext);
			if (!m0_ext_is_valid(&r_ext)) {
				count = unit_size;
				continue;
			}

			count     = m0_ext_length(&r_ext);
			unit_type = m0_pdclust_unit_classify(play, unit);
			if (unit_type == M0_PUT_SPARE ||
				unit_type == M0_PUT_PARITY)
				continue;

			if (ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING) {
				page_pos_get(iomap, r_ext.e_start, grp_size,
					     &row_start, &col);
				page_pos_get(iomap, r_ext.e_end - 1, grp_size,
					     &row_end, &col);
				dbuf = iomap->pi_databufs[row_start][col];
				M0_ASSERT(dbuf != NULL);
				for (row = row_start; row <= row_end; ++row) {
					dbuf = iomap->pi_databufs[row][col];
					M0_ASSERT(dbuf != NULL);
					if (dbuf->db_flags & PA_WRITE) {
						dbuf->db_flags |=
							PA_DGMODE_WRITE;
						m0_bitmap_set(&units_spanned,
							      unit, true);
					}
				}
			}
			src.sa_unit = unit;
			rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src, &tgt,
							   &ti);
			if (rc != 0)
				goto err;

			ti->ti_ops->tio_seg_add(ti, &src, &tgt, r_ext.e_start,
						m0_ext_length(&r_ext),
						iomap);
		}

		M0_ASSERT(ergo(M0_IN(op_code,
				     (M0_CLOVIS_OC_READ, M0_CLOVIS_OC_WRITE)),
			       m0_vec_count(&ioo->ioo_ext.iv_vec) ==
			       m0_vec_count(&ioo->ioo_data.ov_vec)));

		if (M0_IN(ioo->ioo_pbuf_type, (M0_CLOVIS_PBUF_DIR,
					       M0_CLOVIS_PBUF_IND)) ||
		    (ioreq_sm_state(ioo) == IRS_DEGRADED_READING &&
		     iomap->pi_state == PI_DEGRADED)) {

			for (unit = 0; unit < layout_k(play); ++unit) {
				src.sa_unit = layout_n(play) + unit;
				M0_ASSERT(m0_pdclust_unit_classify(play,
					  src.sa_unit) == M0_PUT_PARITY);

				rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src,
								   &tgt, &ti);
				if (rc != 0)
					goto err;

				parity_page_pos_get(iomap, unit * unit_size,
						    &row, &col);

				for (; row < parity_row_nr(play, ioo->ioo_obj);
				     ++row) {
					dbuf = iomap->pi_paritybufs[row][col];
					M0_ASSERT(dbuf != NULL);
					if (m0_pdclust_is_replicated(play) &&
					    iomap->pi_databufs[row][0] == NULL)
						continue;
					M0_ASSERT(ergo(op_code ==
						       M0_CLOVIS_OC_WRITE,
						       dbuf->db_flags &
						       PA_WRITE));
					if (ioreq_sm_state(ioo) ==
					    IRS_DEGRADED_WRITING &&
					    dbuf->db_flags & PA_WRITE) {
						dbuf->db_flags |=
							PA_DGMODE_WRITE;
						m0_bitmap_set(&units_spanned,
							      src.sa_unit,
							      true);
					}
				}
				ti->ti_ops->tio_seg_add(ti, &src, &tgt, pgstart,
							layout_unit_size(play),
							iomap);
			}
			/*
			 * Since CROW is not enabled in non-oostore mode cobs
			 * are present across all nodes.
			 */
			if (!m0_clovis__is_oostore(instance) ||
			    op_code == M0_CLOVIS_OC_READ)
				continue;
			/*
			 * Create cobs for those units not spanned by IO
			 * request.
			 */
			for (unit = 0; unit < m0_pdclust_size(play); ++unit) {
				if (m0_bitmap_get(&units_spanned, unit))
					continue;
				src.sa_unit = unit;
				rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src,
								   &tgt, &ti);
				if (rc != 0) {
					M0_LOG(M0_ERROR, "[%p] map %p,"
					       "nxo_tioreq_map() failed, rc %d"
						,ioo, iomap, rc);
				}
				/*
				 * Skip the case when some other parity group
				 * has spanned the particular target.
				 */
				if (ti->ti_req_type != TI_NONE) {
					m0_bitmap_set(&units_spanned, unit,
						      true);
					continue;
				}
				ti->ti_req_type = TI_COB_CREATE;
				m0_bitmap_set(&units_spanned, unit, true);
			}
			M0_ASSERT(m0_bitmap_set_nr(&units_spanned) ==
				  m0_pdclust_size(play));
		}
	}
	m0_bitmap_fini(&units_spanned);
	M0_ASSERT(ergo(M0_IN(op_code, (M0_CLOVIS_OC_READ, M0_CLOVIS_OC_WRITE)),
		       m0_vec_count(&ioo->ioo_ext.iv_vec) ==
		       m0_vec_count(&ioo->ioo_data.ov_vec)));

	return M0_RC(0);
err:
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		tioreqht_htable_del(&xfer->nxr_tioreqs_hash, ti);
		target_ioreq_fini(ti);
		m0_free0(&ti);
	} m0_htable_endfor;

	return M0_ERR(rc);
}

/**
 * Completes all the target io requests in a network transfer request. Collects
 * the total number of bytes read/written, and determines the final return code.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_req_complete
 * Call with ioo->ioo_sm.sm_grp locked.
 *
 * @param xfer The network transfer request.
 * @param rmw Whether this request was part of a multiple requests (rmw).
 */
static void nw_xfer_req_complete(struct nw_xfer_request *xfer, bool rmw)
{
	struct m0_clovis       *instance;
	struct m0_clovis_op_io *ioo;
	struct target_ioreq    *ti;
	struct ioreq_fop       *irfop;
	struct m0_fop          *fop;
	struct m0_rpc_item     *item;

	M0_ENTRY("nw_xfer_request %p, rmw %s", xfer,
		 rmw ? (char *)"true" : (char *)"false");

	M0_PRE(xfer != NULL);
	xfer->nxr_state = NXS_COMPLETE;
	ioo = bob_of(xfer, struct m0_clovis_op_io, ioo_nwxfer, &ioo_bobtype);
	M0_PRE(m0_sm_group_is_locked(ioo->ioo_sm.sm_grp));

	instance = m0_clovis__op_instance(m0_clovis__ioo_to_op(ioo));
	/*
 	 * Ignore the following invariant check as there exists cases in which
 	 * io fops are created sucessfully for some target services but fail
 	 * for some services in nxo_dispatch (for example, session/connection
 	 * to a service is invalid, resulting a 'dirty' op in which
 	 * nr_iofops != 0 and nxr_state == NXS_COMPLETE.
 	 *
	 * M0_PRE_EX(m0_clovis_op_io_invariant(ioo));
	 */

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {

		/* Maintains only the first error encountered. */
		if (xfer->nxr_rc == 0)
			xfer->nxr_rc = ti->ti_rc;

		xfer->nxr_bytes += ti->ti_databytes;
		ti->ti_databytes = 0;

		if (m0_clovis__is_oostore(instance) &&
		    ti->ti_req_type == TI_COB_CREATE &&
		    ioreq_sm_state(ioo) == IRS_WRITE_COMPLETE) {
			ti->ti_req_type = TI_NONE;
			m0_fop_put_lock(&ti->ti_cc_fop.crf_fop);
			continue;
		}

		if (m0_clovis__is_oostore(instance) &&
		    ti->ti_req_type == TI_COB_TRUNCATE &&
		    ioreq_sm_state(ioo) == IRS_TRUNCATE_COMPLETE) {
			ti->ti_req_type = TI_NONE;
			if (ti->ti_trunc_ivec.iv_vec.v_nr > 0)
				m0_fop_put_lock(&ti->ti_cc_fop.crf_fop);
		}

		m0_tl_teardown(iofops, &ti->ti_iofops, irfop) {
			fop = &irfop->irf_iofop.if_fop;
			item = m0_fop_to_rpc_item(fop);
			M0_LOG(M0_DEBUG, "[%p] fop %p, ref %llu, "
			       "item %p[%u], ri_error %d, ri_state %d",
			       ioo, fop,
			       (unsigned long long)m0_ref_read(&fop->f_ref),
			       item, item->ri_type->rit_opcode, item->ri_error,
			       item->ri_sm.sm_state);

			/* Maintains only the first error encountered. */
			if (xfer->nxr_rc == 0 &&
			    item->ri_sm.sm_state == M0_RPC_ITEM_FAILED) {
				xfer->nxr_rc = item->ri_error;
				M0_LOG(M0_DEBUG, "[%p] nwxfer rc = %d",
				       ioo, xfer->nxr_rc);
			}

			M0_ASSERT(ergo(item->ri_sm.sm_state !=
				       M0_RPC_ITEM_UNINITIALISED,
				       item->ri_rmachine != NULL));
			if (item->ri_rmachine == NULL) {
				M0_ASSERT(ti->ti_session != NULL);
				m0_fop_rpc_machine_set(fop,
					ti->ti_session->s_conn->c_rpc_machine);
			}

			M0_LOG(M0_DEBUG,
			       "[%p] item %p, target fid "FID_F"fop %p, "
			       "ref %llu", ioo, item, FID_P(&ti->ti_fid), fop,
			       (unsigned long long)m0_ref_read(&fop->f_ref));
			m0_fop_put_lock(fop);
		}

	} m0_htable_endfor;

	/** XXX morse: there are better ways of determining whether this is a
	 * read request */
	M0_LOG(M0_INFO, "Number of bytes %s = %"PRIu64,
	       ioreq_sm_state(ioo) == IRS_READ_COMPLETE ? "read" : "written",
	       xfer->nxr_bytes);

	/*
	 * This function is invoked from 4 states - IRS_READ_COMPLETE,
	 * IRS_WRITE_COMPLETE, IRS_DEGRADED_READING, IRS_DEGRADED_WRITING.
	 * And the state change is applicable only for healthy state IO,
	 * meaning for states IRS_READ_COMPLETE and IRS_WRITE_COMPLETE.
	 */
	if (M0_IN(ioreq_sm_state(ioo),
		  (IRS_READ_COMPLETE, IRS_WRITE_COMPLETE,
		   IRS_TRUNCATE_COMPLETE))) {
		if (!rmw)
			ioreq_sm_state_set_locked(ioo, IRS_REQ_COMPLETE);
		else if (ioreq_sm_state(ioo) == IRS_READ_COMPLETE)
			xfer->nxr_bytes = 0;
	}

	/*
	 * nxo_dispatch may fail if connections to services have not been
	 * established yet. In this case, ioo_rc contains error code and
	 * xfer->nxr_rc == 0, don't overwrite ioo_rc.
	 *
	 * TODO: merge this with op->op_sm.sm_rc ?
	 */
	if (xfer->nxr_rc != 0)
		ioo->ioo_rc = xfer->nxr_rc;

	M0_LEAVE();
}

/**
 * Prepares each target io request in the network transfer requests, and
 * submit the fops.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_req_dispatch
 *
 * @param xfer The network transfer request.
 * @return 0 for success, -errno otherwise.
 */
static int nw_xfer_req_dispatch(struct nw_xfer_request *xfer)
{
	int                     rc = 0;
	int                     post_error = 0;
	int                     ri_error;
	uint64_t                nr_dispatched = 0;
	struct ioreq_fop       *irfop;
	struct m0_clovis_op_io *ioo;
	struct m0_clovis_op    *op;
	struct target_ioreq    *ti;
	struct m0_clovis       *instance;

	M0_ENTRY();

	M0_PRE(xfer != NULL);
	ioo = bob_of(xfer, struct m0_clovis_op_io, ioo_nwxfer, &ioo_bobtype);
	op = &ioo->ioo_oo.oo_oc.oc_op;
	instance = m0_clovis__op_instance(op);
	M0_PRE(instance != NULL);

	clovis_to_op_io_map(op, ioo);

	/* FOPs' preparation */
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		if (ti->ti_state != M0_PNDS_ONLINE) {
			M0_LOG(M0_INFO, "Skipped iofops prepare for "FID_F,
			       FID_P(&ti->ti_fid));
			continue;
		}
		ti->ti_start_time = m0_time_now();
		if (ti->ti_req_type == TI_COB_CREATE &&
		    ioreq_sm_state(ioo) == IRS_WRITING) {
			rc = ti->ti_ops->tio_cc_fops_prepare(ti);
			if (rc != 0)
				return M0_ERR_INFO(rc, "[%p] cob create fop"
						   "failed", ioo);
			continue;
		}

		if (ioreq_sm_state(ioo) == IRS_TRUNCATE) {
		    if (ti->ti_req_type == TI_READ_WRITE) {
			ti->ti_req_type = TI_COB_TRUNCATE;
			rc = ti->ti_ops->tio_cc_fops_prepare(ti);
			if (rc != 0)
				return M0_ERR(rc);
			}
			continue;
		}
		rc = ti->ti_ops->tio_iofops_prepare(ti, PA_DATA) ?:
			ti->ti_ops->tio_iofops_prepare(ti, PA_PARITY);
		if (rc != 0)
			return M0_ERR(rc);
	} m0_htable_endfor;

	/* Submit io FOPs */
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		struct m0_rpc_item *item = &ti->ti_cc_fop.crf_fop.f_item;

		/* Skips the target device if it is not online. */
		if (ti->ti_state != M0_PNDS_ONLINE) {
			M0_LOG(M0_INFO, "Skipped device "FID_F,
			       FID_P(&ti->ti_fid));
			continue;
		}
		if (ti->ti_req_type == TI_COB_CREATE &&
		    ioreq_sm_state(ioo) == IRS_WRITING) {
			/*
			 * An error returned by rpc post has been ignored.
			 * It will be handled in the respective bottom half.
			 */
			rc = m0_rpc_post(item);
			M0_CNT_INC(nr_dispatched);
			clovis_op_io_to_rpc_map(ioo, item);
			continue;
		}
		if (op->op_code == M0_CLOVIS_OC_FREE &&
		    ioreq_sm_state(ioo) == IRS_TRUNCATE &&
		    ti->ti_req_type == TI_COB_TRUNCATE) {
			if (ti->ti_trunc_ivec.iv_vec.v_nr > 0) {
				/*
				 * An error returned by rpc post has been
				 * ignored. It will be handled in the
				 * clovis_io_bottom_half().
				 */
				rc = m0_rpc_post(item);
				M0_CNT_INC(nr_dispatched);
				clovis_op_io_to_rpc_map(ioo, item);
			}
			continue;
		}
		m0_tl_for (iofops, &ti->ti_iofops, irfop) {
			rc = ioreq_fop_async_submit(&irfop->irf_iofop,
						    ti->ti_session);
			ri_error = irfop->irf_iofop.if_fop.f_item.ri_error;
			M0_LOG(M0_DEBUG, "[%p] Submitted fop for device "
			       FID_F"@%p, item %p, fop_nr=%llu, rc=%d, "
			       "ri_error=%d", ioo, FID_P(&ti->ti_fid), irfop,
			       &irfop->irf_iofop.if_fop.f_item,
			       (unsigned long long)
			       m0_atomic64_get(&xfer->nxr_iofop_nr),
			       rc, ri_error);

			/* XXX: noisy */
			clovis_op_io_to_rpc_map(ioo,
					&irfop->irf_iofop.if_fop.f_item);

			if (rc != 0)
				goto out;
			m0_atomic64_inc(&instance->m0c_pending_io_nr);
			if (ri_error == 0)
				M0_CNT_INC(nr_dispatched);
			else if (post_error == 0)
				post_error = ri_error;
		} m0_tl_endfor;
	} m0_htable_endfor;

out:
	if (rc == 0 && nr_dispatched == 0 && post_error == 0) {
		/* No fop has been dispatched.
		 *
		 * This might happen in dgmode reading:
		 *    In 'parity verify' mode, a whole parity group, including
		 *    data and parity units are all read from ioservices.
		 *    If some units failed to read, no need to read extra unit.
		 *    The units needed for recvoery are ready.
		 */
		M0_ASSERT(ioreq_sm_state(ioo) == IRS_DEGRADED_READING);
		M0_ASSERT(op->op_code == M0_CLOVIS_OC_READ &&
			  instance->m0c_config->cc_is_read_verify);
		ioreq_sm_state_set_locked(ioo, IRS_READ_COMPLETE);
	} else if (rc == 0)
		xfer->nxr_state = NXS_INFLIGHT;

	M0_LOG(M0_DEBUG, "[%p] nxr_iofop_nr %llu, nxr_rdbulk_nr %llu, "
	       "nr_dispatched %llu", ioo,
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_iofop_nr),
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_rdbulk_nr),
	       (unsigned long long)nr_dispatched);

	return M0_RC(rc);
}

static bool should_spare_be_mapped(struct m0_clovis_op_io *ioo,
				   enum m0_pool_nd_state device_state)
{
	return (M0_IN(ioreq_sm_state(ioo),
		       (IRS_READING, IRS_DEGRADED_READING)) &&
		device_state == M0_PNDS_SNS_REPAIRED) ||
	       (ioreq_sm_state(ioo) == IRS_DEGRADED_WRITING &&
		(device_state == M0_PNDS_SNS_REPAIRED ||
		 (device_state == M0_PNDS_SNS_REPAIRING &&
		  ioo->ioo_sns_state == SRS_REPAIR_DONE)));

}

/**
 * Determines which targets (spare or not) of the io map the network transfer
 * requests should be mapped to.
 * This is heavily based on m0t1fs/linux_kernel/file.c::nw_xfer_tioreq_map
 *
 * @param xfer The network transfer request.
 * @param src The parity group properties.
 * @param tgt The target parameters, contains the specified offset.
 * @param out[out] The retrieved (or allocated) target request.
 * @return 0 for success, -errno otherwise.
 */
static int nw_xfer_tioreq_map(struct nw_xfer_request           *xfer,
			      const struct m0_pdclust_src_addr *src,
			      struct m0_pdclust_tgt_addr       *tgt,
			      struct target_ioreq             **out)
{
	int                         rc;
	struct m0_fid               tfid;
	const struct m0_fid        *gfid;
	struct m0_clovis_op_io     *ioo;
	struct m0_rpc_session      *session;
	struct m0_pdclust_layout   *play;
	struct m0_pdclust_instance *play_instance;
	enum m0_pool_nd_state       device_state;
	enum m0_pool_nd_state       device_state_prev;
	uint32_t                    spare_slot;
	uint32_t                    spare_slot_prev;
	struct m0_pdclust_src_addr  spare;
	struct m0_poolmach         *pm;

	M0_ENTRY("nw_xfer_request %p", xfer);

	M0_PRE(nw_xfer_request_invariant(xfer));
	M0_PRE(src != NULL);
	M0_PRE(tgt != NULL);
	M0_PRE(out != NULL);

	ioo = bob_of(xfer, struct m0_clovis_op_io, ioo_nwxfer, &ioo_bobtype);

	play = pdlayout_get(ioo);
	M0_PRE(play != NULL);
	play_instance = pdlayout_instance(layout_instance(ioo));
	M0_PRE(play_instance != NULL);

	spare = *src;
	m0_fd_fwd_map(play_instance, src, tgt);
	tfid = target_fid(ioo, tgt);
	M0_LOG(M0_DEBUG, "src_id[%"PRIu64":%"PRIu64"] -> "
			 "dest_id[%"PRIu64":%"PRIu64"] @ tfid "FID_F,
	       src->sa_group, src->sa_unit, tgt->ta_frame, tgt->ta_obj,
	       FID_P(&tfid));

	pm = clovis_ioo_to_poolmach(ioo);
	M0_ASSERT(pm != NULL);
	rc = m0_poolmach_device_state(pm, tgt->ta_obj, &device_state);
	if (rc != 0)
		return M0_RC(rc);

	if (M0_FI_ENABLED("poolmach_client_repaired_device1")) {
		if (tfid.f_container == 1)
			device_state = M0_PNDS_SNS_REPAIRED;
	}

	/*
	 * Listed here are various possible combinations of different
	 * parameters. The cumulative result of these values decide
	 * whether given IO request should be redirected to spare
	 * or not.
	 * Note: For normal IO, M0_IN(ioreq_sm_state,
	 * (IRS_READING, IRS_WRITING)), this redirection is not needed with
	 * the exception of read IO case where the failed device is in
	 * REPAIRED state.
	 * Also, req->ir_sns_state member is used only to differentiate
	 * between 2 possible use cases during degraded mode write.
	 * This flag is not used elsewhere.
	 *
	 * Parameters:
	 * - State of IO request.
	 *   Sample set {IRS_DEGRADED_READING, IRS_DEGRADED_WRITING}
	 *
	 * - State of current device.
	 *   Sample set {M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REPAIRED}
	 *
	 * - State of SNS repair process with respect to current global fid.
	 *   Sample set {SRS_REPAIR_DONE, SRS_REPAIR_NOTDONE}
	 *
	 * Common case:
	 * req->ir_state == IRS_DEGRADED_READING &&
	 * M0_IN(req->ir_sns_state, (SRS_REPAIR_DONE || SRS_REPAIR_NOTDONE)
	 *
	 * 1. device_state == M0_PNDS_SNS_REPAIRING
	 *    In this case, data to failed device is not redirected to
	 *    spare device.
	 *    The extent is assigned to the failed device itself but
	 *    it is filtered at the level of io_req_fop.
	 *
	 * 2. device_state == M0_PNDS_SNS_REPAIRED
	 *    Here, data to failed device is redirected to respective spare
	 *    unit.
	 *
	 * Common case:
	 * req->ir_state == IRS_DEGRADED_WRITING.
	 *
	 * 1. device_state   == M0_PNDS_SNS_REPAIRED,
	 *    In this case, the device repair has finished. Ergo, data is
	 *    redirected towards respective spare unit.
	 *
	 * 2. device_state   == M0_PNDS_SNS_REPAIRING &&
	 *    req->ir_sns_state == SRS_REPAIR_DONE.
	 *    In this case, repair has finished for current global fid but
	 *    has not finished completely. Ergo, data is redirected towards
	 *    respective spare unit.
	 *
	 * 3. device_state   == M0_PNDS_SNS_REPAIRING &&
	 *    req->ir_sns_state == SRS_REPAIR_NOTDONE.
	 *    In this case, data to failed device is not redirected to the
	 *    spare unit since we drop all pages directed towards failed device.
	 *
	 * 4. device_state   == M0_PNDS_SNS_REPAIRED &&
	 *    req->ir_sns_state == SRS_REPAIR_NOTDONE.
	 *    Unlikely case! What to do in this case?
	 */
	M0_LOG(M0_INFO, "[%p] tfid "FID_F ", device state = %d\n",
	       ioo, FID_P(&tfid), device_state);
	if (should_spare_be_mapped(ioo, device_state)) {
		gfid = &ioo->ioo_oo.oo_fid;
		rc = m0_sns_repair_spare_map(
				pm, gfid, play, play_instance, src->sa_group,
				src->sa_unit, &spare_slot,
				&spare_slot_prev);

		if (rc != 0)
			return M0_RC(rc);

		/* Check if there is an effective-failure. */
		if (spare_slot_prev != src->sa_unit) {
			spare.sa_unit = spare_slot_prev;
			m0_fd_fwd_map(play_instance, &spare, tgt);
			tfid = target_fid(ioo, tgt);
			rc = m0_poolmach_device_state(
				pm, tgt->ta_obj,
				&device_state_prev);
			if (rc != 0)
				return M0_RC(rc);

		} else
			device_state_prev = M0_PNDS_SNS_REPAIRED;

		if (device_state_prev == M0_PNDS_SNS_REPAIRED) {
			spare.sa_unit = spare_slot;
			m0_fd_fwd_map(play_instance, &spare, tgt);
			tfid = target_fid(ioo, tgt);
		}
		device_state = device_state_prev;
	}
	session = target_session(ioo, tfid);

	rc = nw_xfer_tioreq_get(xfer, &tfid, tgt->ta_obj, session,
				layout_unit_size(play) * ioo->ioo_iomap_nr,
				out);

	if (M0_IN(ioreq_sm_state(ioo), (IRS_DEGRADED_READING,
					IRS_DEGRADED_WRITING)) &&
	    device_state != M0_PNDS_SNS_REPAIRED)
		(*out)->ti_state = device_state;

	return M0_RC(rc);
}

static const struct nw_xfer_ops xfer_ops = {
	.nxo_distribute = nw_xfer_io_distribute,
	.nxo_complete   = nw_xfer_req_complete,
	.nxo_dispatch   = nw_xfer_req_dispatch,
	.nxo_tioreq_map = nw_xfer_tioreq_map,
};

M0_INTERNAL void nw_xfer_request_init(struct nw_xfer_request *xfer)
{
	uint64_t                  bucket_nr;
	struct m0_clovis_op_io   *ioo;
	struct m0_pdclust_layout *play;

	M0_ENTRY("nw_xfer_request : %p", xfer);

	M0_PRE(xfer != NULL);

	ioo = bob_of(xfer, struct m0_clovis_op_io, ioo_nwxfer, &ioo_bobtype);
	nw_xfer_request_bob_init(xfer);
	xfer->nxr_rc        = 0;
	xfer->nxr_bytes     = 0;
	m0_atomic64_set(&xfer->nxr_iofop_nr, 0);
	m0_atomic64_set(&xfer->nxr_rdbulk_nr, 0);
	xfer->nxr_state     = NXS_INITIALIZED;
	xfer->nxr_ops       = &xfer_ops;
	m0_mutex_init(&xfer->nxr_lock);

	play = pdlayout_get(ioo);
	bucket_nr = layout_n(play) + 2 * layout_k(play);
	xfer->nxr_rc = tioreqht_htable_init(&xfer->nxr_tioreqs_hash,
					    bucket_nr);

	M0_POST_EX(nw_xfer_request_invariant(xfer));
	M0_LEAVE();
}

M0_INTERNAL void nw_xfer_request_fini(struct nw_xfer_request *xfer)
{
	M0_ENTRY("nw_xfer_request : %p", xfer);

	M0_PRE(xfer != NULL);
	M0_PRE(M0_IN(xfer->nxr_state, (NXS_COMPLETE, NXS_INITIALIZED)));
	M0_PRE(nw_xfer_request_invariant(xfer));
	M0_LOG(M0_DEBUG, "nw_xfer_request : %p, nxr_rc = %d",
	       xfer, xfer->nxr_rc);

	xfer->nxr_ops = NULL;
	m0_mutex_fini(&xfer->nxr_lock);
	nw_xfer_request_bob_fini(xfer);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);

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
