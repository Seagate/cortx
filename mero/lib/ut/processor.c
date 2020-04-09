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
 * Original author: Rajesh Bhalerao <Rajesh_Bhalerao@xyratex.com>
 * Original creation date: 03/17/2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>

#include "ut/ut.h"
#include "lib/ub.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/processor.h"
#include "lib/string.h"

#define	SYSFS_PATH		"/sys/devices/system"
#define	TEST_SYSFS_PATH		"./cpu_test"

#define MAX_PROCESSOR_FILE	"cpu/kernel_max"
#define POSS_PROCESSOR_FILE	"cpu/possible"
#define AVAIL_PROCESSOR_FILE	"cpu/present"
#define ONLN_PROCESSOR_FILE	"cpu/online"

#define NUMA_FILE1		"cpu/cpu%u/node%u"
#define NUMA_FILE2		"node/node%u/cpu%u"

#define COREID_FILE		"cpu/cpu%u/topology/core_id"
#define PHYSID_FILE		"cpu/cpu%u/topology/physical_package_id"

#define L1SZ_FILE		"cpu/cpu%u/cache/index0/size"
#define L2SZ_FILE1		"cpu/cpu%u/cache/index1/size"
#define L2SZ_FILE2		"cpu/cpu%u/cache/index2/size"

#define C0_LVL_FILE		"cpu/cpu%u/cache/index0/level"
#define C1_LVL_FILE		"cpu/cpu%u/cache/index1/level"
#define M0_LVL_FILE		"cpu/cpu%u/cache/index2/level"

#define C0_SHMAP_FILE		"cpu/cpu%u/cache/index0/shared_cpu_map"
#define C1_SHMAP_FILE		"cpu/cpu%u/cache/index1/shared_cpu_map"
#define M0_SHMAP_FILE		"cpu/cpu%u/cache/index2/shared_cpu_map"

#define	BUF_SZ	512

struct psummary {
	char *kmaxstr;
	char *possstr;
	char *presentstr;
	char *onlnstr;
};

struct pinfo {
	uint32_t    numaid;
	uint32_t    physid;
	uint32_t    coreid;
	uint32_t    c0lvl;
	uint32_t    c1lvl;
	uint32_t    m0lvl;
	const char *c0szstr;
	const char *c1szstr;
	const char *m0szstr;
	const char *c0sharedmapstr;
	const char *c1sharedmapstr;
	const char *m0sharedmapstr;
};

enum {
	UB_ITER = 100000
};

enum {
	POSS_MAP = 1,
	AVAIL_MAP = 2,
	ONLN_MAP = 3
};

char *processor_info_dirp;

uint64_t cpu_masks[] = { 1, 3, 5, 13, 21, 43, 107, 219 };

static int ub_init(const char *opts M0_UNUSED)
{
	return 0;
}

static void ub_fini(void)
{
}

static void ub_init1(int i)
{
}

static void ub_init2(int i)
{
	int rc;

	m0_processors_fini();
	rc = m0_processors_init();
	M0_UT_ASSERT(rc == 0);
}

static void ub_init3(int i)
{
	int rc;

	m0_processors_fini();
	rc = m0_processors_init();
	M0_UT_ASSERT(rc == 0);
}

static uint32_t get_num_from_file(const char *file)
{
	uint32_t  num = 0;
	int       rc;
	FILE	 *fp;

	fp = fopen(file, "r");
	if (fp == NULL)
		return num;
	rc = fscanf(fp, "%u", &num);
	M0_UT_ASSERT(rc != EOF);
	fclose(fp);

	return num;
}

