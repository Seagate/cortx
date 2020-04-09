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
 * Original creation date: 01/17/2011
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_COB
#include "lib/trace.h"

#include <sys/stat.h>    /* S_ISDIR */

#include "lib/misc.h"    /* M0_SET0 */
#include "lib/arith.h"   /* M0_3WAY, m0_align */
#include "lib/errno.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/bitstring.h"
#include "lib/rwlock.h"

#include "fid/fid.h"
#include "fop/fop.h"
#include "cob/cob.h"
#include "mdstore/mdstore.h"
#include "mero/magic.h"

M0_INTERNAL int m0_mdstore_mod_init(void)
{
	return 0;
}

M0_INTERNAL void m0_mdstore_mod_fini(void)
{
}


/** Let's show client that we have 1G storage. */
#define M0_MD_FAKE_BLOCKSIZE 4096
#define M0_MD_FAKE_VOLUME    250000

M0_INTERNAL int m0_mdstore_statfs(struct m0_mdstore        *md,
				  struct m0_statfs         *statfs)
{
	/**
	   We need statfs mostly to provide mdstore root fid to
	   m0t1fs at this point. It is not yet clear what else
	   info should be returned where it can be retrieved from.
	   Hence return zeros so far.
	 */
	M0_SET0(statfs);
	statfs->sf_type = M0_T1FS_SUPER_MAGIC;
	statfs->sf_bsize = M0_MD_FAKE_BLOCKSIZE;
	statfs->sf_blocks = M0_MD_FAKE_VOLUME;
	statfs->sf_bfree = M0_MD_FAKE_VOLUME;
	statfs->sf_bavail = M0_MD_FAKE_VOLUME;
	statfs->sf_files = 1024000;
	statfs->sf_ffree = 1024000;
	statfs->sf_namelen = M0_MD_MAX_NAME_LEN;
	if (md->md_root)
		statfs->sf_root = *m0_cob_fid(md->md_root);
	return 0;
}

M0_INTERNAL int m0_mdstore_init(struct m0_mdstore *md,
				struct m0_be_seg  *db,
				bool               init_root)
{
	int rc;

	M0_PRE(md != NULL && db != NULL);

	/*
	 * TODO FIXME Don't allow md_dom to be NULL here. Instead, check it
	 * by users (mero/setup, ut/ut_rpc_machine).
	 * I will reduce posibility of mistakes.
	 */
	if (md->md_dom == NULL) {
		/* See cs_storage_setup(). */
		return M0_RC(-ENOENT);
	}

	rc = m0_cob_domain_init(md->md_dom, db);
	if (rc != 0)
		return M0_RC(rc);

	if (init_root) {
		struct m0_buf name;

		m0_buf_init(&name, (char*)M0_COB_ROOT_NAME,
				   strlen(M0_COB_ROOT_NAME));
		rc = m0_mdstore_lookup(md, NULL, &name, &md->md_root);
		if (rc == 0)
			/*
			 * Check if omgid can be allocated.
			 */
			rc = m0_cob_alloc_omgid(md->md_dom, NULL);
	}
	if (rc != 0)
		m0_mdstore_fini(md);
	return M0_RC(rc);
}

static void mdstore_fini(struct m0_mdstore *md, bool pre_destroy)
{
	if (md->md_root != NULL)
		m0_cob_put(md->md_root);

	/*
	 * During mdstore destroying we call m0_cob_domain_destroy() which
	 * includes implicit finalisation. So, avoid double finalisation.
	 */
	if (!pre_destroy)
		m0_cob_domain_fini(md->md_dom);
}

M0_INTERNAL void m0_mdstore_fini(struct m0_mdstore *md)
{
	mdstore_fini(md, false);
}

M0_INTERNAL int m0_mdstore_create(struct m0_mdstore       *md,
				  struct m0_sm_group      *grp,
				  struct m0_cob_domain_id *id,
				  struct m0_be_domain     *bedom,
				  struct m0_be_seg        *db)
{
	M0_PRE(md != NULL);

	return m0_cob_domain_create(&md->md_dom, grp, id, bedom, db);
}

