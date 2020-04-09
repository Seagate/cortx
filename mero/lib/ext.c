/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 08/19/2010
 */

#include "lib/ext.h"
#include "lib/arith.h"        /* max_check, min_check */
#include "lib/misc.h"         /* M0_EXPORTED */

/**
   @addtogroup ext
   @{
 */

M0_INTERNAL void m0_ext_init(struct m0_ext *ext)
{
	m0_format_header_pack(&ext->e_header, &(struct m0_format_tag){
		.ot_version = M0_EXT_FORMAT_VERSION,
		.ot_type    = M0_FORMAT_TYPE_EXT,
		.ot_footer_offset = offsetof(struct m0_ext, e_footer)
	});
	m0_format_footer_update(ext);
}

M0_INTERNAL m0_bcount_t m0_ext_length(const struct m0_ext *ext)
{
	return ext->e_end - ext->e_start;
}
M0_EXPORTED(m0_ext_length);

M0_INTERNAL bool m0_ext_is_in(const struct m0_ext *ext, m0_bindex_t index)
{
	return ext->e_start <= index && index < ext->e_end;
}

M0_INTERNAL bool m0_ext_are_overlapping(const struct m0_ext *e0,
					const struct m0_ext *e1)
{
	struct m0_ext i;

	m0_ext_intersection(e0, e1, &i);
	return m0_ext_is_valid(&i);
}

M0_INTERNAL bool m0_ext_is_partof(const struct m0_ext *super,
				  const struct m0_ext *sub)
{
	return
		m0_ext_is_in(super, sub->e_start) &&
		sub->e_end <= super->e_end;
}

M0_INTERNAL bool m0_ext_equal(const struct m0_ext *a, const struct m0_ext *b)
{
	return a->e_start == b->e_start && a->e_end == b->e_end;
}


M0_INTERNAL bool m0_ext_is_empty(const struct m0_ext *ext)
{
	return ext->e_end <= ext->e_start;
}

M0_INTERNAL void m0_ext_intersection(const struct m0_ext *e0,
				     const struct m0_ext *e1,
				     struct m0_ext *result)
{
	result->e_start = max_check(e0->e_start, e1->e_start);
	result->e_end   = min_check(e0->e_end,   e1->e_end);
}
M0_EXPORTED(m0_ext_intersection);

M0_INTERNAL bool m0_ext_is_valid(const struct m0_ext *ext)
{
        return ext->e_end > ext->e_start;
}
M0_EXPORTED(m0_ext_is_valid);

/** @} end of ext group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
