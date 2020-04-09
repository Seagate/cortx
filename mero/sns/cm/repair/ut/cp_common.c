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
 * Original author: Anup Barve <anup_barve@xyratex.com>
 * Original creation date: 10/22/2012
 */

#include "ioservice/io_service.h"        /* m0_ios_cdom_get */
#include "net/lnet/lnet.h"               /* m0_net_lnet_xprt */
#include "mero/setup.h"                  /* m0_mero */
#include "sns/cm/repair/ut/cp_common.h"  /* cs_fini */
#include "ut/misc.h"                     /* M0_UT_PATH */
#include "ut/ut.h"

#include "sns/cm/repair/service.c"
#include "sns/cm/rebalance/service.c"

/* Global structures for setting up mero service. */
const char log_file_name[] = "sr_ut.errlog";
char      *sns_cm_ut_svc_ad[] = { "m0d", "-T", "AD",
                                  "-D", "sr_db", "-S", "sr_stob",
                                  "-A", "linuxstob:sr_addb_stob",
			          "-f", M0_UT_CONF_PROCESS,
			          "-w", "10",
			          "-G", "lnet:0@lo:12345:34:1",
                                  "-e", "lnet:0@lo:12345:34:1",
                                  "-H", "0@lo:12345:34:1",
			          "-c", M0_UT_PATH("conf.xc")};

char      *sns_cm_ut_svc_linux[] = { "m0d", "-T", "LINUX",
                                     "-D", "sr_db", "-S", "sr_stob",
				     "-A", "linuxstob:sr_addb_stob",
				     "-f", M0_UT_CONF_PROCESS,
				     "-w", "10",
				     "-G", "lnet:0@lo:12345:34:1",
				     "-e", "lnet:0@lo:12345:34:1",
				     "-H", "0@lo:12345:34:1",
				     "-c", M0_UT_PATH("conf.xc")};

struct m0_net_xprt *sr_xprts[] = {
        &m0_net_lnet_xprt,
};

FILE           *lfile;
struct m0_mero  sctx;
enum {
	PDCLUST_N = 8,
	PDCLUST_K = 2,
	PDCLUST_P = 24,
	PDCLUST_UNIT_SIZE = 4096,
};
enum {
	LINUX_STOB = 1,
	AD_STOB = 2,
};
void bv_populate(struct m0_bufvec *b, char data, uint32_t seg_nr,
		 uint32_t seg_size)
{
        int i;

        for (i = 0; i < seg_nr; ++i) {
                M0_UT_ASSERT(b->ov_vec.v_count[i] == seg_size);
                M0_UT_ASSERT(b->ov_buf[i] != NULL);
                memset(b->ov_buf[i], data, seg_size);
        }
}

/* Populates the bufvec with a character value. */
void bv_alloc_populate(struct m0_bufvec *b, char data, uint32_t seg_nr,
		 uint32_t seg_size)
{
        M0_UT_ASSERT(b != NULL);
        M0_UT_ASSERT(m0_bufvec_alloc_aligned(b, seg_nr, seg_size,
					     M0_0VEC_SHIFT) == 0);
        M0_UT_ASSERT(b->ov_vec.v_nr == seg_nr);
	bv_populate(b, data, seg_nr, seg_size);
}

/* Compares 2 bufvecs and asserts if not equal. */
void bv_compare(struct m0_bufvec *b1, struct m0_bufvec *b2, uint32_t seg_nr,
		uint32_t seg_size)
{
        int i;

        M0_UT_ASSERT(b1 != NULL);
        M0_UT_ASSERT(b2 != NULL);
        M0_UT_ASSERT(b1->ov_vec.v_nr == seg_nr);
        M0_UT_ASSERT(b2->ov_vec.v_nr == seg_nr);

        for (i = 0; i < seg_nr; ++i) {
                M0_UT_ASSERT(b1->ov_vec.v_count[i] == seg_size);
                M0_UT_ASSERT(b1->ov_buf[i] != NULL);
                M0_UT_ASSERT(b2->ov_vec.v_count[i] == seg_size);
                M0_UT_ASSERT(b2->ov_buf[i] != NULL);
                M0_UT_ASSERT(memcmp(b1->ov_buf[i], b2->ov_buf[i],
                                    seg_size) == 0);
        }
}

void bv_free(struct m0_bufvec *b)
{
        m0_bufvec_free(b);
}

void cp_prepare(struct m0_cm_cp *cp, struct m0_net_buffer *buf,
		uint32_t bv_seg_nr, uint32_t bv_seg_size,
		struct m0_sns_cm_ag *sns_ag,
		char data, struct m0_fom_ops *cp_fom_ops,
		struct m0_reqh *reqh, uint64_t cp_ag_idx, bool is_acc_cp,
		struct m0_cm *cm)
{
	struct m0_reqh_service *service;
	struct m0_sns_cm       *scm;

	M0_UT_ASSERT(cp != NULL);
	M0_UT_ASSERT(buf != NULL);
	M0_UT_ASSERT(sns_ag != NULL);

