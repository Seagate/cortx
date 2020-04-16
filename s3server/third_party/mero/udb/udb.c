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
 * Original author: Huang Hua <Hua_Huang@xyratex.com>
 * Original creation date: 01/04/2011
 */

#include "udb.h"

/**
   @addtogroup udb
   @{
 */


M0_INTERNAL int m0_udb_ctxt_init(struct m0_udb_ctxt *ctxt)
{
	/* TODO add more here. Now it is a stub */
	return 0;
}

M0_INTERNAL void m0_udb_ctxt_fini(struct m0_udb_ctxt *ctxt)
{

	/* TODO add more here. Now it is a stub */
	return;
}

M0_INTERNAL int m0_udb_add(struct m0_udb_ctxt *ctxt,
			   const struct m0_udb_domain *edomain,
			   const struct m0_udb_cred *external,
			   const struct m0_udb_cred *internal)
{
	/* TODO add more here. Now it is a stub */
	return 0;
}

M0_INTERNAL int m0_udb_del(struct m0_udb_ctxt *ctxt,
			   const struct m0_udb_domain *edomain,
			   const struct m0_udb_cred *external,
			   const struct m0_udb_cred *internal)
{

	/* TODO add more here. Now it is a stub */
	return 0;
}

M0_INTERNAL int m0_udb_e2i(struct m0_udb_ctxt *ctxt,
			   const struct m0_udb_cred *external,
			   struct m0_udb_cred *internal)
{

	/* TODO add more here. Now it is a stub */
	return 0;
}

M0_INTERNAL int m0_udb_i2e(struct m0_udb_ctxt *ctxt,
			   const struct m0_udb_cred *internal,
			   struct m0_udb_cred *external)
{

	/* TODO add more here. Now it is a stub */
	return 0;
}

/** @} end group udb */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
