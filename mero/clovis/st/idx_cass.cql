
/* Create a keyspace for Clovis Indices */
CREATE KEYSPACE IF NOT EXISTS clovis_index_keyspace
WITH REPLICATION = { 'class' : 'SimpleStrategy', 'replication_factor' : 1 };

use clovis_index_keyspace;

/*
 * Create column families for Clovis indices, see idx_cass.c for details.
 * Currently, only one column family is used in tests.
 * 'clovis_cass_v150915' is the prefix used for column family names and
 * is secretly used in idx_cass.c(will change it later to allow applications
 * to specify it.).
 */
CREATE TABLE clovis_cass_v150915_0 (
  index_fid TEXT,
  key       BLOB,
  value     BLOB,
  PRIMARY KEY (index_fid, key)
);
/*
 * Currently, only one column family is used in tests.
 * 'clovis_cass_v150915' is the prefix used for column family names and
 * is secretly used in idx_cass.c(will change it later to allow applications
 * to specify it.).
 */
CREATE TABLE clovis_cass_idx_v150915_0 (
  index_fid TEXT,
  PRIMARY KEY (index_fid)
);
