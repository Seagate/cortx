/* -*- C -*- */
/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Leonid Nikulin <leonid.nikulin@seagate.com>
 * Original creation date: 30-Jun-2016
 */


/**
 * @addtogroup dix
 *
 * @{
 *
 *
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_DIX
#include "lib/trace.h"

#include "lib/memory.h"    /* m0_alloc */
#include "lib/errno.h"     /* ENOMEM */
#include "lib/vec.h"
#include "xcode/xcode.h"
#include "dix/layout.h"
#include "dix/encdec.h"
#include "dix/encdec_xc.h"

#define DIX_META_VAL_XCODE_OBJ(ptr) M0_XCODE_OBJ(dix_meta_val_xc, ptr)
#define DIX_LAYOUT_VAL_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_dix_layout_xc, ptr)
#define DIX_LDESC_VAL_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_dix_ldesc_xc, ptr)
#define DIX_FID_VAL_XCODE_OBJ(ptr) M0_XCODE_OBJ(m0_fid_xc, ptr)

M0_INTERNAL int m0_dix__meta_val_enc(const struct m0_fid       *fid,
				     const struct m0_dix_ldesc *dld,
				     uint32_t                   nr,
				     struct m0_bufvec          *vals)
{
	int      rc;
	uint32_t i;

	M0_PRE(fid != NULL);
	M0_PRE(dld != NULL);
	M0_PRE(vals != NULL);
	rc = m0_bufvec_empty_alloc(vals, nr);
	if (rc != 0)
		return M0_ERR(rc);
	for (i = 0; rc == 0 && i < nr; i++) {
		struct dix_meta_val x = { .mv_fid = fid[i], .mv_dld = dld[i] };

		rc = m0_xcode_obj_enc_to_buf(&DIX_META_VAL_XCODE_OBJ(&x),
					     &vals->ov_buf[i],
					     &vals->ov_vec.v_count[i]);
	}
	if (rc != 0)
		m0_bufvec_free(vals);
	return rc;
}

M0_INTERNAL int m0_dix__meta_val_dec(const struct m0_bufvec *vals,
				     struct m0_fid          *out_fid,
				     struct m0_dix_ldesc    *out_dld,
				     uint32_t                nr)
{
	int      rc = 0;
	uint32_t i;

	M0_ENTRY();
	M0_PRE(vals != NULL);
	M0_PRE(out_fid != NULL);
	M0_PRE(out_dld != NULL);
	for (i = 0; i < nr; i++) {
		struct dix_meta_val x;

		M0_SET0(&x);
		rc = m0_xcode_obj_dec_from_buf(&DIX_META_VAL_XCODE_OBJ(&x),
					       vals->ov_buf[i],
					       vals->ov_vec.v_count[i]);
		if (rc == 0) {
			out_fid[i] = x.mv_fid;
			rc = m0_dix_ldesc_copy(&out_dld[i], &x.mv_dld);
			m0_dix_ldesc_fini(&x.mv_dld);
		}
		if (rc != 0)
			break;
	}
	if (rc != 0)
		/*
		 * We've got an error at step i. Finalise already created
		 * descriptors.
		 */
		while(i != 0)
			m0_dix_ldesc_fini(&out_dld[--i]);
	return M0_RC(rc);
}

M0_INTERNAL int m0_dix__layout_vals_enc(const struct m0_fid        *fid,
					const struct m0_dix_layout *dlay,
					uint32_t                    nr,
					struct m0_bufvec           *keys,
					struct m0_bufvec           *vals)
{
	int      rc = 0;
	uint32_t i;
	bool     enc_keys = keys != NULL;
	bool     enc_vals = vals != NULL;

	M0_PRE((fid != NULL) == (keys != NULL));
	M0_PRE((dlay != NULL) == (vals != NULL));
	M0_PRE(enc_keys || enc_vals);
	if (enc_keys) {
		rc = m0_bufvec_empty_alloc(keys, nr);
		if (rc != 0)
			return M0_ERR(rc);
	}
	if (enc_vals) {
		rc = m0_bufvec_empty_alloc(vals, nr);
		if (rc != 0)
			return M0_ERR(-ENOMEM);
	}
	for (i = 0; rc == 0 && i < nr; i++) {
		if (enc_keys)
			rc = m0_xcode_obj_enc_to_buf(
					&DIX_FID_VAL_XCODE_OBJ(
						 (struct m0_fid *)&fid[i]),
					&keys->ov_buf[i],
					&keys->ov_vec.v_count[i]);
		if (enc_vals)
			rc = m0_xcode_obj_enc_to_buf(
					&DIX_LAYOUT_VAL_XCODE_OBJ(
						(struct m0_dix_layout *)dlay),
					&vals->ov_buf[i],
					&vals->ov_vec.v_count[i]);
	}
	if (rc != 0) {
		m0_bufvec_free(keys);
		m0_bufvec_free(vals);
	}
	return rc;
}

