/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 29-May-2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/seg.h"

#include "lib/misc.h"         /* M0_IN */
#include "lib/memory.h"       /* m0_alloc_aligned */
#include "lib/errno.h"        /* ENOMEM */
#include "lib/time.h"         /* m0_time_now */
#include "lib/atomic.h"       /* m0_atomic64 */

#include "mero/version.h"     /* m0_build_info_get */

#include "stob/stob.h"        /* m0_stob, m0_stob_fd */

#include "be/seg_internal.h"  /* m0_be_seg_hdr */
#include "be/io.h"            /* m0_be_io */

#include <sys/mman.h>         /* mmap */
#include <search.h>           /* twalk */

/**
 * @addtogroup be
 *
 * @{
 */

static const struct m0_be_seg_geom *
be_seg_geom_find_by_id(const struct m0_be_seg_hdr *hdr, uint64_t id)
{
	uint16_t i;

	for (i = 0; i < hdr->bh_items_nr; ++i)
		if (hdr->bh_items[i].sg_id == id)
			return &hdr->bh_items[i];

	return NULL;
}

static int be_seg_geom_len(const struct m0_be_seg_geom *geom)
{
	uint16_t len;

	if (geom == NULL || m0_be_seg_geom_eq(geom, &M0_BE_SEG_GEOM0))
		return -ENOENT;

	for (len = 0; !m0_be_seg_geom_eq(&geom[len], &M0_BE_SEG_GEOM0); ++len)
		;

	return len;
}

static int be_seg_hdr_size(void)
{
	return sizeof(struct m0_be_seg_hdr);
}

static int be_seg_hdr_create(struct m0_stob *stob, struct m0_be_seg_hdr *hdr)
{
	struct m0_be_seg_geom *geom = hdr->bh_items;
	uint16_t               len  = hdr->bh_items_nr;
	unsigned char          last_byte;
	int                    rc = -ENOENT;
	int                    i;

	m0_format_header_pack(&hdr->bh_header, &(struct m0_format_tag){
		.ot_version = M0_BE_SEG_HDR_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_BE_SEG_HDR,
		.ot_footer_offset = offsetof(struct m0_be_seg_hdr, bh_footer)
	});

	for (i = 0; i < len; ++i) {
		const struct m0_be_seg_geom *g = &geom[i];
		M0_LOG(M0_DEBUG, "stob=%p size=%lu addr=%p offset=%lu id=%lu",
		       stob, g->sg_size, g->sg_addr, g->sg_offset, g->sg_id);

		M0_PRE(g->sg_addr != NULL);
		M0_PRE(g->sg_size > 0);

		/* offset, size, addr must be a multiple of the page size as
		 * returned by M0_BE_SEG_PAGE_SIZE. */
		M0_PRE(m0_is_aligned((uint64_t)g->sg_addr,
				     M0_BE_SEG_PAGE_SIZE));
		M0_PRE(m0_is_aligned(g->sg_offset, M0_BE_SEG_PAGE_SIZE));
		M0_PRE(m0_is_aligned(g->sg_size, M0_BE_SEG_PAGE_SIZE));

		hdr->bh_id = g->sg_id;
		strncpy(hdr->bh_be_version,
			m0_build_info_get()->bi_xcode_protocol_be_checksum,
			M0_BE_SEG_HDR_VERSION_LEN_MAX);
		M0_CASSERT(sizeof hdr->bh_be_version >
			   M0_BE_SEG_HDR_VERSION_LEN_MAX);
		/* Avoid buffer overflow */
		hdr->bh_be_version[M0_BE_SEG_HDR_VERSION_LEN_MAX] = '\0';
		m0_format_footer_update(hdr);
		rc = m0_be_io_single(stob, SIO_WRITE, hdr, g->sg_offset,
				     be_seg_hdr_size());

		/* Do not move this block out of the cycle.  Segments can come
		 * unordered inside @geom. */
		if (rc == 0) {
			/*
			 * Write the last byte on the backing store.
			 *
			 * mmap() will have problem with regular file mapping
			 * otherwise.  Also checks that device (if used as
			 * backing storage) has enough size to be used as
			 * segment backing store.
			 */
			last_byte = 0;
			rc = m0_be_io_single(stob, SIO_WRITE, &last_byte,
					     g->sg_offset + g->sg_size - 1,
					     sizeof last_byte);
			if (rc != 0)
				M0_LOG(M0_WARN,
				       "can't write segment's last byte");
		} else {
			M0_LOG(M0_WARN, "can't write segment header");
		}
		if (rc != 0)
			break;
	}
	return M0_RC(rc);
}