M0_INTERNAL int m0_mdstore_destroy(struct m0_mdstore   *md,
				   struct m0_sm_group  *grp,
				   struct m0_be_domain *bedom)
{
	/*
	 * We have to finalise mdstore here, but can't call m0_mdstore_fini(),
	 * because m0_cob_domain_destroy() finalises cob domain inmplicitly.
	 * See m0_mdstore_fini().
	 */
	mdstore_fini(md, true);

	return m0_cob_domain_destroy(md->md_dom, grp, bedom);
}

M0_INTERNAL void
m0_mdstore_dir_nlink_update_credit(struct m0_mdstore *md,
				   struct m0_be_tx_credit *accum)
{
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_UPDATE, accum);
}

M0_INTERNAL int m0_mdstore_dir_nlink_update(struct m0_mdstore   *md,
					    struct m0_fid       *fid,
					    int                  inc,
					    struct m0_be_tx     *tx)
{
	struct m0_cob         *cob;
	struct m0_cob_oikey    oikey;
	int                    rc;

	M0_ENTRY("%+d nlinks for dir "FID_F, inc, FID_P(fid));
	/**
	 * Directories cannot have hardlinks, so they can always
	 * be found by oikey(fid, 0):
	 */
	m0_cob_oikey_make(&oikey, fid, 0);
	rc = m0_cob_locate(md->md_dom, &oikey, 0, &cob);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "cannot locate stat data for dir "FID_F" - %d",
		       FID_P(fid), rc);
		goto out;
	}
	M0_LOG(M0_DEBUG, "%u %+d nlinks for "FID_F"<-"FID_F"/%.*s",
	       (unsigned)cob->co_nsrec.cnr_nlink, inc,
	       FID_P(fid), FID_P(&cob->co_nskey->cnk_pfid),
	       m0_bitstring_len_get(&cob->co_nskey->cnk_name),
	       (char *)m0_bitstring_buf_get(&cob->co_nskey->cnk_name));
	cob->co_nsrec.cnr_nlink += inc;
	rc = m0_cob_update(cob, &cob->co_nsrec, NULL, NULL, tx);
	m0_cob_put(cob);
out:
	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

M0_INTERNAL void
m0_mdstore_create_credit(struct m0_mdstore *md,
			 struct m0_be_tx_credit *accum)
{
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_CREATE, accum);
	m0_mdstore_dir_nlink_update_credit(md, accum);
}

M0_INTERNAL int m0_mdstore_fcreate(struct m0_mdstore     *md,
				   struct m0_fid         *pfid,
				   struct m0_cob_attr    *attr,
				   struct m0_cob        **out,
				   struct m0_be_tx       *tx)
{
	struct m0_cob         *cob;
	struct m0_cob_nskey   *nskey = NULL;
	struct m0_cob_nsrec    nsrec = {};
	struct m0_cob_fabrec  *fabrec = NULL;
	struct m0_cob_omgrec   omgrec = {};
	int                    linklen;
	int                    rc;

	M0_ENTRY();
	M0_ASSERT(pfid != NULL);

        /* We don't allow create in .mero and .mero/fid directory. */
        if (m0_fid_cmp(pfid, &M0_MDSERVICE_SLASH_FID) < 0) {
                rc = -EOPNOTSUPP;
                goto out;
        }

	rc = m0_cob_alloc(md->md_dom, &cob);
	if (rc != 0)
		goto out;

	rc = m0_cob_nskey_make(&nskey, pfid, (char *)attr->ca_name.b_addr,
			       attr->ca_name.b_nob);
	if (rc != 0) {
		m0_cob_put(cob);
		return M0_RC(rc);
	}

	m0_cob_nsrec_init(&nsrec);
	nsrec.cnr_fid = attr->ca_tfid;
	M0_ASSERT(attr->ca_nlink > 0);
	nsrec.cnr_nlink = attr->ca_nlink;
	nsrec.cnr_size = attr->ca_size;
	nsrec.cnr_blksize = attr->ca_blksize;
	nsrec.cnr_blocks = attr->ca_blocks;
	nsrec.cnr_atime = attr->ca_atime;
	nsrec.cnr_mtime = attr->ca_mtime;
	nsrec.cnr_ctime = attr->ca_ctime;
	nsrec.cnr_lid   = attr->ca_lid;
	nsrec.cnr_pver  = attr->ca_pver;