static void maptostr(struct m0_bitmap *map, char *str, size_t sz)
{
	uint32_t  i;
	uint32_t  from_idx;
	uint32_t  to_idx;
	char     *pstr;
	bool      val;
	int       ret;

	M0_UT_ASSERT(map != NULL && str != NULL);
	M0_UT_ASSERT(sz > 0);
	*str = '\0';
	pstr = str;

	for (i = 0; i < map->b_nr; i++) {
		val = m0_bitmap_get(map, i);
		if (val == true) {
			if (*str != '\0') {
				M0_UT_ASSERT(sz > 1);
				strcat(str, ",");
				++pstr;
				--sz;
				M0_UT_ASSERT(*pstr == '\0');
			}
			from_idx = i;
			for (; i < map->b_nr && m0_bitmap_get(map, i); ++i)
				;
			to_idx = i - 1;
			if (from_idx == to_idx)
				ret = snprintf(pstr, sz, "%u", from_idx);
			else
				ret = snprintf(pstr, sz, "%u-%u",
					       from_idx, to_idx);
			M0_UT_ASSERT(ret >= 0);
			M0_UT_ASSERT((size_t)ret < sz);
			pstr += ret;
			sz   -= ret;
		}
	}
}

static void verify_id_get(void)
{
	struct m0_bitmap  map;
	m0_processor_nr_t num;
	m0_processor_nr_t id;
	int               rc;

	rc = m0_processors_init();
	M0_UT_ASSERT(rc == 0);

	num = m0_processor_nr_max();
	m0_bitmap_init(&map, num);

	m0_processors_online(&map);

	id = m0_processor_id_get();
	if (id != M0_PROCESSORS_INVALID_ID)
		M0_UT_ASSERT(m0_bitmap_get(&map, id));

	m0_bitmap_fini(&map);
	m0_processors_fini();
}

static void verify_map(int mapid)
{
	char              *expect;
	char              *map_file = NULL;
	char              *fgets_rc;
	char               buf[BUF_SZ];
	char               result[BUF_SZ];
	char               filename[PATH_MAX];
	int                rc;
	FILE              *fp;
	struct m0_bitmap   map;
	m0_processor_nr_t  num;

	rc = m0_processors_init();
	M0_UT_ASSERT(rc == 0);

	num = m0_processor_nr_max();
	m0_bitmap_init(&map, num);

	switch (mapid) {
	case POSS_MAP:
		map_file = POSS_PROCESSOR_FILE;
		m0_processors_possible(&map);
		break;
	case AVAIL_MAP:
		map_file = AVAIL_PROCESSOR_FILE;
		m0_processors_available(&map);
		break;
	case ONLN_MAP:
		map_file = ONLN_PROCESSOR_FILE;
		m0_processors_online(&map);
		break;
	default:
		M0_UT_ASSERT(0);
		break;
	};

	expect = &buf[0];
	maptostr(&map, expect, BUF_SZ);

	sprintf(filename, "%s/%s", processor_info_dirp, map_file);
	fp = fopen(filename, "r");
	fgets_rc = fgets(result, BUF_SZ - 1, fp);
	M0_UT_ASSERT(fgets_rc != NULL);
	fclose(fp);

	rc = strncmp(result, expect, strlen(expect));
	M0_UT_ASSERT(rc == 0);

	m0_bitmap_fini(&map);
	m0_processors_fini();
}

static void verify_max_processors()
{
	char              filename[PATH_MAX];
	int               rc;
	m0_processor_nr_t num;
	m0_processor_nr_t result;

	rc = m0_processors_init();
	M0_UT_ASSERT(rc == 0);

	sprintf(filename, "%s/%s", processor_info_dirp, MAX_PROCESSOR_FILE);
	result = (m0_processor_nr_t) get_num_from_file(filename);
	/* Convert "maximum index" to "maximum number". */
	++result;

	num = m0_processor_nr_max();
	M0_UT_ASSERT(num == result);

	m0_processors_fini();
}

static void verify_a_processor(m0_processor_nr_t id,
			       struct m0_processor_descr *pd)
{
	int         rc1=0;
	int         rc2=0;
	char        filename[PATH_MAX];
	uint32_t    coreid;
	uint32_t    physid;
	uint32_t    mixedid;
	uint32_t    l1_sz;
	uint32_t    lvl;
	uint32_t    l2_sz;
	struct stat statbuf;

	M0_UT_ASSERT(pd->pd_id == id);
	M0_UT_ASSERT(pd->pd_pipeline == id);

