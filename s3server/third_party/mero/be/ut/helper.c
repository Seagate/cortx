/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 5-Jun-2013
 */

#include <stdlib.h>		/* system */
#include <sys/stat.h>		/* mkdir */
#include <sys/types.h>		/* mkdir */
#include <pthread.h>		/* pthread_once */
#include <unistd.h>		/* syscall */
#include <sys/syscall.h>	/* syscall */
#include <unistd.h>		/* chdir, get_current_dir_name */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"
#include "lib/memory.h"		/* m0_alloc */
#include "lib/misc.h"		/* M0_BITS, ARRAY_SIZE */
#include "lib/arith.h"		/* M0_CNT_INC */
#include "lib/errno.h"		/* ENOMEM */
#include "lib/finject.h"        /* M0_FI_ENABLED */
#include "rpc/rpclib.h"		/* m0_rpc_server_start */
#include "net/net.h"		/* m0_net_xprt */
#include "module/instance.h"	/* m0_get */
#include "conf/obj.h"           /* M0_CONF_PROCESS_TYPE */
#include "stob/domain.h"	/* m0_stob_domain_create */

#include "ut/stob.h"		/* m0_ut_stob_linux_get */
#include "be/ut/helper.h"	/* m0_be_ut_backend */
#include "be/tx_internal.h"	/* m0_be_tx__reg_area */
#include "be/seg0.h"            /* m0_be_0type_register */

const struct m0_bob_type m0_ut_be_backend_bobtype;
M0_BOB_DEFINE(M0_INTERNAL, &m0_ut_be_backend_bobtype, m0_be_ut_backend);
const struct m0_bob_type m0_ut_be_backend_bobtype = {
	.bt_name         = "m0_ut_be_backend_bobtype",
	.bt_magix_offset = offsetof(struct m0_be_ut_backend, but_magix),
	.bt_magix        = M0_BE_TX_ENGINE_MAGIC,
	.bt_check        = NULL,
};

struct m0_be_ut_sm_group_thread {
	struct m0_thread    sgt_thread;
	pid_t		    sgt_tid;
	struct m0_semaphore sgt_asts_run_sem;
	struct m0_semaphore sgt_stop_sem;
	struct m0_sm_group  sgt_grp;
	bool		    sgt_lock_new;
};

struct be_ut_helper_struct {
	struct m0_net_xprt      *buh_net_xprt;
	struct m0_rpc_server_ctx buh_rpc_sctx;
	struct m0_reqh         **buh_reqh;
	pthread_once_t		 buh_once_control;
	struct m0_mutex		 buh_seg_lock;
	void			*buh_addr;
	int64_t			 buh_id;
};

static struct be_ut_helper_struct be_ut_helper = {
	/* because there is no m0_mutex static initializer */
	.buh_once_control = PTHREAD_ONCE_INIT,
};

static void be_ut_helper_fini(void)
{
	m0_mutex_fini(&be_ut_helper.buh_seg_lock);
}

/* XXX call this function from m0_init()? */
static void be_ut_helper_init(void)
{
	struct be_ut_helper_struct *h = &be_ut_helper;

	h->buh_addr = (void *) BE_UT_SEG_START_ADDR;
	h->buh_id = BE_UT_SEG_START_ID;
	m0_mutex_init(&h->buh_seg_lock);

	atexit(&be_ut_helper_fini);	/* XXX REFACTORME */
}

