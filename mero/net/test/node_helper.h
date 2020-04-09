/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 11/07/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_NODE_HELPER_H__
#define __MERO_NET_TEST_NODE_HELPER_H__

#include "net/test/commands.h"		/* m0_net_test_role */

/**
   @defgroup NetTestNodeHelperDFS Node helper
   @ingroup NetTestDFS

   Naming in enum is without M0_xxx_xxx prefix because this file
   should be included in .c files only, and long prefixes will destroy
   usability of m0_net_test_nh_sd_update() function.

   @{
 */

/** Test message status */
enum m0_net_test_nh_msg_status {
	MS_SUCCESS,	/**< message was successfully sent&received */
	MS_FAILED,	/**< message transfer failed */
	MS_BAD,		/**< received message contains invalid data */
};

enum m0_net_test_nh_msg_type {
	MT_MSG,		/**< message with buffer descriptors */
	MT_BULK,	/**< bulk test message */
	MT_TRANSFER,
};

/** Single test message transfer direction */
enum m0_net_test_nh_msg_direction {
	MD_SEND,	/**< message was sent */
	MD_RECV,	/**< message was received */
	MD_BOTH,	/**< test message transfer in both directions */
};

/** Node helper */
struct m0_net_test_nh {
	/** Node role */
	enum m0_net_test_role		   ntnh_role;
	/** Test type */
	enum m0_net_test_type		   ntnh_type;
	/**
	 * Node stats.
	 * Usage pattern: use ntnh_sd as primary status data structure.
	 * All statistics should go directly to this structure
	 * in thread-safe manner (from one thread or using external lock).
	 * In some inner loop m0_net_test_nh_sd_copy_lock() should be called
	 * periodically to copy ntnh_sd to ntnh_sd_copy with ntnh_sd_copy_lock
	 * locked. When status data is requested (in other thread etc.)
	 * m0_net_test_nh_sd_get_lock() should be called. It will
	 * copy ntnh_sd_copy to provided structure while holding
	 * ntnh_sd_copy_lock.  This pattern will eliminate locking when
	 * updating statistics in critical testing paths at cost
	 * of delayed live stats.
	 * @todo XXX check grammar
	 */
	struct m0_net_test_cmd_status_data ntnh_sd;
	/** Copy of stats */
	struct m0_net_test_cmd_status_data ntnh_sd_copy;
	/** Lock for the copy of stats ntnh_sd_copy */
	struct m0_mutex			   ntnh_sd_copy_lock;
	/** Test was initialized. Set to true in m0_net_test_nh_init() */
	bool				   ntnh_test_initialized;
	/**
	 * Maximum number of message transfers (in both direction).
	 * Value UINT64_MAX means no limit.
	 */
	uint64_t			   ntnh_transfers_max_nr;
	/** Number of started transfers (including failed) */
	uint64_t			   ntnh_transfers_started_nr;
};

/** Initialize node helper structure. Take some information from icmd */
void m0_net_test_nh_init(struct m0_net_test_nh *nh,
			 const struct m0_net_test_cmd_init *icmd);
/** Invariant for m0_net_test_nh */
bool m0_net_test_nh__invariant(struct m0_net_test_nh *nh);
/** Finalize node helper structure */
void m0_net_test_nh_fini(struct m0_net_test_nh *nh);

/**
 * nh->ntnh_sd_copy = nh->ntnh_sd while holding nh->ntnh_sd_copy_lock.
 * @see m0_net_test_nh.ntnh_sd
 */
void m0_net_test_nh_sd_copy_locked(struct m0_net_test_nh *nh);
/**
 * *sd = nh->ntnh_sd_copy while holding nh->ntnh_sd_copy_lock.
 * Also set sd->ntcsd_time_now to the current time.
 * @see m0_net_test_nh.ntnh_sd
 */
void m0_net_test_nh_sd_get_locked(struct m0_net_test_nh *nh,
				  struct m0_net_test_cmd_status_data *sd);

/**
 * Update statistics for message numbers.
 */
void m0_net_test_nh_sd_update(struct m0_net_test_nh *nh,
			      enum m0_net_test_nh_msg_type type,
			      enum m0_net_test_nh_msg_status status,
			      enum m0_net_test_nh_msg_direction direction);
/** Update round-trip statistics */
void m0_net_test_nh_sd_update_rtt(struct m0_net_test_nh *nh, m0_time_t rtt);
/**
 * Increase number of started transfers.
 * @return false if transfers limit reached
 * @return true otherwise
 */
bool m0_net_test_nh_transfer_next(struct m0_net_test_nh *nh);

/** M0_NET_TEST_CMD_STATUS handler */
void m0_net_test_nh_cmd_status(struct m0_net_test_nh *nh,
			       const struct m0_net_test_cmd *cmd,
			       struct m0_net_test_cmd *reply);

/**
   @} end of NetTestNodeHelperDFS group
 */

#endif /*  __MERO_NET_TEST_NODE_HELPER_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
