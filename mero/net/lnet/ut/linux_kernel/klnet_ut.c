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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>
 *                  Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 1/9/2012
 */

/*
 * Kernel specific LNet unit tests.
 * The tests cases are loaded from the address space agnostic ../lnet_ut.c
 * file.
 */

#include <linux/version.h>        /* LINUX_VERSION_CODE */
#include <linux/module.h>         /* THIS_MODULE */
#include <linux/proc_fs.h>

#include "net/lnet/ut/lnet_drv_ut.h"
#include "net/lnet/linux_kernel/klnet_core.h"

enum {
	UT_PROC_WRITE_SIZE = 8,   /**< max size of data to write to proc file */
	UT_SYNC_DELAY_SEC = 5,    /**< delay for user program to sync */
};

static struct proc_dir_entry *proc_lnet_ut;

static struct m0_mutex ktest_mutex;
static struct m0_cond ktest_cond;
static struct m0_semaphore ktest_sem;
static int ktest_id;
static bool ktest_user_failed;
static bool ktest_done;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static ssize_t read_lnet_ut(struct file *file, char __user *buffer, size_t len,
			    loff_t *offset)
{
	char code;
	int  rc;
	M0_THREAD_ENTER;

	m0_semaphore_down(&ktest_sem);

	m0_mutex_lock(&ktest_mutex);
	if (ktest_user_failed)
		code = UT_TEST_DONE;
	else
		code = ktest_id;

	/* main thread will wait for user to read 1 DONE value */
	if (code == UT_TEST_DONE) {
		ktest_done = true;
		m0_cond_signal(&ktest_cond);
	}
	m0_mutex_unlock(&ktest_mutex);

	rc = copy_to_user(buffer, &code, sizeof code);
	if (rc != 0)
		return -EFAULT;
	return sizeof code;
}
#else
static int read_lnet_ut(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	M0_THREAD_ENTER;
	m0_semaphore_down(&ktest_sem);

	/* page[PAGE_SIZE] and simpleminded proc file */
	m0_mutex_lock(&ktest_mutex);
	if (ktest_user_failed)
		*page = UT_TEST_DONE;
	else
		*page = ktest_id;
	/* main thread will wait for user to read 1 DONE value */
	if (*page == UT_TEST_DONE) {
		ktest_done = true;
		*eof = 1;
		m0_cond_signal(&ktest_cond);
	}
	m0_mutex_unlock(&ktest_mutex);
	return 1;
}
#endif

/**
   Synchronize with user space program, updates ktest_id and signals main UT
   thread about each transition.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static ssize_t write_lnet_ut(struct file *file, const char __user *buffer,
			     size_t count, loff_t *offset)
#else
static int write_lnet_ut(struct file *file, const char __user *buffer,
			 unsigned long count, void *data)
#endif
{
	char buf[UT_PROC_WRITE_SIZE];
	M0_THREAD_ENTER;

	if (count >= UT_PROC_WRITE_SIZE) {
		printk("%s: writing wrong size %ld to proc file, max %d\n",
		       __func__, count, UT_PROC_WRITE_SIZE - 1);
		return -EINVAL;
	}
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	m0_mutex_lock(&ktest_mutex);
	switch (*buf) {
	case UT_USER_READY:
		if (ktest_id != UT_TEST_NONE) {
			ktest_user_failed = true;
			count = -EINVAL;
		} else
			ktest_id = UT_TEST_DEV;
		break;
	case UT_USER_SUCCESS:
		/* test passed */
		if (ktest_id == UT_TEST_NONE) {
			ktest_user_failed = true;
			count = -EINVAL;
		} else if (ktest_id == UT_TEST_MAX)
			ktest_id = UT_TEST_DONE;
		else
			++ktest_id;
		break;
	case UT_USER_FAIL:
		/* test failed */
		if (ktest_id == UT_TEST_NONE)
			count = -EINVAL;
		ktest_user_failed = true;
		break;
	default:
		printk("%s: unknown user test state: %02x\n", __func__, *buf);
		count = -EINVAL;
	}
	m0_cond_signal(&ktest_cond);
	m0_mutex_unlock(&ktest_mutex);
	return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int open_lnet_ut(struct inode *inode, struct file *file)
{
	return 0;
}

static int close_lnet_ut(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations proc_lnet_fops = {
	.owner    = THIS_MODULE,
	.open     = open_lnet_ut,
	.release  = close_lnet_ut,
	.read     = read_lnet_ut,
	.write    = write_lnet_ut,
	.llseek   = default_llseek,
};
#endif

static int ktest_lnet_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	proc_lnet_ut = proc_create(UT_PROC_NAME, 0644, NULL, &proc_lnet_fops);
#else
	proc_lnet_ut = create_proc_entry(UT_PROC_NAME, 0644, NULL);
#endif
	if (proc_lnet_ut == NULL)
		return -ENOENT;

	m0_mutex_init(&ktest_mutex);
	m0_cond_init(&ktest_cond, &ktest_mutex);
	m0_semaphore_init(&ktest_sem, 0);
	ktest_id = UT_TEST_NONE;
	ktest_user_failed = false;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	proc_lnet_ut->read_proc  = read_lnet_ut;
	proc_lnet_ut->write_proc = write_lnet_ut;
#endif
	return 0;
}

static void ktest_lnet_fini(void)
{
	M0_ASSERT(proc_lnet_ut != NULL);
	remove_proc_entry(UT_PROC_NAME, NULL);
	m0_semaphore_fini(&ktest_sem);
	m0_cond_fini(&ktest_cond);
	m0_mutex_fini(&ktest_mutex);
	proc_lnet_ut = NULL;
}

static bool ut_bufvec_alloc(struct m0_bufvec *bv, size_t n)
{
	M0_ALLOC_ARR(bv->ov_vec.v_count, n);
	M0_ALLOC_ARR(bv->ov_buf, n);
	if (bv->ov_vec.v_count == 0 || bv->ov_buf == NULL) {
		m0_free(bv->ov_vec.v_count);
		return false;
	}
	bv->ov_vec.v_nr = n;
	return true;
}

#define UT_BUFVEC_ALLOC(v,n)	\
if (!ut_bufvec_alloc(&v,n)) {	\
	M0_IMPOSSIBLE("no memory");\
	return;			\
}

#define UT_BUFVEC_FREE(v)				\
	m0_free(v.ov_vec.v_count);			\
	m0_free(v.ov_buf)

static void ktest_buf_shape(void)
{
	struct m0_net_domain dom1;
	struct m0_bufvec bv1;
	void *base;
	unsigned num_pages;

	M0_SET0(&dom1);
	M0_UT_ASSERT(!m0_net_domain_init(&dom1, &m0_net_lnet_xprt));

	/* buffer shape APIs */
	M0_UT_ASSERT(m0_net_domain_get_max_buffer_size(&dom1)
		     == LNET_MAX_PAYLOAD);
	M0_UT_ASSERT(m0_net_domain_get_max_buffer_segment_size(&dom1)
		     == PAGE_SIZE);
	M0_UT_ASSERT(m0_net_domain_get_max_buffer_segments(&dom1)
		     == LNET_MAX_IOV);

	/* test the segment page count computation */
	UT_BUFVEC_ALLOC(bv1, 1);
	base = (void *)((uint64_t)&base & PAGE_MASK);/* arbitrary, pg aligned */

#define EXP_SEG_COUNT(ptr,segsize,expcount)		\
	bv1.ov_buf[0] = (ptr);				\
	bv1.ov_vec.v_count[0] = (segsize);		\
	num_pages = bufvec_seg_page_count(&bv1, 0);	\
	M0_UT_ASSERT(num_pages == (expcount))

	EXP_SEG_COUNT(base,             PAGE_SIZE,     1);/* pg aligned, 1 pg */
	EXP_SEG_COUNT(base,             PAGE_SIZE+1,   2);/* pg aligned,>1 pg */
	EXP_SEG_COUNT(base,             PAGE_SIZE-1,   1);/* pg aligned,<1 pg */
	EXP_SEG_COUNT(base,             2*PAGE_SIZE,   2);/* pg aligned, 2 pg */
	EXP_SEG_COUNT(base+PAGE_SIZE/2, 2*PAGE_SIZE,   3);/* mid-pg,  2 pg */
	EXP_SEG_COUNT(base+PAGE_SIZE/2, PAGE_SIZE,     2);/* mid-pg,  1 pg */
	EXP_SEG_COUNT(base+PAGE_SIZE/2, PAGE_SIZE/2+1, 2);/* mid-pg, >0.5 pg */
	EXP_SEG_COUNT(base+PAGE_SIZE/2, PAGE_SIZE/2,   1);/* mid-pg,  0.5 pg */
	EXP_SEG_COUNT(base+PAGE_SIZE/2, PAGE_SIZE/2-1, 1);/* mid-pg, <0.5 pg */

#undef EXP_SEG_COUNT

	/* fini */
	UT_BUFVEC_FREE(bv1);
	m0_net_domain_fini(&dom1);
}