	omgrec.cor_uid = attr->ca_uid;
	omgrec.cor_gid = attr->ca_gid;
	omgrec.cor_mode = attr->ca_mode;

	linklen = attr->ca_link.b_addr ? attr->ca_link.b_nob : 0;
	rc = m0_cob_fabrec_make(&fabrec, (char *)attr->ca_link.b_addr,
				linklen);
	if (rc != 0) {
		m0_cob_put(cob);
		m0_free(nskey);
		goto out;
	}

	rc = m0_cob_create(cob, nskey, &nsrec, fabrec, &omgrec, tx);
	if (rc != 0) {
		m0_cob_put(cob);
		m0_free(nskey);
		m0_free(fabrec);
	} else {
		*out = cob;
		if (S_ISDIR(attr->ca_mode)) {
			/** Increment cnr_nlink of parent directory. */
			rc = m0_mdstore_dir_nlink_update(md, pfid, +1, tx);
		}
	}

out:
	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

M0_INTERNAL void
m0_mdstore_link_credit(struct m0_mdstore *md,
		       struct m0_be_tx_credit *accum)
{
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_NAME_ADD, accum);
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_UPDATE, accum);
}

M0_INTERNAL int m0_mdstore_link(struct m0_mdstore       *md,
				struct m0_fid           *pfid,
				struct m0_cob           *cob,
				struct m0_buf           *name,
				struct m0_be_tx         *tx)
{
	struct m0_cob_nskey   *nskey = NULL;
	struct m0_cob_nsrec    nsrec;
	time_t                 now;
	int                    rc;

	M0_ENTRY();
	M0_ASSERT(pfid != NULL);
	M0_ASSERT(cob != NULL);

        /* We don't allow link in .mero and .mero/fid directory. */
        if (m0_fid_cmp(pfid, &M0_MDSERVICE_SLASH_FID) < 0)
		return M0_RC(-EOPNOTSUPP);

	time(&now);
	M0_SET0(&nsrec);

	/*
	 * Link @nskey to a file described with @cob
	 */
	rc = m0_cob_nskey_make(&nskey, pfid, (char *)name->b_addr,
			       name->b_nob);
	if (rc != 0)
		return M0_RC(rc);
	M0_PRE(m0_fid_is_set(&cob->co_nsrec.cnr_fid));

	m0_cob_nsrec_init(&nsrec);
	nsrec.cnr_fid = cob->co_nsrec.cnr_fid;
	nsrec.cnr_linkno = cob->co_nsrec.cnr_cntr;

	rc = m0_cob_name_add(cob, nskey, &nsrec, tx);
	m0_free(nskey);
	if (rc != 0)
		return M0_RC(rc);

	cob->co_nsrec.cnr_cntr++;
	rc = m0_cob_update(cob, &cob->co_nsrec, NULL, NULL, tx);

	return M0_RC(rc);
}

/**
 * Checks that m0_cob_fid directory is empty.
 *
 * @retval 0           Directory is empty.
 * @retval -ENOTEMPTY  Directory isn not empty.
 * @retval -ENOMEM     Memory allocation error.
 */
M0_INTERNAL int m0_mdstore_dir_empty_check(struct m0_mdstore *md,
					   struct m0_cob     *cob)
{
	struct m0_cob_iterator  it;
	struct m0_bitstring     empty = {.b_len = 0};
	int                     rc;

	M0_ENTRY();
	rc = m0_cob_iterator_init(cob, &it, &empty);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "iterator init: %d", rc);
		goto out;
	}
	rc = m0_cob_iterator_get(&it);
	if (rc == 0) {
		M0_LOG(M0_DEBUG, FID_F"/%.*s contains "FID_F"/%.*s",
		       FID_P(&cob->co_nskey->cnk_pfid),
		       m0_bitstring_len_get(&cob->co_nskey->cnk_name),
		       (char *)m0_bitstring_buf_get(&cob->co_nskey->cnk_name),
		       FID_P(&it.ci_key->cnk_pfid),
		       m0_bitstring_len_get(&it.ci_key->cnk_name),
		       (char *)m0_bitstring_buf_get(&it.ci_key->cnk_name));
		rc = -ENOTEMPTY;
	} else if (rc == -ENOENT)
		rc = 0;
	m0_cob_iterator_fini(&it);
out:
	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

