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
 * Original author: Maxim Medved <Max_Medved@xyratex.com>
 * Original creation date: 3-Jun-2013
 */

#include "ut/ut.h"
#include "be/ut/helper.h"

/**
 * BE UT types
 * - usecase - shows typical component use case;
 * - simple  - simple test that checks predefined patterns;
 * - mt      - multithreaded test;
 * - random  - test that uses RNG to test as much use cases as possible.
 */

extern void m0_be_ut_op_usecase(void);
extern void m0_be_ut_op_mt(void);
extern void m0_be_ut_op_set_usecase(void);
extern void m0_be_ut_op_set_tree(void);

extern void m0_be_ut_pool_usecase(void);

extern void m0_be_ut_reg_d_tree(void);
extern void m0_be_ut_regmap_simple(void);
extern void m0_be_ut_regmap_random(void);
extern void m0_be_ut_reg_area_simple(void);
extern void m0_be_ut_reg_area_random(void);
extern void m0_be_ut_reg_area_merge(void);

extern void m0_be_ut_fmt_log_header(void);
extern void m0_be_ut_fmt_cblock(void);
extern void m0_be_ut_fmt_group(void);
extern void m0_be_ut_fmt_group_size_max(void);
extern void m0_be_ut_fmt_group_size_max_rnd(void);

extern void m0_be_ut_io(void);
extern void m0_be_ut_io_sched(void);

extern void m0_be_ut_log_store_create_simple(void);
extern void m0_be_ut_log_store_create_random(void);
extern void m0_be_ut_log_store_io_window(void);
extern void m0_be_ut_log_store_io_discard(void);
extern void m0_be_ut_log_store_io_translate(void);
extern void m0_be_ut_log_store_rbuf(void);

extern void m0_be_ut_log_sched(void);

extern void m0_be_ut_log_discard_usecase(void);
extern void m0_be_ut_log_discard_getput(void);

extern void m0_be_ut_log_user(void);
extern void m0_be_ut_log_api(void);
extern void m0_be_ut_log_header(void);
extern void m0_be_ut_log_unplaced(void);
extern void m0_be_ut_log_multi(void);

extern void m0_be_ut_recovery(void);

extern void m0_be_ut_pd_usecase(void);

extern void m0_be_ut_seg_open_close(void);
extern void m0_be_ut_seg_io(void);
extern void m0_be_ut_seg_multiple(void);
extern void m0_be_ut_seg_large(void);
extern void m0_be_ut_seg_large_multiple(void);

extern void m0_be_ut_group_format(void);

extern void m0_be_ut_mkfs(void);
extern void m0_be_ut_mkfs_multiseg(void);
extern void m0_be_ut_domain(void);
extern void m0_be_ut_domain_is_stob(void);

extern void m0_be_ut_tx_states(void);
extern void m0_be_ut_tx_empty(void);
extern void m0_be_ut_tx_usecase_success(void);
extern void m0_be_ut_tx_usecase_failure(void);
extern void m0_be_ut_tx_capturing(void);
extern void m0_be_ut_tx_single(void);
extern void m0_be_ut_tx_several(void);
extern void m0_be_ut_tx_persistence(void);
extern void m0_be_ut_tx_fast(void);
extern void m0_be_ut_tx_concurrent(void);
extern void m0_be_ut_tx_concurrent_excl(void);
extern void m0_be_ut_tx_force(void);
extern void m0_be_ut_tx_gc(void);
extern void m0_be_ut_tx_payload(void);

extern void m0_be_ut_tx_bulk_usecase(void);
extern void m0_be_ut_tx_bulk_empty(void);
extern void m0_be_ut_tx_bulk_error_reg(void);
extern void m0_be_ut_tx_bulk_error_payload(void);
extern void m0_be_ut_tx_bulk_large_tx(void);
extern void m0_be_ut_tx_bulk_large_payload(void);
extern void m0_be_ut_tx_bulk_large_all(void);
extern void m0_be_ut_tx_bulk_small_tx(void);
extern void m0_be_ut_tx_bulk_medium_tx(void);
extern void m0_be_ut_tx_bulk_medium_tx_multi(void);
extern void m0_be_ut_tx_bulk_medium_cred(void);
extern void m0_be_ut_tx_bulk_large_cred(void);