static void ktest_buf_reg(void)
{
	struct m0_net_domain dom1;
	struct m0_net_buffer nb1;
	struct m0_net_buffer nb3;
	m0_bcount_t bsize;
	m0_bcount_t bsegsize;
	int32_t     bsegs;
	struct nlx_xo_buffer *xb;
	struct nlx_core_buffer *cb;
	struct nlx_kcore_buffer *kcb1;
	int i;
	struct m0_bufvec *v1;
	struct m0_bufvec *v3;
	m0_bcount_t thunk;
	struct nlx_core_domain *cd;
	struct nlx_xo_domain *dp;
	struct nlx_kcore_domain *kd;

	M0_SET0(&dom1);
	M0_SET0(&nb1);
	M0_SET0(&nb3);

	M0_UT_ASSERT(!m0_net_domain_init(&dom1, &m0_net_lnet_xprt));

	/* TEST
	   Register a network buffer of maximal size and perfectly aligned on
	   page boundaries.
	 */
	bsize    = LNET_MAX_PAYLOAD;
	bsegsize = PAGE_SIZE;
	bsegs    = LNET_MAX_IOV;
	/* Allocate a bufvec into the buffer. */
	M0_UT_ASSERT(m0_bufvec_alloc(&nb1.nb_buffer, bsegs, bsegsize) == 0);
	M0_UT_ASSERT(m0_vec_count(&nb1.nb_buffer.ov_vec) == bsize);

	/* register the buffer */
	nb1.nb_flags = 0;
	M0_UT_ASSERT(!m0_net_buffer_register(&nb1, &dom1));
	M0_UT_ASSERT(nb1.nb_flags & M0_NET_BUF_REGISTERED);

	/* check the kcore data structure */
	xb = nb1.nb_xprt_private;
	cb = &xb->xb_core;
	M0_UT_ASSERT(cb->cb_magic == M0_NET_LNET_CORE_BUF_MAGIC);
	kcb1 = cb->cb_kpvt;
	M0_UT_ASSERT(kcb1->kb_magic == M0_NET_LNET_KCORE_BUF_MAGIC);
	M0_UT_ASSERT(LNetMDHandleIsInvalid(kcb1->kb_mdh));
	M0_UT_ASSERT(kcb1->kb_kiov != NULL);
	M0_UT_ASSERT(kcb1->kb_kiov_len == bsegs);
	for (i = 0; i < kcb1->kb_kiov_len; ++i) {
		void *addr;
		M0_UT_ASSERT(kcb1->kb_kiov[i].kiov_len == bsegsize);
		M0_UT_ASSERT(kcb1->kb_kiov[i].kiov_offset == 0);
		addr = page_address(kcb1->kb_kiov[i].kiov_page);
		M0_UT_ASSERT(addr == nb1.nb_buffer.ov_buf[i]);
	}
	dp = dom1.nd_xprt_private;
	cd = &dp->xd_core;
	kd = cd->cd_kpvt;
	M0_ASSERT(nlx_kcore_domain_invariant(kd));
	v1 = &nb1.nb_buffer;
	/* TEST
	   Provide a buffer whose m0_bufvec shape is legal, but whose kiov will
	   exceed the internal limits.
	   Use the same allocated memory segments from the first network buffer.
	 */
	UT_BUFVEC_ALLOC(nb3.nb_buffer, bsegs);
	v3 = &nb3.nb_buffer;
	thunk = PAGE_SIZE;
	for (i = 0; i < v3->ov_vec.v_nr; ++i) {
		/* each segment spans 2 pages */
		v3->ov_vec.v_count[i] = thunk;
		v3->ov_buf[i] = v1->ov_buf[i] + PAGE_SIZE - 1;
	}

	/* register the buffer */
	nb3.nb_flags = 0;
	i = m0_net_buffer_register(&nb3, &dom1);
	M0_UT_ASSERT(i == -EFBIG);

	/* fini */
	m0_net_buffer_deregister(&nb1, &dom1);
	M0_UT_ASSERT(!(nb1.nb_flags & M0_NET_BUF_REGISTERED));
	M0_UT_ASSERT(nb1.nb_xprt_private == NULL);


	UT_BUFVEC_FREE(nb3.nb_buffer); /* just vector, not segs */
	m0_bufvec_free(&nb1.nb_buffer);
	m0_net_domain_fini(&dom1);
}

static void ktest_core_ep_addr(void)
{
	struct nlx_xo_domain dom;
	struct nlx_core_ep_addr tmaddr;
	const char *epstr[] = {
		"127.0.0.1@tcp:12345:30:10",
		"127.0.0.1@tcp:12345:30:*",
		"4.4.4.4@tcp:42:29:28"
	};
	const char *failepstr[] = {
		"notip@tcp:12345:30:10",
		"notnid:12345:30:10",
		"127.0.0.1@tcp:notpid:30:10",
		"127.0.0.1@tcp:12:notportal:10",
		"127.0.0.1@tcp:12:30:nottm",
		"127.0.0.1@tcp:12:30:-10",        /* positive required */
		"127.0.0.1@tcp:12:30:4096",       /* in range */
	};
	const struct nlx_core_ep_addr ep_addr[] = {
		{
			.cepa_pid = 12345,
			.cepa_portal = 30,
			.cepa_tmid = 10,
		},
		{
			.cepa_pid = 12345,
			.cepa_portal = 30,
			.cepa_tmid = M0_NET_LNET_TMID_INVALID,
		},
		{
			.cepa_pid = 42,
			.cepa_portal = 29,
			.cepa_tmid = 28,
		},
	};
	char buf[M0_NET_LNET_XEP_ADDR_LEN];
	char * const *nidstrs;
	int rc;
	int i;

	M0_UT_ASSERT(!nlx_core_nidstrs_get(&dom.xd_core, &nidstrs));
	M0_UT_ASSERT(nidstrs != NULL);
	for (i = 0; nidstrs[i] != NULL; ++i) {
		char *network;
		network = strchr(nidstrs[i], '@');
		if (network != NULL && strcmp(network, "@tcp") == 0)
			break;
	}

	/* whether we're using tcp network */
	if (nidstrs[i] != NULL) {
		M0_CASSERT(ARRAY_SIZE(epstr) == ARRAY_SIZE(ep_addr));
		for (i = 0; i < ARRAY_SIZE(epstr); ++i) {
			rc = nlx_core_ep_addr_decode(&dom.xd_core, epstr[i],
						     &tmaddr);
			M0_UT_ASSERT(rc == 0);
			M0_UT_ASSERT(ep_addr[i].cepa_pid == tmaddr.cepa_pid);
			M0_UT_ASSERT(ep_addr[i].cepa_portal ==
				     tmaddr.cepa_portal);
			M0_UT_ASSERT(ep_addr[i].cepa_tmid == tmaddr.cepa_tmid);
			nlx_core_ep_addr_encode(&dom.xd_core, &tmaddr, buf);
			M0_UT_ASSERT(strcmp(buf, epstr[i]) == 0);
		}
	}
	nlx_core_nidstrs_put(&dom.xd_core, &nidstrs);
	M0_UT_ASSERT(nidstrs == NULL);

	for (i = 0; i < ARRAY_SIZE(failepstr); ++i) {
		rc = nlx_core_ep_addr_decode(&dom.xd_core, failepstr[i],
					     &tmaddr);
		M0_UT_ASSERT(rc == -EINVAL);
	}
}

static void ktest_enc_dec(void)
{
	uint32_t tmid;
	uint32_t portal;
	struct nlx_core_transfer_mc ctm;
	struct nlx_kcore_transfer_mc ktm = {
		.ktm_magic = M0_NET_LNET_KCORE_TM_MAGIC, /* fake */
	};

	/* TEST
	   Check that hdr data decode reverses encode.
	 */
	nlx_core_kmem_loc_set(&ktm.ktm_ctm_loc, virt_to_page(&ctm),
			      NLX_PAGE_OFFSET((unsigned long) &ctm));
	M0_UT_ASSERT(nlx_kcore_tm_invariant(&ktm)); /* to make this pass */

#define TEST_HDR_DATA_ENCODE(_p, _t)					\
	ktm.ktm_addr.cepa_tmid = (_t);					\
	ktm.ktm_addr.cepa_portal = (_p);				\
	nlx_kcore_hdr_data_decode(nlx_kcore_hdr_data_encode(&ktm),	\
				  &portal, &tmid);			\
	M0_UT_ASSERT(portal == (_p));					\
	M0_UT_ASSERT(tmid == (_t))

	TEST_HDR_DATA_ENCODE(0,  0);
	TEST_HDR_DATA_ENCODE(30, 0);
	TEST_HDR_DATA_ENCODE(30, M0_NET_LNET_TMID_MAX);
	TEST_HDR_DATA_ENCODE(63, 0);
	TEST_HDR_DATA_ENCODE(63, M0_NET_LNET_TMID_MAX);

#undef TEST_HDR_DATA_ENCODE
}

/* ktest_msg */
enum {
	UT_KMSG_OPS  = 4,
};
static bool ut_ktest_msg_LNetMDAttach_called;
static int ut_ktest_msg_LNetMDAttach(struct nlx_kcore_transfer_mc *kctm,
				     struct nlx_core_buffer *lcbuf,
				     struct nlx_kcore_buffer *kcb,
				     lnet_md_t *umd)
{
	uint32_t tmid;
	uint64_t counter;

	ut_ktest_msg_LNetMDAttach_called = true;
	NLXDBG(kctm, 1, printk("intercepted LNetMDAttach\n"));
	NLXDBG(kctm, 1, nlx_kprint_lnet_md("ktest_msg", umd));

	M0_UT_ASSERT(umd->options & LNET_MD_KIOV);
	M0_UT_ASSERT(umd->start == kcb->kb_kiov);
	M0_UT_ASSERT(umd->length == kcb->kb_kiov_len);

	M0_UT_ASSERT(umd->threshold == UT_KMSG_OPS);

	M0_UT_ASSERT(umd->options & LNET_MD_MAX_SIZE);
	M0_UT_ASSERT(umd->max_size == UT_MSG_SIZE);

	M0_UT_ASSERT(umd->options & LNET_MD_OP_PUT);
	M0_UT_ASSERT(umd->user_ptr == kcb);
	M0_UT_ASSERT(LNetHandleIsEqual(umd->eq_handle, kctm->ktm_eqh));

	nlx_core_match_bits_decode(lcbuf->cb_match_bits, &tmid, &counter);
	M0_UT_ASSERT(tmid == kctm->ktm_addr.cepa_tmid);
	M0_UT_ASSERT(counter == 0);

	kcb->kb_ktm = kctm;

	return 0;
}

