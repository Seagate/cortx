/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Ivan Alekhin <ivan.alekhin@seagate.com>
 * Original creation date: 30-May-2017
 */

/**
 * @addtogroup crate
 *
 * @{
 */

#include <unistd.h>       /* getopt(), getpid() */
#include <stdlib.h>       /* rand(), strtoul() */
#include <err.h>          /* errx() */
#include <stdarg.h>       /* va_list */
#include <stdio.h>        /* vfprintf(), stderr */
#include <string.h>       /* strdup() */
#include <fcntl.h>        /* O_CREAT */
#include <assert.h>       /* assert() */
#include <inttypes.h>     /* uint64_t */
#include <aio.h>          /* lio_list(), LIO_* */
#include <sys/stat.h>     /* stat() */
#include <sys/time.h>     /* gettimeofday() */
#include <sys/resource.h> /* getrusage() */
#include <pthread.h>
#include <errno.h>

#include "lib/memory.h"
#include "lib/types.h"
#include "lib/trace.h"
#include "module/instance.h"

#include "clovis/m0crate/logger.h"
#include "clovis/m0crate/workload.h"
#include "clovis/m0crate/parser.h"
#include "clovis/m0crate/crate_clovis.h"
#include "clovis/m0crate/crate_clovis_utils.h"

extern struct crate_clovis_conf *conf;

const char   cr_default_fpattern[] = "./dir%i/f%i.%i";
const bcnt_t cr_default_avg        = 64 * 1024;   /* 64K average file size */
const bcnt_t cr_default_max        = 1024 * 1024; /* 1M maximal file size */
const int    cr_default_ops        = 1000;
const bcnt_t cr_default_block      = 0;
const int    cr_default_nr_dir     = 1;
const int    cr_default_nr_thread  = 1;
const short  cr_default_read_frac  = 50;
const bcnt_t cr_default_blocksize  = 16 * 1024;
const bcnt_t cr_default_csum_size  = 16;


const char *cr_workload_name[CWT_NR] = {
        [CWT_HPCS]         = "hpcs",
        [CWT_CSUM]         = "csum",
	[CWT_CLOVIS_IO]    = "clovis_io",
	[CWT_CLOVIS_INDEX] = "clovis_index",
};

static int hpcs_init  (struct workload *w);
static int hpcs_fini  (struct workload *w);
static void hpcs_run   (struct workload *w, struct workload_task *task);
static void hpcs_op_get(struct workload *w, struct workload_op *op);
static void hpcs_op_run(struct workload *w, struct workload_task *task,
			const struct workload_op *op);
static int  hpcs_parse (struct workload *w, char ch, const char *optarg);
static void hpcs_check (struct workload *w);

static int csum_init  (struct workload *w);
static int csum_fini  (struct workload *w);
static void csum_run   (struct workload *w, struct workload_task *task);
static void csum_op_get(struct workload *w, struct workload_op *op);
static void csum_op_run(struct workload *w, struct workload_task *task,
			const struct workload_op *op);
static int  csum_parse (struct workload *w, char ch, const char *optarg);
static void csum_check (struct workload *w);

static const struct workload_type_ops w_ops[CWT_NR] = {
        [CWT_HPCS] = {
                .wto_init   = hpcs_init,
                .wto_fini   = hpcs_fini,
                .wto_run    = hpcs_run,
                .wto_op_get = hpcs_op_get,
                .wto_op_run = hpcs_op_run,
		.wto_parse  = hpcs_parse,
		.wto_check  = hpcs_check
        },
        [CWT_CSUM] = {
                .wto_init   = csum_init,
                .wto_fini   = csum_fini,
                .wto_run    = csum_run,
                .wto_op_get = csum_op_get,
                .wto_op_run = csum_op_run,
		.wto_parse  = csum_parse,
		.wto_check  = csum_check
        },

	[CWT_CLOVIS_IO] = {
                .wto_init   = clovis_init,
                .wto_fini   = clovis_fini,
                .wto_run    = clovis_run,
                .wto_op_get = NULL,
                .wto_op_run = clovis_op_run,
		.wto_parse  = NULL,
		.wto_check  = clovis_check
        },

	[CWT_CLOVIS_INDEX] = {
                .wto_init   = clovis_init,
                .wto_fini   = clovis_fini,
                .wto_run    = clovis_run_index,
                .wto_op_get = NULL,
                .wto_op_run = clovis_op_run_index,
		.wto_parse  = NULL,
		.wto_check  = clovis_check
        },
};

static void fletcher_2_native(void *buf, uint64_t size);
static void fletcher_4_native(void *buf, uint64_t size);
static void csum_touch(void *buf, uint64_t size);
static void csum_none(void *buf, uint64_t size);

static const struct csum_alg {
        const char *ca_label;
        void      (*ca_func)(void *buf, uint64_t size);
} csums[] = {
        {
                .ca_label = "fletcher2",
                .ca_func  = fletcher_2_native
        },
        {
                .ca_label = "fletcher4",
                .ca_func  = fletcher_4_native
        },
        {
                .ca_label = "touch",
                .ca_func  = csum_touch
        },
        {
                .ca_label = "sha256",
                .ca_func  = NULL
        },
        {
                .ca_label = "none",
                .ca_func  = csum_none
        }
};

static const struct workload_type_ops *wop(struct workload *w)
{
        assert(0 <= w->cw_type && w->cw_type < ARRAY_SIZE(w_ops));
        return &w_ops[w->cw_type];
}

/* get a pseudo-random number in the interval [a, b] */
static unsigned long long getrnd(unsigned long long a, unsigned long long b)
{
        double r;
        unsigned long long scaled;

        assert(a <= b);

        r = rand();
        scaled = r*(b - a + 1.0)/(RAND_MAX+1.0) + a;
        assert(a <= scaled && scaled <= b);
        cr_log(CLL_DEBUG, "random [%llu, %llu]: %llu\n", a, b, scaled);
        return scaled;
}

static long long min(long long a, long long b)
{
        return a < b ? a : b;
}

static long long max(long long a, long long b)
{
        return a < b ? b : a;
}

/** Normalize struct timeval, so that microseconds field (tv_usec)
 * is less than one million.
 */
void timeval_norm(struct timeval *t)
{
        while (t->tv_usec < 0) {
                t->tv_sec--;
                t->tv_usec += 1000000;
        }
        while (t->tv_usec >= 1000000) {
                t->tv_sec++;
                t->tv_usec -= 1000000;
        }
}

