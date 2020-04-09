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
 * Original creation date: 01/13/2012
 */

/* This file is designed to be included in klnet_core.c. */

#ifdef NLX_DEBUG
#include <linux/version.h>
/* LINUX_VERSION_CODE, OBD_OCD_VERSION */
#if M0_LUSTRE_VERSION < 2110
#include <lustre_ver.h>
#else
#include <lustre/lustre_ver.h>
#include <lustre/lustre_idl.h>
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LNET
#include "lib/trace.h"        /* M0_LOG and M0_ENTRY */

#include "net/lnet/linux_kernel/klnet_core.h"

static void __nlx_kprint_lnet_handle(const char *pre, uint64_t cookie)
{
	char buf[32];
	snprintf(buf, sizeof buf, "%#llx", cookie);
	M0_LOG(M0_DEBUG, "%s: %s (lnet_handle_any_t)\n", (char *)pre,
	       (char *)buf);
}
#define nlx_kprint_lnet_handle(pre, h) \
		__nlx_kprint_lnet_handle(pre, (h).cookie)


static void nlx_kprint_lnet_process_id(const char *pre, lnet_process_id_t p)
{
	M0_LOG(M0_DEBUG, "%s: NID=%lu PID=%u\n", (char*)pre,
	       (long unsigned) p.nid, (unsigned) p.pid);
}

static void nlx_kprint_lnet_md(const char *pre, const lnet_md_t *md)
{
	M0_LOG(M0_DEBUG, "%s: %p (lnet_md_t)\n"
		"\t    start: %p\n"
		"\t  options: %x\n"
		"\t   length: %d\n"
		"\tthreshold: %d\n"
		"\t max_size: %d\n"
		"\t user_ptr: %p\n",
		(char*)pre, md, md->start, md->options, md->length,
		md->threshold, md->max_size, md->user_ptr);

	nlx_kprint_lnet_handle("\teq_handle", md->eq_handle);
#if 0
	{
		int i;
		for(i = 0; i < kcb->kb_kiov_len; ++i) {
			M0_LOG(M0_DEBUG, "\t[%d] %p %d %d\n", i,
			       kcb->kb_kiov[i].kiov_page,
			       kcb->kb_kiov[i].kiov_len,
			       kcb->kb_kiov[i].kiov_offset);
		}
	}
#endif
}

static const char *nlx_kcore_lnet_event_type_to_string(lnet_event_kind_t et)
{
	const char *name;

	/*
	 * This enum is used for build-time checks to ensure that we are using
	 * correct mapping of LNet event names
	 *
	 * LNET_EVENT_XXX values has been changed in lustre release 2.2.57.0 see
	 * commit v2_2_57-10-g75a8f4b "LU-56 lnet: split lnet_commit_md and
	 * cleanup" in lustre git repository
	 */
	enum {
#if LUSTRE_VERSION_CODE < OBD_OCD_VERSION(2, 2, 57, 0)
		M0_LNET_EV_GET    = 0,
		M0_LNET_EV_PUT    = 1,
		M0_LNET_EV_REPLY  = 2,
		M0_LNET_EV_ACK    = 3,
		M0_LNET_EV_SEND   = 4,
		M0_LNET_EV_UNLINK = 5,
#else
		M0_LNET_EV_GET    = 1,
		M0_LNET_EV_PUT    = 2,
		M0_LNET_EV_REPLY  = 3,
		M0_LNET_EV_ACK    = 4,
		M0_LNET_EV_SEND   = 5,
		M0_LNET_EV_UNLINK = 6,
#endif
		M0_LNET_EV_UNKNOWN,

		M0_LNET_EV__FIRST = M0_LNET_EV_GET,
		M0_LNET_EV__LAST  = M0_LNET_EV_UNLINK,
	};

	static const char *lnet_event_s[] = {
		[LNET_EVENT_GET]     = "GET",
		[LNET_EVENT_PUT]     = "PUT",
		[LNET_EVENT_REPLY]   = "REPLY",
		[LNET_EVENT_ACK]     = "ACK",
		[LNET_EVENT_SEND]    = "SEND",
		[LNET_EVENT_UNLINK]  = "UNLINK",

		[M0_LNET_EV_UNKNOWN] = "<Unknown>"
	};

	M0_CASSERT((int)LNET_EVENT_GET    == (int)M0_LNET_EV_GET);
	M0_CASSERT((int)LNET_EVENT_PUT    == (int)M0_LNET_EV_PUT);
	M0_CASSERT((int)LNET_EVENT_REPLY  == (int)M0_LNET_EV_REPLY);
	M0_CASSERT((int)LNET_EVENT_ACK    == (int)M0_LNET_EV_ACK);
	M0_CASSERT((int)LNET_EVENT_SEND   == (int)M0_LNET_EV_SEND);
	M0_CASSERT((int)LNET_EVENT_UNLINK == (int)M0_LNET_EV_UNLINK);

	if (et >= (int)M0_LNET_EV__FIRST && et <= (int)M0_LNET_EV__LAST)
		name = lnet_event_s[et];
	else
		name = lnet_event_s[M0_LNET_EV_UNKNOWN];

	return name;
}

