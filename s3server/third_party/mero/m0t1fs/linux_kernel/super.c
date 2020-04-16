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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 *                  Amit Jambure <amit_jambure@xyratex.com>
 * Metadata       : Yuriy Umanets <yuriy_umanets@xyratex.com>
 * Original creation date: 11/07/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"

#include <linux/kernel.h>  /* kstrtoul */
#include <linux/mount.h>
#include <linux/parser.h>  /* substring_t */
#include <linux/slab.h>    /* kmalloc, kfree */
#include <linux/statfs.h>  /* kstatfs */

#include "m0t1fs/linux_kernel/m0t1fs.h"
#include "m0t1fs/linux_kernel/fsync.h"
#include "mero/magic.h"    /* M0_T1FS_POOLS_MAGIC */
#include "lib/finject.h"   /* M0_FI_ENABLED */
#include "lib/misc.h"      /* M0_SET0 */
#include "lib/memory.h"    /* M0_ALLOC_PTR, m0_free */
#include "conf/confc.h"    /* m0_confc */
#include "conf/helpers.h"  /* m0_confc_root_open */
#include "conf/cache.h"    /* m0_conf_cache_contains */
#include "rpc/rpclib.h"    /* m0_rcp_client_connect */
#include "lib/uuid.h"      /* m0_uuid_generate */
#include "net/lnet/lnet.h"
#include "rpc/rpc_internal.h"
#include "net/lnet/lnet_core_types.h"
#include "rm/rm_service.h"     /* m0_rms_type */
#include "reqh/reqh_service.h" /* m0_reqh_service_ctx */
#include "reqh/reqh.h"
#include "addb2/global.h"
#include "addb2/sys.h"
#include "module/instance.h"   /* m0_get */
#include "ha/epoch.h"          /* m0_ha_client_add */
#include "balloc/balloc.h"     /* BALLOC_DEF_BLOCK_SHIFT */
#include "mero/ha.h"           /* m0_mero_ha */
#include "conf/ha.h"           /* m0_conf_ha_process_event_post */

extern struct io_mem_stats iommstats;
extern struct m0_bitmap    m0t1fs_client_ep_tmid;
extern struct m0_mutex     m0t1fs_mutex;

static char *local_addr = "0@lo:12345:45:";
M0_INTERNAL const struct m0_fid M0_ROOT_FID = {
	.f_container = 1ULL,
	.f_key       = 1ULL
};

#define M0T1FS_NAME_LEN 256

module_param(local_addr, charp, S_IRUGO);
MODULE_PARM_DESC(local_addr, "End-point address of m0t1fs "
		 "e.g. 172.18.50.40@o2ib1:12345:34:\n"
		 "the tmid will be generated and filled by every mount");

static uint32_t tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
module_param(tm_recv_queue_min_len , int, S_IRUGO);
MODULE_PARM_DESC(tm_recv_queue_min_len, "TM receive queue minimum length");

static uint32_t max_rpc_msg_size = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
module_param(max_rpc_msg_size, int, S_IRUGO);
MODULE_PARM_DESC(max_rpc_msg_size, "Maximum RPC message size");

static int addb2_net_disable = 0;
module_param(addb2_net_disable, int, S_IRUGO);
MODULE_PARM_DESC(addb2_net_disable, "Disable addb2 records network submit");

M0_INTERNAL void io_bob_tlists_init(void);
static int m0t1fs_statfs(struct dentry *dentry, struct kstatfs *buf);
static void inodes_layout_ref_drop(struct m0t1fs_sb *csb);
static bool m0t1fs_conf_ready_async_cb(struct m0_clink *clink);
static void conf_ready_async_cb_locked(struct m0t1fs_sb *csb);

static const struct super_operations m0t1fs_super_operations = {
	.statfs        = m0t1fs_statfs,
	.alloc_inode   = m0t1fs_alloc_inode,
	.destroy_inode = m0t1fs_destroy_inode,
	.drop_inode    = generic_delete_inode, /* provided by linux kernel */
	.sync_fs       = m0t1fs_sync_fs
};

M0_TL_DESCR_DEFINE(csb_inodes, "m0t1fs_inode anchored in m0t1fs_sb", M0_INTERNAL,
			struct m0t1fs_inode,
			ci_sb_linkage, ci_magic,
			M0_T1FS_INODE_MAGIC, M0_T1FS_INODE_HEAD_MAGIC);

M0_TL_DEFINE(csb_inodes, M0_INTERNAL, struct m0t1fs_inode);

M0_INTERNAL void m0t1fs_fs_lock(struct m0t1fs_sb *csb)
{
	M0_ENTRY();
	m0_mutex_lock(&csb->csb_mutex);
	M0_LEAVE();
}

M0_INTERNAL void m0t1fs_fs_unlock(struct m0t1fs_sb *csb)
{
	M0_ENTRY();
	m0_mutex_unlock(&csb->csb_mutex);
	M0_LEAVE();
}

M0_INTERNAL bool m0t1fs_fs_is_locked(const struct m0t1fs_sb *csb)
{
	return m0_mutex_is_locked(&csb->csb_mutex);
}

/**
 * If use_hint is true, use hash_hint as final hash. This is used
 * to get specified mdservice. For example, readdir() wants to
 * get session for specified mds index.
 */
M0_INTERNAL struct m0_rpc_session *
m0t1fs_filename_to_mds_session(const struct m0t1fs_sb *csb,
			       const unsigned char    *filename,
			       unsigned int            nlen,
			       bool                    use_hint,
			       uint32_t                hash_hint)
{
	struct m0_reqh_service_ctx   *ctx;
	const struct m0_pools_common *pc;
	unsigned long hash;
	M0_ENTRY();

	if (use_hint)
		hash = hash_hint;
	else {
		/* If operations don't have filename, we map it to mds 0 */
		if (filename != NULL && nlen != 0)
			hash = m0_full_name_hash(filename, nlen);
		else
			hash = 0;
	}
	pc = &csb->csb_pools_common;
	ctx = pc->pc_mds_map[hash % pc->pc_nr_svcs[M0_CST_MDS]];
	M0_ASSERT(ctx != NULL);

	M0_LOG(M0_DEBUG, "%8s->index=%llu ctx=%p session=%p",
	       (const char*)filename,
	       hash % pc->pc_nr_svcs[M0_CST_MDS], ctx, &ctx->sc_rlink.rlk_sess);
	M0_LEAVE();
	return &ctx->sc_rlink.rlk_sess;
}

/**
 * Mapping from container_id to ios session.
 * container_id 0 is not valid.
 */
M0_INTERNAL struct m0_rpc_session *
m0t1fs_container_id_to_session(const struct m0_pool_version *pver,
			       uint64_t container_id)
{
	struct m0_reqh_service_ctx *ctx;

	M0_ENTRY();
	M0_PRE(container_id < pver->pv_pc->pc_nr_devices);

	M0_LOG(M0_DEBUG, "container_id=%llu", container_id);

	ctx = pver->pv_pc->pc_dev2svc[container_id].pds_ctx;
	M0_ASSERT(ctx != NULL);

	M0_LOG(M0_DEBUG, "id %llu -> ctx=%p session=%p", container_id, ctx,
			 &ctx->sc_rlink.rlk_sess);
	M0_LEAVE();
	return &ctx->sc_rlink.rlk_sess;
}

