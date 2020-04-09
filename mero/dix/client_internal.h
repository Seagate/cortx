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
 * Original creation date: 8-Jul-2016
 */

#pragma once

#ifndef __MERO_DIX_CLIENT_INTERNAL_H__
#define __MERO_DIX_CLIENT_INTERNAL_H__

/**
 * @defgroup dix
 *
 * @{
 */

/* Import */
struct m0_dix_cli;
struct m0_dix;

/**
 * Fills 'out' structure with root index fid and layout descriptor.
 * User is responsible to finalise 'out' after usage.
 */
M0_INTERNAL int m0_dix__root_set(const struct m0_dix_cli *cli,
				 struct m0_dix           *out);

/**
 * Fills 'out' structure with "layout" index fid and layout descriptor.
 * User is responsible to finalise 'out' after usage.
 */
M0_INTERNAL int m0_dix__layout_set(const struct m0_dix_cli *cli,
				   struct m0_dix           *out);

/**
 * Fills 'out' structure with "layout-descr" index fid and layout descriptor.
 * User is responsible to finalise 'out' after usage.
 */
M0_INTERNAL int m0_dix__ldescr_set(const struct m0_dix_cli *cli,
				   struct m0_dix           *out);

/**
 * Finds pool version structure by pool version fid specified in index layout
 * descriptor.
 *
 * @pre dix->dd_layout.dl_type == DIX_LTYPE_DESCR
 */
M0_INTERNAL struct m0_pool_version *m0_dix_pver(const struct m0_dix_cli *cli,
						const struct m0_dix     *dix);

/** @} end of dix group */
#endif /* __MERO_DIX_CLIENT_INTERNAL_H__ */

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
