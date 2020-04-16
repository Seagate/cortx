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
 * Original author: Atsuro Hoshino <atsuro_hoshino@xyratex.com>
 * Original creation date: 02-Sep-2013
 */

#pragma once

#ifndef __MERO___HA_NOTE_FOPS_H__
#define __MERO___HA_NOTE_FOPS_H__

#include "fop/fop.h"
#include "xcode/xcode_attr.h"

#include "ha/note.h"
#include "ha/note_xc.h"

/**
 * @addtogroup ha-note
 * @{
 */

/**
 * FOP sent between Mero and Halon to exchange object state changes.
 * See ha/note.h.
 */
struct m0_ha_state_fop {
	/** Error code for reply, ignored in request. */
	int32_t           hs_rc;
	/** Objects and (optionally) their states. */
	struct m0_ha_nvec hs_note;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

M0_INTERNAL int m0_ha_state_fop_init(void);
M0_INTERNAL void m0_ha_state_fop_fini(void);

extern struct m0_fop_type m0_ha_state_get_fopt;
extern struct m0_fop_type m0_ha_state_get_rep_fopt;
extern struct m0_fop_type m0_ha_state_set_fopt;

extern const struct m0_fom_type_ops *m0_ha_state_get_fom_type_ops;
extern const struct m0_fom_type_ops *m0_ha_state_set_fom_type_ops;

/** @} END of ha-note */
#endif /* __MERO___HA_NOTE_FOPS_H__ */

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
