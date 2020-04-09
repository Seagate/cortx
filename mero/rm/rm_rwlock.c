/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Egor Nikulenkov <egor.nikulenkov@seagate.com>
 * Original creation date: 02/17/2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_RM
#include "lib/trace.h"

#include "lib/memory.h"    /* M0_ALLOC_PTR */
#include "fid/fid_xc.h"
#include "xcode/xcode.h"
#include "rm/rm_rwlock.h"
#include "addb2/addb2.h"

/**
   @page ReadWrite Distributed Lock DLD

   - @ref RWLockDLD
   - @ref RWLockDLD-req
   - @ref RWLockDLD-depends
   - @subpage RWLockDLD-fspec "Functional Specification" <!-- Note @subpage -->
   - @ref RWLockDLD-lspec
      - @ref RWLockDLD-lspec-comps
      - @ref RWLockDLD-lspec-numa
   - @ref RWLockDLD-conformance
   - @ref RWLockDLD-ut
   - @ref RWLockDLD-st
   - @ref RWLockDLD-ref
   - @ref RWLockDLD-impl-plan


   <hr>
   @section RWLockDLD Overview

   This DLD describes the implementation of a distributed RW lock. The
   distributed RW lock is implemented over Mero resource manager.
   Distributed RW Lock allows concurrent access for read-only operations,
   while write operations require exclusive access.

   <b>Purpose of a DLD</b> @n
   The purpose of the Detailed Level Design (DLD) specification of a
   distributed RW lock is to:
   - Describe RW lockable resource type of Mero resource manager
   - Define interface that allows to acquire RW locks for RW lockable resources

   <hr>
   @section RWLockDLD-req Requirements

   - @b R.rm-rw-lock.async The distributed RW lock should provide async
   interfaces.
   - @b R.rm-rw-lock.RM The distributed RW lock shall use RM
   (resource manager) to implement the interfaces.
   - @b R.rm-rw-lock.sharedread All readers can hold RW lock concurrently
   if no writer exists.
   - @b R.rm-rw-lock.exclusivewrite Only one writer can hold RW lock at any
   given time in the whole cluster. Also no readers can hold RW lock if writer
   holding the RW lock exists.
   - @b R.rm-rw-lock.livelock Livelock should be avoided when writer can't
   acquire the lock because of continuous lock requests from readers.


   @section RWLockDLD-lspec Logical Specification

   - @ref RWLockDLD-lspec-comps
   - @ref RWLockInternal  <!-- Note link -->
   - @ref RWLockDLD-lspec-numa


   @subsection RWLockDLD-lspec-comps Component Overview
   The distributed RW lock implements following resource manager ops:
   - resource type ops
   - resource ops
   - resource credit ops

   Special wrappers are provided for RW lock user.

   <hr>
   @section RWLockDLD-conformance Conformance

   - @b I.rm-rw-lock.async The interfaces are async.
   - @b I.rm-rw-lock.RM  Implements RM ops to implement RW lock resource.
   - @b I.rm-rw-lock.sharedread Usage credit requests from readers are always
   satisfied if no active writer exists.
   - @b I.rm-rw-lock.exclusivewrite Writer requests all existing usage credits
   for RW lock in the whole cluster. Therefore at any given time only one writer
   can hold the lock. Usage credit requests from readers can't be satisfied when
   there is writer holding the lock.
   - @b I.rm-rw-lock.livelock Livelock can be avoided by using M0_RPF_BARRIER
   pins, but not implemented yet.

   <hr>
   @section RWLockDLD-ut Unit Tests

   Following scenarios will be tested:
   @test
   1) Request read lock when RW lock is not held by anyone else
   - result: lock is granted immediately

   @test
   2) Request read lock when RW lock is held by another reader
   - result: lock is granted immediately

   @test
   3) Request read lock when RW lock is held by writer
   - result: lock is granted after writer releases it

   @test
   4) Request write lock when RW lock is not held by anyone else
   - result: lock is granted immediately

   @test
   5) Request write lock when RW lock is held by reader
   - result: lock is granted after reader releases it

   @test
   5) Request write lock when RW lock is held by writer
   - result: lock is granted after writer releases it

   <hr>
   @section RWLockDLD-st System Tests

   System tests will be performed by the subsystem that uses the distributed
   RW lock.

   <hr>
   @section RWLockDLD-ref References

   - <a href="https://docs.google.com/a/seagate.com/document/d/1WYw8MmItpp0KuBbY
fuQQxJaw9UN8OuHKnlICszB8-Zs/edit">HLD of resource manager Interfaces</a>,

   <hr>
   @section RWLockDLD-impl-plan Implementation Plan

   The plan is:
   - m0_rm_resource_type_ops
   - m0_rm_resource_ops
   - m0_rm_credit_ops
   - external interface for RW lock user

 */