M0_INTERNAL void
m0_mdstore_unlink_credit(struct m0_mdstore *md,
		         struct m0_be_tx_credit *accum)
{
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_UPDATE, accum);
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_NAME_UPDATE, accum);
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_DELETE, accum);
}

M0_INTERNAL int m0_mdstore_unlink(struct m0_mdstore     *md,
				  struct m0_fid         *pfid,
				  struct m0_cob         *cob,
				  struct m0_buf         *name,
				  struct m0_be_tx       *tx)
{
	struct m0_cob         *ncob;
	struct m0_cob_nskey   *nskey = NULL;
	struct m0_cob_oikey    oikey;
	time_t                 now;
	int                    rc;

	M0_ENTRY(FID_F"/%.*s", FID_P(pfid),
		 (int)name->b_nob, (char *)name->b_addr);
	M0_ASSERT(pfid != NULL);
	M0_ASSERT(cob != NULL);
	M0_LOG(M0_DEBUG, FID_F"/%.*s->"FID_F",%d cob",
	       FID_P(&cob->co_nskey->cnk_pfid),
	       m0_bitstring_len_get(&cob->co_nskey->cnk_name),
	       (char *)m0_bitstring_buf_get(&cob->co_nskey->cnk_name),
	       FID_P(&cob->co_nsrec.cnr_fid), cob->co_nsrec.cnr_linkno);

        /* We don't allow unlink in .mero and .mero/fid directories. */
        if (m0_fid_cmp(pfid, &M0_MDSERVICE_SLASH_FID) < 0) {
                rc = -EOPNOTSUPP;
                goto out;
        }

        /* We don't allow to kill .mero dir. */
        if (m0_fid_eq(pfid, &M0_MDSERVICE_SLASH_FID) &&
            name->b_nob == strlen(M0_DOT_MERO_NAME) &&
            !strncmp((char *)name->b_addr, M0_DOT_MERO_NAME, (int)name->b_nob)) {
                rc = -EOPNOTSUPP;
                goto out;
        }

	M0_PRE(cob->co_nsrec.cnr_nlink > 0);

	time(&now);

	/*
	 * Check for hardlinks.
	 */
	if (!S_ISDIR(cob->co_omgrec.cor_mode)) {
		/*
		 * New stat data name should get updated nlink value.
		 */
		cob->co_nsrec.cnr_nlink--;

		rc = m0_cob_nskey_make(&nskey, pfid, (char *)name->b_addr,
				       name->b_nob);
		if (rc != 0) {
			M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): nskey make "
			       "failed with %d", rc);
			goto out;
		}

		/*
		 * Check if we're trying to kill stata data entry. We need to
		 * move stat data to another name if so.
		 */
		if (cob->co_nsrec.cnr_nlink > 0) {
			M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): more links exist");
			if (m0_cob_nskey_cmp(nskey, cob->co_nskey) == 0) {
				M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): unlink statdata "
				       "name, find new statdata with %d or more nlinks",
				       cob->co_nsrec.cnr_linkno + 1);

				/*
				 * Find another name (new stat data) in object index to
				 * move old statdata to it.
				 */
				m0_cob_oikey_make(&oikey, m0_cob_fid(cob),
						  cob->co_nsrec.cnr_linkno + 1);

				rc = m0_cob_locate(md->md_dom, &oikey, 0, &ncob);
				if (rc != 0) {
					M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): locate "
					       "failed with %d", rc);
					m0_free(nskey);
					goto out;
				}
				M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): locate found "
				       "name with %d nlinks", ncob->co_oikey.cok_linkno);
				cob->co_nsrec.cnr_linkno = ncob->co_oikey.cok_linkno;
			} else {
				M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): unlink hardlink "
				       "name");
				ncob = cob;
			}

			M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): update statdata on store");

			/**
			   Copy statdata (in case of killing old statdata) or update
			   statdata with new nlink number.
			 */
			rc = m0_cob_update(ncob, &cob->co_nsrec, NULL, NULL, tx);
			if (rc != 0) {
				M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): new statdata "
				       "update failed with %d", rc);
				m0_free(nskey);
				goto out;
			}

			/** Kill the name itself. */
			rc = m0_cob_name_del(cob, nskey, tx);
			if (rc != 0) {
				M0_LOG(M0_DEBUG, "m0_mdstore_unlink(): name del "
				       "failed with %d", rc);
				m0_free(nskey);
				goto out;
			}
		} else {
			/* Zero nlink reached, kill entire object. */
			rc = m0_cob_delete(cob, tx);
		}
		m0_free(nskey);
	} else {
		/*
		 * TODO: we must take some sort of a lock
		 * when doing check-before-modify update to directory.
		 */
		rc = m0_mdstore_dir_empty_check(md, cob);
		if (rc != 0)
			goto out;
		rc = m0_cob_delete(cob, tx);
		if (rc != 0)
			goto out;
		/** Decrement cnr_nlink of parent directory. */
		rc = m0_mdstore_dir_nlink_update(md, pfid, -1, tx);
	}

