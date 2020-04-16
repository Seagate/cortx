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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 09-Sep-2010
 */

#include "lib/arith.h"         /* M0_3WAY */
#include "lib/memory.h"
#include "lib/errno.h"
#include "lib/misc.h"          /* M0_SET0 */
#include "lib/vec.h"
#include "mero/magic.h"
#include "rpc/rpc_opcodes.h"
#include "fol/fol.h"
#include "fol/fol_private.h"
#include "fol/fol_xc.h"       /* m0_xc_fol_init */
#include "fop/fop.h"          /* m0_fop_fol_frag_type */
#include "fop/fop_xc.h"       /* m0_fop_fol_frag_xc */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FOL
#include "lib/trace.h"

/**
   @addtogroup fol

   <b>Implementation notes.</b>

   FOL stores its records in BE log at transaction payloads and
   identifies them by transaction id.

   - struct m0_fol_rec_header
   - followed by rh_frags_nr m0_fol_frag-s
   - followed by rh_data_len bytes
   - followed by the list of fol record fragments.

   When a record is fetched from the fol, it is decoded by m0_fol_rec_decode().
   When a record is placed into the fol, its representation is prepared by
   m0_fol_rec_encode().

  @{
 */

M0_TL_DESCR_DEFINE(m0_rec_frag, "fol record fragment", M0_INTERNAL,
		   struct m0_fol_frag, rp_link, rp_magic,
		   M0_FOL_FRAG_LINK_MAGIC, M0_FOL_FRAG_HEAD_MAGIC);
M0_TL_DEFINE(m0_rec_frag, M0_INTERNAL, struct m0_fol_frag);

#define FRAG_HEADER_XCODE_OBJ(ptr) \
	M0_XCODE_OBJ(m0_fol_frag_header_xc, ptr)

#define FRAG_XCODE_OBJ(r) (struct m0_xcode_obj) { \
	.xo_type = frag->rp_ops != NULL ?             \
		   frag->rp_ops->rpo_type->rpt_xt :   \
		   m0_fop_fol_frag_type.rpt_xt,   \
	.xo_ptr  = r->rp_data                         \
}

/* ------------------------------------------------------------------
 * LSN
 * ------------------------------------------------------------------ */

static size_t fol_rec_header_pack_size(struct m0_fol_rec_header *h);

/* ------------------------------------------------------------------
 * m0_fol operations
 *------------------------------------------------------------------*/

M0_INTERNAL void m0_fol_init(struct m0_fol *fol)
{
	m0_mutex_init(&fol->f_lock);
}

M0_INTERNAL void m0_fol_fini(struct m0_fol *fol)
{
	m0_mutex_fini(&fol->f_lock);
}

/*------------------------------------------------------------------
 * FOL records and their fragments
 *------------------------------------------------------------------*/

M0_INTERNAL void m0_fol_rec_init(struct m0_fol_rec *rec, struct m0_fol *fol)
{
	m0_rec_frag_tlist_init(&rec->fr_frags);
	rec->fr_fol = fol;
}

M0_INTERNAL void m0_fol_rec_fini(struct m0_fol_rec *rec)
{
	struct m0_fol_frag *frag;

	m0_tl_for(m0_rec_frag, &rec->fr_frags, frag) {
		m0_fol_frag_fini(frag);
	} m0_tl_endfor;
	m0_rec_frag_tlist_fini(&rec->fr_frags);
}

M0_INTERNAL bool m0_fol_rec_invariant(const struct m0_fol_rec *rec)
{
	const struct m0_fol_rec_header *h = &rec->fr_header;

	return h->rh_magic == M0_FOL_REC_MAGIC;
}

M0_INTERNAL void m0_fol_frag_init(struct m0_fol_frag *frag, void *data,
				      const struct m0_fol_frag_type *type)
{
	M0_PRE(frag != NULL);
	M0_PRE(type != NULL && type->rpt_ops != NULL);

	frag->rp_data = data;
	type->rpt_ops->rpto_rec_frag_init(frag);
	m0_rec_frag_tlink_init(frag);
}

