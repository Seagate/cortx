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
 * Original creation date: 02/24/2011
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <asm/processor.h>
#include <linux/topology.h>
#include <linux/slab.h>

#include "lib/assert.h"
#include "lib/errno.h"
#include "lib/list.h"
#include "lib/memory.h"
#include "lib/processor.h"

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

/**
   @addtogroup processor

   @section proc-kernel Kernel implementation

   This file includes additional data structures and functions for processing
   processors data - for kernel-mode programs.

   This file will also implement Linux kernel-mode processors interfaces.

   @see lib/processor.h

   @{
 */

#ifndef CONFIG_X86_64
#error "Only X86_64 platform is supported"
#endif

enum {
	/** Default L1 value */
	DEFAULT_L1_SZ                       =   32 * 1024,
	/** Default L2 value */
	DEFAULT_L2_SZ                       = 6144 * 1024,

	/** Intel CPUID op-code */
	PROCESSOR_INTEL_CPUID4_OP           = 4,

	PROCESSOR_INTEL_CTYPE_MASK          = 0x1f,
	PROCESSOR_INTEL_CTYPE_NULL          = 0,

	PROCESSOR_INTEL_CLEVEL_MASK         = 0x7,
	PROCESSOR_INTEL_CLEVEL_SHIFT        = 5,

	PROCESSOR_INTEL_CSHARE_MASK         = 0xfff,
	PROCESSOR_INTEL_CSHARE_SHIFT        = 14,

	PROCESSOR_INTEL_LINESZ_MASK         = 0xfff,

	PROCESSOR_INTEL_PARTITION_MASK      = 0x3f,
	PROCESSOR_INTEL_PARTITION_SHIFT     = 12,

	PROCESSOR_INTEL_ASSOCIATIVITY_MASK  = 0x3f,
	PROCESSOR_INTEL_ASSOCIATIVITY_SHIFT = 22,

	PROCESSOR_L1_CACHE                  = 1,
	PROCESSOR_L2_CACHE                  = 2,

	/** AMD CPUID op-code */
	PROCESSOR_AMD_L1_OP                 = 0x80000005,

	PROCESSOR_AMD_CSIZE_SHIFT           = 24,
};

/**
   A node in the linked list describing processor properties. It
   encapsulates 'struct m0_processor_descr'. This will be used to cache
   attributes of x86 processors.
   @see lib/processor.h
 */
struct processor_node {
	/** Linking structure for node */
	struct m0_list_link       pn_link;

	/** Processor descritor strcture */
	struct m0_processor_descr pn_info;
};

/* Global variables */
static bool processor_init = false;
static struct m0_list x86_cpus;

/**
   Convert bitmap from one format to another. Copy cpumask bitmap to m0_bitmap.

   @param dest -> Processors bitmap for Mero programs.
   @param src -> Processors bitmap used by Linux kernel.
   @param bmpsz -> Size of cpumask bitmap (src)

   @pre Assumes memory is alloacted for outbmp and it's initialized.

   @see lib/processor.h
   @see lib/bitmap.h
 */
static void processors_bitmap_copy(struct m0_bitmap *dest,
				   const cpumask_t *src,
				   uint32_t bmpsz)
{
	uint32_t bit;
	bool     val;

	M0_PRE(dest->b_nr >= bmpsz);

	for (bit = 0; bit < bmpsz; ++bit) {
		val = cpumask_test_cpu(bit, src);
		m0_bitmap_set(dest, bit, val);
	}
}

/**
   Fetch NUMA node id for a given processor.

   @param id -> id of the processor for which information is requested.

   @return id of the NUMA node to which the processor belongs.
 */
