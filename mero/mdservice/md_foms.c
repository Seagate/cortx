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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_COB
#include <sys/stat.h>    /* S_ISDIR */

#include "lib/misc.h"   /* M0_SET0 */
#include "lib/arith.h"   /* M0_3WAY */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/bitstring.h"
#include "lib/rwlock.h"
#include "lib/trace.h"

#include "net/net.h"
#include "fid/fid.h"
#include "fop/fop.h"
#include "cob/cob.h"
#include "reqh/reqh.h"
#include "rpc/rpc_opcodes.h"
#include "fop/fom_generic.h"
#include "layout/layout.h"
#include "layout/pdclust.h"
#include "mdservice/md_fops.h"
#include "mdservice/md_fops_xc.h"
#include "mdservice/md_foms.h"
#include "mdservice/md_service.h"
#include "mdstore/mdstore.h"

static int md_locate(struct m0_mdstore *md, struct m0_fid *tfid,
		     struct m0_cob **cob);
static size_t m0_md_req_fom_locality_get(const struct m0_fom *fom);

M0_INTERNAL void m0_md_cob_wire2mem(struct m0_cob_attr *attr,
				    const struct m0_fop_cob *body)
{
	M0_SET0(attr);
	attr->ca_pfid = body->b_pfid;
	attr->ca_tfid = body->b_tfid;
	attr->ca_valid = body->b_valid;
	if (body->b_valid & M0_COB_MODE)
		attr->ca_mode = body->b_mode;
	if (body->b_valid & M0_COB_UID)
		attr->ca_uid = body->b_uid;
	if (body->b_valid & M0_COB_GID)
		attr->ca_gid = body->b_gid;
	if (body->b_valid & M0_COB_ATIME)
		attr->ca_atime = body->b_atime;
	if (body->b_valid & M0_COB_MTIME)
		attr->ca_mtime = body->b_mtime;
	if (body->b_valid & M0_COB_CTIME)
		attr->ca_ctime = body->b_ctime;
	if (body->b_valid & M0_COB_NLINK)
		attr->ca_nlink = body->b_nlink;
	if (body->b_valid & M0_COB_RDEV)
		attr->ca_rdev = body->b_rdev;
	if (body->b_valid & M0_COB_SIZE)
		attr->ca_size = body->b_size;
	if (body->b_valid & M0_COB_BLKSIZE)
		attr->ca_blksize = body->b_blksize;
	if (body->b_valid & M0_COB_BLOCKS)
		attr->ca_blocks = body->b_blocks;
	if (body->b_valid & M0_COB_LID)
		attr->ca_lid = body->b_lid;
	if (body->b_valid & M0_COB_PVER)
		attr->ca_pver = body->b_pver;
	attr->ca_version = body->b_version;
}

M0_INTERNAL void m0_md_cob_mem2wire(struct m0_fop_cob *body,
				    const struct m0_cob_attr *attr)
{
	body->b_pfid = attr->ca_pfid;
	body->b_tfid = attr->ca_tfid;
	body->b_valid = attr->ca_valid;
	if (body->b_valid & M0_COB_MODE)
		body->b_mode = attr->ca_mode;
	if (body->b_valid & M0_COB_UID)
		body->b_uid = attr->ca_uid;
	if (body->b_valid & M0_COB_GID)
		body->b_gid = attr->ca_gid;
	if (body->b_valid & M0_COB_ATIME)
		body->b_atime = attr->ca_atime;
	if (body->b_valid & M0_COB_MTIME)
		body->b_mtime = attr->ca_mtime;
	if (body->b_valid & M0_COB_CTIME)
		body->b_ctime = attr->ca_ctime;
	if (body->b_valid & M0_COB_NLINK)
		body->b_nlink = attr->ca_nlink;
	if (body->b_valid & M0_COB_RDEV)
		body->b_rdev = attr->ca_rdev;
	if (body->b_valid & M0_COB_SIZE)
		body->b_size = attr->ca_size;
	if (body->b_valid & M0_COB_BLKSIZE)
		body->b_blksize = attr->ca_blksize;
	if (body->b_valid & M0_COB_BLOCKS)
		body->b_blocks = attr->ca_blocks;
	if (body->b_valid & M0_COB_LID)
		body->b_lid = attr->ca_lid;
	if (body->b_valid & M0_COB_PVER)
		body->b_pver = attr->ca_pver;
	body->b_version = attr->ca_version;
}

static void m0_md_create_credit(struct m0_mdstore *md,
				struct m0_be_tx_credit *accum)
{
	m0_mdstore_create_credit(md, accum);
	m0_mdstore_link_credit(md, accum);
	m0_mdstore_setattr_credit(md, accum);
}

/**
   Create object in @pfid with @tfid and attributes from @attr.
   Handle possible variants for existing/missing objects and
   links.
 */
static int m0_md_create(struct m0_mdstore   *md,
			struct m0_fid       *pfid,
			struct m0_fid       *tfid,
			struct m0_cob_attr  *attr,
			struct m0_be_tx     *tx)
{
	struct m0_cob *scob = NULL;
	int            rc;

