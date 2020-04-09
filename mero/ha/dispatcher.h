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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 29-Jun-2016
 */

#pragma once

#ifndef __MERO_HA_DISPATCHER_H__
#define __MERO_HA_DISPATCHER_H__

/**
 * @defgroup ha-dispatcher
 *
 * @{
 */

#include "lib/types.h"          /* bool */
#include "lib/tlist.h"          /* m0_tl */
#include "module/module.h"      /* m0_module */

struct m0_ha;
struct m0_ha_link;
struct m0_ha_msg;
struct m0_ha_note_handler;
struct m0_ha_keepalive_handler;
struct m0_ha_fvec_handler;

struct m0_ha_dispatcher_cfg {
	bool hdc_enable_note;
	bool hdc_enable_keepalive;
	bool hdc_enable_fvec;
};

struct m0_ha_dispatcher {
	struct m0_ha_dispatcher_cfg     hds_cfg;
	struct m0_module                hds_module;
	/*
	 * Is not protected by any lock.
	 * User is responsible for non-concurrent modifications.
	 * Handlers can be added only between m0_mero_ha_init() and
	 * m0_mero_ha_start().
	 */
	struct m0_tl                    hds_handlers;
	/* m0_ha_note_set(), m0_ha_note_get() handler */
	struct m0_ha_note_handler      *hds_note_handler;
	struct m0_ha_fvec_handler      *hds_fvec_handler;
	struct m0_ha_keepalive_handler *hds_keepalive_handler;
};

struct m0_ha_handler {
	struct m0_tlink   hh_link;
	uint64_t          hh_magic;
	void             *hh_data;
	void            (*hh_msg_received_cb)(struct m0_ha_handler *hh,
	                                      struct m0_ha         *ha,
	                                      struct m0_ha_link    *hl,
	                                      struct m0_ha_msg     *msg,
	                                      uint64_t              tag,
	                                      void                 *data);
};

M0_INTERNAL int m0_ha_dispatcher_init(struct m0_ha_dispatcher     *hd,
                                      struct m0_ha_dispatcher_cfg *hd_cfg);
M0_INTERNAL void m0_ha_dispatcher_fini(struct m0_ha_dispatcher *hd);

M0_INTERNAL void m0_ha_dispatcher_attach(struct m0_ha_dispatcher *hd,
                                         struct m0_ha_handler    *hh);
M0_INTERNAL void m0_ha_dispatcher_detach(struct m0_ha_dispatcher *hd,
                                         struct m0_ha_handler    *hh);

M0_INTERNAL void m0_ha_dispatcher_handle(struct m0_ha_dispatcher *hd,
                                         struct m0_ha            *ha,
                                         struct m0_ha_link       *hl,
                                         struct m0_ha_msg        *msg,
                                         uint64_t                 tag);

/** @} end of ha-dispatcher group */
#endif /* __MERO_HA_DISPATCHER_H__ */

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
