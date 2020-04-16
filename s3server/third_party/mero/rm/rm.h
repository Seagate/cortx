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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 04/28/2011
 */

#pragma once

#ifndef __MERO_RM_RM_H__
#define __MERO_RM_RM_H__

#include "lib/tlist.h"
#include "lib/types.h"
#include "lib/cookie.h"
#include "lib/chan.h"   /* m0_clink */
#include "fid/fid.h"    /* m0_fid */
#include "net/net.h"
#include "sm/sm.h"
#include "rm/rm_ha.h"   /* m0_rm_ha_tracker */

/**
 * @defgroup rm Resource management
 *
 * A resource is an entity in Mero for which a notion of ownership can be
 * well-defined. See the HLD referenced below for more details.
 *
 * In Mero almost everything is a resource, except for the low-level
 * types that are used to implement the resource framework.
 *
 * Resource management is split into two parts:
 *
 *     - generic functionality, implemented by the code in rm/ directory and
 *
 *     - resource type specific functionality.
 *
 * These parts interact through the operation vectors (m0_rm_resource_ops,
 * m0_rm_resource_type_ops and m0_rm_credit_ops) provided by a resource type
 * and called by the generic code. Type specific code, in turn, calls
 * generic entry-points described in the @b Resource type interface
 * section.
 *
 * In the documentation below, responsibilities of generic and type specific
 * parts of the resource manager are delineated.
 *
 * @b Overview
 *
 * A resource (m0_rm_resource) is associated with various file system entities:
 *
 *     - file meta-data. Credits to use this resource can be thought of as locks
 *       on file attributes that allow them to be cached or modified locally;
 *
 *     - file data. Credits to use this resource are extents in the file plus
 *       access mode bits (read, write);
 *
 *     - free storage space on a server (a "grant" in Lustre
 *       terminology). Credit to use this resource is a reservation of a given
 *       number of bytes;
 *
 *     - quota;
 *
 *     - many more, see the HLD for examples.
 *
 * A resource owner (m0_rm_owner) represents a collection of credits to use a
 * particular resource.
 *
 * To use a resource, a user of the resource manager creates an incoming
 * resource request (m0_rm_incoming), that describes a wanted usage credit
 * (m0_rm_credit_get()). Sometimes the request can be fulfilled immediately,
 * sometimes it requires changes in the credit ownership. In the latter case
 * outgoing requests are directed to the remote resource owners (which typically
 * means a network communication) to collect the wanted usage credit at the
 * owner. When an outgoing request reaches its target remote domain, an incoming
 * request is created and processed (which in turn might result in sending
 * further outgoing requests). Eventually, a reply is received for the outgoing
 * request. When incoming request processing is complete, it "pins" the wanted
 * credit. This credit can be used until the incoming request structure is
 * destroyed (m0_rm_credit_put()) and the pin is released.
 *
 * See the documentation for individual resource management data-types and
 * interfaces for more detailed description of their behaviour.
 *
 * @b Terminology.
 *
 * Various terms are used to described credit flow of the resources in a
 * cluster.
 *
 * Owners of credits for a particular resource are arranged in a cluster-wide
 * hierarchy. This hierarchical arrangement depends on system structure (e.g.,
 * where devices are connected, how network topology looks like) and dynamic
 * system behaviour (how accesses to a resource are distributed).
 *
 * Originally, all credits on the resource belong to a single owner or a set of
 * owners, residing on some well-known servers. Proxy servers request and cache
 * credits from there. Lower level proxies and clients request credits in
 * turn. According to the order in this hierarchy, one distinguishes "upward"
 * and "downward" owners relative to a given one.
 *
 * In a given ownership transfer operation, a downward owner is "debtor" and
 * upward owner is "creditor". The credit being transferred is called a "loan"
 * (note that this word is used only as a noun). When a credit is transferred
 * from a creditor to a debtor, the latter "borrows" and the former "sub-lets"
 * the loan. When a credit is transferred in the other direction, the creditor
 * "revokes" and debtor "returns" the loan.
 *
 * A debtor can voluntary return a loan. This is called a "cancel" operation.
 *
 * <b> Concurrency control. </b>
 *
 * Generic resource manager makes no assumptions about threading model used by
 * its callers. Generic resource data-structures and code are thread safe.
 *
 * 3 types of locks protect all generic resource manager states:
 *
 *     - per domain m0_rm_domain::rd_lock. This lock serialises addition and
 *       removal of resource types. Typically, it won't be contended much after
 *       the system start-up;
 *
 *     - per resource type m0_rm_resource_type::rt_lock. This lock is taken
 *       whenever a resource or a resource owner is created or
 *       destroyed. Typically, that would be when a file system object is
 *       accessed which is not in the cache;
 *
 *     - per resource owner m0_rm_owner::ro_lock.
 *
 *       These locks protect a bulk of generic resource management state:
 *
 *           - lists of possessed, borrowed and sub-let usage credits;
 *
 *           - incoming requests and their state transitions;
 *
 *           - outgoing requests and their state transitions;
 *
 *           - pins (m0_rm_pin).
 *
 *       Owner lock is accessed (taken and released) at least once during
 *       processing of an incoming request. Main owner state machine logic
 *       (owner_balance()) is structured in a way that is easily adaptable to a
 *       finer grained logic.
 *
 * None of these locks are ever held while waiting for a network communication
 * to complete.
 *
 * Lock ordering: 1) m0_rm_owner::ro_lock
 *                2) m0_rm_resource_type::rt_lock
 *
 * <b>A group of cooperating owners.</b>
 *
 * The owners in a group coordinate their activities internally (by means
 * outside of resource manager control) as far as resource management is
 * concerned.
 *
 * Resource manager assumes that credits granted to the owners from the same
 * group never conflict.
 *
 * Typical usage is to assign all owners from the same distributed
 * transaction (or from the same network client) to a group. The decision
 * about a group scope has concurrency related implications, because the
 * owners within a group must coordinate access between themselves to
 * maintain whatever scheduling properties are desired, like serialisability.
 *
 * @b Liveness.
 *
 * None of the resource manager structures, except for m0_rm_resource, require
 * reference counting, because their liveness is strictly determined by the
 * liveness of an "owning" structure into which they are logically embedded.
 *
 * The resource structure (m0_rm_resource) can be shared between multiple
 * resource owners (m0_rm_owner) and its liveness is determined by the
 * reference counting (m0_rm_resource::r_ref).
 *
 * As in many other places in Mero, liveness of "global" long-living
 * structures (m0_rm_domain, m0_rm_resource_type) is managed by the upper
 * layers which are responsible for determining when it is safe to finalise
 * the structures. Typically, an upper layer would achieve this by first
 * stopping and finalising all possible resource manager users.
 *
 * Similarly, a resource owner (m0_rm_owner) liveness is not explicitly
 * determined by the resource manager. It is up to the user to determine when
 * an owner (which can be associated with a file, or a client, or a similar
 * entity) is safe to be finalised.
 *
 * When a resource owner is finalised (ROS_FINALISING) it tears down the credit
 * network by revoking the loans it sublet to and by retuning the loans it
 * borrowed from other owners.
 *
 * <b> Resource identification and location. </b>
 *
 * @see m0_rm_remote
 *
 * <b> Persistent state. </b>
 *
 * <b> Network protocol. </b>
 *
 * @see https://docs.google.com/document/d/1WYw8MmItpp0KuBbYfuQQxJaw9UN8OuHKnlICszB8-Zs
 *
 * @{
 */

/* import */
struct m0_bufvec_cursor;

/* export */
struct m0_rm_domain;
struct m0_rm_resource;
struct m0_rm_resource_ops;
struct m0_rm_resource_type;
struct m0_rm_resource_type_ops;
struct m0_rm_owner;
struct m0_rm_remote;
struct m0_rm_loan;
struct m0_rm_group;
struct m0_rm_credit;
struct m0_rm_credit_ops;
struct m0_rm_incoming;
struct m0_rm_incoming_ops;
struct m0_rm_outgoing;
struct m0_rm_lease;

enum {
	M0_RM_RESOURCE_TYPE_ID_MAX = 64,
};

/**
 * RM owner fid type id.
 */
enum {
	M0_RM_OWNER_FT = 'O',
};

/**
 * Types of an incoming usage credit request.
 */
enum m0_rm_incoming_type {
	/**
	 * A request for a usage credit from a local user. When the request
	 * succeeds, the credit is held by the owner.
	 */
	M0_RIT_LOCAL,
	/**
	 * A request to loan a usage (credit) to a remote owner. Fulfillment of
	 * this request might cause further outgoing requests to be sent, e.g.,
	 * to revoke credits sub-let to remote owner.
	 */
	M0_RIT_BORROW,
	/**
	 * A request to return a usage credit previously sub-let to this owner.
	 */
	M0_RIT_REVOKE,
	M0_RIT_NR
};

extern const struct m0_uint128 m0_rm_no_group;

/**
 * Domain of resource management.
 *
 * All other resource manager data-structures (resource types, resources,
 * owners, credits, &c.) belong to some domain, directly or indirectly.
 *
 * Domains support multiple independent resource management services in the same
 * address space (user process or kernel). Each request handler and each client
 * kernel instance run a resource management service, but multiple request
 * handlers can co-exist in the same address space.
 */
