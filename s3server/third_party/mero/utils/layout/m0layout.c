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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 07/15/2010
 */

#include <stdio.h>  /* printf */
#include <stdlib.h> /* atoi */
#include <math.h>   /* sqrt */

#include "lib/misc.h"   /* M0_SET0 */
#include "lib/memory.h"
#include "lib/assert.h"
#include "lib/arith.h"
#include "lib/errno.h"  /* ENOMEM */
#include "mero/init.h"
#include "module/instance.h"

#include "pool/pool.h"
#include "ioservice/fid_convert.h" /* m0_fid_gob_make() */
#include "layout/layout.h"
#include "layout/pdclust.h"
#include "layout/linear_enum.h" /* m0_linear_enum_build() */

/**
   @addtogroup layout
   @{
*/

/*
 * Creates dummy domain, registers pdclust layout type and linear
 * enum type and creates dummy enum object.
 * These objects are called as dummy since they are not used by this layout
 * demo test.
 */
static int dummy_create(struct m0_layout_domain *domain,
			uint64_t lid,
			struct m0_pdclust_attr *attr,
			struct m0_pdclust_layout **pl)
{
	int                           rc;
	struct m0_layout_linear_attr  lin_attr;
	struct m0_layout_linear_enum *lin_enum;

	rc = m0_layout_domain_init(domain);
	M0_ASSERT(rc == 0);

	rc = m0_layout_standard_types_register(domain);
	M0_ASSERT(rc == 0);

	lin_attr.lla_nr = attr->pa_P;
	lin_attr.lla_A  = 100;
	lin_attr.lla_B  = 200;
	rc = m0_linear_enum_build(domain, &lin_attr, &lin_enum);
	M0_ASSERT(rc == 0);

	rc = m0_pdclust_build(domain, lid, attr, &lin_enum->lle_base, pl);
	M0_ASSERT(rc == 0);
	return rc;
}

enum m0_pdclust_unit_type classify(const struct m0_pdclust_layout *play,
				   int unit)
{
	if (unit < play->pl_attr.pa_N)
		return M0_PUT_DATA;
	else if (unit < play->pl_attr.pa_N + play->pl_attr.pa_K)
		return M0_PUT_PARITY;
	else
		return M0_PUT_SPARE;
}

/**
 * @todo Allocate the arrays globally so that it does not result into
 * going beyond the stack limit in the kernel mode.
 */
void layout_demo(struct m0_pdclust_instance *pi,
		 struct m0_pdclust_layout *pl,
		 int R, int I, bool print)
{
	uint64_t                   group;
	uint32_t                   unit;
	uint32_t                   N;
	uint32_t                   K;
	uint32_t                   P;
	uint32_t                   W;
	int                        i;
	struct m0_pdclust_src_addr src;
	struct m0_pdclust_tgt_addr tgt;
	struct m0_pdclust_src_addr src1;
	struct m0_pdclust_attr     attr = pl->pl_attr;
	struct m0_pdclust_src_addr map[R][attr.pa_P];
	uint32_t                   incidence[attr.pa_P][attr.pa_P];
	uint32_t                   usage[attr.pa_P][M0_PUT_NR + 1];
	uint32_t                   where[attr.pa_N + 2*attr.pa_K];

#ifndef __KERNEL__
	uint64_t                   frame;
	uint32_t                   obj;
	const char                *brace[M0_PUT_NR] = { "[]", "<>", "{}" };
	const char                *head[M0_PUT_NR+1] = { "D", "P", "S",
							 "total" };
	uint32_t                   min;
	uint32_t                   max;
	uint64_t                   sum;
	uint32_t                   u;
	double                     sq;
	double                     avg;
#endif

	M0_SET_ARR0(usage);
	M0_SET_ARR0(incidence);

	N = attr.pa_N;
	K = attr.pa_K;
	P = attr.pa_P;
	W = N + 2*K;

#ifndef __KERNEL__
	if (print) {
		printf("layout: N: %u K: %u P: %u C: %u L: %u\n",
				N, K, P, pl->pl_C, pl->pl_L);
	}
#endif

	for (group = 0; group < I ; ++group) {
		src.sa_group = group;
		for (unit = 0; unit < W; ++unit) {
			src.sa_unit = unit;
			m0_pdclust_instance_map(pi, &src, &tgt);
			m0_pdclust_instance_inv(pi, &tgt, &src1);
			M0_ASSERT(memcmp(&src, &src1, sizeof src) == 0);
			if (tgt.ta_frame < R)
				map[tgt.ta_frame][tgt.ta_obj] = src;
			where[unit] = tgt.ta_obj;
			usage[tgt.ta_obj][M0_PUT_NR]++;
			usage[tgt.ta_obj][classify(pl, unit)]++;
		}
		for (unit = 0; unit < W; ++unit) {
			for (i = 0; i < W; ++i)
				incidence[where[unit]][where[i]]++;
		}
	}
	if (!print)
		return;

#ifndef __KERNEL__
	printf("map: \n");
	for (frame = 0; frame < R; ++frame) {
		printf("%5i : ", (int)frame);
		for (obj = 0; obj < P; ++obj) {
			int d;

			d = classify(pl, map[frame][obj].sa_unit);
			printf("%c%2i, %2i%c ",
			       brace[d][0],
			       (int)map[frame][obj].sa_group,
			       (int)map[frame][obj].sa_unit,
			       brace[d][1]);
		}
		printf("\n");
	}
	printf("usage : \n");
	for (i = 0; i < M0_PUT_NR + 1; ++i) {
		max = sum = sq = 0;
		min = ~0;
		printf("%5s : ", head[i]);
		for (obj = 0; obj < P; ++obj) {
			u = usage[obj][i];
			printf("%7i ", u);
			min = min32u(min, u);
			max = max32u(max, u);
			sum += u;
			sq += u*u;
		}
		avg = ((double)sum)/P;
		printf(" | %7i %7i %7i %7.2f%%\n", min, max, (int)avg,
		       sqrt(sq/P - avg*avg)*100.0/avg);
	}
	printf("\nincidence:\n");
	for (obj = 0; obj < P; ++obj) {
		max = sum = sq = 0;
		min = ~0;
		for (i = 0; i < P; ++i) {
			if (obj != i) {
				u = incidence[obj][i];
				min = min32u(min, u);
				max = max32u(max, u);
				sum += u;
				sq += u*u;
				printf("%5i ", u);
			} else
				printf("    * ");
		}
		avg = ((double)sum)/(P - 1);
		printf(" | %5i %5i %5i %5.2f%%\n", min, max, (int)avg,
		       sqrt(sq/(P - 1) - avg*avg)*100.0/avg);
	}
#endif
}

