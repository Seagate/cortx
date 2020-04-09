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
 * Original author: Amit Jambure <amit_jambure@xyratex.com>
 * Original creation date: 05/25/2012
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FORMATION
#include "lib/trace.h"

#include "lib/tlist.h"
#include "lib/misc.h"
#include "lib/vec.h"
#include "lib/errno.h"
#include "lib/finject.h"
#include "lib/memory.h"
#include "mero/magic.h"
#include "xcode/xcode.h"
#include "rpc/rpc_internal.h"
#include "reqh/reqh.h"
#include "format/format.h"
#include "addb2/addb2.h"
#include "rpc/addb2.h"


/**
 * @addtogroup rpc
 * @{
 */

#define PACKHD_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_packet_onwire_header_xc, ptr)
#define PACKFT_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_rpc_packet_onwire_footer_xc, ptr)

M0_TL_DESCR_DEFINE(packet_item, "packet_item", M0_INTERNAL, struct m0_rpc_item,
                   ri_plink, ri_magic, M0_RPC_ITEM_MAGIC,
                   M0_RPC_PACKET_HEAD_MAGIC);
M0_TL_DEFINE(packet_item, M0_INTERNAL, struct m0_rpc_item);

M0_INTERNAL m0_bcount_t m0_rpc_packet_onwire_header_size(void)
{
	struct m0_rpc_packet_onwire_header oh;
	struct m0_xcode_ctx                ctx;
	static m0_bcount_t                 packet_header_size;

	if (packet_header_size == 0) {
		m0_xcode_ctx_init(&ctx, &PACKHD_XCODE_OBJ(&oh));
		packet_header_size = m0_xcode_length(&ctx);
	}

	return packet_header_size;
}

M0_INTERNAL m0_bcount_t m0_rpc_packet_onwire_footer_size(void)
{
	struct m0_rpc_packet_onwire_footer of;
	struct m0_xcode_ctx                ctx;
	static m0_bcount_t                 packet_footer_size;

	if (packet_footer_size == 0) {
		m0_xcode_ctx_init(&ctx, &PACKFT_XCODE_OBJ(&of));
		packet_footer_size = m0_xcode_length(&ctx);
	}

	return packet_footer_size;
}

M0_INTERNAL bool m0_rpc_packet_invariant(const struct m0_rpc_packet *p)
{
	struct m0_rpc_item *item;
	m0_bcount_t         size;

	size = 0;
	for_each_item_in_packet(item, p) {
		size += m0_rpc_item_size(item);
	} end_for_each_item_in_packet;

	return
		_0C(p != NULL) &&
		_0C(p->rp_ow.poh_version != 0) &&
		_0C(p->rp_ow.poh_magic == M0_RPC_PACKET_HEAD_MAGIC) &&
		_0C(p->rp_rmachine != NULL) &&
		_0C(p->rp_ow.poh_nr_items ==
			packet_item_tlist_length(&p->rp_items)) &&
		_0C(p->rp_size == size + m0_rpc_packet_onwire_header_size() +
				  m0_rpc_packet_onwire_footer_size());
}

M0_INTERNAL void m0_rpc_packet_init(struct m0_rpc_packet *p,
				    struct m0_rpc_machine *rmach)
{
	M0_ENTRY("packet: %p", p);
	M0_PRE(p != NULL);

	M0_SET0(p);

	p->rp_ow.poh_version = M0_RPC_VERSION_1;
	p->rp_ow.poh_magic = M0_RPC_PACKET_HEAD_MAGIC;
	p->rp_size = m0_rpc_packet_onwire_header_size() +
		     m0_rpc_packet_onwire_footer_size();
	packet_item_tlist_init(&p->rp_items);
	p->rp_rmachine = rmach;