	sprintf(filename, "%s/" NUMA_FILE1, processor_info_dirp,
		id, pd->pd_numa_node);
	rc1 = stat(filename, &statbuf);
	if (rc1 != 0) {
		sprintf(filename, "%s/" NUMA_FILE2, processor_info_dirp,
			pd->pd_numa_node, id);
		rc2 = stat(filename, &statbuf);
		M0_UT_ASSERT(rc2 == 0);
	}
	M0_UT_ASSERT(rc1 == 0 || rc2 == 0 || pd->pd_numa_node == 0);

	sprintf(filename, "%s/" COREID_FILE, processor_info_dirp, id);
	coreid = get_num_from_file(filename);

	sprintf(filename, "%s/" PHYSID_FILE, processor_info_dirp, id);
	physid = get_num_from_file(filename);

	mixedid = physid << 16 | coreid;
	M0_UT_ASSERT(pd->pd_l1 == id || pd->pd_l1 == mixedid);
	M0_UT_ASSERT(pd->pd_l2 == id || pd->pd_l2 == mixedid ||
		     pd->pd_l2 == physid);

	sprintf(filename, "%s/" L1SZ_FILE, processor_info_dirp, id);
	l1_sz = get_num_from_file(filename);

	l1_sz *= 1024;
	M0_UT_ASSERT(pd->pd_l1_sz == l1_sz);

	sprintf(filename, "%s/" C1_LVL_FILE, processor_info_dirp, id);
	lvl = get_num_from_file(filename);
	if (lvl == 1)
		sprintf(filename, "%s/" L2SZ_FILE2, processor_info_dirp, id);
	else
		sprintf(filename, "%s/" L2SZ_FILE1, processor_info_dirp, id);

	l2_sz = get_num_from_file(filename);

	l2_sz *= 1024;
	M0_UT_ASSERT(pd->pd_l2_sz == l2_sz);
}

static void verify_processors()
{
	m0_processor_nr_t         i;
	m0_processor_nr_t         num;
	struct m0_bitmap          onln_map;
	struct m0_processor_descr pd;
	bool                      val;
	int                       rc;

	rc = m0_processors_init();
	M0_UT_ASSERT(rc == 0);

	num = m0_processor_nr_max();
	m0_bitmap_init(&onln_map, num);
	m0_processors_online(&onln_map);

	for (i = 0; i < num; i++) {
		val = m0_bitmap_get(&onln_map, i);
		if (val == true) {
			rc = m0_processor_describe(i, &pd);
			if (rc == 0)
				verify_a_processor(i, &pd);
		}
	}

	m0_bitmap_fini(&onln_map);
	m0_processors_fini();
}

static void write_str_to_file(const char *file, const char *str)
{
	FILE *fp;

	fp = fopen(file, "w");
	if (fp == NULL)
		return;
	fputs(str, fp);
	fclose(fp);
}

static void write_num_to_file(const char *file, uint32_t num)
{
	FILE *fp;

	fp = fopen(file, "w");
	if (fp == NULL)
		return;
	fprintf(fp, "%u\n", num);
	fclose(fp);
}

static void populate_cpu_summary(struct psummary *sum)
{
	int  rc;
	char filename[PATH_MAX];

	sprintf(filename, "mkdir -p %s/cpu", processor_info_dirp);
	rc = system(filename);
	M0_UT_ASSERT(rc != -1);

	if (sum->kmaxstr) {
		sprintf(filename, "%s/" MAX_PROCESSOR_FILE,
			processor_info_dirp);
		write_str_to_file(filename, sum->kmaxstr);
	}

	if (sum->possstr) {
		sprintf(filename, "%s/" POSS_PROCESSOR_FILE,
			processor_info_dirp);
		write_str_to_file(filename, sum->possstr);
	}

	if (sum->presentstr) {
		sprintf(filename, "%s/" AVAIL_PROCESSOR_FILE,
			processor_info_dirp);
		write_str_to_file(filename, sum->presentstr);
	}

	if (sum->onlnstr) {
		sprintf(filename, "%s/" ONLN_PROCESSOR_FILE,
			processor_info_dirp);
		write_str_to_file(filename, sum->onlnstr);
	}
}