static int _fs_stats_fetch(struct m0t1fs_sb *csb, struct m0_fs_stats *stats)
{
	struct m0_spiel_core spc = {
		.spc_profile   = *m0_reqh2profile(&csb->csb_reqh),
		.spc_rmachine  = &csb->csb_rpc_machine,
		.spc_confc     = m0_reqh2confc(&csb->csb_reqh),
	};

	M0_ENTRY();
	M0_ASSERT(spc.spc_rmachine != NULL);
	return M0_RC(m0_spiel__fs_stats_fetch(&spc, stats));
}

static int m0t1fs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct m0t1fs_sb  *csb = M0T1FS_SB(dentry->d_sb);
	struct m0_fs_stats stats = {};
	int                rc;
	M0_THREAD_ENTER;
	M0_ENTRY();

	rc = m0t1fs_fs_conf_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);
	rc = _fs_stats_fetch(csb, &stats);
	if (rc == 0) {
		/**
		 * @todo According to stob_ad_domain_cfg_create_parse(), current
		 * block size forcibly defaulted by BALLOC_DEF_BLOCK_SHIFT
		 * value. Need to understand if it's ever going to be any
		 * different in future, and if so, whether it's worth to be
		 * exposed to fs client. Besides, what if the balloc setup is
		 * going to vary among segments/pools/etc.?
		 */
		buf->f_bsize   = 1 << BALLOC_DEF_BLOCK_SHIFT;

		/* space on disks */
		buf->f_blocks  = stats.fs_total_disk / buf->f_bsize;
		buf->f_bfree   = stats.fs_free_disk / buf->f_bsize;

		/* space in metadata segments (inodes) */
		buf->f_files   = stats.fs_total_seg / 512;
		buf->f_ffree   = stats.fs_free_seg / 512;

		buf->f_bavail  = stats.fs_avail_disk / buf->f_bsize;
		buf->f_namelen = M0T1FS_NAME_LEN;
		buf->f_type    = M0_T1FS_SUPER_MAGIC;
	}
	m0t1fs_fs_conf_unlock(csb);
	return M0_RC(rc);
}

/* ----------------------------------------------------------------
 * Mount options
 * ---------------------------------------------------------------- */

struct mount_opts {
	char     *mo_ha;
	char     *mo_profile;
	char     *mo_process_fid;
	char     *mo_ep;
	uint32_t  mo_fid_start;
};

enum m0t1fs_mntopts {
	M0T1FS_MNTOPT_CONFD,
	M0T1FS_MNTOPT_PROFILE,
	M0T1FS_MNTOPT_PROCESS_FID,
	M0T1FS_MNTOPT_FID_START,
	M0T1FS_MNTOPT_EP,
	M0T1FS_MNTOPT_OOSTORE,
	M0T1FS_MNTOPT_VERIFY,
	M0T1FS_MNTOPT_ERR
};

static const match_table_t m0t1fs_mntopt_tokens = {
	{ M0T1FS_MNTOPT_CONFD,      "ha=%s"         },
	{ M0T1FS_MNTOPT_PROFILE,    "profile=%s"    },
	{ M0T1FS_MNTOPT_PROCESS_FID,"pfid=%s"    },
	{ M0T1FS_MNTOPT_FID_START,  "fid_start=%s"  },
	{ M0T1FS_MNTOPT_EP,         "ep=%s"         },
	{ M0T1FS_MNTOPT_OOSTORE,    "oostore"       },
	{ M0T1FS_MNTOPT_VERIFY,     "verify"        },
	/* match_token() requires 2nd field of the last element to be NULL */
	{ M0T1FS_MNTOPT_ERR, NULL }
};

static void mount_opts_fini(struct mount_opts *mops)
{
	M0_ENTRY();

	/* Here we use kfree() instead of m0_free() because the memory
	 * was allocated using match_strdup(). */
	kfree(mops->mo_ha);
	kfree(mops->mo_profile);
	kfree(mops->mo_process_fid);
	kfree(mops->mo_ep);
	M0_SET0(mops);

	M0_LEAVE();
}

static int str_parse(char **dest, const substring_t *src)
{
	*dest = match_strdup(src);
	return *dest == NULL ? -ENOMEM : 0;
}

static int num_parse(uint32_t *dest, const substring_t *src)
{
	unsigned long n;
	char         *s;
	int           rc;

	s = match_strdup(src);
	if (s == NULL)
		return M0_ERR(-ENOMEM);

	rc = kstrtoul(s, 10, &n);
	if (rc == 0) {
		if (n > UINT32_MAX)
			rc = -EINVAL;
		else
			*dest = (uint32_t)n;
	}

	kfree(s);
	return M0_RC(rc);
}

static bool is_empty(const char *s)
{
	return s == NULL || *s == '\0';
}

static int mount_opts_validate(const struct mount_opts *mops)
{
	if (is_empty(mops->mo_ha))
		return M0_ERR_INFO(-EINVAL,
				   "Mandatory parameter is missing: ha");
	if (is_empty(mops->mo_profile))
		return M0_ERR_INFO(-EINVAL,
				   "Mandatory parameter is missing: profile");
	if (is_empty(mops->mo_process_fid))
		return M0_ERR_INFO(-EINVAL,
				   "Mandatory parameter is missing: pfid");
	if (mops->mo_fid_start != 0 &&
	    mops->mo_fid_start <= M0_MDSERVICE_START_FID.f_key - 1)
		return M0_ERR_INFO(-EINVAL,
				   "fid_start must be greater than %llu",
				   M0_MDSERVICE_START_FID.f_key - 1);
	return M0_RC(0);
}

static int mount_opts_parse(struct m0t1fs_sb *csb, char *options,
			    struct mount_opts *dest)
{
	substring_t args[MAX_OPT_ARGS];
	char       *op;
	int         rc = 0;

	M0_ENTRY();

	if (options == NULL)
		return M0_RC(-EINVAL);

	M0_LOG(M0_INFO, "Mount options: `%s'", options);

	M0_SET0(dest);
	dest->mo_fid_start = M0_MDSERVICE_START_FID.f_key; /* default value */

