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
 * Original author: Amit Jambure <amit_jambure@xyratex.com>
 * Revision       : Anand Vidwansa <anand_vidwansa@xyratex.com>
 * Metadata       : Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 10/14/2011
 */

#include <linux/version.h> /* LINUX_VERSION_CODE */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <linux/uidgid.h>  /* from_kuid */
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
#include <linux/xattr.h>
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"

#include "lib/misc.h"      /* M0_SET0 */
#include "lib/memory.h"    /* M0_ALLOC_PTR */
#include "lib/bob.h"
#include "fop/fop.h"       /* m0_fop_alloc */
#include "rpc/rpclib.h"    /* m0_rpc_post_sync */
#include "rpc/rpc_opcodes.h"
#include "conf/helpers.h"  /* m0_confc_root_open */
#include "mero/magic.h"
#include "layout/layout.h"
#include "layout/layout_internal.h" /* LID_NONE */
#include "layout/pdclust.h"
#include "m0t1fs/linux_kernel/m0t1fs.h"
#include "m0t1fs/linux_kernel/fsync.h"
#include "pool/pool.h"
#include "ioservice/fid_convert.h"  /* m0_fid_convert_gob2cob */

extern const struct m0_uint128 m0_rm_m0t1fs_group;
/**
 * Cob create/delete fop send deadline (in ns).
 * Used to utilize rpc formation when too many fops
 * are sending to one ioservice.
 */
enum {COB_REQ_DEADLINE = 2000000};

struct cob_req {
	struct m0_semaphore cr_sem;
	m0_time_t           cr_deadline;
	struct m0t1fs_sb   *cr_csb;
	int32_t             cr_rc;
	struct m0_fid       cr_fid;
	struct m0_fid       cr_pver;
};

struct cob_fop {
	struct m0_fop   c_fop;
	struct cob_req *c_req;
};

static void cob_rpc_item_cb(struct m0_rpc_item *item)
{
	int                              rc;
	struct m0_fop                   *fop;
	struct cob_fop                  *cfop;
	struct cob_req                  *creq;
	struct m0_fop_cob_op_reply      *reply;
	struct m0_fop_cob_op_rep_common *r_common;

	M0_PRE(item != NULL);

	fop  = m0_rpc_item_to_fop(item);
	M0_ENTRY("rpc_item %p[%u], item->ri_error %d", item,
		 m0_fop_opcode(fop), item->ri_error);
	cfop = container_of(fop, struct cob_fop, c_fop);
	creq = cfop->c_req;

	rc = m0_rpc_item_error(item);
	if (rc != 0)
		goto out;

	M0_ASSERT(m0_is_cob_create_fop(fop) || m0_is_cob_delete_fop(fop) ||
		  m0_is_cob_truncate_fop(fop) || m0_is_cob_setattr_fop(fop));
	reply = m0_fop_data(m0_rpc_item_to_fop(fop->f_item.ri_reply));
	r_common = &reply->cor_common;

	M0_LOG(M0_DEBUG, "%p[%u], item->ri_error %d, reply->cor_rc %d", item,
	       item->ri_type->rit_opcode, item->ri_error, reply->cor_rc);
	rc = reply->cor_rc;
out:
	if (creq->cr_rc == 0)
		creq->cr_rc = rc;
	M0_LOG(M0_DEBUG, "%p[%u] ref %llu, cob_req_fop %p, cr_rc %d, "FID_F,
	       item, m0_fop_opcode(fop),
	       (unsigned long long)m0_ref_read(&fop->f_ref), cfop, creq->cr_rc,
	       FID_P(&creq->cr_fid));
	m0_semaphore_up(&creq->cr_sem);
	M0_LEAVE();
}

static const struct m0_rpc_item_ops cob_item_ops = {
	.rio_replied = cob_rpc_item_cb,
};

M0_INTERNAL void m0t1fs_inode_bob_init(struct m0t1fs_inode *bob);
M0_INTERNAL bool m0t1fs_inode_bob_check(struct m0t1fs_inode *bob);


static int file_lock_acquire(struct m0_rm_incoming *rm_in,
			     struct m0t1fs_inode *ci);
static void file_lock_release(struct m0_rm_incoming *rm_in);
static int m0t1fs_component_objects_op(struct m0t1fs_inode *ci,
				       struct m0t1fs_mdop *mop,
				       int (*func)(struct cob_req *,
						   const struct m0t1fs_inode *,
						   const struct m0t1fs_mdop *,
						   int idx));

static int m0t1fs_ios_cob_create(struct cob_req *cr,
				 const struct m0t1fs_inode *inode,
				 const struct m0t1fs_mdop *mop,
				 int idx);

static int m0t1fs_ios_cob_delete(struct cob_req *cr,
				 const struct m0t1fs_inode *inode,
				 const struct m0t1fs_mdop *mop,
				 int idx);

static int m0t1fs_ios_cob_setattr(struct cob_req *cr,
				  const struct m0t1fs_inode *inode,
				  const struct m0t1fs_mdop *mop,
				  int idx);
static int m0t1fs_ios_cob_truncate(struct cob_req *cr,
				   const struct m0t1fs_inode *inode,
			           const struct m0t1fs_mdop *mop,
				   int idx);

static int name_mem2wire(struct m0_fop_str *tgt,
			 const struct m0_buf *name)
{
	tgt->s_buf = m0_alloc(name->b_nob);
	if (tgt->s_buf == NULL)
		return M0_ERR(-ENOMEM);
	memcpy(tgt->s_buf, name->b_addr, (int)name->b_nob);
	tgt->s_len = name->b_nob;
	return 0;
}

static void body_mem2wire(struct m0_fop_cob *body,
			  const struct m0_cob_attr *attr,
			  int valid)
{
	body->b_pfid = attr->ca_pfid;
	body->b_tfid = attr->ca_tfid;
	if (valid & M0_COB_ATIME)
		body->b_atime = attr->ca_atime;
	if (valid & M0_COB_CTIME)
		body->b_ctime = attr->ca_ctime;
	if (valid & M0_COB_MTIME)
		body->b_mtime = attr->ca_mtime;
	if (valid & M0_COB_BLOCKS)
		body->b_blocks = attr->ca_blocks;
	if (valid & M0_COB_SIZE)
		body->b_size = attr->ca_size;
	if (valid & M0_COB_MODE)
		body->b_mode = attr->ca_mode;
	if (valid & M0_COB_UID)
		body->b_uid = attr->ca_uid;
	if (valid & M0_COB_GID)
		body->b_gid = attr->ca_gid;
	if (valid & M0_COB_BLOCKS)
		body->b_blocks = attr->ca_blocks;
	if (valid & M0_COB_NLINK)
		body->b_nlink = attr->ca_nlink;
	if (valid & M0_COB_LID)
		body->b_lid = attr->ca_lid;
	if (valid & M0_COB_PVER)
		body->b_pver = attr->ca_pver;
	body->b_valid = valid;
}

static void body_wire2mem(struct m0_cob_attr *attr,
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

M0_INTERNAL int m0t1fs_fs_conf_lock(struct m0t1fs_sb *csb)
{
	int rc;

	M0_ENTRY("csb=%p", csb);
	rc = m0t1fs_ref_get_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);
	m0t1fs_fs_lock(csb);
	return M0_RC(0);
}

M0_INTERNAL void m0t1fs_fs_conf_unlock(struct m0t1fs_sb *csb)
{
	m0t1fs_fs_unlock(csb);
	m0t1fs_ref_put_lock(csb);
}
/**
   Allocate fid of global file.

   See "Containers and component objects" section in m0t1fs.h for
   more information.

   XXX temporary.
 */
void m0t1fs_fid_alloc(struct m0t1fs_sb *csb, struct m0_fid *out)
{
	M0_PRE(m0t1fs_fs_is_locked(csb));

	m0_fid_set(out, 0, csb->csb_next_key++);

	M0_LOG(M0_DEBUG, "fid "FID_F, FID_P(out));
}

/**
 * Given a fid of an existing file, update "fid allocator" so that this fid is
 * not given out to another file.
 */
void m0t1fs_fid_accept(struct m0t1fs_sb *csb, const struct m0_fid *fid)
{
	M0_PRE(m0t1fs_fs_is_locked(csb));

	csb->csb_next_key = max64(csb->csb_next_key, fid->f_key + 1);
}

