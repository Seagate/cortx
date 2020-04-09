/* -*- C -*- */
/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Sining Wu       <sining.wu@seagate.com>
 * Revision:        Pratik Shinde   <pratik.shinde@seagate.com>
 * Original creation date: 24-Aug-2015
 *
 */

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_addb.h"
#include "clovis/clovis_idx.h"

#include "lib/errno.h"             /* ENOMEM */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

#ifndef __KERNEL__

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static char *idx_store_path = NULL;

/**-------------------------------------------------------------------------*
 *                          Key/Value Pairs                                 *
 *--------------------------------------------------------------------------*/
struct key {
	int   k_len;
	void *k_data;
};

struct val {
	int   v_len;
	void *v_data;
};

struct kv_pair {
	uint64_t        p_magic;
	struct m0_hlink p_link;

	struct key      p_key;
	struct val      p_val;
};

static uint64_t kv_hash(const struct m0_htable *htable, const struct key *key)
{
	int      i;
	uint64_t h = 0;

	for(i = 0; i < key->k_len; i++)
		h += ((char *)key->k_data)[i];
	h = h % htable->h_bucket_nr;

	return h;
}

static int kv_hash_key_eq(const struct key *k1, const struct key *k2)
{
	if (k1->k_len != k2->k_len)
		return M0_RC(0);

	if (k1->k_data == NULL || k2->k_data == NULL)
		return M0_RC(0);

	if (memcmp(k1->k_data, k2->k_data, k1->k_len) == 0)
		return M0_RC(1);
	else
		return M0_RC(0);
}

M0_HT_DESCR_DEFINE(kv_pairs, "Hash of key value pairs",
		   static, struct kv_pair,
		   p_link, p_magic, 0x41, 0x67, p_key,
		   kv_hash, kv_hash_key_eq);
M0_HT_DEFINE(kv_pairs, static, struct kv_pair, struct key);

/**-------------------------------------------------------------------------*
 *                                 Index                                    *
 *--------------------------------------------------------------------------*/

struct index {
	uint64_t          i_magic;
	struct m0_hlink   i_link;

	struct m0_uint128 i_fid;
	struct m0_htable  i_kv_pairs;
};

/**
 * Generates a hash key for an entity.
 */
static uint64_t idx_hash(const struct m0_htable *htable,
			 const struct m0_uint128 *key)
{
	const uint64_t k = key->u_lo;
	return k % htable->h_bucket_nr;
}

/**
 * Compares the hash keys of two entities.
 */
static int idx_hash_key_eq(const struct m0_uint128 *key1,
			   const struct m0_uint128 *key2)
{
	const uint64_t k1 = key1->u_lo;
	const uint64_t k2 = key2->u_lo;
	return k1 == k2;
}

M0_HT_DESCR_DEFINE(indices, "Hash of key value pairs",
		   static, struct index,
		   i_link, i_magic, 0x43, 0x67, i_fid,
		   idx_hash, idx_hash_key_eq);
M0_HT_DEFINE(indices, static, struct index, struct m0_uint128);
static struct m0_htable idx_htable;


/**-------------------------------------------------------------------------*
 *                             Index store                                  *
 *--------------------------------------------------------------------------*/
#define MAX_IDX_FNAME_LEN (256)
#define IDX_CSV_DELIMITER (',')

static int make_idx_fname(char *fname, struct m0_uint128 idx_fid)
{
	return sprintf(fname,
		       "%s/%"PRIx64"_%"PRIx64,
		       idx_store_path, idx_fid.u_hi, idx_fid.u_lo);
}

static int idx_access(struct m0_uint128 idx_fid)
{
	int   rc;
	char *idx_fname;

	idx_fname = m0_alloc(MAX_IDX_FNAME_LEN);
	if (idx_fname == NULL)
		return -ENOMEM;

	make_idx_fname(idx_fname, idx_fid);
	rc = access(idx_fname, F_OK);
	m0_free(idx_fname);

	return M0_RC(rc);
}

static int idx_rm(struct m0_uint128 idx_fid)
{
	int   rc;
	char *idx_fname;

	idx_fname = m0_alloc(MAX_IDX_FNAME_LEN);
	if (idx_fname == NULL)
		return M0_ERR(-ENOMEM);

	make_idx_fname(idx_fname, idx_fid);
	rc = remove(idx_fname);
	m0_free(idx_fname);

	return M0_RC(rc);
}

