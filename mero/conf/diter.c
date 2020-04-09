/* -*- c -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mandar Sawant <mandar_sawant@seagate.com>
 * Original creation date: 12-Jan-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/diter.h"
#include "conf/obj_ops.h"  /* M0_CONF_DIRNEXT */
#include "lib/memory.h"
#include "lib/errno.h"

/**
 * @page confditer-dld DLD of configuration directory iterator
 *
 * - @ref confditer-ovw
 * - @ref confditer-def
 * - @ref confditer-req
 * - @ref confditer-depends
 * - @ref confditer-highlights
 * - @subpage confditer-fspec
 * - @ref confditer-lspec
 * - @ref confditer-conformance
 * - @ref confditer-ut
 * - @ref confditer-st
 * - @ref confditer-scalability
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-ovw Overview
 *
 * Configuration directory iterator is part of configuration client
 * infrastructure and provides a simplistic interface to iterator over
 * configuration object directories. It implicitly handles the complications of
 * m0_confc_open()ing a configuration directory and them reading the same. It
 * provides both synchronous and asynchronous interfaces.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-def Definitions
 *
 * @b diter (configuration directory iterator):
 * An infrastructure that helps confc users to conveniently iterate over
 * configuration directories.
 *
 * @b lvl (iterator level):
 * A data structure that provides information about configuration directory
 * level.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-req Requirements
 *
 * - @b r.conf.diter.async
 *   Conf directory iterator must provide asynchronous interface to iterate
 *   configuration directory.
 *
 * - @b r.conf.diter.next.obj.filter
 *   Conf directory iterator must return configuration objects which pass a user
 *   provided filter while traversing the directory path.
 *
 * - @b r.conf.diter.break
 *   Conf directory iterator must allow user to break without completing the
 *   entire given directory path traversal in the need of the situation.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-depends Dependencies
 *
 * - Conf directory iterator assumes that the origin is already
 *   m0_confc_open()ed.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-highlights Design Highlights
 *
 * - Conf directory iterator is implemented over existing confc infrastructure.
 * - Conf directory iterator uses relational fids to generically open directory
     which is part of an ambient configuration object.
 * - Conf directory iterator follows depth first order.
 * - Conf directory iterator maintains directory level information in a separate
 *   data structure.
 * - Conf directory iterator accepts a user defined filter to return only
 *   specific set of configuration objects while traversing the configuration
 *   path.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-lspec Logical Specification
 *
 * - @ref confditer-lspec-setup
 * - @ref confditer-lspec-next
 * - @ref confditer-lspec-fini
 *
 * <hr> <!------------------------------------------------------------>
 * @subsection confditer-lspec-setup Configuration directory setup
 *
 * Configuration directory iterator can be initialised from any intermediate
 * configuration object specified as the origin and need not start from
 * the confc root.
 *
 * <hr> <!------------------------------------------------------------>
 * @subsection confditer-lspec-next Configuration directory next
 *
 * Configuration directory iterator follows depth first order to access the
 * specified directory path.
 * Following algorithm illustrates the iterator approach,
 * @verbatim
 * 1) for each level L in configuration path P
 * 1.1) open directory D given by relative path
 * 1.2) Read directory D
 *      - If SUCCESS and L < MAX_LEVELS
 *              - increment L and repeat from step 1
 *      - If end of directory and L > 0
 *              - decrement L and repeat from step 1
 *      - If end of directory and L == 0
 *              return -ENODATA
 *@endverbatim
 * Configuration directory iterator allows user to break the iteration without
 * traversing the entire directory path in the need of the situation.
 *
 * <hr> <!------------------------------------------------------------>
 * @subsection confditer-lspec-fini Configuration directory finalisation
 *
 * Configuration directory iterator finalises all its levels and its
 * corresponding configuration objects in-case the iterator did not
 * complete the full directory path traversal.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-conformance Conformance
 *
 * - @b i.conf.diter.async
 *   Conf directory iterator provides asynchronous interface to iterate
 *   configuration directory using m0_confc_open() and m0_confc_readdir()
 *   interfaces along with struct m0_confc_ctx.
 *
 * - @b r.conf.diter.next.obj.filter
 *   Conf directory iterator returns configuration objects which pass
 *   the user given filter while traversing the directory path.
 *
 * - @b r.conf.diter.break
 *   Conf directory iterator allows user to break without completing the
 *   entire given directory path traversal and finalises the remaining data
 *   structure during iterator finalisation.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-ut Unit test
 *
 * - Test 01 : Travers filesystem to sdev path and verify each configuration
 *             object.
 * - Test 02 : Traverse filesystem to disk path and verify each configuration
 *             object.
 * - Test 03 : Traverse filesystem to diskv path and verify each configuration
 *             object.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-st System test
 *
 * - Test 01: Use iterator during m0t1fs setup.
 *
 * <hr> <!------------------------------------------------------------>
 * @section confditer-scalability Scalability
 *
 * Current iterator design does not impose any restrictions on the number of
 * directory levels that can be traversed.
 */

