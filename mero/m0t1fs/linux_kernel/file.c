/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Anand Vidwansa <anand_vidwansa@xyratex.com>
 * Original creation date: 07/28/2012
 */

#include <linux/version.h>  /* LINUX_VERSION_CODE */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,11,0)
#include <asm/uaccess.h>    /* VERIFY_READ, VERIFY_WRITE */
#endif
#include <asm/atomic.h>     /* atomic_get */
#include <linux/mm.h>       /* get_user_pages, get_page, put_page */
#include <linux/fs.h>       /* struct file_operations */
#include <linux/mount.h>    /* struct vfsmount (f_path.mnt) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <linux/uio.h>      /* struct iovec */
#include <linux/aio.h>      /* struct kiocb */
#endif

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_M0T1FS
#include "lib/trace.h"

#include "fop/fom_generic.h"/* m0_rpc_item_is_generic_reply_fop */
#include "lib/memory.h"     /* m0_alloc, m0_free */
#include "lib/misc.h"       /* m0_round_{up/down} */
#include "lib/bob.h"        /* m0_bob_type */
#include "lib/ext.h"        /* m0_ext */
#include "lib/arith.h"      /* min_type */
#include "lib/finject.h"    /* M0_FI_ENABLED */
#include "layout/pdclust.h" /* M0_PUT_*, m0_layout_to_pdl, */
#include "lib/bob.h"        /* m0_bob_type */
#include "lib/tlist.h"
#include "rpc/rpc_machine.h"      /* m0_rpc_machine, m0_rpc_machine_lock */
#include "ioservice/io_fops.h"    /* m0_io_fop */
#include "mero/magic.h"  /* M0_T1FS_IOREQ_MAGIC */
#include "m0t1fs/linux_kernel/m0t1fs.h" /* m0t1fs_sb */
#include "file/file.h"
#include "fd/fd.h"          /* m0_fd_fwd_map m0_fd_bwd_map */
#include "lib/hash.h"       /* m0_htable */
#include "sns/parity_repair.h"  /*m0_sns_repair_spare_map() */
#include "addb2/addb2.h"
#include "m0t1fs/linux_kernel/file_internal.h"
#include "m0t1fs/linux_kernel/fsync.h"
#include "m0t1fs/linux_kernel/ioctl.h"
#include "ioservice/fid_convert.h" /* m0_fid_cob_device_id */

/**
   @page iosnsrepair I/O with SNS and SNS repair.

   - @ref iosnsrepair-ovw
   - @ref iosnsrepair-def
   - @ref iosnsrepair-req
   - @ref iosnsrepair-depends
   - @ref iosnsrepair-highlights
   - @ref iosnsrepair-lspec
      - @ref iosnsrepair-lspec-comps
      - @ref iosnsrepair-lspec-state
      - @ref iosnsrepair-lspec-thread
      - @ref iosnsrepair-lspec-numa
   - @ref iosnsrepair-conformance
   - @ref iosnsrepair-ut
   - @ref iosnsrepair-st
   - @ref iosnsrepair-O
   - @ref iosnsrepair-ref
   - @ref iosnsrepair-impl-plan


   <hr>
   @section iosnsrepair-ovw Overview
   @note This DLD is written by Huang Hua (hua_huang@xyratex.com), 2012/10/10.

   This DLD describes how the m0t1fs does I/O with SNS in normal condition, in
   de-graded mode, and when SNS repair is completed.

   A file (also known as global object) in Mero is stored in multiple component
   objects, spreading on multiple servers. This is usually also called Server
   Network Striping, a.k.a SNS. Layout is used to describe the mapping of a
   file to its objects. A read request to some specific offset within a file
   will be directed to some parts of its component objects, according to its
   layout. A write request does the same. Some files don't store redundancy
   information in the file system, like RAID0. But in Mero, the default and
   typical mode is to have files with redundancy data stored somewhere. So the
   write requests may include updates to redundancy data.

   In case of node or device failure, lost data may be re-constructed from
   redundancy information. A read request to lost data needs to be satisfied
   by re-constructing data from its parity data. When SNS repair is completed
   for the failed node or device, a read or write request can be served by
   re-directing to its spare unit.

   Write requests to failed node or device should be handled in another way,
   cooperated with SNS repair and NBA (Non-Blocking Availability). Now it is
   out of the scope of this DLD.

   Each client has a cache of Failure Vectors of a pool. With failure vector
   information, clients know whether to re-construct data from other data units
   and parity units, or read from spare units (which contains repaired data).
   The detailed will be discussed in the following logical specification.

   <hr>
   @section iosnsrepair-def Definitions
   Previously defined terms:
   - <b>layout</b> A mapping from Mero file (global object) to component
         objects. See @ref layout for more details.
   - <b>SNS</b> Server Network Striping. See @ref SNS for more details.

   <hr>
   @section iosnsrepair-req Requirements
   - @b R.iosnsrepair.read Read request should be served in normal case, during
        SNS repair, and after SNS repair completes.
   - @b R.iosnsrepair.write Write request should be served in normal case, and
        after SNS repair completes.
   - @b R.iosnsrepair.code Code should be re-used and shared with other m0t1fs
        client features, especially the rmw feature.

   <hr>
   @section iosnsrepair-depends Dependencies
   The feature depends on the following features:
   - layout.
   - SNS and failure vector.

   The implementation of this feature may depend on the m0t1fs read-modify-write
   (rmw) feature, which is under development.

   <hr>
   @section iosnsrepair-highlights Design Highlights
   M0t1fs read-modify-write (rmw) feature has some similar concepts with this
   feature. The same code path will be used to serve both the features.

   <hr>
   @section iosnsrepair-lspec Logical Specification

   - @ref iosnsrepair-lspec-comps
   - @ref iosnsrepair-lspec-state
   - @ref iosnsrepair-lspec-thread
   - @ref iosnsrepair-lspec-numa

   @subsection iosnsrepair-lspec-comps Component Overview
   When an I/O request (read, write, or other) comes to client, m0t1fs first
   checks its cached failure vector to see the status of pool nodes and devices.
   A read or write request will span some node(s) or device(s). If these node(s)
   or device(s) are ONLINE, this is the normal case. If some node or device is
   FAILED or REPAIRING or REPAIRED, it will change the state of a pool. When
   all nodes and devices are in ONLINE status, the pool is ONLINE. I/O requests
   are handled normally. If less than or equal failures happen than the pool is
   configured to sustain, the pool is DEGRADED. I/O requests will be handled
   with the help of parity information or spare units. If more failures happen
   than the pool is configured to sustain , the pool is in DUD state, where all
   I/O requests will fail with -EIO error. The pool states define how client IO
   is made, specifically whether writes use NBA and whether read and writes use
   degraded mode. The pool states can be calculated from the failure vector.

   If the special action is taken to serve this
   request. The following table illustrate the actions:

                           Table 1   I/O request handling
   -----------------------------------------------------------------------------
   |      |  ONLINE    | read from the target device                           |
   |      |------------|-------------------------------------------------------|
   |      |  OFFLINE   | same as FAILED                                        |
   |      |------------|-------------------------------------------------------|
   | read |  FAILED    | read from other data unit(s) and parity unit(s) and   |
   |      |  REPAIRING | re-construct the datai. If NBA** exists, use new      |
   |      |            | layout to do reading if necessary.                    |
   |      |            | See more detail for this degraded read (1)            |
   |      |------------|-------------------------------------------------------|
   |      |  REPAIRED  | read from the repaired spare unit or use new layout   |
   |      |            | if NBA**                                              |
   |------|------------|-------------------------------------------------------|
   |      |  ONLINE    | write to the target device                            |
   |      |------------|-------------------------------------------------------|
   |      |  OFFLINE   | same as FAILED                                        |
   |      |------------|-------------------------------------------------------|
   |write |  FAILED    | NBA** determines to use new layout or old layout      |
   |      |            | if old layout is used, this is called degraded        |
   |      |            | write. See more detail in the following (2)           |
   |      |------------|-------------------------------------------------------|
   |      |  REPAIRING | Concurrent++ write I/O and sns repairing is out of    |
   |      |            | the scope of this DLD.  Not supported currently.      |
   |      |            | -ENOTSUP will be returned at this moment.             |
   |      |            | This is @TODO Concurrent r/w in SNS repair            |
   |      |------------|-------------------------------------------------------|
   |      |  REPAIRED  | write to the repaired spare unit or new layout        |
   |      |            | if NBA**                                              |
   ----------------------------------------------------------------------------|
   NBA** Non-Blocking Availability. When a device/node is not available for
   a write request, the system switches the file to use a new layout, and so the
   data is written to devices in new layout. By such means, the writing request
   will not be blocked waiting the device to be fixed, or SNS repaire to be
   completed. Device/node becomes un-available when it is OFFLINE or FAILED.
   Concurrent++ This should be designed in other module.

   A device never goes from repaired to online. When the re-balancing process
   that moves data from spare space to a new device completes, the *new* device
   goes from REBALANCING to ONLINE state. If the old device is ever "fixed"
   somehow, it becomes a new device in ONLINE state.

   A degraded read request is handled with the following steps:
   (1) Calculate its parity group, find out related data units and parity units.
       This needs help from the file's layout.
   (2) Send read requests to necessary data units and/or parity units
       asynchronously. The read request itself is blocked and waiting
       for those replies. For a N+K+K (N data units, K parity units, K
       spare units) layout, N units of data or parity units are needed
       to re-compute the lost data.
   (3) When those read replies come, ASTs will be called to re-compute the data
       iteratively. Temporary result is stored in the buffer of the original
       read request. This async read request and its reply can be released (no
       cache at this moment).
   (4) When all read replies come back, and the data is finally re-computed,
       the original read request has its data, and can be returned to user.

   A degraded write request is handled as the following:
   (1) Calculate its parity group, find out related data units and parity units.
       This needs help from the file's layout.
   (2) Prepare to async read data and/or parity units.
       (2.1) If this is a full-stripe write request, skip to step (4).
       (2.2) If write request only spans ONLINE devices, this is similar to a
             Read-Modify-Write (rmw), except a little difference: only async
             read the spanned data unit(s). Async read all spanned data units.
       (2.3) If write request spans FAILED/OFFLINE devices, async read all
             survival and un-spanned data units and necessary parity unit(s).
   (3) When these async read requests complete, replies come back to client.
       (3.1) for (2.2) case, prepare new parity units from the old data and
             new data.
       (3.2) for (2.3) case, first, re-calculate the lost data unit, and do
             the same as 3.1.
   (4) Send write request(s) to data units, along with all the new parity data,
       except the failed device(s). Please note here: write to the failed
       devices are excluded to send.
   (5) When those write requests complete, return to user space.

   The same thread used by the rmw will be used here to run the ASTs. The basic
   algorithm is similar in these two features. No new data structures are
   introduced by this feature.

   Pool's failure vector is cached on clients. Every I/O request to ioservices
   is tagged with client known failure vector version, and this version is
   checked against the lastest one by ioservices. If the client known version
   is stale, new version and failure vector updates will be returned back to
   clients and clients need to apply this update and do I/O request according
   to the latest version. Please see @ref pool and @ref poolmach for more
   details.

   Which spare space to use in the SNS repair is managed by failure vector.
   After SNS repair, client can query this information from failure vector and
   send r/w request to corresponding spare space.

   @subsection iosnsrepair-lspec-state State Specification
   N/A

   @subsection iosnsrepair-lspec-thread Threading and Concurrency Model
   See @ref rmw_io_dld for more information.

   @subsection iosnsrepair-lspec-numa NUMA optimizations
   See @ref rmw_io_dld for more information.

   @section iosnsrepair-conformance Conformance
   - @b I.iosnsrepair.read Read request handling is described in logic
        specification. Every node/device state are covered.
   - @b I.iosnsrepair.read Write request handling is described in logic
        specification. Every node/device state are covered.
   - @b I.iosnsrepair.code In logic specification, the design says the same
        code and algorithm will be used to handle io request in SNS repair
        and rmw.

   <hr>
   @section iosnsrepair-ut Unit Tests
   Unit tests for read and write requests in different devices state are
   needed. These states includes: ONLINE, OFFLINE, FAILED, REPAIRING,
   REPAIRED.

   <hr>
   @section iosnsrepair-st System Tests
   System tests are needed to verify m0t1fs can serve read/write properly
   when node/device is in various states, and changes its state from one
   to another. For example:
   - read/write requests in normal case.
   - read/write requests when a device changes from ONLINE to FAILED.
   - read/write requests when a device changes from FAILED to REPAIRING.
   - read/write requests when a device changes from REPAIRING to REPAIRED.

   <hr>
   @section iosnsrepair-O Analysis
   See @ref rmw for more information.

   <hr>
   @section iosnsrepair-ref References

   - <a href="https://docs.google.com/a/seagate.com/document/d/1r8jqkrLweRvEbbmP
XypoY8mKuEQJU9qS2xFbSbKHAGg/edit">HLD of SNS repair</a>,
   - @ref rmw_io_dld m0t1fs client read-modify-write DLD
   - @ref layout Layout
   - @ref pool Pool and @ref poolmach Pool Machine.

   <hr>
   @section iosnsrepair-impl-plan Implementation Plan
   The code implementation depends on the m0t1fs rmw which is under development.
   The rmw is in code inspection phase right now. When this DLD is approved,
   code maybe can start.

 */

struct io_mem_stats iommstats;

M0_INTERNAL void iov_iter_advance(struct iov_iter *i, size_t bytes);

/* Imports */
struct m0_net_domain;
M0_INTERNAL bool m0t1fs_inode_bob_check(struct m0t1fs_inode *bob);
M0_TL_DECLARE(rpcbulk, M0_INTERNAL, struct m0_rpc_bulk_buf);
M0_TL_DESCR_DECLARE(rpcbulk, M0_EXTERN);

M0_TL_DESCR_DEFINE(iofops, "List of IO fops", static,
		   struct io_req_fop, irf_link, irf_magic,
		   M0_T1FS_IOFOP_MAGIC, M0_T1FS_TIOREQ_MAGIC);

M0_TL_DEFINE(iofops,  static, struct io_req_fop);

static const struct m0_bob_type tioreq_bobtype;
static struct m0_bob_type iofop_bobtype;
static const struct m0_bob_type ioreq_bobtype;
static const struct m0_bob_type pgiomap_bobtype;
static const struct m0_bob_type nwxfer_bobtype;
static const struct m0_bob_type dtbuf_bobtype;

M0_BOB_DEFINE(static, &tioreq_bobtype,  target_ioreq);
M0_BOB_DEFINE(static, &iofop_bobtype,   io_req_fop);
M0_BOB_DEFINE(static, &pgiomap_bobtype, pargrp_iomap);
M0_BOB_DEFINE(static, &ioreq_bobtype,   io_request);
M0_BOB_DEFINE(static, &nwxfer_bobtype,  nw_xfer_request);
M0_BOB_DEFINE(static, &dtbuf_bobtype,   data_buf);

static const struct m0_bob_type ioreq_bobtype = {
	.bt_name         = "io_request_bobtype",
	.bt_magix_offset = offsetof(struct io_request, ir_magic),
	.bt_magix        = M0_T1FS_IOREQ_MAGIC,
	.bt_check        = NULL,
};

static const struct m0_bob_type pgiomap_bobtype = {
	.bt_name         = "pargrp_iomap_bobtype",
	.bt_magix_offset = offsetof(struct pargrp_iomap, pi_magic),
	.bt_magix        = M0_T1FS_PGROUP_MAGIC,
	.bt_check        = NULL,
};

static const struct m0_bob_type nwxfer_bobtype = {
	.bt_name         = "nw_xfer_request_bobtype",
	.bt_magix_offset = offsetof(struct nw_xfer_request, nxr_magic),
	.bt_magix        = M0_T1FS_NWREQ_MAGIC,
	.bt_check        = NULL,
};

static const struct m0_bob_type dtbuf_bobtype = {
	.bt_name         = "data_buf_bobtype",
	.bt_magix_offset = offsetof(struct data_buf, db_magic),
	.bt_magix        = M0_T1FS_DTBUF_MAGIC,
	.bt_check        = NULL,
};

static const struct m0_bob_type tioreq_bobtype = {
	.bt_name         = "target_ioreq",
	.bt_magix_offset = offsetof(struct target_ioreq, ti_magic),
	.bt_magix        = M0_T1FS_TIOREQ_MAGIC,
	.bt_check        = NULL,
};

/*
 * These are used as macros since they are used as lvalues which is
 * not possible by using static inline functions.
 */
#define INDEX(ivec, i) ((ivec)->iv_index[(i)])
#define COUNT(ivec, i) ((ivec)->iv_vec.v_count[(i)])
#define SEG_NR(ivec)   ((ivec)->iv_vec.v_nr)

#define V_INDEX(ivec, i) (*(m0_bindex_t*)(m0_varr_ele_get(&(ivec)->iv_index, (i))))
#define V_ADDR(bv,    i) (*(void**)      (m0_varr_ele_get(&(bv  )->iv_index, (i))))
#define V_COUNT(ivec, i) (*(m0_bcount_t*)(m0_varr_ele_get(&(ivec)->iv_count, (i))))
#define V_SEG_NR(ivec)   ((ivec)->iv_nr)

#define PA(pa, i)        (*(enum page_attr*)(m0_varr_ele_get((pa), (i))))

#define indexvec_dump(ivec)                                                    \
do {                                                                           \
	int seg;                                                               \
	for (seg = 0; seg < SEG_NR((ivec)); ++seg) {                           \
		M0_LOG(M0_DEBUG, "seg# %d: [pos, +len) = [%llu, +%llu)",       \
				 seg, INDEX((ivec), seg), COUNT((ivec), seg)); \
	}                                                                      \
} while (0)

#define indexvec_varr_dump(ivec)                                               \
do {                                                                           \
	int seg;                                                               \
	for (seg = 0; seg < V_SEG_NR((ivec)); ++seg) {                         \
		M0_LOG(M0_DEBUG, "seg# %d: [pos, +len) = [%llu, +%llu)",       \
		       seg, V_INDEX((ivec), seg), V_COUNT((ivec), seg));       \
	}                                                                      \
} while (0)

static inline m0_bcount_t seg_endpos(const struct m0_indexvec *ivec, uint32_t i)
{
	M0_PRE(ivec != NULL);

	return INDEX(ivec, i) + COUNT(ivec, i);
}

static inline m0_bcount_t
v_seg_endpos(struct m0_indexvec_varr *ivec, uint32_t i)
{
	M0_PRE(ivec != NULL);

	return V_INDEX(ivec, i) + V_COUNT(ivec, i);
}

M0_INTERNAL struct inode *m0t1fs_file_to_inode(const struct file *file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
	return file->f_path.dentry->d_inode;
#else
	return file->f_dentry->d_inode;
#endif
}

M0_INTERNAL struct m0t1fs_inode *m0t1fs_file_to_m0inode(const struct file *file)
{
	return M0T1FS_I(m0t1fs_file_to_inode(file));
}

M0_INTERNAL struct m0_pool_version *m0t1fs_file_to_pver(const struct file *file)
{
	struct m0t1fs_inode *inode = M0T1FS_I(m0t1fs_file_to_inode(file));
	struct m0t1fs_sb    *csb = M0T1FS_SB(m0t1fs_file_to_inode(file)->i_sb);

	return m0_pool_version_find(&csb->csb_pools_common, &inode->ci_pver);
}

M0_INTERNAL struct m0_poolmach *m0t1fs_file_to_poolmach(const struct file *file)
{
	return &m0t1fs_file_to_pver(file)->pv_mach;
}

M0_INTERNAL struct m0t1fs_inode *m0t1fs_inode_to_m0inode(const struct inode *inode)
{
	return M0T1FS_I(inode);
}

static inline struct inode *iomap_to_inode(const struct pargrp_iomap *map)
{
	return m0t1fs_file_to_inode(map->pi_ioreq->ir_file);
}

M0_INTERNAL struct m0t1fs_sb *m0inode_to_sb(const struct m0t1fs_inode *m0inode)
{
	return M0T1FS_SB(m0inode->ci_inode.i_sb);
}

static inline const struct m0_fid *file_to_fid(const struct file *file)
{
	return m0t1fs_inode_fid(m0t1fs_file_to_m0inode(file));
}

static inline struct m0t1fs_sb *file_to_sb(const struct file *file)
{
	return M0T1FS_SB(m0t1fs_file_to_inode(file)->i_sb);
}

static inline struct m0_sm_group *file_to_smgroup(const struct file *file)
{
	return &file_to_sb(file)->csb_iogroup;
}

static inline uint64_t page_nr(m0_bcount_t size)
{
	return size >> PAGE_SHIFT;
}

static struct m0_layout_instance *
layout_instance(const struct io_request *req)
{
	return m0t1fs_file_to_m0inode(req->ir_file)->ci_layout_instance;
}

static inline struct m0_pdclust_instance *
pdlayout_instance(const struct m0_layout_instance *li)
{
	return m0_layout_instance_to_pdi(li);
}

static inline struct m0_pdclust_layout *
pdlayout_get(const struct io_request *req)
{
	return m0_layout_to_pdl(layout_instance(req)->li_l);
}

static inline uint32_t layout_n(const struct m0_pdclust_layout *play)
{
	return play->pl_attr.pa_N;
}

static inline uint32_t layout_k(const struct m0_pdclust_layout *play)
{
	return play->pl_attr.pa_K;
}

static inline uint64_t layout_unit_size(const struct m0_pdclust_layout *play)
{
	return play->pl_attr.pa_unit_size;
}

static inline uint64_t parity_units_page_nr(const struct m0_pdclust_layout *play)
{
	return page_nr(layout_unit_size(play)) * layout_k(play);
}

static inline uint64_t indexvec_varr_count(struct m0_indexvec_varr *varr)
{
	uint64_t sum = 0;

	m0_varr_for(&varr->iv_count, uint64_t *, i, countp) {
		sum += *(uint64_t*)countp;
	} m0_varr_endfor;
	return sum;
}

static inline uint64_t iomap_page_nr(struct pargrp_iomap *map)
{
	return page_nr(indexvec_varr_count(&map->pi_ivv));
}

static inline uint64_t data_size(const struct m0_pdclust_layout *play)
{
	return layout_n(play) * layout_unit_size(play);
}

static inline struct m0_parity_math *parity_math(struct io_request *req)
{
	return &pdlayout_instance(layout_instance(req))->pi_math;
}

static inline uint64_t group_id(m0_bindex_t index, m0_bcount_t dtsize)
{
	return index / dtsize;
}

static inline bool is_page_read(struct data_buf *dbuf)
{
	return dbuf->db_flags & PA_READ &&
		dbuf->db_tioreq != NULL && dbuf->db_tioreq->ti_rc == 0;
}

static inline uint64_t target_offset(uint64_t                  frame,
				     struct m0_pdclust_layout *play,
				     m0_bindex_t               gob_offset)
{
	return frame * layout_unit_size(play) +
	       (gob_offset % layout_unit_size(play));
}

static inline uint32_t target_ioreq_type_get(struct target_ioreq *ti)
{
	return ti->ti_req_type;
}

static inline void target_ioreq_type_set(struct target_ioreq *ti,
					 enum target_ioreq_type type)
{
	ti->ti_req_type = type;
}

static bool is_pver_dud(uint32_t fdev_nr, uint32_t dev_k, uint32_t fsvc_nr,
			uint32_t svc_k);

static uint64_t tioreqs_hash_func(const struct m0_htable *htable, const void *k)
{
	const uint64_t *key = (uint64_t *)k;

	return *key % htable->h_bucket_nr;
}

static bool tioreq_key_eq(const void *key1, const void *key2)
{
	const uint64_t *k1 = (uint64_t *)key1;
	const uint64_t *k2 = (uint64_t *)key2;

	return *k1 == *k2;
}

M0_HT_DESCR_DEFINE(tioreqht, "Hash of target_ioreq objects", static,
		   struct target_ioreq, ti_link, ti_magic,
		   M0_T1FS_TIOREQ_MAGIC, M0_T1FS_TLIST_HEAD_MAGIC,
		   ti_fid.f_container, tioreqs_hash_func, tioreq_key_eq);

M0_HT_DEFINE(tioreqht, static, struct target_ioreq, uint64_t);

/* Finds the parity group associated with a given target offset.
 * index   - target offset for intended IO.
 * req     - IO-request holding information about IO.
 * tio_req - io-request for given target.
 * src     - output parity group.
 */
static void pargrp_src_addr(m0_bindex_t                 index,
			    const struct io_request    *req,
			    const struct target_ioreq  *tio_req,
			    struct m0_pdclust_src_addr *src)
{
	struct m0_pdclust_tgt_addr tgt;
	struct m0_pdclust_layout  *play;

	M0_PRE(req != NULL);
	M0_PRE(src != NULL);

	play = pdlayout_get(req);
	tgt.ta_obj = tio_req->ti_obj;
	tgt.ta_frame = index / layout_unit_size(play);
	m0_fd_bwd_map(pdlayout_instance(layout_instance(req)), &tgt, src);
}

static inline uint64_t pargrp_id_find(m0_bindex_t              index,
				      const struct io_request *req,
				      const struct io_req_fop *ir_fop)
{
	struct m0_pdclust_src_addr src;

	pargrp_src_addr(index, req, ir_fop->irf_tioreq, &src);
	return src.sa_group;
}

static inline m0_bindex_t gfile_offset(m0_bindex_t                       toff,
				       const struct pargrp_iomap        *map,
				       const struct m0_pdclust_layout   *play,
				       const struct m0_pdclust_src_addr *src)
{
	m0_bindex_t goff;

	M0_PRE(map  != NULL);
	M0_PRE(play != NULL);

	M0_ENTRY("grpid = %llu, target_off = %llu", map->pi_grpid, toff);

	goff = map->pi_grpid * data_size(play) +
	       src->sa_unit * layout_unit_size(play) +
	       toff % layout_unit_size(play);
	M0_LEAVE("global file offset = %llu", goff);

	return goff;
}

static inline struct m0_fid target_fid(const struct io_request    *req,
				       struct m0_pdclust_tgt_addr *tgt)
{
	struct m0_fid fid;

	m0_poolmach_gob2cob(&m0t1fs_file_to_pver(req->ir_file)->pv_mach,
			    file_to_fid(req->ir_file), tgt->ta_obj,
			    &fid);
	return fid;
}

static inline struct m0_rpc_session *target_session(struct io_request *req,
						    struct m0_fid      tfid)
{
	return m0t1fs_container_id_to_session(m0t1fs_file_to_pver(req->ir_file),
					      m0_fid_cob_device_id(&tfid));
}

static inline uint64_t page_id(m0_bindex_t offset)
{
	return offset >> PAGE_SHIFT;
}

static inline uint32_t data_row_nr(struct m0_pdclust_layout *play)
{
	return page_nr(layout_unit_size(play));
}

static inline uint32_t data_col_nr(struct m0_pdclust_layout *play)
{
	return layout_n(play);
}

static inline uint32_t parity_col_nr(struct m0_pdclust_layout *play)
{
	return layout_k(play);
}

static inline uint32_t parity_row_nr(struct m0_pdclust_layout *play)
{
	return data_row_nr(play);
}

#if !defined(round_down)
static inline uint64_t round_down(uint64_t val, uint64_t size)
{
	M0_PRE(m0_is_po2(size));

	/*
	 * Returns current value if it is already a multiple of size,
	 * else m0_round_down() is invoked.
	 */
	return (val & (size - 1)) == 0 ?
	       val : m0_round_down(val, size);
}
#endif

#if !defined(round_up)
static inline uint64_t round_up(uint64_t val, uint64_t size)
{
	M0_PRE(m0_is_po2(size));

	/*
	 * Returns current value if it is already a multiple of size,
	 * else m0_round_up() is invoked.
	 */
	return (val & (size - 1)) == 0 ?
	       val : m0_round_up(val, size);
}
#endif

/* Returns the position of page in matrix of data buffers. */
static void page_pos_get(struct pargrp_iomap *map,
			 m0_bindex_t          index,
			 uint32_t            *row,
			 uint32_t            *col)
{
	uint64_t                  pg_id;
	struct m0_pdclust_layout *play;

	M0_PRE(map != NULL);
	M0_PRE(row != NULL);
	M0_PRE(col != NULL);

	play = pdlayout_get(map->pi_ioreq);

	pg_id = page_id(index - data_size(play) * map->pi_grpid);
	*row  = pg_id % data_row_nr(play);
	*col  = pg_id / data_row_nr(play);
}

static void parity_page_pos_get(struct pargrp_iomap *map,
				m0_bindex_t          index,
				uint32_t            *row,
				uint32_t            *col)
{
	uint64_t                  pg_id;
	struct m0_pdclust_layout *play;

	M0_PRE(map != NULL);
	M0_PRE(row != NULL);
	M0_PRE(col != NULL);

	play = pdlayout_get(map->pi_ioreq);

	pg_id = page_id(index);
	*row  = pg_id % parity_row_nr(play);
	*col  = pg_id / parity_row_nr(play);
}

/*
 * Returns the starting offset of page given its position in data matrix.
 * Acts as opposite of page_pos_get() API.
 */
static m0_bindex_t data_page_offset_get(struct pargrp_iomap *map,
					uint32_t             row,
					uint32_t             col)
{
	struct m0_pdclust_layout *play;
	m0_bindex_t               out;

	M0_PRE(map != NULL);
	M0_ENTRY("gid = %llu, row = %u, col = %u", map->pi_grpid, row, col);

	play = pdlayout_get(map->pi_ioreq);

	M0_ASSERT(row < data_row_nr(play));
	M0_ASSERT(col < data_col_nr(play));

	out = data_size(play) * map->pi_grpid +
	       col * layout_unit_size(play) + row * PAGE_SIZE;

	M0_LEAVE("offsef = %llu", out);
	return out;
}

/* Invoked during m0t1fs mount. */
M0_INTERNAL void io_bob_tlists_init(void)
{
	M0_ASSERT(tioreq_bobtype.bt_magix == M0_T1FS_TIOREQ_MAGIC);
	m0_bob_type_tlist_init(&iofop_bobtype, &iofops_tl);
	M0_ASSERT(iofop_bobtype.bt_magix == M0_T1FS_IOFOP_MAGIC);
}

static void device_state_reset(struct nw_xfer_request *xfer, bool rmw);

static void io_rpc_item_cb (struct m0_rpc_item *item);
static void io_req_fop_release(struct m0_ref *ref);
static void cc_rpc_item_cb(struct m0_rpc_item *item);
static void cc_fop_release(struct m0_ref *ref);

/*
 * io_rpc_item_cb can not be directly invoked from io fops code since it
 * leads to build dependency of ioservice code over kernel-only code (m0t1fs).
 * Hence, a new m0_rpc_item_ops structure is used for fops dispatched
 * by m0t1fs io requests.
 */
static const struct m0_rpc_item_ops io_item_ops = {
	.rio_replied = io_rpc_item_cb,
};

static const struct m0_rpc_item_ops cc_item_ops = {
	.rio_replied = cc_rpc_item_cb,
};

static bool nw_xfer_request_invariant(const struct nw_xfer_request *xfer);

static int  nw_xfer_io_distribute(struct nw_xfer_request *xfer);
static void nw_xfer_req_complete (struct nw_xfer_request *xfer,
				  bool                    rmw);
static int  nw_xfer_req_dispatch (struct nw_xfer_request *xfer);

static int  nw_xfer_tioreq_map   (struct nw_xfer_request           *xfer,
				  const struct m0_pdclust_src_addr *src,
				  struct m0_pdclust_tgt_addr       *tgt,
				  struct target_ioreq             **out);

static int  nw_xfer_tioreq_get   (struct nw_xfer_request *xfer,
				  const struct m0_fid     *fid,
				  uint64_t                ta_obj,
				  struct m0_rpc_session  *session,
				  uint64_t                size,
				  struct target_ioreq   **out);

static const struct nw_xfer_ops xfer_ops = {
	.nxo_distribute  = nw_xfer_io_distribute,
	.nxo_complete    = nw_xfer_req_complete,
	.nxo_dispatch    = nw_xfer_req_dispatch,
	.nxo_tioreq_map  = nw_xfer_tioreq_map,
};

static int  pargrp_iomap_populate     (struct pargrp_iomap        *map,
				       struct m0_indexvec_varr    *ivec,
				       struct m0_ivec_varr_cursor *cursor);

static bool pargrp_iomap_spans_seg    (struct pargrp_iomap *map,
				       m0_bindex_t          index,
				       m0_bcount_t          count);

static int  pargrp_iomap_readrest     (struct pargrp_iomap *map);


static int  pargrp_iomap_seg_process  (struct pargrp_iomap *map,
				       uint64_t             seg,
				       bool                 rmw);

static int  pargrp_iomap_parity_recalc(struct pargrp_iomap *map);
static int  pargrp_iomap_parity_verify(struct pargrp_iomap *map);

