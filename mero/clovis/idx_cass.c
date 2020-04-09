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
 * Original creation date: 11-Sept-2015
 *
 */

#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_addb.h"
#include "clovis/clovis_idx.h"

#include "lib/errno.h"             /* ENOMEM */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLOVIS
#include "lib/trace.h"

/**
 * A few notes on data model for Cassandra index service.
 *
 * Clovis index API defines a Key-Value interface supporting only a few simple
 * operations (GET/PUT/DEL/NEXT). When implementing the driver for Cassandra
 * index service, Cassandra driver should be able to make good use of its
 * advance features to achive good performance, load balance, scalability and
 * HA, we have the following requirements in mind:
 * (1) Clovis index interface requires K-V pairs are sorted in order by keys,
 *     particularly for NEXT operation.
 *
 * (2) A Clovis application may use the index API in different ways, taking S3
 *     server, an application which uses Clovis index API and Cassandra as a
 *     backend index service, as example:
 *
 *     (a) There  may be a large number of indices (for example, one index for
 *     each S3 bucket), and the number of K-V pairs in an index may vary from
 *     100s pairs to hundres of thousands. If an index is mapped to a column
 *     family directly, this large number of CF can cause heavy memory usage.
 *     10s or 100s CF is sugussted.
 *
 *     (b) Cassandra is naturally optimised for write heavy workload, while S3
 *     server exihbits a read dominant workload. This requires the Cassandra
 *     data model has to be optimized for read.
 *
 * (3) Atomic modifications on index.
 *
 * (4) Use BATCH and PREPARED queries whenever it is possible.
 *
 * Clovis defines the following schema for Cassandra index service:
 *
 * CREATE TABLE cass_index_store {
 *     index_fid TEXT,
 *     key       BLOB,
 *     vale      BLOB,
 *     PRIMARY KEY(index_fid, key)
 * };
 *
 * A few things for this schema:
 * (1) As shown in the schema definition, the row key is index fid which spreads
 *     rows into nodes(machines) by indices.
 * (2) 'Key' is clustering key so keys are physically sorted in disks (to
 *     efficiently support NEXT query). 'Key' is of BLOB type to allow
 *     applications customise their keys and values.
 * (3) The schema implies wide row is used, a few limitations on wide rows:
 *     max key(name) size: 64KB, max size for a column value: 2GB, max
 *     number of cells in a row: 2 billions
 * (4) A mapping layer is introduced to translate Clovis index to a physical
 *     Cassandra keyspace/column families. By wisely using the index FID, it
 *     is also possible to group indices into different keyspace/CF's. For
 *     example, Clovis can map S3 bucket indices(object list) into one or more
 *     CF's, while mapping indices of object id to object metadata into
 *     another keyspace/CF.
 */

#ifndef __KERNEL__

#include <uv.h>
#include <cassandra.h>

struct idx_cass_instance {
	char        *ci_keyspace;
	CassCluster *ci_cluster;
	CassSession *ci_session;
};

/**-------------------------------------------------------------------------*
 *                          Helper Functions                                *
 *--------------------------------------------------------------------------*/

static const char cass_table_magic[]     = "clovis_cass_v150915";
static const char cass_idx_table_magic[] = "clovis_cass_idx_v150915";

enum idx_cass_table_query_len {
	MAX_CASS_TABLE_NAME_LEN = 64,
	MAX_CASS_QUERY_LEN      = 128
};

enum idx_cass_prepared_statement_type {
	IDX_CASS_GET = 0,
	IDX_CASS_PUT,
	IDX_CASS_DEL,
	IDX_CASS_NEXT,
	IDX_CASS_STATEMENT_TYPE_NR
};


static int idx_cass_nr_tables;
static const CassPrepared **idx_cass_prepared_set;

/**
 * Map a index fid into a column family. Different policies
 * can be used to achieve good balance of workload.
 */
static int get_table_id(struct m0_uint128 fid)
{
	return 0;
}

static void make_table_name(char *table, int table_id, const char *table_magic)
{
	sprintf(table, "%s_%d", table_magic, table_id);
}

static void print_query_error(CassFuture *future)
{
	const char *err_msg;
	size_t      msg_len;

	cass_future_error_message(future, &err_msg, &msg_len);
	M0_LOG(M0_ERROR, "Cassandra query failed: %.*s", (int)msg_len, err_msg);
}

static CassResult* execute_query_sync(CassSession *session,
				      CassStatement *statement)
{
	int         rc;
	CassFuture *future;
	CassResult *result;

	future = cass_session_execute(session, statement);

	cass_future_wait(future);
	rc = cass_future_error_code(future);
	if (rc != CASS_OK) {
		print_query_error(future);
		return NULL;
	}

	result = (CassResult *)cass_future_get_result(future);
	cass_future_free(future);

	return result;
}

