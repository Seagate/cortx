/* -*- C -*- */
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
 * Original author: Nathan Rutman <nathan_rutman@xyratex.com>
 * Original creation date: 10/24/2010
 *
 * Mdstore changes: Yuriy Umanets <yuriy_umanets@xyratex.com>
 */

#pragma once

#ifndef __MERO_COB_COB_H__
#define __MERO_COB_COB_H__

#include "file/file.h"
#include "lib/atomic.h"
#include "lib/rwlock.h"
#include "lib/refs.h"
#include "lib/bitstring.h"
#include "lib/bitstring_xc.h"
#include "fid/fid.h"
#include "mdservice/md_fid.h"
#include "be/btree.h"
#include "be/btree_xc.h"

/* import */
struct m0_be_tx;
struct m0_be_btree;
struct m0_be_domain;
struct m0_stob;

/**
   @defgroup cob Component objects

   A Component object is a metadata layer that holds metadata
   information.  It references a single storage object and contains
   metadata describing the object. The metadata is stored in database
   tables. A M0 Global Object (i.e. file) is made up of a collection
   of Component Objects (stripes).

   Component object metadata includes:

   - namespace information: parent object id, name, links;
   - file attributes: owner/mode/group, size, m/a/ctime, acls;
   - fol reference information: log sequence number (lsn), version counter.

   Metadata organization:

   COB uses four db tables for storing the following pieces of
   information:

   - Namespace - stores file names and file attributes for readdir
   speedup.  In case of "ls -al" request some metadata needed to
   complete it and it is why minimum necessary attributes are stored
   in namespace table;

   - Object Index - stores links of file (all names of the file);

   - File attributes - stores file version, some replicator fields,
   basically anything that is not needed during stat and readdir
   operations;

   - One more table is needed for so called "omg" records
   (owner/mode/group). They store mode/uid/gid file attributes.

   For traditional file systems' namespace we need two tables: name
   space and object index. These tables are used as following:

   Suppose that there is a file F that has got three names:

   "a/f0", "b/f1", "c/f2"

   Then namespace will have the following records:

   (a.fid, "f0") -> (F stat data, 0)
   (b.fid, "f1") -> (F, 1)
   (c.fid, "f2") -> (F, 2)

   where, in first record, we have "DB key" constructed of f0's
   parent fid (the directory fid) and "f0", the filename itself.
   The namespace record contains the fid of file "f0" together with
   with its stat data, plus the link number for this name.

   Here "stat data" means that this record contains file attributes
   that usually extracted by stat utility or ls -la.

   First name stores the stat data (basic file system attributes)
   and has link number zero.

   All the other records have keys constructed from their parent
   fid and child name ("f1", "f2"), and the record contains only
   file fid and link number that this record describes.

   When the first file name is deleted, the stat data is migrated
   to another name record of the file (if any). This will be shown
   below.

   The object index contains records:

   (F, 0) -> (a.fid, "f0")
   (F, 1) -> (b.fid, "f1")
   (F, 2) -> (c.fid, "f2")

   where the key is constructed of file fid and its link number.
   The record contains the parent fid plus the file name. That is,
   the object index enumerates all the names that a file has. As
   we have already noted, the object index table values have the
   same format as namespace keys and may be used for getting namespace
   data and cob object itself using object's fid.

   When doing "rm a/f0" we need to kill 1 namespace and 1 object
   index record.  That is, we need a key containing (a.fid, "f0").
   Using this key we can find its position in namespace table and
   delete the record.  But before killing it, we need to check if
   this record stores the stat data and whether we should move it
   to another hardlink filename.  If ->m0_cob_nsrec::cnr_nlink > 0
   this means that it is stat data record and there are other
   hardlinks for the file, so stat data should be moved.  To find
   out where to move stat data quickly, we just do lookup in the
   object index for the next linkno of the file:

   (F, 0 + 1) -> (b.fid, "f1")

   Now we can use its record as a key for namespace table and move
   stat data to its record.

   Now kill the record in the object index.

   Stat data could be located in namespace table by using the very
   first record in object index table, which will be (F, 1) now.
   We still can initialize object index key with linkno=0 to find
   it and rely on database cursor which returns the first record
   in the table with the least linkno available.

   We are done with unlink operation. Of course for cases when
   killing name that does not hold stat data, algorithm is much
   simpler. We just need to kill one record in name, find and update
   stat data record in namespace table (decremented nlink should
   be updated in the store), and kill one record in object index.

   File attributes that are stored in separate tables may also be
   easily accessed using key constructed of F, where F is file fid.

   Rationale:

   Hard-links are rare, make common case fast by placing attributes
   in the file's directory entry, make readdir fast by moving other
   attributes out.

   Corner cases:

   Rename and unlink need to move file attributes from zero name.

   Special case:

   Using cob api for ioservice to create data objects is special
   case. The main difference is that, parent fid (in nskey) and
   child fid (in nsrec) are the same.

   Cob iterator.

   In order to iterate over all names that "dir" cob contains, cob
   iterator is used. It is simple cursor based API, that contains
   four methods:

   - m0_cob_iterator_init() - init iterator with cob fid and file name;
   - m0_cob_iterator_get()  - position iterator according with its properies;
   - m0_cob_iterator_next() - move to next position;
   - m0_cob_iterator_fini() - fini iterator.

   `cob' and `name' parameters, passed to m0_cob_iterator_init(),
   specify initial position of the iterator. Then iterator moves
   with m0_cob_iterator_next() method over namespace table until
   end of table is reached or a record with another parent fid is
   found.

   Once iterator is not needed, it is finalized by m0_cob_iterator_fini().

   @see m0_mdstore_readdir() for example of using iterator.

   Mkfs.

   Cob cannot be used before m0_cob_domain_mkfs() is called. For
   details consult with m0_cob_domain_mkfs()

   This function creates the following structures [records? objects?]
   in cob tables:

   - the main root cob with fid M0_COB_ROOT_FID;

   - metadata hierarchy root cob (what potentially metadata client
   can see) with fid M0_COB_ROOT_FID and name M0_COB_ROOT_NAME;

   - omgid terminator record with id = ~0ULL. This is used for omgid
   allocation during m0_cob_create();

   Mdstore based cobs cannot be used properly without mkfs done.
   All unit tests that access cob and also all modules using cobs
   should do m0_cob_domain_mkfs() on startup.

   @{
 */

