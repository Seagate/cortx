/* -*- C -*- */
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
 * Original author: Rajesh Bhalerao <rajesh_bhalerao@xyratex.com>
 * Original creation date: 03/12/2013
 */

#undef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FILE

#include "lib/trace.h"
#include "lib/arith.h"
#include "fid/fid_xc.h"
#include "xcode/xcode.h"
#include "rm/rm.h"
#include "file/file.h"

/**
   @page FileLock Distributed File Lock DLD

   - @ref FileLockDLD-ovw
   - @ref FileLockDLD-req
   - @ref FileLockDLD-depends
   - @subpage FileLockDLD-fspec "Functional Specification" <!-- Note @subpage -->
   - @ref FileLockDLD-lspec
      - @ref FileLockDLD-lspec-comps
      - @ref FileLockDLD-lspec-numa
   - @ref FileLockDLD-conformance
   - @ref FileLockDLD-ut
   - @ref FileLockDLD-st
   - @ref FileLockDLD-ref
   - @ref FileLockDLD-impl-plan


   <hr>
   @section FileLockDLD-ovw Overview

   This DLD describes the implementation of a distributed file lock. The
   distributed file lock is implemented over Mero resource manager. Mero
   resource manager already provides a distributed resource management
   framework within a cluster (provides semantics similar to a distributed
   lock manager).

   <b>Purpose of a DLD</b> @n
   The purpose of the Detailed Level Design (DLD) specification of a
   distributed file lock is to:
   - Implement file lock resource type of Mero resource manager
   - Implement distributed file lock interfaces

   <hr>
   @section FileLockDLD-req Requirements

   - @b R.rm-file-lock.async The distributed file lock should provide async
   interfaces.
   - @b R.rm-file-lock.RM The distributed file lock shall use RM
   (resource manager) to implement the interfaces.

   @section FileLockDLD-lspec Logical Specification

   - @ref FileLockDLD-lspec-comps
   - @ref FileLockInternal  <!-- Note link -->
   - @ref FileLockDLD-lspec-numa


   @subsection FileLockDLD-lspec-comps Component Overview
   The distributed file lock implements following resource manager ops:
   - resource type ops
   - resource ops
   - resource credit ops

   <hr>
   @section FileLockDLD-conformance Conformance

   - @b I.rm-file-lock.async The interfaces are async.
   - @b I.rm-file-lock.RM  Implements RM ops to implement file lock resource

   <hr>
   @section FileLockDLD-ut Unit Tests

   Following scenarios will be tested:
   @test
   1) Lock usage when no other thread is using the lock
   - wait mode: gets the lock

   @test
   2) Lock usage when a local thread is holding the lock
   - wait mode: gets the lock when the other thread releases the lock

   @test
   3) Lock usage when a remote thread is holding the lock
   - wait mode: gets the lock when the other thread releases the lock

   @test
   4) lib/ut/mutex.c like test
   - A set of arbitrary thread perform lock, unlock operations.
   Verify the number of operations match the expected result.

   <hr>
   @section FileLockDLD-st System Tests

   System tests will be performed by the subsystem that uses the distributed
   file lock.

   <hr>
   @section FileLockDLD-ref References

   - <a href="https://docs.google.com/a/seagate.com/document/d/1WYw8MmItpp0KuBbY
fuQQxJaw9UN8OuHKnlICszB8-Zs/edit">HLD of resource manager Interfaces</a>,

   <hr>
   @section FileLockDLD-impl-plan Implementation Plan

   It implements:
   - m0_rm_resource_type_ops
   - m0_rm_resource_ops
   - m0_rm_credit_ops
   - m0_rm_file_lock_* external interfaces

 */

/**
   @defgroup FileLockInternal Distributed File Lock Internals
   @ingroup FileLock

   This section contains the functions that are internal to the distributed
   file lock. They implement various resource manager ops.

   @see @ref FileLockDLD-ovw and @ref FileLockDLD-lspec

   @{
 */