/**
 * @addtogroup confditer
 * @{
 */

enum diter_phase {
	DPH_LVL_OPEN,
	DPH_LVL_READ,
	DPH_LVL_WAIT
};

static bool diter_lvl_invariant(const struct m0_conf_diter_lvl *l)
{
	return _0C(l->dl_di != NULL) && _0C(m0_fid_is_set(&l->dl_rel_fid)) &&
	       _0C(M0_IN(l->dl_mode, (M0_DLM_DIR, M0_DLM_ENTRY))) &&
	       _0C(l->dl_lvl < l->dl_di->di_nr_lvls);
}

static bool diter_invariant(const struct m0_conf_diter *it)
{
	return _0C(it != NULL) && _0C(it->di_confc != NULL) &&
	       _0C(it->di_origin != NULL) && _0C(it->di_nr_lvls != 0) &&
	       _0C(it->di_lvls != NULL) &&
	       _0C(m0_forall(i, it->di_nr_lvls,
			     diter_lvl_invariant(&it->di_lvls[i]))) &&
	       _0C(M0_IN(it->di_phase,
			(DPH_LVL_OPEN, DPH_LVL_READ, DPH_LVL_WAIT))) &&
	       _0C(it->di_nr_lvls != 0);
}

M0_INTERNAL void m0_conf_diter_lvl_init(struct m0_conf_diter_lvl *l,
					struct m0_conf_diter *it,
					struct m0_confc *confc, uint32_t lvl,
					const struct m0_fid *path)
{
	*l = (struct m0_conf_diter_lvl){.dl_di = it,
					.dl_rel_fid = *path,
					.dl_lvl = lvl};
}

static void diter_confc_ctx_fini(struct m0_conf_diter *it,
				 struct m0_confc_ctx *ctx)
{
	if (it->di_locked)
		m0_confc_ctx_fini_locked(ctx);
	else
		m0_confc_ctx_fini(ctx);
}

M0_INTERNAL void m0_conf_diter_lvl_fini(struct m0_conf_diter_lvl *l)
{
	struct m0_confc_ctx *ctx = &l->dl_cctx[M0_DLM_DIR].lc_ctx;
	struct m0_conf_obj  *dir = l->dl_cctx[M0_DLM_DIR].lc_result;
	struct m0_conf_obj  *entry= l->dl_cctx[M0_DLM_ENTRY].lc_result;
	if (dir != NULL && dir->co_nrefs > 0)
		m0_confc_close(dir);
	if (entry != NULL && entry->co_nrefs > 0)
		m0_confc_close(entry);
	if (ctx->fc_confc != NULL)
		diter_confc_ctx_fini(l->dl_di, ctx);
}

static inline struct m0_conf_obj *diter_lvl_dir_obj(struct m0_conf_diter_lvl *l)
{
	return l->dl_cctx[M0_DLM_DIR].lc_result;
}

static inline struct m0_conf_obj *
diter_lvl_entry_obj(struct m0_conf_diter_lvl *l)
{
	return l->dl_cctx[M0_DLM_ENTRY].lc_result;
}