struct m0_cob;
struct m0_fid;
struct m0_cob_id;
struct m0_cob_domain;
struct m0_cob_domain_id;

/*
 * This is used to separate the range of COB IDs for IOS-es and MDS-es, which is
 * needed to do correct restart and match cob domains by id for each service
 * which uses its own cob-domain.
 */
enum {
	M0_MDS_COB_ID_START = 0,
	M0_IOS_COB_ID_START = 1000,
};

/**
   Unique cob domain identifier.

   A cob_domain identifier distinguishes domains within a single
   database environment.
 */
struct m0_cob_domain_id {
	uint64_t id;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
   cob domain

   Component object metadata is stored in database tables. The
   database in turn is stored in a metadata container. A cob_domain
   is a grouping of cobs described by a namespace or object index.
   The objects referenced by the tables will reside in other,
   filedata containers.

   A m0_cob_domain is an in-memory structure that identifies these
   tables.  The list of domains will be created at metadata container
   ingest (when a container is first "started/read/initialized/opened".)

   A m0_cob_domain cannot span multiple containers.  Eventually,
   there should be methods for combining/splitting containers and
   therefore cob domains and databases.

   Note: has to be allocated with m0_be_alloc()
*/
struct m0_cob_domain {
	struct m0_format_header cd_header;
	struct m0_cob_domain_id cd_id;
	struct m0_format_footer cd_footer;
	/*
	 * m0_be_btree has it's own volatile-only fields, so it can't be placed
	 * before the m0_format_footer, where only persistent fields allowed
	 */
	struct m0_be_btree      cd_object_index;
	struct m0_be_btree      cd_namespace;
	struct m0_be_btree      cd_fileattr_basic;
	struct m0_be_btree      cd_fileattr_omg;
	struct m0_be_btree      cd_fileattr_ea;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_cob_domain_format_version {
	M0_COB_DOMAIN_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_COB_DOMAIN_FORMAT_VERSION */
	/*M0_COB_DOMAIN_FORMAT_VERSION_2,*/
	/*M0_COB_DOMAIN_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_COB_DOMAIN_FORMAT_VERSION = M0_COB_DOMAIN_FORMAT_VERSION_1
};

int m0_cob_domain_init(struct m0_cob_domain *dom,
		       struct m0_be_seg     *seg);
void m0_cob_domain_fini(struct m0_cob_domain *dom);

/** Creates and initialises COB domain. */
int m0_cob_domain_create(struct m0_cob_domain          **dom,
			 struct m0_sm_group             *grp,
			 const struct m0_cob_domain_id  *cdid,
			 struct m0_be_domain            *bedom,
			 struct m0_be_seg               *seg);

/**
 * Finalises and destroys COB domain.
 * The dom's memory is released here and must not be accessed after this.
 */
int m0_cob_domain_destroy(struct m0_cob_domain *dom,
			  struct m0_sm_group   *grp,
			  struct m0_be_domain  *bedom);

M0_INTERNAL int m0_cob_domain_credit_add(struct m0_cob_domain          *dom,
					 struct m0_be_domain           *bedom,
					 struct m0_be_seg              *seg,
				         const struct m0_cob_domain_id *cdid,
				         struct m0_be_tx_credit        *cred);

M0_INTERNAL
int m0_cob_domain_create_prepared(struct m0_cob_domain          **dom,
				  struct m0_sm_group             *grp,
				  const struct m0_cob_domain_id  *cdid,
				  struct m0_be_domain            *bedom,
				  struct m0_be_seg               *seg,
				  struct m0_be_tx                *tx);

/**
 * Prepare storage before using. Create root cob for session objects
 * and root for files hierarchy.
 */
M0_INTERNAL int m0_cob_domain_mkfs(struct m0_cob_domain *dom,
				   const struct m0_fid *rootfid,
				   struct m0_be_tx *tx);

/**
 * Flags for cob attributes.
 */
enum m0_cob_valid_flags {
	M0_COB_ATIME   = 1 << 0,
	M0_COB_MTIME   = 1 << 1,
	M0_COB_CTIME   = 1 << 2,
	M0_COB_SIZE    = 1 << 3,
	M0_COB_MODE    = 1 << 4,
	M0_COB_UID     = 1 << 5,
	M0_COB_GID     = 1 << 6,
	M0_COB_BLOCKS  = 1 << 7,
	M0_COB_TYPE    = 1 << 8,
	M0_COB_FLAGS   = 1 << 9,
	M0_COB_NLINK   = 1 << 10,
	M0_COB_RDEV    = 1 << 11,
	M0_COB_BLKSIZE = 1 << 12,
	M0_COB_LID     = 1 << 13,
	M0_COB_PVER    = 1 << 14
};

#define M0_COB_ALL (M0_COB_ATIME | M0_COB_MTIME | M0_COB_CTIME |               \
		    M0_COB_SIZE | M0_COB_MODE | M0_COB_UID | M0_COB_GID |      \
		    M0_COB_BLOCKS | M0_COB_TYPE | M0_COB_FLAGS | M0_COB_NLINK |\
		    M0_COB_RDEV | M0_COB_BLKSIZE | M0_COB_LID | M0_COB_PVER)

/**
 * Attributes describing object that needs to be created or modified.
 * This structure is filled by mdservice and used in mdstore or
 * in cob modules for carrying in-request information to layers that
 * should not be dealing with fop request or response.
 */
struct m0_cob_attr {
	struct m0_fid ca_pfid;    /**< parent fid */
	struct m0_fid ca_tfid;    /**< object fid */
	struct m0_fid ca_pver;    /**< cob pool version */
	uint32_t      ca_valid;   /**< valid bits (enum m0_cob_valid_flags) */
	uint32_t      ca_mode;    /**< protection. */
	uint32_t      ca_uid;     /**< user ID of owner. */
	uint32_t      ca_gid;     /**< group ID of owner. */
	uint64_t      ca_atime;   /**< time of last access. */
	uint64_t      ca_mtime;   /**< time of last modification. */
	uint64_t      ca_ctime;   /**< time of last status change. */
	uint64_t      ca_rdev;    /**< devid for special devices */
	uint32_t      ca_nlink;   /**< number of hard links. */
	uint64_t      ca_size;    /**< total size, in bytes. */
	uint64_t      ca_blksize; /**< blocksize for filesystem I/O. */
	uint64_t      ca_blocks;  /**< number of blocks allocated. */
	uint64_t      ca_version; /**< object version */
	uint64_t      ca_lid;     /**< layout id */
	struct m0_buf ca_name;    /**< object name */
	struct m0_buf ca_link;    /**< symlink body */
	struct m0_buf ca_eakey;   /**< xattr name */
	struct m0_buf ca_eaval;   /**< xattr value */
};

/**
 * Namespace table key. For data objects, cnk_pfid = cfid and cnk_name = "".
 */
struct m0_cob_nskey {
	struct m0_fid       cnk_pfid;
	struct m0_bitstring cnk_name;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

M0_INTERNAL size_t m0_cob_nskey_size(const struct m0_cob_nskey *nskey);

/**
 * Compare two keys. Return: k0 > k1: +1, k0 < k1: -1, 0 - otherwise.
 */
M0_INTERNAL int m0_cob_nskey_cmp(const struct m0_cob_nskey *k0,
				 const struct m0_cob_nskey *k1);

/**
 * Namespace table record "value" part. For each file there may exist
 * several m0_cob_nsrecs, given that there are several hardlinks. First
 * of the records -- so called "zero record", aka stat data -- contains
 * file attributes.
 */
struct m0_cob_nsrec {
	struct m0_format_header cnr_header;
	struct m0_fid           cnr_fid;     /**< object fid */
	uint32_t                cnr_linkno;  /**< number of link for the name */