static uint64_t pargrp_iomap_fullpages_count(struct pargrp_iomap *map);

static int pargrp_iomap_readold_auxbuf_alloc(struct pargrp_iomap *map);

static int pargrp_iomap_paritybufs_alloc(struct pargrp_iomap *map);

static int pargrp_iomap_dgmode_process (struct pargrp_iomap *map,
					struct target_ioreq *tio,
					m0_bindex_t         *index,
					uint32_t             count);

static int pargrp_iomap_dgmode_postprocess(struct pargrp_iomap *map);

static int pargrp_iomap_dgmode_recover  (struct pargrp_iomap *map);

static const struct pargrp_iomap_ops iomap_ops = {
	.pi_populate             = pargrp_iomap_populate,
	.pi_spans_seg            = pargrp_iomap_spans_seg,
	.pi_readrest             = pargrp_iomap_readrest,
	.pi_fullpages_find       = pargrp_iomap_fullpages_count,
	.pi_seg_process          = pargrp_iomap_seg_process,
	.pi_readold_auxbuf_alloc = pargrp_iomap_readold_auxbuf_alloc,
	.pi_parity_recalc        = pargrp_iomap_parity_recalc,
	.pi_parity_verify        = pargrp_iomap_parity_verify,
	.pi_paritybufs_alloc     = pargrp_iomap_paritybufs_alloc,
	.pi_dgmode_process       = pargrp_iomap_dgmode_process,
	.pi_dgmode_postprocess   = pargrp_iomap_dgmode_postprocess,
	.pi_dgmode_recover       = pargrp_iomap_dgmode_recover,
};

static bool pargrp_iomap_invariant_nr (struct io_request *req);
static bool target_ioreq_invariant    (struct target_ioreq *ti);

static void target_ioreq_fini         (struct target_ioreq *ti);

static int target_ioreq_iofops_prepare(struct target_ioreq *ti,
				      enum page_attr       filter);

static void target_ioreq_seg_add(struct target_ioreq              *ti,
				 const struct m0_pdclust_src_addr *src,
				 const struct m0_pdclust_tgt_addr *tgt,
				 m0_bindex_t                       gob_offset,
				 m0_bcount_t                       count,
				 struct pargrp_iomap              *map);

static int target_cob_create_fop_prepare(struct target_ioreq *ti);
static const struct target_ioreq_ops tioreq_ops = {
	.tio_seg_add         = target_ioreq_seg_add,
	.tio_iofops_prepare  = target_ioreq_iofops_prepare,
	.tio_cc_fops_prepare = target_cob_create_fop_prepare,
};

static int io_req_fop_dgmode_read(struct io_req_fop *irfop);

static struct data_buf *data_buf_alloc_init(enum page_attr pattr);

static void data_buf_dealloc_fini(struct data_buf *buf);

static void io_bottom_half(struct m0_sm_group *grp, struct m0_sm_ast *ast);

static void cc_bottom_half(struct m0_sm_group *grp, struct m0_sm_ast *ast);

static int  ioreq_iomaps_prepare(struct io_request *req);

static void ioreq_iomaps_destroy(struct io_request *req);

static int ioreq_user_data_copy (struct io_request   *req,
				 enum copy_direction  dir,
				 enum page_attr       filter);

static int ioreq_parity_recalc  (struct io_request *req);
static int ioreq_parity_verify  (struct io_request *req);

static int ioreq_iosm_handle    (struct io_request *req);

static int  ioreq_file_lock     (struct io_request *req);
static void ioreq_file_unlock   (struct io_request *req);
static int  ioreq_no_lock       (struct io_request *req);
static void ioreq_no_unlock     (struct io_request *req);

static int ioreq_dgmode_read    (struct io_request *req, bool rmw);
static int ioreq_dgmode_write   (struct io_request *req, bool rmw);
static int ioreq_dgmode_recover (struct io_request *req);

static bool should_req_sm_complete(struct io_request *req);

static const struct io_request_ops ioreq_ops = {
	.iro_iomaps_prepare = ioreq_iomaps_prepare,
	.iro_iomaps_destroy = ioreq_iomaps_destroy,
	.iro_user_data_copy = ioreq_user_data_copy,
	.iro_parity_recalc  = ioreq_parity_recalc,
	.iro_parity_verify  = ioreq_parity_verify,
	.iro_iosm_handle    = ioreq_iosm_handle,
	.iro_file_lock      = ioreq_file_lock,
	.iro_file_unlock    = ioreq_file_unlock,
	.iro_dgmode_read    = ioreq_dgmode_read,
	.iro_dgmode_write   = ioreq_dgmode_write,
	.iro_dgmode_recover = ioreq_dgmode_recover,
};

static const struct io_request_ops ioreq_oostore_ops = {
	.iro_iomaps_prepare = ioreq_iomaps_prepare,
	.iro_iomaps_destroy = ioreq_iomaps_destroy,
	.iro_user_data_copy = ioreq_user_data_copy,
	.iro_parity_recalc  = ioreq_parity_recalc,
	.iro_parity_verify  = ioreq_parity_verify,
	.iro_iosm_handle    = ioreq_iosm_handle,
	.iro_file_lock      = ioreq_no_lock,
	.iro_file_unlock    = ioreq_no_unlock,
	.iro_dgmode_read    = ioreq_dgmode_read,
	.iro_dgmode_write   = ioreq_dgmode_write,
	.iro_dgmode_recover = ioreq_dgmode_recover,
};

static inline uint32_t ioreq_sm_state(const struct io_request *req)
{
	return req->ir_sm.sm_state;
}

static struct m0_sm_state_descr io_states[] = {
	[IRS_INITIALIZED]       = {
		.sd_flags       = M0_SDF_INITIAL,
		.sd_name        = "IO_initial",
		.sd_allowed     = M0_BITS(IRS_READING, IRS_WRITING,
					  IRS_FAILED, IRS_REQ_COMPLETE)
	},
	[IRS_READING]           = {
		.sd_name        = "IO_reading",
		.sd_allowed     = M0_BITS(IRS_READ_COMPLETE, IRS_FAILED)
	},
	[IRS_READ_COMPLETE]     = {
		.sd_name        = "IO_read_complete",
		.sd_allowed     = M0_BITS(IRS_WRITING, IRS_REQ_COMPLETE,
					  IRS_DEGRADED_READING, IRS_FAILED,
					  IRS_READING)
	},
	[IRS_DEGRADED_READING]  = {
		.sd_name        = "IO_degraded_read",
		.sd_allowed     = M0_BITS(IRS_READ_COMPLETE, IRS_FAILED)
	},
	[IRS_DEGRADED_WRITING]  = {
		.sd_name        = "IO_degraded_write",
		.sd_allowed     = M0_BITS(IRS_WRITE_COMPLETE, IRS_FAILED)
	},
	[IRS_WRITING]           = {
		.sd_name        = "IO_writing",
		.sd_allowed     = M0_BITS(IRS_WRITE_COMPLETE, IRS_FAILED)
	},
	[IRS_WRITE_COMPLETE]    = {
		.sd_name        = "IO_write_complete",
		.sd_allowed     = M0_BITS(IRS_REQ_COMPLETE, IRS_FAILED,
					  IRS_DEGRADED_WRITING)
	},
	[IRS_FAILED]            = {
		.sd_flags       = M0_SDF_FAILURE,
		.sd_name        = "IO_req_failed",
		.sd_allowed     = M0_BITS(IRS_REQ_COMPLETE)
	},
	[IRS_REQ_COMPLETE]      = {
		.sd_flags       = M0_SDF_TERMINAL,
		.sd_name        = "IO_req_complete",
	},
};

static const struct m0_sm_conf io_sm_conf = {
	.scf_name      = "IO request state machine configuration",
	.scf_nr_states = ARRAY_SIZE(io_states),
	.scf_state     = io_states,
};

static void ioreq_sm_failed(struct io_request *req, int rc)
{
	M0_LOG(M0_DEBUG, "[%p] rc %d", req, rc);
	m0_sm_group_lock(req->ir_sm.sm_grp);
	m0_sm_fail(&req->ir_sm, IRS_FAILED, rc);
	m0_sm_group_unlock(req->ir_sm.sm_grp);
}

static void ioreq_sm_state_set(struct io_request *req, int state)
{
	M0_LOG(M0_INFO, "[%p] change state %s -> %s",
			req, io_states[ioreq_sm_state(req)].sd_name,
			io_states[state].sd_name);
	m0_sm_group_lock(req->ir_sm.sm_grp);
	m0_sm_state_set(&req->ir_sm, state);
	m0_sm_group_unlock(req->ir_sm.sm_grp);
}

static void ioreq_sm_state_set_nolock(struct io_request *req, int state)
{
	M0_LOG(M0_INFO, "[%p] change state %s -> %s",
			req, io_states[ioreq_sm_state(req)].sd_name,
			io_states[state].sd_name);
	m0_sm_state_set(&req->ir_sm, state);
}

static bool io_request_invariant(struct io_request *req)
{
	return
	       _0C(io_request_bob_check(req)) &&
	       _0C(req->ir_type   <= IRT_TYPE_NR) &&
	       _0C(req->ir_iovec  != NULL) &&
	       _0C(req->ir_ops    != NULL) &&
	       _0C(m0_fid_is_valid(file_to_fid(req->ir_file))) &&

	       _0C(ergo(ioreq_sm_state(req) == IRS_READING,
		    !tioreqht_htable_is_empty(&req->ir_nwxfer.
			    nxr_tioreqs_hash))) &&

	       _0C(ergo(ioreq_sm_state(req) == IRS_WRITING,
		    !tioreqht_htable_is_empty(&req->ir_nwxfer.
			    nxr_tioreqs_hash))) &&

	       _0C(ergo(ioreq_sm_state(req) == IRS_WRITE_COMPLETE ||
		    ioreq_sm_state(req) == IRS_READ_COMPLETE,
		    m0_atomic64_get(&req->ir_nwxfer.nxr_iofop_nr) == 0 &&
		    m0_atomic64_get(&req->ir_nwxfer.nxr_rdbulk_nr) == 0)) &&

	       _0C(indexvec_varr_count(&req->ir_ivv) > 0) &&

	       m0_forall(i, V_SEG_NR(&req->ir_ivv) - 1,
			 _0C(v_seg_endpos(&req->ir_ivv, i) <=
			     V_INDEX(&req->ir_ivv, i+1))) &&

	       _0C(pargrp_iomap_invariant_nr(req)) &&

	       _0C(nw_xfer_request_invariant(&req->ir_nwxfer));
}

static bool nw_xfer_request_invariant(const struct nw_xfer_request *xfer)
{
	return
	       _0C(nw_xfer_request_bob_check(xfer)) &&
	       _0C(xfer->nxr_state <= NXS_STATE_NR) &&

	       _0C(ergo(xfer->nxr_state == NXS_INITIALIZED,
		   xfer->nxr_rc == 0 &&
		   xfer->nxr_bytes == 0 &&
		    (m0_atomic64_get(&xfer->nxr_iofop_nr) == 0))) &&

	       _0C(ergo(xfer->nxr_state == NXS_INFLIGHT,
		    !tioreqht_htable_is_empty(&xfer->nxr_tioreqs_hash))) &&

	       _0C(ergo(xfer->nxr_state == NXS_COMPLETE,
		    m0_atomic64_get(&xfer->nxr_iofop_nr) == 0 &&
		    m0_atomic64_get(&xfer->nxr_rdbulk_nr) == 0)) &&

	       m0_htable_forall(tioreqht, tioreq, &xfer->nxr_tioreqs_hash,
			       target_ioreq_invariant(tioreq));
}

static bool data_buf_invariant(const struct data_buf *db)
{
	return
	       db != NULL &&
	       data_buf_bob_check(db) &&
	       ergo(db->db_buf.b_addr != NULL, db->db_buf.b_nob > 0);
}

static bool data_buf_invariant_nr(const struct pargrp_iomap *map)
{
	uint32_t                  row;
	uint32_t                  col;
	struct m0_pdclust_layout *play;

	play = pdlayout_get(map->pi_ioreq);
	for (row = 0; row < data_row_nr(play); ++row) {
		for (col = 0; col < data_col_nr(play); ++col) {
			if (map->pi_databufs[row][col] != NULL &&
			    !data_buf_invariant(map->pi_databufs[row][col]))
				return false;
		}
	}

	if (map->pi_paritybufs != NULL) {
		for (row = 0; row < parity_row_nr(play); ++row) {
			for (col = 0; col < parity_col_nr(play); ++col) {
				if (map->pi_paritybufs[row][col] != NULL &&
				    !data_buf_invariant(map->pi_paritybufs
				    [row][col]))
					return false;
			}
		}
	}
	return true;
}

static void data_buf_init(struct data_buf *buf, void *addr, uint64_t flags)
{
	M0_PRE(buf  != NULL);
	M0_PRE(addr != NULL);

	data_buf_bob_init(buf);
	buf->db_flags = flags;
	m0_buf_init(&buf->db_buf, addr, PAGE_SIZE);
	buf->db_tioreq = NULL;
}

static void data_buf_fini(struct data_buf *buf)
{
	M0_PRE(buf != NULL);

	data_buf_bob_fini(buf);
	buf->db_flags = PA_NONE;
}

static bool io_req_fop_invariant(const struct io_req_fop *fop)
{
	return
		_0C(io_req_fop_bob_check(fop)) &&
		_0C(fop->irf_tioreq      != NULL) &&
		_0C(fop->irf_ast.sa_cb   != NULL) &&
		_0C(fop->irf_ast.sa_mach != NULL);
}

static bool target_ioreq_invariant(struct target_ioreq *ti)
{
	return
		_0C(target_ioreq_bob_check(ti)) &&
		_0C(ti->ti_session       != NULL) &&
		_0C(ti->ti_nwxfer        != NULL) &&
		_0C(m0_fid_is_valid(&ti->ti_fid)) &&
		m0_tl_forall(iofops, iofop, &ti->ti_iofops,
			     io_req_fop_invariant(iofop));
}

static bool pargrp_iomap_invariant(struct pargrp_iomap *map)
{
	return
	       pargrp_iomap_bob_check(map) &&
	       map->pi_ops      != NULL &&
	       map->pi_rtype    < PIR_NR &&
	       map->pi_databufs != NULL &&
	       map->pi_ioreq    != NULL &&
	       ergo(indexvec_varr_count(&map->pi_ivv) > 0 &&
		    V_SEG_NR(&map->pi_ivv) >= 2,
		    m0_forall(i, V_SEG_NR(&map->pi_ivv) - 1,
			      v_seg_endpos(&map->pi_ivv, i) <=
			      V_INDEX(&map->pi_ivv, i+1))) &&
	       data_buf_invariant_nr(map);
}

static bool pargrp_iomap_invariant_nr(struct io_request *req)
{
	return m0_forall(i, req->ir_iomap_nr,
			 pargrp_iomap_invariant(req->ir_iomaps[i]));
}

static void nw_xfer_request_init(struct nw_xfer_request *xfer)
{
	struct io_request        *req;
	struct m0_pdclust_layout *play;

	M0_ENTRY("nw_xfer_request : %p", xfer);
	M0_PRE(xfer != NULL);

	req = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);
	nw_xfer_request_bob_init(xfer);
	xfer->nxr_rc    = 0;
	xfer->nxr_bytes = 0;
	m0_atomic64_set(&xfer->nxr_ccfop_nr, 0);
	m0_atomic64_set(&xfer->nxr_iofop_nr, 0);
	m0_atomic64_set(&xfer->nxr_rdbulk_nr, 0);
	xfer->nxr_state = NXS_INITIALIZED;
	xfer->nxr_ops   = &xfer_ops;
	m0_mutex_init(&xfer->nxr_lock);

	play = pdlayout_get(req);
	xfer->nxr_rc = tioreqht_htable_init(&xfer->nxr_tioreqs_hash,
				layout_n(play) + 2 * layout_k(play));

	M0_POST_EX(nw_xfer_request_invariant(xfer));
	M0_LEAVE();
}

static void nw_xfer_request_fini(struct nw_xfer_request *xfer)
{
	M0_PRE(xfer != NULL && xfer->nxr_state == NXS_COMPLETE);
	M0_PRE_EX(nw_xfer_request_invariant(xfer));
	M0_ENTRY("nw_xfer_request : %p, nxr_rc %d", xfer, xfer->nxr_rc);

	xfer->nxr_ops = NULL;
	m0_mutex_fini(&xfer->nxr_lock);
	nw_xfer_request_bob_fini(xfer);
	tioreqht_htable_fini(&xfer->nxr_tioreqs_hash);
	M0_LEAVE();
}

M0_INTERNAL int user_page_map(struct data_buf *dbuf, unsigned long user_addr)
{
	void *kmapped;
	int   rc;

	M0_ASSERT_INFO((user_addr & ~PAGE_MASK) == 0,
		       "user_addr = %lx", user_addr);
	M0_ASSERT_INFO(dbuf->db_page == NULL,
		       "dbuf->db_page = %p", dbuf->db_page);

	/* XXX these calls can block */
	/* XXX
	 * semaphore locking copy-pasted
	 * from m0_net implementation
	 */
	/*
	 * XXX use PAGE_SIZE and
	 * pin more than one page if needed
	 */
	down_read(&current->mm->mmap_sem);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
	rc = get_user_pages(user_addr, 1, FOLL_WRITE,
			    &dbuf->db_page, NULL);
#else
	rc = get_user_pages(current, current->mm, user_addr, 1, 1, 0,
			    &dbuf->db_page, NULL);
#endif
	up_read(&current->mm->mmap_sem);
	if (rc == 1) {
		kmapped = kmap(dbuf->db_page);
		rc = kmapped == NULL ? -EFAULT : 0;
		if (kmapped != NULL)
			data_buf_init(dbuf, kmapped, 0);
	}
	return M0_RC(rc);
}

static void user_page_unmap(struct data_buf *dbuf, bool set_dirty)
{
	M0_ASSERT(dbuf->db_page != NULL);
	kunmap(dbuf->db_page);
	if (set_dirty)
		set_page_dirty(dbuf->db_page);
	put_page(dbuf->db_page);
	dbuf->db_page = NULL;
}

static int user_data_copy(struct pargrp_iomap *map,
			  m0_bindex_t          start,
			  m0_bindex_t          end,
			  struct iov_iter     *it,
			  enum copy_direction  dir,
			  enum page_attr       filter)
{
	/*
	 * iov_iter should be able to be used with copy_to_user() as well
	 * since it is as good as a vector cursor.
	 * Present kernel 2.6.32 has no support for such requirement.
	 */
	uint64_t                  bytes;
	uint32_t                  row;
	uint32_t                  col;
	struct page              *page;
	struct data_buf          *dbuf;

	M0_ENTRY("Copy %s user-space, start = %8llu, end = %8llu",
		 dir == CD_COPY_FROM_USER ? (char *)"from" : (char *)" to ",
		 start, end);
	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(it != NULL);
	M0_PRE(M0_IN(dir, (CD_COPY_FROM_USER, CD_COPY_TO_USER)));
	M0_PRE(start >> PAGE_SHIFT == (end - 1) >> PAGE_SHIFT);

	/* Finds out the page from pargrp_iomap::pi_databufs. */
	page_pos_get(map, start, &row, &col);
	dbuf = map->pi_databufs[row][col];
	M0_ASSERT(dbuf != NULL);
	M0_ASSERT(ergo(dbuf->db_page != NULL, map->pi_ioreq->ir_direct_io));

	if (dir == CD_COPY_FROM_USER) {
		if ((dbuf->db_flags & filter) == filter) {
			if (dbuf->db_flags & PA_COPY_FRMUSR_DONE)
				return M0_RC(0);

			/*
			 * Copies page to auxiliary buffer before it gets
			 * overwritten by user data. This is needed in order
			 * to calculate delta parity in case of read-old
			 * approach.
			 */
			if (dbuf->db_auxbuf.b_addr != NULL &&
			    map->pi_rtype == PIR_READOLD) {
				if (filter == 0) {
					M0_ASSERT(dbuf->db_page == NULL);
					memcpy(dbuf->db_auxbuf.b_addr,
					       dbuf->db_buf.b_addr, PAGE_SIZE);
				} else
					return M0_RC(0);
			}

			if (dbuf->db_page == NULL) {
				page = virt_to_page(dbuf->db_buf.b_addr);
				/* Copies to appropriate offset within page. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
				bytes = iov_iter_copy_from_user_atomic(page, it,
						start & ~PAGE_MASK,
						end - start);
#else
				bytes = iov_iter_copy_from_user(page, it,
						start & ~PAGE_MASK,
						end - start);
#endif

			} else
				bytes = end - start;

			M0_LOG(M0_DEBUG, "[%p] %llu bytes copied from "
			       "user-space from offset %llu", map->pi_ioreq,
			       bytes, start);

			map->pi_ioreq->ir_copied_nr += bytes;
			/*
			 * user_data_copy() may be called to handle only part
			 * of PA_FULLPAGE_MODIFY page. In this case we should
			 * mark the page as done only when the last piece is
			 * processed. Otherwise, the rest piece of the page
			 * will be ignored.
			 */
			if (ergo(dbuf->db_flags & PA_FULLPAGE_MODIFY,
				 (end & ~PAGE_MASK) == 0))
				dbuf->db_flags |= PA_COPY_FRMUSR_DONE;

			if (bytes != end - start)
				return M0_ERR_INFO(
					-EFAULT, "[%p] Failed to"
					" copy_from_user: %" PRIu64 " !="
					" %" PRIu64 " - %" PRIu64,
					map->pi_ioreq, bytes, end, start);
		}
	} else {
		if (dbuf->db_page == NULL)
			bytes = copy_to_user(it->iov->iov_base + it->iov_offset,
					     (char *)dbuf->db_buf.b_addr +
					     (start & ~PAGE_MASK),
					     end - start);
		else
			bytes = 0;

		map->pi_ioreq->ir_copied_nr += end - start - bytes;

		M0_LOG(M0_DEBUG, "[%p] %llu bytes copied to user-space from "
		       "offset %llu", map->pi_ioreq, end - start - bytes,
		       start);

		if (bytes != 0)
			return M0_ERR_INFO(-EFAULT, "[%p] Failed to "
					   "copy_to_user", map->pi_ioreq);
	}

	return M0_RC(0);
}

static int pargrp_iomap_parity_verify(struct pargrp_iomap *map)
{
	int                       rc;
	uint32_t                  row;
	uint32_t                  col;
	struct m0_buf            *dbufs;
	struct m0_buf            *pbufs;
	struct m0_buf            *old_pbuf;
	struct m0_pdclust_layout *play;
	struct inode             *inode;
	struct m0t1fs_sb         *csb;
	struct page              *page;
	unsigned long             zpage;

	M0_ENTRY("[%p] map = %p", map->pi_ioreq, map);
	M0_PRE_EX(pargrp_iomap_invariant(map));

	inode = iomap_to_inode(map);
	csb = M0T1FS_SB(inode->i_sb);
	if (!(map->pi_ioreq->ir_type == IRT_READ && csb->csb_verify))
		return M0_RC(0);

	play = pdlayout_get(map->pi_ioreq);
	M0_ALLOC_ARR(dbufs, layout_n(play));
	M0_ALLOC_ARR(pbufs, layout_k(play));
	zpage = get_zeroed_page(GFP_KERNEL);

	if (dbufs == NULL || pbufs == NULL || zpage == 0) {
		rc = M0_ERR(-ENOMEM);
		goto last;
	}

	/* temprary buf to hold parity */
	for (col = 0; col < layout_k(play); ++col) {
		page = alloc_pages(GFP_KERNEL, 0);
		if (page == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto last;
		}

		pbufs[col].b_addr = (void *)page_address(page);
		pbufs[col].b_nob  = PAGE_SIZE;
	}

	for (row = 0; row < data_row_nr(play); ++row) {
		/* data */
		for (col = 0; col < data_col_nr(play); ++col) {
			if (map->pi_databufs[row][col] != NULL) {
				dbufs[col] =
					map->pi_databufs[row][col]->db_buf;
			} else {
				dbufs[col].b_addr = (void *)zpage;
				dbufs[col].b_nob  = PAGE_SIZE;
			}
		}
		/* generate parity into new buf */
		m0_parity_math_calculate(parity_math(map->pi_ioreq),
					 dbufs, pbufs);

		/* verify the parity */
		for (col = 0; col < layout_k(play); ++col) {
			old_pbuf = &map->pi_paritybufs[row][col]->db_buf;
			if (memcmp(pbufs[col].b_addr, old_pbuf->b_addr,
				   PAGE_SIZE)) {
				M0_LOG(M0_ERROR, "[%p] parity verification "
				       "failed for %llu [%u:%u], rc %d",
				       map->pi_ioreq, map->pi_grpid, row, col,
				       -EIO);
				rc = M0_ERR(-EIO);
				goto last;
			}
			M0_LOG(M0_DEBUG, "[%p] parity verified for %llu "
			       "[%u:%u]", map->pi_ioreq, map->pi_grpid,
			       row, col);
		}
	}

	rc = 0;
last:
	if (pbufs != NULL) {
		for (col = 0; col < layout_k(play); ++col) {
			/* free_page(NULL) is OK */
			free_page((unsigned long)pbufs[col].b_addr);
		}
	}
	m0_free(dbufs);
	m0_free(pbufs);
	free_page(zpage);
	M0_LOG(M0_DEBUG, "[%p] parity verified for %llu, rc=%d", map->pi_ioreq,
	       map->pi_grpid, rc);
	return M0_RC(rc);
}

static int pargrp_iomap_parity_recalc(struct pargrp_iomap *map)
{
	int                       rc;
	uint32_t                  row;
	uint32_t                  col;
	struct m0_buf            *dbufs;
	struct m0_buf            *pbufs;
	struct m0_pdclust_layout *play;

	M0_PRE_EX(pargrp_iomap_invariant(map));

	M0_ENTRY("[%p] map = %p", map->pi_ioreq, map);

	play = pdlayout_get(map->pi_ioreq);
	M0_ALLOC_ARR(dbufs, layout_n(play));
	M0_ALLOC_ARR(pbufs, layout_k(play));

	if (dbufs == NULL || pbufs == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto last;
	}

	if ((map->pi_ioreq->ir_type == IRT_WRITE && map->pi_rtype == PIR_NONE)
	    || map->pi_rtype == PIR_READREST) {

		unsigned long zpage;

		zpage = get_zeroed_page(GFP_KERNEL);
		if (zpage == 0) {
			rc = M0_ERR(-ENOMEM);
			goto last;
		}

		for (row = 0; row < data_row_nr(play); ++row) {
			for (col = 0; col < data_col_nr(play); ++col)
				if (map->pi_databufs[row][col] != NULL) {
					dbufs[col] = map->pi_databufs
						     [row][col]->db_buf;
				} else {
					dbufs[col].b_addr = (void *)zpage;
					dbufs[col].b_nob  = PAGE_SIZE;
				}

			for (col = 0; col < layout_k(play); ++col)
				pbufs[col] = map->pi_paritybufs[row][col]->
					     db_buf;

			m0_parity_math_calculate(parity_math(map->pi_ioreq),
						 dbufs, pbufs);
		}
		rc = 0;
		free_page(zpage);
		M0_LOG(M0_DEBUG, "[%p] Parity recalculated for %s",
		       map->pi_ioreq,
		       map->pi_rtype == PIR_READREST ? "read-rest" :
		       "aligned write");

	} else {
		struct m0_buf *old;

		M0_ALLOC_ARR(old, layout_n(play));
		if (old == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto last;
		}

		for (row = 0; row < data_row_nr(play); ++row) {
			for (col = 0; col < layout_k(play); ++col)
				pbufs[col] = map->pi_paritybufs[row][col]->
					db_buf;

			for (col = 0; col < data_col_nr(play); ++col) {
				/*
				 * During rmw-IO request with read-old approach
				 * we allocate primary and auxiliary buffers
				 * for those units from a parity group, that
				 * are spanned by input rmw-IO request. If
				 * these units belong to failed devices then
				 * during the degraded reading, primary buffers
				 * are allocated for rest of the units from the
				 * parity group in order to recover the failed
				 * units. Thus if a parity group is in dgmode,
				 * then every unit will have a primary buffer,
				 * but may not have an auxiliary buffer.
				 */
				if (map->pi_databufs[row][col] == NULL ||
				    map->pi_databufs[row][col]->
				     db_auxbuf.b_addr == NULL)
					continue;

				dbufs[col] = map->pi_databufs[row][col]->db_buf;
				old[col]   = map->pi_databufs[row][col]->
					db_auxbuf;

				m0_parity_math_diff(parity_math(map->pi_ioreq),
						    old, dbufs, pbufs, col);
			}

		}
		m0_free(old);
		rc = 0;
	}
last:
	m0_free(dbufs);
	m0_free(pbufs);
	return M0_RC(rc);
}

static int ioreq_parity_verify(struct io_request *req)
{
	int                  rc = 0;
	uint64_t             grp;
	struct pargrp_iomap *iomap;
	struct inode        *inode;
	struct m0t1fs_sb    *csb;

	M0_ENTRY("[%p]", req);
	M0_PRE_EX(io_request_invariant(req));

	inode = m0t1fs_file_to_inode(req->ir_file);
	csb = M0T1FS_SB(inode->i_sb);

	if (!(req->ir_type == IRT_READ && csb->csb_verify))
		return M0_RC(0);

	m0_semaphore_down(&m0t1fs_cpus_sem);

	for (grp = 0; grp < req->ir_iomap_nr; ++grp) {
		iomap = req->ir_iomaps[grp];
		if (iomap->pi_state == PI_DEGRADED) {
			/* data is recovered from existing data and parity.
			 * It's meaningless to do parity verification */
			continue;
		}
		rc = iomap->pi_ops->pi_parity_verify(iomap);
		if (rc != 0)
			break;
	}

	m0_semaphore_up(&m0t1fs_cpus_sem);

	return rc != 0 ? M0_ERR_INFO(rc, "[%p] Parity verification failed for "
				     "grpid=%llu", req,
				     iomap->pi_grpid) : M0_RC(rc);
}

static int ioreq_parity_recalc(struct io_request *req)
{
	int      rc = 0;
	uint64_t map;

	M0_ENTRY("[%p]", req);
	M0_PRE_EX(io_request_invariant(req));

	m0_semaphore_down(&m0t1fs_cpus_sem);

	for (map = 0; map < req->ir_iomap_nr; ++map) {
		rc = req->ir_iomaps[map]->pi_ops->pi_parity_recalc(req->
				ir_iomaps[map]);
		if (rc != 0)
			break;
	}

	m0_semaphore_up(&m0t1fs_cpus_sem);

	return rc != 0 ? M0_ERR_INFO(rc, "[%p] Parity recalc failed for "
				"grpid=%llu", req,
				req->ir_iomaps[map]->pi_grpid) : M0_RC(rc);
}

/* Finds out pargrp_iomap from array of such structures in io_request. */
static void ioreq_pgiomap_find(struct io_request    *req,
			       uint64_t              grpid,
			       uint64_t             *cursor,
			       struct pargrp_iomap **out)
{
	uint64_t id;
	M0_PRE(req    != NULL);
	M0_PRE(out    != NULL);
	M0_PRE(cursor != NULL);
	M0_PRE(*cursor < req->ir_iomap_nr);
	M0_ENTRY("[%p] group_id = %llu, cursor = %llu", req, grpid, *cursor);

	for (id = *cursor; id < req->ir_iomap_nr; ++id) {
		if (req->ir_iomaps[id]->pi_grpid == grpid) {
			*out = req->ir_iomaps[id];
			*cursor = id;
			break;
		}
	}

	M0_POST(id < req->ir_iomap_nr);
	M0_LEAVE("[%p] result iomap = %llu", req, id);
}

static int ioreq_user_data_copy(struct io_request   *req,
				enum copy_direction  dir,
				enum page_attr       filter)
{
	int                        rc;
	uint64_t                   map;
	m0_bindex_t                grpstart;
	m0_bindex_t                grpend;
	m0_bindex_t                pgstart;
	m0_bindex_t                pgend;
	m0_bcount_t                count;
	struct iov_iter            it;
	struct m0_ivec_varr_cursor srccur;
	struct m0_pdclust_layout  *play;
	struct pargrp_iomap       *this_map;

	M0_ENTRY("[%p] %s user-space. filter = 0x%x",
		 req, dir == CD_COPY_FROM_USER ? (char *)"from" : (char *)"to",
		 filter);
	M0_PRE_EX(io_request_invariant(req));
	M0_PRE(dir < CD_NR);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	iov_iter_init(&it, WRITE, req->ir_iovec, V_SEG_NR(&req->ir_ivv),
		      indexvec_varr_count(&req->ir_ivv));
#else
	iov_iter_init(&it, req->ir_iovec, V_SEG_NR(&req->ir_ivv),
		      indexvec_varr_count(&req->ir_ivv), 0);
#endif
	m0_ivec_varr_cursor_init(&srccur, &req->ir_ivv);
	play = pdlayout_get(req);

