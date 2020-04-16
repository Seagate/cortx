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
 * Original author: Dmitriy Chumak <dmitriy_chumak@xyratex.com>
 * Original creation date: 18-Jul-2012
 */

#pragma once

#ifndef __MERO_XCODE_XCODE_ATTR_H__
#define __MERO_XCODE_XCODE_ATTR_H__

/**
 * @addtogroup xcode
 * @{
 */

/**
 * Set xcode attribute on a struct or strucut's field. This sets a special gcc
 * __attribute__ which is ignored by gcc during compilation, but which is then
 * used by gccxml and m0gccxml2xcode to generate xcode data.
 *
 * Please, refer to m0gccxml2xcode documentation for more details.
 */
#ifdef ENABLE_CASTXML
#define M0_XC_ATTR(name, val) __attribute__((annotate("xc_" name "," val)))
#else
#define M0_XC_ATTR(name, val) __attribute__((gccxml("xc_" name, val)))
#endif

/**
 * Shortened versions of M0_XC_ATTR to specifiy m0_xcode_aggr types.
 */
#define M0_XCA_RECORD   M0_XC_ATTR("atype", "M0_XA_RECORD")
#define M0_XCA_SEQUENCE M0_XC_ATTR("atype", "M0_XA_SEQUENCE")
#define M0_XCA_ARRAY    M0_XC_ATTR("atype", "M0_XA_ARRAY")
#define M0_XCA_UNION    M0_XC_ATTR("atype", "M0_XA_UNION")
#define M0_XCA_BLOB     M0_XC_ATTR("atype", "M0_XA_BLOB")
#define M0_XCA_ENUM     M0_XC_ATTR("enum",  "nonce")

#define M0_XCA_OPAQUE(value)   M0_XC_ATTR("opaque", value)
#define M0_XCA_TAG(value)      M0_XC_ATTR("tag", value)
#define M0_XCA_FENUM(value)    M0_XC_ATTR("fenum", #value)
#define M0_XCA_FBITMASK(value) M0_XC_ATTR("fbitmask", #value)

/**
 * Set "xcode domain" attribute on a struct. The domain is used in `m0protocol`
 * utility to separate xcode structs into groups.
 *
 * @param  value  a domain name, valid values are 'be', 'rpc', 'conf' or
 *		  any combination of those separated by a '|' (pipe symbol)
 *		  without spaces, e.g. 'be|conf|rpc'.
 *
 * @example  M0_XCA_DOMAIN(be)
 *           M0_XCA_DOMAIN(conf|rpc)
 */
#define M0_XCA_DOMAIN(value)   M0_XC_ATTR("domain", #value)

/** @} end of xcode group */

/* __MERO_XCODE_XCODE_ATTR_H__ */
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
