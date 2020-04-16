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
 * Original creation date: 15-May-2016
 */

#pragma once

#ifndef __MERO_HA_LINK_SERVICE_H__
#define __MERO_HA_LINK_SERVICE_H__

/**
 * @defgroup ha
 *
 * @{
 */

struct m0_chan;
struct m0_reqh;
struct m0_reqh_service;
struct m0_reqh_service_type;
struct m0_ha_link;

extern struct m0_reqh_service_type m0_ha_link_service_type;

M0_INTERNAL int  m0_ha_link_service_init(struct m0_reqh_service **hl_service,
                                         struct m0_reqh          *reqh);
M0_INTERNAL void m0_ha_link_service_fini(struct m0_reqh_service *hl_service);

/** Find link by link id and increase the link reference counter */
M0_INTERNAL struct m0_ha_link *
m0_ha_link_service_find_get(struct m0_reqh_service  *service,
                            const struct m0_uint128 *link_id,
                            struct m0_uint128       *connection_id);
M0_INTERNAL void m0_ha_link_service_put(struct m0_reqh_service *service,
                                        struct m0_ha_link      *hl);

M0_INTERNAL void
m0_ha_link_service_register(struct m0_reqh_service  *service,
                            struct m0_ha_link       *hl,
                            const struct m0_uint128 *link_id,
                            const struct m0_uint128 *connection_id);
M0_INTERNAL void m0_ha_link_service_deregister(struct m0_reqh_service *service,
                                               struct m0_ha_link      *hl);
M0_INTERNAL void m0_ha_link_service_quiesce(struct m0_reqh_service *service,
                                            struct m0_ha_link      *hl,
                                            struct m0_chan         *chan);

M0_INTERNAL int  m0_ha_link_service_mod_init(void);
M0_INTERNAL void m0_ha_link_service_mod_fini(void);

/** @} end of ha group */
#endif /* __MERO_HA_LINK_SERVICE_H__ */

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