/* Forward Declarations */
static bool file_lock_equal(const struct m0_rm_resource *resource0,
			    const struct m0_rm_resource *resource1);
static m0_bcount_t file_lock_len(const struct m0_rm_resource *resource);
static int file_lock_encode(struct m0_bufvec_cursor     *cur,
			    const struct m0_rm_resource *resource);
static int file_lock_decode(struct m0_bufvec_cursor *cur,
			    struct m0_rm_resource  **resource);
static void file_lock_credit_init(struct m0_rm_resource *resource,
				  struct m0_rm_credit   *credit);
static void file_lock_resource_free(struct m0_rm_resource *resource);

static void file_lock_incoming_complete(struct m0_rm_incoming *in, int32_t rc);
static void file_lock_incoming_conflict(struct m0_rm_incoming *in);

static bool file_lock_cr_intersects(const struct m0_rm_credit *self,
				    const struct m0_rm_credit *c1);
static m0_bcount_t file_lock_cr_len(const struct m0_rm_credit *c0);

static int file_lock_cr_join(struct m0_rm_credit       *self,
			     const struct m0_rm_credit *c1);
static int file_lock_cr_disjoin(struct m0_rm_credit       *self,
				const struct m0_rm_credit *c1,
				struct m0_rm_credit       *intersection);
static int file_lock_cr_copy(struct m0_rm_credit       *dest,
			     const struct m0_rm_credit *self);
static int file_lock_cr_diff(struct m0_rm_credit       *self,
			     const struct m0_rm_credit *c1);
static bool file_lock_cr_conflicts(const struct m0_rm_credit *self,
				   const struct m0_rm_credit *c1);
static bool file_lock_cr_is_subset(const struct m0_rm_credit *self,
				   const struct m0_rm_credit *c1);
static int file_lock_cr_encode(struct m0_rm_credit     *self,
			       struct m0_bufvec_cursor *cur);
static int file_lock_cr_decode(struct m0_rm_credit     *self,
			       struct m0_bufvec_cursor *cur);
static void file_lock_cr_free(struct m0_rm_credit *self);
static void file_lock_cr_initial_capital(struct m0_rm_credit *self);

const struct m0_rm_resource_type_ops file_lock_type_ops = {
	.rto_eq     = file_lock_equal,
	.rto_len    = file_lock_len,
	.rto_decode = file_lock_decode,
	.rto_encode = file_lock_encode,
};

const struct m0_rm_resource_ops file_lock_ops = {
	.rop_credit_init   = file_lock_credit_init,
	.rop_resource_free = file_lock_resource_free,
};

const struct m0_rm_credit_ops file_lock_credit_ops = {
	.cro_intersects      = file_lock_cr_intersects,
	.cro_join            = file_lock_cr_join,
	.cro_copy            = file_lock_cr_copy,
	.cro_diff            = file_lock_cr_diff,
	.cro_free            = file_lock_cr_free,
	.cro_encode          = file_lock_cr_encode,
	.cro_decode          = file_lock_cr_decode,
	.cro_len             = file_lock_cr_len,
	.cro_is_subset       = file_lock_cr_is_subset,
	.cro_disjoin         = file_lock_cr_disjoin,
	.cro_conflicts       = file_lock_cr_conflicts,
	.cro_initial_capital = file_lock_cr_initial_capital,
};

const struct m0_rm_incoming_ops file_lock_incoming_ops = {
	.rio_complete = file_lock_incoming_complete,
	.rio_conflict = file_lock_incoming_conflict
};

#define R_F(resource) container_of(resource, struct m0_file, fi_res)

/** Compare Ids of two file locks */
static bool file_lock_equal(const struct m0_rm_resource *resource0,
			    const struct m0_rm_resource *resource1)
{
	struct m0_file *file0;
	struct m0_file *file1;

	M0_PRE(resource0 != NULL && resource1 != NULL);

	file0 = R_F(resource0);
	file1 = R_F(resource1);

	return m0_fid_eq(file0->fi_fid, file1->fi_fid);
}

