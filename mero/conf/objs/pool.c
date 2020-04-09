/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Rajanikant Chirmade <rajanikant_chirmade@xyratex.com>
 * Original creation date: 24-Nov-2014
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "conf/objs/common.h"
#include "conf/onwire_xc.h"  /* m0_confx_pool_xc */
#include "mero/magic.h"      /* M0_CONF_POOL_MAGIC */
#include "fid/fid.h"

/**
  @page DLD-pools-in-conf-schema DLD of Pools in Configuration Schema

  - @ref DLD-pools-in-conf-schema-ovw
  - @ref DLD-pools-in-conf-schema-highlights
  - @ref DLD-pools-in-conf-schema-def
  - @ref DLD-pools-in-conf-schema-req
  - @ref DLD-pools-in-conf_schema-depends
  - @ref DLD-pools-in-conf-schema-fspecs
  - @ref DLD-pools-in-conf-schema-lspec
  - @ref DLD-pools-in-conf-schema-ut
  - @ref DLD-pools-in-conf-schema-st
  - @ref DLD-pools-in-conf-schema-ref

  <hr>
  @section DLD-pools-in-conf-schema-ovw Overview
  Pools are used to partition hardware resources (devices, servers). Pool
  versions are used to track changes in pool membership. User can combine
  multiple hardware resources to form a pool. Hardware resource can be
  member of multiple pools.

  <hr>
  @section  DLD-pools-in-conf-schema-def Definitions
  <b>Pool</b> is a set of hardware resources.@n
  <b>Pool Version</b> Change set for pool.@n
  <b>Failure Set</b> Set of failed hardware resources.@n
  <b>Permutation set</b> Random combination sequence of hardware
     resources with same type.@n

  <hr>
  @section DLD-pools-in-conf-schema-highlights Design Highlights
  Hardware resources racks, enclosures & controllers are arranged like tree
  structure. Leaves of this hardware resource tree are storage devices (disks).

  Pool is a root node of this hardware resources tree. There may be multiple
  pools defined by user as per requirements.

  In case of resources failure from particular pool, mero uses different variant
  of same pool called pool versions. There are multiple pool versions available
  with a pool. Every hardware resource  maintains information about it's
  subscription with multiple pool versions.

  Every file in mero is associated with a pool and pool version through file
  attributes assigned on creation of object. It uses user assigned default pool
  version OR in case of failure of some of devices in pool, it fetches pool
  version from configuration which does not contain failed resources.

  A pool version is a tree of virtual objects corresponding to hardware
  resources, viz:- rackv, enclv, crtlv and sdevv. Every virtual object is
  associated with its corresponding physical object.

  <hr>
  @section  DLD-pools-in-conf-schema-req Requirements
  @b r.conf.pool
    Implementation must define pool in configuration schema and it's supporting
    operations.@n
  @b r.conf.pool.pool_version
    Implementation must define pool_version in configuration schema and it's
    supporting operations.@n
  @b r.conf.pool.pool_version.layout
    Pool_version stores distribution permutation along with other layout
    attributes, e.g. N, K, P.@n
  @b r.conf.pool.pool_version_get
    Implementation must provide an interface to efficiently find the next
    available pool version which does not intersect with failure set.

  <hr>
  @section DLD-pools-in-conf_schema-depends Dependencies
  @b failure_domains.permutations : Every pool version needs to keep
    failure permutations for hardware devices like racks, enclosure, controller
    etc. Failure domain implementation exports this interface and pool version
    add uses this interface to get permutations for each type of hardware
    resource.

  <hr>
  @section DLD-pools-in-conf-schema-fspecs Functional Specifications
  @subsection DLD-pools-in-conf-schema-fspecs-data Data Structures

  @ref conf-fspec-obj-data Defined as Configuration objects

  @subsection DLD-pools-in-conf-schema-fspecs-if Interfaces

  - m0_conf_pver_get()

  <hr>
  @section DLD-pools-in-conf-schema-lspec Logical Specifications

  @subsection DLD-pools-in-conf-schema-lspec-pool_version-get Get Pool_version
  Provides interface to find latest pool version which does not intersect
  with the failure set on configuration changes.

  Following algorithm illustrates a simplistic implementation of
  m0_conf_pool_version_get(),
  @verbatim
   1) for each pool_version V in pool P
      - for each device D in failure set F
	- find configuration object O for D in confc
	- for each pool_version v in O.pool_versions
	  - if v.fid == V.fid
	    - Fetch next pool version and repeat from step 1
      - return V
  @endverbatim
  <hr>
  @section DLD-pools-in-conf-schema-ut Unit Tests
  - Test 01 : pool object add/read/delete
  - Test 02 : poolversion object add/read/delete
  - Test 03 : process object add/read/delete
  - Test 04 : rack object add/read/delete
  - Test 05 : enclosure object add/read/delete
  - Test 06 : controller object add/read/delete
  - Test 07 : sdev object add/read/delete
  - Test 08 : m0_conf_pool_version_get() with NULL failure_set
  - Test 09 : m0_conf_pool_version_get() with failure set not NULL
	      single device failure
  - Test 10 : m0_conf_pool_version_get() with failure set not NULL
	      multiple device failure

  <hr>
  @section DLD-pools-in-conf-schema-st System Tests
  - Test 01 : m0t1fs mount, NULL failure set
	      Create file.
  - Test 02 : m0t1fs mount, Some devices reported failed (failure set != NULL).
	      Create file.
  <hr>
  @section DLD-pools-in-conf-schema-ref References
  - <a href="https://docs.google.com/a/seagate.com/document/d/
19IdRJBQLglVi0D8FxZ4cTF9G7QwRmm1Wa9YhbetO5qA/edit#heading=h.dw3bqun6qijh">
  Pools in Configuration Schema</a>
 */

