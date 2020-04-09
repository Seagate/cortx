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

/**
 * @addtogroup addb2
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_ADDB

#include "lib/trace.h"
#include "lib/uuid.h"                 /* m0_node_uuid */
#include "lib/errno.h"                /* ENOMEM */
#include "lib/thread.h"               /* m0_thread_tls */
#include "lib/memory.h"
#include "module/instance.h"
#include "addb2/sys.h"
#include "addb2/identifier.h"         /* M0_AVI_THREAD */
#include "addb2/addb2.h"

#define SYS() (m0_addb2_module_get()->am_sys)

M0_INTERNAL void m0_addb2_global_thread_enter(void)
{
	struct m0_addb2_sys  *sys = SYS();
	struct m0_thread_tls *tls = m0_thread_tls();

	M0_PRE(tls->tls_addb2_mach == NULL);
	if (sys != NULL) {
		tls->tls_addb2_mach = m0_addb2_sys_get(sys);
		m0_addb2_push(M0_AVI_NODE, M0_ADDB2_OBJ(&m0_node_uuid));
		M0_ADDB2_PUSH(M0_AVI_PID, m0_pid());
		m0_addb2_push(M0_AVI_THREAD, M0_ADDB2_OBJ(&tls->tls_self->t_h));
		m0_addb2_clock_add(&tls->tls_clock, M0_AVI_CLOCK, -1);
	}
}

M0_INTERNAL void m0_addb2_global_thread_leave(void)
{
	struct m0_addb2_sys  *sys  = SYS();
	struct m0_thread_tls *tls  = m0_thread_tls();
	struct m0_addb2_mach *mach = tls->tls_addb2_mach;

	if (mach != NULL) {
		M0_ASSERT(sys != NULL);
		m0_addb2_pop(M0_AVI_THREAD);
		m0_addb2_pop(M0_AVI_PID);
		m0_addb2_pop(M0_AVI_NODE);
		M0_SET0(&tls->tls_clock);
		tls->tls_addb2_mach = NULL;
		m0_addb2_sys_put(sys, mach);
	}
}

M0_INTERNAL int m0_addb2_global_init(void)
{
	int result;

	result = m0_addb2_sys_init((struct m0_addb2_sys **)&SYS(),
				   &(struct m0_addb2_config) {
					       .co_queue_max = 1024 * 1024,
					       .co_pool_min  = 1024,
					       .co_pool_max  = 1024 * 1024
				   });
	if (result == 0)
		m0_addb2_global_thread_enter();
	return result;
}

M0_INTERNAL void m0_addb2_global_fini(void)
{
	struct m0_addb2_sys *sys = SYS();

	m0_addb2_global_thread_leave();
	if (sys != NULL)
		m0_addb2_sys_fini(sys);
}

M0_INTERNAL struct m0_addb2_sys *m0_addb2_global_get(void)
{
	return SYS();
}

#undef SYS
#undef M0_TRACE_SUBSYSTEM

/** @} end of addb2 group */

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