	for (map = 0; map < req->ir_iomap_nr; ++map) {
		this_map = req->ir_iomaps[map];
		M0_ASSERT_EX(pargrp_iomap_invariant(this_map));

		count    = 0;
		grpstart = data_size(play) * this_map->pi_grpid;
		grpend   = grpstart + data_size(play);

		while (!m0_ivec_varr_cursor_move(&srccur, count) &&
		       m0_ivec_varr_cursor_index(&srccur) < grpend) {

			pgstart = m0_ivec_varr_cursor_index(&srccur);
			pgend = min64u(m0_round_up(pgstart + 1, PAGE_SIZE),
				   pgstart + m0_ivec_varr_cursor_step(&srccur));
			count = pgend - pgstart;

			/*
			 * This takes care of finding correct page from
			 * current pargrp_iomap structure from pgstart
			 * and pgend.
			 */
			rc = user_data_copy(this_map, pgstart, pgend,
					    &it, dir, filter);
			if (rc != 0)
				return M0_ERR_INFO(
					rc, "[%p] Copy failed (pgstart=%" PRIu64
					" pgend=%" PRIu64 ")",
					req, pgstart, pgend);

			iov_iter_advance(&it, count);
		}
	}

	return M0_RC(0);
}

static void indexvec_sort(struct m0_indexvec_varr *ivec)
{
	uint32_t i;
	uint32_t j;

	M0_ENTRY("indexvec = %p", ivec);
	M0_PRE(ivec != NULL && indexvec_varr_count(ivec) != 0);

	/*
	 * TODO Should be replaced by an efficient sorting algorithm,
	 * something like heapsort which is fairly inexpensive in kernel
	 * mode with the least worst case scenario.
	 * Existing heap sort from kernel code can not be used due to
	 * apparent disconnect between index vector and its associated
	 * count vector for same index.
	 */
	for (i = 0; i < V_SEG_NR(ivec); ++i) {
		for (j = i+1; j < V_SEG_NR(ivec); ++j) {
			if (V_INDEX(ivec, i) > V_INDEX(ivec, j)) {
				M0_SWAP(V_INDEX(ivec, i), V_INDEX(ivec, j));
				M0_SWAP(V_COUNT(ivec, i), V_COUNT(ivec, j));
			}
		}
	}
	M0_LEAVE();
}

static int pargrp_iomap_init(struct pargrp_iomap *map,
			     struct io_request   *req,
			     uint64_t             grpid)
{
	int                       rc;
	int                       row;
	struct m0_pdclust_layout *play;
	struct inode             *inode;
	struct m0t1fs_sb         *csb;

	M0_ENTRY("[%p] map = %p, grpid = %llu", req, map, grpid);
	M0_PRE(map != NULL);
	M0_PRE(req != NULL);

	pargrp_iomap_bob_init(map);
	map->pi_ops        = &iomap_ops;
	map->pi_rtype      = PIR_NONE;
	map->pi_grpid      = grpid;
	map->pi_ioreq      = req;
	map->pi_state      = PI_HEALTHY;
	map->pi_paritybufs = NULL;

	inode = iomap_to_inode(map);
	csb = M0T1FS_SB(inode->i_sb);

	play = pdlayout_get(req);
	rc = m0_indexvec_varr_alloc(&map->pi_ivv, page_nr(data_size(play)));
	if (rc != 0)
		goto fail_iv;

	/*
	 * This number is incremented only when a valid segment
	 * is added to the index vector.
	 */
	V_SEG_NR(&map->pi_ivv) = 0;

	M0_ALLOC_ARR(map->pi_databufs, data_row_nr(play));
	if (map->pi_databufs == NULL)
		goto fail;

	for (row = 0; row < data_row_nr(play); ++row) {
		M0_ALLOC_ARR(map->pi_databufs[row], data_col_nr(play));
		if (map->pi_databufs[row] == NULL)
			goto fail;
	}

	if (req->ir_type == IRT_WRITE ||
	    (req->ir_type == IRT_READ && csb->csb_verify)) {
		M0_ALLOC_ARR(map->pi_paritybufs, parity_row_nr(play));
		if (map->pi_paritybufs == NULL)
			goto fail;

		for (row = 0; row < parity_row_nr(play); ++row) {
			M0_ALLOC_ARR(map->pi_paritybufs[row],
				     parity_col_nr(play));
			if (map->pi_paritybufs[row] == NULL)
				goto fail;
		}
	}

	M0_LOG(M0_DEBUG, "[%p] grpid=%llu, ivec has %llu segs, "
			 "databufs=[%u x %u] paritybufs=[%u x %u]",
			 req, grpid, page_nr(data_size(play)),
			 data_row_nr(play), data_col_nr(play),
			 parity_row_nr(play), parity_col_nr(play));

	M0_POST_EX(pargrp_iomap_invariant(map));
	return M0_RC(0);

fail:
	m0_indexvec_varr_free(&map->pi_ivv);

	if (map->pi_databufs != NULL) {
		for (row = 0; row < data_row_nr(play); ++row)
			m0_free(map->pi_databufs[row]);
		m0_free(map->pi_databufs);
	}
	if (map->pi_paritybufs != NULL) {
		for (row = 0; row < parity_row_nr(play); ++row)
			m0_free(map->pi_paritybufs[row]);
		m0_free(map->pi_paritybufs);
	}
fail_iv:
	return M0_ERR_INFO(-ENOMEM, "[%p] Memory allocation failed", req);
}

static void pargrp_iomap_fini(struct pargrp_iomap *map)
{
	uint32_t                  row;
	uint32_t                  col;
	struct m0_pdclust_layout *play;

	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);
	M0_PRE_EX(pargrp_iomap_invariant(map));

	play         = pdlayout_get(map->pi_ioreq);
	map->pi_ops  = NULL;
	map->pi_rtype = PIR_NONE;
	map->pi_state = PI_NONE;

	pargrp_iomap_bob_fini(map);
	m0_indexvec_varr_free(&map->pi_ivv);

	for (row = 0; row < data_row_nr(play); ++row) {
		for (col = 0; col < data_col_nr(play); ++col) {
			if (map->pi_databufs[row][col] != NULL) {
				data_buf_dealloc_fini(map->
						pi_databufs[row][col]);
				map->pi_databufs[row][col] = NULL;
			}
		}
		m0_free0(&map->pi_databufs[row]);
	}

	if (map->pi_paritybufs != NULL) {
		for (row = 0; row < parity_row_nr(play); ++row) {
			for (col = 0; col < parity_col_nr(play); ++col) {
				if (map->pi_paritybufs[row][col] != NULL) {
					data_buf_dealloc_fini(map->
						pi_paritybufs[row][col]);
					map->pi_paritybufs[row][col] = NULL;
				}
			}
			m0_free0(&map->pi_paritybufs[row]);
		}
	}

	m0_free0(&map->pi_databufs);
	m0_free0(&map->pi_paritybufs);
	map->pi_ioreq = NULL;
	M0_LEAVE();
}

/**
 * Check if this [@index, @count] is covered by existing ivec in the @map.
 */
static bool pargrp_iomap_spans_seg(struct pargrp_iomap *map,
				   m0_bindex_t index, m0_bcount_t count)
{
	uint32_t seg;
	bool     spanned = false;

	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);

	M0_PRE_EX(pargrp_iomap_invariant(map));

	for (seg = 0; seg < V_SEG_NR(&map->pi_ivv); ++seg) {
		if (V_INDEX(&map->pi_ivv, seg) <= index &&
		    index + count <= v_seg_endpos(&map->pi_ivv, seg)) {
			spanned = true;
			break;
		}
	}
	return M0_RC(!!spanned);
}

static int pargrp_iomap_databuf_alloc(struct pargrp_iomap *map,
				      uint32_t             row,
				      uint32_t             col)
{
	M0_PRE(map != NULL);
	M0_PRE(map->pi_databufs[row][col] == NULL);

	M0_ENTRY("[%p] map %p, row %u col %u", map->pi_ioreq, map, row, col);
	map->pi_databufs[row][col] = data_buf_alloc_init(0);

	return map->pi_databufs[row][col] == NULL ? M0_ERR(-ENOMEM) : 0;
}

/* Allocates data_buf structures as needed and populates the buffer flags. */
static int pargrp_iomap_seg_process(struct pargrp_iomap *map,
				    uint64_t             seg,
				    bool                 rmw)
{
	int                        rc;
	int                        flags;
	bool                       ret;
	uint32_t                   row;
	uint32_t                   col;
	uint64_t                   count = 0;
	m0_bindex_t                start;
	m0_bindex_t                end;
	struct inode              *inode;
	struct m0_ivec_varr_cursor cur;
	struct m0_pdclust_layout  *play;
	struct io_request         *req = map->pi_ioreq;

	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);
	M0_LOG(M0_DEBUG, "[%p] pgid %llu seg %llu = [%llu, +%llu), %s",
			 map->pi_ioreq, map->pi_grpid, seg,
			 V_INDEX(&map->pi_ivv, seg),
			 V_COUNT(&map->pi_ivv, seg),
			 rmw ? "rmw" : "aligned");
	play  = pdlayout_get(req);
	inode = m0t1fs_file_to_inode(req->ir_file);
	m0_ivec_varr_cursor_init(&cur, &map->pi_ivv);
	ret = m0_ivec_varr_cursor_move_to(&cur, V_INDEX(&map->pi_ivv, seg));
	M0_ASSERT(!ret);

	/* process a page at each iteration */
	while (!m0_ivec_varr_cursor_move(&cur, count)) {
		start = m0_ivec_varr_cursor_index(&cur);
		end   = min64u(m0_round_up(start + 1, PAGE_SIZE),
			       start + m0_ivec_varr_cursor_step(&cur));
		count = end - start;

		flags = 0;
		if (req->ir_type == IRT_WRITE) {
			flags |= PA_WRITE;
			flags |= count == PAGE_SIZE ?
				 PA_FULLPAGE_MODIFY : PA_PARTPAGE_MODIFY;

			/*
			 * Even if PA_PARTPAGE_MODIFY flag is set in
			 * this buffer, the auxiliary buffer can not be
			 * allocated until ::pi_rtype is selected.
			 */
			if (rmw && (flags & PA_PARTPAGE_MODIFY) &&
			    (end < inode->i_size ||
			     (inode->i_size > 0 &&
			      page_id(end - 1) == page_id(inode->i_size - 1))))
				flags |= PA_READ;
		} else {
			/*
			 * For read IO requests, file_aio_read() has already
			 * delimited the index vector to EOF boundary.
			 */
			flags |= PA_READ;
		}

		page_pos_get(map, start, &row, &col);
		rc = pargrp_iomap_databuf_alloc(map, row, col);
		M0_LOG(M0_DEBUG, "[%p] alloc start %8llu count %4llu pgid "
			 "%3llu row %u col %u f 0x%x addr %p",
			 req, start, count, map->pi_grpid, row, col, flags,
			 map->pi_databufs[row][col] != NULL ?
			 map->pi_databufs[row][col]->db_buf.b_addr : NULL);
		if (rc != 0)
			goto err;
		map->pi_databufs[row][col]->db_flags = flags;
	}

	return M0_RC(0);
err:
	for (row = 0; row < data_row_nr(play); ++row) {
		for (col = 0; col < data_col_nr(play); ++col) {
			if (map->pi_databufs[row][col] != NULL) {
				data_buf_dealloc_fini(map->pi_databufs
						      [row][col]);
				map->pi_databufs[row][col] = NULL;
			}
		}
	}
	return M0_ERR_INFO(rc, "[%p] databuf_alloc failed", req);
}

static uint64_t pargrp_iomap_fullpages_count(struct pargrp_iomap *map)
{
	uint32_t                  row;
	uint32_t                  col;
	uint64_t                  nr = 0;
	struct m0_pdclust_layout *play;

	M0_PRE_EX(pargrp_iomap_invariant(map));

	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);
	play = pdlayout_get(map->pi_ioreq);

	for (row = 0; row < data_row_nr(play); ++row) {
		for (col = 0; col < data_col_nr(play); ++col) {

			if (map->pi_databufs[row][col] &&
			    map->pi_databufs[row][col]->db_flags &
			    PA_FULLPAGE_MODIFY)
				++nr;
		}
	}
	M0_LEAVE();
	return nr;
}

static uint64_t pargrp_iomap_auxbuf_alloc(struct pargrp_iomap *map,
					  uint32_t             row,
					  uint32_t             col)
{
	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(map->pi_rtype == PIR_READOLD);

	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);
	map->pi_databufs[row][col]->db_auxbuf.b_addr = (void *)
		get_zeroed_page(GFP_KERNEL);

	if (map->pi_databufs[row][col]->db_auxbuf.b_addr == NULL)
		return M0_ERR(-ENOMEM);
	++iommstats.a_page_nr;
	map->pi_databufs[row][col]->db_auxbuf.b_nob = PAGE_SIZE;

	return M0_RC(0);
}

/*
 * Allocates auxiliary buffer for data_buf structures in
 * pargrp_iomap structure.
 */
static int pargrp_iomap_readold_auxbuf_alloc(struct pargrp_iomap *map)
{
	int                        rc = 0;
	uint64_t                   start;
	uint64_t                   end;
	uint64_t                   count = 0;
	uint32_t                   row;
	uint32_t                   col;
	struct inode              *inode;
	struct m0_ivec_varr_cursor cur;

	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(map->pi_rtype == PIR_READOLD);

	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);
	inode = m0t1fs_file_to_inode(map->pi_ioreq->ir_file);
	m0_ivec_varr_cursor_init(&cur, &map->pi_ivv);

	while (!m0_ivec_varr_cursor_move(&cur, count)) {
		start = m0_ivec_varr_cursor_index(&cur);
		end   = min64u(m0_round_up(start + 1, PAGE_SIZE),
			       start + m0_ivec_varr_cursor_step(&cur));
		count = end - start;
		page_pos_get(map, start, &row, &col);

		if (map->pi_databufs[row][col] != NULL) {
			/*
			 * In Readold approach, all valid pages have to
			 * be read regardless of whether they are fully
			 * occupied or partially occupied.
			 * This is needed in order to calculate correct
			 * parity in differential manner.
			 * Also, read flag should be set only for pages
			 * which lie within end-of-file boundary.
			 */
			if (end < inode->i_size ||
			    (inode->i_size > 0 &&
			     page_id(end - 1) == page_id(inode->i_size - 1)))
				map->pi_databufs[row][col]->db_flags |=
					PA_READ;

			rc = pargrp_iomap_auxbuf_alloc(map, row, col);
			if (rc != 0)
				return M0_ERR_INFO(rc, "[%p] auxbuf_alloc "
						   "failed", map->pi_ioreq);
		}
	}
	return M0_RC(rc);
}

/*
 * A read request from rmw IO request can lead to either
 *
 * read_old - Read the old data for the extent spanned by current
 * IO request, along with the old parity unit. This approach needs
 * to calculate new parity in _iterative_ manner. This approach is
 * selected only if current IO extent lies within file size.
 *
 * read_rest - Read rest of the parity group, which is _not_ spanned
 * by current IO request, so that data for whole parity group can
 * be availble for parity calculation.
 * This approach reads the extent from start of parity group to the
 * point where a page is completely spanned by incoming IO request.
 *
 * Typically, the approach which leads to least size of data to be
 * read and written from server is selected.
 *
 *   N = 5, P = 1, K = 1, unit_size = 4k
 *   F  => Fully occupied
 *   P' => Partially occupied
 *   #  => Parity unit
 *   *  => Spare unit
 *   x  => Start of actual file extent.
 *   y  => End of actual file extent.
 *   a  => Rounded down value of x.
 *   b  => Rounded up value of y.
 *
 *  Read-rest approach
 *
 *   a     x
 *   +---+---+---+---+---+---+---+
 *   |   | P'| F | F | F | # | * |  PG#0
 *   +---+---+---+---+---+---+---+
 *   | F | F | F | F | F | # | * |  PG#1
 *   +---+---+---+---+---+---+---+
 *   | F | F | F | P'|   | # | * |  PG#2
 *   +---+---+---+---+---+---+---+
 *     N   N   N   N   N   K   P
 *                 y     b
 *
 *  Read-old approach
 *
 *   a     x
 *   +---+---+---+---+---+---+---+
 *   |   |   |   | P'| F | # | * |  PG#0
 *   +---+---+---+---+---+---+---+
 *   | F | F | F | F | F | # | * |  PG#1
 *   +---+---+---+---+---+---+---+
 *   | F | P'|   |   |   | # | * |  PG#2
 *   +---+---+---+---+---+---+---+
 *     N   N   N   N   N   K   P
 *                 y     b
 *
 */
static int pargrp_iomap_readrest(struct pargrp_iomap *map)
{
	int                        rc;
	uint32_t                   row;
	uint32_t                   col;
	uint32_t                   seg;
	uint32_t                   seg_nr;
	m0_bindex_t                grpstart;
	m0_bindex_t                grpend;
	m0_bindex_t                start;
	m0_bindex_t                end;
	m0_bcount_t                count = 0;
	struct inode              *inode;
	struct m0_indexvec_varr   *ivec;
	struct m0_ivec_varr_cursor cur;
	struct m0_pdclust_layout  *play;

	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(map->pi_rtype == PIR_READREST);

	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);
	play     = pdlayout_get(map->pi_ioreq);
	ivec     = &map->pi_ivv;
	seg_nr   = V_SEG_NR(&map->pi_ivv);
	grpstart = data_size(play) * map->pi_grpid;
	grpend   = grpstart + data_size(play);

	/* Extends first segment to align with start of parity group. */
	V_COUNT(ivec, 0) += (V_INDEX(ivec, 0) - grpstart);
	V_INDEX(ivec, 0)  = grpstart;

	/* Extends last segment to align with end of parity group. */
	V_COUNT(ivec, seg_nr - 1) = grpend - V_INDEX(ivec, seg_nr - 1);

	/*
	 * All io extents _not_ spanned by pargrp_iomap::pi_ivv
	 * need to be included so that _all_ pages from parity group
	 * are available to do IO.
	 */
	for (seg = 1; seg_nr > 2 && seg <= seg_nr - 2; ++seg) {
		if (v_seg_endpos(ivec, seg) < V_INDEX(ivec, seg + 1))
			V_COUNT(ivec, seg) += V_INDEX(ivec, seg + 1) -
					      v_seg_endpos(ivec, seg);
	}

	inode = m0t1fs_file_to_inode(map->pi_ioreq->ir_file);
	m0_ivec_varr_cursor_init(&cur, &map->pi_ivv);

	while (!m0_ivec_varr_cursor_move(&cur, count)) {

		start = m0_ivec_varr_cursor_index(&cur);
		end   = min64u(m0_round_up(start + 1, PAGE_SIZE),
			       start + m0_ivec_varr_cursor_step(&cur));
		count = end - start;
		page_pos_get(map, start, &row, &col);

		if (map->pi_databufs[row][col] == NULL) {
			rc = pargrp_iomap_databuf_alloc(map, row, col);
			if (rc != 0)
				return M0_ERR_INFO(rc, "[%p] databuf_alloc "
						   "failed", map->pi_ioreq);

			if (end <= inode->i_size || (inode->i_size > 0 &&
			    page_id(end - 1) == page_id(inode->i_size - 1)))
				map->pi_databufs[row][col]->db_flags |=
					PA_READ;
		}
	}

	return M0_RC(0);
}

static int pargrp_iomap_paritybufs_alloc(struct pargrp_iomap *map)
{
	uint32_t                  row;
	uint32_t                  col;
	struct m0_pdclust_layout *play;
	struct inode             *inode;
	struct m0t1fs_sb         *csb;
	struct data_buf          *dbuf;

	M0_PRE_EX(pargrp_iomap_invariant(map));

	M0_ENTRY("[%p] map %p grpid=%llu", map->pi_ioreq, map, map->pi_grpid);
	inode = iomap_to_inode(map);
	csb = M0T1FS_SB(inode->i_sb);

	play = pdlayout_get(map->pi_ioreq);
	for (row = 0; row < parity_row_nr(play); ++row) {
		for (col = 0; col < parity_col_nr(play); ++col) {
			struct file *irf;

			map->pi_paritybufs[row][col] = data_buf_alloc_init(0);
			if (map->pi_paritybufs[row][col] == NULL)
				goto err;
			dbuf = map->pi_paritybufs[row][col];
			if (map->pi_ioreq->ir_type == IRT_WRITE)
				dbuf->db_flags |= PA_WRITE;

			irf = map->pi_ioreq->ir_file;
			if ((map->pi_rtype == PIR_READOLD ||
			    (map->pi_ioreq->ir_type == IRT_READ &&
			     csb->csb_verify)) &&
			    m0t1fs_file_to_inode(irf)->i_size >
			    data_size(play) * map->pi_grpid)
				dbuf->db_flags |= PA_READ;
		}
	}
	return M0_RC(0);
err:
	for (row = 0; row < parity_row_nr(play); ++row) {
		for (col = 0; col < parity_col_nr(play); ++col)
			m0_free0(&map->pi_paritybufs[row][col]);
	}
	return M0_ERR_INFO(-ENOMEM, "[%p] Memory allocation failed for "
			   "data_buf.", map->pi_ioreq);
}

static m0_bcount_t seg_collate(struct pargrp_iomap        *map,
			       struct m0_ivec_varr_cursor *cursor)
{
	uint32_t                  seg;
	uint32_t                  cnt;
	m0_bindex_t               start;
	m0_bindex_t               grpend;
	m0_bcount_t               segcount;
	struct m0_pdclust_layout *play;

	M0_PRE(map    != NULL);
	M0_PRE(cursor != NULL);

	cnt    = 0;
	play   = pdlayout_get(map->pi_ioreq);
	grpend = map->pi_grpid * data_size(play) + data_size(play);
	start  = m0_ivec_varr_cursor_index(cursor);

	for (seg = cursor->vc_seg; start < grpend &&
	     seg < V_SEG_NR(cursor->vc_ivv) - 1; ++seg) {

		segcount = seg == cursor->vc_seg ?
			 m0_ivec_varr_cursor_step(cursor) :
			 V_COUNT(cursor->vc_ivv, seg);

		if (start + segcount ==
		    V_INDEX(&map->pi_ioreq->ir_ivv, seg + 1)) {

			if (start + segcount >= grpend) {
				start = grpend;
				break;
			}
			start += segcount;
		} else
			break;
		++cnt;
	}

	if (cnt == 0)
		return 0;

	/* If this was last segment in vector, add its count too. */
	if (seg == V_SEG_NR(cursor->vc_ivv) - 1) {
		if (start + V_COUNT(cursor->vc_ivv, seg) >= grpend)
			start = grpend;
		else
			start += V_COUNT(cursor->vc_ivv, seg);
	}

	return start - m0_ivec_varr_cursor_index(cursor);
}

static int pargrp_iomap_populate(struct pargrp_iomap        *map,
				 struct m0_indexvec_varr    *ivv,
				 struct m0_ivec_varr_cursor *cursor)
{
	int                       rc;
	bool                      rmw = false;
	uint64_t                  seg;
	uint64_t                  size = 0;
	uint64_t                  grpsize;
	m0_bcount_t               count = 0;
	m0_bindex_t               endpos = 0;
	m0_bcount_t               segcount = 0;
	/* Number of pages _completely_ spanned by incoming io vector. */
	uint64_t                  nr = 0;
	/* Number of pages to be read + written for read-old approach. */
	uint64_t                  ro_page_nr;
	/* Number of pages to be read + written for read-rest approach. */
	uint64_t                  rr_page_nr;
	m0_bindex_t               grpstart;
	m0_bindex_t               grpend;
	m0_bindex_t               currindex;
	struct m0_pdclust_layout *play;
	struct inode             *inode;
	struct m0t1fs_sb         *csb;
	struct io_request        *req = map->pi_ioreq;

	M0_ENTRY("[%p] map %p, indexvec %p", req, map, ivv);
	M0_PRE(map != NULL);
	M0_PRE(ivv != NULL);

	play     = pdlayout_get(map->pi_ioreq);
	grpsize  = data_size(play);
	grpstart = grpsize * map->pi_grpid;
	grpend   = grpstart + grpsize;
	inode = iomap_to_inode(map);
	csb = M0T1FS_SB(inode->i_sb);

	/* For a write in existing region, if size of this map is less
	 * than parity group size, it is a read-modify-write.
	 */
	if (map->pi_ioreq->ir_type == IRT_WRITE && grpstart < inode->i_size) {
		for (seg = cursor->vc_seg; seg < V_SEG_NR(ivv) &&
			V_INDEX(ivv, seg) < grpend; ++seg) {
			currindex = seg == cursor->vc_seg ?
				    m0_ivec_varr_cursor_index(cursor) :
				    V_INDEX(ivv, seg);
			size += min64u(v_seg_endpos(ivv, seg), grpend) -
				currindex;
		}
		if (size < grpsize)
			rmw = true;
	}
	M0_LOG(M0_INFO, "[%p] Group id %llu is %s", req,
	       map->pi_grpid, rmw ? "rmw" : "aligned");

	size = inode->i_size;
	for (seg = 0; !m0_ivec_varr_cursor_move(cursor, count) &&
		      m0_ivec_varr_cursor_index(cursor) < grpend;) {
		/*
		 * Skips the current segment if it is completely spanned by
		 * rounding up/down of earlier segment.
		 */
		if (map->pi_ops->pi_spans_seg(map,
					    m0_ivec_varr_cursor_index(cursor),
					    m0_ivec_varr_cursor_step(cursor))) {
			count = m0_ivec_varr_cursor_step(cursor);
			continue;
		}

		V_INDEX(&map->pi_ivv, seg) = m0_ivec_varr_cursor_index(cursor);
		endpos = min64u(grpend, m0_ivec_varr_cursor_index(cursor) +
				m0_ivec_varr_cursor_step(cursor));

		segcount = seg_collate(map, cursor);
		if (segcount > 0)
			endpos = V_INDEX(&map->pi_ivv, seg) + segcount;

		V_COUNT(&map->pi_ivv, seg) = endpos -
					V_INDEX(&map->pi_ivv, seg);

		/* For read IO request, IO should not go beyond EOF. */
		if (map->pi_ioreq->ir_type == IRT_READ &&
		    v_seg_endpos(&map->pi_ivv, seg) > size) {
			if (V_INDEX(&map->pi_ivv, seg) + 1 < size)
				V_COUNT(&map->pi_ivv, seg) = size -
					V_INDEX(&map->pi_ivv, seg);
			else
				V_COUNT(&map->pi_ivv, seg) = 0;
			if (V_COUNT(&map->pi_ivv, seg) == 0) {
				count = m0_ivec_varr_cursor_step(cursor);
				continue;
			}
		}

		/*
		 * If current segment is _partially_ spanned by previous
		 * segment in pargrp_iomp::pi_ivv, start of segment is
		 * rounded up to move to next page.
		 */
		if (seg > 0 && V_INDEX(&map->pi_ivv, seg) <
		    v_seg_endpos(&map->pi_ivv, seg - 1)) {
			m0_bindex_t newindex;

			newindex = m0_round_up(
					V_INDEX(&map->pi_ivv, seg) + 1,
					PAGE_SIZE);
			V_COUNT(&map->pi_ivv, seg) -= (newindex -
					V_INDEX(&map->pi_ivv, seg));

			V_INDEX(&map->pi_ivv, seg)  = newindex;
		}

		++ V_SEG_NR(&map->pi_ivv);
		M0_LOG(M0_DEBUG, "[%p] pre  grpid = %llu "
		       "seg %llu = [%llu, +%llu)",
		       req, map->pi_grpid, seg,
		       V_INDEX(&map->pi_ivv, seg),
		       V_COUNT(&map->pi_ivv, seg));

		if (!(map->pi_ioreq->ir_type == IRT_READ && csb->csb_verify)) {
			/* if not in 'verify mode', ... */
			rc = map->pi_ops->pi_seg_process(map, seg, rmw);
			if (rc != 0)
				return M0_ERR_INFO(rc, "[%p] seg_process "
						   "failed", req);
		}

		V_INDEX(&map->pi_ivv, seg) =
			round_down(V_INDEX(&map->pi_ivv, seg), PAGE_SIZE);

		V_COUNT(&map->pi_ivv, seg) =
			round_up(endpos, PAGE_SIZE) -
			V_INDEX(&map->pi_ivv, seg);

		M0_LOG(M0_DEBUG, "[%p] post grpid = %llu "
		       "seg %llu = [%llu, +%llu)",
		       req, map->pi_grpid, seg,
		       V_INDEX(&map->pi_ivv, seg),
		       V_COUNT(&map->pi_ivv, seg));

		count = endpos - m0_ivec_varr_cursor_index(cursor);
		M0_LOG(M0_DEBUG, "[%p] cursor will advance +%llu from %llu",
		       req, count, m0_ivec_varr_cursor_index(cursor));
		++seg;
	}

	/* In 'verify mode', read all data units in this parity group */
	if (map->pi_ioreq->ir_type == IRT_READ && csb->csb_verify) {
		indexvec_varr_dump(&map->pi_ivv);
		M0_LOG(M0_DEBUG, "[%p] change ivec to [%llu, +%llu) "
		       "for group id %llu",
		       req, grpstart, grpsize, map->pi_grpid);
		V_SEG_NR(&map->pi_ivv)   = 1;
		V_INDEX(&map->pi_ivv, 0) = grpstart;
		/* full parity group, but limit to file size. */
		count = min64u(grpend, inode->i_size) - grpstart;
		/* and then round to page size */
		V_COUNT(&map->pi_ivv, 0) = round_up(count, PAGE_SIZE);

		rc = map->pi_ops->pi_seg_process(map, 0, rmw);
		if (rc != 0)
			return M0_ERR_INFO(rc, "[%p] seg_process failed", req);
	}
	/*
	 * Decides whether to undertake read-old approach or read-rest for
	 * an rmw IO request.
	 * By default, the segments in index vector pargrp_iomap::pi_ivv
	 * are suitable for read-old approach.
	 * Hence the index vector is changed only if read-rest approach
	 * is selected.
	 */
	if (rmw) {
		nr = map->pi_ops->pi_fullpages_find(map);

		/*
		 * Can use number of data_buf structures instead of using
		 * indexvec_page_nr().
		 */
		ro_page_nr = /* Number of pages to be read. */
			     iomap_page_nr(map) +
			     parity_units_page_nr(play) +
			     /* Number of pages to be written. */
			     iomap_page_nr(map) +
			     parity_units_page_nr(play);

		rr_page_nr = /* Number of pages to be read. */
			     page_nr(grpend - grpstart) - nr +
			     /* Number of pages to be written. */
			     iomap_page_nr(map) +
			     parity_units_page_nr(play);

		if (rr_page_nr < ro_page_nr) {
			M0_LOG(M0_DEBUG, "[%p] RR approach selected", req);
			map->pi_rtype = PIR_READREST;
			rc = map->pi_ops->pi_readrest(map);
		} else {
			M0_LOG(M0_DEBUG, "[%p] RO approach selected", req);
			map->pi_rtype = PIR_READOLD;
			rc = map->pi_ops->pi_readold_auxbuf_alloc(map);
		}
		if (rc != 0)
			return M0_ERR_INFO(rc, "[%p] failed", req);
	}

	/* For READ in verify mode or WRITE */
	if (map->pi_ioreq->ir_type == IRT_WRITE ||
	    (map->pi_ioreq->ir_type == IRT_READ && csb->csb_verify))
		rc = map->pi_ops->pi_paritybufs_alloc(map);

	M0_POST_EX(ergo(rc == 0, pargrp_iomap_invariant(map)));

	return M0_RC(rc);
}

/**
 * Mark all pages in @param map, with the specified @param type, as failed.
 */