/**
   @defgroup RWLockInternal Distributed RW Lock Internals
   @ingroup RWLock

   This section contains the functions that are internal to the distributed
   RW lock. They implement various resource manager ops.

   @b Credit structure

   Credit for RW lock is represented by number of read locks permitted. Reader
   requires 1 such read lock, writer (exclusive access) requires all such read
   locks existing in the cluster.

   Initial capacity of the credit owned by top-level creditor for RW lock
   resource type is ~0 read locks. It is assumed that number of read locks
   will be enough for any possible number of readers in the cluster.

   If reader acquires RW lock then it requests credit for 0x01 read lock. Since
   number of read locks in the system is virtually infinite, all readers can
   hold RW lock simultaneously.
   If writer acquires RW lock then it requests credit for all (~0) read locks.
   So writer will exclusively own all the credits for RW lock and requests for
   this lock from other readers/writers won't be satisfied.

   @see @ref RWLockDLD and @ref RWLockDLD-lspec

   @{
 */

/* Forward Declarations */
static bool rwlockable_equal(const struct m0_rm_resource *resource0,
			     const struct m0_rm_resource *resource1);
static m0_bcount_t rwlockable_len(const struct m0_rm_resource *resource);
static int rwlockable_encode(struct m0_bufvec_cursor     *cur,
			     const struct m0_rm_resource *resource);
static int rwlockable_decode(struct m0_bufvec_cursor *cur,
			     struct m0_rm_resource  **resource);
static void rwlockable_credit_init(struct m0_rm_resource *resource,
				   struct m0_rm_credit   *credit);
static void rwlockable_resource_free(struct m0_rm_resource *resource);

static bool rwlock_cr_intersects(const struct m0_rm_credit *self,
				 const struct m0_rm_credit *c1);
static m0_bcount_t rwlock_cr_len(const struct m0_rm_credit *c0);

static int rwlock_cr_join(struct m0_rm_credit       *self,
			  const struct m0_rm_credit *c1);
static int rwlock_cr_disjoin(struct m0_rm_credit       *self,
			     const struct m0_rm_credit *c1,
			     struct m0_rm_credit       *intersection);
static int rwlock_cr_copy(struct m0_rm_credit       *dest,
			  const struct m0_rm_credit *self);
static int rwlock_cr_diff(struct m0_rm_credit       *self,
			  const struct m0_rm_credit *c1);
static bool rwlock_cr_conflicts(const struct m0_rm_credit *self,
				const struct m0_rm_credit *c1);
static bool rwlock_cr_is_subset(const struct m0_rm_credit *self,
				const struct m0_rm_credit *c1);
static int rwlock_cr_encode(struct m0_rm_credit     *self,
			    struct m0_bufvec_cursor *cur);
static int rwlock_cr_decode(struct m0_rm_credit     *self,
			    struct m0_bufvec_cursor *cur);
static void rwlock_cr_free(struct m0_rm_credit *self);
static void rwlock_cr_initial_capital(struct m0_rm_credit *self);

const struct m0_rm_resource_type_ops rwlockable_type_ops = {
	.rto_eq     = rwlockable_equal,
	.rto_len    = rwlockable_len,
	.rto_decode = rwlockable_decode,
	.rto_encode = rwlockable_encode,
};

const struct m0_rm_resource_ops rwlockable_ops = {
	.rop_credit_init   = rwlockable_credit_init,
	.rop_resource_free = rwlockable_resource_free,
};

const struct m0_rm_credit_ops rwlock_credit_ops = {
	.cro_intersects      = rwlock_cr_intersects,
	.cro_join            = rwlock_cr_join,
	.cro_copy            = rwlock_cr_copy,
	.cro_diff            = rwlock_cr_diff,
	.cro_free            = rwlock_cr_free,
	.cro_encode          = rwlock_cr_encode,
	.cro_decode          = rwlock_cr_decode,
	.cro_len             = rwlock_cr_len,
	.cro_is_subset       = rwlock_cr_is_subset,
	.cro_disjoin         = rwlock_cr_disjoin,
	.cro_conflicts       = rwlock_cr_conflicts,
	.cro_initial_capital = rwlock_cr_initial_capital,
};

