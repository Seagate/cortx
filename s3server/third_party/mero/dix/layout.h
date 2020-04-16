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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 23-Jun-2016
 */

#pragma once

#ifndef __MERO_DIX_LAYOUT_H__
#define __MERO_DIX_LAYOUT_H__

#include "lib/types.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "dix/imask.h"
#include "dix/imask_xc.h"
#include "lib/buf.h"
#include "lib/types_xc.h"

/**
 * @addtogroup dix
 *
 * @{
 *
 * Distributed index layout is based on parity de-clustering layout and
 * determines targets (pool disks) for index records.
 *
 * Layouts of indices are stored centralised in 'layout' meta-index. Layout can
 * be stored there in two forms: layout id and layout descriptor. Layout
 * descriptor shall be known in order to instantiate layout instance
 * (m0_dix_linst) usable for target disks calculation. Therefore, if layout id
 * is stored in 'layout' meta-index then it shall be resolved to full-fledged
 * layout descriptor. The mapping between layout id and corresponding layout
 * descriptor is stored in 'layout-descr' meta-index.
 *
 * For more information about targets calculation for index records please refer
 * to distributed indexing HLD.
 */

/* Import */
struct m0_pdclust_layout;
struct m0_pdclust_instance;
struct m0_pool_version;
struct m0_layout_domain;

enum dix_layout_type {
	DIX_LTYPE_UNKNOWN,
	DIX_LTYPE_ID,
	DIX_LTYPE_DESCR,
	DIX_LTYPE_COMPOSITE_DESCR,
	DIX_LTYPE_CAPTURE_DESCR,
};

enum m0_dix_hash_fnc_type {
	HASH_FNC_NONE,
	HASH_FNC_FNV1,
	HASH_FNC_CITY
};