static struct index* idx_load(struct m0_uint128 idx_fid)
{
	char           *delimiter;
	char           *idx_fname = NULL;
	char           *line = NULL;
	struct kv_pair *kvp;
	struct index   *midx = NULL;
	FILE           *fp;

	/* Open and scan the index file. */
	idx_fname = m0_alloc(MAX_IDX_FNAME_LEN);
	if (idx_fname == NULL)
		goto error;

	make_idx_fname(idx_fname, idx_fid);
	fp = fopen(idx_fname, "r+");
	if (fp == NULL)
		goto error;

	midx = m0_alloc(sizeof *midx);
	if (midx == NULL)
		goto error;
	kv_pairs_htable_init(&midx->i_kv_pairs, 11);

	midx->i_fid = idx_fid;
	m0_tlink_init(&indices_tl, midx);
	indices_htable_add(&idx_htable, midx);

	/* Allocate memory for line buffer and midx.*/
	line = m0_alloc(1024);
	if (line == NULL)
		goto error;

	while(fgets(line, 1024, fp)) {
		kvp = m0_alloc(sizeof *kvp);
		if (kvp == NULL)
			break;

		delimiter = strchr(line, IDX_CSV_DELIMITER);
		kvp->p_key.k_len = delimiter - line;
		kvp->p_key.k_data = strndup(line, kvp->p_key.k_len);

		kvp->p_val.v_len = strlen(line) -  kvp->p_key.k_len - 1;
		kvp->p_val.v_data = strndup(delimiter + 1, kvp->p_val.v_len);

		/* Insert into hash table. */
		m0_tlink_init(&kv_pairs_tl, kvp);
		kv_pairs_htable_add(&midx->i_kv_pairs, kvp);
	}

	return midx;

error:
	m0_free(line);
	m0_free(midx);
	m0_free(idx_fname);

	return NULL;
}

static int idx_dump(struct index *midx)
{
	int             rc;
	int             cursor;
	int             buf_len;
	char           *buf;
	char           *idx_fname;
	struct key     *key;
	struct val     *val;
	struct kv_pair *kvp;
	FILE           *fp;

	/* Assemble index file. */
	idx_fname = m0_alloc(MAX_IDX_FNAME_LEN);
	if (idx_fname == NULL)
		return M0_ERR(-ENOMEM);

	make_idx_fname(idx_fname, midx->i_fid);
	fp = fopen(idx_fname, "w+");
	if (fp == NULL) {
		m0_free(idx_fname);
		return M0_ERR(-ENOMEM);
	}

	/* Allocate memory for buffer.*/
	buf_len = 1024 * 1024; /* 1MB */
	buf = m0_alloc(buf_len);
	if (buf == NULL) {
		fclose(fp);
		m0_free(idx_fname);
		return M0_ERR(-ENOMEM);
	}

	/*
	 * Pack K-V pairs into buffer and flush to disk when it is full.
	 */
	cursor = 0;
	m0_htable_for(kv_pairs, kvp, &midx->i_kv_pairs) {
		key = &kvp->p_key;
		val = &kvp->p_val;
		if (key->k_len + val->v_len > buf_len)
			break;

		if (cursor + key->k_len + val->v_len > buf_len) {
			rc = fwrite(buf, cursor, 1, fp);
			if (rc != cursor)
				break;
			cursor = 0;
		}

		/* Copy the key part. */
		memcpy(buf + cursor, key->k_data, key->k_len);
		cursor += key->k_len;
		buf[cursor] = ',';
		cursor++;

		/* Copy the val part. */
		memcpy(buf + cursor, val->v_data, val->v_len);
		cursor += val->v_len;
		buf[cursor] = '\n';
		cursor++;

		/* Remvoe this K-V pair from hash table. */
		m0_htable_del(&midx->i_kv_pairs, kvp);
		m0_free(kvp);
	}
	m0_htable_endfor;

	indices_htable_fini(&midx->i_kv_pairs);

	/* Flush the last bif. */
	fwrite(buf, cursor, 1, fp);

	/* Free memory */
	m0_free(idx_fname);
	m0_free(buf);
	fclose(fp);

	return M0_RC(0);
}

/**
 * Lookup an index in hash table and disks.
 *
 * @param idx_fid index's FID.
 * @param midx where a found or loaded index is store.
 * @param to_load index is loaded into memory if it is set.
 * @return 0: not found, 1: found, < 0 error.
 */