#define XCAST(xobj) ((struct m0_confx_pool *)(&(xobj)->xo_u))
M0_BASSERT(offsetof(struct m0_confx_pool, xp_header) == 0);

static bool pool_check(const void *bob)
{
	const struct m0_conf_pool *self = bob;

	M0_PRE(m0_conf_obj_type(&self->pl_obj) == &M0_CONF_POOL_TYPE);

	return true;
}

M0_CONF__BOB_DEFINE(m0_conf_pool, M0_CONF_POOL_MAGIC, pool_check);
M0_CONF__INVARIANT_DEFINE(pool_invariant, m0_conf_pool);

static int pool_decode(struct m0_conf_obj *dest, const struct m0_confx_obj *src)
{
	struct m0_conf_pool        *d = M0_CONF_CAST(dest, m0_conf_pool);
	const struct m0_confx_pool *s = XCAST(src);

	M0_ENTRY("dest="FID_F, FID_P(&dest->co_id));
	d->pl_pver_policy = s->xp_pver_policy;
	return M0_RC(m0_conf_dir_new(dest, &M0_CONF_POOL_PVERS_FID,
				     &M0_CONF_PVER_TYPE, &s->xp_pvers,
				     &d->pl_pvers));
}

static int
pool_encode(struct m0_confx_obj *dest, const struct m0_conf_obj *src)
{
	struct m0_conf_pool  *s = M0_CONF_CAST(src, m0_conf_pool);
	struct m0_confx_pool *d = XCAST(dest);

	confx_encode(dest, src);
	d->xp_pver_policy = s->pl_pver_policy;
	return M0_RC(arrfid_from_dir(&d->xp_pvers, s->pl_pvers));
}

static bool
pool_match(const struct m0_conf_obj *cached, const struct m0_confx_obj *flat)
{
	const struct m0_confx_pool *xobj = XCAST(flat);
	const struct m0_conf_pool  *obj = M0_CONF_CAST(cached, m0_conf_pool);

	return obj->pl_pver_policy == xobj->xp_pver_policy &&
		m0_conf_dir_elems_match(obj->pl_pvers, &xobj->xp_pvers);
}

static int pool_lookup(const struct m0_conf_obj *parent,
		       const struct m0_fid *name, struct m0_conf_obj **out)
{
	struct m0_conf_pool *pool = M0_CONF_CAST(parent, m0_conf_pool);
	const struct conf_dir_relation dirs[] = {
		{ pool->pl_pvers, &M0_CONF_POOL_PVERS_FID }
	};

	M0_PRE(parent->co_status == M0_CS_READY);
	return M0_RC(conf_dirs_lookup(out, name, dirs, ARRAY_SIZE(dirs)));
}

static const struct m0_fid **pool_downlinks(const struct m0_conf_obj *obj)
{
	static const struct m0_fid *rels[] = { &M0_CONF_POOL_PVERS_FID,
					       NULL };
	M0_PRE(m0_conf_obj_type(obj) == &M0_CONF_POOL_TYPE);
	return rels;
}

static void pool_delete(struct m0_conf_obj *obj)
{
	struct m0_conf_pool *x = M0_CONF_CAST(obj, m0_conf_pool);

	m0_conf_pool_bob_fini(x);
	m0_free(x);
}

static const struct m0_conf_obj_ops pool_ops = {
	.coo_invariant = pool_invariant,
	.coo_decode    = pool_decode,
	.coo_encode    = pool_encode,
	.coo_match     = pool_match,
	.coo_lookup    = pool_lookup,
	.coo_readdir   = NULL,
	.coo_downlinks = pool_downlinks,
	.coo_delete    = pool_delete
};

M0_CONF__CTOR_DEFINE(pool_create, m0_conf_pool, &pool_ops);

const struct m0_conf_obj_type M0_CONF_POOL_TYPE = {
	.cot_ftype = {
		.ft_id   = M0_CONF__POOL_FT_ID,
		.ft_name = "conf_pool"
	},
	.cot_create  = &pool_create,
	.cot_xt      = &m0_confx_pool_xc,
	.cot_branch  = "u_pool",
	.cot_xc_init = &m0_xc_m0_confx_pool_struct_init,
	.cot_magic   = M0_CONF_POOL_MAGIC
};

#undef XCAST
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