struct m0_rm_domain {
	/**
	 * An array where resource types are registered. Protected by
	 * m0_rm_domain::rd_lock.
	 *
	 * @see m0_rm_resource_type::rt_id
	 */
	struct m0_rm_resource_type *rd_types[M0_RM_RESOURCE_TYPE_ID_MAX];
	struct m0_mutex             rd_lock;
};

/**
 * Represents a resource identity (i.e., a name). Multiple copies of the
 * same name may exist in different resource management domains, but no more
 * than a single copy per domain.
 *
 * m0_rm_resource is allocated and destroyed by the appropriate resource
 * type. An instance of m0_rm_resource would be typically embedded into a
 * larger resource type specific structure containing details of resource
 * identification.
 *
 * Generic code uses m0_rm_resource to efficiently compare resource
 * identities.
 */
struct m0_rm_resource {
	struct m0_rm_resource_type      *r_type;
	const struct m0_rm_resource_ops *r_ops;
	/**
	 * Linkage to a list of all resources of this type, hanging off
	 * m0_rm_resource_type::rt_resources.
	 */
	struct m0_tlink                  r_linkage;
	/**
	 * List of remote owners (linked through m0_rm_remote::rem_res_linkage)
	 * with which local owners of credits to this resource communicates.
	 */
	struct m0_tl                     r_remote;
	/**
	 * List of local owners (linked through m0_rm_owner::ro_owner_linkage)
	 */
	struct m0_tl                     r_local;
	/**
	 * Active references to this resource from resource owners
	 * (m0_rm_resource::r_type) and from remote resource owners
	 * (m0_rm_remote::rem_resource) Protected by
	 * m0_rm_resource_type::rt_lock.
	 */
	uint32_t                         r_ref;
	uint64_t                         r_magix;
};

struct m0_rm_resource_ops {
	/**
	 * Called when a new credit is allocated for the resource. The resource
	 * specific code should parse the credit description stored in the
	 * buffer and fill m0_rm_credit::cr_datum appropriately.
	 */
	int (*rop_credit_decode)(struct m0_rm_resource *resource,
				 struct m0_rm_credit *credit,
				 struct m0_bufvec_cursor *cur);
	/**
	 * Decides which credit should be granted, sublet, or revoked.
	 *
	 * "Policy" defines which credit to actually grant. E.g., a client
	 * doing a write to the first 4KB page in a file asks for [0, 4KB)
	 * extent lock. If nobody else accesses the file, RM would grant
	 * [0, ~0ULL) lock instead to avoid repeated lock requests in case
	 * of sequential IO.  If there are other conflicting locks, already
	 * granted on the file, the policy might expand requested credit to
	 * the largest credit that doesn't overlap with conflicting
	 * credits. And so on, there are multiple options.  So this is
	 * literally a "credit policy" as used by banks. The name stems all
	 * the way back to VAX VMS lock manager.
	 *
	 * @see m0_rm_incoming_policy --- a list of a few predefined policies.
	 */
	void (*rop_policy)(struct m0_rm_resource *resource,
			   struct m0_rm_incoming *in);
	/**
	 * Called to initialise a usage credit for this resource.
	 * Sets up m0_rm_credit::cr_ops.
	 */
	void (*rop_credit_init)(struct m0_rm_resource *resource,
				struct m0_rm_credit *credit);

	void (*rop_resource_free)(struct m0_rm_resource *resource);
};

enum m0_res_type_id {
	M0_RM_FLOCK_RT      = 1,
	M0_RM_RWLOCKABLE_RT = 2
};

/**
 * Resources are classified into disjoint types.
 * Resource type determines how its instances interact with the resource
 * management generic core and defines:
 *
 * - how the resources of this type are named;
 * - where the resources of this type are located;
 * - what resource credits are defined on the resources of this type;
 * - how credits are ordered;
 * - how credit conflicts are resolved.
 */
struct m0_rm_resource_type {
	const struct m0_rm_resource_type_ops *rt_ops;
	const char                           *rt_name;
	/**
	 * A resource type identifier, globally unique within a cluster, used
	 * to identify resource types on wire and storage.
	 *
	 * This identifier is used as an index in m0_rm_domain::rd_types.
	 *
	 * @todo Currently this is assigned manually and centrally. In the
	 * future, resource types identifiers (as well as rpc item opcodes)
	 * will be assigned dynamically by a special service (and then
	 * announced to the clients). Such identifier name-spaces are
	 * resources themselves, so, welcome to a minefield of
	 * bootstrapping.
	 */
	uint64_t			      rt_id;
	struct m0_mutex			      rt_lock;
	/**
	 * List of all resources of this type. Protected by
	 * m0_rm_resource_type::rt_lock.
	 */
	struct m0_tl			      rt_resources;
	/**
	 * Active references to this resource type from resource instances
	 * (m0_rm_owner::ro_resource). Protected by
	 * m0_rm_resource_type::rt_lock.
	 */
	uint32_t			      rt_nr_resources;
	struct m0_sm_group                    rt_sm_grp;
	/**
	 * Executes ASTs for this owner.
	 */
	struct m0_thread                      rt_worker;
	/**
	 * Flag for ro_worker thread to stop.
	 */
	bool                                  rt_stop_worker;
	/**
	 * Domain this resource type is registered with.
	 */
	struct m0_rm_domain		     *rt_dom;

	struct m0_mutex                       rt_queue_guard;

	/**
	 * Incoming events associated with remote RM owner are enqueued
	 * in this event queue. The queue is protected by rt_queue_guard,
	 * and relevant handling of the events is done by the AST thread.
	 * @todo:
	 * 1. Ideally queue shall be operated in lockfree manner to avoid
	 * taking locks in HA callback and in AST thread. Currently since
	 * only one producer and one consumer contend for the queue lock
	 * and critical section is not large this has been differed.
	 * Lockfree queue would also require a bit of memory management.
	 * 2. HA should not send the same state twice, and ideally Mero
	 * should assert on such incidence. This implementation would
	 * circumvent such behaviour by HA. Once Halon is completely
	 * replaced by Hare RM can assert on such spurious notifications.
	 */
	struct m0_queue                       rt_ha_events;
};

struct m0_rm_resource_type_ops {
	/**
	 * Checks if the two resources are equal.
	 */
	bool (*rto_eq)(const struct m0_rm_resource *resource0,
		       const struct m0_rm_resource *resource1);
	/**
	 * Checks if the resource has "id".
	 */
	bool (*rto_is)(const struct m0_rm_resource *resource,
		       uint64_t id);
	/**
	 * Return the size of the resource data
	 */
	m0_bcount_t (*rto_len) (const struct m0_rm_resource *resource);
	/**
	 * De-serialises the resource from a buffer.
	 */
	int  (*rto_decode)(struct m0_bufvec_cursor *cur,
			   struct m0_rm_resource **resource);
	/**
	 * Serialise a resource into a buffer.
	 */
	int  (*rto_encode)(struct m0_bufvec_cursor *cur,
			   const struct m0_rm_resource *resource);
};

/**
 * A resource owner uses the resource via a usage credit (also called
 * resource credit or simply credit as context permits). E.g., a client might
 * have a credit of read-only, write-only, or read-write access to a
 * certain extent in a file. An owner is granted a credit to use a resource.
 *
 * The meaning of a resource credit is determined by the resource type.
 * m0_rm_credit is allocated and managed by the generic code, but it has a
 * scratchpad field (m0_rm_credit::cr_datum), where type specific code stores
 * some additional information.
 *
 * A credit can be something simple as a single bit (conveying, for example,
 * an exclusive ownership of some datum) or a collection of extents tagged
 * with access masks.
 *
 * A credit is said to be "pinned" or "held" when it is necessary for some
 * ongoing operation. A pinned credit has M0_RPF_PROTECT pins (m0_rm_pin) on its
 * m0_rm_credit::cr_pins list. Otherwise a credit is simply "cached".
 *
 * Credits are typically linked into one of m0_rm_owner lists. Pinned credits
 * can only happen on m0_rm_owner::ro_owned[OWOS_HELD] list. They cannot be
 * moved out of this list until unpinned.
 */
struct m0_rm_credit {
	struct m0_rm_owner            *cr_owner;
	const struct m0_rm_credit_ops *cr_ops;
	/**
	 * Group id of the credit.
	 */
	struct m0_uint128              cr_group_id;
	/**
	 * Time when request for this credit acquisition was raised.
	 */
	m0_time_t                      cr_get_time;
	/**
	 * Resource type private field. By convention, 0 means "empty" credit.
	 */
	uint64_t                       cr_datum;
	/**
	 * Linkage of a credit (and the corresponding loan, if applicable) to a
	 * list hanging off m0_rm_owner.
	 */
	struct m0_tlink                cr_linkage;
	/**
	 * A list of pins, linked through m0_rm_pins::rp_credit, stuck into this
	 * credit.
	 */
	struct m0_tl                   cr_pins;
	uint64_t                       cr_magix;
};