int workload_init(struct workload *w, enum cr_workload_type wtype)
{
        pthread_mutex_init(&w->cw_lock, NULL);
        w->cw_type      = wtype;
	w->cw_name      = cr_workload_name[wtype];
        w->cw_avg       = cr_default_avg;
        w->cw_max       = cr_default_max;
        w->cw_ops       = cr_default_ops;
        w->cw_nr_thread = cr_default_nr_thread;
        w->cw_block     = cr_default_block;
        w->cw_fpattern  = strdup(cr_default_fpattern);
        w->cw_nr_dir    = cr_default_nr_dir;
        w->cw_read_frac = cr_default_read_frac;

	return wop(w)->wto_init(w);
}

static void workload_fini(struct workload *w)
{
        wop(w)->wto_fini(w);
	free(w->cw_buf);
	free(w->cw_fpattern);
        pthread_mutex_destroy(&w->cw_lock);
}

/* construct next operation for a given workload. Retuns +ve if workload has
   been completed */
static int workload_op_get(struct workload *w, struct workload_op *op)
{
        int result;
        int opno;
        int percent;

        pthread_mutex_lock(&w->cw_lock);
        if (w->cw_done != w->cw_ops) {
                opno = w->cw_done++;

                if ((w->cw_done % 100000) == 0) {
                        struct timeval orate;
                        struct timeval delay;

                        gettimeofday(&orate, NULL);
                        memset(&delay, 0, sizeof delay);
                        timeval_diff(&w->cw_rate, &orate, &delay);
                        cr_log(CLL_INFO, "rate: %8i %8.8f\n", w->cw_done,
                               rate(100000, &delay, 1));
                        w->cw_rate = orate;
                }

                wop(w)->wto_op_get(w, op); /* released the mutex */
                result = 0;
                /* indicate the progress */
                if (w->cw_progress) {
                        percent = opno * 100 / w->cw_ops;
                        if (percent * w->cw_ops == opno * 100) {
                                if (percent / 10 * 10 == percent)
                                        printf("%02i%%", percent);
                                else
                                        printf(".");
                                fflush(stdout);
                        }
                }
        } else {
                pthread_mutex_unlock(&w->cw_lock);
                result = +1; /* nothing to do */
        }
        return result;
}

static bcnt_t w_size(const struct workload *w)
{
	return min(getrnd(1, w->cw_avg * 2), w->cw_max);
}

static const struct {
	int             opcode;
	const char     *opname;
} optable[COT_NR] = {
	[COT_READ] = {
		.opcode = LIO_READ,
		.opname = "read"
	},
	[COT_WRITE] = {
		.opcode = LIO_WRITE,
		.opname = "write"
	},
};

static enum csum_op_type rw_get(const struct workload *w)
{
	return getrnd(0, 99) < w->cw_read_frac ? COT_READ : COT_WRITE;
}

static void *worker_thread(void *datum)
{
        struct workload_task *wt = datum;
        struct workload      *w  = wt->wt_load;
        struct workload_op    op;

        op.wo_task = wt;

	/*
	 * Clovis can launch multiple operations in a single go.
	 * Single operation in a loop won't work for Clovis.
	 */
	if (w->cw_type == CWT_CLOVIS_IO || w->cw_type == CWT_CLOVIS_INDEX)
		wop(w)->wto_op_run(w, wt, NULL);
	else {
		while (workload_op_get(w, &op) == 0)
			wop(w)->wto_op_run(w, wt, &op);
	}
        return NULL;
}

void workload_start(struct workload *w, struct workload_task *task)
{
        int i;
        int result;

        if (w->cw_nr_thread == 1) {
                task[0].wt_load = w;
                task[0].wt_thread = 0;
                worker_thread(&task[0]);
        } else {
                for (i = 0; i < w->cw_nr_thread; ++i) {
                        task[i].wt_load = w;
                        task[i].wt_thread = i;
                        result = pthread_create(&task[i].wt_pid,
                                                NULL, worker_thread, &task[i]);
                        if (result != 0)
                                err(1, "cannot create worker thread");
                        cr_log(CLL_DEBUG, "created worker thread %i\n", i);
                }
                cr_log(CLL_TRACE, "threads created\n");
        }
}

void workload_join(struct workload *w, struct workload_task *task)
{
        int result;
        int i;

        if (w->cw_nr_thread > 1) {
                for (i = 0; i < w->cw_nr_thread; ++i) {
                        result = pthread_join(task[i].wt_pid, NULL);
                        if (result != 0)
                                warn("cannot join worker thread %i", i);
                        cr_log(CLL_DEBUG, "worker thread done %i\n", i);
                }
        }
}

