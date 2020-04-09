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
 * Original author: Mikhail Antropov <mikhail.v.antropov@seagate.com>
 * Original creation date: 08/06/2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_IOSERVICE
#include "lib/trace.h"

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/finject.h"            /* M0_FI_ENABLED */
#include "lib/string.h"             /* m0_strdup */
#include "balloc/balloc.h"          /* b2m0 */
#include "conf/obj.h"               /* m0_conf_sdev */
#include "conf/confc.h"             /* m0_confc_from_obj */
#include "conf/helpers.h"           /* m0_conf_drive_get */
#include "conf/obj_ops.h"           /* M0_CONF_DIRNEXT */
#include "stob/ad.h"                /* m0_stob_ad_domain2balloc */
#include "stob/linux.h"             /* m0_stob_linux_domain_directio */
#include "stob/stob.h"              /* m0_stob_id_get */
#include "ioservice/fid_convert.h"  /* m0_fid_validate_linuxstob */
#include "ioservice/storage_dev.h"
#include "reqh/reqh.h"              /* m0_reqh */
#include <unistd.h>                 /* fdatasync */
#include <sys/vfs.h>                /* fstatfs */
#ifndef __KERNEL__
#  include "pool/pool.h"            /* m0_pools_common */
#endif

/**
   @addtogroup sdev

   @{
 */

/**
 * tlist descriptor for list of m0_storage_dev objects placed
 * in m0_storage_devs::sds_devices list using isd_linkage.
 */
M0_TL_DESCR_DEFINE(storage_dev, "storage_dev", M0_INTERNAL,
		   struct m0_storage_dev, isd_linkage, isd_magic,
		   M0_STORAGE_DEV_MAGIC, M0_STORAGE_DEV_HEAD_MAGIC);

M0_TL_DEFINE(storage_dev, M0_INTERNAL, struct m0_storage_dev);

static bool storage_dev_state_update_cb(struct m0_clink *link);
static bool storage_devs_conf_expired_cb(struct m0_clink *link);
static bool storage_devs_conf_ready_async_cb(struct m0_clink *link);

static bool storage_devs_is_locked(const struct m0_storage_devs *devs)
{
	return devs->sds_locks_disabled || m0_mutex_is_locked(&devs->sds_lock);
}

M0_INTERNAL void m0_storage_devs_lock(struct m0_storage_devs *devs)
{
	if (!devs->sds_locks_disabled)
		m0_mutex_lock(&devs->sds_lock);
}

M0_INTERNAL void m0_storage_devs_unlock(struct m0_storage_devs *devs)
{
	if (!devs->sds_locks_disabled)
		m0_mutex_unlock(&devs->sds_lock);
}

M0_INTERNAL int m0_storage_devs_init(struct m0_storage_devs   *devs,
				     enum m0_storage_dev_type  type,
				     struct m0_be_seg         *be_seg,
				     struct m0_stob_domain    *bstore_dom,
				     struct m0_reqh           *reqh)
{
	M0_ENTRY();
	M0_PRE(equi(bstore_dom != NULL, type == M0_STORAGE_DEV_TYPE_AD));

	devs->sds_type           = type;
	devs->sds_be_seg         = be_seg;
	devs->sds_back_domain    = bstore_dom;
	devs->sds_locks_disabled = false;
	storage_dev_tlist_init(&devs->sds_devices);
	m0_mutex_init(&devs->sds_lock);
	m0_clink_init(&devs->sds_conf_ready_async,
		      storage_devs_conf_ready_async_cb);
	m0_clink_init(&devs->sds_conf_exp, storage_devs_conf_expired_cb);
	m0_clink_add_lock(&reqh->rh_conf_cache_exp, &devs->sds_conf_exp);
	m0_clink_add_lock(&reqh->rh_conf_cache_ready_async,
			  &devs->sds_conf_ready_async);
	return M0_RC(m0_parallel_pool_init(&devs->sds_pool, 10, 20));
}

M0_INTERNAL void m0_storage_devs_fini(struct m0_storage_devs *devs)
{
	struct m0_storage_dev  *dev;

	M0_ENTRY();
	m0_parallel_pool_terminate_wait(&devs->sds_pool);
	m0_parallel_pool_fini(&devs->sds_pool);
	m0_clink_cleanup(&devs->sds_conf_exp);
	m0_clink_cleanup(&devs->sds_conf_ready_async);
	m0_clink_fini(&devs->sds_conf_exp);
	m0_clink_fini(&devs->sds_conf_ready_async);

	m0_tl_for(storage_dev, &devs->sds_devices, dev) {
		M0_LOG(M0_DEBUG, "fini: dev=%p, ref=%" PRIi64
		       "state=%d type=%d, %"PRIu64,
		       dev,
		       m0_ref_read(&dev->isd_ref),
		       dev->isd_ha_state,
		       dev->isd_srv_type,
		       dev->isd_cid);
	} m0_tl_endfor;

	storage_dev_tlist_fini(&devs->sds_devices);
	m0_mutex_fini(&devs->sds_lock);
	M0_LEAVE();
}

