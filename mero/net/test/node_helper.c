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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 11/07/2012
 */

#include "lib/misc.h"			/* M0_IN */

#include "net/test/node_helper.h"

/**
   @defgroup NetTestNodeHelperInternals Node helper
   @ingroup NetTestInternals

   @{
 */

void m0_net_test_nh_init(struct m0_net_test_nh *nh,
			 const struct m0_net_test_cmd_init *icmd)
{
	M0_PRE(nh != NULL);
	M0_PRE(!nh->ntnh_test_initialized);

	M0_SET0(nh);
	m0_mutex_init(&nh->ntnh_sd_copy_lock);
	nh->ntnh_role		  = icmd->ntci_role;
	nh->ntnh_type		  = icmd->ntci_type;
	nh->ntnh_test_initialized = true;
	nh->ntnh_transfers_max_nr = icmd->ntci_msg_nr == 0 ?
				    UINT64_MAX : icmd->ntci_msg_nr;
	/** @todo reset all stats */
	m0_net_test_stats_reset(&nh->ntnh_sd.ntcsd_rtt);
}

bool m0_net_test_nh__invariant(struct m0_net_test_nh *nh)
{
	return nh != NULL;
}

void m0_net_test_nh_fini(struct m0_net_test_nh *nh)
{
	M0_PRE(m0_net_test_nh__invariant(nh));

	m0_mutex_fini(&nh->ntnh_sd_copy_lock);
}

void m0_net_test_nh_sd_copy_locked(struct m0_net_test_nh *nh)
{
	M0_PRE(m0_net_test_nh__invariant(nh));

	m0_mutex_lock(&nh->ntnh_sd_copy_lock);
	nh->ntnh_sd_copy = nh->ntnh_sd;
	m0_mutex_unlock(&nh->ntnh_sd_copy_lock);
}

void m0_net_test_nh_sd_get_locked(struct m0_net_test_nh *nh,
				  struct m0_net_test_cmd_status_data *sd)
{
	M0_PRE(m0_net_test_nh__invariant(nh));

	m0_mutex_lock(&nh->ntnh_sd_copy_lock);
	*sd = nh->ntnh_sd_copy;
	sd->ntcsd_time_now = m0_time_now();
	m0_mutex_unlock(&nh->ntnh_sd_copy_lock);
}

void m0_net_test_nh_sd_update(struct m0_net_test_nh *nh,
			      enum m0_net_test_nh_msg_type type,
			      enum m0_net_test_nh_msg_status status,
			      enum m0_net_test_nh_msg_direction direction)
{
	struct m0_net_test_cmd_status_data *sd = &nh->ntnh_sd;
	struct m0_net_test_msg_nr	   *msg_nr;
	struct m0_net_test_mps		   *mps;

	M0_PRE(m0_net_test_nh__invariant(nh));
	M0_PRE(M0_IN(type, (MT_MSG, MT_BULK, MT_TRANSFER)));
	M0_PRE(M0_IN(status, (MS_SUCCESS, MS_FAILED, MS_BAD)));
	M0_PRE(M0_IN(direction, (MD_SEND, MD_RECV, MD_BOTH)));
	M0_PRE(equi(type == MT_TRANSFER, direction == MD_BOTH));

	if (type == MT_MSG) {
		msg_nr = direction == MD_SEND ? &sd->ntcsd_msg_nr_send :
						&sd->ntcsd_msg_nr_recv;
	} else if (type == MT_BULK) {
		msg_nr = direction == MD_SEND ? &sd->ntcsd_bulk_nr_send :
						&sd->ntcsd_bulk_nr_recv;
	} else {
		msg_nr = &sd->ntcsd_transfers;
	}

	/* update 'number of messages' statistics */
	++msg_nr->ntmn_total;
	msg_nr->ntmn_failed += status == MS_FAILED;
	msg_nr->ntmn_bad    += status == MS_BAD;
	/* update 'messages per second' statistics */
	if (type != MT_TRANSFER &&
	    equi(type == MT_MSG, nh->ntnh_type == M0_NET_TEST_TYPE_PING)) {
		mps = direction == MD_SEND ? &sd->ntcsd_mps_send :
					     &sd->ntcsd_mps_recv;
		m0_net_test_mps_add(mps, msg_nr->ntmn_total, m0_time_now());
	}
	if (type == MT_TRANSFER && nh->ntnh_role == M0_NET_TEST_ROLE_CLIENT) {
		/* update 'finished' flag */
		M0_ASSERT(msg_nr->ntmn_total <= nh->ntnh_transfers_max_nr);
		if (msg_nr->ntmn_total == nh->ntnh_transfers_max_nr) {
			sd->ntcsd_time_finish = m0_time_now();
			sd->ntcsd_finished = true;
		}
	}
}

void m0_net_test_nh_sd_update_rtt(struct m0_net_test_nh *nh, m0_time_t rtt)
{
	M0_PRE(m0_net_test_nh__invariant(nh));

	m0_net_test_stats_time_add(&nh->ntnh_sd.ntcsd_rtt, rtt);
}

bool m0_net_test_nh_transfer_next(struct m0_net_test_nh *nh)
{
	M0_PRE(m0_net_test_nh__invariant(nh));

	if (nh->ntnh_transfers_started_nr == nh->ntnh_transfers_max_nr)
		return false;
	++nh->ntnh_transfers_started_nr;
	return true;
}

void m0_net_test_nh_cmd_status(struct m0_net_test_nh *nh,
			       const struct m0_net_test_cmd *cmd,
			       struct m0_net_test_cmd *reply)
{
	M0_PRE(m0_net_test_nh__invariant(nh));
	M0_PRE(cmd != NULL && cmd->ntc_type == M0_NET_TEST_CMD_STATUS);
	M0_PRE(reply != NULL);

	reply->ntc_type = M0_NET_TEST_CMD_STATUS_DATA;
	m0_net_test_nh_sd_get_locked(nh, &reply->ntc_status_data);
}

/**
   @} end of NetTestNodeHelperInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
