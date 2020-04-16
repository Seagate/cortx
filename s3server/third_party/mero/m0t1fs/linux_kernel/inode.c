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
 *                  Amit Jambure <amit_jambure@xyratex.com>
 * Metadata       : Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 10/14/2011
 */

#include <linux/slab.h>         /* kmem_cache */

#include "layout/pdclust.h"     /* m0_pdclust_build(), m0_pdl_to_layout(),
				 * m0_pdclust_instance_build()           */
#include "layout/linear_enum.h" /* m0_linear_enum_build()                */
#include "lib/misc.h"           /* M0_SET0()                             */
#include "lib/memory.h"         /* M0_ALLOC_PTR(), m0_free()             */
#include "m0t1fs/linux_kernel/m0t1fs.h"
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"          /* M0_LOG and M0_ENTRY */
#include "mero/magic.h"
#include "rm/rm_service.h"      /* m0_rm_svc_domain_get */
#include "lib/tlist.h"          /* M0_TL_DESCR_DECLARE() */

/* ISPTI -> Inode's Service to Pending Transaction Id list */
M0_TL_DESCR_DEFINE(ispti, "m0_reqh_service_txid pending an inode", ,
			struct m0_reqh_service_txid,
			stx_tlink, stx_link_magic,
			M0_T1FS_INODE_PTI_MAGIC1, M0_T1FS_INODE_PTI_MAGIC2);

M0_TL_DEFINE(ispti, , struct m0_reqh_service_txid);

static struct kmem_cache *m0t1fs_inode_cachep = NULL;

static const struct m0_bob_type m0t1fs_inode_bob = {
	.bt_name         = "m0t1fs_inode",
	.bt_magix_offset = offsetof(struct m0t1fs_inode, ci_magic),
	.bt_magix        = M0_T1FS_INODE_MAGIC,
	.bt_check        = NULL
};

M0_BOB_DEFINE(M0_INTERNAL, &m0t1fs_inode_bob, m0t1fs_inode);

const struct m0_uint128 m0_rm_m0t1fs_group = M0_UINT128(0, 1);

M0_INTERNAL const struct m0_fid *m0t1fs_inode_fid(const struct m0t1fs_inode *ci)
{
	M0_PRE(ci != NULL);

	return &ci->ci_fid;
}

M0_INTERNAL bool m0t1fs_inode_is_root(const struct inode *inode)
{
	struct m0t1fs_inode *ci = M0T1FS_I(inode);

	return m0_fid_eq(m0t1fs_inode_fid(ci),
			 &M0T1FS_SB(inode->i_sb)->csb_root_fid);
}

M0_INTERNAL bool m0t1fs_inode_is_dot_mero(const struct inode *inode)
{
	struct m0t1fs_inode *ci = M0T1FS_I(inode);

	return m0_fid_eq(m0t1fs_inode_fid(ci), &M0_DOT_MERO_FID);
}

M0_INTERNAL bool m0t1fs_inode_is_dot_mero_fid(const struct inode *inode)
{
	struct m0t1fs_inode *ci = M0T1FS_I(inode);

	return m0_fid_eq(m0t1fs_inode_fid(ci), &M0_DOT_MERO_FID_FID);
}

static void init_once(void *foo)
{
	struct m0t1fs_inode *ci = foo;

	M0_ENTRY();

	inode_init_once(&ci->ci_inode);

	M0_LEAVE();
}