/**
 * @note "self credit" in the comments below refers to the credit on which
 * an operation is invoked.
 */
struct m0_rm_credit_ops {
	/**
	 * Called when the generic code is about to free a credit. Type specific
	 * code releases any resources associated with the credit.
	 */
	void (*cro_free)(struct m0_rm_credit *self);
	/**
	 * Serialises a credit of a resource into a buffer.
	 */
	int  (*cro_encode)(struct m0_rm_credit *self,
			   struct m0_bufvec_cursor *cur);
	/**
	 * De-serialises the credit of a resource from a buffer.
	 */
	int  (*cro_decode)(struct m0_rm_credit *self,
			   struct m0_bufvec_cursor *cur);
	/**
	 * Return the size of the credit's data.
	 */
	m0_bcount_t (*cro_len) (const struct m0_rm_credit *self);

	/** @name operations.
	 *
	 *  The following operations are implemented by resource type and used
	 *  by generic code to analyse credits relationships.
	 *
	 *  "0" means the empty credit in the following.
	 */
	/** @{ */
	/**
	 * @retval True, iff 'self credit' intersects with c1.
	 * Credits intersect when there is some usage authorised by credit self
	 * and by credit c1.
	 *
	 * For example, a credit to read an extent [0, 100] (denoted R:[0, 100])
	 * intersects with a credit to read or write an extent [50, 150],
	 * (denoted RW:[50, 150]) because they can be both used to read bytes
	 * in the extent [50, 100].
	 *
	 *  "Intersects" is assumed to satisfy the following conditions:
	 *
	 *      - intersects(A, B) iff intersects(B, A) (symmetrical),
	 *
	 *      - (A != 0) iff intersects(A, A) (almost reflexive),
	 *
	 *      - !intersects(A, 0)
	 */
	bool (*cro_intersects) (const struct m0_rm_credit *self,
				const struct m0_rm_credit *c1);
	/**
	 * @retval True if 'self credit' is subset (or proper subset) of c1.
	 */
	bool (*cro_is_subset) (const struct m0_rm_credit *self,
			       const struct m0_rm_credit *c1);
	/**
	 * Adjoins c1 to 'self credit', updating self in place to be the sum
	 * credit.
	 */
	int (*cro_join) (struct m0_rm_credit *self,
			 const struct m0_rm_credit *c1);
	/**
	 * Splits self into two parts - diff(self,c1) and intersection(self, c1)
	 * Destructively updates 'self credit' with diff(self, c1) and updates
	 * intersection with intersection of (self, c1)
	 */
	int (*cro_disjoin) (struct m0_rm_credit *self,
			    const struct m0_rm_credit *c1,
			    struct m0_rm_credit *intersection);
	/**
	 * @retval True, iff 'self credit' conflicts with c1.
	 * Credits conflict iff one of them authorises a usage incompatible with
	 * another.
	 *
	 * For example, R:[0, 100] conflicts with RW:[50, 150], because the
	 * latter authorises writes to bytes in the [50, 100] extent, which
	 * cannot be done while R:[0, 100] is held by some other owner.
	 *
	 * "Conflicts" is assumed to satisfy the same conditions as
	 * "intersects" and, in addition,
	 *
	 *     - conflicts(A, B) => intersects(A, B), because if credits share
	 *       nothing they cannot conflict. Note that this condition
	 *       restricts possible resource semantics. For example, to satisfy
	 *       it, a credit to write to a variable must always imply a credit
	 *       to read it.
	 */
	bool (*cro_conflicts) (const struct m0_rm_credit *self,
			       const struct m0_rm_credit *c1);
	/** Difference between credits.
	 *
	 *  The difference is a part of self that doesn't intersect with c1.
	 *
	 *  For example, diff(RW:[50, 150], R:[0, 100]) == RW:[101, 150].
	 *
	 *   X <= Y means that diff(X, Y) is 0. X >= Y means Y <= X.
	 *
	 *   Two credits are equal, X == Y, when X <= Y and Y <= X.
	 *
	 *   "Difference" must satisfy the following conditions:
	 *
	 *       - diff(A, A) == 0,
	 *
	 *       - diff(A, 0) == A,
	 *
	 *       - diff(0, A) == 0,
	 *
	 *       - !intersects(diff(A, B), B),
	 *
	 *       - diff(A, diff(A, B)) == diff(B, diff(B, A)).
	 *
	 *  diff(A, diff(A, B)) is called a "meet" of A and B, it's an
	 *  intersection of credits A and B. The condition above ensures
	 *  that meet(A, B) == meet(B, A),
	 *
	 *       - diff(A, B) == diff(A, meet(A, B)),
	 *
	 *       - meet(A, meet(B, C)) == meet(meet(A, B), C),
	 *
	 *       - meet(A, 0) == 0, meet(A, A) == A, &c.,
	 *
	 *       - meet(A, B) <= A,
	 *
	 *       - (X <= A and X <= B) iff X <= meet(A, B),
	 *
	 *       - intersects(A, B) iff meet(A, B) != 0.
	 *
	 *  This function destructively updates "self" in place.
	 */
	int  (*cro_diff)(struct m0_rm_credit *self,
			 const struct m0_rm_credit *c1);
	/** Creates a copy of "src" in "dst".
	 *
	 *  @pre dst is empty.
	 */
	int  (*cro_copy)(struct m0_rm_credit *dst,
			 const struct m0_rm_credit *self);
	/**
	 * Setup initial capital
	 */
	void (*cro_initial_capital)(struct m0_rm_credit *self);
	/** @} end of Credits operations. */
};

enum m0_rm_remote_state {
	REM_FREED = 0,
	REM_INITIALISED,
	REM_SERVICE_LOCATING,
	REM_SERVICE_LOCATED,
	REM_OWNER_LOCATING,
	REM_OWNER_LOCATED
};

/**
 * A representation of a resource owner from another domain.
 *
 * This is a generic structure.
 *
 * m0_rm_remote is a portal through which interaction with the remote resource
 * owners is transacted. m0_rm_remote state transitions happen under its
 * resource's lock.
 *
 * A remote owner is needed to borrow from or sub-let to an owner in a different
 * domain. To establish the communication between local and remote owner the
 * following stages are needed:
 *
 *     - a service managing the remote owner must be located in the
 *       cluster. The particular way to do this depends on a resource type. For
 *       some resource types, the service is immediately known. For example, a
 *       "grant" (i.e., a reservation of a free storage space on a data
 *       service) is provided by the data service, which is already known by
 *       the time the grant is needed. For such resource types,
 *       m0_rm_remote::rem_state is initialised to REM_SERVICE_LOCATED. For
 *       other resource types, a distributed resource location data-base is
 *       consulted to locate the service. While the data-base query is going
 *       on, the remote owner is in REM_SERVICE_LOCATING state;
 *
 *     - once the service is known, the owner within the service should be
 *       located. This is done generically, by sending a resource management fop
 *       to the service. The service responds with the remote owner identifier
 *       (m0_rm_remote::rem_cookie) used for further communications. The service
 *       might respond with an error, if the owner is no longer there. In this
 *       case, m0_rm_state::rem_state goes back to REM_SERVICE_LOCATING.
 *
 *       Owner identification is an optional step, intended to optimise remote
 *       service performance. The service should be able to deal with the
 *       requests without the owner identifier. Because of this, owner
 *       identification can be piggy-backed to the first operation on the
 *       remote owner.
 *
 * @verbatim
 *           fini
 *      +----------------INITIALISED
 *      |                     |
 *      |                     | query the resource data-base
 *      |                     |
 *      |    TIMEOUT          V
 *      +--------------SERVICE_LOCATING<----+
 *      |                     |             |
 *      |                     | reply: OK   |
 *      |                     |             |
 *      V    fini             V             |
 *    FREED<-----------SERVICE_LOCATED      | reply: moved
 *      ^                     |             |
 *      |                     | get id      |
 *      |                     |             |
 *      |    TIMEOUT          V             |
 *      +----------------OWNER_LOCATING-----+
 *      |                     |
 *      |                     | reply: id
 *      |                     |
 *      |    fini             V
 *      +----------------OWNER_LOCATED
 *
 * @endverbatim
 */
struct m0_rm_remote {
	enum m0_rm_remote_state rem_state;
	/**
	 * A resource for which the remote owner is represented.
	 */
	struct m0_rm_resource  *rem_resource;
	/** Clink to track reverse session establishing */
	struct m0_clink         rem_rev_sess_clink;
	struct m0_rpc_session  *rem_session;
	/** A channel to signal state changes. */
	struct m0_chan          rem_signal;
	/**
	 * A linkage into the list of remotes for a given resource hanging off
	 * m0_rm_resource::r_remote.
	 */
	struct m0_tlink         rem_res_linkage;
	/**
	 * An identifier of the remote owner within the service. Valid in
	 * REM_OWNER_LOCATED state. This identifier is generated by the
	 * resource manager service.
	 */
	struct m0_cookie        rem_cookie;
	uint64_t                rem_id;
	/** Used for subscriptions to HA notifications about remote failure. */
	struct m0_rm_ha_tracker rem_tracker;
	/**
	 * When HA has reported remote owner is "dead", this field gets true.
	 */
	bool                    rem_dead;
	uint64_t                rem_magix;
};