#define R_RW(res) container_of(res, struct m0_rw_lockable, rwl_resource)
#define RW_XO(rw) (&M0_XCODE_OBJ(m0_fid_xc, (void *)(rw)->rwl_fid))
#define CR_XO(cr) (&M0_XCODE_OBJ(&M0_XT_U64, (void *)&(cr)->cr_datum))

M0_INTERNAL int
m0_rwlockable_domain_type_init(struct m0_rm_domain        *rwl_dom,
			       struct m0_rm_resource_type *rwl_rt)
{
	M0_SET0(rwl_dom);
	M0_SET0(rwl_rt);
	m0_rm_domain_init(rwl_dom);
	m0_rw_lockable_type_register(rwl_dom, rwl_rt);
	return 0;
}

M0_INTERNAL void
m0_rwlockable_domain_type_fini(struct m0_rm_domain        *rwl_dom,
			       struct m0_rm_resource_type *rwl_rt)
{
	m0_rw_lockable_type_deregister(rwl_rt);
	m0_rm_domain_fini(rwl_dom);
}

/** Compare identifiers of two lockable resources */
static bool rwlockable_equal(const struct m0_rm_resource *resource0,
			     const struct m0_rm_resource *resource1)
{
	struct m0_rw_lockable *lockable0;
	struct m0_rw_lockable *lockable1;

	M0_PRE(resource0 != NULL && resource1 != NULL);

	lockable0 = R_RW(resource0);
	lockable1 = R_RW(resource1);

	return m0_fid_eq(lockable0->rwl_fid, lockable1->rwl_fid);
}

static m0_bcount_t rwlockable_len(const struct m0_rm_resource *resource)
{
	struct m0_rw_lockable *lockable;
	struct m0_xcode_obj    fidobj;
	struct m0_xcode_ctx    ctx;
	static m0_bcount_t     len;

	M0_PRE(resource != NULL);

	/*
	 * len depends only on size of fid, which is constant,
	 * therefore len can be calculated once and then reused.
	 */
	if (len == 0) {
		lockable = R_RW(resource);
		fidobj.xo_type = m0_fid_xc;
		fidobj.xo_ptr  = (void *)lockable->rwl_fid;
		m0_xcode_ctx_init(&ctx, &fidobj);
		len = m0_xcode_length(&ctx);
		M0_ASSERT(len > 0);
	}

	return len;
}

/** Encode lockable resource. Actually sending only RW lock
 *  FID is sufficient to reconstruct it on remote side */
static int rwlockable_encode(struct m0_bufvec_cursor     *cur,
			     const struct m0_rm_resource *resource)
{
	M0_ENTRY();
	M0_PRE(resource != NULL);

	return M0_RC(m0_xcode_encdec(RW_XO(R_RW(resource)),
				     cur, M0_XCODE_ENCODE));
}

static int rwlockable_decode(struct m0_bufvec_cursor *cur,
			     struct m0_rm_resource  **resource)
{
	struct m0_rw_lockable *lockable;
	struct m0_xcode_obj   *xo;
	int                    rc;

	M0_ENTRY();
	M0_PRE(resource != NULL);

	M0_ALLOC_PTR(lockable);
	if (lockable == NULL)
		return M0_ERR(-ENOMEM);

	xo = RW_XO(lockable);
	rc = m0_xcode_encdec(xo, cur, M0_XCODE_DECODE);
	if (rc == 0) {
		lockable->rwl_fid = xo->xo_ptr;
		lockable->rwl_resource.r_ops = &rwlockable_ops;
		*resource = &lockable->rwl_resource;
	} else {
		m0_free(lockable);
	}

	return M0_RC(rc);
}

static void rwlockable_credit_init(struct m0_rm_resource *resource,
				   struct m0_rm_credit   *credit)
{
	M0_PRE(credit != NULL);
	credit->cr_datum = 0;
	credit->cr_ops = &rwlock_credit_ops;
}

static void rwlockable_resource_free(struct m0_rm_resource *resource)
{
	struct m0_rw_lockable *lockable;

	lockable = R_RW(resource);
	m0_xcode_free_obj(&M0_XCODE_OBJ(m0_fid_xc, (void *)lockable->rwl_fid));
	m0_free(lockable);
}