static void nlx_kprint_lnet_event(const char *pre, const lnet_event_t *e)
{

	if (e == NULL) {
		M0_LOG(M0_DEBUG, "%s: <null> (lnet_event_t)\n", (char*) pre);
		return;
	}

	M0_LOG(M0_DEBUG, "%s: %p (lnet_event_t)\n", (char*) pre, e);

	nlx_kprint_lnet_process_id("\t   target:", e->target);
	nlx_kprint_lnet_process_id("\tinitiator:", e->target);

	M0_LOG(M0_DEBUG,
	       "\t    sender: %ld\n"
	       "\t      type: %d %s\n"
	       "\t  pt_index: %u\n"
	       "\tmatch_bits: %lx\n"
	       "\t   rlength: %u\n"
	       "\t   mlength: %u\n",
	       (long unsigned) e->sender, e->type,
	       (char*) nlx_kcore_lnet_event_type_to_string(e->type),
	       e->pt_index, (long unsigned) e->match_bits,
	       e->rlength, e->mlength);

	nlx_kprint_lnet_handle("\t md_handle", e->md_handle);

	M0_LOG(M0_DEBUG,
	       "\t  hdr_data: %lx\n"
	       "\t    status: %d\n"
	       "\t  unlinked: %d\n"
	       "\t    offset: %u\n",
	       (long unsigned) e->hdr_data, e->status, e->unlinked, e->offset);

	nlx_kprint_lnet_md("\t        md", &e->md);
}

static void nlx_kprint_kcore_tm(const char *pre,
				const struct nlx_kcore_transfer_mc *ktm)
{
	M0_LOG(M0_DEBUG, "%s: %p (nlx_kcore_transfer_mc)\n", (char*) pre, ktm);
	if (ktm == NULL)
		return;
	M0_LOG(M0_DEBUG, "\t      magic: %lu\n", (unsigned long)ktm->ktm_magic);
	nlx_kprint_lnet_handle("\t        eqh", ktm->ktm_eqh);
}
#endif

/**
   @addtogroup KLNetCore
   @{
 */

/**
   Helper subroutine to encode header data for LNetPut operations.
   @param tmid Transfer machine id
   @param portal Portal number
   @see nlx_kcore_hdr_data_encode(), nlx_kcore_hdr_data_decode()
 */
static inline uint64_t nlx_kcore_hdr_data_encode_raw(uint32_t tmid,
						     uint32_t portal)
{
	return ((uint64_t) tmid << M0_NET_LNET_TMID_SHIFT) |
		(portal & M0_NET_LNET_PORTAL_MASK);
}

/**
   Helper subroutine to encode header data for LNetPut operations.
   @param kctm Pointer to kcore TM private data.
   @see nlx_kcore_hdr_data_decode(), nlx_kcore_hdr_data_encode_raw()
 */
static uint64_t nlx_kcore_hdr_data_encode(struct nlx_kcore_transfer_mc *kctm)
{
	struct nlx_core_ep_addr *cepa;

	M0_PRE(nlx_kcore_tm_invariant(kctm));
	cepa = &kctm->ktm_addr;
	return nlx_kcore_hdr_data_encode_raw(cepa->cepa_tmid,cepa->cepa_portal);
}