out:
	return M0_RC(rc);
}

M0_INTERNAL int m0_mdstore_open(struct m0_mdstore       *md,
				struct m0_cob           *cob,
				m0_mdstore_locate_flags_t flags,
				struct m0_be_tx         *tx)
{
	/**
	 * @todo: Place cob to open files table.
	 */
	return M0_RC(0);
}

M0_INTERNAL int m0_mdstore_close(struct m0_mdstore      *md,
				 struct m0_cob          *cob,
				 struct m0_be_tx        *tx)
{
	int rc = 0;

	M0_ASSERT(cob != NULL);

	/**
	 * @todo:
	 *   - orphans handling?
	 *   - quota handling?
	 */
	return M0_RC(rc);
}

M0_INTERNAL void
m0_mdstore_rename_credit(struct m0_mdstore *md,
		         struct m0_be_tx_credit *accum)
{
	m0_mdstore_unlink_credit(md, accum);
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_NAME_UPDATE, accum);
}

M0_INTERNAL int m0_mdstore_rename(struct m0_mdstore     *md,
				  struct m0_fid         *pfid_tgt,
				  struct m0_fid         *pfid_src,
				  struct m0_cob         *cob_tgt,
				  struct m0_cob         *cob_src,
				  struct m0_buf         *tname,
				  struct m0_buf         *sname,
				  struct m0_be_tx       *tx)
{
	struct m0_cob_nskey  *srckey = NULL;
	struct m0_cob_nskey  *tgtkey = NULL;
	struct m0_cob        *tncob = NULL;
	bool                  unlink;
	time_t                now;
	int                   rc;

	M0_ENTRY();
	M0_ASSERT(pfid_tgt != NULL);
	M0_ASSERT(pfid_src != NULL);

	time(&now);

        /* We don't allow rename in/with .mero/fid directories. */
        if (m0_fid_cmp(pfid_tgt, &M0_MDSERVICE_SLASH_FID) < 0 ||
            m0_fid_cmp(pfid_src, &M0_MDSERVICE_SLASH_FID) < 0 ||
            m0_fid_cmp(m0_cob_fid(cob_tgt), &M0_MDSERVICE_SLASH_FID) < 0 ||
            m0_fid_cmp(m0_cob_fid(cob_src), &M0_MDSERVICE_SLASH_FID) < 0) {
                rc = -EOPNOTSUPP;
                goto out;
        }

	/*
	 * Let's kill existing target name.
	 */
	rc = m0_mdstore_lookup(md, pfid_tgt, tname, &tncob);
	unlink = (tncob != NULL &&
	    m0_cob_nskey_cmp(tncob->co_nskey, cob_tgt->co_nskey) != 0);

	if (!m0_fid_eq(m0_cob_fid(cob_tgt), m0_cob_fid(cob_src)) || unlink) {
		rc = m0_mdstore_unlink(md, pfid_tgt, cob_tgt, tname, tx);
		if (rc != 0) {
			if (tncob)
				m0_cob_put(tncob);
			goto out;
		}
	}
	if (tncob)
		m0_cob_put(tncob);
	/*
	 * Prepare src and dst keys.
	 */
	m0_cob_nskey_make(&srckey, pfid_src,
			  (char *)sname->b_addr, sname->b_nob);
	m0_cob_nskey_make(&tgtkey, pfid_tgt,
			  (char *)tname->b_addr, tname->b_nob);

	rc = m0_cob_name_update(cob_src, srckey, tgtkey, tx);

