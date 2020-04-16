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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 08/01/2013
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_SNSCM
#include "lib/trace.h"
#include "lib/misc.h"
#include "lib/errno.h"
#include "sns/parity_repair.h"
#include "ioservice/fid_convert.h"  /* m0_fid_cob_device_id */

static void device_index_get(struct m0_pdclust_instance *pi,
			     uint64_t group_number, uint64_t unit_number,
			     uint32_t *device_index_out)
{
        struct m0_pdclust_src_addr  sa;
        struct m0_pdclust_tgt_addr  ta;

	/* Find out the device index. */
	M0_SET0(&sa);
	M0_SET0(&ta);

        sa.sa_group = group_number;
        sa.sa_unit = unit_number;
        m0_fd_fwd_map(pi, &sa, &ta);
	*device_index_out = ta.ta_obj;
	M0_LEAVE("index:%d", (int)ta.ta_obj);
}

static int _spare_next(struct m0_poolmach *pm,
		       const struct m0_fid *fid,
		       struct m0_pdclust_layout *pl,
		       struct m0_pdclust_instance *pi,
		       uint64_t group_number,
		       uint64_t unit_number,
		       uint32_t *spare_slot_out,
		       uint32_t *spare_slot_out_prev,
		       bool break_next)
{
	uint32_t device_index;
	uint32_t device_index_new;
	int      rc;

	M0_ENTRY("unit number:%d", (int)unit_number);

	M0_PRE(pm != NULL && fid != NULL && pl != NULL);

	device_index_get(pi, group_number, unit_number, &device_index);
	*spare_slot_out_prev = unit_number;

	while (1) {
		rc = m0_poolmach_sns_repair_spare_query(pm, device_index,
							spare_slot_out);
		if (rc != 0)
			return M0_ERR(rc);

		/*
		 * Find out if spare slot's corresponding device index is
		 * failed. If yes, find out new spare.
		 */
		device_index_get(pi, group_number,
				m0_pdclust_N(pl) + m0_pdclust_K(pl) +
				*spare_slot_out, &device_index_new);

		if (m0_poolmach_device_is_in_spare_usage_array(pm,
					device_index_new) && !break_next) {
			device_index = device_index_new;
			*spare_slot_out_prev = *spare_slot_out;
		} else
			break;
	}
	/*
	 * Return the absolute index of spare with respect to the aggregation
	 * group.
	 */
	if (rc == 0) {
		*spare_slot_out += m0_pdclust_N(pl) + m0_pdclust_K(pl);
		if (*spare_slot_out_prev != unit_number)
			*spare_slot_out_prev += m0_pdclust_N(pl) +
				m0_pdclust_K(pl);
	}

	return M0_RC(rc);
}

M0_INTERNAL int m0_sns_repair_spare_map(struct m0_poolmach *pm,
					const struct m0_fid *fid,
					struct m0_pdclust_layout *pl,
					struct m0_pdclust_instance *pi,
					uint64_t group, uint64_t unit,
					uint32_t *spare_slot_out,
					uint32_t *spare_slot_out_prev)
{
	return _spare_next(pm, fid, pl, pi, group, unit, spare_slot_out,
			   spare_slot_out_prev, false);
}

M0_INTERNAL int m0_sns_repair_spare_rebalancing(struct m0_poolmach *pm,
						const struct m0_fid *fid,
						struct m0_pdclust_layout *pl,
						struct m0_pdclust_instance *pi,
						uint64_t group, uint64_t unit,
						uint32_t *spare_slot_out,
						uint32_t *spare_slot_out_prev)
{
	enum m0_pool_nd_state state_out;
	uint32_t              device_index;
	int                   rc;

	do {
		rc = _spare_next(pm, fid, pl, pi, group, unit, spare_slot_out,
				 spare_slot_out_prev, true);
		if (rc == 0) {
			device_index_get(pi, group, *spare_slot_out, &device_index);
			rc = m0_poolmach_device_state(pm, device_index, &state_out);
		}
		unit = *spare_slot_out;
	} while (rc == 0 && state_out != M0_PNDS_SNS_REBALANCING);

	return M0_RC(rc);
}