/**
   Helper subroutine to decode header data from an LNetPut event.
   @param hdr_data
   @param portal Pointer to portal.
   @param tmid Pointer to transfer machine identifier.
   @see nlx_kcore_hdr_data_encode()
 */
static inline void nlx_kcore_hdr_data_decode(uint64_t hdr_data,
					     uint32_t *portal,
					     uint32_t *tmid)
{
	*portal = (uint32_t) (hdr_data & M0_NET_LNET_PORTAL_MASK);
	*tmid = hdr_data >> M0_NET_LNET_TMID_SHIFT;
}

/**
   Helper subroutine to fill in the common fields of the lnet_md_t associated
   with a network buffer and the common fields of the kcore buffer private data.
   @param kctm Pointer to kcore TM private data.
   @param lcbuf Pointer to core buffer private data with match bits set.
   @param kcb Pointer to kcore buffer private data with match bits set.
   @param threshold Value for threshold field. Should be at least 1.
   @param max_size Max size value, if not zero. If provided the
   LNET_MD_MAX_SIZE flag is set.
   @param options Optional flags to be set.  If not 0, only LNET_MD_OP_PUT or
   LNET_MD_OP_GET are accepted.
   @param isLNetGetOp Set to true if the lnet_md_t is to be used to create an
   MD that will be used in an LNetGet operation. The threshold field is forced
   to 2, and the out-of-order fields of the nlx_kcore_buffer are reset.
   @param umd Pointer to return structure to be filled in.  The ktm_eqh handle
   is used by default. Adjust if necessary.
   @post ergo(isLNetGetOp, umd->threshold == 2 && !kcb->kb_ooo_reply)
 */
static void nlx_kcore_umd_init(struct nlx_kcore_transfer_mc *kctm,
			       struct nlx_core_buffer *lcbuf,
			       struct nlx_kcore_buffer *kcb,
			       int threshold,
			       int max_size,
			       unsigned options,
			       bool isLNetGetOp,
			       lnet_md_t *umd)
{
	M0_PRE(nlx_kcore_tm_invariant(kctm));
	M0_PRE(nlx_core_buffer_invariant(lcbuf));
	M0_PRE(nlx_kcore_buffer_invariant(kcb));
	M0_PRE(threshold > 0);
	M0_PRE(kcb->kb_kiov_len > 0);
	M0_PRE(max_size >= 0);
	M0_PRE(options == 0 ||
	       options == LNET_MD_OP_PUT ||
	       options == LNET_MD_OP_GET);

	M0_SET0(umd);
	umd->options = options;
	umd->start = kcb->kb_kiov;
	umd->options |= LNET_MD_KIOV;
	umd->length = kcb->kb_kiov_len;
	kcb->kb_qtype = lcbuf->cb_qtype;
	kcb->kb_add_time = m0_time_now();
	if (isLNetGetOp) {
		umd->threshold = 2;
		kcb->kb_ooo_reply   = false;
		kcb->kb_ooo_mlength = 0;
		kcb->kb_ooo_status  = 0;
		kcb->kb_ooo_offset  = 0;
	} else
		umd->threshold = threshold;
	if (max_size != 0) {
		umd->max_size = max_size;
		umd->options |= LNET_MD_MAX_SIZE;
	}
	umd->user_ptr = kcb;
	umd->eq_handle = kctm->ktm_eqh;

	NLXDBG(kctm, 2, nlx_kprint_lnet_md("umd init", umd));
	M0_POST(ergo(isLNetGetOp, umd->threshold == 2 && !kcb->kb_ooo_reply));
}

/**
   Helper subroutine to adjust the length of the kiov vector in a UMD
   to match a specified byte length.
   This is needed for SEND and active buffer operations.
   Restore the kiov with nlx_kcore_kiov_restore_length().
   @param ktm Pointer to kcore TM private data.
   @param kcb Pointer to kcore buffer private data with match bits set.
   @param umd Pointer to the UMD.
   @param bytes The byte count desired.
   @see nlx_kcore_kiov_restore_length()
   @post kcb->kb_kiov_adj_idx >= 0
   @post kcb->kb_kiov_adj_idx < kcb->kb_kiov_len
   @post nlx_kcore_kiov_invariant(umd->start, umd->length)
 */