static void execute_query_async(CassSession *session, CassStatement *statement,
				CassFutureCallback cb, void *cb_data)
{
	CassFuture *future;

	future = cass_session_execute(session, statement);
	cass_future_set_callback(future, cb, cb_data);

	cass_future_free(future);
}

static CassError execute_query_batch(CassSession *session,
				     CassBatch *batch,
				     CassFutureCallback cb,
				     void *cb_data)
{
	CassError   error;
	CassFuture *future;

	future = cass_session_execute_batch(session, batch);
	error = cass_future_set_callback(future, cb, cb_data);
	cass_future_free(future);

	return error;
}

static const CassPrepared* create_prepared(CassSession *session, char *query)
{
	CassError           rc;
	CassFuture         *future = NULL;
	const CassPrepared *prepared;

	future = cass_session_prepare(session, query);
	cass_future_wait_timed(future, 300000);

	rc = cass_future_error_code(future);
	if (rc != CASS_OK) {
		print_query_error(future);
		prepared = NULL;
	} else
		prepared = cass_future_get_prepared(future);

	cass_future_free(future);
	return prepared;
}

#define CQL_GET \
	("SELECT * FROM %s WHERE index_fid = ? AND key = ?")
	//("SELECT * FROM %s WHERE index_fid = ?;")
#define CQL_PUT \
	("INSERT INTO %s (index_fid, key, value) VALUES(?, ?, ?)")
#define CQL_DEL \
	("DELETE FROM %s WHERE index_fid = ? AND key = ?")
#define CQL_NEXT \
	("SELECT * FROM %s WHERE index_fid = ? AND key > ?")

static const char *query_type_map[] = {
		[IDX_CASS_GET]  = CQL_GET,
		[IDX_CASS_PUT]  = CQL_PUT,
		[IDX_CASS_DEL]  = CQL_DEL,
		[IDX_CASS_NEXT] = CQL_NEXT
};

static char *make_query_string(int query_type, int table_id,
			       const char *table_magic)
{
	char  table[MAX_CASS_TABLE_NAME_LEN];
	char *query = NULL;

	query = m0_alloc(MAX_CASS_QUERY_LEN);
	if (query == NULL)
		goto exit;

	make_table_name(table, table_id, table_magic);

	M0_CASSERT(ARRAY_SIZE(query_type_map) == IDX_CASS_STATEMENT_TYPE_NR);
	if (IS_IN_ARRAY(query_type, query_type_map))
		sprintf(query, query_type_map[query_type], table);
	else {
		M0_LOG(M0_ERROR, "Query type %i NOT supported.", query_type);
		m0_free0(&query);
	}
exit:
	return query;
}

static int set_prepared(CassSession *session, int query_type, int table_id)
{
	int                 loc;
	char               *query;
	const CassPrepared *prepared = NULL;

	M0_ENTRY();

	query = make_query_string(query_type, table_id, cass_table_magic);
	if (query == NULL)
		return M0_ERR(-ENOMEM);

	/* Put into prepared statement array for future reference. */
	prepared = create_prepared(session, query);
	loc = table_id * IDX_CASS_STATEMENT_TYPE_NR + query_type;
	M0_LOG(M0_DEBUG, "loc = %d", loc);
	idx_cass_prepared_set[loc] = prepared;

	m0_free(query);
	return M0_RC(0);
}

static const CassPrepared* get_prepared(CassSession *session,
					int query_type, int table_id)
{
	int                 rc;
	int                 loc;
	const CassPrepared *prepared = NULL;

	M0_ENTRY();

	loc  = table_id * IDX_CASS_STATEMENT_TYPE_NR + query_type;
	if( idx_cass_prepared_set[loc] != NULL) {
		prepared =  idx_cass_prepared_set[loc];
		goto exit;
	}

	/* Create a prepared statement if it does not exist. */
	rc = set_prepared(session, query_type, table_id);
	if (rc != 0)
		prepared = NULL;
	else
		prepared = idx_cass_prepared_set[loc];

exit:
	M0_LEAVE();
	return prepared;
}

static int init_prepared_set(CassSession *session)
{
	int i;
	int nr_statements;

	nr_statements =
		IDX_CASS_STATEMENT_TYPE_NR * idx_cass_nr_tables;
	idx_cass_prepared_set =
		m0_alloc(nr_statements * sizeof(CassPrepared *));
	if (idx_cass_prepared_set == NULL)
		return M0_ERR(-ENOMEM);

	for (i = 0; i < nr_statements; i++)
		idx_cass_prepared_set[i] = NULL;

	return M0_RC(0);
}

