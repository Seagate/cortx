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

#pragma once

#ifndef __MERO_FDMI_SOURCE_DOCK_INTERNAL_H__
#define __MERO_FDMI_SOURCE_DOCK_INTERNAL_H__

#include "lib/types.h"

#include "fdmi/fdmi.h"
#include "fdmi/source_dock.h"
#include "fdmi/filterc.h"
#include "fdmi/flt_eval.h"
#include "rpc/conn_pool.h"

/* This file describes FDMI source dock internals */

/**
   @defgroup fdmi_sd_int FDMI Source Dock internals
   @ingroup fdmi_main

   @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
   @{
*/

/** FDMI source context data */
struct m0_fdmi_src_ctx {
	uint64_t                fsc_magic;

	/** FDMI source */
	struct m0_fdmi_src      fsc_src;

	/** Service field for linked list */
	struct m0_tlink         fsc_linkage;

	/** Source is registered.
	 *  Flag is cleared if m0_fdmi_source_deregister() is called */
	bool                    fsc_registered;
};

M0_TL_DESCR_DECLARE(fdmi_record_list, M0_EXTERN);
M0_TL_DECLARE(fdmi_record_list, M0_EXTERN, struct m0_fdmi_src_rec);

M0_TL_DESCR_DECLARE(fdmi_matched_filter_list, M0_EXTERN);
M0_TL_DECLARE(fdmi_matched_filter_list, M0_EXTERN, struct m0_conf_fdmi_filter);

/** FDMI source dock FOM */
struct fdmi_sd_fom {
	uint64_t                fsf_magic;
	struct m0_fom           fsf_fom;
	struct m0_sm_ast        fsf_wakeup_ast;
	struct m0_filterc_ctx   fsf_filter_ctx;
	struct m0_filterc_iter  fsf_filter_iter;
	struct m0_fdmi_eval_ctx fsf_flt_eval;
	struct m0_rpc_conn_pool fsf_conn_pool;
	struct m0_tl            fsf_pending_fops;
	/** Mutex to protect list of pending fops. */
	struct m0_mutex         fsf_pending_fops_lock;
	struct m0_semaphore     fsf_shutdown;
	char                   *fsf_client_ep;
};

/** FDMI source dock Release Record FOM */
struct fdmi_rr_fom {
	uint64_t                frf_magic;
	struct m0_fom           frf_fom;
};

/** FDMI source dock main context */
struct m0_fdmi_src_dock {
	/** FDMI source dock started flag */
	bool                  fsdc_started;

	/**
	   List of registered FDMI source instances.
	   Links using m0_fdmi_src_ctx.fsc_linkage
	 */
	struct m0_tl          fsdc_src_list;

	/**
	   FDMI record context data list, stores records
	   posted by source until they are handled by FDMI source dock.
	   Links using link m0_fdmi_src_rec.fsr_linkage. Protected with
	   ->fsdc_list_mutex.
	 */
	struct m0_tl          fsdc_posted_rec_list;

	/** Mutex to protect ->fsdc_posted_rec_list list operations. */
	struct m0_mutex       fsdc_list_mutex;

	/** Cluster-wide unique source-dock instance ID.  Used as u_hi part of
	 * fdmi_rec_id.  Changes on restart. */
	uint64_t	      fsdc_instance_id;

	/** FDMI source dock FOM object */
	struct fdmi_sd_fom    fsdc_sd_fom;
};

/**
 * Function posts new fdmi data for analysis by FDMI source dock.
 * @return Returns 0 on success, error code otherwise.
 */
M0_INTERNAL int m0_fdmi__record_post(struct m0_fdmi_src_rec *src_rec);

/**
 * Function generates new fdmi record ID unque across the cluster.
 */
M0_INTERNAL void m0_fdmi__rec_id_gen(struct m0_fdmi_src_rec *src_rec);

/** Initialise source dock fom */
M0_INTERNAL void m0_fdmi__src_dock_fom_init(void);

/** Starts source dock fom to handle posted FDMI records */
M0_INTERNAL int m0_fdmi__src_dock_fom_start(
		struct m0_fdmi_src_dock     *src_dock,
		const struct m0_filterc_ops *filterc_ops,
		struct m0_reqh *reqh);

/** Stop source dock fom to handle posted FDMI records */
M0_INTERNAL void
m0_fdmi__src_dock_fom_stop(struct m0_fdmi_src_dock *src_dock);

/** Returns registered source context */
M0_INTERNAL struct m0_fdmi_src_ctx *m0_fdmi__src_ctx_get(
	enum m0_fdmi_rec_type_id    src_type_id);

/**
   Helper function, call "increment reference counter" function for the record
 */
M0_INTERNAL void m0_fdmi__fs_get(struct m0_fdmi_src_rec *src_rec);

/**
   Helper function, call "decrement reference counter" function for the source
 */
M0_INTERNAL void m0_fdmi__fs_put(struct m0_fdmi_src_rec *src_rec);

/** Helper function, call "processing start" for the source */
M0_INTERNAL void m0_fdmi__fs_begin(struct m0_fdmi_src_rec *src_rec);

/** Helper function, call "processing complete" for the source */
M0_INTERNAL void m0_fdmi__fs_end(struct m0_fdmi_src_rec *src_rec);

/** Initialize FDMI source record */
M0_INTERNAL void m0_fdmi__record_init(struct m0_fdmi_src_rec *src_rec);

/** Deinitialize and release FDMI record context */
M0_INTERNAL void m0_fdmi__record_deinit(struct m0_fdmi_src_rec *src_rec);

/** Helper function, returns FDMI record type enumeration */
M0_INTERNAL enum m0_fdmi_rec_type_id
m0_fdmi__sd_rec_type_id_get(struct m0_fdmi_src_rec *src_rec);

/** Handles received reply for sent FDMI record. */
M0_INTERNAL int m0_fdmi__handle_reply(struct m0_fdmi_src_dock *sd_ctx,
				      struct m0_fdmi_src_rec  *src_rec,
				      int                      send_res);

/** Release FDMI record handle by record id */
M0_INTERNAL int m0_fdmi__handle_release(struct m0_uint128 *fdmi_rec_id);

/** Wakeup FDMI source dock fom */
M0_INTERNAL void m0_fdmi__src_dock_fom_wakeup(struct fdmi_sd_fom *sd_fom);

/** @} end of fdmi_sd_int group */

#endif /* __MERO_FDMI_SOURCE_DOCK_INTERNAL_H__ */

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
