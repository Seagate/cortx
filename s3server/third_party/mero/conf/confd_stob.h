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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 19-May-2015
 */

#pragma once
#ifndef __MERO_CONF_CONFD_STOB_H__
#define __MERO_CONF_CONFD_STOB_H__

#include "fid/fid.h"
#include "fop/fom.h"
#include "stob/domain.h"
#include "stob/stob.h"

/**
   @addtogroup confd_stob
   @{
 */

/**
 * Generate IO stob fid for confd file configuration.
 */
#define M0_CONFD_FID(old_version, new_version, tx_id)   \
	M0_FID_INIT((uint64_t)(old_version) << 32 | (uint64_t)(new_version), \
		    (tx_id))

/**
 * Initialise stob for confd file configuration.
 * Create/init stob domain by location and create/init stob by FID.
 *
 * @pre stob != NULL
 * @pre location != NULL
 * @pre fid != NULL
 */
M0_INTERNAL int m0_confd_stob_init(struct m0_stob **stob,
				   const char      *location,
				   struct m0_fid   *confd_fid);

/**
 * Finalize stob for confd file configuration
 * Finalize stob domain and put stob.
 *
 * @pre stob != NULL
 */
M0_INTERNAL void m0_confd_stob_fini(struct m0_stob *stob);

/**
 * Reads data from stob as simple string
 * Get data length (if stob type is linuxstob), allocate memory for string and
 * read data.
 * Note! For allocate uses m0_alloc_alingned. Free must be used like
 * m0_free_aligned(str, strlen(str) + 1, m0_stob_block_shift(stob))
 *
 * @pre stob != NULL
 * @pre str != NULL
 */
M0_INTERNAL int m0_confd_stob_read(struct m0_stob *stob, char **str);

/**
 * Writes data to stob
 *
 * @pre stob != NULL
 * @pre bufvec != NULL
 */
M0_INTERNAL int m0_confd_stob_bufvec_write(struct m0_stob   *stob,
					   struct m0_bufvec *bufvec);

/**
 * Writes data to stob as simple string
 *
 * @pre stob != NULL
 * @pre str != NULL
 * @pre m0_addr_is_aligned(str, m0_stob_block_shift(stob))
 */
M0_INTERNAL int m0_confd_stob_write(struct m0_stob *stob, char *str);

/**
 * Generates stob domain location
 * Result location is "linuxstob:PATH/configure", where PATH is path to
 * confd configure file.
 *
 * @pre fom != NULL
 * @pre location != NULL
 */
M0_INTERNAL int m0_conf_stob_location_generate(struct m0_fom  *fom,
					       char          **location);

/** @} end group confd_stob */
#endif /* __MERO_CONF_CONFD_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
