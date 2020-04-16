/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Nikita Danilov <Nikita_Danilov@xyratex.com>
 * Original creation date: 05/21/2010
 */

#include "stob/ioq.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_STOB
#include "lib/trace.h"

#include <limits.h>			/* IOV_MAX */
#include <sys/uio.h>			/* iovec */
#include <libaio.h>                     /* io_getevents */

#include "ha/ha.h"                      /* m0_ha_send */
#include "ha/msg.h"                     /* m0_ha_msg */

#include "lib/misc.h"			/* M0_SET0 */
#include "lib/errno.h"			/* ENOMEM */
#include "lib/finject.h"		/* M0_FI_ENABLED */
#include "lib/locality.h"
#include "lib/memory.h"			/* M0_ALLOC_PTR */

#include "module/instance.h"            /* m0_get() */
#include "reqh/reqh.h"                  /* m0_reqh */
#include "rpc/session.h"                /* m0_rpc_session */
#include "addb2/addb2.h"

#include "stob/addb2.h"
#include "stob/linux.h"			/* m0_stob_linux_container */
#include "stob/io.h"			/* m0_stob_io */
#include "stob/ioq_error.h"             /* m0_stob_ioq_error */

/**
   @addtogroup stoblinux

   <b>Linux stob adieu</b>

   adieu implementation for Linux stob is based on Linux specific asynchronous
   IO interfaces: io_{setup,destroy,submit,cancel,getevents}().

   IO admission control and queueing in Linux stob adieu are implemented on a
   storage object domain level, that is, each domain has its own set of queues,
   threads and thresholds.

   On a high level, adieu IO request is first split into fragments. A fragment
   is initially placed into a per-domain queue (admission queue,
   linux_domain::ioq_queue) where it is held until there is enough space in the
   AIO ring buffer (linux_domain::ioq_ctx). Placing a fragment into the ring
   buffer (ioq_queue_submit()) means that kernel AIO is launched for it. When IO
   completes, the kernel delivers an IO completion event via the ring buffer.

   A number (M0_STOB_IOQ_NR_THREADS by default) of worker adieu threads is
   created for each storage object domain. These threads are implementing
   admission control and completion notification, they

       - listen for the AIO completion events in the ring buffer. When an AIO is
         completed, worker thread signals completion event to AIO users;

       - when space becomes available in the ring buffer, a worker thread moves
         some number of pending fragments from the admission queue to the ring
         buffer.

   Admission queue separate from the ring buffer is needed to

       - be able to handle more pending fragments than a kernel can support and

       - potentially do some pre-processing on the pending fragments (like
         elevator does).

   <b>Concurrency control</b>

   Per-domain data structures (queue, thresholds, etc.) are protected by
   linux_domain::ioq_mutex.

   Concurrency control for an individual adieu fragment is very simple: user is
   not allowed to touch it in SIS_BUSY state and io_getevents() exactly-once
   delivery semantics guarantee that there is no concurrency for busy->idle
   transition. This nice picture would break apart if IO cancellation were to be
   implemented, because it requires synchronization between user actions
   (cancellation) and ongoing IO in SIS_BUSY state.

   @todo use explicit state machine instead of ioq threads

   @see http://www.kernel.org/doc/man-pages/online/pages/man2/io_setup.2.html

   @{
 */

/* ---------------------------------------------------------------------- */

/**
   AIO fragment.

   A ioq_qev is created for each fragment of original adieu request (see
   linux_stob_io_launch()).
 */
struct ioq_qev {
	struct iocb           iq_iocb;
	m0_bcount_t           iq_nbytes;
	m0_bindex_t           iq_offset;
	/** Linkage to a per-domain admission queue
	    (linux_domain::ioq_queue). */
	struct m0_queue_link  iq_linkage;
	struct m0_stob_io    *iq_io;
};

/**
   Linux adieu specific part of generic m0_stob_io structure.
 */
struct stob_linux_io {
	/** Number of fragments in this adieu request. */
	uint32_t           si_nr;
	/** Number of completed fragments. */
	struct m0_atomic64 si_done;
	/** Number of completed bytes. */
	struct m0_atomic64 si_bdone;
	/** Array of fragments. */
	struct ioq_qev    *si_qev;
	/** Main ioq struct */
	struct m0_stob_ioq *si_ioq;
};

