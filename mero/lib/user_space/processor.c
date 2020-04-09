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
 * Original author: Rajesh Bhalerao <Rajesh_Bhalerao@xyratex.com>
 * Original creation date: 03/11/2011
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>  /* syscall, SYS_getcpu */
#include <dirent.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sched.h>        /* sched_getcpu() */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER
#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/processor.h"
#include "lib/list.h"
#include "lib/thread.h"
#include "module/instance.h"

/**
   @addtogroup processor

   @section proc-user User-space implementation

   This file includes additional data structures and function for caching
   processors data - for user-mode programs.

   This file will also implement Linux user-mode processors interfaces.

   @section TestCases

   @subsection Configuration
   Get sysfs file system data for following configurations:
      - Single CPU
      - SMP
      - SMP with multicore CPUs
      - NUMA
      - Configurations with Intel, AMD processors
      - Developer VMs
   @subsection Procedure
   - Point the directory for sysfs file system data for the above
     configurations using environment variable 'M0_PROCESSORS_INFO_DIR'.
   - Create expected reults file.
   -  Run unit test and compare the processor info structure with expected
      results.
   @{
 */
#define PROCESSORS_INFO_ENV             "M0_PROCESSORS_INFO_DIR"

#define PROCESSORS_SYSFS_DIR            "/sys/devices/system"
#define PROCESSORS_CPU_DIR              "cpu/"
#define PROCESSORS_NODE_DIR             "node/"

#define PROCESSORS_MAX_FILE             "cpu/kernel_max"
#define PROCESSORS_POSS_FILE            "cpu/possible"
#define PROCESSORS_PRESENT_FILE         "cpu/present"
#define PROCESSORS_ONLINE_FILE          "cpu/online"

#define PROCESSORS_CACHE1_LEVEL_FILE    "cache/index0/level"
#define PROCESSORS_CACHE2_LEVEL_FILE    "cache/index1/level"
#define PROCESSORS_CACHE3_LEVEL_FILE    "cache/index2/level"

#define PROCESSORS_CACHE1_SHCPUMAP_FILE "cache/index0/shared_cpu_map"
#define PROCESSORS_CACHE2_SHCPUMAP_FILE "cache/index1/shared_cpu_map"
#define PROCESSORS_CACHE3_SHCPUMAP_FILE "cache/index2/shared_cpu_map"

#define PROCESSORS_CACHE1_SIZE_FILE     "cache/index0/size"
#define PROCESSORS_CACHE2_SIZE_FILE     "cache/index1/size"
#define PROCESSORS_CACHE3_SIZE_FILE     "cache/index2/size"

#define PROCESSORS_COREID_FILE          "topology/core_id"
#define PROCESSORS_PHYSID_FILE          "topology/physical_package_id"

#define PROCESSORS_CPU_DIR_PREFIX       "cpu/cpu"

#define PROCESSORS_NODE_STR             "node"
#define PROCESSORS_CPU_STR              "cpu"

enum {
	PROCESSORS_RANGE_SET_SEPARATOR = ',',
	PROCESSORS_RANGE_SEPARATOR     = '-',

	PROCESSORS_L1                  = 1,
	PROCESSORS_L2                  = 2,

	MAX_LINE_LEN                   = 256,
};

enum map {
	PROCESSORS_POSS_MAP  = 0,
	PROCESSORS_AVAIL_MAP = 1,
	PROCESSORS_ONLN_MAP  = 2,
};

/**
   System wide summary of all the processors. This is the head node.
   It contains various processor statistics and a linked list of
   processor info (struct m0_processor_descr).

   @see lib/processor.h
 */
struct processor_sys_summary {
	/** Head of the list for processor info */
	struct m0_list    pss_head;

	/** Number of possible processors that can be attached to this OS.
          This means the maximum number of processors that OS can handle.  */
	m0_processor_nr_t pss_max;

	/** bitmap of possible processors on this node */
	struct m0_bitmap  pss_poss_map;

	/** bitmap of processors that are present on this node */
	struct m0_bitmap  pss_avail_map;

	/** bitmap of onln processors on this node */
	struct m0_bitmap  pss_onln_map;
};

/**
   A node in the linked list describing processor properties. It
   encapsulates 'struct m0_processor_descr'.
   @see lib/processor.h
 */
struct processor_node {
	/** Linking structure for node */
	struct m0_list_link       pn_link;

	/** Processor descriptor structure */
	struct m0_processor_descr pn_info;
};