extern void m0_be_ut_fl(void);

extern void m0_be_ut_alloc_init_fini(void);
extern void m0_be_ut_alloc_create_destroy(void);
extern void m0_be_ut_alloc_multiple(void);
extern void m0_be_ut_alloc_concurrent(void);
extern void m0_be_ut_alloc_oom(void);
extern void m0_be_ut_alloc_info(void);
extern void m0_be_ut_alloc_spare(void);

extern void m0_be_ut_list(void);
extern void m0_be_ut_btree_simple(void);
extern void m0_be_ut_emap(void);
extern void m0_be_ut_seg_dict(void);
extern void m0_be_ut_seg0_test(void);

extern void m0_be_ut_obj_test(void);
extern void m0_be_ut_fmt(void);

extern void m0_be_ut_actrec_test(void);

struct m0_ut_suite be_ut = {
	.ts_name = "be-ut",
	.ts_yaml_config_string = "{ valgrind: { timeout: 3600 },"
				 "  helgrind: { timeout: 3600 },"
				 "  exclude:  ["
				 "    btree,"
				 "    emap,"
				 "    tx-concurrent,"
				 "    tx-concurrent-excl"
				 "  ] }",
	.ts_init = NULL,
	.ts_fini = NULL,
	.ts_tests = {
#ifndef __KERNEL__
		{ "op-usecase",              m0_be_ut_op_usecase              },
		{ "op-mt",                   m0_be_ut_op_mt                   },
		{ "op_set-usecase",          m0_be_ut_op_set_usecase          },
		{ "op_set-tree",             m0_be_ut_op_set_tree             },
		{ "pool-usecase",            m0_be_ut_pool_usecase            },
		{ "reg_d_tree",              m0_be_ut_reg_d_tree              },
// XXX		{ "regmap-simple",           m0_be_ut_regmap_simple           },
// XXX		{ "regmap-random",           m0_be_ut_regmap_random           },
// XXX		{ "reg_area-simple",         m0_be_ut_reg_area_simple         },
		{ "reg_area-random",         m0_be_ut_reg_area_random         },
		{ "reg_area-merge",          m0_be_ut_reg_area_merge          },
		{ "fmt-log_header",          m0_be_ut_fmt_log_header          },
		{ "fmt-cblock",              m0_be_ut_fmt_cblock              },
		{ "fmt-group",               m0_be_ut_fmt_group               },
		{ "fmt-group_size_max",      m0_be_ut_fmt_group_size_max      },
		{ "fmt-group_size_max_rnd",  m0_be_ut_fmt_group_size_max_rnd  },
		{ "io-noop",                 m0_be_ut_io                      },
		{ "io_sched",                m0_be_ut_io_sched                },
		{ "log_store-create_simple", m0_be_ut_log_store_create_simple },
		{ "log_store-create_random", m0_be_ut_log_store_create_random },
		{ "log_store-io_window",     m0_be_ut_log_store_io_window     },
		{ "log_store-io_discard",    m0_be_ut_log_store_io_discard    },
		{ "log_store-io_translate",  m0_be_ut_log_store_io_translate  },
		{ "log_store-rbuf",          m0_be_ut_log_store_rbuf          },
		{ "log_sched-noop",          m0_be_ut_log_sched               },
		{ "log_discard-usecase",     m0_be_ut_log_discard_usecase     },
		{ "log_discard-getput",      m0_be_ut_log_discard_getput      },
		{ "log-user",                m0_be_ut_log_user                },
		{ "log-api",                 m0_be_ut_log_api                 },
		{ "log-header",              m0_be_ut_log_header              },
		{ "log-unplaced",            m0_be_ut_log_unplaced            },
/* XXX this test writes and discards records in random order
		{ "log-multi",               m0_be_ut_log_multi               },
*/
		{ "recovery",                m0_be_ut_recovery                },
		{ "pd-usecase",              m0_be_ut_pd_usecase              },
		{ "seg-open",                m0_be_ut_seg_open_close          },
		{ "seg-io",                  m0_be_ut_seg_io                  },
		{ "seg-multiple",            m0_be_ut_seg_multiple            },
		{ "seg-large",               m0_be_ut_seg_large               },
		{ "seg-large-multiple",      m0_be_ut_seg_large_multiple      },
		{ "group_format",            m0_be_ut_group_format            },
		{ "mkfs",                    m0_be_ut_mkfs                    },
		{ "mkfs-multiseg",           m0_be_ut_mkfs_multiseg           },
		{ "domain",                  m0_be_ut_domain                  },
		{ "domain-is_stob",          m0_be_ut_domain_is_stob          },
		{ "tx-states",               m0_be_ut_tx_states               },
		{ "tx-empty",                m0_be_ut_tx_empty                },
		{ "tx-usecase_success",      m0_be_ut_tx_usecase_success      },
		{ "tx-usecase_failure",      m0_be_ut_tx_usecase_failure      },
		{ "tx-capturing",            m0_be_ut_tx_capturing            },
		{ "tx-gc",                   m0_be_ut_tx_gc                   },
		{ "tx-single",               m0_be_ut_tx_single               },
		{ "tx-several",              m0_be_ut_tx_several              },
		{ "tx-persistence",          m0_be_ut_tx_persistence          },
// XXX		{ "tx-force",                m0_be_ut_tx_force                },
		{ "tx-fast",                 m0_be_ut_tx_fast                 },
		{ "tx-payload",              m0_be_ut_tx_payload              },
		{ "tx-concurrent",           m0_be_ut_tx_concurrent           },
		{ "tx-concurrent-excl",      m0_be_ut_tx_concurrent_excl      },
		{ "tx_bulk-usecase",         m0_be_ut_tx_bulk_usecase         },
		{ "tx_bulk-empty",           m0_be_ut_tx_bulk_empty           },
		{ "tx_bulk-error_reg",       m0_be_ut_tx_bulk_error_reg       },
		{ "tx_bulk-error_payload",   m0_be_ut_tx_bulk_error_payload   },
		{ "tx_bulk-large_tx",        m0_be_ut_tx_bulk_large_tx        },
		{ "tx_bulk-large_payload",   m0_be_ut_tx_bulk_large_payload   },
		{ "tx_bulk-large_all",       m0_be_ut_tx_bulk_large_all       },
		{ "tx_bulk-small_tx",        m0_be_ut_tx_bulk_small_tx        },
		{ "tx_bulk-medium_tx",       m0_be_ut_tx_bulk_medium_tx       },
		{ "tx_bulk-medium_tx_multi", m0_be_ut_tx_bulk_medium_tx_multi },
		{ "tx_bulk-medium_cred",     m0_be_ut_tx_bulk_medium_cred     },
		{ "tx_bulk-large_cred",      m0_be_ut_tx_bulk_large_cred      },
		{ "fl",                      m0_be_ut_fl                      },
		{ "alloc-init",              m0_be_ut_alloc_init_fini         },
		{ "alloc-create",            m0_be_ut_alloc_create_destroy    },
		{ "alloc-multiple",          m0_be_ut_alloc_multiple          },
		{ "alloc-concurrent",        m0_be_ut_alloc_concurrent        },
		{ "alloc-oom",               m0_be_ut_alloc_oom               },
		{ "alloc-info",              m0_be_ut_alloc_info              },
		{ "alloc-spare",             m0_be_ut_alloc_spare             },
		{ "obj",                     m0_be_ut_obj_test                },
		{ "actrec",                  m0_be_ut_actrec_test             },
#endif /* __KERNEL__ */
		{ "list",                    m0_be_ut_list                    },
		{ "btree",                   m0_be_ut_btree_simple            },
		{ "seg_dict",                m0_be_ut_seg_dict                },
#ifndef __KERNEL__
		{ "seg0",                    m0_be_ut_seg0_test               },
#endif /* __KERNEL__ */
		{ "emap",                    m0_be_ut_emap                    },
		{ NULL, NULL }
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
