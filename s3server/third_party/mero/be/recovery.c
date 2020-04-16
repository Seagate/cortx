/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Dmytro Podgornyi <dmytro.podgornyi@seagate.com>
 * Original creation date: 8-Dec-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_BE
#include "lib/trace.h"

#include "be/recovery.h"

#include "lib/arith.h"          /* max_check */
#include "lib/errno.h"          /* -ENOSYS */
#include "lib/memory.h"
#include "be/fmt.h"
#include "be/log.h"
#include "mero/magic.h"         /* M0_BE_RECOVERY_MAGIC */

/**
 * @page Recovery DLD
 *
 * - @ref recovery-ovw
 * - @ref recovery-def
 * - @ref recovery-req
 * - @ref recovery-depends
 * - @ref @subpage recovery-fspec "Functional Specification"
 * - @ref recovery-lspec
 -    - @ref recovery-lspec-comps
 *
 * <hr>
 * @section recovery-ovw Recovery overview
 * BE allows users to modify segments in transactional way. A segment is backed
 * with a linux stob, which doesn't provide atomic writes. BE should have
 * consistent segments even after crash. Therefore BE should have a part that
 * can recover BE segments data after crash. This part is called recovery.
 *
 * @section recovery-def Definitions
 *
 * - <b>Recovery</b> is process of consistency reconstruction of segments, which
 *   current BE domain contains of.
 * - <b>Log</b> is persistent storage where transactions are written before they
 *   are placed to segment.
 * - <b>Valid group</b> is a transaction group that is fully logged and contains
 *   complete commit block with proper magic and checksum.
 * - <b>Log-only group</b> is group that was completely logged but it's not
 *   known if it was placed.
 * - <b>Dirty bit</b> is a flag that indicates whether BE was shut down
 *   correctly or not during previous run.
 *
 * @section recovery-req Requirements
 *
 * - On successful recovery completion segments have to contain only complete
 *   and not partial transactions.
 * - Recovery shouldn't take long after normal shutdown;
 * - Recovery time is not important (yet);
 * - Log is stored on persistent storage with random access.
 *
 * Recovery should be started and succeed after:
 * - Power loss;
 * - m0d crash:
 *    - OOM m0d kill;
 *    - SIGSEGV or other signal.
 * - Recovery should succeed if some of the above failures happen during recovery.
 *
 * Recovery may fail after:
 * - Memory corruption;
 * - Segment or log corruption;
 * - I/O error in segment or log;
 *
 * @section recovery-depends Dependencies
 * Recovery depends on these BE components:
 * - log;
 * - engine;
 * - tx_group;
 * - seg0.
 *
 * @section recovery-lspec Logical Specification
 *
 * - @ref recovery-lspec-comps
 * - @ref recovery-lspec-sub
 *
 * @subsection recovery-lspec-comps Component Overview
 * The following BE subsystems are involved into recovery process, so they have
 * to be updated according to the list:
 * - <b>Engine</b> starts recovery process inside m0_be_domain_start(). Recovery
 *   process is started every time engine starts. It analyses special bits
 *   inside seg0 and starts re-applying groups of transactions stored in Log to
 *   make filesystem stored in be segments consistent.
 * - <b>Log</b> stores groups of transactions in order to perform durability of
 *   BE storage. For recovery process, it has to provide interface or functions
 *   for efficient iteration of the groups stored inside. Algorithm has to
 *   iterate groups from last placed to last logged.
 * - <b>Seg0</b> is a storage of metadata related to the filesystem. Recovery
 *   needs to store inside seg0 special bit, analysing which it makes a decision
 *   to start log scanning and groups re-applying. These bit (dirty bit) have to
 *   be set or cleared with a special (non-transactional) procedure and it can
 *   be stored in struct m0_be_seg_hdr nearby the start of the segment. In
 *   future, some redundancy can be added: dirty bit can be stored in different
 *   places of the segment.
 * - <b>Recovery</b> encapsulates a set of algorithms and data structures and
 *   formats which are needed to make data stored inside BE segments consistent
 *   during failures.
 * - From recovery point of view <b>PageD</b> are special interfaces which have
 *   to be used to apply changes to in-memory representation of the segments.
 *   Using m0_be_reg_{get,put}() interface recovery loads the page, where data
 *   corresponding to the scanned group region area lives to apply changes to
 *   the region.
 *
 * @subsection recovery-lspec-sub Component Subroutines
 * <b>Scanning algorithm which finds last logged group and last placed group</b>
 * Log header contains pointer to a valid logged group. Scanning algorithm
 * begins from the pointed group. Groups are scanned in both forward and
 * backward directions. Lsn must follow with increasing order, therefore
 * scanning algorithm repeats while lsn is increased for the forward scanning
 * and while lsn is decreased for backward scanning respectively. Last logged
 * group must be the last handled group during the forward scanning. It
 * contains pointer to last placed group.
 *
 * Backward scanning is stopped if the following condition is met: lsn of the
 * current group is less than lsn of the last placed group.
 *
 * Note: BE log operates with a log record that is representation of
 * transactional group in the log.
 *
 * <b>Iterative interface for looking over groups that need to be re-applied</b>
 * Recovery provides interface for pick next group for re-applying.
 */