/**
   Global data for M0_MODULE_PROCESSOR.
 */
struct processor_global {
	bool pg_getcpu_inited;
	/**
	 * True when sched_getcpu() is not functional on current platform.
	 * @see m0_processor_id_get()
	 */
	bool pg_getcpu_workaround;
};

struct getcpu_thr_data {
	/** Processor id */
	m0_processor_nr_t  ptd_idx;
	struct m0_thread   ptd_thread;
	/** Bitmap for thread affinity */
	struct m0_bitmap   ptd_cpu_map;
	/** Result of a single cpu check */
	bool               ptd_success;
};

/* Global Variables. */
static struct processor_sys_summary sys_cpus;
static bool processor_init = false;

/**
   This function converts a bitmap string into a bitmap of m0_bitmap type.

   @pre map != NULL
   @pre mapstr != NULL

   @param map -> m0 bitmap structure that will store the bitmap. Memory
                 for this parameter should be allocated before calling.
   @param mapstr -> bitmap string
   @see processor_map_type_set()
 */
static void processor_map_set(struct m0_bitmap *map, const char *mapstr)
{
	uint32_t id;
	uint32_t to_id;
	char    *ptr;

	M0_PRE(map != NULL);
	M0_PRE(mapstr != NULL);

	/*
	 * Walk the string, parsing separate cpu ranges.
	 * For example, the string looks like 0-3,7,8-12
	 */
	for (; mapstr != NULL && *mapstr != 0; mapstr = ptr) {
		/*
		 * Parse from and to indices within the range.
		 */
		id = strtoul(mapstr, &ptr, 0);
		if (*ptr == PROCESSORS_RANGE_SEPARATOR)
			to_id = strtoul(ptr + 1, &ptr, 0);
		else
			to_id = id;
		/*
		 * Set the bitmap for the given range.
		 */
		M0_ASSERT(to_id < map->b_nr);
		while (id <= to_id)
			m0_bitmap_set(map, id++, true);
		if (*ptr == PROCESSORS_RANGE_SET_SEPARATOR)
			++ptr;
		else
			ptr = NULL;
	} /* for - string is parsed */
}

/**
   Read map files under sysfs. Read the present cpu string and
   convert it into a bitmap.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @see processor_map_set()
 */
static int processor_map_type_set(enum map map_type)
{
	int                rc;
	char               buf[MAX_LINE_LEN + 1];
	char              *str;
	FILE              *fp;
	struct m0_bitmap  *pbitmap;
	static const char *fname[] = {
		[PROCESSORS_POSS_MAP]  = PROCESSORS_POSS_FILE,
		[PROCESSORS_AVAIL_MAP] = PROCESSORS_PRESENT_FILE,
		[PROCESSORS_ONLN_MAP]  = PROCESSORS_ONLINE_FILE,
	};

	M0_PRE((unsigned) map_type <= PROCESSORS_ONLN_MAP);
	fp = fopen(fname[map_type], "r");
	if (fp == NULL)
		return -errno;

        str = fgets(buf, MAX_LINE_LEN, fp);
	fclose(fp);
	if (str == NULL)
		return M0_ERR(-ENODATA);

	switch (map_type) {
	case PROCESSORS_POSS_MAP :
		pbitmap = &sys_cpus.pss_poss_map;
		break;

	case PROCESSORS_AVAIL_MAP :
		pbitmap = &sys_cpus.pss_avail_map;
		break;

	case PROCESSORS_ONLN_MAP :
		pbitmap = &sys_cpus.pss_onln_map;
		break;
	default:
		M0_IMPOSSIBLE("Incorrect map_type value");
		pbitmap = NULL;
		break;
	}

	rc = m0_bitmap_init(pbitmap, sys_cpus.pss_max);
	if (rc == 0)
		processor_map_set(pbitmap, buf);
	return M0_RC(rc);
}

/**
   Reads a file. Returns an unsigned number.

   @param filename -> file to read
   @return non-negative number, if successful;
           M0_PROCESSORS_INVALID_ID, upon failure.
 */
static uint32_t number_read(const char *filename)
{
	uint32_t val = M0_PROCESSORS_INVALID_ID;
	int      rc;
	FILE    *fp;

	fp = fopen(filename, "r");
	if (fp == NULL)
		return val;

	rc = fscanf(fp, "%u", &val);
	if (rc <= 0)
		val = M0_PROCESSORS_INVALID_ID;

	fclose(fp);
	return val;
}