static int idx_find(struct m0_uint128 idx_fid,
		    struct index **midx, bool to_load)
{
	int           rc;
	struct index *found_midx;

	found_midx = m0_htable_lookup(&idx_htable, &idx_fid);
	if (found_midx != NULL) {
		if (midx != NULL)
			*midx = found_midx;
		return 1;
	}

	if (to_load) {
		if (midx == NULL)
			return M0_ERR(-EINVAL);

		*midx = idx_load(idx_fid);
		rc = (*midx != NULL) ? 1 : 0;
	} else {
		rc = idx_access(idx_fid);
		rc = (rc == 0) ? 1 : 0;
	}

	return rc;
}

/**-------------------------------------------------------------------------*
 *                           Query Operations                               *
 *--------------------------------------------------------------------------*/

static int idx_mock_namei_new(struct m0_clovis_op_idx *oi)
{
	int                found;
	struct m0_uint128  idx_fid;
	struct index      *midx;

	idx_fid = oi->oi_idx->in_entity.en_id;
	found = idx_find(idx_fid, NULL, false);
	if (found == 1)
		return M0_ERR(-EEXIST);

	/* Create a new index and insert into hash table. */
	midx = m0_alloc(sizeof *midx);
	if (midx == NULL)
		return M0_ERR(-ENOMEM);
	kv_pairs_htable_init(&midx->i_kv_pairs, 11);

	midx->i_fid = idx_fid;
	m0_tlink_init(&indices_tl, midx);
	indices_htable_add(&idx_htable, midx);

	return M0_RC(0);
}

static int idx_mock_namei_del(struct m0_clovis_op_idx *oi)
{
	int                rc;
	struct m0_uint128  idx_fid;
	struct index      *midx = NULL;
	struct kv_pair    *kvp;

	idx_fid = oi->oi_idx->in_entity.en_id;
	rc = idx_find(idx_fid, &midx, false);
	if (rc == 0)
		return M0_ERR(-ENOENT);
	else if (rc < 0)
		return M0_RC(rc);

	if (midx == NULL)
		goto rm_index_file;

	/* Remove and free each K-V pair. */
	m0_htable_for(kv_pairs, kvp, &midx->i_kv_pairs) {
		m0_htable_del(&midx->i_kv_pairs, kvp);
		m0_free(kvp);
	}
	m0_htable_endfor;

	indices_htable_fini(&midx->i_kv_pairs);

	/* Remove from index hash table.*/
	m0_htable_del(&idx_htable, midx);
	m0_free(midx);

rm_index_file:
	/* Remove index file on disk. */
	idx_rm(idx_fid);

	return M0_RC(0);
}

static int idx_mock_namei_lookup(struct m0_clovis_op_idx *oi)
{
	return M0_ERR(-ENOSYS);
}

static int idx_mock_namei_list(struct m0_clovis_op_idx *oi)
{
	return M0_ERR(-ENOSYS);
}

static int idx_mock_get(struct m0_clovis_op_idx *oi)
{
	int                i;
	int                rc;
	int                nr_found = 0;
	struct key         mkey;
	struct val         mval;
	struct kv_pair    *kvp;
	struct index      *midx;
	struct m0_bufvec  *keys;
	struct m0_bufvec  *vals;
	struct m0_uint128  idx_fid;

	M0_ENTRY();

	keys = oi->oi_keys;
	vals = oi->oi_vals;
	if (keys->ov_vec.v_nr != vals->ov_vec.v_nr)
		return M0_ERR(-EINVAL);

	idx_fid = oi->oi_idx->in_entity.en_id;
	rc = idx_find(idx_fid, &midx, true);
	if (rc == 0)
		return M0_RC(-ENOENT);
	else if (rc < 0)
		return M0_RC(rc);

	/* Exaamine each input keys and find their values. */
	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		mkey.k_len  = keys->ov_vec.v_count[i];
		mkey.k_data = keys->ov_buf[i];
		kvp = m0_htable_lookup(&midx->i_kv_pairs, &mkey);

		if (kvp == NULL) {
			oi->oi_rcs[i] = -ENOENT;
			vals->ov_buf[i] = NULL;
			continue;
		}
		mval = kvp->p_val;

		/* Copy value. */
		vals->ov_buf[i] = m0_alloc(mval.v_len);
		if (vals->ov_buf[i] == NULL)
			break;
		memcpy(vals->ov_buf[i], mval.v_data, mval.v_len);
		vals->ov_vec.v_count[i] = mval.v_len;
		oi->oi_rcs[i] = 0;

		nr_found++;
	}

	/* Whether or not key found, operation is successful */
	return M0_RC(0);
}