	m0_free(srckey);
	m0_free(tgtkey);
out:
	M0_LEAVE("rc: %d", rc);
	return M0_RC(rc);
}

M0_INTERNAL void
m0_mdstore_setattr_credit(struct m0_mdstore *md,
		          struct m0_be_tx_credit *accum)
{
	m0_cob_tx_credit(md->md_dom, M0_COB_OP_UPDATE, accum);
}

M0_INTERNAL int m0_mdstore_setattr(struct m0_mdstore    *md,
				   struct m0_cob        *cob,
				   struct m0_cob_attr   *attr,
				   struct m0_be_tx      *tx)
{
	M0_ENTRY();
	M0_ASSERT(cob != NULL);
	return M0_RC(m0_cob_setattr(cob, attr, tx));
}

M0_INTERNAL int m0_mdstore_getattr(struct m0_mdstore       *md,
				   struct m0_cob           *cob,
				   struct m0_cob_attr      *attr)
{
	int                rc = 0;

	M0_ENTRY();
	M0_ASSERT(cob != NULL);

	M0_SET0(attr);
	attr->ca_valid = 0;
	attr->ca_tfid = cob->co_nsrec.cnr_fid;
	attr->ca_pfid = cob->co_nskey->cnk_pfid;

	/*
	 * Copy permissions and owner info into rep.
	 */
	if (cob->co_flags & M0_CA_OMGREC) {
		attr->ca_valid |= M0_COB_UID | M0_COB_GID | M0_COB_MODE;
		attr->ca_uid = cob->co_omgrec.cor_uid;
		attr->ca_gid = cob->co_omgrec.cor_gid;
		attr->ca_mode = cob->co_omgrec.cor_mode;
	}

	/*
	 * Copy nsrec fields into response.
	 */
	if (cob->co_flags & M0_CA_NSREC) {
		attr->ca_valid |= M0_COB_ATIME | M0_COB_CTIME | M0_COB_MTIME |
				  M0_COB_SIZE | M0_COB_BLKSIZE | M0_COB_BLOCKS/* |
				  M0_COB_RDEV*/ | M0_COB_LID | M0_COB_PVER;

		attr->ca_atime = cob->co_nsrec.cnr_atime;
		attr->ca_ctime = cob->co_nsrec.cnr_ctime;
		attr->ca_mtime = cob->co_nsrec.cnr_mtime;
		attr->ca_blksize = cob->co_nsrec.cnr_blksize;
		attr->ca_blocks = cob->co_nsrec.cnr_blocks;
		attr->ca_nlink = cob->co_nsrec.cnr_nlink;
		//attr->ca_rdev = cob->co_nsrec.cnr_rdev;
		attr->ca_size = cob->co_nsrec.cnr_size;
		attr->ca_lid = cob->co_nsrec.cnr_lid;
		attr->ca_pver = cob->co_nsrec.cnr_pver;
		//attr->ca_version = cob->co_nsrec.cnr_version;
		M0_LOG(M0_DEBUG, "attrs of "FID_F"/%.*s->"FID_F",%u: "
		       "cntr:%u, nlink:%u",
		       FID_P(&attr->ca_pfid),
		       m0_bitstring_len_get(&cob->co_nskey->cnk_name),
		       (char *)m0_bitstring_buf_get(&cob->co_nskey->cnk_name),
		       FID_P(&attr->ca_tfid),
		       (unsigned)cob->co_nsrec.cnr_linkno,
		       (unsigned)cob->co_nsrec.cnr_cntr,
		       (unsigned)attr->ca_nlink);
		M0_LOG(M0_DEBUG, "size:%u, lid:%u",
		       (unsigned)attr->ca_size,
		       (unsigned)attr->ca_lid);
	}

	/*
	 * @todo: Copy rest of the fab fields.
	 */
	return M0_RC(rc);
}

M0_INTERNAL int m0_mdstore_readdir(struct m0_mdstore       *md,
				   struct m0_cob           *cob,
				   struct m0_rdpg          *rdpg)
{
	struct m0_cob_iterator         it;
	struct m0_dirent              *ent;
	struct m0_dirent              *last = NULL;
	int                            nob;
	int                            reclen;
	int                            rc;
	int                            dot;
	char                          *s_buf;
	uint32_t                       s_len;
	struct m0_bitstring            empty = {.b_len = 0};
	struct m0_bitstring           *pos;