static void populate_cpus(struct pinfo cpus[], uint32_t sz)
{
	char      filename[PATH_MAX];
	FILE     *fp;
	uint32_t  i;
	int       rc;

	for (i = 0; i < sz; i++) {
		if (cpus[i].numaid == M0_PROCESSORS_INVALID_ID)
			continue;
		sprintf(filename, "mkdir -p %s/cpu/cpu%u/topology",
			processor_info_dirp, i);
		rc = system(filename);
		M0_UT_ASSERT(rc != -1);
		sprintf(filename, "mkdir -p %s/cpu/cpu%u/cache/index0",
			processor_info_dirp, i);
		rc = system(filename);
		M0_UT_ASSERT(rc != -1);
		sprintf(filename, "mkdir -p %s/cpu/cpu%u/cache/index1",
			processor_info_dirp, i);
		rc = system(filename);
		M0_UT_ASSERT(rc != -1);
		sprintf(filename, "mkdir -p %s/cpu/cpu%u/cache/index2",
			processor_info_dirp, i);
		rc = system(filename);
		M0_UT_ASSERT(rc != -1);

		sprintf(filename, "%s/" NUMA_FILE1, processor_info_dirp, i,
			cpus[i].numaid);
		fp = fopen(filename, "w");
		fclose(fp);

		sprintf(filename, "%s/" COREID_FILE, processor_info_dirp, i);
		write_num_to_file(filename, cpus[i].coreid);

		sprintf(filename, "%s/" PHYSID_FILE, processor_info_dirp, i);
		write_num_to_file(filename, cpus[i].physid);

		sprintf(filename, "%s/" C0_LVL_FILE, processor_info_dirp, i);
		write_num_to_file(filename, cpus[i].c0lvl);

		sprintf(filename, "%s/" C1_LVL_FILE, processor_info_dirp, i);
		write_num_to_file(filename, cpus[i].c1lvl);

		sprintf(filename, "%s/" M0_LVL_FILE, processor_info_dirp, i);
		write_num_to_file(filename, cpus[i].m0lvl);

		if (cpus[i].c0szstr) {
			sprintf(filename, "%s/" L1SZ_FILE,
				processor_info_dirp, i);
			write_str_to_file(filename, cpus[i].c0szstr);
		}

		if (cpus[i].c1szstr) {
			sprintf(filename, "%s/" L2SZ_FILE1,
				processor_info_dirp, i);
			write_str_to_file(filename, cpus[i].c1szstr);
		}

		if (cpus[i].m0szstr) {
			sprintf(filename, "%s/" L2SZ_FILE2,
				processor_info_dirp, i);
			write_str_to_file(filename, cpus[i].m0szstr);
		}

		if (cpus[i].c0sharedmapstr) {
			sprintf(filename, "%s/" C0_SHMAP_FILE,
				processor_info_dirp, i);
			write_str_to_file(filename, cpus[i].c0sharedmapstr);
		}

		if (cpus[i].c1sharedmapstr) {
			sprintf(filename, "%s/" C1_SHMAP_FILE,
				processor_info_dirp, i);
			write_str_to_file(filename, cpus[i].c1sharedmapstr);
		}

		if (cpus[i].m0sharedmapstr) {
			sprintf(filename, "%s/" M0_SHMAP_FILE,
				processor_info_dirp, i);
			write_str_to_file(filename, cpus[i].m0sharedmapstr);
		}

	}			/* for - populate test data for all CPUs */
}

static void clean_test_dataset(void)
{
	char cmd[PATH_MAX];
	int  rc;

	sprintf(cmd, "rm -rf %s", processor_info_dirp);
	rc = system(cmd);
	M0_UT_ASSERT(rc != -1);
}

static void verify_all_params()
{
	verify_max_processors();
	verify_map(POSS_MAP);
	verify_map(AVAIL_MAP);
	verify_map(ONLN_MAP);
	verify_processors();
	if (strcmp(processor_info_dirp, SYSFS_PATH) == 0)
		verify_id_get();
}

static struct psummary *psummary_new(m0_processor_nr_t cpu_max,
				     struct m0_bitmap *map_poss,
				     struct m0_bitmap *map_avail,
				     struct m0_bitmap *map_onln)
{
	struct psummary *ps;
	char             buf[BUF_SZ];
	char            *str = &buf[0];