static void workload_run(struct workload *w)
{
        struct workload_task *tasks; /* joys of C99 */
        struct rusage        u0;
        struct rusage        u1;
        struct timeval       wall_start;
        struct timeval       wall_end;

	M0_ALLOC_ARR(tasks, w->cw_nr_thread);
	if (tasks == NULL)
		return ;
        if (w->cw_block != 0 && w->cw_directio)
                w->cw_block += getpagesize();

        cr_log(CLL_INFO, "workload type          %s/%i\n",
               w->cw_name, w->cw_type);
        cr_log(CLL_INFO, "random seed:           %u\n", w->cw_rstate);
        cr_log(CLL_INFO, "number of threads:     %u\n", w->cw_nr_thread);
	if (CWT_CLOVIS_IO != w->cw_type) {
		cr_log(CLL_INFO, "average size:          %llu\n", w->cw_avg);
		cr_log(CLL_INFO, "maximal size:          %llu\n", w->cw_max);
		/*
		 * XXX: cw_block could be reused for Clovis instead of cwi_bs,
		 * but we can't always access `struct workload' there.
		 * That's pity and should be fixed some day probably.
		 */
		cr_log(CLL_INFO, "block size:            %llu\n", w->cw_block);
		cr_log(CLL_INFO, "number of operations:  %u\n", w->cw_ops);
		cr_log(CLL_INFO, "oflags:                %o\n", w->cw_oflag);
		cr_log(CLL_INFO, "bound mode:            %s\n",
		       w->cw_bound ? "on" : "off");
	}

        if (w->cw_rstate == 0)
                w->cw_rstate = time(0) + getpid();
        srand(w->cw_rstate);

        getrusage(RUSAGE_SELF, &u0);
        gettimeofday(&w->cw_rate, NULL);
        gettimeofday(&wall_start, NULL);
        wop(w)->wto_run(w, tasks);
	m0_free(tasks);
        gettimeofday(&wall_end, NULL);
        getrusage(RUSAGE_SELF, &u1);
        timeval_sub(&wall_end, &wall_start);
        if (w->cw_usage) {
                timeval_sub(&u1.ru_utime, &u0.ru_utime);
                timeval_sub(&u1.ru_stime, &u0.ru_stime);
                u1.ru_maxrss   -= u0.ru_maxrss;   /* integral max resident set
                                                     size */
                u1.ru_ixrss    -= u0.ru_ixrss;    /* integral shared text memory
                                                     size */
                u1.ru_idrss    -= u0.ru_idrss;    /* integral unshared data
                                                     size */
                u1.ru_isrss    -= u0.ru_isrss;    /* integral unshared stack
                                                     size */
                u1.ru_minflt   -= u0.ru_minflt;   /* page reclaims */
                u1.ru_majflt   -= u0.ru_majflt;   /* page faults */
                u1.ru_nswap    -= u0.ru_nswap;    /* swaps */
                u1.ru_inblock  -= u0.ru_inblock;  /* block input operations */
                u1.ru_oublock  -= u0.ru_oublock;  /* block output operations */
                u1.ru_msgsnd   -= u0.ru_msgsnd;   /* messages sent */
                u1.ru_msgrcv   -= u0.ru_msgrcv;   /* messages received */
                u1.ru_nsignals -= u0.ru_nsignals; /* signals received */
                u1.ru_nvcsw    -= u0.ru_nvcsw;    /* voluntary context
                                                     switches */
                u1.ru_nivcsw   -= u0.ru_nivcsw;   /* involuntary context
                                                     switches */
                printf("time: (w: %f u: %f s: %f)\n"
               "\tmaxrss:   %6li ixrss:    %6li idrss:  %6li isrss: %6li\n"
               "\tminflt:   %6li majflt:   %6li nswap:  %6li\n"
               "\tinblock:  %6li outblock: %6li\n"
               "\tmsgsnd:   %6li msgrcv:   %6li\n"
               "\tnsignals: %6li nvcsw:    %6li nivcsw: %6li\n",
                       tsec(&wall_end), tsec(&u1.ru_utime), tsec(&u1.ru_stime),
                       u1.ru_maxrss,
                       u1.ru_ixrss,
                       u1.ru_idrss,
                       u1.ru_isrss,
                       u1.ru_minflt,
                       u1.ru_majflt,
                       u1.ru_nswap,
                       u1.ru_inblock,
                       u1.ru_oublock,
                       u1.ru_msgsnd,
                       u1.ru_msgrcv,
                       u1.ru_nsignals,
                       u1.ru_nvcsw,
                       u1.ru_nivcsw);
        }
}


/*
 * HPCS workload for Creation Rate (crate) benchmark.
 */
static inline struct cr_hpcs *w2hpcs(struct workload *w)
{
        return &w->u.cw_hpcs;
}

static int hpcs_init(struct workload *w)
{
	return 0;
}

static int hpcs_fini(struct workload *w)
{
	return 0;
}

static void hpcs_op_get(struct workload *w, struct workload_op *op)
{
        unsigned dir;
        int nob;
        int opno;
        unsigned long long fid;

        opno = w->cw_done;
        /*
         * All calls to PRNG have to be protected by the workload lock to
         * maintain repeatability.
         */
        /* select a directory */
        dir = getrnd(0, w->cw_nr_dir - 1);
        /* select file identifier */
        fid = getrnd(0, w->cw_ops - 1);
        /* choose file size */
        op->wo_size = w_size(w);
        pthread_mutex_unlock(&w->cw_lock);

        /*
         * If a task is bound to a directory, force creation in this
         * directory. Note, that PRNG has to be called though its result is
         * discarded.
         */
        if (op->wo_task->u.wt_hpcs.th_bind >= 0)
                dir = op->wo_task->u.wt_hpcs.th_bind;

        op->u.wo_hpcs.oh_dirno = dir;
        op->u.wo_hpcs.oh_opno  = opno;
        /* form a file name */
        nob = snprintf(op->u.wo_hpcs.oh_fname, sizeof op->u.wo_hpcs.oh_fname,
                       w->cw_fpattern, dir, (int)fid, opno);
        if (nob >= sizeof op->u.wo_hpcs.oh_fname)
                errx(1, "buffer [%zi] is too small for %s (%i,%i,%i)",
                     sizeof op->u.wo_hpcs.oh_fname,
                     w->cw_fpattern, dir, (int)fid, opno);
        cr_log(CLL_TRACE, "op %i: \"%s\" %llu\n",
               opno, op->u.wo_hpcs.oh_fname, op->wo_size);
}

/* execute one operation from an HPCS workload */
static void hpcs_op_run(struct workload *w, struct workload_task *task,
			const struct workload_op *op)
{
        int         fd;
        int         result;
        struct stat st;
        bcnt_t      nob;
        bcnt_t      towrite;
        int         psize;
        void       *buf;

        struct timeval t0;
        struct timeval t1;

        struct workload_task *wt = op->wo_task;
        const char           *fname;

        fname = op->u.wo_hpcs.oh_fname;
        cr_log(CLL_TRACE, "thread %i write to \"%s\"\n", wt->wt_thread, fname);
        gettimeofday(&t0, NULL);
        fd = open(fname, O_CREAT|O_WRONLY|w->cw_oflag, S_IWUSR);
        if (fd == -1)
                err(2, "cannot create %s", fname);

        gettimeofday(&t1, NULL);
        timeval_diff(&t0, &t1, &wt->u.wt_hpcs.th_open);

        wt->wt_ops++;
        psize = getpagesize();
        if (w->cw_block == 0 || w->cw_buf == NULL) {
                result = fstat(fd, &st);
                if (result == -1)
                        err(2, "stat(\"%s\") failed", fname);
                pthread_mutex_lock(&w->cw_lock);
                if (w->cw_block == 0)
                        w->cw_block = st.st_blksize;
                if (w->cw_buf == NULL) {
                        size_t toalloc;

                        toalloc = w->cw_block;
                        if (w->cw_directio)
                                toalloc += psize;

                        w->cw_buf = malloc(toalloc);
                        if (w->cw_buf == NULL)
                                errx(3, "cannot allocate buffer (%llu)",
                                     w->cw_block);
                }
                pthread_mutex_unlock(&w->cw_lock);
        }
        nob = 0;

        buf = w->cw_buf;
        if (w->cw_directio)
                buf -= ((unsigned long)buf) % psize;

        while (nob < op->wo_size) {
                towrite = min(op->wo_size - nob, w->cw_block);
                if (w->cw_directio)
                        towrite = max(towrite / psize * psize, psize);

                result = write(fd, buf, towrite);
                if (result == -1)
                        err(2, "write on \"%s\" failed (%p, %llu)", fname,
                            buf, towrite);
                nob += result;
                cr_log(result != towrite ? CLL_WARN : CLL_TRACE,
                       "thread %i wrote %llu of %llu on \"%s\"\n",
                       wt->wt_thread, nob, op->wo_size, fname);
        }
        gettimeofday(&t0, NULL);
        result = close(fd);
        if (result == -1)
                warn("close");
        wt->wt_total += nob;
        timeval_diff(&t1, &t0, &wt->u.wt_hpcs.th_write);

        cr_log(CLL_TRACE, "thread %i done writing %llu to \"%s\"\n",
               wt->wt_thread, op->wo_size, fname);
}

