/* -*- C -*- */
/*
 * COPYRIGHT 2018 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 16-Jan-2019
 */

#pragma once

#ifndef __MERO_MERO_SETUP_DIX_H__
#define __MERO_MERO_SETUP_DIX_H__

/**
 * @addtogroup m0d
 *
 * @{
 */

/**
 * Initialises DIX meta-indices locally.
 *
 * Creates DIX meta-indices involving only local CAS. Returns immediately if
 * there is no CAS service configured for the current process.
 *
 * After running m0_cs_dix_setup() on all m0d's, the DIX state is the same as
 * after running m0dixinit utility. In opposite to m0dixinit, this function
 * can be used to re-initialise a single m0d.
 *
 * XXX Current solution is temporary and contains copy-paste from DIX code.
 * Proper solution must use DIX interface.
 */
M0_INTERNAL int m0_cs_dix_setup(struct m0_mero *cctx);

/** @} end of m0d group */
#endif /* __MERO_MERO_SETUP_DIX_H__ */

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