static void free_prepared_set()
{
	int i;
	int nr_statements;

	nr_statements =
		IDX_CASS_STATEMENT_TYPE_NR * idx_cass_nr_tables;
	for (i = 0; i < nr_statements; i++) {
		if (idx_cass_prepared_set[i] != NULL)
			cass_prepared_free(idx_cass_prepared_set[i]);
	}

	m0_free(idx_cass_prepared_set);
}

#ifdef CLOVIS_IDX_CASS_DRV_V22 /* Cassandra cpp driver version >=2.2. */

static int table_exists(CassSession *session, char *keyspace, int table_id,
		        const char *table_magic)
{
	int                     rc;
	char                    table[MAX_CASS_TABLE_NAME_LEN];
	const CassSchemaMeta   *schema_meta;
	const CassKeyspaceMeta *keyspace_meta;
	const CassTableMeta    *table_meta;

	schema_meta = cass_session_get_schema_meta(session);
	keyspace_meta =
		cass_schema_meta_keyspace_by_name(schema_meta, keyspace);
	if (keyspace_meta == NULL) {
		cass_schema_meta_free(schema_meta);
		rc = -EINVAL;
		goto exit;
	}

	make_table_name(table, table_id, table_magic);
	table_meta = cass_keyspace_meta_table_by_name(keyspace_meta, table);
	if (table_meta != NULL)
		rc = 1;
	else
		rc = 0;

exit:
	cass_schema_meta_free(schema_meta);
	return M0_RC(rc);
}

#else /* Cassandra cpp driver version < 2.2. */

static int table_exists(CassSession *session, char *keyspace, int table_id,
		        const char *table_magic)
{
	int                   rc;
	char                  table[MAX_CASS_TABLE_NAME_LEN];
	const CassSchema     *schema;
	const CassSchemaMeta *keyspace_meta;
	const CassSchemaMeta *table_meta;

	schema = cass_session_get_schema(session);
	keyspace_meta = cass_schema_get_keyspace(schema, keyspace);
	if (keyspace_meta == NULL) {
		rc = -EINVAL;
		goto exit;
	}

	make_table_name(table, table_id, table_magic);
	table_meta = cass_schema_meta_get_entry(keyspace_meta, table);
	if (table_meta != NULL)
		rc = 1;
	else
		rc = 0;

exit:
	cass_schema_free(schema);
	return M0_RC(rc);
}
#endif

static int row_exists(CassSession *session, int table_id,
		      struct m0_uint128 idx_fid, const char *table_magic)
{
	int           rc = 0;
	char          *query;
	CassStatement *statement;
	CassResult    *query_result;
	CassIterator  *iterator;
	char           table[MAX_CASS_TABLE_NAME_LEN];
	char           idx_fid_str[64];

	query = m0_alloc(MAX_CASS_QUERY_LEN);
	if (query == NULL)
		return M0_ERR(-ENOMEM);

	make_table_name(table, table_id, table_magic);
	sprintf(query, "SELECT * FROM %s WHERE index_fid = ?", table);

	statement = cass_statement_new(query, 1);
	sprintf(idx_fid_str, "%"PRIx64"_%"PRIx64, idx_fid.u_hi, idx_fid.u_lo);
	cass_statement_bind_string(statement, 0, idx_fid_str);

	query_result = execute_query_sync(session, statement);
	if (query_result == NULL)
		return M0_ERR(-EINVAL);

	/* Check if Cassandra returns any rows. */
	iterator = cass_iterator_from_result(query_result);
	if (cass_iterator_next(iterator))
		rc = 1;
	else
		rc = 0;

	m0_free(query);
	cass_statement_free(statement);
	cass_iterator_free(iterator);
	cass_result_free(query_result);

	return M0_RC(rc);
}

static struct idx_cass_instance* get_cass_inst(struct m0_clovis_op_idx *oi)
{
	struct m0_clovis                 *m0c;

	m0c = m0_clovis__entity_instance(oi->oi_oc.oc_op.op_entity);
	return (struct idx_cass_instance *) m0c->m0c_idx_svc_ctx.isc_svc_inst;
}

static bool idx_exists(struct m0_clovis_op_idx *oi)
{
	int                 table_id;
	struct m0_uint128   idx_fid;
	CassSession        *session;
	int                 exist;
	char               *keyspace;

	M0_ENTRY();

	idx_fid = oi->oi_idx->in_entity.en_id;

	/* Form a 'SELECT' query string. */
	session  = get_cass_inst(oi)->ci_session;
	keyspace = get_cass_inst(oi)->ci_keyspace;
	table_id = get_table_id(idx_fid);

	/* Check if the table storing rows of this index exists. */
	exist = table_exists(session, keyspace, table_id, cass_idx_table_magic);
	if (exist > 0) {
		/* Check if there exists any row for this index. */
		exist = row_exists(session, table_id, idx_fid,
				   cass_idx_table_magic);
	}

	return exist > 0 ? M0_RC(true) : M0_RC(false);
}