	rc = m0_mdstore_locate(md, tfid, &scob, M0_MD_LOCATE_STORED);
	if (rc == -ENOENT) {
		/*
		 * Statdata cob is not found, let's create it. This
		 * must be normal create case.
		 */
		rc = m0_mdstore_fcreate(md, pfid, attr, &scob, tx);
	} else if (rc == 0) {
		/*
		 * There is statdata name, this must be hardlink
		 * case.
		 */
		rc = m0_mdstore_link(md, pfid, scob, &attr->ca_name, tx);
		/*
		 * Each operation changes target attributes (times).
		 * We want to keep them up-to-date.
		 */
		if (rc == 0)
			rc = m0_mdstore_setattr(md, scob, attr, tx);
	}
	if (scob)
		m0_cob_put(scob);
	M0_LOG(M0_DEBUG, "create \"%.*s\" finished with %d and lid=%d",
	       (int)attr->ca_name.b_nob, (char *)attr->ca_name.b_addr, rc,
	       (int)attr->ca_lid);
	return M0_RC(rc);
}

static inline int m0_md_tick_generic(struct m0_fom *fom)
{
	M0_PRE(m0_fom_invariant(fom));

	if (m0_fom_phase(fom) < M0_FOPH_NR) {
		if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN) {
			/* XXX: should be fixed after layout converted to BE */
			struct m0_be_tx_credit cred = M0_BE_TX_CREDIT(512, 65536);
			m0_be_tx_credit_add(&fom->fo_tx.tx_betx_cred, &cred);
		}
		return m0_fom_tick_generic(fom);
	}

	return 0;
}

static int md_tail(struct m0_fom *fom, struct m0_fop_cob *body,
		   struct m0_fop_mod_rep *mod, int rc)
{
	const struct m0_fop *fop = fom->fo_fop;

	M0_LOG(M0_DEBUG, "%s "FID_F" finished with %d.",
	       fop->f_type->ft_name, FID_P(&body->b_pfid), rc);
	body->b_rc = rc;
	m0_fom_mod_rep_fill(mod, fom);
	m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	return M0_FSO_AGAIN;
}

static int m0_md_tick_create(struct m0_fom *fom)
{
	struct m0_mdstore        *md;
	struct m0_cob_attr        attr;
	struct m0_fop_cob        *body;
	struct m0_fop_create     *req;
	struct m0_fop_create_rep *rep;
	struct m0_fop            *fop;
	struct m0_fop            *fop_rep;
	int                       rc;
	struct m0_rpc_item       *item;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_md_create_credit(md, m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	item = m0_fop_to_rpc_item(fop);
	req = m0_fop_data(fop);

	body = &req->c_body;
	m0_md_cob_wire2mem(&attr, body);

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	m0_buf_init(&attr.ca_name, req->c_name.s_buf, req->c_name.s_len);

	M0_LOG(M0_DEBUG, "%p[%u] Create "FID_F"/"FID_F" %.*s",
	       item, item->ri_type->rit_opcode,
	       FID_P(&body->b_pfid), FID_P(&body->b_tfid),
	       (int)attr.ca_name.b_nob, (char *)attr.ca_name.b_addr);

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	if (S_ISLNK(attr.ca_mode)) {
		/** Symlink body size is stored in @attr->ca_size */
		m0_buf_init(&attr.ca_link, (char *)req->c_target.s_buf, attr.ca_size);
	}

	rc = m0_md_create(md, &body->b_pfid,
			  &body->b_tfid, &attr, m0_fom_tx(fom));
out:
	return md_tail(fom, &rep->c_body, &rep->c_mod_rep, rc);
}

static int m0_md_tick_link(struct m0_fom *fom)
{
	struct m0_mdstore        *md;
	struct m0_fop_cob        *body;
	struct m0_fop_link       *req;
	struct m0_fop_link_rep   *rep;
	struct m0_fop            *fop;
	struct m0_fop            *fop_rep;
	struct m0_cob_attr        attr;
	int                       rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_md_create_credit(md, m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	body = &req->l_body;
	m0_md_cob_wire2mem(&attr, body);
	m0_buf_init(&attr.ca_name, req->l_name.s_buf, req->l_name.s_len);

	M0_LOG(M0_DEBUG, "Link "FID_F"/%.*s -> "FID_F" started",
	       FID_P(&body->b_pfid), (int)req->l_name.s_len,
	       (char *)req->l_name.s_buf, FID_P(&body->b_tfid));

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	rc = m0_md_create(md,
			  &body->b_pfid, &body->b_tfid, &attr,
			  m0_fom_tx(fom));
out:
	return md_tail(fom, &rep->l_body, &rep->l_mod_rep, rc);
}

static int m0_md_tick_unlink(struct m0_fom *fom)
{
	struct m0_mdstore        *md;
	struct m0_cob_attr        attr;
	struct m0_fop_cob        *body;
	struct m0_cob            *scob = NULL;
	struct m0_fop_unlink     *req;
	struct m0_fop_unlink_rep *rep;
	struct m0_fop            *fop;
	struct m0_fop            *fop_rep;
	struct m0_be_tx          *tx;
	int                       rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_mdstore_unlink_credit(md, m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);
	body = &req->u_body;

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	md = m0_fom_reqh(fom)->rh_mdstore;
	tx = m0_fom_tx(fom);

	m0_md_cob_wire2mem(&attr, body);
	m0_buf_init(&attr.ca_name, req->u_name.s_buf, req->u_name.s_len);

	M0_LOG(M0_DEBUG, "Unlink "FID_F"/%.*s started",
	       FID_P(&body->b_pfid),
	       (int)req->u_name.s_len, (char *)req->u_name.s_buf);

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	rc = md_locate(md, &body->b_tfid, &scob);
	if (rc != 0)
		goto out;

	rc = m0_mdstore_unlink(md, &body->b_pfid, scob, &attr.ca_name, tx);
	m0_cob_put(scob);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "Unlink "FID_F"/%.*s failed with %d",
		       FID_P(&body->b_pfid),
		       (int)req->u_name.s_len, (char *)req->u_name.s_buf,
			rc);
		goto out;
	}
out:
	return md_tail(fom, &rep->u_body, &rep->u_mod_rep, rc);
}