static bool ut_ktest_msg_LNetPut_called;
static struct m0_net_end_point *ut_ktest_msg_LNetPut_ep;
static int ut_ktest_msg_LNetPut(struct nlx_kcore_transfer_mc *kctm,
				struct nlx_core_buffer *lcbuf,
				struct nlx_kcore_buffer *kcb,
				lnet_md_t *umd)
{
	struct nlx_core_ep_addr *cepa;
	size_t len;
	unsigned last;

	ut_ktest_msg_LNetPut_called = true;
	NLXDBG(kctm, 1, printk("intercepted LNetPut\n"));
	NLXDBG(kctm, 1, nlx_kprint_lnet_md("ktest_msg", umd));

	M0_UT_ASSERT((lnet_kiov_t *) umd->start == kcb->kb_kiov);
	len = nlx_kcore_num_kiov_entries_for_bytes(kcb->kb_kiov,
						   kcb->kb_kiov_len,
						   lcbuf->cb_length,
						   &last);
	M0_UT_ASSERT(umd->length == len);
	M0_UT_ASSERT(umd->options & LNET_MD_KIOV);
	M0_UT_ASSERT(umd->threshold == 1);
	M0_UT_ASSERT(umd->user_ptr == kcb);
	M0_UT_ASSERT(umd->max_size == 0);
	M0_UT_ASSERT(!(umd->options & (LNET_MD_OP_PUT | LNET_MD_OP_GET)));
	M0_UT_ASSERT(LNetHandleIsEqual(umd->eq_handle, kctm->ktm_eqh));

	M0_UT_ASSERT(ut_ktest_msg_LNetPut_ep != NULL);
	cepa = nlx_ep_to_core(ut_ktest_msg_LNetPut_ep);
	M0_UT_ASSERT(nlx_core_ep_eq(cepa, &lcbuf->cb_addr));

	kcb->kb_ktm = kctm;

	return 0;
}

static struct m0_atomic64 ut_ktest_msg_buf_event_wait_stall;
static struct m0_chan *ut_ktest_msg_buf_event_wait_delay_chan;
int ut_ktest_msg_buf_event_wait(struct nlx_core_domain *lcdom,
				struct nlx_core_transfer_mc *lctm,
				m0_time_t timeout)
{
	if (m0_atomic64_get(&ut_ktest_msg_buf_event_wait_stall) > 0) {
		struct m0_clink cl;
		M0_ASSERT(ut_ktest_msg_buf_event_wait_delay_chan != NULL);
		m0_atomic64_inc(&ut_ktest_msg_buf_event_wait_stall);
		m0_clink_init(&cl, NULL);
		m0_clink_add_lock(ut_ktest_msg_buf_event_wait_delay_chan,&cl);
		m0_chan_timedwait(&cl, timeout);
		m0_clink_del_lock(&cl);
		return -ETIMEDOUT;
	}
	return nlx_core_buf_event_wait(lcdom, lctm, timeout);
}

static struct m0_atomic64 ut_ktest_msg_ep_create_fail;
static int ut_ktest_msg_ep_create(struct m0_net_end_point **epp,
				  struct m0_net_transfer_mc *tm,
				  const struct nlx_core_ep_addr *cepa)
{
	if (m0_atomic64_get(&ut_ktest_msg_ep_create_fail) > 0) {
		m0_atomic64_inc(&ut_ktest_msg_ep_create_fail);
		return -ENOMEM;
	}
	return nlx_ep_create(epp, tm, cepa);
}

static void ut_ktest_msg_put_event(struct nlx_kcore_buffer *kcb,
				   unsigned mlength,
				   unsigned offset,
				   int status,
				   int unlinked,
				   struct nlx_core_ep_addr *addr)
{
	lnet_event_t ev;

	M0_SET0(&ev);
	ev.md.user_ptr   = kcb;
	ev.type          = LNET_EVENT_PUT;
	ev.mlength       = mlength;
	ev.rlength       = mlength;
	ev.offset        = offset;
	ev.status        = status;
	ev.unlinked      = unlinked;
	ev.initiator.nid = addr->cepa_nid;
	ev.initiator.pid = addr->cepa_pid;
	ev.hdr_data      = nlx_kcore_hdr_data_encode_raw(addr->cepa_tmid,
							 addr->cepa_portal);
	nlx_kcore_eq_cb(&ev);
}

static void ut_ktest_msg_send_event(struct nlx_kcore_buffer *kcb,
				    unsigned mlength,
				    int status)
{
	lnet_event_t ev;

	M0_SET0(&ev);
	ev.md.user_ptr   = kcb;
	ev.type          = LNET_EVENT_SEND;
	ev.mlength       = mlength;
	ev.rlength       = mlength;
	ev.offset        = 0;
	ev.status        = status;
	ev.unlinked      = 1;
	ev.hdr_data      = 0;
	nlx_kcore_eq_cb(&ev);
}

/* Scatter ACK events in all the test suites. They should be ignored. */
static void ut_ktest_ack_event(struct nlx_kcore_buffer *kcb)
{
	lnet_event_t ev;

	M0_SET0(&ev);
	ev.type          = LNET_EVENT_ACK;
	ev.unlinked      = 1;
	nlx_kcore_eq_cb(&ev);
}


/* memory duplicate only; no ref count increment */
static lnet_kiov_t *ut_ktest_kiov_mem_dup(const lnet_kiov_t *kiov, size_t len)
{
	lnet_kiov_t *k;
	size_t i;

	M0_ALLOC_ARR(k, len);
	M0_UT_ASSERT(k != NULL);
	if (k == NULL)
		return NULL;
	for (i = 0; i < len; ++i, ++kiov) {
		k[i] = *kiov;
	}
	return k;
}

/* inverse of mem_dup */
static void ut_ktest_kiov_mem_free(lnet_kiov_t *kiov)
{
	m0_free(kiov);
}

static bool ut_ktest_kiov_eq(const lnet_kiov_t *k1,
			     const lnet_kiov_t *k2,
			     size_t len)
{
	int i;

	for (i = 0; i < len; ++i, ++k1, ++k2)
		if (k1->kiov_page   != k2->kiov_page  ||
		    k1->kiov_len    != k2->kiov_len   ||
		    k1->kiov_offset != k2->kiov_offset)
			return false;
	return true;
}

static unsigned ut_ktest_kiov_count(const lnet_kiov_t *k, size_t len)
{
	unsigned count;
	size_t i;

	for (i = 0, count = 0; i < len; ++i, ++k) {
		count += k->kiov_len;
	}
	return count;
}

