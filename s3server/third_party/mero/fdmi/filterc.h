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

#ifndef __MERO_FDMI_FILTERC_H__
#define __MERO_FDMI_FILTERC_H__

#include "rpc/rpclib.h"
#include "conf/confc.h"
#include "fdmi/fdmi.h"

struct m0_mero;

/**
 * @defgroup FDMI_DLD_fspec_filterc FDMI filter client (filterC)
 * @ingroup fdmi_main
 * @see @ref FDMI-DLD-fspec "FDMI Functional Specification"
 *
 * @{
 */

struct m0_filterc_ctx;
struct m0_filterc_iter;

struct m0_filterc_ops {
/**
 * Start filterC instance. Should be called before
 * opening any filterC iterator. After successful start
 * any number of fco_open/fco_close calls is allowed.
 *
 * @param ctx         filterC context
 * @param reqh        request handler
 * @return 0 if filterC is started successfully; @n
 *         error code otherwise
 */
	int  (*fco_start)(struct m0_filterc_ctx *ctx,
			  struct m0_reqh 	*reqh);
/**
 * Stop filterC instance.
 *
 * @param ctx        filterC context
 */
	void (*fco_stop)(struct m0_filterc_ctx *ctx);
/**
 * Open iterator for traversing FDMI filters with provided
 * FDMI record type ID
 *
 * @param  ctx          FDMI filter client context
 * @param  rec_type_id  FDMI filter client context
 * @param  iter         Iterator to be opened
 *
 * @return 0 if filters for provided FDMI record type are found
 *         and iterator is opened successfully; @n
 *         error code otherwise
 */
	int  (*fco_open)(struct m0_filterc_ctx   *ctx,
                        enum m0_fdmi_rec_type_id  rec_type_id,
                        struct m0_filterc_iter   *iter);
/**
 * Get next filter using iterator
 *
 * @param iter  FDMI filters iterator
 * @param out   output filter
 *
 * @return 0  if there is no available filter (all filters are traversed) @n
 *         >0 if out parameter points to next filter @n
 *         <0 some error occurred
 */
	int  (*fco_get_next)(struct m0_filterc_iter      *iter,
	                     struct m0_conf_fdmi_filter **out);
/**
 * Closes FDMI filters iterator
 *
 * @param iter Iterator to be closed
 */
	void (*fco_close)(struct m0_filterc_iter *iter);
};

extern const struct m0_filterc_ops filterc_def_ops;

/**
 * FDMI filter client context
 */
struct m0_filterc_ctx {
	/** Getting filters from this rconfc */
	struct m0_confc                 *fcc_confc;
	/** FilterC operations */
	const struct m0_filterc_ops     *fcc_ops;
};

/**
 * Iterator to traverse filters
 *
 * It stores pointers to directory of filters with the
 * same FDMI record type and to current filter in this directory.
 *
 * Iterator is initialized with @ref m0_filterc_ops::fco_open,
 * iteration is done via @ref m0_filterc_ops::fco_get_next.
 * Iterator deinitializes with @ref m0_filterc_ops::fco_close.
 */
struct m0_filterc_iter {
	struct m0_filterc_ctx       *fci_filterc_ctx;
	struct m0_conf_obj          *fci_dir;
	struct m0_conf_fdmi_filter  *fci_cur_flt;
};

/**
 * Initialize filter client context
 * @param ctx  filterC context
 * @param ops  filterC operations. Normally @ref filterc_def_ops should be
 *             passed, but UT can provide its own implementation
 */
M0_INTERNAL void m0_filterc_ctx_init(struct m0_filterc_ctx       *ctx,
                                     const struct m0_filterc_ops *ops);

/**
 * Finalize filter client context
 * @param ctx filterC context
 */
M0_INTERNAL void m0_filterc_ctx_fini(struct m0_filterc_ctx *ctx);

/** @} end of FDMI_DLD_fspec_filterc */

#endif /* __MERO_FDMI_FDMI_SERVICE_H__ */

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