static void nlx_kcore_kiov_adjust_length(struct nlx_kcore_transfer_mc *ktm,
					 struct nlx_kcore_buffer *kcb,
					 lnet_md_t *umd,
					 m0_bcount_t bytes)
{
	size_t num;
	unsigned last;

	M0_PRE(umd->start != NULL);
	M0_PRE(umd->options & LNET_MD_KIOV);
	M0_PRE(umd->length > 0);
	M0_PRE(nlx_kcore_tm_invariant(ktm));
	M0_PRE(nlx_kcore_buffer_invariant(kcb));
	M0_PRE(umd->start == kcb->kb_kiov);

	num = nlx_kcore_num_kiov_entries_for_bytes((lnet_kiov_t *) umd->start,
						   umd->length, bytes, &last);
	NLXDBGP(ktm, 2, "%p: kbuf:%p size:%ld vec:%lu/%lu loff:%u\n",
		ktm, kcb, (unsigned long) bytes,
		(unsigned long) num, (unsigned long) umd->length, last);
	kcb->kb_kiov_adj_idx = num - 1;
	M0_POST(kcb->kb_kiov_adj_idx >= 0);
	M0_POST(kcb->kb_kiov_adj_idx < kcb->kb_kiov_len);
	kcb->kb_kiov_orig_len = kcb->kb_kiov[kcb->kb_kiov_adj_idx].kiov_len;
	kcb->kb_kiov[kcb->kb_kiov_adj_idx].kiov_len = last;
	umd->length = num;
	M0_POST(nlx_kcore_kiov_invariant(umd->start, umd->length));
	return;
}

/**
   Helper subroutine to restore the original length of the buffer's kiov.
   @param kcb Pointer to kcore buffer private data with match bits set.
   @see nlx_kcore_kiov_adjust_length()
   @pre kcb->kb_kiov_adj_idx >= 0
   @pre kcb->kb_kiov_adj_idx < kcb->kb_kiov_len
   @post nlx_kcore_kiov_invariant(kcb->kb_kiov, kcb->kb_kiov_len)
*/
static void nlx_kcore_kiov_restore_length(struct nlx_kcore_buffer *kcb)
{
	M0_PRE(nlx_kcore_buffer_invariant(kcb));
	M0_PRE(kcb->kb_kiov_adj_idx >= 0);
	M0_PRE(kcb->kb_kiov_adj_idx < kcb->kb_kiov_len);
	kcb->kb_kiov[kcb->kb_kiov_adj_idx].kiov_len = kcb->kb_kiov_orig_len;
	M0_POST(nlx_kcore_kiov_invariant(kcb->kb_kiov, kcb->kb_kiov_len));
	return;
}

/**
   Helper subroutine to attach a network buffer to the match list
   associated with the transfer machine's portal.
   - The ME entry created is put at the end of the match list.
   - The ME and MD are set up to automatically unlink.
   - The MD handle is set in the struct nlx_kcore_buffer::kb_mdh field.
   - Sets the kb_ktm field in the KCore buffer private data.
   @param kctm Pointer to kcore TM private data.
   @param lcbuf Pointer to core buffer private data with match bits set in
   the cb_match_bits field, and the network address in cb_addr.
   @param kcb Pointer to kcore buffer private data.
   @param umd Pointer to lnet_md_t structure for the buffer, with appropriate
   values set for the desired operation.
   @note LNet event could potentially be delivered before this sub returns.
 */
