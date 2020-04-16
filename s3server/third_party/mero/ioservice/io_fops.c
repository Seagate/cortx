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
 * Original author: Anand Vidwansa <Anand_Vidwansa@xyratex.com>
 *                  Anup Barve <Anup_Barve@xyratex.com>
 * Original creation date: 03/21/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_IOSERVICE
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/vec.h"    /* m0_0vec */
#include "lib/misc.h"   /* M0_IN */
#include "lib/tlist.h"
#include "reqh/reqh.h"
#include "mero/magic.h"
#include "fop/fop_item_type.h"
#include "rpc/item.h"
#include "rpc/rpc_opcodes.h"
#include "rpc/rpc.h"
#include "fop/fom_generic.h"
#include "file/file.h"
#include "lib/finject.h"
#include "cob/cob.h"
#include "mdservice/fsync_foms.h"       /* m0_fsync_fom_conf */
#include "mdservice/fsync_fops.h"       /* m0_fsync_fom_ops */
#include "mdservice/fsync_fops_xc.h"    /* m0_fop_fsync_xc */
#include "ioservice/io_addb2.h"
#include "ioservice/io_foms.h"
#include "ioservice/io_fops.h"
#include "ioservice/io_fops_xc.h"
#include "ioservice/cob_foms.h"
#ifndef __KERNEL__
  #include "clovis/clovis_internal.h"
#else
  #include "m0t1fs/linux_kernel/m0t1fs.h"
#endif

/* tlists and tlist APIs referred from rpc layer. */
M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);
M0_TL_DESCR_DECLARE(rpcitem, M0_EXTERN);
M0_TL_DECLARE(rpcbulk, M0_INTERNAL, struct m0_rpc_bulk_buf);
M0_TL_DECLARE(rpcitem, M0_INTERNAL, struct m0_rpc_item);

static struct m0_fid *io_fop_fid_get(struct m0_fop *fop);

static void io_item_replied (struct m0_rpc_item *item);
static void io_fop_replied  (struct m0_fop *fop, struct m0_fop *bkpfop);
static void io_fop_desc_get (struct m0_fop *fop,
			     struct m0_net_buf_desc_data **desc);
static int  io_fop_coalesce (struct m0_fop *res_fop, uint64_t size);
static void item_io_coalesce(struct m0_rpc_item *head, struct m0_list *list,
			     uint64_t size);

struct m0_fop_type m0_fop_cob_readv_fopt;
struct m0_fop_type m0_fop_cob_writev_fopt;
struct m0_fop_type m0_fop_cob_readv_rep_fopt;
struct m0_fop_type m0_fop_cob_writev_rep_fopt;
struct m0_fop_type m0_fop_cob_create_fopt;
struct m0_fop_type m0_fop_cob_delete_fopt;
struct m0_fop_type m0_fop_cob_truncate_fopt;
struct m0_fop_type m0_fop_cob_op_reply_fopt;
struct m0_fop_type m0_fop_fv_notification_fopt;
struct m0_fop_type m0_fop_cob_getattr_fopt;
struct m0_fop_type m0_fop_cob_getattr_reply_fopt;
struct m0_fop_type m0_fop_fsync_ios_fopt;
struct m0_fop_type m0_fop_cob_setattr_fopt;
struct m0_fop_type m0_fop_cob_setattr_reply_fopt;

M0_EXPORTED(m0_fop_cob_writev_fopt);
M0_EXPORTED(m0_fop_cob_readv_fopt);

static struct m0_fop_type *ioservice_fops[] = {
	&m0_fop_cob_readv_fopt,
	&m0_fop_cob_writev_fopt,
	&m0_fop_cob_readv_rep_fopt,
	&m0_fop_cob_writev_rep_fopt,
	&m0_fop_cob_create_fopt,
	&m0_fop_cob_delete_fopt,
	&m0_fop_cob_truncate_fopt,
	&m0_fop_cob_op_reply_fopt,
	&m0_fop_fv_notification_fopt,
	&m0_fop_cob_getattr_fopt,
	&m0_fop_cob_getattr_reply_fopt,
	&m0_fop_fsync_ios_fopt,
	&m0_fop_cob_setattr_fopt,
	&m0_fop_cob_setattr_reply_fopt,
};

/* Used for IO REQUEST items only. */
const struct m0_rpc_item_ops io_req_rpc_item_ops = {
	.rio_replied = io_item_replied,
};

static const struct m0_rpc_item_type_ops io_item_type_ops = {
	M0_FOP_DEFAULT_ITEM_TYPE_OPS,
        .rito_io_coalesce = item_io_coalesce,
};

static int io_fol_frag_undo_redo_op(struct m0_fop_fol_frag *frag,
					struct m0_fol *fol)
{
	struct m0_fop_cob_writev_rep *wfop;

	M0_PRE(frag != NULL);

	wfop = frag->ffrp_rep;
	switch(frag->ffrp_fop_code) {
	case M0_IOSERVICE_WRITEV_OPCODE:
		M0_ASSERT(wfop->c_rep.rwr_rc == 0);
		break;
	}
	return 0;
}

M0_INTERNAL void m0_dump_cob_attr(const struct m0_cob_attr *attr)
{
	uint32_t valid = attr->ca_valid;
#define	level M0_DEBUG
	M0_LOG(level, "pfid = "FID_F, FID_P(&attr->ca_pfid));
	M0_LOG(level, "tfid = "FID_F, FID_P(&attr->ca_tfid));
	if (valid & M0_COB_MODE)
		M0_LOG(level, "mode = %o", attr->ca_mode);
	if (valid & M0_COB_UID)
		M0_LOG(level, "uid = %u", attr->ca_uid);
	if (valid & M0_COB_GID)
		M0_LOG(level, "gid = %u", attr->ca_gid);
	if (valid & M0_COB_ATIME)
		M0_LOG(level, "atime = %llu",
			      (unsigned long long)attr->ca_atime);
	if (valid & M0_COB_MTIME)
		M0_LOG(level, "mtime = %llu",
			      (unsigned long long)attr->ca_mtime);
	if (valid & M0_COB_CTIME)
		M0_LOG(level, "ctime = %llu",
			      (unsigned long long)attr->ca_ctime);
	if (valid & M0_COB_NLINK)
		M0_LOG(level, "nlink = %u", attr->ca_nlink);
	if (valid & M0_COB_RDEV)
		M0_LOG(level, "rdev = %llu", (unsigned long long)attr->ca_rdev);
	if (valid & M0_COB_SIZE)
		M0_LOG(level, "size = %llu", (unsigned long long)attr->ca_size);
	if (valid & M0_COB_BLKSIZE)
		M0_LOG(level, "blksize = %llu",
			      (unsigned long long)attr->ca_blksize);
	if (valid & M0_COB_BLOCKS)
		M0_LOG(level, "blocks = %llu",
			      (unsigned long long)attr->ca_blocks);
	if (valid & M0_COB_LID)
		M0_LOG(level, "lid = %llu", (unsigned long long)attr->ca_lid);
	if (valid & M0_COB_PVER)
		M0_LOG(level, "pver = "FID_F, FID_P(&attr->ca_pver));
#undef level
}

#ifndef __KERNEL__
M0_BASSERT(M0_IOSERVICE_COB_DELETE_OPCODE ==
	   M0_IOSERVICE_COB_CREATE_OPCODE + 1);
M0_BASSERT(sizeof(struct m0_fop_cob_create) ==
	   sizeof(struct m0_fop_cob_delete));

static int io_fol_cd_rec_frag_op(struct m0_fop_fol_frag *frag,
				 struct m0_fol *fol, bool undo)
{
	int                    result;
	struct m0_fop         *fop;
	struct m0_reqh        *reqh = container_of(fol, struct m0_reqh, rh_fol);
	struct m0_fom         *fom;
	int                    delete;
	struct m0_rpc_machine *rpcmach;

	M0_PRE(reqh != NULL);
	M0_PRE(frag != NULL);
	M0_PRE(M0_IN(frag->ffrp_fop_code, (M0_IOSERVICE_COB_CREATE_OPCODE,
					    M0_IOSERVICE_COB_DELETE_OPCODE)));

	rpcmach = m0_reqh_rpc_mach_tlist_head(&reqh->rh_rpc_machines);
	M0_ASSERT(rpcmach != NULL);

	delete = frag->ffrp_fop_code - M0_IOSERVICE_COB_CREATE_OPCODE;
	if (undo)
		delete = 1 - delete;
	fop = m0_fop_alloc(delete ?
			   &m0_fop_cob_delete_fopt : &m0_fop_cob_create_fopt,
			   frag->ffrp_fop, rpcmach);
	result = fop != NULL ? m0_cob_fom_create(fop, &fom, reqh) : -ENOMEM;
	if (result == 0) {
		fom->fo_local = true;
		m0_fom_queue(fom);
	}
	return result;
}
#else
static int io_fol_cd_rec_frag_op(struct m0_fop_fol_frag *frag,
				 struct m0_fol *fol, bool undo)
{
	return 0;
}
#endif

static int io_fol_cd_rec_frag_undo(struct m0_fop_fol_frag *frag,
				   struct m0_fol *fol)
{
	return io_fol_cd_rec_frag_op(frag, fol, true);
}

static int io_fol_cd_rec_frag_redo(struct m0_fop_fol_frag *frag,
				   struct m0_fol *fol)
{
	return io_fol_cd_rec_frag_op(frag, fol, false);
}

