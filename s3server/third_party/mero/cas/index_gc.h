/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Evgeny Exarevskiy  <evgeny.exarevskiy@seagate.com>
 * Original creation date: 15-Nov-2016
 */

#pragma once

#ifndef __MERO_CAS_INDEX_GC_H__
#define __MERO_CAS_INDEX_GC_H__

/* Import */
struct m0_reqh;
struct m0_be_op;

/** Initialises index garbage collector. */
M0_INTERNAL void m0_cas_gc_init(void);

/** Finalises index garbage collector. */
M0_INTERNAL void m0_cas_gc_fini(void);

/**
 * Instructs index garbage collector to destroy all indices referenced in "dead
 * index" catalogue.
 *
 * After all records in "dead index" catalogue are processed a garbage collector
 * stops. In other words, garbage collector doesn't monitor the contents of
 * "dead index" catalogue and this function should be called every time when
 * user decides to destroy catalogues referenced in "dead index" catalogue.
 *
 * Can be called when garbage collector is already started. In this case garbage
 * collector will not stop after destroying catalogues and will recheck "dead
 * index" contents for new records.
 */
M0_INTERNAL void m0_cas_gc_start(struct m0_reqh_service *service);

/**
 * Asynchronously waits until garbage collector stops.
 *
 * Provided 'beop' will be moved to M0_BOS_DONE state once garbage collector
 * stops.
 */
M0_INTERNAL void m0_cas_gc_wait_async(struct m0_be_op *beop);

/**
 * Synchronously waits until garbage collector stops.
 */
M0_INTERNAL void m0_cas_gc_wait_sync(void);

#endif /* __MERO_CAS_INDEX_GC_H__ */

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