static void m0_md_rename_credit(struct m0_mdstore  *md,
				struct m0_be_tx_credit *accum)
{
	m0_mdstore_rename_credit(md, accum);
	m0_mdstore_setattr_credit(md, accum);
}

static int m0_md_rename(struct m0_mdstore   *md,
			struct m0_fid       *pfid_tgt,
			struct m0_fid       *pfid_src,
			struct m0_fid       *tfid_tgt,
			struct m0_fid       *tfid_src,
			struct m0_cob_attr  *tattr,
			struct m0_cob_attr  *sattr,
			struct m0_cob       *tcob,
			struct m0_cob       *scob,
			struct m0_be_tx     *tx)
{
	int rc;

	M0_ASSERT(scob != NULL);
	M0_ASSERT(tcob != NULL);

	/*
	 * Do normal rename as all objects are fine.
	 */
	rc = m0_mdstore_rename(md, pfid_tgt, pfid_src, tcob, scob,
			       &tattr->ca_name, &sattr->ca_name, tx);
	if (rc != 0)
		return M0_RC(rc);
	/*
	 * Update attributes of source and target.
	 */
	if (m0_fid_eq(m0_cob_fid(scob), m0_cob_fid(tcob))) {
		if (tcob->co_nsrec.cnr_nlink > 0)
			rc = m0_mdstore_setattr(md, tcob, tattr, tx);
	} else {
		rc = m0_mdstore_setattr(md, scob, sattr, tx);
		if (rc == 0 && tcob->co_nsrec.cnr_nlink > 0)
			rc = m0_mdstore_setattr(md, tcob, tattr, tx);
	}
	return M0_RC(rc);
}