	while ((op = strsep(&options, ",")) != NULL && *op != '\0') {
		switch (match_token(op, m0t1fs_mntopt_tokens, args)) {
		case M0T1FS_MNTOPT_CONFD:
			rc = str_parse(&dest->mo_ha, args);
			if (rc != 0)
				goto out;
			M0_LOG(M0_INFO, "HA: %s", dest->mo_ha);
			break;

		case M0T1FS_MNTOPT_PROFILE:
			rc = str_parse(&dest->mo_profile, args);
			if (rc != 0)
				goto out;
			M0_LOG(M0_INFO, "profile: %s", dest->mo_profile);
			break;

		case M0T1FS_MNTOPT_PROCESS_FID:
			rc = str_parse(&dest->mo_process_fid, args);
			if (rc != 0)
				goto out;
			M0_LOG(M0_INFO, "pfid: %s", dest->mo_process_fid);
			break;

		case M0T1FS_MNTOPT_FID_START:
			rc = num_parse(&dest->mo_fid_start, args);
			if (rc != 0)
				goto out;
			M0_LOG(M0_INFO, "fid-start: %lu",
				(unsigned long)dest->mo_fid_start);
			break;

		case M0T1FS_MNTOPT_EP:
			rc = str_parse(&dest->mo_ep, args);
			if (rc != 0)
				goto out;
			M0_LOG(M0_INFO, "ep: %s", dest->mo_ep);
			break;

		case M0T1FS_MNTOPT_OOSTORE:
			csb->csb_oostore = true;
			M0_LOG(M0_DEBUG, "OOSTORE mode!!");
			break;
		case M0T1FS_MNTOPT_VERIFY:
			csb->csb_verify = true;
			M0_LOG(M0_DEBUG, "Parity verify mode!!");
			break;
		default:
			return M0_ERR_INFO(-EINVAL, "Unsupported option: %s", op);
		}
	}
out:
	/*
	 * If there is an error, the allocated memory will be freed by
	 * mount_opts_fini(), called by m0t1fs_fill_super().
	 */
	return M0_RC(rc ?: mount_opts_validate(dest));
}


/* ----------------------------------------------------------------
 * Superblock
 * ---------------------------------------------------------------- */

static void ast_thread(struct m0t1fs_sb *csb);
static void ast_thread_stop(struct m0t1fs_sb *csb);
static void m0t1fs_obf_dealloc(struct m0t1fs_sb *csb);

M0_INTERNAL void m0t1fs_sb_init(struct m0t1fs_sb *csb)
{
	M0_ENTRY("csb = %p", csb);
	M0_PRE(csb != NULL);

	M0_SET0(csb);
	m0_mutex_init(&csb->csb_mutex);
	m0_sm_group_init(&csb->csb_iogroup);
	csb->csb_active = true;
	m0_chan_init(&csb->csb_iowait, &csb->csb_iogroup.s_lock);
	m0_atomic64_set(&csb->csb_pending_io_nr, 0);
	csb->csb_oostore = false;
	csb->csb_verify  = false;
	csb->csb_reqs_nr = 0;
	csb->csb_confc_state.cus_state = M0_CC_REVOKED;
	m0_mutex_init(&csb->csb_confc_state.cus_lock);
	m0_mutex_init(&csb->csb_inodes_lock);
	csb_inodes_tlist_init(&csb->csb_inodes);
	m0_chan_init(&csb->csb_conf_ready_chan, &csb->csb_confc_state.cus_lock);
	M0_LEAVE();
}

M0_INTERNAL void m0t1fs_sb_fini(struct m0t1fs_sb *csb)
{
	M0_ENTRY();
	M0_PRE(csb != NULL);
	m0_chan_fini_lock(&csb->csb_conf_ready_chan);
	csb_inodes_tlist_fini(&csb->csb_inodes);
	m0_mutex_fini(&csb->csb_inodes_lock);
	m0_mutex_fini(&csb->csb_confc_state.cus_lock);
	m0_chan_fini_lock(&csb->csb_iowait);
	m0_sm_group_fini(&csb->csb_iogroup);
	m0_mutex_fini(&csb->csb_mutex);
	csb->csb_next_key = 0;
	csb->csb_confc_state.cus_state = M0_CC_REVOKED;
	M0_LEAVE();
}

/* ----------------------------------------------------------------
 * HA service connectivity
 * ---------------------------------------------------------------- */

static void m0t1fs_ha_process_event(struct m0t1fs_sb              *csb,
                                    enum m0_conf_ha_process_event  event)
{
	m0_conf_ha_process_event_post(&csb->csb_mero_ha.mh_ha,
	                              csb->csb_mero_ha.mh_link,
	                              &csb->csb_process_fid, 0,
				      event, M0_CONF_HA_PROCESS_KERNEL);
}

/**
 * Establishes rpc session to HA service. The session is set up to be used
 * globally.
 */
int m0t1fs_ha_init(struct m0t1fs_sb *csb, const char *ha_addr)
{
	struct m0_mero_ha_cfg mero_ha_cfg;
	int                   rc;

	M0_ENTRY();
	mero_ha_cfg = (struct m0_mero_ha_cfg){
		.mhc_dispatcher_cfg = {
			.hdc_enable_note      = true,
			.hdc_enable_keepalive = true,
			.hdc_enable_fvec      = true,
		},
		.mhc_addr           = ha_addr,
		.mhc_rpc_machine    = &csb->csb_rpc_machine,
		.mhc_reqh           = &csb->csb_reqh,
		.mhc_process_fid    = csb->csb_process_fid,
	};
	rc = m0_mero_ha_init(&csb->csb_mero_ha, &mero_ha_cfg);
	M0_ASSERT(rc == 0);
	rc = m0_mero_ha_start(&csb->csb_mero_ha);
	M0_ASSERT(rc == 0);
	m0_mero_ha_connect(&csb->csb_mero_ha);
	m0t1fs_ha_process_event(csb, M0_CONF_HA_PROCESS_STARTING);
	return M0_RC(0);
}

/**
 * Clears global HA session info and terminates rpc session to HA service.
 */
void m0t1fs_ha_fini(struct m0t1fs_sb *csb)
{
	M0_ENTRY("csb: %p", csb);
	m0t1fs_ha_process_event(csb, M0_CONF_HA_PROCESS_STOPPED);
	m0_mero_ha_disconnect(&csb->csb_mero_ha);
	m0_mero_ha_stop(&csb->csb_mero_ha);
	m0_mero_ha_fini(&csb->csb_mero_ha);

	M0_LEAVE();
}

/*
 * ----------------------------------------------------------------
 * Layout
 * ----------------------------------------------------------------
 */

static int m0t1fs_sb_mdpool_layouts_init(struct m0t1fs_sb *csb)
{
	M0_ENTRY();
	return M0_RC(m0_reqh_mdpool_layout_build(&csb->csb_reqh));
}

static void m0t1fs_sb_layouts_fini(struct m0t1fs_sb *csb)
{
	M0_ENTRY();
	m0_reqh_layouts_cleanup(&csb->csb_reqh);
	M0_LEAVE();
}

void m0t1fs_rm_service_quit(struct m0t1fs_sb *csb)
{
	m0_reqh_service_quit(csb->csb_rm_service);
}

int m0t1fs_rm_service_start(struct m0t1fs_sb *csb)
{
	struct m0_reqh *reqh = &csb->csb_reqh;
	struct m0_fid           sfid;
	int                     rc = 0;
	struct m0_fid           fake_pfid
		= M0_FID_TINIT(M0_CONF_PROCESS_TYPE.cot_ftype.ft_id, 0, 0);

	/*
	 * @todo Use fake process fid for init script based cluster setup.
	 *       Init script based setup not adding m0t1fs process
	 *       to configuration and also not passing m0t1fs process
	 *       FID to mount.
	 */
	if (!m0_fid_eq(&reqh->rh_fid, &fake_pfid)) {
		rc = m0_conf_process2service_get(&reqh->rh_rconfc.rc_confc,
						 &reqh->rh_fid, M0_CST_RMS,
						 &sfid);
		if (rc)
			return M0_RC(rc);
	} else {
		m0_uuid_generate((struct m0_uint128 *)&sfid);
		m0_fid_tassume(&sfid, &M0_CONF_SERVICE_TYPE.cot_ftype);
	}

	rc = m0_reqh_service_setup(&csb->csb_rm_service, &m0_rms_type,
				   reqh, NULL, &sfid);
	return rc == 0 ? M0_RC(rc) :
	       M0_ERR_INFO(rc, "failed to start RM service: rc=%d", rc);
}