M0_TL_DESCR_DEFINE(log_record_iter, "m0_be_log_record_iter list in recovery",
		   static, struct m0_be_log_record_iter, lri_linkage, lri_magic,
		   M0_BE_RECOVERY_MAGIC, M0_BE_RECOVERY_HEAD_MAGIC);
M0_TL_DEFINE(log_record_iter, static, struct m0_be_log_record_iter);

M0_INTERNAL void m0_be_recovery_init(struct m0_be_recovery     *rvr,
                                     struct m0_be_recovery_cfg *cfg)
{
	rvr->brec_cfg = *cfg;
	m0_mutex_init(&rvr->brec_lock);
	log_record_iter_tlist_init(&rvr->brec_iters);
}

M0_INTERNAL void m0_be_recovery_fini(struct m0_be_recovery *rvr)
{
	m0_mutex_fini(&rvr->brec_lock);
	log_record_iter_tlist_fini(&rvr->brec_iters);
}

static int be_recovery_log_record_iter_new(struct m0_be_log_record_iter **iter)
{
	int rc;

	M0_ALLOC_PTR(*iter);
	rc = *iter == NULL ? -ENOMEM : 0;
	rc = rc ?: m0_be_log_record_iter_init(*iter);

	return rc;
}

static void
be_recovery_log_record_iter_destroy(struct m0_be_log_record_iter *iter)
{
	if (iter != NULL) {
		m0_be_log_record_iter_fini(iter);
		m0_free(iter);
	}
}

