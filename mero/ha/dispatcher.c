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


/**
 * @addtogroup ha-dispatcher
 *
 * TODO put magics
 *
 * @{
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_HA
#include "lib/trace.h"

#include "ha/dispatcher.h"

#include "lib/memory.h"         /* M0_ALLOC_PTR */
#include "lib/errno.h"          /* ENOMEM */
#include "ha/note.h"            /* m0_ha_note_handler_init */
#include "ha/failvec.h"         /* m0_ha_fvec_handler_init */
#include "mero/keepalive.h"     /* m0_ha_keepalive_handler_init */
#include "module/instance.h"    /* m0_get */

M0_TL_DESCR_DEFINE(ha_dispatcher_handlers, "m0_ha_dispatcher::hds_handlers",
		   static, struct m0_ha_handler, hh_link, hh_magic,
		   21, 22);
M0_TL_DEFINE(ha_dispatcher_handlers, static, struct m0_ha_handler);

enum ha_dispatcher_level {
	HA_DISPATCHER_LEVEL_TLIST,
	HA_DISPATCHER_LEVEL_NOTE_HANDLER_ALLOC,
	HA_DISPATCHER_LEVEL_NOTE_HANDLER_INIT,
	HA_DISPATCHER_LEVEL_FVEC_HANDLER_ALLOC,
	HA_DISPATCHER_LEVEL_FVEC_HANDLER_INIT,
	HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_ALLOC,
	HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_INIT,
	HA_DISPATCHER_LEVEL_READY,
};

static int ha_dispatcher_level_enter(struct m0_module *module)
{
	enum ha_dispatcher_level  level = module->m_cur + 1;
	struct m0_ha_dispatcher  *hd;

	hd = container_of(module, struct m0_ha_dispatcher, hds_module);
	M0_ENTRY("hd=%p level=%d", hd, level);
	switch (level) {
	case HA_DISPATCHER_LEVEL_TLIST:
		ha_dispatcher_handlers_tlist_init(&hd->hds_handlers);
		return M0_RC(0);
	case HA_DISPATCHER_LEVEL_NOTE_HANDLER_ALLOC:
		if (!hd->hds_cfg.hdc_enable_note)
			return M0_RC(0);
		M0_ALLOC_PTR(hd->hds_note_handler);
		return hd->hds_note_handler == NULL ? M0_ERR(-ENOMEM) :
						      M0_RC(0);
	case HA_DISPATCHER_LEVEL_NOTE_HANDLER_INIT:
		if (!hd->hds_cfg.hdc_enable_note)
			return M0_RC(0);
		return M0_RC(m0_ha_note_handler_init(hd->hds_note_handler,
						     hd));
	case HA_DISPATCHER_LEVEL_FVEC_HANDLER_ALLOC:
		if (!hd->hds_cfg.hdc_enable_fvec)
			return M0_RC(0);
		M0_ALLOC_PTR(hd->hds_fvec_handler);
		return hd->hds_fvec_handler == NULL ? M0_ERR(-ENOMEM) :
						      M0_RC(0);
	case HA_DISPATCHER_LEVEL_FVEC_HANDLER_INIT:
		if (!hd->hds_cfg.hdc_enable_fvec)
			return M0_RC(0);
		return M0_RC(m0_ha_fvec_handler_init(hd->hds_fvec_handler,
						     hd));
	case HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_ALLOC:
		if (!hd->hds_cfg.hdc_enable_keepalive)
			return M0_RC(0);
		M0_ALLOC_PTR(hd->hds_keepalive_handler);
		return hd->hds_keepalive_handler == NULL ? M0_ERR(-ENOMEM) :
							   M0_RC(0);
	case HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_INIT:
		if (!hd->hds_cfg.hdc_enable_keepalive)
			return M0_RC(0);
		return M0_RC(m0_ha_keepalive_handler_init(
					hd->hds_keepalive_handler, hd));
	case HA_DISPATCHER_LEVEL_READY:
		M0_IMPOSSIBLE("can't be here");
	}
	return M0_ERR(-ENOSYS);
}

static void ha_dispatcher_level_leave(struct m0_module *module)
{
	enum ha_dispatcher_level  level = module->m_cur;
	struct m0_ha_dispatcher  *hd;

	hd = container_of(module, struct m0_ha_dispatcher, hds_module);
	M0_ENTRY("hd=%p level=%d", hd, level);
	switch (level) {
	case HA_DISPATCHER_LEVEL_TLIST:
		ha_dispatcher_handlers_tlist_fini(&hd->hds_handlers);
		break;
	case HA_DISPATCHER_LEVEL_NOTE_HANDLER_ALLOC:
		if (hd->hds_cfg.hdc_enable_note)
			m0_free(hd->hds_note_handler);
		break;
	case HA_DISPATCHER_LEVEL_NOTE_HANDLER_INIT:
		if (hd->hds_cfg.hdc_enable_note)
			m0_ha_note_handler_fini(hd->hds_note_handler);
		break;
	case HA_DISPATCHER_LEVEL_FVEC_HANDLER_ALLOC:
		if (hd->hds_cfg.hdc_enable_fvec)
			m0_free(hd->hds_fvec_handler);
		break;
	case HA_DISPATCHER_LEVEL_FVEC_HANDLER_INIT:
		if (hd->hds_cfg.hdc_enable_fvec)
			m0_ha_fvec_handler_fini(hd->hds_fvec_handler);
		break;
	case HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_ALLOC:
		if (hd->hds_cfg.hdc_enable_keepalive)
			m0_free(hd->hds_keepalive_handler);
		break;
	case HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_INIT:
		if (hd->hds_cfg.hdc_enable_keepalive)
			m0_ha_keepalive_handler_fini(hd->hds_keepalive_handler);
		break;
	case HA_DISPATCHER_LEVEL_READY:
		M0_IMPOSSIBLE("can't be here");
	}
	M0_LEAVE();
}