/**-------------------------------------------------------------------------*
 *                           Query Operations                               *
 *--------------------------------------------------------------------------*/

/**
 * Default callback for every query to return the control to Clovis side.
 */
static void idx_cass_query_cb(CassFuture* future, void* data)
{
	struct m0_clovis_op_idx *oi;
	CassError                rc;

	M0_ENTRY();

	oi = (struct m0_clovis_op_idx *)data;

	rc = cass_future_error_code(future);
	if (rc != CASS_OK) {
		oi->oi_query_rc = rc;
		print_query_error(future);
	}

	oi->oi_nr_queries--;
	if (oi->oi_nr_queries != 0)
		return;

	/*
	 * An index GET queryClovis is considered success only if operation is
	 * executed successfully. If operation failed to execute, i.e. global
	 * error code, oi->oi_query_rc, is not success clovis will call
	 * fail callback.
	 * In all other cases, operation is considered success.
	 * (Even if all the keys presented in queries are absent from store.).
	 */
	if (rc == CASS_OK) {
		oi->oi_ar.ar_ast.sa_cb = &clovis_idx_op_ast_complete;
		oi->oi_ar.ar_rc = 0;
		m0_sm_ast_post(oi->oi_sm_grp, &oi->oi_ar.ar_ast);

	} else {
		oi->oi_ar.ar_ast.sa_cb = &clovis_idx_op_ast_fail;
		oi->oi_ar.ar_rc = -rc;
		m0_sm_ast_post(oi->oi_sm_grp, &oi->oi_ar.ar_ast);
	}
}

static int idx_cass_namei_new(struct m0_clovis_op_idx *oi)
{
	char                     *query;
	char                      table[MAX_CASS_TABLE_NAME_LEN];
	CassSession              *session;
	CassStatement            *statement;
	struct idx_cass_instance *cass_inst;
	char                idx_fid_str[64];
	struct m0_uint128         idx_fid;

	cass_inst = get_cass_inst(oi);
	session   = cass_inst->ci_session;
	idx_fid  = oi->oi_idx->in_entity.en_id;

	/* If the index exits */
	if (idx_exists(oi))
		return M0_ERR(-EEXIST);

	oi->oi_query_rc = CASS_OK;
	oi->oi_nr_queries = 1;

	query = m0_alloc(MAX_CASS_QUERY_LEN);
	if (query == NULL)
		return M0_ERR(-ENOMEM);

	make_table_name(table,
			get_table_id(oi->oi_idx->in_entity.en_id),
			cass_idx_table_magic);
	sprintf(query,
		"INSERT INTO %s (index_fid) VALUES(?)", table);
	statement = cass_statement_new(query, 1);
	sprintf(idx_fid_str, "%"PRIx64"_%"PRIx64,
		idx_fid.u_hi, idx_fid.u_lo);

	cass_statement_bind_string(statement, 0, idx_fid_str);

	execute_query_async(session,
			    statement, idx_cass_query_cb, oi);
	m0_free(query);
	cass_statement_free(statement);

	/* Return 1 */
	return M0_RC(1);
}

static int idx_cass_namei_drop(struct m0_clovis_op_idx *oi)
{
	char                     *query;
	char                      table[MAX_CASS_TABLE_NAME_LEN];
	CassSession              *session;
	CassStatement            *statement;
	struct idx_cass_instance *cass_inst;
	char                      idx_fid_str[64];
	struct m0_uint128         idx_fid;
	CassBatch                *batch;

	cass_inst = get_cass_inst(oi);
	session   = cass_inst->ci_session;
	idx_fid   = oi->oi_idx->in_entity.en_id;
	batch     = cass_batch_new(CASS_BATCH_TYPE_LOGGED);

	/*
	 * Delete all the KV-pairs associated with an index.
	 */
	oi->oi_query_rc = CASS_OK;
	oi->oi_nr_queries = 1;

	query = m0_alloc(MAX_CASS_QUERY_LEN);
	if (query == NULL)
		return M0_ERR(-ENOMEM);

	make_table_name(table, get_table_id(oi->oi_idx->in_entity.en_id),
			cass_table_magic);
	sprintf(query,
		"DELETE from %s where  index_fid = ?", table);

	statement = cass_statement_new(query, 1);
	sprintf(idx_fid_str, "%"PRIx64"_%"PRIx64, idx_fid.u_hi, idx_fid.u_lo);
	cass_statement_bind_string(statement, 0, idx_fid_str);
	cass_batch_add_statement(batch, statement);
	cass_statement_free(statement);

	/* Now delete the index entry from the index table. */
	make_table_name(table, get_table_id(oi->oi_idx->in_entity.en_id),
			cass_idx_table_magic);
	sprintf(query,
		"DELETE from %s where  index_fid = ?", table);

	statement = cass_statement_new(query, 1);
	sprintf(idx_fid_str, "%"PRIx64"_%"PRIx64, idx_fid.u_hi, idx_fid.u_lo);
	cass_statement_bind_string(statement, 0, idx_fid_str);
	cass_batch_add_statement(batch, statement);
	execute_query_batch(session, batch, idx_cass_query_cb, oi);

	m0_free(query);
	cass_statement_free(statement);

	return M0_RC(1);
}