static void hpcs_run(struct workload *w, struct workload_task *task)
{
        int            i;

        unsigned       ops;
        unsigned       dir;
        bcnt_t         nob;
        struct timeval t_open;
        struct timeval t_write;
        struct timeval wall_start;
        struct timeval wall_end;

        ops = 0;
        nob = 0;
        memset(&t_open, 0, sizeof t_open);
        memset(&t_write, 0, sizeof t_write);

        cr_log(CLL_INFO, "file name pattern:     \"%s\"\n", w->cw_fpattern);
        cr_log(CLL_INFO, "number of directories: %u\n", w->cw_nr_dir);

        if (w->cw_nr_dir == 0)
                errx(1, "no directories");

        gettimeofday(&wall_start, NULL);
        for (dir = 0, i = 0; i < w->cw_nr_thread; ++i) {
                if (w->cw_bound) {
                        task[i].u.wt_hpcs.th_bind = dir;
                        dir = (dir + 1) % w->cw_nr_dir;
                } else
                        task[i].u.wt_hpcs.th_bind = -1;
        }
        workload_start(w, task);
        workload_join(w, task);
        gettimeofday(&wall_end, NULL);
        cr_log(CLL_TRACE, "threads done\n");
        for (i = 0; i < w->cw_nr_thread; ++i) {
                ops += task[i].wt_ops;
                nob += task[i].wt_total;
                timeval_add(&t_open, &task[i].u.wt_hpcs.th_open);
                timeval_add(&t_write, &task[i].u.wt_hpcs.th_write);
        }
        if (w->cw_progress)
                printf("\n");

        timeval_sub(&wall_end, &wall_start);

        if (w->cw_header) {
                printf("           time            ops     c-rate         "
		       "nob          M-rate\n");
                printf("   wall    open    write         wall proc"
		       "                    wall proc\n");
        }

        printf("%7.0f %7.0f %7.0f %6u %6.0f %6.0f  %10llu  %6.0f %6.0f\n",
               tsec(&wall_end) * 100., tsec(&t_open) * 100.,
               tsec(&t_write) * 100., ops, rate(ops, &wall_end, 1),
               rate(ops, &t_open, 1), nob,
               rate(nob, &wall_end, 1000000), rate(nob, &t_write, 1000000));
}

static int hpcs_parse (struct workload *w, char ch, const char *optarg)
{
	switch (ch) {
	case 'f':
		if (w->cw_fpattern != NULL)
			free(w->cw_fpattern);
		w->cw_fpattern = strdup(optarg);
		return +1;
	case 'd':
		w->cw_nr_dir = getnum(optarg, "nr_dir");
		return +1;
	}
	return 0;
}

static void hpcs_check (struct workload *w)
{
	if (w->cw_fpattern == NULL)
		err(1, "cannot duplicate pattern");
	if (w->cw_bound)
		w->cw_nr_thread *= w->cw_nr_dir;
}

/*
 * CSUM workload for Creation Rate (crate) benchmark.
 */

static inline struct cr_csum *w2csum(struct workload *w)
{
        return &w->u.cw_csum;
}

static int csum_init(struct workload *w)
{
        struct cr_csum *s;
        int             i;

        s = w2csum(w);
        for (i = 0; i < ARRAY_SIZE(s->c_dev); ++i) {
                s->c_dev[i].d_fd       = -1;
                s->c_dev[i].d_csum_fd  = -1;
        }
        s->c_blocksize = cr_default_blocksize;
        s->c_csum_size = cr_default_csum_size;
	return 0;
}

static int csum_fini(struct workload *w)
{
        struct cr_csum  *s;
        int              i;
        struct csum_dev *dev;

        s = w2csum(w);
        for (i = 0, dev = s->c_dev; i < ARRAY_SIZE(s->c_dev); ++i, ++dev) {
                if (dev->d_fd >= 0)
                        close(dev->d_fd);
                if (dev->d_csum_fd >= 0)
                        close(dev->d_csum_fd);
		free(dev->d_name);
		free(dev->d_csum_name);
        }
	return 0;
}

static void csum_op_get(struct workload *w, struct workload_op *op)
{
        struct cr_csum *s;

        int               opno;
        bcnt_t            count;
        bcnt_t            offset;
        enum csum_op_type otype;

        s = w2csum(w);

        opno = w->cw_done;
        otype = rw_get(w);
        count = min(w->cw_block ?: w_size(w), s->c_dev_size);
        offset = getrnd(0, max(s->c_dev_size - count - 1, 0));

        pthread_mutex_unlock(&w->cw_lock);

        op->u.wo_csum.oc_type   = otype;
        op->u.wo_csum.oc_offset = offset;
        op->wo_size             = count;

        cr_log(CLL_TRACE, "op %i: %s [%llu, %llu]\n",
               opno, optable[otype].opname, offset, count);
}

static void csum_none(void *buf, uint64_t size)
{
}

