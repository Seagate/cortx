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
 * Original creation date: 26-Apr-2016
 */

/**
 * @addtogroup ha
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/msg.h"

#include "lib/misc.h"   /* memcmp */

M0_INTERNAL uint64_t m0_ha_msg_tag(const struct m0_ha_msg *msg)
{
	return msg->hm_tag;
}

M0_INTERNAL enum m0_ha_msg_type m0_ha_msg_type_get(const struct m0_ha_msg *msg)
{
	return msg->hm_data.hed_type;
}

M0_INTERNAL bool m0_ha_msg_eq(const struct m0_ha_msg *msg1,
			      const struct m0_ha_msg *msg2)
{
	return m0_fid_eq(&msg1->hm_fid, &msg2->hm_fid) &&
	       m0_fid_eq(&msg1->hm_source_process, &msg2->hm_source_process) &&
	       m0_fid_eq(&msg1->hm_source_service, &msg2->hm_source_service) &&
	       msg1->hm_time == msg2->hm_time &&
	       msg1->hm_data.hed_type == msg2->hm_data.hed_type &&
	       /*
		* Note: it's not reqired by the standard because structs can
		* have padding bytes. Please remove this memcmp if you see any
		* problem with it.
		*/
	       memcmp(&msg1->hm_data,
		      &msg2->hm_data, sizeof msg1->hm_data) == 0;
}

