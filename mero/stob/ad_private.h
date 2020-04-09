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
 * Original author: Madhavrao Vemuri<madhav_vemuri@xyratex.com>
 * Original creation date: 27/02/2013
 */

#pragma once

#ifndef __MERO_STOB_AD_PRIVATE_H__
#define __MERO_STOB_AD_PRIVATE_H__

#include "fid/fid.h"       /* m0_fid */
#include "fid/fid_xc.h"    /* m0_fid_xc */
#include "be/extmap.h"     /* m0_be_emap_seg */
#include "be/extmap_xc.h"
#include "stob/stob.h"     /* m0_stob_id */
#include "stob/stob_xc.h"  /* m0_stob_id */

struct m0_stob_domain;
struct m0_stob_ad_domain;

struct stob_ad_0type_rec {
	struct m0_format_header   sa0_header;
	/* XXX pointer won't work with be_segment migration */
	struct m0_stob_ad_domain *sa0_ad_domain;
	struct m0_format_footer   sa0_footer;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

struct stob_ad_rec_frag_seg {
	uint32_t                ps_segments;
	struct m0_be_emap_seg  *ps_old_data;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(be);

struct stob_ad_rec_frag {
	struct m0_fid               arp_dom_id;
	struct m0_stob_id           arp_stob_id;
	struct stob_ad_rec_frag_seg arp_seg;
} M0_XCA_RECORD M0_XCA_DOMAIN(be);

M0_INTERNAL struct m0_stob_ad_domain *
stob_ad_domain2ad(const struct m0_stob_domain *dom);

/* __MERO_STOB_AD_PRIVATE_H__ */
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
