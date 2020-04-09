/* -*- c -*- */
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
 * Original authors: Valery V. Vorotyntsev <valery_vorotyntsev@xyratex.com>,
 *                   Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>.
 * Authors:          Andriy Tkachuk <andriy.tkachuk@seagate.com>
 *
 * Original creation date: 20-Aug-2012
 */
#pragma once
#ifndef __MERO_CONF_ONWIRE_H__
#define __MERO_CONF_ONWIRE_H__

#include "xcode/xcode.h"
#include "lib/types.h"       /* m0_conf_verno_t */
#include "lib/bitmap.h"      /* m0_bitmap_onwire */
#include "lib/bitmap_xc.h"   /* m0_bitmap_onwire */
#include "lib/buf_xc.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "conf/schema_xc.h"  /* m0_xc_m0_conf_service_type_enum */
#include "fdmi/filter.h"     /* m0_fdmi_flt_node */
#include "fdmi/filter_xc.h"  /* m0_fdmi_flt_node_xc */
#include "pool/policy_xc.h"     /* m0_pver_policy_code */

/* export */
struct m0_conf_fetch;
struct m0_conf_fetch_resp;
struct m0_conf_update;
struct m0_conf_update_resp;

struct arr_u32 {
	uint32_t  au_count;
	uint32_t *au_elems;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(conf|rpc);

/* ------------------------------------------------------------------
 * Configuration objects
 * ------------------------------------------------------------------ */

/* Note that m0_confx_dir does not exist. */

/** Common header of all confx objects. */
struct m0_confx_header {
	struct m0_fid ch_id;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_root {
	struct m0_confx_header xt_header;
	uint64_t               xt_verno;
	struct m0_fid          xt_rootfid;
	struct m0_fid          xt_mdpool;
	struct m0_fid          xt_imeta_pver;
	uint32_t               xt_mdredundancy;
	struct m0_bufs         xt_params;
	struct m0_fid_arr      xt_nodes;
	struct m0_fid_arr      xt_sites;
	struct m0_fid_arr      xt_pools;
	struct m0_fid_arr      xt_profiles;
	struct m0_fid_arr      xt_fdmi_flt_grps;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_profile {
	struct m0_confx_header xp_header;
	struct m0_fid_arr      xp_pools;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_pool {
	struct m0_confx_header xp_header;
	uint32_t               xp_pver_policy M0_XCA_FENUM(m0_pver_policy_code);
	struct m0_fid_arr      xp_pvers;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_pver_actual {
	/* Number of data units in a parity group. */
	uint32_t          xva_N;
	/* Number of parity units in a parity group. */
	uint32_t          xva_K;
	/* Pool width. */
	uint32_t          xva_P;
	/*
	 * NOTE: The number of elements must be equal to M0_CONF_PVER_HEIGHT.
	 */
	struct arr_u32    xva_tolerance;
	struct m0_fid_arr xva_sitevs;
	/*
	 * NOTE: "recd" attribute is not transferred over the wire,
	 * it exists in local conf cache only.
	 */
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_pver_formulaic {
	uint32_t       xvf_id;
	struct m0_fid  xvf_base;
	/*
	 * NOTE: The number of elements must be equal to M0_CONF_PVER_HEIGHT.
	 */
	struct arr_u32 xvf_allowance;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

enum { M0_CONFX_PVER_ACTUAL, M0_CONFX_PVER_FORMULAIC };

struct m0_confx_pver_u {
	uint8_t xpv_is_formulaic;
	union {
		struct m0_confx_pver_actual    xpv_actual
			M0_XCA_TAG("M0_CONFX_PVER_ACTUAL");
		struct m0_confx_pver_formulaic xpv_formulaic
			M0_XCA_TAG("M0_CONFX_PVER_FORMULAIC");
	} u;
} M0_XCA_UNION M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_pver {
	struct m0_confx_header xv_header;
	struct m0_confx_pver_u xv_u;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_objv {
	struct m0_confx_header xj_header;
	struct m0_fid          xj_real;
	struct m0_fid_arr      xj_children;
	/*
	 * NOTE: "ix" attribute is not transferred over the wire,
	 * it exists in local conf cache only.
	 */
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_node {
	struct m0_confx_header xn_header;
	uint32_t               xn_memsize;
	uint32_t               xn_nr_cpu;
	uint64_t               xn_last_state;
	uint64_t               xn_flags;
	struct m0_fid_arr      xn_processes;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_process {
	struct m0_confx_header  xr_header;
	struct m0_bitmap_onwire xr_cores;
	uint64_t                xr_mem_limit_as;
	uint64_t                xr_mem_limit_rss;
	uint64_t                xr_mem_limit_stack;
	uint64_t                xr_mem_limit_memlock;
	struct m0_buf           xr_endpoint;
	struct m0_fid_arr       xr_services;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_service {
	struct m0_confx_header xs_header;
	uint32_t               xs_type M0_XCA_FENUM(m0_conf_service_type);
	struct m0_bufs         xs_endpoints;
	struct m0_bufs         xs_params;
	struct m0_fid_arr      xs_sdevs;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_sdev {
	struct m0_confx_header xd_header;
	uint32_t               xd_dev_idx;
	uint32_t               xd_iface M0_XCA_FENUM(
		m0_cfg_storage_device_interface_type);
	uint32_t               xd_media M0_XCA_FENUM(
		m0_cfg_storage_device_media_type);
	uint32_t               xd_bsize;
	uint64_t               xd_size;
	uint64_t               xd_last_state;
	uint64_t               xd_flags;
	struct m0_buf          xd_filename;
	/*
	 * NOTE: "drive" attribute is not transferred over the wire,
	 * it exists in local conf cache only.
	 */
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_site {
	struct m0_confx_header xi_header;
	struct m0_fid_arr      xi_racks;
	struct m0_fid_arr      xi_pvers;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_rack {
	struct m0_confx_header xr_header;
	struct m0_fid_arr      xr_encls;
	struct m0_fid_arr      xr_pvers;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_enclosure {
	struct m0_confx_header xe_header;
	struct m0_fid_arr      xe_ctrls;
	struct m0_fid_arr      xe_pvers;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_controller {
	struct m0_confx_header xc_header;
	struct m0_fid          xc_node;
	struct m0_fid_arr      xc_drives;
	struct m0_fid_arr      xc_pvers;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_drive {
	struct m0_confx_header xk_header;
	struct m0_fid          xk_sdev;
	struct m0_fid_arr      xk_pvers;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_fdmi_flt_grp {
	struct m0_confx_header xfg_header;
	uint32_t               xfg_rec_type;
	struct m0_fid_arr      xfg_filters;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_fdmi_filter {
	struct m0_confx_header xf_header;
	struct m0_fid          xf_filter_id;
	/* String representation of FDMI filter root. */
	struct m0_buf          xf_filter_root;
	struct m0_fid          xf_node;
	struct m0_bufs         xf_endpoints;
} M0_XCA_RECORD M0_XCA_DOMAIN(conf|rpc);

struct m0_confx_obj {
	uint64_t xo_type; /* see m0_fid_type::ft_id for values */
	union {
		/**
		 * Allows to access the header of concrete m0_confx_* objects.
		 */
		struct m0_confx_header u_header;
	} xo_u;
};

/**
 * xcode type of the union above.
 *
 * This type is build dynamically, when new conf object types are
 * registered. See m0_conf_obj_type_register().
 */
M0_EXTERN struct m0_xcode_type *m0_confx_obj_xc;
M0_INTERNAL void m0_xc_m0_confx_obj_struct_init(void);
M0_INTERNAL void m0_xc_m0_confx_obj_struct_fini(void);

/** Encoded configuration --- a sequence of m0_confx_objs. */
struct m0_confx {
	uint32_t             cx_nr;
	/**
	 * Objects in the configuration.
	 *
	 * @note Do not access this field directly, because actual in-memory
	 * size of object is larger than sizeof(struct m0_confx_obj). Use
	 * M0_CONFX_AT() instead.
	 */
	struct m0_confx_obj *cx__objs;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(conf|rpc);

/** Returns specific element of m0_confx::cx__objs. */
#define M0_CONFX_AT(cx, idx)                                    \
({                                                              \
	typeof(cx)   __cx  = (cx);                              \
	uint32_t     __idx = (idx);                             \
	M0_ASSERT(__idx <= __cx->cx_nr);                        \
	(typeof(&(cx)->cx__objs[0]))(((char *)__cx->cx__objs) + \
				    __idx * m0_confx_sizeof()); \
})

M0_INTERNAL size_t m0_confx_sizeof(void);

/* ------------------------------------------------------------------
 * Configuration fops
 * ------------------------------------------------------------------ */

/** Configuration fetch request. */
struct m0_conf_fetch {
	/** Configuration object the path originates from. */
	struct m0_fid     f_origin;
	/** Path components. */
	struct m0_fid_arr f_path;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** Confd's response to m0_conf_fetch. */
struct m0_conf_fetch_resp {
	/** Result of configuration retrieval (-Exxx = failure, 0 = success). */
	int32_t         fr_rc;
	/** configuration version number */
	uint64_t        fr_ver;
	/** A sequence of configuration object descriptors. */
	struct m0_confx fr_data;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** XXX FUTURE: Configuration update request. */
struct m0_conf_update {
	/** Configuration object the path originates from. */
	struct m0_fid   u_origin;
	/** A sequence of configuration object descriptors. */
	struct m0_confx u_data;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** XXX FUTURE: Confd's response to m0_conf_update. */
struct m0_conf_update_resp {
	/** Result of update request (-Exxx = failure, 0 = success). */
	int32_t ur_rc;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#endif /* __MERO_CONF_ONWIRE_H__ */