static struct ioq_qev *ioq_queue_get   (struct m0_stob_ioq *ioq);
static void            ioq_queue_put   (struct m0_stob_ioq *ioq,
					struct ioq_qev *qev);
static void            ioq_queue_submit(struct m0_stob_ioq *ioq);
static void            ioq_queue_lock  (struct m0_stob_ioq *ioq);
static void            ioq_queue_unlock(struct m0_stob_ioq *ioq);

static const struct m0_stob_io_op stob_linux_io_op;

enum {
	/*
	 * Alignment for direct-IO.
	 *
	 * According to open(2) manpage: "Under Linux 2.6, alignment to
	 * 512-byte boundaries suffices".
	 */
	STOB_IOQ_BSHIFT = 12, /* pow(2, 12) == 4096 */
	STOB_IOQ_BSIZE	= 1 << STOB_IOQ_BSHIFT,
	STOB_IOQ_BMASK	= STOB_IOQ_BSIZE - 1
};

M0_INTERNAL int m0_stob_linux_io_init(struct m0_stob *stob,
				      struct m0_stob_io *io)
{
	struct m0_stob_linux *lstob = m0_stob_linux_container(stob);
	struct stob_linux_io *lio;
	int                   result;

	M0_PRE(io->si_state == SIS_IDLE);

	M0_ALLOC_PTR(lio);
	if (lio != NULL) {
		io->si_stob_private = lio;
		io->si_op = &stob_linux_io_op;
		lio->si_ioq = &lstob->sl_dom->sld_ioq;
		result = 0;
	} else
		result = M0_ERR(-ENOMEM);
	return result;
}

static void stob_linux_io_release(struct stob_linux_io *lio)
{
	if (lio->si_qev != NULL)
		m0_free(lio->si_qev->iq_iocb.u.c.buf);
	m0_free0(&lio->si_qev);
}

static void stob_linux_io_fini(struct m0_stob_io *io)
{
	struct stob_linux_io *lio = io->si_stob_private;

	stob_linux_io_release(lio);
	m0_free(lio);
}

/**
   Launch asynchronous IO.

   - calculate how many fragments IO operation has;

   - allocate ioq_qev array and fill it with fragments;

   - queue all fragments and submit as many as possible;
 */
