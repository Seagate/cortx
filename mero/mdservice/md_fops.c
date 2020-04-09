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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 03/29/2011
 */

#include "lib/string.h"
#include "lib/memory.h"
#include "lib/errno.h"
#include "fop/fop.h"
#include "fop/fom_generic.h"
#include "fop/fop_item_type.h"
#include "rpc/rpc_opcodes.h"
#include "mdservice/md_foms.h"
#include "mdservice/md_fops.h"
#include "mdservice/md_fops_xc.h"

static int md_fol_frag_undo(struct m0_fop_fol_frag *ffrag,
			    struct m0_fol *fol);
static int md_fol_frag_redo(struct m0_fop_fol_frag *ffrag,
				struct m0_fol *fol);

const struct m0_fop_type_ops m0_md_fop_ops = {
	.fto_undo = md_fol_frag_undo,
	.fto_redo = md_fol_frag_redo,
};

#ifndef __KERNEL__
struct m0_fom_type_ops m0_md_fom_ops = {
	/* This field is patched by UT. */
	.fto_create = m0_md_req_fom_create
};

extern struct m0_reqh_service_type m0_mds_type;
#endif

struct m0_fop_type m0_fop_create_fopt;
struct m0_fop_type m0_fop_lookup_fopt;
struct m0_fop_type m0_fop_link_fopt;
struct m0_fop_type m0_fop_unlink_fopt;
struct m0_fop_type m0_fop_open_fopt;
struct m0_fop_type m0_fop_close_fopt;
struct m0_fop_type m0_fop_setattr_fopt;
struct m0_fop_type m0_fop_getattr_fopt;
struct m0_fop_type m0_fop_setxattr_fopt;
struct m0_fop_type m0_fop_getxattr_fopt;
struct m0_fop_type m0_fop_delxattr_fopt;
struct m0_fop_type m0_fop_listxattr_fopt;
struct m0_fop_type m0_fop_statfs_fopt;
struct m0_fop_type m0_fop_rename_fopt;
struct m0_fop_type m0_fop_readdir_fopt;

struct m0_fop_type m0_fop_create_rep_fopt;
struct m0_fop_type m0_fop_lookup_rep_fopt;
struct m0_fop_type m0_fop_link_rep_fopt;
struct m0_fop_type m0_fop_unlink_rep_fopt;
struct m0_fop_type m0_fop_open_rep_fopt;
struct m0_fop_type m0_fop_close_rep_fopt;
struct m0_fop_type m0_fop_setattr_rep_fopt;
struct m0_fop_type m0_fop_getattr_rep_fopt;
struct m0_fop_type m0_fop_setxattr_rep_fopt;
struct m0_fop_type m0_fop_getxattr_rep_fopt;
struct m0_fop_type m0_fop_delxattr_rep_fopt;
struct m0_fop_type m0_fop_listxattr_rep_fopt;
struct m0_fop_type m0_fop_statfs_rep_fopt;
struct m0_fop_type m0_fop_rename_rep_fopt;
struct m0_fop_type m0_fop_readdir_rep_fopt;

