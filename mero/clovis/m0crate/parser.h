/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Ivan Alekhin <ivan.alekhin@seagate.com>
 * Original creation date: 30-May-2017
 */

#pragma once

#ifndef __MERO_CLOVIS_M0CRATE_PARSER_H__
#define __MERO_CLOVIS_M0CRATE_PARSER_H__

#include <yaml.h>

#include "clovis/m0crate/logger.h"
#include "clovis/m0crate/workload.h"
#include "clovis/m0crate/crate_utils.h"

/**
 * @defgroup crate_parser
 *
 * @{
 */


int parse_yaml_file(struct workload *load, int max_workload, int *index,
		    char *config_file);

/** @} end of crate_parser group */
#endif /* __MERO_CLOVIS_M0CRATE_PARSER_H__ */

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