static void ktest_msg_body(struct ut_data *td)
{
	struct m0_net_buffer            *nb1 = &td->bufs1[0];
	struct nlx_xo_transfer_mc       *tp1 = TM1->ntm_xprt_private;
	struct nlx_core_transfer_mc   *lctm1 = &tp1->xtm_core;
	struct nlx_xo_buffer            *bp1 = nb1->nb_xprt_private;
	struct nlx_core_buffer       *lcbuf1 = &bp1->xb_core;
	struct nlx_kcore_transfer_mc  *kctm1 = lctm1->ctm_kpvt;
	struct nlx_kcore_buffer        *kcb1 = lcbuf1->cb_kpvt;
	struct nlx_core_ep_addr        *cepa;
	struct nlx_core_ep_addr         addr;
	lnet_md_t umd;
	lnet_kiov_t *kdup;
	int needed;
	unsigned len;
	unsigned offset;
	unsigned bevs_left;
	unsigned count;
	unsigned last;

	/* TEST
	   Check that the lnet_md_t is properly constructed from a registered
	   network buffer.
	 */
	NLXDBGPnl(td,1,"TEST: net buffer to MD\n");

	nb1->nb_min_receive_size = UT_MSG_SIZE;
	nb1->nb_max_receive_msgs = 1;
	nb1->nb_qtype = M0_NET_QT_MSG_RECV;

	nlx_kcore_umd_init(kctm1, lcbuf1, kcb1, 1, 1, 0, false, &umd);
	M0_UT_ASSERT(umd.start == kcb1->kb_kiov);
	M0_UT_ASSERT(umd.length == kcb1->kb_kiov_len);
	M0_UT_ASSERT(umd.options & LNET_MD_KIOV);
	M0_UT_ASSERT(umd.user_ptr == kcb1);
	M0_UT_ASSERT(LNetHandleIsEqual(umd.eq_handle, kctm1->ktm_eqh));

	/* TEST
	   Check that the count of the number of kiov elements required
	   for different byte sizes.
	 */
	NLXDBGPnl(td,1,"TEST: kiov size arithmetic\n");

#define KEB(b)								\
	nlx_kcore_num_kiov_entries_for_bytes((lnet_kiov_t *) umd.start,	\
					     umd.length, (b), &last)

	M0_UT_ASSERT(KEB(td->buf_size1) == umd.length);
	M0_UT_ASSERT(last               == PAGE_SIZE);

	M0_UT_ASSERT(KEB(1)             == 1);
	M0_UT_ASSERT(last               == 1);

	M0_UT_ASSERT(KEB(PAGE_SIZE - 1) == 1);
	M0_UT_ASSERT(last               == PAGE_SIZE - 1);

	M0_UT_ASSERT(KEB(PAGE_SIZE)     == 1);
	M0_UT_ASSERT(last               == PAGE_SIZE);

	M0_UT_ASSERT(KEB(PAGE_SIZE + 1) == 2);
	M0_UT_ASSERT(last               == 1);

	M0_UT_ASSERT(KEB(UT_MSG_SIZE)   == 1);
	M0_UT_ASSERT(last               == UT_MSG_SIZE);

#undef KEB

	/* TEST
	   Ensure that the kiov adjustment and restoration logic works
	   correctly.
	 */
	NLXDBGPnl(td,1,"TEST: kiov adjustments for length\n");

	kdup = ut_ktest_kiov_mem_dup(kcb1->kb_kiov, kcb1->kb_kiov_len);
	if (kdup == NULL)
		goto done;
	M0_UT_ASSERT(ut_ktest_kiov_eq(kcb1->kb_kiov, kdup, kcb1->kb_kiov_len));

	/* init the UMD that will be adjusted */
	nlx_kcore_umd_init(kctm1, lcbuf1, kcb1, 1, 0, 0, false, &umd);
	M0_UT_ASSERT(kcb1->kb_kiov == umd.start);
	M0_UT_ASSERT(ut_ktest_kiov_count(umd.start,umd.length)
		     == td->buf_size1);
	M0_UT_ASSERT(UT_MSG_SIZE < td->buf_size1);

	/* Adjust for message size. This should not modify the kiov data. */
	{
		size_t size;
		lnet_kiov_t *k = kcb1->kb_kiov;
		size = kcb1->kb_kiov_len;
		nlx_kcore_kiov_adjust_length(kctm1, kcb1, &umd, UT_MSG_SIZE);
		M0_UT_ASSERT(kcb1->kb_kiov == k);
		M0_UT_ASSERT(kcb1->kb_kiov_len == size);
	}

	/* validate adjustments */
	M0_UT_ASSERT(ut_ktest_kiov_count(umd.start, umd.length) == UT_MSG_SIZE);
	M0_UT_ASSERT(umd.length == (UT_MSG_SIZE / PAGE_SIZE + 1));
	M0_UT_ASSERT(kcb1->kb_kiov_len != umd.length);
	M0_UT_ASSERT(kcb1->kb_kiov_adj_idx == umd.length - 1);
	M0_UT_ASSERT(!ut_ktest_kiov_eq(kcb1->kb_kiov, kdup, kcb1->kb_kiov_len));
	M0_UT_ASSERT(kcb1->kb_kiov[umd.length - 1].kiov_len !=
		     kdup[umd.length - 1].kiov_len);
	M0_UT_ASSERT(kcb1->kb_kiov_orig_len == kdup[umd.length - 1].kiov_len);

	/* validate restoration */
	nlx_kcore_kiov_restore_length(kcb1);
	M0_UT_ASSERT(ut_ktest_kiov_eq(kcb1->kb_kiov, kdup, kcb1->kb_kiov_len));
	M0_UT_ASSERT(ut_ktest_kiov_count(kcb1->kb_kiov, kcb1->kb_kiov_len)
		     == td->buf_size1);

	ut_ktest_kiov_mem_free(kdup);

	/* TEST
	   Enqueue a buffer for message reception.
	   Check that buf_msg_recv sends the correct arguments to LNet.
	   Check that the needed count is correctly incremented.
	   Intercept the utils sub to validate.
	*/
	NLXDBGPnl(td,1,"TEST: receive queue logic\n");

	ut_ktest_msg_LNetMDAttach_called = false;

	nb1->nb_min_receive_size = UT_MSG_SIZE;
	nb1->nb_max_receive_msgs = UT_KMSG_OPS;
	nb1->nb_qtype = M0_NET_QT_MSG_RECV;
	needed = lctm1->ctm_bev_needed;
	bevs_left = nb1->nb_max_receive_msgs;

	m0_net_lnet_tm_set_debug(TM1, 0);
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_msg_LNetMDAttach_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + UT_KMSG_OPS);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);
	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 1);
	count = bev_cqueue_size(&lctm1->ctm_bevq);
	M0_UT_ASSERT(count >= lctm1->ctm_bev_needed);

	/* TEST
	   Send a sequence of successful events.
	   The buffer should not get dequeued until the last event.
	   The length and offset reported should be as sent.
	   The reference count on end points is as it should be.
	 */
	NLXDBGPnl(td,1,"TEST: bevq delivery, single\n");

	ut_cbreset();
	cb_save_ep1 = true;
	cepa = nlx_ep_to_core(TM1->ntm_ep);
	addr.cepa_nid = cepa->cepa_nid; /* use real NID */
	addr.cepa_pid = 22;  /* arbitrary */
	M0_UT_ASSERT(cepa->cepa_tmid > 10);
	addr.cepa_tmid = cepa->cepa_tmid - 10;  /* arbitrarily different */
	addr.cepa_portal = 35; /* arbitrary */
	offset = 0;
	len = 1;
	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 0, &addr);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_RECV);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_length1 == len);
	M0_UT_ASSERT(cb_offset1 == offset);
	M0_UT_ASSERT(cb_ep1 != NULL);
	cepa = nlx_ep_to_core(cb_ep1);
	M0_UT_ASSERT(nlx_core_ep_eq(cepa, &addr));
	M0_UT_ASSERT(m0_atomic64_get(&cb_ep1->nep_ref.ref_cnt) == 1);
	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 2);
	/* do not release end point yet */
	cb_ep1 = NULL;
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	cb_save_ep1 = true;
	offset += len;
	len = 10; /* arbitrary */
	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 0, &addr);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_RECV);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_length1 == len);
	M0_UT_ASSERT(cb_offset1 == offset);
	M0_UT_ASSERT(cb_ep1 != NULL);
	cepa = nlx_ep_to_core(cb_ep1);
	M0_UT_ASSERT(nlx_core_ep_eq(cepa, &addr));
	M0_UT_ASSERT(m0_atomic64_get(&cb_ep1->nep_ref.ref_cnt) == 2);
	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 2);
	m0_net_end_point_put(cb_ep1);
	m0_net_end_point_put(cb_ep1);
	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 1);
	cb_ep1 = NULL;
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	cb_save_ep1 = true;
	M0_UT_ASSERT(addr.cepa_tmid > 12);
	addr.cepa_tmid -= 12;
	offset += len;
	len = 11;
	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 1, &addr);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_RECV);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_length1 == len);
	M0_UT_ASSERT(cb_offset1 == offset);
	M0_UT_ASSERT(cb_ep1 != NULL);
	cepa = nlx_ep_to_core(cb_ep1);
	M0_UT_ASSERT(nlx_core_ep_eq(cepa, &addr));
	M0_UT_ASSERT(m0_atomic64_get(&cb_ep1->nep_ref.ref_cnt) == 1);
	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 2);
	m0_net_end_point_put(cb_ep1);
	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 1);
	cb_ep1 = NULL;

	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	M0_UT_ASSERT(bev_cqueue_size(&lctm1->ctm_bevq) == count); /* !freed */

	/* TEST
	   Send a sequence of successful events.
	   Arrange that they build up in the circular queue, so
	   that the "deliver_all" will have multiple events
	   to process.
	*/
	NLXDBGPnl(td,1,"TEST: bevq delivery, batched\n");

	/* enqueue buffer */
	nb1->nb_min_receive_size = UT_MSG_SIZE;
	nb1->nb_max_receive_msgs = UT_KMSG_OPS;
	nb1->nb_qtype = M0_NET_QT_MSG_RECV;
	needed = lctm1->ctm_bev_needed;
	bevs_left = nb1->nb_max_receive_msgs;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_msg_LNetMDAttach_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + UT_KMSG_OPS);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	/* stall event delivery */
	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);
	cb_save_ep1 = false;
	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	ut_ktest_msg_buf_event_wait_delay_chan = &TM2->ntm_chan;/* unused */
	m0_atomic64_set(&ut_ktest_msg_buf_event_wait_stall, 1);
	while(m0_atomic64_get(&ut_ktest_msg_buf_event_wait_stall) == 1)
		ut_chan_timedwait(&td->tmwait1, 1);/* wait for acknowledgment */

	/* start pushing put events */
	count = 0;

	offset = 0;
	len = 5;
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 0, &addr);
	count++;
	M0_UT_ASSERT(cb_called1 == 0);

	offset += len;
	len = 10;
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 0, &addr);
	count++;
	M0_UT_ASSERT(cb_called1 == 0);

	offset += len;
	len = 15;
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 1, &addr);
	count++;
	M0_UT_ASSERT(cb_called1 == 0);

	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 1);

	/* open the spigot ... */
	m0_atomic64_set(&ut_ktest_msg_buf_event_wait_stall, 0);
	m0_chan_signal_lock(ut_ktest_msg_buf_event_wait_delay_chan);
	while (cb_called1 < count) {
		ut_chan_timedwait(&td->tmwait1,1);
	}
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == count);

	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 1);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);

	/* TEST
	   Enqueue a receive buffer.  Send a sequence of events. Arrange for
	   failure in the ep creation subroutine.
	   The buffer should not get dequeued until the last event.
	   The ep failure should not invoke the callback, but the failure
	   statistics should be updated.
	 */
	NLXDBGPnl(td,1,"TEST: EP failure during message receive\n");

	M0_UT_ASSERT(!m0_net_tm_stats_get(TM1,M0_NET_QT_MSG_RECV,&td->qs,true));

	/* enqueue buffer */
	nb1->nb_min_receive_size = UT_MSG_SIZE;
	nb1->nb_max_receive_msgs = UT_KMSG_OPS;
	nb1->nb_qtype = M0_NET_QT_MSG_RECV;
	needed = lctm1->ctm_bev_needed;
	bevs_left = nb1->nb_max_receive_msgs;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_msg_LNetMDAttach_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + UT_KMSG_OPS);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);
	cb_save_ep1 = true;

	/* verify normal delivery */
	offset = 0;
	len = 5;
	M0_UT_ASSERT(bevs_left-- > 0);
	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 0, &addr);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_RECV);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_length1 == len);
	M0_UT_ASSERT(cb_offset1 == offset);
	M0_UT_ASSERT(cb_ep1 != NULL);
	cepa = nlx_ep_to_core(cb_ep1);
	M0_UT_ASSERT(nlx_core_ep_eq(cepa, &addr));
	M0_UT_ASSERT(m0_atomic64_get(&cb_ep1->nep_ref.ref_cnt) == 1);
	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 2);
	m0_net_end_point_put(cb_ep1);
	cb_ep1 = NULL;

	/* arrange for ep creation failure */
	m0_atomic64_set(&ut_ktest_msg_ep_create_fail, 1);
	count = 0;
	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);
	cb_save_ep1 = true;

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);

	addr.cepa_portal += 1;
	offset += len;
	len = 15;
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 0, &addr);
	count++;

	offset += len;
	len = 5;
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_msg_put_event(kcb1, len, offset, 0, 1, &addr);
	count++;

	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1); /* just one callback! */
	M0_UT_ASSERT(cb_status1 == -ENOMEM);
	M0_UT_ASSERT(m0_atomic64_get(&ut_ktest_msg_ep_create_fail)
		     == count + 1);
	M0_UT_ASSERT(cb_ep1 == NULL); /* no EP */
	M0_UT_ASSERT(!m0_net_tm_stats_get(TM1,M0_NET_QT_MSG_RECV,&td->qs,true));
	M0_UT_ASSERT(td->qs.nqs_num_adds == 1);
	M0_UT_ASSERT(td->qs.nqs_num_dels == 0);
	M0_UT_ASSERT(td->qs.nqs_num_s_events == 1);
	M0_UT_ASSERT(td->qs.nqs_num_f_events == count);

	M0_UT_ASSERT(m0_nep_tlist_length(&TM1->ntm_end_points) == 1);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_atomic64_set(&ut_ktest_msg_ep_create_fail, 0);

	/* TEST
	   Enqueue a buffer for sending.
	   Ensure that the destination address is correctly conveyed.
	   Intercept the utils sub to validate.
	   Ensure that the reference count of the ep is maintained correctly.
	   Send a success event.
	 */
	NLXDBGPnl(td,1,"TEST: send queue success logic\n");

	ut_ktest_msg_LNetPut_called = false;
	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	{       /* create a destination end point */
		char epstr[M0_NET_LNET_XEP_ADDR_LEN];
		sprintf(epstr, "%s:%d:%d:1024",
			td->nidstrs1[0], STARTSTOP_PID, STARTSTOP_PORTAL+1);
		zUT(m0_net_end_point_create(&ut_ktest_msg_LNetPut_ep,
					    TM1, epstr));
	}

	nb1->nb_min_receive_size = 0;
	nb1->nb_max_receive_msgs = 0;
	nb1->nb_qtype = M0_NET_QT_MSG_SEND;
	nb1->nb_length = UT_MSG_SIZE;
	nb1->nb_ep = ut_ktest_msg_LNetPut_ep;
	M0_UT_ASSERT(m0_atomic64_get(&nb1->nb_ep->nep_ref.ref_cnt) == 1);
	needed = lctm1->ctm_bev_needed;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_msg_LNetPut_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);
	/* this transport does not pin the ep, but the network layer does */
	M0_UT_ASSERT(m0_atomic64_get(&nb1->nb_ep->nep_ref.ref_cnt) == 2);
	m0_net_end_point_put(ut_ktest_msg_LNetPut_ep);
	ut_ktest_msg_LNetPut_ep = NULL;

	/* deliver the completion event */
	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_msg_send_event(kcb1, UT_MSG_SIZE, 0);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_SEND);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_ep1 == NULL);
	M0_UT_ASSERT(!m0_net_tm_stats_get(TM1,M0_NET_QT_MSG_SEND,&td->qs,true));
	M0_UT_ASSERT(td->qs.nqs_num_adds == 1);
	M0_UT_ASSERT(td->qs.nqs_num_dels == 0);
	M0_UT_ASSERT(td->qs.nqs_num_s_events == 1);
	M0_UT_ASSERT(td->qs.nqs_num_f_events == 0);

	/* TEST
	   Repeat previous test, but send a failure event.
	 */
	NLXDBGPnl(td,1,"TEST: send queue failure logic\n");

	ut_ktest_msg_LNetPut_called = false;
	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	{       /* create a destination end point */
		char epstr[M0_NET_LNET_XEP_ADDR_LEN];
		sprintf(epstr, "%s:%d:%d:1024",
			td->nidstrs1[0], STARTSTOP_PID, STARTSTOP_PORTAL+1);
		zUT(m0_net_end_point_create(&ut_ktest_msg_LNetPut_ep,
					    TM1, epstr));
	}

	nb1->nb_min_receive_size = 0;
	nb1->nb_max_receive_msgs = 0;
	nb1->nb_qtype = M0_NET_QT_MSG_SEND;
	nb1->nb_length = UT_MSG_SIZE;
	nb1->nb_ep = ut_ktest_msg_LNetPut_ep;
	M0_UT_ASSERT(m0_atomic64_get(&nb1->nb_ep->nep_ref.ref_cnt) == 1);
	needed = lctm1->ctm_bev_needed;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_msg_LNetPut_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);
	/* this transport does not pin the ep, but the network layer does */
	M0_UT_ASSERT(m0_atomic64_get(&nb1->nb_ep->nep_ref.ref_cnt) == 2);
	m0_net_end_point_put(ut_ktest_msg_LNetPut_ep);
	ut_ktest_msg_LNetPut_ep = NULL;

	/* deliver the completion event */
	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	ut_ktest_msg_send_event(kcb1, UT_MSG_SIZE, -1);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_MSG_SEND);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));
	M0_UT_ASSERT(cb_status1 == -1);
	M0_UT_ASSERT(cb_ep1 == NULL);
	M0_UT_ASSERT(!m0_net_tm_stats_get(TM1,M0_NET_QT_MSG_SEND,&td->qs,true));
	M0_UT_ASSERT(td->qs.nqs_num_adds == 1);
	M0_UT_ASSERT(td->qs.nqs_num_dels == 0);
	M0_UT_ASSERT(td->qs.nqs_num_s_events == 0);
	M0_UT_ASSERT(td->qs.nqs_num_f_events == 1);

 done:
	cb_ep1 = NULL;
	m0_net_lnet_tm_set_debug(TM1, 0);
}