/**
   m0_rm_owner state machine states.

   @dot
   digraph rm_owner {
	ROS_INITIAL -> ROS_INITIALISING
	ROS_INITIALISING -> ROS_ACTIVE
	ROS_ACTIVE -> ROS_QUIESCE
	ROS_QUIESCE -> ROS_FINALISING
	ROS_QUIESCE -> ROS_FINAL
	ROS_FINALISING -> ROS_FINAL
	ROS_FINALISING -> ROS_INSOLVENT
	ROS_FINALISING -> ROS_DEAD_CREDITOR
	ROS_DEAD_CREDITOR -> ROS_ACTIVE
	ROS_DEAD_CREDITOR -> ROS_FINAL
   }
   @enddot
 */
enum m0_rm_owner_state {
	/**
	 *  Initial state.
	 *
	 *  In this state owner credits lists are empty (including incoming and
	 *  outgoing request lists).
	 */
	ROS_INITIAL = 1,
	/**
	 * Initial network setup state:
	 *
	 *     - registering with the resource data-base;
	 *
	 *     - &c.
         */
	ROS_INITIALISING,
	/**
	 * Active request processing state. Once an owner reached this state it
	 * must pass through the finalising state.
	 */
	ROS_ACTIVE,
	/**
	 * No new requests are allowed in this state.
	 * Existing incoming requests are drained in this state.
	 */
	ROS_QUIESCE,
	/**
	 * Flushes all the loans.
	 * The owner collects from debtors and repays creditors.
	 */
	ROS_FINALISING,

	/**
	 * Failure state.
	 *
	 * Creditor was considered dead by HA. Owner made credits cleanup and is
	 * not able to satisfy any new incoming requests. Owner can recover from
	 * this state back to ROS_ACTIVE after HA notification saying RM
	 * creditor is online again or if user provides another creditor via
	 * m0_rm_owner_creditor_reset().
	 */
	ROS_DEAD_CREDITOR,
	/**
	 * Final state.
	 *
	 * During finalisation, if owner fails to clear the loans, it enters
	 * INSOLVENT state.
	 */
	ROS_INSOLVENT,
	/**
	 * Final state.
	 *
	 * In this state owner credits lists are empty (including incoming and
	 * outgoing request lists).
	 */
	ROS_FINAL
};

enum {
	/**
	 * Incoming requests are assigned a priority (greater numerical value
	 * is higher). When multiple requests are ready to be fulfilled, higher
	 * priority ones have a preference.
	 */
	M0_RM_REQUEST_PRIORITY_MAX = 3,
	M0_RM_REQUEST_PRIORITY_NR
};

/**
 * m0_rm_owner::ro_owned[] list of usage credits possessed by the owner is split
 * into sub-lists enumerated by this enum.
 */
enum m0_rm_owner_owned_state {
	/**
	 * Sub-list of pinned credits.
	 *
	 * @see m0_rm_credit
	 */
	OWOS_HELD,
	/**
	 * Not-pinned credit is "cached". Such credit can be returned to an
	 * upward owner from which it was previously borrowed (i.e., credit can
	 * be "cancelled") or sub-let to downward owners.
	 */
	OWOS_CACHED,
	OWOS_NR
};

/**
 * Lists of incoming and outgoing requests are subdivided into sub-lists.
 */
enum m0_rm_owner_queue_state {
	/**
	 * "Ground" request is not excited.
	 */
	OQS_GROUND,
	/**
	 * Excited requests are those for which something has to be done. An
	 * outgoing request is excited when it completes (or times out). An
	 * incoming request is excited when it's ready to go from RI_WAIT to
	 * RI_CHECK state.
	 *
	 * Resource owner state machine goes through lists of excited requests
	 * processing them. This processing can result in more excitement
	 * somewhere, but eventually terminates.
	 *
	 * @see http://en.wikipedia.org/wiki/Excited_state
	 */
	OQS_EXCITED,
	OQS_NR
};

/**
 * Resource ownership is used for two purposes:
 *
 *  - concurrency control. Only resource owner can manipulate the resource
 *    and ownership transfer protocol assures that owners do not step on
 *    each other. That is, resources provide traditional distributed
 *    locking mechanism;
 *
 *  - replication control. Resource owner can create a (local) copy of a
 *    resource. The ownership transfer protocol with the help of version
 *    numbers guarantees that multiple replicas are re-integrated
 *    correctly. That is, resources provide a cache coherency
 *    mechanism. Global cluster-wide cache management policy can be
 *    implemented on top of resources.
 *
 * A resource owner possesses credits on a particular resource. Multiple
 * owners within the same domain can possess credits on the same resource,
 * but no two owners in the cluster can possess conflicting credits at the
 * same time. The last statement requires some qualification:
 *
 *  - "time" here means the logical time in an observable history of the
 *    file system. It might so happen, that at a certain moment in physical
 *    time, data-structures (on different nodes, typically) would look as
 *    if conflicting credits were granted, but this is only possible when
 *    such credits will never affect visible system behaviour (e.g., a
 *    consensual decision has been made by that time to evict one of the
 *    nodes);
 *
 *  - in a case of optimistic conflict resolution, "no conflicting credits"
 *    means "no credits on which conflicts cannot be resolved afterwards by
 *    the optimistic conflict resolution policy".
 *
 * m0_rm_owner is a generic structure, created and maintained by the
 * generic resource manager code.
 *
 * Off a m0_rm_owner, hang several lists and arrays of lists for credits
 * book-keeping: m0_rm_owner::ro_borrowed, m0_rm_owner::ro_sublet and
 * m0_rm_owner::ro_owned[], further subdivided by states.
 *
 * As credits form a lattice (see m0_rm_credit_ops), it is always possible to
 * represent the cumulative sum of all credits on a list as a single
 * m0_rm_credit. The reason the lists are needed is that credits in the lists
 * have some additional state associated with them (e.g., loans for
 * m0_rm_owner::ro_borrowed, m0_rm_owner::ro_sublet or pins
 * (m0_rm_credit::cr_pins) for m0_rm_owner::ro_owned[]) that can be manipulated
 * independently.
 *
 * Owner state diagram:
 *
 * @verbatim
 *
 *                                  INITIAL
 *                                     |
 *                                     V
 *                               INITIALISING-------+
 *                                     |            |
 *                                     V            |
 *                                   ACTIVE         |
 *                                     |            |
 *                                     V            |
 *                                  QUIESCE         |
 *                                     |            |
 *                                     V            V
 *                   INSOLVENT<----FINALISING---->FINAL
 *
 * @endverbatim
 *
 * @invariant under ->ro_lock { // keep books balanced at all times
 *         join of credits on ->ro_owned[] and
 *                 credits on ->ro_sublet equals to
 *         join of credits on ->ro_borrowed           &&
 *
 *         meet of (join of credits on ->ro_owned[]) and
 *                 (join of credits on ->ro_sublet) is empty.
 * }
 *
 * @invariant under ->ro_lock {
 *         ->ro_owned[OWOS_HELD] is exactly the list of all held credits (ones
 *         with elevated user count)
 *
 * invariant is checked by rm/rm.c:owner_invariant().
 *
 * }
 */
struct m0_rm_owner {
	struct m0_sm           ro_sm;
	/**
	 * Owner non-zero FID unique through the cluster.
	 * Provided by user or generated randomly.
	 * Used to avoid dead-locks for requests with RIF_RESERVE flag.
	 */
	struct m0_fid          ro_fid;
	/**
	 * Resource this owner possesses the credits on.
	 */
	struct m0_rm_resource *ro_resource;
	/**
	 * A group this owner is part of.
	 *
	 * If this is m0_rm_no_group (0), the owner is not a member of any
	 * group (a "standalone" owner).
	 */
	struct m0_uint128      ro_group_id;
	/**
	 * An upward creditor, from where this owner borrows credits.
	 */
	struct m0_rm_remote   *ro_creditor;
	/**
	 * A list of loans, linked through m0_rm_loan::rl_credit:cr_linkage that
	 * this owner borrowed from other owners.
	 *
	 * @see m0_rm_loan
	 */
	struct m0_tl           ro_borrowed;
	/**
	 * A list of loans, linked through m0_rm_loan::rl_credit:cr_linkage that
	 * this owner extended to other owners. Credits on this list are not
	 * longer possessed by this owner: they are counted in
	 * m0_rm_owner::ro_borrowed, but not in m0_rm_owner::ro_owned.
	 *
	 * @see m0_rm_loan
	 */
	struct m0_tl           ro_sublet;
	/**
	 * A list of credits, linked through m0_rm_credit::cr_linkage possessed
	 * by the owner.
	 */
	struct m0_tl           ro_owned[OWOS_NR];
	/**
	 * An array of lists, sorted by priority, of incoming requests. Requests
	 * are linked through m0_rm_incoming::rin_want::cr_linkage.
	 *
	 * @see m0_rm_incoming
	 */
	struct m0_tl           ro_incoming[M0_RM_REQUEST_PRIORITY_NR][OQS_NR];
	/**
	 * An array of lists, of outgoing, not yet completed, requests.
	 */
	struct m0_tl           ro_outgoing[OQS_NR];
	/**
	 * Linkage of an owner to a list m0_rm_resource::r_local.
	 */
	struct m0_tlink        ro_owner_linkage;
	/**
	 * Generation count associated with an owner cookie.
	 */
	uint64_t               ro_id;
	/**
	 * True if user requested owner to windup. It is used to distinguish the
	 * reason of windup: self windup on creditor death or by request.
	 */
	bool                   ro_user_windup;
	/**
	 * Sequence number of the next local incoming request.
	 * Automatically increments inside incoming_queue() for local
	 * requests.
	 */
	uint64_t               ro_seq;
	uint64_t               ro_magix;
};