/* Decoding key and val bufvecs and returns fid and dix_layout. */
M0_INTERNAL int m0_dix__layout_vals_dec(const struct m0_bufvec *keys,
				        const struct m0_bufvec *vals,
				        struct m0_fid          *out_fid,
				        struct m0_dix_layout   *out_dlay,
				        uint32_t                nr)
{
	int      rc = 0;
	uint32_t i;
	bool     dec_keys = out_fid != NULL;
	bool     dec_vals = out_dlay != NULL;

	M0_PRE((out_fid != NULL) == (keys != NULL));
	M0_PRE((out_dlay != NULL) == (vals != NULL));
	M0_PRE(dec_keys || dec_vals);
	for (i = 0; rc == 0 && i < nr; i++) {
		if (dec_keys)
			rc = m0_xcode_obj_dec_from_buf(
				&DIX_FID_VAL_XCODE_OBJ(&out_fid[i]),
				keys->ov_buf[i], keys->ov_vec.v_count[i]);
		if (dec_vals) {
			M0_SET0(out_dlay);
			rc = m0_xcode_obj_dec_from_buf(
				&DIX_LAYOUT_VAL_XCODE_OBJ(out_dlay),
				vals->ov_buf[i], vals->ov_vec.v_count[i]);
			if (rc != 0)
				break;
		}
	}
	/*
	 * We've got an error at i'th step. Already created layout descriptors
	 * should be finalised.
	 */
	if (rc != 0)
		while(i != 0)
			m0_dix_ldesc_fini(&out_dlay[--i].u.dl_desc);
	return rc;
}

M0_INTERNAL int m0_dix__ldesc_vals_enc(const uint64_t            *lid,
				       const struct m0_dix_ldesc *ldesc,
				       uint32_t                   nr,
				       struct m0_bufvec          *keys,
				       struct m0_bufvec          *vals)
{
	int      rc = 0;
	uint32_t i;
	bool     enc_keys = keys != NULL;
	bool     enc_vals = vals != NULL;

	M0_PRE((lid != NULL) == (keys != NULL));
	M0_PRE((ldesc != NULL) == (vals != NULL));
	M0_PRE(enc_keys || enc_vals);
	if (enc_keys) {
		rc = m0_bufvec_alloc(keys, nr, sizeof *lid);
		if (rc != 0)
			return M0_ERR(rc);
	}
	if (enc_vals)
		rc = m0_bufvec_empty_alloc(vals, nr);
	for (i = 0; rc == 0 && i < nr; i++) {
		if (enc_keys)
			*(uint64_t *)keys->ov_buf[i] = lid[i];
		if (enc_vals) {
			struct m0_dix_ldesc *x = (struct m0_dix_ldesc *)ldesc;
			rc = m0_xcode_obj_enc_to_buf(
					&DIX_LDESC_VAL_XCODE_OBJ(x),
					&vals->ov_buf[i],
					&vals->ov_vec.v_count[i]);
		}
	}
	if (rc != 0) {
		m0_bufvec_free(keys);
		m0_bufvec_free(vals);
	}
	return rc;
}

M0_INTERNAL int m0_dix__ldesc_vals_dec(const struct m0_bufvec *keys,
				       const struct m0_bufvec *vals,
				       uint64_t               *out_lid,
				       struct m0_dix_ldesc    *out_ldesc,
				       uint32_t                nr)
{
	int      rc = 0;
	uint32_t i;
	bool     dec_keys = out_lid != NULL;
	bool     dec_vals = out_ldesc != NULL;

	M0_PRE((out_lid != NULL) == (keys != NULL));
	M0_PRE((out_ldesc != NULL) == (vals != NULL));
	M0_PRE(dec_keys || dec_vals);
	for (i = 0; rc == 0 && i < nr; i++) {
		if (dec_keys)
			out_lid[i] = *(uint64_t *)keys->ov_buf[i];
		if (dec_vals) {
			struct m0_dix_ldesc x;

			M0_SET0(&x);
			rc = m0_xcode_obj_dec_from_buf(
				&DIX_LDESC_VAL_XCODE_OBJ(&x),
				vals->ov_buf[i], vals->ov_vec.v_count[i]);
			if (rc == 0) {
				rc = m0_dix_ldesc_copy(&out_ldesc[i], &x);
				m0_dix_ldesc_fini(&x);
			}
		}
	}
	/*
	 * We've got an error for in step i. In case i != 0 we must fini
	 * already created ldesc.
	 */
	if (rc != 0)
		while(i != 0)
			m0_dix_ldesc_fini(&out_ldesc[--i]);
	return rc;
}
#undef M0_TRACE_SUBSYSTEM

/** @} end of dix group */

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