static int stob_linux_io_launch(struct m0_stob_io *io)
{
	struct m0_stob_linux *lstob = m0_stob_linux_container(io->si_obj);
	struct stob_linux_io *lio   = io->si_stob_private;
	struct m0_stob_ioq   *ioq   = lio->si_ioq;
	struct ioq_qev       *qev;
	struct iovec         *iov;
	struct m0_vec_cursor  src;
	struct m0_vec_cursor  dst;
	uint32_t              frags = 0;
	uint32_t              chunks; /* contiguous stob chunks */
	m0_bcount_t           frag_size;
	int                   result = 0;
	int                   i;
	bool                  eosrc;
	bool                  eodst;
	int                   opcode;

	M0_PRE(M0_IN(io->si_opcode, (SIO_READ, SIO_WRITE)));
	/* prefix fragments execution mode is not yet supported */
	M0_ASSERT((io->si_flags & SIF_PREFIX) == 0);
	M0_PRE(!m0_vec_is_empty(&io->si_user.ov_vec));

	M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_LIO_LAUNCH);
	chunks = io->si_stob.iv_vec.v_nr;

	m0_vec_cursor_init(&src, &io->si_user.ov_vec);
	m0_vec_cursor_init(&dst, &io->si_stob.iv_vec);

	do {
		frag_size = min_check(m0_vec_cursor_step(&src),
				      m0_vec_cursor_step(&dst));
		M0_ASSERT(frag_size > 0);
		frags++;
		eosrc = m0_vec_cursor_move(&src, frag_size);
		eodst = m0_vec_cursor_move(&dst, frag_size);
		M0_ASSERT(eosrc == eodst);
	} while (!eosrc);

	m0_vec_cursor_init(&src, &io->si_user.ov_vec);
	m0_vec_cursor_init(&dst, &io->si_stob.iv_vec);

	lio->si_nr = max_check(frags / IOV_MAX + 1, chunks);
	M0_LOG(M0_DEBUG, "chunks=%d frags=%d si_nr=%d",
	       chunks, frags, lio->si_nr);
	m0_atomic64_set(&lio->si_done, 0);
	m0_atomic64_set(&lio->si_bdone, 0);
	M0_ALLOC_ARR(lio->si_qev, lio->si_nr);
	M0_ALLOC_ARR(iov, frags);
	qev = lio->si_qev;
	if (qev == NULL || iov == NULL) {
		m0_free(iov);
		result = M0_ERR(-ENOMEM);
		goto out;
	}
	opcode = io->si_opcode == SIO_READ ? IO_CMD_PREADV : IO_CMD_PWRITEV;

	ioq_queue_lock(ioq);
	while (result == 0) {
		struct iocb *iocb = &qev->iq_iocb;
		m0_bindex_t  off = io->si_stob.iv_index[dst.vc_seg] +
				   dst.vc_offset;
		m0_bindex_t  prev_off = ~0;
		m0_bcount_t  chunk_size = 0;

		qev->iq_io = io;
		m0_queue_link_init(&qev->iq_linkage);

		iocb->u.v.vec = iov;
		iocb->aio_fildes = lstob->sl_fd;
		iocb->u.v.nr = min32u(frags, IOV_MAX);
		iocb->u.v.offset = off << m0_stob_ioq_bshift(ioq);
		iocb->aio_lio_opcode = opcode;

		for (i = 0; i < iocb->u.v.nr; ++i) {
			void        *buf;
			m0_bindex_t  off;

			buf = io->si_user.ov_buf[src.vc_seg] + src.vc_offset;
			off = io->si_stob.iv_index[dst.vc_seg] + dst.vc_offset;

			if (prev_off != ~0 && prev_off + frag_size != off)
				break;
			prev_off = off;

			frag_size = min_check(m0_vec_cursor_step(&src),
					      m0_vec_cursor_step(&dst));
			if (frag_size > (size_t)~0ULL) {
				result = M0_ERR(-EOVERFLOW);
				break;
			}

			iov->iov_base = m0_stob_addr_open(buf,
						m0_stob_ioq_bshift(ioq));
			iov->iov_len  = frag_size << m0_stob_ioq_bshift(ioq);
			chunk_size += frag_size;

			m0_vec_cursor_move(&src, frag_size);
			m0_vec_cursor_move(&dst, frag_size);
			++iov;
		}
		M0_LOG(M0_DEBUG, FID_F"(%p) %2d: frags=%d op=%d off=%lx sz=%lx"
				 ": rc = %d",
		       FID_P(m0_stob_fid_get(io->si_obj)), io,
		       (int)(qev - lio->si_qev), i, io->si_opcode,
		       (unsigned long)off, (unsigned long)chunk_size, result);
		if (result == 0) {
			iocb->u.v.nr = i;
			qev->iq_nbytes = chunk_size << m0_stob_ioq_bshift(ioq);
			qev->iq_offset = off << m0_stob_ioq_bshift(ioq);

			ioq_queue_put(ioq, qev);

			frags -= i;
			if (frags == 0)
				break;

			++qev;
			M0_ASSERT(qev - lio->si_qev < lio->si_nr);
		}
	}
	lio->si_nr = ++qev - lio->si_qev;
	/* The lock should be held until all 'qev's are pushed into queue and
	 * the lio->si_nr is correctly updated. When this lock is released,
	 * these 'qev's may be submitted.
	 */
	ioq_queue_unlock(ioq);
out:
	if (result != 0) {
		M0_LOG(M0_ERROR, "Launch op=%d io=%p failed: rc=%d",
				 io->si_opcode, io, result);
		stob_linux_io_release(lio);
	} else
		ioq_queue_submit(ioq);

	return result;
}

static const struct m0_stob_io_op stob_linux_io_op = {
	.sio_launch  = stob_linux_io_launch,
	.sio_fini    = stob_linux_io_fini
};

/**
   Removes an element from the (non-empty) admission queue and returns it.
 */
static struct ioq_qev *ioq_queue_get(struct m0_stob_ioq *ioq)
{
	struct m0_queue_link *head;

