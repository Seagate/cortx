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
 * Original author: Huang Hua <hua_huang@xyratex.com>
 * Original creation date: 01/04/2011
 */

#include "capa.h"

/**
   @addtogroup capa
   @{
 */

M0_INTERNAL int m0_capa_init(struct m0_capa_ctxt *ctxt)
{
	/* TODO This is only stub */
	return 0;
}

M0_INTERNAL void m0_capa_fini(struct m0_capa_ctxt *ctxt)
{
	/* TODO This is only stub */
	return;
}

M0_INTERNAL int m0_capa_new(struct m0_object_capa *capa,
			    enum m0_capa_entity_type type,
			    enum m0_capa_operation opcode, void *data)
{
	capa->oc_ctxt = NULL;
	capa->oc_owner = NULL;
	capa->oc_type = type;
	capa->oc_opcode = opcode;
	capa->oc_data = data;
	m0_atomic64_set(&capa->oc_ref, 0);
	return 0;
}

M0_INTERNAL int m0_capa_get(struct m0_capa_ctxt *ctxt,
			    struct m0_capa_issuer *owner,
			    struct m0_object_capa *capa)
{
	/* TODO This is only stub */
	capa->oc_ctxt = ctxt;
	capa->oc_owner = owner;

	m0_atomic64_inc(&capa->oc_ref);
	return 0;
}

M0_INTERNAL void m0_capa_put(struct m0_capa_ctxt *ctxt,
			     struct m0_object_capa *capa)
{
	/* TODO This is only stub */
	M0_ASSERT(m0_atomic64_get(&capa->oc_ref) > 0);
	m0_atomic64_dec(&capa->oc_ref);
	return;
}

M0_INTERNAL int m0_capa_auth(struct m0_capa_ctxt *ctxt,
			     struct m0_object_capa *capa,
			     enum m0_capa_operation op)
{
	/* TODO This is only stub */
	return 0;
}

M0_INTERNAL int m0_capa_ctxt_init(struct m0_capa_ctxt *ctxt)
{
	/* TODO This is only stub */
	return 0;
}

M0_INTERNAL void m0_capa_ctxt_fini(struct m0_capa_ctxt *ctxt)
{
	/* TODO This is only stub */
}

/** @} end group capa */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