M0_INTERNAL void m0_fol_frag_fini(struct m0_fol_frag *frag)
{
	M0_PRE(frag != NULL);
	M0_PRE(frag->rp_ops != NULL);
	M0_PRE(frag->rp_data != NULL);

	if (m0_rec_frag_tlink_is_in(frag))
		m0_rec_frag_tlist_del(frag);
	m0_rec_frag_tlink_fini(frag);

	if (frag->rp_flag == M0_XCODE_DECODE) {
		m0_xcode_free_obj(&FRAG_XCODE_OBJ(frag));
		m0_free(frag);
	} else {
	    if (frag->rp_ops->rpo_type == &m0_fop_fol_frag_type) {
		m0_free(frag->rp_data);
		m0_free(frag);
	    } else
		m0_xcode_free_obj(&FRAG_XCODE_OBJ(frag));
	}
}

/* ------------------------------------------------------------------
 * Record fragment types
 * ------------------------------------------------------------------ */

enum {
	FOL_FRAG_TYPE_MAX = 128,
	PART_TYPE_START_INDEX = 1
};

static const struct m0_fol_frag_type *rptypes[FOL_FRAG_TYPE_MAX];
static struct m0_mutex rptypes_lock;

M0_INTERNAL int m0_fols_init(void)
{
	m0_mutex_init(&rptypes_lock);
	return 0;
}

M0_INTERNAL void m0_fols_fini(void)
{
	m0_mutex_fini(&rptypes_lock);
}

M0_INTERNAL int
m0_fol_frag_type_register(struct m0_fol_frag_type *type)
{
	int		result;
	static uint32_t index = PART_TYPE_START_INDEX;

	M0_PRE(type != NULL);
	M0_PRE(type->rpt_xt != NULL && type->rpt_ops != NULL);
	M0_PRE(type->rpt_index == 0);

	m0_mutex_lock(&rptypes_lock);
	if (IS_IN_ARRAY(index, rptypes)) {
		M0_ASSERT(rptypes[index] == NULL);
		rptypes[index]  = type;
		type->rpt_index = index;
		++index;
		result = 0;
	} else
		result = -EFBIG;
	m0_mutex_unlock(&rptypes_lock);
	return result;
}

M0_INTERNAL void
m0_fol_frag_type_deregister(struct m0_fol_frag_type *type)
{
	M0_PRE(type != NULL);

	m0_mutex_lock(&rptypes_lock);
	M0_PRE(IS_IN_ARRAY(type->rpt_index, rptypes));
	M0_PRE(rptypes[type->rpt_index] == type ||
	       rptypes[type->rpt_index] == NULL);

	rptypes[type->rpt_index] = NULL;
	m0_mutex_unlock(&rptypes_lock);
	type->rpt_index = 0;
	type->rpt_xt	= NULL;
	type->rpt_ops	= NULL;
}

static const struct m0_fol_frag_type *
fol_frag_type_lookup(uint32_t index)
{
	M0_PRE(IS_IN_ARRAY(index, rptypes));
	return rptypes[index];
}

/* ------------------------------------------------------------------
 * Record encoding/decoding
 * ------------------------------------------------------------------ */

static size_t fol_rec_header_pack_size(struct m0_fol_rec_header *h)
{
	struct m0_xcode_ctx ctx;
	int len = m0_xcode_data_size(&ctx, &M0_REC_HEADER_XCODE_OBJ(h));

	M0_POST(len > 0);
	return len;
}

static size_t fol_record_pack_size(struct m0_fol_rec *rec)
{
	struct m0_fol_rec_header     *h = &rec->fr_header;
	const struct m0_fol_frag     *frag;
	struct m0_fol_frag_header     rph;
	struct m0_xcode_ctx           ctx;
	size_t                        len;

	len = fol_rec_header_pack_size(h) +
	      h->rh_frags_nr *
		m0_xcode_data_size(&ctx, &FRAG_HEADER_XCODE_OBJ(&rph));

	m0_tl_for(m0_rec_frag, &rec->fr_frags, frag) {
		len += m0_xcode_data_size(&ctx, &FRAG_XCODE_OBJ(frag));
	} m0_tl_endfor;

	len = m0_align(len, 8);
	M0_POST(len <= FOL_REC_MAXSIZE);
	return len;
}