static void ktest_msg(void)
{
	ut_save_subs();

	/* intercept these before the TM starts */
	nlx_xo_iv._nlx_core_buf_event_wait = ut_ktest_msg_buf_event_wait;
	m0_atomic64_set(&ut_ktest_msg_buf_event_wait_stall, 0);
	nlx_xo_iv._nlx_ep_create = ut_ktest_msg_ep_create;
	m0_atomic64_set(&ut_ktest_msg_ep_create_fail, 0);

	nlx_kcore_iv._nlx_kcore_LNetMDAttach = ut_ktest_msg_LNetMDAttach;
	nlx_kcore_iv._nlx_kcore_LNetPut = ut_ktest_msg_LNetPut;

	ut_test_framework(&ktest_msg_body, NULL, ut_verbose);

	ut_restore_subs();
	ut_ktest_msg_buf_event_wait_delay_chan = NULL;
}

static struct m0_atomic64 ut_ktest_bulk_fake_LNetMDAttach;
static bool ut_ktest_bulk_LNetMDAttach_called;
static int ut_ktest_bulk_LNetMDAttach(struct nlx_kcore_transfer_mc *kctm,
				      struct nlx_core_buffer *lcbuf,
				      struct nlx_kcore_buffer *kcb,
				      lnet_md_t *umd)
{
	uint32_t tmid;
	uint64_t counter;

	ut_ktest_bulk_LNetMDAttach_called = true;
	NLXDBG(kctm, 1, printk("intercepted LNetMDAttach (bulk)\n"));
	NLXDBG(kctm, 2, nlx_kprint_lnet_md("ktest_bulk", umd));

	M0_UT_ASSERT(umd->options & LNET_MD_KIOV);
	M0_UT_ASSERT(umd->start == kcb->kb_kiov);

	M0_UT_ASSERT(umd->threshold == 1);
	M0_UT_ASSERT(!(umd->options & LNET_MD_MAX_SIZE));

	M0_UT_ASSERT(lcbuf->cb_qtype == M0_NET_QT_PASSIVE_BULK_RECV ||
		     lcbuf->cb_qtype == M0_NET_QT_PASSIVE_BULK_SEND);
	if (lcbuf->cb_qtype == M0_NET_QT_PASSIVE_BULK_RECV) {
		M0_UT_ASSERT(umd->options & LNET_MD_OP_PUT);
		M0_UT_ASSERT(!(umd->options & LNET_MD_OP_GET));
		M0_UT_ASSERT(umd->length == kcb->kb_kiov_len);
	} else {
		size_t len;
		unsigned last;
		M0_UT_ASSERT(umd->options & LNET_MD_OP_GET);
		M0_UT_ASSERT(!(umd->options & LNET_MD_OP_PUT));
		len = nlx_kcore_num_kiov_entries_for_bytes(kcb->kb_kiov,
							   kcb->kb_kiov_len,
							   lcbuf->cb_length,
							   &last);
		M0_UT_ASSERT(umd->length == len);
	}

	M0_UT_ASSERT(umd->user_ptr == kcb);
	M0_UT_ASSERT(LNetHandleIsEqual(umd->eq_handle, kctm->ktm_eqh));

	nlx_core_match_bits_decode(lcbuf->cb_match_bits, &tmid, &counter);
	M0_UT_ASSERT(tmid == kctm->ktm_addr.cepa_tmid);
	M0_UT_ASSERT(counter >= M0_NET_LNET_BUFFER_ID_MIN);
	M0_UT_ASSERT(counter <= M0_NET_LNET_BUFFER_ID_MAX);

	if (m0_atomic64_get(&ut_ktest_bulk_fake_LNetMDAttach) > 0) {
		kcb->kb_ktm = kctm;
		return 0;
	}
	return nlx_kcore_LNetMDAttach(kctm, lcbuf, kcb, umd);
}