enum {
	/**
	 * Value of m0_rm_loan::rl_id for a self-loan.
	 * This value is invalid for any other type of loan.
	 */
	M0_RM_LOAN_SELF_ID = 1
};

/**
 * A loan (a credit) from one owner to another. In finance world, a loan
 * is closed-end credit.
 *
 * m0_rm_loan is always on some list (to which it is linked through
 * m0_rm_loan::rl_credit:cr_linkage field) in an owner structure. This owner is
 * one party of the loan. Another party is m0_rm_loan::rl_other. Which party is
 * creditor and which is debtor is determined by the list the loan is on.
 */
struct m0_rm_loan {
	struct m0_rm_credit  rl_credit;
	/**
	 * Other party (or parties) in the loan. Either an "upward" creditor
	 * or "downward" debtor, or "self" in case of a fake loan issued by
	 * the top-level creditor to maintain its invariants.
	 */
	struct m0_rm_remote *rl_other;
	/**
	 * An identifier generated by the remote end that should be passed back
	 * whenever operating on a loan (think loan agreement number).
	 */
	struct m0_cookie     rl_cookie;
	uint64_t             rl_id;
	uint64_t             rl_magix;
};

/**
   States of incoming request. See m0_rm_incoming for description.

   @dot
   digraph rm_incoming_state {
	RI_INITIALISED -> RI_CHECK
	RI_INITIALISED -> RI_FINAL
	RI_CHECK -> RI_SUCCESS
	RI_CHECK -> RI_FAILURE [label="Live lock"]
	RI_CHECK -> RI_WAIT [label="Pins placed"]
	RI_WAIT -> RI_FAILURE [label="Timeout"]
	RI_WAIT -> RI_CHECK [label="Last completion"]
	RI_WAIT -> RI_WAIT [label="Completion"]
	RI_SUCCESS -> RI_RELEASED [label="Credit released"]
	RI_FAILURE -> RI_FINAL [label="Finalised"]
	RI_RELEASED -> RI_FINAL
   }
   @enddot
 */
enum m0_rm_incoming_state {
	RI_INITIALISED = 1,
	/** Ready to check whether the request can be fulfilled. */
	RI_CHECK,
	/** Request has been fulfilled. */
	RI_SUCCESS,
	/** Request cannot be fulfilled. */
	RI_FAILURE,
	/** Has to wait for some future event, like outgoing request completion
	 *  or release of a locally held or reserved usage credit.
	 */
	RI_WAIT,
	/** Credit has been released (possibly by m0_rm_credit_put()). */
	RI_RELEASED,
	/** Request finalised. */
	RI_FINAL
};

/**
 * Some universal (i.e., not depending on a resource type) granting policies.
 */
enum m0_rm_incoming_policy {
	RIP_NONE = 1,
	/**
	 * If possible, don't insert a new credit into the list of possessed
	 * credits. Instead, pin possessed credits overlapping with the
	 * requested credit.
	 */
	RIP_INPLACE,
	/**
	 * Insert a new credit into the list of possessed credits, equal to the
	 * requested credit.
	 */
	RIP_STRICT,
	/**
	 * ...
	 */
	RIP_JOIN,
	/**
	 * Grant maximal possible credit, not conflicting with others.
	 */
	RIP_MAX,
	RIP_NR
};

/**
 * Flags controlling incoming usage credit request processing. These flags are
 * stored in m0_rm_incoming::rin_flags and analysed in m0_rm_credit_get().
 */
enum m0_rm_incoming_flags {
	/**
	 * Previously sub-let credits may be revoked, if necessary, to fulfill
	 * this request.
	 */
	RIF_MAY_REVOKE = (1 << 0),
	/**
	 * More credits may be borrowed, if necessary, to fulfill this request.
	 */
	RIF_MAY_BORROW = (1 << 1),
	/**
	 * The interaction between the request and locally possessed credits is
	 * the following:
	 *
	 *     - by default, locally possessed credits are ignored. This
	 *       scenario is typical for a local request (M0_RIT_LOCAL),
	 *       because local users resolve conflicts by some other means
	 *       (usually some form of concurrency control, like locking);
	 *
	 *     - if RIF_LOCAL_WAIT is set, the request will wait until
	 *       there is no locally possessed credits conflicting with the
	 *       wanted credit. This is typical for a remote request
	 *       (M0_RIT_BORROW or M0_RIT_REVOKE);
	 *
	 *     - if RIF_LOCAL_TRY is set, the request will be immediately
	 *       denied, if there are conflicting local credits. This allows to
	 *       implement a "try-lock" like functionality.
	 *
	 * RIF_LOCAL_WAIT and RIF_LOCAL_TRY flags are mutually exclusive.
	 */
	RIF_LOCAL_WAIT = (1 << 2),
	/**
	 * Fail the request if it cannot be fulfilled because of the local
	 * conflicts.
	 *
	 * @see RIF_LOCAL_WAIT
	 */
	RIF_LOCAL_TRY  = (1 << 3),
	/**
	 * Reserve credits that fulfill incoming request by putting
	 * M0_RPF_BARRIER pins. Reserved credit can't be granted to other
	 * incoming request until request which made reservation is granted.
	 * The only exception is when incoming request also has RIF_RESERVE flag
	 * and has bigger reserve priority (see m0_rm_incoming documentation).
	 */
	RIF_RESERVE    = (1 << 4),
};

/**
 * Structure that determines reserve priority of the request.
 *
 * If there are several requests willing to reserve the same credit,
 * then following rules apply:
 *
 *     - request with smallest timestamp has highest priority;
 *
 *     - if timestamps are equal, then request with smaller owner FID
 *       has higher priority.
 *
 *     - if timestamps and owner FIDs are equal, then request with smaller
 *       sequence number has higher priority.
 *
 * @see m0_rm_incoming
 */
struct m0_rm_reserve_prio {
	/**
	 * Timestamp of the original request.
	 */
	m0_time_t     rrp_time;
	/**
	 * Owner of the original request.
	 */
	struct m0_fid rrp_owner;
	/**
	 * Sequence number of the original request.
	 * It is a counter maintained by an owner and incremented
	 * for every incoming local request.
	 *
	 * Normally all local request timestamps for one owner are different,
	 * because requests are added under lock. They can be equal because of
	 * HW clock inaccuracy and sequence number is used in this case.
	 */
	uint64_t      rrp_seq;
};