static int fol_rec_encdec(struct m0_fol_rec *rec,
			  struct m0_bufvec_cursor *cur,
			  enum m0_xcode_what what)
{
	struct m0_fol_rec_header *h = &rec->fr_header;
	int			  rc;

	M0_PRE(ergo(what == M0_XCODE_ENCODE, h->rh_magic == M0_FOL_REC_MAGIC));

	rc = m0_xcode_encdec(&M0_REC_HEADER_XCODE_OBJ(h), cur, what);
	if (rc != 0)
		return M0_RC(rc);

	M0_POST(ergo(what == M0_XCODE_DECODE, h->rh_magic == M0_FOL_REC_MAGIC));
	return 0;
}

static int fol_record_pack(struct m0_fol_rec *rec, struct m0_buf *buf)
{
	struct m0_fol_frag     *frag;
	m0_bcount_t             len = buf->b_nob;
	struct m0_bufvec        bvec = M0_BUFVEC_INIT_BUF(&buf->b_addr, &len);
	struct m0_bufvec_cursor cur;
	int			rc;

	m0_bufvec_cursor_init(&cur, &bvec);

	rc = fol_rec_encdec(rec, &cur, M0_XCODE_ENCODE);
	if (rc != 0)
		return M0_RC(rc);

	m0_tl_for(m0_rec_frag, &rec->fr_frags, frag) {
		struct m0_fol_frag_header rph;
		uint32_t		  index;

		index = frag->rp_ops != NULL ?
			frag->rp_ops->rpo_type->rpt_index :
			m0_fop_fol_frag_type.rpt_index;

		rph = (struct m0_fol_frag_header) {
			.rph_index = index,
			.rph_magic = M0_FOL_FRAG_MAGIC
		};

		rc = m0_xcode_encdec(&FRAG_HEADER_XCODE_OBJ(&rph),
				     &cur, M0_XCODE_ENCODE) ?:
		     m0_xcode_encdec(&FRAG_XCODE_OBJ(frag),
				     &cur, M0_XCODE_ENCODE);
		if (rc != 0)
			return M0_RC(rc);
	} m0_tl_endfor;
	buf->b_nob = m0_bufvec_cursor_addr(&cur) - buf->b_addr;

	return M0_RC(rc);
}

M0_INTERNAL int m0_fol_rec_encode(struct m0_fol_rec *rec, struct m0_buf *at)
{
	struct m0_fol_rec_header *h = &rec->fr_header;
	size_t                    size;

	h->rh_magic = M0_FOL_REC_MAGIC;
	h->rh_frags_nr = m0_rec_frag_tlist_length(&rec->fr_frags);

	size = fol_record_pack_size(rec);
	M0_ASSERT(M0_IS_8ALIGNED(size));
	M0_ASSERT(size <= at->b_nob);

	h->rh_data_len = size;

	return fol_record_pack(rec, at);
}

M0_INTERNAL int m0_fol_rec_decode(struct m0_fol_rec *rec, struct m0_buf *at)
{
	struct m0_bufvec        bvec = M0_BUFVEC_INIT_BUF(&at->b_addr,
							  &at->b_nob);
	struct m0_bufvec_cursor cur;
	uint32_t                i;
	int                     rc;

	m0_bufvec_cursor_init(&cur, &bvec);

	rc = fol_rec_encdec(rec, &cur, M0_XCODE_DECODE);
	if (rc != 0)
		return M0_RC(rc);

	for (i = 0; rc == 0 && i < rec->fr_header.rh_frags_nr; ++i) {
		struct m0_fol_frag            *frag;
		const struct m0_fol_frag_type *frag_type;
		struct m0_fol_frag_header      ph;

		rc = m0_xcode_encdec(&FRAG_HEADER_XCODE_OBJ(&ph), &cur,
				     M0_XCODE_DECODE);
		if (rc == 0) {
			void *rp_data;

			frag_type = fol_frag_type_lookup(ph.rph_index);

			M0_ALLOC_PTR(frag);
			if (frag == NULL)
				return M0_ERR(-ENOMEM);

			rp_data = m0_alloc(frag_type->rpt_xt->xct_sizeof);
			if (rp_data == NULL) {
				m0_free(frag);
				return M0_ERR(-ENOMEM);
			}

			frag->rp_flag = M0_XCODE_DECODE;

			m0_fol_frag_init(frag, rp_data, frag_type);
			rc = m0_xcode_encdec(&FRAG_XCODE_OBJ(frag), &cur,
					     M0_XCODE_DECODE);
			if (rc == 0)
				m0_fol_frag_add(rec, frag);
		}
	}
	return M0_RC(rc);
}