int m0t1fs_inode_set_layout_id(struct m0t1fs_inode *ci,
				struct m0t1fs_mdop *mo,
				int layout_id)
{
	struct m0t1fs_sb           *csb = M0T1FS_SB(ci->ci_inode.i_sb);
	struct m0_fop              *rep_fop = NULL;
	int                         rc;

	/*
	 * Layout can be changed only for the freshly
	 * created file which does not contain any data yet.
	 */
	if (ci->ci_inode.i_size != 0) {
		rc = -EEXIST;
		return rc;
	}

	if (layout_id == LID_NONE) {
		rc = -EINVAL;
		return rc;
	}

	if (layout_id == ci->ci_layout_id)
		return 0;

	ci->ci_layout_id = layout_id;

	rc = m0t1fs_inode_layout_init(ci);
	if (rc != 0)
		return rc;

	mo->mo_attr.ca_lid = ci->ci_layout_id;
	mo->mo_attr.ca_valid |= M0_COB_LID;

	if (!csb->csb_oostore) {
		rc = m0t1fs_mds_cob_setattr(csb, mo, &rep_fop);
		m0_fop_put0_lock(rep_fop);
		goto out;
	}
	M0_LOG(M0_DEBUG, "Changing lid to %lld", mo->mo_attr.ca_lid);
	rc = m0t1fs_component_objects_op(ci, mo, m0t1fs_ios_cob_setattr);
out:
	if (rc == 0)
		ci->ci_layout_changed = true;
	return rc;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
int m0t1fs_setxattr(const struct xattr_handler *handler,
		    struct dentry *dentry, struct inode *inode,
		    const char *name, const void *value,
		    size_t size, int flags)
#else
int m0t1fs_setxattr(struct dentry *dentry, const char *name,
		    const void *value, size_t size, int flags)
#endif
{
	struct m0t1fs_inode *ci = M0T1FS_I(dentry->d_inode);
	struct m0t1fs_sb    *csb = M0T1FS_SB(ci->ci_inode.i_sb);
	int                  layout_id;
	struct m0t1fs_mdop   mo;
	int                  rc;
	struct m0_fop       *rep_fop = NULL;

	M0_THREAD_ENTER;

	M0_ENTRY("Setting %.*s's xattr %s=%.*s", dentry->d_name.len,
		 (char*)dentry->d_name.name, name, (int)size, (char *)value);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	if (value == NULL)
		return m0t1fs_removexattr(dentry, name);
#endif
	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);
	M0_SET0(&mo);
	mo.mo_attr.ca_tfid = *m0t1fs_inode_fid(ci);
	m0_buf_init(&mo.mo_attr.ca_name, (char*)dentry->d_name.name,
		    dentry->d_name.len);
	mo.mo_attr.ca_pver = ci->ci_pver;

	if (m0_streq(name, "lid")) {
		char *endp;
		char  buf[40];

		rc = -EINVAL;
		if (value == NULL || size >= ARRAY_SIZE(buf))
			goto out;
		memcpy(buf, value, size);
		buf[size] = '\0';
		layout_id = simple_strtoul(buf, &endp, 0);
		if (endp - buf < size || layout_id == LID_NONE)
			goto out;

		rc = m0t1fs_inode_set_layout_id(ci, &mo, layout_id);
		if (rc != 0)
			goto out;
	} else if (m0_streq(name, "writesize")) {
		/* parse `single_write_size[;total_file_size]' value(s) */
		/* TODO: total_file_size is not used yet */
		size_t buffsize = 0;
		size_t filesize = 0;
		char *bsptr, *wsptr, *endp;
		char  buf[40], *ptr;

		rc = -EINVAL;
		if (value == NULL || size >= ARRAY_SIZE(buf))
			goto out;

		memcpy(buf, value, size);
		buf[size] = '\0';
		ptr = buf;

		bsptr = strsep(&ptr, ";");
		if (bsptr != NULL)
			buffsize = simple_strtoul(bsptr, &endp, 0);
		M0_LOG(M0_DEBUG, "New IO buffsize: %d.", (int)buffsize);

		wsptr = strsep(&ptr, ";");
		if (wsptr != NULL) {
			filesize = simple_strtoul(wsptr, &endp, 0);
			M0_LOG(M0_DEBUG, "Total IO size: %d.", (int)filesize);
		}

		/* Find optimal lid and set it to the inode.*/
		layout_id = m0_layout_find_by_buffsize(&csb->csb_reqh.rh_ldom,
							&ci->ci_pver, buffsize);
		rc = m0t1fs_inode_set_layout_id(ci, &mo, layout_id);
	} else {
		if (csb->csb_oostore) {
			rc = -EOPNOTSUPP;
			goto out;
		}
		m0_buf_init(&mo.mo_attr.ca_eakey, (void*)name, strlen(name));
		m0_buf_init(&mo.mo_attr.ca_eaval, (void*)value, size);
		rc = m0t1fs_mds_cob_setxattr(csb, &mo, &rep_fop);
		m0_fop_put0_lock(rep_fop);
	}
out:
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
int m0t1fs_fid_getxattr(const struct xattr_handler *handler,
			struct dentry *dentry, struct inode *inode,
			const char *name, void *buffer,
			size_t size)
#else
ssize_t m0t1fs_fid_getxattr(struct dentry *dentry, const char *name,
			    void *buffer, size_t size)
#endif
{
	M0_THREAD_ENTER;
	return M0_ERR(-EOPNOTSUPP);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
int m0t1fs_getxattr(const struct xattr_handler *handler,
		    struct dentry *dentry, struct inode *inode,
		    const char *name, void *buffer,
		    size_t size)
#else
ssize_t m0t1fs_getxattr(struct dentry *dentry, const char *name,
			void *buffer, size_t size)
#endif
{
	struct m0t1fs_inode        *ci = M0T1FS_I(dentry->d_inode);
	struct m0t1fs_sb           *csb = M0T1FS_SB(ci->ci_inode.i_sb);
	struct m0_fop_getxattr_rep *rep = NULL;
	struct m0t1fs_mdop          mo;
	int                         rc;
	struct m0_fop              *rep_fop = NULL;
	M0_THREAD_ENTER;

	M0_ENTRY("Getting %.*s's xattr %s", dentry->d_name.len,
		 (char*)dentry->d_name.name, name);
	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);
	if (m0_streq(name, "pver")) {
		M0_LOG(M0_DEBUG, "buffer:%p size:%d"FID_F, buffer, (int)size,
				FID_P(&ci->ci_pver));
		if (buffer != NULL) {
			if ((size_t)sizeof(struct m0_fid) > size) {
				rc = M0_ERR(-ERANGE);
				goto out;
			}
			sprintf(buffer, FID_F, FID_P(&ci->ci_pver));
			rc = strlen(buffer);
			goto out;
		}
		rc = M0_FID_STR_LEN;
		goto out;
	}
	if (m0_streq(name, "lid")) {
		M0_LOG(M0_DEBUG, "buffer:%p size:%d lid:%d"FID_F, buffer,
				(int)size, (int)ci->ci_layout_id,
				FID_P(&ci->ci_pver));
		if (buffer != NULL) {
			if ((size_t)sizeof(uint64_t) > size) {
				rc = M0_ERR(-ERANGE);
				goto out;
			}
			sprintf(buffer, "%d", (int)ci->ci_layout_id);
			rc = strlen(buffer);
			goto out;
		}
		rc = UINT32_STR_LEN;
		goto out;
	}
	if (csb->csb_oostore) {
		rc = M0_ERR(-EOPNOTSUPP);
		goto out;
	}

	M0_SET0(&mo);
	mo.mo_attr.ca_tfid = *m0t1fs_inode_fid(ci);
	m0_buf_init(&mo.mo_attr.ca_name, (char*)dentry->d_name.name,
		    dentry->d_name.len);
	m0_buf_init(&mo.mo_attr.ca_eakey, (char*)name, strlen(name));
	mo.mo_attr.ca_pver = ci->ci_pver;

	rc = m0t1fs_mds_cob_getxattr(csb, &mo, &rep_fop);
	if (rc == 0) {
		rep = m0_fop_data(rep_fop);
		if (buffer != NULL) {
			if ((size_t)rep->g_value.s_len > size) {
				rc = M0_ERR(-ERANGE);
				goto out;
			}
			memcpy(buffer, rep->g_value.s_buf, rep->g_value.s_len);
		}
		rc = rep->g_value.s_len; /* return xattr length */
	} else if (rc == -ENOENT)
		rc = M0_ERR(-ENODATA);
out:
	m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

ssize_t m0t1fs_fid_listxattr(struct dentry *dentry, char *buffer, size_t size)
{
	M0_THREAD_ENTER;
	return M0_ERR(-EOPNOTSUPP);
}

ssize_t m0t1fs_listxattr(struct dentry *dentry, char *buffer, size_t size)
{
	M0_THREAD_ENTER;
	return M0_ERR(-EOPNOTSUPP);
}

int m0t1fs_fid_removexattr(struct dentry *dentry, const char *name)
{
	M0_THREAD_ENTER;
	return M0_ERR(-EOPNOTSUPP);
}

int m0t1fs_removexattr(struct dentry *dentry, const char *name)
{
	struct m0t1fs_inode        *ci = M0T1FS_I(dentry->d_inode);
	struct m0t1fs_sb           *csb = M0T1FS_SB(ci->ci_inode.i_sb);
	struct m0t1fs_mdop          mo;
	int                         rc;
	struct m0_fop              *rep_fop;

	M0_THREAD_ENTER;
	M0_ENTRY("Deleting %.*s's xattr %s", dentry->d_name.len,
		 (char*)dentry->d_name.name, name);

	if (csb->csb_oostore)
		return M0_RC(-EOPNOTSUPP);
	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);

	M0_SET0(&mo);
	mo.mo_attr.ca_tfid = *m0t1fs_inode_fid(ci);
	m0_buf_init(&mo.mo_attr.ca_name, (char*)dentry->d_name.name,
		    dentry->d_name.len);
	m0_buf_init(&mo.mo_attr.ca_eakey, (char*)name, strlen(name));

	rc = m0t1fs_mds_cob_delxattr(csb, &mo, &rep_fop);
	if (rc == -ENOENT)
		rc = M0_ERR(-ENODATA);
	m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int m0t1fs_fid_create(struct inode     *dir,
			     struct dentry    *dentry,
			     umode_t           mode,
			     bool              excl)
#else
static int m0t1fs_fid_create(struct inode     *dir,
			     struct dentry    *dentry,
			     int               mode,
			     struct nameidata *nd)
#endif
{
	return M0_ERR(-EOPNOTSUPP);
}

static int m0t1fs_fid_check(struct m0_fid *fid)
{
	if ((fid->f_container & ~M0_FID_GOB_CONTAINER_MASK) != 0) {
		M0_LOG(M0_ERROR, "Invalid gob fid detected: "FID_F". Container "
		       "component should not be longer than 32bit.",
		       FID_P(fid));
		return -EINVAL;
	}
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int m0t1fs_create(struct inode     *dir,
			 struct dentry    *dentry,
			 umode_t           mode,
			 bool              excl)
#else
static int m0t1fs_create(struct inode     *dir,
			 struct dentry    *dentry,
			 int               mode,
			 struct nameidata *nd)
#endif
{
	struct super_block       *sb  = dir->i_sb;
	struct m0t1fs_sb         *csb = M0T1FS_SB(sb);
	struct m0t1fs_inode      *ci;
	struct m0t1fs_mdop        mo;
	struct inode             *inode;
	struct m0_fid             new_fid;
	struct m0_fid             gfid;
	int                       rc;
	struct m0_fop            *rep_fop = NULL;
	struct m0_pool_version   *pv;
	bool                      i_err = false;

	M0_THREAD_ENTER;
	M0_ENTRY();

	M0_LOG(M0_DEBUG, "Creating \"%s\" in pdir %lu "FID_F,
	       (char*)dentry->d_name.name, dir->i_ino,
	       FID_P(m0t1fs_inode_fid(M0T1FS_I(dir))));

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);
	rc = m0_pool_version_get(&csb->csb_pools_common, NULL, &pv);
	if (rc != 0)
		goto out;

	inode = new_inode(sb);
	if (inode == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto out;
	}
	if (csb->csb_oostore) {
		rc = m0_fid_sscanf(dentry->d_name.name, &new_fid);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "Cannot parse fid \"%s\" in oostore",
					 (char*)dentry->d_name.name);
			i_err = true;
			goto out;
		}
	} else {
		m0t1fs_fid_alloc(csb, &new_fid);
	}
	rc = m0t1fs_fid_check(&new_fid);
	if (rc != 0) {
		i_err = true;
		goto out;
	}

	m0_fid_gob_make(&gfid, new_fid.f_container, new_fid.f_key);
	M0_LOG(M0_DEBUG, "New fid = "FID_F, FID_P(&new_fid));
	inode->i_ino = m0_fid_hash(&gfid);
	ci = M0T1FS_I(inode);
	ci->ci_fid = gfid;

	inode->i_mode = mode;
	inode->i_uid = current_fsuid();
	if (dir->i_mode & S_ISGID) {
		inode->i_gid = dir->i_gid;
		if (S_ISDIR(mode))
			inode->i_mode |= S_ISGID;
	} else
		inode->i_gid = current_fsgid();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	inode->i_mtime  = inode->i_atime = inode->i_ctime = current_time(inode);
#else
	inode->i_mtime  = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;
#endif
	inode->i_blocks = 0;
	if (S_ISDIR(mode)) {
		inode->i_op  = &m0t1fs_dir_inode_operations;
		inode->i_fop = &m0t1fs_dir_file_operations;
		inc_nlink(inode);  /* one more link (".") for directories */
	} else {
		inode->i_op             = &m0t1fs_reg_inode_operations;
		inode->i_fop            = &m0t1fs_reg_file_operations;
		inode->i_mapping->a_ops = &m0t1fs_aops;
	}

	ci->ci_pver = pv->pv_id;
	M0_LOG(M0_INFO, "Creating \"%s\" with pool version "FID_F,
	       (char*)dentry->d_name.name, FID_P(&ci->ci_pver));
	/* layout id for new file */
        ci->ci_layout_id = M0_DEFAULT_LAYOUT_ID;

	m0t1fs_file_lock_init(ci, csb);
	rc = m0t1fs_inode_layout_init(ci);
	if (rc != 0) {
		i_err = true;
		goto out;
	}

	M0_SET0(&mo);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	mo.mo_attr.ca_uid       = from_kuid(current_user_ns(), inode->i_uid);
	mo.mo_attr.ca_gid       = from_kgid(current_user_ns(), inode->i_gid);
#else
	mo.mo_attr.ca_uid       = inode->i_uid;
	mo.mo_attr.ca_gid       = inode->i_gid;
#endif
	mo.mo_attr.ca_atime     = inode->i_atime.tv_sec;
	mo.mo_attr.ca_ctime     = inode->i_ctime.tv_sec;
	mo.mo_attr.ca_mtime     = inode->i_mtime.tv_sec;
	mo.mo_attr.ca_mode      = inode->i_mode;
	mo.mo_attr.ca_blocks    = inode->i_blocks;
	mo.mo_attr.ca_pfid      = *m0t1fs_inode_fid(M0T1FS_I(dir));
	mo.mo_attr.ca_tfid      = *m0t1fs_inode_fid(ci);
	mo.mo_attr.ca_pver      = ci->ci_pver;
	mo.mo_attr.ca_lid       = ci->ci_layout_id;
	mo.mo_attr.ca_nlink     = inode->i_nlink;
	mo.mo_attr.ca_valid     = (M0_COB_UID    | M0_COB_GID   | M0_COB_ATIME |
				   M0_COB_CTIME  | M0_COB_MTIME | M0_COB_MODE  |
				   M0_COB_BLOCKS | M0_COB_SIZE  | M0_COB_LID   |
				   M0_COB_NLINK  | M0_COB_PVER);
	m0_buf_init(&mo.mo_attr.ca_name, (char*)dentry->d_name.name,
		    dentry->d_name.len);

	if (!csb->csb_oostore) {
		rc = m0t1fs_mds_cob_create(csb, &mo, &rep_fop);
		if (rc != 0) {
			i_err = true;
			goto out;
		}
	}
	if (S_ISREG(mode) && csb->csb_oostore) {
		rc = m0t1fs_component_objects_op(ci, &mo, m0t1fs_ios_cob_create);
		if (rc != 0) {
			i_err = true;
			goto out;
		}
	}

	if (insert_inode_locked4(inode, inode->i_ino,
				 &m0t1fs_inode_test, &gfid) < 0) {
		M0_LOG(M0_ERROR, "Duplicate inode: "FID_F, FID_P(&gfid));
		rc = M0_ERR(-EIO);
		i_err = true;
		goto out;
	}
	m0_mutex_lock(&csb->csb_inodes_lock);
	csb_inodes_tlist_add_tail(&csb->csb_inodes, ci);
	m0_mutex_unlock(&csb->csb_inodes_lock);
	unlock_new_inode(inode);
	mark_inode_dirty(dir);
	d_instantiate(dentry, inode);

out:
	m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	if (i_err) {
		clear_nlink(inode);
		make_bad_inode(inode);
		iput(inode);
	}
	M0_ADDB2_ADD(M0_AVI_FS_CREATE,
		     new_fid.f_container, new_fid.f_key, mode, rc);
	return M0_RC(rc);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int m0t1fs_fid_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
#else
static int m0t1fs_fid_mkdir(struct inode *dir, struct dentry *dentry, int mode)
#endif
{
	M0_THREAD_ENTER;
	return m0t1fs_fid_create(dir, dentry, mode | S_IFDIR, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int m0t1fs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
#else
static int m0t1fs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
#endif
{
	M0_THREAD_ENTER;
	return m0t1fs_create(dir, dentry, mode | S_IFDIR, NULL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static struct dentry *m0t1fs_lookup(struct inode     *dir,
				    struct dentry    *dentry,
				    unsigned int      flags)
#else
static struct dentry *m0t1fs_lookup(struct inode     *dir,
				    struct dentry    *dentry,
				    struct nameidata *nd)
#endif
{
	struct m0t1fs_sb         *csb;
	struct inode             *inode = NULL;
	struct m0_fop_lookup_rep *rep = NULL;
	struct m0t1fs_mdop        mo;
	int                       rc;
	void                     *err_ptr = NULL;
	struct m0_fop            *rep_fop = NULL;
	bool                      dcache_splice = false;
	bool                      i_err = false;

	M0_THREAD_ENTER;
	M0_ENTRY();

	m0_addb2_add(M0_AVI_FS_LOOKUP, M0_ADDB2_OBJ(&M0T1FS_I(dir)->ci_fid));
	csb = M0T1FS_SB(dir->i_sb);

	if (dentry->d_name.len > csb->csb_namelen) {
		M0_LEAVE("ERR_PTR: %p", ERR_PTR(-ENAMETOOLONG));
		return ERR_PTR(-ENAMETOOLONG);
	}

	M0_LOG(M0_INFO, "Name: \"%s\"", (char*)dentry->d_name.name);
	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return ERR_PTR(rc);
	if (csb->csb_oostore) {
		struct m0_fid           new_fid;
		struct m0_fid           gfid;
		struct m0_fop_cob       body;
		struct m0_pool_version *pv;

		rc = m0_fid_sscanf(dentry->d_name.name, &new_fid);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "Cannot parse fid \"%s\" in oostore",
					 (char*)dentry->d_name.name);
			err_ptr = ERR_PTR(-EINVAL);
			goto out;
		}
		rc = m0t1fs_fid_check(&new_fid);
		if (rc != 0) {
			err_ptr = ERR_PTR(-EINVAL);
			goto out;
		}
		m0_fid_gob_make(&gfid, new_fid.f_container, new_fid.f_key);
		body.b_valid = (M0_COB_MODE | M0_COB_LID);
		body.b_lid = M0_DEFAULT_LAYOUT_ID;
		body.b_mode = S_IFREG;
		rc = m0_pool_version_get(&csb->csb_pools_common, NULL, &pv);
		if (rc != 0) {
			M0_LEAVE("ERROR: rc:%d", rc);
			err_ptr = ERR_PTR(rc);
			goto out;
		}
		body.b_pver = pv->pv_id;
		inode = m0t1fs_iget(dir->i_sb, &gfid, &body);
		if (IS_ERR(inode)) {
			M0_LEAVE("ERROR: %p", ERR_CAST(inode));
			err_ptr =  ERR_CAST(inode);
			goto out;
		}
		rc = m0t1fs_cob_getattr(inode);
		if (rc != 0) {
			M0_LEAVE("rc:%d", rc);
			i_err = true;
			goto out;
		}
		dcache_splice = true;
		goto out;
	}

	M0_SET0(&mo);
	mo.mo_attr.ca_pfid = *m0t1fs_inode_fid(M0T1FS_I(dir));
	m0_buf_init(&mo.mo_attr.ca_name, (char*)dentry->d_name.name,
		    dentry->d_name.len);
	rc = m0t1fs_mds_cob_lookup(csb, &mo, &rep_fop);
	if (rc != 0) {
		M0_LEAVE("rc:%d", rc);
		goto out;
	}
	rep = m0_fop_data(rep_fop);
	mo.mo_attr.ca_tfid = rep->l_body.b_tfid;
	inode = m0t1fs_iget(dir->i_sb, &mo.mo_attr.ca_tfid,
			    &rep->l_body);
	if (IS_ERR(inode)) {
		M0_LEAVE("ERROR: %p", ERR_CAST(inode));
		err_ptr =  ERR_CAST(inode);
		goto out;
	}
	dcache_splice = true;
out:
	m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	if (i_err) {
		make_bad_inode(inode);
		iput(inode);
	}
	M0_LEAVE();

	return dcache_splice ? d_splice_alias(inode, dentry) : err_ptr;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static struct dentry *m0t1fs_fid_lookup(struct inode     *dir,
					struct dentry    *dentry,
					unsigned int      flags)
#else
static struct dentry *m0t1fs_fid_lookup(struct inode     *dir,
					struct dentry    *dentry,
					struct nameidata *nd)
#endif
{
	struct m0_fid fid;
	int rc;

	M0_THREAD_ENTER;
	rc = m0_fid_sscanf(dentry->d_name.name, &fid);
	if (rc != 0) {
		M0_LEAVE("Cannot parse fid \"%s\"", (char*)dentry->d_name.name);
		return ERR_PTR(rc);
	}
	m0_fid_tassume(&fid, &m0_file_fid_type);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	return m0t1fs_lookup(dir, dentry, flags);
#else
	return m0t1fs_lookup(dir, dentry, nd);
#endif
}


struct m0_dirent *dirent_next(struct m0_dirent *ent)
{
	struct m0_dirent *dent = NULL;

	if (ent->d_reclen > 0)
		dent = (struct m0_dirent *)((char*)ent + ent->d_reclen);

	return dent;
}

struct m0_dirent *dirent_first(struct m0_fop_readdir_rep *rep)
{
	struct m0_dirent *ent = (struct m0_dirent *)rep->r_buf.b_addr;
	return ent->d_namelen > 0 ? ent : NULL;
}

static int m0t1fs_opendir(struct inode *inode, struct file *file)
{
	struct m0t1fs_filedata *fd;
	M0_THREAD_ENTER;
	M0_ENTRY();

	M0_ALLOC_PTR(fd);
	if (fd == NULL)
		return M0_ERR(-ENOMEM);
	file->private_data = fd;

	/** Setup readdir initial pos with "." and max possible namelen. */
	fd->fd_dirpos = m0_alloc(M0T1FS_SB(inode->i_sb)->csb_namelen);
	if (fd->fd_dirpos == NULL) {
		m0_free(fd);
		return M0_ERR(-ENOMEM);
	}
	m0_bitstring_copy(fd->fd_dirpos, ".", 1);
	fd->fd_direof    = 0;
	fd->fd_mds_index = 0;
	return 0;
}

static int m0t1fs_fid_opendir(struct inode *inode, struct file *file)
{
	M0_THREAD_ENTER;
	return m0t1fs_opendir(inode, file);
}

static int m0t1fs_releasedir(struct inode *inode, struct file *file)
{
	struct m0t1fs_filedata *fd = file->private_data;
	M0_THREAD_ENTER;
	M0_ENTRY();

	m0_free(fd->fd_dirpos);
	m0_free0(&file->private_data);
	return 0;
}

static int m0t1fs_fid_releasedir(struct inode *inode, struct file *file)
{
	M0_THREAD_ENTER;
	return m0t1fs_releasedir(inode, file);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
static int m0t1fs_fid_readdir(struct file *f, struct dir_context *ctx)
{
	M0_THREAD_ENTER;
	return M0_RC(0);
}
#else
static int m0t1fs_fid_readdir(struct file *f,
			      void        *buf,
			      filldir_t    filldir)
{
	M0_THREAD_ENTER;
	return M0_RC(0);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
static int m0t1fs_readdir(struct file *f, struct dir_context *ctx)
#else
static int m0t1fs_readdir(struct file *f,
			  void        *buf,
			  filldir_t    filldir)
#endif
{
	struct m0t1fs_inode       *ci;
	struct m0t1fs_mdop         mo;
	struct m0t1fs_sb          *csb;
	struct m0_fop_readdir_rep *rep;
	struct dentry             *dentry;
	struct inode              *dir;
	struct m0_dirent          *ent;
	struct m0t1fs_filedata    *fd;
	int                        type;
	ino_t                      ino;
	int                        i;
	int                        rc;
	int                        over;
	bool                       dot_filled = false;
	bool                       dotdot_filled = false;
	struct m0_fop             *rep_fop;

	M0_THREAD_ENTER;
	M0_ENTRY();

	dentry = f->f_path.dentry;
	dir    = dentry->d_inode;
	ci     = M0T1FS_I(dir);
	csb    = M0T1FS_SB(dir->i_sb);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
	i      = ctx->pos;
#else
	i      = f->f_pos;
#endif

	if (csb->csb_oostore)
		return M0_RC(0);
	fd = f->private_data;
	if (fd->fd_direof)
		return M0_RC(0);

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);

switch_mds:
	M0_SET0(&mo);
	/**
	   In case that readdir will be interrupted by enomem situation
	   (filldir fails) on big dir and finishes before eof is reached,
	   it has chance to start from there. This way f->f_pos and string
	   readdir pos are in sync.
	 */
	m0_buf_init(&mo.mo_attr.ca_name, m0_bitstring_buf_get(fd->fd_dirpos),
		    m0_bitstring_len_get(fd->fd_dirpos));
	mo.mo_attr.ca_tfid = *m0t1fs_inode_fid(ci);
	mo.mo_use_hint  = true;
	mo.mo_hash_hint = fd->fd_mds_index;

	do {
		M0_LOG(M0_DEBUG, "readdir from position \"%*s\"@ mds%d",
		       (int)mo.mo_attr.ca_name.b_nob,
		       (char *)mo.mo_attr.ca_name.b_addr,
			mo.mo_hash_hint);

		rc = m0t1fs_mds_cob_readdir(csb, &mo, &rep_fop);
		rep = m0_fop_data(rep_fop);
		if (rc < 0) {
			M0_LOG(M0_ERROR,
			       "Failed to read dir from pos \"%*s\". Error %d",
			       (int)mo.mo_attr.ca_name.b_nob,
			       (char *)mo.mo_attr.ca_name.b_addr, rc);
			goto out;
		}

		for (ent = dirent_first(rep); ent != NULL;
		     ent = dirent_next(ent)) {
			if (ent->d_namelen == 1 &&
			    memcmp(ent->d_name, ".", 1) == 0) {
				if (dot_filled)
					continue;
				ino = dir->i_ino;
				type = DT_DIR;
				dot_filled = true;
			} else if (ent->d_namelen == 2 &&
				   memcmp(ent->d_name, "..", 2) == 0) {
				if (dotdot_filled)
					continue;
				ino = parent_ino(dentry);
				type = DT_DIR;
				dotdot_filled = true;
			} else {
				/**
				 * TODO: Entry type is unknown and ino is
				 * pretty random, should be fixed later.
				 */
				ino = ++i;
				type = DT_UNKNOWN;
			}

			M0_LOG(M0_DEBUG, "filled off %lu ino %lu name"
			       " \"%.*s\", ino %lu, type %d",
			       (unsigned long)f->f_pos, (unsigned long)ino,
			       ent->d_namelen, (char *)ent->d_name,
			       ino, type);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
			over = dir_emit(ctx, ent->d_name, ent->d_namelen,
					ino, type);
			if (!over) {
				rc = 0;
				M0_LOG(M0_DEBUG, "full");
				goto out;
			}
			ctx->pos++;
#else
			over = filldir(buf, ent->d_name, ent->d_namelen,
				       f->f_pos, ino, type);
			if (over) {
				rc = 0;
				M0_LOG(M0_DEBUG, "full");
				goto out;
			}
			f->f_pos++;
#endif
		}
		m0_bitstring_copy(fd->fd_dirpos, rep->r_end.s_buf,
				  rep->r_end.s_len);
		m0_buf_init(&mo.mo_attr.ca_name,
			    m0_bitstring_buf_get(fd->fd_dirpos),
			    m0_bitstring_len_get(fd->fd_dirpos));

		M0_LOG(M0_DEBUG, "set position to \"%*s\" rc == %d",
		       (int)mo.mo_attr.ca_name.b_nob,
		       (char *)mo.mo_attr.ca_name.b_addr, rc);
		m0_fop_put0_lock(rep_fop);
		/*
		 * Return codes for m0t1fs_mds_cob_readdir() are the following:
		 * - <0 - some error occured;
		 * - >0 - EOF signaled by mdservice, some number of entries
		 *        available;
		 * -  0 - no error, some number of entries is sent by mdservice.
		 */
	} while (rc == 0);
	/*
	 * EOF from one mdservice is detected. Switch to another mds.
	 */
	if (++fd->fd_mds_index < csb->csb_pools_common.pc_nr_svcs[M0_CST_MDS]) {
		m0_bitstring_copy(fd->fd_dirpos, ".", 1);
		M0_LOG(M0_DEBUG, "switch to mds %d", fd->fd_mds_index);
		goto switch_mds;
	}

	/*
	 * EOF detected from all mds. Set return code to 0 to make vfs happy.
	 */
	fd->fd_direof = 1;
	rc = 0;
out:
	if (rc != 0)
		m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

static int m0t1fs_fid_link(struct dentry *old, struct inode *dir,
			   struct dentry *new)
{
	M0_THREAD_ENTER;
	return M0_ERR(-EOPNOTSUPP);
}

static int m0t1fs_link(struct dentry *old, struct inode *dir,
		       struct dentry *new)
{
	struct m0t1fs_sb    *csb;
	struct m0t1fs_mdop   mo;
	struct m0t1fs_inode *ci;
	struct inode        *inode = old->d_inode;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	struct timespec64    now = current_time(inode);
#else
	struct timespec      now = CURRENT_TIME_SEC;
#endif
	int                  rc;
	struct m0_fop       *rep_fop;

	M0_THREAD_ENTER;
	/*
	 * file -> mds is mapped by hash of filename.
	 * Link will create a new file entry in dir, but object may
	 * be on another mds. So link is disabled until this problem is solved.
	 */
	return M0_ERR(-EOPNOTSUPP);

	ci    = M0T1FS_I(inode);
	csb   = M0T1FS_SB(inode->i_sb);

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);

	M0_SET0(&mo);
	mo.mo_attr.ca_pfid  = *m0t1fs_inode_fid(M0T1FS_I(dir));
	mo.mo_attr.ca_tfid  = *m0t1fs_inode_fid(ci);
	mo.mo_attr.ca_nlink = inode->i_nlink + 1;
	mo.mo_attr.ca_ctime = now.tv_sec;
	mo.mo_attr.ca_valid = (M0_COB_CTIME | M0_COB_NLINK);
	m0_buf_init(&mo.mo_attr.ca_name, (char*)new->d_name.name,
		    new->d_name.len);

	rc = m0t1fs_mds_cob_link(csb, &mo, &rep_fop);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "mdservive link fop failed: %d", rc);
		goto out;
	}

	inode_inc_link_count(inode);
	inode->i_ctime = now;
	atomic_inc(&inode->i_count);
	d_instantiate(new, inode);
	mark_inode_dirty(dir);

out:
	m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

static int m0t1fs_fid_unlink(struct inode *dir, struct dentry *dentry)
{
	M0_THREAD_ENTER;
	return M0_ERR(-EOPNOTSUPP);
}

static int m0t1fs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct m0t1fs_sb     *csb;
	struct inode         *inode;
	struct m0t1fs_inode  *ci;
	struct m0t1fs_mdop    mo;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	struct timespec64     now;
#else
	struct timespec       now;
#endif
	int                   rc = 0;
	struct m0_fop        *lookup_rep_fop = NULL;
	struct m0_fop        *unlink_rep_fop = NULL;
	struct m0_fop        *setattr_rep_fop = NULL;

	M0_THREAD_ENTER;
	M0_ENTRY();

	M0_LOG(M0_INFO, "Name: \"%s\"", (char*)dentry->d_name.name);

	inode = dentry->d_inode;
	csb   = M0T1FS_SB(inode->i_sb);
	ci    = M0T1FS_I(inode);

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);

	M0_SET0(&mo);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	now = current_time(inode);
#else
	now = CURRENT_TIME_SEC;
#endif
	mo.mo_attr.ca_pfid  = *m0t1fs_inode_fid(M0T1FS_I(dir));
	mo.mo_attr.ca_tfid  = *m0t1fs_inode_fid(ci);
	mo.mo_attr.ca_nlink = inode->i_nlink - 1;
	mo.mo_attr.ca_ctime = now.tv_sec;
	mo.mo_attr.ca_valid = (M0_COB_NLINK | M0_COB_CTIME);
	m0_buf_init(&mo.mo_attr.ca_name,
		    (char*)dentry->d_name.name, dentry->d_name.len);
	mo.mo_attr.ca_pver = ci->ci_pver;

	if (csb->csb_oostore) {
		rc = m0t1fs_component_objects_op(ci, &mo,
						 m0t1fs_ios_cob_delete);
		if (rc == 0) {
			inode->i_ctime = dir->i_ctime = dir->i_mtime = now;
			inode_dec_link_count(inode);
			mark_inode_dirty(dir);
		} else
			M0_LOG(M0_ERROR,
			       "ioservice delete fop failed: %d, gob fid "
			       FID_F", size=%lld",
			       rc, FID_P(m0t1fs_inode_fid(ci)),
			       ci->ci_inode.i_size);
		goto out;
	}

	rc = m0t1fs_mds_cob_lookup(csb, &mo, &lookup_rep_fop);
	if (rc != 0)
		goto out;

	/* @todo: According to MM, a hash function will be used to choose
	 * a list of extra ioservices, on which "meta component objects" will
	 * be created. For unlink, it is similar to that:
	 *     rc = m0t1fs_component_objects_op(ci, &mo, m0t1fs_ios_cob_delete);
	 * That will be done in separated MM tasks.
	 */
	rc = m0t1fs_mds_cob_unlink(csb, &mo, &unlink_rep_fop);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "mdservive unlink fop failed: %d", rc);
		goto out;
	}

	/*
	 * IOS cob may not yet created. So skip deletion of
	 * ios cob if file size is zero.
	 */
	if (mo.mo_attr.ca_nlink == 0 && ci->ci_inode.i_size > 0) {
		rc = m0t1fs_component_objects_op(ci, &mo, m0t1fs_ios_cob_delete);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "ioservice delete fop failed: %d", rc);
			goto out;
		}
	}

	/** Update ctime and mtime on parent dir. */
	M0_SET0(&mo);
	mo.mo_attr.ca_tfid  = *m0t1fs_inode_fid(M0T1FS_I(dir));
	mo.mo_attr.ca_ctime = now.tv_sec;
	mo.mo_attr.ca_mtime = now.tv_sec;
	mo.mo_attr.ca_valid = (M0_COB_CTIME | M0_COB_MTIME);
	m0_buf_init(&mo.mo_attr.ca_name,
		    (char*)dentry->d_name.name, dentry->d_name.len);
	mo.mo_attr.ca_pver = ci->ci_pver;

	rc = m0t1fs_mds_cob_setattr(csb, &mo, &setattr_rep_fop);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "Setattr on parent dir failed with %d", rc);
		goto out;
	}

	inode->i_ctime = dir->i_ctime = dir->i_mtime = now;
	inode_dec_link_count(inode);
	mark_inode_dirty(dir);