/**
 * Resource usage credit request.
 *
 * The same m0_rm_incoming structure is used to track state of the incoming
 * requests both "local", i.e., from the same domain where the owner resides
 * and "remote".
 *
 * An incoming request is created for
 *
 *     - local credit request, when some user wants to use the resource;
 *
 *     - remote credit request from a "downward" owner which asks to sub-let
 *       some credits;
 *
 *     - remote credit request from an "upward" owner which wants to revoke some
 *       credits.
 *
 * These usages are differentiated by m0_rm_incoming::rin_type.
 *
 * An incoming request is a state machine, going through the following stages:
 *
 *     - [CHECK]   This stage determines whether the request can be fulfilled
 *                 immediately. Local request can be fulfilled immediately if
 *                 the wanted credit is possessed by the owner, that is, if
 *                 in->rin_want is implied by a join of owner->ro_owned[].
 *
 *                 A non-local (loan or revoke) request can be fulfilled
 *                 immediately if the wanted credit is implied by a join of
 *                 owner->ro_owned[OWOS_CACHED], that is, if the owner has
 *                 enough credits to grant the loan and the wanted credit does
 *                 not conflict with locally held credits.
 *
 *     - [POLICY]  If the request can be fulfilled immediately, the "policy" is
 *                 invoked which decides which credit should be actually
 *                 granted, sublet or revoked. That credit can be larger than
 *                 requested. A policy is, generally, resource type dependent,
 *                 with a few universal policies defined by enum
 *                 m0_rm_incoming_policy.
 *
 *     - [SUCCESS] Finally, fulfilled request succeeds.
 *
 *     - [ISSUE]   Otherwise, if the request can not be fulfilled immediately,
 *                 "pins" (m0_rm_pin) are added which will notify the request
 *                 when the fulfillment check might succeed.
 *
 *                 Pins are added to:
 *
 *                     - every conflicting credit held by this owner (when
 *                       RIF_LOCAL_WAIT flag is set on the request and always
 *                       for a remote request);
 *
 *                     - outgoing requests to revoke conflicting credits sub-let
 *                       to remote owners (when RIF_MAY_REVOKE flag is set);
 *
 *                     - outgoing requests to borrow missing credits from remote
 *                       owners (when RIF_MAY_BORROW flag is set);
 *
 *                     - reserved credits if current request has smaller reserve
 *                       priority;
 *
 *                 Outgoing requests mentioned above are created as necessary
 *                 in the ISSUE stage.
 *
 *     - [CYCLE]   When all the pins stuck in the ISSUE state are released
 *                 (either when a local credit is released or when an outgoing
 *                 request completes or when reserved credits are granted),
 *                 go back to the CHECK state.
 *
 * Looping back to the CHECK state is necessary, because possessed non-reserved
 * credits are not "pinned" during wait and can go away (be revoked or sub-let).
 * The credits are not pinned to avoid dependencies between credits that can
 * lead to dead-locks and "cascading evictions". But in this case there is
 * possibility of live-lock.
 *
 * The alternative is to use RIF_RESERVE flag that leads to pinning credits with
 * M0_RPF_BARRIER. Dead-locks are avoided by determining global strict ordering
 * between such requests using "reserve priority" (@ref m0_rm_reserve_prio).
 * Reserve priority is assigned once to the local incoming request and then
 * inherited by all remote requests created in sake of that local request
 * fulfillment. Therefore reserve priorities are handled consistently through
 * the whole cluster.
 *
 * Reserve priorities are used only to avoid possible dead-locks and live-locks.
 * There is no guarantee that request with higher reserve priority will be fully
 * fulfilled before request with lower reserve priority.
 *
 * If probability of a live-lock is low enough then using incoming requests
 * without RIF_RESERVE flag is preferable.
 *
 * How many outgoing requests are sent out in ISSUE state is a matter of
 * policy. The fewer requests are sent, the more CHECK-ISSUE-WAIT loop
 * iterations would typically happen. An extreme case of sending no more than a
 * single request is also possible and has some advantages: outgoing request can
 * be allocated as part of incoming request, simplifying memory management.
 *
 * It is also a matter of policy, how exactly the request is satisfied after a
 * successful CHECK state. Suppose, for example, that the owner possesses
 * credits C0 and C1 such that wanted credit W is implied by join(C0, C1), but
 * neither C0 nor C1 alone imply W. Some possible CHECK outcomes are:
 *
 *     - increase user counts in both C0 and C1;
 *
 *     - insert a new credit equal to W into owner->ro_owned[];
 *
 *     - insert a new credit equal to join(C0, C1) into owner->ro_owned[].
 *
 * All have their advantages and drawbacks:
 *
 *     - elevating C0 and C1 user counts keeps owner->ro_owned[] smaller, but
 *       pins more credits than strictly necessary;
 *
 *     - inserting W behaves badly in a standard use case where a thread doing
 *       sequential IO requests a credit on each iteration;
 *
 *     - inserting the join pins more credits than strictly necessary.
 *
 * All policy questions are settled by per-request flags and owner settings,
 * based on access pattern analysis.
 *
 * Following is a state diagram, where stages that are performed without
 * blocking (for network communication) are lumped into a single state:
 *
 * @verbatim
 *                                 SUCCESS-----------------------+
 *                                    ^                          |
 *             too many iterations    |                          |
 *                  live-lock         |    last completion       |
 *                +-----------------CHECK<-----------------+     |
 *                |                   |                    |     |
 *                |                   |                    |     |
 *                V                   |                    |     |
 *        +----FAILURE                | pins placed        |     |
 *        |       ^                   |                    |     |
 *        |       |                   |                    |     |
 *        |       |                   V                    |     |
 *        |       +----------------WAITING-----------------+     |
 *        |            timeout      ^   |                        |
 *        |                         |   | completion             |
 *        |                         |   |                        |
 *        |                         +---+                        |
 *        |                                                      |
 *        |                         RELEASED<--------------------+
 *        |                            |
 *        |                            |
 *        |                            V
 *        +------------------------->FINAL
 *
 * @endverbatim
 *
 * m0_rm_incoming fields and state transitions are protected by the owner's
 * mutex.
 *
 * @note a cedent can grant a usage credit larger than requested.
 *
 * An incoming request is placed by m0_rm_credit_get() on one of owner's
 * m0_rm_owner::ro_incoming[] lists depending on its priority. It remains on
 * this list until request processing failure or m0_rm_credit_put() call.
 *
 * @todo a new type of incoming request M0_RIT_GRANT (M0_RIT_FOIEGRAS?) can be
 * added to forcibly grant new credits to the owner, for example, as part of a
 * coordinated global distributed resource usage balancing between
 * owners. Processing of requests of this type would be very simple, because
 * adding new credits never blocks. Similarly, a new outgoing request type
 * M0_ROT_TAKE could be added.
 */
struct m0_rm_incoming {
	enum m0_rm_incoming_type         rin_type;
	struct m0_sm                     rin_sm;
	/**
	 * Stores the error code for incoming request. A separate field is
	 * needed because rin_sm.sm_rc is associated with an error of a state.
	 *
	 * For incoming it's possible that an error is set in RI_WAIT and
	 * then incoming has to be put back in RI_CHECK state before it can
	 * be put into RI_FAILURE. The state-machine model does not handle
	 * this well.
	 */
	int32_t                          rin_rc;
	enum m0_rm_incoming_policy       rin_policy;
	uint64_t                         rin_flags;
	/** The credit requested. */
	struct m0_rm_credit              rin_want;
	/**
	 * List of pins, linked through m0_rm_pin::rp_incoming_linkage, for all
	 * credits held to satisfy this request.
	 *
	 * @invariant meaning of this list depends on the request state:
	 *
	 *     - RI_CHECK, RI_SUCCESS: a list of M0_RPF_PROTECT pins on credits
	 *       in ->rin_want.cr_owner->ro_owned[];
	 *
	 *     - RI_WAIT: a list of M0_RPF_TRACK pins on outgoing requests
	 *       (through m0_rm_outgoing::rog_want::rl_credit::cr_pins) and held
	 *       credits in ->rin_want.cr_owner->ro_owned[OWOS_HELD];
	 *
	 *     - other states: empty.
	 */
	struct m0_tl                     rin_pins;
	/**
	 * Request priority from 0 to M0_RM_REQUEST_PRIORITY_MAX.
	 */
	int                              rin_priority;
	const struct m0_rm_incoming_ops *rin_ops;
	/** Start time for this request */
	m0_time_t                        rin_req_time;
	/** Determines reserve priority of the request. */
	struct m0_rm_reserve_prio        rin_reserve;
	uint64_t                         rin_magix;
};

/**
 * Operation assigned by a resource manager user to an incoming
 * request. Resource manager calls methods in this operation vector when events
 * related to the request happen. Therefore no RM functions should be called
 * in methods of this operation vector since resource manager state is
 * undefined.
 */
struct m0_rm_incoming_ops {
	/**
	 * This is called when incoming request processing completes either
	 * successfully (rc == 0) or with an error (-ve rc).
	 */
	void (*rio_complete)(struct m0_rm_incoming *in, int32_t rc);
	/**
	 * This is called when a request arrives that conflicts with the credit
	 * held by this incoming request.
	 */
	void (*rio_conflict)(struct m0_rm_incoming *in);
};

/**
 * Types of outgoing requests sent by the request manager.
 */
enum m0_rm_outgoing_type {
	/**
	 * A request to borrow a credit from an upward resource owner.
	 * This translates into a M0_RIT_BORROW incoming request.
	 */
	M0_ROT_BORROW = 1,
	/**
	 * A request returning a previously borrowed credit.
	 * Cancel is voluntary.
	 */
	M0_ROT_CANCEL,
	/**
	 * A request to return previously granted credit. This translates
	 * into a M0_RIT_REVOKE incoming request on the remote owner.
	 */
	M0_ROT_REVOKE
};

/**
 * An outgoing request is created on behalf of some incoming request to track
 * the state of credit transfer with some remote domain.
 *
 * An outgoing request is created to:
 *
 *     - borrow a new credit from some remote owner (an "upward" request) or
 *
 *     - revoke a credit sublet to some remote owner (a "downward" request) or
 *
 *     - cancel this owner's credit and return it to an upward owner.
 *
 * Before a new outgoing request is created, a list of already existing
 * outgoing requests (m0_rm_owner::ro_outgoing) is scanned. If an outgoing
 * request of a matching type for a greater or equal credit exists, new request
 * is not created. Instead, the incoming request pins existing outgoing
 * request.
 *
 * m0_rm_outgoing fields and state transitions are protected by the owner's
 * mutex.
 */
struct m0_rm_outgoing {
	enum m0_rm_outgoing_type rog_type;
	/*
	 * The error code (from reply or timeout) for this outgoing request.
	 */
	int32_t                  rog_rc;
	/** A credit that is to be transferred. */
	struct m0_rm_loan        rog_want;
	/** Flag indicating whether outgoing request is posted to RPC layer */
	bool                     rog_sent;
	uint64_t                 rog_magix;
};