static int idx_cass_namei_lookup(struct m0_clovis_op_idx *oi)
{
	return M0_ERR(-ENOSYS);
}

static int idx_cass_namei_list(struct m0_clovis_op_idx *oi)
{
	return M0_ERR(-ENOSYS);
}

struct get_cb_data {
	int                      gcd_kv_idx;
	struct m0_clovis_op_idx *gcd_oi;
};

static void idx_cass_get_cb(CassFuture* future, void* data)
{
	int                      kv_idx = 0;
	struct m0_bufvec        *vals;
	struct m0_clovis_op_idx *oi;
	const CassResult        *result;
	CassIterator            *iterator = NULL;
	const CassRow           *row;
	const cass_byte_t       *bytes;
	size_t                   nr_bytes;

	M0_ENTRY();

	oi     = ((struct get_cb_data *)data)->gcd_oi;
	kv_idx = ((struct get_cb_data *)data)->gcd_kv_idx;
	vals   = oi->oi_vals;

	result = cass_future_get_result(future);
	if (result == NULL) {
		oi->oi_rcs[kv_idx] = -ENOENT;
		goto query_cb;
	}

	iterator = cass_iterator_from_result(result);
	if (iterator == NULL) {
		oi->oi_rcs[kv_idx] = -ENOENT;
		goto query_cb;
	}

	if (cass_iterator_next(iterator)) {
		M0_LOG(M0_DEBUG, "Get row and extract values.");
		row = cass_iterator_get_row(iterator);
		cass_value_get_bytes(cass_row_get_column(row, 2),
				     &bytes, &nr_bytes);

		/* Copy value back to K-V pair. */
		M0_LOG(M0_DEBUG, "Copy value.");
		vals->ov_buf[kv_idx] = m0_alloc(nr_bytes);
		if (vals->ov_buf[kv_idx] == NULL) /*TODO: How to handle this error? */
			goto query_cb;
		memcpy(vals->ov_buf[kv_idx], bytes, nr_bytes);
		vals->ov_vec.v_count[kv_idx] = nr_bytes;
		oi->oi_rcs[kv_idx] = 0;
	} else {
		oi->oi_rcs[kv_idx] = -ENOENT;
	}


query_cb:
	idx_cass_query_cb(future, oi);

	/* Free result and 'data'. */
	cass_iterator_free(iterator);
	cass_result_free(result);
	m0_free(data);
	M0_LEAVE();
}

/*
 * The simplest implementation: one query for one K-V pair, it is not
 * optimized for performance just to make index GET work.
 */
static int idx_cass_get(struct m0_clovis_op_idx *oi)
{
	int                 i;
	int                 rc = 0;
	int                 nr_selects = 0;
	int                 table_id;
	char               *query = NULL;
	char                idx_fid_str[64];
	struct m0_uint128   idx_fid;
	struct m0_bufvec   *keys;
	CassSession        *session;
	CassStatement      *statement;
	struct get_cb_data *gcd;
	bool                exist;

	M0_ENTRY();

	idx_fid = oi->oi_idx->in_entity.en_id;
	sprintf(idx_fid_str, "%"PRIx64"_%"PRIx64, idx_fid.u_hi, idx_fid.u_lo);

	/* Form a 'SELECT' query string. */
	session  = get_cass_inst(oi)->ci_session;
	table_id = get_table_id(idx_fid);

	/* If the index exits */
	exist = idx_exists(oi);
	if (!exist)
		return M0_ERR(-ENOENT);

	query = make_query_string(IDX_CASS_GET, table_id, cass_table_magic);
	if (query == NULL)
		return M0_ERR(-ENOMEM);

	/*
	 * Examine each key, form a new statement for a key and
	 * pack into a batch query.
	 */
	keys  = oi->oi_keys;
	oi->oi_query_rc = CASS_OK;
	oi->oi_nr_queries = keys->ov_vec.v_nr;
	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		/* Crate a statement for each K-V pair. */
		statement = cass_statement_new(query, 2);
		cass_statement_bind_string(statement, 0, idx_fid_str); //"0_8ad6");
		cass_statement_bind_bytes(statement, 1,
				          (cass_byte_t *)keys->ov_buf[i],
				          keys->ov_vec.v_count[i]);

		/* Set callback and its data, then issue the query. */
		gcd = m0_alloc(sizeof(struct get_cb_data));
		if (gcd == NULL) {
			rc = -ENOMEM;
			break;
		}

		gcd->gcd_oi = oi;
		gcd->gcd_kv_idx = i;
		execute_query_async(session, statement, idx_cass_get_cb, gcd);

		/* Post-processing.*/
		nr_selects++;
		cass_statement_free(statement);
	}
	rc = (nr_selects > 0)?1:rc;

	m0_free(query);
	return M0_RC(rc);
}