static void be_ut_helper_init_once(void)
{
	int rc;

	rc = pthread_once(&be_ut_helper.buh_once_control, &be_ut_helper_init);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void *m0_be_ut_seg_allocate_addr(m0_bcount_t size)
{
	struct be_ut_helper_struct *h = &be_ut_helper;
	void *addr;

	be_ut_helper_init_once();

	size = m0_align(size, m0_pagesize_get());

	m0_mutex_lock(&h->buh_seg_lock);
	addr	     = h->buh_addr;
	h->buh_addr += size;
	m0_mutex_unlock(&h->buh_seg_lock);

	return addr;
}

M0_INTERNAL uint64_t m0_be_ut_seg_allocate_id(void)
{
	struct be_ut_helper_struct *h = &be_ut_helper;
	uint64_t		    id;

	be_ut_helper_init_once();

	m0_mutex_lock(&h->buh_seg_lock);
	id	     = h->buh_id++;
	m0_mutex_unlock(&h->buh_seg_lock);

	return id;
}

/*
 * XXX m0_be_ut_reqh_create() function shall be removed after m0_reqh is
 * modularized (i.e., initialised/finalised using m0_module API).
 */
M0_INTERNAL void m0_be_ut_reqh_create(struct m0_reqh **pptr)
{
	struct be_ut_helper_struct *h   = &be_ut_helper;
	struct m0_fid               fid = M0_FID_TINIT(
					   M0_CONF_PROCESS_TYPE.cot_ftype.ft_id,
					   0, 1);
	int                         rc;

	M0_PRE(*pptr == NULL && h->buh_reqh == NULL);

	be_ut_helper_init_once();

	M0_ALLOC_PTR(*pptr);
	/*
	 * We don't bother with error handling here, because be_ut_helper
	 * is a kludge and should be removed.
	 */
	M0_ASSERT(*pptr != NULL);
	rc = M0_REQH_INIT(*pptr, .rhia_fid = &fid);
	M0_ASSERT(rc == 0);
	/*
	 * Remember the address of allocated pointer, so that it can be
	 * freed in m0_be_ut_reqh_destroy().
	 */
	h->buh_reqh = pptr;
}

M0_INTERNAL void m0_be_ut_reqh_destroy(void)
{
	struct be_ut_helper_struct *h = &be_ut_helper;

	if (h->buh_reqh != NULL) {
		m0_reqh_fini(*h->buh_reqh);
		m0_free0(h->buh_reqh);
		h->buh_reqh = NULL;
	}
}

static pid_t gettid_impl(void)
{
	return syscall(SYS_gettid);
}

static void be_ut_sm_group_thread_func(struct m0_be_ut_sm_group_thread *sgt)
{
	struct m0_sm_group *grp = &sgt->sgt_grp;
	bool                need_to_up_asts_run_sem;

	while (!m0_semaphore_trydown(&sgt->sgt_stop_sem)) {
		m0_chan_wait(&grp->s_clink);
		m0_sm_group_lock(grp);
		need_to_up_asts_run_sem =
			m0_semaphore_trydown(&sgt->sgt_asts_run_sem);
		m0_sm_asts_run(grp);
		if (need_to_up_asts_run_sem)
			m0_semaphore_up(&sgt->sgt_asts_run_sem);
		m0_sm_group_unlock(grp);
	}
}

static int m0_be_ut_sm_group_thread_init(struct m0_be_ut_sm_group_thread **sgtp,
					 bool lock_new)
{
	struct m0_be_ut_sm_group_thread *sgt;
	int				 rc;

	M0_ALLOC_PTR(*sgtp);
	sgt = *sgtp;
	if (sgt != NULL) {
		sgt->sgt_tid = gettid_impl();
		sgt->sgt_lock_new = lock_new;

		m0_sm_group_init(&sgt->sgt_grp);
		m0_semaphore_init(&sgt->sgt_stop_sem, 0);
		m0_semaphore_init(&sgt->sgt_asts_run_sem, 0);
		rc = M0_THREAD_INIT(&sgt->sgt_thread,
				    struct m0_be_ut_sm_group_thread *, NULL,
				    &be_ut_sm_group_thread_func, sgt,
				    "be_ut sgt");
		if (rc == 0) {
			if (sgt->sgt_lock_new)
				m0_sm_group_lock(&sgt->sgt_grp);
		} else {
			m0_semaphore_fini(&sgt->sgt_stop_sem);
			m0_sm_group_fini(&sgt->sgt_grp);
			m0_free(sgt);
		}
	} else {
		rc = -ENOMEM;
	}
	return rc;
}

static void m0_be_ut_sm_group_thread_fini(struct m0_be_ut_sm_group_thread *sgt)
{
	int rc;

	m0_semaphore_up(&sgt->sgt_stop_sem);

	m0_clink_signal(&sgt->sgt_grp.s_clink);
	if (sgt->sgt_lock_new)
		m0_sm_group_unlock(&sgt->sgt_grp);

	rc = m0_thread_join(&sgt->sgt_thread);
	M0_ASSERT(rc == 0);
	m0_thread_fini(&sgt->sgt_thread);

	m0_semaphore_fini(&sgt->sgt_asts_run_sem);
	m0_semaphore_fini(&sgt->sgt_stop_sem);
	m0_sm_group_fini(&sgt->sgt_grp);
	m0_free(sgt);
}

#define M0_BE_LOG_NAME  "M0_BE:LOG"
#define M0_BE_SEG0_NAME "M0_BE:SEG0"
#define M0_BE_SEG_NAME  "M0_BE:SEG%08lu"

void m0_be_ut_backend_cfg_default(struct m0_be_domain_cfg *cfg)
{
	extern struct m0_be_0type m0_stob_ad_0type;
	extern struct m0_be_0type m0_be_cob0;
	extern struct m0_be_0type m0_be_active_record0;

	static struct m0_atomic64        dom_key = { .a_value = 0xbef11e };
	static const struct m0_be_0type *zts[] = {
		&m0_stob_ad_0type,
		&m0_be_cob0,
		&m0_be_active_record0,
	};
	struct m0_reqh *reqh = cfg->bc_engine.bec_reqh;

	*cfg = (struct m0_be_domain_cfg) {
	    .bc_engine = {
		.bec_group_nr		  = 2,
		.bec_group_cfg = {
			.tgc_tx_nr_max	  = 128,
			.tgc_seg_nr_max	  = 256,
			.tgc_size_max	 = M0_BE_TX_CREDIT(1 << 18, 44UL << 20),
			.tgc_payload_max  = 1 << 24,
		},
		.bec_tx_size_max	 = M0_BE_TX_CREDIT(1 << 18, 44UL << 20),
		.bec_tx_payload_max	  = 1 << 21,
		.bec_group_freeze_timeout_min =  1 * M0_TIME_ONE_MSEC,
		.bec_group_freeze_timeout_max = 50 * M0_TIME_ONE_MSEC,
		.bec_reqh		  = reqh,
		.bec_wait_for_recovery	  = true,
	    },
		.bc_log = {
			.lc_store_cfg = {
				.lsc_stob_id = {
					/*
					 * .si_domain_fid is set by the log
					 * store
					 */
					.si_fid = {
					    .f_container = 0,
					    .f_key       = BE_UT_LOG_ID,
					}
				},
				.lsc_stob_domain_key =
					m0_atomic64_add_return(&dom_key, 1),
				.lsc_size = 1 << 27,
				.lsc_stob_create_cfg = NULL,
				.lsc_stob_dont_zero = true,
				.lsc_rbuf_nr = 3,
				/* other fields are filled by domain and log */
			},
			.lc_sched_cfg = {
				.lsch_io_sched_cfg = {
				},
			},
			.lc_full_threshold = 20 * (1 << 20),
			/* other fields are filled by the domain */
		},
		.bc_0types                 = zts,
		.bc_0types_nr              = ARRAY_SIZE(zts),
		.bc_stob_domain_location   = "linuxstob:./be_segments",
		.bc_stob_domain_cfg_init   = NULL,
		.bc_seg0_stob_key	   = BE_UT_SEG_START_ID - 1,
		.bc_mkfs_mode		   = false,
		.bc_stob_domain_cfg_create = NULL,
		.bc_stob_domain_key	= m0_atomic64_add_return(&dom_key, 1),
		.bc_seg0_cfg = {
			.bsc_stob_key	     = BE_UT_SEG_START_ID - 1,
			.bsc_size	     = 1 << 20,
			.bsc_preallocate     = false,
			.bsc_addr	  = m0_be_ut_seg_allocate_addr(1 << 20),
			.bsc_stob_create_cfg = NULL,
		},
		.bc_seg_cfg		   = NULL,
		.bc_seg_nr		   = 0,
		.bc_pd_cfg = {
			.bpdc_seg_io_nr = 0x2,
		},
		.bc_log_discard_cfg = {
			.ldsc_items_max         = 0x100,
			.ldsc_items_threshold   = 0x80,
			.ldsc_loc               = m0_locality0_get(),
			.ldsc_sync_timeout      = M0_TIME_ONE_SECOND * 5ULL,
		},
		.bc_zone_pcnt = { [M0_BAP_NORMAL] = 100 }
	};
}

M0_INTERNAL int m0_be_ut_backend_init_cfg(struct m0_be_ut_backend *ut_be,
					  const struct m0_be_domain_cfg *cfg,
					  bool mkfs)
{
	static bool mkfs_executed = false;
	struct m0_be_domain_cfg *c;
	int                      rc;

	m0_bob_init(&m0_ut_be_backend_bobtype, ut_be);
	if (!mkfs_executed && cfg == NULL &&
	    ut_be->but_stob_domain_location == NULL)
		mkfs = mkfs_executed = true;

	ut_be->but_sm_groups_unlocked = false;
	m0_mutex_init(&ut_be->but_sgt_lock);

	if (cfg == NULL)
		m0_be_ut_backend_cfg_default(&ut_be->but_dom_cfg);
	else
		/*
		 * Make a copy of `cfg': it can be an automatic variable,
		 * which scope we should not rely on.
		 */
		ut_be->but_dom_cfg = *cfg;
	c = &ut_be->but_dom_cfg;

	/* Create reqh, if necessary. */
	if (c->bc_engine.bec_reqh == NULL)
		m0_be_ut_reqh_create(&c->bc_engine.bec_reqh);

	/* Use m0_be_ut_backend's stob domain location, if possible. */
	if (ut_be->but_stob_domain_location != NULL)
		c->bc_stob_domain_location = ut_be->but_stob_domain_location;

	c->bc_mkfs_mode = mkfs;

check_mkfs:
	m0_be_domain_module_setup(&ut_be->but_dom, c);
	rc = m0_module_init(&ut_be->but_dom.bd_module,
			    M0_BE_DOMAIN_LEVEL_READY);
	if (!c->bc_mkfs_mode && rc == -ENOENT) {
		m0_module_fini(&ut_be->but_dom.bd_module, M0_MODLEV_NONE);
		M0_SET0(&ut_be->but_dom);
		c->bc_mkfs_mode = true;
		goto check_mkfs;
	}
	if (rc != 0)
		m0_mutex_fini(&ut_be->but_sgt_lock);

	return rc;
}

void m0_be_ut_backend_init(struct m0_be_ut_backend *ut_be)
{
	int rc = m0_be_ut_backend_init_cfg(ut_be, NULL, true);
	M0_ASSERT(rc == 0);
}

void m0_be_ut_backend_fini(struct m0_be_ut_backend *ut_be)
{
	m0_forall(i, ut_be->but_sgt_size,
		  m0_be_ut_sm_group_thread_fini(ut_be->but_sgt[i]), true);
	m0_free(ut_be->but_sgt);
	m0_module_fini(&ut_be->but_dom.bd_module, M0_MODLEV_NONE);
	m0_mutex_fini(&ut_be->but_sgt_lock);
	m0_be_ut_reqh_destroy();
	m0_bob_fini(&m0_ut_be_backend_bobtype, ut_be);
}

M0_INTERNAL void
m0_be_ut_backend_seg_add(struct m0_be_ut_backend	   *ut_be,
			 const struct m0_be_0type_seg_cfg  *seg_cfg,
			 struct m0_be_seg		  **out)
{
	int rc;

	rc = m0_be_domain_seg_create(&ut_be->but_dom, NULL, seg_cfg, out);
	M0_ASSERT(rc == 0);
}

M0_INTERNAL void
m0_be_ut_backend_seg_add2(struct m0_be_ut_backend	   *ut_be,
			  m0_bcount_t			    size,
			  bool				    preallocate,
			  const char			   *stob_create_cfg,
			  struct m0_be_seg		  **out)
{
	struct m0_be_0type_seg_cfg seg_cfg = {
		.bsc_stob_key	     = m0_be_ut_seg_allocate_id(),
		.bsc_size	     = size,
		.bsc_preallocate     = preallocate,
		.bsc_addr	     = m0_be_ut_seg_allocate_addr(size),
		.bsc_stob_create_cfg = stob_create_cfg,
	};
	m0_be_ut_backend_seg_add(ut_be, &seg_cfg, out);
}

M0_INTERNAL void
m0_be_ut_backend_seg_del(struct m0_be_ut_backend	   *ut_be,
			 struct m0_be_seg		   *seg)
{
	struct m0_be_tx_credit	cred = {};
	struct m0_be_domain    *dom = &ut_be->but_dom;
	struct m0_sm_group     *grp = m0_be_ut_backend_sm_group_lookup(ut_be);
	struct m0_be_tx		tx = {};
	int			rc;

	m0_be_ut_tx_init(&tx, ut_be);
	if (ut_be->but_sm_groups_unlocked)
		m0_sm_group_lock(grp);
	m0_be_domain_seg_destroy_credit(dom, seg, &cred);
	m0_be_tx_prep(&tx, &cred);
	rc = m0_be_tx_exclusive_open_sync(&tx);
	M0_ASSERT(rc == 0);
	rc = m0_be_domain_seg_destroy(dom, &tx, seg);
	M0_ASSERT(rc == 0);
	m0_be_tx_close_sync(&tx);
	m0_be_tx_fini(&tx);
	if (ut_be->but_sm_groups_unlocked)
		m0_sm_group_unlock(grp);
}

static void be_ut_sm_group_thread_add(struct m0_be_ut_backend *ut_be,
				      struct m0_be_ut_sm_group_thread *sgt)
{
	struct m0_be_ut_sm_group_thread **sgt_arr;
	size_t				  size = ut_be->but_sgt_size;

	M0_ALLOC_ARR(sgt_arr, size + 1);
	M0_ASSERT(sgt_arr != NULL);

	m0_forall(i, size, sgt_arr[i] = ut_be->but_sgt[i], true);
	sgt_arr[size] = sgt;

	m0_free(ut_be->but_sgt);
	ut_be->but_sgt = sgt_arr;
	++ut_be->but_sgt_size;
}

static size_t be_ut_backend_sm_group_find(struct m0_be_ut_backend *ut_be)
{
	size_t i;
	pid_t  tid = gettid_impl();

	for (i = 0; i < ut_be->but_sgt_size; ++i) {
		if (ut_be->but_sgt[i]->sgt_tid == tid)
			break;
	}
	return i;
}

static struct m0_sm_group *
be_ut_backend_sm_group_lookup(struct m0_be_ut_backend *ut_be, bool lock_new)
{
	struct m0_be_ut_sm_group_thread *sgt;
	struct m0_sm_group              *grp;
	pid_t                            tid = gettid_impl();
	unsigned                         i;
	int                              rc;

	m0_mutex_lock(&ut_be->but_sgt_lock);
	grp = NULL;
	for (i = 0; i < ut_be->but_sgt_size; ++i) {
		sgt = ut_be->but_sgt[i];
		if (sgt->sgt_tid == tid) {
			grp = &sgt->sgt_grp;
			break;
		}
	}
	if (grp == NULL) {
		rc = m0_be_ut_sm_group_thread_init(&sgt, lock_new);
		M0_ASSERT(rc == 0);
		be_ut_sm_group_thread_add(ut_be, sgt);
		grp = &sgt->sgt_grp;
	}
	m0_mutex_unlock(&ut_be->but_sgt_lock);
	M0_POST(grp != NULL);
	return grp;
}

struct m0_sm_group *
m0_be_ut_backend_sm_group_lookup(struct m0_be_ut_backend *ut_be)
{
	return be_ut_backend_sm_group_lookup(ut_be,
					     !ut_be->but_sm_groups_unlocked);
}

void m0_be_ut_backend_sm_group_asts_run(struct m0_be_ut_backend *ut_be)
{
	struct m0_be_ut_sm_group_thread *sgt;
	struct m0_sm_group              *grp;

	grp = m0_be_ut_backend_sm_group_lookup(ut_be);
	sgt = container_of(grp, struct m0_be_ut_sm_group_thread, sgt_grp);
	m0_semaphore_up(&sgt->sgt_asts_run_sem);
	m0_clink_signal(&sgt->sgt_grp.s_clink);
	m0_semaphore_down(&sgt->sgt_asts_run_sem);
}

void m0_be_ut_backend_new_grp_lock_state_set(struct m0_be_ut_backend *ut_be,
					     bool unlocked_new)
{
	ut_be->but_sm_groups_unlocked = unlocked_new;
}

void m0_be_ut_backend_thread_exit(struct m0_be_ut_backend *ut_be)
{
	size_t index;
	size_t i;

	m0_mutex_lock(&ut_be->but_sgt_lock);
	index = be_ut_backend_sm_group_find(ut_be);
	if (index != ut_be->but_sgt_size) {
		m0_be_ut_sm_group_thread_fini(ut_be->but_sgt[index]);
		for (i = index + 1; i < ut_be->but_sgt_size; ++i)
			ut_be->but_sgt[i - 1] = ut_be->but_sgt[i];
		--ut_be->but_sgt_size;
	}
	m0_mutex_unlock(&ut_be->but_sgt_lock);
}

static void be_ut_tx_lock_if(struct m0_sm_group *grp,
		      struct m0_be_ut_backend *ut_be)
{
	if (ut_be->but_sm_groups_unlocked)
		m0_sm_group_lock(grp);
}

static void be_ut_tx_unlock_if(struct m0_sm_group *grp,
			struct m0_be_ut_backend *ut_be)
{
	if (ut_be->but_sm_groups_unlocked)
		m0_sm_group_unlock(grp);
}

void m0_be_ut_tx_init(struct m0_be_tx *tx, struct m0_be_ut_backend *ut_be)
{
	struct m0_sm_group *grp = m0_be_ut_backend_sm_group_lookup(ut_be);

	M0_SET0(tx);
	be_ut_tx_lock_if(grp, ut_be);
	m0_be_tx_init(tx, 0, &ut_be->but_dom, grp, NULL, NULL, NULL, NULL);
	be_ut_tx_unlock_if(grp, ut_be);
}

void m0_be_ut_seg_init(struct m0_be_ut_seg *ut_seg,
		       struct m0_be_ut_backend *ut_be,
		       m0_bcount_t size)
{
	struct m0_be_0type_seg_cfg seg_cfg;
	int			   rc;

	if (ut_be == NULL) {
		M0_ALLOC_PTR(ut_seg->bus_seg);
		M0_ASSERT(ut_seg->bus_seg != NULL);
		m0_be_seg_init(ut_seg->bus_seg, m0_ut_stob_linux_get(),
			       &ut_be->but_dom, M0_BE_SEG_FAKE_ID);
		rc = m0_be_seg_create(ut_seg->bus_seg, size,
				      m0_be_ut_seg_allocate_addr(size));
		M0_ASSERT(rc == 0);
		rc = m0_be_seg_open(ut_seg->bus_seg);
		M0_ASSERT(rc == 0);
	} else {
		seg_cfg = (struct m0_be_0type_seg_cfg){
			.bsc_stob_key	 = m0_be_ut_seg_allocate_id(),
			.bsc_size	 = size,
			.bsc_preallocate = false,
			.bsc_addr	 = m0_be_ut_seg_allocate_addr(size),
			.bsc_stob_create_cfg = NULL,
		};
		m0_be_ut_backend_seg_add(ut_be, &seg_cfg, &ut_seg->bus_seg);
	}

	ut_seg->bus_backend = ut_be;
}

void m0_be_ut_seg_fini(struct m0_be_ut_seg *ut_seg)
{
	struct m0_stob *stob = ut_seg->bus_seg->bs_stob;
	int		rc;

	if (ut_seg->bus_backend == NULL) {
		m0_be_seg_close(ut_seg->bus_seg);
		rc = m0_be_seg_destroy(ut_seg->bus_seg);
		M0_ASSERT(rc == 0);
		m0_be_seg_fini(ut_seg->bus_seg);
		m0_free(ut_seg->bus_seg);

		m0_ut_stob_put(stob, false);
	} else {
		m0_be_ut_backend_seg_del(ut_seg->bus_backend, ut_seg->bus_seg);
	}
}

M0_INTERNAL void m0_be_ut_alloc(struct m0_be_ut_backend *ut_be,
				struct m0_be_ut_seg *ut_seg,
				void **ptr,
				m0_bcount_t size)
{
	struct m0_be_allocator *a = m0_be_seg_allocator(ut_seg->bus_seg);

	M0_BE_UT_TRANSACT(ut_be, tx, cred,
		  m0_be_allocator_credit(a, M0_BAO_ALLOC, size, 0, &cred),
		  M0_BE_OP_SYNC(op, m0_be_alloc(a, tx, &op, ptr, size)));
	M0_ASSERT(*ptr != NULL);
}

M0_INTERNAL void m0_be_ut_free(struct m0_be_ut_backend *ut_be,
			       struct m0_be_ut_seg *ut_seg,
			       void *ptr)
{
	struct m0_be_allocator *a = m0_be_seg_allocator(ut_seg->bus_seg);

	M0_BE_UT_TRANSACT(ut_be, tx, cred,
			  m0_be_allocator_credit(a, M0_BAO_FREE, 0, 0, &cred),
			  M0_BE_OP_SYNC(op, m0_be_free(a, tx, &op, ptr)));
}

void m0_be_ut_seg_reload(struct m0_be_ut_seg *ut_seg)
{
	m0_be_seg_close(ut_seg->bus_seg);
	m0_be_seg_open(ut_seg->bus_seg);
}

static void be_ut_seg_allocator_initfini(struct m0_be_seg *seg,
					 struct m0_be_ut_backend *ut_be,
					 bool init)
{
	struct m0_be_tx_credit	credit = {};
	struct m0_be_allocator *a;
	struct m0_be_tx         tx;
	uint32_t                percent[M0_BAP_NR] = { [M0_BAP_NORMAL] = 100 };
	int                     rc;

	a = m0_be_seg_allocator(seg);

	if (ut_be != NULL) {
		m0_be_ut_tx_init(&tx, ut_be);
		be_ut_tx_lock_if(tx.t_sm.sm_grp, ut_be);
		m0_be_allocator_credit(a, init ? M0_BAO_CREATE : M0_BAO_DESTROY,
				       0, 0, &credit);
		m0_be_tx_prep(&tx, &credit);
		rc = m0_be_tx_open_sync(&tx);
		M0_ASSERT(rc == 0);
	}

	if (init) {
		rc = m0_be_allocator_init(a, seg);
		M0_ASSERT(rc == 0);
		if (M0_FI_ENABLED("repair_zone_50")) {
			percent[M0_BAP_REPAIR] = 50;
			percent[M0_BAP_NORMAL] = 50;
		}
		rc = m0_be_allocator_create(a, ut_be == NULL ? NULL : &tx,
					    percent, ARRAY_SIZE(percent));
		M0_ASSERT(rc == 0);
	} else {
		m0_be_allocator_destroy(a, ut_be == NULL ? NULL : &tx);
		m0_be_allocator_fini(a);
	}

	if (ut_be != NULL) {
		m0_be_tx_close_sync(&tx);
		m0_be_tx_fini(&tx);
		be_ut_tx_unlock_if(tx.t_sm.sm_grp, ut_be);
	}
}

void m0_be_ut_seg_allocator_init(struct m0_be_ut_seg *ut_seg,
				 struct m0_be_ut_backend *ut_be)
{
	be_ut_seg_allocator_initfini(ut_seg->bus_seg, ut_be, true);
}

void m0_be_ut_seg_allocator_fini(struct m0_be_ut_seg *ut_seg,
				 struct m0_be_ut_backend *ut_be)
{
	be_ut_seg_allocator_initfini(ut_seg->bus_seg, ut_be, false);
}

#undef M0_TRACE_SUBSYSTEM

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
