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
 * Original author: Amit Jambure <Amit_Jambure@xyratex.com>
 * Original creation date: 11-Feb-2013
 */

#include "lib/misc.h"

#include "mero/magic.h"
#include "rpc/rpc.h"
#include "rpc/rpc_internal.h"

/**
 * @addtogroup rpc
 *
 * @{
 */

M0_TL_DESCR_DEFINE(item_source, "item-source-list", M0_INTERNAL,
		   struct m0_rpc_item_source, ris_tlink, ris_magic,
		   M0_RPC_ITEM_SOURCE_MAGIC, M0_RPC_ITEM_SOURCE_HEAD_MAGIC);
M0_TL_DEFINE(item_source, M0_INTERNAL, struct m0_rpc_item_source);

static bool item_source_invariant(const struct m0_rpc_item_source *ris)
{
	return  _0C(ris != NULL)                                 &&
		_0C(ris->ris_magic == M0_RPC_ITEM_SOURCE_MAGIC)  &&
		_0C(ris->ris_name != NULL)                       &&
		_0C(ris->ris_ops != NULL)                        &&
		_0C(ris->ris_ops->riso_has_item != NULL)         &&
		_0C(ris->ris_ops->riso_get_item != NULL)         &&
		_0C(ris->ris_ops->riso_conn_terminating != NULL) &&
		_0C(equi(ris->ris_conn != NULL, item_source_tlink_is_in(ris)));
}

void m0_rpc_item_source_init(struct m0_rpc_item_source *ris,
			     const char *name,
			     const struct m0_rpc_item_source_ops *ops)
{
	M0_PRE(ris != NULL && name != NULL && ops != NULL &&
	       ops->riso_has_item != NULL && ops->riso_get_item != NULL &&
	       ops->riso_conn_terminating != NULL);

	M0_SET0(ris);
	ris->ris_name  = name;
	ris->ris_ops   = ops;
	ris->ris_conn  = NULL;
	ris->ris_magic = M0_RPC_ITEM_SOURCE_MAGIC;
	item_source_tlink_init(ris);

	M0_ASSERT(item_source_invariant(ris));
}

void m0_rpc_item_source_fini(struct m0_rpc_item_source *ris)
{
	M0_PRE(!m0_rpc_item_source_is_registered(ris));

	item_source_tlink_fini(ris);
}

bool m0_rpc_item_source_is_registered(const struct m0_rpc_item_source *ris)
{
	M0_ASSERT(item_source_invariant(ris));
	return item_source_tlink_is_in(ris);
}

void m0_rpc_item_source_register_locked(struct m0_rpc_conn *conn,
					struct m0_rpc_item_source *ris)
{
	M0_PRE(conn != NULL);
	M0_PRE(conn->c_rpc_machine != NULL &&
	       m0_rpc_machine_is_locked(conn->c_rpc_machine));
	M0_PRE(!m0_rpc_item_source_is_registered(ris));
	ris->ris_conn = conn;
	item_source_tlist_add(&conn->c_item_sources, ris);
	M0_POST(m0_rpc_item_source_is_registered(ris));
}

void m0_rpc_item_source_register(struct m0_rpc_conn *conn,
				 struct m0_rpc_item_source *ris)
{
	M0_PRE(conn != NULL);
	M0_PRE(conn->c_rpc_machine != NULL &&
	       m0_rpc_machine_is_not_locked(conn->c_rpc_machine));

	m0_rpc_machine_lock(conn->c_rpc_machine);
	m0_rpc_item_source_register_locked(conn, ris);
	m0_rpc_machine_unlock(conn->c_rpc_machine);
}

void m0_rpc_item_source_deregister(struct m0_rpc_item_source *ris)
{
	struct m0_rpc_conn    *conn;
	struct m0_rpc_machine *machine;

	if (!m0_rpc_item_source_is_registered(ris))
		return;

	conn    = ris->ris_conn;
	machine = conn->c_rpc_machine;
	M0_PRE(m0_rpc_machine_is_not_locked(machine));

	m0_rpc_machine_lock(machine);

	item_source_tlist_del(ris);
	ris->ris_conn = NULL;

	M0_POST(!m0_rpc_item_source_is_registered(ris));
	m0_rpc_machine_unlock(machine);
}

/** @} end of rpc group */

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