static const struct m0_modlev ha_dispatcher_levels[] = {
	[HA_DISPATCHER_LEVEL_TLIST] = {
		.ml_name  = "HA_DISPATCHER_LEVEL_TLIST",
		.ml_enter = ha_dispatcher_level_enter,
		.ml_leave = ha_dispatcher_level_leave,
	},
	[HA_DISPATCHER_LEVEL_NOTE_HANDLER_ALLOC] = {
		.ml_name  = "HA_DISPATCHER_LEVEL_NOTE_HANDLER_ALLOC",
		.ml_enter = ha_dispatcher_level_enter,
		.ml_leave = ha_dispatcher_level_leave,
	},
	[HA_DISPATCHER_LEVEL_NOTE_HANDLER_INIT] = {
		.ml_name  = "HA_DISPATCHER_LEVEL_NOTE_HANDLER_INIT",
		.ml_enter = ha_dispatcher_level_enter,
		.ml_leave = ha_dispatcher_level_leave,
	},
	[HA_DISPATCHER_LEVEL_FVEC_HANDLER_ALLOC] = {
		.ml_name  = "HA_DISPATCHER_LEVEL_FVEC_HANDLER_ALLOC",
		.ml_enter = ha_dispatcher_level_enter,
		.ml_leave = ha_dispatcher_level_leave,
	},
	[HA_DISPATCHER_LEVEL_FVEC_HANDLER_INIT] = {
		.ml_name  = "HA_DISPATCHER_LEVEL_FVEC_HANDLER_INIT",
		.ml_enter = ha_dispatcher_level_enter,
		.ml_leave = ha_dispatcher_level_leave,
	},
	[HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_ALLOC] = {
		.ml_name  = "HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_ALLOC",
		.ml_enter = ha_dispatcher_level_enter,
		.ml_leave = ha_dispatcher_level_leave,
	},
	[HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_INIT] = {
		.ml_name  = "HA_DISPATCHER_LEVEL_KEEPALIVE_HANDLER_INIT",
		.ml_enter = ha_dispatcher_level_enter,
		.ml_leave = ha_dispatcher_level_leave,
	},
	[HA_DISPATCHER_LEVEL_READY] = {
		.ml_name  = "HA_DISPATCHER_LEVEL_READY",
	},
};

M0_INTERNAL int m0_ha_dispatcher_init(struct m0_ha_dispatcher     *hd,
                                      struct m0_ha_dispatcher_cfg *hd_cfg)
{
	int rc;

	M0_ENTRY("hd=%p", hd);
	M0_PRE(M0_IS0(hd));

	hd->hds_cfg = *hd_cfg;
	m0_module_setup(&hd->hds_module, "m0_ha_dispatcher",
			ha_dispatcher_levels,
			ARRAY_SIZE(ha_dispatcher_levels), m0_get());
	rc = m0_module_init(&hd->hds_module, HA_DISPATCHER_LEVEL_READY);
	if (rc != 0) {
		m0_module_fini(&hd->hds_module, M0_MODLEV_NONE);
		return M0_ERR(rc);
	}
	return M0_RC(0);
}

M0_INTERNAL void m0_ha_dispatcher_fini(struct m0_ha_dispatcher *hd)
{
	M0_ENTRY("hd=%p", hd);
	m0_module_fini(&hd->hds_module, M0_MODLEV_NONE);
	M0_LEAVE();
}

M0_INTERNAL void m0_ha_dispatcher_attach(struct m0_ha_dispatcher *hd,
                                         struct m0_ha_handler    *hh)
{
	ha_dispatcher_handlers_tlink_init_at_tail(hh, &hd->hds_handlers);
}

M0_INTERNAL void m0_ha_dispatcher_detach(struct m0_ha_dispatcher *hd,
                                         struct m0_ha_handler    *hh)
{
	ha_dispatcher_handlers_tlink_del_fini(hh);
}

M0_INTERNAL void m0_ha_dispatcher_handle(struct m0_ha_dispatcher *hd,
                                         struct m0_ha            *ha,
                                         struct m0_ha_link       *hl,
                                         struct m0_ha_msg        *msg,
                                         uint64_t                 tag)
{
	struct m0_ha_handler *hh;

	m0_tl_for(ha_dispatcher_handlers, &hd->hds_handlers, hh) {
		hh->hh_msg_received_cb(hh, ha, hl, msg, tag, hh->hh_data);
	} m0_tl_endfor;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of ha-dispatcher group */

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