static int m0_md_tick_rename(struct m0_fom *fom)
{
	struct m0_mdstore        *md;
	struct m0_fop_cob        *sbody;
	struct m0_fop_cob        *tbody;
	struct m0_fop_rename     *req;
	struct m0_fop_rename_rep *rep;
	struct m0_fop            *fop;
	struct m0_fop            *fop_rep;
	struct m0_cob            *tcob = NULL;
	struct m0_cob            *scob = NULL;
	struct m0_cob_attr        sattr;
	struct m0_cob_attr        tattr;
	struct m0_be_tx          *tx;
	int                       rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_md_rename_credit(md, m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	tx = m0_fom_tx(fom);

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	sbody = &req->r_sbody;
	tbody = &req->r_tbody;

	m0_md_cob_wire2mem(&tattr, tbody);
	m0_buf_init(&tattr.ca_name, req->r_tname.s_buf, req->r_tname.s_len);

	m0_md_cob_wire2mem(&sattr, sbody);
	m0_buf_init(&sattr.ca_name, req->r_sname.s_buf, req->r_sname.s_len);

	rc = md_locate(md, &sbody->b_tfid, &scob);
	if (rc != 0) {
		goto out;
	}
	if (m0_fid_eq(&tbody->b_tfid, &sbody->b_tfid)) {
		rc = m0_md_rename(md, &tbody->b_pfid, &sbody->b_pfid,
				  &tbody->b_tfid, &sbody->b_tfid, &tattr,
				  &sattr, scob, scob, tx);
	} else {
		rc = md_locate(md, &tbody->b_tfid, &tcob);
		if (rc != 0) {
			goto out;
		}
		rc = m0_md_rename(md, &tbody->b_pfid, &sbody->b_pfid,
				  &tbody->b_tfid, &sbody->b_tfid, &tattr,
				  &sattr, tcob, scob, tx);
		m0_cob_put(tcob);
	}
	m0_cob_put(scob);
out:
	return md_tail(fom, &rep->r_body, &rep->r_mod_rep, rc);
}

static int m0_md_tick_open(struct m0_fom *fom)
{
	struct m0_mdstore        *md;
	struct m0_fop_cob        *body;
	struct m0_cob            *cob;
	struct m0_fop_open       *req;
	struct m0_fop_open_rep   *rep;
	struct m0_fop            *fop;
	struct m0_fop            *fop_rep;
	struct m0_cob_attr        attr;
	int                       rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_mdstore_setattr_credit(md, m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	body = &req->o_body;
	m0_md_cob_wire2mem(&attr, body);

	rc = m0_mdstore_locate(md, &body->b_tfid, &cob, M0_MD_LOCATE_STORED);
	if (rc == 0) {
		rc = m0_mdstore_open(md,
				     cob, body->b_flags, m0_fom_tx(fom));
		if (rc == 0 &&
		    (!(attr.ca_valid & M0_COB_NLINK) || attr.ca_nlink > 0)) {
			/*
			 * Mode contains open flags that we don't need
			 * to store to db.
			 */
			attr.ca_valid &= ~M0_COB_MODE;
			rc = m0_mdstore_setattr(md,
				cob, &attr, m0_fom_tx(fom));
		}
		m0_cob_put(cob);
	} else if (rc == -ENOENT) {
		/*
		 * Lustre has create before open in case of OPEN_CREATE.
		 * We don't have to create anything here as file already
		 * should exist, let's just check this.
		 */
		/* M0_ASSERT(!(body->b_flags & M0_MD_OPEN_CREAT)); */
	} else if (rc != 0) {
		goto out;
	}

out:
	return md_tail(fom, &rep->o_body, &rep->o_mod_rep, rc);
}

static int m0_md_tick_close(struct m0_fom *fom)
{
	struct m0_mdstore        *md;
	struct m0_fop_cob        *body;
	struct m0_cob            *cob;
	struct m0_fop_close      *req;
	struct m0_fop_close_rep  *rep;
	struct m0_fop            *fop;
	struct m0_fop            *fop_rep;
	struct m0_cob_attr        attr;
	int                       rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_mdstore_setattr_credit(md, m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	body = &req->c_body;
	m0_md_cob_wire2mem(&attr, body);

	/*
	 * @TODO: This should lookup for cobs in special opened
	 * cobs table. But so far orphans and open/close are not
	 * quite implemented and we lookup on main store to make
	 * ut happy.
	 */
	rc = md_locate(md, &body->b_tfid, &cob);
	if (rc != 0) {
		goto out;
	}

	rc = m0_mdstore_close(md, cob, m0_fom_tx(fom));
	if (rc == 0 &&
	    (!(attr.ca_valid & M0_COB_NLINK) || attr.ca_nlink > 0)) {
		/*
		 * Mode contains open flags that we don't need
		 * to store to db.
		 */
		attr.ca_valid &= ~M0_COB_MODE;
		rc = m0_mdstore_setattr(md, cob, &attr, m0_fom_tx(fom));
	}
	m0_cob_put(cob);
out:
	rep->c_body.b_rc = rc;
	m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	return M0_FSO_AGAIN;
}

static int m0_md_tick_setattr(struct m0_fom *fom)
{
	struct m0_mdstore             *md;
	struct m0_cob_attr             attr;
	struct m0_fop_cob             *body;
	struct m0_cob                 *cob;
	struct m0_fop_setattr         *req;
	struct m0_fop_setattr_rep     *rep;
	struct m0_fop                 *fop;
	struct m0_fop                 *fop_rep;
	int                            rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_mdstore_setattr_credit(md, m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	body = &req->s_body;
	m0_md_cob_wire2mem(&attr, body);

	M0_LOG(M0_DEBUG, "Setattr for "FID_F" started", FID_P(&body->b_tfid));

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	/*
	 * Setattr fop does not carry enough information to create
	 * an object in case there is no target yet. This is why
	 * we return quickly if no object is found.
	 */
	rc = md_locate(md, &body->b_tfid, &cob);
	if (rc != 0) {
		goto out;
	}

	rc = m0_mdstore_setattr(md, cob, &attr, m0_fom_tx(fom));
	m0_cob_put(cob);
out:
	return md_tail(fom, &rep->s_body, &rep->s_mod_rep, rc);
}

static int m0_md_tick_lookup(struct m0_fom *fom)
{
	struct m0_mdstore             *md;
	struct m0_cob_attr             attr;
	struct m0_fop_cob             *body;
	struct m0_cob                 *cob;
	struct m0_fop_lookup          *req;
	struct m0_fop_lookup_rep      *rep;
	struct m0_fop                 *fop;
	struct m0_fop                 *fop_rep;
	struct m0_fid                  tfid;
	int                            rc;
	struct m0_buf                  name;

	md = m0_fom_reqh(fom)->rh_mdstore;
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);
	body = &req->l_body;

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	m0_buf_init(&name, req->l_name.s_buf, req->l_name.s_len);

	M0_LOG(M0_DEBUG, "Lookup for \"%.*s\" in object "FID_F,
	       (int)name.b_nob, (char *)name.b_addr, FID_P(&body->b_pfid));

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	rc = m0_mdstore_lookup(md, &body->b_pfid, &name, &cob);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "m0_mdstore_lookup() failed with %d", rc);
		goto out;
	}
	tfid = *m0_cob_fid(cob);
	m0_cob_put(cob);

	M0_LOG(M0_DEBUG, "Found object "FID_F" go for getattr", FID_P(&tfid));

	rc = md_locate(md, &tfid, &cob);
	if (rc != 0) {
		goto out;
	}

	rc = m0_mdstore_getattr(md, cob, &attr);
	m0_cob_put(cob);

	if (rc == 0) {
		attr.ca_valid = M0_COB_ALL;
		m0_md_cob_mem2wire(&rep->l_body, &attr);
	} else {
		M0_LOG(M0_DEBUG, "Getattr on object "FID_F" failed with %d",
		       FID_P(m0_cob_fid(cob)), rc);
	}
out:
	M0_LOG(M0_DEBUG, "Lookup for \"%.*s\" finished with %d",
	       (int)name.b_nob, (char *)name.b_addr, rc);

	rep->l_body.b_rc = rc;
	m0_fom_phase_move(fom, rc, rc != 0 ? M0_FOPH_FAILURE : M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

static int m0_md_tick_getattr(struct m0_fom *fom)
{
	struct m0_mdstore             *md;
	struct m0_cob_attr             attr;
	struct m0_fop_cob             *body;
	struct m0_cob                 *cob;
	struct m0_fop_getattr         *req;
	struct m0_fop_getattr_rep     *rep;
	struct m0_fop                 *fop;
	struct m0_fop                 *fop_rep;
	int                            rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);
	body = &req->g_body;

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	M0_LOG(M0_DEBUG, "Getattr for "FID_F" started", FID_P(&body->b_tfid));

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	rc = md_locate(md, &body->b_tfid, &cob);
	if (rc != 0)
		goto out;

	rc = m0_mdstore_getattr(md, cob, &attr);
	m0_cob_put(cob);
	if (rc == 0) {
		attr.ca_valid = M0_COB_ALL;
		m0_md_cob_mem2wire(&rep->g_body, &attr);
	}
out:
	M0_LOG(M0_DEBUG, "Getattr for "FID_F" finished with %d",
	       FID_P(&body->b_tfid), rc);
	rep->g_body.b_rc = rc;
	m0_fom_phase_move(fom, rc, rc != 0 ? M0_FOPH_FAILURE : M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

static void md_statfs_mem2wire(struct m0_fop_statfs_rep *rep,
			       struct m0_statfs *statfs)
{
	rep->f_type = statfs->sf_type;
	rep->f_bsize = statfs->sf_bsize;
	rep->f_blocks = statfs->sf_blocks;
	rep->f_bfree = statfs->sf_bfree;
	rep->f_bavail = statfs->sf_bavail;
	rep->f_files = statfs->sf_files;
	rep->f_ffree = statfs->sf_ffree;
	rep->f_namelen = statfs->sf_namelen;
	rep->f_root = statfs->sf_root;
}

static int m0_md_tick_getxattr(struct m0_fom *fom)
{
	struct m0_mdstore             *md;
	struct m0_cob_eakey           *eakey;
	struct m0_cob_earec           *earec;
	struct m0_fop_cob             *body;
	struct m0_cob                 *cob;
	struct m0_fop_getxattr        *req;
	struct m0_fop_getxattr_rep    *rep;
	struct m0_fop                 *fop;
	struct m0_fop                 *fop_rep;
	int                            rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);
	body = &req->g_body;

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	M0_LOG(M0_DEBUG, "Getxattr %.*s for "FID_F" started",
	       req->g_key.s_len, (char *)req->g_key.s_buf, FID_P(&body->b_pfid));

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	rc = md_locate(md, &body->b_tfid, &cob);
	if (rc != 0)
		goto out;

	rc = m0_cob_eakey_make(&eakey, m0_cob_fid(cob),
			       (char *)req->g_key.s_buf,
			       req->g_key.s_len);
	if (rc != 0) {
		goto out;
	}

	earec = m0_alloc(m0_cob_max_earec_size());
	if (earec == NULL) {
		m0_free(eakey);
		goto out;
	}

	rc = m0_cob_ea_get(cob, eakey, earec, m0_fom_tx(fom));
	m0_cob_put(cob);
	if (rc == 0) {
		rep->g_value.s_len = earec->cer_size;
		rep->g_value.s_buf = m0_alloc(earec->cer_size);
		if (rep->g_value.s_buf == NULL) {
			m0_free(eakey);
			m0_free(earec);
			rc = -ENOMEM;
			goto out;
		}
		memcpy(rep->g_value.s_buf, earec->cer_body, earec->cer_size);
	}
	m0_free(eakey);
	m0_free(earec);
out:
	M0_LOG(M0_DEBUG, "Getxattr for "FID_F" finished with %d",
	       FID_P(&body->b_pfid), rc);
	rep->g_body.b_rc = rc;
	m0_fom_phase_move(fom, rc, rc != 0 ? M0_FOPH_FAILURE : M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

static int m0_md_tick_setxattr(struct m0_fom *fom)
{
	struct m0_mdstore             *md;
	struct m0_cob_eakey           *eakey;
	struct m0_cob_earec           *earec;
	struct m0_fop_cob             *body;
	struct m0_cob                 *cob;
	struct m0_fop_setxattr        *req;
	struct m0_fop_setxattr_rep    *rep;
	struct m0_fop                 *fop;
	struct m0_fop                 *fop_rep;
	int                            rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_cob_tx_credit(md->md_dom, M0_COB_OP_FEA_SET,
				 m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);
	body = &req->s_body;

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	M0_LOG(M0_DEBUG, "Setxattr %.*s=%.*s for "FID_F" started",
	       req->s_key.s_len, (char *)req->s_key.s_buf,
	       req->s_value.s_len, (char *)req->s_value.s_buf,
	       FID_P(&body->b_pfid));

	/*
	 * Init some fop fields (full path) that require mdstore and other
	 * initialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	rc = md_locate(md, &body->b_tfid, &cob);
	if (rc != 0)
		goto out;

	rc = m0_cob_eakey_make(&eakey, m0_cob_fid(cob),
			       (char *)req->s_key.s_buf,
			       req->s_key.s_len);
	if (rc != 0) {
		goto out;
	}

	earec = m0_alloc(m0_cob_max_earec_size());
	if (earec == NULL) {
		m0_free(eakey);
		goto out;
	}

	earec->cer_size = req->s_value.s_len;
	memcpy(earec->cer_body, req->s_value.s_buf, earec->cer_size);

	rc = m0_cob_ea_set(cob, eakey, earec, m0_fom_tx(fom));
	m0_cob_put(cob);
	m0_free(eakey);
	m0_free(earec);
out:
	return md_tail(fom, &rep->s_body, &rep->s_mod_rep, rc);
}

static int m0_md_tick_delxattr(struct m0_fom *fom)
{
	struct m0_mdstore             *md;
	struct m0_cob_eakey           *eakey;
	struct m0_fop_cob             *body;
	struct m0_cob                 *cob;
	struct m0_fop_delxattr        *req;
	struct m0_fop_delxattr_rep    *rep;
	struct m0_fop                 *fop;
	struct m0_fop                 *fop_rep;
	int                            rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	if (m0_fom_phase(fom) == M0_FOPH_TXN_OPEN)
		m0_cob_tx_credit(md->md_dom, M0_COB_OP_FEA_DEL,
				 m0_fom_tx_credit(fom));
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);
	body = &req->d_body;

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	M0_LOG(M0_DEBUG, "Delxattr for "FID_F" started", FID_P(&body->b_tfid));

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	rc = md_locate(md, &body->b_tfid, &cob);
	if (rc != 0)
		goto out;

	rc = m0_cob_eakey_make(&eakey, m0_cob_fid(cob),
			       (char *)req->d_key.s_buf,
			       req->d_key.s_len);
	if (rc != 0) {
		goto out;
	}

	rc = m0_cob_ea_del(cob, eakey, m0_fom_tx(fom));
	m0_cob_put(cob);
	m0_free(eakey);
out:
	return md_tail(fom, &rep->d_body, &rep->d_mod_rep, rc);
}

static int m0_md_tick_listxattr(struct m0_fom *fom)
{
	int rc = -EOPNOTSUPP;
	m0_fom_phase_move(fom, rc, rc != 0 ? M0_FOPH_FAILURE : M0_FOPH_SUCCESS);
	return M0_FSO_AGAIN;
}

static int m0_md_tick_statfs(struct m0_fom *fom)
{
	struct m0_fop_statfs_rep *rep;
	struct m0_fop            *fop;
	struct m0_fop            *fop_rep;
	struct m0_statfs          statfs;
	int                       rc;

	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	rc = m0_mdstore_statfs(m0_fom_reqh(fom)->rh_mdstore, &statfs);
	if (rc == 0)
		md_statfs_mem2wire(rep, &statfs);
out:
	rep->f_rc = rc;
	m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	return M0_FSO_AGAIN;
}

/** Readdir fop data buffer size */
#define M0_MD_READDIR_BUFF_SIZE 4096

static int m0_md_tick_readdir(struct m0_fom *fom)
{
	struct m0_mdstore             *md;
	struct m0_fop_cob             *body;
	struct m0_cob                 *cob;
	struct m0_fop_readdir         *req;
	struct m0_fop_readdir_rep     *rep;
	struct m0_fop                 *fop;
	struct m0_fop                 *fop_rep;
	struct m0_rdpg                 rdpg;
	void                          *addr;
	int                            rc;

	md = m0_fom_reqh(fom)->rh_mdstore;
	rc = m0_md_tick_generic(fom);
	if (rc != 0)
		return M0_RC(rc);

	fop = fom->fo_fop;
	M0_ASSERT(fop != NULL);
	req = m0_fop_data(fop);

	fop_rep = fom->fo_rep_fop;
	M0_ASSERT(fop_rep != NULL);
	rep = m0_fop_data(fop_rep);

	/**
	 * Init some fop fields (full path) that require mdstore and other
	 * initialialized structures.
	 */
	rc = m0_md_fop_init(fop, fom);
	if (rc != 0)
		goto out;

	body = &req->r_body;
	rc = md_locate(md, &body->b_tfid, &cob);
	if (rc != 0)
		goto out;

	if (!S_ISDIR(cob->co_omgrec.cor_mode)) {
		rc = -ENOTDIR;
		m0_cob_put(cob);
		goto out;
	}

	M0_SET0(&rdpg);
	rdpg.r_pos = m0_alloc(M0_MD_MAX_NAME_LEN);
	if (rdpg.r_pos == NULL) {
		m0_cob_put(cob);
		rc = -ENOMEM;
		goto out;
	}
	m0_bitstring_copy(rdpg.r_pos,
			 (char *)req->r_pos.s_buf, req->r_pos.s_len);

	addr = m0_alloc(M0_MD_READDIR_BUFF_SIZE);
	if (addr == NULL) {
		m0_bitstring_free(rdpg.r_pos);
		m0_cob_put(cob);
		rc = -ENOMEM;
		goto out;
	}

	m0_buf_init(&rdpg.r_buf, addr, M0_MD_READDIR_BUFF_SIZE);

	rc = m0_mdstore_readdir(md, cob, &rdpg);
	m0_free(rdpg.r_pos);
	m0_cob_put(cob);
	if (rc < 0) {
		m0_free(addr);
		goto out;
	}

	/*
	 * Prepare end position.
	 */
	rep->r_end.s_len = m0_bitstring_len_get(rdpg.r_end);
	rep->r_end.s_buf = m0_alloc(rep->r_end.s_len);
	if (rep->r_end.s_buf == NULL) {
		m0_free(addr);
		rc = -ENOMEM;
		goto out;
	}
	memcpy(rep->r_end.s_buf, m0_bitstring_buf_get(rdpg.r_end),
	       rep->r_end.s_len);
	m0_bitstring_free(rdpg.r_end);

	/*
	 * Prepare buf with data.
	 */
	rep->r_buf.b_count = rdpg.r_buf.b_nob;
	rep->r_buf.b_addr = rdpg.r_buf.b_addr;
out:
	rep->r_body.b_rc = rc;

	/*
	 * Readddir return convention:
	 * <0 - some error occured;
	 *  0 - no errors, more data available for next readdir;
	 * >0 - EOF detyected, more data available but this is the last page.
	 *
	 * Return code according to this convention should go to client but
	 * local state machine requires "normal" errors. Let's adopt @rc.
	 */
	rc = (rc < 0 ? rc : 0);
	m0_fom_phase_moveif(fom, rc, M0_FOPH_SUCCESS, M0_FOPH_FAILURE);
	return M0_FSO_AGAIN;
}

static int m0_md_req_path_get(struct m0_mdstore *mdstore,
			      struct m0_fid *fid,
			      struct m0_fop_str *str)
{
	/*
	 * This was solely used for replicator, which is not needed right now.
	 * The reason to disable this is that, m0_mdstore_path() tries to init
	 * fid's cob, which is not existing for .mero/fid directories. Later
	 * when we need this path, we will rework it different way.
	 */
#if 0
	int rc;

	rc = m0_mdstore_path(mdstore, fid, (char **)&str->s_buf);
	if (rc != 0)
		return M0_RC(rc);
	str->s_len = strlen((char *)str->s_buf);
#endif
	return 0;
}

M0_INTERNAL int m0_md_fop_init(struct m0_fop *fop, struct m0_fom *fom)
{
	struct m0_fop_create    *create;
	struct m0_fop_unlink    *unlink;
	struct m0_fop_rename    *rename;
	struct m0_fop_link      *link;
	struct m0_fop_setattr   *setattr;
	struct m0_fop_getattr   *getattr;
	struct m0_fop_lookup    *lookup;
	struct m0_fop_open      *open;
	struct m0_fop_close     *close;
	struct m0_fop_readdir   *readdir;
	struct m0_mdstore       *md;
	int                      rc;

	M0_PRE(fop != NULL);
	md = m0_fom_reqh(fom)->rh_mdstore;

	switch (m0_fop_opcode(fop)) {
	case M0_MDSERVICE_CREATE_OPCODE:
		create = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&create->c_body.b_pfid,
					&create->c_path);
		break;
	case M0_MDSERVICE_LINK_OPCODE:
		link = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&link->l_body.b_pfid,
					&link->l_tpath);
		if (rc != 0)
			return M0_RC(rc);
		rc = m0_md_req_path_get(md,
					&link->l_body.b_tfid,
					&link->l_spath);
		if (rc != 0) {
			m0_free0(&link->l_tpath.s_buf);
			link->l_tpath.s_len = 0;
			return M0_RC(rc);
		}
		break;
	case M0_MDSERVICE_UNLINK_OPCODE:
		unlink = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&unlink->u_body.b_pfid,
					&unlink->u_path);
		break;
	case M0_MDSERVICE_RENAME_OPCODE:
		rename = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&rename->r_sbody.b_pfid,
					&rename->r_spath);
		if (rc != 0)
			return M0_RC(rc);
		rc = m0_md_req_path_get(md,
					&rename->r_tbody.b_pfid,
					&rename->r_tpath);
		if (rc != 0) {
			m0_free0(&rename->r_spath.s_buf);
			rename->r_spath.s_len = 0;
			return M0_RC(rc);
		}
		break;
	case M0_MDSERVICE_OPEN_OPCODE:
		open = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&open->o_body.b_tfid,
					&open->o_path);
		break;
	case M0_MDSERVICE_CLOSE_OPCODE:
		close = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&close->c_body.b_tfid,
					&close->c_path);
		break;
	case M0_MDSERVICE_SETATTR_OPCODE:
		setattr = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&setattr->s_body.b_tfid,
					&setattr->s_path);
		break;
	case M0_MDSERVICE_GETATTR_OPCODE:
		getattr = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&getattr->g_body.b_tfid,
					&getattr->g_path);
		break;
	case M0_MDSERVICE_LOOKUP_OPCODE:
		lookup = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&lookup->l_body.b_pfid,
					&lookup->l_path);
		break;
	case M0_MDSERVICE_READDIR_OPCODE:
		readdir = m0_fop_data(fop);
		rc = m0_md_req_path_get(md,
					&readdir->r_body.b_tfid,
					&readdir->r_path);
		break;
	default:
		rc = 0;
		break;
	}

	return M0_RC(rc);
}