/**
   Read "cpu/kernel_max" file under sysfs. Read the kernel_max cpu string and
   convert it into a number.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @see processor_map_set()
   @see processor_map_type_set()
 */
static void processor_maxsz_get()
{
	/*
	 * cpu/kernel_max contains maximum index which starts from 0. Therefore,
	 * we need to convert it to maximum number of CPUs.
	 */
	sys_cpus.pss_max = number_read(PROCESSORS_MAX_FILE) + 1;
}

/**
   Fetch NUMA node id for a given processor.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @param id -> id of the processor for which information is requested.
   @return id of the NUMA node to which the processor belongs. If the
           machine is not configured as NUMA, returns 0.
 */
static uint32_t processor_numanodeid_get(m0_processor_nr_t id)
{
	uint32_t       numa_node_id = 0;
	bool           gotid = false;
	int            rc;
	char           dirname[PATH_MAX];
	DIR           *dirp;
	struct stat    statbuf;
	struct dirent *fname;

	sprintf(dirname, PROCESSORS_CPU_DIR_PREFIX"%u", id);
	dirp = opendir(dirname);
	if (dirp == NULL)
		return numa_node_id;

	/*
	 * Find node id under .../cpu/cpu<id>/node<id>
	 */
	while ((fname = readdir(dirp)) != NULL) {
		if (strncmp(fname->d_name, PROCESSORS_NODE_STR,
			    sizeof(PROCESSORS_NODE_STR) - 1) == 0) {
			sscanf(fname->d_name,
			       PROCESSORS_NODE_STR"%u", &numa_node_id);
			gotid = true;
			break;
		}
	}/* while - entire cpuX dir is scanned */

	closedir(dirp);
	if (gotid)
		return numa_node_id;

	/*
	 * If nodeid file is not found in previous search, look for cpuX file
	 * under node/nod<id>/cpu<id>
	 */
	dirp = opendir(PROCESSORS_NODE_DIR);
	if (dirp == NULL)
		return numa_node_id;

	while ((fname = readdir(dirp)) != NULL) {
		if (strncmp(fname->d_name, PROCESSORS_NODE_STR,
			    sizeof(PROCESSORS_NODE_STR) - 1) == 0) {
			sprintf(dirname, "%s%s/%s%u",
				PROCESSORS_NODE_DIR, fname->d_name,
				PROCESSORS_CPU_STR, id);
			rc = stat(dirname, &statbuf);
			if (rc == 0) {
				sscanf(fname->d_name,
				       PROCESSORS_NODE_STR"%u", &numa_node_id);
				break;
			}
		}
	}/* while - entire node dir is scanned */
	closedir(dirp);

	/*
	 * Note : If no numa node id is found, it's assumed to be 0.
	 */
	return numa_node_id;
}

/**
   Read "cpu/cpu<id>/toplology/core_id" file under sysfs. Read the core id
   string convert it into a number.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @param id -> id of the processor for which information is requested.
   @return "core" id for a given processor, on success.
           M0_PROCESSORS_INVALID_ID, on failure.
 */
static uint32_t processor_coreid_get(m0_processor_nr_t id)
{
	char filebuf[PATH_MAX];

	sprintf(filebuf, PROCESSORS_CPU_DIR_PREFIX"%u/"
			 PROCESSORS_COREID_FILE, id);
	return number_read(filebuf);
}

/**
   Read "cpu/cpu<id>/toplology/physical_package_id" file under sysfs. Read
   the physical package id string convert it into a number.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @param id -> id of the processor for which information is requested.
   @return "phys" id for a given processor, on success.
           M0_PROCESSORS_INVALID_ID, on failure.
 */
static uint32_t processor_physid_get(m0_processor_nr_t id)
{
	char filebuf[PATH_MAX];

	sprintf(filebuf, PROCESSORS_CPU_DIR_PREFIX"%u/"
			 PROCESSORS_PHYSID_FILE, id);
	return number_read(filebuf);
}

/**
   Read string bitmap and check if the other CPUs share the resource.

   @param mapstr -> a string representing a bitmap
   @retval true  if the processor shares the map
   @retval false if the processor does not the map
 */
