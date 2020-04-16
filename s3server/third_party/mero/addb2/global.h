/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 21-Mar-2015
 */

#pragma once

#ifndef __MERO_ADDB2_GLOBAL_H__
#define __MERO_ADDB2_GLOBAL_H__

/**
 * @defgroup addb2
 *
 * @{
 */

struct m0_addb2_sys;
struct m0_addb2_counter;

M0_INTERNAL int  m0_addb2_global_init(void);
M0_INTERNAL void m0_addb2_global_fini(void);
M0_INTERNAL struct m0_addb2_sys *m0_addb2_global_get(void);
M0_INTERNAL void m0_addb2_global_thread_enter(void);
M0_INTERNAL void m0_addb2_global_thread_leave(void);

/** @} end of addb2 group */
#endif /* __MERO_ADDB2_GLOBAL_H__ */

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