const struct m0_fop_type_ops io_fop_rwv_ops = {
	.fto_fop_replied = io_fop_replied,
	.fto_io_coalesce = io_fop_coalesce,
	.fto_io_desc_get = io_fop_desc_get,
	.fto_undo        = io_fol_frag_undo_redo_op,
	.fto_redo        = io_fol_frag_undo_redo_op,
};

const struct m0_fop_type_ops io_fop_cd_ops = {
	.fto_undo = io_fol_cd_rec_frag_undo,
	.fto_redo = io_fol_cd_rec_frag_redo,
};

extern struct m0_reqh_service_type m0_ios_type;
extern const struct m0_fom_type_ops cob_fom_type_ops;
extern const struct m0_fom_type_ops io_fom_type_ops;

extern struct m0_sm_conf io_conf;
extern struct m0_sm_state_descr io_phases[];
extern const struct m0_sm_conf cob_ops_conf;
extern struct m0_sm_state_descr cob_ops_phases[];

M0_INTERNAL void m0_ioservice_fop_fini(void)
{
	m0_fop_type_addb2_deinstrument(&m0_fop_cob_readv_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_cob_writev_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_cob_create_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_cob_delete_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_cob_getattr_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_cob_setattr_fopt);
	m0_fop_type_addb2_deinstrument(&m0_fop_cob_truncate_fopt);

	m0_fop_type_fini(&m0_fop_cob_readv_fopt);
	m0_fop_type_fini(&m0_fop_cob_writev_fopt);
	m0_fop_type_fini(&m0_fop_cob_readv_rep_fopt);
	m0_fop_type_fini(&m0_fop_cob_writev_rep_fopt);
	m0_fop_type_fini(&m0_fop_cob_create_fopt);
	m0_fop_type_fini(&m0_fop_cob_delete_fopt);
	m0_fop_type_fini(&m0_fop_cob_truncate_fopt);
	m0_fop_type_fini(&m0_fop_cob_op_reply_fopt);
	m0_fop_type_fini(&m0_fop_fv_notification_fopt);
	m0_fop_type_fini(&m0_fop_cob_getattr_fopt);
	m0_fop_type_fini(&m0_fop_cob_getattr_reply_fopt);
	m0_fop_type_fini(&m0_fop_fsync_ios_fopt);
	m0_fop_type_fini(&m0_fop_cob_setattr_fopt);
	m0_fop_type_fini(&m0_fop_cob_setattr_reply_fopt);

#ifndef __KERNEL__
	m0_sm_conf_fini(&io_conf);
#endif
}

M0_INTERNAL int m0_ioservice_fop_init(void)
{
	const struct m0_sm_conf *p_cob_ops_conf;
#ifndef __KERNEL__
	p_cob_ops_conf = &cob_ops_conf;
	m0_sm_conf_extend(m0_generic_conf.scf_state, io_phases,
			  m0_generic_conf.scf_nr_states);
	m0_sm_conf_extend(m0_generic_conf.scf_state, cob_ops_phases,
			  m0_generic_conf.scf_nr_states);

	m0_sm_conf_trans_extend(&m0_generic_conf, &io_conf);

	io_conf.scf_state[M0_FOPH_TXN_INIT].sd_allowed |=
		M0_BITS(M0_FOPH_IO_FOM_PREPARE);

	m0_sm_conf_init(&io_conf);
#else
	p_cob_ops_conf = &m0_generic_conf;
#endif
	M0_FOP_TYPE_INIT(&m0_fop_cob_readv_fopt,
			 .name      = "read",
			 .opcode    = M0_IOSERVICE_READV_OPCODE,
			 .xt        = m0_fop_cob_readv_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
			 .fop_ops   = &io_fop_rwv_ops,
#ifndef __KERNEL__
			 .fom_ops   = &io_fom_type_ops,
			 .sm        = &io_conf,
			 .svc_type  = &m0_ios_type,
#endif
			 .rpc_ops   = &io_item_type_ops);

	M0_FOP_TYPE_INIT(&m0_fop_cob_writev_fopt,
			 .name      = "write",
			 .opcode    = M0_IOSERVICE_WRITEV_OPCODE,
			 .xt        = m0_fop_cob_writev_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &io_fop_rwv_ops,
#ifndef __KERNEL__
			 .fom_ops   = &io_fom_type_ops,
			 .sm        = &io_conf,
			 .svc_type  = &m0_ios_type,
#endif
			 .rpc_ops   = &io_item_type_ops);

	M0_FOP_TYPE_INIT(&m0_fop_cob_readv_rep_fopt,
			 .name      = "read-reply",
			 .opcode    = M0_IOSERVICE_READV_REP_OPCODE,
			 .xt        = m0_fop_cob_readv_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);

	M0_FOP_TYPE_INIT(&m0_fop_cob_writev_rep_fopt,
			 .name      = "write-reply",
			 .opcode    = M0_IOSERVICE_WRITEV_REP_OPCODE,
			 .xt        = m0_fop_cob_writev_rep_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);

	M0_FOP_TYPE_INIT(&m0_fop_cob_create_fopt,
			 .name      = "cob-create",
			 .opcode    = M0_IOSERVICE_COB_CREATE_OPCODE,
			 .xt        = m0_fop_cob_create_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &io_fop_cd_ops,
#ifndef __KERNEL__
			 .fom_ops   = &cob_fom_type_ops,
			 .svc_type  = &m0_ios_type,
#endif
			 .sm        = p_cob_ops_conf);

	M0_FOP_TYPE_INIT(&m0_fop_cob_delete_fopt,
			 .name      = "cob-delete",
			 .opcode    = M0_IOSERVICE_COB_DELETE_OPCODE,
			 .xt        = m0_fop_cob_delete_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &io_fop_cd_ops,
#ifndef __KERNEL__
			 .fom_ops   = &cob_fom_type_ops,
			 .svc_type  = &m0_ios_type,
#endif
			 .sm        = p_cob_ops_conf);

	M0_FOP_TYPE_INIT(&m0_fop_cob_truncate_fopt,
			 .name      = "cob-truncate",
			 .opcode    = M0_IOSERVICE_COB_TRUNCATE_OPCODE,
			 .xt        = m0_fop_cob_truncate_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = &io_fop_cd_ops,
#ifndef __KERNEL__
			 .fom_ops   = &cob_fom_type_ops,
			 .svc_type  = &m0_ios_type,
#endif
			 .sm        = p_cob_ops_conf);

	M0_FOP_TYPE_INIT(&m0_fop_cob_op_reply_fopt,
			 .name      = "cob-reply",
			 .opcode    = M0_IOSERVICE_COB_OP_REPLY_OPCODE,
			 .xt        = m0_fop_cob_op_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);

	M0_FOP_TYPE_INIT(&m0_fop_cob_getattr_fopt,
			 .name      = "getattr",
			 .opcode    = M0_IOSERVICE_COB_GETATTR_OPCODE,
			 .xt        = m0_fop_cob_getattr_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST,
#ifndef __KERNEL__
			 .fom_ops   = &cob_fom_type_ops,
			 .svc_type  = &m0_ios_type,
#endif
			 .sm        = p_cob_ops_conf);

	M0_FOP_TYPE_INIT(&m0_fop_cob_getattr_reply_fopt,
			 .name      = "getattr-reply",
			 .opcode    = M0_IOSERVICE_COB_GETATTR_REP_OPCODE,
			 .xt        = m0_fop_cob_getattr_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);

	M0_FOP_TYPE_INIT(&m0_fop_fsync_ios_fopt,
			 .name      = "fsync-ios",
			 .opcode    = M0_FSYNC_IOS_OPCODE,
			 .xt        = m0_fop_fsync_xc,
#ifndef __KERNEL__
			 .svc_type  = &m0_ios_type,
			 .sm        = &m0_fsync_fom_conf,
			 .fom_ops   = &m0_fsync_fom_ops,
#endif
			 .rpc_flags = M0_RPC_ITEM_TYPE_REQUEST);
	M0_FOP_TYPE_INIT(&m0_fop_cob_setattr_fopt,
			 .name      = "setattr",
			 .opcode    = M0_IOSERVICE_COB_SETATTR_OPCODE,
			 .xt        = m0_fop_cob_setattr_xc,
			 .rpc_flags = M0_RPC_MUTABO_REQ,
			 .fop_ops   = NULL,
#ifndef __KERNEL__
			 .fom_ops   = &cob_fom_type_ops,
			 .svc_type  = &m0_ios_type,
#endif
			 .sm        = p_cob_ops_conf);

	M0_FOP_TYPE_INIT(&m0_fop_cob_setattr_reply_fopt,
			 .name      = "setattr-reply",
			 .opcode    = M0_IOSERVICE_COB_SETATTR_REP_OPCODE,
			 .xt        = m0_fop_cob_setattr_reply_xc,
			 .rpc_flags = M0_RPC_ITEM_TYPE_REPLY);

	return  m0_fop_type_addb2_instrument(&m0_fop_cob_readv_fopt)   ?:
		m0_fop_type_addb2_instrument(&m0_fop_cob_writev_fopt)  ?:
		m0_fop_type_addb2_instrument(&m0_fop_cob_create_fopt)  ?:
		m0_fop_type_addb2_instrument(&m0_fop_cob_delete_fopt)  ?:
		m0_fop_type_addb2_instrument(&m0_fop_cob_getattr_fopt) ?:
		m0_fop_type_addb2_instrument(&m0_fop_cob_setattr_fopt) ?:
		m0_fop_type_addb2_instrument(&m0_fop_cob_truncate_fopt);
}