M0_INTERNAL void m0_ha_msg_debug_print(const struct m0_ha_msg *msg,
				       const char             *prefix)
{
	const struct m0_ha_msg_data *data     = &msg->hm_data;
	const struct m0_ha_msg_nvec *nvec;
	uint32_t                     ha_state;
	int                          i;

	M0_LOG(M0_DEBUG, "%s: msg=%p hm_fid="FID_F" hm_source_process="FID_F" "
	       "hm_source_service="FID_F" hm_time=%"PRIu64,
	       prefix, msg, FID_P(&msg->hm_fid), FID_P(&msg->hm_source_process),
	       FID_P(&msg->hm_source_service), msg->hm_time);
	M0_LOG(M0_DEBUG, "%s: msg=%p hm_tag=%"PRIu64" hm_epoch=%"PRIu64,
	       prefix, msg, msg->hm_tag, msg->hm_epoch);

	switch ((enum m0_ha_msg_type)data->hed_type) {
	case M0_HA_MSG_INVALID:
		M0_LOG(M0_WARN, "message has INVALID type");
		return;
	case M0_HA_MSG_STOB_IOQ:
		M0_LOG(M0_DEBUG, "STOB_IOQ msg=%p conf_sdev="FID_F
		       " stob_id="STOB_ID_F" fd=%"PRId64,
		       msg,
		       FID_P(&data->u.hed_stob_ioq.sie_conf_sdev),
		       STOB_ID_P(&data->u.hed_stob_ioq.sie_stob_id),
		       data->u.hed_stob_ioq.sie_fd);
		M0_LOG(M0_DEBUG, "STOB_IOQ msg=%p opcode=%"PRId64" rc=%"PRId64
		       " bshift=%"PRIu32" size=%"PRIu64" offset=%"PRIu64,
		       msg,
		       data->u.hed_stob_ioq.sie_opcode,
		       data->u.hed_stob_ioq.sie_rc,
		       data->u.hed_stob_ioq.sie_bshift,
		       data->u.hed_stob_ioq.sie_size,
		       data->u.hed_stob_ioq.sie_offset);
		return;
	case M0_HA_MSG_NVEC:
		nvec = &data->u.hed_nvec;
		M0_LOG(M0_DEBUG, "NVEC hmnv_type=%"PRIu64" hmnv_nr=%"PRIu64" "
		       "hmnv_id_of_get=%"PRIu64" "
		       "hmnv_ignore_same_state=%"PRIu64,
		       nvec->hmnv_type, nvec->hmnv_nr, nvec->hmnv_id_of_get,
		       nvec->hmnv_ignore_same_state);
		for (i = 0; i < data->u.hed_nvec.hmnv_nr; ++i) {
			M0_LOG(M0_DEBUG, "hmnv_arr.hmna_arr[%d]=(no_id="FID_F" "
			       "no_state=%"PRIu32")", i,
			       FID_P(&nvec->hmnv_arr.hmna_arr[i].no_id),
			       nvec->hmnv_arr.hmna_arr[i].no_state);
			if (nvec->hmnv_arr.hmna_arr[i].no_id.f_container <
			    0x10000000000UL) {
				M0_LOG(M0_ERROR, "invalid note: no_id="FID_F" "
				       "no_state=%"PRIu32,
				       FID_P(&nvec->hmnv_arr.hmna_arr[i].no_id),
				       nvec->hmnv_arr.hmna_arr[i].no_state);
			}

		}
		return;
	case M0_HA_MSG_FAILURE_VEC_REQ:
		M0_LOG(M0_DEBUG, "FAILURE_VEC_REQ mvq_pool="FID_F,
		       FID_P(&data->u.hed_fvec_req.mfq_pool));
		return;
	case M0_HA_MSG_FAILURE_VEC_REP:
		M0_LOG(M0_DEBUG, "FAILURE_VEC_REP mvp_pool="FID_F" "
		       "mvp_nr=%"PRIu32, FID_P(&data->u.hed_fvec_rep.mfp_pool),
		       data->u.hed_fvec_rep.mfp_nr);
		for (i = 0; i < data->u.hed_fvec_rep.mfp_nr; ++i) {
			ha_state =
			  data->u.hed_fvec_rep.mfp_vec.hmna_arr[i].no_state;
			M0_LOG(M0_DEBUG, "mvf_nvec[%d]=(no_id="FID_F","
			       "no_state = %"PRIu32")", i,
			 FID_P(&data->u.hed_fvec_rep.mfp_vec.hmna_arr[i].no_id),
			      ha_state);
		}
		return;
	case M0_HA_MSG_KEEPALIVE_REQ:
		M0_LOG(M0_DEBUG, "KEEPALIVE_REQ hm_fid="FID_F" kaq_id="U128X_F,
		       FID_P(&msg->hm_fid),
		       U128_P(&data->u.hed_keepalive_req.kaq_id));
		return;
	case M0_HA_MSG_KEEPALIVE_REP:
		M0_LOG(M0_DEBUG, "KEEPALIVE_REP hm_fid="FID_F" "
		       "kap_id="U128X_F" kap_counter=%"PRIu64,
		       FID_P(&msg->hm_fid),
		       U128_P(&data->u.hed_keepalive_rep.kap_id),
		       data->u.hed_keepalive_rep.kap_counter);
		return;
	case M0_HA_MSG_EVENT_PROCESS:
		M0_LOG(M0_DEBUG, "EVENT_PROCESS hm_fid="FID_F" "
		       "chp_event=%"PRIu64" chp_type=%"PRIu64" chp_pid=%"PRIu64,
		       FID_P(&msg->hm_fid),
		       data->u.hed_event_process.chp_event,
		       data->u.hed_event_process.chp_type,
		       data->u.hed_event_process.chp_pid);
		return;
	case M0_HA_MSG_EVENT_SERVICE:
		M0_LOG(M0_DEBUG, "EVENT_SERVICE hm_fid="FID_F" "
		       "chs_event=%"PRIu64" chs_type=%"PRIu64,
		       FID_P(&msg->hm_fid),
		       data->u.hed_event_service.chs_event,
		       data->u.hed_event_service.chs_type);
		return;
	case M0_HA_MSG_EVENT_RPC:
		M0_LOG(M0_DEBUG, "EVENT_RPC hm_fid="FID_F" "
		       "hmr_state=%"PRIu64" hmr_attempts=%"PRIu64,
		       FID_P(&msg->hm_fid),
		       data->u.hed_event_rpc.hmr_state,
		       data->u.hed_event_rpc.hmr_attempts);
		return;
	case M0_HA_MSG_BE_IO_ERR:
		M0_LOG(M0_DEBUG, "BE_IO_ERR hm_fid="FID_F" "
		       "ber_errcode=%"PRIu32" ber_location=%u ber_io_opcode=%u",
		       FID_P(&msg->hm_fid),
		       data->u.hed_be_io_err.ber_errcode,
		       data->u.hed_be_io_err.ber_location,
		       data->u.hed_be_io_err.ber_io_opcode);
		return;
	case M0_HA_MSG_SNS_ERR:
		M0_LOG(M0_DEBUG, "HA_SNS_ERR hm_fid="FID_F" "
		       "hse_errcode=%"PRIu32" hse_opcode=%u",
		       FID_P(&msg->hm_fid),
		       data->u.hed_ha_sns_err.hse_errcode,
		       data->u.hed_ha_sns_err.hse_opcode);
		return;
	default:
		M0_LOG(M0_WARN, "unknown m0_ha_msg type: %"PRIu64,
		       data->hed_type);
	}
}

#undef M0_TRACE_SUBSYSTEM
/** @} end of ha group */

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