static void m0_md_fom_fini(struct m0_fom *fom)
{
	struct m0_fom_md *fom_obj;

	M0_ENTRY("fom=%p", fom);

	fom_obj = container_of(fom, struct m0_fom_md, fm_fom);
	/* Fini fom itself. */
	m0_fom_fini(fom);
	m0_free(fom_obj);

	M0_LEAVE();
}

/**
 * md fom finaliser is called indirectly through this pointer so that UT can
 * redirect it.
 */
void (*m0_md_req_fom_fini_func)(struct m0_fom *fom) = &m0_md_fom_fini;

static void m0_md_req_fom_fini(struct m0_fom *fom)
{
	(*m0_md_req_fom_fini_func)(fom);
}

static size_t m0_md_req_fom_locality_get(const struct m0_fom *fom)
{
	M0_PRE(fom != NULL);
	M0_PRE(fom->fo_fop != NULL);

	return m0_fop_opcode(fom->fo_fop);
}

static const struct m0_fom_ops m0_md_fom_create_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick          = m0_md_tick_create,
	.fo_fini          = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_link_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick          = m0_md_tick_link,
	.fo_fini          = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_unlink_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick          = m0_md_tick_unlink,
	.fo_fini          = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_rename_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_rename,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_open_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_open,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_close_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick  = m0_md_tick_close,
	.fo_fini  = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_setattr_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_setattr,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_getattr_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_getattr,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_setxattr_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_setxattr,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_getxattr_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_getxattr,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_delxattr_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_delxattr,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_listxattr_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_listxattr,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_lookup_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_lookup,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_statfs_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_statfs,
	.fo_fini   = m0_md_req_fom_fini
};