int m0t1fs_net_init(struct m0t1fs_sb *csb, const char *ep)
{
	struct m0_net_xprt   *xprt;
	struct m0_net_domain *ndom;
	int                   rc;
	char                 *laddr = NULL;

	M0_ENTRY();
	if (ep == NULL) {
		laddr = m0_alloc(M0_NET_LNET_NIDSTR_SIZE * 2);
		if (laddr == NULL)
			return M0_RC(-ENOMEM);

		m0_mutex_lock(&m0t1fs_mutex);
		csb->csb_tmid = m0_bitmap_ffz(&m0t1fs_client_ep_tmid);
		if (csb->csb_tmid == ((size_t)-1)) {
			m0_mutex_unlock(&m0t1fs_mutex);
			m0_free(laddr);
			return M0_RC(-EMFILE);
		}
		m0_bitmap_set(&m0t1fs_client_ep_tmid, csb->csb_tmid, true);
		m0_mutex_unlock(&m0t1fs_mutex);

		snprintf(laddr, M0_NET_LNET_NIDSTR_SIZE * 2,
		         "%s%d", local_addr, (int)csb->csb_tmid);
		M0_LOG(M0_DEBUG, "local ep is %s", laddr);
		csb->csb_laddr = laddr;
	}
	csb->csb_xprt  = &m0_net_lnet_xprt;
	xprt           =  csb->csb_xprt;
	ndom           = &csb->csb_ndom;

	rc = m0_net_domain_init(ndom, xprt);
	if (rc != 0) {
		csb->csb_laddr = NULL;
		if (ep == NULL) {
			m0_free(laddr);
			m0_mutex_lock(&m0t1fs_mutex);
			m0_bitmap_set(&m0t1fs_client_ep_tmid,
				      csb->csb_tmid, false);
			m0_mutex_unlock(&m0t1fs_mutex);
		}
	}
	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

void m0t1fs_net_fini(struct m0t1fs_sb *csb)
{
	M0_ENTRY();

	m0_net_domain_fini(&csb->csb_ndom);
	m0_free(csb->csb_laddr);
	m0_mutex_lock(&m0t1fs_mutex);
	m0_bitmap_set(&m0t1fs_client_ep_tmid, csb->csb_tmid, false);
	m0_mutex_unlock(&m0t1fs_mutex);

	M0_LEAVE();
}

int m0t1fs_rpc_init(struct m0t1fs_sb *csb, const char *ep)
{
	struct m0_rpc_machine     *rpc_machine = &csb->csb_rpc_machine;
	struct m0_reqh            *reqh        = &csb->csb_reqh;
	struct m0_net_domain      *ndom        = &csb->csb_ndom;
	const char                *laddr;
	struct m0_net_buffer_pool *buffer_pool = &csb->csb_buffer_pool;
	struct m0_net_transfer_mc *tm;
	int                        rc;
	uint32_t		   bufs_nr;
	uint32_t		   tms_nr;

	M0_ENTRY();

	m0_be_ut_seg_init(&csb->csb_ut_seg,
			  &csb->csb_ut_be, 1ULL << 24);

	tms_nr = 1;
	bufs_nr = m0_rpc_bufs_nr(tm_recv_queue_min_len, tms_nr);

	rc = m0_rpc_net_buffer_pool_setup(ndom, buffer_pool,
					  bufs_nr, tms_nr);
	if (rc != 0)
		goto be_fini;

	rc = M0_REQH_INIT(reqh,
			  .rhia_dtm = (void*)1,
			  .rhia_db = csb->csb_ut_seg.bus_seg,
			  .rhia_mdstore = (void*)1,
			  .rhia_pc = &csb->csb_pools_common,
			  .rhia_fid = &csb->csb_process_fid);
	if (rc != 0)
		goto pool_fini;
	laddr = ep == NULL ? csb->csb_laddr : ep;
	rc = m0_rpc_machine_init(rpc_machine, ndom, laddr, reqh,
				 buffer_pool, M0_BUFFER_ANY_COLOUR,
				 max_rpc_msg_size, tm_recv_queue_min_len);
	if (rc != 0)
		goto reqh_fini;
	m0_reqh_start(reqh);
	tm = &rpc_machine->rm_tm;
	M0_ASSERT(tm->ntm_recv_pool == buffer_pool);
	return M0_RC(rc);
reqh_fini:
	m0_reqh_fini(reqh);
pool_fini:
	m0_rpc_net_buffer_pool_cleanup(buffer_pool);
be_fini:
	m0_be_ut_seg_fini(&csb->csb_ut_seg);
	return M0_ERR(rc);
}

struct m0t1fs_sb *reqh2sb(struct m0_reqh *reqh)
{
	return container_of(reqh, struct m0t1fs_sb, csb_reqh);
}

static struct m0t1fs_sb *rconfc2csb(struct m0_rconfc *rconfc)
{
	return reqh2sb(container_of(rconfc, struct m0_reqh, rh_rconfc));
}

M0_INTERNAL struct m0_rconfc *m0_csb2rconfc(struct m0t1fs_sb *csb)
{
	return &csb->csb_reqh.rh_rconfc;
}

M0_INTERNAL struct m0_confc *m0_csb2confc(struct m0t1fs_sb *csb)
{
	return &m0_csb2rconfc(csb)->rc_confc;
}

void m0t1fs_rpc_fini(struct m0t1fs_sb *csb)
{
	M0_ENTRY();

	m0_rpc_machine_fini(&csb->csb_rpc_machine);
	if (m0_reqh_state_get(&csb->csb_reqh) != M0_REQH_ST_STOPPED)
		m0_reqh_services_terminate(&csb->csb_reqh);
	m0_reqh_fini(&csb->csb_reqh);
	m0_rpc_net_buffer_pool_cleanup(&csb->csb_buffer_pool);
	m0_be_ut_seg_fini(&csb->csb_ut_seg);

	M0_LEAVE();
}

/*
 * XXX Under various scenarios it's possible that the following function blocks
 * indefinitely. MERO-2341 addresses the issue by taking into consideration all
 * the possibilities.
 */
M0_INTERNAL int m0t1fs_ref_get_lock(struct m0t1fs_sb *csb)
{
	struct m0_clink clink;
	int             rc;

	M0_ENTRY("csb=%p", csb);
	while (1) {
		m0_mutex_lock(&csb->csb_confc_state.cus_lock);
		if (M0_IN(csb->csb_confc_state.cus_state,
			  (M0_CC_READY, M0_CC_FAILED)))
			break;
		m0_clink_init(&clink, NULL);
		m0_clink_add(&csb->csb_conf_ready_chan, &clink);

		m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
		/* Wait till configuration is updated. */
		m0_chan_wait(&clink);

		m0_clink_del_lock(&clink);
		m0_clink_fini(&clink);
	}
	M0_CNT_INC(csb->csb_reqs_nr);
	rc = csb->csb_confc_state.cus_state == M0_CC_READY ? 0 : -ESTALE;
	m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
	return M0_RC(rc);
}

M0_INTERNAL void m0t1fs_ref_put_lock(struct m0t1fs_sb *csb)
{
	m0_mutex_lock(&csb->csb_confc_state.cus_lock);
	M0_CNT_DEC(csb->csb_reqs_nr);
	if (csb->csb_reqs_nr == 0 &&
	    csb->csb_confc_state.cus_state == M0_CC_GETTING_READY)
		conf_ready_async_cb_locked(csb);
	m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
}

static bool m0t1fs_conf_expired_cb(struct m0_clink *clink)
{
	struct m0t1fs_sb           *csb = container_of(clink, struct m0t1fs_sb,
						       csb_conf_exp);
	struct m0_reqh_service_ctx *ctx;
	uint32_t                    i;

	M0_ENTRY("super %p", csb);
	if (csb->csb_reqh.rh_rconfc.rc_stopping)
		return true;

	m0_mutex_lock(&csb->csb_confc_state.cus_lock);
	/*
	 * Close the gate for new io and metadata requests.
	 */
	csb->csb_confc_state.cus_state = M0_CC_REVOKED;
	m0_mutex_unlock(&csb->csb_confc_state.cus_lock);

	/*
	 * Cancel sessions to IO services that were used during IO request
	 * handling. This is done in order to call rio_replied callback for
	 * the rpc items and interrupt io_requests.
	 * A cancelled session is reconnected. See MERO-1642.
	 *
	 * Note: m0t1fs doesn't use CAS services, so they are not of interest.
	 */
	for (i = 0; i < csb->csb_pools_common.pc_nr_devices; i++) {
		ctx = csb->csb_pools_common.pc_dev2svc[i].pds_ctx;
		if (ctx != NULL && ctx->sc_type == M0_CST_IOS
		    && ctx->sc_rlink.rlk_connected)
			m0_reqh_service_cancel_reconnect(ctx);
	}

	M0_LEAVE();
	return true;
}

static void inodes_layout_ref_drop(struct m0t1fs_sb *csb)
{
	struct m0t1fs_inode    *ci;
	struct m0_pool_version *pver;

	m0_mutex_lock(&csb->csb_inodes_lock);
	m0_tl_for(csb_inodes, &csb->csb_inodes, ci) {
		pver = m0_pool_version_find(&csb->csb_pools_common,
					    &ci->ci_pver);
		if (pver != NULL && !pver->pv_is_stale)
			continue;
		m0_mutex_lock(&ci->ci_layout_lock);
		if (ci->ci_layout_instance != NULL) {
			m0_layout_instance_fini(ci->ci_layout_instance);
			ci->ci_layout_instance = NULL;
		}
		m0_mutex_unlock(&ci->ci_layout_lock);
	} m0_tl_endfor;
	m0_mutex_unlock(&csb->csb_inodes_lock);
}

static bool m0t1fs_conf_ready_async_cb(struct m0_clink *clink)
{
	struct m0t1fs_sb *csb = M0_AMB(csb, clink, csb_conf_ready_async);

	M0_ENTRY("super %p", csb);

	m0_mutex_lock(&csb->csb_confc_state.cus_lock);
	if (csb->csb_reqs_nr == 0 &&
	    csb->csb_confc_state.cus_state == M0_CC_GETTING_READY)
		conf_ready_async_cb_locked(csb);
	m0_mutex_unlock(&csb->csb_confc_state.cus_lock);

	M0_LEAVE();
	return true;
}

static void conf_ready_async_cb_locked(struct m0t1fs_sb *csb)
{
	struct m0_clink *pc_clink = &csb->csb_pools_common.pc_conf_ready_async;

	M0_PRE(m0_mutex_is_locked(&csb->csb_confc_state.cus_lock));

	m0_pool_versions_stale_mark(&csb->csb_pools_common,
				    &csb->csb_confc_state);
	inodes_layout_ref_drop(csb);
	/*
	 * During m0t1fs_setup() pools_common stays uninitialized
	 * at this point.
	 */
	if (csb->csb_pools_common.pc_confc != NULL)
		m0_pools_common_conf_ready_async_cb(pc_clink);
	csb->csb_confc_state.cus_state = M0_CC_READY;
	m0_chan_broadcast(&csb->csb_conf_ready_chan);
}

/**
 * When configuration is revoked, m0t1fs_sb::csb_confc_state changes to
 * M0_CC_REVOKED. Subsequent io requests do not update
 * m0t1fs_sb::csb_reqs_nr, and instead wait on m0t1fs_sb::csb_conf_ready_chan,
 * till m0t1fs_sb returns to M0_CC_READY. Any io request that is not waiting and
 * has registered its presence by incrementing m0t1fs_sb::csb_reqs_nr is called
 * as an active request. An active request continues with its usual
 * flow of operations even when state of m0t1fs_sb changes to M0_CC_REVOKED.
 *
 * When new configuration becomes available, in-memory representations of
 * stale pool versions need to be removed. In order to do this it is
 * necessary to ensure that there are no active requests, since they are the
 * potential users of these pool versions.
 *
 * On availability of the new configuration the callback works in two phases.
 * Callback that's triggered on m0_reqh::rh_conf_cache_ready channel changes
 * the state of m0t1fs_sb to M0_CC_GETTING_READY. When a callback on
 * m0_reqh::rh_conf_cache_ready_async is triggered, it checks if any active
 * requests are present and cleans the stale pool versions in case there are
 * no active requests. When an active request is present this callback
 * returns without removing stale pool versions. In this case the last active
 * request clears the stale pool versions before leaving.
 *
 * The process of clearing stale pool versions also involves removal of layouts
 * associated with them from the layout domain, and updating the state of
 * m0t1fs_sb to M0_CC_READY, before signalling the requests waiting on
 * m0t1fs_sb::csb_conf_ready_chan.
 */
static bool m0t1fs_conf_ready_cb(struct m0_clink *clink)
{
	struct m0t1fs_sb *csb = M0_AMB(csb, clink, csb_conf_ready);
	m0_mutex_lock(&csb->csb_confc_state.cus_lock);
	csb->csb_confc_state.cus_state = M0_CC_GETTING_READY;
	m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
	return true;
}

static void m0t1fs_rconfc_fatal_cb(struct m0_rconfc *rconfc)
{
	struct m0t1fs_sb *csb = rconfc2csb(rconfc);

	M0_ENTRY("rconfc %p", rconfc);
	m0_mutex_lock(&csb->csb_confc_state.cus_lock);
	csb->csb_confc_state.cus_state = M0_CC_FAILED;
	m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
	m0_chan_broadcast_lock(&csb->csb_conf_ready_chan);
	M0_LEAVE();
}

static int m0t1fs_fid_sscanf(const char    *s,
			     struct m0_fid *fid,
			     const char    *descr)
{
	int rc = m0_fid_sscanf(s, fid);

	if (rc != 0)
		return M0_ERR_INFO(rc, "can't m0_fid_sscanf() %s %s", descr, s);
	return M0_RC(0);
}

int m0t1fs_setup(struct m0t1fs_sb *csb, const struct mount_opts *mops)
{
	struct m0_addb2_sys       *sys = m0_addb2_global_get();
	struct m0_pools_common    *pc = &csb->csb_pools_common;
	struct m0_confc_args      *confc_args;
	struct m0_reqh            *reqh = &csb->csb_reqh;
	struct m0_conf_root       *root;
	struct m0_pool_version    *pv;
	int                        rc;

	M0_ENTRY();
	M0_PRE(csb->csb_astthread.t_state == TS_RUNNING);

	rc = m0t1fs_fid_sscanf(mops->mo_process_fid, &csb->csb_process_fid,
	                       "process fid");
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0t1fs_fid_sscanf(mops->mo_profile, &csb->csb_profile_fid,
	                       "profile");
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0t1fs_net_init(csb, mops->mo_ep);
	if (rc != 0)
		return M0_ERR(rc);

	rc = m0t1fs_rpc_init(csb, mops->mo_ep);
	if (rc != 0)
		goto err_net_fini;

	csb->csb_next_key = mops->mo_fid_start;

	confc_args = &(struct m0_confc_args) {
		.ca_profile = mops->mo_profile,
		.ca_rmach   = &csb->csb_rpc_machine,
		.ca_group   = m0_locality0_get()->lo_grp,
	};

	rc = m0t1fs_ha_init(csb, mops->mo_ha);
	if (rc != 0)
		goto err_rpc_fini;

	rc = m0_reqh_conf_setup(reqh, confc_args);
	if (rc != 0)
		goto err_ha_fini;
	m0_clink_init(&csb->csb_conf_exp, m0t1fs_conf_expired_cb);
	m0_clink_init(&csb->csb_conf_ready, m0t1fs_conf_ready_cb);
	m0_clink_init(&csb->csb_conf_ready_async, m0t1fs_conf_ready_async_cb);
	/*
	 * Note: csb clinks shall be registered with conf update channels before
	 * pools_common's clinks. @see m0_reqh.
	 */
	m0_clink_add_lock(&reqh->rh_conf_cache_exp, &csb->csb_conf_exp);
	m0_clink_add_lock(&reqh->rh_conf_cache_ready, &csb->csb_conf_ready);
	m0_clink_add_lock(&reqh->rh_conf_cache_ready_async,
			  &csb->csb_conf_ready_async);

	rc = m0_rconfc_start_sync(m0_csb2rconfc(csb));
	if (rc != 0)
		goto err_rconfc_stop;
	rc = m0_ha_client_add(m0_reqh2confc(reqh));
	if (rc != 0)
		goto err_rconfc_stop;
	rc = m0_confc_root_open(m0_reqh2confc(reqh), &root);
	if (rc != 0)
		goto err_ha_client_del;

	rc = m0_conf_full_load(root);
	if (rc != 0)
		goto err_conf_fs_close;
	rc = m0_conf_confc_ha_update(m0_reqh2confc(reqh));
	if (rc != 0)
		goto err_conf_fs_close;

	rc = m0_pools_common_init(pc, &csb->csb_rpc_machine);
	if (rc != 0)
		goto err_conf_fs_close;
	M0_ASSERT(ergo(csb->csb_oostore, pc->pc_md_redundancy > 0));

	rc = m0_pools_setup(pc, &csb->csb_profile_fid, NULL, NULL);
	if (rc != 0)
		goto err_pools_common_fini;

	rc = m0_pools_service_ctx_create(pc);
	if (rc != 0)
		goto err_pools_destroy;
	m0_pools_common_service_ctx_connect_sync(pc);

	rc = m0_pool_versions_setup(pc);
	if (rc != 0)
		goto err_pools_service_ctx_destroy;

	/* Find pool and pool version to use. */
	rc = m0_pool_version_get(&csb->csb_pools_common, NULL, &pv);
	if (!M0_IN(rc, (0, -ENOENT)))
		goto err_pool_versions_destroy;

	rc = m0t1fs_rm_service_start(csb);
	if (rc != 0)
		goto err_pool_versions_destroy;

	rc = m0t1fs_sb_mdpool_layouts_init(csb);
	if (rc != 0)
		goto err_rm_service_quit;

	rc = m0_addb2_sys_net_start_with(sys, &pc->pc_svc_ctxs);
	if (rc == 0) {
		m0_confc_close(&root->rt_obj);
		m0_rconfc_lock(m0_csb2rconfc(csb));
		m0_rconfc_fatal_cb_set(m0_csb2rconfc(csb),
				       m0t1fs_rconfc_fatal_cb);
		m0_rconfc_unlock(m0_csb2rconfc(csb));
		return M0_RC(0);
	}

err_rm_service_quit:
	m0t1fs_rm_service_quit(csb);
err_pool_versions_destroy:
	m0t1fs_sb_layouts_fini(csb);
	m0_pool_versions_destroy(&csb->csb_pools_common);
err_pools_service_ctx_destroy:
	m0_pools_service_ctx_destroy(&csb->csb_pools_common);
err_pools_destroy:
	m0_pools_destroy(&csb->csb_pools_common);
err_pools_common_fini:
	m0_pools_common_fini(&csb->csb_pools_common);
err_conf_fs_close:
	m0_confc_close(&root->rt_obj);
err_ha_client_del:
	m0_ha_client_del(m0_reqh2confc(reqh));
err_rconfc_stop:
	m0_rconfc_stop_sync(m0_csb2rconfc(csb));
	m0_rconfc_fini(m0_csb2rconfc(csb));
	m0_clink_del_lock(&csb->csb_conf_exp);
	m0_clink_del_lock(&csb->csb_conf_ready);
	m0_clink_del_lock(&csb->csb_conf_ready_async);
	m0_clink_fini(&csb->csb_conf_exp);
	m0_clink_fini(&csb->csb_conf_ready);
	m0_clink_fini(&csb->csb_conf_ready_async);
err_ha_fini:
	m0t1fs_ha_fini(csb);
err_rpc_fini:
	/*
	 * Reqh services are terminated not in reverse order because
	 * m0_reqh_services_terminate() terminates all services including
	 * rpc_service. Rpc_service starts in m0t1fs_rpc_init() implicitly.
	 */
	m0_reqh_services_terminate(reqh);
	m0t1fs_rpc_fini(csb);
err_net_fini:
	m0t1fs_net_fini(csb);

	return M0_ERR(rc);
}

static void m0t1fs_teardown(struct m0t1fs_sb *csb)
{
	m0_addb2_sys_net_stop(m0_addb2_global_get());
	m0t1fs_sb_layouts_fini(csb);
	/* @todo Make a separate unconfigure api and do this in that */
	m0_pool_versions_destroy(&csb->csb_pools_common);
	m0_pools_service_ctx_destroy(&csb->csb_pools_common);
	m0_pools_destroy(&csb->csb_pools_common);
	m0_pools_common_fini(&csb->csb_pools_common);
	m0_ha_client_del(m0_reqh2confc(&csb->csb_reqh));
	m0_rconfc_stop_sync(m0_csb2rconfc(csb));
	m0_rconfc_fini(m0_csb2rconfc(csb));
	m0_clink_del_lock(&csb->csb_conf_exp);
	m0_clink_del_lock(&csb->csb_conf_ready);
	m0_clink_del_lock(&csb->csb_conf_ready_async);
	m0_clink_fini(&csb->csb_conf_exp);
	m0_clink_fini(&csb->csb_conf_ready);
	m0_clink_fini(&csb->csb_conf_ready_async);
	/*
	 * Finalise explicitly to send HA service events STOPPING/STOPPED.
	 * See MERO-2427 for the reference.
	 * @see m0_conf_ha_service_event, m0_conf_ha_service_event_post().
	 */
	m0t1fs_rm_service_quit(csb);
	m0t1fs_ha_fini(csb);
	m0_reqh_services_terminate(&csb->csb_reqh);
	m0t1fs_rpc_fini(csb);
	m0t1fs_net_fini(csb);
}

static void m0t1fs_dput(struct dentry *dentry)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	clear_nlink(dentry->d_inode);
#else
	dentry->d_inode->i_nlink = 0;
#endif
	d_delete(dentry);
	dput(dentry);
}