static bool ut_ktest_bulk_LNetGet_called;
static int ut_ktest_bulk_LNetGet(struct nlx_kcore_transfer_mc *kctm,
				 struct nlx_core_buffer *lcbuf,
				 struct nlx_kcore_buffer *kcb,
				 lnet_md_t *umd)
{
	size_t len;
	unsigned last;

	ut_ktest_bulk_LNetGet_called = true;
	NLXDBG(kctm, 1, printk("intercepted LNetGet (bulk)\n"));
	NLXDBG(kctm, 2, nlx_kprint_lnet_md("ktest_bulk", umd));

	M0_UT_ASSERT((lnet_kiov_t *) umd->start == kcb->kb_kiov);
	len = nlx_kcore_num_kiov_entries_for_bytes(kcb->kb_kiov,
						   kcb->kb_kiov_len,
						   lcbuf->cb_length,
						   &last);
	M0_UT_ASSERT(umd->length == len);
	M0_UT_ASSERT(umd->options & LNET_MD_KIOV);
	M0_UT_ASSERT(umd->threshold == 2); /* note */
	M0_UT_ASSERT(umd->user_ptr == kcb);
	M0_UT_ASSERT(umd->max_size == 0);
	M0_UT_ASSERT(!(umd->options & (LNET_MD_OP_PUT | LNET_MD_OP_GET)));
	M0_UT_ASSERT(LNetHandleIsEqual(umd->eq_handle, kctm->ktm_eqh));

	kcb->kb_ktm = kctm;

	return 0;
}

static bool ut_ktest_bulk_LNetPut_called;
static int ut_ktest_bulk_LNetPut(struct nlx_kcore_transfer_mc *kctm,
				 struct nlx_core_buffer *lcbuf,
				 struct nlx_kcore_buffer *kcb,
				 lnet_md_t *umd)
{
	size_t len;
	unsigned last;

	ut_ktest_bulk_LNetPut_called = true;
	NLXDBG(kctm, 1, printk("intercepted LNetPut (bulk)\n"));
	NLXDBG(kctm, 2, nlx_kprint_lnet_md("ktest_bulk", umd));

	M0_UT_ASSERT((lnet_kiov_t *) umd->start == kcb->kb_kiov);
	len = nlx_kcore_num_kiov_entries_for_bytes(kcb->kb_kiov,
						   kcb->kb_kiov_len,
						   lcbuf->cb_length,
						   &last);
	M0_UT_ASSERT(umd->length == len);
	M0_UT_ASSERT(umd->options & LNET_MD_KIOV);
	M0_UT_ASSERT(umd->threshold == 1);
	M0_UT_ASSERT(umd->user_ptr == kcb);
	M0_UT_ASSERT(umd->max_size == 0);
	M0_UT_ASSERT(!(umd->options & (LNET_MD_OP_PUT | LNET_MD_OP_GET)));
	M0_UT_ASSERT(LNetHandleIsEqual(umd->eq_handle, kctm->ktm_eqh));

	kcb->kb_ktm = kctm;

	return 0;
}

static void ut_ktest_bulk_put_event(struct nlx_kcore_buffer *kcb,
				    unsigned mlength,
				    int status)
{
	lnet_event_t ev;

	M0_SET0(&ev);
	ev.md.user_ptr   = kcb;
	ev.type          = LNET_EVENT_PUT;
	ev.mlength       = mlength;
	ev.rlength       = mlength;
	ev.status        = status;
	ev.unlinked      = 1;
	nlx_kcore_eq_cb(&ev);
}

static void ut_ktest_bulk_get_event(struct nlx_kcore_buffer *kcb,
				    unsigned mlength,
				    int status)
{
	lnet_event_t ev;

	M0_SET0(&ev);
	ev.md.user_ptr   = kcb;
	ev.type          = LNET_EVENT_GET;
	ev.mlength       = mlength;
	ev.rlength       = mlength;
	ev.offset        = 0;
	ev.status        = status;
	ev.unlinked      = 1;
	nlx_kcore_eq_cb(&ev);
}

static void ut_ktest_bulk_send_event(struct nlx_kcore_buffer *kcb,
				     unsigned mlength,
				     int status,
				     int unlinked,
				     int threshold)
{
	lnet_event_t ev;

	M0_SET0(&ev);
	ev.md.user_ptr   = kcb;
	ev.type          = LNET_EVENT_SEND;
	ev.mlength       = mlength;
	ev.rlength       = mlength;
	ev.status        = status;
	ev.unlinked      = unlinked;
	ev.md.threshold  = threshold;
	nlx_kcore_eq_cb(&ev);
}

static void ut_ktest_bulk_reply_event(struct nlx_kcore_buffer *kcb,
				      unsigned mlength,
				      int status,
				      int unlinked,
				      int threshold)
{
	lnet_event_t ev;

	M0_SET0(&ev);
	ev.md.user_ptr   = kcb;
	ev.type          = LNET_EVENT_REPLY;
	ev.mlength       = mlength;
	ev.rlength       = mlength;
	ev.status        = status;
	ev.unlinked      = unlinked;
	ev.md.threshold  = threshold;
	nlx_kcore_eq_cb(&ev);
}

static void ut_ktest_bulk_unlink_event(struct nlx_kcore_buffer *kcb)
{
	lnet_event_t ev;

	M0_SET0(&ev);
	ev.md.user_ptr   = kcb;
	ev.type          = LNET_EVENT_UNLINK;
	ev.unlinked      = 1;
	nlx_kcore_eq_cb(&ev);
}