	M0_ASSERT(!m0_queue_is_empty(&ioq->ioq_queue));
	M0_ASSERT(m0_mutex_is_locked(&ioq->ioq_lock));

	head = m0_queue_get(&ioq->ioq_queue);
	ioq->ioq_queued--;
	M0_ASSERT_EX(ioq->ioq_queued == m0_queue_length(&ioq->ioq_queue));
	return container_of(head, struct ioq_qev, iq_linkage);
}

/**
   Adds an element to the admission queue.
 */
static void ioq_queue_put(struct m0_stob_ioq *ioq,
			  struct ioq_qev *qev)
{
	M0_ASSERT(!m0_queue_link_is_in(&qev->iq_linkage));
	M0_ASSERT(m0_mutex_is_locked(&ioq->ioq_lock));
	// M0_ASSERT(qev->iq_io->si_obj->so_domain == &ioq->sdl_base);

	m0_queue_put(&ioq->ioq_queue, &qev->iq_linkage);
	ioq->ioq_queued++;
	M0_ASSERT_EX(ioq->ioq_queued == m0_queue_length(&ioq->ioq_queue));
}

static void ioq_queue_lock(struct m0_stob_ioq *ioq)
{
	m0_mutex_lock(&ioq->ioq_lock);
}

static void ioq_queue_unlock(struct m0_stob_ioq *ioq)
{
	m0_mutex_unlock(&ioq->ioq_lock);
}

/**
   Transfers fragments from the admission queue to the ring buffer in batches
   until the ring buffer is full.
 */
static void ioq_queue_submit(struct m0_stob_ioq *ioq)
{
	int got;
	int put;
	int avail;
	int i;

	struct ioq_qev  *qev[M0_STOB_IOQ_BATCH_IN_SIZE];
	struct iocb    *evin[M0_STOB_IOQ_BATCH_IN_SIZE];

	do {
		ioq_queue_lock(ioq);
		avail = m0_atomic64_get(&ioq->ioq_avail);
		got = min32(ioq->ioq_queued, min32(avail, ARRAY_SIZE(evin)));
		m0_atomic64_sub(&ioq->ioq_avail, got);
		for (i = 0; i < got; ++i) {
			qev[i] = ioq_queue_get(ioq);
			evin[i] = &qev[i]->iq_iocb;
		}
		ioq_queue_unlock(ioq);

		if (got > 0) {
			put = io_submit(ioq->ioq_ctx, got, evin);
			if (put < 0)
				M0_LOG(M0_ERROR, "got=%d put=%d", got, put);
			if (put < 0)
				put = 0;
			ioq_queue_lock(ioq);
			for (i = put; i < got; ++i)
				ioq_queue_put(ioq, qev[i]);
			ioq_queue_unlock(ioq);

			if (got > put)
				m0_atomic64_add(&ioq->ioq_avail, got - put);
		}
	} while (got > 0);
}

/* Temporary solution until Halon can use M0_HA_MSG_STOB_IOQ */
static void stob_ioq_notify_nvec(const struct m0_fid *conf_sdev)
{
	struct m0_ha_note note;
	struct m0_ha_nvec nvec;

	note = (struct m0_ha_note){
		.no_id    = *conf_sdev,
		.no_state = M0_NC_FAILED,
	};
	nvec = (struct m0_ha_nvec){
		.nv_nr   = 1,
		.nv_note = &note,
	};
	m0_ha_state_set(&nvec);
}

/**
 * Handles detection of drive IO error by signalling HA.
 */