M0_INTERNAL bool m0_be_seg_geom_eq(const struct m0_be_seg_geom *left,
				   const struct m0_be_seg_geom *right)
{
	return memcmp(left, right, sizeof(struct m0_be_seg_geom)) == 0;
}

static bool be_seg_geom_has_no_overlapps(const struct m0_be_seg_geom *geom,
					 int len)
{
	int           i;
	int           j;
	struct m0_ext ei;
	struct m0_ext ej;
	struct m0_ext eai;
	struct m0_ext eaj;


	M0_PRE(len > 0);

	for (i = 0; i < len; ++i) {
		ei = M0_EXT(geom[i].sg_offset,
			    geom[i].sg_offset + geom[i].sg_size);
		eai = M0_EXT((m0_bindex_t)geom[i].sg_addr,
			     (m0_bindex_t)geom[i].sg_addr +
			     geom[i].sg_size);

		for (j = 0; j < len; ++j) {
			if (&geom[j] == &geom[i])
				continue;

			ej = M0_EXT(geom[j].sg_offset,
				    geom[j].sg_offset + geom[j].sg_size);
			eaj = M0_EXT((m0_bindex_t)geom[j].sg_addr,
				     (m0_bindex_t)geom[j].sg_addr +
				     geom[j].sg_size);

			if (m0_ext_are_overlapping(&ei, &ej) ||
			    m0_ext_are_overlapping(&eai, &eaj))
				return false;
		}
	}

	return true;
}

M0_INTERNAL int m0_be_seg_create_multiple(struct m0_stob *stob,
					  const struct m0_be_seg_geom *geom)
{
	struct m0_be_seg_hdr  *hdr;
	int		       len;
	int                    i;
	int		       rc;

	len = be_seg_geom_len(geom);
	if (len < 0)
		return M0_RC(len);

	M0_ASSERT(len <= M0_BE_SEG_HDR_GEOM_ITMES_MAX);
	M0_ASSERT(be_seg_geom_has_no_overlapps(geom, len));

	hdr = m0_alloc(be_seg_hdr_size());
	if (hdr == NULL)
		return M0_RC(-ENOMEM);

	hdr->bh_items_nr = len;
	for (i = 0; i < len; ++i)
		hdr->bh_items[i] = geom[i];

	rc = be_seg_hdr_create(stob, hdr);
	m0_free(hdr);

	return M0_RC(rc);
}

M0_INTERNAL int m0_be_seg_create(struct m0_be_seg *seg,
				 m0_bcount_t size,
				 void *addr)
{
	struct m0_be_seg_geom geom[] = {
		[0] = {
			.sg_size = size,
			.sg_addr = addr,
			.sg_offset = 0ULL,
			.sg_id = seg->bs_id,
		},

		[1] = M0_BE_SEG_GEOM0,
	};

	return m0_be_seg_create_multiple(seg->bs_stob, geom);
}

M0_INTERNAL int m0_be_seg_destroy(struct m0_be_seg *seg)
{
	M0_ENTRY("seg=%p", seg);
	M0_PRE(M0_IN(seg->bs_state, (M0_BSS_INIT, M0_BSS_CLOSED)));

	/* XXX TODO: seg destroy ... */

	return M0_RC(0);
}

M0_INTERNAL void m0_be_seg_init(struct m0_be_seg    *seg,
				struct m0_stob      *stob,
				struct m0_be_domain *dom,
				uint64_t             seg_id)
{
	M0_ENTRY("seg=%p", seg);

	/* XXX: Remove this eventually, after explicitly called
	 * m0_be_seg_create_multiple() is used everywhere */
	if (seg_id == M0_BE_SEG_FAKE_ID)
		seg_id = m0_stob_fid_get(stob)->f_key;

	*seg = (struct m0_be_seg) {
		.bs_domain   = dom,
		.bs_stob     = stob,
		.bs_state    = M0_BSS_INIT,
		.bs_id       = seg_id,
	};
	m0_stob_get(seg->bs_stob);
	M0_LEAVE();
}