static inline uint32_t processor_numanodeid_get(m0_processor_nr_t id)
{
	return cpu_to_node(id);
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
   Fetch the default L1 or L2 cache size for a given processor.

   @param id -> id of the processor for which information is requested.
   @param cache_level -> cache level (L1 or L2) for which id is requested.

   @return size of L1 or L2 cache size, in bytes, for the given processor.
 */
static size_t processor_cache_sz_get(m0_processor_nr_t id, uint32_t cache_level)
{
	uint32_t sz = 0;

	switch (cache_level) {
	case PROCESSOR_L1_CACHE:
		sz = DEFAULT_L1_SZ;
		break;
	case PROCESSOR_L2_CACHE:
		sz = DEFAULT_L2_SZ;
		break;
	default:
		break;
	}
	return sz;
}

/**
   Obtain cache level for a given INTEL x86 processor.

   @param eax -> value in eax register for INTEL x86.

   @return cache level of an intel x86 processor.
 */
static inline uint32_t processor_x86cache_level_get(uint32_t eax)
{
	return (eax >> PROCESSOR_INTEL_CLEVEL_SHIFT) &
	        PROCESSOR_INTEL_CLEVEL_MASK;
}

/**
   Obtain number of processors sharing a given cache.

   @param eax -> value in eax register for INTEL x86.

   @return number of intel x86 processors sharing the cache (within
           the core or the physical package).
 */
static inline uint32_t processor_x86cache_shares_get(uint32_t eax)
{
	return (eax >> PROCESSOR_INTEL_CSHARE_SHIFT) &
	        PROCESSOR_INTEL_CSHARE_MASK;
}

/**
   Get the number cache leaves for x86 processor. For Intel use cpuid4
   instruction. For AMD (or other x86 vendors) assume that L2 is supported.

   @param id -> id of the processor for which caches leaves are requested.

   @return number of caches leaves.
 */
static uint32_t processor_x86cache_leaves_get(m0_processor_nr_t id)
{
	uint32_t            eax;
	uint32_t            ebx;
	uint32_t            ecx;
	uint32_t            edx;
	uint32_t            cachetype;
	/*
	 * Assume AMD supports at least L2. For AMD processors this
	 * value is not used later.
	 */
	uint32_t            leaves = 3;
	int                 count = -1;
	struct cpuinfo_x86 *p = &cpu_data(id);

	if (p->x86_vendor == X86_VENDOR_INTEL) {
		do {
			count++;
			cpuid_count(PROCESSOR_INTEL_CPUID4_OP, count,
				    &eax, &ebx, &ecx, &edx);
			cachetype = eax & PROCESSOR_INTEL_CTYPE_MASK;

		} while (cachetype != PROCESSOR_INTEL_CTYPE_NULL);
		leaves = count;
	}

	return leaves;
}

/**
   Fetch L1 or L2 cache id for a given x86 processor.

   @param id -> id of the processor for which information is requested.
   @param cache_level -> cache level (L1 or L2) for which id is requested.
   @param cache_leaves -> Number of cache leaves (levels) for the given
                          processor.

   @return id of L2 cache for the given x86 processor.
 */
static uint32_t processor_x86_cacheid_get(m0_processor_nr_t id,
					  uint32_t cache_level,
					  uint32_t cache_leaves)
{
	uint32_t            cache_id = id;
	uint32_t            eax;
	uint32_t            ebx;
	uint32_t            ecx;
	uint32_t            edx;
	uint32_t            shares;
	uint32_t            phys;
	uint32_t            core;

	bool                l3_present = false;
	bool                cache_shared_at_core = false;

	struct cpuinfo_x86 *p = &cpu_data(id);

	/*
	 * Get L1/L2 cache id for INTEL cpus. If INTEL cpuid level is less
	 * than 4, then use default value.
	 * For AMD cpus, like Linux kernel, assume that L1/L2 is not shared.
	 */
	if (p->x86_vendor == X86_VENDOR_INTEL &&
	    p->cpuid_level >= PROCESSOR_INTEL_CPUID4_OP &&
	    cache_level < cache_leaves) {

		cpuid_count(PROCESSOR_INTEL_CPUID4_OP, cache_level,
			    &eax, &ebx, &ecx, &edx);
		shares = processor_x86cache_shares_get(eax);

		if (shares > 0) {
			/*
			 * Check if L3 is present. We assume that if L3 is
			 * present then L2 is shared at core. Otherwise L2 is
			 * shared at physical package level.
			 */
			if (cache_leaves > 3)
				l3_present = true;
			phys = topology_physical_package_id(id);
			core = topology_core_id(id);
			switch (cache_level) {
			case PROCESSOR_L1_CACHE:
				cache_shared_at_core = true;
				break;
			case PROCESSOR_L2_CACHE:
				if (l3_present)
					cache_shared_at_core = true;
				else
					cache_id = phys;
				break;
			default:
				break;
			}
			if (cache_shared_at_core)
				cache_id = phys << 16 | core;
		}/* cache is shared */

	}/* end of if - Intel processor with CPUID4 support */

	return cache_id;
}

/**
   A function to fetch cache size for an AMD x86 processor.

   @param id -> id of the processor for which information is requested.
   @param cache_level -> cache level (L1 or L2) for which id is requested.

   @return size of cache (in bytes) for the given AMD x86 processor.
 */
static uint32_t processor_amd_cache_sz_get(m0_processor_nr_t id,
					   uint32_t cache_level)
{
	uint32_t            eax;
	uint32_t            ebx;
	uint32_t            ecx;
	uint32_t            l1;
	uint32_t            sz = 0;

	struct cpuinfo_x86 *p;

	switch (cache_level) {
	case PROCESSOR_L1_CACHE:
		cpuid(PROCESSOR_AMD_L1_OP, &eax, &ebx, &ecx, &l1);
		sz = (l1 >> PROCESSOR_AMD_CSIZE_SHIFT) * 1024;
		break;
	case PROCESSOR_L2_CACHE:
		p = &cpu_data(id);
		sz = p->x86_cache_size;
		break;
	default:
		break;
	}

	return sz;
}

/**
   A generic function to fetch cache size for an INTEL x86 processor.
   If Intel CPU does not support CPUID4, use default values.

   @param id -> id of the processor for which information is requested.
   @param cache_level -> cache level (L1 or L2) for which id is requested.

   @return size of cache (in bytes) for the given INTEL x86 processor.
 */
static uint32_t processor_intel_cache_sz_get(m0_processor_nr_t id,
					     uint32_t cache_level)
{
	uint32_t            eax;
	uint32_t            ebx;
	uint32_t            ecx;
	uint32_t            edx;
	uint32_t            sets;
	uint32_t            linesz;
	uint32_t            partition;
	uint32_t            asso;
	uint32_t            level;
	uint32_t            sz = 0;

	bool                use_defaults = true;
	struct cpuinfo_x86 *p = &cpu_data(id);

	if (p->cpuid_level >= PROCESSOR_INTEL_CPUID4_OP) {
		cpuid_count(PROCESSOR_INTEL_CPUID4_OP, cache_level,
			    &eax, &ebx, &ecx, &edx);
		level = processor_x86cache_level_get(eax);
		if (level == cache_level) {
			linesz = ebx & PROCESSOR_INTEL_LINESZ_MASK;
			partition = (ebx >> PROCESSOR_INTEL_PARTITION_SHIFT)
				    & PROCESSOR_INTEL_PARTITION_MASK;
			asso = (ebx >> PROCESSOR_INTEL_ASSOCIATIVITY_SHIFT)
				& PROCESSOR_INTEL_ASSOCIATIVITY_MASK;
			sets = ecx;
			sz = (linesz+1) * (sets+1) * (partition+1) * (asso+1);
			use_defaults = false;
		}
	}

	if (use_defaults) {
		switch (cache_level) {
		case PROCESSOR_L1_CACHE:
			sz = DEFAULT_L1_SZ;
			break;
		case PROCESSOR_L2_CACHE:
			sz = DEFAULT_L2_SZ;
			break;
		default:
			break;
		}
	}

	return sz;
}

/**
   Fetch L1 or L2 cache size for a given x86 processor.

   @param id -> id of the processor for which information is requested.
   @param cache_level -> cache level for which information is requested.

   @return size of L1 or L2 cache (in bytes) for the given x86 processor.
 */
static uint32_t processor_x86_cache_sz_get(m0_processor_nr_t id,
					   uint32_t cache_level)
{
	uint32_t            sz;
	struct cpuinfo_x86 *p = &cpu_data(id);

	switch (p->x86_vendor) {
	case X86_VENDOR_AMD:
		/*
		 * Get L1/L2 cache size for AMD processors.
		 */
		sz = processor_amd_cache_sz_get(id, cache_level);
		break;
	case X86_VENDOR_INTEL:
		/*
		 * Get L1/L2 cache size for INTEL processors.
		 */
		sz = processor_intel_cache_sz_get(id, cache_level);
		break;
	default:
		/*
		 * Use default function for all other x86 vendors.
		 */
		sz = processor_cache_sz_get(id, cache_level);
		break;
	}/* end of switch - vendor name */

	return sz;
}

/**
   Fetch attributes for the x86 processor.

   @param arg -> argument passed to this function, a struct processor_node.

   @see processor_x86cache_create
   @see smp_call_function_single (Linux kernel)
 */
static void processor_x86_attrs_get(void *arg)
{
	uint32_t               c_leaves;
	m0_processor_nr_t      cpu   = smp_processor_id();
	struct processor_node *pinfo = (struct processor_node *) arg;

	/*
	 * Fetch other generic properties.
	 */
	pinfo->pn_info.pd_id = cpu;
	pinfo->pn_info.pd_numa_node = processor_numanodeid_get(cpu);
	pinfo->pn_info.pd_pipeline = processor_pipelineid_get(cpu);

	c_leaves = processor_x86cache_leaves_get(cpu);
	/*
	 * Now fetch the x86 cache information.
	 */
	pinfo->pn_info.pd_l1 =
	    processor_x86_cacheid_get(cpu, PROCESSOR_L1_CACHE, c_leaves);
	pinfo->pn_info.pd_l2 =
	    processor_x86_cacheid_get(cpu, PROCESSOR_L2_CACHE, c_leaves);

	pinfo->pn_info.pd_l1_sz =
	    processor_x86_cache_sz_get(cpu, PROCESSOR_L1_CACHE);
	pinfo->pn_info.pd_l2_sz =
	    processor_x86_cache_sz_get(cpu, PROCESSOR_L2_CACHE);
}

/**
   Obtain information on the processor with a given id.
   @param id -> id of the processor for which information is requested.
   @param pd -> processor descripto structure. Memory for this should be
                allocated by the calling function. Interface does not allocate
                memory.

   @retval 0 if processor information is found
   @retval -EINVAL if processor information is not found

   @pre Memory must be allocated for pd. Interface does not allocate memory.
   @pre m0_processors_init() must be called before calling this function.

   @see m0_processor_describe
 */
static int processor_x86_info_get(m0_processor_nr_t id,
				  struct m0_processor_descr *pd)
{
	struct processor_node *pinfo;

	M0_PRE(pd != NULL);
	M0_PRE(processor_init);

	m0_list_for_each_entry(&x86_cpus, pinfo, struct processor_node,
			       pn_link) {
		if (pinfo->pn_info.pd_id == id) {
			*pd = pinfo->pn_info;
			return 0;
		}/* if - matching CPU id found */

	}/* for - iterate over all the processor nodes */

	return M0_ERR(-EINVAL);
}

/**
   Cache clean-up.

   @see m0_processors_fini
   @see m0_list_fini
 */
static void processor_x86cache_destroy(void)
{
	struct m0_list_link   *node;
	struct processor_node *pinfo;

	/*
	 * Remove all the processor nodes.
	 */
	node = x86_cpus.l_head;
	while((struct m0_list *)node != &x86_cpus) {
		pinfo = m0_list_entry(node, struct processor_node, pn_link);
		m0_list_del(&pinfo->pn_link);
		m0_free(pinfo);
		node = x86_cpus.l_head;
	}
	m0_list_fini(&x86_cpus);
}

/**
   Create cache for x86 processors. We have support for Intel and AMD.

   This is a blocking call.

   @see m0_processors_init
   @see smp_call_function_single (Linux kernel)
 */
static int processor_x86cache_create(void)
{
	uint32_t               cpu;
	struct processor_node *pinfo;

	m0_list_init(&x86_cpus);

	/*
	 * Using online CPU mask get details of each processor.
	 * Unless CPU is online, we cannot execute on it.
	 */
	for_each_online_cpu(cpu) {
		M0_ALLOC_PTR(pinfo);
		if (pinfo == NULL) {
			processor_x86cache_destroy();
			return M0_ERR(-ENOMEM);
		}

		/*
		 * We may not be running on the same processor for which cache
		 * info is needed. Hence run the function on the requested
		 * processor. smp_call... has all the optimization necessary.
		 */
		smp_call_function_single(cpu, processor_x86_attrs_get,
					 (void *)pinfo, true);
		m0_list_add(&x86_cpus, &pinfo->pn_link);
	}/* for - scan all the online processors */

	if (m0_list_is_empty(&x86_cpus)) {
		m0_list_fini(&x86_cpus);
		return M0_ERR(-ENODATA);
	}

	return 0;
}

/* ---- Processor Interfaces ---- */

M0_INTERNAL int m0_processors_init()
{
	int rc;

	M0_PRE(!processor_init);
	rc = processor_x86cache_create();
	processor_init = (rc == 0);
	return M0_RC(rc);
}

M0_INTERNAL void m0_processors_fini()
{
	M0_PRE(processor_init);
	processor_x86cache_destroy();
	processor_init = false;
}

M0_INTERNAL m0_processor_nr_t m0_processor_nr_max(void)
{
	return NR_CPUS;
}

M0_INTERNAL void m0_processors_possible(struct m0_bitmap *map)
{
	processors_bitmap_copy(map, cpu_possible_mask, nr_cpu_ids);
}

M0_INTERNAL void m0_processors_available(struct m0_bitmap *map)
{
	processors_bitmap_copy(map, cpu_present_mask, nr_cpu_ids);
}

M0_INTERNAL void m0_processors_online(struct m0_bitmap *map)
{
	processors_bitmap_copy(map, cpu_online_mask, nr_cpu_ids);
}

M0_INTERNAL int m0_processor_describe(m0_processor_nr_t id,
				      struct m0_processor_descr *pd)
{
	M0_PRE(pd != NULL);
	if (id >= nr_cpu_ids)
		return M0_ERR(-EINVAL);

	return processor_x86_info_get(id, pd);
}

M0_INTERNAL m0_processor_nr_t m0_processor_id_get(void)
{
	return smp_processor_id();
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
