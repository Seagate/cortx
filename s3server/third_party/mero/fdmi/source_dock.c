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
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include <unistd.h>
#include "lib/memory.h"
#include "fdmi/fdmi.h"
#include "fdmi/source_dock.h"
#include "fdmi/source_dock_internal.h"
#include "fdmi/service.h"

/* fdmi registered sources list declaration */
M0_TL_DESCR_DEFINE(fdmi_src_dock_src_list,
		   "fdmi src list",
		   M0_INTERNAL,
		   struct m0_fdmi_src_ctx,
		   fsc_linkage, fsc_magic,
		   M0_FDMI_SRC_DOCK_SRC_CTX_MAGIC,
		   M0_FDMI_SRC_DOCK_SRC_CTX_HEAD_MAGIC);

M0_TL_DEFINE(fdmi_src_dock_src_list, M0_INTERNAL, struct m0_fdmi_src_ctx);

/*
 * FDMI source records list declaration.  This list keeps all records that are
 * waiting for filter processing and reply confirmation from plugins.  Once
 * all plugins received the record and confirmed it, record is deleted from
 * this list.
 *
 * NOTE. 'release' reply is handled in a different way, when release comes in
 * the record is looked up in a different way.  This list is only needed for
 * record resend operations, not for release.
 */
M0_TL_DESCR_DEFINE(fdmi_record_list, "fdmi rec list", M0_INTERNAL,
		   struct m0_fdmi_src_rec, fsr_linkage, fsr_magic,
		   M0_FDMI_SRC_DOCK_REC_MAGIC,
		   M0_FDMI_SRC_DOCK_REC_HEAD_MAGIC);

M0_TL_DEFINE(fdmi_record_list, M0_INTERNAL, struct m0_fdmi_src_rec);

/* Matched filters list declaration */
M0_TL_DESCR_DEFINE(fdmi_matched_filter_list, "fdmi matched filter list",
		   M0_INTERNAL, struct m0_conf_fdmi_filter,
		   ff_linkage, ff_magic,
		   M0_FDMI_SRC_DOCK_MATCHED_FILTER_MAGIC,
		   M0_FDMI_SRC_DOCK_MATCHED_FILTER_HEAD_MAGIC);

M0_TL_DEFINE(fdmi_matched_filter_list, M0_INTERNAL, struct m0_conf_fdmi_filter);

