/* -*- C -*- */
/*
 * COPYRIGHT 2017 SEAGATE TECHNOLOGY LIMITED
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
 * https://www.seagate.com/contacts
 *
 * Original author: Jean-Philippe Bernardy <jean-philippe.bernardy@tweag.io>
 * Original creation date: 15 Feb 2016
 * Modifications: Nachiket Sahasrabudhe <nachiket.sahasrabuddhe@seagate.com>
 * Date of modification: 27 Nov 2017
 */

#pragma once

#ifndef __MERO_ISCSERVICE_ISC_SERVICE_H__
#define __MERO_ISCSERVICE_ISC_SERVICE_H__

#include "reqh/reqh_service.h" /* m0_reqh_service */
#include "lib/hash.h"          /* m0_hlink */

struct m0_isc_comp_private;

/**
 * State of a computation with respect to its registration with the
 * ISC service.
 */
enum m0_isc_comp_state {
	/** If a computation is present in hash-table. */
	M0_ICS_REGISTERED,
	/** If a computation is not present in hash-table. */
	M0_ICS_UNREGISTERED,
};

/**
 * Represents a computation abstraction. This structure resides with the
 * hash table that's part of ISC service. A concurrent access to a computation
 * is handled by concurrent hash table.
 */
struct m0_isc_comp {
	/** A unique identifier for a computation. */
	struct m0_fid           ic_fid;
	/** Human readable name of the computation. */
	char                   *ic_name;
	/** A linkage in hash-table for storing computations. */
	struct m0_hlink         ic_hlink;
	/** A generation count for cookie associated with this computation. */
	uint64_t                ic_gen;
	/**
	 * A pointer to operation. The output of the function is populated
	 * in result and caller is expected to free it after usage.
	 */
	int                   (*ic_op)(struct m0_buf *args_in,
				       struct m0_buf *result,
				       struct m0_isc_comp_private *comp_data,
				       int *rc);
	/** Indicates one of the states from m0_isc_comp_state. */
	 enum m0_isc_comp_state ic_reg_state;
	/** Count for ongoing instances of the operation. */
	uint32_t                ic_ref_count;
	uint64_t                ic_magic;
};

/**
 * ISC service that resides with Mero request handler.
 */
struct m0_reqh_isc_service {
	/** Generic reqh service object */
	struct m0_reqh_service riscs_gen;
	uint64_t               riscs_magic;
};

/** Creates the hash-table of computations in m0 instance. */
M0_INTERNAL int m0_isc_mod_init(void);
M0_INTERNAL void m0_isc_mod_fini(void);

/** Returns the hash-table of computations stored with m0 instance. */
M0_INTERNAL struct m0_htable *m0_isc_htable_get(void);

M0_INTERNAL int m0_iscs_register(void);
M0_INTERNAL void m0_iscs_unregister(void);

/** Methods for hash-table holding external computations linked with Mero. */
M0_HT_DECLARE(m0_isc, M0_INTERNAL, struct m0_isc_comp, struct m0_fid);

extern struct m0_reqh_service_type m0_iscs_type;

#endif /* __MERO_ISCSERVICE_ISC_SERVICE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