static int pargrp_iomap_pages_mark_as_failed(struct pargrp_iomap       *map,
					     enum m0_pdclust_unit_type  type)
{
	int                         rc = 0;
	uint32_t                    row;
	uint32_t                    row_nr;
	uint32_t                    col;
	uint32_t                    col_nr;
	struct data_buf          ***bufs;
	struct m0_pdclust_layout   *play;
	M0_PRE(map != NULL);
	M0_PRE(M0_IN(type, (M0_PUT_DATA, M0_PUT_PARITY)));
	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);

	play = pdlayout_get(map->pi_ioreq);

	if (type == M0_PUT_DATA) {
		M0_ASSERT(map->pi_databufs != NULL);
		row_nr = data_row_nr(play);
		col_nr = data_col_nr(play);
		bufs   = map->pi_databufs;
	} else {
		row_nr = parity_row_nr(play);
		col_nr = parity_col_nr(play);
		bufs   = map->pi_paritybufs;
	}

	/*
	 * Allocates data_buf structures from either ::pi_databufs
	 * or ::pi_paritybufs array.
	 * The loop traverses the matrix, column (unit) by column (unit).
	 */
	for (col = 0; col < col_nr; ++col) {
		for (row = 0; row < row_nr; ++row) {
			/*
			 * If the page is marked as PA_READ_FAILED, all
			 * other pages belonging to the unit same as
			 * the failed one, are also marked as PA_READ_FAILED,
			 * hence the loop breaks from here.
			 */
			if (bufs[row][col] != NULL &&
			    bufs[row][col]->db_flags & PA_READ_FAILED)
				break;
		}

		if (row == row_nr)
			continue;

		for (row = 0; row < row_nr; ++row) {
			if (bufs[row][col] == NULL) {
				bufs[row][col] = data_buf_alloc_init(0);
				if (bufs[row][col] == NULL) {
					rc = M0_ERR(-ENOMEM);
					break;
				}
			}
			bufs[row][col]->db_flags |= PA_READ_FAILED;
		}
	}
	return M0_RC(rc);
}

static int unit_state(const struct m0_pdclust_src_addr *src,
		      const struct io_request *req,
		      enum m0_pool_nd_state *state)
{
	struct m0_pdclust_instance *play_instance;
	struct m0_pdclust_tgt_addr  tgt;
	int                         rc;
	struct m0_poolmach         *pm;

	M0_ENTRY("[%p]", req);

	play_instance = pdlayout_instance(layout_instance(req));
	m0_fd_fwd_map(play_instance, src, &tgt);

	pm = m0t1fs_file_to_poolmach(req->ir_file);
	M0_ASSERT(pm != NULL);
	rc = m0_poolmach_device_state(pm, tgt.ta_obj, state);
	if (rc != 0)
		return M0_RC(rc);
	return M0_RC(rc);
}

static int io_spare_map(const struct pargrp_iomap *map,
			const struct m0_pdclust_src_addr *src,
			uint32_t *spare_slot, uint32_t *spare_slot_prev,
			enum m0_pool_nd_state *eff_state)
{

	struct m0_pdclust_layout   *play;
	struct m0_pdclust_instance *play_instance;
	const struct m0_fid        *gfid;
	struct m0_pdclust_src_addr  spare;
	int                         rc;
	struct m0_poolmach         *pm;

	M0_ENTRY("[%p]", map->pi_ioreq);
	play = pdlayout_get(map->pi_ioreq);
	play_instance = pdlayout_instance(layout_instance(map->pi_ioreq));
	gfid = file_to_fid(map->pi_ioreq->ir_file);

	pm = m0t1fs_file_to_poolmach(map->pi_ioreq->ir_file);
	M0_ASSERT(pm != NULL);
	rc = m0_sns_repair_spare_map(pm, gfid, play, play_instance,
				     src->sa_group, src->sa_unit,
				     spare_slot, spare_slot_prev);
	if (rc != 0) {
		return M0_RC(rc);
	}
	/* Check if there is an effective failure of unit. */
	spare.sa_group = src->sa_group;
	spare.sa_unit = *spare_slot_prev;
	rc = unit_state(&spare, map->pi_ioreq, eff_state);
	return M0_RC(rc);
}


static void mark_page_as_read_failed(struct pargrp_iomap *map, uint32_t row,
				     uint32_t col, enum page_attr page_type)
{
	struct m0_pdclust_layout  *play;
	struct m0_pdclust_src_addr src;
	enum m0_pool_nd_state      state;
	uint32_t                   spare_slot;
	uint32_t                   spare_prev;
	int                        rc;

	M0_ENTRY("[%p] pid=%llu, row = %u, col=%u, type=0x%x",
		 map->pi_ioreq, map->pi_grpid, row, col, page_type);
	M0_PRE(M0_IN(page_type,(PA_DATA, PA_PARITY)));
	M0_PRE(ergo(page_type == PA_DATA, map->pi_databufs[row][col] != NULL));
	M0_PRE(ergo(page_type == PA_PARITY,
		    map->pi_paritybufs[row][col] != NULL));

	play = pdlayout_get(map->pi_ioreq);
	src.sa_group = map->pi_grpid;
	if (page_type == PA_DATA)
		src.sa_unit = col;
	else
		src.sa_unit = col + layout_n(play);

	rc = unit_state(&src, map->pi_ioreq, &state);
	M0_ASSERT(rc == 0);
	if (state == M0_PNDS_SNS_REPAIRED) {
		/* gets the state of corresponding spare unit */
		rc = io_spare_map(map, &src, &spare_slot, &spare_prev,
				  &state);
		M0_ASSERT(rc == 0);
	}
	/*
	 * Checking state M0_PNDS_SNS_REBALANCING allows concurrent read during
	 * sns rebalancing in oostore mode. This works similarly to
	 * M0_PNDS_FAILED.
	 * To handle concurrent i/o in non-oostore mode, some more changes are
	 * required to write data to live unit (on earlier failed device) if the
	 * device state is M0_PNDS_SNS_REBALANCING.
	 */
	if (M0_IN(state, (M0_PNDS_FAILED, M0_PNDS_OFFLINE,
			  M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REBALANCING))) {
		if (page_type == PA_DATA)
			map->pi_databufs[row][col]->db_flags |=
				PA_READ_FAILED;
		else
			map->pi_paritybufs[row][col]->db_flags |=
				PA_READ_FAILED;
	}
	M0_LEAVE();
}

/**
 * @param map the failed map.
 * @param tio the failed target io request.
 * @param index target offset array.
 * @param count the array length of the above array.
 */
static int pargrp_iomap_dgmode_process(struct pargrp_iomap *map,
				       struct target_ioreq *tio,
				       m0_bindex_t         *index,
				       uint32_t             count)
{
	int                        rc = 0;
	uint32_t                   row;
	uint32_t                   col;
	m0_bindex_t                goff;
	struct m0_pdclust_layout  *play;
	struct m0_pdclust_src_addr src;
	enum m0_pool_nd_state      dev_state;
	uint32_t                   spare_slot;
	uint32_t                   spare_slot_prev;
	struct m0_poolmach        *pm;
	struct io_request         *req;

	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_ENTRY("[%p] grpid = %llu, count = %u\n",
		 map->pi_ioreq, map->pi_grpid, count);
	M0_PRE(tio   != NULL);
	M0_PRE(index != NULL);
	M0_PRE(count >  0);

	req = map->pi_ioreq;
	pm = m0t1fs_file_to_poolmach(map->pi_ioreq->ir_file);
	M0_ASSERT(pm != NULL);
	rc = m0_poolmach_device_state(pm, tio->ti_obj, &dev_state);
	play = pdlayout_get(req);
	pargrp_src_addr(index[0], req, tio, &src);
	M0_ASSERT(src.sa_group == map->pi_grpid);
	M0_ASSERT(src.sa_unit  <  layout_n(play) + layout_k(play));
	M0_LOG(M0_DEBUG, "[%p] src=[%llu:%llu] device state=%d",
			 map->pi_ioreq, src.sa_group, src.sa_unit, dev_state);
	if (dev_state == M0_PNDS_SNS_REPAIRED) {
		rc = io_spare_map(map, &src, &spare_slot, &spare_slot_prev,
				  &dev_state);
		M0_ASSERT(rc == 0);
		M0_LOG(M0_DEBUG, "[%p] spare=[%u] spare_prev=[%u] state=%d",
				 map->pi_ioreq, spare_slot,
				 spare_slot_prev, dev_state);
		if (dev_state == M0_PNDS_SNS_REPAIRED) {
			M0_LOG(M0_DEBUG, "reading from spare");
			return M0_RC(0);
		}
	}
	map->pi_state = PI_DEGRADED;
	++req->ir_dgmap_nr;
	/* Failed segment belongs to a data unit. */
	if (src.sa_unit < layout_n(play)) {
		goff = gfile_offset(index[0], map, play, &src);
		page_pos_get(map, goff, &row, &col);
		M0_ASSERT(map->pi_databufs[row][col] != NULL);
		map->pi_databufs[row][col]->db_flags |= PA_READ_FAILED;
	} else {
		/* Failed segment belongs to a parity unit. */
		row = page_nr(index[0]) % page_nr(layout_unit_size(play));
		col = src.sa_unit - layout_n(play);
		M0_ASSERT(map->pi_paritybufs[row][col] != NULL);
		map->pi_paritybufs[row][col]->db_flags |= PA_READ_FAILED;
	}
	/*
	 * Since m0_parity_math_recover() API will recover one or more
	 * _whole_ units, all pages from a failed unit can be marked as
	 * PA_READ_FAILED. These pages need not be read again.
	 */
	rc = pargrp_iomap_pages_mark_as_failed(map, M0_PUT_DATA);
	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] Failed to mark pages from parity "
				   "group", req);

	/*
	 * If parity buffers are not allocated, they should be allocated
	 * since they are needed for recovering lost data.
	 */
	if (map->pi_paritybufs == NULL) {
		M0_ALLOC_ARR(map->pi_paritybufs, parity_row_nr(play));
		if (map->pi_paritybufs == NULL)
			return M0_ERR_INFO(-ENOMEM, "[%p] Failed to allocate "
					   "parity buffers", req);

		for (row = 0; row < parity_row_nr(play); ++row) {
			M0_ALLOC_ARR(map->pi_paritybufs[row],
				     parity_col_nr(play));
			if (map->pi_paritybufs[row] == NULL) {
				rc = M0_ERR(-ENOMEM);
				goto par_fail;
			}
		}
	}
	rc = pargrp_iomap_pages_mark_as_failed(map, M0_PUT_PARITY);
	return M0_RC(rc);

par_fail:
	M0_ASSERT(rc != 0);
	for (row = 0; row < parity_row_nr(play); ++row)
		m0_free0(&map->pi_paritybufs[row]);
	m0_free0(&map->pi_paritybufs);

	return M0_ERR_INFO(rc, "[%p] dgmode_process failed", req);
}

static int pargrp_iomap_dgmode_postprocess(struct pargrp_iomap *map)
{
	int                       rc = 0;
	bool                      within_eof;
	uint32_t                  row;
	uint32_t                  col;
	m0_bindex_t               start;
	struct inode             *inode;
	struct data_buf          *dbuf;
	struct m0_pdclust_layout *play;
	struct m0t1fs_sb         *csb;
	struct io_request        *req;

	M0_PRE_EX(pargrp_iomap_invariant(map));

	/*
	 * read_old: Reads unavailable data subject to condition that
	 *           data lies within file size. Parity is already read.
	 * read_rest: Reads parity units. Data for parity group is already
	 *            read.
	 * simple_read: Reads unavailable data subject to condition that
	 *              data lies within file size. Parity also has to be read.
	 */

	req = map->pi_ioreq;
	M0_ENTRY("[%p] parity group id %llu, map state = %d",
		 req, map->pi_grpid, map->pi_state);

	inode = iomap_to_inode(map);
	play = pdlayout_get(req);

	/*
	 * Data matrix from parity group.
	 * The loop traverses column by column to be in sync with
	 * increasing file offset.
	 * This is necessary in order to generate correct index vector.
	 */
	for (col = 0; col < data_col_nr(play); ++col) {
		for (row = 0; row < data_row_nr(play); ++row) {

			start = data_page_offset_get(map, row, col);
			within_eof = start + PAGE_SIZE < inode->i_size ||
				     (inode->i_size > 0 &&
				      page_id(start + PAGE_SIZE - 1) ==
				      page_id(inode->i_size - 1));
			if (map->pi_databufs[row][col] != NULL) {
				if (map->pi_databufs[row][col]->db_flags &
				    PA_READ_FAILED)
					continue;
			} else {
				/*
				 * If current parity group map is degraded,
				 * then recovery is needed and a new
				 * data buffer needs to be allocated subject to
				 * limitation of file size.
				 */
				if (map->pi_state == PI_DEGRADED &&
				    within_eof) {
					map->pi_databufs[row][col] =
						data_buf_alloc_init(0);
					if (map->pi_databufs[row][col] ==
					    NULL) {
						rc = M0_ERR(-ENOMEM);
						break;
					}
					mark_page_as_read_failed(map, row, col,
								 PA_DATA);
				}
				if (map->pi_state == PI_HEALTHY)
					continue;
			}
			dbuf = map->pi_databufs[row][col];
			/*
			 * Marks only those data buffers which lie within EOF.
			 * Since all IO fops receive error
			 * once sns repair starts (M0_PNDS_SNS_REPAIRING state)
			 * read is not done for any of these fops.
			 * Hence all pages other than the one which encountered
			 * failure (PA_READ_FAILED flag set) are read in
			 * degraded mode.
			 */
			if (within_eof) {
				if (dbuf->db_flags & PA_READ_FAILED ||
				    is_page_read(dbuf)) {
					continue;
				}
				dbuf->db_flags |= PA_DGMODE_READ;
			}
		}
	}

	if (rc != 0)
		goto err;

	csb = M0T1FS_SB(inode->i_sb);
	/* If parity group is healthy, there is no need to read parity. */
	if (map->pi_state != PI_DEGRADED && !csb->csb_verify)
		return M0_RC(0);

	/*
	 * Populates the index vector if original read IO request did not
	 * span it. Since recovery is needed using parity algorithms,
	 * whole parity group needs to be read subject to file size limitation.
	 * Ergo, parity group index vector contains only one segment
	 * worth the parity group in size.
	 */
	V_INDEX(&map->pi_ivv, 0) = map->pi_grpid * data_size(play);
	V_COUNT(&map->pi_ivv, 0) = min64u(V_INDEX(&map->pi_ivv, 0) +
					 data_size(play),
					 inode->i_size) -
				  V_INDEX(&map->pi_ivv, 0);
	/*
	 * m0_0vec requires all members except the last one to have data count
	 * multiple of 4K.
	 */
	V_COUNT(&map->pi_ivv, 0) = round_up(
						V_COUNT(&map->pi_ivv, 0),
						PAGE_SIZE);
	V_SEG_NR(&map->pi_ivv)   = 1;
	indexvec_varr_dump(&map->pi_ivv);
	/* parity matrix from parity group. */
	for (row = 0; row < parity_row_nr(play); ++row) {
		for (col = 0; col < parity_col_nr(play); ++col) {

			if (map->pi_paritybufs[row][col] == NULL) {
				map->pi_paritybufs[row][col] =
					data_buf_alloc_init(0);
				if (map->pi_paritybufs[row][col] == NULL) {
					rc = M0_ERR(-ENOMEM);
					break;
				}
			}
			dbuf = map->pi_paritybufs[row][col];
			mark_page_as_read_failed(map, row, col, PA_PARITY);
			/* Skips the page if it is marked as PA_READ_FAILED. */
			if (dbuf->db_flags & PA_READ_FAILED ||
			    is_page_read(dbuf)) {
				continue;
			}
			dbuf->db_flags |= PA_DGMODE_READ;
		}
	}
	if (rc != 0)
		goto err;
	return M0_RC(rc);
err:
	return M0_ERR_INFO(rc,"[%p] %s", req,
			   rc == -ENOMEM ? "Failed to allocate "
			   "data buffer": "Illegal device queried for status");
}

static uint32_t iomap_dgmode_recov_prepare(struct pargrp_iomap *map,
					   uint8_t *failed)
{
	struct m0_pdclust_layout *play;
	uint32_t                  col;
	uint32_t                  K = 0;

	play = pdlayout_get(map->pi_ioreq);
	for (col = 0; col < data_col_nr(play); ++col) {
		if (map->pi_databufs[0][col] != NULL &&
		    map->pi_databufs[0][col]->db_flags &
		    PA_READ_FAILED) {
			failed[col] = 1;
			++K;
		}

	}
	for (col = 0; col < parity_col_nr(play); ++col) {
		M0_ASSERT(map->pi_paritybufs[0][col] != NULL);
		if (map->pi_paritybufs[0][col]->db_flags &
		    PA_READ_FAILED) {
			failed[col + layout_n(play)] = 1;
			++K;
		}
	}
	return K;
}

static int pargrp_iomap_dgmode_recover(struct pargrp_iomap *map)
{
	int                       rc = 0;
	uint32_t                  row;
	uint32_t                  col;
	uint32_t                  K;
	unsigned long             zpage;
	struct m0_buf            *data;
	struct m0_buf            *parity;
	struct m0_buf             failed;
	struct m0_pdclust_layout *play;

	M0_PRE_EX(pargrp_iomap_invariant(map));
	M0_PRE(map->pi_state == PI_DEGRADED);

	M0_ENTRY("[%p] map %p", map->pi_ioreq, map);

	play = pdlayout_get(map->pi_ioreq);
	M0_ALLOC_ARR(data, layout_n(play));
	if (data == NULL)
		return M0_ERR_INFO(-ENOMEM, "[%p] Failed to allocate memory"
				   " for data buf", map->pi_ioreq);

	M0_ALLOC_ARR(parity, layout_k(play));
	if (parity == NULL) {
		m0_free(data);
		return M0_ERR_INFO(-ENOMEM, "[%p] Failed to allocate memory"
				   " for parity buf", map->pi_ioreq);
	}

	zpage = get_zeroed_page(GFP_KERNEL);
	if (zpage == 0) {
		m0_free(data);
		m0_free(parity);
		return M0_ERR_INFO(-ENOMEM, "[%p] Failed to allocate page.",
				   map->pi_ioreq);
	}

	failed.b_nob = layout_n(play) + layout_k(play);
	failed.b_addr = m0_alloc(failed.b_nob);
	if (failed.b_addr == NULL) {
		m0_free(data);
		m0_free(parity);
		free_page(zpage);
		return M0_ERR_INFO(-ENOMEM, "[%p] Failed to allocate memory "
				   "for m0_buf", map->pi_ioreq);
	}
	K = iomap_dgmode_recov_prepare(map, (uint8_t *)failed.b_addr);
	if (K > layout_k(play)) {
		M0_LOG(M0_ERROR, "More failures in group %d",
				 (int)map->pi_grpid);
		rc = -EIO;
		goto end;
	}
	if (parity_math(map->pi_ioreq)->pmi_parity_algo ==
	    M0_PARITY_CAL_ALGO_REED_SOLOMON) {
		rc = m0_parity_recov_mat_gen(parity_math(map->pi_ioreq),
				(uint8_t *)failed.b_addr);
		if (rc != 0)
			goto end;
	}
	/* Populates data and failed buffers. */
	for (row = 0; row < data_row_nr(play); ++row) {
		for (col = 0; col < data_col_nr(play); ++col) {
			data[col].b_nob = PAGE_SIZE;
			if (map->pi_databufs[row][col] == NULL) {
				data[col].b_addr = (void *)zpage;
				continue;
			}
			data[col].b_addr = map->pi_databufs[row][col]->
					   db_buf.b_addr;
		}
		for (col = 0; col < parity_col_nr(play); ++col) {
			M0_ASSERT(map->pi_paritybufs[row][col] != NULL);
			parity[col].b_addr = map->pi_paritybufs[row][col]->
				db_buf.b_addr;
			parity[col].b_nob  = PAGE_SIZE;
		}
		m0_parity_math_recover(parity_math(map->pi_ioreq), data,
				       parity, &failed, M0_LA_INVERSE);
	}

	if (parity_math(map->pi_ioreq)->pmi_parity_algo ==
	    M0_PARITY_CAL_ALGO_REED_SOLOMON)
		m0_parity_recov_mat_destroy(parity_math(map->pi_ioreq));
end:
	m0_free(data);
	m0_free(parity);
	m0_free(failed.b_addr);
	free_page(zpage);
	return rc == 0 ? M0_RC(0) : M0_ERR_INFO(rc, "Number of failed units"
						"in parity group exceeds the"
						"total number of parity units"
						"in a parity group %d.",
						(int)map->pi_grpid);
}

static int ioreq_iomaps_parity_groups_cal(struct io_request *req)
{
	uint64_t                  seg;
	uint64_t                  grp;
	uint64_t                  grpstart;
	uint64_t                  grpend;
	uint64_t                 *grparray;
	uint64_t                  grparray_sz;
	struct m0_pdclust_layout *play;

	M0_ENTRY("[%p]", req);

	play = pdlayout_get(req);

	/* Array of maximum possible number of groups spanned by req. */
	grparray_sz = indexvec_varr_count(&req->ir_ivv) / data_size(play) +
		      2 * V_SEG_NR(&req->ir_ivv);
	M0_LOG(M0_DEBUG, "[%p] arr_sz=%llu", req, grparray_sz);
	M0_ALLOC_ARR(grparray, grparray_sz);
	if (grparray == NULL)
		return M0_ERR_INFO(-ENOMEM, "[%p] Failed to allocate memory"
				   " for int array", req);
	/*
	 * Finds out total number of parity groups spanned by
	 * io_request::ir_ivec.
	 */
	for (seg = 0; seg < V_SEG_NR(&req->ir_ivv); ++seg) {
		grpstart = group_id(V_INDEX(&req->ir_ivv, seg),
				    data_size(play));
		grpend = group_id(v_seg_endpos(&req->ir_ivv, seg) - 1,
				    data_size(play));
		for (grp = grpstart; grp <= grpend; ++grp) {
			uint64_t i;
			/*
			 * grparray is a temporary array to record found groups.
			 * Scan this array for [grpstart, grpend].
			 * If not found, record it in this array and
			 * increase ir_iomap_nr.
			 */
			for (i = 0; i < req->ir_iomap_nr; ++i) {
				if (grparray[i] == grp)
					break;
			}
			/* 'grp' is not found. Adding it to @grparray */
			if (i == req->ir_iomap_nr) {
				M0_ASSERT_INFO(i < grparray_sz,
					"[%p] nr=%llu size=%llu",
					 req, i , grparray_sz);
				grparray[i] = grp;
				++req->ir_iomap_nr;
			}
		}
	}
	m0_free(grparray);
	return M0_RC(0);
}

static int ioreq_iomaps_prepare(struct io_request *req)
{
	int                         rc;
	uint64_t                    map;
	struct m0_ivec_varr_cursor  cursor;
	struct m0_pdclust_layout   *play;

	M0_PRE(req != NULL);

	M0_ENTRY("[%p]", req);
	play = pdlayout_get(req);

	rc = ioreq_iomaps_parity_groups_cal(req);
	if (rc != 0)
		return M0_RC(rc);

	M0_LOG(M0_DEBUG, "[%p] spanned_groups=%llu [N,K,us]=[%d,%d,%llu]",
			 req, req->ir_iomap_nr, layout_n(play),
			 layout_k(play), layout_unit_size(play));

	/* req->ir_iomaps is zeroed out on allocation. */
	M0_ALLOC_ARR(req->ir_iomaps, req->ir_iomap_nr);
	if (req->ir_iomaps == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto failed;
	}

	m0_ivec_varr_cursor_init(&cursor, &req->ir_ivv);

	/*
	 * cursor is advanced maximum by parity group size in one iteration
	 * of this loop.
	 * This is done by pargrp_iomap::pi_ops::pi_populate().
	 */
	for (map = 0; !m0_ivec_varr_cursor_move(&cursor, 0); ++map) {
		M0_ASSERT(map < req->ir_iomap_nr);
		M0_ASSERT(req->ir_iomaps[map] == NULL);
		M0_ALLOC_PTR(req->ir_iomaps[map]);
		if (req->ir_iomaps[map] == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto failed;
		}

		++iommstats.a_pargrp_iomap_nr;
		rc = pargrp_iomap_init(req->ir_iomaps[map], req,
				    group_id(m0_ivec_varr_cursor_index(&cursor),
				    data_size(play)));
		if (rc != 0) {
			m0_free0(&req->ir_iomaps[map]);
			goto failed;
		}

		/* @cursor is advanced in the following function */
		rc = req->ir_iomaps[map]->pi_ops->pi_populate(req->
				ir_iomaps[map], &req->ir_ivv, &cursor);
		if (rc != 0)
			goto failed;
		M0_LOG(M0_INFO, "[%p] pargrp_iomap id : %llu populated",
		       req, req->ir_iomaps[map]->pi_grpid);
	}
	return M0_RC(0);
failed:
	if (req->ir_iomaps != NULL)
		req->ir_ops->iro_iomaps_destroy(req);

	return M0_ERR_INFO(rc, "[%p] iomaps_prepare failed", req);
}

static void ioreq_iomaps_destroy(struct io_request *req)
{
	uint64_t id;

	M0_ENTRY("[%p]", req);

	M0_PRE(req != NULL);
	M0_PRE(req->ir_iomaps != NULL);

	for (id = 0; id < req->ir_iomap_nr; ++id) {
		if (req->ir_iomaps[id] != NULL) {
			pargrp_iomap_fini(req->ir_iomaps[id]);
			m0_free(req->ir_iomaps[id]);
			++iommstats.d_pargrp_iomap_nr;
		}
	}
	m0_free0(&req->ir_iomaps);
	req->ir_iomap_nr = 0;
}

static int dgmode_rwvec_alloc_init(struct target_ioreq *ti)
{
	int                       rc;
	uint64_t                  cnt;
	struct io_request        *req;
	struct dgmode_rwvec      *dg;
	struct m0_pdclust_layout *play;

	M0_ENTRY();
	M0_PRE(ti           != NULL);
	M0_PRE(ti->ti_dgvec == NULL);

	req = bob_of(ti->ti_nwxfer, struct io_request, ir_nwxfer,
		     &ioreq_bobtype);
	play = pdlayout_get(req);
	cnt = page_nr(req->ir_iomap_nr * layout_unit_size(play) *
		      (layout_n(play) + layout_k(play)));
	M0_LOG(M0_DEBUG, "[%p]", req);

	M0_ALLOC_PTR(dg);
	if (dg == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto failed;
	}

	dg->dr_tioreq = ti;

	rc = m0_indexvec_varr_alloc(&dg->dr_ivec_varr, cnt);
	if (rc != 0)
		goto failed_free_dg;

	rc = m0_indexvec_varr_alloc(&dg->dr_bufvec, cnt);
	if (rc != 0)
		goto failed_free_iv;

	rc = m0_varr_init(&dg->dr_pageattrs, cnt, sizeof(enum page_attr),
			  (size_t)m0_pagesize_get());
	if (rc != 0)
		goto failed_free_bv;

	/*
	 * This value is incremented every time a new segment is added
	 * to this index vector.
	 */
	V_SEG_NR(&dg->dr_ivec_varr) = 0;

	ti->ti_dgvec = dg;
	return M0_RC(0);

failed_free_bv:
	m0_indexvec_varr_free(&dg->dr_bufvec);
failed_free_iv:
	m0_indexvec_varr_free(&dg->dr_ivec_varr);
failed_free_dg:
	m0_free(dg);
failed:
	return M0_ERR_INFO(rc, "[%p] Dgmode read vector allocation failed",
			   req);
}

static void dgmode_rwvec_dealloc_fini(struct dgmode_rwvec *dg)
{
	M0_ENTRY();

	M0_PRE(dg != NULL);

	dg->dr_tioreq = NULL;
	m0_indexvec_varr_free(&dg->dr_ivec_varr);
	m0_indexvec_varr_free(&dg->dr_bufvec);
	m0_varr_fini(&dg->dr_pageattrs);
}

/*
 * Distributes file data into target_ioreq objects as required and populates
 * target_ioreq::ti_ivv and target_ioreq::ti_bufvec.
 */
static int nw_xfer_io_distribute(struct nw_xfer_request *xfer)
{
	int                         rc;
	uint64_t                    map;
	uint64_t                    unit;
	uint64_t                    unit_size;
	uint64_t                    count;
	uint64_t                    pgstart;
	uint64_t                    pgend;
	/* Extent representing a data unit. */
	struct m0_ext               u_ext;
	/* Extent representing resultant extent. */
	struct m0_ext               r_ext;
	/* Extent representing a segment from index vector. */
	struct m0_ext               v_ext;
	struct io_request          *req;
	struct target_ioreq        *ti;
	struct m0_ivec_varr_cursor  cur;
	struct m0_pdclust_layout   *play;
	enum m0_pdclust_unit_type   unit_type;
	struct m0_pdclust_src_addr  src;
	struct m0_pdclust_tgt_addr  tgt;
	uint32_t                    row_start;
	uint32_t                    row_end;
	uint32_t                    row;
	uint32_t                    col;
	struct data_buf            *dbuf;
	struct pargrp_iomap        *iomap;
	struct inode               *inode;
	struct m0t1fs_sb           *csb;

	M0_ENTRY("nw_xfer_request %p", xfer);
	M0_PRE_EX(nw_xfer_request_invariant(xfer));

	req       = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);
	play      = pdlayout_get(req);
	unit_size = layout_unit_size(play);

	M0_LOG(M0_DEBUG, "[%p]", req);

	for (map = 0; map < req->ir_iomap_nr; ++map) {
		count        = 0;
		iomap        = req->ir_iomaps[map];
		pgstart      = data_size(play) * iomap->pi_grpid;
		pgend        = pgstart + data_size(play);
		src.sa_group = iomap->pi_grpid;

		M0_LOG(M0_DEBUG, "[%p] map %p [grpid = %llu state=%u]",
		       req, iomap, iomap->pi_grpid, iomap->pi_state);

		/* Cursor for pargrp_iomap::pi_ivv. */
		m0_ivec_varr_cursor_init(&cur, &iomap->pi_ivv);

		while (!m0_ivec_varr_cursor_move(&cur, count)) {

			unit = (m0_ivec_varr_cursor_index(&cur) - pgstart) /
			       unit_size;

			u_ext.e_start = pgstart + unit * unit_size;
			u_ext.e_end   = u_ext.e_start + unit_size;
			m0_ext_init(&u_ext);

			v_ext.e_start  = m0_ivec_varr_cursor_index(&cur);
			v_ext.e_end    = v_ext.e_start +
					  m0_ivec_varr_cursor_step(&cur);
			m0_ext_init(&v_ext);

			m0_ext_intersection(&u_ext, &v_ext, &r_ext);
			if (!m0_ext_is_valid(&r_ext)) {
				count = unit_size;
				continue;
			}

			count     = m0_ext_length(&r_ext);
			unit_type = m0_pdclust_unit_classify(play, unit);
			if (unit_type == M0_PUT_SPARE ||
			    unit_type == M0_PUT_PARITY)
				continue;
			if (ioreq_sm_state(req) == IRS_DEGRADED_WRITING) {
				page_pos_get(iomap, r_ext.e_start, &row_start,
						&col);
				page_pos_get(iomap, r_ext.e_end - 1, &row_end,
						&col);
				dbuf = iomap->pi_databufs[row_start][col];
				M0_ASSERT(dbuf != NULL);
				for (row = row_start; row <= row_end; ++row) {
					dbuf = iomap->pi_databufs[row][col];
					M0_ASSERT(dbuf != NULL);
					if (dbuf->db_flags & PA_WRITE)
						dbuf->db_flags |=
							PA_DGMODE_WRITE;
				}
			}
			src.sa_unit = unit;
			rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src, &tgt,
							   &ti);
			if (rc != 0) {
				M0_LOG(M0_DEBUG, "[%p] map %p, "
				       "nxo_tioreq_map() failed, rc %d",
				       req, iomap, rc);
				goto err;
			}

			M0_LOG(M0_DEBUG, "[%p] adding data. ti state=%d\n",
			       req, ti->ti_state);
			ti->ti_ops->tio_seg_add(ti, &src, &tgt, r_ext.e_start,
						m0_ext_length(&r_ext),
						iomap);
		}

		inode = iomap_to_inode(iomap);
		csb = M0T1FS_SB(inode->i_sb);

		if (req->ir_type == IRT_WRITE ||
		    (req->ir_type == IRT_READ && csb->csb_verify) ||
		    (ioreq_sm_state(req) == IRS_DEGRADED_READING &&
		     iomap->pi_state == PI_DEGRADED)) {

			for (unit = 0; unit < layout_k(play); ++unit) {

				src.sa_unit = layout_n(play) + unit;

				M0_ASSERT(m0_pdclust_unit_classify(play,
					  src.sa_unit) == M0_PUT_PARITY);

				rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src,
								   &tgt, &ti);
				if (rc != 0) {
					M0_LOG(M0_DEBUG, "[%p] map %p, "
					       "nxo_tioreq_map() failed, rc %d",
					       req, iomap, rc);
					goto err;
				}

				parity_page_pos_get(iomap, unit * unit_size,
						    &row, &col);
				for (; row < parity_row_nr(play); ++row) {
					dbuf = iomap->pi_paritybufs[row][col];
					M0_ASSERT(dbuf != NULL);
					M0_ASSERT(ergo(req->ir_type ==
						       IRT_WRITE,
						       dbuf->db_flags &
						       PA_WRITE));
					if (ioreq_sm_state(req) ==
					    IRS_DEGRADED_WRITING &&
					    dbuf->db_flags & PA_WRITE)
						dbuf->db_flags |=
							PA_DGMODE_WRITE;
				}
				ti->ti_ops->tio_seg_add(ti, &src, &tgt, pgstart,
							layout_unit_size(play),
							iomap);
			}
			if (!csb->csb_oostore || req->ir_type != IRT_WRITE)
				continue;
			/* Cob create for spares. */
			for (unit = layout_k(play); unit < 2 * layout_k(play);
			     ++unit) {
				src.sa_unit = layout_n(play) + unit;
				rc = xfer->nxr_ops->nxo_tioreq_map(xfer, &src,
								   &tgt, &ti);
				if (rc != 0) {
					M0_LOG(M0_ERROR, "[%p] map %p,"
					       "nxo_tioreq_map() failed, rc %d"
						,req, iomap, rc);
				}
				if (target_ioreq_type_get(ti) != TI_NONE)
					continue;
				target_ioreq_type_set(ti, TI_COB_CREATE);
			}
		}
	}

	return M0_RC(0);