int main(int argc, char **argv)
{
	uint32_t                    N;
	uint32_t                    K;
	uint32_t                    P;
	int                         R;
	int                         I;
	int                         rc;
	uint32_t                    cache_nr;
	uint64_t                   *cache_len;
	uint64_t                    unitsize = 4096;
	struct m0_pool_version      pool_ver;
	struct m0_layout           *l;
	struct m0_pdclust_layout   *play;
	struct m0_pdclust_attr      attr;
	struct m0_pool              pool;
	uint64_t                    id;
	struct m0_uint128           seed;
	struct m0_layout_domain     domain;
	struct m0_pdclust_instance *pi;
	struct m0_fid               gfid;
	struct m0_layout_instance  *li;
	static struct m0            instance;
	if (argc != 6 && argc != 8) {
		printf(
"\t\tm0layout N K P R I\nwhere\n"
"\tN  : number of data units in a parity group\n"
"\tK  : number of parity units in a parity group\n"
"\tP  : number of target objects to stripe over\n"
"\tR  : number of frames to show in a layout map\n"
"\tI  : number of groups to iterate over while\n"
"\t     calculating incidence and frame distributions\n"
"\tf_c: container-id for gfid\n"
"\tf_k: key for gfid\n"
"\noutput:\n"
"\tmap:       an R*P map showing initial fragment of layout\n"
"\t                   [G, U] - data unit U from a group G\n"
"\t                   <G, U> - parity unit U from a group G\n"
"\t                   {G, U} - spare unit U from a group G\n"
"\tusage:     counts of data, parity, spare and total frames\n"
"\t           occupied on each target object, followed by MIN,\n"
"\t           MAX, AVG, STD/AVG\n"
"\tincidence: a matrix showing a number of parity groups having\n"
"\t           units on a given pair of target objects, followed by\n"
"\t           MIN, MAX, AVG, STD/AVG\n");
		return 1;
	}
	N = atoi(argv[1]);
	K = atoi(argv[2]);
	P = atoi(argv[3]);
	R = atoi(argv[4]);
	I = atoi(argv[5]);

	id = 0x4A494E4E49455349; /* "jinniesi" */
	m0_uint128_init(&seed, M0_PDCLUST_SEED);

	rc = m0_init(&instance);
	if (rc != 0)
		return rc;

	rc = m0_pool_init(&pool, &M0_FID_INIT(0, id), 0);
	if (rc == 0) {
		attr.pa_N = N;
		attr.pa_K = K;
		attr.pa_P = P;
		attr.pa_unit_size = unitsize;
		attr.pa_seed = seed;

		rc = dummy_create(&domain, id, &attr, &play);
		if (rc == 0) {
			if (argc != 8)
				m0_fid_gob_make(&gfid, 0, 999);
			else
				m0_fid_gob_make(&gfid,
						strtol(argv[6], NULL, 0),
						strtol(argv[7], NULL, 0));
			/*TODO: Currently in this utility we assume that no
			 * tolerance is supported for levels higher than disks.
			 * Hence only single cache is sufficient. In future we
			 * need to read configuration string.
			 */
			cache_nr  = 1;
			pool_ver.pv_fd_tree.ft_cache_info.fci_nr = cache_nr;
			M0_ALLOC_ARR(cache_len, cache_nr);
			if (cache_len == NULL) {
				rc = -ENOMEM;
				goto out;
			}
			pool_ver.pv_fd_tree.ft_cache_info.fci_info = cache_len;
			cache_len[0] = P;
			l = m0_pdl_to_layout(play);
			l->l_pver = &pool_ver;
			rc = m0_layout_instance_build(l, &gfid, &li);
			pi = m0_layout_instance_to_pdi(li);
			if (rc == 0) {
				layout_demo(pi, play, R, I, true);
				pi->pi_base.li_ops->lio_fini(&pi->pi_base);
			}
out:
			m0_layout_put(m0_pdl_to_layout(play));
			m0_layout_standard_types_unregister(&domain);
			m0_layout_domain_fini(&domain);
		}
		m0_pool_fini(&pool);
	}

	m0_fini();
	return rc;
}

/** @} end of layout group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
