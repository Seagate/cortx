/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov  <nikita_danilov@seagate.com>
 * AuthorS:	    Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                  James  Morse    <james.s.morse@seagate.com>
 *                  Sining Wu       <sining.wu@seagate.com>
 * Revision:        Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 14-Oct-2013
 */

#include "clovis/clovis.h"
#include "clovis/pg.h"
#include "clovis/io.h"
#include "lib/trace.h"           /* M0_LOG */
#include "lib/finject.h"         /* M0_FI_ENABLE */

M0_INTERNAL bool addr_is_network_aligned(void *addr)
{
	return ((uint64_t)addr & CLOVIS_NETBUF_MASK) == 0;
}

M0_INTERNAL uint64_t obj_buffer_size(const struct m0_clovis_obj *obj)
{
	M0_PRE(obj != NULL);

	return 1ULL<<obj->ob_attr.oa_bshift;
}

M0_INTERNAL uint64_t m0_clovis__page_size(const struct m0_clovis_op_io *ioo)
{
	M0_PRE(ioo != NULL);
	M0_PRE(ioo->ioo_obj != NULL);

	return obj_buffer_size(ioo->ioo_obj);
}

M0_INTERNAL uint64_t page_nr(m0_bcount_t size, struct m0_clovis_obj *obj)
{
	M0_PRE(obj != NULL);
	M0_PRE(obj->ob_attr.oa_bshift >= CLOVIS_MIN_BUF_SHIFT);

	return size >> obj->ob_attr.oa_bshift;
}

M0_INTERNAL uint32_t layout_n(struct m0_pdclust_layout *play)
{
	M0_PRE(play != NULL);

	return play->pl_attr.pa_N;
}

M0_INTERNAL uint32_t layout_k(struct m0_pdclust_layout *play)
{
	M0_PRE(play != NULL);

	return play->pl_attr.pa_K;
}

/** TODO: this code is the same as page_nr -> combine them? */
M0_INTERNAL uint64_t page_id(m0_bindex_t offset, struct m0_clovis_obj *obj)
{
	M0_PRE(obj != NULL);
	M0_PRE(obj->ob_attr.oa_bshift >= CLOVIS_MIN_BUF_SHIFT);

	return offset >> obj->ob_attr.oa_bshift;
}

M0_INTERNAL uint64_t layout_unit_size(struct m0_pdclust_layout *play)
{
	M0_PRE(play != NULL);

	return play->pl_attr.pa_unit_size;
}

M0_INTERNAL uint32_t data_row_nr(struct m0_pdclust_layout *play,
				   struct m0_clovis_obj *obj)
{
	M0_PRE(play != NULL);
	M0_PRE(obj != NULL);

	return page_nr(layout_unit_size(play), obj);
}

M0_INTERNAL uint32_t data_col_nr(struct m0_pdclust_layout *play)
{
	M0_PRE(play != NULL);

	return layout_n(play);
}

M0_INTERNAL uint32_t parity_col_nr(struct m0_pdclust_layout *play)
{
	M0_PRE(play != NULL);

	return layout_k(play);
}

/** TODO: This is clearly useless */
M0_INTERNAL uint32_t parity_row_nr(struct m0_pdclust_layout *play,
				     struct m0_clovis_obj *obj)
{
	M0_PRE(play != NULL);
	M0_PRE(obj != NULL);

	return data_row_nr(play, obj);
}

M0_INTERNAL uint64_t data_size(struct m0_pdclust_layout *play)
{
	M0_PRE(play != NULL);

	return layout_n(play) * layout_unit_size(play);
}

M0_INTERNAL struct m0_pdclust_instance *
pdlayout_instance(struct m0_layout_instance *li)
{
	M0_PRE(li != NULL);

	return m0_layout_instance_to_pdi(li);
}

M0_INTERNAL struct m0_pdclust_layout *
pdlayout_get(const struct m0_clovis_op_io *ioo)
{
	return m0_layout_to_pdl(ioo->ioo_oo.oo_layout_instance->li_l);
}

M0_INTERNAL struct m0_layout_instance *
layout_instance(const struct m0_clovis_op_io *ioo)
{
	M0_PRE(ioo != NULL);

	return ioo->ioo_oo.oo_layout_instance;
}

M0_INTERNAL struct m0_parity_math *parity_math(struct m0_clovis_op_io *ioo)
{
	return &pdlayout_instance(layout_instance(ioo))->pi_math;
}

M0_INTERNAL uint64_t target_offset(uint64_t                 frame,
				     struct m0_pdclust_layout *play,
				     m0_bindex_t               gob_offset)
{
	M0_PRE(play != NULL);

	return frame * layout_unit_size(play) +
		(gob_offset % layout_unit_size(play));
}

M0_INTERNAL uint64_t group_id(m0_bindex_t index, m0_bcount_t dtsize)
{
	/* XXX add any CLOVIS_PRE()? => update UTs */
	return index / dtsize;
}

M0_INTERNAL m0_bcount_t seg_endpos(const struct m0_indexvec *ivec, uint32_t i)
{
	M0_PRE(ivec != NULL);

	return ivec->iv_index[i] + ivec->iv_vec.v_count[i];
}