static bool processor_is_cache_shared(const char *mapstr)
{
	char    *ptr;
	uint32_t num;
	int      shared_cpus = 0;

	/*
	 * The string is a bitmap. Each byte string is separated by ','.
	 */
	for (; mapstr != NULL && *mapstr != 0; mapstr = ptr) {
		/*
		 * Convert the "bytes" hexstring into a number, add to bits set.
		 */
		for (num = strtoul(mapstr, &ptr, 16); num != 0; num >>= 1)
			shared_cpus += num & 1;
		/*
		 * If we have already found more than one cpu, don't
		 * scan further.
		 */
		if (shared_cpus > 1) /* Is cache shared? */
			break;
		if (*ptr == PROCESSORS_RANGE_SET_SEPARATOR)
			++ptr;
		else
			ptr = NULL;
	} /* for - entire string is parsed */

	M0_POST(shared_cpus >= 1);
	return shared_cpus > 1;
}

/**
   Fetch L1 cache size for a given processor.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @param id -> id of the processor for which information is requested.
   @return size of L1 cache for the given processor.
 */
static size_t processor_l1_size_get(m0_processor_nr_t id)
{
	uint32_t level;
	uint32_t sz;
	int      rc;
	size_t   size = M0_PROCESSORS_INVALID_ID;
	char     filename[PATH_MAX];
	FILE    *fp;

	sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
				   PROCESSORS_CACHE1_LEVEL_FILE);
	level = number_read(filename);
	if (level == M0_PROCESSORS_INVALID_ID)
		return size;

	M0_ASSERT(level == PROCESSORS_L1);

	/*
	 * Set path to appropriate cache size file
	 */
	sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
				   PROCESSORS_CACHE1_SIZE_FILE);

	/*
	 * Get the size string. It's in format 32K, 6144K etc.
	 */
	fp = fopen(filename, "r");
	if (fp == NULL)
		return size;

	rc = fscanf(fp, "%uK", &sz);
	fclose(fp);
	if (rc <= 0)
		return size;

	size = (size_t) sz * 1024;
	return size;
}

/**
   Fetch L2 cache size for a given processor.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @param id -> id of the processor for which information is requested.
   @return size of L2 cache for the given processor, on success.
           M0_PROCESSORS_INVALID_ID, on failure.
 */
static size_t processor_l2_size_get(m0_processor_nr_t id)
{
	uint32_t level;
	uint32_t sz;
	int      rc;
	size_t   size = M0_PROCESSORS_INVALID_ID;
	char     filename[PATH_MAX];
	FILE    *fp;

	sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
				   PROCESSORS_CACHE3_LEVEL_FILE);
	level = number_read(filename);
	if (level == M0_PROCESSORS_INVALID_ID)
		return size;

	if (level != PROCESSORS_L2) { /* If L2 level is not found */
		sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
					   PROCESSORS_CACHE2_LEVEL_FILE);
		level = number_read(filename);
		if (level == M0_PROCESSORS_INVALID_ID)
			return size;

		M0_ASSERT(level == PROCESSORS_L2);
		/*
		 * Set path to appropriate cache size file
		 */
		sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
					   PROCESSORS_CACHE2_SIZE_FILE);
	} else {
		/*
		 * Set path to appropriate cache size file
		 */
		sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
					   PROCESSORS_CACHE3_SIZE_FILE);
	}

	/*
	 * Get the size string. It's in format 32K, 6144K etc.
	 */
	fp = fopen(filename, "r");
	if (fp == NULL)
		return size;

	rc = fscanf(fp, "%uK", &sz);
	fclose(fp);
	if (rc <= 0)
		return size;

	size = (size_t)(sz * 1024);
	return size;
}

/**
   Fetch L1 cache id for a given processor.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @param id -> id of the processor for which information is requested.
   @return id of L1 cache for the given processor, on success.
           M0_PROCESSORS_INVALID_ID, on failure.
 */
static uint32_t processor_l1_cacheid_get(m0_processor_nr_t id)
{
	uint32_t level;
	uint32_t coreid;
	uint32_t physid;
	uint32_t l1_id = id;

	bool     is_shared;

	char     filename[PATH_MAX];
	char     buf[MAX_LINE_LEN + 1];
	char    *str;

	FILE    *fp;

	sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
				   PROCESSORS_CACHE1_LEVEL_FILE);
	level = number_read(filename);
	if (level == M0_PROCESSORS_INVALID_ID)
		return l1_id;

	M0_ASSERT(level == PROCESSORS_L1);
	/*
	 * Set path to appropriate shared cpu file name
	 */
	sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
			  PROCESSORS_CACHE1_SHCPUMAP_FILE);

	/*
	 * Get the shared cpu map string.
	 */
	fp = fopen(filename, "r");
	if (fp == NULL)
		return l1_id;

        str = fgets(buf, MAX_LINE_LEN, fp);
	fclose(fp);
	if (str == NULL)
		return l1_id;

	/*
	 * Scan the map string to find how many bits are set in the string
	 * If more than one bit is set, then cache is shared.
	 */
	is_shared = processor_is_cache_shared(buf);
	if (is_shared) { /* L1 cache is shared */
		physid = processor_physid_get(id);
		coreid = processor_coreid_get(id);
		l1_id = physid << 16 | coreid;
	}

	return l1_id;
}