M0_INTERNAL void m0_fdmi_source_dock_init(struct m0_fdmi_src_dock *src_dock)
{
	M0_ENTRY();
	M0_SET0(src_dock);
	fdmi_src_dock_src_list_tlist_init(&src_dock->fsdc_src_list);
	fdmi_record_list_tlist_init(&src_dock->fsdc_posted_rec_list);
	m0_mutex_init(&src_dock->fsdc_list_mutex);
	src_dock->fsdc_instance_id = 0;
	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi_source_dock_fini(struct m0_fdmi_src_dock *src_dock)
{
	struct m0_fdmi_src_ctx *src_ctx;

	M0_ENTRY();

	/* Deinitialize FDMI source dock instance */

	m0_tl_teardown(fdmi_src_dock_src_list,
		       &src_dock->fsdc_src_list, src_ctx) {
		M0_ASSERT(src_ctx->fsc_registered == false);
		m0_free(src_ctx);
	}

	fdmi_src_dock_src_list_tlist_fini(&src_dock->fsdc_src_list);

	/* Posted record list is handled by FOM and should be empty here */
	fdmi_record_list_tlist_fini(&src_dock->fsdc_posted_rec_list);
	m0_mutex_fini(&src_dock->fsdc_list_mutex);

	M0_LEAVE();
}

M0_INTERNAL bool m0_fdmi__record_is_valid(struct m0_fdmi_src_rec *src_rec)
{
	return  src_rec != NULL &&
		src_rec->fsr_src != NULL &&
		src_rec->fsr_src_ctx != NULL &&
		src_rec->fsr_magic == M0_FDMI_SRC_DOCK_REC_MAGIC;
}

static void src_rec_free(struct m0_ref *ref)
{
	struct m0_fdmi_src_rec *src_rec = M0_AMB(src_rec, ref, fsr_ref);

	M0_ENTRY("ref %p", ref);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));
	M0_PRE(!fdmi_record_list_tlink_is_in(src_rec));

	/* Finalize list and link. @src_rec is still alive. */
	m0_fdmi__record_deinit(src_rec);

	/**
	 * Inform source that fdmi record is sent to all peers,
	 * call fs_end(). Note, that this will release src_rec
	 * itself (it lives in fol_record).
	 *
	 * Nothing about @src_rec should be done below this point.
	 */
	m0_fdmi__fs_end(src_rec);

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi__record_init(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec %p", src_rec);
	M0_ASSERT(src_rec != NULL);

	fdmi_record_list_tlink_init(src_rec);
	fdmi_matched_filter_list_tlist_init(&src_rec->fsr_filter_list);
	src_rec->fsr_src_ctx =
		m0_fdmi__src_ctx_get(src_rec->fsr_src->fs_type_id);
	if (src_rec->fsr_src_ctx == NULL) {
		M0_LOG(M0_ERROR, "FDMI context for record type %x is not found.",
		       src_rec->fsr_src->fs_type_id);
	}

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi__record_deinit(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("rec_src %p", src_rec);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));

	fdmi_matched_filter_list_tlist_fini(&src_rec->fsr_filter_list);
	fdmi_record_list_tlink_fini(src_rec);

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi__rec_id_gen(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec %p", src_rec);
	M0_PRE(m0_fdmi__record_is_valid(src_rec));
	M0_PRE(src_rec->fsr_src != NULL);

        /**
	 * @todo Phase 2: m0_fdmi__rec_id_gen() should return unique ID within
	 * whole Mero system. Some generic function should be used.
	 * For this moment, to provide unique values within several running Mero
	 * instances, lets populate hi value part with rand value, low part
	 * will be incremented.  As a bare minimum, for phase 2 we need to
	 * make sure fsdc_instance_id is globally unique, and that re-start
	 * generates a new one, again globally unique. (Note this is required,
	 * if we leave the same ID over restart, new instance will try to
	 * process replies aimed at previous one. */

	/**
	 * @todo Phase 2: WARNING!
	 * When modifying this function, make sure to rework
	 * m0_fdmi__handle_release, as it is VERY tightly coupled with this
	 * one. */

	if (m0_fdmi_src_dock_get()->fsdc_instance_id == 0) {
#ifndef __KERNEL__
		uint64_t seed = m0_time_now() ^ getpid();
#else
		uint64_t seed = m0_time_now();
#endif
		uint64_t id = 0;

		/* this loop is to make sure ID is never 0. */
		while (id == 0) {
			id = m0_rnd64(&seed);
			seed++;
		}
		m0_fdmi_src_dock_get()->fsdc_instance_id = id;
	}

	M0_ASSERT(sizeof(src_rec) <= sizeof(uint64_t));
	src_rec->fsr_rec_id = M0_UINT128(
		m0_fdmi_src_dock_get()->fsdc_instance_id,
		(uint64_t)src_rec
		);
	M0_LOG(M0_DEBUG, "new FDMI record id = "U128X_F,
	       U128_P(&src_rec->fsr_rec_id));
	M0_LEAVE();
}

M0_INTERNAL int m0_fdmi__record_post(struct m0_fdmi_src_rec *src_rec)
{
	struct m0_fdmi_src_dock *src_dock = m0_fdmi_src_dock_get();

	M0_ENTRY("src %p, fdmi_data %p, rec_id "U128X_F_SAFE,
		 (src_rec ? src_rec->fsr_src : (void*)(-1)),
		 (src_rec ? src_rec->fsr_data : (void*)(-1)),
		 U128_P_SAFE_EX(src_rec, &src_rec->fsr_rec_id));

	M0_ASSERT(src_rec != NULL && src_rec->fsr_src != NULL);

	m0_fdmi__record_init(src_rec);
	m0_fdmi__rec_id_gen(src_rec);
	m0_ref_init(&src_rec->fsr_ref, 1, src_rec_free);

	/** @todo Phase 2: Call m0_fdmi__fs_get(), remove inc_ref in FOL
	 * source */

	m0_mutex_lock(&src_dock->fsdc_list_mutex);
	fdmi_record_list_tlist_add_tail(
			&src_dock->fsdc_posted_rec_list, src_rec);
	m0_fdmi__src_dock_fom_wakeup(&src_dock->fsdc_sd_fom);
	m0_mutex_unlock(&src_dock->fsdc_list_mutex);

	return M0_RC(0);
}