static void m0t1fs_obf_dealloc(struct m0t1fs_sb *csb) {
	M0_ENTRY();

	M0_PRE(csb != NULL);

	if (csb->csb_fid_dentry != NULL) {
		m0t1fs_dput(csb->csb_fid_dentry);
		csb->csb_fid_dentry = NULL;
	}
	if (csb->csb_mero_dentry != NULL) {
		m0t1fs_dput(csb->csb_mero_dentry);
		csb->csb_mero_dentry = NULL;
	}

	M0_LEAVE();
}

M0_INTERNAL int m0t1fs_fill_cob_attr(struct m0_fop_cob *body)
{
	struct m0t1fs_sb *csb = M0_AMB(csb, body, csb_virt_body);

	M0_PRE(body != NULL);

	body->b_atime = body->b_ctime = body->b_mtime =
					m0_time_seconds(m0_time_now());
	body->b_valid = (M0_COB_MTIME | M0_COB_CTIME | M0_COB_ATIME |
	                 M0_COB_UID | M0_COB_GID | M0_COB_BLOCKS |
	                 M0_COB_SIZE | M0_COB_NLINK | M0_COB_MODE |
	                 M0_COB_BLKSIZE | M0_COB_LID | M0_COB_PVER);
        body->b_blocks = 16;
        body->b_size = 4096;
        body->b_blksize = 4096;
        body->b_nlink = 2;
        body->b_lid = M0_DEFAULT_LAYOUT_ID;
        body->b_mode = (S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR |/*rwx for owner*/
                        S_IRGRP | S_IXGRP |                    /*r-x for group*/
                        S_IROTH | S_IXOTH);

	return M0_RC(0);
}