/**
   @page IOFOLDLD IO FOL DLD

   - @ref IOFOLDLD-ovw
   - @ref IOFOLDLD-def
   - @ref IOFOLDLD-req
   - @ref IOFOLDLD-depends
   - @ref IOFOLDLD-highlights
   - @ref IOFOLDLD-fspec "Functional Specification"
   - @ref IOFOLDLD-lspec
   - @ref IOFOLDLD-ut
   - @ref IOFOLDLD-st
   - @ref IOFOLDLD-O
   - @ref IOFOLDLD-ref

   <hr>
   @section IOFOLDLD-ovw Overview
   This document describes the design of logging of FOL records for create, delete
   and write operations in ioservice.

   <hr>
   @section IOFOLDLD-def Definitions

   <hr>
   @section IOFOLDLD-req Requirements

   <hr>
   @section IOFOLDLD-depends Dependencies

   For every new write data must be written to new block.
   So that these older data blocks can be used to perform write undo.
   <hr>
   @section IOFOLDLD-highlights Design Highlights

   For each update made on server corresponding FOL record fragment is
   populated and added in the FOM transaction FOL record fragments list.

   These FOL record fragments are encoded in a single FOL record in a
   FOM generic phase after updates are executed.

   <hr>
   @section IOFOLDLD-fspec Functional Specification

   @see ad_rec_frag is added for AD write operation.

   For each of create, delete and write IO operations FOL record fragments are
   added in FOM generic phase M0_FOPH_FOL_FRAG_ADD using m0_fop_fol_add().

   For create and delete operations fop data and reply fop data is stored
   in FOL record fragments.
	- fop data including fid.
	- Reply fop data is added in FOL records so that it can be used
	  as Reply Cache.

   For write operation, in ad_write_launch() store AD allocated extents in
   FOL record fragment struct ad_rec_frag.

   All these FOL record fragments are to the list in the transaction record.

   <hr>
   @section IOFOLDLD-ut Unit Tests

   1) For create and delete updates,
	An io fop with a given fid is send to the ioservice, where it creates
	a cob with that fid and logs a FOL record.

	Now retrieve that FOL record using the same LSN and assert for fid and
	reply fop data.

	Also using this data, execute the cob delete operation on server side
	(undo operation).

	Similarly, do the same things for delete operation.

   2) For Write update,
	Send the data having value "A" from client to ioservice which logs fid
	and data extents in FOL record. Then send the data having value "B" to
	the ioservice.

	Now retrieve the data extents of the first write operation from FOL record
	and update the AD table by decoding ad_rec_frag from FOL record.
	Then read the data from ioservice and assert for data "A".

   @endcode
   <hr>
   @section IOFOLDLD-st System Tests

   <hr>
   @section IOFOLDLD-O Analysis

   <hr>
   @section IOFOLDLD-ref References
   - <a href="https://docs.google.com/a/seagate.com/document/d/1tHxI-UksRRSB-gkM
nLi2FJhUeLPWCnnPuucqAI9cZzw/edit"> HLD of version numbers </a>
   - <a href="https://docs.google.com/a/seagate.com/document/d/1Rca4BVw3EatIQ-wQ
6XsB-xRBSlVmN9wIcbuVKeZ8lD4/edit"> HLD of FOL</a>,
   - <a href="https://docs.google.com/a/seagate.com/document/d/1b1HmJJCrn4IzY8QT
E6IwXtA7gywIl_sjYxd8laakiAw/edit">HLD of data block allocator</a>.
   - @ref fol
   - @ref stobad

 */

/**
   @page io_bulk_client IO bulk transfer Detailed Level Design.

   - @ref bulkclient-ovw
   - @ref bulkclient-def
   - @ref bulkclient-req
   - @ref bulkclient-depends
   - @ref bulkclient-highlights
   - @subpage bulkclient-fspec "IO bulk client Func Spec"
   - @ref bulkclient-lspec
      - @ref bulkclient-lspec-comps
      - @ref bulkclient-lspec-sc1
      - @ref bulkclient-lspec-state
      - @ref bulkclient-lspec-thread
      - @ref bulkclient-lspec-numa
   - @ref bulkclient-conformance
   - @ref bulkclient-ut
   - @ref bulkclient-st
   - @ref bulkclient-O
   - @ref bulkclient-ref

   <hr>
   @section bulkclient-ovw Overview

   This document describes the working of client side of io bulk transfer.
   This functionality is used only for io path.
   IO bulk client constitues the client side of bulk IO carried out between
   Mero client file system and data server (ioservice aka bulk io server).
   Mero network layer incorporates a bulk transport mechanism to
   transfer user buffers in zero-copy fashion.
   The generic io fop contains a network buffer descriptor which refers to a
   network buffer.
   The bulk client creates IO fops and attaches the kernel pages or a vector
   in user mode to net buffer associated with io fop and submits it
   to rpc layer.
   The rpc layer populates the net buffer descriptor from io fop and sends
   the fop over wire.
   The receiver starts the zero-copy of buffers using the net buffer
   descriptor from io fop.

   <hr>
   @section bulkclient-def Definitions

   - m0t1fs - Mero client file system. It works as a kernel module.
   - Bulk transport - Event based, asynchronous message passing functionality
   of Mero network layer.
   - io fop - A generic io fop that is used for read and write.
   - rpc bulk - An interface to abstract the usage of network buffers by
   client and server programs.
   - ioservice - A service providing io routines in Mero. It runs only
   on server side.

   <hr>
   @section bulkclient-req Requirements

   - R.bulkclient.rpcbulk The bulk client should use rpc bulk abstraction
   while enqueueing buffers for bulk transfer.
   - R.bulkclient.fopcreation The bulk client should create io fops as needed
   if pages overrun the existing rpc bulk structure.
   - R.bulkclient.netbufdesc The generic io fop should contain a network
   buffer descriptor which points to an in-memory network buffer.
   - R.bulkclient.iocoalescing The IO coalescing code should conform to
   new format of io fop. This is actually a side-effect and not a core
   part of functionality. Since the format of IO fop changes, the IO
   coalescing code which depends on it, needs to be restructured.

   <hr>
   @section bulkclient-depends Dependencies

   - r.misc.net_rpc_convert Bulk Client needs Mero client file system to be
   using new network layer apis which include m0_net_domain and m0_net_buffer.
   - r.fop.referring_another_fop With introduction of a net buffer
   descriptor in io fop, a mechanism needs to be introduced so that fop
   definitions from one component can refer to definitions from another
   component. m0_net_buf_desc is a fop used to represent on-wire
   representation of a m0_net_buffer. @see m0_net_buf_desc.

   <hr>
   @section bulkclient-highlights Design Highlights

   IO bulk client uses a generic in-memory structure representing an io fop
   and its associated network buffer.
   This in-memory io fop contains another abstract structure to represent
   the network buffer associated with the fop.
   The bulk client creates m0_io_fop structures as necessary and attaches
   kernel pages or user space vector to associated m0_rpc_bulk structure
   and submits the fop to rpc layer.
   Rpc layer populates the network buffer descriptor embedded in the io fop
   and sends the fop over wire. The associated network buffer is added to
   appropriate buffer queue of transfer machine owned by rpc layer.
   Once, the receiver side receives the io fop, it acquires a local network
   buffer and calls a m0_rpc_bulk apis to start the zero-copy.
   So, io fop typically carries the net buf descriptor and bulk server asks
   the transfer machine belonging to rpc layer to start zero copy of
   data buffers.

   <hr>
   @section bulkclient-lspec Logical Specification

   - @ref bulkclient-lspec-comps
   - @ref bulkclient-lspec-sc1
      - @ref bulkclient-lspec-ds1
      - @ref bulkclient-lspec-sub1
      - @ref bulkclientDFSInternal
   - @ref bulkclient-lspec-state
   - @ref bulkclient-lspec-thread
   - @ref bulkclient-lspec-numa


   @subsection bulkclient-lspec-comps Component Overview

   The following @@dot diagram shows the interaction of bulk client
   program with rpc layer and net layer.
   @dot
   digraph {
     node [style=box];
     label = "IO bulk client interaction with rpc and net layer";
     io_bulk_client [label = "IO bulk client"];
     Rpc_bulk [label = "RPC bulk abstraction"];
     IO_fop [label = "IO fop"];
     nwlayer [label = "Network layer"];
     zerovec [label = "Zero vector"];
     io_bulk_client -> IO_fop;
     IO_fop -> Rpc_bulk;
     IO_fop -> zerovec;
     Rpc_bulk -> zerovec;
     Rpc_bulk -> nwlayer;
   }
   @enddot

   @subsection bulkclient-lspec-sc1 Subcomponent design

   Ioservice subsystem primarily comprises of 2 sub-components
   - IO client (comprises of IO coalescing code)
   - IO server (server part of io routines)

   The IO client subsystem under which IO requests belonging to same fid
   and intent (read/write) are clubbed together in one fop and this resultant
   fop is sent instead of member io fops.

   @subsubsection bulkclient-lspec-ds1 Subcomponent Data Structures

   The IO coalescing subsystem from ioservice primarily works on IO segments.
   IO segment is in-memory structure that represents a contiguous chunk of
   IO data along with extent information.
   An internal data structure ioseg represents the IO segment.
   - ioseg An in-memory structure used to represent a segment of IO data.

   @subsubsection bulkclient-lspec-sub1 Subcomponent Subroutines

   - ioseg_get() - Retrieves an ioseg given its index in zero vector.

   @subsection bulkclient-lspec-state State Specification

   @dot
   digraph bulk_io_client_states {
	size = "5,6"
	label = "States encountered during io from bulk client"
	node [shape = record, fontname=Helvetica, fontsize=12]
	S0 [label = "", shape="plaintext", layer=""]
	S1 [label = "IO fop initialized"]
	S2 [label = "Rpc bulk structure initialized"]
	S3 [label = "Pages added to rpc bulk structure"]
	S4 [label = "Net buf desc stored in io fop wire format."]
	S5 [label = "Rpc item posted to rpc layer"]
	S6 [label = "Client waiting for reply"]
	S7 [label = "Reply received"]
	S8 [label = "Terminate"]
	S0 -> S1 [label = "Allocate"]
	S1 -> S2 [label = "m0_rpc_bulk_init()"]
	S1 -> S8 [label = "Failed"]
	S2 -> S8 [label = "Failed"]
	S2 -> S3 [label = "m0_rpc_bulk_buf_page_add()"]
	S3 -> S8 [label = "Failed"]
	S3 -> S4 [label = "m0_rpc_bulk_store()"]
	S4 -> S5 [label = "m0_rpc_post()"]
	S5 -> S6 [label = "m0_chan_wait(item->ri_chan)"]
	S6 -> S7 [label = "m0_chan_signal(item->ri_chan)"]
	S7 -> S8 [label = "m0_rpc_bulk_fini(rpc_bulk)"]
   }
   @enddot

   @subsection bulkclient-lspec-thread Threading and Concurrency Model

   No need of explicit locking for structures like m0_io_fop and ioseg
   since they are taken care by locking at upper layers like locking at
   the m0t1fs part for dispatching IO requests.

   @subsection bulkclient-lspec-numa NUMA optimizations

   The performance need not be optimized by associating the incoming thread
   to a particular processor. However, keeping in sync with the design of
   request handler which tries to protect the locality of threads executing
   in a particular context by establishing affinity to some designated
   processor, this can be achieved. But this is still at a level higher than
   the io fop processing.

   <hr>
   @section bulkclient-conformance Conformance

   - I.bulkclient.rpcbulk The bulk client uses rpc bulk APIs to enqueue
   kernel pages to the network buffer.
   - I.bulkclient.fopcreation bulk client creates new io fops until all
   kernel pages are enqueued.
   - I.bulkclient.netbufdesc The on-wire definition of io_fop contains a
   net buffer descriptor. @see m0_net_buf_desc
   - I.bulkclient.iocoalescing Since all IO coalescing code is built around
   the definition of IO fop, it will conform to new format of io fop.

   <hr>
   @section bulkclient-ut Unit Tests

   All external interfaces based on m0_io_fop and m0_rpc_bulk will be
   unit tested. All unit tests will stress success and failure conditions.
   Boundary condition testing is also included.
   - The m0_io_fop* and m0_rpc_bulk* interfaces will be unit tested
   first in the order
	- m0_io_fop_init Check if the inline m0_fop and m0_rpc_bulk are
	initialized properly.
	- m0_rpc_bulk_page_add/m0_rpc_bulk_buffer_add to add pages/buffers
	to the rpc_bulk structure and cross check if they are actually added
	or not.
	- Add more pages/buffers to rpc_bulk structure to check if they
	return proper error code.
	- Try m0_io_fop_fini to check if an initialized m0_io_fop and
	the inline m0_rpc_bulk get properly finalized.
	- Initialize and start a network transport and a transfer machine.
	Invoke m0_rpc_bulk_store on rpc_bulk structure and cross check if
	the net buffer descriptor is properly populated in the io fop.
	- Tweak the parameters of transfer machine so that it goes into
	degraded/failed state and invoke m0_rpc_bulk_store and check if
	m0_rpc_bulk_store returns proper error code.
	- Start another transfer machine and invoke m0_rpc_bulk_load to
	check if it recognizes the net buf descriptor and starts buffer
	transfer properly.
	- Tweak the parameters of second transfer machine so that it goes
	into degraded/failed state and invoke m0_rpc_bulk_load and check if
	it returns proper error code.

   <hr>
   @section bulkclient-st System Tests

   Not applicable.

   <hr>
   @section bulkclient-O Analysis

   - m denotes the number of IO fops with same fid and intent (read/write).
   - n denotes the total number of IO segments in m IO fops.
   - Memory consumption O(n) During IO coalescing, n number of IO segments
   are allocated and subsequently deallocated, once the resulting IO fop
   is created.
   - Processor cycles O(n) During IO coalescing, all n segments are traversed
   and resultant IO fop is created.
   - Locks Minimal locks since locking is mostly taken care by upper layers.
   - Messages Not applicable.

   <hr>
   @section bulkclient-ref References

   - <a href="https://docs.google.com/a/seagate.com/document/d/1pDOQXWDZ9t9XDcyX
sx4T_aGjFvsyjjvN1ygOtfoXcFg/edit?hl=en_US">RPC Bulk Transfer Task Plan</a>
   - <a href="https://docs.google.com/a/xyratex.com/
Doc?docid=0ATg1HFjUZcaZZGNkNXg4cXpfMjQ3Z3NraDI4ZG0&hl=en_US">
Detailed level design HOWTO</a>,
   an older document on which this style guide is partially based.

 */