err:
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		tioreqht_htable_del(&xfer->nxr_tioreqs_hash, ti);
		M0_LOG(M0_INFO, "[%p] target_ioreq deleted for "FID_F,
		       req, FID_P(&ti->ti_fid));
		target_ioreq_fini(ti);
		m0_free0(&ti);
		++iommstats.d_target_ioreq_nr;
	} m0_htable_endfor;

	return M0_ERR_INFO(rc, "[%p] io_prepare failed", req);
}

static inline int ioreq_sm_timedwait(struct io_request *req,
				     uint64_t           state)
{
	int rc;
	M0_PRE(req != NULL);

	M0_ENTRY("[%p] Waiting for %s -> %s, Pending fops %llu, "
		 "Pending rdbulk %llu", req,
		 io_states[ioreq_sm_state(req)].sd_name,
		 io_states[state].sd_name,
		 m0_atomic64_get(&req->ir_nwxfer.nxr_iofop_nr),
		 m0_atomic64_get(&req->ir_nwxfer.nxr_rdbulk_nr));

	m0_sm_group_lock(req->ir_sm.sm_grp);
	rc = m0_sm_timedwait(&req->ir_sm, M0_BITS(state, IRS_FAILED),
			     M0_TIME_NEVER);
	m0_sm_group_unlock(req->ir_sm.sm_grp);

	if (rc != 0)
		M0_LOG(M0_DEBUG, "[%p] rc %d", req, rc);
	M0_LEAVE("[%p] rc %d", req, rc);
	return rc;
}

static int ioreq_dgmode_recover(struct io_request *req)
{
	int      rc = 0;
	uint64_t cnt;

	M0_ENTRY("[%p]", req);
	M0_PRE_EX(io_request_invariant(req));
	M0_PRE(ioreq_sm_state(req) == IRS_READ_COMPLETE);

	for (cnt = 0; cnt < req->ir_iomap_nr; ++cnt) {
		if (req->ir_iomaps[cnt]->pi_state == PI_DEGRADED) {
			rc = req->ir_iomaps[cnt]->pi_ops->
				pi_dgmode_recover(req->ir_iomaps[cnt]);
			if (rc != 0)
				return M0_ERR_INFO(rc, "[%p] Failed to recover"
						   " data", req);
		}
	}

	return M0_RC(rc);
}

/**
 * @todo This code is not required once MERO-899 lands into master.
 * Tolerance for the given level.
 */
static uint64_t tolerance_of_level(struct io_request *req, uint64_t lv)
{
	struct m0_pdclust_instance *play_instance;
	struct m0_pool_version     *pver;

	M0_PRE(lv < M0_CONF_PVER_HEIGHT);

	play_instance = pdlayout_instance(layout_instance(req));
	pver = play_instance->pi_base.li_l->l_pver;
	return pver->pv_fd_tol_vec[lv];
}

/**
 * @todo  This code is not required once MERO-899 lands into master.
 * Returns true if a given session is already marked as failed. In case
 * a session is not already marked for failure, the functions marks it
 * and returns false.
 */
static bool is_session_marked(struct io_request *req,
			      struct m0_rpc_session *session)
{
	uint64_t i;
	uint64_t max_failures;
	uint64_t session_id;

	session_id = session->s_session_id;
	max_failures = tolerance_of_level(req, M0_CONF_PVER_LVL_CTRLS);
	for (i = 0; i < max_failures; ++i) {
		if (req->ir_failed_session[i] == session_id)
			return true;
		else if (req->ir_failed_session[i] == ~(uint64_t)0) {
			req->ir_failed_session[i] = session_id;
			return false;
		}
	}
	return false;
}

/**
 * Returns number of failed devices or -EIO if number of failed devices exceeds
 * the value of K (number of spare devices in parity group). Once MERO-899 lands
 * into master the code for this function will change. In that case it will only
 * check if a given pool is dud.
 */
static int device_check(struct io_request *req)
{
	int                       rc = 0;
	uint32_t                  fdev_nr = 0;
	uint32_t                  fsvc_nr = 0;
	struct target_ioreq      *ti;
	struct m0_pdclust_layout *play;
	enum m0_pool_nd_state     state;
	uint64_t                  max_failures;
	struct m0_poolmach       *pm = m0t1fs_file_to_poolmach(req->ir_file);

	max_failures = tolerance_of_level(req, M0_CONF_PVER_LVL_CTRLS);

	M0_ENTRY("[%p]", req);
	M0_PRE(req != NULL);
	M0_PRE(M0_IN(ioreq_sm_state(req), (IRS_READ_COMPLETE,
					   IRS_WRITE_COMPLETE)));
	play = pdlayout_get(req);
	m0_htable_for (tioreqht, ti, &req->ir_nwxfer.nxr_tioreqs_hash) {
		rc = m0_poolmach_device_state(pm, ti->ti_obj, &state);
		if (rc != 0)
			return M0_ERR_INFO(rc, "[%p] Failed to retrieve target "
					   "device state", req);
		/* The case when a particular service is down. */
		if (ti->ti_rc == -ECANCELED) {
			if (!is_session_marked(req, ti->ti_session)) {
				M0_CNT_INC(fsvc_nr);
			}
		/* The case when multiple devices under the same service are
		 * unavailable. */
		} else if (M0_IN(state, (M0_PNDS_FAILED, M0_PNDS_OFFLINE,
			   M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REPAIRED)) &&
			   !is_session_marked(req, ti->ti_session)) {
			M0_CNT_INC(fdev_nr);
		}
	} m0_htable_endfor;
	M0_LOG(M0_DEBUG, "failed devices = %d\ttolerance=%d", (int)fdev_nr,
			(int)layout_k(play));
	if (is_pver_dud(fdev_nr, layout_k(play), fsvc_nr, max_failures))
		return M0_ERR_INFO(-EIO, "[%p] Failed to recover data "
				"since number of failed data units "
				"(%lu) exceeds number of parity "
				"units in parity group (%lu) OR "
				"number of failed services (%lu) "
				"exceeds number of max failures "
				"supported (%lu)",
				req, (unsigned long)fdev_nr,
				(unsigned long)layout_k(play),
				(unsigned long)fsvc_nr,
				(unsigned long)max_failures);
	return M0_RC(fdev_nr);
}

/* If there are F(l) failures at level l, and K(l) failures are tolerable for
 * the level l, then the condition for pool-version to be non-dud is:
 *			\sum_over_l {F(l) / K(l)} <= 1
 * Once MERO-899 lands into master, this function will go away.
 */
static bool is_pver_dud(uint32_t fdev_nr, uint32_t dev_k, uint32_t fsvc_nr,
			uint32_t svc_k)
{
	if (fdev_nr > 0 && dev_k == 0)
		return true;
	if (fsvc_nr > 0 && svc_k == 0)
		return true;
	return (svc_k + fsvc_nr > 0) ?
		(fdev_nr * svc_k + fsvc_nr * dev_k) > dev_k * svc_k :
		fdev_nr > dev_k;
}

static int ioreq_dgmode_write(struct io_request *req, bool rmw)
{
	int                      rc;
	struct target_ioreq     *ti;
	struct m0t1fs_sb        *csb;
	struct nw_xfer_request  *xfer;

	M0_PRE_EX(io_request_invariant(req));

	xfer = &req->ir_nwxfer;
	M0_ENTRY("[%p]", req);
	csb = file_to_sb(req->ir_file);
	/* In oostore mode we do not enter the degraded mode write. */
	if (csb->csb_oostore || M0_IN(xfer->nxr_rc, (0, -E2BIG, -ESTALE)))
		return M0_RC(xfer->nxr_rc);

	rc = device_check(req);
	if (rc < 0 ) {
		return M0_RC(rc);
	}
	ioreq_sm_state_set(req, IRS_DEGRADED_WRITING);
	/*
	 * This IO request has already acquired distributed lock on the
	 * file by this time.
	 * Degraded mode write needs to handle 2 prime use-cases.
	 * 1. SNS repair still to start on associated global fid.
	 * 2. SNS repair has completed for associated global fid.
	 * Both use-cases imply unavailability of one or more devices.
	 *
	 * In first use-case, repair is yet to start on file. Hence,
	 * rest of the file data which goes on healthy devices can be
	 * written safely.
	 * In this case, the fops meant for failed device(s) will be simply
	 * dropped and rest of the fops will be sent to respective ioservice
	 * instances for writing data to servers.
	 * Later when this IO request relinquishes the distributed lock on
	 * associated global fid and SNS repair starts on the file, the lost
	 * data will be regenerated using parity recovery algorithms.
	 *
	 * The second use-case implies completion of SNS repair for associated
	 * global fid and the lost data is regenerated on distributed spare
	 * units.
	 * Ergo, all the file data meant for lost device(s) will be redirected
	 * towards corresponding spare unit(s). Later when SNS rebalance phase
	 * commences, it will migrate the data from spare to a new device, thus
	 * making spare available for recovery again.
	 * In this case, old fops will be discarded and all pages spanned by
	 * IO request will be reshuffled by redirecting pages meant for
	 * failed device(s) to its corresponding spare unit(s).
	 */

	/*
	 * Finalizes current fops which are not valid anymore.
	 * Fops need to be finalized in either case since old network buffers
	 * from IO fops are still enqueued in transfer machine and removal
	 * of these buffers would lead to finalization of rpc bulk object.
	 */
	M0_LOG(M0_ERROR, "[%p] Degraded write:About to nxo_complete()", req);
	xfer->nxr_ops->nxo_complete(xfer, rmw);
	/*
	 * Resets count of data bytes and parity bytes along with
	 * return status.
	 * Fops meant for failed devices are dropped in
	 * nw_xfer_req_dispatch().
	 */
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		ti->ti_databytes = 0;
		ti->ti_parbytes  = 0;
		ti->ti_rc        = 0;
		ti->ti_req_type  = TI_NONE;
	} m0_htable_endfor;

	/*
	 * Redistributes all pages by routing pages for repaired devices
	 * to spare units for each parity group.
	 */
	rc = xfer->nxr_ops->nxo_distribute(xfer);
	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] Failed to redistribute file data "
				   "between target_ioreq objects", req);

	xfer->nxr_rc = 0;
	req->ir_rc = xfer->nxr_rc;

	rc = xfer->nxr_ops->nxo_dispatch(xfer);
	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] Failed to dispatch degraded mode"
				   "write IO fops", req);

	rc = ioreq_sm_timedwait(req, IRS_WRITE_COMPLETE);
	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] Degraded mode write IO failed",
				   req);
	return M0_RC(xfer->nxr_rc);
}

static int ioreq_dgmode_read(struct io_request *req, bool rmw)
{
	int                      rc = 0;
	uint64_t                 id;
	struct io_req_fop       *irfop;
	struct target_ioreq     *ti;
	enum m0_pool_nd_state    state;
	struct m0_poolmach      *pm;
	struct nw_xfer_request  *xfer;
	struct m0t1fs_sb       *csb;


	M0_PRE_EX(io_request_invariant(req));

	csb = M0T1FS_SB(m0t1fs_file_to_inode(req->ir_file)->i_sb);
	xfer = &req->ir_nwxfer;
	M0_ENTRY("[%p] xfer->nxr_rc %d", req, xfer->nxr_rc);

	/*
	 * If all devices are ONLINE, all requests return success.
	 * In case of read before write, due to CROW, COB will not be present,
	 * resulting into ENOENT error. When conf cache is drained io should
	 * not proceed.
	 */
	if (M0_IN(xfer->nxr_rc, (0, -ENOENT, -ESTALE)) ||
	   /*
	    * For rmw in oostore case return immediately without
	    * bothering to check if degraded read can be done.
	    * Write IO should be aborted in this case.
	    */
	    (csb->csb_oostore && req->ir_type == IRT_WRITE))
		return M0_RC(xfer->nxr_rc);

	rc = device_check(req);
	/*
	 * Number of failed devices is not a criteria good enough
	 * by itself. Even if one/more devices failed but IO request
	 * could complete if IO request did not send any pages to
	 * failed device(s) at all.
	 */
	if (rc < 0)
		return M0_RC(rc);
	M0_LOG(M0_DEBUG, "[%p] Proceeding with the degraded read", req);
	pm = m0t1fs_file_to_poolmach(req->ir_file);
	M0_ASSERT(pm != NULL);
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		/*
		 * Data was retrieved successfully, so no need to check the
		 * state of the device.
		 */
		if (ti->ti_rc == 0)
			continue;
		/* state is already queried in device_check() and stored
		 * in ti->ti_state. Why do we do this again?
		 */
		rc = m0_poolmach_device_state(pm, ti->ti_obj, &state);
		if (rc != 0)
			return M0_ERR_INFO(rc, "[%p] Failed to retrieve device "
					   "state", req);
		M0_LOG(M0_INFO, "[%p] device state for "FID_F" is %d",
		       req, FID_P(&ti->ti_fid), state);
		ti->ti_state = state;
		if (!M0_IN(state, (M0_PNDS_FAILED, M0_PNDS_OFFLINE,
			   M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REPAIRED,
			   M0_PNDS_SNS_REBALANCING)))
			continue;
		/*
		 * Finds out parity groups for which read IO failed and marks
		 * them as DEGRADED. This is necessary since read IO request
		 * could be reading only a part of a parity group but if it
		 * failed, rest of the parity group also needs to be read
		 * (subject to file size) in order to re-generate lost data.
		 */
		m0_tl_for (iofops, &ti->ti_iofops, irfop) {
			rc = io_req_fop_dgmode_read(irfop);
			if (rc != 0)
				break;
		} m0_tl_endfor;
	} m0_htable_endfor;

	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] dgmode failed", req);

	M0_LOG(M0_DEBUG, "[%p] dgmap_nr=%u is in dgmode",
	       req, req->ir_dgmap_nr);
	/*
	 * Starts processing the pages again if any of the parity groups
	 * spanned by input IO-request is in degraded mode.
	 */
	if (req->ir_dgmap_nr > 0) {
		M0_LOG(M0_DEBUG, "[%p] processing the failed parity groups",
				req);
		if (ioreq_sm_state(req) == IRS_READ_COMPLETE)
			ioreq_sm_state_set(req, IRS_DEGRADED_READING);

		for (id = 0; id < req->ir_iomap_nr; ++id) {
			rc = req->ir_iomaps[id]->pi_ops->
				pi_dgmode_postprocess(req->ir_iomaps[id]);
			if (rc != 0)
				break;
		}
	} else {
		M0_ASSERT(ioreq_sm_state(req) == IRS_READ_COMPLETE);
		ioreq_sm_state_set(req, IRS_READING);
		/*
		 * By this time, the page count in target_ioreq::ti_ivec and
		 * target_ioreq::ti_bufvec is greater than 1, but it is
		 * invalid since the distribution is about to change.
		 * Ergo, page counts in index and buffer vectors are reset.
		 */

		m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
			V_SEG_NR(&ti->ti_ivv) = 0;
		} m0_htable_endfor;
	}

	M0_LOG(M0_DEBUG, "[%p] About to nxo_complete()", req);
	xfer->nxr_ops->nxo_complete(xfer, rmw);

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		ti->ti_databytes = 0;
		ti->ti_parbytes  = 0;
		ti->ti_rc        = 0;
	} m0_htable_endfor;

	/* Resets the status code before starting degraded mode read IO. */
	req->ir_rc = xfer->nxr_rc = 0;

	rc = xfer->nxr_ops->nxo_distribute(xfer);
	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] Failed to prepare dgmode IO "
				   "fops.", req);

	rc = xfer->nxr_ops->nxo_dispatch(xfer);
	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] Failed to dispatch degraded mode "
				   "IO.", req);

	rc = ioreq_sm_timedwait(req, IRS_READ_COMPLETE);
	if (rc != 0)
		return M0_ERR_INFO(rc, "[%p] Degraded mode read IO failed.",
				   req);

	if (xfer->nxr_rc != 0)
		return M0_ERR_INFO(xfer->nxr_rc,
				   "[%p] Degraded mode read IO failed.", req);
	/*
	 * Recovers lost data using parity recovery algorithms only if
	 * one or more devices were in FAILED, OFFLINE, REPAIRING state.
	 */
	if (req->ir_dgmap_nr > 0) {
		rc = req->ir_ops->iro_dgmode_recover(req);
		if (rc != 0)
			return M0_ERR_INFO(rc, "[%p] Failed to recover lost "
					   "data.", req);
	}

	return M0_RC(rc);
}

extern const struct m0_uint128 m0_rm_m0t1fs_group;

static int ioreq_file_lock(struct io_request *req)
{
	int                  rc;
	struct m0t1fs_inode *mi;

	M0_PRE(req != NULL);
	M0_ENTRY("[%p]", req);

	mi = m0t1fs_file_to_m0inode(req->ir_file);
	req->ir_in.rin_want.cr_group_id = m0_rm_m0t1fs_group;
	m0_file_lock(&mi->ci_fowner, &req->ir_in);
	m0_rm_owner_lock(&mi->ci_fowner);
	rc = m0_sm_timedwait(&req->ir_in.rin_sm,
			     M0_BITS(RI_SUCCESS, RI_FAILURE),
			     M0_TIME_NEVER);
	m0_rm_owner_unlock(&mi->ci_fowner);
	rc = rc ?: req->ir_in.rin_rc;

	return M0_RC(rc);
}

static void ioreq_file_unlock(struct io_request *req)
{
	M0_PRE(req != NULL);
	M0_ENTRY("[%p]", req);
	m0_file_unlock(&req->ir_in);
}

static int ioreq_no_lock(struct io_request *req)
{
	return 0;
}

static void ioreq_no_unlock(struct io_request *req)
{;}

static void device_state_reset(struct nw_xfer_request *xfer, bool rmw)
{
	struct target_ioreq *ti;

	M0_PRE(xfer != NULL);
	M0_PRE(xfer->nxr_state == NXS_COMPLETE);

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		ti->ti_state = M0_PNDS_ONLINE;
	} m0_htable_endfor;
}

static int ioreq_iosm_handle(struct io_request *req)
{
	int                     rc;
	bool                    rmw;
	uint64_t                map;
	struct inode           *inode;
	struct target_ioreq    *ti;
	struct nw_xfer_request *xfer;
	struct m0t1fs_sb       *csb;

	M0_PRE_EX(io_request_invariant(req));
	xfer = &req->ir_nwxfer;
	M0_ENTRY("[%p] sb %p", req, file_to_sb(req->ir_file));
	csb = M0T1FS_SB(m0t1fs_file_to_inode(req->ir_file)->i_sb);

	for (map = 0; map < req->ir_iomap_nr; ++map) {
		if (M0_IN(req->ir_iomaps[map]->pi_rtype,
			  (PIR_READOLD, PIR_READREST)))
			break;
	}

	/*
	 * Acquires lock before proceeding to do actual IO.
	 */
	rc = req->ir_ops->iro_file_lock(req);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "[%p] iro_file_lock() failed: rc=%d", req, rc);
		goto fail;
	}

	/* @todo Do error handling based on m0_sm::sm_rc. */
	/*
	 * Since m0_sm is part of io_request, for any parity group
	 * which is partial, read-modify-write state transition is followed
	 * for all parity groups.
	 */
	M0_LOG(M0_DEBUG, "[%p] map=%llu map_nr=%llu",
	       req, map, req->ir_iomap_nr);
	if (map == req->ir_iomap_nr) {
		enum io_req_state state;

		rmw = false;
		state = req->ir_type == IRT_READ ? IRS_READING :
						   IRS_WRITING;
		if (state == IRS_WRITING) {
			rc = req->ir_ops->iro_user_data_copy(req,
					CD_COPY_FROM_USER, 0);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] iro_user_data_copy() "
				       "failed: rc=%d", req, rc);
				goto fail_locked;
			}
			rc = req->ir_ops->iro_parity_recalc(req);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] iro_parity_recalc() "
				       "failed: rc=%d", req, rc);
				goto fail_locked;
			}
		}
		ioreq_sm_state_set(req, state);
		rc = xfer->nxr_ops->nxo_dispatch(xfer);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "[%p] nxo_dispatch() failed: rc=%d",
			       req, rc);
			goto fail_locked;
		}
		state = req->ir_type == IRT_READ ? IRS_READ_COMPLETE:
						   IRS_WRITE_COMPLETE;
		rc = ioreq_sm_timedwait(req, state);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "[%p] ioreq_sm_timedwait() failed: "
			       "rc=%d", req, rc);
			goto fail_locked;
		}
		if (req->ir_rc != 0) {
			rc = req->ir_rc;
			M0_LOG(M0_ERROR, "[%p] ir_rc=%d", req, rc);
			goto fail_locked;
		}
		if (state == IRS_READ_COMPLETE) {

			/*
			 * Returns immediately if all devices are
			 * in healthy state.
			 */
			rc = req->ir_ops->iro_dgmode_read(req, rmw);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] iro_dgmode_read() "
				       "failed: rc=%d", req, rc);
				goto fail_locked;
			}
			rc = req->ir_ops->iro_parity_verify(req);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] parity verification "
				       "failed: rc=%d", req, rc);
				goto fail_locked;
			}
			rc = req->ir_ops->iro_user_data_copy(req,
					CD_COPY_TO_USER, 0);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] iro_user_data_copy() "
				       "failed: rc=%d", req, rc);
				goto fail_locked;
			}
		} else {
			M0_ASSERT(state == IRS_WRITE_COMPLETE);
			/*
			 * Returns immediately if all devices are
			 * in healthy state.
			 */
			rc = req->ir_ops->iro_dgmode_write(req, rmw);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] iro_dgmode_write() "
				       "failed: rc=%d", req, rc);
				goto fail_locked;
			}
		}
	} else {
		uint32_t    seg;
		m0_bcount_t read_pages = 0;

		rmw = true;
		m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
			for (seg = 0; seg < V_SEG_NR(&ti->ti_bufvec); ++seg)
				if (PA(&ti->ti_pageattrs, seg) & PA_READ)
					++read_pages;
		} m0_htable_endfor;

		/* Read IO is issued only if byte count > 0. */
		if (read_pages > 0) {
			ioreq_sm_state_set(req, IRS_READING);
			rc = xfer->nxr_ops->nxo_dispatch(xfer);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] nxo_dispatch() failed: "
				       "rc=%d", req, rc);
				goto fail_locked;
			}
		}

		/* Waits for read completion if read IO was issued. */
		if (read_pages > 0) {
			rc = ioreq_sm_timedwait(req, IRS_READ_COMPLETE);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] ioreq_sm_timedwait() "
				       "failed: rc=%d", req, rc);
				goto fail_locked;
			}

			/*
			 * Returns immediately if all devices are
			 * in healthy state.
			 */
			rc = req->ir_ops->iro_dgmode_read(req, rmw);
			if (rc != 0) {
				M0_LOG(M0_ERROR, "[%p] iro_dgmode_read() "
				       "failed: rc=%d", req, rc);
				goto fail_locked;
			}
		}

		/*
		 * If fops dispatch fails, we need to wait till all io fop
		 * callbacks are acked since some IO fops might have been
		 * dispatched.
		 *
		 * Only fully modified pages from parity groups which have
		 * chosen read-rest approach or aligned parity groups,
		 * are copied since read-old approach needs reading of
		 * all spanned pages,
		 * (no matter fully modified or partially modified)
		 * in order to calculate parity correctly.
		 */
		rc = req->ir_ops->iro_user_data_copy(req, CD_COPY_FROM_USER,
						     PA_FULLPAGE_MODIFY);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "[%p] iro_user_data_copy() failed: "
			       "rc=%d", req, rc);
			goto fail_locked;
		}

		/* Copies
		 * - fully modified pages from parity groups which have
		 *   chosen read_old approach and
		 * - partially modified pages from all parity groups.
		 */
		rc = req->ir_ops->iro_user_data_copy(req, CD_COPY_FROM_USER, 0);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "[%p] iro_user_data_copy() failed: "
			       "rc=%d", req, rc);
			goto fail_locked;
		}

		/* Finalizes the old read fops. */
		if (read_pages > 0) {
			M0_LOG(M0_DEBUG, "[%p] About to nxo_complete()", req);
			xfer->nxr_ops->nxo_complete(xfer, rmw);
			if (req->ir_rc != 0) {
				M0_LOG(M0_ERROR, "[%p] nxo_complete() failed: "
				       "rc=%d", req, rc);
				rc = req->ir_rc;
				goto fail_locked;
			}
			device_state_reset(xfer, rmw);
		}
		ioreq_sm_state_set(req, IRS_WRITING);
		rc = req->ir_ops->iro_parity_recalc(req);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "[%p] iro_parity_recalc() failed: "
			       "rc=%d", req, rc);
			goto fail_locked;
		}
		rc = xfer->nxr_ops->nxo_dispatch(xfer);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "[%p] nxo_dispatch() failed: rc=%d",
			       req, rc);
			goto fail_locked;
		}

		rc = ioreq_sm_timedwait(req, IRS_WRITE_COMPLETE);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "[%p] ioreq_sm_timedwait() failed: "
			       "rc=%d", req,
			       rc);
			goto fail_locked;
		}

		/* Returns immediately if all devices are in healthy state. */
		rc = req->ir_ops->iro_dgmode_write(req, rmw);
		if (rc != 0) {
			M0_LOG(M0_ERROR, "[%p] iro_dgmode_write() failed: "
					"rc=%d", req, rc);
			goto fail_locked;
		}
	}

	/*
	 * Updates file size on successful write IO.
	 * New file size is maximum value between old file size and
	 * valid file position written in current write IO call.
	 */
	inode = m0t1fs_file_to_inode(req->ir_file);
	if (ioreq_sm_state(req) == IRS_WRITE_COMPLETE) {
		uint64_t newsize = max64u(inode->i_size,
				v_seg_endpos(&req->ir_ivv,
					V_SEG_NR(&req->ir_ivv) - 1));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
		rc = m0t1fs_size_update(req->ir_file->f_path.dentry, newsize);
#else
		rc = m0t1fs_size_update(req->ir_file->f_dentry, newsize);
#endif
		m0_mutex_lock(&csb->csb_confc_state.cus_lock);
		if (rc != 0 && csb->csb_confc_state.cus_state != M0_CC_READY) {
			m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
			rc = M0_ERR(-ESTALE);
			goto fail_locked;
		}
		m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
		M0_LOG(M0_INFO, "[%p] File size set to %llu", req,
		       inode->i_size);
	}

	req->ir_ops->iro_file_unlock(req);

	M0_LOG(M0_DEBUG, "[%p] About to nxo_complete()", req);
	xfer->nxr_ops->nxo_complete(xfer, rmw);

	if (rmw)
		ioreq_sm_state_set(req, IRS_REQ_COMPLETE);

	return M0_RC(0);

fail_locked:
	req->ir_ops->iro_file_unlock(req);
fail:
	ioreq_sm_failed(req, rc);
	M0_LOG(M0_DEBUG, "[%p] About to nxo_complete()", req);
	xfer->nxr_ops->nxo_complete(xfer, false);
	ioreq_sm_state_set(req, IRS_REQ_COMPLETE);
	return M0_ERR_INFO(rc, "[%p] ioreq_iosm_handle failed", req);
}

static int io_request_init(struct io_request        *req,
			   struct file              *file,
			   const struct iovec       *iov,
			   struct m0_indexvec_varr  *ivv,
			   enum io_req_type          rw)
{
	struct m0t1fs_inode       *ci;
	struct m0t1fs_sb          *csb;
	struct m0_pool_version    *pver;
	struct m0_layout_instance *li;
	int                        rc;
	uint32_t                   seg;
	uint32_t                   i;
	uint32_t                   max_failures;

	M0_ENTRY("[%p] rw %d", req, rw);

	M0_PRE(req  != NULL);
	M0_PRE(file != NULL);
	M0_PRE(iov  != NULL);
	M0_PRE(ivv != NULL);
	M0_PRE(M0_IN(rw, (IRT_READ, IRT_WRITE)));
	M0_PRE(M0_IS0(req));

	csb = file_to_sb(file);
	rc = m0t1fs_ref_get_lock(csb);
	if (rc != 0)
		return M0_ERR(rc);
	req->ir_rc	  = 0;
	req->ir_file      = file;
	req->ir_type      = rw;
	req->ir_iovec     = iov;
	req->ir_iomap_nr  = 0;
	req->ir_copied_nr = 0;
	req->ir_direct_io = !!(file->f_flags & O_DIRECT);
	req->ir_sns_state = SRS_UNINITIALIZED;
	req->ir_ops       = csb->csb_oostore ? &ioreq_oostore_ops : &ioreq_ops;

	/*
	 * rconfc might have refreshed pool versions, and pool version for
	 * this file might have got evicted forever. Check if we still have
	 * the ground underneath.
	 */
	ci = m0t1fs_file_to_m0inode(req->ir_file);
	pver =  m0_pool_version_find(&csb->csb_pools_common, &ci->ci_pver);
	if (pver == NULL) {
		rc = M0_ERR_INFO(-ENOENT, "Cannot find pool version "FID_F,
				 FID_P(&ci->ci_pver));
		goto err;
	}
	li = ci->ci_layout_instance;
	/*
	 * File resides on a virtual pool version that got refreshed during
	 * rconfc update leading to evicting the layout.
	 */
	if (li == NULL) {
		rc = m0t1fs_inode_layout_init(ci);
		if (rc != 0)
			goto err;
	}
	io_request_bob_init(req);
	nw_xfer_request_init(&req->ir_nwxfer);
	if (req->ir_nwxfer.nxr_rc != 0) {
		rc = M0_ERR_INFO(req->ir_nwxfer.nxr_rc,
				 "[%p] nw_xfer_req_init() failed", req);
		goto err;
	}
	max_failures = tolerance_of_level(req, M0_CONF_PVER_LVL_CTRLS);
	M0_ALLOC_ARR(req->ir_failed_session, max_failures + 1);
	if (req->ir_failed_session == NULL) {
		rc = M0_ERR_INFO(-ENOMEM, "[%p] Allocation of an array of "
			         "failed sessions.", req);
		goto err;
	}
	for (i = 0; i < max_failures; ++i) {
		req->ir_failed_session[i] = ~(uint64_t)0;
	}

	m0_sm_init(&req->ir_sm, &io_sm_conf, IRS_INITIALIZED,
		   file_to_smgroup(req->ir_file));

	rc = m0_indexvec_varr_alloc(&req->ir_ivv, V_SEG_NR(ivv));

	if (rc != 0) {
		m0_free(req->ir_failed_session);
		M0_LOG(M0_ERROR, "[%p] Allocation of m0_indexvec_varr", req);
		goto err;
	}

	for (seg = 0; seg < V_SEG_NR(ivv); ++seg) {
		V_INDEX(&req->ir_ivv, seg) = V_INDEX(ivv, seg);
		V_COUNT(&req->ir_ivv, seg) = V_COUNT(ivv, seg);
	}

	/* Sorts the index vector in increasing order of file offset. */
	indexvec_sort(&req->ir_ivv);
	indexvec_varr_dump(&req->ir_ivv);
	M0_POST_EX(ergo(rc == 0, io_request_invariant(req)));

	return M0_RC(0);
err:
	m0t1fs_ref_put_lock(csb);
	return M0_ERR(rc);
}