static void ktest_bulk_body(struct ut_data *td)
{
	struct m0_net_buffer            *nb1 = &td->bufs1[0];
	struct nlx_xo_transfer_mc       *tp1 = TM1->ntm_xprt_private;
	struct nlx_core_transfer_mc   *lctm1 = &tp1->xtm_core;
	struct nlx_xo_buffer            *bp1 = nb1->nb_xprt_private;
	struct nlx_core_buffer       *lcbuf1 = &bp1->xb_core;
	struct nlx_kcore_buffer        *kcb1 = lcbuf1->cb_kpvt;
	int needed;
	unsigned bevs_left;
	struct m0_net_buf_desc nbd_recv;
	struct m0_net_buf_desc nbd_send;

	M0_SET0(&nbd_recv);
	M0_SET0(&nbd_send);

	/* sanity check */
	M0_UT_ASSERT(td->buf_size1 >= UT_BULK_SIZE);
	M0_UT_ASSERT(td->buf_size2 >= UT_BULK_SIZE);

	/* TEST
	   Enqueue a passive receive buffer.
	   Block the real MDAttach call.
	   Send the expected LNet events to indicate that the buffer has
	   been filled.
	*/
	NLXDBGPnl(td, 1, "TEST: passive receive event delivery\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	m0_atomic64_set(&ut_ktest_bulk_fake_LNetMDAttach, 1);
	ut_ktest_bulk_LNetMDAttach_called = false;

	nb1->nb_qtype = M0_NET_QT_PASSIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	nb1->nb_desc.nbd_len = 0;
	nb1->nb_desc.nbd_data = NULL;
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetMDAttach_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);
	M0_UT_ASSERT(nb1->nb_desc.nbd_len != 0);
	M0_UT_ASSERT(nb1->nb_desc.nbd_data != NULL);

	M0_UT_ASSERT(!m0_net_desc_copy(&nb1->nb_desc, &nbd_recv));

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_bulk_put_event(kcb1, UT_BULK_SIZE - 1, 0);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_length1 == UT_BULK_SIZE - 1);
	M0_UT_ASSERT(cb_offset1 == 0);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Passive receive cancelled.
	*/
	NLXDBGPnl(td, 1, "TEST: passive receive event delivery (UNLINK)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	m0_atomic64_set(&ut_ktest_bulk_fake_LNetMDAttach, 1);
	ut_ktest_bulk_LNetMDAttach_called = false;

	nb1->nb_qtype = M0_NET_QT_PASSIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	nb1->nb_desc.nbd_len = 0;
	nb1->nb_desc.nbd_data = NULL;
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetMDAttach_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);
	M0_UT_ASSERT(nb1->nb_desc.nbd_len != 0);
	M0_UT_ASSERT(nb1->nb_desc.nbd_data != NULL);

	M0_UT_ASSERT(!m0_net_desc_copy(&nb1->nb_desc, &nbd_recv));

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_bulk_unlink_event(kcb1);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_PASSIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == -ECANCELED);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Enqueue a passive send buffer.
	   Block the real MDAttach call.
	   Send the expected LNet events to indicate that the buffer has
	   been consumed.
	*/
	NLXDBGPnl(td, 1, "TEST: passive send event delivery\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	m0_atomic64_set(&ut_ktest_bulk_fake_LNetMDAttach, 1);
	ut_ktest_bulk_LNetMDAttach_called = false;

	nb1->nb_qtype = M0_NET_QT_PASSIVE_BULK_SEND;
	nb1->nb_length = UT_BULK_SIZE;
	nb1->nb_desc.nbd_len = 0;
	nb1->nb_desc.nbd_data = NULL;
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetMDAttach_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);
	M0_UT_ASSERT(nb1->nb_desc.nbd_len != 0);
	M0_UT_ASSERT(nb1->nb_desc.nbd_data != NULL);

	M0_UT_ASSERT(!m0_net_desc_copy(&nb1->nb_desc, &nbd_send));

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_bulk_get_event(kcb1, UT_BULK_SIZE, 0);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_PASSIVE_BULK_SEND);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_offset1 == 0);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Passive send cancelled.
	*/
	NLXDBGPnl(td, 1, "TEST: passive send event delivery (UNLINK)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	m0_atomic64_set(&ut_ktest_bulk_fake_LNetMDAttach, 1);
	ut_ktest_bulk_LNetMDAttach_called = false;

	nb1->nb_qtype = M0_NET_QT_PASSIVE_BULK_SEND;
	nb1->nb_length = UT_BULK_SIZE;
	nb1->nb_desc.nbd_len = 0;
	nb1->nb_desc.nbd_data = NULL;
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetMDAttach_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);
	M0_UT_ASSERT(nb1->nb_desc.nbd_len != 0);
	M0_UT_ASSERT(nb1->nb_desc.nbd_data != NULL);

	M0_UT_ASSERT(!m0_net_desc_copy(&nb1->nb_desc, &nbd_send));

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_bulk_unlink_event(kcb1);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_PASSIVE_BULK_SEND);
	M0_UT_ASSERT(cb_status1 == -ECANCELED);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Enqueue an active receive buffer.
	   Block the real LNetGet call.
	   Send the expected LNet events to indicate that the buffer has
	   been filled.
	*/
	NLXDBGPnl(td, 1, "TEST: active receive event delivery (SEND/REPLY)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetGet_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_send, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetGet_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	ut_ktest_bulk_send_event(kcb1, UT_BULK_SIZE, 0, 0, 1);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_bulk_reply_event(kcb1, UT_BULK_SIZE, 0, 1, 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_length1 == UT_BULK_SIZE);
	M0_UT_ASSERT(cb_offset1 == 0);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	NLXDBGPnl(td, 1, "TEST: active receive event delivery (REPLY/SEND)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetGet_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_send, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetGet_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	ut_ktest_bulk_reply_event(kcb1, UT_BULK_SIZE, 0, 0, 1);
	M0_UT_ASSERT(kcb1->kb_ooo_reply);
	M0_UT_ASSERT(kcb1->kb_ooo_status == 0);
	M0_UT_ASSERT(kcb1->kb_ooo_mlength == UT_BULK_SIZE);
	M0_UT_ASSERT(kcb1->kb_ooo_offset == 0);
	ut_ktest_bulk_send_event(kcb1, 0, 0, 1, 0); /* size is wrong */
	ut_ktest_ack_event(kcb1); /* bad event */
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(cb_length1 == UT_BULK_SIZE); /* size must match REPLY */
	M0_UT_ASSERT(cb_offset1 == 0);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Test failure cases:
	   - SEND failure in SEND/REPLY
	   - REPLY failure in SEND/REPLY
	   - REPLY failure in REPLY/SEND
	 */
	NLXDBGPnl(td, 1, "TEST: active receive event delivery "
		  "(SEND failure [/no REPLY])\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetGet_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_send, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetGet_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	ut_ktest_bulk_send_event(kcb1, UT_BULK_SIZE, -EIO, 1, 1);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == -EIO);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	NLXDBGPnl(td, 1, "TEST: active receive event delivery "
		  "(SEND success/REPLY failure)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetGet_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_send, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetGet_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	ut_ktest_bulk_send_event(kcb1, UT_BULK_SIZE, 0, 0, 1);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_bulk_reply_event(kcb1, UT_BULK_SIZE, -EIO, 1, 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == -EIO);
	M0_UT_ASSERT(cb_offset1 == 0);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	NLXDBGPnl(td, 1, "TEST: active receive event delivery "
		  "(REPLY failure/SEND success)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetGet_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_send, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetGet_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	ut_ktest_bulk_reply_event(kcb1, UT_BULK_SIZE, -EIO, 0, 1);
	M0_UT_ASSERT(kcb1->kb_ooo_reply);
	M0_UT_ASSERT(kcb1->kb_ooo_status == -EIO);
	M0_UT_ASSERT(kcb1->kb_ooo_mlength == UT_BULK_SIZE);
	M0_UT_ASSERT(kcb1->kb_ooo_offset == 0);
	ut_ktest_bulk_send_event(kcb1, UT_BULK_SIZE, 0, 1, 0);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == -EIO);
	M0_UT_ASSERT(cb_offset1 == 0);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Test cancellation cases:
	   - UNLINK piggy-backed on SEND in a SEND/REPLY sequence.
	   - UNLINK by itself.
	 */
	NLXDBGPnl(td, 1, "TEST: active receive event delivery "
		  "(SEND + UNLINK [/no REPLY])\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetGet_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_send, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetGet_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_bulk_send_event(kcb1, UT_BULK_SIZE, 0, 1, 1);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == -ECANCELED);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	NLXDBGPnl(td,1,"TEST: active receive event delivery (UNLINK)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetGet_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_RECV;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_send, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetGet_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);
	ut_ktest_bulk_unlink_event(kcb1);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_RECV);
	M0_UT_ASSERT(cb_status1 == -ECANCELED);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));
	M0_UT_ASSERT(!kcb1->kb_ooo_reply);

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Enqueue an active send buffer.
	   Block the real LNetGet call.
	   Send the expected LNet events to indicate that the buffer has
	   been filled.
	   The success case is indistinguishable from a piggy-backed UNLINK.
	*/
	NLXDBGPnl(td, 1, "TEST: active send event delivery\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetPut_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_SEND;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_recv, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetPut_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_bulk_send_event(kcb1, UT_BULK_SIZE, 0, 1, 0);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_SEND);
	M0_UT_ASSERT(cb_status1 == 0);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Send failure situation.
	*/
	NLXDBGPnl(td, 1, "TEST: active send event delivery (SEND failed)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetPut_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_SEND;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_recv, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetPut_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_bulk_send_event(kcb1, UT_BULK_SIZE, -EIO, 1, 0);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_SEND);
	M0_UT_ASSERT(cb_status1 == -EIO);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	/* TEST
	   Send cancellation.
	*/
	NLXDBGPnl(td, 1, "TEST: active send event delivery (UNLINK)\n");

	m0_net_lnet_tm_set_debug(TM1, 0);
	ut_ktest_bulk_LNetPut_called = false;

	nb1->nb_qtype = M0_NET_QT_ACTIVE_BULK_SEND;
	nb1->nb_length = UT_BULK_SIZE;
	M0_UT_ASSERT(!m0_net_desc_copy(&nbd_recv, &nb1->nb_desc));
	needed = lctm1->ctm_bev_needed;
	bevs_left = 1;
	zUT(m0_net_buffer_add(nb1, TM1));
	M0_UT_ASSERT(ut_ktest_bulk_LNetPut_called);
	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed + 1);
	M0_UT_ASSERT(nb1->nb_flags & M0_NET_BUF_QUEUED);

	ut_cbreset();
	M0_UT_ASSERT(cb_called1 == 0);

	m0_clink_add_lock(&TM1->ntm_chan, &td->tmwait1);
	M0_UT_ASSERT(bevs_left-- > 0);
	ut_ktest_ack_event(kcb1); /* bad event */
	ut_ktest_bulk_unlink_event(kcb1);
	m0_chan_wait(&td->tmwait1);
	m0_clink_del_lock(&td->tmwait1);
	M0_UT_ASSERT(cb_called1 == 1);
	M0_UT_ASSERT(cb_nb1 == nb1);
	M0_UT_ASSERT(cb_qt1 == M0_NET_QT_ACTIVE_BULK_SEND);
	M0_UT_ASSERT(cb_status1 == -ECANCELED);
	M0_UT_ASSERT(!(nb1->nb_flags & M0_NET_BUF_QUEUED));

	M0_UT_ASSERT(lctm1->ctm_bev_needed == needed);
	m0_net_desc_free(&nb1->nb_desc);

	m0_net_desc_free(&nbd_recv);
	m0_net_desc_free(&nbd_send);
	return;
}

static void ktest_bulk(void)
{
	ut_save_subs();

	/* intercept these before the TM starts */
	m0_atomic64_set(&ut_ktest_bulk_fake_LNetMDAttach, 0);
	nlx_kcore_iv._nlx_kcore_LNetMDAttach = ut_ktest_bulk_LNetMDAttach;
	nlx_kcore_iv._nlx_kcore_LNetGet = ut_ktest_bulk_LNetGet;
	nlx_kcore_iv._nlx_kcore_LNetPut = ut_ktest_bulk_LNetPut;

	ut_test_framework(&ktest_bulk_body, NULL, ut_verbose);

	ut_restore_subs();
}

#undef UT_BUFVEC_FREE
#undef UT_BUFVEC_ALLOC

static int ut_dev_opens;
static int ut_dev_closes;
static int ut_dev_cleanups;
static int ut_dev_dom_inits;
static int ut_dev_dom_finis;
static int ut_dev_tm_starts;
static int ut_dev_tm_stops;

static int ut_kcore_core_dom_init(struct nlx_kcore_domain *kd,
				  struct nlx_core_domain *cd)
{
	int rc = nlx_kcore_core_dom_init(kd, cd);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(nlx_kcore_domain_invariant(kd));
	M0_UT_ASSERT(!nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc));
	M0_UT_ASSERT(cd->cd_kpvt == kd);
	ut_dev_dom_inits++;
	return rc;
}

static void ut_kcore_core_dom_fini(struct nlx_kcore_domain *kd,
				   struct nlx_core_domain *cd)
{
	M0_UT_ASSERT(drv_bufs_tlist_is_empty(&kd->kd_drv_bufs));
	M0_UT_ASSERT(drv_tms_tlist_is_empty(&kd->kd_drv_tms));
	nlx_kcore_core_dom_fini(kd, cd);
	M0_UT_ASSERT(cd->cd_kpvt == NULL);
	ut_dev_dom_finis++;
}

static int ut_kcore_tm_start(struct nlx_kcore_domain      *kd,
			     struct nlx_core_transfer_mc  *ctm,
			     struct nlx_kcore_transfer_mc *ktm)
{
	if (kd == NULL || ctm == NULL || ktm == NULL)
		return 1;