out:
	m0_fop_put0_lock(lookup_rep_fop);
	m0_fop_put0_lock(unlink_rep_fop);
	m0_fop_put0_lock(setattr_rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

static int m0t1fs_fid_rmdir(struct inode *dir, struct dentry *dentry)
{
	M0_THREAD_ENTER;
	return M0_ERR(-EOPNOTSUPP);
}

static int m0t1fs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int rc;

	M0_THREAD_ENTER;
	M0_ENTRY();
	rc = m0t1fs_unlink(dir, dentry);
	if (rc == 0) {
		inode_dec_link_count(dentry->d_inode);
		drop_nlink(dir);
	}
	return M0_RC(rc);
}

static int m0t1fs_inode_update_stat(struct inode *inode,
				    struct m0_fop_cob *body, struct kstat *stat)
{
	struct m0t1fs_inode *ci = M0T1FS_I(inode);
	struct m0t1fs_sb    *csb   = M0T1FS_SB(inode->i_sb);
	int rc;

	if (body != NULL) {
		m0t1fs_inode_update(inode, body);
		if (m0_pool_version_find(&csb->csb_pools_common,
					 &body->b_pver) == NULL)
			return M0_ERR(-EINVAL);
		rc = m0t1fs_inode_layout_rebuild(ci, body);
		if (rc != 0)
			return rc;
	}