M0_INTERNAL int m0_be_recovery_run(struct m0_be_recovery *rvr)
{
	struct m0_be_fmt_log_header   log_hdr = {};
	struct m0_be_log_record_iter *prev    = NULL;
	struct m0_be_log_record_iter *iter;
	struct m0_be_log             *log = rvr->brec_cfg.brc_log;
	m0_bindex_t                   last_discarded;
	m0_bindex_t                   log_discarded;
	m0_bindex_t                   next_pos = M0_BINDEX_MAX;
	int                           rc;

	/* TODO avoid reading of header from disk, log reads it during init */
	rc = m0_be_fmt_log_header_init(&log_hdr, NULL);
	M0_ASSERT(rc == 0);
	rc = m0_be_log_header_read(log, &log_hdr);
	M0_ASSERT(rc == 0);
	log_discarded = log_hdr.flh_discarded;

	rc = be_recovery_log_record_iter_new(&iter);
	rc = rc ?: m0_be_log_record_initial(log, iter);
	while (rc == 0) {
		log_record_iter_tlink_init_at_tail(iter, &rvr->brec_iters);
		prev = iter;
		rc = be_recovery_log_record_iter_new(&iter);
		rc = rc ?: m0_be_log_record_next(log, prev, iter);
	}
	be_recovery_log_record_iter_destroy(iter);
	if (!M0_IN(rc, (0, -ENOENT)))
		goto err;
	if (rc == -ENOENT && prev == NULL)
		goto empty;

	last_discarded = max_check(prev->lri_header.lrh_discarded,
				   log_discarded);
	prev = log_record_iter_tlist_head(&rvr->brec_iters);
	while (prev != NULL && prev->lri_header.lrh_pos < last_discarded) {
		next_pos = prev->lri_header.lrh_pos + prev->lri_header.lrh_size;
		log_record_iter_tlink_del_fini(prev);
		be_recovery_log_record_iter_destroy(prev);
		prev = log_record_iter_tlist_head(&rvr->brec_iters);
	}
	M0_ASSERT(ergo(prev == NULL,
		       last_discarded == log_discarded &&
	               last_discarded == next_pos));
	if (prev == NULL)
		goto empty;

	rc = 0;
	while (rc == 0 && prev->lri_header.lrh_pos > last_discarded) {
		rc = be_recovery_log_record_iter_new(&iter);
		rc = rc ?: m0_be_log_record_prev(log, prev, iter);
		if (rc == 0) {
			log_record_iter_tlink_init_at(iter, &rvr->brec_iters);
			prev = iter;
		} else
			be_recovery_log_record_iter_destroy(iter);
	}
	M0_ASSERT(ergo(rc == 0, prev->lri_header.lrh_pos == last_discarded));

	if (rc != 0)
		goto err;

	iter = log_record_iter_tlist_tail(&rvr->brec_iters);

	rvr->brec_last_record_pos  = iter->lri_header.lrh_pos;
	rvr->brec_last_record_size = iter->lri_header.lrh_size;
	rvr->brec_current          = rvr->brec_last_record_pos +
				     rvr->brec_last_record_size;
	rvr->brec_discarded        = prev->lri_header.lrh_pos;

	goto out;

err:
	m0_tl_for(log_record_iter, &rvr->brec_iters, iter) {
		log_record_iter_tlink_del_fini(iter);
		be_recovery_log_record_iter_destroy(iter);
	} m0_tl_endfor;
	goto out;

empty:
	rc = 0;
	rvr->brec_last_record_pos  = log_hdr.flh_group_lsn;
	rvr->brec_last_record_size = log_hdr.flh_group_size;
	rvr->brec_current          = log_discarded;
	rvr->brec_discarded        = log_discarded;
	M0_POST(log_record_iter_tlist_is_empty(&rvr->brec_iters));
out:
	m0_be_fmt_log_header_fini(&log_hdr);
	return rc;
}

M0_INTERNAL m0_bindex_t m0_be_recovery_current(struct m0_be_recovery *rvr)
{
	return rvr->brec_current;
}

M0_INTERNAL m0_bindex_t m0_be_recovery_discarded(struct m0_be_recovery *rvr)
{
	return rvr->brec_discarded;
}

M0_INTERNAL m0_bindex_t
m0_be_recovery_last_record_pos(struct m0_be_recovery *rvr)
{
	return rvr->brec_last_record_pos;
}

M0_INTERNAL m0_bcount_t
m0_be_recovery_last_record_size(struct m0_be_recovery *rvr)
{
	return rvr->brec_last_record_size;
}

M0_INTERNAL bool
m0_be_recovery_log_record_available(struct m0_be_recovery *rvr)
{
	bool result;

	m0_mutex_lock(&rvr->brec_lock);
	result = !log_record_iter_tlist_is_empty(&rvr->brec_iters);
	m0_mutex_unlock(&rvr->brec_lock);

	return result;
}

M0_INTERNAL void
m0_be_recovery_log_record_get(struct m0_be_recovery        *rvr,
			      struct m0_be_log_record_iter *iter)
{
	struct m0_be_log_record_iter *next;

	m0_mutex_lock(&rvr->brec_lock);
	next = log_record_iter_tlist_pop(&rvr->brec_iters);
	m0_mutex_unlock(&rvr->brec_lock);
	M0_ASSERT(next != NULL);
	m0_be_log_record_iter_copy(iter, next);
	log_record_iter_tlink_fini(next);
	be_recovery_log_record_iter_destroy(next);
	M0_LOG(M0_DEBUG, "iter pos=%lu size=%lu discarded=%lu",
	       iter->lri_header.lrh_pos,
	       iter->lri_header.lrh_size,
	       iter->lri_header.lrh_discarded);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
