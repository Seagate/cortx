/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 24-Oct-2014
 */

/*
 * Start and stop worker threads
 */
#include "clovis/st/clovis_st_misc.h"
#include "clovis/st/clovis_st.h"

#ifdef __KERNEL__

# include <linux/types.h>

enum { CLOVIS_ST_MAX_THREAD_NAME_LEN = 64 };

typedef struct task_struct clovis_st_thread_t;
static clovis_st_thread_t **workers = NULL;

static int thread_init(clovis_st_thread_t **ret_th,
		       int (*func)(void *), void *args,
		       const char *name_fmt, ...)
{
	int                 rc;
	char                name_str[CLOVIS_ST_MAX_THREAD_NAME_LEN];
	va_list             vl;
	clovis_st_thread_t *th;

	/* construct the name of this thread*/
	va_start(vl, name_fmt);
	rc = vsnprintf(name_str, sizeof(name_str), name_fmt, vl);
	va_end(vl);

	/* create and start a thread */
	th = kthread_create(func, args, "%s", name_str);
	if (th == NULL) {
		rc = PTR_ERR(th);
	} else {
		rc = 0;
		wake_up_process(th);
		*ret_th = th;
	}

	return rc;
}

static int thread_join(clovis_st_thread_t *th)
{
	return kthread_stop(th);
}

static void thread_fini(clovis_st_thread_t *th)
{
	return;
}

void thread_exit(void)
{
	/*
	 * A thread doesn't exit until thread_join is called
	 * here, kthread_should_stop is called to check if this condition
	 * is met
	 */

	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);
}

#else  /* user space*/

#include <stdint.h>
#include <errno.h>
#include <pthread.h>

typedef pthread_t clovis_st_thread_t;
static clovis_st_thread_t **workers = NULL;

static int thread_init(clovis_st_thread_t **ret_th,
		       void* (*func)(void *), void *args, ...)
{
	int                 rc;
	clovis_st_thread_t *th;

	th = (clovis_st_thread_t *) mem_alloc(sizeof(th));
	if (th == NULL)
		return -ENOMEM;

	rc = pthread_create(th, NULL, func, args);
	if (rc == 0)
		*ret_th = th;

	return -rc;
}

void thread_exit(void)
{

}

static int thread_join(clovis_st_thread_t *th)
{
	void *result;
	int   rc;

	if (pthread_join(*th, &result) == 0) {
		rc = *((int *)result);
		m0_free(result);
		return rc;
	} else
		return -1;
}

static void thread_fini(clovis_st_thread_t *th)
{
	mem_free(th);
	return;
}

#endif

#include "lib/thread.h"

#ifdef __KERNEL__
static int clovis_st_worker(void *in)
#else
static void* clovis_st_worker(void *in)
#endif
{
	int                   idx;
	int                   nr_rounds;
	uint64_t              start;
	uint64_t              end;
	pid_t                 tid;
	struct clovis_st_cfg  cfg;
	int                  *nr_failed_assertions;

#ifndef __KERNEL__
	struct m0_thread     *mthread;

	MEM_ALLOC_PTR(mthread);
	if (mthread == NULL)
		return 0;
	memset(mthread, 0, sizeof(struct m0_thread));

	m0_thread_adopt(mthread, clovis_st_get_mero());

#else
	int                    k_sys_return;
	M0_CLOVIS_THREAD_ENTER;
#endif

	nr_failed_assertions = m0_alloc(sizeof *nr_failed_assertions);
	if (nr_failed_assertions == NULL)
#ifndef __KERNEL__
		return NULL;
#else
		return -ENOMEM;
#endif

	/* set tid of this worker thread*/
	idx = *((int *)in);
	tid = get_tid();
	clovis_st_set_worker_tid(idx, tid);

	cfg = clovis_st_get_cfg();
	nr_rounds = cfg.sc_nr_rounds;

	start = time_now();
	end = start + cfg.sc_deadline;

	/* main loop */
	while(1) {
		/* TODO: synchronize all threads to start the tests
		 * at the same time
		 */

		*nr_failed_assertions = clovis_st_run(clovis_st_get_tests());

		/*
		 * tests don't continue if the thread runs out of
		 * time or reaches the number of rounds
		 */
		if (nr_rounds-- == 0 || time_now() > end)
			break;
	}

#ifndef __KERNEL__
	m0_thread_shun();
	mem_free(mthread);
#endif

	/* wait for the thread_join is called*/
	thread_exit();
#ifndef __KERNEL__
	return nr_failed_assertions;
#else
	k_sys_return = *nr_failed_assertions;
	m0_free(nr_failed_assertions);
	return k_sys_return;
#endif
}

int clovis_st_start_workers(void)
{
	int        i;
	int        rc=0;
	int        nr_workers;
	static int indices[CLOVIS_ST_MAX_WORKER_NUM];

	nr_workers = clovis_st_get_nr_workers();
	if (nr_workers == 0)
		return -EINVAL;

	/* start a few test threads */
	if (workers == NULL) {
		MEM_ALLOC_ARR(workers, nr_workers);
		if (workers == NULL)
			return -ENOMEM;
	}

	for (i = 0; i < nr_workers; i++) {
		indices[i] = i;
		rc = thread_init(workers+i, &clovis_st_worker,
				 indices+i, "Clovis_st_worker_%d", i);
		if (rc < 0)
			break;
	}

	if (rc < 0)
		mem_free(workers);

	return rc;
}

int clovis_st_stop_workers(void)
{
	int i;
	int nr_workers;
	int rc = 0; /* required */

	nr_workers = clovis_st_get_nr_workers();

	if (nr_workers == 0 || workers == NULL)
		return rc;

	for (i = 0; i < nr_workers; i++) {
		rc = thread_join(workers[i]);
		thread_fini(workers[i]);
	}

	/*free workers*/
	mem_free(workers);
	return rc;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
