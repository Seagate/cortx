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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 02/18/2011
 */

#pragma once

#ifndef __MERO_LIB_LINUX_KERNEL_THREAD_H__
#define __MERO_LIB_LINUX_KERNEL_THREAD_H__

#include <linux/kthread.h>
#include <linux/hardirq.h>

/**
   @addtogroup thread Thread

   <b>Linux kernel m0_thread implementation</b>

   Kernel space implementation is based <linux/kthread.h>

   @see m0_thread

   @{
 */

enum { M0_THREAD_NAME_LEN = TASK_COMM_LEN };

struct m0_thread_handle {
	struct task_struct *h_tsk;
	unsigned long       h_pid;
};

/** Kernel thread-local storage. */
struct m0_thread_arch_tls {
	void *tat_prev;
};

struct m0_thread;
M0_INTERNAL void m0_thread_enter(struct m0_thread *thread, bool full);
M0_INTERNAL void m0_thread_leave(void);
M0_INTERNAL void m0_thread__cleanup(struct m0_thread *bye);

#define M0_THREAD_ENTER						\
	struct m0_thread __th						\
		__attribute__((cleanup(m0_thread__cleanup))) = { 0, };	\
	m0_thread_enter(&__th, true)

M0_INTERNAL struct m0_thread_tls *m0_thread_tls_pop(void);
M0_INTERNAL void m0_thread_tls_back(struct m0_thread_tls *tls);

/** @} end of thread group */
#endif /* __MERO_LIB_LINUX_KERNEL_THREAD_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