static inline struct m0_confc_ctx *diter_lvl_ctx(struct m0_conf_diter_lvl *l)
{
	return &l->dl_cctx[l->dl_mode].lc_ctx;
}

static inline struct m0_conf_diter_lvl *diter_lvl(struct m0_conf_diter *it)
{
	return &it->di_lvls[it->di_lvl];
}

static inline void diter_lvl_mode_set(struct m0_conf_diter_lvl *l,
				      enum m0_diter_lvl_mode mode)
{
	l->dl_mode = mode;
}

static bool diter_chan_cb(struct m0_clink *link)
{
	struct m0_conf_diter *it = container_of(link, struct m0_conf_diter,
						di_clink);
	struct m0_confc_ctx  *ctx = diter_lvl_ctx(&it->di_lvls[it->di_lvl]);

	if (m0_confc_ctx_is_completed(ctx)) {
		m0_chan_signal_lock(&it->di_wait);
		m0_clink_del(link);
	}
	return true;
}

M0_INTERNAL int m0_conf__diter_init(struct m0_conf_diter *it,
				    struct m0_confc *confc,
				    struct m0_conf_obj *origin,
				    uint32_t nr_lvls,
				    const struct m0_fid *path)
{
	int lvl;

	M0_ENTRY("it=%p confc=%p origin=" FID_F " nr_lvls=%u path=" FID_F,
		 it, confc, FID_P(&origin->co_id), nr_lvls, FID_P(path));
	M0_PRE(origin->co_status == M0_CS_READY);

	*it = (struct m0_conf_diter){ .di_confc = confc,
				      .di_nr_lvls = nr_lvls,
				      .di_origin = origin,
				      .di_phase = DPH_LVL_OPEN};

	M0_ALLOC_ARR(it->di_lvls, it->di_nr_lvls);
	if (it->di_lvls == NULL)
		return M0_ERR(-ENOMEM);
	for (lvl = 0; lvl < nr_lvls; ++lvl)
		m0_conf_diter_lvl_init(&it->di_lvls[lvl], it, confc, lvl,
				       &path[lvl]);
	m0_clink_init(&it->di_clink, diter_chan_cb);
	m0_mutex_init(&it->di_wait_mutex);
	m0_chan_init(&it->di_wait, &it->di_wait_mutex);

	M0_POST(diter_invariant(it));
	return M0_RC(0);
}

M0_INTERNAL void m0_conf_diter_fini(struct m0_conf_diter *it)
{
	int lvl;

	M0_ENTRY();

	M0_PRE(diter_invariant(it));

	for (lvl = 0; lvl < it->di_nr_lvls; ++lvl)
		m0_conf_diter_lvl_fini(&it->di_lvls[lvl]);
	m0_free(it->di_lvls);
	m0_clink_fini(&it->di_clink);
	m0_chan_fini_lock(&it->di_wait);
	m0_mutex_fini(&it->di_wait_mutex);

	M0_LEAVE();
}

M0_INTERNAL void m0_conf_diter_locked_set(struct m0_conf_diter *it,
					  bool locked)
{
	it->di_locked = locked;
}

M0_INTERNAL void m0_conf_diter_wait_arm(struct m0_conf_diter *it,
					struct m0_clink *clink)
{
	M0_PRE(diter_invariant(it));

	m0_clink_add_lock(&it->di_wait, clink);
}

static void diter_wait_arm(struct m0_conf_diter *it, struct m0_confc_ctx *ctx)
{
	if (it->di_locked)
		m0_clink_add(&ctx->fc_mach.sm_chan, &it->di_clink);
	else
		m0_clink_add_lock(&ctx->fc_mach.sm_chan, &it->di_clink);
}

static void diter_wait_cancel(struct m0_clink *clink, bool locked)
{
	if (locked)
		m0_clink_del(clink);
	else
		m0_clink_del_lock(clink);
}

static void diter_lvl_result_set(struct m0_conf_diter_lvl *l,
				 struct m0_conf_obj *result)
{
	l->dl_cctx[l->dl_mode].lc_result = result;
}

