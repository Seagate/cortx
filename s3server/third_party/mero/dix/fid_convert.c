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
 * Original author: Sergey Shilov <sergey.shilov@seagate.com>
 * Original creation date: 26-July-2016
 */

#include "dix/fid_convert.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"
#include "lib/assert.h" /* M0_PRE */

#include "fid/fid.h"    /* m0_fid */
#include "cas/cas.h"    /* m0_dix_fid_type, m0_cctg_fid_type */

/**
 * @addtogroup dix
 *
 * @{
 */

/* extract bits [32, 56) from fid->f_container */
M0_INTERNAL uint32_t m0_dix_fid__device_id_extract(const struct m0_fid *fid)
{
	return (fid->f_container & M0_DIX_FID_DEVICE_ID_MASK) >>
	       M0_DIX_FID_DEVICE_ID_OFFSET;
}

M0_INTERNAL void m0_dix_fid_dix_make(struct m0_fid *dix_fid,
				     uint32_t       container,
				     uint64_t       key)
{
	m0_fid_tset(dix_fid, m0_dix_fid_type.ft_id, container, key);

	M0_POST(m0_dix_fid_validate_dix(dix_fid));
}

M0_INTERNAL void m0_dix_fid_convert_dix2cctg(const struct m0_fid *dix_fid,
					     struct m0_fid       *cctg_fid,
					     uint32_t             device_id)
{
	M0_PRE(m0_dix_fid_validate_dix(dix_fid));
	M0_PRE(device_id <= M0_DIX_FID_DEVICE_ID_MAX);

	*cctg_fid = *dix_fid;
	m0_fid_tassume(cctg_fid, &m0_cctg_fid_type);
	cctg_fid->f_container |= (uint64_t)device_id <<
		M0_DIX_FID_DEVICE_ID_OFFSET;

	M0_POST(m0_dix_fid_validate_cctg(cctg_fid));
}

M0_INTERNAL void m0_dix_fid_convert_cctg2dix(const struct m0_fid *cctg_fid,
					     struct m0_fid       *dix_fid)
{
	M0_PRE(m0_dix_fid_validate_cctg(cctg_fid));

	m0_fid_tset(dix_fid, m0_dix_fid_type.ft_id,
		    cctg_fid->f_container & M0_DIX_FID_DIX_CONTAINER_MASK,
		    cctg_fid->f_key);

	M0_POST(m0_dix_fid_validate_dix(dix_fid));
}

M0_INTERNAL uint32_t m0_dix_fid_cctg_device_id(const struct m0_fid *cctg_fid)
{
	M0_PRE(m0_dix_fid_validate_cctg(cctg_fid));

	return m0_dix_fid__device_id_extract(cctg_fid);
}

M0_INTERNAL bool m0_dix_fid_validate_dix(const struct m0_fid *dix_fid)
{
	return m0_fid_tget(dix_fid) == m0_dix_fid_type.ft_id &&
	       m0_dix_fid__device_id_extract(dix_fid) == 0;
}

M0_INTERNAL bool m0_dix_fid_validate_cctg(const struct m0_fid *cctg_fid)
{
	return m0_fid_tget(cctg_fid) == m0_cctg_fid_type.ft_id;
}

#undef M0_TRACE_SUBSYSTEM
/** @} end of dix group */

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