M0_INTERNAL uint64_t indexvec_page_nr(const struct m0_vec *vec,
				      struct m0_clovis_obj *obj)
{
	M0_PRE(vec != NULL);
	M0_PRE(obj != NULL);

	return page_nr(m0_vec_count(vec), obj);
}

M0_INTERNAL uint64_t iomap_page_nr(const struct pargrp_iomap *map)
{
	M0_PRE(map != NULL);

	return indexvec_page_nr(&map->pi_ivec.iv_vec, map->pi_ioo->ioo_obj);
}

M0_INTERNAL uint64_t parity_units_page_nr(struct m0_pdclust_layout *play,
					    struct m0_clovis_obj *obj)
{
	M0_PRE(play != NULL);
	M0_PRE(obj != NULL);

	return page_nr(layout_unit_size(play), obj) * layout_k(play);
}

#if !defined(round_down)
M0_INTERNAL uint64_t round_down(uint64_t val, uint64_t size)
{
	M0_PRE(m0_is_po2(size));

	/*
	 * Returns current value if it is already a multiple of size,
	 * else m0_round_down() is invoked.
	 */
	return (val & (size - 1)) == 0 ?
		val : m0_round_down(val, size);
}
#endif

#if !defined(round_up)
M0_INTERNAL uint64_t round_up(uint64_t val, uint64_t size)
{
	M0_PRE(m0_is_po2(size));

	/*
	 * Returns current value if it is already a multiple of size,
	 * else m0_round_up() is invoked.
	 */
	return (val & (size - 1)) == 0 ?
		val : m0_round_up(val, size);
}
#endif

M0_INTERNAL uint32_t io_desc_size(struct m0_net_domain *ndom)
{
	M0_PRE(ndom != NULL);

	return
		/* size of variables ci_nr and nbd_len */
		M0_MEMBER_SIZE(struct m0_io_indexvec, ci_nr) +
		M0_MEMBER_SIZE(struct m0_net_buf_desc, nbd_len) +

		/* size of nbd_data */
		m0_net_domain_get_max_buffer_desc_size(ndom);
}

M0_INTERNAL uint32_t io_seg_size(void)
{
	return sizeof(struct m0_ioseg);
}

/** TODO: obj can be retrieved from map->pi_ioo */
M0_INTERNAL void page_pos_get(struct pargrp_iomap  *map,
			      m0_bindex_t           index,
			      m0_bindex_t           grp_size,
			      uint32_t             *row,
			      uint32_t             *col)
{
	uint64_t                  pg_id;
	struct m0_clovis_obj     *obj;
	struct m0_pdclust_layout *play;

	M0_PRE(map != NULL);
	M0_PRE(row != NULL);
	M0_PRE(col != NULL);

	obj = map->pi_ioo->ioo_obj;
	play = pdlayout_get(map->pi_ioo);

	pg_id = page_id(index - grp_size, obj);
	*row  = pg_id % data_row_nr(play, obj);
	*col = play->pl_attr.pa_K == 0 ? 0 : pg_id / data_row_nr(play, obj);
}

M0_INTERNAL m0_bindex_t data_page_offset_get(struct pargrp_iomap *map,
					       uint32_t           row,
					       uint32_t           col)
{
	struct m0_pdclust_layout *play;
	struct m0_clovis_op_io   *ioo;
	m0_bindex_t               out;

	M0_PRE(map != NULL);

	ioo = map->pi_ioo;
	play = pdlayout_get(ioo);

	M0_PRE(row < data_row_nr(play, ioo->ioo_obj));
	M0_PRE(col < data_col_nr(play));

	out = data_size(play) * map->pi_grpid +
	      col * layout_unit_size(play) + row * m0_clovis__page_size(ioo);

	return out;
}

M0_INTERNAL uint32_t ioreq_sm_state(const struct m0_clovis_op_io *ioo)
{
	M0_PRE(ioo != NULL);

	return ioo->ioo_sm.sm_state;
}

M0_INTERNAL uint64_t tolerance_of_level(struct m0_clovis_op_io *ioo, uint64_t lv)
{
	struct m0_pdclust_instance *play_instance;
	struct m0_pool_version     *pver;

	M0_PRE(lv < M0_CONF_PVER_HEIGHT);

	if (M0_FI_ENABLED("fake_tolerance_of_level"))
		return 0;

	play_instance = pdlayout_instance(layout_instance(ioo));
	pver = play_instance->pi_base.li_l->l_pver;
	return pver->pv_fd_tol_vec[lv];
}

M0_INTERNAL bool m0_clovis__is_update_op(struct m0_clovis_op *op)
{
	return M0_IN(op->op_code, (M0_CLOVIS_OC_WRITE,
				   M0_CLOVIS_OC_FREE));
}

M0_INTERNAL bool m0_clovis__is_read_op(struct m0_clovis_op *op)
{
	return op->op_code == M0_CLOVIS_OC_READ;
}

M0_INTERNAL struct m0_clovis_obj_attr *
m0_clovis_io_attr(struct m0_clovis_op_io *ioo)
{
	return &ioo->ioo_obj->ob_attr;
}

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