static bool rwlock_credit_invariant(const struct m0_rm_credit *rwlock_cr)
{
	return rwlock_cr->cr_datum <= RM_RW_WRITE_LOCK;
}

static bool rwlock_cr_intersects(const struct m0_rm_credit *self,
				 const struct m0_rm_credit *c1)
{
	M0_PRE(c1 != NULL);
	M0_PRE(rwlock_credit_invariant(self) &&
	       rwlock_credit_invariant(c1));

	return  self->cr_datum != 0 && c1->cr_datum != 0;
}

static m0_bcount_t rwlock_cr_len(const struct m0_rm_credit *c0)
{
	struct m0_xcode_obj datumobj;
	struct m0_xcode_ctx ctx;

	M0_PRE(c0 != NULL);
	M0_PRE(rwlock_credit_invariant(c0));

	datumobj.xo_type = &M0_XT_U64;
	datumobj.xo_ptr  = (void *)&c0->cr_datum;
	m0_xcode_ctx_init(&ctx, &datumobj);
	return m0_xcode_length(&ctx);
}

static int rwlock_cr_join(struct m0_rm_credit       *self,
			  const struct m0_rm_credit *c1)
{
	M0_PRE(rwlock_credit_invariant(self) &&
	       rwlock_credit_invariant(c1));
	/* Max credit value is write lock, also check for overflow */
	M0_PRE(self->cr_datum + c1->cr_datum >= self->cr_datum &&
	       self->cr_datum + c1->cr_datum <= RM_RW_WRITE_LOCK);

	self->cr_datum += c1->cr_datum;

	M0_POST(rwlock_credit_invariant(self));
	return 0;
}

static int rwlock_cr_disjoin(struct m0_rm_credit       *self,
			     const struct m0_rm_credit *c1,
			     struct m0_rm_credit       *intersection)
{
	M0_PRE(rwlock_credit_invariant(self) &&
	       rwlock_credit_invariant(c1));

	if (self->cr_datum > c1->cr_datum) {
		self->cr_datum -= c1->cr_datum;
		intersection->cr_datum = c1->cr_datum;
	} else {
		self->cr_datum = 0;
		intersection->cr_datum = self->cr_datum;
	}

	M0_POST(rwlock_credit_invariant(self) &&
		rwlock_credit_invariant(intersection));
	return 0;
}

static int rwlock_cr_copy(struct m0_rm_credit       *dest,
			  const struct m0_rm_credit *self)
{
	M0_PRE(dest != NULL);
	M0_PRE(rwlock_credit_invariant(self));

	dest->cr_datum = self->cr_datum;
	dest->cr_owner = self->cr_owner;
	dest->cr_ops = self->cr_ops;
	M0_POST(rwlock_credit_invariant(dest));
	return 0;
}

static int rwlock_cr_diff(struct m0_rm_credit       *self,
			  const struct m0_rm_credit *c1)
{
	M0_PRE(c1 != NULL);
	M0_PRE(rwlock_credit_invariant(self) &&
	       rwlock_credit_invariant(c1));

	self->cr_datum = (self->cr_datum > c1->cr_datum) ?
				self->cr_datum-c1->cr_datum : 0;
	M0_POST(rwlock_credit_invariant(self));
	return 0;
}

static bool rwlock_cr_conflicts(const struct m0_rm_credit *self,
				const struct m0_rm_credit *c1)
{
	M0_PRE(c1 != NULL);
	M0_PRE(rwlock_credit_invariant(self) &&
	       rwlock_credit_invariant(c1));

	return (self->cr_datum != 0 && c1->cr_datum == RM_RW_WRITE_LOCK) ||
	       (self->cr_datum == RM_RW_WRITE_LOCK && c1->cr_datum != 0);
}

static bool rwlock_cr_is_subset(const struct m0_rm_credit *self,
				const struct m0_rm_credit *c1)
{
	M0_PRE(rwlock_credit_invariant(self) &&
	       rwlock_credit_invariant(c1));
	return self->cr_datum <= c1->cr_datum;
}

static int rwlock_cr_encode(struct m0_rm_credit     *self,
			    struct m0_bufvec_cursor *cur)
{
	M0_PRE(rwlock_credit_invariant(self));
	return M0_RC(m0_xcode_encdec(CR_XO(self), cur, M0_XCODE_ENCODE));
}

