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
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 23-May-2016
 */

#pragma once

#ifndef __MERO_SPIEL_SPIEL_INTERNAL_H__
#define __MERO_SPIEL_SPIEL_INTERNAL_H__

/****************************************************/
/*                    Helpers                       */
/****************************************************/

static inline struct m0_confc *spiel_confc(struct m0_spiel *spl)
{
	return spl->spl_core.spc_confc;
}

static inline struct m0_fid *spiel_profile(struct m0_spiel *spl)
{
	return &spl->spl_core.spc_profile;
}

static inline struct m0_rpc_machine *spiel_rmachine(struct m0_spiel *spl)
{
	return spl->spl_core.spc_rmachine;
}

#endif /* __MERO_SPIEL_SPIEL_INTERNAL_H__*/

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