static int nlx_kcore_LNetMDAttach(struct nlx_kcore_transfer_mc *kctm,
				  struct nlx_core_buffer *lcbuf,
				  struct nlx_kcore_buffer *kcb,
				  lnet_md_t *umd)
{
	lnet_handle_me_t meh;
	lnet_process_id_t id;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(kctm));
	M0_PRE(nlx_core_buffer_invariant(lcbuf));
	M0_PRE(nlx_kcore_buffer_invariant(kcb));
	M0_PRE(lcbuf->cb_match_bits != 0);

	id.nid = LNET_NID_ANY;
	id.pid = LNET_PID_ANY;
	rc = LNetMEAttach(lcbuf->cb_addr.cepa_portal, id,
			  lcbuf->cb_match_bits, 0,
			  LNET_UNLINK, LNET_INS_AFTER, &meh);
	if (rc != 0) {
		NLXDBGP(kctm, 1,"LNetMEAttach: %d\n", rc);
		return M0_RC(rc);
	}
	M0_POST(!LNetHandleIsInvalid(meh));
	NLXDBG(kctm, 2, nlx_print_core_buffer("nlx_kcore_LNetMDAttach", lcbuf));

	kcb->kb_ktm = kctm; /* loopback can deliver in the LNetPut call */
	rc = LNetMDAttach(meh, *umd, LNET_UNLINK, &kcb->kb_mdh);
	if (rc == 0) {
		NLXDBG(kctm, 1, nlx_kprint_lnet_handle("MDAttach",kcb->kb_mdh));
	} else {
		int trc = LNetMEUnlink(meh);
		NLXDBGP(kctm, 1, "LNetMDAttach: %d\n", rc);
		NLXDBGP(kctm, 1, "LNetMEUnlink: %d\n", trc);
		M0_ASSERT(trc == 0);
		LNetInvalidateMDHandle(&kcb->kb_mdh);
		kcb->kb_ktm = NULL;
	}

	/* Cannot make these assertions here as delivery is asynchronous, and
	   could have completed before we got here.
	   M0_POST(ergo(rc == 0, !LNetMDHandleIsInvalid(kcb->kb_mdh)));
	   M0_POST(ergo(rc == 0, kcb->kb_ktm == kctm));
	*/
	return M0_RC(rc);
}

/**
   Helper subroutine to unlink an MD.
   @param kctm Pointer to kcore TM private data.
   @param kcb Pointer to kcore buffer private data with kb_mdh set.
   @note LNet event could potentially be delivered before this sub returns.
   There is always an inherent race condition between the invocation of this
   subroutine and ongoing activity.  No assumptions can be made about the value
   of ephemeral fields in related buffer data structures.
 */
static int nlx_kcore_LNetMDUnlink(struct nlx_kcore_transfer_mc *kctm,
				  struct nlx_kcore_buffer *kcb)
{
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(kctm));
	M0_PRE(nlx_kcore_buffer_invariant(kcb));
	rc = LNetMDUnlink(kcb->kb_mdh);
	NLXDBG(kctm, 1, NLXP("LNetMDUnlink: %d\n", rc));
	return M0_RC(rc);
}

/**
   Helper subroutine to send a buffer to a remote destination using
   @c LNetPut().
   - The MD is set up to automatically unlink.
   - The MD handle is set in the struct nlx_kcore_buffer::kb_mdh field.
   - The TM's portal and TMID are encoded in the header data.
   - Sets the kb_ktm field in the KCore buffer private data.
   @param kctm Pointer to kcore TM private data.
   @param lcbuf Pointer to core buffer private data with match bits set, and
   the address of the remote destination in struct nlx_core_buffer::cb_addr.
   @param kcb Pointer to kcore buffer private data.
   @param umd Pointer to lnet_md_t structure for the buffer, with appropriate
   values set for the desired operation.
   @see nlx_kcore_hdr_data_encode(), nlx_kcore_hdr_data_decode()
   @note LNet event could potentially be delivered before this sub returns.
 */