static int idx_cass_put(struct m0_clovis_op_idx *oi)
{
	int                 i;
	int                 table_id;
	CassSession        *session;
	CassStatement      *statement;
	const CassPrepared *prepared;
	CassBatch          *batch;
	struct m0_bufvec   *keys;
	struct m0_bufvec   *vals;
	struct m0_uint128   idx_fid;
	char                idx_fid_str[64];
	bool                exist;

	M0_ENTRY();

	keys = oi->oi_keys;
	vals = oi->oi_vals;
	if (keys->ov_vec.v_nr != vals->ov_vec.v_nr)
		return M0_RC(-EINVAL);

	idx_fid  = oi->oi_idx->in_entity.en_id;
	sprintf(idx_fid_str, "%"PRIx64"_%"PRIx64, idx_fid.u_hi, idx_fid.u_lo);

	/* All K-V pair insert are added into one 'big' BATCH query. */
	table_id = get_table_id(idx_fid);
	session  = get_cass_inst(oi)->ci_session;
	batch    = cass_batch_new(CASS_BATCH_TYPE_LOGGED);

	/* If the index exits */
	exist = idx_exists(oi);
	if (!exist)
		return M0_ERR(-ENOENT);

	oi->oi_nr_queries = 1;
	oi->oi_query_rc = CASS_OK;
	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		prepared = get_prepared(session, IDX_CASS_PUT, table_id);
		if (prepared == NULL)
			goto exit;

		M0_LOG(M0_DEBUG, "Bind Cassandra statement.");
		statement = cass_prepared_bind(prepared);
		cass_statement_bind_string(statement, 0, idx_fid_str);
		cass_statement_bind_bytes(statement, 1,
				keys->ov_buf[i], keys->ov_vec.v_count[i]);
		cass_statement_bind_bytes(statement, 2,
				vals->ov_buf[i], vals->ov_vec.v_count[i]);

		M0_LOG(M0_DEBUG, "Add Cassandra statement to batch.");
		cass_batch_add_statement(batch, statement);

		cass_statement_free(statement);
	}

	/* Actual BATCH query is executed here. */
	M0_LOG(M0_DEBUG, "Issue batch query for K-V PUT.");
	execute_query_batch(session, batch, idx_cass_query_cb, oi);

exit:
	cass_batch_free(batch);
	return M0_RC(1);
}

static int idx_cass_del(struct m0_clovis_op_idx *oi)
{
	int                 i;
	int                 table_id;
	CassSession        *session;
	CassStatement      *statement;
	CassBatch          *batch;
	struct m0_bufvec   *keys;
	struct m0_uint128   idx_fid;
	char                idx_fid_str[64];
	char               *query;

	M0_ENTRY();

	keys = oi->oi_keys;
	idx_fid  = oi->oi_idx->in_entity.en_id;
	sprintf(idx_fid_str, "%"PRIx64"_%"PRIx64, idx_fid.u_hi, idx_fid.u_lo);

	/* All K-V pair insert are added into one 'big' BATCH query. */
	batch    = cass_batch_new(CASS_BATCH_TYPE_LOGGED);

	/* Form a 'DELETE' query string. */
	session  = get_cass_inst(oi)->ci_session;
	table_id = get_table_id(idx_fid);
	query = make_query_string(IDX_CASS_DEL, table_id, cass_table_magic);
	if (query == NULL)
		return M0_ERR(-ENOMEM);

	oi->oi_nr_queries = 1;
	oi->oi_query_rc = CASS_OK;
	for (i = 0; i < keys->ov_vec.v_nr; i++) {
		M0_LOG(M0_DEBUG, "Bind Cassandra statement.");
		statement = cass_statement_new(query, 2);
		cass_statement_bind_string(statement, 0, idx_fid_str);
		cass_statement_bind_bytes(statement, 1,
				keys->ov_buf[i], keys->ov_vec.v_count[i]);

		M0_LOG(M0_DEBUG, "Add Cassandra statement to batch.");
		cass_batch_add_statement(batch, statement);

		cass_statement_free(statement);
	}

	/* Actual BATCH query is executed here. */
	M0_LOG(M0_DEBUG, "Issue batch query for K-V DEL.");
	execute_query_batch(session, batch, idx_cass_query_cb, oi);

	cass_batch_free(batch);
	m0_free(query);
	return M0_RC(1);
}

