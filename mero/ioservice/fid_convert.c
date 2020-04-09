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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 11-Mar-2015
 */

#include "ioservice/fid_convert.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_IOSERVICE
#include "lib/trace.h"
#include "lib/assert.h"         /* M0_PRE */

#include "fid/fid.h"            /* m0_fid */
#include "file/file.h"          /* m0_file_fid_type */
#include "cob/cob.h"            /* m0_cob_fid_type */

/**
 * @addtogroup fidconvert
 *
 * @{
 */

/* extract bits [32, 56) from fid->f_container */
M0_INTERNAL uint32_t m0_fid__device_id_extract(const struct m0_fid *fid)
{
	return (fid->f_container & M0_FID_DEVICE_ID_MASK) >>
	       M0_FID_DEVICE_ID_OFFSET;
}

M0_INTERNAL void m0_fid_gob_make(struct m0_fid *gob_fid,
				 uint32_t       container,
				 uint64_t       key)
{
	m0_fid_tset(gob_fid, m0_file_fid_type.ft_id, container, key);

	M0_POST(m0_fid_validate_gob(gob_fid));
}

M0_INTERNAL void m0_fid_convert_gob2cob(const struct m0_fid *gob_fid,
					struct m0_fid       *cob_fid,
					uint32_t             device_id)
{
	M0_PRE(m0_fid_validate_gob(gob_fid));
	M0_PRE(device_id <= M0_FID_DEVICE_ID_MAX);

	*cob_fid = *gob_fid;
	m0_fid_tassume(cob_fid, &m0_cob_fid_type);
	cob_fid->f_container |= (uint64_t)device_id << M0_FID_DEVICE_ID_OFFSET;

	M0_POST(m0_fid_validate_cob(cob_fid));
}

M0_INTERNAL void m0_fid_convert_cob2gob(const struct m0_fid *cob_fid,
					struct m0_fid       *gob_fid)
{
	M0_PRE(m0_fid_validate_cob(cob_fid));

	m0_fid_tset(gob_fid, m0_file_fid_type.ft_id,
		    cob_fid->f_container & M0_FID_GOB_CONTAINER_MASK,
		    cob_fid->f_key);

	M0_POST(m0_fid_validate_gob(gob_fid));
}

M0_INTERNAL uint32_t m0_fid_cob_device_id(const struct m0_fid *cob_fid)
{
	M0_PRE(m0_fid_validate_cob(cob_fid));

	return m0_fid__device_id_extract(cob_fid);
}

M0_INTERNAL uint64_t m0_fid_conf_sdev_device_id(const struct m0_fid *sdev_fid)
{
	return sdev_fid->f_key & ((1ULL << M0_FID_DEVICE_ID_BITS) - 1);
}

M0_INTERNAL bool m0_fid_validate_gob(const struct m0_fid *gob_fid)
{
	return m0_fid_tget(gob_fid) == m0_file_fid_type.ft_id &&
	       m0_fid__device_id_extract(gob_fid) == 0;
}

M0_INTERNAL bool m0_fid_validate_cob(const struct m0_fid *cob_fid)
{
	return m0_fid_tget(cob_fid) == m0_cob_fid_type.ft_id;
}

#undef M0_TRACE_SUBSYSTEM
/** @} end of fidconvert group */

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