/**
   Fetch L2 cache id for a given processor.

   @pre Assumes the directory has been changed to approriate CPU info dir.
   @param id -> id of the processor for which information is requested.
   @return id of L2 cache for the given processor, on success.
           M0_PROCESSORS_INVALID_ID, on failure.
 */
static uint32_t processor_l2_cacheid_get(m0_processor_nr_t id)
{
	uint32_t level;
	uint32_t l2_id = id;
	uint32_t coreid;
	uint32_t physid;

	bool     is_shared;
	bool     l3_cache_present = false;

	char     filename[PATH_MAX];
	char     buf[MAX_LINE_LEN + 1];
	char    *str;

	FILE    *fp;

	sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
				   PROCESSORS_CACHE3_LEVEL_FILE);
	level = number_read(filename);
	if (level == M0_PROCESSORS_INVALID_ID)
		return l2_id;

	if (level != PROCESSORS_L2) {
		sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
					   PROCESSORS_CACHE2_LEVEL_FILE);
		level = number_read(filename);
		if (level == M0_PROCESSORS_INVALID_ID)
			return l2_id;

		M0_ASSERT(level == PROCESSORS_L2);
		l3_cache_present = true;
		/*
		 * Set path to appropriate shared cpu file name
		 */
		sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
					   PROCESSORS_CACHE2_SHCPUMAP_FILE);
	} else {
		/*
		 * Set path to appropriate shared cpu file name
		 */
		sprintf(filename, "%s%u/%s", PROCESSORS_CPU_DIR_PREFIX, id,
					   PROCESSORS_CACHE3_SHCPUMAP_FILE);
	}

	/*
	 * Get the shared cpu map string.
	 */
	fp = fopen(filename, "r");
	if (fp == NULL)
		return l2_id;

        str = fgets(buf, MAX_LINE_LEN, fp);
	fclose(fp);
	if (str == NULL)
		return l2_id;

	/*
	 * Scan the map string to find how many bits are set in the string
	 */
	is_shared = processor_is_cache_shared(buf);
	if (is_shared) { /* L2 cache is shared */
		physid = processor_physid_get(id);

		if (l3_cache_present) { /* L3 cache is present */
			coreid = processor_coreid_get(id);
			l2_id = physid << 16 | coreid;
		} else
			l2_id = physid;
	}

	return l2_id;
}

/**
   Fetch pipeline id for a given processor.
   Curently pipeline id is same as processor id.

   @param id -> id of the processor for which information is requested.
   @return id of pipeline for the given processor.
 */
static inline uint32_t processor_pipelineid_get(m0_processor_nr_t id)
{
	return id;
}

/**
   Collect all the information needed to describe a single processor.
   This function will scan,parse directories and files under "sysfs".
   This data is cached.

   This function will be called from processors_summary_get().

   @param id -> id of the processor for which information is requested.
   @param pn -> A linked list node containing processor information.

   @pre Memory to 'pn' must be allocated by the calling function
   @post pn structure will be filled with processor information

   @see processors_summary_get()
 */
static int processor_info_get(m0_processor_nr_t id, struct processor_node *pn)
{
	M0_PRE(pn != NULL);

	pn->pn_info.pd_numa_node = processor_numanodeid_get(id);
	if (pn->pn_info.pd_numa_node == M0_PROCESSORS_INVALID_ID)
		return M0_ERR(-EINVAL);

	pn->pn_info.pd_l1 = processor_l1_cacheid_get(id);
	if (pn->pn_info.pd_l1 == M0_PROCESSORS_INVALID_ID)
		return M0_ERR(-EINVAL);

	pn->pn_info.pd_l1_sz = processor_l1_size_get(id);
	if (pn->pn_info.pd_l1_sz == M0_PROCESSORS_INVALID_ID)
		return M0_ERR(-EINVAL);

	pn->pn_info.pd_l2 = processor_l2_cacheid_get(id);
	if (pn->pn_info.pd_l2 == M0_PROCESSORS_INVALID_ID)
		return M0_ERR(-EINVAL);

	pn->pn_info.pd_l2_sz = processor_l2_size_get(id);
	if (pn->pn_info.pd_l2_sz == M0_PROCESSORS_INVALID_ID)
		return M0_ERR(-EINVAL);

	pn->pn_info.pd_id = id;
	pn->pn_info.pd_pipeline = processor_pipelineid_get(id);

	return 0;
}