static void io_request_fini(struct io_request *req)
{
	struct target_ioreq *ti;
	struct m0_sm_group  *grp;
	struct m0t1fs_sb    *csb;

	M0_PRE_EX(io_request_invariant(req));

	M0_ENTRY("[%p]", req);

	csb = file_to_sb(req->ir_file);
	grp = req->ir_sm.sm_grp;

	m0_sm_group_lock(grp);

	m0_sm_fini(&req->ir_sm);
	io_request_bob_fini(req);
	req->ir_file   = NULL;
	req->ir_iovec  = NULL;
	req->ir_iomaps = NULL;
	req->ir_ops    = NULL;
	m0_indexvec_varr_free(&req->ir_ivv);

	m0_htable_for(tioreqht, ti, &req->ir_nwxfer.nxr_tioreqs_hash) {
		tioreqht_htable_del(&req->ir_nwxfer.nxr_tioreqs_hash, ti);
		M0_LOG(M0_DEBUG, "[%p] target_ioreq %p deleted for "FID_F,
		       req, ti, FID_P(&ti->ti_fid));
		/*
		 * All io_req_fop structures in list target_ioreq::ti_iofops
		 * are already finalized in nw_xfer_req_complete().
		 */
		target_ioreq_fini(ti);
		m0_free(ti);
		++iommstats.d_target_ioreq_nr;
	} m0_htable_endfor;

	nw_xfer_request_fini(&req->ir_nwxfer);

	m0_free(req->ir_failed_session);
	m0_sm_group_unlock(grp);
	m0t1fs_ref_put_lock(csb);
	M0_LEAVE();
}

static bool should_spare_be_mapped(struct io_request  *req,
				   enum m0_pool_nd_state device_state)
{
	return   (M0_IN(ioreq_sm_state(req),
		 (IRS_READING, IRS_DEGRADED_READING)) &&
		 device_state        == M0_PNDS_SNS_REPAIRED) ||

		 (ioreq_sm_state(req) == IRS_DEGRADED_WRITING &&
		 (device_state == M0_PNDS_SNS_REPAIRED ||
		 (device_state == M0_PNDS_SNS_REPAIRING &&
		 req->ir_sns_state == SRS_REPAIR_DONE)));
}

static int nw_xfer_tioreq_map(struct nw_xfer_request           *xfer,
			      const struct m0_pdclust_src_addr *src,
			      struct m0_pdclust_tgt_addr       *tgt,
			      struct target_ioreq             **out)
{
	struct m0_fid               tfid;
	const struct m0_fid        *gfid;
	struct io_request          *req;
	struct m0_rpc_session      *session;
	struct m0_pdclust_layout   *play;
	struct m0_pdclust_instance *play_instance;
	enum m0_pool_nd_state       device_state;
	int                         rc;
	struct m0_poolmach         *pm;


	M0_ENTRY("nw_xfer_request %p", xfer);
	M0_PRE_EX(nw_xfer_request_invariant(xfer));
	M0_PRE(src != NULL);
	M0_PRE(tgt != NULL);

	req  = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);
	play = pdlayout_get(req);
	play_instance = pdlayout_instance(layout_instance(req));

	m0_fd_fwd_map(play_instance, src, tgt);
	tfid = target_fid(req, tgt);

	M0_LOG(M0_DEBUG, "[%p] src_id[%llu:%llu] -> dest_id[%llu:%llu] "
	       "@ tfid "FID_F, req, src->sa_group, src->sa_unit,
	       tgt->ta_frame, tgt->ta_obj, FID_P(&tfid));

	pm = m0t1fs_file_to_poolmach(req->ir_file);
	M0_ASSERT(pm != NULL);
	rc = m0_poolmach_device_state(pm, tgt->ta_obj, &device_state);
	if (rc != 0)
		return M0_RC(rc);

	M0_ADDB2_ADD(M0_AVI_FS_IO_MAP, ioreq_sm_state(req),
		     tfid.f_container, tfid.f_key,
		     m0_pdclust_unit_classify(play, src->sa_unit),
		     device_state,
		     tgt->ta_frame, tgt->ta_obj,
		     src->sa_group, src->sa_unit);

	if (M0_FI_ENABLED("poolmach_client_repaired_device1")) {
		if (tfid.f_container == 1)
			device_state = M0_PNDS_SNS_REPAIRED;
	}

	/*
	 * Listed here are various possible combinations of different
	 * parameters. The cumulative result of these values decide
	 * whether given IO request should be redirected to spare
	 * or not.
	 * Note: For normal IO, M0_IN(ioreq_sm_state,
	 * (IRS_READING, IRS_WRITING)), this redirection is not needed with
	 * the exception of read IO case where the failed device is in
	 * REPAIRED state.
	 * Also, req->ir_sns_state member is used only to differentiate
	 * between 2 possible use cases during degraded mode write.
	 * This flag is not used elsewhere.
	 *
	 * Parameters:
	 * - State of IO request.
	 *   Sample set {IRS_DEGRADED_READING, IRS_DEGRADED_WRITING}
	 *
	 * - State of current device.
	 *   Sample set {M0_PNDS_SNS_REPAIRING, M0_PNDS_SNS_REPAIRED}
	 *
	 * - State of SNS repair process with respect to current global fid.
	 *   Sample set {SRS_REPAIR_DONE, SRS_REPAIR_NOTDONE}
	 *
	 * Common case:
	 * req->ir_state == IRS_DEGRADED_READING &&
	 * M0_IN(req->ir_sns_state, (SRS_REPAIR_DONE || SRS_REPAIR_NOTDONE)
	 *
	 * 1. device_state == M0_PNDS_SNS_REPAIRING
	 *    In this case, data to failed device is not redirected to
	 *    spare device.
	 *    The extent is assigned to the failed device itself but
	 *    it is filtered at the level of io_req_fop.
	 *
	 * 2. device_state == M0_PNDS_SNS_REPAIRED
	 *    Here, data to failed device is redirected to respective spare
	 *    unit.
	 *
	 * Common case:
	 * req->ir_state == IRS_DEGRADED_WRITING.
	 *
	 * 1. device_state   == M0_PNDS_SNS_REPAIRED,
	 *    In this case, the device repair has finished. Ergo, data is
	 *    redirected towards respective spare unit.
	 *
	 * 2. device_state   == M0_PNDS_SNS_REPAIRING &&
	 *    req->ir_sns_state == SRS_REPAIR_DONE.
	 *    In this case, repair has finished for current global fid but
	 *    has not finished completely. Ergo, data is redirected towards
	 *    respective spare unit.
	 *
	 * 3. device_state   == M0_PNDS_SNS_REPAIRING &&
	 *    req->ir_sns_state == SRS_REPAIR_NOTDONE.
	 *    In this case, data to failed device is not redirected to the
	 *    spare unit since we drop all pages directed towards failed device.
	 *
	 * 4. device_state   == M0_PNDS_SNS_REPAIRED &&
	 *    req->ir_sns_state == SRS_REPAIR_NOTDONE.
	 *    Unlikely case! What to do in this case?
	 */

	M0_LOG(M0_INFO, "[%p] tfid "FID_F ", device state = %d\n",
	       req, FID_P(&tfid), device_state);
	if (should_spare_be_mapped(req, device_state)) {
		struct m0_pdclust_src_addr  spare = *src;
		uint32_t                    spare_slot;
		uint32_t                    spare_slot_prev;
		enum m0_pool_nd_state       device_state_prev;

		gfid = file_to_fid(req->ir_file);
		rc = m0_sns_repair_spare_map(pm, gfid, play,
					     play_instance, src->sa_group,
					     src->sa_unit, &spare_slot,
					     &spare_slot_prev);
		if (M0_FI_ENABLED("poolmach_client_repaired_device1")) {
			if (tfid.f_container == 1) {
				rc = 0;
				spare_slot = layout_n(play) + layout_k(play);
			}
		}

		if (rc != 0)
			return M0_RC(rc);
		/* Check if there is an effective-failure. */
		if (spare_slot_prev != src->sa_unit) {
			spare.sa_unit = spare_slot_prev;
			m0_fd_fwd_map(play_instance, &spare, tgt);
			tfid = target_fid(req, tgt);
			rc = m0_poolmach_device_state(pm,
						      tgt->ta_obj,
						      &device_state_prev);
			if (rc != 0)
				return M0_RC(rc);
		} else
			device_state_prev = M0_PNDS_SNS_REPAIRED;

		if (device_state_prev == M0_PNDS_SNS_REPAIRED) {
			spare.sa_unit = spare_slot;
			m0_fd_fwd_map(play_instance, &spare, tgt);
			tfid = target_fid(req, tgt);
		}
		device_state = device_state_prev;
		M0_LOG(M0_DEBUG, "[%p] REPAIRED: [%llu:%llu] -> [%llu:%llu] "
		       "@ tfid " FID_F, req, spare.sa_group, spare.sa_unit,
		       tgt->ta_frame, tgt->ta_obj, FID_P(&tfid));
		M0_ADDB2_ADD(M0_AVI_FS_IO_MAP, ioreq_sm_state(req),
			     tfid.f_container, tfid.f_key,
			     m0_pdclust_unit_classify(play, spare.sa_unit),
			     device_state,
			     tgt->ta_frame, tgt->ta_obj,
			     spare.sa_group, spare.sa_unit);
	}

	session = target_session(req, tfid);

	rc = nw_xfer_tioreq_get(xfer, &tfid, tgt->ta_obj, session,
				layout_unit_size(play) * req->ir_iomap_nr,
				out);

	if (M0_IN(ioreq_sm_state(req), (IRS_DEGRADED_READING,
					IRS_DEGRADED_WRITING)) &&
	    device_state != M0_PNDS_SNS_REPAIRED)
		(*out)->ti_state = device_state;

	return M0_RC(rc);
}

static int target_ioreq_init(struct target_ioreq    *ti,
			     struct nw_xfer_request *xfer,
			     const struct m0_fid    *cobfid,
			     uint64_t                ta_obj,
			     struct m0_rpc_session  *session,
			     uint64_t                size)
{
	int                rc;
	struct io_request *req;
	uint64_t           cnt;

	M0_PRE(ti      != NULL);
	M0_PRE(xfer    != NULL);
	M0_PRE(cobfid  != NULL);
	M0_PRE(session != NULL);
	M0_PRE(size    >  0);

	M0_ENTRY("target_ioreq %p, nw_xfer_request %p, "FID_F,
		 ti, xfer, FID_P(cobfid));

	ti->ti_rc        = 0;
	ti->ti_ops       = &tioreq_ops;
	ti->ti_fid       = *cobfid;
	ti->ti_nwxfer    = xfer;
	ti->ti_dgvec     = NULL;
	ti->ti_req_type  = TI_NONE;
	M0_SET0(&ti->ti_cc_fop);
	/*
	 * Target object is usually in ONLINE state unless explicitly
	 * told otherwise.
	 */
	ti->ti_state     = M0_PNDS_ONLINE;
	ti->ti_session   = session;
	ti->ti_parbytes  = 0;
	ti->ti_databytes = 0;

	req        = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);
	ti->ti_obj = ta_obj;

	M0_LOG(M0_DEBUG, "[%p] ti %p", req, ti);
	iofops_tlist_init(&ti->ti_iofops);
	tioreqht_tlink_init(ti);
	target_ioreq_bob_init(ti);
	cnt = page_nr(size);

	rc = m0_indexvec_varr_alloc(&ti->ti_ivv, cnt);
	if (rc != 0)
		goto fail;

	rc = m0_indexvec_varr_alloc(&ti->ti_bufvec, cnt);
	if (rc != 0)
		goto fail_free_iv;

	rc = m0_varr_init(&ti->ti_pageattrs, cnt, sizeof(enum page_attr),
			  (size_t)m0_pagesize_get());
	if (rc != 0)
		goto fail_free_bv;

	/*
	 * This value is incremented when new segments are added to the
	 * index vector in target_ioreq_seg_add().
	 */
	V_SEG_NR(&ti->ti_ivv) = 0;

	M0_POST_EX(target_ioreq_invariant(ti));
	return M0_RC(0);

fail_free_bv:
	m0_indexvec_varr_free(&ti->ti_bufvec);
fail_free_iv:
	m0_indexvec_varr_free(&ti->ti_ivv);
fail:
	return M0_ERR_INFO(-ENOMEM, "[%p] Failed to allocate memory in "
			   "target_ioreq_init", req);
}

static void target_ioreq_fini(struct target_ioreq *ti)
{
	M0_ENTRY("target_ioreq %p, ti->ti_nwxfer %p", ti, ti->ti_nwxfer);
	M0_PRE_EX(target_ioreq_invariant(ti));

	target_ioreq_bob_fini(ti);
	tioreqht_tlink_fini(ti);
	iofops_tlist_fini(&ti->ti_iofops);
	ti->ti_ops     = NULL;
	ti->ti_session = NULL;
	ti->ti_nwxfer  = NULL;

	m0_indexvec_varr_free(&ti->ti_ivv);
	m0_indexvec_varr_free(&ti->ti_bufvec);
	m0_varr_fini(&ti->ti_pageattrs);
	if (ti->ti_dgvec != NULL)
		dgmode_rwvec_dealloc_fini(ti->ti_dgvec);
	M0_LEAVE();
}

static struct target_ioreq *target_ioreq_locate(struct nw_xfer_request *xfer,
						const struct m0_fid    *fid)
{
	struct target_ioreq *ti;

	M0_ENTRY("nw_xfer_request %p, fid %p", xfer, fid);
	M0_PRE_EX(nw_xfer_request_invariant(xfer));
	M0_PRE(fid != NULL);

	ti = tioreqht_htable_lookup(&xfer->nxr_tioreqs_hash, &fid->f_container);
	M0_ASSERT(ergo(ti != NULL, m0_fid_cmp(fid, &ti->ti_fid) == 0));

	M0_LEAVE();
	return ti;
}

static int nw_xfer_tioreq_get(struct nw_xfer_request *xfer,
			      const struct m0_fid    *fid,
			      uint64_t                ta_obj,
			      struct m0_rpc_session  *session,
			      uint64_t                size,
			      struct target_ioreq   **out)
{
	int                  rc = 0;
	struct target_ioreq *ti;
	struct io_request   *req;

	M0_PRE_EX(nw_xfer_request_invariant(xfer));
	M0_PRE(fid     != NULL);
	M0_PRE(session != NULL);
	M0_PRE(out     != NULL);

	req = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);
	M0_ENTRY("[%p] "FID_F" ta_obj=%llu size=%llu",
		 req, FID_P(fid), ta_obj, size);

	ti = target_ioreq_locate(xfer, fid);
	if (ti == NULL) {
		M0_ALLOC_PTR(ti);
		if (ti == NULL)
			return M0_ERR_INFO(-ENOMEM, "[%p] Failed to allocate "
					   "memory for target_ioreq", req);

		rc = target_ioreq_init(ti, xfer, fid, ta_obj, session, size);
		if (rc == 0) {
			tioreqht_htable_add(&xfer->nxr_tioreqs_hash, ti);
			M0_LOG(M0_INFO, "[%p] New target_ioreq %p added for "
			       FID_F, req, ti, FID_P(fid));
		} else {
			m0_free(ti);
			return M0_ERR_INFO(rc, "[%p] target_ioreq_init() "
					   "failed", req);
		}
		++iommstats.a_target_ioreq_nr;
	}
	if (ti->ti_dgvec == NULL && M0_IN(ioreq_sm_state(req),
	    (IRS_DEGRADED_READING, IRS_DEGRADED_WRITING)))
		rc = dgmode_rwvec_alloc_init(ti);

	*out = ti;
	return M0_RC(rc);
}

static struct data_buf *data_buf_alloc_init(enum page_attr pattr)
{
	struct data_buf *buf;
	unsigned long    addr;

	M0_ENTRY();
	addr = get_zeroed_page(GFP_KERNEL);
	if (addr == 0) {
		M0_LOG(M0_ERROR, "Failed to get free page");
		return NULL;
	}

	++iommstats.a_page_nr;
	M0_ALLOC_PTR(buf);
	if (buf == NULL) {
		free_page(addr);
		M0_LOG(M0_ERROR, "Failed to allocate data_buf");
		return NULL;
	}

	++iommstats.a_data_buf_nr;
	data_buf_init(buf, (void *)addr, pattr);
	M0_POST(data_buf_invariant(buf));
	M0_LEAVE();
	return buf;
}

static void buf_page_free(struct m0_buf *buf)
{
	M0_PRE(buf != NULL);

	free_page((unsigned long)buf->b_addr);
	++iommstats.d_page_nr;
	buf->b_addr = NULL;
	buf->b_nob  = 0;
}

static void data_buf_dealloc_fini(struct data_buf *buf)
{
	M0_ENTRY("data_buf %p", buf);
	M0_PRE(data_buf_invariant(buf));

	if (buf->db_page != NULL)
		user_page_unmap(buf, (buf->db_flags & PA_WRITE) ? false : true);
	else if (buf->db_buf.b_addr != NULL)
		buf_page_free(&buf->db_buf);

	if (buf->db_auxbuf.b_addr != NULL)
		buf_page_free(&buf->db_auxbuf);

	data_buf_fini(buf);
	m0_free(buf);
	++iommstats.d_data_buf_nr;
	M0_LEAVE();
}

static void target_ioreq_seg_add(struct target_ioreq              *ti,
				 const struct m0_pdclust_src_addr *src,
				 const struct m0_pdclust_tgt_addr *tgt,
				 m0_bindex_t                       gob_offset,
				 m0_bcount_t                       count,
				 struct pargrp_iomap              *map)
{
	uint32_t                   seg;
	m0_bindex_t                toff;
	m0_bindex_t                goff;
	m0_bindex_t                pgstart;
	m0_bindex_t                pgend;
	struct data_buf           *buf;
	struct io_request         *req;
	struct m0_pdclust_layout  *play;
	uint64_t                   frame = tgt->ta_frame;
	uint64_t                   unit  = src->sa_unit;
	struct m0_indexvec_varr   *ivv;
	struct m0_indexvec_varr   *bvec;
	enum m0_pdclust_unit_type  unit_type;
	struct m0_varr            *pattr;
	uint64_t                   cnt;

	M0_ENTRY("tio req %p, gob_offset %llu, count %llu frame %llu unit %llu",
		 ti, gob_offset, count, frame, unit);
	M0_PRE_EX(target_ioreq_invariant(ti));
	M0_PRE(map != NULL);

	req     = bob_of(ti->ti_nwxfer, struct io_request, ir_nwxfer,
			 &ioreq_bobtype);
	play    = pdlayout_get(req);

	unit_type = m0_pdclust_unit_classify(play, unit);
	M0_ASSERT(M0_IN(unit_type, (M0_PUT_DATA, M0_PUT_PARITY)));

	toff    = target_offset(frame, play, gob_offset);
	pgstart = toff;
	goff    = unit_type == M0_PUT_DATA ? gob_offset : 0;

	M0_LOG(M0_DEBUG, "[%p] %llu: "
	       "[gpos %6llu, +%llu][%llu,%llu]->[%llu,%llu] %c",
	       req, map->pi_grpid,
	       gob_offset, count, src->sa_group, src->sa_unit,
	       tgt->ta_frame, tgt->ta_obj,
	       unit_type == M0_PUT_DATA ? 'D' : 'P');

	/* Use ti_dgvec as long as it is dgmode-read/write. */
	if (ioreq_sm_state(req) == IRS_DEGRADED_READING ||
	    ioreq_sm_state(req) == IRS_DEGRADED_WRITING) {
		M0_ASSERT(ti->ti_dgvec != NULL);
		ivv   = &ti->ti_dgvec->dr_ivec_varr;
		bvec  = &ti->ti_dgvec->dr_bufvec;
		pattr = &ti->ti_dgvec->dr_pageattrs;
		cnt = page_nr(req->ir_iomap_nr * layout_unit_size(play) *
		      (layout_n(play) + layout_k(play)));
		M0_LOG(M0_DEBUG, "[%p] map_nr=%llu req state=%u cnt=%llu",
		       req, req->ir_iomap_nr, ioreq_sm_state(req), cnt);
	} else {
		ivv   = &ti->ti_ivv;
		bvec  = &ti->ti_bufvec;
		pattr = &ti->ti_pageattrs;
		cnt = page_nr(req->ir_iomap_nr * layout_unit_size(play) *
		      layout_n(play));
		M0_LOG(M0_DEBUG, "[%p] map_nr=%llu req state=%u cnt=%llu",
		       req, req->ir_iomap_nr, ioreq_sm_state(req), cnt);
	}

	while (pgstart < toff + count) {
		pgend = min64u(pgstart + PAGE_SIZE, toff + count);
		seg   = V_SEG_NR(ivv);

		V_INDEX(ivv, seg) = pgstart;
		V_COUNT(ivv, seg) = pgend - pgstart;

		if (unit_type == M0_PUT_DATA) {
			uint32_t row;
			uint32_t col;

			page_pos_get(map, goff, &row, &col);
			buf = map->pi_databufs[row][col];

			PA(pattr,seg) |= PA_DATA;
			M0_LOG(M0_DEBUG, "[%p] ti %p, Data seg %u added",
			       req, ti, seg);
		} else {
			buf = map->pi_paritybufs[page_id(goff)]
						[unit % data_col_nr(play)];
			PA(pattr,seg) |= PA_PARITY;
			M0_LOG(M0_DEBUG, "[%p] ti %p, Parity seg %u added",
			       req, ti, seg);
		}
		buf->db_tioreq = ti;
		V_ADDR (bvec, seg) = buf->db_buf.b_addr;
		V_COUNT(bvec, seg) = V_COUNT(ivv, seg);
		PA(pattr, seg) |= buf->db_flags;
		M0_LOG(M0_DEBUG, "[%p] ti %p, Seg id %d pageaddr=%p "
		       "[%llu, %llu] added to target_ioreq with "FID_F
		       " with flags 0x%x", req, ti, seg, V_ADDR(bvec, seg),
		       V_INDEX(ivv, seg),
		       V_COUNT(ivv, seg),
		       FID_P(&ti->ti_fid),
		       PA(pattr, seg));

		goff += V_COUNT(ivv, seg);
		pgstart = pgend;
		++ V_SEG_NR(ivv);
		M0_ASSERT_INFO(V_SEG_NR(ivv) <= cnt,
			       "[%p] ti %p, v_nr=%u, page_nr=%llu",
			       req, ti, V_SEG_NR(ivv), cnt);
	}
	target_ioreq_type_set(ti, TI_READ_WRITE);
	M0_LEAVE();
}

static int io_req_fop_init(struct io_req_fop   *fop,
			   struct target_ioreq *ti,
			   enum page_attr       pattr)
{
	int                rc;
	struct io_request *req;

	M0_ENTRY("io_req_fop %p, target_ioreq %p", fop, ti);
	M0_PRE(fop != NULL);
	M0_PRE(ti  != NULL);
	M0_PRE(M0_IN(pattr, (PA_DATA, PA_PARITY)));

	io_req_fop_bob_init(fop);
	iofops_tlink_init(fop);
	fop->irf_pattr     = pattr;
	fop->irf_tioreq    = ti;
	fop->irf_reply_rc  = 0;
	fop->irf_ast.sa_cb = io_bottom_half;

	req = bob_of(ti->ti_nwxfer, struct io_request, ir_nwxfer,
		     &ioreq_bobtype);
	M0_ASSERT(M0_IN(ioreq_sm_state(req),
		  (IRS_READING, IRS_DEGRADED_READING,
		   IRS_WRITING, IRS_DEGRADED_WRITING)));

	fop->irf_ast.sa_mach = &req->ir_sm;

	rc  = m0_io_fop_init(&fop->irf_iofop, file_to_fid(req->ir_file),
			     M0_IN(ioreq_sm_state(req),
				   (IRS_WRITING, IRS_DEGRADED_WRITING)) ?
			     &m0_fop_cob_writev_fopt : &m0_fop_cob_readv_fopt,
			     io_req_fop_release);
	/*
	 * Changes ri_ops of rpc item so as to execute m0t1fs's own
	 * callback on receiving a reply.
	 */
	fop->irf_iofop.if_fop.f_item.ri_ops = &io_item_ops;

	M0_LOG(M0_DEBUG, "[%p] fop %p, m0_ref %p, "FID_F", %p[%u], "
	       "rbulk %p", req, &fop->irf_iofop.if_fop,
	       &fop->irf_iofop.if_fop.f_ref,
	       FID_P(&fop->irf_tioreq->ti_fid), &fop->irf_iofop.if_fop.f_item,
	       fop->irf_iofop.if_fop.f_item.ri_type->rit_opcode,
	       &fop->irf_iofop.if_rbulk);
	M0_POST(ergo(rc == 0, io_req_fop_invariant(fop)));
	return M0_RC(rc);
}

static void io_req_fop_fini(struct io_req_fop *fop)
{
	M0_ENTRY("io_req_fop %p", fop);
	M0_PRE(io_req_fop_invariant(fop));

	/*
	 * IO fop is finalized (m0_io_fop_fini()) through rpc sessions code
	 * using m0_rpc_item::m0_rpc_item_ops::rio_free().
	 * see m0_io_item_free().
	 */

	iofops_tlink_fini(fop);

	/*
	 * io_req_bob_fini() is not done here so that struct io_req_fop
	 * can be retrieved from struct m0_rpc_item using bob_of() and
	 * magic numbers can be checked.
	 */

	fop->irf_tioreq = NULL;
	fop->irf_ast.sa_cb = NULL;
	fop->irf_ast.sa_mach = NULL;
	M0_LEAVE();
}

static void irfop_fini(struct io_req_fop *irfop)
{
	M0_PRE(irfop != NULL);

	M0_ENTRY("io_req_fop %p, rbulk %p, fop %p, %p[%u]", irfop,
		 &irfop->irf_iofop.if_rbulk, &irfop->irf_iofop.if_fop,
		 &irfop->irf_iofop.if_fop.f_item,
		 irfop->irf_iofop.if_fop.f_item.ri_type->rit_opcode);
	m0_rpc_bulk_buflist_empty(&irfop->irf_iofop.if_rbulk);
	io_req_fop_fini(irfop);
	m0_free(irfop);
	M0_LEAVE();
}

static void ioreq_failed_fini(struct io_request *req, int rc)
{
	ioreq_sm_failed(req, rc);
	ioreq_sm_state_set(req, IRS_REQ_COMPLETE);
	req->ir_nwxfer.nxr_state = NXS_COMPLETE;
	io_request_fini(req);
}

/*
 * This function can be used by the ioctl which supports fully vectored
 * scatter-gather IO. The caller is supposed to provide an index vector
 * aligned with user buffers in struct iovec array.
 * This function is also used by file->f_op->aio_{read/write} path.
 */
M0_INTERNAL ssize_t m0t1fs_aio(struct kiocb             *kcb,
			       const struct iovec       *iov,
			       struct m0_indexvec_varr  *ivv,
			       enum io_req_type          rw)
{
	int                      rc;
	ssize_t                  count;
	struct io_request       *req;
	struct m0t1fs_sb        *csb;

	M0_THREAD_ENTER;
	M0_ENTRY("indexvec %p, rw %d", ivv, rw);
	M0_PRE(kcb  != NULL);
	M0_PRE(iov  != NULL);
	M0_PRE(ivv != NULL);
	M0_PRE(M0_IN(rw, (IRT_READ, IRT_WRITE)));

	csb   = file_to_sb(kcb->ki_filp);
again:
	M0_ALLOC_PTR(req);
	if (req == NULL)
		return M0_ERR_INFO(-ENOMEM, "Failed to allocate memory"
			       " for io_request");
	++iommstats.a_ioreq_nr;

	rc = io_request_init(req, kcb->ki_filp, iov, ivv, rw);
	if (rc != 0) {
		count = 0;
		goto last;
	}
	rc = req->ir_ops->iro_iomaps_prepare(req);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "[%p] Failed to prepare IO fops, rc %d",
		       req, rc);
		ioreq_failed_fini(req, rc);
		count = 0;
		goto last;
	}

	rc = req->ir_nwxfer.nxr_ops->nxo_distribute(&req->ir_nwxfer);
	if (rc != 0) {
		M0_LOG(M0_ERROR, "[%p] Failed to distribute file data "
		       "between target_ioreq objects, rc %d", req, rc);
		req->ir_ops->iro_iomaps_destroy(req);
		ioreq_failed_fini(req, rc);
		count = 0;
		goto last;
	}

	rc = req->ir_ops->iro_iosm_handle(req);
	if (rc == 0)
		rc = req->ir_rc;
	count = min64u(req->ir_nwxfer.nxr_bytes, req->ir_copied_nr);
	M0_LOG(M0_INFO, "[%p] nxr_bytes = %llu, copied_nr = %llu, count %lu, "
	       "rc %d", req, req->ir_nwxfer.nxr_bytes, req->ir_copied_nr,
	       count, rc);

	req->ir_ops->iro_iomaps_destroy(req);

	io_request_fini(req);
last:
	M0_LOG(M0_DEBUG, "[%p] rc = %d, io request returned %lu bytes",
	       req, rc, count);
	m0_free(req);
	++iommstats.d_ioreq_nr;

	if (rc == -EAGAIN)
		goto again;

	M0_LEAVE();
	return rc != 0 ? rc : count;
}

