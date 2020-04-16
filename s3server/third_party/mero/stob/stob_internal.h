/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro_podgornyi@xyratex.com>
 * Original creation date: 26-Feb-2014
 */

#pragma once
#ifndef __MERO_STOB_STOB_INTERNAL_H__
#define __MERO_STOB_STOB_INTERNAL_H__

/**
 * @defgroup stob Storage object
 *
 * @{
 */

#include "lib/types.h"	/* uint64_t */

enum m0_stob_state;
struct m0_stob;
struct m0_stob_domain;

M0_INTERNAL void m0_stob__id_set(struct m0_stob *stob,
				 const struct m0_fid *stob_fid);
M0_INTERNAL void m0_stob__state_set(struct m0_stob *stob,
				    enum m0_stob_state state);

M0_INTERNAL void m0_stob__cache_evict(struct m0_stob *stob);

/** @} end of stob group */
#endif /* __MERO_STOB_STOB_INTERNAL_H__ */

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