static int idx_mock_put(struct m0_clovis_op_idx *oi)
{
	int                i;
	int                rc;
	int                nr_inserted = 0;
	int                nr_kvp;
	struct key         mkey;
	struct kv_pair    *kvp = NULL;
	struct index      *midx;
	struct m0_bufvec  *keys;
	struct m0_bufvec  *vals;
	struct m0_uint128  idx_fid;

	M0_ENTRY();

	keys = oi->oi_keys;
	vals = oi->oi_vals;
	if (keys->ov_vec.v_nr != vals->ov_vec.v_nr)
		return M0_ERR(-EINVAL);

	idx_fid = oi->oi_idx->in_entity.en_id;
	rc = idx_find(idx_fid, &midx, true);
	if (rc == 0)
		return M0_RC(-ENOENT);
	else if (rc < 0)
		return M0_RC(rc);

	/* Exaamine each input keys and find their values. */
	nr_kvp = keys->ov_vec.v_nr;
	for (i = 0; i < nr_kvp; i++) {
		mkey.k_len  = keys->ov_vec.v_count[i];
		mkey.k_data = keys->ov_buf[i];
		kvp = m0_htable_lookup(&midx->i_kv_pairs, &mkey);

		if (kvp == NULL) {
			kvp = m0_alloc(sizeof *kvp);
			if (kvp == NULL)
				break;

			kvp->p_key.k_len = mkey.k_len;
			kvp->p_key.k_data = m0_alloc(mkey.k_len);
			if (kvp->p_key.k_data == NULL)
				break;
			memcpy(kvp->p_key.k_data, mkey.k_data, mkey.k_len);
			kvp->p_val.v_len  = 0;
			kvp->p_val.v_data = NULL;

			m0_tlink_init(&kv_pairs_tl, kvp);
			kv_pairs_htable_add(&midx->i_kv_pairs, kvp);
		}

		if (kvp->p_val.v_len != vals->ov_vec.v_count[i]) {
			if (kvp->p_val.v_len != 0)
				m0_free(kvp->p_val.v_data);

			kvp->p_val.v_len  = vals->ov_vec.v_count[i];
			kvp->p_val.v_data = m0_alloc(kvp->p_val.v_len);
			if (kvp->p_val.v_data == NULL)
				break;
			memcpy(kvp->p_val.v_data, vals->ov_buf[i],
			       kvp->p_val.v_len);
		}

		nr_inserted++;
	}

	if (i != nr_kvp) {
		if (kvp && kvp->p_key.k_data)
			m0_free(kvp->p_key.k_data);
		if (kvp && kvp->p_val.v_data)
			m0_free(kvp->p_val.v_data);
		if (kvp)
			m0_free(kvp);
	}

	/* The query is considered successful if nr_inserted > 0. */
	rc = (nr_inserted > 0) ? 0 : -ENOMEM;

	return M0_RC(rc);
}

static int idx_mock_del(struct m0_clovis_op_idx *oi)
{
	int                i;
	int                rc;
	int                nr_deleted = 0;
	int                nr_kvp;
	struct key         mkey;
	struct kv_pair    *kvp;
	struct index      *midx;
	struct m0_uint128  idx_fid;

	M0_ENTRY();

	idx_fid = oi->oi_idx->in_entity.en_id;
	rc = idx_find(idx_fid, &midx, true);
	if (rc == 0)
		return M0_RC(-ENOENT);
	else if (rc < 0)
		return M0_RC(rc);

	/* Exaamine each input keys and find their values. */
	nr_kvp = oi->oi_keys->ov_vec.v_nr;
	for (i = 0; i < nr_kvp; i++) {
		if (oi->oi_keys->ov_buf[i] == NULL)
			continue;

		mkey.k_len  = oi->oi_keys->ov_vec.v_count[i];
		mkey.k_data = oi->oi_keys->ov_buf[i];
		kvp = m0_htable_lookup(&midx->i_kv_pairs, &mkey);
		if (kvp == NULL)
			continue;

		m0_htable_del(&midx->i_kv_pairs, kvp);
		m0_free(kvp->p_key.k_data);
		m0_free(kvp->p_val.v_data);
		m0_free(kvp);

		nr_deleted++;
	}

	/* The query is considered successful if nr_deleted > 0. */
	rc = (nr_deleted > 0) ? 0 : -ENOENT;

	return M0_RC(rc);
}