	/**
	 * The following fields are only important for 0-nsrec, that is,
	 * stat data. For other records, only two fields above are valid.
	 */
	uint32_t                cnr_nlink;   /**< number of hard links */
	uint32_t                cnr_cntr;    /**< linkno allocation counter */
	char                    cnr_pad[4];
	uint64_t                cnr_omgid;   /**< uid/gid/mode slot reference */
	uint64_t                cnr_size;    /**< total size, in bytes */
	uint64_t                cnr_blksize; /**< blocksize for filesystem I/O */
	uint64_t                cnr_blocks;  /**< number of blocks allocated */
	uint64_t                cnr_atime;   /**< time of last access */
	uint64_t                cnr_mtime;   /**< time of last modification */
	uint64_t                cnr_ctime;   /**< time of last status change */
	uint64_t                cnr_lid;     /**< layout id */
	struct m0_fid           cnr_pver;    /**< cob pool version */
	struct m0_format_footer cnr_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

enum m0_cob_nsrec_format_version {
	M0_COB_NSREC_FORMAT_VERSION_1 = 1,

	/* future versions, uncomment and update M0_COB_NSREC_FORMAT_VERSION */
	/*M0_COB_NSREC_FORMAT_VERSION_2,*/
	/*M0_COB_NSREC_FORMAT_VERSION_3,*/