/**
   @defgroup bulkclientDFSInternal IO bulk client Detailed Function Spec
   @brief Detailed Function Specification for IO bulk client.

   @{
 */

/**
 * Generic io segment that represents a contiguous stream of bytes
 * along with io extent. This structure is typically used by io coalescing
 * code from ioservice.
 */
struct ioseg {
	/* Magic constant to verify sanity of structure. */
	uint64_t		 is_magic;
	/* Index in target object to start io from. */
	m0_bindex_t		 is_index;
	/* Number of bytes in io segment. */
	m0_bcount_t		 is_size;
	/* Starting address of buffer. */
	void			*is_buf;
	/*
	 * Linkage to have such IO segments in a list hanging off
	 * io_seg_set::iss_list.
	 */
	struct m0_tlink		 is_linkage;
};

/** Represents coalesced set of io segments. */
struct io_seg_set {
	/** Magic constant to verify sanity of structure. */
	uint64_t	iss_magic;
	/** List of struct ioseg. */
	struct m0_tl	iss_list;
};

M0_TL_DESCR_DEFINE(iosegset, "list of coalesced io segments", static,
		   struct ioseg, is_linkage, is_magic,
		   M0_IOS_IO_SEGMENT_MAGIC, M0_IOS_IO_SEGMENT_SET_MAGIC);

M0_TL_DEFINE(iosegset, static, struct ioseg);

static void ioseg_get(const struct m0_0vec *zvec, uint32_t seg_index,
		      struct ioseg *seg)
{
	M0_PRE(zvec != NULL);
	M0_PRE(seg_index < zvec->z_bvec.ov_vec.v_nr);
	M0_PRE(seg != NULL);

	seg->is_index = zvec->z_index[seg_index];
	seg->is_size = zvec->z_bvec.ov_vec.v_count[seg_index];
	seg->is_buf = zvec->z_bvec.ov_buf[seg_index];
}

static bool io_fop_invariant(struct m0_io_fop *iofop)
{
	return  _0C(iofop != NULL) &&
		_0C(iofop->if_magic == M0_IO_FOP_MAGIC) &&
		_0C(m0_exists(i, ARRAY_SIZE(ioservice_fops),
			      iofop->if_fop.f_type == ioservice_fops[i]));
}

M0_INTERNAL int m0_io_fop_init(struct m0_io_fop *iofop,
			       const struct m0_fid *gfid,
			       struct m0_fop_type *ftype,
			       void (*fop_release)(struct m0_ref *))
{
	int                   rc;
	struct m0_fop_cob_rw *rw;

	M0_PRE(iofop != NULL);
	M0_PRE(ftype != NULL);
	M0_PRE(gfid  != NULL);

	M0_LOG(M0_DEBUG, "iofop %p", iofop);

	m0_fop_init(&iofop->if_fop, ftype, NULL,
		    fop_release ?: m0_io_fop_release);
	rc = m0_fop_data_alloc(&iofop->if_fop);
	if (rc == 0) {
		iofop->if_fop.f_item.ri_ops = &io_req_rpc_item_ops;
		iofop->if_magic = M0_IO_FOP_MAGIC;

		m0_rpc_bulk_init(&iofop->if_rbulk);
		rw = io_rw_get(&iofop->if_fop);
		rw->crw_gfid = *gfid;
		if (ftype == &m0_fop_cob_writev_fopt)
			rw->crw_flags |= M0_IO_FLAG_CROW;

		M0_POST(io_fop_invariant(iofop));
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_io_fop_fini(struct m0_io_fop *iofop)
{
	M0_PRE(io_fop_invariant(iofop));
	m0_rpc_bulk_fini(&iofop->if_rbulk);
	m0_fop_fini(&iofop->if_fop);
}

M0_INTERNAL struct m0_rpc_bulk *m0_fop_to_rpcbulk(const struct m0_fop *fop)
{
	struct m0_io_fop *iofop;

	M0_PRE(fop != NULL);

	iofop = container_of(fop, struct m0_io_fop, if_fop);
	return &iofop->if_rbulk;
}

/** @} end of bulkclientDFSInternal */

M0_INTERNAL bool m0_is_read_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_cob_readv_fopt;
}

M0_INTERNAL bool m0_is_write_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_cob_writev_fopt;
}

