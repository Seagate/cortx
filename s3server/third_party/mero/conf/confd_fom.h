/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 05-May-2012
 */
#pragma once
#ifndef __MERO_CONFD_FOM_H__
#define __MERO_CONFD_FOM_H__

#include "fop/fom.h"  /* m0_fom */

/**
 * @addtogroup confd_dfspec
 *
 * @{
 */

struct m0_confd_fom {
	struct m0_fom dm_fom;
};

M0_INTERNAL int m0_confd_fom_create(struct m0_fop   *fop,
				    struct m0_fom  **out,
				    struct m0_reqh  *reqh);

/** @} confd_dfspec */
#endif /* __MERO_CONFD_FOM_H__ */
