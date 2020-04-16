/* -*- C -*- */
/*
 * COPYRIGHT 2013 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 19-Sep-2012
 */
#pragma once
#ifndef __MERO_CONF_DB_H__
#define __MERO_CONF_DB_H__

#include "be/tx_credit.h"
#include "be/seg.h"


struct m0_confx;
struct m0_confx_obj;

/**
 * Calculates BE credits required by configuration database tables and @conf.
 */
M0_INTERNAL int m0_confdb_create_credit(struct m0_be_seg *seg,
					const struct m0_confx *conf,
					struct m0_be_tx_credit *accum);

/**
 * Creates configuration database, populating it with provided
 * configuration data.
 *
 * @pre  conf->cx_nr > 0
 */
M0_INTERNAL int m0_confdb_create(struct m0_be_seg *seg, struct m0_be_tx *tx,
                                 const struct m0_confx *conf);

/**
 * Finalises in-memory configuration database.
 */
M0_INTERNAL void m0_confdb_fini(struct m0_be_seg *seg);
/**
 * Calculates BE credits in-order to destroy configuration database from
 * persistent store.
 */
M0_INTERNAL void m0_confdb_destroy_credit(struct m0_be_seg *seg,
					  struct m0_be_tx_credit *accum);
M0_INTERNAL int m0_confdb_destroy(struct m0_be_seg *seg, struct m0_be_tx *tx);

/**
 * Creates m0_confx and populates it with data read from a
 * configuration database.
 *
 * @note If the call succeeds, the user is responsible for freeing
 *       allocated memory with m0_confx_free(*out).
 */
M0_INTERNAL int m0_confdb_read(struct m0_be_seg *seg, struct m0_confx **out);

#endif /* __MERO_CONF_DB_H__ */