M0_INTERNAL void m0_be_seg_fini(struct m0_be_seg *seg)
{
	M0_ENTRY("seg=%p", seg);
	M0_PRE(M0_IN(seg->bs_state, (M0_BSS_INIT, M0_BSS_CLOSED)));
	m0_stob_put(seg->bs_stob);
	M0_LEAVE();
}

M0_INTERNAL bool m0_be_seg__invariant(const struct m0_be_seg *seg)
{
	return _0C(seg != NULL) &&
	       _0C(seg->bs_addr != NULL) &&
	       _0C(seg->bs_size > 0);
}

bool m0_be_reg__invariant(const struct m0_be_reg *reg)
{
	return _0C(reg != NULL) && _0C(reg->br_seg != NULL) &&
	       _0C(reg->br_size > 0) && _0C(reg->br_addr != NULL) &&
	       _0C(m0_be_seg_contains(reg->br_seg, reg->br_addr)) &&
	       _0C(m0_be_seg_contains(reg->br_seg,
				      reg->br_addr + reg->br_size - 1));
}

static void be_seg_madvise(struct m0_be_seg *seg, m0_bcount_t dump_limit,
			   int flag)
{
	int rc;

	if (dump_limit >= seg->bs_size)
		return;

	if (flag == MADV_DONTDUMP)
		rc = m0_dont_dump(seg->bs_addr + dump_limit,
				  seg->bs_size - dump_limit);
	else
		rc = madvise(seg->bs_addr + dump_limit,
			     seg->bs_size - dump_limit,
			     flag);

	if (rc == 0)
		M0_LOG(M0_INFO, "madvise(%p, %"PRIu64", %d) = %d",
		       seg->bs_addr, seg->bs_size, flag, rc);
	else
		M0_LOG(M0_ERROR, "madvise(%p, %"PRIu64", %d) = %d",
		       seg->bs_addr, seg->bs_size, flag, rc);

}