static void csum_touch(void *buf, uint64_t size)
{
        uint32_t          *ip = buf;
        uint32_t          *ipend = ip + (size / sizeof (uint32_t));
        volatile uint32_t  word;

        for (; ip < ipend; ip++)
                word = *ip;
	(void)word; /* suppress "set but not used" warning. */
}

/*
 * Hash functions copied from zfs.
 */

static void fletcher_2_native(void *buf, uint64_t size)
{
        const uint64_t *ip = buf;
        const uint64_t *ipend = ip + (size / sizeof (uint64_t));
        uint64_t a0, b0, a1, b1;

        for (a0 = b0 = a1 = b1 = 0; ip < ipend; ip += 2) {
                a0 += ip[0];
                a1 += ip[1];
                b0 += a0;
                b1 += a1;
        }
}

static void fletcher_4_native(void *buf, uint64_t size)
{
        const uint32_t *ip = buf;
        const uint32_t *ipend = ip + (size / sizeof (uint32_t));
        uint64_t a, b, c, d;

        for (a = b = c = d = 0; ip < ipend; ip++) {
                a += ip[0];
                b += a;
                c += b;
                d += c;
        }
}

/* do some bit shuffling */
static inline uint32_t csum_shuffle32(uint32_t in)
{
        return
                ((in &     0xff00) >> 8) |
                ((in &       0xff) << 8) |
                ((in & 0xff000000) >> 8) |
                ((in &   0xff0000) << 8);
}

static inline uint64_t csum_shuffle64(uint64_t in)
{
        return
                ((in &             0xff00ULL) >> 8) |
                ((in &               0xffULL) << 8) |
                ((in &         0xff000000ULL) >> 8) |
                ((in &           0xff0000ULL) << 8) |
                ((in &     0xff0000000000ULL) >> 8) |
                ((in &       0xff00000000ULL) << 8) |
                ((in & 0xff00000000000000ULL) >> 8) |
                ((in &   0xff000000000000ULL) << 8);
}

static void csum_compute(struct workload *w, void *buf, size_t count)
{
        struct cr_csum *s;

        uint32_t          *ip32 = buf;
        uint32_t          *ipend32 = ip32 + (count / sizeof (uint32_t));
        volatile uint32_t  word32;

        uint64_t          *ip64 = buf;
        uint64_t          *ipend64 = ip64 + (count / sizeof (uint64_t));
        volatile uint64_t  word64;

	/*
	 * ST_32 and ST_64 modes don't write result of the bit swap operation.
	 * They are intended to be used for performance/overhead checks.
	 * Mark word32/word64 with volatile keyword to switch off compiler's
	 * optimisation.
	 *
	 * Same implies for csum_touch() implementation.
	 */

        s = w2csum(w);
        assert(0 <= s->c_csum && s->c_csum < ARRAY_SIZE(csums));

        switch (s->c_swab) {
        case ST_NONE:
                break;
        case ST_32:
                for (; ip32 < ipend32; ip32++)
                        word32 = csum_shuffle32(*ip32);
                break;
        case ST_32W:
                for (; ip32 < ipend32; ip32++)
                        *ip32 = csum_shuffle32(*ip32);
                break;
        case ST_64:
                for (; ip64 < ipend64; ip64++)
                        word64 = csum_shuffle64(*ip64);
                break;
        case ST_64W:
                for (; ip64 < ipend64; ip64++)
                        *ip64 = csum_shuffle64(*ip64);
                break;
        default:
                assert(0);
        }
        csums[s->c_csum].ca_func(buf, count);
	(void)word64; /* suppress "set but not used" warning. */
	(void)word32; /* suppress "set but not used" warning. */
}

static void csum_csum(struct workload *w, struct cr_csum *s,
                      char *buf, bcnt_t count)
{
        int i;
        int j;
        unsigned bsize;

        bsize = s->c_blocksize;
        for (i = 0; i < s->c_nr_devs; ++i) {
                for (j = 0; j < count / bsize; ++j)
                        csum_compute(w, buf + i*count + j*bsize, bsize);
        }
}

static void csum_io(struct workload *w, struct cr_csum *s,
                    const struct workload_op *op,
                    bcnt_t offset, bcnt_t count, int opcode, const char *opname)
{
        bcnt_t           csum_offset = offset / s->c_blocksize * s->c_csum_size;
        bcnt_t           csum_blocks = count / s->c_blocksize;
        bcnt_t           csum_count  = csum_blocks * s->c_csum_size;
        int              nr_devs     = s->c_nr_devs;
        struct aiocb    *cb;
        struct aiocb   **rag;
        char            *csum_buf;
        struct task_csum *tc;
        struct csum_dev *dev;
        int              i;
        int              rc;
        ssize_t          nob;
        int              ops;

        tc = &op->wo_task->u.wt_csum;
        cb       = tc->tc_cb;
        rag      = tc->tc_rag;
        csum_buf = tc->tc_csum_buf;

        memset(cb, 0, sizeof *cb);

        if (opcode == LIO_WRITE)
                csum_csum(w, s, tc->tc_buf, count);

        for (ops = 0, i = 0, dev = s->c_dev; i < nr_devs; ++i, ++dev) {
                rag[i] = &cb[i];
                if (dev->d_fd >= 0) {
                        cb[i].aio_fildes     = dev->d_fd;
                        cb[i].aio_lio_opcode = opcode;
                        /* read into separate buffers so that checksum function
                           dutifully incurs cache misses on each. */
                        cb[i].aio_buf    = tc->tc_buf + i*count;
                        cb[i].aio_nbytes = count;
                        cb[i].aio_offset = offset;
                        ops++;
                } else
                        cb[i].aio_lio_opcode = LIO_NOP;

                rag[i + nr_devs] = &cb[i + nr_devs];
                if (dev->d_csum_fd >= 0) {
                        cb[i + nr_devs].aio_fildes     = dev->d_csum_fd;
                        cb[i + nr_devs].aio_lio_opcode = opcode;
                        cb[i + nr_devs].aio_buf = &csum_buf[csum_count * i];
                        cb[i + nr_devs].aio_nbytes = csum_count;
                        cb[i + nr_devs].aio_offset = csum_offset;
                        ops++;
                } else
                        cb[i + nr_devs].aio_lio_opcode = LIO_NOP;
        }

        if (ops > 0) {
                rc = lio_listio(LIO_WAIT, rag, nr_devs * 2, NULL);
                if (rc != 0) {
                        for (i = 0; i < nr_devs * 2; ++i) {
                                const char *name;
                                const char *pref;

                                if (i < nr_devs) {
                                        name = s->c_dev[i].d_name;
                                        pref = "";
                                } else {
                                        name = s->c_dev[i-nr_devs].d_csum_name;
                                        pref = "checksum ";
                                }
                                nob = aio_return(&cb[i]);
                                if (nob < count)
                                        fprintf(stderr,
                                                "short async %s %s%s: "
                                                "%zi < %llu\n",
                                                opname, pref, name, nob, count);
                                else if (nob < 0)
                                        warn("async %s%s failed on %s with %i",
                                             pref, opname, name,
                                             aio_error(&cb[i]));
                        }
                        err(1, "async %s failed", opname);
                }
        }
        if (opcode == LIO_READ)
                csum_csum(w, s, tc->tc_buf, count);
}