enum m0_rm_pin_flags {
	M0_RPF_TRACK   = (1 << 0),
	M0_RPF_PROTECT = (1 << 1),
	M0_RPF_BARRIER = (1 << 2)
};

/**
 * A pin is used to
 *
 *     - M0_RPF_TRACK: track when a credit changes its state;
 *
 *     - M0_RPF_PROTECT: to protect a credit from revocation;
 *
 *     - M0_RPF_BARRIER: to prohibit granting credit to another request.
 *
 * Fields of this struct are protected by the owner's lock.
 *
 * Abstractly speaking, pins allow N:M (many to many) relationships between
 * incoming requests and credits: an incoming request has a list of pins "from"
 * it and a credit has a list of pins "to" it. A typical use case is as follows:
 *
 * @b Protection.
 *
 * While a credit is actively used, it cannot be revoked. For example, while
 * file write is going on, the credit to write in the target file extent must be
 * held. A credit is held (or pinned) from the return from m0_rm_credit_get()
 * until the matching call to m0_rm_credit_put(). To mark the credit as pinned,
 * m0_rm_credit_get() adds a M0_RPF_PROTECT pin from the incoming request to the
 * returned credit (generally, more than one credit can be pinned as result on
 * m0_rm_credit_get()). This pin is removed by the call to
 * m0_rm_credit_put(). Multiple incoming requests can pin the same credit.
 *
 * @b Tracking.
 *
 * M0_RPF_TRACK pin is added from the incoming request to the credit when
 *
 *     - An incoming request with a RIF_LOCAL_WAIT flag need to wait until a
 *       conflicting pinned credit becomes unpinned;
 *
 *     - An incoming request need to wait until reserved credit pinned
 *       with M0_RPF_BARRIER is unpinned;
 *
 *     - An incoming request need to wait for outgoing request completion.
 *
 * When the last M0_RPF_PROTECT pin is removed from a credit (credit becomes
 * "cached") or M0_RPF_BARRIER pin is removed, then the list of pins to the
 * credit is scanned. For each M0_RPF_TRACK pin on the list, its incoming
 * request is checked to see whether this was the last tracking pin the request
 * is waiting for.
 *
 * An incoming request might also issue an outgoing request to borrow or revoke
 * some credits, necessary to fulfill the request. An M0_RPF_TRACK pin is added
 * from the incoming request to the credit embedded in the outgoing request
 * (m0_rm_outgoing::rog_want::rl_credit). Multiple incoming requests can pin the
 * same outgoing request. When the outgoing request completes, the incoming
 * requests waiting for it are checked as above.
 *
 * @b Barrier.
 *
 * Barrier is necessary to avoid live-locks and guarantee progress
 * of incoming request processing by pinning the credits with a
 * M0_RPF_BARRIER pin. Only RIF_RESERVE requests pin credits with
 * a M0_RPF_BARRIER.
 *
 * Credit which is pinned with M0_RPF_BARRIER is called "reserved". Only
 * one incoming request can reserve the credit at any given time, others
 * should wait completion of this incoming request (by placing M0_RPF_TRACK
 * pin, as usual). It is possible that already reserved credit should be
 * reassigned to another incoming request with higher reserve priority
 * than the current one reserving the credit (@ref m0_rm_incoming). Such
 * situation is called "barrier overcome".
 *
 * M0_RPF_BARRIER pins are not set for outgoing requests, but they set on
 * outgoing requests replies in the following way. When the request is
 * complete and credit is placed at owner->ro_owned[OWOS_CACHED] list, we
 * check if there are any correspondent RIF_RESERVE incoming requests who
 * pinned it with M0_RPF_TRACK. If there are, the highest priority one is
 * selected and the credit is pinned for it with M0_RPF_BARRIER. It is done
 * this way to add M0_RPF_BARRIER before any other waiting for the same
 * credit request could be excited and possibly grab the credit.
 *
 * @verbatim
 *
 *
 *      ->ro_owned[]--->R------>R        R<------R<----------+
 *                      |       |        |       |           |
 *>ro_incoming[]        |       |        |       |           |
 *      |               |       |        |       |           |
 *      |               |       |        |       |    ->ro_outgoing[]
 *      V               |       |        |       |
 *  INC[CHECK]----------T-------T--------T-------T
 *      |                       |                |
 *      |                       |                |
 *      V                       |                |
 *  INC[SUCCESS]----------------P                |
 *      |                                        |
 *      |                                        |
 *      V                                        |
 *  INC[CHECK]-----------------------------------T
 *
 * @endverbatim
 *
 * On this diagram, INC[S] is an incoming request in a state S, R is a credit, T
 * is an M0_RPF_TRACK pin and P is an M0_RPF_PROTECT pin.
 *
 * The incoming request in the middle has been processed successfully and now
 * protects its credit.
 *
 * The topmost incoming request waits for 2 possessed credits to become unpinned
 * and also waiting for completion of 2 outgoing requests. The incoming request
 * on the bottom waits for completion of the same outgoing request.
 *
 * m0_rm_credit_put() scans the request's pin list (horizontal direction) and
 * removes all pins. If the last pin was removed from a credit, credit's pin
 * list is scanned (vertical direction), checking incoming requests for possible
 * state transitions.
 */
struct m0_rm_pin {
	uint32_t               rp_flags;
	struct m0_rm_credit   *rp_credit;
	/** An incoming request that stuck this pin. */
	struct m0_rm_incoming *rp_incoming;
	/**
	 * Linkage into a list of all pins for a credit, hanging off
	 * m0_rm_credit::cr_pins.
	 */
	struct m0_tlink        rp_credit_linkage;
	/**
	 * Linkage into a list of all pins, held to satisfy an incoming
	 * request. This list hangs off m0_rm_incoming::rin_pins.
	 */
	struct m0_tlink        rp_incoming_linkage;
	uint64_t               rp_magix;
};

M0_INTERNAL void m0_rm_domain_init(struct m0_rm_domain *dom);
M0_INTERNAL void m0_rm_domain_fini(struct m0_rm_domain *dom);

/**
 * Registers a resource type with a domain.
 *
 * @pre  rtype->rt_dom == NULL
 * @post IS_IN_ARRAY(rtype->rt_id, dom->rd_types) && rtype->rt_dom == dom
 */
M0_INTERNAL int m0_rm_type_register(struct m0_rm_domain *dom,
				    struct m0_rm_resource_type *rt);

/**
 * Deregisters a resource type.
 *
 * @pre  IS_IN_ARRAY(rtype->rt_id, dom->rd_types) && rtype->rt_dom != NULL
 * @post rtype->rt_dom == NULL
 */
M0_INTERNAL void m0_rm_type_deregister(struct m0_rm_resource_type *rtype);

/**
 * Lookup registered resource type from given domain.
 */
M0_INTERNAL struct m0_rm_resource_type *
m0_rm_resource_type_lookup(const struct m0_rm_domain *dom,
			   const uint64_t             rtype_id);

/**
 * Returns a resource equal to a given one from a resource type's resource list
 * or NULL if none.
 */
M0_INTERNAL struct m0_rm_resource *
m0_rm_resource_find(const struct m0_rm_resource_type *rt,
		    const struct m0_rm_resource      *res);

/**
 * Adds a resource to the list of resources and increments resource type
 * reference count.
 *
 * @pre m0_tlist_is_empty(res->r_linkage) && res->r_ref == 0
 * @pre rtype->rt_resources does not contain a resource equal (in the
 *      m0_rm_resource_type_ops::rto_eq() sense) to res
 *
 * @post res->r_ref > 0
 * @post res->r_type == rtype
 * @post m0_tlist_contains(&rtype->rt_resources, &res->r_linkage)
 */
M0_INTERNAL void m0_rm_resource_add(struct m0_rm_resource_type *rtype,
				    struct m0_rm_resource *res);
/**
 * Removes a resource from the list of resources. Dual to m0_rm_resource_add().
 *
 * @pre res->r_type->rt_nr_resources > 0
 * @pre m0_tlist_contains(&rtype->rt_resources, &res->r_linkage)
 *
 * @post !m0_tlist_contains(&rtype->rt_resources, &res->r_linkage)
 */
M0_INTERNAL void m0_rm_resource_del(struct m0_rm_resource *res);

/**
 * Frees the resource.
 * A mero resource may be embedded in some structure. This function calls
 * resource operation, rop_resource_free.
 *
 * @pre res->r_ops->rop_resource_free != NULL
 */
M0_INTERNAL void m0_rm_resource_free(struct m0_rm_resource *res);

/**
 * Encode resource onto a buffer.
 *
 * @pre res->r_type != 0
 * @pre res->r_type->rt_ops != NULL
 */
M0_INTERNAL int m0_rm_resource_encode(struct m0_rm_resource *res,
				      struct m0_buf         *buf);

/**
 * Assign initial value to credit
 */
M0_INTERNAL void
m0_rm_resource_initial_credit(const struct m0_rm_resource *resource,
			      struct m0_rm_credit *credit);