static const struct m0_fom_ops m0_md_fom_readdir_ops = {
	.fo_home_locality = m0_md_req_fom_locality_get,
	.fo_tick   = m0_md_tick_readdir,
	.fo_fini   = m0_md_req_fom_fini
};

M0_INTERNAL int m0_md_rep_fom_create(struct m0_fop *fop, struct m0_fom **m,
				     struct m0_reqh *reqh)
{
	return 0;
}

M0_INTERNAL int m0_md_req_fom_create(struct m0_fop *fop, struct m0_fom **m,
				     struct m0_reqh *reqh)
{
	struct m0_fop           *rep_fop;
	struct m0_fom           *fom;
	struct m0_fom_md        *fom_obj;
	struct m0_fop_type      *rep_fopt;
	const struct m0_fom_ops *ops;

	M0_PRE(fop != NULL);
	M0_PRE(m != NULL);

	fom_obj = m0_alloc(sizeof(struct m0_fom_md));
	if (fom_obj == NULL)
		return M0_ERR(-ENOMEM);

	switch (m0_fop_opcode(fop)) {
	case M0_MDSERVICE_CREATE_OPCODE:
		ops = &m0_md_fom_create_ops;
		rep_fopt = &m0_fop_create_rep_fopt;
		break;
	case M0_MDSERVICE_LOOKUP_OPCODE:
		ops = &m0_md_fom_lookup_ops;
		rep_fopt = &m0_fop_lookup_rep_fopt;
		break;
	case M0_MDSERVICE_LINK_OPCODE:
		ops = &m0_md_fom_link_ops;
		rep_fopt = &m0_fop_link_rep_fopt;
		break;
	case M0_MDSERVICE_UNLINK_OPCODE:
		ops = &m0_md_fom_unlink_ops;
		rep_fopt = &m0_fop_unlink_rep_fopt;
		break;
	case M0_MDSERVICE_RENAME_OPCODE:
		ops = &m0_md_fom_rename_ops;
		rep_fopt = &m0_fop_rename_rep_fopt;
		break;
	case M0_MDSERVICE_OPEN_OPCODE:
		ops = &m0_md_fom_open_ops;
		rep_fopt = &m0_fop_open_rep_fopt;
		break;
	case M0_MDSERVICE_CLOSE_OPCODE:
		ops = &m0_md_fom_close_ops;
		rep_fopt = &m0_fop_close_rep_fopt;
		break;
	case M0_MDSERVICE_SETATTR_OPCODE:
		ops = &m0_md_fom_setattr_ops;
		rep_fopt = &m0_fop_setattr_rep_fopt;
		break;
	case M0_MDSERVICE_GETATTR_OPCODE:
		ops = &m0_md_fom_getattr_ops;
		rep_fopt = &m0_fop_getattr_rep_fopt;
		break;
	case M0_MDSERVICE_SETXATTR_OPCODE:
		ops = &m0_md_fom_setxattr_ops;
		rep_fopt = &m0_fop_setxattr_rep_fopt;
		break;
	case M0_MDSERVICE_GETXATTR_OPCODE:
		ops = &m0_md_fom_getxattr_ops;
		rep_fopt = &m0_fop_getxattr_rep_fopt;
		break;
	case M0_MDSERVICE_DELXATTR_OPCODE:
		ops = &m0_md_fom_delxattr_ops;
		rep_fopt = &m0_fop_delxattr_rep_fopt;
		break;
	case M0_MDSERVICE_LISTXATTR_OPCODE:
		ops = &m0_md_fom_listxattr_ops;
		rep_fopt = &m0_fop_listxattr_rep_fopt;
		break;
	case M0_MDSERVICE_STATFS_OPCODE:
		ops = &m0_md_fom_statfs_ops;
		rep_fopt = &m0_fop_statfs_rep_fopt;
		break;
	case M0_MDSERVICE_READDIR_OPCODE:
		ops = &m0_md_fom_readdir_ops;
		rep_fopt = &m0_fop_readdir_rep_fopt;
		break;
	default:
		m0_free(fom_obj);
		return M0_ERR(-EOPNOTSUPP);
	}

	rep_fop = m0_fop_reply_alloc(fop, rep_fopt);
	if (rep_fop == NULL) {
		m0_free(fom_obj);
		return M0_ERR(-ENOMEM);
	}
	fom = &fom_obj->fm_fom;
	m0_fom_init(fom, &fop->f_type->ft_fom_type, ops, fop, rep_fop, reqh);
	*m = fom;
	return 0;
}

static int md_locate(struct m0_mdstore *md, struct m0_fid *tfid,
		     struct m0_cob **cob)
{
	int rc;

	rc = m0_mdstore_locate(md, tfid, cob, M0_MD_LOCATE_STORED);
	if (rc != 0)
		M0_LOG(M0_DEBUG, "m0_mdstore_locate() failed for "FID_F" (%d)",
		       FID_P(tfid), rc);
	return M0_RC(rc);
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