M0_INTERNAL bool m0_is_io_fop(const struct m0_fop *fop)
{
	return m0_is_read_fop(fop) || m0_is_write_fop(fop);
}

static bool is_read_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_cob_readv_rep_fopt;
}

static bool is_write_rep(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type == &m0_fop_cob_writev_rep_fopt;
}

M0_INTERNAL bool m0_is_io_fop_rep(const struct m0_fop *fop)
{
	return is_read_rep(fop) || is_write_rep(fop);
}

M0_INTERNAL bool m0_is_cob_create_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type->ft_rpc_item_type.rit_opcode ==
				M0_IOSERVICE_COB_CREATE_OPCODE;
}

M0_INTERNAL bool m0_is_cob_delete_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type->ft_rpc_item_type.rit_opcode ==
				M0_IOSERVICE_COB_DELETE_OPCODE;
}

M0_INTERNAL bool m0_is_cob_truncate_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type->ft_rpc_item_type.rit_opcode ==
				M0_IOSERVICE_COB_TRUNCATE_OPCODE;
}

M0_INTERNAL bool m0_is_cob_getattr_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type->ft_rpc_item_type.rit_opcode ==
				M0_IOSERVICE_COB_GETATTR_OPCODE;
}

M0_INTERNAL bool m0_is_cob_setattr_fop(const struct m0_fop *fop)
{
	M0_PRE(fop != NULL);
	return fop->f_type->ft_rpc_item_type.rit_opcode ==
				M0_IOSERVICE_COB_SETATTR_OPCODE;
}

M0_INTERNAL bool m0_is_cob_create_delete_fop(const struct m0_fop *fop)
{
	return m0_is_cob_create_fop(fop) || m0_is_cob_delete_fop(fop);
}

M0_INTERNAL struct m0_fop_cob_common *m0_cobfop_common_get(struct m0_fop *fop)
{
	struct m0_fop_cob_create   *cc;
	struct m0_fop_cob_delete   *cd;
	struct m0_fop_cob_truncate *ct;
	struct m0_fop_cob_getattr  *cg;
	struct m0_fop_cob_setattr  *cs;

	M0_PRE(fop != NULL);
	M0_PRE(fop->f_type != NULL);

	if (m0_is_cob_create_fop(fop)) {
		cc = m0_fop_data(fop);
		return &cc->cc_common;
	} else if (m0_is_cob_delete_fop(fop)) {
		cd = m0_fop_data(fop);
		return &cd->cd_common;
	} else if (fop->f_type == &m0_fop_cob_truncate_fopt) {
		ct = m0_fop_data(fop);
		return &ct->ct_common;
	} else if (m0_is_cob_getattr_fop(fop)) {
		cg = m0_fop_data(fop);
		return &cg->cg_common;
	} else if (m0_is_cob_setattr_fop(fop)) {
		cs = m0_fop_data(fop);
		return &cs->cs_common;
	} else
		M0_IMPOSSIBLE("Invalid fop type!");
}

M0_INTERNAL uint32_t m0_io_fop_segs_nr(struct m0_fop *fop, uint32_t index)
{
        struct m0_fop_cob_rw *rwfop;
	m0_bcount_t	      used_size;
	uint32_t	      segs_nr;
	m0_bcount_t           max_seg_size;

	rwfop = io_rw_get(fop);
	used_size = rwfop->crw_desc.id_descs[index].bdd_used;
	max_seg_size = m0_net_domain_get_max_buffer_segment_size(
				m0_fop_domain_get(fop));
	segs_nr = used_size / max_seg_size;
	M0_LOG(M0_DEBUG, "segs_nr %d", segs_nr);

	return segs_nr;
}

M0_INTERNAL struct m0_fop_cob_rw *io_rw_get(struct m0_fop *fop)
{
	struct m0_fop_cob_readv  *rfop;
	struct m0_fop_cob_writev *wfop;

	M0_PRE(fop != NULL);
	M0_ASSERT_INFO(m0_is_io_fop(fop), "%s %i %i",
		       fop->f_type != NULL ? fop->f_type->ft_name : "untyped",
		       fop->f_item.ri_error, fop->f_item.ri_type->rit_opcode);

	if (m0_is_read_fop(fop)) {
		rfop = m0_fop_data(fop);
		return &rfop->c_rwv;
	} else {
		wfop = m0_fop_data(fop);
		return &wfop->c_rwv;
	}
}

M0_INTERNAL struct m0_fop_cob_rw_reply *io_rw_rep_get(struct m0_fop *fop)
{
	struct m0_fop_cob_readv_rep	*rfop;
	struct m0_fop_cob_writev_rep	*wfop;

	M0_PRE(fop != NULL);
	M0_PRE(m0_is_io_fop_rep(fop));

	if (is_read_rep(fop)) {
		rfop = m0_fop_data(fop);
		return &rfop->c_rep;
	} else {
		wfop = m0_fop_data(fop);
		return &wfop->c_rep;
	}
}

static struct m0_0vec *io_0vec_get(struct m0_rpc_bulk_buf *rbuf)
{
	M0_PRE(rbuf != NULL);

	return &rbuf->bb_zerovec;
}

static void ioseg_unlink_free(struct ioseg *ioseg)
{
	M0_PRE(ioseg != NULL);
	M0_PRE(iosegset_tlink_is_in(ioseg));

	iosegset_tlist_del(ioseg);
	m0_free(ioseg);
}

/**
   Returns if given 2 fops belong to same type.
 */
__attribute__((unused))
static bool io_fop_type_equal(const struct m0_fop *fop1,
			      const struct m0_fop *fop2)
{
	M0_PRE(fop1 != NULL);
	M0_PRE(fop2 != NULL);

	return fop1->f_type == fop2->f_type;
}

static int io_fop_seg_init(struct ioseg **ns, const struct ioseg *cseg)
{
	struct ioseg *new_seg = 0;

	M0_PRE(ns != NULL);
	M0_PRE(cseg != NULL);

	M0_ALLOC_PTR(new_seg);
	if (new_seg == NULL)
		return M0_ERR(-ENOMEM);

	*ns = new_seg;
	M0_ASSERT(new_seg != NULL); /* suppress compiler warning on next stmt */
	*new_seg = *cseg;
	iosegset_tlink_init(new_seg);
	return 0;
}

static int io_fop_seg_add_cond(struct ioseg *cseg, const struct ioseg *nseg)
{
	int           rc;
	struct ioseg *new_seg;

	M0_PRE(cseg != NULL);
	M0_PRE(nseg != NULL);

	if (nseg->is_index < cseg->is_index) {
		rc = io_fop_seg_init(&new_seg, nseg);
		if (rc != 0)
			return M0_RC(rc);

		iosegset_tlist_add_before(cseg, new_seg);
	} else
		rc = -EINVAL;

	return M0_RC(rc);
}

static void io_fop_seg_coalesce(const struct ioseg *seg,
				struct io_seg_set *aggr_set)
{
	int           rc;
	struct ioseg *new_seg;
	struct ioseg *ioseg;

	M0_PRE(seg != NULL);
	M0_PRE(aggr_set != NULL);

	/*
	 * Coalesces all io segments in increasing order of offset.
	 * This will create new net buffer/s which will be associated with
	 * only one io fop and it will be sent on wire. While rest of io fops
	 * will hang off a list m0_rpc_item::ri_compound_items.
	 */
	m0_tl_for(iosegset, &aggr_set->iss_list, ioseg) {
		rc = io_fop_seg_add_cond(ioseg, seg);
		if (rc == 0 || rc == -ENOMEM)
			return;
	} m0_tl_endfor;

	rc = io_fop_seg_init(&new_seg, seg);
	if (rc != 0)
		return;
	iosegset_tlist_add_tail(&aggr_set->iss_list, new_seg);
}

static void io_fop_segments_coalesce(const struct m0_0vec *iovec,
				     struct io_seg_set *aggr_set)
{
	uint32_t     i;
	struct ioseg seg = { 0 };

	M0_PRE(iovec != NULL);
	M0_PRE(aggr_set != NULL);

	/*
	 * For each segment from incoming IO vector, check if it can
	 * be merged with any of the existing segments from aggr_set.
	 * If yes, merge it else, add a new entry in aggr_set.
	 */
	for (i = 0; i < iovec->z_bvec.ov_vec.v_nr; ++i) {
		ioseg_get(iovec, i, &seg);
		io_fop_seg_coalesce(&seg, aggr_set);
	}
}

/*
 * Creates and populates net buffers as needed using the list of
 * coalesced io segments.
 */
static int io_netbufs_prepare(struct m0_fop *coalesced_fop,
			      struct io_seg_set *seg_set)
{
	int			 rc;
	int32_t			 max_segs_nr;
	int32_t			 curr_segs_nr;
	int32_t			 nr;
	m0_bcount_t		 max_bufsize;
	m0_bcount_t		 curr_bufsize;
	uint32_t		 segs_nr;
	struct ioseg		*ioseg;
	struct m0_net_domain	*netdom;
	struct m0_rpc_bulk	*rbulk;
	struct m0_rpc_bulk_buf	*buf;

	M0_PRE(coalesced_fop != NULL);
	M0_PRE(seg_set != NULL);
	M0_PRE(!iosegset_tlist_is_empty(&seg_set->iss_list));