M0_INTERNAL void m0_storage_devs_use_directio(struct m0_storage_devs *devs,
					      bool                    directio)
{
	M0_PRE(storage_dev_tlist_is_empty(&devs->sds_devices));
	M0_PRE(ergo(devs->sds_type == M0_STORAGE_DEV_TYPE_AD,
		    m0_stob_linux_domain_directio(devs->sds_back_domain) ==
		    directio));

	devs->sds_use_directio = directio;
}

M0_INTERNAL void m0_storage_devs_locks_disable(struct m0_storage_devs *devs)
{
	M0_PRE(!storage_devs_is_locked(devs));
	devs->sds_locks_disabled = true;
}

M0_INTERNAL struct m0_storage_dev *
m0_storage_devs_find_by_cid(struct m0_storage_devs *devs,
			    uint64_t                cid)
{
	M0_PRE(storage_devs_is_locked(devs));
	return m0_tl_find(storage_dev, dev, &devs->sds_devices,
			  dev->isd_cid == cid);
}

M0_INTERNAL struct m0_storage_dev *
m0_storage_devs_find_by_dom(struct m0_storage_devs *devs,
			    struct m0_stob_domain  *dom)
{
	M0_PRE(storage_devs_is_locked(devs));
	return m0_tl_find(storage_dev, dev, &devs->sds_devices,
			  dev->isd_domain == dom);
}

M0_INTERNAL void m0_storage_dev_clink_add(struct m0_clink *link,
					  struct m0_chan *chan)
{
	m0_clink_init(link, storage_dev_state_update_cb);
	m0_clink_add_lock(chan, link);
}

M0_INTERNAL void m0_storage_dev_clink_del(struct m0_clink *link)
{
	m0_clink_cleanup(link);
	m0_clink_fini(link);
}

static bool storage_dev_state_update_cb(struct m0_clink *link)
{
	struct m0_storage_dev *dev =
		container_of(link, struct m0_storage_dev, isd_clink);
	struct m0_conf_obj *obj =
		container_of(link->cl_chan, struct m0_conf_obj, co_ha_chan);
	M0_PRE(m0_conf_fid_type(&obj->co_id) == &M0_CONF_SDEV_TYPE);
	dev->isd_ha_state = obj->co_ha_state;
	return true;
}

static void dev_filename_update(struct m0_storage_dev    *dev,
				const struct m0_conf_obj *obj)
{
	M0_ENTRY();
	m0_free0(&dev->isd_filename);
	dev->isd_filename = m0_strdup(
		M0_CONF_CAST(obj, m0_conf_sdev)->sd_filename);
	if (dev->isd_filename == NULL)
		M0_ERR_INFO(-ENOMEM, "Unable to duplicate sd_filename %s for "
			    FID_F, M0_CONF_CAST(obj, m0_conf_sdev)->sd_filename,
			    FID_P(&obj->co_id));
	M0_LEAVE();
}