static int m0t1fs_obf_alloc(struct super_block *sb)
{
        struct inode             *mero_inode;
        struct dentry            *mero_dentry;
        struct inode             *fid_inode;
        struct dentry            *fid_dentry;
        struct m0t1fs_sb         *csb = M0T1FS_SB(sb);
	struct m0_fop_cob        *body = &csb->csb_virt_body;
	int                       rc;

	M0_ENTRY();

	rc = m0t1fs_fill_cob_attr(body);
	if (rc != 0)
		return M0_ERR(rc);

        /* Init virtual .mero directory */
        mero_dentry = d_alloc_name(sb->s_root, M0_DOT_MERO_NAME);
        if (mero_dentry == NULL)
                return M0_RC(-ENOMEM);

	m0t1fs_fs_lock(csb);
	mero_inode = m0t1fs_iget(sb, &M0_DOT_MERO_FID, body);
	m0t1fs_fs_unlock(csb);
        if (IS_ERR(mero_inode)) {
                dput(mero_dentry);
		M0_LOG(M0_ERROR, "m0t1fs_iget(M0_DOT_MERO_FID) failed, rc=%d",
		       (int)PTR_ERR(mero_inode));
                return M0_RC((int)PTR_ERR(mero_inode));
        }

        /* Init virtual .mero/fid directory */
        fid_dentry = d_alloc_name(mero_dentry, M0_DOT_MERO_FID_NAME);
        if (fid_dentry == NULL) {
                iput(mero_inode);
                dput(mero_dentry);
		M0_LOG(M0_ERROR, "m0t1fs_iget(M0_DOT_MERO_FID_NAME) "
		       "failed, rc=%d", -ENOMEM);
                return M0_RC(-ENOMEM);
        }

	m0t1fs_fs_lock(csb);
	fid_inode = m0t1fs_iget(sb, &M0_DOT_MERO_FID_FID, body);
	m0t1fs_fs_unlock(csb);
        if (IS_ERR(fid_inode)) {
                dput(fid_dentry);
                iput(mero_inode);
                dput(mero_dentry);
		M0_LOG(M0_ERROR, "m0t1fs_iget(M0_DOT_MERO_FID_FID) "
		       "failed, rc=%d", (int)PTR_ERR(fid_inode));
                return M0_RC((int)PTR_ERR(fid_inode));
        }

	d_add(mero_dentry, mero_inode);
	csb->csb_mero_dentry = mero_dentry;

	d_add(fid_dentry, fid_inode);
	csb->csb_fid_dentry = fid_dentry;

	return M0_RC(0);
}