	netdom = m0_fop_domain_get(coalesced_fop);
	max_bufsize = m0_net_domain_get_max_buffer_size(netdom);
	max_segs_nr = m0_net_domain_get_max_buffer_segments(netdom);
	rbulk = m0_fop_to_rpcbulk(coalesced_fop);
	curr_segs_nr = iosegset_tlist_length(&seg_set->iss_list);

	while (curr_segs_nr != 0) {
		curr_bufsize = 0;
		segs_nr = 0;
		/*
		 * Calculates the number of segments that can fit into max
		 * buffer size. These are needed to add a m0_rpc_bulk_buf
		 * structure into struct m0_rpc_bulk. Selected io segments
		 * are removed from io segments list, hence the loop always
		 * starts from the first element.
		 */
		m0_tl_for(iosegset, &seg_set->iss_list, ioseg) {
			if (curr_bufsize + ioseg->is_size <= max_bufsize &&
			    segs_nr <= max_segs_nr) {
				curr_bufsize += ioseg->is_size;
				++segs_nr;
			} else
				break;
		} m0_tl_endfor;

		rc = m0_rpc_bulk_buf_add(rbulk, segs_nr, curr_bufsize,
					 netdom, NULL, &buf);
		if (rc != 0)
			goto cleanup;

		nr = 0;
		m0_tl_for(iosegset, &seg_set->iss_list, ioseg) {
			rc = m0_rpc_bulk_buf_databuf_add(buf, ioseg->is_buf,
							 ioseg->is_size,
							 ioseg->is_index,
							 netdom);

			/*
			 * Since size and fragment calculations are made before
			 * hand, this buffer addition should succeed.
			 */
			M0_ASSERT(rc == 0);

			ioseg_unlink_free(ioseg);
			if (++nr == segs_nr)
				break;
		} m0_tl_endfor;
		M0_POST(m0_vec_count(&buf->bb_zerovec.z_bvec.ov_vec) <=
			max_bufsize);
		M0_POST(buf->bb_zerovec.z_bvec.ov_vec.v_nr <= max_segs_nr);
		curr_segs_nr -= segs_nr;
	}
	return 0;
cleanup:
	M0_ASSERT(rc != 0);
	m0_rpc_bulk_buflist_empty(rbulk);
	return M0_RC(rc);
}

/* Deallocates memory claimed by index vector/s from io fop wire format. */
M0_INTERNAL void io_fop_ivec_dealloc(struct m0_fop *fop)
{
	struct m0_fop_cob_rw  *rw;
	struct m0_io_indexvec *ivec;

	M0_PRE(fop != NULL);

	rw   = io_rw_get(fop);
	ivec = &rw->crw_ivec;

	m0_free(ivec->ci_iosegs);
	ivec->ci_nr     = 0;
	ivec->ci_iosegs = NULL;
}

#define ZNR(zvec)       zvec->z_bvec.ov_vec.v_nr
#define ZCOUNT(zvec, i) zvec->z_bvec.ov_vec.v_count[i]
#define ZINDEX(zvec, i) zvec->z_index[i]

#define INR(ivec)       ivec->ci_nr
#define IINDEX(ivec, i) ivec->ci_iosegs[i].ci_index
#define ICOUNT(ivec, i) ivec->ci_iosegs[i].ci_count

static uint32_t iosegs_nr(struct m0_rpc_bulk *rbulk)
{
	struct m0_rpc_bulk_buf *buf;
	uint32_t                cnt   = 0;
	m0_bindex_t             index = 0;
	m0_bcount_t             count = 0;

	m0_tl_for(rpcbulk, &rbulk->rb_buflist, buf) {
		uint32_t        i  = 0;
		uint32_t        nr = 0;
		struct m0_0vec *zvec = &buf->bb_zerovec;

		if (index == 0) {
			index = ZINDEX(zvec, 0);
			count = ZCOUNT(zvec, 0);
			i     = 1;
		}
		for (; i < ZNR(zvec); ++i) {
			if (index + count == ZINDEX(zvec, i))
				++nr;
			index = ZINDEX(zvec, i);
			count = ZCOUNT(zvec, i);
		}
		cnt += ZNR(zvec) - nr;
	} m0_tl_endfor;

	return cnt;
}

static void iosegs_squeeze(struct m0_rpc_bulk    *rbulk,
			   struct m0_io_indexvec *ivec)
{
	m0_bindex_t             index = 0;
	struct m0_rpc_bulk_buf *buf;

	m0_tl_for(rpcbulk, &rbulk->rb_buflist, buf) {
		uint32_t        j    = 0;
		struct m0_0vec *zvec = &buf->bb_zerovec;

		if (IINDEX(ivec, 0) == 0 && ICOUNT(ivec, 0) == 0) {
			IINDEX(ivec, 0) = ZINDEX(zvec, 0);
			ICOUNT(ivec, 0) = ZCOUNT(zvec, 0);
			j = 1;
		}
		for (; j < ZNR(zvec); ++j) {
			if (IINDEX(ivec, index) + ICOUNT(ivec, index) ==
			    ZINDEX(zvec, j)) {
				ICOUNT(ivec, index) += ZCOUNT(zvec, j);
			} else {
				++index;
				IINDEX(ivec, index) = ZINDEX(zvec, j);
				ICOUNT(ivec, index) = ZCOUNT(zvec, j);
			}
		}
	} m0_tl_endfor;
}

/* Populates index vector/s from io fop wire format. */
static int io_fop_ivec_prepare(struct m0_fop      *res_fop,
			       struct m0_rpc_bulk *rbulk)
{
	struct m0_fop_cob_rw  *rw;
	struct m0_io_indexvec *ivec;

	M0_PRE(res_fop != NULL);
	M0_PRE(rbulk != NULL);
	M0_PRE(m0_mutex_is_locked(&rbulk->rb_mutex));

	rw          = io_rw_get(res_fop);
	ivec        = &rw->crw_ivec;
	ivec->ci_nr = iosegs_nr(rbulk);

	M0_ALLOC_ARR(ivec->ci_iosegs, ivec->ci_nr);
	if (ivec->ci_iosegs == NULL)
		return M0_ERR(-ENOMEM);

	iosegs_squeeze(rbulk, ivec);

	return 0;
}

static int io_fop_di_prepare(struct m0_fop *fop)
{
	uint64_t		   size;
	struct m0_fop_cob_rw	  *rw;
	struct m0_io_indexvec	  *io_info;
	struct m0_bufvec	   cksum_data;
	struct m0_rpc_bulk	  *rbulk;
	struct m0_rpc_bulk_buf	  *rbuf;
	struct m0_file		  *file;
	uint64_t		   curr_size = 0;
	uint64_t		   todo = 0;
	int			   rc = 0;
	struct m0_indexvec	   io_vec;

	if (M0_FI_ENABLED("skip_di_for_ut"))
		return 0;
#ifndef ENABLE_DATA_INTEGRITY
	return M0_RC(rc);
#endif
	M0_PRE(fop != NULL);

	rbulk = m0_fop_to_rpcbulk(fop);
	M0_ASSERT(rbulk != NULL);
	M0_ASSERT(m0_mutex_is_locked(&rbulk->rb_mutex));
	rw      = io_rw_get(fop);
	io_info = &rw->crw_ivec;
#ifndef __KERNEL__
	file    = m0_clovis_fop_to_file(fop);
#else
	file    = m0_fop_to_file(fop);
#endif
	if (file->fi_di_ops->do_out_shift(file) == 0)
		return M0_RC(rc);
	rc = m0_indexvec_wire2mem(io_info, io_info->ci_nr, 0, &io_vec);
	if (rc != 0)
		return M0_RC(rc);
	size = m0_di_size_get(file, m0_io_count(io_info));
	rw->crw_di_data.b_nob = size;
	rw->crw_di_data.b_addr = m0_alloc(size);
	if (rw->crw_di_data.b_addr == NULL) {
		rc = -ENOMEM;
		goto out;
	}
	m0_tl_for (rpcbulk, &rbulk->rb_buflist, rbuf) {
		struct m0_indexvec ivec;
		uint32_t	   di_size;
		struct m0_buf	   buf;
		uint32_t	   curr_pos;

		curr_pos = m0_di_size_get(file, curr_size);
		todo = m0_vec_count(&rbuf->bb_zerovec.z_bvec.ov_vec);
		di_size = m0_di_size_get(file, todo);
		buf = M0_BUF_INIT(di_size, rw->crw_di_data.b_addr + curr_pos);
		cksum_data = (struct m0_bufvec) M0_BUFVEC_INIT_BUF(&buf.b_addr,
								   &buf.b_nob);
		rc = m0_indexvec_split(&io_vec, curr_size, todo, 0, &ivec);
		if (rc != 0)
			goto out;
		file->fi_di_ops->do_sum(file, &ivec, &rbuf->bb_nbuf->nb_buffer,
				        &cksum_data);
		curr_size += todo;
		m0_indexvec_free(&ivec);
	} m0_tl_endfor;

out:
	m0_indexvec_free(&io_vec);
	return M0_RC(rc);
}