static bool storage_devs_conf_expired_cb(struct m0_clink *clink)
{
	struct m0_storage_dev  *dev;
	struct m0_conf_obj     *obj;
	struct m0_storage_devs *storage_devs = M0_AMB(storage_devs, clink,
						      sds_conf_exp);
	struct m0_reqh         *reqh = M0_AMB(reqh, clink->cl_chan,
					      rh_conf_cache_exp);
	struct m0_pools_common *pc = reqh->rh_pools;
	struct m0_conf_cache   *cache = &m0_reqh2confc(reqh)->cc_cache;
	struct m0_fid           sdev_fid;

	if (M0_FI_ENABLED("skip_storage_devs_expire_cb"))
		return true;

	M0_ENTRY();
	m0_storage_devs_lock(storage_devs);
	m0_tl_for (storage_dev, &storage_devs->sds_devices, dev) {
		/*
		 * Step 1. Need to save current sdev filename attribute in
		 * order to use it later in storage_dev_update_by_conf() when
		 * new conf is ready.
		 */
		if (dev->isd_srv_type != M0_CST_IOS &&
		    dev->isd_srv_type != M0_CST_CAS &&
		    /* In some cases indices are valid even though it is
		     * non-IOS/non-CAS device. i.e. in spiel-conf-ut big-db
		     * test, device ID is hardcoded to
		     * M0_AD_STOB_DOM_KEY_DEFAULT (0x1), In such case should
		     * not skip through loop.
		     */
		    dev->isd_cid >= pc->pc_nr_devices) {
			/*
			 * For non-IOS/non-CAS devices base device ids
			 * (dev->isd_cid) are lifted up by design and they
			 * can not be valid indices for pc->pc_dev2svc[ ].
			 */
			continue;
		}
		M0_ASSERT(dev->isd_cid < pc->pc_nr_devices);

		sdev_fid = pc->pc_dev2svc[dev->isd_cid].pds_sdev_fid;
		if (m0_fid_is_set(&sdev_fid) &&
		    /*
		     * Not all storage devices have a corresponding m0_conf_sdev
		     * object.
		     */
		    (obj = m0_conf_cache_lookup(cache, &sdev_fid)) != NULL)
			dev_filename_update(dev, obj);
		/*
		 * Step 2. Un-link from HA chan to let conf cache be ultimately
		 * drained by rconfc.
		 */
		if (!m0_clink_is_armed(&dev->isd_clink))
			continue;
		obj = M0_AMB(obj, dev->isd_clink.cl_chan, co_ha_chan);
		M0_ASSERT(m0_conf_obj_invariant(obj));
		if (!m0_fid_is_set(&sdev_fid))
			/* Step 1, second chance to catch up with filename. */
			dev_filename_update(dev, obj);
		/* Un-link from HA chan and un-pin the conf object. */
		m0_storage_dev_clink_del(&dev->isd_clink);
		m0_confc_close(obj);
		dev->isd_ha_state = M0_NC_UNKNOWN;
	} m0_tl_endfor;
	m0_storage_devs_unlock(storage_devs);
	M0_LEAVE();
	return true;
}

static int storage_dev_update_by_conf(struct m0_storage_dev  *dev,
				      struct m0_conf_sdev    *sdev,
				      struct m0_storage_devs *storage_devs)
{
	struct m0_storage_dev *dev_new;
	int                    rc;

	M0_ENTRY("dev cid:%"PRIx64", fid: "FID_F", old: %s, new: %s",
		 dev->isd_cid, FID_P(&sdev->sd_obj.co_id), dev->isd_filename,
		 M0_MEMBER(sdev, sd_filename));
	M0_PRE(sdev != NULL);
	M0_PRE(storage_devs_is_locked(storage_devs));

	if (m0_streq(dev->isd_filename, sdev->sd_filename))
		return M0_RC(0);  /* the dev did not change */

	M0_PRE(m0_ref_read(&dev->isd_ref) == 1);
	m0_storage_dev_detach(dev);
	rc = m0_storage_dev_new_by_conf(storage_devs, sdev, false, &dev_new);
	if (rc != 0)
		return M0_ERR(rc);
	M0_ASSERT(dev_new != NULL);
	m0_storage_dev_attach(dev_new, storage_devs);
	return M0_RC(0);
}

static void storage_devs_conf_refresh(struct m0_storage_devs *storage_devs,
				      struct m0_reqh         *reqh)
{
	struct m0_confc       *confc = m0_reqh2confc(reqh);
	struct m0_storage_dev *dev;
	struct m0_fid          sdev_fid;
	struct m0_conf_sdev   *conf_sdev;
	int                    rc;

	M0_ENTRY();
	M0_PRE(storage_devs_is_locked(storage_devs));

	m0_tl_for(storage_dev, &storage_devs->sds_devices, dev) {
		rc = m0_conf_device_cid_to_fid(confc, dev->isd_cid,
					       &sdev_fid);
		if (rc != 0)
			/* Not all storage devices have a corresponding
			 * m0_conf_sdev object.
			 */
			continue;
		conf_sdev = NULL;
		rc = m0_conf_sdev_get(confc, &sdev_fid, &conf_sdev) ?:
			storage_dev_update_by_conf(dev, conf_sdev,
						   storage_devs);
		if (rc != 0)
			M0_ERR(rc);
		if (conf_sdev != NULL)
			m0_confc_close(&conf_sdev->sd_obj);
	} m0_tl_endfor;
	M0_LEAVE();
}