static void service_connect_wait(struct m0t1fs_sb *csb)
{
	struct m0t1fs_mdop          mo;
	struct m0_rpc_session      *session;
	struct m0_reqh_service_ctx *ctx;

	M0_SET0(&mo);
	session = m0t1fs_filename_to_mds_session(csb,
			mo.mo_attr.ca_name.b_addr,
			mo.mo_attr.ca_name.b_nob,
			mo.mo_use_hint,
			mo.mo_hash_hint);
	ctx = m0_reqh_service_ctx_from_session(session);
	m0_reqh_service_connect_wait(ctx);
}

static int m0t1fs_root_alloc(struct super_block *sb)
{
	struct inode             *root_inode;
	int                       rc = 0;
	struct m0t1fs_sb         *csb = M0T1FS_SB(sb);
	struct m0_fop_statfs_rep *rep = NULL;
	struct m0_fop            *rep_fop = NULL;
	M0_THREAD_ENTER;

	M0_ENTRY();

	if (!csb->csb_oostore) {
		service_connect_wait(csb);
		rc = m0t1fs_mds_statfs(csb, &rep_fop);
		if (rc != 0)
			goto out;
		rep = m0_fop_data(rep_fop);
		sb->s_magic = rep->f_type;
		csb->csb_namelen = rep->f_namelen;

		M0_LOG(M0_DEBUG, "Got mdservice root "FID_F,
				FID_P(&rep->f_root));
	} else
		csb->csb_namelen = M0T1FS_NAME_LEN;

	m0t1fs_fs_lock(csb);
	root_inode = m0t1fs_root_iget(sb, csb->csb_oostore ? &M0_ROOT_FID :
							     &rep->f_root);
	m0t1fs_fs_unlock(csb);
	if (IS_ERR(root_inode)) {
		rc = (int)PTR_ERR(root_inode);
		goto out;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	sb->s_root = d_make_root(root_inode);
#else
	sb->s_root = d_alloc_root(root_inode);
#endif
	if (sb->s_root == NULL) {
		iput(root_inode);
		rc = -ENOMEM;
		goto out;
	}

out:
	m0_fop_put0_lock(rep_fop);
	return M0_RC(rc);
}

static int m0t1fs_fill_super(struct super_block *sb, void *data,
			     int silent __attribute__((unused)))
{
	struct m0t1fs_sb *csb;
	int               rc;
	struct mount_opts mops = {0};

	M0_ENTRY();

	if (addb2_net_disable != 0)
		m0_get()->i_disable_addb2_storage = true;

	M0_ALLOC_PTR(csb);
	if (csb == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto end;
	}
	m0t1fs_sb_init(csb);
	csb->csb_sb = sb;
	rc = mount_opts_parse(csb, data, &mops);
	if (rc != 0)
		goto sb_fini;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	rc = bdi_init(&csb->csb_backing_dev_info);
	if (rc != 0)
		goto sb_fini;
#endif

	rc = M0_THREAD_INIT(&csb->csb_astthread, struct m0t1fs_sb *, NULL,
			    &ast_thread, csb, "m0_ast_thread");
	if (rc != 0)
		goto sb_fini;

	rc = m0t1fs_setup(csb, &mops);
	if (rc != 0)
		goto thread_stop;

	sb->s_fs_info        = csb;
	sb->s_blocksize      = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;
	sb->s_maxbytes       = MAX_LFS_FILESIZE;
	sb->s_op             = &m0t1fs_super_operations;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	/* for .sync_fs() callback to be called by kernel */
	sb->s_bdi = NULL;
	rc = bdi_register_dev(&csb->csb_backing_dev_info, sb->s_dev);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "bdi_register_dev() failed, rc=%d", rc);
		goto m0t1fs_teardown;
	}
	sb->s_bdi = &csb->csb_backing_dev_info;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	sb->s_xattr = m0t1fs_xattr_handlers;
