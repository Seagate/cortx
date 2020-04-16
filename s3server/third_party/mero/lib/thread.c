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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>,
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 05/01/2010
 */

#ifndef __KERNEL__
#  include <stdarg.h>
#  include <stdio.h>          /* vsnprintf */
#endif

#include "lib/thread.h"
#include "lib/misc.h"         /* M0_SET0 */
#include "module/instance.h"  /* m0_get */
#include "addb2/global.h"

/**
   @addtogroup thread Thread

   Common m0_thread implementation.

   @{
 */

int m0_thread_init(struct m0_thread *q, int (*init)(void *),
		   void (*func)(void *), void *arg, const char *namefmt, ...)
{
	int     result;
	va_list varargs;

	M0_PRE(M0_IS0(q));
	M0_PRE(q->t_func == NULL);
	M0_PRE(q->t_state == TS_PARKED);

	va_start(varargs, namefmt);
	result = vsnprintf(q->t_namebuf, sizeof q->t_namebuf, namefmt, varargs);
	va_end(varargs);
	M0_ASSERT_INFO(result < sizeof q->t_namebuf,
		       "namebuf truncated to \"%s\"", q->t_namebuf);

	q->t_state = TS_RUNNING;
	q->t_init  = init;
	q->t_func  = func;
	q->t_arg   = arg;

	result = m0_semaphore_init(&q->t_wait, 0);
	if (result != 0)
		return result;

	/* Let `q' inherit the pointer from current thread's TLS.
	 * m0_set() is of no use here, since it has no impact on the TLS
	 * of `q'. */
	q->t_tls.tls_m0_instance = m0_get();
	q->t_tls.tls_self = q;

	result = m0_thread_init_impl(q, q->t_namebuf);
	if (result != 0)
		goto err;

	if (q->t_init != NULL) {
		m0_semaphore_down(&q->t_wait);
		result = q->t_initrc;
		if (result != 0)
			m0_thread_join(q);
	}

	if (result == 0)
		return 0;
err:
	m0_semaphore_fini(&q->t_wait);
	q->t_state = TS_PARKED;
	return result;
}
M0_EXPORTED(m0_thread_init);

void m0_thread_fini(struct m0_thread *q)
{
	M0_PRE(q->t_state == TS_PARKED);

	m0_semaphore_fini(&q->t_wait);
	M0_SET0(q);
}
M0_EXPORTED(m0_thread_fini);

M0_INTERNAL void *m0_thread_trampoline(void *arg)
{
	struct m0_thread *t = arg;

	M0_PRE(t->t_state == TS_RUNNING);
	M0_PRE(t->t_initrc == 0);
	M0_PRE(t->t_tls.tls_m0_instance != NULL);
	M0_PRE(t->t_tls.tls_self == t);

	m0_set(t->t_tls.tls_m0_instance);
	m0_addb2_global_thread_enter();
	if (t->t_init != NULL) {
		t->t_initrc = t->t_init(t->t_arg);
		m0_semaphore_up(&t->t_wait);
	}
	if (t->t_initrc == 0)
		t->t_func(t->t_arg);
	m0_addb2_global_thread_leave();
	return NULL;
}

M0_INTERNAL struct m0_thread *m0_thread_self(void)
{
	return m0_thread_tls()->tls_self;
}

M0_INTERNAL int m0_thread_adopt(struct m0_thread *thread, struct m0 *instance)
{
	M0_PRE(M0_IS0(thread));
	M0_PRE(m0_thread_tls() == NULL);

	return m0_thread_arch_adopt(thread, instance, true);
}

M0_INTERNAL void m0_thread_shun(void)
{
	m0_thread_arch_shun();
}

/** @} end of thread group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