M0_INTERNAL int m0_mdservice_fopts_init(void)
{
	M0_FOP_TYPE_INIT(&m0_fop_create_fopt,
			 .name      = "create",
			 .opcode    = M0_MDSERVICE_CREATE_OPCODE,
			 .xt        = m0_fop_create_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_lookup_fopt,
			 .name      = "lookup",
			 .opcode    = M0_MDSERVICE_LOOKUP_OPCODE,
			 .xt        = m0_fop_lookup_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_link_fopt,
			 .name      = "hardlink",
			 .opcode    = M0_MDSERVICE_LINK_OPCODE,
			 .xt        = m0_fop_link_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_unlink_fopt,
			 .name      = "unlink",
			 .opcode    = M0_MDSERVICE_UNLINK_OPCODE,
			 .xt        = m0_fop_unlink_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_open_fopt,
			 .name      = "open",
			 .opcode    = M0_MDSERVICE_OPEN_OPCODE,
			 .xt        = m0_fop_open_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_close_fopt,
			 .name      = "close",
			 .opcode    = M0_MDSERVICE_CLOSE_OPCODE,
			 .xt        = m0_fop_close_xc,
			 /*
			  * Close needs transactions for open-unlinked case.
			  */
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_setattr_fopt,
			 .name      = "setattr",
			 .opcode    = M0_MDSERVICE_SETATTR_OPCODE,
			 .xt        = m0_fop_setattr_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_getattr_fopt,
			 .name      = "getattr",
			 .opcode    = M0_MDSERVICE_GETATTR_OPCODE,
			 .xt        = m0_fop_getattr_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_setxattr_fopt,
			 .name      = "setxattr",
			 .opcode    = M0_MDSERVICE_SETXATTR_OPCODE,
			 .xt        = m0_fop_setxattr_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_getxattr_fopt,
			 .name      = "getxattr",
			 .opcode    = M0_MDSERVICE_GETXATTR_OPCODE,
			 .xt        = m0_fop_getxattr_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_delxattr_fopt,
			 .name      = "delxattr",
			 .opcode    = M0_MDSERVICE_DELXATTR_OPCODE,
			 .xt        = m0_fop_delxattr_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_listxattr_fopt,
			 .name      = "listxattr",
			 .opcode    = M0_MDSERVICE_LISTXATTR_OPCODE,
			 .xt        = m0_fop_listxattr_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_statfs_fopt,
			 .name      = "statfs",
			 .opcode    = M0_MDSERVICE_STATFS_OPCODE,
			 .xt        = m0_fop_statfs_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_rename_fopt,
			 .name      = "rename",
			 .opcode    = M0_MDSERVICE_RENAME_OPCODE,
			 .xt        = m0_fop_rename_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	M0_FOP_TYPE_INIT(&m0_fop_readdir_fopt,
			 .name      = "readdir",
			 .opcode    = M0_MDSERVICE_READDIR_OPCODE,
			 .xt        = m0_fop_readdir_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &m0_md_fop_ops,
#ifndef __KERNEL__
			 .fom_ops   = &m0_md_fom_ops,
			 .svc_type  = &m0_mds_type,
#endif
			 .sm        = &m0_generic_conf);
	return 0;
}

M0_INTERNAL int m0_mdservice_rep_fopts_init(void)
{
	M0_FOP_TYPE_INIT(&m0_fop_create_rep_fopt,
			 .name      = "create-reply",
			 .opcode    = M0_MDSERVICE_CREATE_REP_OPCODE,
			 .xt        = m0_fop_create_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_lookup_rep_fopt,
			 .name      = "lookup-reply",
			 .opcode    = M0_MDSERVICE_LOOKUP_REP_OPCODE,
			 .xt        = m0_fop_lookup_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_link_rep_fopt,
			 .name      = "hardlink-reply",
			 .opcode    = M0_MDSERVICE_LINK_REP_OPCODE,
			 .xt        = m0_fop_link_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_unlink_rep_fopt,
			 .name      = "unlink-reply",
			 .opcode    = M0_MDSERVICE_UNLINK_REP_OPCODE,
			 .xt        = m0_fop_unlink_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_open_rep_fopt,
			 .name      = "open-reply",
			 .opcode    = M0_MDSERVICE_OPEN_REP_OPCODE,
			 .xt        = m0_fop_open_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_close_rep_fopt,
			 .name      = "close-reply",
			 .opcode    = M0_MDSERVICE_CLOSE_REP_OPCODE,
			 .xt        = m0_fop_close_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_setattr_rep_fopt,
			 .name      = "setattr-reply",
			 .opcode    = M0_MDSERVICE_SETATTR_REP_OPCODE,
			 .xt        = m0_fop_setattr_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_getattr_rep_fopt,
			 .name      = "getattr-reply",
			 .opcode    = M0_MDSERVICE_GETATTR_REP_OPCODE,
			 .xt        = m0_fop_getattr_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_setxattr_rep_fopt,
			 .name      = "setxattr-reply",
			 .opcode    = M0_MDSERVICE_SETXATTR_REP_OPCODE,
			 .xt        = m0_fop_setxattr_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_getxattr_rep_fopt,
			 .name      = "getxattr-reply",
			 .opcode    = M0_MDSERVICE_GETXATTR_REP_OPCODE,
			 .xt        = m0_fop_getxattr_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_delxattr_rep_fopt,
			 .name      = "delxattr-reply",
			 .opcode    = M0_MDSERVICE_DELXATTR_REP_OPCODE,
			 .xt        = m0_fop_delxattr_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_listxattr_rep_fopt,
			 .name      = "listxattr-reply",
			 .opcode    = M0_MDSERVICE_LISTXATTR_REP_OPCODE,
			 .xt        = m0_fop_listxattr_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_statfs_rep_fopt,
			 .name      = "statfs-reply",
			 .opcode    = M0_MDSERVICE_STATFS_REP_OPCODE,
			 .xt        = m0_fop_statfs_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_rename_rep_fopt,
			 .name      = "rename-reply",
			 .opcode    = M0_MDSERVICE_RENAME_REP_OPCODE,
			 .xt        = m0_fop_rename_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	M0_FOP_TYPE_INIT(&m0_fop_readdir_rep_fopt,
			 .name      = "readdir-reply",
			 .opcode    = M0_MDSERVICE_READDIR_REP_OPCODE,
			 .xt        = m0_fop_readdir_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);
	return 0;
}

M0_INTERNAL int m0_mdservice_fop_init(void)
{
	return	m0_mdservice_fopts_init() ?:
		m0_mdservice_rep_fopts_init();
}
M0_EXPORTED(m0_mdservice_fop_init);

M0_INTERNAL void m0_mdservice_fop_fini(void)
{
	m0_fop_type_fini(&m0_fop_create_fopt);
	m0_fop_type_fini(&m0_fop_lookup_fopt);
	m0_fop_type_fini(&m0_fop_link_fopt);
	m0_fop_type_fini(&m0_fop_unlink_fopt);
	m0_fop_type_fini(&m0_fop_open_fopt);
	m0_fop_type_fini(&m0_fop_close_fopt);
	m0_fop_type_fini(&m0_fop_setattr_fopt);
	m0_fop_type_fini(&m0_fop_getattr_fopt);
	m0_fop_type_fini(&m0_fop_setxattr_fopt);
	m0_fop_type_fini(&m0_fop_getxattr_fopt);
	m0_fop_type_fini(&m0_fop_delxattr_fopt);
	m0_fop_type_fini(&m0_fop_listxattr_fopt);
	m0_fop_type_fini(&m0_fop_statfs_fopt);
	m0_fop_type_fini(&m0_fop_rename_fopt);
	m0_fop_type_fini(&m0_fop_readdir_fopt);

	m0_fop_type_fini(&m0_fop_create_rep_fopt);
	m0_fop_type_fini(&m0_fop_lookup_rep_fopt);
	m0_fop_type_fini(&m0_fop_link_rep_fopt);
	m0_fop_type_fini(&m0_fop_unlink_rep_fopt);
	m0_fop_type_fini(&m0_fop_open_rep_fopt);
	m0_fop_type_fini(&m0_fop_close_rep_fopt);
	m0_fop_type_fini(&m0_fop_setattr_rep_fopt);
	m0_fop_type_fini(&m0_fop_getattr_rep_fopt);
	m0_fop_type_fini(&m0_fop_setxattr_rep_fopt);
	m0_fop_type_fini(&m0_fop_getxattr_rep_fopt);
	m0_fop_type_fini(&m0_fop_delxattr_rep_fopt);
	m0_fop_type_fini(&m0_fop_listxattr_rep_fopt);
	m0_fop_type_fini(&m0_fop_statfs_rep_fopt);
	m0_fop_type_fini(&m0_fop_rename_rep_fopt);
	m0_fop_type_fini(&m0_fop_readdir_rep_fopt);

	m0_xc_mdservice_md_fops_fini();
}
M0_EXPORTED(m0_mdservice_fop_fini);

static int md_fol_frag_undo(struct m0_fop_fol_frag *ffrag,
			        struct m0_fol *fol)
{
	/**
	 * @todo Perform the undo operation for meta-data
	 * updates using the generic fop fol fragment.
	 */
	return 0;
}

static int md_fol_frag_redo(struct m0_fop_fol_frag *ffrag,
			        struct m0_fol *fol)
{
	/**
	 * @todo Perform the redo operation for meta-data
	 * updates using the generic fop fol fragment.
	 */
	return 0;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
