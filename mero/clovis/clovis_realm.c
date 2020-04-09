/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original authors: Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Revision:         Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 15-Sep-2014
 */

#include "clovis/clovis.h"
#include "clovis/clovis_addb.h"
#include "clovis/clovis_internal.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"                /* M0_LOG */
#include "fid/fid.h"                  /* m0_fid */

void m0_clovis_container_init(struct m0_clovis_container *con,
			      struct m0_clovis_realm     *parent,
			      const struct m0_uint128    *id,
			      struct m0_clovis           *instance)
{
	M0_PRE(con != NULL);
	M0_PRE(id != NULL);
	M0_PRE(instance != NULL);

	if (m0_uint128_cmp(&M0_CLOVIS_UBER_REALM, id) == 0) {
		/* This should be an init/open cycle for the uber realm */
		M0_ASSERT(parent == NULL);

		con->co_realm.re_entity.en_id = *id;
		con->co_realm.re_entity.en_type = M0_CLOVIS_ET_REALM;
		con->co_realm.re_entity.en_realm = &con->co_realm;
		con->co_realm.re_instance = instance;
		con->co_realm.re_entity.en_sm.sm_rc = 0;
	} else
		M0_ASSERT_INFO(0, "Feature not implemented");
}
M0_EXPORTED(m0_clovis_container_init);

#undef M0_TRACE_SUBSYSTEM

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
