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
 * Original author: Mikhail Antropov <mikhail.v.antropov@seagate.com>
 * Original creation date: 08/06/2015
 */

#pragma once

#ifndef __MERO_IOSERVICE_STORAGE_DEV_H__
#define __MERO_IOSERVICE_STORAGE_DEV_H__

#include "lib/tlist.h"
#include "lib/mutex.h"
#include "lib/thread_pool.h"
#include "lib/chan.h"
#include "ha/note.h"
#include "conf/obj.h"
#include "sm/sm.h"            /* m0_sm_ast */

/* import */
struct m0_stob;
struct m0_stob_id;
struct m0_stob_domain;
struct m0_dtx;
struct m0_be_seg;
struct m0_conf_sdev;
struct m0_reqh;

/**
 * @defgroup sdev Storage devices.
 *
 * This module provides functionality to work with storage devices used by IO
 * service. It is intended to be used to attach/detach storage devices
 * dynamically and query them for some information (i.e. used/free space).
 *
 * Backing store domain and BE segment for storage devices are provided by user.
 *
 * Structure m0_storage_devs contains list of storage devices used by IO
 * service and intended to be the one and only place to hold such a list for
 * IO service.
 *
 * Functions provided by this module do not take any locks internally. User
 * should lock m0_storage_devs structure on demand using
 * m0_storage_devs_lock(), m0_storage_devs_unlock() functions.
 *
 * @{
 */

enum m0_storage_dev_type {
	M0_STORAGE_DEV_TYPE_LINUX = 1,
	M0_STORAGE_DEV_TYPE_AD,
};

/**
 * Data structure represents storage device object.
 */
struct m0_storage_dev {
	/** Type of underlying stobs. */
	enum m0_storage_dev_type   isd_type;
	/** Linux stob which is backing store for the AD stob domain. */
	struct m0_stob            *isd_stob;
	/** Stob domain for stobs of device. */
	struct m0_stob_domain     *isd_domain;
	/** Storage device ID. */
	uint64_t                   isd_cid;
	/** Linkage into list of storage devices. */
	struct m0_tlink            isd_linkage;
	/**
	 * A link to receive HA state change notification. It waits
	 * on disk obj's wait channel, m0_conf_obj::co_ha_chan.
	 */
	struct m0_clink            isd_clink;
	/** HA state associated with the disk corresponding to the
	 * storage device.
	 */
	enum m0_ha_obj_state       isd_ha_state;
	/** Type of the parent service. */
	enum m0_conf_service_type  isd_srv_type;
	/** Magic for isd_linkage */
	uint64_t                   isd_magic;
	/**
	 * Reference counter. Stob attach operation and a user which performs
	 * I/O to the stob must hold reference to this object.
	 */
	struct m0_ref              isd_ref;
	/**
	 * Signalled when the last reference is released and the object
	 * is detached.
	 */
	struct m0_chan             isd_detached_chan;
	struct m0_mutex            isd_detached_lock;
	/**
	 * Filename duplicated from m0_conf_sdev::sd_filename after expired conf
	 * event is fired. The name is needed for matching existing storage
	 * device against newly delivered conf when the one becomes ready.
	 *
	 * @note No guarantee the field is valid under any circumstances other
	 * than matching on conf ready event!
	 */
	char                      *isd_filename;
};

M0_TL_DESCR_DECLARE(storage_dev, M0_EXTERN);
M0_TL_DECLARE(storage_dev, M0_EXTERN, struct m0_storage_dev);

/**
 * Structure contains list of storage device
 * and their common additional data.
 */
struct m0_storage_devs {
	/** Type of storage devices. */
	enum m0_storage_dev_type sds_type;
	/** Mutex to protect sds_devices list. */
	struct m0_mutex          sds_lock;
	/** Linkage into list of storage devices. */
	struct m0_tl             sds_devices;
	/** Backing store stob domain. One per all storage devices. */
	struct m0_stob_domain   *sds_back_domain;
	/** Backend segment. One per all storage devices. */
	struct m0_be_seg        *sds_be_seg;
	/** Parallel pool processing list of storage devs. */
	struct m0_parallel_pool  sds_pool;
	/** Use directio for linuxstob domains. */
	bool                     sds_use_directio;

	/* Conf event callbacks provisioning. */

	/**
	 * Link to subscribe to conf expiration event m0_rconfc::rc_expired_cb.
	 */
	struct m0_clink          sds_conf_exp;
	/** Link to subscribe to conf ready event m0_rconfc::rc_ready_cb. */
	struct m0_clink          sds_conf_ready_async;
	/** Disable locks used during configuration update */
	bool                     sds_locks_disabled;
};

/**
 * Initialises storage devices structure.
 *
 * Backing store domain should be already created and initialised.
 */
M0_INTERNAL int m0_storage_devs_init(struct m0_storage_devs   *devs,
				     enum m0_storage_dev_type  type,
				     struct m0_be_seg         *be_seg,
				     struct m0_stob_domain    *bstore_dom,
				     struct m0_reqh           *reqh);
/**
 * Finalises storage devices structure.
 */
M0_INTERNAL void m0_storage_devs_fini(struct m0_storage_devs *devs);

/**
 * Locks storage devices structure.
 */
M0_INTERNAL void m0_storage_devs_lock(struct m0_storage_devs *devs);