	/** Now its time to return inode stat data to user. */
	stat->dev = inode->i_sb->s_dev;
	stat->ino = inode->i_ino;
	stat->mode = inode->i_mode;
	stat->nlink = inode->i_nlink;
	stat->uid = inode->i_uid;
	stat->gid = inode->i_gid;
	stat->rdev = inode->i_rdev;
	stat->atime = inode->i_atime;
	stat->mtime = inode->i_mtime;
	stat->ctime = inode->i_ctime;
#ifdef HAVE_INODE_BLKSIZE
	stat->blksize = inode->i_blksize;
#else
	stat->blksize = 1 << inode->i_blkbits;
#endif
	stat->size = i_size_read(inode);
	stat->blocks = stat->blksize ? stat->size / stat->blksize : 0;
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
M0_INTERNAL int m0t1fs_fid_getattr(const struct path *path, struct kstat *stat,
			           uint32_t request_mask, uint32_t query_flags)
#else
M0_INTERNAL int m0t1fs_fid_getattr(struct vfsmount *mnt, struct dentry *dentry,
				   struct kstat *stat)
#endif
{
	struct m0t1fs_sb    *csb;
	struct inode        *inode;
	struct m0_fop_cob   *body;
	int                  rc;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
	struct dentry       *dentry = path->dentry;
#endif
	M0_THREAD_ENTER;
	M0_ENTRY();


	M0_LOG(M0_INFO, "Name: \"%s\"", (char*)dentry->d_name.name);

	inode = dentry->d_inode;
	csb   = M0T1FS_SB(inode->i_sb);

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);

