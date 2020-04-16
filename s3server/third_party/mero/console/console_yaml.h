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
 * Original author: Dipak Dudhabhate <dipak_dudhabhate@xyratex.com>
 * Original creation date: 22/03/2011
 */

#pragma once

#ifndef __MERO_CONSOLE_YAML_H__
#define __MERO_CONSOLE_YAML_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <yaml.h>

/**
   @addtogroup console_yaml
   @{
*/

/** enable/disable yaml support */
M0_EXTERN bool yaml_support;

/**
 * @brief Keeps info for YAML parser.
 */
struct m0_cons_yaml_info {
        /** YAML parser structure */
        yaml_parser_t    cyi_parser;
        /** YAML event structure */
        yaml_event_t     cyi_event;
        /** YAML document structure */
        yaml_document_t  cyi_document;
        /** Current Node in document */
	yaml_node_t	*cyi_current;
        /** YAML file pointer */
        FILE            *cyi_file;
};

/**
 * @brief Inititalizes parser by opening given file.
 *	  and also checks for error by getting root node.
 *
 * @param path YAML file path.
 *
 * @return 0 success and -errno failure.
 */
M0_INTERNAL int m0_cons_yaml_init(const char *path);

/**
 * @brief  Search for specified string and get the respctive value
 *	   form YAML file. (like "name : console")
 *
 * @param value Search string (like name).
 * @param data  Respective data (like console).
 *
 * @return 0 success and -errno failure.
 */
M0_INTERNAL int m0_cons_yaml_set_value(const char *value, void *data);

/**
 * @brief  Search for specified string and set the respctive value
 *	   form YAML file. (like "name : console")
 *
 * @param value Search string (like name).
 *
 * @return 0 success and -errno failure.
 */
M0_INTERNAL void *m0_cons_yaml_get_value(const char *value);

/**
 * @brief Deletes the parser and closes the YAML file.
 */
M0_INTERNAL void m0_cons_yaml_fini(void);

/** @} end of console_yaml group */
/* __MERO_CONSOLE_YAML_H__ */
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