static bool storage_devs_conf_ready_async_cb(struct m0_clink *clink)
{
	struct m0_storage_devs *storage_devs = M0_AMB(storage_devs, clink,
						      sds_conf_ready_async);
	struct m0_reqh         *reqh = M0_AMB(reqh, clink->cl_chan,
					      rh_conf_cache_ready_async);
	struct m0_storage_dev  *dev;
	struct m0_confc        *confc = m0_reqh2confc(reqh);
	struct m0_fid           sdev_fid;
	struct m0_conf_sdev    *conf_sdev = NULL;
	struct m0_conf_service *conf_service;
	struct m0_conf_obj     *srv_obj;
	int                     rc;

	if (M0_FI_ENABLED("skip_storage_devs_ready_cb"))
		return true;

	M0_ENTRY();
	m0_storage_devs_lock(storage_devs);
	storage_devs_conf_refresh(storage_devs, reqh);

	m0_tl_for (storage_dev, &storage_devs->sds_devices, dev) {
		rc = m0_conf_device_cid_to_fid(confc, dev->isd_cid, &sdev_fid);
		if (rc != 0)
			/* Not all storage devices have a corresponding
			 * m0_conf_sdev object.
			 */
			continue;
		rc = m0_conf_sdev_get(confc, &sdev_fid, &conf_sdev);
		M0_ASSERT_INFO(rc == 0, "No sdev: "FID_F, FID_P(&sdev_fid));
		M0_ASSERT(conf_sdev != NULL);
		M0_LOG(M0_DEBUG, "cid:0x%"PRIx64" -> sdev_fid:"FID_F" idx:0x%x",
		       dev->isd_cid, FID_P(&sdev_fid), conf_sdev->sd_dev_idx);
		if (!m0_clink_is_armed(&dev->isd_clink))
			m0_storage_dev_clink_add(&dev->isd_clink,
						 &conf_sdev->sd_obj.co_ha_chan);
		dev->isd_ha_state = conf_sdev->sd_obj.co_ha_state;
		srv_obj = m0_conf_obj_grandparent(&conf_sdev->sd_obj);
		conf_service = M0_CONF_CAST(srv_obj, m0_conf_service);
		dev->isd_srv_type = conf_service->cs_type;
	} m0_tl_endfor;

	m0_storage_devs_unlock(storage_devs);
	M0_LEAVE();
	return true;
}

static int stob_domain_create_or_init(struct m0_storage_dev  *dev,
				      struct m0_storage_devs *devs,
				      m0_bcount_t             size,
				      bool                    force)
{
	enum m0_storage_dev_type  type     = dev->isd_type;
	unsigned long long        cid      = (unsigned long long)dev->isd_cid;
	char                     *cfg      = NULL;
	char                     *cfg_init = NULL;
	char                     *location;
	int                       len;
	int                       rc;

	M0_PRE(ergo(type == M0_STORAGE_DEV_TYPE_LINUX,
		    dev->isd_filename != NULL));
	M0_PRE(ergo(type == M0_STORAGE_DEV_TYPE_AD, devs->sds_be_seg != NULL));
	M0_PRE(ergo(type == M0_STORAGE_DEV_TYPE_AD, dev->isd_stob != NULL));

	switch (type) {
	case M0_STORAGE_DEV_TYPE_LINUX:
		len = snprintf(NULL, 0, "linuxstob:%s", dev->isd_filename);
		location = len > 0 ? m0_alloc(len + 1) : NULL;
		if (location == NULL)
			return len < 0 ? M0_ERR(len) : M0_ERR(-ENOMEM);
		rc = snprintf(location, len + 1, "linuxstob:%s",
			      dev->isd_filename);
		cfg_init = devs->sds_use_directio ? "directio=true" :
						    "directio=false";
		M0_ASSERT_INFO(rc == len, "rc=%d", rc);
		break;
	case M0_STORAGE_DEV_TYPE_AD:
		len = snprintf(NULL, 0, "adstob:%llu", cid);
		location = len > 0 ? m0_alloc(len + 1) : NULL;
		if (location == NULL)
			return len < 0 ? M0_ERR(len) : M0_ERR(-ENOMEM);
		rc = snprintf(location, len + 1, "adstob:%llu", cid);
		M0_ASSERT_INFO(rc == len, "rc=%d", rc);
		m0_stob_ad_cfg_make(&cfg, devs->sds_be_seg,
				    m0_stob_id_get(dev->isd_stob), size);
		m0_stob_ad_init_cfg_make(&cfg_init,
					 devs->sds_be_seg->bs_domain);
		if (cfg == NULL || cfg_init == NULL) {
			m0_free(location);
			m0_free(cfg_init);
			m0_free(cfg);
			return M0_ERR(-ENOMEM);
		}
		break;
	default:
		M0_IMPOSSIBLE("Unknown m0_storage_dev type.");
	};

	rc = m0_stob_domain_init(location, cfg_init, &dev->isd_domain);
	if (force && rc == 0) {
		rc = m0_stob_domain_destroy(dev->isd_domain);
		if (rc != 0)
			goto out_free;
	}
	if (force || rc != 0)
		rc = m0_stob_domain_create(location, cfg_init, cid, cfg,
					   &dev->isd_domain);
out_free:
	m0_free(location);
	if (type == M0_STORAGE_DEV_TYPE_AD)
		m0_free(cfg_init);
	m0_free(cfg);

	return M0_RC(rc);
}