static int nlx_kcore_LNetPut(struct nlx_kcore_transfer_mc *kctm,
			     struct nlx_core_buffer *lcbuf,
			     struct nlx_kcore_buffer *kcb,
			     lnet_md_t *umd)
{
	lnet_process_id_t target;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(kctm));
	M0_PRE(nlx_core_buffer_invariant(lcbuf));
	M0_PRE(nlx_kcore_buffer_invariant(kcb));
	M0_PRE(lcbuf->cb_match_bits != 0);

	rc = LNetMDBind(*umd, LNET_UNLINK, &kcb->kb_mdh);
	if (rc != 0) {
		NLXDBGP(kctm, 1,"LNetMDBind: %d\n", rc);
		return M0_RC(rc);
	}
	NLXDBG(kctm, 2, nlx_print_core_buffer("nlx_kcore_LNetPut", lcbuf));
	NLXDBG(kctm, 2, nlx_kprint_lnet_handle("LNetMDBind", kcb->kb_mdh));

	target.nid = lcbuf->cb_addr.cepa_nid;
	target.pid = lcbuf->cb_addr.cepa_pid;
	kcb->kb_ktm = kctm; /* loopback can deliver in the LNetPut call */
	rc = LNetPut(kctm->ktm_addr.cepa_nid, kcb->kb_mdh, LNET_NOACK_REQ,
		     target, lcbuf->cb_addr.cepa_portal,
		     lcbuf->cb_match_bits, 0,
		     nlx_kcore_hdr_data_encode(kctm));
	if (rc != 0) {
		int trc = LNetMDUnlink(kcb->kb_mdh);
		NLXDBGP(kctm, 1, "LNetPut: %d\n", rc);
		NLXDBGP(kctm, 1, "LNetMDUnlink: %d\n", trc);
		M0_ASSERT(trc == 0);
		LNetInvalidateMDHandle(&kcb->kb_mdh);
		kcb->kb_ktm = NULL;
	}

	/* Cannot make these assertions here, because loopback can deliver
	   before we get here.  Leaving the assertions in the comment.
	   M0_POST(ergo(rc == 0, !LNetMDHandleIsInvalid(kcb->kb_mdh)));
	   M0_POST(ergo(rc == 0, kcb->kb_ktm == kctm));
	*/
	return M0_RC(rc);
}

/**
   Helper subroutine to fetch a buffer from a remote destination using
   @c LNetGet().
   - The MD is set up to automatically unlink.
   - The MD handle is set in the struct nlx_kcore_buffer::kb_mdh field.
   - The TM's portal and TMID are encoded in the header data.
   - Sets the kb_ktm field in the KCore buffer private data.
   @param kctm Pointer to kcore TM private data.
   @param lcbuf Pointer to kcore buffer private data with match bits set, and
   the address of the remote destination in struct nlx_core_buffer::cb_addr.
   @param kcb Pointer to kcore buffer private data.
   @param umd Pointer to lnet_md_t structure for the buffer, with appropriate
   values set for the desired operation.
   @pre umd->threshold == 2
   @see nlx_kcore_hdr_data_encode(), nlx_kcore_hdr_data_decode()
   @note LNet event could potentially be delivered before this sub returns.
 */
static int nlx_kcore_LNetGet(struct nlx_kcore_transfer_mc *kctm,
			     struct nlx_core_buffer *lcbuf,
			     struct nlx_kcore_buffer *kcb,
			     lnet_md_t *umd)
{
	lnet_process_id_t target;
	int rc;

	M0_PRE(nlx_kcore_tm_invariant(kctm));
	M0_PRE(nlx_core_buffer_invariant(lcbuf));
	M0_PRE(nlx_kcore_buffer_invariant(kcb));
	M0_PRE(lcbuf->cb_match_bits != 0);

	M0_PRE(umd->threshold == 2);

	rc = LNetMDBind(*umd, LNET_UNLINK, &kcb->kb_mdh);
	if (rc != 0) {
		NLXDBGP(kctm, 1,"LNetMDBind: %d\n", rc);
		return M0_RC(rc);
	}
	NLXDBG(kctm, 2, nlx_print_core_buffer("nlx_kcore_LNetGet", lcbuf));
	NLXDBG(kctm, 2, nlx_kprint_lnet_handle("LNetMDBind", kcb->kb_mdh));

	target.nid = lcbuf->cb_addr.cepa_nid;
	target.pid = lcbuf->cb_addr.cepa_pid;
	kcb->kb_ktm = kctm; /* loopback can deliver in the LNetGet call */
	rc = LNetGet(kctm->ktm_addr.cepa_nid, kcb->kb_mdh,
		     target, lcbuf->cb_addr.cepa_portal,
		     lcbuf->cb_match_bits, 0
#if LUSTRE_VERSION_CODE >= OBD_OCD_VERSION(2, 11, 55, 0)
		     , false
#endif
		     );

	if (rc != 0) {
		int trc = LNetMDUnlink(kcb->kb_mdh);
		NLXDBGP(kctm, 1, "LNetGet: %d\n", rc);
		NLXDBGP(kctm, 1, "LNetMDUnlink: %d\n", trc);
		M0_ASSERT(trc == 0);
		LNetInvalidateMDHandle(&kcb->kb_mdh);
		kcb->kb_ktm = NULL;
	}

	/* Cannot make these assertions here, because loopback can deliver
	   before we get here.  Leaving the assertions in the comment.
	   M0_POST(ergo(rc == 0, !LNetMDHandleIsInvalid(kcb->kb_mdh)));
	   M0_POST(ergo(rc == 0, kcb->kb_ktm == kctm));
	*/
	return M0_RC(rc);
}