static m0_bcount_t file_lock_len(const struct m0_rm_resource *resource)
{
	struct m0_file      *fl;
	struct m0_xcode_obj  fidobj;
	struct m0_xcode_ctx  ctx;
	static m0_bcount_t   flock_len;

	M0_ASSERT(resource != NULL);

	if (flock_len == 0) {
		fl = R_F(resource);
		fidobj.xo_type = m0_fid_xc;
		fidobj.xo_ptr  = (void *)fl->fi_fid;
		m0_xcode_ctx_init(&ctx, &fidobj);
		flock_len = m0_xcode_length(&ctx);
		M0_ASSERT(flock_len > 0);
	}

	return flock_len;
}

static int file_lock_encdec(struct m0_file          *file,
			    struct m0_bufvec_cursor *cur,
			    enum m0_xcode_what what)
{
	int		    rc;
	struct m0_xcode_obj xo = M0_XCODE_OBJ(m0_fid_xc, (void *)file->fi_fid);

	M0_ENTRY();
	M0_ASSERT(cur != NULL);

	rc = m0_xcode_encdec(&xo, cur, what);
	if (rc == 0 && file->fi_fid == NULL)
		file->fi_fid = xo.xo_ptr;
	/**
	 * * @todo ->rto_decode() decode for file resource should create an
	 * entire ambient object: an inode on client and a cob on server.
	 */

	return M0_RC(rc);
}

/** Encode file_lock - ready to send over the wire */
static int file_lock_encode(struct m0_bufvec_cursor     *cur,
			    const struct m0_rm_resource *resource)
{
	struct m0_file *fl;
	int             rc;

	M0_ENTRY();
	M0_PRE(resource != NULL);

	fl = R_F(resource);
	rc = file_lock_encdec(fl, cur, M0_XCODE_ENCODE);
	return M0_RC(rc);
}

/** Decode file_lock - from the wire */
static int file_lock_decode(struct m0_bufvec_cursor *cur,
			    struct m0_rm_resource  **resource)
{
	struct m0_file *fl;
	int             rc;

	M0_ENTRY();
	M0_PRE(resource != NULL);

	M0_ALLOC_PTR(fl);
	if (fl == NULL)
		return M0_ERR(-ENOMEM);
	rc = file_lock_encdec(fl, cur, M0_XCODE_DECODE);
	if (rc == 0) {
		fl->fi_res.r_ops = &file_lock_ops;
		/*
		 * Other resource parameters are initialised by
		 * m0_rm_resource_add()
		 */
		*resource = &fl->fi_res;
	} else
		m0_free(fl);
	return M0_RC(rc);
}

/** Initialises credit (lock state) and ops vector for the file_lock */
static void file_lock_credit_init(struct m0_rm_resource *resource,
				  struct m0_rm_credit   *credit)
{
	M0_ASSERT(credit != NULL);
	credit->cr_datum = 0;
	credit->cr_ops = &file_lock_credit_ops;
}

static void file_lock_resource_free(struct m0_rm_resource *resource)
{
	struct m0_file *fl;

	fl = R_F(resource);
	m0_xcode_free_obj(&M0_XCODE_OBJ(m0_fid_xc, (void *)fl->fi_fid));
	m0_free(fl);
}

/** Lock request completion callback */
static void file_lock_incoming_complete(struct m0_rm_incoming *in, int32_t rc)
{
	/* Do nothing */
	return;
}

/** Lock request conflict callback */
static void file_lock_incoming_conflict(struct m0_rm_incoming *in)
{
	/* Do nothing */
	return;
}

static bool file_lock_credit_invariant(const struct m0_rm_credit *file_cr)
{
	return M0_IN(file_cr->cr_datum, (0, RM_FILE_LOCK));
}