/* execute one operation from a CSUM workload */
static void csum_op_run(struct workload *w, struct workload_task *task,
			const struct workload_op *op)
{
        csum_io(w, w2csum(w), op, op->u.wo_csum.oc_offset, op->wo_size,
                optable[op->u.wo_csum.oc_type].opcode,
                optable[op->u.wo_csum.oc_type].opname);
        op->wo_task->wt_total += op->wo_size;
}

static int csum_dev_open(struct workload *w, const char *dev_name)
{
        int fd;

        if (strcmp(dev_name, "/dev/zero")) {
                fd = open(dev_name, O_RDWR|O_CREAT|w->cw_oflag,
                          S_IWUSR|S_IRUSR);
                if (fd == -1)
                        err(2, "open(\"%s\")", dev_name);
        } else
                fd = -1;
        return fd;
}

static void csum_run(struct workload *w, struct workload_task *task)
{
        struct cr_csum  *s;
        struct timeval   wall_start;
        struct timeval   wall_end;
        struct csum_dev *dev;
        int              i;
        int              nr_devs;
        bcnt_t           bufsize0;
        bcnt_t           bufsize;
        bcnt_t           nob;

        s = w2csum(w);
        for (i = 0; i < ARRAY_SIZE(s->c_dev); ++i) {
                if (!s->c_dev[i].d_name != !s->c_dev[i].d_csum_name)
                        errx(1, "wrong checksum configuration");
                if (!s->c_dev[i].d_name && !s->c_dev[i].d_csum_name)
                        break;
        }
        nr_devs = s->c_nr_devs = i;

        if (nr_devs == 0)
                errx(1, "No devices specified.");

        for (i = 0, dev = s->c_dev; i < nr_devs; ++i, ++dev) {
                dev->d_fd      = csum_dev_open(w, dev->d_name);
                dev->d_csum_fd = csum_dev_open(w, dev->d_csum_name);
        }

        if (s->c_dev_size == 0) { /* oh well... "portable" */
                if (s->c_dev[0].d_fd >= 0) {
                        off_t seek;

                        seek = lseek(s->c_dev[0].d_fd, 0, SEEK_END);
                        if (seek == (off_t)-1)
                                err(2, "lseek(\"%s\", 0, SEEK_END)",
                                    s->c_dev[0].d_name);
                        s->c_dev_size = seek;
                } else
                        s->c_dev_size = ~0ULL >> 1; /* signed */
        }

        bufsize = bufsize0 = max(w->cw_block, w->cw_max) * nr_devs;
        if (w->cw_bound)
                bufsize *= w->cw_nr_thread;

        w->cw_buf = malloc(bufsize);
        if (w->cw_buf == NULL)
                err(2, "malloc");
        memset(w->cw_buf, '!', bufsize);

        for (i = 0; i < w->cw_nr_thread; ++i) {
                struct task_csum *tc = &task[i].u.wt_csum;
                tc->tc_cb = malloc(2 * nr_devs * sizeof(struct aiocb));
                tc->tc_rag = malloc(2 * nr_devs * sizeof(struct aiocb *));
                tc->tc_csum_buf = malloc(w->cw_max / s->c_blocksize *
                                         s->c_csum_size * nr_devs);
                if (tc->tc_cb == NULL || tc->tc_rag == NULL ||
                    tc->tc_csum_buf == NULL)
                        err(2, "malloc");
                if (w->cw_bound)
                        tc->tc_buf = w->cw_buf + bufsize0 * i;
                else
                        tc->tc_buf = w->cw_buf;
        }

        cr_log(CLL_INFO, "devices:               %i\n", s->c_nr_devs);
        for (i = 0, dev = s->c_dev; i < nr_devs; ++i, ++dev) {
                cr_log(CLL_INFO, "       main device:    \"%s\"/%i\n",
                       dev->d_name, dev->d_fd);
                cr_log(CLL_INFO, "   checksum device:    \"%s\"/%i\n",
                       dev->d_csum_name, dev->d_csum_fd);
        }
        cr_log(CLL_INFO, "device size:           %llu\n", s->c_dev_size);
        cr_log(CLL_INFO, "csum block size:       %u\n", s->c_blocksize);
        cr_log(CLL_INFO, "async mode:            %s\n",
               s->c_async ? "on" : "off");
        cr_log(CLL_INFO, "read fraction:         %i%%\n", w->cw_read_frac);
        cr_log(CLL_INFO, "checksum algorithm:    %s\n",
               csums[s->c_csum].ca_label);

        gettimeofday(&wall_start, NULL);
        workload_start(w, task);
        workload_join(w, task);
        gettimeofday(&wall_end, NULL);
        cr_log(CLL_TRACE, "threads done\n");
        timeval_sub(&wall_end, &wall_start);
        nob = 0;
        for (i = 0; i < w->cw_nr_thread; ++i) {
                nob += task[i].wt_total;
                free(task[i].u.wt_csum.tc_cb);
                free(task[i].u.wt_csum.tc_rag);
                free(task[i].u.wt_csum.tc_csum_buf);
        }
        printf("%7.0f %10llu %6.0f\n",
               tsec(&wall_end) * 100., nob, rate(nob, &wall_end, 1000000));
}

