/* -*- C -*- */
/*
 * COPYRIGHT 2018 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original authors: Ajay Nair       <ajay.nair@seagate.com>
 *                   Ujjwal Lanjewar <ujjwal.lanjewar@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 05-July-2017
 */

#include "helpers/ufid.h"

#include "lib/errno.h"
#include "lib/arith.h"
#include "lib/finject.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

static bool ufid_invariant(struct m0_ufid *ufid)
{
	return M0_RC(!(ufid->uf_salt & ~M0_UFID_SALT_MASK ||
		       ufid->uf_seq_id & ~M0_UFID_SEQID_MASK ||
		       ufid->uf_gen_id & ~M0_UFID_GENID_MASK ||
		       ufid->uf_proc_id & ~M0_UFID_PROCID_MASK));
}

/**
 * Fills random number as salt.
 *
 * @param gr - Pointer to the UFID generator.
 */
static void ufid_salt_refresh(struct m0_ufid_generator *gr)
{
	m0_time_t now;

	now = m0_time_now();
	gr->ufg_ufid_cur.uf_salt = m0_rnd64(&now) % M0_UFID_SALT_MASK;
}

/**
 * Refreshes generation ID of UFID structure
 *
 * @param gr - Pointer to the UFID generator.
 * @return   - Returns 0 on success. Negative errno in case of error.
 */
static int
ufid_generation_id_refresh(struct m0_ufid_generator *gr)
{
	uint32_t new_gen_id = 0;
	uint32_t ts;
	int      retry_count = M0_UFID_RETRY_MAX + 1;

	M0_ENTRY();

	if (M0_FI_ENABLED("retries_exhausted"))
		retry_count = 1;

	while (--retry_count) {
		/* Note: m0_time_now returns value in nanoseconds. */
		ts = m0_time_seconds(m0_time_now());
		if (M0_FI_ENABLED("clock_lt_base_ts"))
			ts = M0_UFID_BASE_TS - 1;

		/* Time must be larger than base time set. */
		if (ts <= M0_UFID_BASE_TS)
			return M0_ERR(-ETIME);

		/* Calculate the new generation id. */
		new_gen_id = ts - M0_UFID_BASE_TS;
		if (M0_FI_ENABLED("clock_skew"))
			new_gen_id = 0;

		/*
		 * Make sure the generation has changed reasonably since
		 * the last time.
		 */
		if (gr->ufg_ufid_cur.uf_gen_id >
		    new_gen_id + M0_UFID_CLOCK_SKEW_LIMIT) {
			M0_LOG(M0_ERROR,
			       "Large clock skew Detected: old=0x%X new=0x%X.",
			       gr->ufg_ufid_cur.uf_gen_id, new_gen_id);

			return M0_ERR(-ETIME);
		}

		if (new_gen_id <= gr->ufg_ufid_cur.uf_gen_id) {
			M0_LOG(M0_WARN,
			       "Clock reset/skew detected: old=0x%X new=0x%X."
			       " Retrying...",
			       gr->ufg_ufid_cur.uf_gen_id, new_gen_id);
			m0_nanosleep(m0_time(1, 0), NULL);
			continue;
		}

    		gr->ufg_ufid_cur.uf_gen_id = new_gen_id;
		break;
	}

  	if (retry_count == 0) /* We have exhausted all the retries, fail */
    		return M0_ERR(-ETIME);
	else
		return M0_RC(0);
}

/**
 * Refreshes process id of an ufid generator
 *
 * @param gr - Pointer to the UFID generator.
 * @return   - Returns 0 on success. Negative errno in case of error.
 */
static int ufid_proc_id_refresh(struct m0_ufid_generator *gr)
{
	uint64_t          proc_id_64;
	struct m0_fid     proc_fid;
	struct m0_clovis *m0c;

	M0_ENTRY();
	M0_PRE(gr != NULL);

	m0c = gr->ufg_m0c;
	M0_ASSERT(m0c != NULL);
	m0_clovis_process_fid(m0c, &proc_fid);
	proc_id_64 = proc_fid.f_key;

	if (M0_FI_ENABLED("proc_id_overflow"))
		proc_id_64 = M0_UFID_PROCID_MAX + 1;
	if (M0_FI_ENABLED("proc_id_warn"))
		proc_id_64 = (M0_UFID_PROCID_MAX >>
			      M0_UFID_PROCID_WARN_BITS) + 1;

	if (proc_id_64 >> M0_UFID_PROCID_BITS > 0) {
		/*
		 * Exceeded allotted bits. service_id will no longer be unique.
		 * FID generator can no longer be used.
		 */
		M0_LOG(M0_ERROR, "Process ID overflowed.");
		return M0_ERR(-EOVERFLOW);
	}

	if (proc_id_64 >> M0_UFID_PROCID_SAFE_BITS > 0)
		/*
		 * The last bits of our allotted bits are being used.
		 * Raise warning message to allow developers to deal with
		 * potential process id overflow.
		 */
		M0_LOG(M0_WARN, "Process ID exceeded safety threshold !");

	gr->ufg_ufid_cur.uf_proc_id =
		(uint32_t)(proc_id_64 & M0_UFID_PROCID_MASK);

	return M0_RC(0);
}

/**
 * Reserves a range of seq IDs by incrementing current sequence id
 * stored in UFID generator. If the number of available sequence ids
 * under current generation id cann't satisfy the number of sequence
 * ids wanted, a new generation id will be created.
 *
 * @param gr      - Pointer to the UFID generator.
 * @param nr_seqs - Number of sequences to reserve/skip
 *
 * @return - Returns 0 on success. Negative errno in case of error.
 */
