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
 */
/*
 * Copyright 2009 ClusterStor.
 *
 * Nikita Danilov.
 */

#pragma once

#ifndef __MERO_DESIM_STORAGE_H__
#define __MERO_DESIM_STORAGE_H__

#include "desim/sim.h"

/**
   @addtogroup desim desim
   @{
 */

typedef unsigned long long sector_t;

struct storage_dev;

struct storage_conf {
	unsigned   sc_sector_size;
};

enum storage_req_type {
	SRT_READ,
	SRT_WRITE
};

typedef void (*storage_end_io_t)(struct storage_dev *dev);
typedef void (*storage_submit_t)(struct storage_dev *dev,
				 enum storage_req_type type,
				 sector_t sector, unsigned long count);

struct elevator;
struct storage_dev {
	struct sim          *sd_sim;
	struct storage_conf *sd_conf;
	storage_end_io_t     sd_end_io;
	storage_submit_t     sd_submit;
	struct elevator     *sd_el;
	char                *sd_name;
};

#endif /* __MERO_DESIM_STORAGE_H__ */

/** @} end of desim group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