/**
 * Initialises owner fields and increments resource reference counter.
 *
 * The owner's credit lists are initially empty.
 *
 * @pre creditor->rem_state >= REM_SERVICE_LOCATED
 * @pre fid != NULL && m0_fid_tget(fid) == M0_RM_OWNER_FT
 *
 * @post M0_IN(owner->ro_state, (ROS_INITIALISING, ROS_ACTIVE)) &&
 *       owner->ro_resource == res)
 */
M0_INTERNAL void m0_rm_owner_init(struct m0_rm_owner      *owner,
				  struct m0_fid           *fid,
				  const struct m0_uint128 *group,
				  struct m0_rm_resource   *res,
				  struct m0_rm_remote     *creditor);

/**
 * The same as m0_rm_owner_init, but owner FID is generated randomly.
 *
 * @see m0_rm_owner_init
 */
M0_INTERNAL void m0_rm_owner_init_rfid(struct m0_rm_owner      *owner,
				       const struct m0_uint128 *group,
				       struct m0_rm_resource   *res,
				       struct m0_rm_remote     *creditor);

/**
 * Loans a credit to an owner from itself.
 *
 * This is used to initialise a "top-most" resource owner that has no upward
 * creditor.
 *
 * This call doesn't copy "r": user supplied credit is linked into owner lists.
 *
 * @see m0_rm_owner_init()
 *
 * @pre  owner->ro_state == ROS_INITIALISING
 * @post owner->ro_state == ROS_ACTIVE
 * @post m0_tlist_contains(&owner->ro_owned[OWOS_CACHED], &r->cr_linkage))
 */
M0_INTERNAL int m0_rm_owner_selfadd(struct m0_rm_owner *owner,
				    struct m0_rm_credit *r);

/**
 * Wind up the owner before finalising it. This function will revoke sublets
 * and give up loans.
 *
 * @pre M0_IN(owner->ro_state, (ROS_ACTIVE, ROS_QUIESCE))
 * @see m0_rm_owner_fini
 *
 */
M0_INTERNAL void m0_rm_owner_windup(struct m0_rm_owner *owner);

/**
 * Set new creditor for the owner. User is not allowed to change the creditor
 * at the arbitrary point in time. It's possible in two cases:
 * - Creditor is not set (owner->ro_creditor == NULL);
 * - Previous creditor is considered dead by HA and owner made internal cleanup
 *   of all credits and changed its state to ROS_DEAD_CREDITOR.
 *
 * @pre owner->ro_state == ROS_DEAD_CREDITOR || owner->ro_creditor == NULL
 */
M0_INTERNAL void m0_rm_owner_creditor_reset(struct m0_rm_owner  *owner,
					    struct m0_rm_remote *creditor);

/**
 * Wait for owner to get to a particular state. Once the winding up process
 * on owner has started, it can take a while. The following function will
 * typically used to check if the owner has reached ROS_FINAL state.
 * The user can then safely call m0_rm_owner_fini().
 * Calling m0_rm_owner_fini() immediately after m0_rm_owner_windup() may
 * cause unexpected behaviour.
 */
M0_INTERNAL int m0_rm_owner_timedwait(struct m0_rm_owner *owner,
				      uint64_t state,
				      const m0_time_t abs_timeout);
/**
 * Finalises the owner. Dual to m0_rm_owner_init().
 *
 * @pre M0_IN(owner->ro_state, (ROS_FINAL, ROS_INSOLVENT, ROS_DEAD_CREDITOR))
 * @pre m0_tlist_is_empty(owner->ro_borrowed) &&
 *      m0_tlist_is_empty(owner->ro_sublet) &&
 *      m0_tlist_is_empty(owner->ro_owned[*]) &&
 *      m0_tlist_is_empty(owner->ro_incoming[*][*]) &&
 *      m0_tlist_is_empty(owner->ro_outgoing[*]) &&
 * @see m0_rm_owner_timedwait()
 */
M0_INTERNAL void m0_rm_owner_fini(struct m0_rm_owner *owner);

/**
 * Locks state machine group of an owner
 */
M0_INTERNAL void m0_rm_owner_lock(struct m0_rm_owner *owner);
/**
 * Unlocks state machine group of an owner
 */
M0_INTERNAL void m0_rm_owner_unlock(struct m0_rm_owner *owner);

/**
 * Initialises generic fields in struct m0_rm_credit.
 *
 * This is called by generic RM code to initialise an empty credit of any
 * resource type and by resource type specific code to initialise generic fields
 * of a struct m0_rm_credit.
 *
 * This function calls m0_rm_resource_ops::rop_credit_init().
 */
M0_INTERNAL void m0_rm_credit_init(struct m0_rm_credit *credit,
				   struct m0_rm_owner *owner);

/**
 * Finalised generic fields in struct m0_rm_credit. Dual to m0_rm_credit_init().
 */
M0_INTERNAL void m0_rm_credit_fini(struct m0_rm_credit *credit);

/**
 * @param src_credit - A source credit which is to be duplicated.
 * @param dest_credit - A destination credit. This credit will be allocated,
 *                     initialised and then filled with src_credit.
 * Allocates and duplicates a credit.
 */
M0_INTERNAL int m0_rm_credit_dup(const struct m0_rm_credit *src_credit,
				struct m0_rm_credit **dest_credit);

/*
 * Makes another copy of credit src.
 */
M0_INTERNAL int
m0_rm_credit_copy(struct m0_rm_credit *dst, const struct m0_rm_credit *src);

/**
 * Initialises the fields of incoming structure.
 * This creates an incoming request with an empty m0_rm_incoming::rin_want
 * credit.
 *
 * @param in - incoming credit request structure
 * @param owner - for which incoming request is intended.
 * @param type - incoming request type
 * @param policy - applicable policy
 * @param flags - type of request (borrow, revoke, local)
 * @see m0_rm_incoming_fini
 */
M0_INTERNAL void m0_rm_incoming_init(struct m0_rm_incoming *in,
				     struct m0_rm_owner *owner,
				     enum m0_rm_incoming_type type,
				     enum m0_rm_incoming_policy policy,
				     uint64_t flags);

/**
 * Finalises the fields of
 * @param in
 * @see Dual to m0_rm_incoming_init().
 */
M0_INTERNAL void m0_rm_incoming_fini(struct m0_rm_incoming *in);

/**
 * Initialises the fields of remote owner.
 * @param rem
 * @param res - Resource for which proxy is obtained.
 * @see m0_rm_remote_fini
 */
M0_INTERNAL void m0_rm_remote_init(struct m0_rm_remote *rem,
				   struct m0_rm_resource *res);

/**
 * Finalises the fields of remote owner.
 *
 * @param rem
 * @see m0_rm_remote_init
 * @pre rem->rem_state == REM_INITIALISED ||
 *      rem->rem_state == REM_SERVICE_LOCATED ||
 *      rem->rem_state == REM_OWNER_LOCATED
 */
M0_INTERNAL void m0_rm_remote_fini(struct m0_rm_remote *rem);

/**
 * Starts a state machine for a resource usage credit request. Adds pins for
 * this request. Asynchronous operation - the credit will not generally be held
 * at exit.
 *
 * @pre IS_IN_ARRAY(in->rin_priority, owner->ro_incoming)
 * @pre in->rin_state == RI_INITIALISED
 * @pre m0_tlist_is_empty(&in->rin_want.cr_linkage)
 *
 */
M0_INTERNAL void m0_rm_credit_get(struct m0_rm_incoming *in);

/**
 * Allocates suitably sized buffer and encode it into that buffer.
 */
M0_INTERNAL int m0_rm_credit_encode(struct m0_rm_credit *credit,
				    struct m0_buf *buf);

/**
 * Decodes a credit from its serialised presentation.
 */
M0_INTERNAL int m0_rm_credit_decode(struct m0_rm_credit *credit,
				   struct m0_buf *buf);

/**
 * Releases the credit pinned by struct m0_rm_incoming.
 *
 * @pre in->rin_state == RI_SUCCESS
 * @post m0_tlist_empty(&in->rin_pins)
 */
M0_INTERNAL void m0_rm_credit_put(struct m0_rm_incoming *in);

/** @} */

/**
 * @defgroup rmnet Resource Manager Networking
 */
/** @{ */

/**
 * Constructs a remote owner associated with "credit".
 *
 * After this function returns, "other" is in the process of locating the remote
 * service and remote owner, as described in the comment on m0_rm_remote.
 */
M0_INTERNAL int m0_rm_net_locate(struct m0_rm_credit *credit,
				 struct m0_rm_remote *other);

/**
   @todo Assigns a service to a given remote.

   @pre  rem->rem_state < REM_SERVICE_LOCATED
   @post rem->rem_state == REM_SERVICE_LOCATED

M0_INTERNAL void m0_rm_remote_service_set(struct m0_rm_remote *rem,
					  struct m0_service_id *sid);
*/

/**
 * Assigns an owner id to a given remote.
 *
 * @pre  rem->rem_state < REM_OWNER_LOCATED
 * @post rem->rem_state == REM_OWNER_LOCATED
 */
M0_INTERNAL void m0_rm_remote_owner_set(struct m0_rm_remote *rem, uint64_t id);

/** @} end of Resource manager networking */

/* __MERO_RM_RM_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
