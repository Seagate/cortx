/* -*- c -*- */
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
 * Original author: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>
 * Original creation date: 16-Mar-2012
 */
#pragma once
#ifndef __MERO_CONF_PRELOAD_H__
#define __MERO_CONF_PRELOAD_H__

struct m0_confx;

/**
 * @page conf-fspec-preload Pre-Loading of Configuration Cache
 *
 * - @ref conf-fspec-preload-string
 *   - @ref conf-fspec-preload-string-format
 *   - @ref conf-fspec-preload-string-examples
 * - @ref conf_dfspec_preload "Detailed Functional Specification"
 *
 * When configuration cache is created, it can be pre-loaded with
 * configuration data.  Cache pre-loading can be useful for testing,
 * boot-strapping, and manual control. One of use cases is a situation
 * when confc cannot or should not communicate with confd.
 *
 * <hr> <!------------------------------------------------------------>
 * @section conf-fspec-preload-string Configuration string
 *
 * The application pre-loads confc cache by passing textual
 * description of configuration objects -- so called configuration
 * string -- to m0_confc_init() via `local_conf' parameter.
 *
 * When confc API is used by a kernel module, configuration string is
 * provided via mount(8) option.
 *
 * <!---------------------------------------------------------------->
 * @subsection conf-fspec-preload-string-format Format
 *
 * The format of configuration string corresponds to the format of
 * string argument of m0_xcode_read() function.
 *
 * <!---------------------------------------------------------------->
 * @subsection conf-fspec-preload-string-examples Examples
 *
 * See examples of configuration strings in ut/conf.cg and
 * m0t1fs/linux_kernel/st/st.
 *
 * @see @ref conf_dfspec_preload "Detailed Functional Specification"
 */

/**
 * @defgroup conf_dfspec_preload Pre-Loading of Configuration Cache
 * @brief Detailed Functional Specification.
 *
 * @see @ref conf, @ref conf-fspec-preload "Functional Specification"
 *
 * @{
 */

/**
 * Encodes configuration string.
 *
 * @note If the call succeeds, the user is responsible for freeing
 *       allocated memory with m0_confx_free(*out).
 */
M0_INTERNAL int m0_confstr_parse(const char *str, struct m0_confx **out);

/** Frees the memory, dynamically allocated by m0_confstr_parse(). */
M0_INTERNAL void m0_confx_free(struct m0_confx *enc);

/**
 * @note If the call succeeds, the user is responsible for calling
 *       m0_confx_string_free(*out).
 */
M0_INTERNAL int m0_confx_to_string(struct m0_confx *confx, char **out);

/**
 * @pre m0_addr_is_aligned(str, PAGE_SHIFT)
 */
M0_INTERNAL void m0_confx_string_free(char *str);

/** @} conf_dfspec_preload */
#endif /* __MERO_CONF_PRELOAD_H__ */