/**
   Maps a page that should point to a nlx_core_domain.
   Uses kmap() because this subroutine can be used on contexts that will block.
   @pre nlx_kcore_domain_invariant(kd) &&
   !nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc)
   @param kd kernel private object containing location reference to be mapped
   @returns core object, never NULL
 */
static struct nlx_core_domain *nlx_kcore_core_domain_map(
						   struct nlx_kcore_domain *kd)
{
	char *ptr;
	struct nlx_core_kmem_loc *loc;
	struct nlx_core_domain *ret;

	M0_PRE(nlx_kcore_domain_invariant(kd));
	loc = &kd->kd_cd_loc;
	M0_PRE(!nlx_core_kmem_loc_is_empty(loc));
	ptr = kmap(loc->kl_page);
	ret = (struct nlx_core_domain *) (ptr + loc->kl_offset);
	M0_POST(ret != NULL);
	return ret;
}

/**
   Unmaps the page that contains a nlx_core_domain using kunmap().
   @pre nlx_kcore_domain_invariant(kd)
   @param kd Pointer to kcore domain private data.
 */
static void nlx_kcore_core_domain_unmap(struct nlx_kcore_domain *kd)
{
	M0_PRE(nlx_kcore_domain_invariant(kd));
	kunmap(kd->kd_cd_loc.kl_page);
}

/**
   Maps a page that should point to a nlx_core_buffer.
   Uses kmap() because this subroutine can be used on contexts that will block.
   @pre nlx_kcore_buffer_invariant(kb)
   @post ret != NULL
   @param kb Pointer to kcore buffer private data.
   @returns core object, never NULL
 */
static struct nlx_core_buffer *nlx_kcore_core_buffer_map(
						   struct nlx_kcore_buffer *kb)
{
	char *ptr;
	struct nlx_core_kmem_loc *loc;
	struct nlx_core_buffer *ret;

	M0_PRE(nlx_kcore_buffer_invariant(kb));
	loc = &kb->kb_cb_loc;
	ptr = kmap(loc->kl_page);
	ret = (struct nlx_core_buffer *) (ptr + loc->kl_offset);
	M0_POST(ret != NULL);
	return ret;
}

/**
   Unmaps the page that contains a nlx_core_buffer using kunmap().
   @pre nlx_core_kmem_loc_invariant(&kcb->kb_cb_loc)
   @param kb Pointer to kcore buffer private data.
 */
static void nlx_kcore_core_buffer_unmap(struct nlx_kcore_buffer *kb)
{
	M0_PRE(nlx_core_kmem_loc_invariant(&kb->kb_cb_loc));
	kunmap(kb->kb_cb_loc.kl_page);
}

/**
   Maps a page that should point to a nlx_core_buffer_event.
   Uses kmap() because this subroutine can be used on contexts that will block.
   @pre nlx_kcore_buffer_event_invariant(kbe)
   @post ret != NULL
   @param kbe Pointer to kcore buffer event private data.
   @returns core object, never NULL
 */
static struct nlx_core_buffer_event *nlx_kcore_core_bev_map(
					    struct nlx_kcore_buffer_event *kbe)
{
	char *ptr;
	struct nlx_core_kmem_loc *loc;
	struct nlx_core_buffer_event *ret;

