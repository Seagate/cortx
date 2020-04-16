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
 * Original creation date: 13-Mar-2015
 */

#include "ioservice/fid_convert.h"

#include "cob/cob.h"            /* m0_cob_fid_type */

#include "stob/type.h"          /* m0_stob_type */
#include "stob/ad.h"            /* m0_stob_ad_type */
#include "stob/linux.h"         /* m0_stob_linux_type */
#include "module/instance.h"    /* m0_get */

/**
 * @addtogroup fidconvert
 *
 * @{
 */

M0_INTERNAL void m0_fid_convert_cob2adstob(const struct m0_fid *cob_fid,
					   struct m0_stob_id   *stob_id)
{
	uint32_t device_id;

	M0_PRE(m0_fid_validate_cob(cob_fid));

	device_id = m0_fid__device_id_extract(cob_fid);
	stob_id->si_fid = *cob_fid;
	m0_fid_tassume(&stob_id->si_fid, &m0_stob_ad_type.st_fidt);
	m0_fid_tset(&stob_id->si_domain_fid, m0_stob_ad_type.st_fidt.ft_id,
		    0, device_id);

	M0_POST(m0_fid_validate_adstob(stob_id));
}

M0_INTERNAL void m0_fid_convert_adstob2cob(const struct m0_stob_id *stob_id,
					   struct m0_fid           *cob_fid)
{
	M0_PRE(m0_fid_validate_adstob(stob_id));

	*cob_fid = stob_id->si_fid;
	m0_fid_tassume(cob_fid, &m0_cob_fid_type);

	M0_POST(m0_fid_validate_cob(cob_fid));
}

M0_INTERNAL void
m0_fid_convert_bstore2adstob(const struct m0_fid *bstore_fid,
			     struct m0_fid       *stob_domain_fid)
{
	M0_PRE(m0_fid_validate_bstore(bstore_fid));

	*stob_domain_fid = *bstore_fid;
	m0_fid_tassume(stob_domain_fid, &m0_stob_ad_type.st_fidt);
}

M0_INTERNAL void
m0_fid_convert_adstob2bstore(const struct m0_fid *stob_domain_fid,
			     struct m0_fid       *bstore_fid)
{
	M0_PRE(m0_fid_tget(stob_domain_fid) == m0_stob_ad_type.st_fidt.ft_id);

	*bstore_fid = *stob_domain_fid;
	m0_fid_tassume(bstore_fid, &m0_stob_linux_type.st_fidt);

	M0_POST(m0_fid_validate_bstore(bstore_fid));
}

M0_INTERNAL bool m0_fid_validate_adstob(const struct m0_stob_id *stob_id)
{
	return m0_fid_tget(&stob_id->si_fid) == m0_stob_ad_type.st_fidt.ft_id &&
	       m0_fid_tget(&stob_id->si_domain_fid) ==
	       m0_stob_ad_type.st_fidt.ft_id &&
	       m0_fid__device_id_extract(&stob_id->si_fid) ==
	       stob_id->si_domain_fid.f_key &&
	       (stob_id->si_domain_fid.f_container & M0_FID_TYPE_MASK) == 0;

}

M0_INTERNAL bool m0_fid_validate_bstore(const struct m0_fid *bstore_fid)
{
	return m0_fid_tget(bstore_fid) == m0_stob_linux_type.st_fidt.ft_id;
}

M0_INTERNAL bool m0_fid_validate_linuxstob(const struct m0_stob_id *stob_id)
{
	return m0_fid_tget(&stob_id->si_fid) ==
			m0_stob_linux_type.st_fidt.ft_id &&
	       m0_fid_tget(&stob_id->si_domain_fid) ==
			m0_stob_linux_type.st_fidt.ft_id &&
		(stob_id->si_domain_fid.f_container & M0_FID_TYPE_MASK) == 0;

}

M0_INTERNAL void m0_fid_convert_cob2linuxstob(const struct m0_fid *cob_fid,
					      struct m0_stob_id   *stob_id)
{
	uint32_t device_id;

	M0_PRE(m0_fid_validate_cob(cob_fid));

	device_id = m0_get()->i_storage_is_fake ? M0_SDEV_CID_DEFAULT :
					     m0_fid__device_id_extract(cob_fid);
	stob_id->si_fid = *cob_fid;
	m0_fid_tassume(&stob_id->si_fid, &m0_stob_linux_type.st_fidt);
	m0_fid_tset(&stob_id->si_domain_fid, m0_stob_linux_type.st_fidt.ft_id,
		    0, device_id);

	M0_POST(m0_fid_validate_linuxstob(stob_id));
}

M0_INTERNAL void m0_fid_convert_linuxstob2cob(const struct m0_stob_id *stob_id,
					      struct m0_fid           *cob_fid)
{
	M0_PRE(m0_fid_validate_linuxstob(stob_id));

	*cob_fid = stob_id->si_fid;
	m0_fid_tassume(cob_fid, &m0_cob_fid_type);

	M0_POST(m0_fid_validate_cob(cob_fid));
}

M0_INTERNAL void m0_fid_convert_cob2stob(const struct m0_fid *cob_fid,
					 struct m0_stob_id   *stob_id)
{
	bool stob_ad = m0_get()->i_reqh_uses_ad_stob;

	if (stob_ad)
		m0_fid_convert_cob2adstob(cob_fid, stob_id);
	else
		m0_fid_convert_cob2linuxstob(cob_fid, stob_id);
}

M0_INTERNAL void m0_fid_convert_stob2cob(const struct m0_stob_id   *stob_id,
					 struct m0_fid *cob_fid)
{
	bool stob_ad = m0_get()->i_reqh_uses_ad_stob;

	if (stob_ad)
		m0_fid_convert_adstob2cob(stob_id, cob_fid);
	else
		m0_fid_convert_linuxstob2cob(stob_id, cob_fid);
}

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
