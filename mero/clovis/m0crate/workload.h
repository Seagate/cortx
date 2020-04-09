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

#pragma once

#ifndef __MERO_CLOVIS_M0CRATE_WORKLOAD_H__
#define __MERO_CLOVIS_M0CRATE_WORKLOAD_H__

/**
 * @defgroup crate_workload
 *
 * @{
 */

#include <sys/param.h>    /* MAXPATHLEN */
#include "lib/memory.h"
#include "clovis/m0crate/crate_utils.h"

/* used for both file offsets and file sizes. */


/*
 * Fundamental data-types.
 */

/*
 * Workloads.
 */

enum {
        CR_WORKLOAD_MAX = 32,
        CR_CSUM_DEV_MAX = 8
};

enum cr_workload_type {
        CWT_HPCS,   /* HPCS type file creation workload */
        CWT_CSUM,   /* checksumming workload */
	CWT_CLOVIS_IO,
	CWT_CLOVIS_INDEX,
        CWT_NR
};


enum swab_type {
        ST_NONE = 0,
        ST_32   = 1,
        ST_32W  = 2,
        ST_64   = 3,
        ST_64W  = 4,
        ST_NR
};

/* description of the whole workload */
struct workload {
        enum cr_workload_type  cw_type;
        const char            *cw_name;
        unsigned               cw_ops;
        unsigned               cw_done;
        unsigned               cw_nr_thread;
        unsigned               cw_rstate;
        bcnt_t                 cw_avg;
        bcnt_t                 cw_max;
        bcnt_t                 cw_block;
        char                  *cw_buf;
        int                    cw_progress;
        int                    cw_header;
        int                    cw_oflag;
        int                    cw_usage;
        int                    cw_directio;
        int                    cw_bound;
	int                    cw_log_level;
        char                  *cw_fpattern; /* "/mnt/m0/dir%d/f%d.%d" */
        unsigned               cw_nr_dir;
	short                  cw_read_frac;
        struct timeval         cw_rate;
        pthread_mutex_t        cw_lock;

        union {
		void *cw_clovis_io;
		void *cw_clovis_index;
                struct cr_hpcs {
                } cw_hpcs;
                struct cr_csum {
                        int      c_nr_devs;
                        struct csum_dev {
                                char *d_name;
                                char *d_csum_name;
                                int   d_fd;
                                int   d_csum_fd;
                        } c_dev[CR_CSUM_DEV_MAX];
                        int      c_async;
                        unsigned c_blocksize;
                        bcnt_t   c_dev_size;
                        int      c_csum;
                        bcnt_t   c_csum_size;
                        int      c_swab;
                } cw_csum;

        } u;
};

enum csum_op_type {
        COT_READ,
        COT_WRITE,
        COT_NR
};

/* description of a single task (thread) executing workload */
struct workload_task {
        struct workload *wt_load;
        unsigned         wt_thread;
        pthread_t        wt_pid;
        bcnt_t           wt_total;
        unsigned         wt_ops;
        union {
                struct task_hpcs {
                        struct timeval   th_open;
                        struct timeval   th_write;
                        int              th_bind;
                } wt_hpcs;
                struct task_csum {
                        struct aiocb    *tc_cb;
                        struct aiocb   **tc_rag;
                        char            *tc_csum_buf;
                        char            *tc_buf;
                } wt_csum;
		void *clovis_task;
		// void *clovis_index_task;
        } u;
};

/* particular operation from a workload */
struct workload_op {
        bcnt_t                wo_size;
        struct workload_task *wo_task;
        union {
                struct op_hpcs {
                        unsigned oh_dirno;
                        unsigned oh_opno;
                        char     oh_fname[MAXPATHLEN];
                } wo_hpcs;
                struct op_csum {
                        enum csum_op_type  oc_type;
                        bcnt_t             oc_offset;
                } wo_csum;
        } u;
};

struct workload_type_ops {
        int (*wto_init)(struct workload *w);
        int (*wto_fini)(struct workload *w);

        void (*wto_run)(struct workload *w, struct workload_task *task);
        void (*wto_op_get)(struct workload *w, struct workload_op *op);
        void (*wto_op_run)(struct workload *w, struct workload_task *task, const struct workload_op *op);
        int  (*wto_parse)(struct workload *w, char ch, const char *optarg);
        void (*wto_check)(struct workload *w);
};
int workload_init(struct workload *w, enum cr_workload_type wtype);
void workload_start(struct workload *w, struct workload_task *task);
void workload_join(struct workload *w, struct workload_task *task);

/** @} end of crate_workload group */
#endif /* __MERO_CLOVIS_M0CRATE_WORKLOAD_H__ */

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