static void storage_dev_release(struct m0_ref *ref)
{
	struct m0_storage_dev *dev =
		container_of(ref, struct m0_storage_dev, isd_ref);

	M0_ENTRY("dev=%p", dev);

	storage_dev_tlink_del_fini(dev);
	m0_chan_broadcast_lock(&dev->isd_detached_chan);
	m0_storage_dev_destroy(dev);

	M0_LEAVE();
}

M0_INTERNAL void m0_storage_dev_get(struct m0_storage_dev *dev)
{
	m0_ref_get(&dev->isd_ref);
}

M0_INTERNAL void m0_storage_dev_put(struct m0_storage_dev *dev)
{
	m0_ref_put(&dev->isd_ref);
}

static int storage_dev_new(struct m0_storage_devs *devs,
			   uint64_t                cid,
			   bool                    fi_no_dev,
			   const char             *path_orig,
			   uint64_t                size,
			   struct m0_conf_sdev    *conf_sdev,
			   bool                    force,
			   struct m0_storage_dev **out)
{
	enum m0_storage_dev_type  type = devs->sds_type;
	struct m0_storage_dev    *device;
	struct m0_conf_service   *conf_service;
	struct m0_conf_obj       *srv_obj;
	struct m0_stob_id         stob_id;
	struct m0_stob           *stob;
	const char               *path = fi_no_dev ? NULL : path_orig;
	int                       rc;

	M0_ENTRY("cid=%"PRIu64, cid);
	M0_PRE(M0_IN(type, (M0_STORAGE_DEV_TYPE_LINUX, M0_STORAGE_DEV_TYPE_AD)));
	M0_PRE(ergo(type == M0_STORAGE_DEV_TYPE_LINUX, path_orig != NULL));

	M0_ALLOC_PTR(device);
	if (device == NULL)
		return M0_ERR(-ENOMEM);

	if (path_orig != NULL) {
		device->isd_filename = m0_strdup(path_orig);
		if (device->isd_filename == NULL) {
			m0_free(device);
			return M0_ERR(-ENOMEM);
		}
	}
	m0_ref_init(&device->isd_ref, 0, storage_dev_release);
	device->isd_type = type;
	device->isd_cid  = cid;
	device->isd_stob = NULL;

	if (type == M0_STORAGE_DEV_TYPE_AD) {
		m0_stob_id_make(0, cid, &devs->sds_back_domain->sd_id, &stob_id);
		rc = m0_stob_find(&stob_id, &stob);
		if (rc != 0)
			goto end;

		if (m0_stob_state_get(stob) == CSS_UNKNOWN) {
			rc = m0_stob_locate(stob);
			if (rc != 0)
				goto stob_put;
		}
		if (m0_stob_state_get(stob) == CSS_NOENT) {
			rc = m0_stob_create(stob, NULL, path);
			if (rc != 0)
				goto stob_put;
		}
		device->isd_stob = stob;
	}

	rc = stob_domain_create_or_init(device, devs, size, force);
	M0_ASSERT(ergo(rc == 0, device->isd_domain != NULL));
	if (rc == 0) {
		if (M0_FI_ENABLED("ad_domain_locate_fail")) {
			m0_stob_domain_fini(device->isd_domain);
			M0_ASSERT(conf_sdev != NULL);
			m0_confc_close(&conf_sdev->sd_obj);
			rc = M0_ERR(-EINVAL);
		} else if (conf_sdev != NULL) {
			if (type == M0_STORAGE_DEV_TYPE_AD &&
			    m0_fid_validate_linuxstob(&stob_id))
				m0_stob_linux_conf_sdev_associate(stob,
						&conf_sdev->sd_obj.co_id);
			m0_conf_obj_get_lock(&conf_sdev->sd_obj);
			srv_obj = m0_conf_obj_grandparent(&conf_sdev->sd_obj);
			conf_service = M0_CONF_CAST(srv_obj, m0_conf_service);
			m0_storage_dev_clink_add(&device->isd_clink,
					&conf_sdev->sd_obj.co_ha_chan);
			device->isd_srv_type = conf_service->cs_type;
		}
	}
stob_put:
	if (type == M0_STORAGE_DEV_TYPE_AD) {
		/* Release reference, taken by m0_stob_find(). */
		m0_stob_put(stob);
	}
end:
	if (rc == 0) {
		m0_mutex_init(&device->isd_detached_lock);
		m0_chan_init(&device->isd_detached_chan,
			     &device->isd_detached_lock);
		*out = device;
	} else {
		m0_free(M0_MEMBER(device, isd_filename));
		m0_free(device);
	}
	return rc == 0 ? M0_RC(0) :
	       M0_ERR_INFO(rc, "path_orig=%s cid=%"PRIu64" conf_sdev="FID_F,
			   path_orig, cid,
			   FID_P(conf_sdev == NULL ? &M0_FID0 :
				 &conf_sdev->sd_obj.co_id));
}