	/** Current version, should point to the latest version present */
	M0_COB_NSREC_FORMAT_VERSION = M0_COB_NSREC_FORMAT_VERSION_1
};

M0_INTERNAL void m0_cob_nsrec_init(struct m0_cob_nsrec *nsrec);

/** Object index table key. The oi table record is a struct m0_cob_nskey. */
struct m0_cob_oikey {
	struct m0_fid     cok_fid;
	uint32_t          cok_linkno;  /**< hardlink ordinal index */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
 * Fileattr_basic table, key is m0_cob_fabkey
 *
 * @note version should change on every namespace manipulation and data write.
 * If version and mtime/ctime both change frequently, at the same time,
 * it is arguably better to put version info in the namespace table instead
 * of fileattr_basic table so that there is only 1 table write.
 *
 * The reasoning behind the current design is that namespace table should be
 * as compact as possible to reduce lookup footprint. Also, readdir benefits
 * from smaller namespace entries.
 */
struct m0_cob_fabkey {
	struct m0_fid     cfb_fid;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct m0_cob_fabrec {
	struct m0_fid     cfb_pver;     /**< pool version fid */
	uint64_t          cfb_version;  /**< version from last fop */
	uint64_t          cfb_layoutid; /**< reference to layout */
	uint32_t          cfb_linklen;  /**< symlink len if any */
	char              cfb_link[0];  /**< symlink body */
	/* add ACL, Besides ACL, no further metadata is needed for stat(2). */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
 * Omg (owner/mode/group) table key
 */
struct m0_cob_omgkey {
	uint64_t          cok_omgid;   /**< omg id ref */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
 * Protection and access flags are stored in omg table.
 */
struct m0_cob_omgrec {
	uint32_t          cor_uid;     /**< user ID of owner */
	uint32_t          cor_mode;    /**< protection */
	uint32_t          cor_gid;     /**< group ID of owner */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/** Extended attributes table key */
struct m0_cob_eakey {
	struct m0_fid       cek_fid;   /**< EA owner fid */
	struct m0_bitstring cek_name;  /**< EA name */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/** Extended attributes table value */
struct m0_cob_earec {
	uint32_t          cer_size;    /**< EA len */
	char              cer_body[0]; /**< EA body */
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

/**
 * In-memory representation of a component object.
 *
 * m0_cob is an in-memory structure, populated from database tables
 * on demand. m0_cob is not cached for long time (except for root)
 * and is only used as a temporary handle of database structures for the
 * sake of handiness. This means, we don't need to bother with locking
 * as every time we pass cob to some function this is new instance of it.
 *
 * The exposed methods to get a new instance of cob are:
 * - m0_cob_lookup() - allocate new in-memory cob and populate it with lookup
 *                     by name.
 * - m0_cob_locate() - allocate new in-memory cob and populate it with lookup
 *                     by fid.
 * - m0_cob_create() - allocate new in-memory cob, populate it with passed
 *                     records and create database structures.
 *
 * The cobs returned by these methods are always populated.
 *
 * Not populated cob is only exposed with m0_cob_alloc() method, which is used
 * by request handler and (possibly) others. Such a cob is used as an input
 * parameter to functions mentioned above.
 *
 * Users use m0_cob_get/put to hold/release references; a cob may be destroyed
 * on the last put, or it may be cached for some time (just like root cob is).
 * @todo at some point, we may replace the co_ref by taking a reference on the
 * underlying co_stob.  At that point, we will need a callback at last put.
 * We wait to see how cob users will use these references, whether they need
 * callbacks in turn, etc.
 *
 * A m0_cob serves several purposes:
 *
 * - it acts as a handle for the underlying metadata storage. Meta-data
 * operations can be executed on persistent storage by calling functions on
 * m0_cob;
 *
 * - it caches certain metadata attributes in memory (controlled by ->co_flags);
 *
 * <b>Liveness</b>
 * A m0_cob may be freed when the reference count drops to 0.
 *
 * @note m0_cob_nskey is allocated separately because it is variable
 * length. Once allocated, it can be released if M0_CA_NSKEY_FREE flag
 * is specified in its allocation time.
 *
 * <b>Caching and concurrency</b>
 * Cobs are not cached by cob domain, neither by cob API users.
 * Rationale is the following:
 *
 * - we use db[45] for storing metadata and it already has cache that may work
 * in a way that satisfies our needs;
 *
 * - using cache means substantial complications with locking and concurrent
 * access. Currently these issues are completely covered by db[45].
 */
struct m0_cob {
	struct m0_cob_domain  *co_dom;
	struct m0_stob        *co_stob;     /**< underlying storage object */
	struct m0_ref          co_ref;      /**< refcounter for caching cobs */
	uint64_t               co_flags;    /**< @see enum m0_cob_valid_flags */
	struct m0_file	       co_file;     /**< File containig fid which is
					         reference to the nsrec fid */
	struct m0_cob_nskey   *co_nskey;    /**< cob statdata nskey */
	struct m0_cob_oikey    co_oikey;    /**< object fid, linkno */
	struct m0_cob_nsrec    co_nsrec;    /**< object fid, basic stat data */
	struct m0_cob_fabrec  *co_fabrec;   /**< fileattr_basic data (acl...) */
	struct m0_cob_omgrec   co_omgrec;   /**< permission data */
};

/**
 * This is all standard readdir related stuff. This is one readdir entry.
 */
struct m0_dirent {
	uint32_t             d_namelen;
	uint32_t             d_reclen;
	char                 d_name[0];
};

/**
 * Readdir page.
 */
struct m0_rdpg {
	struct m0_bitstring *r_pos;
	struct m0_buf        r_buf;
	struct m0_bitstring *r_end;
};

/**
 * Cob iterator. Holds current position inside a cob (used by readdir).
 */
struct m0_cob_iterator {
	struct m0_cob            *ci_cob;      /**< the cob we iterate */
	struct m0_be_btree_cursor ci_cursor;   /**< cob iterator cursor */
	struct m0_cob_nskey      *ci_key;      /**< current iterator pos */
};

/**
 * Cob EA iterator. Holds current position inside EA table.
 */
struct m0_cob_ea_iterator {
	struct m0_cob            *ci_cob;      /**< the cob we iterate */
	struct m0_be_btree_cursor ci_cursor;   /**< cob iterator cursor */
	struct m0_cob_eakey      *ci_key;      /**< current iterator pos */
	struct m0_cob_earec      *ci_rec;      /**< current iterator rec */
};

/**
 * Cob flags and valid attributes.
 */
enum m0_cob_flags {
	M0_CA_NSKEY      = (1 << 0),  /**< nskey in cob is up-to-date */
	M0_CA_NSKEY_FREE = (1 << 1),  /**< cob will dealloc the nskey */
	M0_CA_NSREC      = (1 << 2),  /**< nsrec in cob is up-to-date */
	M0_CA_FABREC     = (1 << 3),  /**< fabrec in cob is up-to-date */
	M0_CA_OMGREC     = (1 << 4),  /**< omgrec in cob is up-to-date */
	M0_CA_LAYOUT     = (1 << 5),  /**< layout in cob is up-to-date */
};

/**
 * Lookup a filename in the namespace table.
 *
 * Allocate a new cob and populate it with the contents of the
 * namespace record; i.e. the stat data and fid. This function
 * also looks up fab and omg tables, depending on "need" flag.
 *
 * @param dom   cob domain to use
 * @param nskey name to lookup
 * @param flags flags specifying what parts of cob to populate
 * @param out   resulting cob is store here
 * @param tx    db transaction to be used
 *
 * @see m0_cob_locate
 */
M0_INTERNAL int m0_cob_lookup(struct m0_cob_domain *dom,
			      struct m0_cob_nskey *nskey,
			      uint64_t flags,
			      struct m0_cob **out);

/**
 * Locate cob by object index key.
 *
 * Create a new cob and populate it with the contents of the
 * a record; i.e. the filename. This also lookups for all attributes,
 * that is, fab, omg, etc., according to @need flags.
 *
 * @param dom   cob domain to use
 * @param oikey oikey (fid) to lookup
 * @param flags flags specifying what parts of cob to populate
 * @param out   resulting cob is store here
 * @param tx    db transaction to be used
 *
 * @see m0_cob_lookup
 */
M0_INTERNAL int m0_cob_locate(struct m0_cob_domain *dom,
			      struct m0_cob_oikey *oikey,
			      uint64_t flags,
			      struct m0_cob **out);

/**
 * Add a cob to the namespace.
 *
 * This doesn't create a new storage object; just creates
 * metadata table entries for it to enable namespace and oi lookup.
 *
 * @param cob    cob instance allocated with m0_cob_alloc()
 * @param nskey  namespace key made with m0_cob_nskey_make()
 * @param nsrec  namespace record with all attributes set
 * @param fabrec basic attributes record
 * @param omgrec owner/mode/group record
 * @param tx     transaction handle
 */
M0_INTERNAL int m0_cob_create(struct m0_cob *cob,
			      struct m0_cob_nskey *nskey,
			      struct m0_cob_nsrec *nsrec,
			      struct m0_cob_fabrec *fabrec,
			      struct m0_cob_omgrec *omgrec,
			      struct m0_be_tx *tx);

/**
 * Delete name with stat data, entry in object index and all file
 * attributes from fab, omg, etc., tables.
 *
 * @param cob this cob will be deleted
 * @param tx db transaction to use
 *
 * Note, that m0_cob_delete() does not decrement cob's reference counter.
 * Use m0_cob_delete_put() to have the counter decremented.
 *
 * @see m0_cob_delete_put()
 */
M0_INTERNAL int m0_cob_delete(struct m0_cob *cob, struct m0_be_tx *tx);

/**
 * Deletes and puts cob.
 *
 * @see m0_cob_delete(), m0_cob_put()
 */
M0_INTERNAL int m0_cob_delete_put(struct m0_cob *cob, struct m0_be_tx *tx);

/**
 * Update file attributes of passed cob with @nsrec, @fabrec
 * and @omgrec fields.
 *
 * @param cob    cob store updates to
 * @param nsrec  new nsrec to store to cob
 * @param fabrec fab record to store or null
 * @param omgrec omg record to store or null
 * @param tx     db transaction to be used
 */
M0_INTERNAL int m0_cob_update(struct m0_cob *cob,
			      struct m0_cob_nsrec *nsrec,
			      struct m0_cob_fabrec *fabrec,
			      struct m0_cob_omgrec *omgrec,
			      struct m0_be_tx *tx);

/**
 * Add name to namespace and object index.
 * @param cob   stat data (zero name) cob;
 * @param nskey new name to add to the file;
 * @param nsrec nsrec that will be added;
 * @param tx    transaction handle.
 */
M0_INTERNAL int m0_cob_name_add(struct m0_cob *cob,
				struct m0_cob_nskey *nskey,
				struct m0_cob_nsrec *nsrec,
				struct m0_be_tx *tx);

/**
 * Delete name from namespace and object index.
 *
 * @param cob   stat data (zero name) cob;
 * @param nskey name to kill (may be the name of statdata);
 * @param tx    transaction handle.
 */
M0_INTERNAL int m0_cob_name_del(struct m0_cob *cob,
				struct m0_cob_nskey *nskey,
				struct m0_be_tx *tx);

/**
 * Rename old record with new record.
 *
 * @param cob    stat data (zero name) cob;
 * @param srckey source name;
 * @param tgtkey target name;
 * @param tx     transaction handle
*/
M0_INTERNAL int m0_cob_name_update(struct m0_cob *cob,
				   struct m0_cob_nskey *srckey,
				   struct m0_cob_nskey *tgtkey,
				   struct m0_be_tx *tx);

/** Max possible size of earec. */
M0_INTERNAL size_t m0_cob_max_earec_size(void);

/**
   Create ea table key from passed file fid and ea name.
 */
M0_INTERNAL int m0_cob_eakey_make(struct m0_cob_eakey **keyh,
				  const struct m0_fid *fid,
				  const char *name,
				  size_t namelen);

/**
   Search for a record to the extended attributes table
 */
M0_INTERNAL int m0_cob_ea_get(struct m0_cob *cob,
                              struct m0_cob_eakey *eakey,
                              struct m0_cob_earec *out,
                              struct m0_be_tx *tx);

/**
   Add a record to the extended attributes table. If key already exists
   then kill the old record.
 */
M0_INTERNAL int m0_cob_ea_set(struct m0_cob *cob,
			      struct m0_cob_eakey *eakey,
			      struct m0_cob_earec *earec,
			      struct m0_be_tx *tx);

/**
   Del a record in the extended attributes table
 */
M0_INTERNAL int m0_cob_ea_del(struct m0_cob *cob,
			      struct m0_cob_eakey *eakey,
			      struct m0_be_tx *tx);

/**
 * Init ea iterator on passed @cob and @name as a start position.
 */
M0_INTERNAL int m0_cob_ea_iterator_init(struct m0_cob *cob,
				        struct m0_cob_ea_iterator *it,
				        struct m0_bitstring *name);

/**
 * Position to next name in a ea table.
 *
 * @retval 0        Success.
 * @retval -ENOENT  Next name is not found in ea table.
 * @retval -errno   Other error.
 */
M0_INTERNAL int m0_cob_ea_iterator_next(struct m0_cob_ea_iterator *it);

/**
 * Position in table according with @it properties.
 *
 * @retval 0        Success.
 * @retval -ENOENT  Specified position not found in table.
 * @retval -errno   Other error.
 */
M0_INTERNAL int m0_cob_ea_iterator_get(struct m0_cob_ea_iterator *it);

/**
 * Finish cob ea iterator.
 */
M0_INTERNAL void m0_cob_ea_iterator_fini(struct m0_cob_ea_iterator *it);

/**
 * Init cob iterator on passed @cob and @name as a start position.
 */
M0_INTERNAL int m0_cob_iterator_init(struct m0_cob *cob,
				     struct m0_cob_iterator *it,
				     struct m0_bitstring *name);

/**
 * Position to next name in a dir cob.
 *
 * @retval 0        Success.
 * @retval -ENOENT  Next name not found in directory.
 * @retval -errno   Other error.
 */
M0_INTERNAL int m0_cob_iterator_next(struct m0_cob_iterator *it);

/**
 * Position in table according with @it properties.
 *
 * @retval 0        Success.
 * @retval -ENOENT  Specified position not found in table.
 * @retval -errno   Other error.
 */
M0_INTERNAL int m0_cob_iterator_get(struct m0_cob_iterator *it);

/**
 * Finish cob iterator.
 */
M0_INTERNAL void m0_cob_iterator_fini(struct m0_cob_iterator *it);

/**
 * Allocate a new cob on passed @dom.
 */
M0_INTERNAL int m0_cob_alloc(struct m0_cob_domain *dom, struct m0_cob **out);

/**
 * Acquires an additional reference on the object.
 *
 * @see m0_cob_put()
 */
M0_INTERNAL void m0_cob_get(struct m0_cob *obj);

/**
 * Releases a reference on the object.
 *
 * When the last reference is released, the object can either return to the
 * cache or can be immediately destroyed.
 *
 * @see m0_cob_get(), m0_cob_delete_put()
 */
M0_INTERNAL void m0_cob_put(struct m0_cob *obj);

/**
 * Create object index key that is used for operations on object index table.
 * It consists of object fid an linkno depending on what record we want to
 * find.
 */
M0_INTERNAL void m0_cob_oikey_make(struct m0_cob_oikey *oikey,
				   const struct m0_fid *fid, int linkno);

/**
 * Create namespace table key for ns table manipulation. It contains parent fid
 * and child name.
 */
M0_INTERNAL int m0_cob_nskey_make(struct m0_cob_nskey **keyh,
				  const struct m0_fid *pfid,
				  const char *name, size_t namelen);

/**
 * Allocate fabrec record according with @link and @linklen and setup record
 * fields.
 */
M0_INTERNAL int m0_cob_fabrec_make(struct m0_cob_fabrec **rech,
				   const char *link, size_t linklen);

/**
 * Sets cob attributes to those present in attr.
 *
 * @param cob    cob for which attributes will be changed
 * @param attr   attributes to be loaded into a cob
 * @param tx     be transaction to be used
 */
M0_INTERNAL int m0_cob_setattr(struct m0_cob *cob,
			       struct m0_cob_attr *attr,
			       struct m0_be_tx *tx);

M0_INTERNAL int m0_cob_size_update(struct m0_cob *cob, uint64_t size,
				   struct m0_be_tx *tx);
/**
 * Try to allocate new omgid using omg table and terminator record. Save
 * allocated id in @omgid if not NULL.
 */
/* XXX move tx to the end of the declaration */
M0_INTERNAL int m0_cob_alloc_omgid(struct m0_cob_domain *dom,uint64_t * omgid);

enum m0_cob_op {
	M0_COB_OP_DOMAIN_MKFS,
	M0_COB_OP_LOOKUP,
	M0_COB_OP_LOCATE,
	M0_COB_OP_CREATE,
	M0_COB_OP_DELETE,
	M0_COB_OP_TRUNCATE,
	M0_COB_OP_DELETE_PUT,
	M0_COB_OP_UPDATE,
	M0_COB_OP_FEA_SET,
	M0_COB_OP_FEA_DEL,
	M0_COB_OP_NAME_ADD,
	M0_COB_OP_NAME_DEL,
	M0_COB_OP_NAME_UPDATE,
} M0_XCA_ENUM;

M0_INTERNAL void m0_cob_tx_credit(struct m0_cob_domain *dom,
				  enum m0_cob_op optype,
				  struct m0_be_tx_credit *accum);

/** XXX not yet implemented */
M0_INTERNAL void m0_cob_ea_get_credit(struct m0_cob *cob,
				      struct m0_cob_eakey *eakey,
				      struct m0_cob_earec *out,
				      struct m0_be_tx_credit *accum);
/** XXX not yet implemented */
M0_INTERNAL void m0_cob_ea_set_credit(struct m0_cob *cob,
				      struct m0_cob_eakey *eakey,
				      struct m0_cob_earec *earec,
				      struct m0_be_tx_credit *accum);
/** XXX not yet implemented */
M0_INTERNAL void m0_cob_ea_del_credit(struct m0_cob *cob,
				      struct m0_cob_eakey *eakey,
				      struct m0_be_tx_credit *accum);
/** XXX not yet implemented */
M0_INTERNAL void m0_cob_ea_iterator_init_credit(struct m0_cob *cob,
						struct m0_cob_ea_iterator *it,
						struct m0_bitstring *name,
						struct m0_be_tx_credit *accum);

M0_INTERNAL const struct m0_fid *m0_cob_fid(const struct m0_cob *cob);

/**
   Module initializer.
 */
M0_INTERNAL int m0_cob_mod_init(void);

/**
   Module finalizer.
 */
M0_INTERNAL void m0_cob_mod_fini(void);

extern const struct m0_fid_type m0_cob_fid_type;
enum m0_cob_type {
	M0_COB_IO,
	M0_COB_MD,
};

extern const struct m0_fid_type m0_cob_fid_type;

/** @} end group cob */
#endif /* __MERO_COB_COB_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
