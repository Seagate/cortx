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
 * Original author: Manish Honap <manish_honap@xyratex.com>
 * Original creation date: 9-Jan-2013
 */

#include "lib/lockers.h" /* m0_lockers */
#include "lib/types.h"   /* uint32_t */
#include "lib/string.h"  /* memset */
#include "lib/assert.h"  /* M0_PRE */
#include "lib/misc.h"    /* M0_SET0 */

/**
 * @addtogroup lockers
 *
 * @{
 */

static bool key_is_valid(const struct m0_lockers_type *lt, int key);

M0_INTERNAL void m0_lockers_init(const struct m0_lockers_type *lt,
				 struct m0_lockers            *lockers)
{
	memset(lockers->loc_slots, 0,
	       lt->lot_max * sizeof lockers->loc_slots[0]);
}

M0_INTERNAL int m0_lockers_allot(struct m0_lockers_type *lt)
{
	int i;

	for (i = 0; i < lt->lot_max; ++i) {
		if (!lt->lot_inuse[i]) {
			lt->lot_inuse[i] = true;
			return i;
		}
	}
	M0_IMPOSSIBLE("Lockers table overflow.");
}

M0_INTERNAL void m0_lockers_free(struct m0_lockers_type *lt, int key)
{
	M0_PRE(key_is_valid(lt, key));
	lt->lot_inuse[key] = false;
}

M0_INTERNAL void m0_lockers_set(const struct m0_lockers_type *lt,
				struct m0_lockers            *lockers,
				uint32_t                      key,
				void                         *data)
{
	M0_PRE(key_is_valid(lt, key));
	lockers->loc_slots[key] = data;
}

M0_INTERNAL void *m0_lockers_get(const struct m0_lockers_type *lt,
				 const struct m0_lockers      *lockers,
				 uint32_t                      key)
{
	M0_PRE(key_is_valid(lt, key));
	return lockers->loc_slots[key];
}

M0_INTERNAL void m0_lockers_clear(const struct m0_lockers_type *lt,
				  struct m0_lockers            *lockers,
				  uint32_t                      key)
{
	M0_PRE(key_is_valid(lt, key));
	lockers->loc_slots[key] = NULL;
}

M0_INTERNAL bool m0_lockers_is_empty(const struct m0_lockers_type *lt,
				     const struct m0_lockers      *lockers,
				     uint32_t                      key)
{
	M0_PRE(key_is_valid(lt, key));
	return lockers->loc_slots[key] == NULL;
}

M0_INTERNAL void m0_lockers_fini(struct m0_lockers_type *lt,
				 struct m0_lockers      *lockers)
{
}

static bool key_is_valid(const struct m0_lockers_type *lt, int key)
{
	return key < lt->lot_max && lt->lot_inuse[key];
}

/** @} end of lockers group */


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