static bool frame_eq(struct m0_pdclust_instance *pi, uint64_t group_number,
                     uint64_t frame, uint32_t device_index)
{
        struct m0_pdclust_src_addr sa;
        struct m0_pdclust_tgt_addr ta;

        M0_PRE(pi != NULL);

        M0_SET0(&sa);
        M0_SET0(&ta);

        ta.ta_frame = frame;
        ta.ta_obj = device_index;

        m0_fd_bwd_map(pi, &ta, &sa);
        return sa.sa_group == group_number;
}

static uint64_t frame_get(struct m0_pdclust_instance *pi, uint64_t spare_frame,
			  uint64_t group_number, uint32_t device_index)
{
        uint64_t                   frame;
        bool                       frame_found;

        M0_PRE(pi != NULL);

	/* We look at spare_frame - 1, spare_frame and spare_frame + 1 to find
	 * appropriate data/parity unit corresponding to spare unit for a
	 * given group number.
	 */
        if (spare_frame != 0) {
                frame = spare_frame - 1;
                frame_found = frame_eq(pi, group_number, frame, device_index);
                if (frame_found)
                        goto out;
        }

        frame = spare_frame;
        frame_found = frame_eq(pi, group_number, frame, device_index);
        if (frame_found)
                goto out;

        frame = spare_frame + 1;
        frame_found = frame_eq(pi, group_number, frame, device_index);

out:
        if (frame_found)
                return frame;
        else
                return M0_RC(-ENOENT);
}

M0_INTERNAL int m0_sns_repair_data_map(struct m0_poolmach *pm,
				       struct m0_pdclust_layout *pl,
				       struct m0_pdclust_instance *pi,
				       uint64_t group_number,
				       uint64_t spare_unit_number,
				       uint64_t *data_unit_id_out)
{
        int                         rc;
        struct m0_pdclust_src_addr  sa;
        struct m0_pdclust_tgt_addr  ta;
	enum m0_pool_nd_state       state_out;
	uint64_t                    spare_in;
        uint32_t                    device_index;
        uint64_t                    spare_id;
        uint64_t                    frame;

        M0_PRE(pm != NULL && pl != NULL && pi != NULL);

	spare_in = spare_unit_number;

	do {
		spare_id = spare_in - m0_pdclust_N(pl) -
			m0_pdclust_K(pl);
		/*
		 * Fetch the correspinding data/parity unit device index for
		 * the given spare unit.
		 */
		device_index = pm->pm_state->pst_spare_usage_array[spare_id].
			psu_device_index;

		if (device_index == POOL_PM_SPARE_SLOT_UNUSED) {
			rc = -ENOENT;
			goto out;
		}

		M0_SET0(&sa);
		M0_SET0(&ta);
		sa.sa_group = group_number;
		sa.sa_unit  = spare_in;
		m0_fd_fwd_map(pi, &sa, &ta);
		/*
		 * Find the data/parity unit frame for the @group_number on the
		 * given device represented by @device_index.
		 */
		frame = frame_get(pi, ta.ta_frame, group_number, device_index);
		if (frame == -ENOENT) {
			rc = -ENOENT;
			goto out;
		}

		M0_SET0(&sa);
		M0_SET0(&ta);

		ta.ta_frame = frame;
		ta.ta_obj = device_index;

		rc = m0_poolmach_device_state(pm, device_index, &state_out);
		if (rc != 0) {
			rc = -ENOENT;
			goto out;
		}

		/*
		 * Doing inverse mapping from the frame in the device to the
		 * corresponding unit in parity group @group_number.
		 */
		m0_fd_bwd_map(pi, &ta, &sa);

		*data_unit_id_out = sa.sa_unit;

		/*
		 * It is possible that the unit mapped corresponding to the given
		 * spare_unit_number is same as the spare_unit_number.
		 * Thus this means that there is no data/parity unit repaired on
		 * the given spare_unit_number and the spare is empty.
		 */
		if (spare_unit_number == sa.sa_unit) {
			rc = -ENOENT;
			goto out;
		}

		/*
		 * We have got another spare unit, so further try again to map
		 * this spare unit to the actual failed data/parity unit.
		 */
		spare_in = sa.sa_unit;

	} while(m0_pdclust_unit_classify(pl, sa.sa_unit) == M0_PUT_SPARE &&
			M0_IN(state_out, (M0_PNDS_SNS_REPAIRED,
					  M0_PNDS_SNS_REBALANCING)));
out:
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