M0_INTERNAL int m0t1fs_inode_cache_init(void)
{
	int rc = 0;

	M0_ENTRY();

	m0t1fs_inode_cachep = kmem_cache_create("m0t1fs_inode_cache",
						sizeof(struct m0t1fs_inode),
						0, SLAB_HWCACHE_ALIGN,
						init_once);
	if (m0t1fs_inode_cachep == NULL)
		rc = -ENOMEM;

	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

M0_INTERNAL void m0t1fs_inode_cache_fini(void)
{
	M0_ENTRY();

	if (m0t1fs_inode_cachep != NULL) {
		kmem_cache_destroy(m0t1fs_inode_cachep);
		m0t1fs_inode_cachep = NULL;
	}

	M0_LEAVE();
}

M0_INTERNAL struct m0_rm_domain *m0t1fs_rm_domain_get(struct m0t1fs_sb *sb)
{
	return m0_rm_svc_domain_get(m0_reqh_service_find(&m0_rms_type,
							 &sb->csb_reqh));
}

M0_INTERNAL void m0t1fs_file_lock_init(struct m0t1fs_inode *ci,
				       struct m0t1fs_sb *csb)
{
	struct m0_rm_domain    *rdom;
	struct m0_pools_common *pc = &csb->csb_pools_common;
	const struct m0_fid    *fid = &ci->ci_fid;

	M0_ENTRY();

	M0_LOG(M0_INFO, FID_F, FID_P(fid));
	rdom = m0t1fs_rm_domain_get(csb);
	M0_ASSERT(rdom != NULL);
	/**
	 * @todo Get di type from configuration.
	 */
	m0_file_init(&ci->ci_flock, fid, rdom, M0_DI_DEFAULT_TYPE);
	m0_rm_remote_init(&ci->ci_creditor, &ci->ci_flock.fi_res);
	ci->ci_creditor.rem_session = m0_pools_common_active_rm_session(pc);
	M0_ASSERT(ci->ci_creditor.rem_session != NULL);
	ci->ci_creditor.rem_state = REM_SERVICE_LOCATED;
	m0_file_owner_init(&ci->ci_fowner, &m0_rm_m0t1fs_group,
			   &ci->ci_flock, &ci->ci_creditor);

	M0_LEAVE();
}

M0_INTERNAL void m0t1fs_file_lock_fini(struct m0t1fs_inode *ci)
{
	int rc;
	M0_ENTRY();

	m0_rm_owner_windup(&ci->ci_fowner);
	rc = m0_rm_owner_timedwait(&ci->ci_fowner,
				   M0_BITS(ROS_FINAL, ROS_INSOLVENT),
				   M0_TIME_NEVER);
	M0_ASSERT(rc == 0);
	m0_file_owner_fini(&ci->ci_fowner);
	m0_rm_remote_fini(&ci->ci_creditor);
	m0_file_fini(&ci->ci_flock);
	M0_LEAVE();
}

static void m0t1fs_inode_init(struct m0t1fs_inode *ci)
{
	M0_ENTRY("ci: %p", ci);
	M0_SET0(&ci->ci_fid);
	M0_SET0(&ci->ci_flock);
	M0_SET0(&ci->ci_creditor);
	M0_SET0(&ci->ci_fowner);
	ci->ci_layout_instance = NULL;
	ci->ci_layout_changed = false;
	m0_mutex_init(&ci->ci_layout_lock);
	m0_mutex_init(&ci->ci_pending_tx_lock);
	ispti_tlist_init(&ci->ci_pending_tx);
	m0t1fs_inode_bob_init(ci);
	csb_inodes_tlink_init(ci);
	M0_LEAVE();
}

static void m0t1fs_inode_ispti_fini(struct m0t1fs_inode *ci)
{
	struct m0_reqh_service_txid *iter = NULL;

	M0_ENTRY();

	m0_mutex_lock(&ci->ci_pending_tx_lock);
	m0_tl_teardown(ispti, &ci->ci_pending_tx, iter)
		m0_free0(&iter);

	ispti_tlist_fini(&ci->ci_pending_tx);
	m0_mutex_unlock(&ci->ci_pending_tx_lock);

	M0_LEAVE();
}

static void m0t1fs_inode_fini(struct m0t1fs_inode *ci)
{
	struct m0t1fs_sb *csb;
	struct inode     *inode;

	M0_THREAD_ENTER;
	M0_ENTRY("ci: %p, is_root %s, layout_instance %p",
		 ci, m0_bool_to_str(m0t1fs_inode_is_root(&ci->ci_inode)),
		 ci->ci_layout_instance);

	M0_PRE(m0t1fs_inode_bob_check(ci));
	csb = M0T1FS_SB(ci->ci_inode.i_sb);
	inode = &ci->ci_inode;
	m0_mutex_lock(&csb->csb_inodes_lock);
	csb_inodes_tlist_remove(ci);
	m0_mutex_unlock(&csb->csb_inodes_lock);
	m0t1fs_inode_bob_fini(ci);
	/* Empty the list, then free the list lock */
	m0t1fs_inode_ispti_fini(ci);
	m0_mutex_fini(&ci->ci_pending_tx_lock);
	m0_mutex_fini(&ci->ci_layout_lock);
	M0_LEAVE();
}

/**
   Implementation of super_operations::alloc_inode() interface.
 */
M0_INTERNAL struct inode *m0t1fs_alloc_inode(struct super_block *sb)
{
	struct m0t1fs_inode *ci;
	M0_THREAD_ENTER;

	M0_ENTRY("sb: %p", sb);

	ci = kmem_cache_alloc(m0t1fs_inode_cachep, GFP_KERNEL);
	if (ci == NULL) {
		M0_LEAVE("inode: %p", NULL);
		return NULL;
	}
	m0t1fs_inode_init(ci);
	M0_LEAVE("inode: %p", &ci->ci_inode);
	return &ci->ci_inode;
}

/**
   Implementation of super_operations::destroy_inode() interface.
 */
M0_INTERNAL void m0t1fs_destroy_inode(struct inode *inode)
{
	struct m0t1fs_inode *ci  = M0T1FS_I(inode);
	const struct m0_fid *fid = m0t1fs_inode_fid(ci);
	M0_THREAD_ENTER;

	M0_ENTRY("inode: %p, fid: "FID_F, inode, FID_P(fid));
	if (m0_fid_is_set(fid) && !m0t1fs_inode_is_root(inode)) {
		/**
		 * The function is called by kernel, and thus can be called
		 * concurrently with rconfc refresh callbacks.
		 * @see inodes_layout_ref_drop().
		 */
		m0_mutex_lock(&ci->ci_layout_lock);
		if (ci->ci_layout_instance != NULL)
			m0_layout_instance_fini(ci->ci_layout_instance);
		m0_mutex_unlock(&ci->ci_layout_lock);
		m0t1fs_file_lock_fini(ci);
	}
	m0t1fs_inode_fini(ci);
	kmem_cache_free(m0t1fs_inode_cachep, ci);
	M0_LEAVE();
}

M0_INTERNAL struct inode *m0t1fs_root_iget(struct super_block *sb,
					   const struct m0_fid *root_fid)
{
	struct m0_fop_getattr_rep *rep = NULL;
	struct m0t1fs_mdop         mo;
	struct inode              *inode;
	int                        rc;
	struct m0_fop             *rep_fop = NULL;
	struct m0t1fs_sb          *csb;
	struct m0_fop_cob         *body;

	M0_ENTRY("sb: %p", sb);

	csb = M0T1FS_SB(sb);
	M0_SET0(&mo);
	mo.mo_attr.ca_tfid = *root_fid;
	csb->csb_root_fid = *root_fid;

	if (!csb->csb_oostore) {
		rc = m0t1fs_mds_cob_getattr(csb, &mo, &rep_fop);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "m0t1fs_mds_cob_getattr() failed"
					"with %d", rc);
			m0_fop_put0_lock(rep_fop);
			return ERR_PTR(rc);
		}
		rep = m0_fop_data(rep_fop);
		body = &rep->g_body;
	} else {
		body = &csb->csb_virt_body;
		rc = m0t1fs_fill_cob_attr(body);
		if (rc != 0)
			return ERR_PTR(rc);
	}

	inode = m0t1fs_iget(sb, root_fid, body);

	m0_fop_put0_lock(rep_fop);

	M0_LEAVE("root_inode: %p", inode);
	return inode;
}