M0_INTERNAL int m0_storage_dev_new(struct m0_storage_devs *devs,
				   uint64_t                cid,
				   const char             *path,
				   uint64_t                size,
				   struct m0_conf_sdev    *conf_sdev,
				   bool                    force,
				   struct m0_storage_dev **dev)
{
	M0_ENTRY();
	return M0_RC(storage_dev_new(devs, cid,
				     M0_FI_ENABLED("no_real_dev"), path,
				     size, conf_sdev, force, dev));
}

M0_INTERNAL int m0_storage_dev_new_by_conf(struct m0_storage_devs *devs,
					   struct m0_conf_sdev    *sdev,
					   bool                    force,
					   struct m0_storage_dev **dev)
{
	M0_ENTRY();
	return M0_RC(storage_dev_new(devs, sdev->sd_dev_idx,
				     M0_FI_ENABLED("no_real_dev"),
				     sdev->sd_filename, sdev->sd_size,
				     M0_FI_ENABLED("no-conf-dev") ? NULL : sdev,
				     force, dev));
}

M0_INTERNAL void m0_storage_dev_destroy(struct m0_storage_dev *dev)
{
	struct m0_stob *stob;
	int             rc = 0;

	M0_ENTRY();
	M0_PRE(m0_ref_read(&dev->isd_ref) == 0);

	/* Acquire a reference to the backing store for adstob configuration. */
	if (dev->isd_type == M0_STORAGE_DEV_TYPE_AD)
		rc = m0_stob_find(m0_stob_id_get(dev->isd_stob), &stob);

	m0_stob_domain_fini(dev->isd_domain);

	if (dev->isd_type == M0_STORAGE_DEV_TYPE_AD && rc == 0) {
		/* Destroy backing store. */
		if (m0_stob_state_get(stob) == CSS_EXISTS)
			rc = m0_stob_destroy(stob, NULL);
		else
			m0_stob_put(stob);
	}
	if (rc != 0)
		M0_LOG(M0_ERROR, "Failed to destroy backing store rc=%d", rc);
	m0_chan_fini_lock(&dev->isd_detached_chan);
	m0_mutex_fini(&dev->isd_detached_lock);
	m0_free(dev->isd_filename);
	m0_free(dev);
	M0_LEAVE();
}

M0_INTERNAL void m0_storage_dev_attach(struct m0_storage_dev  *dev,
				       struct m0_storage_devs *devs)
{
	M0_ENTRY();
	M0_PRE(storage_devs_is_locked(devs));
	M0_PRE(m0_storage_devs_find_by_cid(devs, dev->isd_cid) == NULL);

	M0_LOG(M0_DEBUG, "get: dev=%p, ref=%" PRIi64 " "
	       "state=%d type=%d, %"PRIu64,
	       dev,
	       m0_ref_read(&dev->isd_ref),
	       dev->isd_ha_state,
	       dev->isd_srv_type,
	       dev->isd_cid);
	m0_storage_dev_get(dev);
	storage_dev_tlink_init_at_tail(dev, &devs->sds_devices);
	M0_LEAVE();
}

M0_INTERNAL void m0_storage_dev_detach(struct m0_storage_dev *dev)
{
	struct m0_conf_obj *obj;

	M0_ENTRY();
	if (m0_clink_is_armed(&dev->isd_clink)) {
		obj = container_of(dev->isd_clink.cl_chan, struct m0_conf_obj,
				   co_ha_chan);
		m0_storage_dev_clink_del(&dev->isd_clink);
		m0_confc_close(obj);
	}
	M0_LOG(M0_DEBUG, "put: dev=%p, ref=%" PRIi64 " "
	       "state=%d type=%d, %"PRIu64,
	       dev,
	       m0_ref_read(&dev->isd_ref),
	       dev->isd_ha_state,
	       dev->isd_srv_type,
	       dev->isd_cid);
	m0_storage_dev_put(dev);
	M0_LEAVE();
}

M0_INTERNAL void m0_storage_dev_space(struct m0_storage_dev   *dev,
				      struct m0_storage_space *space)
{
	struct m0_balloc *balloc;
	struct statfs     st;
	int               fd = -1;
	int               rc;
	int               rc1;

	M0_ENTRY();
	M0_PRE(dev != NULL);