/* @todo Return number of bytes that needed to print everything */
int m0_fol_rec_to_str(struct m0_fol_rec *rec, char *str, int str_len)
{
	int ret;
	struct m0_fol_rec_header *h = &rec->fr_header;

	ret = snprintf(str, str_len, "rec->fr_header->rh_frags_nr: %u\n"
		 "rec->fr_header->rh_data_len: %u\n"
		 "rec->fr_header->rh_self->ui_node: %u\n"
		 "rec->fr_header->rh_self->ui_update: %" PRIu64 "\n"
		 "rec->fr_epoch: %p\n"
		 "rec->fr_sibling: %p\n",
		 h->rh_frags_nr, h->rh_data_len, h->rh_self.ui_node,
		 h->rh_self.ui_update, rec->fr_epoch, rec->fr_sibling);

	if (ret >= str_len) {
		return -ENOMEM;
	}
	else {
		struct m0_fol_frag     *frag;
		str_len -= ret;
		str += ret;

		m0_tl_for(m0_rec_frag, &rec->fr_frags, frag) {
			uint32_t		  index;

			index = frag->rp_ops != NULL ?
				frag->rp_ops->rpo_type->rpt_index :
				m0_fop_fol_frag_type.rpt_index;

			ret = snprintf(str, str_len,
				"frag->rp_ops: %p\n"
				"frag->rp_ops->rpo_type: %p\n"
				"frag->rp_ops->rpo_type->rpt_index: %d\n"
				"frag->rp_ops->rpo_type->rpt_name: %s\n"
				"frag->rp_data: %p\n",
				frag->rp_ops, frag->rp_ops->rpo_type, index,
				frag->rp_ops ?
				    frag->rp_ops->rpo_type->rpt_name : "(nil)",
				frag->rp_data);

			if (ret >= str_len) {
				return -ENOMEM;
			}
			else {
				str_len -= ret;
				str += ret;
			}

			if (frag->rp_ops &&
			    frag->rp_ops->rpo_type == &m0_fop_fol_frag_type) {
				struct m0_fop_fol_frag *rp = frag->rp_data;
				struct m0_xcode_obj obj = {
					.xo_type = m0_fop_fol_frag_xc,
					.xo_ptr = rp
				};

				ret = snprintf(str, str_len,
						"rp->ffrp_fop_code: %u\n"
						"rp->ffrp_rep_code: %u\n",
						rp->ffrp_fop_code,
						rp->ffrp_rep_code);

				if (ret >= str_len) {
					return -ENOMEM;
				}
				else {
					str_len -= ret;
					str += ret;
				}

				ret = m0_xcode_print(&obj, str, str_len);

				if (ret < 0) {
					return ret;
				} else if (ret >= str_len) {
					return -ENOMEM;
				} else {
					str_len -= ret;
					str += ret;
				}
			}
		} m0_tl_endfor;
	}

	return 0;
}

M0_INTERNAL void m0_fol_frag_add(struct m0_fol_rec *rec,
				     struct m0_fol_frag *frag)
{
	M0_PRE(rec != NULL && frag != NULL);
	m0_rec_frag_tlist_add_tail(&rec->fr_frags, frag);
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of fol group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