#endif
	rc = m0t1fs_root_alloc(sb);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "m0t1fs_root_alloc() failed, rc=%d", rc);
		goto m0t1fs_teardown;
	}

        rc = m0t1fs_obf_alloc(sb);
        if (rc != 0) {
		M0_LOG(M0_ERROR, "m0t1fs_obf_alloc() failed, rc=%d", rc);
                goto m0t1fs_teardown;
        }

	io_bob_tlists_init();
	M0_SET0(&iommstats);

	m0t1fs_ha_process_event(csb, M0_CONF_HA_PROCESS_STARTED);

	return M0_RC(0);

m0t1fs_teardown:
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	if (sb->s_bdi != NULL)
		bdi_unregister(sb->s_bdi);
#endif
	m0t1fs_teardown(csb);
thread_stop:
	ast_thread_stop(csb);
sb_fini:
	m0t1fs_sb_fini(csb);
	m0_free(csb);
	mount_opts_fini(&mops);
end:
	sb->s_fs_info = NULL;
	return M0_ERR(rc);
}

/** Implementation of file_system_type::get_sb() interface. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
M0_INTERNAL struct dentry *m0t1fs_mount(struct file_system_type *fstype,
					int flags, const char *devname,
					void *data)
#else
M0_INTERNAL int m0t1fs_get_sb(struct file_system_type *fstype, int flags,
			      const char *devname, void *data,
			      struct vfsmount *mnt)
#endif
{
	M0_THREAD_ENTER;
	M0_ENTRY("flags: 0x%x, devname: %s, data: %s", flags, devname,
		 (char *)data);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	return mount_nodev(fstype, flags, data, m0t1fs_fill_super);
#else
	return M0_RC(get_sb_nodev(fstype, flags, data, m0t1fs_fill_super, mnt));
#endif
}

/** Implementation of file_system_type::kill_sb() interface. */
M0_INTERNAL void m0t1fs_kill_sb(struct super_block *sb)
{
	struct m0t1fs_sb *csb = M0T1FS_SB(sb);

	M0_THREAD_ENTER;
	M0_ENTRY("csb = %p", csb);

	if (csb != NULL)
		m0t1fs_ha_process_event(csb, M0_CONF_HA_PROCESS_STOPPING);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	if (sb->s_bdi != NULL)
		bdi_unregister(sb->s_bdi);
#endif

	/*
	 * Dealloc virtual .mero/fid dirs. This should be done _before_
	 * kill_anon_super()
	 */
	if (csb != NULL)
		m0t1fs_obf_dealloc(csb);

	kill_anon_super(sb);

	/*
	 * If m0t1fs_fill_super() fails then deactivate_locked_super() calls
	 * m0t1fs_fs_type->kill_sb(). In that case, csb == NULL.
	 * But still not sure, such csb != NULL handling is a good idea.
	 */
	if (csb != NULL) {
		m0t1fs_teardown(csb);
		ast_thread_stop(csb);
		m0t1fs_sb_fini(csb);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
		bdi_destroy(&csb->csb_backing_dev_info);
#endif
		m0_free(csb);
	}

	M0_LOG(M0_INFO, "mem stats :\n a_ioreq_nr = %llu, d_ioreq_nr = %llu\n"
			"a_pargrp_iomap_nr = %llu, d_pargrp_iomap_nr = %llu\n"
			"a_target_ioreq_nr = %llu, d_target_ioreq_nr = %llu\n",
	       iommstats.a_ioreq_nr, iommstats.d_ioreq_nr,
	       iommstats.a_pargrp_iomap_nr, iommstats.d_pargrp_iomap_nr,
	       iommstats.a_target_ioreq_nr, iommstats.d_target_ioreq_nr);

	M0_LOG(M0_INFO, "a_io_req_fop_nr = %llu, d_io_req_fop_nr = %llu\n"
			"a_data_buf_nr = %llu, d_data_buf_nr = %llu\n"
			"a_page_nr = %llu, d_page_nr = %llu\n",
	       iommstats.a_io_req_fop_nr, iommstats.d_io_req_fop_nr,
	       iommstats.a_data_buf_nr, iommstats.d_data_buf_nr,
	       iommstats.a_page_nr, iommstats.d_page_nr);

	M0_LEAVE();
}


/* ----------------------------------------------------------------
 * Misc.
 * ---------------------------------------------------------------- */

static void ast_thread(struct m0t1fs_sb *csb)
{
	while (1) {
		m0_chan_wait(&csb->csb_iogroup.s_clink);
		m0_sm_group_lock(&csb->csb_iogroup);
		m0_sm_asts_run(&csb->csb_iogroup);
		m0_sm_group_unlock(&csb->csb_iogroup);
		if (!csb->csb_active &&
		    m0_atomic64_get(&csb->csb_pending_io_nr) == 0) {
			m0_chan_signal_lock(&csb->csb_iowait);
			return;
		}
	}
}

static void ast_thread_stop(struct m0t1fs_sb *csb)
{
	struct m0_clink w;

	m0_clink_init(&w, NULL);
	m0_clink_add_lock(&csb->csb_iowait, &w);

	csb->csb_active = false;
	m0_chan_signal_lock(&csb->csb_iogroup.s_chan);
	m0_chan_wait(&w);
	m0_thread_join(&csb->csb_astthread);

	m0_clink_del_lock(&w);
	m0_clink_fini(&w);
}

#undef M0_TRACE_SUBSYSTEM