static void io_fop_bulkbuf_move(struct m0_fop *src, struct m0_fop *dest)
{
	struct m0_rpc_bulk	*sbulk;
	struct m0_rpc_bulk	*dbulk;
	struct m0_rpc_bulk_buf	*rbuf;
	struct m0_fop_cob_rw	*srw;
	struct m0_fop_cob_rw	*drw;

	M0_PRE(src != NULL);
	M0_PRE(dest != NULL);

	sbulk = m0_fop_to_rpcbulk(src);
	dbulk = m0_fop_to_rpcbulk(dest);
	m0_mutex_lock(&sbulk->rb_mutex);
	m0_tl_teardown(rpcbulk, &sbulk->rb_buflist, rbuf) {
		rpcbulk_tlist_add(&dbulk->rb_buflist, rbuf);
	}
	dbulk->rb_bytes = sbulk->rb_bytes;
	dbulk->rb_rc = sbulk->rb_rc;
	m0_mutex_unlock(&sbulk->rb_mutex);

	srw = io_rw_get(src);
	drw = io_rw_get(dest);
	drw->crw_desc = srw->crw_desc;
	drw->crw_ivec = srw->crw_ivec;
}

static int io_fop_desc_alloc(struct m0_fop *fop, struct m0_rpc_bulk *rbulk)
{
	struct m0_fop_cob_rw	*rw;

	M0_PRE(fop != NULL);
	M0_PRE(rbulk != NULL);
	M0_PRE(m0_mutex_is_locked(&rbulk->rb_mutex));

	rbulk = m0_fop_to_rpcbulk(fop);
	rw = io_rw_get(fop);
	rw->crw_desc.id_nr = rpcbulk_tlist_length(&rbulk->rb_buflist);
	M0_ALLOC_ARR(rw->crw_desc.id_descs, rw->crw_desc.id_nr);
	return rw->crw_desc.id_descs == NULL ? M0_ERR(-ENOMEM) : 0;
}

static void io_fop_desc_dealloc(struct m0_fop *fop)
{
	uint32_t                 i;
	struct m0_fop_cob_rw	*rw;

	M0_PRE(fop != NULL);

	rw = io_rw_get(fop);

	/*
	 * These descriptors are allocated by m0_rpc_bulk_store()
	 * code during adding them as part of on-wire representation
	 * of io fop. They should not be deallocated by rpc code
	 * since it will unnecessarily pollute rpc layer code
	 * with io details.
	 */
	for (i = 0; i < rw->crw_desc.id_nr; ++i)
		m0_net_desc_free(&rw->crw_desc.id_descs[i].bdd_desc);

	m0_free0(&rw->crw_desc.id_descs);
	rw->crw_desc.id_nr = 0;
}

/*
 * Allocates memory for net buf descriptors array and index vector array
 * and populates the array of index vectors in io fop wire format.
 */
M0_INTERNAL int m0_io_fop_prepare(struct m0_fop *fop)
{
	int		       rc;
	struct m0_rpc_bulk    *rbulk;
	enum m0_net_queue_type q;
	M0_ENTRY();

	M0_PRE(fop != NULL);
	M0_PRE(m0_is_io_fop(fop));

	rbulk = m0_fop_to_rpcbulk(fop);
	m0_mutex_lock(&rbulk->rb_mutex);
	rc = io_fop_desc_alloc(fop, rbulk);
	if (rc != 0) {
		rc = -ENOMEM;
		goto err;
	}

	rc = io_fop_ivec_prepare(fop, rbulk);
	if (rc != 0) {
		io_fop_desc_dealloc(fop);
		rc = -ENOMEM;
		goto err;
	}

	q = m0_is_read_fop(fop) ? M0_NET_QT_PASSIVE_BULK_RECV :
			   M0_NET_QT_PASSIVE_BULK_SEND;
	m0_rpc_bulk_qtype(rbulk, q);
	if (rc == 0 && m0_is_write_fop(fop))
		rc = io_fop_di_prepare(fop);
err:
	m0_mutex_unlock(&rbulk->rb_mutex);
	return M0_RC(rc);
}

/*
 * Creates new net buffers from aggregate list and adds them to
 * associated m0_rpc_bulk object. Also calls m0_io_fop_prepare() to
 * allocate memory for net buf desc sequence and index vector
 * sequence in io fop wire format.
 */
static int io_fop_desc_ivec_prepare(struct m0_fop *fop,
				    struct io_seg_set *aggr_set)
{
	int			rc;
	struct m0_rpc_bulk     *rbulk;

	M0_PRE(fop != NULL);
	M0_PRE(aggr_set != NULL);

	rbulk = m0_fop_to_rpcbulk(fop);

	rc = io_netbufs_prepare(fop, aggr_set);
	if (rc != 0) {
		return M0_RC(rc);
	}

	rc = m0_io_fop_prepare(fop);
	if (rc != 0)
		m0_rpc_bulk_buflist_empty(rbulk);

	return M0_RC(rc);
}

/*
 * Deallocates memory for sequence of net buf desc and sequence of index
 * vectors from io fop wire format.
 */
M0_INTERNAL void m0_io_fop_destroy(struct m0_fop *fop)
{
	M0_PRE(fop != NULL);

	io_fop_desc_dealloc(fop);
	io_fop_ivec_dealloc(fop);
}

M0_INTERNAL size_t m0_io_fop_size_get(struct m0_fop *fop)
{
	struct m0_xcode_ctx  ctx;

	M0_PRE(fop != NULL);
	M0_PRE(fop->f_type != NULL);

	m0_xcode_ctx_init(&ctx, &M0_FOP_XCODE_OBJ(fop));
	return m0_xcode_length(&ctx);
}

/**
 * Coalesces the io fops with same fid and intent (read/write). A list of
 * coalesced io segments is generated which is attached to a single
 * io fop - res_fop (which is already bound to a session) in form of
 * one of more network buffers and rest of the io fops hang off a list
 * m0_rpc_item::ri_compound_items in resultant fop.
 * The index vector array from io fop is also populated from the list of
 * coalesced io segments.
 * The res_fop contents are backed up and restored on receiving reply
 * so that upper layer is transparent of these operations.
 * @see item_io_coalesce().
 * @see m0_io_fop_init().
 * @see m0_rpc_bulk_init().
 */
static int io_fop_coalesce(struct m0_fop *res_fop, uint64_t size)
{
	int			   rc;
	struct m0_fop		  *fop;
	struct m0_fop		  *bkp_fop;
	struct m0_tl		  *items_list;
	struct m0_0vec		  *iovec;
	struct ioseg		  *ioseg;
	struct m0_io_fop	  *cfop;
	struct io_seg_set	   aggr_set;
	struct m0_rpc_item	  *item;
	struct m0_rpc_bulk	  *rbulk;
	struct m0_rpc_bulk	  *bbulk;
	struct m0_fop_cob_rw	  *rw;
	struct m0_rpc_bulk_buf    *rbuf;
	struct m0_net_transfer_mc *tm;

	M0_PRE(res_fop != NULL);
	M0_PRE(m0_is_io_fop(res_fop));

	M0_ALLOC_PTR(cfop);
	if (cfop == NULL)
		return M0_ERR(-ENOMEM);

	rw = io_rw_get(res_fop);
	rc = m0_io_fop_init(cfop, &rw->crw_gfid, res_fop->f_type, NULL);
	if (rc != 0) {
		m0_free(cfop);
		return M0_RC(rc);
	}
	tm = m0_fop_tm_get(res_fop);
	bkp_fop = &cfop->if_fop;
	aggr_set.iss_magic = M0_IOS_IO_SEGMENT_SET_MAGIC;
	iosegset_tlist_init(&aggr_set.iss_list);

	/*
	 * Traverses the fop_list, get the IO vector from each fop,
	 * pass it to a coalescing routine and get result back
	 * in another list.
	 */
	items_list = &res_fop->f_item.ri_compound_items;
	M0_ASSERT(!rpcitem_tlist_is_empty(items_list));

	m0_tl_for(rpcitem, items_list, item) {
		fop = m0_rpc_item_to_fop(item);
		rbulk = m0_fop_to_rpcbulk(fop);
		m0_mutex_lock(&rbulk->rb_mutex);
		m0_tl_for(rpcbulk, &rbulk->rb_buflist, rbuf) {
			iovec = io_0vec_get(rbuf);
			io_fop_segments_coalesce(iovec, &aggr_set);
		} m0_tl_endfor;
		m0_mutex_unlock(&rbulk->rb_mutex);
	} m0_tl_endfor;

	/*
	 * Removes m0_rpc_bulk_buf from the m0_rpc_bulk::rb_buflist and
	 * add it to same list belonging to bkp_fop.
	 */
	io_fop_bulkbuf_move(res_fop, bkp_fop);

	/*
	 * Prepares net buffers from set of io segments, allocates memory
	 * for net buf desriptors and index vectors and populates the index
	 * vectors
	 */
	rc = io_fop_desc_ivec_prepare(res_fop, &aggr_set);
	if (rc != 0)
		goto cleanup;

	/*
	 * Adds the net buffers from res_fop to transfer machine and
	 * populates res_fop with net buf descriptor/s got from network
	 * buffer addition.
	 */
	rw = io_rw_get(res_fop);
	rbulk = m0_fop_to_rpcbulk(res_fop);
	rc = m0_rpc_bulk_store(rbulk, res_fop->f_item.ri_session->s_conn,
			       rw->crw_desc.id_descs, &m0_rpc__buf_bulk_cb);
	if (rc != 0) {
		m0_io_fop_destroy(res_fop);
		goto cleanup;
	}

	/*
	 * Checks if current size of res_fop fits into the size
	 * provided as input.
	 */
	if (m0_io_fop_size_get(res_fop) > size) {
		m0_mutex_lock(&rbulk->rb_mutex);
		m0_tl_for(rpcbulk, &rbulk->rb_buflist, rbuf) {
			m0_net_buffer_del(rbuf->bb_nbuf, tm);
		} m0_tl_endfor;
		m0_mutex_unlock(&rbulk->rb_mutex);
		m0_io_fop_destroy(res_fop);
		goto cleanup;
	}

	/*
	 * Removes the net buffers belonging to coalesced member fops
	 * from transfer machine since these buffers are coalesced now
	 * and are part of res_fop.
	 */
	m0_tl_for(rpcitem, items_list, item) {
		fop = m0_rpc_item_to_fop(item);
		if (fop == res_fop)
			continue;
		rbulk = m0_fop_to_rpcbulk(fop);
		m0_mutex_lock(&rbulk->rb_mutex);
		m0_tl_for(rpcbulk, &rbulk->rb_buflist, rbuf) {
			m0_net_buffer_del(rbuf->bb_nbuf, tm);
		} m0_tl_endfor;
		m0_mutex_unlock(&rbulk->rb_mutex);
	} m0_tl_endfor;

	/*
	 * Removes the net buffers from transfer machine contained by rpc bulk
	 * structure belonging to res_fop since they will be replaced by
	 * new coalesced net buffers.
	 */
	bbulk = m0_fop_to_rpcbulk(bkp_fop);
	rbulk = m0_fop_to_rpcbulk(res_fop);
	m0_mutex_lock(&bbulk->rb_mutex);
	m0_mutex_lock(&rbulk->rb_mutex);
	m0_tl_teardown(rpcbulk, &bbulk->rb_buflist, rbuf) {
		rpcbulk_tlist_add(&rbulk->rb_buflist, rbuf);
		m0_net_buffer_del(rbuf->bb_nbuf, tm);
		rbulk->rb_bytes -= m0_vec_count(&rbuf->bb_nbuf->
						nb_buffer.ov_vec);
	}
	m0_mutex_unlock(&rbulk->rb_mutex);
	m0_mutex_unlock(&bbulk->rb_mutex);

	M0_LOG(M0_DEBUG, "io fops coalesced successfully.");
	rpcitem_tlist_add(items_list, &bkp_fop->f_item);
	return M0_RC(rc);
cleanup:
	M0_ASSERT(rc != 0);
	m0_tl_for(iosegset, &aggr_set.iss_list, ioseg) {
		ioseg_unlink_free(ioseg);
	} m0_tl_endfor;
	iosegset_tlist_fini(&aggr_set.iss_list);
	io_fop_bulkbuf_move(bkp_fop, res_fop);
	m0_io_fop_fini(cfop);
	m0_free(cfop);
	return M0_RC(rc);
}