	switch (dev->isd_type) {
	case M0_STORAGE_DEV_TYPE_AD:
		balloc = m0_stob_ad_domain2balloc(dev->isd_domain);
		M0_ASSERT(balloc != NULL);
		*space = (struct m0_storage_space) {
#ifdef __SPARE_SPACE__
			.sds_free_blocks = balloc->cb_sb.bsb_freeblocks +
						balloc->cb_sb.bsb_freespare,
#else
			.sds_free_blocks = balloc->cb_sb.bsb_freeblocks,
#endif
			.sds_block_size  = balloc->cb_sb.bsb_blocksize,
			.sds_avail_blocks = balloc->cb_sb.bsb_freeblocks,
			.sds_total_size  = balloc->cb_sb.bsb_totalsize,
		};
		break;
	case M0_STORAGE_DEV_TYPE_LINUX:
		rc = m0_stob_linux_domain_fd_get(dev->isd_domain, &fd);
		if (rc == 0) {
			rc  = fstatfs(fd, &st);
			rc  = rc == -1 ? M0_ERR(-errno) : 0;
			rc1 = m0_stob_linux_domain_fd_put(dev->isd_domain, fd);
			if (rc1 != 0)
				M0_LOG(M0_ERROR, "m0_stob_linux_domain_fd_put: "
						 "rc=%d", rc1);
		}
		if (rc == 0)
			*space = (struct m0_storage_space) {
				.sds_free_blocks = st.f_bfree,
				.sds_block_size  = st.f_bsize,
				.sds_avail_blocks = st.f_bfree,
				.sds_total_size  = st.f_blocks * st.f_bsize
			};
		if (rc != 0)
			M0_LOG(M0_ERROR, "Can't obtain fstatfs, rc=%d", rc);

		break;
	default:
		M0_IMPOSSIBLE("Unknown storage_dev type.");
	}
	M0_LEAVE();
}

struct storage_devs_wait {
	struct m0_clink     sdw_clink;
	struct m0_semaphore sdw_sem;
};

static bool storage_devs_detached_cb(struct m0_clink *clink)
{
	struct storage_devs_wait *wait =
		container_of(clink, struct storage_devs_wait, sdw_clink);

	m0_semaphore_up(&wait->sdw_sem);
	return true;
}

M0_INTERNAL void m0_storage_devs_detach_all(struct m0_storage_devs *devs)
{
	struct storage_devs_wait  wait = {};
	struct m0_storage_dev    *dev;
	struct m0_clink          *clink;

	M0_ENTRY();
	m0_semaphore_init(&wait.sdw_sem, 0);
	clink = &wait.sdw_clink;
	m0_tl_for(storage_dev, &devs->sds_devices, dev) {
		M0_SET0(clink);
		m0_clink_init(clink, storage_devs_detached_cb);
		clink->cl_is_oneshot = true;
		m0_clink_add_lock(&dev->isd_detached_chan, clink);

		m0_storage_dev_detach(dev);

		m0_semaphore_down(&wait.sdw_sem);
		m0_clink_fini(clink);
	} m0_tl_endfor;
	m0_semaphore_fini(&wait.sdw_sem);
	M0_LEAVE();
}

M0_INTERNAL int m0_storage_dev_format(struct m0_storage_dev *dev,
				      uint64_t               cid)
{
	/*
	 * Nothing do for Format command.
	 */
	return M0_RC(0);
}

static int sdev_stob_fsync(void *psdev)
{
	struct m0_storage_dev *sdev = (struct m0_storage_dev *)psdev;
	int                    fd = -1;
	int                    rc;
	int                    rc1;

	M0_ENTRY("sdev=%p", sdev);

	if (sdev->isd_type == M0_STORAGE_DEV_TYPE_AD) {
		fd = m0_stob_fd(sdev->isd_stob);
		rc = fdatasync(fd);
		rc = rc == 0 ? 0 : M0_ERR_INFO(-errno, "fd=%d", fd);
	} else {
		M0_ASSERT(sdev->isd_type == M0_STORAGE_DEV_TYPE_LINUX);
		rc = m0_stob_linux_domain_fd_get(sdev->isd_domain, &fd);
		if (rc == 0) {
			rc  = syncfs(fd);
			rc  = rc == 0 ? 0 : M0_ERR_INFO(-errno, "fd=%d", fd);
			rc1 = m0_stob_linux_domain_fd_put(sdev->isd_domain, fd);
			rc  = rc ?: rc1;
		}
	}
	return M0_RC(rc);
}

M0_INTERNAL int m0_storage_devs_fdatasync(struct m0_storage_devs *sdevs)
{
	M0_PRE(storage_devs_is_locked(sdevs));

	return M0_PARALLEL_FOR(storage_dev, &sdevs->sds_pool,
			       &sdevs->sds_devices, sdev_stob_fsync);
}

M0_INTERNAL int m0_storage_dev_stob_create(struct m0_storage_devs *devs,
					   struct m0_stob_id      *sid,
					   struct m0_dtx          *dtx)
{
	struct m0_stob *stob = NULL;
	int             rc;

	M0_ENTRY("stob_id="STOB_ID_F, STOB_ID_P(sid));