/**
   Cache clean-up.

   @see m0_processors_fini
   @see processors_summary_get
 */
static void processor_cache_destroy(void)
{
	struct m0_list_link   *node;
	struct processor_node *pinfo;

	m0_bitmap_fini(&sys_cpus.pss_poss_map);
	m0_bitmap_fini(&sys_cpus.pss_avail_map);
	m0_bitmap_fini(&sys_cpus.pss_onln_map);
	sys_cpus.pss_max = 0;

	/* Remove all the processor nodes. */
	node = sys_cpus.pss_head.l_head;
	while((struct m0_list *)node != &sys_cpus.pss_head) {
		pinfo = m0_list_entry(node, struct processor_node, pn_link);
		m0_list_del(&pinfo->pn_link);
		m0_free(pinfo);
		node = sys_cpus.pss_head.l_head;
	}
	m0_list_fini(&sys_cpus.pss_head);
}

/**
   Parse "sysfs" (/sys/devices/system) directory to fetch the summary of
   processors on this system.

   To facilitate testing, this function will fetch the directory string from
   environment variable M0_PROCESSORS_INFO_DIR. This environment variable
   should be used only for unit testing.
   Under normal operation, a default value of "sysfs" directory is used.

   @pre  M0_PROCESSORS_INFO_DIR/default directory must exist.
   @post A global variable of type processor_sys_summary will be filled in

   @see lib/processor.h
   @see void m0_processors_init()
 */
static int processors_summary_get()
{
	int                    rc;
	uint32_t               cpuid;
	char                  *dirp = getenv(PROCESSORS_INFO_ENV);
	char                  *str;
	char                   cwd[PATH_MAX];
	struct processor_node *pn;

	if (dirp == NULL)
		dirp = PROCESSORS_SYSFS_DIR;

	/*
	 * Obtain current working directory.
	 */
	str = getcwd(cwd, sizeof(cwd) - 1);
	if (str == NULL)
		return -errno;

	/*
	 * Change directory to desired "sysfs" directory.
	 * Subsequent functions will use file names "relative" to syfs dir.
	 * Subsequent function will work in the context of "sysfs" directory.
	 */
	rc = chdir(dirp);
	if (rc != 0)
		return -errno;

	/*
	 * Now get the summary of max/possible/avail/online CPUs
	 * All these functions will collect summary in 'sys_cpus'.
	 */

	processor_maxsz_get();
	if (sys_cpus.pss_max == M0_PROCESSORS_INVALID_ID) {
		rc = chdir(cwd);
		if (rc != 0)
			M0_LOG(M0_ERROR, "failed to chdir to '%s'", (char*)cwd);
		return M0_ERR(-EINVAL);
	}

	rc = processor_map_type_set(PROCESSORS_POSS_MAP);
	if (rc != 0) {
		int rc2 = chdir(cwd);
		if (rc2 != 0)
			M0_LOG(M0_ERROR, "failed to chdir to '%s'", (char*)cwd);
		return M0_RC(rc);
	}

	rc = processor_map_type_set(PROCESSORS_AVAIL_MAP);
	if (rc != 0) {
		int rc2;
		m0_bitmap_fini(&sys_cpus.pss_poss_map);
		rc2 = chdir(cwd);
		if (rc2 != 0)
			M0_LOG(M0_ERROR, "failed to chdir to '%s'", (char*)cwd);
		return M0_RC(rc);
	}

	rc = processor_map_type_set(PROCESSORS_ONLN_MAP);
	if (rc != 0) {
		int rc2;
		m0_bitmap_fini(&sys_cpus.pss_poss_map);
		m0_bitmap_fini(&sys_cpus.pss_avail_map);
		rc2 = chdir(cwd);
		if (rc2 != 0)
			M0_LOG(M0_ERROR, "failed to chdir to '%s'", (char*)cwd);
		return M0_RC(rc);
	}

	m0_list_init(&sys_cpus.pss_head);

	/*
	 * Using present/available CPU mask get details of each processor.
	 */
	for (cpuid = 0; cpuid < sys_cpus.pss_avail_map.b_nr; ++cpuid) {
		if (m0_bitmap_get(&sys_cpus.pss_avail_map, cpuid)) {
			M0_ALLOC_PTR(pn);
			if (pn == NULL) {
				processor_cache_destroy();
				return M0_ERR(-ENOMEM);
			}
			rc = processor_info_get(cpuid, pn);
			if (rc != 0)
				m0_free(pn);
			else
				m0_list_add(&sys_cpus.pss_head, &pn->pn_link);

		} /* if - processor is present on the system */

	}/* for - scan all the available processors */

	if (m0_list_is_empty(&sys_cpus.pss_head)) {
		processor_cache_destroy();
		rc = chdir(cwd);
		if (rc != 0)
			M0_LOG(M0_ERROR, "failed to chdir to '%s'", (char*)cwd);
		return M0_ERR(-ENODATA);
	}

	/*
	 * Change back to previous working dir
	 */
	rc = chdir(cwd);
	if (rc != 0)
		return -errno;
	return 0;
}