static int rwlock_cr_decode(struct m0_rm_credit     *self,
			    struct m0_bufvec_cursor *cur)
{
	return M0_RC(m0_xcode_encdec(CR_XO(self), cur, M0_XCODE_DECODE));
}

static void rwlock_cr_free(struct m0_rm_credit *self)
{
	self->cr_datum = 0;
}

static void rwlock_cr_initial_capital(struct m0_rm_credit *self)
{
	 self->cr_datum = RM_RW_WRITE_LOCK;
}

/** @} */ /* end internal RWLockInternal */

/**
 * @addtogroup RWLock
 * @{
 */
M0_INTERNAL void m0_rw_lockable_init(struct m0_rw_lockable *lockable,
				     const struct m0_fid   *fid,
				     struct m0_rm_domain   *dom)
{
	M0_PRE(lockable != NULL);
	M0_PRE(fid != NULL);

	lockable->rwl_resource.r_ops = &rwlockable_ops;
	lockable->rwl_fid = fid;
	if (dom != NULL)
		m0_rm_resource_add(dom->rd_types[M0_RM_RWLOCKABLE_RT],
				   &lockable->rwl_resource);
}
M0_EXPORTED(m0_rw_lockable_init);

M0_INTERNAL void m0_rw_lockable_fini(struct m0_rw_lockable *lockable)
{
	/* Delete embedded resource from domain */
	m0_rm_resource_del(&lockable->rwl_resource);
	lockable->rwl_resource.r_ops = NULL;
	lockable->rwl_fid = NULL;
}
M0_EXPORTED(m0_rw_lockable_fini);

M0_INTERNAL void m0_rm_rwlock_owner_init(struct m0_rm_owner    *owner,
				         struct m0_fid         *fid,
				         struct m0_rw_lockable *lockable,
				         struct m0_rm_remote   *creditor)
{
	m0_rm_owner_init(owner, fid, &m0_rm_no_group,
		&lockable->rwl_resource, creditor);
}
M0_EXPORTED(m0_rm_rwlock_owner_init);

M0_INTERNAL void m0_rm_rwlock_owner_fini(struct m0_rm_owner *owner)
{
	m0_rm_owner_fini(owner);
}
M0_EXPORTED(m0_rm_rwlock_owner_fini);

M0_INTERNAL void m0_rm_rwlock_req_init(struct m0_rm_incoming           *req,
				       struct m0_rm_owner              *owner,
				       const struct m0_rm_incoming_ops *ops,
				       enum m0_rm_incoming_flags        flags,
				       enum m0_rm_rwlock_req_type       type)
{
	M0_ENTRY();
	M0_PRE(M0_IN(type, (RM_RWLOCK_WRITE, RM_RWLOCK_READ)));

	m0_rm_incoming_init(req, owner, M0_RIT_LOCAL, RIP_NONE, flags);

	req->rin_want.cr_datum = (type == RM_RWLOCK_WRITE) ?
				  RM_RW_WRITE_LOCK : RM_RW_READ_LOCK;
	req->rin_ops = ops;

	M0_LEAVE();
}
M0_EXPORTED(m0_rm_rwlock_req_init);

M0_INTERNAL void m0_rm_rwlock_req_fini(struct m0_rm_incoming *req)
{
	M0_ENTRY();
	m0_rm_incoming_fini(req);
	M0_LEAVE();
}
M0_EXPORTED(m0_rm_rwlock_req_fini);

M0_INTERNAL int m0_rw_lockable_type_register(struct m0_rm_domain        *dom,
					     struct m0_rm_resource_type *rtype)
{
	M0_ENTRY();

	rtype->rt_id = M0_RM_RWLOCKABLE_RT;
	rtype->rt_name = "rw-lockable";
	rtype->rt_ops = &rwlockable_type_ops;
	return M0_RC(m0_rm_type_register(dom, rtype));
}
M0_EXPORTED(m0_rw_lockable_type_register);

M0_INTERNAL
void m0_rw_lockable_type_deregister(struct m0_rm_resource_type *rtype)
{
	M0_ENTRY();
	m0_rm_type_deregister(rtype);
	M0_LEAVE();
}
M0_EXPORTED(m0_rw_lockable_type_deregister);

const struct m0_fid M0_RWLOCK_FID = M0_FID_TINIT('R', 'W', 'L');

/** @} end of RWLock */
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