struct m0_dix_ldesc {
	uint32_t            ld_hash_fnc;
	struct m0_fid       ld_pver;
	struct m0_dix_imask ld_imask;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dix_capture_ldesc {
	struct m0_uint128 ca_orig_id;
	struct m0_fid     ca_pver;
	uint64_t          ca_lid;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dix_composite_layer {
	struct m0_uint128 cr_subobj;
	uint64_t          cr_lid;
	int               cr_priority;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

struct m0_dix_composite_ldesc {
	int                            cld_nr_layers;
	struct m0_dix_composite_layer *cld_layers;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

struct m0_dix_layout {
	uint32_t dl_type;
	union {
		uint64_t                      dl_id
					M0_XCA_TAG("DIX_LTYPE_ID");
		struct m0_dix_ldesc           dl_desc
					M0_XCA_TAG("DIX_LTYPE_DESCR");
		struct m0_dix_capture_ldesc   dl_cap_desc
					M0_XCA_TAG("DIX_LTYPE_CAPTURE_DESCR");
		struct m0_dix_composite_ldesc dl_comp_desc
					M0_XCA_TAG("DIX_LTYPE_COMPOSITE_DESCR");
	} u;
} M0_XCA_UNION M0_XCA_DOMAIN(rpc);

struct m0_dix_linst {
	struct m0_dix_ldesc        *li_ldescr;
	struct m0_pdclust_layout   *li_pl;
	struct m0_pdclust_instance *li_pi;
};

/**
 * Iterator over targets of index record parity group units.
 *
 * The order of iteration is:
 * Tn, Tp1 ... Tpk, Ts1, ..., Tsk,
 * where Tn - target for data unit. There is always one data unit;
 *       Tp1 ... Tpk - targets for parity units;
 *       Ts1 ... Tsk - targets for spare units;
 *       'k' is determined by pool version 'K' attribute.
 * Iterator constructs distributed index layout internally and uses it to
 * calculate successive target on every iteration.
 *
 * Target in this case is a device index in a pool version.
 */
struct m0_dix_layout_iter {
	/** Layout instance. */
	struct m0_dix_linst dit_linst;

	/** Width of a parity group. */
	uint32_t            dit_W;

	/** Current position. */
	uint64_t            dit_unit;

	/**
	 * Key of the record that should be distributed after application of the
	 * identity mask.
	 */
	struct m0_buf       dit_key;
};

/**
 * Calculates target for specified 'unit' in parity group of the record with
 * specified 'key'. Calculated target is stored in 'out_id'.
 */
M0_INTERNAL void m0_dix_target(struct m0_dix_linst *inst,
			       uint64_t             unit,
			       struct m0_buf       *key,
			       uint64_t            *out_id);

/**
 * Returns total number of devices (targets) accounted by layout instance.
 */
M0_INTERNAL uint32_t m0_dix_devices_nr(struct m0_dix_linst *linst);

/**
 * Returns pool device structure by target (e.g. calculated by m0_dix_target()).
 */
M0_INTERNAL struct m0_pooldev *m0_dix_tgt2sdev(struct m0_dix_linst *linst,
					       uint64_t             tgt);

/**
 * Builds DIX layout instance.
 *
 * Internal function, user should use m0_dix_layout_iter_init() instead.
 */
M0_INTERNAL int m0_dix_layout_init(struct m0_dix_linst     *dli,
				   struct m0_layout_domain *domain,
				   const struct m0_fid     *fid,
				   uint64_t                 layout_id,
				   struct m0_pool_version  *pver,
				   struct m0_dix_ldesc     *dld);

/**
 * Finalises DIX layout instance.
 */
M0_INTERNAL void m0_dix_layout_fini(struct m0_dix_linst *dli);

/**
 * Initialises layout descriptor.
 */
M0_INTERNAL int m0_dix_ldesc_init(struct m0_dix_ldesc       *ld,
				  struct m0_ext             *range,
				  m0_bcount_t                range_nr,
				  enum m0_dix_hash_fnc_type  htype,
				  struct m0_fid             *pver);

/**
 * Copies layout descriptor.
 *
 * Copied layout descriptor 'dst' shall be finalised by user afterwards.
 */
M0_INTERNAL int m0_dix_ldesc_copy(struct m0_dix_ldesc       *dst,
				  const struct m0_dix_ldesc *src);

/**
 * Finalises layout descriptor.
 */
M0_INTERNAL void m0_dix_ldesc_fini(struct m0_dix_ldesc *ld);

/**
 * Initialises DIX layout iterator.
 *
 * After initialisation iterator stay on first unit (data unit) in parity group.
 *
 * @param iter   Layout iterator.
 * @param index  Fid of distributed index having layout 'ldesc'.
 * @param ldom   Layout domain where layout instance is created.
 * @param pver   Pool version where distributed index is stored.
 * @param ldesc  Distributed index layout descriptor.
 * @param key    Key of the record for which targets are calculated.
 */
M0_INTERNAL
int m0_dix_layout_iter_init(struct m0_dix_layout_iter *iter,
			    const struct m0_fid       *index,
			    struct m0_layout_domain   *ldom,
			    struct m0_pool_version    *pver,
			    struct m0_dix_ldesc       *ldesc,
			    struct m0_buf             *key);

/**
 * Calculates target for the next unit in record parity group.
 *
 * User is responsible to not overcome parity group boundary. Number of units in
 * parity group can be obtained via m0_dix_liter_W().
 */
M0_INTERNAL void m0_dix_layout_iter_next(struct m0_dix_layout_iter *iter,
					 uint64_t                  *tgt);

/**
 * Moves iterator current position to unit with number 'unit_nr'.
 *
 * Next m0_dix_layout_iter_next() invocation will return target for 'unit_nr'
 * unit.
 */
M0_INTERNAL void m0_dix_layout_iter_goto(struct m0_dix_layout_iter *iter,
					 uint64_t                   unit_nr);
/**
 * Resets iterator current position to the beginning.
 */
M0_INTERNAL void m0_dix_layout_iter_reset(struct m0_dix_layout_iter *iter);

/**
 * Calculates target for specified 'unit' in a parity group.
 *
 * It doesn't affect iterator current position.
 */
M0_INTERNAL void m0_dix_layout_iter_get_at(struct m0_dix_layout_iter *iter,
					   uint64_t                   unit,
					   uint64_t                  *tgt);

/**
 * Returns number of data units in a parity group.
 *
 * Shall be always 1 in current implementation.
 */
M0_INTERNAL uint32_t m0_dix_liter_N(struct m0_dix_layout_iter *iter);

/**
 * Returns total number of targets.
 */
M0_INTERNAL uint32_t m0_dix_liter_P(struct m0_dix_layout_iter *iter);

/**
 * Returns number of parity units in a parity group.
 *
 * Number of parity units always equal to number of spare units in current
 * implementation.
 */
M0_INTERNAL uint32_t m0_dix_liter_K(struct m0_dix_layout_iter *iter);

/**
 * Returns total number of units (datai + parity + spare) in a parity group.
 */
M0_INTERNAL uint32_t m0_dix_liter_W(struct m0_dix_layout_iter *iter);

/**
 * Returns number of first spare unit in a parity group.
 */
M0_INTERNAL uint32_t m0_dix_liter_spare_offset(struct m0_dix_layout_iter *iter);

/**
 * Classify specified 'unit' to one of the classes listed in
 * m0_pdclust_unit_type.
 */
M0_INTERNAL
uint32_t m0_dix_liter_unit_classify(struct m0_dix_layout_iter *iter,
				    uint64_t                   unit);

/**
 * Finalises DIX layout iterator.
 */
M0_INTERNAL void m0_dix_layout_iter_fini(struct m0_dix_layout_iter *iter);

/**
 * Checks whether two distributed index layouts are equal.
 */
M0_INTERNAL bool m0_dix_layout_eq(const struct m0_dix_layout *layout1,
				  const struct m0_dix_layout *layout2);

/** @} end of dix group */
#endif /* __MERO_DIX_LAYOUT_H__ */

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