static int  csum_parse (struct workload *w, char ch, const char *optarg)
{
	int              i;
        int              devs;
        int              cdevs;
        struct cr_csum  *csw;

	csw   = w2csum(w);
        devs  = 0;
        cdevs = 0;

	switch (ch) {
	case 'D':
		if (devs == ARRAY_SIZE(csw->c_dev))
			errx(1, "Too many devices.");
		csw->c_dev[devs].d_name = strdup(optarg);
		devs++;
		return +1;
	case 'C':
		if (cdevs == ARRAY_SIZE(csw->c_dev))
			errx(1, "Too many checksum devices.");
		csw->c_dev[cdevs].d_csum_name = strdup(optarg);
		cdevs++;
		return +1;
	case 'S':
		csw->c_blocksize = getnum(optarg, "blocksize");
		return +1;
	case 'z':
		csw->c_csum_size = getnum(optarg, "checksum size");
		return +1;
	case 'c':
		for (i = 0; i < ARRAY_SIZE(csums); ++i) {
			if (!strcmp(optarg, csums[i].ca_label))
				break;
		}
		if (i == ARRAY_SIZE(csums))
			errx(1, "wrong checksum (%s)", optarg);
		csw->c_csum = i;
		return +1;
	case 'w':
		csw->c_swab = getnum(optarg, "byte swap");
		if (csw->c_swab < ST_NONE || csw->c_swab >= ST_NR)
			errx(1, "wrong byte swapping type (%i)", csw->c_swab);
		return +1;
	}
	return 0;
}

static void csum_check (struct workload *w)
{
}

static void usage(void)
{
        int i;

        fprintf(stderr,
"Usage: crate GENERIC OPTIONS -W WORKLOAD_TYPE WORKLOAD_OPTIONS ...\n"
"       Benchmarks various workloads. Each workload is specified \n"
"       by -W option, multiple workloads are executed consecutively.\n\n"
"       Possible workload types are:\n"
"               \"hpcs\" (file creation),\n"
"               \"csum\" (check-summing device),\n"
"                \"db4\" (db4 meta-data back-end),\n"
"           and \"stob\" (Mero storage object).\n"
"\n"
"Options with [defaults]: \n"
"      Generic options\n"
"-v                    increase verbosity level. Can be given multiple times.\n"
"-h                    print this help message.\n\n"
"      Options common for all workload types\n"
"-s SEED               set pseudo-random number generator seed to \n"
"                      a given value. See srand(3).\n"
"-o NR_OPERATIONS      execute given number of operations [%i].\n"
"-t NR_THREAD          number of threads to use [%i].\n"
"-p                    indicate workload execution progress.\n"
"-H                    print header.\n"
"-U                    output resource usage summary on workload completion.\n"
"                      See getrusage(2).\n"
"-b BUFFER_SIZE        IO buffer size for hpcs and stob. If 0, then st_blksize\n"
"                      is used for hpcs, see stat(2). For csum---IO size to\n"
"                      use. If 0, generate IO size randomly, see next options.\n"
"                      For db4---page size, if non-0 [%llu].\n"
"-a AVERAGE_SIZE       average file size for hpcs and stob, average\n"
"                      IO size for csum, log file size for db4 [%llu].\n"
"-M MAXIMAL_SIZE       maximal file size for hpcs and stob, maximal\n"
"                      IO size for csum, cache size if non-0 for db4 [%llu].\n"
"-i                    use O_DIRECT flag when opening files [off].\n"
"-e                    use O_EXCL flag when opening files [off].\n\n"
"-B                    bound threads mode. For hpcs: NR_THREADS work in\n"
"                      each of NR_DIR directories; for csum---every thread\n"
"                      works with its own buffer. For stob: NR_THREADS do IO\n"
"                      against each storage object (NR_LINUX or \n"
"                      NR_LINUX*NR_AD). [off].\n"
"-r PERCENT            percentage of reads in read-write work-loads [%i].\n"
"      \"hpcs\" workload specific options\n"
"-f FILE_PATTERN       file name pattern for sprintf(3). It is formatted with\n"
"                      three unsigned int arguments:\n"
"                          . a directory index;\n"
"                          . a randomly selected file index;\n"
"                          . a sequential operation index.\n"
"                      (Positional arguments %%nn$ can be used.) [%s]\n"
"-d NR_DIR             number of directories [%i].\n"
"      \"csum\" workload specific options\n"
"-D DEVICE             path to main device. If not set, stdio is used.\n"
"-C CSUM_DEVICE        path to check-summing device. No check-summing if\n"
"                      not set.\n"
"                      Up to %i (device, checksum device) pair can be\n"
"                      specified.\n"
"-c LABEL              check-sum algorithm to use, see below [%s].\n"
"-S CSUM_BLOCK_SIZE    size of block to check-sum [%llu].\n"
"-z CSUM_SIZE          size of a checksum [%llu].\n"
"-w TYPE               byte swapping. Valid types are:\n"
"                          0 no byte swapping;\n"
"                          1 32bit swapping;\n"
"                          2 32bit swapping with writeback;\n"
"                          3 64bit swapping;\n"
"                          4 64bit swapping with writeback\n"
"                      [0].\n"
"      \"db4\" workload specific options\n"
"-f, -d                have the same meaning as in \"hpcs\".\n"
"-k NR_PAGES           kill NR_PAGES pages from db4 pool every 1s with\n"
"                      ->mem_trickle().\n"
"-R REC_SIZE           date-base record size [use structure size.\n"
"-l                    instead of populating data-base with records,\n"
"                      dump records from existing data-base, checking their\n"
"                      consistency.\n"
"-z {FLAG}             set locking flag:\n"
"                          d     automatically detect and resolve\n"
"                                deadlocks by aborting yongest transactions;\n"
"                          o{NR} use internal locking instead of db4 one\n"
"                                with NR locks per directory.\n"
"-F {TYPE}{FLAG}       specify db4 flag. TYPE can be one of \n"
"                          e DB_ENV flag;\n"
"                          d DB flag;\n"
"                          t DB_TXN flag.\n"
"                      FLAG of 0 clears all flags. Valid flags for each type\n"
"                      are listed below.\n"
"      \"stob\" workload specific options\n"
"-d NR_LINUX           number of storage objects in linux storage domain [%i].\n"
"-D OBJECT             path to an existing file (or device) to be used for\n"
"                      benchmarking. Can be given multiple times. If fewer\n"
"                      than NR_LINUX objects are given, missing objects are\n"
"                      created. Extra objects are ignored.\n"
"-A NR_AD              number of ad (allocation data) objects to be created\n"
"                      in each linux object. If this option is not specified,\n"
"                      no ad domains are created.\n"
"                      (with -D option) [0].\n"
"-q                    Generate sequential offsets in workload.\n"
"-T                    Parse trace log produced by crashed stob workload.\n"
"-S <filename>         Read workload options from a yaml file.\n"
"\n"
"Numerical values can be in decimal, octal and hexadecimal as understood\n"
"by strtoull(3), optionally followed by a suffix \'b\', \'k\', \'m\', \'g\',\n"
"\'B\', \'K\', \'M\', \'G\', where lower-case multipliers are binary and\n"
"upper-case---decimal ('b' means 512 and 'B' means 500).\n"
"\n"
"All sizes are in bytes. Available checksum algorithms:\n",
                cr_default_ops,
                cr_default_nr_thread,
                cr_default_block,
                cr_default_avg,
                cr_default_max,
                cr_default_read_frac,
                cr_default_fpattern,
                cr_default_nr_dir,
                CR_CSUM_DEV_MAX,
                csums[0].ca_label,
                cr_default_blocksize,
                cr_default_csum_size,
                cr_default_nr_dir
		);
        for (i = 0; i < ARRAY_SIZE(csums); ++i)
                printf("%s ", csums[i].ca_label);
        printf("\n");
}