/**
   Copy m0_bitmap.

   @param dst -> Destination bitmap
   @param src -> Source bitmap.
 */
static void processors_m0bitmap_copy(struct m0_bitmap *dst,
				     const struct m0_bitmap *src)
{
	M0_PRE(dst->b_nr >= sys_cpus.pss_max);
	m0_bitmap_copy(dst, src);
}

static int getcpu_thr_init(struct getcpu_thr_data *data)
{
	return m0_thread_confine(&data->ptd_thread, &data->ptd_cpu_map);
}

static void getcpu_thr_func(struct getcpu_thr_data *data)
{
	m0_processor_nr_t idx = m0_processor_id_get();

	data->ptd_success = idx == data->ptd_idx;
}

/**
   Checks source of processor id. It runs threads on every online processor
   and compares processors index with value returned by m0_processor_id_get().
   On some systems sched_getcpu() may be broken and return zero on any
   processor. If it happens the syscall getcpu() is used as workaround.

   @param[out] success -> Result of the check.

   @see https://jts.seagate.com/browse/MERO-2500.
 */
static int processor_getcpu_check(bool *success)
{
	struct getcpu_thr_data  *tdata;
	struct m0_bitmap         map_online = {};
	size_t                   cpu_max;
	size_t                   cpu_nr;
	size_t                   i;
	size_t                   j;
	bool                     result = true;
	int                      rc;
	int                      rc2;

	cpu_max = m0_processor_nr_max();
	rc = m0_bitmap_init(&map_online, cpu_max);
	if (rc != 0)
		return M0_RC(rc);
	m0_processors_online(&map_online);
	cpu_nr = m0_bitmap_set_nr(&map_online);

	M0_ALLOC_ARR(tdata, cpu_nr);
	if (tdata == NULL) {
		m0_bitmap_fini(&map_online);
		return M0_ERR(-ENOMEM);
	}

	for (i = 0, j = 0; i < map_online.b_nr; ++i) {
		if (!m0_bitmap_get(&map_online, i))
			continue;

		tdata[j].ptd_idx = (m0_processor_nr_t)i;
		rc = m0_bitmap_init(&tdata[j].ptd_cpu_map, cpu_max);
		if (rc != 0)
			break;
		m0_bitmap_set(&tdata[j].ptd_cpu_map, i, true);

		rc = M0_THREAD_INIT(&tdata[j].ptd_thread,
				    struct getcpu_thr_data *,
				    getcpu_thr_init, &getcpu_thr_func,
				    &tdata[j], "m0_getcpu_check");
		if (rc != 0) {
			m0_bitmap_fini(&tdata[i].ptd_cpu_map);
			break;
		}
		++j;
	}
	M0_ASSERT(ergo(rc == 0, j == cpu_nr));

	for (i = 0; i < j; ++i) {
		rc2 = m0_thread_join(&tdata[i].ptd_thread);
		rc  = rc ?: rc2;
		result = rc2 == 0 && result && tdata[i].ptd_success;
		m0_thread_fini(&tdata[i].ptd_thread);
		m0_bitmap_fini(&tdata[i].ptd_cpu_map);
	}

	m0_free(tdata);
	m0_bitmap_fini(&map_online);

	if (rc == 0)
		*success = result;
	return M0_RC(rc);
}