static void ioq_io_error(struct m0_stob_ioq *ioq, struct ioq_qev *qev)
{
	struct m0_stob_io    *io    = qev->iq_io;
	struct m0_stob_linux *lstob = m0_stob_linux_container(io->si_obj);
	struct m0_ha_msg     *msg;
	uint64_t              tag;

	M0_ENTRY();
	M0_LOG(M0_WARN, "IO error: stob_id=" STOB_ID_F " conf_sdev=" FID_F,
	       STOB_ID_P(&lstob->sl_stob.so_id), FID_P(&lstob->sl_conf_sdev));

	if (m0_get()->i_ha == NULL || m0_get()->i_ha_link == NULL) {
		/*
		 * HA is not initialised. It may happen when I/O error occurs
		 * in UT or some subsystem performs I/O after HA finalisation.
		 */
		M0_LOG(M0_DEBUG, "IO error rc=%d is not sent to HA", io->si_rc);
		M0_LEAVE();
		return;
	}

	M0_ALLOC_PTR(msg);
	if (msg == NULL) {
		M0_LOG(M0_ERROR, "Can't allocate memory for msg");
		M0_LEAVE();
		return;
	}
	*msg = (struct m0_ha_msg){
		.hm_fid  = lstob->sl_conf_sdev,
		.hm_time = m0_time_now(),
		.hm_data = {
			.hed_type       = M0_HA_MSG_STOB_IOQ,
			.u.hed_stob_ioq = {
				/* stob info */
				.sie_conf_sdev = lstob->sl_conf_sdev,
				.sie_stob_id   = lstob->sl_stob.so_id,
				.sie_fd        = lstob->sl_fd,
				/* IO info */
				.sie_opcode    = io->si_opcode,
				.sie_rc        = io->si_rc,
				.sie_bshift    = m0_stob_ioq_bshift(ioq),
				.sie_size      = qev->iq_nbytes,
				.sie_offset    = qev->iq_offset,
			},
		},
	};
	stob_ioq_notify_nvec(&lstob->sl_conf_sdev);
	m0_ha_send(m0_get()->i_ha, m0_get()->i_ha_link, msg, &tag);
	m0_free(msg);

	M0_LEAVE("tag=%"PRIu64, tag);
}

/* Note: it is not the number of emulated errors, see below. */
int64_t emulate_disk_errors_nr = 0;

/**
   Handles AIO completion event from the ring buffer.

   When all fragments of a certain adieu request have completed, signals
   m0_stob_io::si_wait.
 */
static void ioq_complete(struct m0_stob_ioq *ioq, struct ioq_qev *qev,
			 long res, long res2)
{
	struct m0_stob_io    *io   = qev->iq_io;
	struct stob_linux_io *lio  = io->si_stob_private;
	struct iocb          *iocb = &qev->iq_iocb;
	const struct m0_fid  *fid  = m0_stob_fid_get(io->si_obj);

	M0_LOG(M0_DEBUG, "io=%p iocb=%p res=%lx nbytes=%lx",
	       io, iocb, (unsigned long)res, (unsigned long)qev->iq_nbytes);

	M0_ASSERT(!m0_queue_link_is_in(&qev->iq_linkage));
	M0_ASSERT(io->si_state == SIS_BUSY);
	M0_ASSERT(m0_atomic64_get(&lio->si_done) < lio->si_nr);

	/* short read. */
	if (io->si_opcode == SIO_READ && res >= 0 && res < qev->iq_nbytes) {
		/* fill the rest of the user buffer with zeroes. */
		const struct iovec *iov = iocb->u.v.vec;
		int i;

		for (i = 0; i < iocb->u.v.nr; ++i) {
			if (iov->iov_len < res) {
				res -= iov->iov_len;
			} else {
				memset(iov->iov_base + res, 0,
				       iov->iov_len - res);
				res = 0;
			}
		}
		res = qev->iq_nbytes;
	}

	if (res > 0) {
		if ((res & m0_stob_ioq_bmask(ioq)) != 0)
			res = M0_ERR(-EIO);
		else
			m0_atomic64_add(&lio->si_bdone, res);
	}
	if (res < 0 && io->si_rc == 0)
		io->si_rc = res;

	if (emulate_disk_errors_nr > 0) {
		static struct m0_fid disk = {};
		static bool found = false;
		/*
		 * Yes, this is not the actual number of errors,
		 * but rather the number of checks. Otherwise,
		 * after the disks for which errors were emulated
		 * will be detached - emulate_disk_errors_nr will
		 * be never decreased and we will get stuck in
		 * these emulation checks.
		 */
		if (m0_atomic64_dec_and_test((void*)&emulate_disk_errors_nr)) {
			if (!found)
				disk = M0_FID0;
			found = false;
		}

		if (!m0_fid_is_set(&disk) && emulate_disk_errors_nr > 0)
			disk = *fid;

		if (m0_fid_eq(&disk, fid)) {
			io->si_rc = M0_ERR(-EIO);
			found = true;
		}
	}

	if (io->si_rc != 0)
		ioq_io_error(ioq, qev);
	/*
	 * The position of this operation is critical:
	 * all threads must complete the above code until
	 * some of them finds here out that all frags are done.
	 */
	if (m0_atomic64_add_return(&lio->si_done, 1) == lio->si_nr) {
		m0_bcount_t bdone = m0_atomic64_get(&lio->si_bdone);

		M0_LOG(M0_DEBUG, FID_F" nr=%d sz=%lx si_rc=%d", FID_P(fid),
		       lio->si_nr, (unsigned long)bdone, (int)io->si_rc);
		io->si_count = bdone >> m0_stob_ioq_bshift(ioq);
		M0_ADDB2_ADD(M0_AVI_STOB_IO_END, FID_P(fid),
			     m0_time_sub(m0_time_now(), io->si_start),
			     io->si_rc, io->si_count, lio->si_nr);
		stob_linux_io_release(lio);
		io->si_state = SIS_IDLE;
		M0_ADDB2_ADD(M0_AVI_STOB_IO_REQ, io->si_id, M0_AVI_LIO_ENDIO);
		m0_chan_broadcast_lock(&io->si_wait);
	}
}