static struct m0_indexvec_varr *indexvec_create(unsigned long       seg_nr,
						const struct iovec *iov,
						loff_t              pos)
{
	int                      rc;
	uint32_t                 i;
	struct m0_indexvec_varr *ivv;

	/*
	 * Apparently, we need to use a new API to process io request
	 * which can accept m0_indexvec_varr so that it can be reused by
	 * the ioctl which provides fully vectored scatter-gather IO
	 * to cluster library users.
	 * For that, we need to prepare a m0_indexvec_varr and supply it
	 * to this function.
	 */
	M0_ENTRY("seg_nr %lu position %llu", seg_nr, pos);
	M0_ALLOC_PTR(ivv);
	if (ivv == NULL) {
		M0_LEAVE();
		return NULL;
	}

	rc = m0_indexvec_varr_alloc(ivv, seg_nr);
	if (rc != 0) {
		m0_free(ivv);
		M0_LEAVE();
		return NULL;
	}

	for (i = 0; i < seg_nr; ++i) {
		V_INDEX(ivv, i) = pos;
		V_COUNT(ivv, i) = iov[i].iov_len;
		pos += iov[i].iov_len;
	}
	M0_POST(indexvec_varr_count(ivv) > 0);

	M0_LEAVE();
	return ivv;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static ssize_t file_dio_write(struct kiocb *kcb, struct iov_iter *from)
{
	struct file  *file  = kcb->ki_filp;
	struct inode *inode = m0t1fs_file_to_inode(file);
	ssize_t       written;

	M0_THREAD_ENTER;
	M0_ENTRY();

	inode_lock(inode);
	written = __generic_file_write_iter(kcb, from);
	inode_unlock(inode);

	if (written > 0)
		written = generic_write_sync(kcb, written);

	M0_LEAVE();
	return written;
}
#else
static ssize_t file_dio_write(struct kiocb       *kcb,
			      const struct iovec *iov,
			      unsigned long       seg_nr,
			      loff_t              pos)
{
	struct file  *file  = kcb->ki_filp;
	struct inode *inode = m0t1fs_file_to_inode(file);
	ssize_t       written;

	M0_THREAD_ENTER;
	M0_ENTRY();
	BUG_ON(kcb->ki_pos != pos);

	mutex_lock(&inode->i_mutex);
	written = __generic_file_aio_write(kcb, iov, seg_nr, &kcb->ki_pos);
	mutex_unlock(&inode->i_mutex);

	if (written > 0) {
		written = generic_write_sync(file, pos, written) ?: written;
	}

	M0_LEAVE();
	return written;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static ssize_t aio_write(struct kiocb *kcb, struct iov_iter *from)
{
	size_t                  count = 0;
	ssize_t                 written;
	struct m0_indexvec_varr *ivv;

	M0_THREAD_ENTER;
	M0_PRE(kcb != NULL);
	M0_PRE(from != NULL);
	M0_ENTRY("struct iovec %p position %llu seg_nr %lu", from->iov, kcb->ki_pos, from->nr_segs);

	if (!file_to_sb(kcb->ki_filp)->csb_active) {
		M0_LEAVE();
		return M0_ERR(-EINVAL);
	}

	count = generic_write_checks(kcb, from);
	if (count <= 0) {
		M0_LEAVE();
		return 0;
	}

	if (kcb->ki_filp->f_flags & O_DIRECT) {
		written = file_dio_write(kcb, from);
		M0_LEAVE();
		return written;
	}

	ivv = indexvec_create(from->nr_segs, from->iov, kcb->ki_pos);
	if (ivv == NULL)
		return M0_ERR(-ENOMEM);

	indexvec_varr_dump(ivv);

	M0_LOG(M0_INFO, "Write vec-count = %llu seg_nr %lu",
			indexvec_varr_count(ivv), from->nr_segs);
	written = m0t1fs_aio(kcb, from->iov, ivv, IRT_WRITE);

	/* Updates file position. */
	if (written > 0)
		kcb->ki_pos = kcb->ki_pos + written;

	m0_indexvec_varr_free(ivv);
	m0_free(ivv);
	M0_LOG(M0_DEBUG, "written %llu", (unsigned long long)written);
	M0_LEAVE();
	return written;
}
#else
static ssize_t aio_write(struct kiocb *kcb, const struct iovec *iov,
			 unsigned long seg_nr, loff_t pos)
{
	int                 rc;
	size_t              count = 0;
	size_t              saved_count;
	ssize_t             written;
	struct m0_indexvec_varr *ivv;

	M0_THREAD_ENTER;
	M0_ENTRY("struct iovec %p position %llu seg_nr %lu", iov, pos, seg_nr);
	M0_PRE(kcb != NULL);
	M0_PRE(iov != NULL);
	M0_PRE(seg_nr > 0);

	if (!file_to_sb(kcb->ki_filp)->csb_active) {
		M0_LEAVE();
		return M0_ERR(-EINVAL);
	}

	rc = generic_segment_checks(iov, &seg_nr, &count, VERIFY_READ);
	if (rc != 0) {
		M0_LEAVE();
		return 0;
	}

	saved_count = count;
	rc = generic_write_checks(kcb->ki_filp, &pos, &count, 0);
	if (rc != 0 || count == 0) {
		M0_LEAVE();
		return 0;
	}

	if (count != saved_count)
		seg_nr = iov_shorten((struct iovec *)iov, seg_nr, count);

	if (kcb->ki_filp->f_flags & O_DIRECT) {
		written = file_dio_write(kcb, iov, seg_nr, pos);
		M0_LEAVE();
		return written;
	}

	ivv = indexvec_create(seg_nr, iov, pos);
	if (ivv == NULL)
		return M0_ERR(-ENOMEM);

	indexvec_varr_dump(ivv);

	M0_LOG(M0_INFO, "Write vec-count = %llu seg_nr %lu",
			indexvec_varr_count(ivv), seg_nr);
	written = m0t1fs_aio(kcb, iov, ivv, IRT_WRITE);

	/* Updates file position. */
	if (written > 0)
		kcb->ki_pos = pos + written;

	m0_indexvec_varr_free(ivv);
	m0_free(ivv);
	M0_LOG(M0_DEBUG, "written %llu", (unsigned long long)written);
	M0_LEAVE();
	return written;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static ssize_t file_aio_write(struct kiocb *kcb, struct iov_iter *from)
#else
static ssize_t file_aio_write(struct kiocb       *kcb,
			      const struct iovec *iov,
			      unsigned long       seg_nr,
			      loff_t              pos)
#endif
{
	ssize_t              res;
	struct m0t1fs_inode *ci = m0t1fs_file_to_m0inode(kcb->ki_filp);

	M0_THREAD_ENTER;

	m0_addb2_push(M0_AVI_FS_WRITE, M0_ADDB2_OBJ(&ci->ci_fid));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	res = aio_write(kcb, from);
	M0_ADDB2_ADD(M0_AVI_FS_IO_DESCR, kcb->ki_pos, res);
#else
	res = aio_write(kcb, iov, seg_nr, pos);
	M0_ADDB2_ADD(M0_AVI_FS_IO_DESCR, pos, res);
#endif
	m0_addb2_pop(M0_AVI_FS_WRITE);
	return res;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static ssize_t aio_read(struct kiocb *kcb, struct iov_iter *from)
{
	int                 seg;
	size_t              count = 0;
	loff_t              size;
	ssize_t             res;
	struct file        *filp;
	struct m0_indexvec_varr *ivv;

	M0_THREAD_ENTER;
	M0_PRE(kcb != NULL);
	M0_PRE(from != NULL);
	M0_ENTRY("struct iovec %p position %llu", from->iov, kcb->ki_pos);

	filp = kcb->ki_filp;
	size = i_size_read(m0t1fs_file_to_inode(filp));

	/* Returns if super block is inactive. */
	if (!file_to_sb(filp)->csb_active)
		return M0_ERR(-EINVAL);
	if (kcb->ki_pos >= size)
		return M0_RC(0);

	if (filp->f_flags & O_DIRECT) {
		res = generic_file_read_iter(kcb, from);
		M0_LEAVE();
		return res;
	}

	count = iov_iter_count(from);
	if (count == 0)
		/*
		 * And thus spake POSIX: "Before any action described below is
		 * taken, and if nbyte is zero, the read() function may detect
		 * and return errors as described below. In the absence of
		 * errors, or if error detection is not performed, the read()
		 * function shall return zero and have no other results."
		 */
		return M0_RC(0);

	/* Index vector has to be created before io_request is created. */
	ivv = indexvec_create(from->nr_segs, from->iov, kcb->ki_pos);
	if (ivv == NULL)
		return M0_ERR(-ENOMEM);

	/*
	 * For read IO, if any segment from index vector goes beyond EOF,
	 * they are dropped and the index vector is truncated to EOF boundary.
	 */
	for (seg = 0; seg < V_SEG_NR(ivv); ++seg) {
		if (v_seg_endpos(ivv, seg) > size) {
			V_COUNT(ivv, seg) = size - V_INDEX(ivv, seg);
			V_SEG_NR(ivv) = seg + 1;
			break;
		}
	}
	indexvec_varr_dump(ivv);
	if (indexvec_varr_count(ivv) == 0) {
		m0_indexvec_varr_free(ivv);
		m0_free(ivv);
		return M0_RC(0);
	}

	M0_LOG(M0_INFO, "Read vec-count = %llu", indexvec_varr_count(ivv));
	res = m0t1fs_aio(kcb, from->iov, ivv, IRT_READ);
	M0_LOG(M0_DEBUG, "Read @%llu vec-count = %8llu return = %8llu(%d)",
			 kcb->ki_pos, indexvec_varr_count(ivv),
			 (unsigned long long)res, (int)res);
	/* Updates file position. */
	if (res > 0)
		kcb->ki_pos = kcb->ki_pos + res;

	m0_indexvec_varr_free(ivv);
	m0_free(ivv);
	M0_LEAVE();
	return res;
}
#else
static ssize_t aio_read(struct kiocb *kcb, const struct iovec *iov,
			unsigned long seg_nr, loff_t pos)
{
	int                 seg;
	size_t              count = 0;
	loff_t              size;
	ssize_t             res;
	struct file        *filp;
	struct m0_indexvec_varr *ivv;

	M0_THREAD_ENTER;
	M0_ENTRY("struct iovec %p position %llu", iov, pos);
	M0_PRE(kcb != NULL);
	M0_PRE(iov != NULL);
	M0_PRE(seg_nr > 0);

	filp = kcb->ki_filp;
	size = i_size_read(m0t1fs_file_to_inode(filp));

	/* Returns if super block is inactive. */
	if (!file_to_sb(filp)->csb_active)
		return M0_ERR(-EINVAL);
	if (pos >= size)
		return M0_RC(0);

	if (filp->f_flags & O_DIRECT) {
		res = generic_file_aio_read(kcb, iov, seg_nr, pos);
		M0_LEAVE();
		return res;
	}

	/*
	 * Checks for access privileges and adjusts all segments
	 * for proper count and total number of segments.
	 */
	res = generic_segment_checks(iov, &seg_nr, &count, VERIFY_WRITE);
	if (res != 0) {
		M0_LEAVE();
		return res;
	}

	if (count == 0)
		/*
		 * And thus spake POSIX: "Before any action described below is
		 * taken, and if nbyte is zero, the read() function may detect
		 * and return errors as described below. In the absence of
		 * errors, or if error detection is not performed, the read()
		 * function shall return zero and have no other results."
		 */
		return M0_RC(0);

	/* Index vector has to be created before io_request is created. */
	ivv = indexvec_create(seg_nr, iov, pos);
	if (ivv == NULL)
		return M0_ERR(-ENOMEM);

	/*
	 * For read IO, if any segment from index vector goes beyond EOF,
	 * they are dropped and the index vector is truncated to EOF boundary.
	 */
	for (seg = 0; seg < V_SEG_NR(ivv); ++seg) {
		if (v_seg_endpos(ivv, seg) > size) {
			V_COUNT(ivv, seg) = size - V_INDEX(ivv, seg);
			V_SEG_NR(ivv) = seg + 1;
			break;
		}
	}
	indexvec_varr_dump(ivv);
	if (indexvec_varr_count(ivv) == 0) {
		m0_indexvec_varr_free(ivv);
		m0_free(ivv);
		return M0_RC(0);
	}

	M0_LOG(M0_INFO, "Read vec-count = %llu", indexvec_varr_count(ivv));
	res = m0t1fs_aio(kcb, iov, ivv, IRT_READ);
	M0_LOG(M0_DEBUG, "Read @%llu vec-count = %8llu return = %8llu(%d)",
			 pos, indexvec_varr_count(ivv),
			 (unsigned long long)res, (int)res);
	/* Updates file position. */
	if (res > 0)
		kcb->ki_pos = pos + res;

	m0_indexvec_varr_free(ivv);
	m0_free(ivv);
	M0_LEAVE();
	return res;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
static ssize_t file_aio_read(struct kiocb *kcb, struct iov_iter *from)
#else
static ssize_t file_aio_read(struct kiocb       *kcb,
			     const struct iovec *iov,
			     unsigned long       seg_nr,
			     loff_t              pos)
#endif
{
	ssize_t              res;
	struct m0t1fs_inode *ci = m0t1fs_file_to_m0inode(kcb->ki_filp);

	M0_THREAD_ENTER;

	m0_addb2_push(M0_AVI_FS_READ, M0_ADDB2_OBJ(&ci->ci_fid));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	res = aio_read(kcb, from);
	M0_ADDB2_ADD(M0_AVI_FS_IO_DESCR, kcb->ki_pos, res);
#else
	res = aio_read(kcb, iov, seg_nr, pos);
	M0_ADDB2_ADD(M0_AVI_FS_IO_DESCR, pos, res);
#endif

	m0_addb2_pop(M0_AVI_FS_READ);
	return res;
}

int m0t1fs_flush(struct file *file, fl_owner_t id)
{
	struct inode        *inode = m0t1fs_file_to_inode(file);
	struct m0t1fs_inode *ci = M0T1FS_I(inode);
	struct m0t1fs_mdop   mo;
	struct m0t1fs_sb    *csb = m0inode_to_sb(ci);
	int                  rc;

	M0_THREAD_ENTER;
	M0_ENTRY("inode links:%d inode writecount = %d close size %d",
		 (unsigned int)inode->i_nlink,
		 atomic_read(&inode->i_writecount),
		 (unsigned int)inode->i_size);

	if (!csb->csb_oostore || inode->i_nlink == 0 ||
	    atomic_read(&inode->i_writecount) == 0)
		return M0_RC(0);

	M0_SET0(&mo);
	mo.mo_attr.ca_tfid   = *m0t1fs_inode_fid(ci);
	mo.mo_attr.ca_size   = inode->i_size;
	mo.mo_attr.ca_nlink  = inode->i_nlink;
	mo.mo_attr.ca_pver   = m0t1fs_file_to_pver(file)->pv_id;
	mo.mo_attr.ca_lid    = ci->ci_layout_id;
	mo.mo_attr.ca_valid |= (M0_COB_SIZE | M0_COB_NLINK |
				M0_COB_PVER | M0_COB_LID);

	rc = m0t1fs_cob_setattr(inode, &mo);
	return rc != 0 ? M0_ERR_INFO(rc, FID_F, FID_P(&mo.mo_attr.ca_tfid)) :
			 M0_RC(rc);
}

const struct file_operations m0t1fs_reg_file_operations = {
	.llseek         = generic_file_llseek,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
	.read_iter      = file_aio_read,
	.write_iter     = file_aio_write,
#else
	.aio_read       = file_aio_read,
	.aio_write      = file_aio_write,
	.read           = do_sync_read,
	.write          = do_sync_write,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	.unlocked_ioctl = m0t1fs_ioctl,
#else
	.ioctl     = m0t1fs_ioctl,
#endif
	.fsync          = m0t1fs_fsync,
	.flush          = m0t1fs_flush,
};

static void client_passive_recv(const struct m0_net_buffer_event *evt)
{
	struct m0_rpc_bulk     *rbulk;
	struct m0_rpc_bulk_buf *buf;
	struct m0_net_buffer   *nb;
	struct m0_io_fop       *iofop;
	struct io_req_fop      *reqfop;
	struct io_request      *ioreq;
	uint32_t                req_sm_state;

	M0_ENTRY();

	M0_PRE(evt != NULL);
	M0_PRE(evt->nbe_buffer != NULL);

	nb = evt->nbe_buffer;
	buf = (struct m0_rpc_bulk_buf *)nb->nb_app_private;
	rbulk = buf->bb_rbulk;
	iofop = container_of(rbulk, struct m0_io_fop, if_rbulk);
	reqfop = bob_of(iofop, struct io_req_fop, irf_iofop, &iofop_bobtype);
	ioreq = bob_of(reqfop->irf_tioreq->ti_nwxfer, struct io_request,
		       ir_nwxfer, &ioreq_bobtype);
	M0_ASSERT(rbulk == &reqfop->irf_iofop.if_rbulk);
	M0_LOG(M0_DEBUG, "[%p] PASSIVE recv, e %p, status %d, len %llu, "
	       "nbuf %p", ioreq, evt, evt->nbe_status, evt->nbe_length, nb);

	M0_ASSERT(m0_is_read_fop(&iofop->if_fop));
	M0_LOG(M0_DEBUG, "[%p] Pending fops %llu, Pending rdbulk %llu, "
	       "fop %p, item %p, "FID_F", rbulk %p",
	       ioreq, m0_atomic64_get(&ioreq->ir_nwxfer.nxr_iofop_nr),
	       m0_atomic64_get(&ioreq->ir_nwxfer.nxr_rdbulk_nr) - 1,
	       &iofop->if_fop, &iofop->if_fop.f_item,
	       FID_P(&reqfop->irf_tioreq->ti_fid), rbulk);

	/*
	 * buf will be released in this callback. But rbulk is still valid
	 * after that.
	 */
	m0_rpc_bulk_default_cb(evt);
	if (evt->nbe_status != 0)
		return;
	m0_mutex_lock(&ioreq->ir_nwxfer.nxr_lock);
	req_sm_state = ioreq_sm_state(ioreq);
	if (req_sm_state != IRS_READ_COMPLETE &&
	    req_sm_state != IRS_WRITE_COMPLETE) {
		/*
		 * It is possible that io_bottom_half() has already
		 * reduced the nxr_rdbulk_nr to 0 by this time, due to FOP
		 * receiving some error.
		 */
		if (m0_atomic64_get(&ioreq->ir_nwxfer.nxr_rdbulk_nr) > 0)
			m0_atomic64_dec(&ioreq->ir_nwxfer.nxr_rdbulk_nr);
		if (should_req_sm_complete(ioreq)) {
			ioreq_sm_state_set(ioreq,
					   (M0_IN(req_sm_state,
						  (IRS_READING,
						   IRS_DEGRADED_READING)) ?
					   IRS_READ_COMPLETE :
					   IRS_WRITE_COMPLETE));
		}
	}

	m0_mutex_unlock(&ioreq->ir_nwxfer.nxr_lock);
	M0_LEAVE();
}

const struct m0_net_buffer_callbacks client_buf_bulk_cb  = {
	.nbc_cb = {
		[M0_NET_QT_PASSIVE_BULK_SEND] = m0_rpc_bulk_default_cb,
		[M0_NET_QT_PASSIVE_BULK_RECV] = client_passive_recv,
		[M0_NET_QT_ACTIVE_BULK_RECV]  = m0_rpc_bulk_default_cb,
		[M0_NET_QT_ACTIVE_BULK_SEND]  = m0_rpc_bulk_default_cb
	}
};

static int iofop_async_submit(struct m0_io_fop      *iofop,
			      struct m0_rpc_session *session)
{
	int                   rc;
	struct m0_fop_cob_rw *rwfop;
	struct io_req_fop    *reqfop;
	struct io_request    *req;
	struct m0_rpc_item   *item;

	M0_ENTRY("m0_io_fop %p m0_rpc_session %p", iofop, session);
	M0_PRE(iofop   != NULL);
	M0_PRE(session != NULL);

	rwfop = io_rw_get(&iofop->if_fop);
	M0_ASSERT(rwfop != NULL);

	reqfop = bob_of(iofop, struct io_req_fop, irf_iofop, &iofop_bobtype);
	req    = bob_of(reqfop->irf_tioreq->ti_nwxfer, struct io_request,
			ir_nwxfer, &ioreq_bobtype);

	rc = m0_rpc_bulk_store(&iofop->if_rbulk, session->s_conn,
			       rwfop->crw_desc.id_descs,
			       &client_buf_bulk_cb);
	if (rc != 0)
		goto out;

	iofop->if_fop.f_item.ri_session = session;
	item = &iofop->if_fop.f_item;
	item->ri_nr_sent_max     = M0T1FS_RPC_MAX_RETRIES;
	item->ri_resend_interval = M0T1FS_RPC_RESEND_INTERVAL;
	rc = m0_rpc_post(item);
	M0_LOG(M0_DEBUG, "[%p] IO fop %p, %p[%u], rbulk %p, submitted to rpc, "
	       "rc %d, ri_error %d", req, &iofop->if_fop, item,
	       item->ri_type->rit_opcode, &iofop->if_rbulk, rc, item->ri_error);
	/*
	 * Ignoring error from m0_rpc_post() so that the subsequent fop
	 * submission goes on. This is to ensure that the ioreq gets into dgmode
	 * subsequently without exiting from the healthy mode IO itself.
	 */

	return M0_RC(0);
	/*
	 * In case error is encountered either by m0_rpc_bulk_store() or
	 * m0_rpc_post(), queued net buffers, if any, will be deleted at
	 * io_req_fop_release.
	 */
out:
	return M0_RC(rc);
}

static void io_req_fop_release(struct m0_ref *ref)
{
	struct m0_fop          *fop;
	struct m0_io_fop       *iofop;
	struct io_req_fop      *reqfop;
	struct m0_rpc_bulk     *rbulk;
	struct nw_xfer_request *xfer;
	struct m0_fop_cob_rw   *rwfop;
	struct m0_rpc_machine  *rmach;
	struct m0_rpc_item     *item;
	struct io_request      *req;

	M0_ENTRY("ref %p", ref);
	M0_PRE(ref != NULL);

	fop    = container_of(ref, struct m0_fop, f_ref);
	rmach  = m0_fop_rpc_machine(fop);
	iofop  = container_of(fop, struct m0_io_fop, if_fop);
	reqfop = bob_of(iofop, struct io_req_fop, irf_iofop, &iofop_bobtype);
	rbulk  = &iofop->if_rbulk;
	xfer   = reqfop->irf_tioreq->ti_nwxfer;
	req    = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);
	item   = &fop->f_item;

	M0_LOG(M0_DEBUG, "[%p] fop %p, Pending fops %llu, Pending rdbulk %llu",
	       req, fop,
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_iofop_nr),
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_rdbulk_nr));
	M0_LOG(M0_DEBUG, "[%p] fop %p, "FID_F", %p[%u], ri_error %d, "
	       "rbulk %p", req, &iofop->if_fop,
	       FID_P(&reqfop->irf_tioreq->ti_fid), item,
	       item->ri_type->rit_opcode, item->ri_error, rbulk);

	/*
	 * Release the net buffers if rpc bulk object is still dirty.
	 * And wait on channel till all net buffers are deleted from
	 * transfer machine.
	 */
	m0_mutex_lock(&xfer->nxr_lock);
	m0_mutex_lock(&rbulk->rb_mutex);
	if (!m0_tlist_is_empty(&rpcbulk_tl, &rbulk->rb_buflist)) {
		struct m0_clink clink;
		size_t          buf_nr;
		size_t          non_queued_buf_nr;

		m0_clink_init(&clink, NULL);
		m0_clink_add(&rbulk->rb_chan, &clink);
		buf_nr = rpcbulk_tlist_length(&rbulk->rb_buflist);
		non_queued_buf_nr = m0_rpc_bulk_store_del_unqueued(rbulk);
		m0_mutex_unlock(&rbulk->rb_mutex);

		m0_rpc_bulk_store_del(rbulk);
		M0_LOG(M0_DEBUG, "[%p] fop %p, %p[%u], bulk %p, buf_nr %llu, "
		       "non_queued_buf_nr %llu", req, &iofop->if_fop, item,
		       item->ri_type->rit_opcode, rbulk,
		       (unsigned long long)buf_nr,
		       (unsigned long long)non_queued_buf_nr);
		if (m0_is_read_fop(&iofop->if_fop))
			m0_atomic64_sub(&xfer->nxr_rdbulk_nr,
				        non_queued_buf_nr);
		if (item->ri_sm.sm_state == M0_RPC_ITEM_UNINITIALISED)
			/* rio_replied() is not invoked for this item. */
			m0_atomic64_dec(&xfer->nxr_iofop_nr);
		m0_mutex_unlock(&xfer->nxr_lock);
		/*
		 * If there were some queued net bufs which had to be deleted,
		 * then it is required to wait for their callbacks.
		 */
		if (buf_nr > non_queued_buf_nr) {
			/*
			 * rpc_machine_lock may be needed from nlx_tm_ev_worker
			 * thread, which is going to wake us up. So we should
			 * release it to avoid deadlock.
			 */
			m0_rpc_machine_unlock(rmach);
			m0_chan_wait(&clink);
			m0_rpc_machine_lock(rmach);
		}
		m0_clink_del_lock(&clink);
		m0_clink_fini(&clink);
	} else {
		m0_mutex_unlock(&rbulk->rb_mutex);
		m0_mutex_unlock(&xfer->nxr_lock);
	}
	M0_ASSERT(m0_rpc_bulk_is_empty(rbulk));
	M0_LOG(M0_DEBUG, "[%p] fop %p, Pending fops %llu, Pending rdbulk %llu",
	       req, fop,
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_iofop_nr),
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_rdbulk_nr));
	M0_LOG(M0_DEBUG, "[%p] fop %p, "FID_F", %p[%u], ri_error %d, "
	       "rbulk %p", req, &iofop->if_fop,
	       FID_P(&reqfop->irf_tioreq->ti_fid), item,
	       item->ri_type->rit_opcode, item->ri_error, rbulk);

	rwfop = io_rw_get(&iofop->if_fop);
	M0_ASSERT(rwfop != NULL);
	io_req_fop_fini(reqfop);
	/* see io_req_fop_fini(). */
	io_req_fop_bob_fini(reqfop);
	m0_io_fop_fini(iofop);
	m0_free(reqfop);
	++iommstats.d_io_req_fop_nr;
}

static void cc_rpc_item_cb(struct m0_rpc_item *item)
{
	struct io_request          *req;
	struct cc_req_fop          *cc_fop;
	struct target_ioreq        *ti;
	struct m0_fop              *fop;
	struct m0_fop              *rep_fop;

	fop = m0_rpc_item_to_fop(item);
	cc_fop = container_of(fop, struct cc_req_fop, crf_fop);
	ti = container_of(cc_fop, struct target_ioreq, ti_cc_fop);
	req  = bob_of(ti->ti_nwxfer, struct io_request,
		      ir_nwxfer, &ioreq_bobtype);
	cc_fop->crf_ast.sa_cb = cc_bottom_half;
	cc_fop->crf_ast.sa_datum = (void *)ti;
	/* Reference on fop and its reply are released in cc_bottom_half. */
	m0_fop_get(fop);
	if (item->ri_reply != NULL) {
		rep_fop = m0_rpc_item_to_fop(item->ri_reply);
		m0_fop_get(rep_fop);
	}

	m0_sm_ast_post(req->ir_sm.sm_grp, &cc_fop->crf_ast);
}

static void cc_bottom_half(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct nw_xfer_request          *xfer;
	struct target_ioreq             *ti;
	struct cc_req_fop               *cc_fop;
	struct io_request               *req;
	struct m0_fop_cob_op_reply      *reply;
	struct m0_fop                   *reply_fop = NULL;
	struct m0t1fs_inode             *inode;
	struct m0t1fs_sb                *csb;
	struct m0_rpc_item              *req_item;
	struct m0_rpc_item              *reply_item;
	int                              rc;

	ti = (struct target_ioreq *)ast->sa_datum;
	req    = bob_of(ti->ti_nwxfer, struct io_request, ir_nwxfer,
			&ioreq_bobtype);
	xfer = ti->ti_nwxfer;
	cc_fop = &ti->ti_cc_fop;
	req_item = &cc_fop->crf_fop.f_item;
	reply_item = req_item->ri_reply;
	rc = req_item->ri_error;
	if (reply_item != NULL) {
		reply_fop = m0_rpc_item_to_fop(reply_item);
		rc = rc ?: m0_rpc_item_generic_reply_rc(reply_item);
	}
	if (rc < 0 || reply_item == NULL) {
		M0_ASSERT(ergo(reply_item == NULL, rc != 0));
		goto ref_dec;
	}

	reply = m0_fop_data(m0_rpc_item_to_fop(reply_item));
	/*
	 * Ignoring the case when an attempt is made to create a cob on target
	 * where previous IO had created it.
	 */
	rc = rc ? M0_IN(reply->cor_rc, (0, -EEXIST)) ? 0 : reply->cor_rc : 0;

	/*
	 * In case the conf is updated is revoked
	 * abort the ongoing request.
	 */
	inode = m0t1fs_file_to_m0inode(req->ir_file);
	csb = M0T1FS_SB(inode->ci_inode.i_sb);
	m0_mutex_lock(&csb->csb_confc_state.cus_lock);
	if (csb->csb_confc_state.cus_state != M0_CC_READY)
		rc = M0_ERR(-ESTALE);
	m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
ref_dec:
	if (ti->ti_rc == 0 && rc != 0)
		ti->ti_rc = rc;
	if (xfer->nxr_rc == 0 && rc != 0)
		xfer->nxr_rc = rc;
	m0_fop_put0_lock(&cc_fop->crf_fop);
	if (reply_fop != NULL)
		m0_fop_put0_lock(reply_fop);
	m0_mutex_lock(&xfer->nxr_lock);
	m0_atomic64_dec(&xfer->nxr_ccfop_nr);
	if (should_req_sm_complete(req))
		ioreq_sm_state_set_nolock(req, IRS_WRITE_COMPLETE);
	m0_mutex_unlock(&xfer->nxr_lock);
}

static bool should_req_sm_complete(struct io_request *req)
{
	struct m0t1fs_sb    *csb;
	struct m0t1fs_inode *inode;


	inode = m0t1fs_file_to_m0inode(req->ir_file);
	csb = M0T1FS_SB(inode->ci_inode.i_sb);

	return m0_atomic64_get(&req->ir_nwxfer.nxr_iofop_nr) == 0  &&
	    m0_atomic64_get(&req->ir_nwxfer.nxr_rdbulk_nr) == 0 &&
	    ((csb->csb_oostore && ioreq_sm_state(req) == IRS_WRITING) ?
	    m0_atomic64_get(&req->ir_nwxfer.nxr_ccfop_nr) == 0 : true);
}

static void io_rpc_item_cb(struct m0_rpc_item *item)
{
	struct m0_fop     *fop;
	struct m0_fop     *rep_fop;
	struct m0_io_fop  *iofop;
	struct io_req_fop *reqfop;
	struct io_request *ioreq;

	M0_PRE(item != NULL);
	M0_ENTRY("rpc_item %p[%u]", item, item->ri_type->rit_opcode);

	fop    = m0_rpc_item_to_fop(item);
	iofop  = container_of(fop, struct m0_io_fop, if_fop);
	reqfop = bob_of(iofop, struct io_req_fop, irf_iofop, &iofop_bobtype);
	ioreq  = bob_of(reqfop->irf_tioreq->ti_nwxfer, struct io_request,
			ir_nwxfer, &ioreq_bobtype);
	/*
	 * NOTE: RPC errors are handled in io_bottom_half(), which is called
	 * by reqfop->irf_ast.
	 */

	/*
	 * Acquire a reference on IO reply fop since its contents
	 * are needed for policy decisions in io_bottom_half().
	 * io_bottom_half() takes care of releasing the reference.
	 */
	if (item->ri_reply != NULL) {
		rep_fop = m0_rpc_item_to_fop(item->ri_reply);
		m0_fop_get(rep_fop);
	}

	M0_LOG(M0_INFO, "[%p] io_req_fop %p, target fid "FID_F" item %p[%u], "
	       "ri_error %d", ioreq, reqfop, FID_P(&reqfop->irf_tioreq->ti_fid),
	       item, item->ri_type->rit_opcode, item->ri_error);
	/*
	 * Acquire a reference on IO fop so that it does not get
	 * released until io_bottom_half() is executed for it.
	 * io_bottom_half() takes care of releasing the reference.
	 */
	m0_fop_get(&reqfop->irf_iofop.if_fop);
	m0_sm_ast_post(ioreq->ir_sm.sm_grp, &reqfop->irf_ast);
	M0_LEAVE();
}

M0_INTERNAL struct m0_file *m0_fop_to_file(struct m0_fop *fop)
{
	struct m0_io_fop  *iofop;
	struct io_req_fop *irfop;
	struct io_request *ioreq;

	iofop = container_of(fop, struct m0_io_fop, if_fop);
	irfop = bob_of(iofop, struct io_req_fop, irf_iofop, &iofop_bobtype);
	ioreq = bob_of(irfop->irf_tioreq->ti_nwxfer, struct io_request,
			ir_nwxfer, &ioreq_bobtype);

	return &m0t1fs_file_to_m0inode(ioreq->ir_file)->ci_flock;
}

M0_INTERNAL struct m0t1fs_sb *m0_fop_to_sb(struct m0_fop *fop)
{
	struct m0_io_fop  *iofop;
	struct io_req_fop *irfop;
	struct io_request *ioreq;

	iofop = container_of(fop, struct m0_io_fop, if_fop);
	irfop = bob_of(iofop, struct io_req_fop, irf_iofop, &iofop_bobtype);
	ioreq  = bob_of(irfop->irf_tioreq->ti_nwxfer, struct io_request,
			ir_nwxfer, &ioreq_bobtype);
	return file_to_sb(ioreq->ir_file);
}