void print_workload_detail(struct workload *w, int idx)
{
	int i;
	cr_log(CLL_INFO, "Workload:%d:%p\n", idx, &w[0]);
	for(i = 0; i <= idx; i++) {
		cr_log(CLL_INFO, "File name:%s\n",
		 ((struct clovis_workload_io *)(
		   w[i].u.cw_clovis_io))->cwi_filename);
	}
}

int main(int argc, char **argv)
{
        char                       ch;
        int                        idx;
        int                        i;
        struct workload           *w;
        struct workload           *load;
	struct clovis_workload_io *cwi;
	static struct m0           instance;
	int rc;
        static const char opts[] =
		"k:s:o:f:t:W:a:r:R:ez:D:UM:BA:S:Hw:iF:d:C:c:pqb:Thvl";

	idx = -1;
	w   = NULL;

	m0_threads_once_init();
	m0_instance_setup(&instance);

	M0_ALLOC_ARR(load, CR_WORKLOAD_MAX);
	if (load == NULL) {
		return -ENOMEM;
	}

        while ((ch = getopt(argc, argv, opts)) != -1) {
                /*
                 * generic options..
                 */
                switch (ch) {
                case 'v':
                        continue;
                default:
                        if (w == NULL)
                                errx(1, "-W must precede workload options");
                        else
                                break;
                        /* fall through */
                case '?':
                        cr_log(CLL_ERROR, "unknown option\n");
                        /* fall through */
                case 'h':
                        usage();
			free(load);
                        return 1;
                case 'W':
                        if (++idx >= CR_WORKLOAD_MAX)
                                errx(1, "too many workloads (%i)", idx);
                        for (i = 0; i < ARRAY_SIZE(cr_workload_name); ++i) {
                                if (!strcmp(optarg, cr_workload_name[i]))
                                        break;
                        }
                        if (i == ARRAY_SIZE(cr_workload_name))
                                errx(1, "unknown workload type (%s)", optarg);
                        w = &load[idx];
                        rc = workload_init(w, i);
			if (rc != 0)
				errx(1, "failed to init the workload: %d", rc);
                        continue;
		case 'S':
			/* All workloads are specified in a yaml file. */
			M0_ASSERT(idx == -1);
			rc = parse_yaml_file(load, CR_WORKLOAD_MAX, &idx, optarg);
			if (rc != 0) {
				fprintf(stderr, "Unable to parse workload: %d\n", rc);
				m0_free(load);
				return -EINVAL;
			}
			w = &load[idx];
			cwi = load->u.cw_clovis_io;
			if (cwi->cwi_io_size < cwi->cwi_bs) {
				cr_log(CLL_INFO, "IO size should always be "
						 "greater than block size  "
						 "CLOVIS_IOSIZE =%"PRIu64
						 " BLOCK_SIZE =%"PRIu64"\n",
						 cwi->cwi_io_size, cwi->cwi_bs);
				return M0_ERR(-EINVAL);
			}
			continue;
                }
                /*
                 * Workload options.
                 */

                /*
                 * options valid for any workload type
                 */
                switch (ch) {
			int rfrac;
                case 's':
                        w->cw_rstate = getnum(optarg, "seed");
                        continue;
                case 't':
                        w->cw_nr_thread = getnum(optarg, "nr_thread");
                        continue;
                case 'a':
                        w->cw_avg = getnum(optarg, "average size");
                        continue;
                case 'M':
                        w->cw_max = getnum(optarg, "maximal size");
                        continue;
                case 'o':
                        w->cw_ops = getnum(optarg, "operations");
                        continue;
                case 'b':
                        w->cw_block = getnum(optarg, "block");
                        continue;
                case 'e':
                        w->cw_oflag |= O_EXCL;
                        continue;
                case 'H':
                        w->cw_header = 1;
                        continue;
                case 'p':
                        w->cw_progress = 1;
                        continue;
                case 'U':
                        w->cw_usage = 1;
                        continue;
                case 'i':
                        w->cw_directio = 1;
                        w->cw_oflag |= O_DIRECT;
                        continue;
                case 'B':
                        w->cw_bound = 1;
                        continue;
		case 'r':
			rfrac = getnum(optarg, "read percentage");
			if (rfrac < 0 || rfrac > 100)
				errx(1, "invalid percentage (%s)", optarg);
			w->cw_read_frac = rfrac;
			continue;
                }

                /*
                 * workload type specific options
                 */
		if (wop(w)->wto_parse(w, ch, optarg) == 0)
			errx(1, "unknown option '%c' for workload type %s",
			     ch, w->cw_name);
        }

	cr_set_debug_level(conf->log_level);

        if (idx < 0)
                cr_log(CLL_INFO, "no workloads were specified\n");
        for (i = 0; i <= idx; ++i) {
                w = &load[i];
		wop(w)->wto_check(w);
                cr_log(CLL_INFO, "starting workload %i\n", i);
                workload_run(w);
                workload_fini(w);
                cr_log(CLL_INFO, "done workload %i\n", i);
                cr_log(CLL_INFO, "---------------------------------------\n");
        }
	m0_free(load);
        return 0;
}

/** @} end of crate group */

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