static struct m0_conf_obj *diter_lvl_origin(struct m0_conf_diter *it)
{
	struct m0_conf_obj       *origin = NULL;
	struct m0_conf_diter_lvl *curr_lvl;
	struct m0_conf_diter_lvl *prev_lvl;

	curr_lvl = diter_lvl(it);
	switch (curr_lvl->dl_mode) {
	case M0_DLM_DIR:
		M0_ASSERT(it->di_phase == DPH_LVL_OPEN);
		if (it->di_lvl == 0 && curr_lvl->dl_nr_open == 1)
			origin = it->di_origin;
		else {
			prev_lvl = &it->di_lvls[it->di_lvl - 1];
			origin = diter_lvl_entry_obj(prev_lvl);
		}
		break;
	case M0_DLM_ENTRY:
		M0_ASSERT(it->di_phase == DPH_LVL_READ);
		origin = diter_lvl_dir_obj(curr_lvl);
		break;
	default:
		M0_IMPOSSIBLE("Invalid level mode");
	}

	return origin;
}

static int diter_wait(struct m0_conf_diter *it)
{
	struct m0_conf_diter_lvl *lvl;
	struct m0_confc_ctx      *ctx;
	struct m0_conf_obj       *prev;
	int                       rc;

	M0_ENTRY();

	lvl = diter_lvl(it);
	ctx = diter_lvl_ctx(lvl);
	prev = diter_lvl_dir_obj(lvl);
	rc = it->di_locked ? m0_confc_ctx_error(ctx) :
			     m0_confc_ctx_error_lock(ctx);
	if (rc != 0) {
		diter_confc_ctx_fini(it, ctx);
		return M0_ERR(rc);
	}
	diter_lvl_result_set(lvl, m0_confc_ctx_result(ctx));
	diter_confc_ctx_fini(it, ctx);
	if (lvl->dl_mode == M0_DLM_DIR) {
		if (prev != NULL)
			m0_confc_close(prev);
		it->di_phase = DPH_LVL_READ;
		return M0_RC(0);
	}

	/* We go depth first. */
	it->di_phase = (it->di_lvl < it->di_nr_lvls - 1) ?
			DPH_LVL_OPEN : DPH_LVL_READ;

	M0_POST(lvl->dl_mode == M0_DLM_ENTRY);
	return M0_RC(M0_CONF_DIRNEXT);
}

static void diter_lvl_check(struct m0_conf_diter *it)
{
	struct m0_conf_diter_lvl *lvl;

	M0_ENTRY();

	lvl = diter_lvl(it);
	if (it->di_lvl < it->di_nr_lvls - 1 && lvl->dl_mode == M0_DLM_ENTRY)
		M0_CNT_INC(it->di_lvl);

	M0_LEAVE();
}

M0_INTERNAL int diter_lvl_open(struct m0_conf_diter *it)
{
	struct m0_confc_ctx      *ctx;
	struct m0_conf_obj       *origin;
	struct m0_conf_diter_lvl *lvl;

	M0_ENTRY();

	diter_lvl_check(it);
	lvl = diter_lvl(it);
	M0_CNT_INC(lvl->dl_nr_open);
	/* Set level mode to M0_DLM_DIR to use appropriate m0_confc_ctx. */
	diter_lvl_mode_set(lvl, M0_DLM_DIR);
	ctx = diter_lvl_ctx(lvl);
	origin = diter_lvl_origin(it);
	m0_confc_ctx_init(diter_lvl_ctx(lvl), it->di_confc);
	diter_wait_arm(it, ctx);
	m0_confc_open(ctx, origin, lvl->dl_rel_fid);
	it->di_phase = DPH_LVL_WAIT;

	return M0_RC(M0_CONF_DIRMISS);
}