M0_INTERNAL int m0_be_seg_open(struct m0_be_seg *seg)
{
	const struct m0_be_seg_geom *g;
	struct m0_be_seg_hdr        *hdr;
	const char                  *runtime_be_version;
	void                        *p;
	int                          fd;
	int                          rc;

	M0_ENTRY("seg=%p", seg);
	M0_PRE(M0_IN(seg->bs_state, (M0_BSS_INIT, M0_BSS_CLOSED)));

	hdr = m0_alloc(be_seg_hdr_size());
	if (hdr == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_be_io_single(seg->bs_stob, SIO_READ,
			     hdr, M0_BE_SEG_HEADER_OFFSET, be_seg_hdr_size());
	if (rc != 0) {
		m0_free(hdr);
		return M0_ERR(rc);
	}

	runtime_be_version = m0_build_info_get()->
				bi_xcode_protocol_be_checksum;
	if (strncmp(hdr->bh_be_version, runtime_be_version,
		    M0_BE_SEG_HDR_VERSION_LEN_MAX) != 0) {
		rc = M0_ERR_INFO(-EPROTO, "BE protocol checksum mismatch:"
				 " expected '%s', stored on disk '%s'",
				 runtime_be_version, (char*)hdr->bh_be_version);
		m0_free(hdr);
		return rc;
	}

	g = be_seg_geom_find_by_id(hdr, seg->bs_id);
	if (g == NULL) {
		m0_free(hdr);
		return M0_ERR(-ENOENT);
	}

	fd = m0_stob_fd(seg->bs_stob);
	p = mmap(g->sg_addr, g->sg_size, PROT_READ | PROT_WRITE,
		 MAP_FIXED | MAP_PRIVATE | MAP_NORESERVE, fd, g->sg_offset);
	if (p != g->sg_addr) {
		rc = M0_ERR_INFO(-errno, "p=%p g->sg_addr=%p fd=%d",
				 p, g->sg_addr, fd);
		/* `g' is a part of `hdr'. Don't print it after free. */
		m0_free(hdr);
		return rc;
	}

	/* rc = be_seg_read_all(seg, &hdr); */
	rc = 0;
	if (rc == 0) {
		seg->bs_reserved = be_seg_hdr_size();
		seg->bs_size     = g->sg_size;
		seg->bs_addr     = g->sg_addr;
		seg->bs_offset   = g->sg_offset;
		seg->bs_state    = M0_BSS_OPENED;
		be_seg_madvise(seg, M0_BE_SEG_CORE_DUMP_LIMIT, MADV_DONTDUMP);
		be_seg_madvise(seg,                      0ULL, MADV_DONTFORK);
	} else {
		munmap(g->sg_addr, g->sg_size);
	}

	m0_free(hdr);
	return M0_RC(rc);
}

M0_INTERNAL void m0_be_seg_close(struct m0_be_seg *seg)
{
	M0_ENTRY("seg=%p", seg);
	M0_PRE(seg->bs_state == M0_BSS_OPENED);

	munmap(seg->bs_addr, seg->bs_size);
	seg->bs_state = M0_BSS_CLOSED;
	M0_LEAVE();
}

M0_INTERNAL bool m0_be_seg_contains(const struct m0_be_seg *seg,
				    const void *addr)
{
	return seg->bs_addr <= addr && addr < seg->bs_addr + seg->bs_size;
}

M0_INTERNAL bool m0_be_reg_eq(const struct m0_be_reg *r1,
			      const struct m0_be_reg *r2)
{
	return r1->br_seg == r2->br_seg &&
	       r1->br_size == r2->br_size &&
	       r1->br_addr == r2->br_addr;
}

M0_INTERNAL m0_bindex_t m0_be_seg_offset(const struct m0_be_seg *seg,
					 const void *addr)
{
	M0_PRE(m0_be_seg_contains(seg, addr));
	return addr - seg->bs_addr + seg->bs_offset;
}

M0_INTERNAL m0_bindex_t m0_be_reg_offset(const struct m0_be_reg *reg)
{
	return m0_be_seg_offset(reg->br_seg, reg->br_addr);
}

M0_INTERNAL m0_bcount_t m0_be_seg_reserved(const struct m0_be_seg *seg)
{
	return seg->bs_reserved;
}

M0_INTERNAL struct m0_be_allocator *m0_be_seg_allocator(struct m0_be_seg *seg)
{
	return &seg->bs_allocator;
}

static int
be_seg_io(struct m0_be_reg *reg, void *ptr, enum m0_stob_io_opcode opcode)
{
	return m0_be_io_single(reg->br_seg->bs_stob, opcode,
			       ptr, m0_be_reg_offset(reg), reg->br_size);
}

M0_INTERNAL int m0_be_seg__read(struct m0_be_reg *reg, void *dst)
{
	return be_seg_io(reg, dst, SIO_READ);
}

M0_INTERNAL int m0_be_seg__write(struct m0_be_reg *reg, void *src)
{
	return be_seg_io(reg, src, SIO_WRITE);
}

M0_INTERNAL int m0_be_reg__read(struct m0_be_reg *reg)
{
	return m0_be_seg__read(reg, reg->br_addr);
}

M0_INTERNAL int m0_be_reg__write(struct m0_be_reg *reg)
{
	return m0_be_seg__write(reg, reg->br_addr);
}

/* #define USE_TIME_NOW_AS_GEN_IDX */

M0_INTERNAL unsigned long m0_be_reg_gen_idx(const struct m0_be_reg *reg)
{
#ifdef USE_TIME_NOW_AS_GEN_IDX
	m0_time_t now = m0_time_now();

	M0_CASSERT(sizeof now == sizeof(unsigned long));
	return now;
#else
	static struct m0_atomic64 global_gen_idx = {};

	M0_CASSERT(sizeof global_gen_idx == sizeof(unsigned long));
	return m0_atomic64_add_return(&global_gen_idx, 1);
#endif
}

M0_INTERNAL bool m0_be_seg_contains_stob(struct m0_be_seg        *seg,
                                         const struct m0_stob_id *stob_id)
{
	return m0_stob_id_eq(m0_stob_id_get(seg->bs_stob), stob_id);
}

/** @} end of be group */
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