static int copy_to_kv(int i, struct m0_bufvec *keys, struct m0_bufvec *vals,
		      const cass_byte_t *k_bytes, size_t k_size,
		      const cass_byte_t *v_bytes, size_t v_size)
{
	keys->ov_buf[i] = m0_alloc(k_size);
	if (keys->ov_buf[i] == NULL)
		return M0_ERR(-ENOMEM);

	vals->ov_buf[i] = m0_alloc(v_size);
	if (vals->ov_buf[i] == NULL) {
		m0_free(keys->ov_buf[i]);
		return M0_ERR(-ENOMEM);
	}

	memcpy(keys->ov_buf[i], k_bytes, k_size);
	keys->ov_vec.v_count[i] = k_size;

	memcpy(vals->ov_buf[i], v_bytes, v_size);
	vals->ov_vec.v_count[i] = v_size;

	return M0_RC(0);
}

static void idx_cass_next_cb(CassFuture* future, void* data)
{
	int                      rc;
	int                      kv_cnt = 0;
	struct m0_bufvec        *keys;
	struct m0_bufvec        *vals;
	size_t                   k_size;
	size_t                   v_size;
	const cass_byte_t       *k_bytes;
	const cass_byte_t       *v_bytes;
	const CassResult        *result;
	CassIterator            *iterator = NULL;
	const CassRow           *row;
	struct m0_clovis_op_idx *oi;

	oi   = (struct m0_clovis_op_idx *)data;
	keys = oi->oi_keys;
	vals = oi->oi_vals;

	/* Reset buf for the first key. */
	m0_free(keys->ov_buf[0]);
	keys->ov_buf[0] = NULL;
	keys->ov_vec.v_count[0] = 0;

	/* Iterate over each returned rows and copy keys and values. */
	result = cass_future_get_result(future);
	if (result == NULL)
		goto query_cb;

	iterator = cass_iterator_from_result(result);
	if (iterator == NULL)
		goto query_cb;

	while (cass_iterator_next(iterator)) {
		row = cass_iterator_get_row(iterator);

		cass_value_get_bytes(cass_row_get_column(row, 1),
				     &k_bytes, &k_size);
		cass_value_get_bytes(cass_row_get_column(row, 2),
				     &v_bytes, &v_size);

		/* Copy key and value. */
		rc = copy_to_kv(kv_cnt, keys, vals,
				k_bytes, k_size, v_bytes, v_size);
		if (rc != 0) {
			break;
		}

		kv_cnt++;
		if (kv_cnt >= keys->ov_vec.v_nr)
			break;
	}


query_cb:
	idx_cass_query_cb(future, oi);

	cass_iterator_free(iterator);
	/* Free query result. */
	cass_result_free(result);
}

static int idx_cass_next(struct m0_clovis_op_idx *oi)
{
	int                 table_id;
	char               *query = NULL;
	char                idx_fid_str[64];
	char		    keys_v_nr[12];
	struct m0_uint128   idx_fid;
	struct m0_bufvec   *keys;
	CassSession        *session;
	CassStatement      *statement;
	int64_t             null_key = 0;
	bool                exist;

	M0_ENTRY();

	keys  = oi->oi_keys;
	idx_fid = oi->oi_idx->in_entity.en_id;
	sprintf(idx_fid_str, "%"PRIx64"_%"PRIx64, idx_fid.u_hi, idx_fid.u_lo);

	session  = get_cass_inst(oi)->ci_session;

	/* Make a query statement for NEXT. */
	table_id = get_table_id(idx_fid);

	/* If the index exists */
	exist = idx_exists(oi);
	if (!exist)
		return M0_ERR(-ENOENT);

	query = make_query_string(IDX_CASS_NEXT, table_id, cass_table_magic);
	if (query == NULL)
		return M0_ERR(-ENOMEM);

	strcat(query, " LIMIT ");
	sprintf(keys_v_nr, "%d", keys->ov_vec.v_nr);
	strcat(query, keys_v_nr);

	statement = cass_statement_new(query, 2);
	cass_statement_bind_string(statement, 0, idx_fid_str);

	/*
	 * If the start key is not specified, records are retrieved
	 * from the very beginning of the index. 0 can be safely seen
	 * as the minimum key value no matter how an application defines
	 * its own key data structure.
	 */
	if (keys->ov_buf[0] == NULL) {
		cass_statement_bind_bytes(statement, 1,
			(cass_byte_t *)&null_key, sizeof null_key);
	} else {
		cass_statement_bind_bytes(statement, 1,
			keys->ov_buf[0], keys->ov_vec.v_count[0]);

	}

	/* Set callback and its data, then issue the query. */
	oi->oi_nr_queries = 1;
	oi->oi_query_rc = CASS_OK;
	execute_query_async(session, statement, idx_cass_next_cb, oi);

	/* Post-processing.*/
	cass_statement_free(statement);

	m0_free(query);
	return M0_RC(1);
}