	M0_ASSERT(m0_rpc_packet_invariant(p));
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_packet_fini(struct m0_rpc_packet *p)
{
	M0_ENTRY("packet: %p nr_items: %llu", p,
		 (unsigned long long)p->rp_ow.poh_nr_items);
	M0_PRE(m0_rpc_packet_invariant(p) && p->rp_ow.poh_nr_items == 0);

	packet_item_tlist_fini(&p->rp_items);
	M0_SET0(p);

	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_packet_discard(struct m0_rpc_packet *packet)
{
	m0_rpc_packet_remove_all_items(packet);
	m0_rpc_packet_fini(packet);
	m0_free(packet);
}

M0_INTERNAL void m0_rpc_packet_add_item(struct m0_rpc_packet *p,
					struct m0_rpc_item *item)
{
	M0_ENTRY("packet: %p, item: %p[%s/%u]", p, item,
		 item_kind(item), item->ri_type->rit_opcode);
	M0_PRE_EX(m0_rpc_packet_invariant(p) && item != NULL);
	M0_PRE(!packet_item_tlink_is_in(item));
	M0_PRE(m0_rpc_machine_is_locked(p->rp_rmachine));

	m0_rpc_item_get(item);
	item->ri_rmachine = p->rp_rmachine;
	item->ri_packet = p;
	packet_item_tlink_init_at_tail(item, &p->rp_items);
	++p->rp_ow.poh_nr_items;
	p->rp_size += m0_rpc_item_size(item);

	M0_LOG(M0_DEBUG, "packet: %p nr_items: %llu packet size: %llu",
			p, (unsigned long long)p->rp_ow.poh_nr_items,
			(unsigned long long)p->rp_size);
	M0_ASSERT_EX(m0_rpc_packet_invariant(p));
	M0_POST(m0_rpc_packet_is_carrying_item(p, item));
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_packet_remove_item(struct m0_rpc_packet *p,
					   struct m0_rpc_item *item)
{
	M0_ENTRY("packet: %p, item: %p[%s/%u]", p, item,
		 item_kind(item), item->ri_type->rit_opcode);
	M0_PRE_EX(m0_rpc_packet_invariant(p) && item != NULL);
	M0_PRE(m0_rpc_packet_is_carrying_item(p, item));
	M0_PRE(m0_rpc_machine_is_locked(p->rp_rmachine));

	packet_item_tlink_del_fini(item);
	item->ri_packet = NULL;
	--p->rp_ow.poh_nr_items;
	p->rp_size -= m0_rpc_item_size(item);

	M0_LOG(M0_DEBUG, "p %p, nr_items: %llu->%llu packet size: %llu->%llu",
			p, (unsigned long long)p->rp_ow.poh_nr_items + 1,
			(unsigned long long)p->rp_ow.poh_nr_items,
			(unsigned long long)p->rp_size + m0_rpc_item_size(item),
			(unsigned long long)p->rp_size);
	M0_POST(!packet_item_tlink_is_in(item));
	m0_rpc_item_put(item);
	M0_ASSERT_EX(m0_rpc_packet_invariant(p));
	M0_LEAVE();
}

M0_INTERNAL void m0_rpc_packet_remove_all_items(struct m0_rpc_packet *p)
{
	struct m0_rpc_item *item;

	M0_ENTRY("packet: %p", p);
	M0_PRE_EX(m0_rpc_packet_invariant(p));
	M0_PRE(m0_rpc_machine_is_locked(p->rp_rmachine));

	M0_LOG(M0_DEBUG, "p %p, nr_items: %d", p, (int)p->rp_ow.poh_nr_items);

	for_each_item_in_packet(item, p) {
		m0_rpc_packet_remove_item(p, item);
	} end_for_each_item_in_packet;

	M0_POST_EX(m0_rpc_packet_invariant(p) && m0_rpc_packet_is_empty(p));
	M0_LEAVE();
}

M0_INTERNAL bool m0_rpc_packet_is_carrying_item(const struct m0_rpc_packet *p,
						const struct m0_rpc_item *item)
{
	return packet_item_tlist_contains(&p->rp_items, item);
}

M0_INTERNAL bool m0_rpc_packet_is_empty(const struct m0_rpc_packet *p)
{
	M0_PRE_EX(m0_rpc_packet_invariant(p));

	return p->rp_ow.poh_nr_items == 0;
}

M0_INTERNAL int m0_rpc_packet_encode(struct m0_rpc_packet *p,
				     struct m0_bufvec *bufvec)
{
	struct m0_bufvec_cursor cur;
	m0_bcount_t             bufvec_size;

	M0_ENTRY("packet: %p bufvec: %p", p, bufvec);
	M0_PRE_EX(m0_rpc_packet_invariant(p) && bufvec != NULL);
	M0_PRE(!m0_rpc_packet_is_empty(p));

	if (M0_FI_ENABLED("fake_error"))
		return M0_ERR(-EFAULT);

	bufvec_size = m0_vec_count(&bufvec->ov_vec);

	M0_ASSERT(M0_IS_8ALIGNED(bufvec_size));
	M0_ASSERT(m0_forall(i, bufvec->ov_vec.v_nr,
			    M0_IS_8ALIGNED(bufvec->ov_vec.v_count[i])));
	M0_ASSERT(bufvec_size >= p->rp_size);

	m0_bufvec_cursor_init(&cur, bufvec);
	M0_ASSERT(M0_IS_8ALIGNED(m0_bufvec_cursor_addr(&cur)));

	return M0_RC(m0_rpc_packet_encode_using_cursor(p, &cur));
}

static int packet_header_encdec(struct m0_rpc_packet_onwire_header *ph,
				struct m0_bufvec_cursor            *cursor,
				enum m0_xcode_what                  what)

{
	M0_ENTRY();
	return M0_RC(m0_xcode_encdec(&PACKHD_XCODE_OBJ(ph), cursor, what));
}

static int packet_footer_encdec(struct m0_rpc_packet_onwire_footer *pf,
				struct m0_bufvec_cursor            *cursor,
				enum m0_xcode_what                  what)

{
	M0_ENTRY();
	return M0_RC(m0_xcode_encdec(&PACKFT_XCODE_OBJ(pf), cursor, what));
}

static int item_encode(struct m0_rpc_item       *item,
		       struct m0_bufvec_cursor  *cursor)
{
	struct m0_rpc_item_header1 ioh;
	struct m0_format_tag       item_format_tag = {
		.ot_version = M0_RPC_ITEM_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_RPC_ITEM,
	};
	struct m0_rpc_item_footer  iof;
	int                        rc;
	struct m0_ha_domain       *ha_dom;
	uint64_t                   epoch = M0_HA_EPOCH_NONE;

	M0_ENTRY("item: %p cursor: %p", item, cursor);
	M0_PRE(item != NULL && cursor != NULL);
	M0_PRE(item->ri_type != NULL &&
	       item->ri_type->rit_ops != NULL &&
	       item->ri_type->rit_ops->rito_encode != NULL);

	if (item->ri_ha_epoch != M0_HA_EPOCH_NONE)
		epoch = item->ri_ha_epoch;
	else if (item->ri_rmachine != NULL &&
		 item->ri_rmachine->rm_reqh != NULL) {
		ha_dom = &item->ri_rmachine->rm_reqh->rh_hadom;
		epoch = ha_dom->hdo_epoch;
	}

	M0_LOG(M0_DEBUG, "ha_epoch: %lu", (unsigned long)epoch);
	ioh = (struct m0_rpc_item_header1){
		.ioh_opcode   = item->ri_type->rit_opcode,
		.ioh_flags    = item->ri_flags,
		.ioh_ha_epoch = epoch,
		.ioh_magic    = M0_RPC_ITEM_MAGIC,
	};

	/* measured in bytes: including header, payload, and footer */
	item_format_tag.ot_size = item->ri_size;
	m0_format_header_pack(&ioh.ioh_header, &item_format_tag);

	if (item->ri_nr_sent > 0)
		ioh.ioh_flags |= M0_RIF_DUP;
	rc = m0_rpc_item_header1_encdec(&ioh, cursor, M0_XCODE_ENCODE);
	if (rc == 0)
		rc = item->ri_type->rit_ops->rito_encode(item->ri_type,
							 item, cursor);
	if (rc == 0) {
		m0_format_footer_generate(&iof.iof_footer, NULL, 0);
		rc = m0_rpc_item_footer_encdec(&iof, cursor, M0_XCODE_ENCODE);
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_rpc_packet_encode_using_cursor(struct m0_rpc_packet *packet,
						  struct m0_bufvec_cursor
						  *cursor)
{
	struct m0_rpc_item                *item;
	bool                               end_of_bufvec;
	struct m0_rpc_packet_onwire_footer pf;
	int                                rc;
	struct m0_format_tag               packet_format_tag = {
		.ot_version = M0_RPC_PACKET_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_RPC_PACKET,
	};

	M0_ENTRY("packet: %p cursor: %p", packet, cursor);
	M0_PRE_EX(m0_rpc_packet_invariant(packet) && cursor != NULL);
	M0_PRE(!m0_rpc_packet_is_empty(packet));

	/* measured in bytes: including header, items, and footer */
	packet_format_tag.ot_size = packet->rp_size;
	m0_format_header_pack(&packet->rp_ow.poh_header, &packet_format_tag);

	rc = packet_header_encdec(&packet->rp_ow, cursor, M0_XCODE_ENCODE);
	if (rc == 0) {
		for_each_item_in_packet(item, packet) {
			uint64_t item_sm_id = m0_sm_id_get(&item->ri_sm);

			m0_rpc_item_xid_assign(item);
			M0_ADDB2_ADD(M0_AVI_RPC_ITEM_ID_ASSIGN,
				     item_sm_id,
				     (uint64_t)item->ri_type->rit_opcode,
				     item->ri_header.osr_xid,
				     item->ri_header.osr_session_id);

			rc = item_encode(item, cursor);
			if (rc != 0)
				break;
		} end_for_each_item_in_packet;
	}
	if (rc == 0) {
		m0_format_footer_generate(&pf.pof_footer, NULL, 0);
		rc = packet_footer_encdec(&pf, cursor, M0_XCODE_ENCODE);
	}

	end_of_bufvec = m0_bufvec_cursor_align(cursor, 8);
	M0_ASSERT(end_of_bufvec ||
		  M0_IS_8ALIGNED(m0_bufvec_cursor_addr(cursor)));
	return M0_RC(rc);
}

M0_INTERNAL int m0_rpc_packet_decode(struct m0_rpc_packet *p,
				     struct m0_bufvec *bufvec,
				     m0_bindex_t off, m0_bcount_t len)
{
	struct m0_bufvec_cursor cursor;
	int                     rc;

	M0_ENTRY();
	M0_PRE_EX(m0_rpc_packet_invariant(p) && bufvec != NULL && len > 0);
	M0_PRE(len <= m0_vec_count(&bufvec->ov_vec));
	M0_PRE(M0_IS_8ALIGNED(off) && M0_IS_8ALIGNED(len));
	M0_ASSERT(m0_forall(i, bufvec->ov_vec.v_nr,
			    M0_IS_8ALIGNED(bufvec->ov_vec.v_count[i])));

	m0_bufvec_cursor_init(&cursor, bufvec);
	m0_bufvec_cursor_move(&cursor, off);
	M0_ASSERT(M0_IS_8ALIGNED(m0_bufvec_cursor_addr(&cursor)));
	rc = m0_rpc_packet_decode_using_cursor(p, &cursor, len);
	M0_ASSERT(ergo(rc == 0, m0_bufvec_cursor_move(&cursor, 0) ||
		       M0_IS_8ALIGNED(m0_bufvec_cursor_addr(&cursor))));
	return M0_RC(rc);
}

static int item_decode(struct m0_bufvec_cursor  *cursor,
		       struct m0_rpc_item      **item_out)
{
	struct m0_rpc_item_type    *item_type;
	struct m0_rpc_item_header1  ioh;
	struct m0_rpc_item_footer   iof;
	int                         rc;
	struct m0_format_tag        ioh_t;

	M0_ENTRY();
	M0_PRE(cursor != NULL && item_out != NULL);

	*item_out = NULL;

	rc = m0_rpc_item_header1_encdec(&ioh, cursor, M0_XCODE_DECODE);
	if (rc != 0)
		return M0_ERR(rc);

	if (ioh.ioh_magic != M0_RPC_ITEM_MAGIC)
		return M0_ERR(-EPROTO);

	/* check version compatibility. */
	if (M0_FI_ENABLED("header_unpack"))
		return M0_ERR(-EPROTO);

	m0_format_header_unpack(&ioh_t, &ioh.ioh_header);
	if (ioh_t.ot_version != M0_RPC_ITEM_FORMAT_VERSION ||
	    ioh_t.ot_type != M0_FORMAT_TYPE_RPC_ITEM)
		return M0_ERR(-EPROTO);

	item_type = m0_rpc_item_type_lookup(ioh.ioh_opcode);
	if (item_type == NULL)
		return M0_ERR(-EPROTO);

	M0_ASSERT(item_type->rit_ops != NULL &&
		  item_type->rit_ops->rito_decode != NULL);

	if (M0_FI_ENABLED("rito_decode_nomem"))
		return M0_ERR(-ENOMEM);

	rc = item_type->rit_ops->rito_decode(item_type, item_out, cursor);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0_rpc_item_footer_encdec(&iof, cursor, M0_XCODE_DECODE);
	if (rc == 0)
		rc = m0_format_footer_verify_generic(&iof.iof_footer, NULL, 0);
	if (rc != 0)
		return M0_ERR(rc);

	(*item_out)->ri_ha_epoch = ioh.ioh_ha_epoch;
	(*item_out)->ri_flags    = ioh.ioh_flags;
	M0_LOG(M0_DEBUG, "ha_epoch: %lu", (unsigned long)ioh.ioh_ha_epoch);

	return M0_RC(0);
}

M0_INTERNAL int m0_rpc_packet_decode_using_cursor(struct m0_rpc_packet *p,
						  struct m0_bufvec_cursor
						  *cursor, m0_bcount_t len)
{
	struct m0_rpc_packet_onwire_header poh;
	struct m0_rpc_packet_onwire_footer pof;
	struct m0_rpc_item                *item;
	int                                rc;
	int                                i;
	struct m0_format_tag               rpc_t;

	M0_ENTRY();
	M0_PRE_EX(m0_rpc_packet_invariant(p) && cursor != NULL);

	rc = packet_header_encdec(&poh, cursor, M0_XCODE_DECODE);
	if (rc != 0)
		return M0_RC(rc);
	if (poh.poh_version != M0_RPC_VERSION_1 || poh.poh_nr_items == 0 ||
	    poh.poh_magic != M0_RPC_PACKET_HEAD_MAGIC)
		return M0_RC(-EPROTO);

	/* check version compatibility. */
	m0_format_header_unpack(&rpc_t, &poh.poh_header);
	if (rpc_t.ot_version != M0_RPC_PACKET_FORMAT_VERSION ||
	    rpc_t.ot_type != M0_FORMAT_TYPE_RPC_PACKET)
		return M0_RC(-EPROTO);

	/*
	 * p->rp_ow.poh_{version,magic} are initialized in m0_rpc_packet_init().
	 * p->rp_ow.poh_nr_items will be updated while decoding items in
	 * the following item_decode().
	 */
	p->rp_ow.poh_header = poh.poh_header;

	for (i = 0; i < poh.poh_nr_items; ++i) {
		rc = item_decode(cursor, &item);
		if (item == NULL) {
			/* Here fop is not allocated, no need to release it. */
			return M0_ERR(rc);
		} else if (rc != 0) {
			struct m0_fop *fop = m0_rpc_item_to_fop(item);
			uint64_t count = m0_ref_read(&fop->f_ref);

			/* After item_decode(), there's no conception of rpc or
			 * fop, and rpc_mach is set in m0_rpc_packet_add_item()
			 * later. So it's impossible to perform lock on rpc_mach
			 * inside m0_fop_put_lock() and it's replaced with
			 * explicit m0_ref_put(). */
			M0_ASSERT(count == 1);
			m0_ref_put(&fop->f_ref);

			return M0_ERR(rc);
		}
		m0_rpc_machine_lock(p->rp_rmachine);
		m0_rpc_packet_add_item(p, item);
		m0_rpc_item_put(item);
		m0_rpc_machine_unlock(p->rp_rmachine);
		item = NULL;
	}
	rc = packet_footer_encdec(&pof, cursor, M0_XCODE_DECODE);
	if (rc == 0)
		rc = m0_format_footer_verify_generic(&pof.pof_footer, NULL, 0);
	if (rc != 0)
		return M0_ERR(rc);
	m0_bufvec_cursor_align(cursor, 8);

	/* assert the decoded packet has the same number of items and size */
	M0_ASSERT(p->rp_ow.poh_nr_items == poh.poh_nr_items);
	M0_ASSERT(p->rp_size == rpc_t.ot_size);

	M0_ASSERT_EX(m0_rpc_packet_invariant(p));

	return M0_RC(0);
}

M0_INTERNAL void m0_rpc_packet_traverse_items(struct m0_rpc_packet *p,
					      item_visit_fn *visit,
					      int opaque_data)
{
	struct m0_rpc_item *item;

	M0_ENTRY("p: %p visit: %p", p, visit);
	M0_ASSERT_EX(m0_rpc_packet_invariant(p));
	M0_LOG(M0_DEBUG, "%p nr_items: %u", p,
	       (unsigned int)p->rp_ow.poh_nr_items);

	for_each_item_in_packet(item, p) {
		visit(p, item, opaque_data);
	} end_for_each_item_in_packet;

	M0_ASSERT_EX(m0_rpc_packet_invariant(p));
	M0_LEAVE();
}

/** @} rpc */
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
