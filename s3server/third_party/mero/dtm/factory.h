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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 15-Mar-2013
 */


#pragma once

#ifndef __MERO_DTM_FACTORY_H__
#define __MERO_DTM_FACTORY_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* export */
struct m0_dtm_factory;
struct m0_dtm_factory_ops;
struct m0_dtm_factory_type;

/* import */
struct m0_dtm_dtx;
struct m0_dtm_oper;

struct m0_dtm_factory {
	const struct m0_dtm_factory_ops *fa_ops;
};

struct m0_dtm_factory_ops {
	const struct m0_dtm_factory_type *fao_type;

	int (*fao_dtx_make) (struct m0_dtm_factory *fa, struct m0_dtm_dtx *dt);
	int (*fao_oper_make)(struct m0_dtm_factory *fa,
			     struct m0_dtm_oper *oper);
};

struct m0_dtm_factory_type {
	const char *fat_name;
};

/** @} end of dtm group */

#endif /* __MERO_DTM_FACTORY_H__ */


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
