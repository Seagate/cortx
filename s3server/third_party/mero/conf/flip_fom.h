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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 16-Mar-2015
 */

#pragma once

#ifndef __MERO_CONF_FLIP_FOMS_H__
#define __MERO_CONF_FLIP_FOMS_H__

/**
 * @defgroup spiel_foms Fop State Machines for Spiel FOPs
 *
 * Fop state machine for Spiel operations
 * @see fom
 *
 * FOP state machines for various Spiel operation
 *
 * @note Naming convention: For operation xyz, the FOP is named
 * as m0_fop_xyz, its corresponding reply FOP is named as m0_fop_xyz_rep
 * and FOM is named as m0_fom_xyz. For each FOM type, its corresponding
 * create, state and fini methods are named as m0_fom_xyz_create,
 * m0_fom_xyz_state, m0_fom_xyz_fini respectively.
 *
 *  @{
 */

#include "fop/fop.h"
#include "conf/flip_fop.h"
#include "net/net.h"
#include "fop/fom.h"
#include "fop/fom_generic.h"   /* M0_FOPH_NR */


M0_INTERNAL int m0_conf_flip_fom_create(struct m0_fop   *fop,
					struct m0_fom  **out,
					struct m0_reqh  *reqh);

/**
 * Object encompassing FOM for Spiel flip
 * operation and necessary context data
 */
struct m0_conf_flip_fom {
	/** Generic m0_fom object. */
	struct m0_fom clm_gen;
};

/**
 * The various phases for Confd flip FOM.
 * complete FOM and reqh infrastructure is in place.
 */
enum m0_conf_flip_fom_phase {
	M0_FOPH_CONF_FLIP_PREPARE = M0_FOPH_NR + 1,
	M0_FOPH_CONF_APPLY,
 };

/** @} end of spiel_foms */

#endif /* __MERO_CONF_FLIP_FOMS_H__ */
 /*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