static const struct timespec ioq_timeout_default = {
	.tv_sec  = 1,
	.tv_nsec = 0
};

static unsigned long stob_ioq_timer_cb(unsigned long data)
{
	struct m0_semaphore *stop_sem = (void *)data;

	m0_semaphore_up(stop_sem);
	return 0;
}

static int stob_ioq_thread_init(struct m0_stob_ioq *ioq)
{
	struct m0_timer_locality *timer_loc;
	int                       thread_index;
	int rc;

	thread_index = m0_thread_self() - ioq->ioq_thread;
	timer_loc = &ioq->ioq_stop_timer_loc[thread_index];
	m0_timer_locality_init(timer_loc);
	rc = m0_timer_thread_attach(timer_loc);
	if (rc != 0) {
		m0_timer_locality_fini(timer_loc);
		return M0_ERR(rc);
	}
	rc = m0_timer_init(&ioq->ioq_stop_timer[thread_index], M0_TIMER_HARD,
	                   timer_loc, &stob_ioq_timer_cb,
	                   (unsigned long)&ioq->ioq_stop_sem[thread_index]);
	if (rc != 0) {
		m0_timer_thread_detach(timer_loc);
		m0_timer_locality_fini(timer_loc);
		return M0_ERR(rc);
	}
	m0_semaphore_init(&ioq->ioq_stop_sem[thread_index], 0);
	return M0_RC(rc);
}

/**
   Linux adieu worker thread.

   Listens to the completion events from the ring buffer. Delivers completion
   events to the users. Moves fragments from the admission queue to the ring
   buffer.
 */