	M0_ALLOC_PTR(ps);
	M0_UT_ASSERT(ps != NULL);
	/* kmaxstr contains maximum index which starts from 0. */
	snprintf(str, BUF_SZ, "%u", (unsigned)cpu_max - 1);
	ps->kmaxstr = m0_strdup(str);
	M0_UT_ASSERT(ps->kmaxstr != NULL);
	maptostr(map_poss, str, BUF_SZ);
	ps->possstr = m0_strdup(str);
	M0_UT_ASSERT(ps->possstr != NULL);
	maptostr(map_avail, str, BUF_SZ);
	ps->presentstr = m0_strdup(str);
	M0_UT_ASSERT(ps->presentstr != NULL);
	maptostr(map_onln, str, BUF_SZ);
	ps->onlnstr = m0_strdup(str);
	M0_UT_ASSERT(ps->onlnstr != NULL);

	return ps;
}

static void psummary_destroy(struct psummary *ps)
{
	m0_free(ps->kmaxstr);
	m0_free(ps->possstr);
	m0_free(ps->presentstr);
	m0_free(ps->onlnstr);
	m0_free(ps);
}

static struct pinfo *pinfo_new(m0_processor_nr_t cpu_max,
			       struct m0_bitmap *map_poss,
			       struct m0_bitmap *map_avail,
			       struct m0_bitmap *map_onln,
			       size_t           *nr_out)
{
	struct pinfo *pi;
	size_t        nr = 0;
	size_t        i;

	for (i = 0; i < map_avail->b_nr; ++i)
		if (m0_bitmap_get(map_avail, i))
			nr = i + 1;
	M0_UT_ASSERT(nr > 0);
	M0_ALLOC_ARR(pi, nr);
	M0_UT_ASSERT(pi != NULL);

	for (i = 0; i < nr; ++i) {
		if (m0_bitmap_get(map_onln, i)) {
			/* online */
			pi[i].numaid = 1;
			pi[i].physid = 0;
			pi[i].coreid = 0;
			pi[i].c0lvl = 1;
			pi[i].c1lvl = 1;
			pi[i].m0lvl = 2;
			pi[i].c0szstr = "64K\n";
			pi[i].c1szstr = "64K\n";
			pi[i].m0szstr = "540K\n";
			pi[i].c0sharedmapstr = "00000000,00000000,00000000,"
			       "00000000,00000000,00000000,00000000,00000001\n";
			pi[i].c1sharedmapstr = "00000000,00000000,00000000,"
			       "00000000,00000000,00000000,00000000,00000001\n";
			pi[i].m0sharedmapstr = "00000000,00000000,00000000,"
			       "00000000,00000000,00000000,00000000,00000003\n";
		} else if (m0_bitmap_get(map_avail, i)) {
			/* present */
			pi[i].numaid = 1;
			pi[i].physid = 0;
			pi[i].coreid = 0;
			pi[i].c0lvl = 1;
			pi[i].c1lvl = 1;
			pi[i].m0lvl = 2;
			pi[i].c0szstr = "64K\n";
			pi[i].c1szstr = "64K\n";
			pi[i].m0szstr = "540K\n";
			pi[i].c0sharedmapstr = "00000000,00000000,00000000,"
			       "00000000,00000000,00000000,00000000,00000001\n";
			pi[i].c1sharedmapstr = "00000000,00000000,00000000,"
			       "00000000,00000000,00000000,00000000,00000001\n";
			pi[i].m0sharedmapstr = "00000000,00000000,00000000,"
			       "00000000,00000000,00000000,00000000,00000003\n";
		} else {
			/* not present */
			pi[i].numaid = M0_PROCESSORS_INVALID_ID;
		}
	}

	*nr_out = nr;
	return pi;
}

void pinfo_destroy(struct pinfo *pi)
{
	m0_free(pi);
}