M0_INTERNAL int m0_fdmi_source_alloc(enum m0_fdmi_rec_type_id type_id,
				     struct m0_fdmi_src     **src)
{
	struct m0_fdmi_src_ctx *src_ctx;

	M0_ALLOC_PTR(src_ctx);
	if (src_ctx == NULL) {
		*src = NULL;
		return -ENOMEM;
	}
	src_ctx->fsc_src.fs_type_id = type_id;
	*src = &src_ctx->fsc_src;

	return 0;
}

M0_INTERNAL void m0_fdmi_source_free(struct m0_fdmi_src *src)
{
	struct m0_fdmi_src_ctx *src_ctx;
	M0_PRE(src != NULL);
	src_ctx = container_of(src, struct m0_fdmi_src_ctx, fsc_src);
	M0_ASSERT(!src_ctx->fsc_registered);
	fdmi_src_dock_src_list_tlist_remove(src_ctx);
	m0_free(src_ctx);
}

M0_INTERNAL int m0_fdmi_source_register(struct m0_fdmi_src *src)
{
	struct m0_fdmi_src_ctx  *src_ctx;
	struct m0_fdmi_src_dock *src_dock = m0_fdmi_src_dock_get();

	M0_PRE(src != NULL);
	M0_PRE(src->fs_node_eval != NULL);
	M0_PRE(src->fs_encode != NULL);
	M0_PRE(m0_tl_find(fdmi_src_dock_src_list, src_ctx,
			  &src_dock->fsdc_src_list,
			  (src->fs_type_id == src_ctx->fsc_src.fs_type_id &&
			   src_ctx->fsc_registered)) == NULL);
	/** @todo Phase 2: Check that fdmi_rec_type_id exists */

	M0_ENTRY();
	src_ctx = container_of(src, struct m0_fdmi_src_ctx, fsc_src);
	src_ctx->fsc_registered = true;
	fdmi_src_dock_src_list_tlink_init_at(
		src_ctx,
		&src_dock->fsdc_src_list);

	src->fs_record_post = m0_fdmi__record_post;

	return M0_RC(0);
}

M0_INTERNAL void m0_fdmi_source_deregister(struct m0_fdmi_src *src)
{
	struct m0_fdmi_src_dock *src_dock = m0_fdmi_src_dock_get();
	struct m0_fdmi_src_ctx  *src_ctx;
	struct m0_fdmi_src_rec  *src_rec;

	M0_ENTRY("src = %p", src);
	M0_PRE(src != NULL);

	m0_mutex_lock(&src_dock->fsdc_list_mutex);

	src_ctx = m0_tl_find(
		fdmi_src_dock_src_list, src_ctx,
		&m0_fdmi_src_dock_get()->fsdc_src_list,
		(src->fs_type_id == src_ctx->fsc_src.fs_type_id &&
		 src_ctx->fsc_registered));

	M0_POST(src_ctx != NULL);

	src_ctx->fsc_registered = false;
	src->fs_record_post     = NULL;

	m0_tlist_for(&fdmi_record_list_tl, &src_dock->fsdc_posted_rec_list,
		     src_rec) {
		M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
		if (src_rec->fsr_src == src) {
			fdmi_record_list_tlist_remove(src_rec);
			m0_fdmi__record_deinit(src_rec);
		}
	} m0_tlist_endfor;

	m0_mutex_unlock(&src_dock->fsdc_list_mutex);

	M0_LEAVE();
}

M0_INTERNAL struct m0_fdmi_src_ctx *
m0_fdmi__src_ctx_get(enum m0_fdmi_rec_type_id  src_type_id)
{
	struct m0_fdmi_src_ctx  *ret;
	struct m0_fdmi_src_dock *src_dock = m0_fdmi_src_dock_get();

	M0_ENTRY("src_dock=%p, src_type_id=%d", src_dock, src_type_id);

	ret = m0_tl_find(fdmi_src_dock_src_list, ret, &src_dock->fsdc_src_list,
			 ret->fsc_src.fs_type_id == src_type_id);

	M0_LEAVE("ret=%p", ret);
	return ret;
}