__attribute__((unused))
static struct m0_fid *io_fop_fid_get(struct m0_fop *fop)
{
	return &(io_rw_get(fop))->crw_fid;
}

__attribute__((unused))
static bool io_fop_fid_equal(struct m0_fop *fop1, struct m0_fop *fop2)
{
        return m0_fid_eq(io_fop_fid_get(fop1), io_fop_fid_get(fop2));
}

static void io_fop_replied(struct m0_fop *fop, struct m0_fop *bkpfop)
{
	struct m0_io_fop     *cfop;
	struct m0_rpc_bulk   *rbulk;
	struct m0_fop_cob_rw *srw;
	struct m0_fop_cob_rw *drw;

	M0_PRE(fop != NULL);
	M0_PRE(bkpfop != NULL);
	M0_PRE(m0_is_io_fop(fop));
	M0_PRE(m0_is_io_fop(bkpfop));

	rbulk = m0_fop_to_rpcbulk(fop);
	m0_mutex_lock(&rbulk->rb_mutex);
	M0_ASSERT(rpcbulk_tlist_is_empty(&rbulk->rb_buflist));
	m0_mutex_unlock(&rbulk->rb_mutex);

	srw = io_rw_get(bkpfop);
	drw = io_rw_get(fop);
	drw->crw_desc = srw->crw_desc;
	drw->crw_ivec = srw->crw_ivec;
	cfop = container_of(bkpfop, struct m0_io_fop, if_fop);
	m0_io_fop_fini(cfop);
	m0_free(cfop);
}

static void io_fop_desc_get(struct m0_fop *fop,
			    struct m0_net_buf_desc_data **desc)
{
	struct m0_fop_cob_rw *rw;

	M0_PRE(fop != NULL);
	M0_PRE(desc != NULL);

	rw = io_rw_get(fop);
	*desc = rw->crw_desc.id_descs;
}

/* Rpc item ops for IO operations. */
static void io_item_replied(struct m0_rpc_item *item)
{
	struct m0_fop		   *fop;
	struct m0_fop		   *rfop;
	struct m0_fop		   *bkpfop;
	struct m0_rpc_item	   *ritem;
	struct m0_rpc_bulk	   *rbulk;
	struct m0_fop_cob_rw_reply *reply;

	M0_PRE(item != NULL);

	if (m0_rpc_item_error(item) != 0)
		return;

	fop = m0_rpc_item_to_fop(item);
	rbulk = m0_fop_to_rpcbulk(fop);
	rfop = m0_rpc_item_to_fop(item->ri_reply);
	reply = io_rw_rep_get(rfop);
	M0_ASSERT(ergo(reply->rwr_rc == 0,
		       reply->rwr_count == rbulk->rb_bytes));

if (0) /* if (0) is used instead of #if 0 to avoid code rot. */
{
	/** @todo Rearrange IO item merging code to work with new
		  formation code.
	 */
	/*
	 * Restores the contents of master coalesced fop from the first
	 * rpc item in m0_rpc_item::ri_compound_items list. This item
	 * is inserted by io coalescing code.
	 */
	if (!rpcitem_tlist_is_empty(&item->ri_compound_items)) {
		M0_LOG(M0_DEBUG, "Reply received for coalesced io fops.");
		ritem = rpcitem_tlist_pop(&item->ri_compound_items);
		bkpfop = m0_rpc_item_to_fop(ritem);
		if (fop->f_type->ft_ops->fto_fop_replied != NULL)
			fop->f_type->ft_ops->fto_fop_replied(fop, bkpfop);
	}

	/*
	 * The rpc_item->ri_chan is signaled by sessions code
	 * (rpc_item_replied()) which is why only member coalesced items
	 * (items which were member of a parent coalesced item) are
	 * signaled from here as they are not sent on wire but hang off
	 * a list from parent coalesced item.
	 */
	m0_tl_for(rpcitem, &item->ri_compound_items, ritem) {
		fop = m0_rpc_item_to_fop(ritem);
		rbulk = m0_fop_to_rpcbulk(fop);
		m0_mutex_lock(&rbulk->rb_mutex);
		M0_ASSERT(rbulk != NULL && m0_tlist_is_empty(&rpcbulk_tl,
			  &rbulk->rb_buflist));
		/* Notifies all member coalesced items of completion status. */
		rbulk->rb_rc = item->ri_error;
		m0_mutex_unlock(&rbulk->rb_mutex);
		/* XXX Use rpc_item_replied()
		       But we'll fix it later because this code path will need
		       significant changes because of new formation code.
		 */
		/* m0_chan_broadcast(&ritem->ri_chan); */
	} m0_tl_endfor;
}
}

static void item_io_coalesce(struct m0_rpc_item *head, struct m0_list *list,
			     uint64_t size)
{
	/* Coalescing RPC items is not yet supported */
if (0)
{
	int			 rc;
	struct m0_fop		*bfop;
	struct m0_rpc_item	*item;

	M0_PRE(head != NULL);
	M0_PRE(list != NULL);
	M0_PRE(size > 0);

	if (m0_list_is_empty(list))
		return;

	/*
	 * Traverses through the list and finds out items that match with
	 * head on basis of fid and intent (read/write). Matching items
	 * are removed from session->s_unbound_items list and added to
	 * head->compound_items list.
	 */
	bfop = m0_rpc_item_to_fop(head);

	if (rpcitem_tlist_is_empty(&head->ri_compound_items))
		return;

	/*
	 * Add the bound item to list of compound items as this will
	 * include the bound item's io vector in io coalescing
	 */
	rpcitem_tlist_add(&head->ri_compound_items, head);

	rc = bfop->f_type->ft_ops->fto_io_coalesce(bfop, size);
	if (rc != 0) {
		m0_tl_teardown(rpcitem, &head->ri_compound_items, item) {
			(void)item; /* remove the "unused variable" warning.*/
		}
	} else {
		/*
		 * Item at head is the backup item which is not present
		 * in sessions unbound list.
		 */
		rpcitem_tlist_del(head);
	}
}
}

M0_INTERNAL m0_bcount_t m0_io_fop_byte_count(struct m0_io_fop *iofop)
{
	m0_bcount_t             count = 0;
	struct m0_rpc_bulk_buf *rbuf;

	M0_PRE(iofop != NULL);

	m0_tl_for (rpcbulk, &iofop->if_rbulk.rb_buflist, rbuf) {
		count += m0_vec_count(&rbuf->bb_zerovec.z_bvec.ov_vec);
	} m0_tl_endfor;

	return count;
}

M0_INTERNAL void m0_io_fop_release(struct m0_ref *ref)
{
        struct m0_io_fop *iofop;
        struct m0_fop    *fop;

        fop   = container_of(ref, struct m0_fop, f_ref);
        iofop = container_of(fop, struct m0_io_fop, if_fop);
	M0_LOG(M0_DEBUG, "iofop %p", iofop);
        m0_io_fop_fini(iofop);
        m0_free(iofop);
}

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