static void stob_ioq_thread(struct m0_stob_ioq *ioq)
{
	int got;
	int avail;
	int i;
	struct io_event      evout[M0_STOB_IOQ_BATCH_OUT_SIZE];
	struct timespec      timeout;
	struct m0_addb2_hist inflight = {};
	struct m0_addb2_hist queued   = {};
	struct m0_addb2_hist gotten   = {};
	int                  thread_index;

	thread_index = m0_thread_self() - ioq->ioq_thread;
	M0_ADDB2_PUSH(M0_AVI_STOB_IOQ, thread_index);
	m0_addb2_hist_add_auto(&inflight, 1000, M0_AVI_STOB_IOQ_INFLIGHT, -1);
	m0_addb2_hist_add_auto(&queued,   1000, M0_AVI_STOB_IOQ_QUEUED, -1);
	m0_addb2_hist_add_auto(&gotten,   1000, M0_AVI_STOB_IOQ_GOT, -1);
	while (!m0_semaphore_trydown(&ioq->ioq_stop_sem[thread_index])) {
		timeout = ioq_timeout_default;
		got = io_getevents(ioq->ioq_ctx, 1, ARRAY_SIZE(evout),
				   evout, &timeout);
		if (got > 0) {
			avail = m0_atomic64_add_return(&ioq->ioq_avail, got);
			M0_ASSERT(avail <= M0_STOB_IOQ_RING_SIZE);
		}
		for (i = 0; i < got; ++i) {
			struct ioq_qev  *qev;
			struct io_event *iev;

			iev = &evout[i];
			qev = container_of(iev->obj, struct ioq_qev, iq_iocb);
			M0_ASSERT(!m0_queue_link_is_in(&qev->iq_linkage));
			ioq_complete(ioq, qev, iev->res, iev->res2);
		}
		ioq_queue_submit(ioq);
		m0_addb2_hist_mod(&gotten, got);
		m0_addb2_hist_mod(&queued, ioq->ioq_queued);
		m0_addb2_hist_mod(&inflight, M0_STOB_IOQ_RING_SIZE -
				     m0_atomic64_get(&ioq->ioq_avail));
		m0_addb2_force(M0_MKTIME(5, 0));
	}
	m0_addb2_pop(M0_AVI_STOB_IOQ);
	m0_semaphore_fini(&ioq->ioq_stop_sem[thread_index]);
	m0_timer_stop(&ioq->ioq_stop_timer[thread_index]);
	m0_timer_fini(&ioq->ioq_stop_timer[thread_index]);
	m0_timer_thread_detach(&ioq->ioq_stop_timer_loc[thread_index]);
	m0_timer_locality_fini(&ioq->ioq_stop_timer_loc[thread_index]);
}

M0_INTERNAL int m0_stob_ioq_init(struct m0_stob_ioq *ioq)
{
	int result;
	int i;

	ioq->ioq_ctx      = NULL;
	m0_atomic64_set(&ioq->ioq_avail, M0_STOB_IOQ_RING_SIZE);
	ioq->ioq_queued   = 0;

	m0_queue_init(&ioq->ioq_queue);
	m0_mutex_init(&ioq->ioq_lock);

	result = io_setup(M0_STOB_IOQ_RING_SIZE, &ioq->ioq_ctx);
	if (result == 0) {
		for (i = 0; i < ARRAY_SIZE(ioq->ioq_thread); ++i) {
			result = M0_THREAD_INIT(&ioq->ioq_thread[i],
			                        struct m0_stob_ioq *,
			                        &stob_ioq_thread_init,
						&stob_ioq_thread, ioq,
						"ioq_thread%d", i);
			if (result != 0)
				break;
			m0_stob_ioq_directio_setup(ioq, false);
		}
	}
	if (result != 0)
		m0_stob_ioq_fini(ioq);
	return result;
}

M0_INTERNAL void m0_stob_ioq_fini(struct m0_stob_ioq *ioq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ioq->ioq_stop_timer); ++i)
		m0_timer_start(&ioq->ioq_stop_timer[i], M0_TIME_IMMEDIATELY);
	for (i = 0; i < ARRAY_SIZE(ioq->ioq_thread); ++i) {
		if (ioq->ioq_thread[i].t_func != NULL)
			m0_thread_join(&ioq->ioq_thread[i]);
	}
	if (ioq->ioq_ctx != NULL)
		io_destroy(ioq->ioq_ctx);
	m0_queue_fini(&ioq->ioq_queue);
	m0_mutex_fini(&ioq->ioq_lock);
}

M0_INTERNAL uint32_t m0_stob_ioq_bshift(struct m0_stob_ioq *ioq)
{
	return ioq->ioq_use_directio ? STOB_IOQ_BSHIFT : 0;
}

M0_INTERNAL m0_bcount_t m0_stob_ioq_bsize(struct m0_stob_ioq *ioq)
{
	return ioq->ioq_use_directio ? STOB_IOQ_BSIZE : 0;
}

M0_INTERNAL m0_bcount_t m0_stob_ioq_bmask(struct m0_stob_ioq *ioq)
{
	return ioq->ioq_use_directio ? STOB_IOQ_BMASK : 0;
}

M0_INTERNAL bool m0_stob_ioq_directio(struct m0_stob_ioq *ioq)
{
	return ioq->ioq_use_directio;
}

M0_INTERNAL void m0_stob_ioq_directio_setup(struct m0_stob_ioq *ioq,
					    bool use_directio)
{
	ioq->ioq_use_directio = use_directio;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end group stoblinux */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