static int processor_getcpu_init(void)
{
	struct processor_global *pg = m0_get()->i_moddata[M0_MODULE_PROCESSOR];
	bool                     success;
	int                      rc;

	pg->pg_getcpu_inited = true;
	pg->pg_getcpu_workaround = false;
	rc = processor_getcpu_check(&success);
	if (rc == 0 && !success) {
		M0_LOG(M0_WARN, "sched_getcpu(3) doesn't return expected"
				"values, fall back to syscall getcpu(2).");
		pg->pg_getcpu_workaround = true;
		rc = processor_getcpu_check(&success);
		if (rc == 0 && !success)
			rc = M0_ERR(-ENODEV);
	}
	if (rc != 0)
		pg->pg_getcpu_inited = false;

	return M0_RC(rc);
}

static void processor_getcpu_fini(void)
{
}

/* ---- Processor Interface Implementation ---- */

M0_INTERNAL int m0_processors_init()
{
	struct processor_global *pg;
	int                      rc;

	M0_PRE(!processor_init);
	M0_ALLOC_PTR(pg);
	m0_get()->i_moddata[M0_MODULE_PROCESSOR] = pg;
	rc = pg == NULL ? M0_ERR(-ENOMEM) : 0;
	rc = rc ?: processors_summary_get();
	/* Required by processor_getcpu_init(). */
	processor_init = (rc == 0);
	rc = rc ?: processor_getcpu_init();

	if (rc != 0) {
		m0_free(pg);
		m0_get()->i_moddata[M0_MODULE_PROCESSOR] = NULL;
		processor_init = false;
	}
	return M0_RC(rc);
}

M0_INTERNAL void m0_processors_fini()
{
	M0_PRE(processor_init);
	processor_getcpu_fini();
	processor_cache_destroy();
	m0_free0(&m0_get()->i_moddata[M0_MODULE_PROCESSOR]);
	processor_init = false;
}

M0_INTERNAL m0_processor_nr_t m0_processor_nr_max(void)
{
	M0_PRE(processor_init);
	return sys_cpus.pss_max;
}

M0_INTERNAL void m0_processors_possible(struct m0_bitmap *map)
{
	M0_PRE(processor_init);
	M0_PRE(map != NULL);
	processors_m0bitmap_copy(map, &sys_cpus.pss_poss_map);
}

M0_INTERNAL void m0_processors_available(struct m0_bitmap *map)
{
	M0_PRE(processor_init);
	M0_PRE(map != NULL);
	processors_m0bitmap_copy(map, &sys_cpus.pss_avail_map);
}

M0_INTERNAL void m0_processors_online(struct m0_bitmap *map)
{
	M0_PRE(processor_init);
	M0_PRE(map != NULL);
	processors_m0bitmap_copy(map, &sys_cpus.pss_onln_map);
}

M0_INTERNAL int m0_processor_describe(m0_processor_nr_t id,
				      struct m0_processor_descr *pd)
{
	struct processor_node *pinfo;

	M0_PRE(processor_init);
	M0_PRE(pd != NULL);

	m0_list_for_each_entry(&sys_cpus.pss_head, pinfo,
			       struct processor_node, pn_link) {
		if (pinfo->pn_info.pd_id == id) {
			*pd = pinfo->pn_info;
			return 0;
		}/* if - matching CPU id found */
	}/* for - iterate over all the processor nodes */

	return M0_ERR(-EINVAL);
}

M0_INTERNAL m0_processor_nr_t m0_processor_id_get(void)
{
	struct processor_global *pg = m0_get()->i_moddata[M0_MODULE_PROCESSOR];
	unsigned                 cpu;
	int                      rc;

	M0_PRE(pg->pg_getcpu_inited);

	if (pg->pg_getcpu_workaround) {
		rc = syscall(SYS_getcpu, &cpu, NULL, NULL);
		if (rc < 0)
			cpu = M0_PROCESSORS_INVALID_ID;
	} else {
		rc = sched_getcpu();
		cpu = rc < 0 ? M0_PROCESSORS_INVALID_ID : (unsigned)rc;
	}
	if (rc < 0)
		M0_LOG(M0_ERROR, "%s(): rc=%d errno=%d",
		       pg->pg_getcpu_workaround ? "syscall" : "sched_getcpu",
		       rc, errno);
	return cpu;
}

#undef M0_TRACE_SUBSYSTEM

/** @} end of processor group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