static void io_bottom_half(struct m0_sm_group *grp, struct m0_sm_ast *ast)
{
	struct io_req_fop           *irfop;
	struct io_request           *req;
	struct target_ioreq         *tioreq;
	struct nw_xfer_request      *xfer;
	struct m0_io_fop            *iofop;
	struct m0_fop               *reply_fop = NULL;
	struct m0_rpc_item          *req_item;
	struct m0_rpc_item          *reply_item;
	struct m0_fop_cob_rw_reply  *rw_reply;
	struct m0_reqh_service_ctx  *ctx;
	struct m0t1fs_inode         *inode;
	struct m0t1fs_sb            *csb;
	struct m0_be_tx_remid       *remid;
	uint64_t                     actual_bytes = 0;
	int                          rc;

	M0_ENTRY("sm_group %p sm_ast %p", grp, ast);
	M0_PRE(grp != NULL);
	M0_PRE(ast != NULL);

	irfop  = bob_of(ast, struct io_req_fop, irf_ast, &iofop_bobtype);
	tioreq = irfop->irf_tioreq;
	req    = bob_of(tioreq->ti_nwxfer, struct io_request, ir_nwxfer,
			&ioreq_bobtype);
	xfer   = tioreq->ti_nwxfer;

	M0_ASSERT(xfer == &req->ir_nwxfer);
	M0_ASSERT(M0_IN(irfop->irf_pattr, (PA_DATA, PA_PARITY)));
	M0_ASSERT(M0_IN(ioreq_sm_state(req), (IRS_READING, IRS_WRITING,
					      IRS_DEGRADED_READING,
					      IRS_DEGRADED_WRITING,
					      IRS_FAILED)));
	M0_ASSERT(req->ir_file != NULL);

	iofop      = &irfop->irf_iofop;
	req_item   = &iofop->if_fop.f_item;
	reply_item = req_item->ri_reply;
	M0_LOG(M0_DEBUG, "[%p] nxr_iofop_nr %llu, nxr_rdbulk_nr %llu, "
		"req item %p[%u], ri_error %d", req,
		(unsigned long long)m0_atomic64_get(&xfer->nxr_iofop_nr),
		(unsigned long long)m0_atomic64_get(&xfer->nxr_rdbulk_nr),
		req_item, req_item->ri_type->rit_opcode, req_item->ri_error);

	rc = req_item->ri_error;
	if (reply_item != NULL) {
		rc = rc ?: m0_rpc_item_generic_reply_rc(reply_item);
	}
	if (rc < 0 || reply_item == NULL) {
		M0_ASSERT(ergo(reply_item == NULL, rc != 0));
		M0_LOG(M0_ERROR, "[%p] item %p, rc=%d", req, req_item, rc);
		goto ref_dec;
	}

	reply_fop = m0_rpc_item_to_fop(reply_item);
	M0_ASSERT(m0_is_io_fop_rep(reply_fop));

	rw_reply = io_rw_rep_get(reply_fop);
	rc = rw_reply->rwr_rc;
	remid = &rw_reply->rwr_mod_rep.fmr_remid;
	req->ir_sns_state = rw_reply->rwr_repair_done;
	M0_LOG(M0_DEBUG, "[%p] item %p[%u], reply received = %d, "
			 "sns state = %d", req, req_item,
			 req_item->ri_type->rit_opcode, rc, req->ir_sns_state);

	irfop->irf_reply_rc = rc;

	/* update pending transaction number */
	ctx = m0_reqh_service_ctx_from_session(reply_item->ri_session);
	inode = m0t1fs_file_to_m0inode(req->ir_file);
	csb = M0T1FS_SB(inode->ci_inode.i_sb);
	m0_mutex_lock(&csb->csb_confc_state.cus_lock);
	if (csb->csb_confc_state.cus_state != M0_CC_READY) {
		m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
		rc = M0_ERR(-ESTALE);
		goto ref_dec;
	}
	m0_mutex_unlock(&csb->csb_confc_state.cus_lock);
	m0t1fs_fsync_record_update(ctx, m0inode_to_sb(inode), inode, remid);
	actual_bytes = rw_reply->rwr_count;

ref_dec:
	/* For whatever reason, io didn't complete successfully.
	 * Clear read bulk count */
	if (rc < 0 && m0_is_read_fop(&iofop->if_fop))
		m0_atomic64_sub(&xfer->nxr_rdbulk_nr,
				m0_rpc_bulk_buf_length(&iofop->if_rbulk));
	if (tioreq->ti_rc == 0)
		tioreq->ti_rc = rc;

	/* For stale conf cache override the error. */
	if (rc == -ESTALE || (xfer->nxr_rc == 0 && rc != 0)) {
		xfer->nxr_rc = rc;
		M0_LOG(M0_ERROR, "[%p][type=%d] rc %d, tioreq->ti_rc %d, "
				 "nwxfer rc = %d @"FID_F,
				 req, req->ir_type, rc, tioreq->ti_rc,
				 xfer->nxr_rc, FID_P(&tioreq->ti_fid));
	}

	if (irfop->irf_pattr == PA_DATA)
		tioreq->ti_databytes += iofop->if_rbulk.rb_bytes;
	else
		tioreq->ti_parbytes  += iofop->if_rbulk.rb_bytes;

	M0_LOG(M0_INFO, "[%p] fop %p, Returned no of bytes = %llu, "
	       "expected = %llu", req, &iofop->if_fop, actual_bytes,
	       iofop->if_rbulk.rb_bytes);
	/* Drop reference on request and reply fop. */
	m0_fop_put0_lock(&iofop->if_fop);
	m0_fop_put0_lock(reply_fop);
	m0_atomic64_dec(&file_to_sb(req->ir_file)->csb_pending_io_nr);

	m0_mutex_lock(&xfer->nxr_lock);
	m0_atomic64_dec(&xfer->nxr_iofop_nr);
	if (should_req_sm_complete(req)) {
		ioreq_sm_state_set_nolock(req, (M0_IN(ioreq_sm_state(req),
			       (IRS_READING, IRS_DEGRADED_READING)) ?
			       IRS_READ_COMPLETE : IRS_WRITE_COMPLETE));
	}
	m0_mutex_unlock(&xfer->nxr_lock);

	M0_LOG(M0_DEBUG, "[%p] item %p, ref %llu, "FID_F", Pending fops %llu, "
	       "Pending rdbulk %llu", req, req_item,
	       (unsigned long long)m0_ref_read(&iofop->if_fop.f_ref),
	       FID_P(&tioreq->ti_fid), m0_atomic64_get(&xfer->nxr_iofop_nr),
	       m0_atomic64_get(&xfer->nxr_rdbulk_nr));
	M0_LEAVE();
}

static int nw_xfer_req_dispatch(struct nw_xfer_request *xfer)
{
	int                      rc = 0;
	struct io_req_fop       *irfop;
	struct io_request       *req;
	struct target_ioreq     *ti;
	struct m0t1fs_sb        *csb;
	uint64_t                 nr_dispatched = 0;
	int                      post_error = 0;
	int                      ri_error;

	M0_ENTRY();

	M0_PRE(xfer != NULL);
	req = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);

	M0_LOG(M0_DEBUG, "[%p]", req);
	M0_ASSERT(nw_xfer_request_invariant(xfer));
	csb = req->ir_file->f_path.mnt->mnt_sb->s_fs_info;
	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		if (ti->ti_state != M0_PNDS_ONLINE) {
			M0_LOG(M0_INFO, "[%p] Skipped iofops prepare for "FID_F,
			       req, FID_P(&ti->ti_fid));
			continue;
		}
		if (target_ioreq_type_get(ti) == TI_COB_CREATE &&
		    ioreq_sm_state(req) == IRS_WRITING) {
			rc = ti->ti_ops->tio_cc_fops_prepare(ti);
			if (rc != 0)
				return M0_ERR_INFO(rc, "[%p] cob create fop"
						   "failed", req);
			continue;
		}
		rc = ti->ti_ops->tio_iofops_prepare(ti, PA_DATA);
		if (rc != 0)
			return M0_ERR_INFO(rc, "[%p] data fop failed", req);

		rc = ti->ti_ops->tio_iofops_prepare(ti, PA_PARITY);
		if (rc != 0)
			return M0_ERR_INFO(rc, "[%p] parity fop failed", req);
	} m0_htable_endfor;

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		/* Skips the target device if it is not online. */
		if (ti->ti_state != M0_PNDS_ONLINE) {
			M0_LOG(M0_INFO, "[%p] Skipped device "FID_F,
			       req, FID_P(&ti->ti_fid));
			continue;
		}
		M0_LOG(M0_DEBUG, "[%p] Before Submitting fops for device "FID_F
		       ", fops length of ti %u, total fops nr %llu", req,
		       FID_P(&ti->ti_fid),
		      (int)iofops_tlist_length(&ti->ti_iofops),
		      m0_atomic64_get(&xfer->nxr_iofop_nr));

		if (target_ioreq_type_get(ti) == TI_COB_CREATE &&
		    ioreq_sm_state(req) == IRS_WRITING) {
			/*
			 * An error returned by rpc post has been ignored.
			 * It will be handled in the respective bottom half.
			 */
			rc = m0_rpc_post(&ti->ti_cc_fop.crf_fop.f_item);
			continue;
		}
		m0_tl_for (iofops, &ti->ti_iofops, irfop) {
			rc = iofop_async_submit(&irfop->irf_iofop,
						ti->ti_session);
			ri_error = irfop->irf_iofop.if_fop.f_item.ri_error;
			M0_LOG(M0_DEBUG, "[%p] Submitted fops for device "
			       FID_F"@%p, item %p, fops nr=%llu, rc=%d, "
			       "ri_error=%d", req, FID_P(&ti->ti_fid), irfop,
			       &irfop->irf_iofop.if_fop.f_item,
			       m0_atomic64_get(&xfer->nxr_iofop_nr), rc,
			       ri_error);
			if (rc != 0)
				goto out;

			m0_atomic64_inc(&file_to_sb(req->ir_file)->
					csb_pending_io_nr);
			if (ri_error == 0)
				M0_CNT_INC(nr_dispatched);
			else if (post_error == 0)
				post_error = ri_error;
		} m0_tl_endfor;

	} m0_htable_endfor;

out:
	if (rc == 0 && nr_dispatched == 0 && post_error == 0) {
		/* No fop has been dispatched.
		 *
		 * This might happen in dgmode reading:
		 *    In 'parity verify' mode, a whole parity group, including
		 *    data and parity units are all read from ioservices.
		 *    If some units failed to read, no need to read extra unit.
		 *    The units needed for recovery are ready.
		 */
		M0_ASSERT(ioreq_sm_state(req) == IRS_DEGRADED_READING);
		M0_ASSERT(req->ir_type == IRT_READ && csb->csb_verify);
		ioreq_sm_state_set(req, IRS_READ_COMPLETE);
	} else if (rc == 0)
		xfer->nxr_state = NXS_INFLIGHT;
	M0_LOG(M0_DEBUG, "[%p] nxr_iofop_nr %llu, nxr_rdbulk_nr %llu, "
	       "nr_dispatched %llu", req,
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_iofop_nr),
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_rdbulk_nr),
	       (unsigned long long)nr_dispatched);

	return M0_RC(rc);
}

static void nw_xfer_req_complete(struct nw_xfer_request *xfer, bool rmw)
{
	struct io_request   *req;
	struct target_ioreq *ti;
	struct io_req_fop   *irfop;
	struct m0_fop       *fop;
	struct m0_rpc_item  *item;
	struct m0t1fs_inode *inode;
	struct m0t1fs_sb    *csb;

	M0_ENTRY("nw_xfer_request %p, rmw %s", xfer,
		 rmw ? (char *)"true" : (char *)"false");
	M0_PRE(xfer != NULL);

	xfer->nxr_state = NXS_COMPLETE;
	req = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);
	inode = m0t1fs_file_to_m0inode(req->ir_file);
	csb = M0T1FS_SB(inode->ci_inode.i_sb);

	M0_LOG(M0_DEBUG, "[%p] nxr_iofop_nr %llu, nxr_rdbulk_nr %llu, "
	       "rmw %s", req,
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_iofop_nr),
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_rdbulk_nr),
	       rmw ? (char *)"true" : (char *)"false");

	m0_htable_for(tioreqht, ti, &xfer->nxr_tioreqs_hash) {
		/* Maintains only the first error encountered. */
		if (xfer->nxr_rc == 0) {
			xfer->nxr_rc = ti->ti_rc;
			M0_LOG(M0_DEBUG, "[%p] nwxfer rc = %d",
			       req, xfer->nxr_rc);
		}

		xfer->nxr_bytes += ti->ti_databytes;
		ti->ti_databytes = 0;

		if (csb->csb_oostore && ti->ti_req_type == TI_COB_CREATE &&
		    ioreq_sm_state(req) == IRS_WRITE_COMPLETE) {
			target_ioreq_type_set(ti, TI_NONE);
			m0_fop_put_lock(&ti->ti_cc_fop.crf_fop);
			continue;
		}
		m0_tl_teardown(iofops, &ti->ti_iofops, irfop) {
			fop = &irfop->irf_iofop.if_fop;
			item = m0_fop_to_rpc_item(fop);
			M0_LOG(M0_DEBUG, "[%p] fop %p, ref %llu, "
			       "item %p[%u], ri_error %d, ri_state %d",
			       req, fop,
			       (unsigned long long)m0_ref_read(&fop->f_ref),
			       item, item->ri_type->rit_opcode, item->ri_error,
			       item->ri_sm.sm_state);

			/* Maintains only the first error encountered. */
			if (xfer->nxr_rc == 0 &&
			    item->ri_sm.sm_state == M0_RPC_ITEM_FAILED) {
				xfer->nxr_rc = item->ri_error;
				M0_LOG(M0_DEBUG, "[%p] nwxfer rc = %d",
				       req, xfer->nxr_rc);
			}

			M0_ASSERT(ergo(item->ri_sm.sm_state !=
				       M0_RPC_ITEM_UNINITIALISED,
				       item->ri_rmachine != NULL));
			if (item->ri_rmachine == NULL) {
				M0_ASSERT(ti->ti_session != NULL);
				m0_fop_rpc_machine_set(fop,
					ti->ti_session->s_conn->c_rpc_machine);
			}

			M0_LOG(M0_DEBUG, "[%p] item %p, target fid "
					 FID_F"fop %p, "
			       "ref %llu", req, item, FID_P(&ti->ti_fid), fop,
			       (unsigned long long)m0_ref_read(&fop->f_ref));
			m0_fop_put_lock(fop);
		}
	} m0_htable_endfor;

	M0_LOG(M0_INFO, "[%p] Number of bytes %s = %llu",
			req, req->ir_type == IRT_READ? "read" : "written",
			xfer->nxr_bytes);

	M0_LOG(M0_DEBUG, "[%p] nxr_rc %d, nxr_iofop_nr %llu, "
	       "nxr_rdbulk_nr %llu", req, xfer->nxr_rc,
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_iofop_nr),
	       (unsigned long long)m0_atomic64_get(&xfer->nxr_rdbulk_nr));
	M0_ASSERT(ergo(xfer->nxr_rc == 0, nw_xfer_request_invariant(xfer)));

	/*
	 * This function is invoked from 4 states - IRS_READ_COMPLETE,
	 * IRS_WRITE_COMPLETE, IRS_DEGRADED_READING, IRS_DEGRADED_WRITING.
	 * And the state change is applicable only for healthy state IO,
	 * meaning for states IRS_READ_COMPLETE and IRS_WRITE_COMPLETE.
	 */
	if (M0_IN(ioreq_sm_state(req),
		  (IRS_READ_COMPLETE, IRS_WRITE_COMPLETE))) {
		if (!rmw)
			ioreq_sm_state_set(req, IRS_REQ_COMPLETE);
		else if (ioreq_sm_state(req) == IRS_READ_COMPLETE)
			xfer->nxr_bytes = 0;
	}
	req->ir_rc = xfer->nxr_rc;
	M0_LEAVE();
}

/**
 * Degraded mode read support for IO request fop.
 * Invokes degraded mode read support routines for upper
 * data structures like pargrp_iomap.
 */
static int io_req_fop_dgmode_read(struct io_req_fop *irfop)
{
	int                         rc;
	uint32_t                    cnt;
	uint32_t                    seg;
	uint32_t                    seg_nr;
	uint64_t                    grpid;
	uint64_t                    pgcur = 0;
	m0_bindex_t                *index;
	struct io_request          *req;
	struct m0_fop              *fop;
	struct m0_rpc_bulk         *rbulk;
	struct pargrp_iomap        *map = NULL;
	struct m0_rpc_bulk_buf     *rbuf;

	M0_PRE(irfop != NULL);

	req   = bob_of(irfop->irf_tioreq->ti_nwxfer, struct io_request,
		       ir_nwxfer, &ioreq_bobtype);
	rbulk = &irfop->irf_iofop.if_rbulk;
	fop   = &irfop->irf_iofop.if_fop;

	M0_ENTRY("[%p] target fid "FID_F", fop %p, %p[%u] ", req,
		 FID_P(&irfop->irf_tioreq->ti_fid), fop,
		 &fop->f_item, m0_fop_opcode(fop));

	m0_tl_for (rpcbulk, &rbulk->rb_buflist, rbuf) {

		index  = rbuf->bb_zerovec.z_index;
		seg_nr = rbuf->bb_zerovec.z_bvec.ov_vec.v_nr;

		for (seg = 0; seg < seg_nr; ) {

			grpid = pargrp_id_find(index[seg], req, irfop);
			for (cnt = 1, ++seg; seg < seg_nr; ++seg) {

				M0_ASSERT(ergo(seg > 0, index[seg] >
					       index[seg - 1]));
				M0_ASSERT((index[seg] & ~PAGE_MASK) == 0);

				if (grpid ==
				    pargrp_id_find(index[seg], req, irfop))
					++cnt;
				else
					break;
			}
			ioreq_pgiomap_find(req, grpid, &pgcur, &map);
			M0_ASSERT(map != NULL);
			rc = map->pi_ops->pi_dgmode_process(map,
					irfop->irf_tioreq, &index[seg - cnt],
					cnt);
			if (rc != 0)
				return M0_ERR_INFO(rc, "[%p] fop %p, %p[%u] "
					"Parity group dgmode process failed",
					req, fop, &fop->f_item,
					m0_fop_opcode(fop));
		}
	} m0_tl_endfor;
	return M0_RC(0);
}

/*
 * Used in precomputing io fop size while adding rpc bulk buffer and
 * data buffers.
 */
static inline uint32_t io_desc_size(struct m0_net_domain *ndom)
{
	return
		/* size of variables ci_nr and nbd_len */
		M0_MEMBER_SIZE(struct m0_io_indexvec, ci_nr) +
		M0_MEMBER_SIZE(struct m0_net_buf_desc, nbd_len) +
		/* size of nbd_data */
		m0_net_domain_get_max_buffer_desc_size(ndom);
}

static inline uint32_t io_seg_size(void)
{
	return sizeof(struct m0_ioseg);
}

static uint32_t io_di_size(const struct io_request *req)
{
	struct m0_file *file;

	file = &m0t1fs_file_to_m0inode(req->ir_file)->ci_flock;
	if (file->fi_di_ops->do_out_shift(file) == 0)
		return 0;
	return file->fi_di_ops->do_out_shift(file) * M0_DI_ELEMENT_SIZE;
}

static int bulk_buffer_add(struct io_req_fop       *irfop,
			   struct m0_net_domain    *dom,
			   struct m0_rpc_bulk_buf **rbuf,
			   uint32_t                *delta,
			   uint32_t                 maxsize)
{
	int                      rc;
	int                      seg_nr;
	struct io_request       *req;
	struct m0_indexvec_varr *ivv;

	M0_PRE(irfop  != NULL);
	M0_PRE(dom    != NULL);
	M0_PRE(rbuf   != NULL);
	M0_PRE(delta  != NULL);
	M0_PRE(maxsize > 0);
	M0_ENTRY("io_req_fop %p net_domain %p delta_size %d",
		 irfop, dom, *delta);

	req     = bob_of(irfop->irf_tioreq->ti_nwxfer, struct io_request,
			 ir_nwxfer, &ioreq_bobtype);

	if (M0_IN(ioreq_sm_state(req), (IRS_READING, IRS_WRITING))) {
		ivv = &irfop->irf_tioreq->ti_ivv;
	} else {
		ivv = &irfop->irf_tioreq->ti_dgvec->dr_ivec_varr;
	}

	seg_nr  = min32(m0_net_domain_get_max_buffer_segments(dom),
		        V_SEG_NR(ivv));
	*delta += io_desc_size(dom);

	if (m0_io_fop_size_get(&irfop->irf_iofop.if_fop) + *delta < maxsize) {

		rc = m0_rpc_bulk_buf_add(&irfop->irf_iofop.if_rbulk, seg_nr, 0,
					 dom, NULL, rbuf);
		if (rc != 0) {
			*delta -= io_desc_size(dom);
			return M0_ERR_INFO(rc, "[%p] Failed to add "
					   "rpc_bulk_buffer", req);
		}
	} else {
		rc      = M0_ERR(-ENOSPC);
		*delta -= io_desc_size(dom);
	}

	M0_POST(ergo(rc == 0, *rbuf != NULL));
	return M0_RC(rc);
}

static void cc_fop_release(struct m0_ref *ref)
{
	struct m0_fop *fop;

	M0_ENTRY();
	fop  = container_of(ref, struct m0_fop, f_ref);
	m0_fop_fini(fop);
	M0_LEAVE();
}

static int target_cob_create_fop_prepare(struct target_ioreq *ti)
{
	struct m0_fop            *fop;
	struct m0_fop_cob_common *common;
	struct io_request        *req;
	int                       rc;

	M0_PRE(ti->ti_req_type == TI_COB_CREATE);
	fop = &ti->ti_cc_fop.crf_fop;
	m0_fop_init(fop, &m0_fop_cob_create_fopt, NULL, cc_fop_release);
	rc = m0_fop_data_alloc(fop);
	if (rc != 0) {
		m0_fop_fini(fop);
		goto out;
	}
	fop->f_item.ri_rmachine = m0_fop_session_machine(ti->ti_session);

	fop->f_item.ri_session         = ti->ti_session;
	fop->f_item.ri_ops             = &cc_item_ops;
	fop->f_item.ri_nr_sent_max     = M0T1FS_RPC_MAX_RETRIES;
	fop->f_item.ri_resend_interval = M0T1FS_RPC_RESEND_INTERVAL;
	req = bob_of(ti->ti_nwxfer, struct io_request, ir_nwxfer,
		     &ioreq_bobtype);
	common = m0_cobfop_common_get(fop);
	common->c_gobfid = *file_to_fid(req->ir_file);
	common->c_cobfid = ti->ti_fid;
	common->c_pver = m0t1fs_file_to_m0inode(req->ir_file)->ci_pver;
	common->c_cob_type = M0_COB_IO;
	common->c_cob_idx = m0_fid_cob_device_id(&ti->ti_fid);
	common->c_flags |= M0_IO_FLAG_CROW;
	common->c_body.b_pver = m0t1fs_file_to_m0inode(req->ir_file)->ci_pver;
	common->c_body.b_nlink = 1;
	common->c_body.b_valid |= M0_COB_PVER;
	common->c_body.b_valid |= M0_COB_NLINK;
	common->c_body.b_valid |= M0_COB_LID;
	common->c_body.b_lid = m0t1fs_file_to_m0inode(req->ir_file)->ci_layout_id;
	m0_atomic64_inc(&ti->ti_nwxfer->nxr_ccfop_nr);

out:
	return M0_RC(rc);
}

static int target_ioreq_iofops_prepare(struct target_ioreq *ti,
				       enum page_attr       filter)
{
	int                      rc = 0;
	uint32_t                 seg = 0;
	/* Number of segments in one m0_rpc_bulk_buf structure. */
	uint32_t                 bbsegs;
	uint32_t                 maxsize;
	uint32_t                 delta;
	enum page_attr           rw;
	struct m0_varr          *pattr;
	struct m0_indexvec_varr *bvec;
	struct io_request       *req;
	struct m0_indexvec_varr *ivv = NULL;
	struct io_req_fop       *irfop;
	struct m0_net_domain    *ndom;
	struct m0_rpc_bulk_buf  *rbuf;
	struct m0_io_fop        *iofop;
	struct m0_fop_cob_rw    *rw_fop;
	struct nw_xfer_request  *xfer;

	M0_PRE_EX(target_ioreq_invariant(ti));
	M0_PRE(M0_IN(filter, (PA_DATA, PA_PARITY)));

	xfer = ti->ti_nwxfer;
	req  = bob_of(xfer, struct io_request, ir_nwxfer, &ioreq_bobtype);

	M0_ASSERT(M0_IN(ioreq_sm_state(req),
		  (IRS_READING, IRS_DEGRADED_READING,
		   IRS_WRITING, IRS_DEGRADED_WRITING)));

	M0_ENTRY("[%p] prepare io fops for target ioreq %p filter 0x%x, tfid "
		 FID_F, req, ti, filter, FID_P(&ti->ti_fid));

	rc = m0_rpc_session_validate(ti->ti_session);
	if (rc != 0 && rc != -ECANCELED)
		return M0_ERR(rc);

	if (M0_IN(ioreq_sm_state(req), (IRS_READING, IRS_WRITING))) {
		ivv   = &ti->ti_ivv;
		bvec  = &ti->ti_bufvec;
		pattr = &ti->ti_pageattrs;
	} else {
		if (ti->ti_dgvec == NULL) {
			return M0_RC(0);
		}
		ivv   = &ti->ti_dgvec->dr_ivec_varr;
		bvec  = &ti->ti_dgvec->dr_bufvec;
		pattr = &ti->ti_dgvec->dr_pageattrs;
	}

	ndom = ti->ti_session->s_conn->c_rpc_machine->rm_tm.ntm_dom;
	rw = ioreq_sm_state(req) == IRS_DEGRADED_WRITING ? PA_DGMODE_WRITE :
	     ioreq_sm_state(req) == IRS_WRITING ? PA_WRITE :
	     ioreq_sm_state(req) == IRS_DEGRADED_READING ? PA_DGMODE_READ :
	     PA_READ;
	maxsize = m0_rpc_session_get_max_item_payload_size(ti->ti_session);

	while (seg < V_SEG_NR(ivv)) {

		delta  = 0;
		bbsegs = 0;

		M0_LOG(M0_DEBUG, "[%p] seg=%u@%u pageattr=0x%x, filter=0x%x, "
		       "rw=0x%x",
		       req, seg, V_SEG_NR(ivv),
		       PA(pattr, seg), filter, rw);

		if (!(PA(pattr, seg) & filter) || !(PA(pattr, seg) & rw)) {
			M0_LOG(M0_DEBUG, "[%p] skipping, pageattr = 0x%x, "
			       "filter = 0x%x, rw = 0x%x",
			       req, PA(pattr, seg), filter, rw);
			++seg;
			continue;
		}
		M0_ALLOC_PTR(irfop);
		if (irfop == NULL) {
			rc = M0_ERR(-ENOMEM);
			goto err;
		}
		rc = io_req_fop_init(irfop, ti, filter);
		if (rc != 0) {
			m0_free(irfop);
			goto err;
		}
		++iommstats.a_io_req_fop_nr;

		iofop = &irfop->irf_iofop;
		rw_fop = io_rw_get(&iofop->if_fop);

		rc = bulk_buffer_add(irfop, ndom, &rbuf, &delta, maxsize);
		if (rc != 0) {
			io_req_fop_fini(irfop);
			m0_free(irfop);
			goto err;
		}
		delta += io_seg_size();

		/*
		 * Adds io segments and io descriptor only if it fits within
		 * permitted size.
		 */
		while (seg < V_SEG_NR(ivv) &&
		       m0_io_fop_size_get(&iofop->if_fop) + delta < maxsize) {

			M0_LOG(M0_DEBUG, "[%p] adding: seg=%u@%u pa=0x%x, "
			       "filter=0x%x, rw=0x%x", req, seg,
			       V_SEG_NR(ivv),
			       PA(pattr, seg), filter, rw);

			/*
			 * Adds a page to rpc bulk buffer only if it passes
			 * through the filter.
			 */
			if ((PA(pattr, seg) & rw) && (PA(pattr, seg) & filter)) {
				delta += io_seg_size() + io_di_size(req);

				rc = m0_rpc_bulk_buf_databuf_add(rbuf,
						V_ADDR (bvec, seg),
						V_COUNT(ivv, seg),
						V_INDEX(ivv, seg),
						ndom);

				if (rc == -EMSGSIZE) {

					/*
					 * Fix the number of segments in
					 * current m0_rpc_bulk_buf structure.
					 */
					rbuf->bb_nbuf->nb_buffer.ov_vec.v_nr =
						bbsegs;
					rbuf->bb_zerovec.z_bvec.ov_vec.v_nr =
						bbsegs;
					bbsegs = 0;

					delta -= io_seg_size() -
							io_di_size(req);
					rc     = bulk_buffer_add(irfop, ndom,
							&rbuf, &delta, maxsize);
					if (rc == -ENOSPC)
						break;
					else if (rc != 0)
						goto fini_fop;

					/*
					 * Since current bulk buffer is full,
					 * new bulk buffer is added and
					 * existing segment is attempted to
					 * be added to new bulk buffer.
					 */
					continue;
				} else if (rc == 0)
					++bbsegs;
			}
			++seg;
		}

		if (m0_io_fop_byte_count(iofop) == 0) {
			irfop_fini(irfop);
			continue;
		}

		rbuf->bb_nbuf->nb_buffer.ov_vec.v_nr = bbsegs;
		rbuf->bb_zerovec.z_bvec.ov_vec.v_nr = bbsegs;

		rw_fop->crw_fid = ti->ti_fid;
		rw_fop->crw_index = ti->ti_obj;
		rw_fop->crw_pver =
			m0t1fs_file_to_m0inode(req->ir_file)->ci_pver;
		rw_fop->crw_lid = m0t1fs_file_to_m0inode(req->ir_file)->ci_layout_id;

		rc = m0_io_fop_prepare(&iofop->if_fop);
		if (rc != 0)
			goto fini_fop;

		if (m0_is_read_fop(&iofop->if_fop))
			m0_atomic64_add(&xfer->nxr_rdbulk_nr,
					m0_rpc_bulk_buf_length(
					&iofop->if_rbulk));

		m0_atomic64_inc(&xfer->nxr_iofop_nr);
		iofops_tlist_add(&ti->ti_iofops, irfop);

		M0_LOG(M0_DEBUG, "[%p] fop=%p bulk=%p (%s) @"FID_F
		       " pending io fops = %llu, pending read bulks = %llu "
		       "list_len=%d",
		       req, &iofop->if_fop, &iofop->if_rbulk,
		       m0_is_read_fop(&iofop->if_fop) ? "r" : "w",
		       FID_P(&ti->ti_fid),
		       m0_atomic64_get(&xfer->nxr_iofop_nr),
		       m0_atomic64_get(&xfer->nxr_rdbulk_nr),
		       (int)iofops_tlist_length(&ti->ti_iofops));
	}

	return M0_RC(0);
fini_fop:
	irfop_fini(irfop);
err:
	m0_tl_teardown(iofops, &ti->ti_iofops, irfop) {
		irfop_fini(irfop);
	}

	return M0_ERR_INFO(rc, "[%p] iofops_prepare failed", req);
}

const struct inode_operations m0t1fs_reg_inode_operations = {
	.setattr     = m0t1fs_setattr,
	.getattr     = m0t1fs_getattr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,9,0)
	.setxattr    = m0t1fs_setxattr,
	.getxattr    = m0t1fs_getxattr,
	.removexattr = m0t1fs_removexattr,
#endif
	.listxattr   = m0t1fs_listxattr,
};


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
static ssize_t m0t1fs_direct_IO(struct kiocb *kcb,
				struct iov_iter *from)
{
	struct m0_indexvec_varr *ivv;
	ssize_t                  retval;
	loff_t                   size;
	int                      seg;
	int                      rw;

	M0_THREAD_ENTER;
	M0_ENTRY();
	rw = iov_iter_rw(from);
	M0_LOG(M0_DEBUG, "m0t1fs_direct_IO: rw=%s pos=%lld seg_nr=%lu "
	       "addr=%p len=%lu", rw == READ ? "READ" : "WRITE",
	       (long long)kcb->ki_pos, from->nr_segs, from->iov->iov_base,
	        from->iov->iov_len);

	M0_PRE(M0_IN(rw, (READ, WRITE)));

	size = i_size_read(m0t1fs_file_to_inode(kcb->ki_filp));
	ivv = indexvec_create(from->nr_segs, from->iov, kcb->ki_pos);
	if (ivv == NULL)
		return M0_ERR(-ENOMEM);
	if (rw == READ) {
		/* Truncate vector to eliminate reading beyond the EOF */
		for (seg = 0; seg < V_SEG_NR(ivv); ++seg)
			if (v_seg_endpos(ivv, seg) > size) {
				V_SEG_NR(ivv) = seg + 1;
				V_COUNT(ivv, seg) = size - V_INDEX(ivv, seg);
				break;
			}
	}

	retval = m0t1fs_aio(kcb, from->iov, ivv, rw == READ ? IRT_READ : IRT_WRITE);

	/*
	 * m0t1fs_direct_IO() must process all requested data or return error.
	 * Otherwise generic kernel code will use unimplemented callbacks to
	 * continue buffered I/O (e.g. write_begin()).
	 */
	M0_ASSERT_INFO(retval < 0 || retval == indexvec_varr_count(ivv),
		       "%" PRIi64 " != %" PRIi64, (int64_t)retval,
		       indexvec_varr_count(ivv));

	m0_indexvec_varr_free(ivv);
	m0_free(ivv);
	M0_LEAVE();
	return retval;
}


#else
static ssize_t m0t1fs_direct_IO(int rw,
				struct kiocb *kcb,
				const struct iovec *iov,
				loff_t pos,
				unsigned long seg_nr)
{
	struct m0_indexvec_varr *ivv;
	ssize_t             retval;
	loff_t              size;
	int                 seg;

	M0_THREAD_ENTER;
	M0_ENTRY();
	M0_LOG(M0_DEBUG, "m0t1fs_direct_IO: rw=%s pos=%lld seg_nr=%lu "
	       "addr=%p len=%lu", rw == READ ? "READ" : "WRITE",
	       (long long)pos, seg_nr, iov->iov_base, iov->iov_len);

	M0_PRE(M0_IN(rw, (READ, WRITE)));

	size = i_size_read(m0t1fs_file_to_inode(kcb->ki_filp));
	ivv = indexvec_create(seg_nr, iov, pos);
	if (ivv == NULL)
		return M0_ERR(-ENOMEM);
	if (rw == READ) {
		/* Truncate vector to eliminate reading beyond the EOF */
		for (seg = 0; seg < V_SEG_NR(ivv); ++seg)
			if (v_seg_endpos(ivv, seg) > size) {
				V_SEG_NR(ivv) = seg + 1;
				V_COUNT(ivv, seg) = size - V_INDEX(ivv, seg);
				break;
			}
	}

	retval = m0t1fs_aio(kcb, iov, ivv, rw == READ ? IRT_READ : IRT_WRITE);

	/*
	 * m0t1fs_direct_IO() must process all requested data or return error.
	 * Otherwise generic kernel code will use unimplemented callbacks to
	 * continue buffered I/O (e.g. write_begin()).
	 */
	M0_ASSERT_INFO(retval < 0 || retval == indexvec_varr_count(ivv),
		       "%" PRIi64 " != %" PRIi64, (int64_t)retval,
		       indexvec_varr_count(ivv));

	m0_indexvec_varr_free(ivv);
	m0_free(ivv);
	M0_LEAVE();
	return retval;
}
#endif

const struct address_space_operations m0t1fs_aops = {
	.direct_IO = m0t1fs_direct_IO,
};

#undef M0_TRACE_SUBSYSTEM