	M0_PRE(nlx_kcore_buffer_event_invariant(kbe));
	loc = &kbe->kbe_bev_loc;
	ptr = kmap(loc->kl_page);
	ret = (struct nlx_core_buffer_event *) (ptr + loc->kl_offset);
	M0_POST(ret != NULL);
	return ret;
}

/**
   Unmaps the page that contains a nlx_core_buffer_event using kunmap().
   @pre nlx_core_kmem_loc_invariant(&kbe->kbe_bev_loc)
   @param kbe Pointer to kcore buffer event private data.
 */
static void nlx_kcore_core_bev_unmap(struct nlx_kcore_buffer_event *kbe)
{
	M0_PRE(nlx_core_kmem_loc_invariant(&kbe->kbe_bev_loc));
	kunmap(kbe->kbe_bev_loc.kl_page);
}

/**
   Maps a page that should point to a nlx_core_transfer_mc.
   Uses kmap() because this subroutine can be used on contexts that will block.
   @pre nlx_kcore_tm_invariant(ktm)
   @post ret != NULL
   @param ktm Pointer to kcore transfer machine private data.
   @returns core object, never NULL
 */
static struct nlx_core_transfer_mc *nlx_kcore_core_tm_map(
					     struct nlx_kcore_transfer_mc *ktm)
{
	char *ptr;
	struct nlx_core_kmem_loc *loc;
	struct nlx_core_transfer_mc *ret;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	loc = &ktm->ktm_ctm_loc;
	ptr = kmap(loc->kl_page);
	ret = (struct nlx_core_transfer_mc *) (ptr + loc->kl_offset);
	M0_POST(ret != NULL);
	return ret;
}

/**
   Unmaps the page that contains a nlx_core_transfer_mc using kunmap().
   @pre nlx_core_kmem_loc_invariant(&kctm->ktm_ctm_loc)
   @param ktm Pointer to kcore transfer machine private data.
 */
static void nlx_kcore_core_tm_unmap(struct nlx_kcore_transfer_mc *ktm)
{
	M0_PRE(nlx_core_kmem_loc_invariant(&ktm->ktm_ctm_loc));
	kunmap(ktm->ktm_ctm_loc.kl_page);
}

/**
   Maps a page that should point to a nlx_core_transfer_mc.
   Uses kmap_atomic(), thus it is user responsibility to serialize this routine
   CPU-wise (each CPU has its own set of slots).

   @pre nlx_kcore_tm_invariant(ktm)
   @post ret != NULL
   @param ktm Pointer to kcore transfer machine private data.
   @returns core object, never NULL
 */
static struct nlx_core_transfer_mc *
nlx_kcore_core_tm_map_atomic(struct nlx_kcore_transfer_mc *ktm)
{
	char *ptr;
	struct nlx_core_kmem_loc *loc;
	struct nlx_core_transfer_mc *ret;

	M0_PRE(nlx_kcore_tm_invariant(ktm));
	loc = &ktm->ktm_ctm_loc;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	ptr = kmap_atomic(loc->kl_page);
#else
	ptr = kmap_atomic(loc->kl_page, KM_USER0);
#endif
	ret = (struct nlx_core_transfer_mc *) (ptr + loc->kl_offset);
	M0_POST(ret != NULL);
	return ret;
}

/**
   Unmaps the page that contains a nlx_core_transfer_mc.
   Uses kunmap_atomic().
   @note this signature differs from nlx_kcore_core_tm_unmap() due to the
   differing requirements of kunmap() vs kunmap_atomic(); the former requires
   a struct page while the latter requires a mapped address.
   @pre nlx_core_tm_invariant(ctm)
   @param ctm Pointer to corresponding core TM private data.
 */
static void nlx_kcore_core_tm_unmap_atomic(struct nlx_core_transfer_mc *ctm)
{
	M0_PRE(nlx_core_tm_invariant(ctm));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	kunmap_atomic(ctm);
#else
	kunmap_atomic(ctm, KM_USER0);
#endif
}

/** @} */ /* KLNetCore */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