	if (buf->nb_buffer.ov_buf == NULL)
		bv_alloc_populate(&buf->nb_buffer, data, bv_seg_nr, bv_seg_size);
	else
		bv_populate(&buf->nb_buffer, data, bv_seg_nr, bv_seg_size);
	cp->c_ag = &sns_ag->sag_base;
	if (cm == NULL) {
		service = m0_reqh_service_find(&sns_repair_cmt.ct_stype, reqh);
		M0_UT_ASSERT(service != NULL);
		cm = container_of(service, struct m0_cm, cm_service);
		M0_UT_ASSERT(cm != NULL);
		scm = cm2sns(cm);
		m0_ios_cdom_get(reqh, &scm->sc_cob_dom);
	}
	cp->c_ag->cag_cm = cm;
	if (!is_acc_cp)
		cp->c_ops = &m0_sns_cm_repair_cp_ops;
	cp->c_ops = &m0_sns_cm_acc_cp_ops;
	m0_cm_cp_fom_init(cm, cp, NULL, NULL);
	m0_cm_cp_buf_add(cp, buf);
	cp->c_data_seg_nr = bv_seg_nr;
	buf->nb_pool->nbp_seg_nr = bv_seg_nr;
	buf->nb_pool->nbp_seg_size = bv_seg_size;
	cp->c_fom.fo_ops = cp_fom_ops;
	cp->c_ag_cp_idx = cp_ag_idx;
}

struct m0_sns_cm *reqh2snscm(struct m0_reqh *reqh)
{
	struct m0_reqh_service *service;
	struct m0_cm           *cm;

	service = m0_reqh_service_find(&sns_repair_cmt.ct_stype,
				       reqh);
	M0_UT_ASSERT(service != NULL);
	cm = container_of(service, struct m0_cm, cm_service);
	M0_UT_ASSERT(cm != NULL);
	return cm2sns(cm);
}

static int cs_init_setup_env(struct m0_mero *sctx, int stob_type)
{
	int rc;

	M0_SET0(sctx);

	lfile = fopen(log_file_name, "w+");
	M0_ASSERT(lfile != NULL);

	rc = m0_cs_init(sctx, sr_xprts, ARRAY_SIZE(sr_xprts), lfile, true);
	if (rc != 0)
		return rc;

	if (stob_type == LINUX_STOB) {
		rc = m0_cs_setup_env(sctx, ARRAY_SIZE(sns_cm_ut_svc_linux),
				     sns_cm_ut_svc_linux);
	} else {
		rc = m0_cs_setup_env(sctx, ARRAY_SIZE(sns_cm_ut_svc_ad),
				     sns_cm_ut_svc_ad);
	}
	if (rc == 0)
		rc = m0_cs_start(sctx);
	if (rc != 0)
		cs_fini(sctx);

	return rc;
}

/*
 * Starts mero service, which internally creates and sets up linux stob domain.
 * This stob domain is used in read and write phases of the copy packet.
 */
int cs_init(struct m0_mero *sctx)
{
	return cs_init_setup_env(sctx, LINUX_STOB);
}

/*
 * Starts mero service, which internally creates and sets up ad stob domain.
 * This stob domain is used in read and write phases of the copy packet.
 */
int cs_init_with_ad_stob(struct m0_mero *sctx)
{
	return cs_init_setup_env(sctx, AD_STOB);
}

/* Finalises the mero service. */
void cs_fini(struct m0_mero *sctx)
{
	m0_cs_fini(sctx);
	fclose(lfile);
}

void layout_gen(struct m0_pdclust_layout **pdlay, struct m0_reqh *reqh)
{
	struct m0_layout_linear_enum *llenum;
	struct m0_layout_linear_attr  llattr;
	struct m0_pdclust_attr        pdattr;
	int                           rc;

	llattr = (struct m0_layout_linear_attr) {
	        .lla_nr = PDCLUST_P,
	        .lla_A  = 1,
	        .lla_B  = 1,
	};
	llenum = NULL;
	rc = m0_linear_enum_build(&reqh->rh_ldom, &llattr, &llenum);
	M0_ASSERT(rc == 0);

	pdattr = (struct m0_pdclust_attr) {
	        .pa_N         = PDCLUST_N,
	        .pa_K         = PDCLUST_K,
	        .pa_P         = PDCLUST_P,
	        .pa_unit_size = PDCLUST_UNIT_SIZE,
	};
	m0_uint128_init(&pdattr.pa_seed, "upjumpandpumpim,");
	rc = m0_pdclust_build(&reqh->rh_ldom, M0_DEFAULT_LAYOUT_ID,
			      &pdattr, &llenum->lle_base, pdlay);
	M0_ASSERT(rc == 0);
	M0_ASSERT(*pdlay != NULL);
}

void layout_destroy(struct m0_pdclust_layout *pdlay)
{
	m0_layout_put(&pdlay->pl_base.sl_base);
}

void pool_mach_transit(struct m0_reqh *reqh, struct m0_poolmach *pm,
			uint64_t fd, enum m0_pool_nd_state state)
{
	struct m0_poolmach_event pme;
	int                      rc;

	M0_SET0(&pme);
	pme.pe_type  = M0_POOL_DEVICE;
	pme.pe_index = fd;
	pme.pe_state = state;

	rc = m0_poolmach_state_transit(pm, &pme);
	M0_UT_ASSERT(rc == 0);
}


/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
