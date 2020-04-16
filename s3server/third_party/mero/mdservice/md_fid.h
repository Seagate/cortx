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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 04-Apr-2014
 */

#pragma once

#ifndef __MERO_MDSERVICE_MD_FID_H__
#define __MERO_MDSERVICE_MD_FID_H__

/**
   @defgroup md_fid Md fid constants

   @{
 */

/* import */
#include "fid/fid.h"

/* Namespace name for root cob (not exposed to user) */
M0_EXTERN const char M0_COB_ROOT_NAME[];

/* Grobal cob root fid. */
M0_EXTERN const struct m0_fid M0_COB_ROOT_FID;

/* Namespace name for virtual .mero directory */
M0_EXTERN const char M0_DOT_MERO_NAME[];

/* .mero directory fid. */
M0_EXTERN const struct m0_fid M0_DOT_MERO_FID;

/* Namespace name for virtual .mero/fid directory */
M0_EXTERN const char M0_DOT_MERO_FID_NAME[];

/* .mero/fid directory fid. */
M0_EXTERN const struct m0_fid M0_DOT_MERO_FID_FID;

/* Hierarchy root fid (exposed to user). */
M0_EXTERN const struct m0_fid M0_MDSERVICE_SLASH_FID;

/* First fid that is allowed to be used by client for normal files and dirs. */
M0_EXTERN const struct m0_fid M0_MDSERVICE_START_FID;

/** @} end of md_fid group */
#endif /* __MERO_MDSERVICE_MD_FID_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