	M0_ENTRY();
	M0_ASSERT(cob != NULL && rdpg != NULL);

	pos = rdpg->r_pos;
	s_buf = (char *)m0_bitstring_buf_get(pos);
	s_len = m0_bitstring_len_get(pos);

	M0_LOG(M0_DEBUG, "Readdir on object "FID_F" starting from \"%.*s\"",
	       FID_P(m0_cob_fid(cob)), s_len, s_buf);

	ent = rdpg->r_buf.b_addr;
	nob = rdpg->r_buf.b_nob;
	if (nob < sizeof(struct m0_dirent)) {
		rc = -EINVAL;
		goto out_end;
	}

	/* Emulate "." and ".." entries at the 1st and 2nd positions. */
	if (s_len == 1 && s_buf[0] == '.') {
		dot = 1;
	} else if (s_len == 2 && strncmp(s_buf, "..", 2) == 0) {
		dot = 2;
	} else {
		dot = 0;
	}

	rc = m0_cob_iterator_init(cob, &it, dot ? &empty : pos);
	if (rc != 0) {
		M0_LOG(M0_DEBUG, "Iterator failed to position with %d", rc);
		goto out;
	}

	/*
	 * Positions iterator to the closest key greater or equal
	 * to the starting position. Returns -ENOENT if no such
	 * key exists.
	 */
	rc = m0_cob_iterator_get(&it);
	while (rc == 0 || dot) {
		if (dot == 1) {
			/* pos already contains "." */
		} else if (dot == 2) {
			m0_bitstring_copy(pos, "..", 2);
		} else {
			s_buf = m0_bitstring_buf_get(&it.ci_key->cnk_name);
			s_len = m0_bitstring_len_get(&it.ci_key->cnk_name);
			/* pos was allocated with large buffer:
			 * rdpg.r_pos = m0_alloc(M0_MD_MAX_NAME_LEN) */
			m0_bitstring_copy(pos, s_buf, s_len);
		}

		reclen = m0_align(sizeof(*ent) + m0_bitstring_len_get(pos), 8);

		if (nob >= reclen) {
			ent->d_namelen = m0_bitstring_len_get(pos);
			memcpy(ent->d_name, m0_bitstring_buf_get(pos),
			       ent->d_namelen);
			ent->d_reclen = reclen;
			M0_LOG(M0_DEBUG,
			       "Readdir filled entry \"%.*s\" recsize %d",
			       ent->d_namelen, (char *)ent->d_name,
			       ent->d_reclen);
		} else {
			/*
			 * If buffer was too small to hold even one record,
			 * return -EINVAL. Otherwise return 0.
			 */
			rc = last == NULL ? -EINVAL : 0;
			goto out_end;
		}
		last = ent;
		ent = (struct m0_dirent *)((char *)ent + reclen);
		nob -= reclen;
		if (!dot)
			rc = m0_cob_iterator_next(&it);
		else if (++dot > 2)
			dot = 0;
	}
out_end:
	m0_cob_iterator_fini(&it);
	if (last != NULL) {
		last->d_reclen = 0;  /* The last record indicator. */
		s_buf = (char *)m0_bitstring_buf_get(pos);
		s_len = m0_bitstring_len_get(pos);
		rdpg->r_end = m0_bitstring_alloc(s_buf, s_len);
		M0_LOG(M0_DEBUG, "%s entry: \"%.*s\"", rc ? "last" : "next",
		       s_len, s_buf);
		rc = rc ? ENOENT : 0;
	}
	if (rc == -ENOENT)
		rc = ENOENT;
out:
	return M0_RC(rc);
}

/**
   Finds cob by @fid and store its pointer to passed @cob.
 */