static int idx_mock_next(struct m0_clovis_op_idx *oi)
{
	int                rc;
	struct index      *midx;
	struct m0_uint128  idx_fid;

	M0_ENTRY();

	idx_fid = oi->oi_idx->in_entity.en_id;
	rc = idx_find(idx_fid, &midx, true);
	if (rc == 0)
		return M0_RC(-ENOENT);
	else if (rc < 0)
		return M0_RC(rc);

	return M0_RC(0);
}

static struct m0_clovis_idx_query_ops idx_mock_query_ops = {
	.iqo_namei_create = idx_mock_namei_new,
	.iqo_namei_delete = idx_mock_namei_del,
	.iqo_namei_lookup = idx_mock_namei_lookup,
	.iqo_namei_list   = idx_mock_namei_list,

	.iqo_get          = idx_mock_get,
	.iqo_put          = idx_mock_put,
	.iqo_del          = idx_mock_del,
	.iqo_next         = idx_mock_next,
};

static int idx_mock_init(void *svc)
{
	int                               rc;
	struct m0_clovis_idx_service_ctx *ctx;

	M0_ENTRY();

	if (svc == NULL)
		return M0_ERR(-EINVAL);
	ctx = (struct m0_clovis_idx_service_ctx *)svc;

	/* Check permissions on index store path. */
	idx_store_path = (char *)ctx->isc_svc_conf;
	if (idx_store_path == NULL)
		return M0_ERR(-EINVAL);

	rc = access(idx_store_path, R_OK | W_OK | X_OK);
	if (rc == 0)
		goto idx_htable;

	if ( errno != ENOENT) {
		rc = -errno;
		goto exit;
	}

	/* Create the directory for all index files. */
	rc = mkdir(idx_store_path, 0777);
	if (rc != 0) {
		rc = -errno;
		goto exit;
	}

idx_htable:
	indices_htable_init(&idx_htable, 11);

exit:
	return M0_RC(rc);
}

static int idx_mock_fini(void *svc)
{
	struct index *idx;

	m0_htable_for(indices, idx, &idx_htable) {
		/* Dump all key/value pairs into disk. */
		idx_dump(idx);

		m0_htable_del(&idx_htable, idx);
		m0_free(idx);
	}
	m0_htable_endfor;

	indices_htable_fini(&idx_htable);

	return M0_RC(0);
}

static struct m0_clovis_idx_service_ops idx_mock_svc_ops = {
	.iso_init = idx_mock_init,
	.iso_fini = idx_mock_fini
};

#else

static int idx_mock_init(void *svc)
{
	return 0;
}

static int idx_mock_fini(void *svc)
{
	return 0;
}

static int idx_mock_namei_new(struct m0_clovis_op_idx *oi)
{
	return 0;
}

static int idx_mock_namei_del(struct m0_clovis_op_idx *oi)
{
	return 0;
}

static int idx_mock_get(struct m0_clovis_op_idx *oi)
{
	return 0;
}

static int idx_mock_put(struct m0_clovis_op_idx *oi)
{
	return 0;
}

static int idx_mock_del(struct m0_clovis_op_idx *oi)
{
	return 0;
}

static int idx_mock_next(struct m0_clovis_op_idx *oi)
{
	return 0;
}

static struct m0_clovis_idx_query_ops idx_mock_query_ops = {
	.iqo_namei_create = idx_mock_namei_new,
	.iqo_namei_delete = idx_mock_namei_del,
	.iqo_namei_lookup = NULL,
	.iqo_namei_list   = NULL,
	.iqo_get          = idx_mock_get,
	.iqo_put          = idx_mock_put,
	.iqo_del          = idx_mock_del,
	.iqo_next         = idx_mock_next,
};

static struct m0_clovis_idx_service_ops idx_mock_svc_ops = {
	.iso_init = idx_mock_init,
	.iso_fini = idx_mock_fini
};

#endif

M0_INTERNAL void m0_clovis_idx_mock_register(void)
{
	m0_clovis_idx_service_register(M0_CLOVIS_IDX_MOCK,
				       &idx_mock_svc_ops, &idx_mock_query_ops);
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
