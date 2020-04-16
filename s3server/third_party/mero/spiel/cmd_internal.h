/* -*- C -*- */
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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 17-Aug-2015
 */

#pragma once

#ifndef __MERO_SPIEL_CMD_INTERNAL_H__
#define __MERO_SPIEL_CMD_INTERNAL_H__

/**
 * @addtogroup spiel-api-fspec-intr
 * @{
 */

#define SPIEL_CONF_OBJ_FIND(confc, fid, conf_obj, filter, ...) \
	_spiel_conf_obj_find(confc, fid, filter,               \
			     M0_COUNT_PARAMS(__VA_ARGS__) + 1, \
			     (const struct m0_fid []){         \
			     __VA_ARGS__, M0_FID0 },           \
			     conf_obj)

#define SPIEL_CONF_DIR_ITERATE(confc, ctx, iter_cb, ...)             \
	_spiel_conf_dir_iterate(confc, ctx, iter_cb,                 \
				M0_COUNT_PARAMS(__VA_ARGS__) + 1,    \
				(const struct m0_fid []){            \
				__VA_ARGS__, M0_FID0 })

enum {
	SPIEL_MAX_RPCS_IN_FLIGHT = 1,
	SPIEL_CONN_TIMEOUT       = 5, /* seconds */
};

#define SPIEL_DEVICE_FORMAT_TIMEOUT   m0_time_from_now(10*60, 0)

struct spiel_string_entry {
	char            *sse_string;
	struct m0_tlink  sse_link;
	uint64_t         sse_magic;
};

/****************************************************/
/*                    Filesystem                    */
/****************************************************/

/** filesystem stats collection context */
struct _fs_stats_ctx {
	/**
	 * The most recent retcode. Normally it must be zero during all the
	 * stats collection. And when it becomes non-zero, the stats collection
	 * is interrupted, and the retcode is conveyed to client.
	 */
	int                   fx_rc;
	struct m0_spiel_core *fx_spc;         /**< spiel instance      */
	uint32_t              fx_svc_total;   /**< fs total IOS and MDS count  */
	uint32_t              fx_svc_processed; /**< count queried services */
	uint32_t              fx_svc_replied; /**< fs services replied to call */
	m0_bcount_t           fx_free_seg;    /**< free space in BE segments */
	m0_bcount_t           fx_total_seg;   /**< total space in BE segments */
	m0_bcount_t           fx_free_disk;   /**< free space on disks */
	m0_bcount_t           fx_avail_disk;  /**< space available for user data */
	m0_bcount_t           fx_total_disk;  /**< total space on disks */
	struct m0_tl          fx_items;       /**< m0_fid_item list    */
	/** stats item type to be enlisted */
	const struct m0_conf_obj_type *fx_type;
	struct m0_mutex       fx_guard;       /** protects access to fields */
	/** Barrier for waiting requests completion */
	struct m0_semaphore   fx_barrier;
};

#define SPIEL_LOGLVL M0_DEBUG

/** @} */
#endif /* __MERO_SPIEL_CMD_INTERNAL_H__*/

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
