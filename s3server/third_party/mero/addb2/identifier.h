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
 * Original author: Nikita Danilov <nikita.danilov@seagate.com>
 * Original creation date: 25-Feb-2015
 */

#pragma once

#ifndef __MERO_ADDB2_IDENTIFIER_H__
#define __MERO_ADDB2_IDENTIFIER_H__

/**
 * @defgroup addb2
 *
 * Identifiers list
 * ----------------
 *
 * addb2/identifier.h contains the list of well-known addb2 measurements and
 * label identifiers.
 *
 * @{
 */

#include "addb2/addb2_internal.h"

enum m0_addb2_value_id {
	M0_AVI_NULL,

	M0_AVI_GENERAL_RANGE_START = 0x1000,
	/** Label: node fid. */
	M0_AVI_NODE,
	/** Label: process fid. */
	M0_AVI_PID,
	/** Measurement: current clock reading (m0_time_now()). */
	M0_AVI_CLOCK,

	M0_AVI_FOM_RANGE_START     = 0x2000,
	/** Label: locality number. */
	M0_AVI_LOCALITY,
	/** Label: thread handle. */
	M0_AVI_THREAD,
	/** Label: service fid. */
	M0_AVI_SERVICE,
	/** Label: fom address. */
	M0_AVI_FOM,
	/** Measurement: fom phase transition. */
	M0_AVI_PHASE,
	/** Measurement: fom state transition. */
	M0_AVI_STATE,
	M0_AVI_STATE_COUNTER,
	M0_AVI_STATE_COUNTER_END = M0_AVI_STATE_COUNTER + 0x100,
	/** Measurement: fom description. */
	M0_AVI_FOM_DESCR,
	/** Measurement: active foms in locality counter. */
	M0_AVI_FOM_ACTIVE,
	/** Measurement: fom SM to bulk id */
	M0_AVI_FOM_TO_BULK,
	/** Measurement: fom SM to tx SM. */
	M0_AVI_FOM_TO_TX,
	/** Measurement: fom SM to stob_io SM. */
	M0_AVI_FOM_TO_STIO,
	/** Measurement: run queue length. */
	M0_AVI_RUNQ,
	/** Measurement: wait list length. */
	M0_AVI_WAIL,
	/** Label: ast context. */
	M0_AVI_AST,
	/** Label: fom call-back context. */
	M0_AVI_FOM_CB,
	/** Label: locality chore. */
	M0_AVI_CHORE,
	/** Measurement: duration of asts posted to locality fork-queue. */
	M0_AVI_LOCALITY_FORQ_DURATION,
	/** Counter: asts posted to locality fork-queue. */
	M0_AVI_LOCALITY_FORQ,
	/** Counter: wait times on locality runrun channel. */
	M0_AVI_LOCALITY_CHAN_WAIT,
	/** Counter: call-back times on locality runrun channel. */
	M0_AVI_LOCALITY_CHAN_CB,
	/** Counter: queue-length of locality runrun channel. */
	M0_AVI_LOCALITY_CHAN_QUEUE,
	/** Counter for FOM long lock. */
	M0_AVI_LONG_LOCK,
	/** Measurement: generic attribute. */
	M0_AVI_ATTR,

	M0_AVI_LIB_RANGE_START     = 0x3000,
	/** Measurement: memory allocation. */
	M0_AVI_ALLOC,

	M0_AVI_RM_RANGE_START      = 0x4000,
	M0_AVI_M0T1FS_RANGE_START  = 0x5000,
	M0_AVI_IOS_RANGE_START     = 0x6000,
	M0_AVI_STOB_RANGE_START    = 0x7000,
	M0_AVI_RPC_RANGE_START     = 0x8000,
	M0_AVI_ADDB2_RANGE_START   = 0x9000,
	M0_AVI_BE_RANGE_START      = 0xa000,
	M0_AVI_NET_RANGE_START     = 0xb000,
	M0_AVI_CAS_RANGE_START     = 0xc000,
	M0_AVI_CLOVIS_RANGE_START  = 0xd000,
	M0_AVI_DIX_RANGE_START     = 0xe000,
	M0_AVI_KEM_RANGE_START     = 0xf000,

	/**
	 * Ranges reserved for using in external projects (S3, NFS)
	 * and m0addb2dump plugins
	 */
	M0_AVI_EXTERNAL_RANGE_1 = M0_ADDB2__EXT_RANGE_1, /* 0x0010000 */
	M0_AVI_EXTERNAL_RANGE_2 = M0_ADDB2__EXT_RANGE_2, /* 0x0020000 */
	M0_AVI_EXTERNAL_RANGE_3 = M0_ADDB2__EXT_RANGE_3, /* 0x0030000 */
	M0_AVI_EXTERNAL_RANGE_4 = M0_ADDB2__EXT_RANGE_4, /* 0x0040000 */

	/**
	 * Reserve a range of identifiers for per-fop-type per-locality
	 * counters. Identifier bits in this range are interpreted as following:
	 *
	 * @verbatim
	 * 1 FFFF FFFF FFFF CCCC TTTT TTTT
	 * @endverbatim
	 *
	 * Where
	 *
	 * FFFF FFFF FFFF: fop type identifier (rpc item opcode)
	 *           CCCC: counter identifier
	 *      TTTT TTTT: transition identifier.
	 *
	 * @see m0_fop_type_addb2_instrument().
	 */
	M0_AVI_FOP_TYPES_RANGE_START = 0x1000000,
	M0_AVI_FOP_TYPES_RANGE_END   = 0x1ffffff,
	M0_AVI_SIT,
	M0_AVI_LAST,
	/** No data. */
	M0_AVI_NODATA = 0x00ffffffffffffffull,
};

/** @} end of addb2 group */

#endif /* __MERO_ADDB2_IDENTIFIER_H__ */

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