	/* Return cached attributes for files in .mero/fid dir */
	body = &csb->csb_virt_body;
	rc = m0t1fs_inode_update_stat(inode, body, stat);
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
M0_INTERNAL int m0t1fs_getattr(const struct path *path, struct kstat *stat,
			       uint32_t request_mask, uint32_t query_flags)
#else
M0_INTERNAL int m0t1fs_getattr(struct vfsmount *mnt, struct dentry *dentry,
			       struct kstat *stat)
#endif
{
	struct m0_fop_getattr_rep *getattr_rep;
	struct m0t1fs_sb          *csb;
	struct inode              *inode;
	struct m0_fop_cob         *body;
	struct m0t1fs_inode       *ci;
	struct m0t1fs_mdop         mo;
	int                        rc = 0;
	struct m0_fop             *rep_fop = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
	struct dentry             *dentry = path->dentry;
#endif
	M0_THREAD_ENTER;
	M0_ENTRY();

	M0_LOG(M0_INFO, "Name: \"%s\"", (char*)dentry->d_name.name);
	inode = dentry->d_inode;

	csb   = M0T1FS_SB(inode->i_sb);
	ci    = M0T1FS_I(inode);

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);
	if (m0t1fs_inode_is_root(&ci->ci_inode)) {
		m0t1fs_inode_update_stat(inode, NULL, stat);
		goto out;
	}
	if (csb->csb_oostore) {
		rc = m0t1fs_cob_getattr(inode);
		m0t1fs_inode_update_stat(inode, NULL, stat);
		goto out;
	}

	M0_SET0(&mo);
	mo.mo_attr.ca_tfid = *m0t1fs_inode_fid(ci);
	m0_buf_init(&mo.mo_attr.ca_name,
		    (char*)dentry->d_name.name, dentry->d_name.len);

	/**
	   @todo When we have rm locking working, this will be changed to
	   revalidate inode with checking cached lock. If lock is cached
	   (not canceled), which means inode did not change, then we don't
	   have to do getattr and can just use @inode cached data.
	*/
	rc = m0t1fs_mds_cob_getattr(csb, &mo, &rep_fop);
	if (rc != 0)
		goto out;
	getattr_rep = m0_fop_data(rep_fop);
	body = &getattr_rep->g_body;
	rc = m0t1fs_inode_update_stat(inode, body, stat);
out:
	m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

M0_INTERNAL int m0t1fs_size_update(struct dentry *dentry, uint64_t newsize)
{
	struct m0t1fs_sb                *csb;
	struct inode                    *inode;
	struct m0t1fs_inode             *ci;
	struct m0t1fs_mdop               mo;
	int                              rc = 0;
	struct m0_fop                   *rep_fop = NULL;

	inode = dentry->d_inode;
	csb   = M0T1FS_SB(inode->i_sb);
	ci    = M0T1FS_I(inode);

	m0t1fs_fs_lock(csb);
	if (csb->csb_oostore) {
		inode->i_size = newsize;
		goto out;
	}
	M0_SET0(&mo);
	mo.mo_attr.ca_tfid   = *m0t1fs_inode_fid(ci);
	mo.mo_attr.ca_size   = newsize;
	mo.mo_attr.ca_valid |= M0_COB_SIZE;
	m0_buf_init(&mo.mo_attr.ca_name, (char*)dentry->d_name.name,
		    dentry->d_name.len);

	rc = m0t1fs_mds_cob_setattr(csb, &mo, &rep_fop);
	if (rc != 0)
		goto out;
	inode->i_size = newsize;
out:
	m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_unlock(csb);
	return M0_RC(rc);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
M0_INTERNAL int m0t1fs_fid_setxattr(const struct xattr_handler *handler,
				    struct dentry *dentry, struct inode *inode,
				    const char *name, const void *value,
				    size_t size, int flags)
#else
M0_INTERNAL int m0t1fs_fid_setxattr(struct dentry *dentry, const char *name,
				    const void *value, size_t size, int flags)
#endif
{
	M0_THREAD_ENTER;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	if (value == NULL)
		return m0t1fs_fid_removexattr(dentry, name);
#endif

	return M0_ERR(-EOPNOTSUPP);
}

M0_INTERNAL int m0t1fs_fid_setattr(struct dentry *dentry, struct iattr *attr)
{
	M0_THREAD_ENTER;
	return M0_ERR(-EOPNOTSUPP);
}

M0_INTERNAL int m0t1fs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct m0t1fs_sb     *csb;
	struct inode         *inode;
	struct m0t1fs_inode  *ci;
	struct m0t1fs_mdop    mo;
	int                   rc;
	struct m0_fop        *rep_fop = NULL;
	struct m0_rm_incoming rm_in;

	M0_THREAD_ENTER;
	M0_ENTRY();

	M0_LOG(M0_INFO, "Name: \"%s\"", (char*)dentry->d_name.name);

	inode = dentry->d_inode;
	csb   = M0T1FS_SB(inode->i_sb);
	ci    = M0T1FS_I(inode);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	rc = setattr_prepare(dentry, attr);
#else
	rc = inode_change_ok(inode, attr);
#endif
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);

	if (!csb->csb_oostore) {
		rc = file_lock_acquire(&rm_in, ci);
		if (rc != 0)
			goto out;
	}
	M0_SET0(&mo);

	if (m0_fid_is_set(&ci->ci_pver) && m0_fid_is_valid(&ci->ci_pver)) {
		mo.mo_attr.ca_pver = ci->ci_pver;
		mo.mo_attr.ca_valid |= M0_COB_PVER;
		M0_LOG(M0_DEBUG, "pver" FID_F"for object" FID_F,
				FID_P(&ci->ci_pver),
				FID_P(m0t1fs_inode_fid(ci)));
	} else {
		M0_LOG(M0_ERROR, "invalid inode");
		rc = -EINVAL;
		goto out;
	}
	mo.mo_attr.ca_lid    = ci->ci_layout_id;
	mo.mo_attr.ca_valid |= M0_COB_LID;

	mo.mo_attr.ca_tfid = *m0t1fs_inode_fid(ci);
	m0_buf_init(&mo.mo_attr.ca_name, (char*)dentry->d_name.name,
		    dentry->d_name.len);

	if (attr->ia_valid & ATTR_CTIME) {
		mo.mo_attr.ca_ctime = attr->ia_ctime.tv_sec;
		mo.mo_attr.ca_valid |= M0_COB_CTIME;
	}

	if (attr->ia_valid & ATTR_MTIME) {
		mo.mo_attr.ca_mtime = attr->ia_mtime.tv_sec;
		mo.mo_attr.ca_valid |= M0_COB_MTIME;
	}

	if (attr->ia_valid & ATTR_ATIME) {
		mo.mo_attr.ca_atime = attr->ia_atime.tv_sec;
		mo.mo_attr.ca_valid |= M0_COB_ATIME;
	}

	if (attr->ia_valid & ATTR_SIZE) {
		mo.mo_attr.ca_size = attr->ia_size;
		mo.mo_attr.ca_valid |= M0_COB_SIZE;
	}

	if (attr->ia_valid & ATTR_MODE) {
		mo.mo_attr.ca_mode = attr->ia_mode;
		mo.mo_attr.ca_valid |= M0_COB_MODE;
	}

	if (attr->ia_valid & ATTR_UID) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
		mo.mo_attr.ca_uid = from_kuid(current_user_ns(), attr->ia_uid);
#else
		mo.mo_attr.ca_uid = attr->ia_uid;
#endif
		mo.mo_attr.ca_valid |= M0_COB_UID;
	}

	if (attr->ia_valid & ATTR_GID) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
		mo.mo_attr.ca_gid = from_kgid(current_user_ns(), attr->ia_gid);
#else
		mo.mo_attr.ca_gid = attr->ia_gid;