/**
 * Unlocks storage devices structure.
 */
M0_INTERNAL void m0_storage_devs_unlock(struct m0_storage_devs *devs);

/**
 * Enables or disables direct I/O for linuxstobs.
 *
 * @pre storage_dev_tlist_is_empty(&devs->sds_devices)
 */
M0_INTERNAL void m0_storage_devs_use_directio(struct m0_storage_devs *devs,
					      bool                    directio);

/** Disable sdev locks. Use case: 1+0+0 configuration. */
M0_INTERNAL void m0_storage_devs_locks_disable(struct m0_storage_devs *devs);

/**
 * Finds storage device by its cid.
 */
M0_INTERNAL struct m0_storage_dev *
m0_storage_devs_find_by_cid(struct m0_storage_devs *devs,
			    uint64_t                cid);
/**
 * Finds storage device by its AD stob domain.
 */
M0_INTERNAL struct m0_storage_dev *
m0_storage_devs_find_by_dom(struct m0_storage_devs *devs,
			    struct m0_stob_domain  *dom);

/** Obtains reference to the storage device. */
M0_INTERNAL void m0_storage_dev_get(struct m0_storage_dev *dev);
/**
 * Releases reference to the storage device.
 *
 * m0_storage_devs structure which contains the storage device must be locked,
 * because this function can lead to execution of device detach.
 */
M0_INTERNAL void m0_storage_dev_put(struct m0_storage_dev *dev);

/**
 * Initialises device using information from configuration object.
 *
 * Extracts device parameters and executes m0_storage_dev_new.
 */
M0_INTERNAL int m0_storage_dev_new_by_conf(struct m0_storage_devs *devs,
					   struct m0_conf_sdev    *sdev,
					   bool                    force,
					   struct m0_storage_dev **dev);

/**
 * Allocates and initialises new storage device.
 *
 * Creates backing store stob in domain provided in m0_storage_devs_init.
 * Also creates AD stob domain for storage device.
 */
M0_INTERNAL int m0_storage_dev_new(struct m0_storage_devs *devs,
				   uint64_t                cid,
				   const char             *path,
				   uint64_t                size,
				   struct m0_conf_sdev    *sdev,
				   bool                    force,
				   struct m0_storage_dev **dev);

/**
 * Destroys storage device object.
 *
 * Finalises (but doesn't destroy) the underlying stob domain.
 */
M0_INTERNAL void m0_storage_dev_destroy(struct m0_storage_dev *dev);

/**
 * Attaches storage device to the devs list.
 *
 * @pre storage_devs_is_locked(devs)
 */
M0_INTERNAL void m0_storage_dev_attach(struct m0_storage_dev  *dev,
				       struct m0_storage_devs *devs);

/**
 * Detaches storage device.
 *
 * In fact, this function releases a reference to the storage device which is
 * detached when all references are released. On detach the storage device is
 * destroyed implicitly.
 * m0_storage_devs structure which contains the storage device must be locked.
 */
M0_INTERNAL void m0_storage_dev_detach(struct m0_storage_dev *dev);

/**
 * Synchronously detaches all storage devices.
 */
M0_INTERNAL void m0_storage_devs_detach_all(struct m0_storage_devs *devs);

/**
 * Used/free space of storage device.
 *
 * @see m0_storage_dev_space()
 */
struct m0_storage_space {
	m0_bcount_t sds_free_blocks;
	m0_bcount_t sds_block_size;
	m0_bcount_t sds_avail_blocks;
	m0_bcount_t sds_total_size;
};

/**
 * Obtains information about free space of the device.
 */
M0_INTERNAL void m0_storage_dev_space(struct m0_storage_dev   *dev,
				      struct m0_storage_space *space);

/**
 * Formats storage device.
 *
 * Not implemented yet.
 */
M0_INTERNAL int m0_storage_dev_format(struct m0_storage_dev *dev,
				      uint64_t               cid);

/**
 * Does fdatasync on all stobs in storage devices.
 */
M0_INTERNAL int m0_storage_devs_fdatasync(struct m0_storage_devs *devs);

/**
 * Creates a stob if it doesn't exist.
 *
 * This function is synchronised with possible storage device detach operation.
 */
M0_INTERNAL int m0_storage_dev_stob_create(struct m0_storage_devs *devs,
					   struct m0_stob_id      *sid,
					   struct m0_dtx          *dtx);

/**
 * Destroys stob and releases reference to the respective storage device.
 */
M0_INTERNAL int m0_storage_dev_stob_destroy(struct m0_storage_devs *devs,
					    struct m0_stob         *stob,
					    struct m0_dtx          *dtx);

/**
 * Finds and locates a stob which belongs to a storage device from the devs.
 * On success acquires reference to the stob and respective storage device.
 *
 * @note CSS_NOENT is a valid state of the resulting stob.
 */
M0_INTERNAL int m0_storage_dev_stob_find(struct m0_storage_devs  *devs,
					 struct m0_stob_id       *sid,
					 struct m0_stob         **stob);

/**
 * Releases reference to the stob and respective storage device.
 */
M0_INTERNAL void m0_storage_dev_stob_put(struct m0_storage_devs *devs,
					 struct m0_stob         *stob);

/** @} end of sdev group */
#endif /* __MERO_IOSERVICE_STORAGE_DEV_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