	m0_storage_devs_lock(devs);
	rc = m0_stob_find(sid, &stob);
	if (rc == 0) {
		M0_ASSERT(stob->so_state != CSS_DELETE);
		rc = rc ?: m0_stob_state_get(stob) == CSS_UNKNOWN ?
			   m0_stob_locate(stob) : 0;
		rc = rc ?: stob->so_state == CSS_NOENT ?
			   m0_stob_create(stob, dtx, NULL) : 0;
		M0_ASSERT_EX(m0_storage_devs_find_by_dom(devs,
			     m0_stob_dom_get(stob)) != NULL);
		m0_stob_put(stob);
	}
	m0_storage_devs_unlock(devs);

	return M0_RC(rc);
}

M0_INTERNAL int m0_storage_dev_stob_destroy(struct m0_storage_devs *devs,
					    struct m0_stob         *stob,
					    struct m0_dtx          *dtx)
{
	struct m0_stob_domain *dom;
	struct m0_storage_dev *dev;
	int                    rc;

	M0_ENTRY("stob=%p stob_id="STOB_ID_F,
		 stob, STOB_ID_P(m0_stob_id_get(stob)));
	M0_PRE(m0_stob_state_get(stob) == CSS_DELETE);

	m0_storage_devs_lock(devs);
	dom = m0_stob_dom_get(stob);
	rc  = m0_stob_destroy(stob, dtx);
	if (rc == 0) {
		dev = m0_storage_devs_find_by_dom(devs, dom);
		M0_ASSERT(dev != NULL);
		M0_LOG(M0_DEBUG, "put: dev=%p, ref=%" PRIi64 " "
		       "state=%d type=%d, %"PRIu64,
		       dev,
		       m0_ref_read(&dev->isd_ref),
		       dev->isd_ha_state,
		       dev->isd_srv_type,
		       dev->isd_cid);
		m0_storage_dev_put(dev);
	}
	m0_storage_devs_unlock(devs);

	return M0_RC(rc);
}

M0_INTERNAL int m0_storage_dev_stob_find(struct m0_storage_devs  *devs,
					 struct m0_stob_id       *sid,
					 struct m0_stob         **stob)
{
	struct m0_stob_domain *dom;
	struct m0_storage_dev *dev;
	struct m0_stob        *stob2 = NULL;
	enum m0_stob_state     stob_state;
	int                    rc;

	M0_ENTRY("stob_id="STOB_ID_F, STOB_ID_P(sid));

	m0_storage_devs_lock(devs);
	rc = m0_stob_find(sid, &stob2);
	if (rc == 0) {
		stob_state = m0_stob_state_get(stob2);
		if (stob_state == CSS_DELETE)
			rc = -ENOENT;
		else if (stob_state == CSS_UNKNOWN)
			   rc = m0_stob_locate(stob2);
		if (rc != 0)
			m0_stob_put(stob2);
	}
	if (rc == 0) {
		dom = m0_stob_dom_get(stob2);
		dev = m0_storage_devs_find_by_dom(devs, dom);
		M0_ASSERT(dev != NULL);
		M0_LOG(M0_DEBUG, "get: dev=%p, ref=%" PRIi64 " "
		       "state=%d type=%d, %"PRIu64,
		       dev,
		       m0_ref_read(&dev->isd_ref),
		       dev->isd_ha_state,
		       dev->isd_srv_type,
		       dev->isd_cid);
		m0_storage_dev_get(dev);
	}
	m0_storage_devs_unlock(devs);

	if (rc == 0)
		*stob = stob2;

	return M0_RC(rc);
}

M0_INTERNAL void m0_storage_dev_stob_put(struct m0_storage_devs *devs,
					 struct m0_stob         *stob)
{
	struct m0_stob_domain *dom;
	struct m0_storage_dev *dev;

	M0_ENTRY("stob=%p stob_id="STOB_ID_F,
		 stob, STOB_ID_P(m0_stob_id_get(stob)));

	dom = m0_stob_dom_get(stob);
	m0_stob_put(stob);

	m0_storage_devs_lock(devs);
	dev = m0_storage_devs_find_by_dom(devs, dom);
	M0_ASSERT(dev != NULL);
	M0_LOG(M0_DEBUG, "put: dev=%p, ref=%" PRIi64 " "
	       "state=%d type=%d, %"PRIu64,
	       dev,
	       m0_ref_read(&dev->isd_ref),
	       dev->isd_ha_state,
	       dev->isd_srv_type,
	       dev->isd_cid);
	m0_storage_dev_put(dev);
	m0_storage_devs_unlock(devs);

	M0_LEAVE();
}

#undef M0_TRACE_SUBSYSTEM
/** @} end group sdev */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