#endif
		mo.mo_attr.ca_valid |= M0_COB_GID;
	}

	mo.mo_attr.ca_nlink = inode->i_nlink;
	mo.mo_attr.ca_valid |= M0_COB_NLINK;

	/*
	 * Layout can be changed explicitly in setattr()
	 * to a new layout, e.g. to a composite layout in NBA.
	 * Check for that use case and update layout id for this
	 * file. When that happens, a special setattr() with
	 * valid layout id should be called.
	 */

	if (!csb->csb_oostore) {
		rc = m0t1fs_mds_cob_setattr(csb, &mo, &rep_fop);
		if (rc != 0)
			goto out;
	} else {
		rc = m0t1fs_component_objects_op(ci, &mo,
						 m0t1fs_ios_cob_setattr);
		if (rc != 0)
			goto out;
	}
	if (attr->ia_valid & ATTR_SIZE && attr->ia_size < inode->i_size) {
		rc = m0t1fs_component_objects_op(ci, &mo,
						 m0t1fs_ios_cob_truncate);
		if (rc != 0)
			goto out;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	setattr_copy(inode, attr);
	if (attr->ia_valid & ATTR_SIZE)
		i_size_write(inode, attr->ia_size);
#else
	rc = inode_setattr(inode, attr);
	if (rc != 0)
		goto out;
#endif
out:
	if (!csb->csb_oostore)
		file_lock_release(&rm_in);

	m0_fop_put0_lock(rep_fop);
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

static int file_lock_acquire(struct m0_rm_incoming *rm_in,
			     struct m0t1fs_inode *ci)
{
	struct m0t1fs_sb       *csb;
	int 			rc;

	csb = M0T1FS_SB(ci->ci_inode.i_sb);
	M0_PRE(!csb->csb_oostore);

	rm_in->rin_want.cr_group_id = m0_rm_m0t1fs_group;
	m0_file_lock(&ci->ci_fowner, rm_in);
	m0_rm_owner_lock(&ci->ci_fowner);
	rc = m0_sm_timedwait(&rm_in->rin_sm, M0_BITS(RI_SUCCESS, RI_FAILURE),
			     M0_TIME_NEVER);
	m0_rm_owner_unlock(&ci->ci_fowner);
	rc = rc ? : rm_in->rin_rc;
	return rc;
}

static void file_lock_release(struct m0_rm_incoming *rm_in)
{
	M0_PRE(rm_in != NULL);
	m0_file_unlock(rm_in);
}

static int m0t1fs_component_objects_op(struct m0t1fs_inode *ci,
				       struct m0t1fs_mdop *mop,
				       int (*func)(struct cob_req *,
						   const struct m0t1fs_inode *,
						   const struct m0t1fs_mdop *,
						   int idx))
{
	struct m0t1fs_sb       *csb;
	struct cob_req          cob_req;
	int                     i = 0;
	int                     rc = 0;
	const char             *op_name;
	struct m0_pool_version *pv;
	struct m0_pools_common *pc;

	M0_PRE(ci != NULL);
	M0_PRE(func != NULL);

	M0_ENTRY();

	op_name = (func == m0t1fs_ios_cob_create) ? "create" :
		  (func == m0t1fs_ios_cob_delete) ? "delete" :
		  (func == m0t1fs_ios_cob_truncate) ? "truncate" : "setattr";
	csb = M0T1FS_SB(ci->ci_inode.i_sb);
	pc = &csb->csb_pools_common;

	M0_LOG(M0_DEBUG, "%s Component object %s for "FID_F,
			 csb->csb_oostore ? "oostore mode" : "",
			 op_name, FID_P(m0t1fs_inode_fid(ci)));

	m0_semaphore_init(&cob_req.cr_sem, 0);

	cob_req.cr_deadline = m0_time_from_now(0, COB_REQ_DEADLINE);
	cob_req.cr_csb = csb;
	cob_req.cr_rc = 0;
	cob_req.cr_fid = *m0t1fs_inode_fid(ci);
	cob_req.cr_pver = ci->ci_pver;

	if (csb->csb_oostore && func != m0t1fs_ios_cob_truncate) {
		M0_LOG(M0_DEBUG,"oostore mode");
		mop->mo_cob_type = M0_COB_MD;
		/*
		 * Create, delete and setattr operations on meta data cobs on
		 * ioservices are performed.
		 */
		for (i = 0; i < pc->pc_md_redundancy && rc == 0; i++)
			rc = func(&cob_req, ci, mop, i);
		while (--i >= 0)
			m0_semaphore_down(&cob_req.cr_sem);

		if (rc != 0 ||
		    func == m0t1fs_ios_cob_setattr ||
		    func == m0t1fs_ios_cob_create ||
		    /*  Data cobs are not createed until first write (CROW). */
		    ci->ci_inode.i_size <= 0)
			goto out;
	}

	pv = m0_pool_version_find(pc, &ci->ci_pver);
	if (pv == NULL) {
		M0_LOG(M0_ERROR, "Failed to get pool version "FID_F"",
		       FID_P(&ci->ci_pver));
		goto out;
	}

	mop->mo_cob_type = M0_COB_IO;
	for (i = 0; i < pv->pv_attr.pa_P && rc == 0; i++)
		rc = func(&cob_req, ci, mop, i);
	while (--i >= 0)
		m0_semaphore_down(&cob_req.cr_sem);
out:
	m0_semaphore_fini(&cob_req.cr_sem);
	M0_LOG(M0_DEBUG, "Cob %s "FID_F" with %d", op_name,
			FID_P(&cob_req.cr_fid), rc ?: cob_req.cr_rc);

	return M0_RC(rc ?: cob_req.cr_rc);
}

static int m0t1fs_mds_cob_fop_populate(struct m0t1fs_sb         *csb,
				       const struct m0t1fs_mdop *mo,
				       struct m0_fop            *fop)
{
	struct m0_fop_create    *create;
	struct m0_fop_unlink    *unlink;
	struct m0_fop_link      *link;
	struct m0_fop_lookup    *lookup;
	struct m0_fop_getattr   *getattr;
	struct m0_fop_statfs    *statfs;
	struct m0_fop_setattr   *setattr;
	struct m0_fop_readdir   *readdir;
	struct m0_fop_setxattr  *setxattr;
	struct m0_fop_getxattr  *getxattr;
	struct m0_fop_listxattr *listxattr;
	struct m0_fop_delxattr  *delxattr;
	struct m0_fop_cob       *req;
	int                      rc = 0;

	switch (m0_fop_opcode(fop)) {
	case M0_MDSERVICE_CREATE_OPCODE:
		create = m0_fop_data(fop);
		req = &create->c_body;

		req->b_pfid = mo->mo_attr.ca_pfid;
		req->b_tfid = mo->mo_attr.ca_tfid;
		req->b_pver = mo->mo_attr.ca_pver;
		body_mem2wire(req, &mo->mo_attr, mo->mo_attr.ca_valid);
		rc = name_mem2wire(&create->c_name, &mo->mo_attr.ca_name);
		break;
	case M0_MDSERVICE_LINK_OPCODE:
		link = m0_fop_data(fop);
		req = &link->l_body;

		req->b_pfid = mo->mo_attr.ca_pfid;
		req->b_tfid = mo->mo_attr.ca_tfid;
		body_mem2wire(req, &mo->mo_attr, mo->mo_attr.ca_valid);
		rc = name_mem2wire(&link->l_name, &mo->mo_attr.ca_name);
		break;
	case M0_MDSERVICE_UNLINK_OPCODE:
		unlink = m0_fop_data(fop);
		req = &unlink->u_body;

		req->b_pfid = mo->mo_attr.ca_pfid;
		req->b_tfid = mo->mo_attr.ca_tfid;
		body_mem2wire(req, &mo->mo_attr, mo->mo_attr.ca_valid);
		rc = name_mem2wire(&unlink->u_name, &mo->mo_attr.ca_name);
		break;
	case M0_MDSERVICE_STATFS_OPCODE:
		statfs = m0_fop_data(fop);
		statfs->f_flags = 0;
		break;
	case M0_MDSERVICE_LOOKUP_OPCODE:
		lookup = m0_fop_data(fop);
		req = &lookup->l_body;

		req->b_pfid = mo->mo_attr.ca_pfid;
		rc = name_mem2wire(&lookup->l_name, &mo->mo_attr.ca_name);
		break;
	case M0_MDSERVICE_GETATTR_OPCODE:
		getattr = m0_fop_data(fop);
		req = &getattr->g_body;

		req->b_tfid = mo->mo_attr.ca_tfid;
		break;
	case M0_MDSERVICE_SETATTR_OPCODE:
		setattr = m0_fop_data(fop);
		req = &setattr->s_body;

		req->b_tfid = mo->mo_attr.ca_tfid;
		body_mem2wire(req, &mo->mo_attr, mo->mo_attr.ca_valid);
		break;
	case M0_MDSERVICE_READDIR_OPCODE:
		readdir = m0_fop_data(fop);
		req = &readdir->r_body;

		req->b_tfid = mo->mo_attr.ca_tfid;
		rc = name_mem2wire(&readdir->r_pos, &mo->mo_attr.ca_name);
		break;
	case M0_MDSERVICE_SETXATTR_OPCODE:
		setxattr = m0_fop_data(fop);
		req = &setxattr->s_body;

		req->b_tfid = mo->mo_attr.ca_tfid;
		rc = name_mem2wire(&setxattr->s_key, &mo->mo_attr.ca_eakey) ?:
			name_mem2wire(&setxattr->s_value,
				      &mo->mo_attr.ca_eaval);
		break;
	case M0_MDSERVICE_GETXATTR_OPCODE:
		getxattr = m0_fop_data(fop);
		req = &getxattr->g_body;

		req->b_tfid = mo->mo_attr.ca_tfid;
		rc = name_mem2wire(&getxattr->g_key, &mo->mo_attr.ca_eakey);
		break;
	case M0_MDSERVICE_LISTXATTR_OPCODE:
		listxattr = m0_fop_data(fop);
		req = &listxattr->l_body;

		req->b_tfid = mo->mo_attr.ca_tfid;
		break;
	case M0_MDSERVICE_DELXATTR_OPCODE:
		delxattr = m0_fop_data(fop);
		req = &delxattr->d_body;

		req->b_tfid = mo->mo_attr.ca_tfid;
		rc = name_mem2wire(&delxattr->d_key, &mo->mo_attr.ca_eakey);
		break;
	default:
		rc = M0_ERR(-ENOSYS);
		break;
	}

	return M0_RC(rc);
}

static int m0t1fs_mds_cob_op(struct m0t1fs_sb            *csb,
			     const struct m0t1fs_mdop    *mo,
			     struct m0_fop_type          *ftype,
			     struct m0_fop              **rep_fop)
{
	int                                rc;
	struct m0_fop                     *fop;
	struct m0_rpc_session             *session;
	struct m0_rpc_item                *item = NULL;
	union {
		struct m0_fop_create_rep    *create_rep;
		struct m0_fop_unlink_rep    *unlink_rep;
		struct m0_fop_rename_rep    *rename_rep;
		struct m0_fop_link_rep      *link_rep;
		struct m0_fop_setattr_rep   *setattr_rep;
		struct m0_fop_getattr_rep   *getattr_rep;
		struct m0_fop_statfs_rep    *statfs_rep;
		struct m0_fop_lookup_rep    *lookup_rep;
		struct m0_fop_open_rep      *open_rep;
		struct m0_fop_close_rep     *close_rep;
		struct m0_fop_readdir_rep   *readdir_rep;
		struct m0_fop_setxattr_rep  *setxattr_rep;
		struct m0_fop_getxattr_rep  *getxattr_rep;
		struct m0_fop_listxattr_rep *listxattr_rep;
		struct m0_fop_delxattr_rep  *delxattr_rep;
	} u;
	void                              *reply_data;
	struct m0_reqh_service_ctx        *ctx;
	struct m0_be_tx_remid             *remid = NULL;

	M0_PRE(csb != NULL);
	M0_PRE(mo != NULL);
	M0_PRE(rep_fop != NULL);
	M0_PRE(ftype != NULL);

	M0_ENTRY();

	*rep_fop = NULL;
	session = m0t1fs_filename_to_mds_session(csb,
						 mo->mo_attr.ca_name.b_addr,
						 mo->mo_attr.ca_name.b_nob,
						 mo->mo_use_hint,
						 mo->mo_hash_hint);
	rc = m0_rpc_session_validate(session);
	if (rc != 0)
		return M0_ERR(rc);

	fop = m0_fop_alloc_at(session, ftype);
	if (fop == NULL) {
		rc = M0_ERR(-ENOMEM);
		M0_LOG(M0_ERROR, "m0_fop_alloc() failed with %d", rc);
		goto out;
	}

	item = m0_fop_to_rpc_item(fop);

	rc = m0t1fs_mds_cob_fop_populate(csb, mo, fop);
	if (rc != 0) {
		M0_LOG(M0_ERROR,
		       "%p[%u] m0t1fs_mds_cob_fop_populate() failed with %d",
		       item, m0_fop_opcode(fop), rc);
		goto out;
	}

	M0_LOG(M0_DEBUG, "%p[%u] Send md operation to session %p (sid=%lu)",
			 item, m0_fop_opcode(fop), session,
			 (unsigned long)session->s_session_id);
	rc = m0_rpc_post_sync(fop, session, NULL, 0 /* deadline */);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "%p[%u] m0_rpc_post_sync failed with %d",
		       item, m0_fop_opcode(fop), rc);
		goto out;
	}

	*rep_fop = m0_rpc_item_to_fop(fop->f_item.ri_reply);
	m0_fop_get(*rep_fop);
	reply_data = m0_fop_data(*rep_fop);

	/**
	 * @todo remid can be found generically, outside of this switch through
	 * the use of 'm0_xcode_find()' - this function should be cleaned up
	 * later.
	 */
	switch (m0_fop_opcode(fop)) {
	case M0_MDSERVICE_CREATE_OPCODE:
		u.create_rep = reply_data;
		rc = u.create_rep->c_body.b_rc;
		remid = &u.create_rep->c_mod_rep.fmr_remid;
		break;
	case M0_MDSERVICE_STATFS_OPCODE:
		u.statfs_rep = reply_data;
		rc = u.statfs_rep->f_rc;
		break;
	case M0_MDSERVICE_LOOKUP_OPCODE:
		u.lookup_rep = reply_data;
		rc = u.lookup_rep->l_body.b_rc;
		break;
	case M0_MDSERVICE_LINK_OPCODE:
		u.link_rep = reply_data;
		rc = u.link_rep->l_body.b_rc;
		remid = &u.link_rep->l_mod_rep.fmr_remid;
		break;
	case M0_MDSERVICE_UNLINK_OPCODE:
		u.unlink_rep = reply_data;
		rc = u.unlink_rep->u_body.b_rc;
		remid = &u.unlink_rep->u_mod_rep.fmr_remid;
		break;
	case M0_MDSERVICE_RENAME_OPCODE:
		u.rename_rep = reply_data;
		rc = u.rename_rep->r_body.b_rc;
		remid = &u.rename_rep->r_mod_rep.fmr_remid;
		break;
	case M0_MDSERVICE_SETATTR_OPCODE:
		u.setattr_rep = reply_data;
		rc = u.setattr_rep->s_body.b_rc;
		remid = &u.setattr_rep->s_mod_rep.fmr_remid;
		break;
	case M0_MDSERVICE_GETATTR_OPCODE:
		u.getattr_rep = reply_data;
		rc = u.getattr_rep->g_body.b_rc;
		break;
	case M0_MDSERVICE_OPEN_OPCODE:
		u.open_rep = reply_data;
		rc = u.open_rep->o_body.b_rc;
		remid = &u.open_rep->o_mod_rep.fmr_remid;
		break;
	case M0_MDSERVICE_CLOSE_OPCODE:
		u.close_rep = reply_data;
		rc = u.close_rep->c_body.b_rc;
		break;
	case M0_MDSERVICE_READDIR_OPCODE:
		u.readdir_rep = reply_data;
		rc = u.readdir_rep->r_body.b_rc;
		break;
	case M0_MDSERVICE_SETXATTR_OPCODE:
		u.setxattr_rep = reply_data;
		rc = u.setxattr_rep->s_body.b_rc;
		remid = &u.setxattr_rep->s_mod_rep.fmr_remid;
		break;
	case M0_MDSERVICE_GETXATTR_OPCODE:
		u.getxattr_rep = reply_data;
		rc = u.getxattr_rep->g_body.b_rc;
		break;
	case M0_MDSERVICE_LISTXATTR_OPCODE:
		u.listxattr_rep = reply_data;
		rc = u.listxattr_rep->l_body.b_rc;
		break;
	case M0_MDSERVICE_DELXATTR_OPCODE:
		u.delxattr_rep = reply_data;
		rc = u.delxattr_rep->d_body.b_rc;
		remid = &u.delxattr_rep->d_mod_rep.fmr_remid;
		break;
	default:
		M0_LOG(M0_ERROR, "%p[%u] Unexpected fop opcode",
		       item, m0_fop_opcode(fop));
		rc = M0_ERR(-ENOSYS);
		goto out;
	}

	/* update pending transaction number */
	ctx = m0_reqh_service_ctx_from_session(session);
	if (remid != NULL)
		m0t1fs_fsync_record_update(ctx, csb, NULL, remid);