static int ufid_seq_id_refresh(struct m0_ufid_generator *gr,
			       uint32_t nr_seqs)
{
	int             rc = 0;
	struct m0_ufid *cursor;

	M0_ENTRY();
	M0_PRE(gr != NULL);
	M0_PRE(nr_seqs <= M0_UFID_SEQID_MAX);

	/*
	 * If seqence id is about to exhaust then generate new
	 * generation ID. Note: the ufid generator's lock must have
	 * already be held when reaching here.
	 */
	cursor = &gr->ufg_ufid_cur;
	if (cursor->uf_seq_id + nr_seqs >= M0_UFID_SEQID_MAX) {
		rc = ufid_generation_id_refresh(gr);
		if (rc < 0) {
			M0_LOG(M0_ERROR,
			       "Failed to refresh generation ID, rc=%d", rc);
			return M0_ERR(rc);
		}

		cursor->uf_seq_id = 0;
	}

	cursor->uf_seq_id += nr_seqs;
	return M0_RC(0);
}

static void ufid_to_id128(struct m0_ufid *ufid,
			  struct m0_uint128 *id128)
{
	uint64_t id_lo;
	uint64_t id_hi;
	uint64_t id_hi_reserved;
	uint64_t genid_hi;
	uint64_t genid_lo;
	uint64_t salt;
	uint64_t proc_id;
	uint64_t seq_id;

	M0_ENTRY();
	M0_PRE(ufid != NULL);
	M0_PRE(id128 != NULL);
	M0_PRE(ufid_invariant(ufid));

	salt     = ufid->uf_salt;
	proc_id  = ufid->uf_proc_id;
	seq_id   = ufid->uf_seq_id;
	genid_hi = ufid->uf_gen_id >> M0_UFID_GENID_LO_BITS;
	genid_lo = ufid->uf_gen_id & M0_UFID_GENID_LO_MASK;

	id_hi_reserved = id128->u_hi & M0_UFID_HI_RESERVED;
 	id_hi = salt & M0_UFID_SALT_MASK;
	id_hi = (id_hi << M0_UFID_GENID_HI_BITS) |
		 (genid_hi & M0_UFID_GENID_HI_MASK);
	id_hi |= id_hi_reserved;

	id_lo = genid_lo & M0_UFID_GENID_LO_MASK;
	id_lo = (id_lo << M0_UFID_PROCID_BITS) |
		 (proc_id & M0_UFID_PROCID_MASK);
	id_lo = (id_lo << M0_UFID_SEQID_BITS) |
		 (seq_id & M0_UFID_SEQID_MASK);

	id128->u_hi = id_hi;
	id128->u_lo = id_lo;
}

int m0_ufid_init(struct m0_clovis *m0c,
		 struct m0_ufid_generator *gr)
{
	int rc;

	M0_ENTRY();
	M0_PRE(m0c != NULL && gr != NULL);

	if (gr->ufg_is_initialised)
		return M0_RC(0);
	gr->ufg_m0c = m0c;
	ufid_salt_refresh(gr);

	rc = ufid_proc_id_refresh(gr);
	if (rc < 0) {
 		M0_LOG(M0_ERROR, "%d: Can not generate service id", rc);
		goto error;
	}

	rc = ufid_generation_id_refresh(gr);
	if (rc < 0) {
		M0_LOG(M0_ERROR, "%d: Can not generate generation id", rc);
		goto error;
	}

	m0_mutex_init(&gr->ufg_lock);
	gr->ufg_is_initialised = true;
	return M0_RC(0);

error:
	M0_SET0(gr);
	return M0_ERR(rc);
}
M0_EXPORTED(m0_ufid_init);

void m0_ufid_fini(struct m0_ufid_generator *gr)
{
	M0_ENTRY();
	M0_PRE(gr != NULL);

	if (!gr->ufg_is_initialised)
		return;

	m0_mutex_fini(&gr->ufg_lock);
	M0_SET0(gr);

	M0_LEAVE();
}
M0_EXPORTED(m0_ufid_fini);

int m0_ufid_new(struct m0_ufid_generator *gr,
		uint32_t nr_ids, uint32_t nr_skip_ids,
		struct m0_uint128 *id128)
{
	int            rc = 0;
	struct m0_ufid ufid;

	M0_ENTRY();
	M0_PRE(id128 != NULL);
	M0_PRE(gr != NULL);
	M0_PRE(gr->ufg_is_initialised);

	/*
	 * nr_skip_ids is not allowed to be larger than
	 * M0_UFID_SEQID_MAX as generation ids may not be continous.
	 */
	if (nr_ids == 0 || nr_ids > M0_UFID_SEQID_MAX ||
	    nr_skip_ids > M0_UFID_SEQID_MAX)
		return M0_ERR(-EINVAL);

	m0_mutex_lock(&gr->ufg_lock);

 	rc = ufid_seq_id_refresh(gr, nr_skip_ids)?:
	     ufid_seq_id_refresh(gr, nr_ids);
	if (rc < 0) {
		m0_mutex_unlock(&gr->ufg_lock);
		return M0_ERR(rc);
	}
	ufid = gr->ufg_ufid_cur;
	/* Set back to the start sequence id allocated. */
	ufid.uf_seq_id -= nr_ids;

	m0_mutex_unlock(&gr->ufg_lock);

	ufid_to_id128(&ufid, id128);
	return M0_RC(0);

}
M0_EXPORTED(m0_ufid_new);

int m0_ufid_next(struct m0_ufid_generator *gr,
		 uint32_t nr_ids, struct m0_uint128 *id128)
{
	return m0_ufid_new(gr, nr_ids, 0UL, id128);
};
M0_EXPORTED(m0_ufid_next);

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