static bool file_lock_cr_intersects(const struct m0_rm_credit *self,
				    const struct m0_rm_credit *c1)
{
	M0_ASSERT(c1 != NULL);
	M0_PRE(file_lock_credit_invariant(self) &&
	       file_lock_credit_invariant(c1));
	return self->cr_datum == c1->cr_datum;
}

static m0_bcount_t file_lock_cr_len(const struct m0_rm_credit *c0)
{
	struct m0_xcode_obj datumobj;
	struct m0_xcode_ctx ctx;

	M0_ASSERT(c0 != NULL);
	M0_PRE(file_lock_credit_invariant(c0));

	datumobj.xo_type = &M0_XT_U64;
	datumobj.xo_ptr  = (void *)&c0->cr_datum;
	m0_xcode_ctx_init(&ctx, &datumobj);
	return m0_xcode_length(&ctx);
}

static int file_lock_cr_join(struct m0_rm_credit       *self,
			     const struct m0_rm_credit *c1)
{
	return 0;
}

static int file_lock_cr_disjoin(struct m0_rm_credit       *self,
				const struct m0_rm_credit *c1,
				struct m0_rm_credit       *intersection)
{
	return M0_ERR(-EPERM);
}

static int file_lock_cr_copy(struct m0_rm_credit       *dest,
			     const struct m0_rm_credit *self)
{
	M0_ASSERT(dest != NULL);
	M0_PRE(file_lock_credit_invariant(self));

	dest->cr_datum = self->cr_datum;
	dest->cr_owner = self->cr_owner;
	dest->cr_ops = self->cr_ops;
	M0_POST(file_lock_credit_invariant(dest));
	return 0;
}

static int file_lock_cr_diff(struct m0_rm_credit       *self,
			     const struct m0_rm_credit *c1)
{
	M0_ASSERT(c1 != NULL);
	M0_PRE(file_lock_credit_invariant(self) &&
	       file_lock_credit_invariant(c1));

	self->cr_datum = max64((int64_t)(self->cr_datum - c1->cr_datum), 0);
	M0_POST(file_lock_credit_invariant(self));
	return 0;
}

static bool file_lock_cr_conflicts(const struct m0_rm_credit *self,
				   const struct m0_rm_credit *c1)
{
	M0_ASSERT(c1 != NULL);
	M0_PRE(file_lock_credit_invariant(self) &&
	       file_lock_credit_invariant(c1));

	return self->cr_datum & c1->cr_datum;
}

static bool file_lock_cr_is_subset(const struct m0_rm_credit *self,
				   const struct m0_rm_credit *c1)
{
	M0_PRE(file_lock_credit_invariant(self) &&
	       file_lock_credit_invariant(c1));
	return self->cr_datum <= c1->cr_datum;
}

static int file_lock_cr_encdec(struct m0_rm_credit     *self,
			       struct m0_bufvec_cursor *cur,
			       enum m0_xcode_what what)
{
	M0_ENTRY();
	M0_ASSERT(cur != NULL);

	return M0_RC(m0_xcode_encdec(&M0_XCODE_OBJ(&M0_XT_U64,
						    &self->cr_datum),
				      cur, what));
}

static int file_lock_cr_encode(struct m0_rm_credit     *self,
			       struct m0_bufvec_cursor *cur)
{
	M0_PRE(file_lock_credit_invariant(self));
	return file_lock_cr_encdec(self, cur, M0_XCODE_ENCODE);
}

static int file_lock_cr_decode(struct m0_rm_credit     *self,
			       struct m0_bufvec_cursor *cur)
{
	return file_lock_cr_encdec(self, cur, M0_XCODE_DECODE);
}

static void file_lock_cr_free(struct m0_rm_credit *self)
{
	self->cr_datum = 0;
}

static void file_lock_cr_initial_capital(struct m0_rm_credit *self)
{
	self->cr_datum = RM_FILE_LOCK;
}

/** @} */ /* end internal FileLockInternal */