static struct m0_clovis_idx_query_ops idx_cass_query_ops = {
	.iqo_namei_create = idx_cass_namei_new,
	.iqo_namei_delete = idx_cass_namei_drop,
	.iqo_namei_lookup = idx_cass_namei_lookup,
	.iqo_namei_list   = idx_cass_namei_list,

	.iqo_get          = idx_cass_get,
	.iqo_put          = idx_cass_put,
	.iqo_del          = idx_cass_del,
	.iqo_next         = idx_cass_next,
};

/**-------------------------------------------------------------------------*
 *               Service Initialisation and Finalisation                    *
 *--------------------------------------------------------------------------*/

static CassCluster* create_cluster(char *ep)
{
	CassCluster *cluster;

	cluster = cass_cluster_new();
	cass_cluster_set_contact_points(cluster, ep);

	return cluster;
}

static CassSession* connect_session(CassCluster *cluster, char *keyspace)
{
	CassError    rc;
	CassSession *session;
	CassFuture  *future;

	session = cass_session_new();
	future = cass_session_connect_keyspace(session, cluster, keyspace);

	cass_future_wait(future);
	rc = cass_future_error_code(future);
	if (rc != CASS_OK) {
		print_query_error(future);

		cass_session_free(session);
		session = NULL;
	}
	cass_future_free(future);

	return session;
}

static void terminate_session(CassSession *session)
{
	CassFuture *future;

	future = cass_session_close(session);
	cass_future_wait(future);
	cass_future_free(future);
}

static int idx_cass_init(void *svc)
{
	int                               rc;
	char                             *ep;
	char                             *keyspace = NULL;
	struct m0_clovis_idx_service_ctx *ctx;
	struct m0_idx_cass_config        *conf;
	struct idx_cass_instance         *inst;
	CassCluster                      *cluster;
	CassSession                      *session = NULL;

	M0_ENTRY();

	/* Connect to Cassandra cluster. */
	if (svc == NULL)
		return M0_ERR(-EINVAL);
	ctx  = (struct m0_clovis_idx_service_ctx *)svc;
	conf = (struct m0_idx_cass_config *)ctx->isc_svc_conf;
	ep   = conf->cc_cluster_ep;
	keyspace = conf->cc_keyspace;

	rc = -ENETUNREACH;
	cluster = create_cluster(ep);
	if (cluster == NULL)
		goto error;

	session = connect_session(cluster, keyspace);
	if (session == NULL)
		goto error;

	/* Set Cassandra instance. */
	inst = m0_alloc(sizeof *inst);
	if (inst == NULL) {
		rc = -ENOMEM;
		goto error;
	}

	inst->ci_cluster  = cluster;
	inst->ci_session  = session;
	inst->ci_keyspace = keyspace;
	ctx->isc_svc_inst = inst;

	/* Set prepared statements. */
	idx_cass_nr_tables = conf->cc_max_column_family_num;
	init_prepared_set(session);

	return M0_RC(0);

error:
	if (session != NULL) {
		terminate_session(session);
		cass_session_free(session);
	}
	if (cluster != NULL)
		cass_cluster_free(cluster);

	return M0_RC(rc);
}

static int idx_cass_fini(void *svc)
{
	struct idx_cass_instance *inst;

	M0_ENTRY();

	if (svc == NULL)
		return M0_ERR(-EINVAL);
	inst  = ((struct m0_clovis_idx_service_ctx *)svc)->isc_svc_inst;

	terminate_session(inst->ci_session);
	cass_session_free(inst->ci_session);
	cass_cluster_free(inst->ci_cluster);

	m0_free(inst);

	/*TODO: prepared statements.  */
	free_prepared_set();

	return M0_RC(0);
}

static struct m0_clovis_idx_service_ops idx_cass_svc_ops = {
	.iso_init = idx_cass_init,
	.iso_fini = idx_cass_fini
};

#else
static struct m0_clovis_idx_query_ops idx_cass_query_ops = {
	.iqo_namei_create = NULL,
	.iqo_namei_delete = NULL,
	.iqo_namei_lookup = NULL,
	.iqo_namei_list   = NULL,
	.iqo_get          = NULL,
	.iqo_put          = NULL,
	.iqo_del          = NULL,
	.iqo_next         = NULL,
};

static struct m0_clovis_idx_service_ops idx_cass_svc_ops = {
	.iso_init = NULL,
	.iso_fini = NULL
};

#endif

M0_INTERNAL void m0_clovis_idx_cass_register(void)
{
	m0_clovis_idx_service_register(M0_CLOVIS_IDX_CASS,
				       &idx_cass_svc_ops, &idx_cass_query_ops);
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