void test_processor(void)
{
	struct m0_bitmap   map_poss  = {};
	struct m0_bitmap   map_avail = {};
	struct m0_bitmap   map_onln  = {};
	struct m0_bitmap   map       = {};
	struct m0_bitmap  *map_avail2;
	struct psummary   *ps;
	struct pinfo      *pi;
	m0_processor_nr_t  cpu_max   = m0_processor_nr_max();
	m0_processor_nr_t  cpu_poss;
	m0_processor_nr_t  cpu_avail;
	m0_processor_nr_t  cpu_onln;
	m0_processor_nr_t  i;
	uint64_t           rmask;
	size_t             nr;
	size_t             j;
	int                rc;

	rc = m0_bitmap_init(&map_poss, cpu_max);
	M0_UT_ASSERT(rc == 0);
	rc = m0_bitmap_init(&map_avail, cpu_max);
	M0_UT_ASSERT(rc == 0);
	rc = m0_bitmap_init(&map_onln, cpu_max);
	M0_UT_ASSERT(rc == 0);
	rc = m0_bitmap_init(&map, cpu_max);
	M0_UT_ASSERT(rc == 0);
	m0_processors_possible(&map_poss);
	cpu_poss = m0_bitmap_set_nr(&map_poss);
	m0_processors_available(&map_avail);
	cpu_avail = m0_bitmap_set_nr(&map_avail);
	m0_processors_online(&map_onln);
	cpu_onln = m0_bitmap_set_nr(&map_onln);

	M0_UT_ASSERT(cpu_poss <= cpu_max);
	M0_UT_ASSERT(cpu_avail <= cpu_poss);
	M0_UT_ASSERT(cpu_onln <= cpu_avail);
	for (i = 0; i < cpu_max; ++i) {
		M0_UT_ASSERT(ergo(m0_bitmap_get(&map_onln, i),
				  m0_bitmap_get(&map_avail, i)));
		M0_UT_ASSERT(ergo(m0_bitmap_get(&map_avail, i),
				  m0_bitmap_get(&map_poss, i)));
	}

	m0_processors_fini(); /* clean normal data so we can load test data */

	processor_info_dirp = SYSFS_PATH;
	verify_all_params();

	processor_info_dirp = TEST_SYSFS_PATH;
	setenv("M0_PROCESSORS_INFO_DIR", TEST_SYSFS_PATH, 1);

	for (i = 0; i < cpu_onln && i < ARRAY_SIZE(cpu_masks); ++i) {
		/*
		 * apply a mask to real online cpu map in order to create
		 * test data.
		 */
		rmask = cpu_masks[i];
		for (j = 0; j < map_onln.b_nr; ++j) {
			if (m0_bitmap_get(&map_onln, j)) {
				m0_bitmap_set(&map, j, rmask % 2 == 1);
				rmask /= 2;
			} else
				m0_bitmap_set(&map, j, false);
		}
		/* map of present CPUs equals to online's every 2nd step */
		map_avail2 = i % 2 == 0 ? &map_avail : &map;

		ps = psummary_new(cpu_max, &map_poss, map_avail2, &map);
		pi = pinfo_new(cpu_max, &map_poss, map_avail2, &map, &nr);
		populate_cpu_summary(ps);
		populate_cpus(pi, nr);
		verify_all_params();
		clean_test_dataset();
		pinfo_destroy(pi);
		psummary_destroy(ps);
	}

	m0_bitmap_fini(&map_poss);
	m0_bitmap_fini(&map_avail);
	m0_bitmap_fini(&map_onln);
	m0_bitmap_fini(&map);

	unsetenv("M0_PROCESSORS_INFO_DIR");
	/* restore normal data */
	rc = m0_processors_init();
	M0_UT_ASSERT(rc == 0);
}

struct m0_ub_set m0_processor_ub = {
	.us_name = "processor-ub",
	.us_init = ub_init,
	.us_fini = ub_fini,
	.us_run = {
		   {.ub_name = "Init1",
		    .ub_iter = UB_ITER,
		    .ub_round = ub_init1},
		   {.ub_name = "Init2",
		    .ub_iter = UB_ITER,
		    .ub_round = ub_init2},
		   {.ub_name = "Init3",
		    .ub_iter = UB_ITER,
		    .ub_round = ub_init3},
		   {.ub_name = NULL}
		   }
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