out:
	M0_LOG(M0_DEBUG, "%p[%u], rc %d", item, m0_fop_opcode(fop), rc);
	m0_fop_put0_lock(fop);
	return M0_RC(rc);
}

int m0t1fs_mds_statfs(struct m0t1fs_sb *csb, struct m0_fop **rep_fop)
{
	struct m0t1fs_mdop mo;
	M0_SET0(&mo);
	return m0t1fs_mds_cob_op(csb, &mo, &m0_fop_statfs_fopt, rep_fop);
}

int m0t1fs_mds_cob_create(struct m0t1fs_sb          *csb,
			  const struct m0t1fs_mdop  *mo,
			  struct m0_fop            **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_create_fopt, rep_fop);
}

int m0t1fs_mds_cob_unlink(struct m0t1fs_sb          *csb,
			  const struct m0t1fs_mdop  *mo,
			  struct m0_fop            **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_unlink_fopt, rep_fop);
}

int m0t1fs_mds_cob_link(struct m0t1fs_sb          *csb,
			const struct m0t1fs_mdop  *mo,
			struct m0_fop            **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_link_fopt, rep_fop);
}

int m0t1fs_mds_cob_lookup(struct m0t1fs_sb          *csb,
			  const struct m0t1fs_mdop  *mo,
			  struct m0_fop            **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_lookup_fopt, rep_fop);
}

int m0t1fs_mds_cob_getattr(struct m0t1fs_sb           *csb,
			   const struct m0t1fs_mdop   *mo,
			   struct m0_fop             **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_getattr_fopt, rep_fop);
}

int m0t1fs_mds_cob_setattr(struct m0t1fs_sb           *csb,
			   const struct m0t1fs_mdop   *mo,
			   struct m0_fop             **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_setattr_fopt, rep_fop);
}

int m0t1fs_mds_cob_readdir(struct m0t1fs_sb           *csb,
			   const struct m0t1fs_mdop   *mo,
			   struct m0_fop             **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_readdir_fopt, rep_fop);
}

int m0t1fs_mds_cob_setxattr(struct m0t1fs_sb            *csb,
			    const struct m0t1fs_mdop    *mo,
			    struct m0_fop              **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_setxattr_fopt, rep_fop);
}

int m0t1fs_mds_cob_getxattr(struct m0t1fs_sb            *csb,
			    const struct m0t1fs_mdop    *mo,
			    struct m0_fop              **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_getxattr_fopt, rep_fop);
}

int m0t1fs_mds_cob_listxattr(struct m0t1fs_sb             *csb,
			     const struct m0t1fs_mdop     *mo,
			     struct m0_fop               **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_listxattr_fopt, rep_fop);
}

int m0t1fs_mds_cob_delxattr(struct m0t1fs_sb            *csb,
			    const struct m0t1fs_mdop    *mo,
			    struct m0_fop              **rep_fop)
{
	return m0t1fs_mds_cob_op(csb, mo, &m0_fop_delxattr_fopt, rep_fop);
}

static int m0t1fs_ios_cob_fop_populate(struct m0t1fs_sb         *csb,
				       const struct m0t1fs_mdop *mop,
				       struct m0_fop            *fop,
				       const struct m0_fid      *cob_fid,
				       const struct m0_fid      *gob_fid,
				       uint32_t                  cob_idx)
{
	struct m0_fop_cob_common   *common;
	struct m0_fop_cob_truncate *ct;

	M0_PRE(fop != NULL);
	M0_PRE(fop->f_type != NULL);
	M0_PRE(cob_fid != NULL);
	M0_PRE(gob_fid != NULL);

	M0_ENTRY();

	common = m0_cobfop_common_get(fop);
	M0_ASSERT(common != NULL);

	body_mem2wire(&common->c_body, &mop->mo_attr, mop->mo_attr.ca_valid);

	common->c_gobfid = *gob_fid;
	common->c_cobfid = *cob_fid;
	common->c_pver   = mop->mo_attr.ca_pver;
	common->c_cob_idx = cob_idx;
	common->c_cob_type = mop->mo_cob_type;

	if (fop->f_type != &m0_fop_cob_getattr_fopt)
		common->c_flags |= M0_IO_FLAG_CROW;

	if (m0_is_cob_truncate_fop(fop)) {
		ct = m0_fop_data(fop);
		ct->ct_size = mop->mo_attr.ca_size;
	}

	return M0_RC(0);
}

static void cfop_release(struct m0_ref *ref)
{
	struct m0_fop  *fop;
	struct cob_fop *cfop;
	struct cob_req *creq;

	M0_ENTRY();
	M0_PRE(ref != NULL);

	fop  = container_of(ref, struct m0_fop, f_ref);
	cfop = container_of(fop, struct cob_fop, c_fop);
	creq = cfop->c_req;
	M0_LOG(M0_DEBUG, "%p[%u] ri_error %d, cob_req_fop %p, cr_rc %d, "
	       FID_F, &fop->f_item, m0_fop_opcode(fop),
	       fop->f_item.ri_error, cfop, creq->cr_rc,
	       FID_P(&creq->cr_fid));
	m0_fop_fini(fop);
	m0_free(cfop);

	M0_LEAVE();
}

static int m0t1fs_ios_cob_op(struct cob_req            *cr,
			     const struct m0t1fs_inode *ci,
			     const struct m0t1fs_mdop  *mop,
			     int                       idx,
			     struct m0_fop_type       *ftype)
{
	int                         rc;
	struct m0t1fs_sb           *csb;
	struct m0_fid               cob_fid;
	const struct m0_fid        *gob_fid;
	uint32_t                    cob_idx;
	struct m0_fop              *fop = NULL;
	struct cob_fop             *cfop;
	struct m0_rpc_session      *session;
	struct m0_pool_version     *pver;

	M0_PRE(ci != NULL);
	M0_PRE(ftype != NULL);

	M0_ENTRY();

	csb = M0T1FS_SB(ci->ci_inode.i_sb);
	gob_fid = m0t1fs_inode_fid(ci);

	pver = m0_pool_version_find(&csb->csb_pools_common, &ci->ci_pver);
	if (pver == NULL) {
		M0_ASSERT(ci->ci_layout_instance == NULL);
		return M0_ERR_INFO(-ENOENT, "Pool version "FID_F" associated"
				   " with gob "FID_F" is unavailable."
				   " Has configuration got updated?",
				   FID_P(&ci->ci_pver), FID_P(gob_fid));
	}
	if (mop->mo_cob_type == M0_COB_MD) {
		session = m0_reqh_mdpool_service_index_to_session(
				&csb->csb_reqh, gob_fid, idx);
		m0_fid_convert_gob2cob(gob_fid, &cob_fid, 0);
		cob_idx = idx;
	} else {
		m0_poolmach_gob2cob(&pver->pv_mach, gob_fid, idx, &cob_fid);
		cob_idx = m0_fid_cob_device_id(&cob_fid);
		M0_ASSERT(cob_idx != ~0);
		session = m0t1fs_container_id_to_session(pver, cob_idx);
	}
	M0_ASSERT(session != NULL);

