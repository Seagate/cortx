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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 14-Apr-2016
 */

#pragma once

#ifndef __MERO_STOB_IOQ_ERROR_H__
#define __MERO_STOB_IOQ_ERROR_H__

#include "lib/types.h"  /* m0_bindex_t */
#include "fid/fid.h"    /* m0_fid */
#include "stob/stob.h"  /* m0_stob_id */
#include "stob/io.h"    /* m0_stob_io_opcode */
#include "stob/stob_xc.h"       /* XXX workaround */

/**
 * @defgroup stob
 *
 * @{
 */

struct m0_stob_ioq_error {

	/* stob info */

	/** m0_stob_linux::sl_conf_sdev */
	struct m0_fid          sie_conf_sdev;
	/** m0_stob::so_id */
	struct m0_stob_id      sie_stob_id;
	/** m0_stob_linux::sl_fd */
	int64_t                sie_fd;

	/* I/O info */

	/* enum m0_stob_io_opcode */
	int64_t                sie_opcode;
	int64_t                sie_rc;
	m0_bindex_t            sie_offset;
	m0_bcount_t            sie_size;
	uint32_t               sie_bshift;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** @} end of stob group */
#endif /* __MERO_STOB_IOQ_ERROR_H__ */

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