M0_INTERNAL void m0_fdmi__fs_get(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec=%p", src_rec);
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	if (src_rec->fsr_src->fs_get != NULL) {
		src_rec->fsr_src->fs_get(src_rec);
	}

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi__fs_put(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec=%p", src_rec);
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	if (src_rec->fsr_src->fs_put != NULL)
		src_rec->fsr_src->fs_put(src_rec);

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi__fs_begin(
	struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec = %p", src_rec);
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	if (src_rec->fsr_src->fs_begin != NULL)
		src_rec->fsr_src_ctx->fsc_src.fs_begin(src_rec);

	M0_LEAVE();
}

M0_INTERNAL void m0_fdmi__fs_end(struct m0_fdmi_src_rec *src_rec)
{
	M0_ENTRY("src_rec = %p", src_rec);
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	if (src_rec->fsr_src->fs_end != NULL)
		src_rec->fsr_src->fs_end(src_rec);

	M0_LEAVE();
}

M0_INTERNAL enum m0_fdmi_rec_type_id
m0_fdmi__sd_rec_type_id_get(struct m0_fdmi_src_rec *src_rec)
{
	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));
	return src_rec->fsr_src->fs_type_id;
}

/**
 * Handles received reply for sent FDMI record.
 */
M0_INTERNAL int m0_fdmi__handle_reply(struct m0_fdmi_src_dock *sd_ctx,
				      struct m0_fdmi_src_rec  *src_rec,
				      int                      send_res)
{
	M0_ENTRY("send_res %d, src_rec %p",
		 send_res, src_rec);

	M0_ASSERT(m0_fdmi__record_is_valid(src_rec));

	if (send_res != 0)
		m0_fdmi__fs_put(src_rec);

	m0_ref_put(&src_rec->fsr_ref);

	return M0_RC(0);
}

M0_INTERNAL int m0_fdmi__handle_release(struct m0_uint128 *fdmi_rec_id)
{
	struct m0_fdmi_src_rec *src_rec;
	uint64_t                expected;
	uint64_t                actual;

	M0_PRE(fdmi_rec_id != NULL);

	M0_ENTRY("fdmi_rec_id "U128X_F_SAFE, U128_P_SAFE(fdmi_rec_id));

	/**
	 * First, validate that this rec_id was generated by this running
	 * instance (see m0_fdmi__rec_id_gen).
	 */
	actual = fdmi_rec_id->u_hi;
	expected = m0_fdmi_src_dock_get()->fsdc_instance_id;
	if (actual != expected) {
		M0_LOG(M0_WARN,
		       "Failed rc=1. Received release-record aimed at "
		       "another source dock instance: u_hi = %lx, "
		       "expected %lx", actual, expected);
		/**
		 * @todo FDMI Phase 2 -- post addb event about invalid
		 * fdmi_record_id.
		 */
		return M0_RC(1);
	}

	/**
	 * Black magic.  Lower 64 bit of fdmi_record_id are a pointer to
	 * src_rec.  See m0_fdmi__rec_id_gen.
	 */
	src_rec = (void *)fdmi_rec_id->u_lo;
	if (src_rec == NULL ||
	    src_rec->fsr_magic != M0_FDMI_SRC_DOCK_REC_MAGIC) {
#if 0
		/**
		 * @todo Phase 2.  In Phase 1, we release transaction BEFORE
		 * this 'release' even comes.  So in Phase 1 we always end up
		 * in this code branch.  But in Phase 2, it will all change.
		 */
		M0_LOG(M0_WARN,
		       "Attempt to release non-existent FDMI record with ID "
		       U128X_F_SAFE" (magic %ld)", U128_P_SAFE(fdmi_rec_id),
		       (src_rec ? src_rec->fsr_magic : 0));
#endif
		/**
		 * @todo FDMI Phase 2 -- post addb event about invalid
		 * fdmi_record_id.
		 */
		return M0_RC(0);
	}

	m0_fdmi__fs_put(src_rec);

	/* @todo Phase 2: clear map <fdmi record id, endpoint>. */
	return M0_RC(0);
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