	/* Init just enough of ktm for driver UT (eg no use of ktm_addr).
	   KCore UT already tested nlx_kcore_tm_start().
	 */
	drv_tms_tlink_init(ktm);
	drv_bevs_tlist_init(&ktm->ktm_drv_bevs);
	ctm->ctm_mb_counter = M0_NET_LNET_BUFFER_ID_MIN;
	spin_lock_init(&ktm->ktm_bevq_lock);
	init_waitqueue_head(&ktm->ktm_wq);
	ctm->ctm_kpvt = ktm;
	ctm->ctm_magic = M0_NET_LNET_CORE_TM_MAGIC;
	M0_UT_ASSERT(nlx_kcore_tm_invariant(ktm));
	M0_UT_ASSERT(nlx_core_tm_invariant(ctm));
	ut_dev_tm_starts++;
	return 0;
}

static void ut_kcore_tm_stop(struct nlx_core_transfer_mc *ctm,
			     struct nlx_kcore_transfer_mc *ktm)
{
	M0_UT_ASSERT(nlx_kcore_tm_invariant(ktm));
	M0_UT_ASSERT(drv_bevs_tlist_is_empty(&ktm->ktm_drv_bevs));

	drv_bevs_tlist_fini(&ktm->ktm_drv_bevs);
	drv_tms_tlink_fini(ktm);
	ktm->ktm_magic = 0;
	if (nlx_core_tm_invariant(ctm))
		ctm->ctm_kpvt = NULL;
	ut_dev_tm_stops++;
}

static struct nlx_kcore_ops ut_kcore_ops = {
	.ko_dom_init = ut_kcore_core_dom_init,
	.ko_dom_fini = ut_kcore_core_dom_fini,
	.ko_buf_register = nlx_kcore_buf_register,
	.ko_buf_deregister = nlx_kcore_buf_deregister,
	.ko_tm_start = ut_kcore_tm_start,
	.ko_tm_stop = ut_kcore_tm_stop,
	.ko_buf_msg_recv = nlx_kcore_buf_msg_recv,
	.ko_buf_msg_send = nlx_kcore_buf_msg_send,
	.ko_buf_active_recv = nlx_kcore_buf_active_recv,
	.ko_buf_active_send = nlx_kcore_buf_active_send,
	.ko_buf_passive_recv = nlx_kcore_buf_passive_recv,
	.ko_buf_passive_send = nlx_kcore_buf_passive_send,
	.ko_buf_del = nlx_kcore_LNetMDUnlink,
	.ko_buf_event_wait = nlx_kcore_buf_event_wait,
};

static int ut_dev_open(struct inode *inode, struct file *file)
{
	struct nlx_kcore_domain *kd;
	int rc;

	M0_THREAD_ENTER;
	rc = nlx_dev_open(inode, file);
	M0_UT_ASSERT(rc == 0 || rc == -EPERM);
	if (rc == 0) {
		m0_mutex_lock(&ktest_mutex);
		ut_dev_opens++;
		m0_mutex_unlock(&ktest_mutex);
		kd = file->private_data;
		M0_UT_ASSERT(nlx_kcore_domain_invariant(kd));
		if (nlx_kcore_domain_invariant(kd)) {
			M0_UT_ASSERT(nlx_core_kmem_loc_is_empty(
							      &kd->kd_cd_loc));
			kd->kd_drv_ops = &ut_kcore_ops;
		}
	}
	return rc;
}

int ut_dev_close(struct inode *inode, struct file *file)
{
	struct nlx_kcore_domain *kd =
	    (struct nlx_kcore_domain *) file->private_data;
	int rc;

	M0_THREAD_ENTER;
	M0_UT_ASSERT(nlx_kcore_domain_invariant(kd));
	if (nlx_kcore_domain_invariant(kd) &&
	    (!drv_bufs_tlist_is_empty(&kd->kd_drv_bufs) ||
	     !drv_tms_tlist_is_empty(&kd->kd_drv_tms)))
		ut_dev_cleanups++;

	rc = nlx_dev_close(inode, file);
	M0_UT_ASSERT(rc == 0);
	M0_UT_ASSERT(file->private_data == NULL);
	m0_mutex_lock(&ktest_mutex);
	ut_dev_closes++;
	m0_cond_signal(&ktest_cond);
	m0_mutex_unlock(&ktest_mutex);
	return rc;
}

bool user_helper_wait(int id)
{
	m0_time_t to = m0_time_from_now(UT_SYNC_DELAY_SEC, 0);
	bool ok = true;

	M0_UT_ASSERT(ktest_id == id);
	m0_semaphore_up(&ktest_sem);
	m0_mutex_lock(&ktest_mutex);
	while ((ktest_id == id && !ktest_user_failed && ok) ||
	       ut_dev_opens > ut_dev_closes)
		ok = m0_cond_timedwait(&ktest_cond, to);
	m0_mutex_unlock(&ktest_mutex);
	M0_UT_ASSERT(ok);
	M0_UT_ASSERT(!ktest_user_failed);
	return ok;
}

static void ktest_dev(void)
{
	bool ok;
	m0_time_t to = m0_time_from_now(UT_SYNC_DELAY_SEC, 0);

#define USER_HELPER_WAIT(id)						\
({									\
	ok = user_helper_wait((id));					\
	if (!ok)							\
		goto restore_fops;					\
})

	ut_dev_opens = 0;
	nlx_dev_file_ops.release = ut_dev_close;
	nlx_dev_file_ops.open    = ut_dev_open;

	/* initial handshake with the user program */
	m0_mutex_lock(&ktest_mutex);
	if (ktest_id == UT_TEST_NONE)
		ok = m0_cond_timedwait(&ktest_cond, to);
	else
		ok = true;
	M0_UT_ASSERT(!ktest_user_failed);
	m0_mutex_unlock(&ktest_mutex);

	M0_UT_ASSERT(ok);
	if (!ok)
		goto restore_fops;

	/* UT_TEST_DEV: just wait for user program to verify device */
	USER_HELPER_WAIT(UT_TEST_DEV);

	/* UT_TEST_OPEN: wait for user program to open/close */
	M0_UT_ASSERT(ut_dev_opens == 0);
	USER_HELPER_WAIT(UT_TEST_OPEN);
	M0_UT_ASSERT(ut_dev_opens == 1);
	M0_UT_ASSERT(ut_dev_cleanups == 0);

	/* UT_TEST_RDWR: wait for user program to try read/write */
	USER_HELPER_WAIT(UT_TEST_RDWR);
	M0_UT_ASSERT(ut_dev_opens == 2);

	/* UT_TEST_RDWR: wait for user program to try invalid ioctls */
	USER_HELPER_WAIT(UT_TEST_BADIOCTL);
	M0_UT_ASSERT(ut_dev_opens == 3);

	/* UT_TEST_DOMINIT: wait for user program dominit/fini */
	USER_HELPER_WAIT(UT_TEST_DOMINIT);
	M0_UT_ASSERT(ut_dev_opens == 4);
	M0_UT_ASSERT(ut_dev_dom_inits == 1);
	M0_UT_ASSERT(ut_dev_dom_finis == ut_dev_dom_inits);
	M0_UT_ASSERT(ut_dev_cleanups == 0);

	/* UT_TEST_TMS */
	USER_HELPER_WAIT(UT_TEST_TMS);
	M0_UT_ASSERT(ut_dev_opens == 5);
	M0_UT_ASSERT(ut_dev_dom_inits == 2);
	M0_UT_ASSERT(ut_dev_dom_finis == ut_dev_dom_inits);
	M0_UT_ASSERT(ut_dev_cleanups == 0);
	M0_UT_ASSERT(ut_dev_tm_starts == MULTI_TM_NR);
	M0_UT_ASSERT(ut_dev_tm_starts == ut_dev_tm_stops);

	/* UT_TEST_DUPTM */
	USER_HELPER_WAIT(UT_TEST_DUPTM);
	M0_UT_ASSERT(ut_dev_opens == 6);
	M0_UT_ASSERT(ut_dev_dom_inits == 3);
	M0_UT_ASSERT(ut_dev_dom_finis == ut_dev_dom_inits);
	M0_UT_ASSERT(ut_dev_cleanups == 0);
	M0_UT_ASSERT(ut_dev_tm_starts == MULTI_TM_NR + 1);
	M0_UT_ASSERT(ut_dev_tm_starts == ut_dev_tm_stops);

	/* UT_TEST_TMCLEANUP */
	USER_HELPER_WAIT(UT_TEST_TMCLEANUP);
	M0_UT_ASSERT(ut_dev_opens == 7);
	M0_UT_ASSERT(ut_dev_dom_inits == 4);
	M0_UT_ASSERT(ut_dev_dom_finis == ut_dev_dom_inits);
	M0_UT_ASSERT(ut_dev_cleanups == 1);
	M0_UT_ASSERT(ut_dev_tm_starts == MULTI_TM_NR * 2 + 1);
	M0_UT_ASSERT(ut_dev_tm_starts == ut_dev_tm_stops);

	/* final handshake before proc file is deregistered */
	M0_UT_ASSERT(ut_dev_opens == ut_dev_closes);
	m0_semaphore_up(&ktest_sem);
	to = m0_time_from_now(UT_SYNC_DELAY_SEC, 0);
	m0_mutex_lock(&ktest_mutex);
	while (!ktest_done && !ktest_user_failed && ok)
		ok = m0_cond_timedwait(&ktest_cond, to);
	m0_mutex_unlock(&ktest_mutex);
	M0_UT_ASSERT(ok);
	M0_UT_ASSERT(!ktest_user_failed);
	M0_UT_ASSERT(ktest_done);

#undef USER_HELPER_WAIT

restore_fops:
	nlx_dev_file_ops.open    = nlx_dev_open;
	m0_mutex_lock(&ktest_mutex);
	while (ut_dev_opens > ut_dev_closes)
		m0_cond_wait(&ktest_cond);
	m0_mutex_unlock(&ktest_mutex);
	nlx_dev_file_ops.release = nlx_dev_close;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