/**
   In file-systems like m0t1fs or nfs, inode number is not enough to identify
   a file. For such file-systems structure and semantics of file identifier
   are file-system specific e.g. fid in case of m0t1fs, file handle for nfs.

   m0t1fs_inode_test() and m0t1fs_inode_set() are the implementation of
   interfaces that are used by generic vfs code, to compare identities of
   inodes, in a generic manner.
 */
M0_INTERNAL int m0t1fs_inode_test(struct inode *inode, void *opaque)
{
	struct m0t1fs_inode *ci;
	struct m0_fid       *fid = opaque;
	int                  rc;

	M0_ENTRY();

	ci = M0T1FS_I(inode);

	M0_LOG(M0_DEBUG, "inode (%p) "FID_F" opaque "FID_F,
	       inode, FID_P(m0t1fs_inode_fid(ci)), FID_P(fid));

	rc = m0_fid_eq(m0t1fs_inode_fid(ci), fid);

	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

static int m0t1fs_inode_set(struct inode *inode, void *opaque)
{
	struct m0t1fs_inode *ci = M0T1FS_I(inode);
	struct m0_fid       *fid;
	struct m0t1fs_sb    *csb = M0T1FS_SB(inode->i_sb);

	M0_ENTRY();

	ci->ci_fid = *(fid = (struct m0_fid *)opaque);

	if (m0_fid_eq(fid, &csb->csb_root_fid))
		ci->ci_flock.fi_fid = fid;
	else
		m0t1fs_file_lock_init(ci, csb);
	inode->i_ino = m0_fid_hash(fid);

	M0_LOG(M0_DEBUG, "inode (%p) "FID_F, inode, FID_P(fid));
	return M0_RC(0);
}

M0_INTERNAL void m0t1fs_inode_update_blksize(struct inode *inode,
					     struct m0_layout *layout)
{
	struct m0_pdclust_attr 	*pa;
	size_t 			 buffsize;

	pa = &m0_layout_to_pdl(layout)->pl_attr;
	buffsize = pa->pa_unit_size * pa->pa_N;

#ifdef HAVE_INODE_BLKSIZE
	inode->i_blksize  = buffsize;
#else
	inode->i_blkbits = 12;
	while ((1 << inode->i_blkbits) < buffsize)
		inode->i_blkbits++;
#endif
}

M0_INTERNAL void m0t1fs_inode_update(struct inode      *inode,
				     struct m0_fop_cob *body)
{
	M0_ENTRY();

	if (body->b_valid & M0_COB_ATIME)
		inode->i_atime.tv_sec  = body->b_atime;
	if (body->b_valid & M0_COB_MTIME)
		inode->i_mtime.tv_sec  = body->b_mtime;
	if (body->b_valid & M0_COB_CTIME)
		inode->i_ctime.tv_sec  = body->b_ctime;
	if (body->b_valid & M0_COB_UID)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
		inode->i_uid    = make_kuid(current_user_ns(), body->b_uid);
#else
		inode->i_uid    = body->b_uid;
#endif
	if (body->b_valid & M0_COB_GID)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
		inode->i_gid    = make_kgid(current_user_ns(), body->b_gid);
#else
		inode->i_gid    = body->b_gid;
#endif
	if (body->b_valid & M0_COB_BLOCKS)
		inode->i_blocks = body->b_blocks;
	if (body->b_valid & M0_COB_SIZE)
		inode->i_size = body->b_size;
	if (body->b_valid & M0_COB_NLINK)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
		set_nlink(inode, body->b_nlink);
#else
		inode->i_nlink = body->b_nlink;
#endif
	if (body->b_valid & M0_COB_MODE)
		inode->i_mode = body->b_mode;

	M0_LEAVE();
}

static int m0t1fs_inode_read(struct inode      *inode,
			     struct m0_fop_cob *body)
{
	struct m0t1fs_inode *ci  = M0T1FS_I(inode);
	struct m0t1fs_sb    *csb = M0T1FS_SB(inode->i_sb);
	int                  rc  = 0;

	M0_ENTRY();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
#else
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	inode->i_uid   = GLOBAL_ROOT_UID;
	inode->i_gid   = GLOBAL_ROOT_GID;
#else
	inode->i_uid   = 0;
	inode->i_gid   = 0;
#endif
	inode->i_rdev  = 0;

	m0t1fs_inode_update(inode, body);
	if (S_ISREG(inode->i_mode)) {
		inode->i_op             = &m0t1fs_reg_inode_operations;
		inode->i_fop            = &m0t1fs_reg_file_operations;
		inode->i_mapping->a_ops = &m0t1fs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
	        if ((m0t1fs_inode_is_dot_mero(inode) ||
		    m0t1fs_inode_is_dot_mero_fid(inode)) &&
		    csb->csb_oostore) {
			/* currently open-by-fid is only in OOSTORE mode */
		        inode->i_op   = &m0t1fs_fid_dir_inode_operations;
		        inode->i_fop  = &m0t1fs_fid_dir_file_operations;
	        } else {
		        inode->i_op   = &m0t1fs_dir_inode_operations;
		        inode->i_fop  = &m0t1fs_dir_file_operations;
		}
	} else {
		rc = -ENOSYS;
	}
	if (!m0t1fs_inode_is_root(inode) && !m0t1fs_inode_is_dot_mero(inode) &&
	    !m0t1fs_inode_is_dot_mero_fid(inode)) {
		ci->ci_layout_id = body->b_lid;
		ci->ci_pver = body->b_pver;
		if (m0_pool_version_find(&csb->csb_pools_common, &ci->ci_pver)
		    == NULL)
			return M0_ERR(-EINVAL);
		rc = m0t1fs_inode_layout_init(ci);
		if (rc != 0)
			M0_LOG(M0_WARN, "m0t1fs_inode_layout_init() failed,"
					"rc=%d", rc);
		m0_mutex_lock(&csb->csb_inodes_lock);
		csb_inodes_tlist_add_tail(&csb->csb_inodes, ci);
		m0_mutex_unlock(&csb->csb_inodes_lock);
	}
	if (rc == 0)
		m0t1fs_fid_accept(csb, m0t1fs_inode_fid(M0T1FS_I(inode)));
	return M0_RC(rc);
}

M0_INTERNAL struct inode *m0t1fs_iget(struct super_block *sb,
				      const struct m0_fid *fid,
				      struct m0_fop_cob *body)
{
	struct inode *inode;
	unsigned long hash;
	int           err = 0;

	M0_ENTRY();

	hash = m0_fid_hash(fid);

	/*
	 * Search inode cache for an inode that has matching @fid.
	 * Use m0t1fs_inode_test() to compare fid_s. If not found, allocate a
	 * new inode. Set its fid to @fid using m0t1fs_inode_set(). Also
	 * set I_NEW flag in inode->i_state for newly allocated inode.
	 */
	inode = iget5_locked(sb, hash, m0t1fs_inode_test, m0t1fs_inode_set,
			     (void *)fid);
	if (IS_ERR(inode)) {
		M0_LEAVE("inode: %p", ERR_CAST(inode));
		return ERR_CAST(inode);
	}
	if ((inode->i_state & I_NEW) != 0) {
		/* New inode, set its fields from @body */
		err = m0t1fs_inode_read(inode, body);
	} else if (!(inode->i_state & (I_FREEING | I_CLEAR))) {
		/* Not a new inode, let's update its attributes from @body */
		m0t1fs_inode_update(inode, body);
	}
	if (err != 0)
		goto out_err;
	if ((inode->i_state & I_NEW) != 0)
		unlock_new_inode(inode);
	M0_LEAVE("inode: %p", inode);
	return inode;

out_err:
	iget_failed(inode);
	M0_LEAVE("ERR: %p", ERR_PTR(err));
	return ERR_PTR(err);
}

static int m0t1fs_build_layout_instance(struct m0t1fs_sb           *csb,
					const uint64_t              layout_id,
					const struct m0_fid        *fid,
					struct m0_layout_instance **linst)
{
	struct m0_layout *layout;
	int               rc;

	M0_ENTRY();
	M0_PRE(fid != NULL);
	M0_PRE(linst != NULL);

	/*
	 * All the layouts should already be generated on startup and added
	 * to the list unless wrong layout_id is used.
	 */
	layout = m0_layout_find(&csb->csb_reqh.rh_ldom, layout_id);
	if (layout == NULL) {
		rc = -EINVAL;
		goto out;
	}

	*linst = NULL;
	rc = m0_layout_instance_build(layout, fid, linst);
	m0_layout_put(layout);

out:
	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0t1fs_inode_layout_init(struct m0t1fs_inode *ci)
{
	struct m0t1fs_sb          *csb;
	struct m0_layout_instance *linst;
	int                        rc;
	uint64_t                   layout_id;

	M0_ENTRY();
	M0_PRE(m0_fid_is_valid(&ci->ci_pver));
	M0_LOG(M0_DEBUG, "fid:"FID_F"pver"FID_F"lid:%d",
			FID_P(m0t1fs_inode_fid(ci)),
			FID_P(&ci->ci_pver),
			(int)ci->ci_layout_id);

	csb = M0T1FS_SB(ci->ci_inode.i_sb);

	layout_id = m0_pool_version2layout_id(&ci->ci_pver, ci->ci_layout_id);
	rc = m0t1fs_build_layout_instance(csb, layout_id,
					  m0t1fs_inode_fid(ci), &linst);
	if (rc == 0) {
		if (ci->ci_layout_instance != NULL)
			m0_layout_instance_fini(ci->ci_layout_instance);
		ci->ci_layout_instance = linst;
		m0t1fs_inode_update_blksize(&ci->ci_inode, linst->li_l);
	}

	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0t1fs_inode_layout_rebuild(struct m0t1fs_inode *ci,
					    struct m0_fop_cob *body)
{
	M0_ENTRY();
	M0_PRE(ci != NULL);
	M0_PRE(body != NULL);
	M0_PRE(body->b_valid & (M0_COB_LID | M0_COB_PVER));

	if (ci->ci_layout_id != body->b_lid ||
	    m0_fid_cmp(&ci->ci_pver, &body->b_pver) ||
	    /*
	     * Layout instance belongs to a virtual pool version.
	     * There is a possibility that the instance has been
	     * deleted by confc update.
	     */
	    ci->ci_layout_instance == NULL) {
		ci->ci_layout_id = body->b_lid;
		ci->ci_pver = body->b_pver;
		return M0_RC(m0t1fs_inode_layout_init(ci));
	}
	return M0_RC(0);
}