M0_INTERNAL int m0_mdstore_locate(struct m0_mdstore     *md,
				  const struct m0_fid   *fid,
				  struct m0_cob        **cob,
				  int                    flags)
{
	struct m0_cob_oikey oikey;
	int                 rc;

	M0_ENTRY(FID_F, FID_P(fid));
	m0_cob_oikey_make(&oikey, fid, 0);

	if (flags == M0_MD_LOCATE_STORED) {
		rc = m0_cob_locate(md->md_dom, &oikey,
				   (M0_CA_FABREC | M0_CA_OMGREC), cob);
	} else {
		/*
		 * @todo: locate cob in opened cobs table.
		 */
		rc = -EOPNOTSUPP;
	}
	if (rc == 0) {
		M0_LEAVE(FID_F"<-"FID_F"/%.*s",
			 FID_P(&(*cob)->co_nsrec.cnr_fid),
			 FID_P(&(*cob)->co_nskey->cnk_pfid),
			 m0_bitstring_len_get(&(*cob)->co_nskey->cnk_name),
			 (char *)m0_bitstring_buf_get(&(*cob)->co_nskey->cnk_name));
	} else {
		M0_LEAVE("rc: %d", rc);
	}
	return M0_RC(rc);
}

/**
   Finds cob by name and store its pointer to passed @cob.

   In order to handle possible obf-like names like this:
   (cat /mnt/mero/.mero/1:5), parent fid is checked and
   special action is taken to extract fid components from
   file name in obf case. Then m0_cob_locate() is used
   to find cob by fid rather than by name.

   @see m0_cob_domain_mkfs
   @see m0_mdstore_locate
 */
M0_INTERNAL int m0_mdstore_lookup(struct m0_mdstore     *md,
				  struct m0_fid         *pfid,
				  struct m0_buf         *name,
				  struct m0_cob        **cob)
{
	struct m0_cob_nskey *nskey = NULL;
        struct m0_fid fid;
	int flags;
	int rc;

	M0_ENTRY();
	if (pfid == NULL)
		pfid = (struct m0_fid *)&M0_COB_ROOT_FID;

        /*
          Check for obf case and use m0_cob_locate() to get cob by fid
          extracted from name.
         */
        if (m0_fid_eq(pfid, &M0_DOT_MERO_FID_FID)) {
                rc = m0_fid_sscanf((char *)name->b_addr, &fid);
                if (rc != 0)
                        goto out;
                rc = m0_mdstore_locate(md, &fid, cob, M0_MD_LOCATE_STORED);
        } else {
	        rc = m0_cob_nskey_make(&nskey, pfid, (char *)name->b_addr, name->b_nob);
	        if (rc != 0)
		        goto out;
	        flags = (M0_CA_NSKEY_FREE | M0_CA_FABREC | M0_CA_OMGREC);
		rc = m0_cob_lookup(md->md_dom, nskey, flags, cob);
	}
out:
	return M0_RC(rc);
}

#define MDSTORE_PATH_MAX 1024
#define MDSTORE_NAME_MAX 255

M0_INTERNAL int m0_mdstore_path(struct m0_mdstore       *md,
				struct m0_fid           *fid,
				char                   **path)
{
	struct m0_cob   *cob;
	struct m0_fid    pfid;
	int              rc;

	M0_ENTRY(FID_F, FID_P(fid));
	*path = m0_alloc(MDSTORE_PATH_MAX);
	if (*path == NULL)
		return M0_ERR(-ENOMEM);

restart:
	pfid = *fid;

	do {
		char name[MDSTORE_NAME_MAX] = {0,};

		rc = m0_mdstore_locate(md, &pfid, &cob, M0_MD_LOCATE_STORED);
		if (rc != 0)
			goto out;

		if (!m0_fid_eq(m0_cob_fid(cob), m0_cob_fid(md->md_root))) {
			strncat(name,
				m0_bitstring_buf_get(&cob->co_nskey->cnk_name),
				m0_bitstring_len_get(&cob->co_nskey->cnk_name));
		}
		if (!m0_fid_eq(m0_cob_fid(cob), fid) ||
		    m0_fid_eq(m0_cob_fid(cob), m0_cob_fid(md->md_root)))
			strcat(name, "/");
		memmove(*path + strlen(name), *path, strlen(*path));
		memcpy(*path, name, strlen(name));
		pfid = cob->co_nskey->cnk_pfid;
		m0_cob_put(cob);
	} while (!m0_fid_eq(&pfid, &M0_COB_ROOT_FID));
out:
	if (rc == -EDEADLK) {
		memset(*path, 0, MDSTORE_PATH_MAX);
		goto restart;
	}
	if (rc != 0)
		m0_free0(path);
	M0_LEAVE("rc: %d, path: %s", rc, *path);
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
