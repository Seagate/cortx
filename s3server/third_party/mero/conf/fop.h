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
 * Original author: Anatoliy Bilenko <Anatoliy_Bilenko@xyratex.com>
 * Original creation date: 05/05/2012
 */
#pragma once
#ifndef __MERO_CONF_FOP_H__
#define __MERO_CONF_FOP_H__

#include "fop/fop.h"

/**
 * @defgroup conf_fop Configuration FOPs
 *
 * @{
 */

extern struct m0_fop_type m0_conf_fetch_fopt;
extern struct m0_fop_type m0_conf_fetch_resp_fopt;

extern struct m0_fop_type m0_conf_update_fopt;
extern struct m0_fop_type m0_conf_update_resp_fopt;

extern struct m0_fop_type m0_fop_conf_load_fopt;
extern struct m0_fop_type m0_fop_conf_load_rep_fopt;

extern struct m0_fop_type m0_fop_conf_flip_fopt;
extern struct m0_fop_type m0_fop_conf_flip_rep_fopt;

M0_INTERNAL int m0_conf_fops_init(void);
M0_INTERNAL void m0_conf_fops_fini(void);

M0_INTERNAL int m0_confx_types_init(void);
M0_INTERNAL void m0_confx_types_fini(void);


/** @} conf_fop */
#endif /* __MERO_CONF_FOP_H__ */