/**
 * @addtogroup FileLock
 * @{
 */
M0_INTERNAL void m0_file_init(struct m0_file      *file,
			      const struct m0_fid *fid,
			      struct m0_rm_domain *dom,
			      enum m0_di_types	   di_type)
{
	M0_PRE(file != NULL);
	M0_PRE(fid != NULL);

	M0_LOG(M0_DEBUG, FID_F, FID_P(fid));
	file->fi_res.r_ops = &file_lock_ops;
	file->fi_fid = fid;
	if (dom != NULL)
		m0_rm_resource_add(dom->rd_types[M0_RM_FLOCK_RT],
				   &file->fi_res);
	file->fi_di_ops = m0_di_ops_get(di_type);
}
M0_EXPORTED(m0_file_init);

M0_INTERNAL void m0_file_fini(struct m0_file *file)
{
	m0_rm_resource_del(&file->fi_res);
	file->fi_res.r_ops = NULL;
	file->fi_di_ops = NULL;
	file->fi_fid = NULL;
}
M0_EXPORTED(m0_file_fini);

M0_INTERNAL void m0_file_owner_init(struct m0_rm_owner      *owner,
				    const struct m0_uint128 *grp_id,
				    struct m0_file          *file,
				    struct m0_rm_remote     *creditor)
{
	m0_rm_owner_init_rfid(owner, grp_id, &file->fi_res, creditor);
}
M0_EXPORTED(m0_file_owner_init);

M0_INTERNAL void m0_file_owner_fini(struct m0_rm_owner *owner)
{
	m0_rm_owner_fini(owner);
}
M0_EXPORTED(m0_file_owner_fini);

M0_INTERNAL void m0_file_lock(struct m0_rm_owner    *owner,
			      struct m0_rm_incoming *req)
{
	M0_ENTRY();
/*
 * @todo This API should be called before m0_file_lock is invoked to ensure that
 * the rm incoming chan is initialised. The calling fom will listen on this chan
 * for the file lock events.
 */
	m0_rm_incoming_init(req, owner, M0_RIT_LOCAL, RIP_NONE,
		RIF_LOCAL_WAIT | RIF_MAY_BORROW | RIF_MAY_REVOKE);
	req->rin_want.cr_datum = RM_FILE_LOCK;
	req->rin_ops = &file_lock_incoming_ops;
	m0_rm_credit_get(req);
	M0_LEAVE();
}
M0_EXPORTED(m0_file_lock);

M0_INTERNAL void m0_file_unlock(struct m0_rm_incoming *req)
{
	M0_ENTRY();
	m0_rm_credit_put(req);
	m0_rm_incoming_fini(req);
	M0_LEAVE();
}
M0_EXPORTED(m0_file_unlock);

M0_INTERNAL int m0_file_lock_type_register(struct m0_rm_domain *dom,
					   struct m0_rm_resource_type *flock_rt)
{
	M0_ENTRY();

	flock_rt->rt_id = M0_RM_FLOCK_RT;
	flock_rt->rt_ops = &file_lock_type_ops;
	return M0_RC(m0_rm_type_register(dom, flock_rt));
}
M0_EXPORTED(m0_file_lock_type_register);

M0_INTERNAL
void m0_file_lock_type_deregister(struct m0_rm_resource_type *flock_rt)
{
	M0_ENTRY();
	m0_rm_type_deregister(flock_rt);
	M0_LEAVE();
}
M0_EXPORTED(m0_file_lock_type_deregister);

const struct m0_fid_type m0_file_fid_type = {
	.ft_id   = 'G',
	.ft_name = "file fid"
};

M0_INTERNAL int m0_file_mod_init(void)
{
	m0_fid_type_register(&m0_file_fid_type);
	return 0;
}

M0_INTERNAL void m0_file_mod_fini(void)
{
	m0_fid_type_unregister(&m0_file_fid_type);
}

/** @} end of FileLock */

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