	M0_ALLOC_PTR(cfop);
	if (cfop == NULL) {
		rc = M0_ERR(-ENOMEM);
		M0_LOG(M0_ERROR, "cob_fop malloc failed");
		goto out;
	}

	cfop->c_req = cr;
	fop = &cfop->c_fop;

	m0_fop_init(fop, ftype, NULL, cfop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc != 0) {
		m0_fop_fini(fop);
		m0_free(cfop);
		M0_LOG(M0_ERROR, "cob_fop data malloc failed");
		goto out;
	}
	fop->f_item.ri_rmachine = m0_fop_session_machine(session);

	M0_ASSERT(m0_is_cob_create_fop(fop) || m0_is_cob_delete_fop(fop) ||
		  m0_is_cob_truncate_fop(fop) || m0_is_cob_setattr_fop(fop));

	rc = m0t1fs_ios_cob_fop_populate(csb, mop, fop, &cob_fid, gob_fid,
					 cob_idx);
	if (rc != 0)
		goto fop_put;

	M0_LOG(M0_DEBUG, "%p[%u] Send %s %d:"FID_F" to session %p (sid=%lu)",
	       &fop->f_item, m0_fop_opcode(fop), m0_fop_name(fop), (int)cob_idx,
	       FID_P(&cob_fid), session, (unsigned long)session->s_session_id);

	fop->f_item.ri_session         = session;
	fop->f_item.ri_ops             = &cob_item_ops;
	fop->f_item.ri_deadline        = cr->cr_deadline; /* pack fops */
	fop->f_item.ri_nr_sent_max     = M0T1FS_RPC_MAX_RETRIES;
	fop->f_item.ri_resend_interval = M0T1FS_RPC_RESEND_INTERVAL;
	rc = m0_rpc_post(&fop->f_item);
fop_put:
	m0_fop_put0_lock(fop);
out:
	return M0_RC(rc);
}

static int m0t1fs_ios_cob_create(struct cob_req *cr,
				 const struct m0t1fs_inode *inode,
				 const struct m0t1fs_mdop *mop,
				 int idx)
{
	return m0t1fs_ios_cob_op(cr, inode, mop, idx, &m0_fop_cob_create_fopt);
}

static int m0t1fs_ios_cob_delete(struct cob_req *cr,
				 const struct m0t1fs_inode *inode,
				 const struct m0t1fs_mdop *mop,
				 int idx)
{
	return m0t1fs_ios_cob_op(cr, inode, mop, idx, &m0_fop_cob_delete_fopt);
}

static int m0t1fs_ios_cob_truncate(struct cob_req *cr,
				   const struct m0t1fs_inode *inode,
			           const struct m0t1fs_mdop *mop,
				   int idx)
{
	return m0t1fs_ios_cob_op(cr, inode, mop, idx,
				 &m0_fop_cob_truncate_fopt);
}

static int m0t1fs_ios_cob_setattr(struct cob_req *cr,
				  const struct m0t1fs_inode *inode,
				  const struct m0t1fs_mdop *mop,
				  int idx)
{
	return m0t1fs_ios_cob_op(cr, inode, mop, idx, &m0_fop_cob_setattr_fopt);
}

M0_INTERNAL int m0t1fs_cob_getattr(struct inode *inode)
{
	struct m0t1fs_sb                *csb;
	struct m0t1fs_inode             *ci;
	const struct m0_fid             *gob_fid;
	struct m0_fop                   *fop = NULL;
	struct m0_fop_cob               *body;
	struct m0_fid                    cob_fid;
	int                              rc = 0;
	struct m0_cob_attr               attr;
	struct m0_rpc_session           *session = NULL;
	struct m0t1fs_mdop               mo;
	uint32_t                         i;
	struct m0_fop_cob_getattr_reply *getattr_rep;

	M0_ENTRY();

	csb = M0T1FS_SB(inode->i_sb);
	ci  = M0T1FS_I(inode);

	gob_fid = m0t1fs_inode_fid(ci);

	for (i = 0; i < csb->csb_pools_common.pc_md_redundancy; i++) {
		session = m0_reqh_mdpool_service_index_to_session(
				&csb->csb_reqh, gob_fid, i);
		rc = m0_rpc_session_validate(session);
		if (rc != 0) {
			if (rc == -ECANCELED)
				continue;
			return M0_ERR(rc);
		}
		m0_fid_convert_gob2cob(gob_fid, &cob_fid, 0);

		M0_LOG(M0_DEBUG, "Getattr for "FID_F "~" FID_F,
			 FID_P(gob_fid), FID_P(&cob_fid));

		fop = m0_fop_alloc_at(session, &m0_fop_cob_getattr_fopt);
		if (fop == NULL) {
			rc = -ENOMEM;
			M0_LOG(M0_ERROR, "m0_fop_alloc() failed with %d", rc);
			return M0_RC(rc);
		}

		M0_SET0(&mo);
		mo.mo_attr.ca_pfid = *gob_fid;
		mo.mo_attr.ca_tfid = *gob_fid;
		mo.mo_cob_type = M0_COB_MD;
		mo.mo_attr.ca_pver = ci->ci_pver;
		m0t1fs_ios_cob_fop_populate(csb, &mo, fop, &cob_fid, gob_fid, i);

		M0_LOG(M0_DEBUG, "%p[%u] Send cob operation %s to session %p"
		       "(sid=%lu)", &fop->f_item, m0_fop_opcode(fop),
		       m0_fop_name(fop), session,
		       (unsigned long)session->s_session_id);

		fop->f_item.ri_nr_sent_max     = M0T1FS_RPC_MAX_RETRIES;
		fop->f_item.ri_resend_interval = M0T1FS_RPC_RESEND_INTERVAL;
		rc = m0_rpc_post_sync(fop, session, NULL, 0 /* deadline */);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "%p[%u] getattr returned: %d", &fop->f_item,
					m0_fop_opcode(fop), rc);
			m0_fop_put0_lock(fop);
			if (rc == -ECANCELED)
				continue;
			return M0_RC(rc);
		}
		getattr_rep = m0_fop_data(m0_rpc_item_to_fop(
					  fop->f_item.ri_reply));
		rc = getattr_rep->cgr_rc;
		M0_LOG(M0_DEBUG, "%p[%u] getattr returned: %d", &fop->f_item,
		       m0_fop_opcode(fop), rc);

		if (rc == 0) {
			body = &getattr_rep->cgr_body;
			body_wire2mem(&attr, body);
			m0_dump_cob_attr(&attr);
			/* Update inode fields with data from @getattr_rep. */
			m0t1fs_inode_update(inode, body);
			if (m0_pool_version_find(&csb->csb_pools_common,
						 &body->b_pver) == NULL)
				return M0_ERR(-EINVAL);
			rc = m0t1fs_inode_layout_rebuild(ci, body);
			m0_fop_put0_lock(fop);
			break;
		}
		m0_fop_put0_lock(fop);
	}

	return M0_RC(rc);
}

M0_INTERNAL int m0t1fs_cob_setattr(struct inode *inode, struct m0t1fs_mdop *mo)
{
	struct m0t1fs_inode *ci = M0T1FS_I(inode);
	struct m0t1fs_sb    *csb = M0T1FS_SB(inode->i_sb);
	int                  rc;
	M0_ENTRY();

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);

	/* Updating size to ios. */
	rc = m0t1fs_component_objects_op(ci, mo, m0t1fs_ios_cob_setattr);

	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
const struct xattr_handler m0t1fs_xattr_lid = {
	.prefix = "lid",
	.flags  = 0,
	.get    = m0t1fs_getxattr,
	.set    = m0t1fs_setxattr,
};

const struct xattr_handler m0t1fs_xattr_writesize = {
	.prefix = "writesize",
	.flags  = 0,
	.get    = m0t1fs_getxattr,
	.set    = m0t1fs_setxattr,
};
static const struct xattr_handler m0t1fs_xattr_fid_lid = {
	.prefix = "fid_lid",
	.flags  = 0,
	.get    = m0t1fs_fid_getxattr,
	.set    = m0t1fs_fid_setxattr,
};

static const struct xattr_handler m0t1fs_xattr_fid_writesize = {
	.prefix = "fid_writesize",
	.flags  = 0,
	.get    = m0t1fs_fid_getxattr,
	.set    = m0t1fs_fid_setxattr,
};

const struct xattr_handler *m0t1fs_xattr_handlers[] = {
	&m0t1fs_xattr_lid,
	&m0t1fs_xattr_writesize,
	&m0t1fs_xattr_fid_lid,
	&m0t1fs_xattr_fid_writesize,
	NULL
};
#endif

const struct file_operations m0t1fs_dir_file_operations = {
	.read    = generic_read_dir,    /* provided by linux kernel */
	.open    = m0t1fs_opendir,
	.release = m0t1fs_releasedir,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
	.iterate = m0t1fs_readdir,
#else
	.readdir = m0t1fs_readdir,
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	.fsync          = generic_file_fsync,  /* provided by linux kernel */
#else
	.fsync          = simple_fsync,	/* provided by linux kernel */
#endif
	.llseek         = generic_file_llseek, /* provided by linux kernel */
};

const struct file_operations m0t1fs_fid_dir_file_operations = {
	.read    = generic_read_dir,    /* provided by linux kernel */
	.open    = m0t1fs_fid_opendir,
	.release = m0t1fs_fid_releasedir,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
	.iterate = m0t1fs_fid_readdir,
#else
	.readdir = m0t1fs_fid_readdir,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	.fsync   = generic_file_fsync,  /* provided by linux kernel */
#else
	.fsync   = simple_fsync,	/* provided by linux kernel */
#endif
	.llseek  = generic_file_llseek, /* provided by linux kernel */
};

const struct inode_operations m0t1fs_dir_inode_operations = {
	.create         = m0t1fs_create,
	.lookup         = m0t1fs_lookup,
	.unlink         = m0t1fs_unlink,
	.link           = m0t1fs_link,
	.mkdir          = m0t1fs_mkdir,
	.rmdir          = m0t1fs_rmdir,
	.setattr        = m0t1fs_setattr,
	.getattr        = m0t1fs_getattr,

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	.setxattr       = m0t1fs_setxattr,
	.getxattr       = m0t1fs_getxattr,
	.removexattr    = m0t1fs_removexattr,
#endif
	.listxattr      = m0t1fs_listxattr,
};

const struct inode_operations m0t1fs_fid_dir_inode_operations = {
	.create         = m0t1fs_fid_create,
	.lookup         = m0t1fs_fid_lookup,
	.unlink         = m0t1fs_fid_unlink,
	.link           = m0t1fs_fid_link,
	.mkdir          = m0t1fs_fid_mkdir,
	.rmdir          = m0t1fs_fid_rmdir,
	.setattr        = m0t1fs_fid_setattr,
	.getattr        = m0t1fs_fid_getattr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	.setxattr       = m0t1fs_fid_setxattr,
	.getxattr       = m0t1fs_fid_getxattr,
	.removexattr    = m0t1fs_fid_removexattr,
#endif
	.listxattr      = m0t1fs_fid_listxattr,
};

#undef M0_TRACE_SUBSYSTEM