static int diter_lvl_read(struct m0_conf_diter *it)
{
	struct m0_confc_ctx      *ctx;
	struct m0_conf_diter_lvl *lvl;
	struct m0_conf_obj       *origin;
	struct m0_conf_obj       *entry = NULL;
	int                       rc;

	M0_ENTRY();

	lvl = diter_lvl(it);
	M0_CNT_INC(lvl->dl_nr_read);
	/* Set level mode to M0_DLM_ENTRY to use appropriate m0_confc_ctx. */
	diter_lvl_mode_set(lvl, M0_DLM_ENTRY);
	ctx = diter_lvl_ctx(lvl);
	origin = diter_lvl_origin(it);
	M0_ASSERT(origin != NULL);
	/*
	 * Fetch previous entry configuration object to continue directory
	 * traversal.
	 */
	entry = diter_lvl_entry_obj(lvl);
	m0_confc_ctx_init(ctx, it->di_confc);
	/* Wait on m0_confc_ctx's sm chan to receive completion event. */
	diter_wait_arm(it, ctx);
	rc = m0_confc_readdir(ctx, origin, &entry);
	if (rc != M0_CONF_DIRMISS)
		diter_wait_cancel(&it->di_clink, it->di_locked);
	switch (rc) {
	case M0_CONF_DIREND:
		if (it->di_lvl == 0) {
			rc = -ENODATA;
		} else {
			/*
			 * We have read all the configuration objects at the
			 * current level, go back to previous level and read next
			 * configuration object.
			 */
			M0_CNT_DEC(it->di_lvl);
			rc = 0;
		}
		diter_lvl_result_set(lvl, NULL);
		/* Finalise m0_confc_ctx to re-use next time. */
		diter_confc_ctx_fini(it, ctx);
		break;
	case M0_CONF_DIRMISS:
		diter_lvl_result_set(lvl, entry);
		it->di_phase = DPH_LVL_WAIT;
		break;
	case M0_CONF_DIRNEXT:
		diter_lvl_result_set(lvl, entry);
		diter_confc_ctx_fini(it, ctx);
		/* We go depth first. */
		it->di_phase = (it->di_lvl < it->di_nr_lvls - 1) ?
				DPH_LVL_OPEN : it->di_phase;
		break;
	default:
		M0_IMPOSSIBLE("Invalid case !!");
	}

	M0_POST(ergo(rc > 0, M0_IN(rc, (M0_CONF_DIRNEXT,
					M0_CONF_DIRMISS))));
	return M0_RC(rc);
}

M0_INTERNAL int
m0_conf_diter_next(struct m0_conf_diter *it,
		   bool (*filter)(const struct m0_conf_obj *obj))
{
	static int (*dit_action[])(struct m0_conf_diter *it) = {
		[DPH_LVL_OPEN] = diter_lvl_open,
		[DPH_LVL_WAIT] = diter_wait,
		[DPH_LVL_READ] = diter_lvl_read
	};
	int          rc;

	M0_ENTRY("iterator level=%u phase=%u", it->di_lvl, it->di_phase);

	do {
		M0_ASSERT(diter_invariant(it));
		rc = dit_action[it->di_phase](it);
	} while (rc == 0 ||
		 (rc == M0_CONF_DIRNEXT && filter != NULL &&
		 !filter(m0_conf_diter_result(it))));

	return M0_RC(rc == -ENODATA ? 0 : rc);
}

M0_INTERNAL int
m0_conf_diter_next_sync(struct m0_conf_diter *it,
			bool (*filter)(const struct m0_conf_obj *obj))
{
	struct m0_clink link;
	int             rc;

	M0_ENTRY();

	m0_clink_init(&link, NULL);
	do {
		m0_conf_diter_wait_arm(it, &link);
		rc = m0_conf_diter_next(it, filter);
		if (rc == M0_CONF_DIRMISS)
			m0_chan_wait(&link);
		diter_wait_cancel(&link, false);
	} while (rc == M0_CONF_DIRMISS);

	return M0_RC(rc);
}

M0_INTERNAL struct m0_conf_obj *
m0_conf_diter_result(const struct m0_conf_diter *it)
{
	struct m0_conf_diter_lvl *lvl;

	M0_ENTRY();

	lvl = &it->di_lvls[it->di_lvl];
	M0_ASSERT(lvl->dl_mode == M0_DLM_ENTRY);

	M0_LEAVE();
	return diter_lvl_entry_obj(lvl);
}

/** @} endgroup confditer */
#undef M0_TRACE_SUBSYSTEM
