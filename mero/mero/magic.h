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
 * Original author: Manish Honap <manish_honap@xyratex.com>
 * Original creation date: 09/07/2012
 */

#pragma once

#ifndef __MERO_MERO_MAGIC_H__
#define __MERO_MERO_MAGIC_H__

/*
 * Magic values used to tag Mero structures.
 * Create magic numbers by referring to
 *     http://www.nsftools.com/tips/HexWords.htm
 */

enum m0_magic_satchel {
/* ADDB2 */
	/** m0_addb2_mach::ma_magix (acceeded deal) */
	M0_ADDB2_MACH_MAGIC        = 0x33acceededdea177,
	/** addb2/addb2.c, mach tlist head (decoded Coala) */
	M0_ADDB2_MACH_HEAD_MAGIC   = 0x33dec0dedc0a1a77,
	/** addb2/addb2.c, buffer::b_magix (callable loeb) */
	M0_ADDB2_BUF_MAGIC         = 0x33ca11ab1e10eb77,
	/** addb2/addb2.c, buf_tlist head magic (libel offload) */
	M0_ADDB2_BUF_HEAD_MAGIC    = 0x3311be10ff10ad77,
	/** addb2/addb2.c, m0_addb2_sensor::s_magix (diesel doodle) */
	M0_ADDB2_SENSOR_MAGIC      = 0x33d1e5e1d0dd1e77,
	/** addb2/addb2.c, sensor_tlist head magic (filled Balzac) */
	M0_ADDB2_SENSOR_HEAD_MAGIC = 0x33f111edba15ac77,
	/** addb2/consumer.c, m0_addb2_philter::ph_magix (Belial biased) */
	M0_ADDB2_PHILTER_MAGIC      = 0x33be11a1b1a5ed77,
	/** addb2/consumer.c, philter_tlist head magic (beblooded fee) */
	M0_ADDB2_PHILTER_HEAD_MAGIC = 0x33beb100dedfee77,
	/** addb2/consumer.c, m0_addb2_callback::ca_magix (zealless odel) */
	M0_ADDB2_CALLBACK_MAGIC      = 0x332ea11e550de177,
	/** addb2/consumer.c, callback_tlist head magic (coccidioides) */
	M0_ADDB2_CALLBACK_HEAD_MAGIC = 0x33c0cc1d101de577,

	/* befell Daedal */
	M0_ADDB2_FRAME_HEADER_MAGIX  = 0x33befe11Daeda177,
	/* feodal feoffee [sic] */
	M0_ADDB2_TRACE_MAGIC         = 0x33fe0da1fe0ffe77,
	/* Adolfo decade */
	M0_ADDB2_TRACE_HEAD_MAGIC    = 0x33ad01f0decade77,
	/* Lebedeff obol */
	M0_ADDB2_FRAME_MAGIC         = 0x331ebedeff0b0177,
	/* fleeced Ebola */
	M0_ADDB2_FRAME_HEAD_MAGIC    = 0x33f1eecedeb01a77,
	/* I, coldblooded */
	M0_ADDB2_SOURCE_MAGIC        = 0x331c01db100ded77,
	/* Leo falabella */
	M0_ADDB2_SOURCE_HEAD_MAGIC   = 0x331e0fa1abe11a77,

/* balloc */
	/* m0_balloc_super_block::bsb_magic (blessed baloc) */
	M0_BALLOC_SB_MAGIC = 0x33b1e55edba10c77,

/* BE */
	/* m0_be_tx::t_magic (I feel good) */
	M0_BE_TX_MAGIC = 0x331fee190000d177,

	/* m0_be_tx_engine::te_txs[] (lifeless gel)  */
	M0_BE_TX_ENGINE_MAGIC = 0x3311fe1e556e1277,

	/* m0_be_tx_group::tg_txs (codified bee)  */
	M0_BE_TX_GROUP_MAGIC = 0x33c0d1f1edbee377,

	/* be_alloc_chunk::bac_magic0 (eloise laiose) */
	M0_BE_ALLOC_MAGIC0 = 0xe1015e1a105e,

	/* be_alloc_chunk::bac_magic1 (codices bad id) */
	M0_BE_ALLOC_MAGIC1 = 0xc0d1ce5bad1d,

	/* m0_be_allocator_header::bah_chunks (alcaaba solod) */
	M0_BE_ALLOC_ALL_MAGIC = 0xa1caaba5010d,

	/* be_alloc_chunk::bac_magic (official feis) */
	M0_BE_ALLOC_ALL_LINK_MAGIC = 0x0ff1c1a1fe15,

	/* m0_be_fl::bfl_free[index]::bfs_list (cascadia aloe) */
	M0_BE_ALLOC_FREE_MAGIC = 0xca5cad1aa10e,

	/* be_alloc_chunk::bac_magic_free (edifice faded) */
	M0_BE_ALLOC_FREE_LINK_MAGIC = 0xed1f1cefaded,

	/* m0_be_0type::b0_magic (bee fires stig) */
	M0_BE_0TYPE_MAGIC = 0x33beef17e5519177,

	/* m0_be_seg::bs_magic (bee seg faded) */
	M0_BE_SEG_MAGIC = 0x33bee5e9faded177,

	/* seg_dict_keyval::dkv_magic (be seg lillie) */
	M0_BE_SEG_DICT_MAGIC = 0x33be5e911111e077,

	/* be/seg_dict.c::seg_dict_be_list_d (be head lillie) */
	M0_BE_SEG_DICT_HEAD_MAGIC = 0x33be4ead11111e77,

	/* m0_be_op::bo_children (befooled fifo) */
	M0_BE_OP_SET_MAGIC = 0x33bef001edf1f077,

	/* m0_be_op::bo_set_link_magic (offloaded bel) */
	M0_BE_OP_SET_LINK_MAGIC = 0x330ff10adedbe177,

	/* m0_be_pool_item::bpli_link (add be class) */
	M0_BE_POOL_MAGIC = 0x33addbec1a5577,

	/* be/pool.c::be_pool_tl (del be head) */
	M0_BE_POOL_HEAD_MAGIC = 0x33de1be4ead77,

	/* m0_be_pool_queue_item::bplq_link (be fifo all) */
	M0_BE_POOL_QUEUE_MAGIC = 0x33bef1f0a1177,

	/* be/pool.c::be_pool_q_tl (be fifo head) */
	M0_BE_POOL_QUEUE_HEAD_MAGIC = 0x33bef1f04ead77,

	/* m0_be_log_record::lgr_linkage (be lossless) */
	M0_BE_LOG_RECORD_MAGIC = 0x33be10551e5577,

	/* be/log.c::record_tl (be loss head) */
	M0_BE_LOG_RECORD_HEAD_MAGIC = 0x33be10554ead77,

	/* m0_be_log_discard::lds_start_q (feed flies) */
	M0_BE_LOG_DISCARD_MAGIC = 0x33feedf11e577,

	/* be/log_discard.c::ld_start_tl (be is cool) */
	M0_BE_LOG_DISCARD_HEAD_MAGIC = 0x33be15c00177,

	/* m0_be_log_discard::lds_item_pool (flood blob) */
	M0_BE_LOG_DISCARD_POOL_MAGIC = 0x33f100db10b77,

	/* m0_be_log_record_iter::lri_linkage (be safe heal) */
	M0_BE_RECOVERY_MAGIC = 0x33be5afe4ea177,

	/* be/recovery.c::log_record_iter_tl (be safe head) */
	M0_BE_RECOVERY_HEAD_MAGIC = 0x33be5afe4ead77,

	/* m0_be_io::bio_sched_magic (bad be io base) */
	M0_BE_IO_SCHED_MAGIC = 0x33badbe10ba5e77,

	/* m0_be_io_sched::bis_ios (bad be io head) */
	M0_BE_IO_SCHED_HEAD_MAGIC = 0x33badbe104ead77,

	/* m0_be_pd::bpd_io_pool (be dead cache) */
	M0_BE_PD_IO_MAGIC = 0x33bedeadcac4e77,

	/* m0_be_active_record_domain::ard_list (be blood flood) */
	M0_BE_ACT_REC_DOM_MAGIC = 0x33beb100df100d7,

	/* m0_be_active_record_domain_subsystem::rds_list (be glad blood) */
	M0_BE_ACT_REC_DOM_SUB_MAGIC = 0x33be91adb100d77,

/* m0t1fs */
	/* m0t1fs_sb::s_magic (cozie filesis) */
	M0_T1FS_SUPER_MAGIC = 0x33c021ef11e51577,

	/* m0t1fs_inode_bob::bt_magix (idolised idol) */
	M0_T1FS_INODE_MAGIC = 0x331d0115ed1d0177,

	/* m0t1fs_dir_ent::de_magic (looseleaf oil) */
	M0_T1FS_DIRENT_MAGIC = 0x331005e1eaf01177,

	/* dir_ents_tl::td_head_magic (lidless slide) */
	M0_T1FS_DIRENT_HEAD_MAGIC = 0x3311d1e55511de77,

	/* rw_desc::rd_magic (alfalfa alibi) */
	M0_T1FS_RW_DESC_MAGIC = 0x33a1fa1faa11b177,

	/* rwd_tl::td_head_magic (assail assoil) */
	M0_T1FS_RW_DESC_HEAD_MAGIC = 0x33a55a11a5501177,

	/* m0t1fs_buf::cb_magic (balled azalia) */
	M0_T1FS_BUF_MAGIC = 0x33ba11eda2a11a77,

	/* bufs_tl::td_head_magic (bedded celiac) */
	M0_T1FS_BUF_HEAD_MAGIC = 0x33beddedce11ac77,

        /* io_request::ir_magic (fearsome acts) */
        M0_T1FS_IOREQ_MAGIC  = 0x33fea2503eac1577,

        /* nw_xfer_request::nxr_magic (coffee arabic) */
        M0_T1FS_NWREQ_MAGIC  = 0x33c0ffeea2ab1c77,

        /* target_ioreq::ti_magic (falafel bread) */
        M0_T1FS_TIOREQ_MAGIC = 0x33fa1afe1b2ead77,

        /* io_req_fop::irf_magic (desirability) */
        M0_T1FS_IOFOP_MAGIC  = 0x33de512ab1111777,

        /* data_buf::db_magic (fire incoming) */
        M0_T1FS_DTBUF_MAGIC  = 0x33f12e19c0319977,

        /* pargrp_iomap::pi_magic (incandescent) */
        M0_T1FS_PGROUP_MAGIC = 0x3319ca9de5ce9177,

	/* hashbucket::hb_tioreqs::td_head_magic (affable close) */
	M0_T1FS_TLIST_HEAD_MAGIC = 0x33affab1ec105e77,

	/* m0t1fs_fsync_fop_wrapper (sliced zodiac) */
	M0_T1FS_FFW_TLIST_MAGIC1 = 0x33511ced20d1ac77,

	/* m0t1fs_fsync_fop_wrapper (idealised dab) */
	M0_T1FS_FFW_TLIST_MAGIC2 = 0x331dea115eddab77,

	/* m0t1fs_inode::ci_service_pending_txid_list (baseball zeal) */
	M0_T1FS_INODE_PTI_MAGIC1 = 0x33ba5eba112ea177,

	/* m0t1fs_inode::ci_service_pending_txid_list (so sidesaddle) */
	M0_T1FS_INODE_PTI_MAGIC2 = 0x335051de5add1e77,

	/* m0t1fs_csb::csb_inode_list::tl_magic (biblical bell) */
	M0_T1FS_INODE_HEAD_MAGIC = 0x33b1b11ca1be1177,


/* Configuration */
	/* m0_conf_cache::ca_registry::t_magic (fabled feodal) */
	M0_CONF_CACHE_MAGIC = 0x33fab1edfe0da177,

	/* m0_conf_obj::co_gen_magic (selfless cell) */
	M0_CONF_OBJ_MAGIC = 0x335e1f1e55ce1177,

	/* m0_conf_dir::cd_obj.co_con_magic (old calaboose) */
	M0_CONF_DIR_MAGIC = 0x3301dca1ab005e77,

	/* m0_conf_root::crt_obj.co_con_magic (addable libel) */
	M0_CONF_ROOT_MAGIC = 0x33addab1e11be177,

	/* m0_conf_profile::cp_obj.co_con_magic (closable seal) */
	M0_CONF_PROFILE_MAGIC = 0x33c105ab1e5ea177,

	/* m0_conf_pool::cp_obj.co_con_magic (cocobolo coco) */
	M0_CONF_POOL_MAGIC = 0x33c0c0b010c0c077,

	/* m0t1fs_pools_tl::td_head_magic (seize sicilia) */
	M0_POOLS_HEAD_MAGIC = 0x335e12e51c111a77,

	/* m0_conf_pver::cpv_obj.co_con_magic (besides bezel) */
	M0_CONF_PVER_MAGIC = 0x33be51de5be2e177,

	/* m0_conf_objv::cv_obj.co_con_magic (soleless solo) */
	M0_CONF_OBJV_MAGIC = 0x33501e1e55501077,

	/* m0_conf_fdmi_filter::cs_obj.co_con_magic (biased access) */
	M0_CONF_FDMI_FILTER_MAGIC = 0x33b1a5edacce5577,

	/* m0_conf_fdmi_flt_grp::cs_obj.co_con_magic (biblical babe) */
	M0_CONF_FDMI_FLT_GRP_MAGIC = 0x33b1b11ca1babe78,

	/* m0_conf_node::cn_obj.co_con_magic (colossal dosa) */
	M0_CONF_NODE_MAGIC = 0x33c01055a1d05a77,

	/* m0_conf_process::cp_obj.co_con_magic (cococool dood) */
	M0_CONF_PROCESS_MAGIC = 0x33c0c0c001d00d77,

	/* m0_conf_service::cs_obj.co_con_magic (biased locale) */
	M0_CONF_SERVICE_MAGIC = 0x33b1a5ed10ca1e77,

	/* m0_conf_site::ct_obj.co_con_magic (colossal deed) */
	M0_CONF_SITE_MAGIC = 0x33c01055a1deed77,

	/* m0_conf_rack::cr_obj.co_con_magic (cocozelle cod) */
	M0_CONF_RACK_MAGIC = 0x33c0c02e11ec0d77,

	/* m0_conf_enclosure::ce_obj.co_con_magic (bedside befoo) */
	M0_CONF_ENCLOSURE_MAGIC = 0x33bed51debef0077,

	/* m0_conf_controller::cc_obj.co_con_magic (biased bellis) */
	M0_CONF_CONTROLLER_MAGIC = 0x33b1a5edbe111577,

	/* m0_conf_sdev::sd_obj.co_con_magic (allseed salad) */
	M0_CONF_SDEV_MAGIC = 0x33a115eed5a1ad77,

	/* m0_conf_drive::ck_obj.co_con_magic (billfold bise) */
	M0_CONF_DRIVE_MAGIC = 0x3b111f01db15e77,

	/* m0_conf_partition::pa_obj.co_con_magic (bacca is aloof)
	 * Let's hope we won't be sued by Symantec for this name. */
	M0_CONF_PARTITION_MAGIC = 0x33bacca15a100f77,

	/* m0_confc::cc_magic (zodiac doable) */
	M0_CONFC_MAGIC = 0x3320d1acd0ab1e77,

	/* m0_confc_ctx::fc_magic (ablaze filial) */
	M0_CONFC_CTX_MAGIC = 0x33ab1a2ef111a177,

	/* m0_confd::c_magic (isolable lasi) */
	M0_CONFD_MAGIC = 0x331501ab1e1a5177,

	/* m0_confc_link::rl_magic (bald billed de) */
	M0_RCONFC_LINK_MAGIC = 0x33ba1db111edde77,

	/* m0_confc_link::rl_magic (aeolic deface) */
	M0_RCONFC_HERD_HEAD_MAGIC = 0x33ae011cdeface77,

	/* m0_confc_link::rl_magic (addible faced) */
	M0_RCONFC_ACTIVE_HEAD_MAGIC = 0x33add1b1efaced77,

	/* rconfc_ha_link::rhl_ha (ablaze boozed) */
	M0_RCONFC_HA_LINK_MAGIC = 0x33ab1a2eb002ed77,

	/* rconfc_ha_link::rhl_magic (disable sable) */
	M0_RCONFC_HA_LINK_HEAD_MAGIC = 0x33d12ab1e2ab1e77,

/* Mero Setup */
	/* cs_buffer_pool::cs_bp_magic (felicia feliz) */
	M0_CS_BUFFER_POOL_MAGIC = 0x33fe11c1afe11277,

	/* cs_buffer_pools_tl::td_head_magic (edible doodle) */
	M0_CS_BUFFER_POOL_HEAD_MAGIC = 0x33ed1b1ed00d1e77,

	/* cs_reqh_context::rc_magix (cooled coffee) */
	M0_CS_REQH_CTX_MAGIC = 0x33c001edc0ffee77,

	/* cs_endpoint_and_xprt::ex_magix (adios alibaba) */
	M0_CS_ENDPOINT_AND_XPRT_MAGIC = 0x33ad105a11baba77,

	/* cs_eps_tl::td_head_magic (felic felicis) */
	M0_CS_EPS_HEAD_MAGIC = 0x33fe11cfe11c1577,

	/* ndom_tl::td_head_magic (baffled basis) */
	M0_CS_NET_DOMAIN_HEAD_MAGIC = 0x33baff1edba51577,

	/* m0_mero::cc_magic (sealless dacs) */
	M0_CS_MERO_MAGIC = 0x335ea11e55dac577,

/* Copy machine */
	/* cmtypes_tl::td_head_magic (dacefacebace) */
	CM_TYPE_HEAD_MAGIX = 0x33DACEFACEBACE77,

	/* m0_cm_type::ct_magix (badedabadebe) */
	CM_TYPE_LINK_MAGIX = 0x33BADEDABADEBE77,

	/* cm_ag_tl::td_head_magic (deafbeefdead) */
	CM_AG_HEAD_MAGIX = 0x33DEAFBEEFDEAD77,

	/* m0_cm_aggr_group::cag_magic (feedbeefdeed) */
	CM_AG_LINK_MAGIX = 0x33FEEDBEEFDEED77,

/* Copy packet */
	/* m0_cm_cp::cp_bob (ecobabble ace) */
	CM_CP_MAGIX = 0x33ec0babb1eace77,

	/* m0_cm_cp::c_buffers (deadfoodbaad) */
	CM_CP_DATA_BUF_HEAD_MAGIX = 0x33DEADF00DBAAD77,

	/* px_pending_cps::td_head_magic () */
	CM_PROXY_CP_HEAD_MAGIX = 0x33C001F001B00177,

/* Copy machine proxy */
        /* m0_cm_proxy_tl::td_head_magic (caadbaadfaad) */
	CM_PROXY_HEAD_MAGIC = 0x33CAADBAADFAAD77,

	/* m0_cm_proxy::px_magic (C001D00DF00D) */
	CM_PROXY_LINK_MAGIC = 0x33C001D00DF00D77,

/* desim */
	/* client_write_ext::cwe_magic (abasic access) */
	M0_DESIM_CLIENT_WRITE_EXT_MAGIC = 0x33aba51cacce5577,

	/* cl_tl::td_head_magic (abscessed ace) */
	M0_DESIM_CLIENT_WRITE_EXT_HEAD_MAGIC = 0x33ab5ce55edace77,

	/* cnt::c_magic (al azollaceae) */
	M0_DESIM_CNT_MAGIC = 0x33a1a2011aceae77,

	/* cnts_tl::td_head_magic (biased balzac) */
	M0_DESIM_CNTS_HEAD_MAGIC = 0x33b1a5edba12ac77,

	/* io_req::ir_magic (biblical bias) */
	M0_DESIM_IO_REQ_MAGIC = 0x33b1b11ca1b1a577,

	/* req_tl::td_head_magic (bifolded case) */
	M0_DESIM_IO_REQ_HEAD_MAGIC = 0x33b1f01dedca5e77,

	/* net_rpc::nr_magic (das classless) */
	M0_DESIM_NET_RPC_MAGIC = 0x33da5c1a551e5577,

	/* rpc_tl::td_head_magic (delible diazo) */
	M0_DESIM_NET_RPC_HEAD_MAGIC = 0x33de11b1ed1a2077,

	/* sim_callout::sc_magic (escalade fall) */
	M0_DESIM_SIM_CALLOUT_MAGIC = 0x33e5ca1adefa1177,

	/* ca_tl::td_head_magic (leaded lescol) */
	M0_DESIM_SIM_CALLOUT_HEAD_MAGIC = 0x331eaded1e5c0177,

	/* sim_callout::sc_magic (odessa saddle) */
	M0_DESIM_SIM_THREAD_MAGIC = 0x330de55a5add1e77,

	/* ca_tl::td_head_magic (scaffold sale) */
	M0_DESIM_SIM_THREAD_HEAD_MAGIC = 0x335caff01d5a1e77,

/* DTM */
	/* m0_dtm_up::du_magix (blessed feoff) */
	M0_DTM_UP_MAGIX = 0x33b1e55edfe0ff77,
	/* m0_dtm_hi::hi_ups::t_magic (biblical food) */
	M0_DTM_HI_MAGIX = 0x33b1b11ca1f00d77,
	/* m0_dtm_op::op_ups::t_magic (feeble fiddle) */
	M0_DTM_OP_MAGIX = 0x33feeb1ef1dd1e77,
	/* m0_dtm_catalogue::d_cat[]::ca_el::t_magic (accessible 42) */
	M0_DTM_CAT_MAGIX = 0x33acce551b1e4277,
	/* m0_dtm::d_excited::t_magic (flooded baboo) */
	M0_DTM_EXC_MAGIX = 0x33f100dedbab0077,

/* Failure Domains */
	/* m0_fd_perm_cache::fpc_magic (fascia doodia) */
	M0_FD_PRMCACHE_MAGIC = 0x33FA5C1AD00D1A77,
	/* m0_fd_tree::ft_perm_cache::td_head_magic (ecesis filial)*/
	M0_FD_PRMCACHE_HEAD_MAGIC = 0x33ECE515F111A177,

/* Fault Injection */
	/* fi_dynamic_id::fdi_magic (diabolic dill) */
	M0_FI_DYNAMIC_ID_MAGIC = 0x33d1ab011cd11177,

	/* fi_dynamic_id_tl::td_head_magic (decoded diode) */
	M0_FI_DYNAMIC_ID_HEAD_MAGIC = 0x33dec0dedd10de77,

/* FOP */
	/* m0_fop_type::ft_magix (balboa saddle) */
	M0_FOP_TYPE_MAGIC = 0x33ba1b0a5add1e77,

	/* fop_types_list::t_magic (baffle bacili) */
	M0_FOP_TYPE_HEAD_MAGIC = 0x33baff1ebac11177,

	/* m0_fom::fo_magic (leadless less) */
	M0_FOM_MAGIC = 0x331ead1e551e5577,

	/* m0_fom_locality::fl_runq::td_head_magic (alas albizzia) */
	M0_FOM_RUNQ_MAGIC = 0x33a1a5a1b1221a77,

	/* m0_fom_locality::fl_wail::td_head_magic (baseless bole) */
	M0_FOM_WAIL_MAGIC = 0x33ba5e1e55501e77,

	/* m0_fom_thread::lt_magix (falsifiable C) */
	M0_FOM_THREAD_MAGIC = 0x33fa151f1ab1ec77,

	/* thr_tl::td_head_magic (declassified) */
	M0_FOM_THREAD_HEAD_MAGIC = 0x33dec1a551f1ed77,

	/* m0_long_lock_link::lll_magix (idealised ice) */
	M0_FOM_LL_LINK_MAGIC = 0x331dea115ed1ce77,

	/* m0_long_lock::l_magix (blessed boss) */
	M0_FOM_LL_MAGIC = 0x330b1e55edb05577,

/* FOL */
	/* m0_fol_rec_header::rh_magic (facade decile) */
	M0_FOL_REC_MAGIC = 0x33facadedec11e77,
	/* m0_fol_frag_header:rph_magic (baseball aced) */
	M0_FOL_FRAG_MAGIC = 0x33ba5eba11aced77,
	/* m0_fol_frag:rp_link (ceaseless deb) */
	M0_FOL_FRAG_LINK_MAGIC = 0x33cea5e1e55deb77,
	/* m0_fol_frag:rp_magic (bloodied bozo) */
	M0_FOL_FRAG_HEAD_MAGIC = 0x33b100d1edb02077,

/* HA */
	/* m0_ha_epoch_monitor::ham_magic (bead Adelaide) */
	M0_HA_EPOCH_MONITOR_MAGIC = 0x33beadade1a1de77,
	/* m0_ha_domain::hdo_monitors::t_magic (beef official) */
	M0_HA_DOMAIN_MAGIC = 0x33beef0ff1c1a177,
	/* ha_client_ctx::hx_magic (booze defaced) */
	M0_HA_CLIENT_MAGIC = 0x33b002edefaced77,
	/* ha_client_ctx::hx_magic (cliff dazzled) */
	M0_HA_CLIENT_HEAD_MAGIC = 0x33c11ffda221ed77,
	/* m0_ha_msg_queue::mq_queue (ha fifo base) */
	M0_HA_MSG_QUEUE_HEAD_MAGIC = 0x334af1f0ba5e0077,
	/* m0_ha_msg_qitem::hmq_magic (ha fifo head) */
	M0_HA_MSG_QITEM_MAGIC = 0x334af1f04ead0077,
	/* m0_halon_interface_internal::hii_magix (deles felizio) */
	M0_HALON_INTERFACE_MAGIC = 0x33de1e5fe1121077,
	/* m0_ha_link::hln_service_magic (sessed loofas) */
	M0_HA_LINK_SERVICE_LINK_MAGIC = 0x335e55ed100fa577,
	/* ha_link_service::hls_links (boldfaced els) */
	M0_HA_LINK_SERVICE_HEAD_MAGIC = 0x33b01dfacede1577,
	/* ha_link_service::hls_magic (silicifies os) */
	M0_HA_LINK_SERVICE_MAGIC = 0x335111c1f1e50577,

/* ioservice */
	/* m0_tmp_stob_io_descr::siod_linkage (zealos obsses) */
	M0_STOB_IO_DESC_LINK_MAGIC = 0x332ea1050b55e577,

	/* stobio_tl::td_head_magic (official ball) */
	M0_STOB_IO_DESC_HEAD_MAGIC = 0x330ff1c1a1ba1177,

	/* netbufs_tl::td_head_magic (fiscal diesel ) */
	M0_IOS_NET_BUFFER_HEAD_MAGIC = 0x33f15ca1d1e5e177,

	/* m0_reqh_io_service::rios_magic (cocigeal cell) */
	M0_IOS_REQH_SVC_MAGIC = 0x33c0c19ea1ce1177,

	/* m0_reqh_md_service::rmds_magic (abscissa cell) */
	M0_MDS_REQH_SVC_MAGIC = 0x33ab5c155ace1177,

	/* bufferpools_tl::rios_bp_magic (cafe accolade) */
	M0_IOS_BUFFER_POOL_MAGIC = 0x33cafeacc01ade77,

	/* bufferpools_tl::td_head_magic (colossal face) */
	M0_IOS_BUFFER_POOL_HEAD_MAGIC = 0x33c01055a1face77,

	/* m0_io_fop::if_magic (affable aided) */
	M0_IO_FOP_MAGIC = 0x33affab1ea1ded77,

	/* ioseg::is_magic (soleless zeal) */
	M0_IOS_IO_SEGMENT_MAGIC = 0x33501e1e552ea177,

	/* iosegset::td_head_magic (doddle fascia) */
	M0_IOS_IO_SEGMENT_SET_MAGIC = 0x33d0dd1efa5c1a77,

/* In-storage-compute service. */
	/* m0_isc_comp::ic_magic (fabaceae else) */
	M0_ISC_COMP_MAGIC = 0x33fabaceaee15e77,
	/* m0_reqh_isc_service::riscs_comp_ht (leafed osasco)*/
	M0_ISC_TLIST_HEAD_MAGIC = 0x331eafed05a5c077,
	/*  m0_reqh_isc_service::riscs_magic (loasa silicle)*/
	M0_ISCS_REQH_SVC_MAGIC = 0x3310a5a5111c1e77,

/* Layout */
	/* m0_layout::l_magic (edible sassie) */
	M0_LAYOUT_MAGIC = 0x33ed1b1e5a551e77,

	/* m0_layout_enum::le_magic (ideal follies) */
	M0_LAYOUT_ENUM_MAGIC = 0x331dea1f0111e577,

	/* layout_tlist::head_magic (biddable blad) */
	M0_LAYOUT_HEAD_MAGIC = 0x33b1ddab1eb1ad77,

	/* m0_layout_instance::li_magic (cicilial cell) */
	M0_LAYOUT_INSTANCE_MAGIC = 0x33c1c111a1ce1177,

	/* m0_pdclust_layout::pl_magic (balolo ballio) */
	M0_LAYOUT_PDCLUST_MAGIC = 0x33ba1010ba111077,

	/* m0_pdclust_instance::pi_magic (de coccolobis) */
	M0_LAYOUT_PDCLUST_INSTANCE_MAGIC = 0x33dec0cc010b1577,

	/* m0_layout_list_enum::lle_magic (ofella soiled) */
	M0_LAYOUT_LIST_ENUM_MAGIC = 0x330fe11a5011ed77,

	/* m0_layout_linear_enum::lla_magic (boldface blob) */
	M0_LAYOUT_LINEAR_ENUM_MAGIC = 0x33b01dfaceb10b77,

/* Net */
	/* m0_net_domain::nd_magix (acidic access) */
	M0_NET_DOMAIN_MAGIC = 0x33ac1d1cacce5577,

	/* ndom_tl::td_head_magic (adelaide aide) */
	M0_NET_DOMAIN_HEAD_MAGIC = 0x33ade1a1dea1de77,

	/* netbufs_tl::td_head_magic (saleable sale) */
	M0_NET_BUFFER_HEAD_MAGIC = 0x335a1eab1e5a1e77,

	/* m0_net_buffer::nb_tm_linkage (social silica) */
	M0_NET_BUFFER_LINK_MAGIC = 0x3350c1a15111ca77,

	/* m0_net_pool_tl::td_head_magic (zodiacal feed) */
	M0_NET_POOL_HEAD_MAGIC = 0x3320d1aca1feed77,

	/* m0_net_bulk_mem_end_point::xep_magic (bedside flood) */
	M0_NET_BULK_MEM_XEP_MAGIC = 0x33bed51def100d77,

	/* nlx_kcore_domain::kd_magic (classical cob) */
	M0_NET_LNET_KCORE_DOM_MAGIC = 0x33c1a551ca1c0b77,

	/* nlx_kcore_transfer_mc::ktm_magic (eggless abode) */
	M0_NET_LNET_KCORE_TM_MAGIC  = 0x33e991e55ab0de77,

	/* tms_tl::td_head_magic (alfaa bacilli) */
	M0_NET_LNET_KCORE_TMS_MAGIC = 0x33a1faabac111177,

	/* nlx_kcore_buffer::kb_magic (dissociablee) */
	M0_NET_LNET_KCORE_BUF_MAGIC = 0x33d1550c1ab1ee77,

	/* nlx_kcore_buffer_event::bev_magic (salsa bacilli) */
	M0_NET_LNET_KCORE_BEV_MAGIC = 0x335a15abac111177,

	/* drv_tms_tl::td_head_magic (cocci bacilli) */
	M0_NET_LNET_DEV_TMS_MAGIC   = 0x33c0cc1bac111177,

	/* drv_bufs_tl::td_head_magic (cicadellidae) */
	M0_NET_LNET_DEV_BUFS_MAGIC  = 0x33c1cade111dae77,

	/* drv_bevs_tl::td_head_magic (le cisco disco) */
	M0_NET_LNET_DEV_BEVS_MAGIC  = 0x331ec15c0d15c077,

	/* nlx_ucore_domain::ud_magic (blooded blade) */
	M0_NET_LNET_UCORE_DOM_MAGIC = 0x33b100dedb1ade77,

	/* nlx_ucore_transfer_mc::utm_magic (obsessed lila) */
	M0_NET_LNET_UCORE_TM_MAGIC  = 0x330b5e55ed111a77,

	/* nlx_ucore_buffer::ub_magic (ideal icefall) */
	M0_NET_LNET_UCORE_BUF_MAGIC = 0x331dea11cefa1177,

	/* nlx_core_buffer::cb_magic (edible icicle) */
	M0_NET_LNET_CORE_BUF_MAGIC = 0x33ed1b1e1c1c1e77,

	/* nlx_core_transfer_mc::ctm_magic (focal edifice) */
	M0_NET_LNET_CORE_TM_MAGIC  = 0x33f0ca1ed1f1ce77,

	/* nlx_xo_ep::xe_magic (failed fiasco) */
	M0_NET_LNET_XE_MAGIC = 0x33fa11edf1a5c077,

	/* bsb_tl::tl_head_magic (collides lail) */
	M0_NET_TEST_BSB_HEAD_MAGIC = 0x33c0111de51a1177,

	/* buf_status_bulk::bsb_magic (colloidal dal) */
	M0_NET_TEST_BSB_MAGIC = 0x33c01101da1da177,

	/* bsp_tl::tl_head_magic (delocalize so) */
	M0_NET_TEST_BSP_HEAD_MAGIC = 0x33de10ca112e5077,

	/* buf_status_ping::bsp_magic (lossless bafo) */
	M0_NET_TEST_BSP_MAGIC = 0x3310551e55baf077,

	/* buf_state_tl::tl_head_magic (official oecd) */
	M0_NET_TEST_BS_HEAD_MAGIC = 0x330ff1c1a10ecd77,

	/* buf_state::bs_link_magic (decibel aedes) */
	M0_NET_TEST_BS_LINK_MAGIC = 0x33dec1be1aede577,

	/* net_test_network_bds_header::ntnbh_magic (boldfaces esd) */
	M0_NET_TEST_NETWORK_BDS_MAGIC = 0x33b01dface5e5d77,

	/* net_test_network_bd::ntnbd_magic (socialized io) */
	M0_NET_TEST_NETWORK_BD_MAGIC = 0x3350c1a112ed1077,

	/* slist_params::sp_magic (sodaless adze) */
	M0_NET_TEST_SLIST_MAGIC = 0x3350da1e55ad2e77,

	/* ssb_tl::tl_head_magic (coloss caball) */
	M0_NET_TEST_SSB_HEAD_MAGIC = 0x33c01055caba1177,

	/* server_status_bulk::ssb_magic (closes doddie) */
	M0_NET_TEST_SSB_MAGIC = 0x33c105e5d0dd1e77,

	/* net_test_str_len::ntsl_magic (boldfaced sao) */
	M0_NET_TEST_STR_MAGIC = 0x33b01dfaced5a077,

	/* m0_net_test_timestamp::ntt_magic (allied cabiai) */
	M0_NET_TEST_TIMESTAMP_MAGIC = 0x33a111edcab1a177,

	/* net/sock.c: sock list element, sock::s_magix (abdicable ace) */
	M0_NET_SOCK_SOCK_MAGIC = 0x33abd1cab1eace77,

	/* net/sock.c: sock list head (a bald bailiff) */
	M0_NET_SOCK_SOCK_HEAD_MAGIC = 0x33aba1dba111ff77,

	/* net/sock.c: mover list element, mover::m_magix (decaf debacle) */
	M0_NET_SOCK_MOVER_MAGIC = 0x33decafdebac1e77,

	/* net/sock.c: mover list head (leafless Edda) */
	M0_NET_SOCK_MOVER_HEAD_MAGIC = 0x331eaf1e55edda77,

	/* net/sock.c: buf list element, buf::b_magix (fed ace decade) */
	M0_NET_SOCK_BUF_MAGIC = 0x33fedacedecade77,

	/* net/sock.c: buf list head (bad dada decaf) */
	M0_NET_SOCK_BUF_HEAD_MAGIC = 0x33baddadadecaf77,

	/* net/net.h: m0_nep list element, endpoint (obsessed loll) */
	M0_NET_NEP_MAGIC = 0x330b5e55ed101177,

	/* net/net.h: m0_nep list head (lidless cobol) */
	M0_NET_NEP_HEAD_MAGIC = 0x3311d1e55c0b0177,

/* Pool */
	/* m0_pool::po_magic (baseless bass) */
	M0_POOL_MAGIC = 0x33ba5e1e55ba5577,

	/* m0_pool_event_link::pel_magic (pool evnt list)*/
	M0_POOL_EVENTS_LIST_MAGIC = 0x3360013747712777,

	/* poolmach_tl::tl_head_magic (pool evnt head)*/
	M0_POOL_EVENTS_HEAD_MAGIC = 0x33600137474ead77,

	/* pools_common_svc_ctx_tl::td_head_magic (feedable food) */
	M0_POOL_SVC_CTX_HEAD_MAGIC = 0x33feedab1ef00d77,

	/* m0_pool_version::pv_magic (scalable code) */
	M0_POOL_VERSION_MAGIC = 0x335ca1ab1ec0de77,

	/* pool_version_tl::td_head_magic (feasible code) */
	M0_POOL_VERSION_HEAD_MAGIC = 0x33fea51b1ec0de77,

	/* m0_pooldev::pd_footer::ft_magic (cool fido dido) */
	M0_POOL_DEV_MAGIC = 0x33c001f1d0d1d077,

	/* pool_failed_device_tl::td_head_magic (soiled seabed) */
	M0_POOL_DEVICE_HEAD_MAGIC = 0x335011ed5eabed77,

	/* pver_policy_types_tl::td_head_magic (doss dose doze) */
	M0_PVER_POLICY_HEAD_MAGIC = 0x33d055d05ed02e77,

	/* m0_pver_policy_type::ppt_magic (pool polisee) */
	M0_PVER_POLICY_MAGIC      = 0x33b001b0115ee77,

/* Request handler */
	/* m0_reqh_service::rs_magix (bacilli babel) */
	M0_REQH_SVC_MAGIC = 0x33bac1111babe177,

	/* m0_reqh_service_type::rst_magix (fiddless cobe) */
	M0_REQH_SVC_TYPE_MAGIC = 0x33f1dd1e55c0be77,

	/* m0_reqh_svc_tl::td_head_magic (calcific boss) */
	M0_REQH_SVC_HEAD_MAGIC = 0x33ca1c1f1cb05577,

	/* m0_reqh_service_context::sc_magic (failed facade) */
	M0_REQH_SVC_CTX_MAGIC = 0x33fa11edfacade77,

	/* m0_reqh_rpc_mach_tl::td_head_magic (laissez eifel) */
	M0_REQH_RPC_MACH_HEAD_MAGIC = 0x331a155e2e1fe177,

	/* rev_conn_tl::rcf_linkage (abless ablaze) */
	M0_RM_REV_CONN_LIST_MAGIC = 0x33AB1E55AB1A2E77,

	/* rev_conn_tl::td_head_magic (belief abelia) */
	M0_RM_REV_CONN_LIST_HEAD_MAGIC = 0x33BE11EFABE11A77,

/* State Machine */
	/* m0_sm_conf::scf_magic (falsie zodiac) */
	M0_SM_CONF_MAGIC = 0x33FA151E20D1AC77,

/* Resource Manager */
	/* m0_rm_pin::rp_magix (bellicose bel) */
	M0_RM_PIN_MAGIC = 0x33be111c05ebe177,

	/* m0_rm_loan::rl_magix (biblical bill) */
	M0_RM_LOAN_MAGIC = 0x33b1b11ca1b11177,

	/* m0_rm_incoming::rin_magix (cacalia boole) */
	M0_RM_INCOMING_MAGIC = 0x33caca11ab001e77,

	/* m0_rm_outgoing::rog_magix (calcific call) */
	M0_RM_OUTGOING_MAGIC = 0x33ca1c1f1cca1177,

	/* pr_tl::td_head_magic (collide colsa) */
	M0_RM_CREDIT_PIN_HEAD_MAGIC = 0x33c0111dec015a77,

	/* pi_tl::td_head_magic (diabolise del) */
	M0_RM_INCOMING_PIN_HEAD_MAGIC = 0x33d1ab0115ede177,

	/* m0_rm_resource::r_magix (di doliolidae) */
	M0_RM_RESOURCE_MAGIC = 0x33d1d011011dae77,

	/* res_tl::td_head_magic (feeble eagles) */
	M0_RM_RESOURCE_HEAD_MAGIC = 0x33feeb1eea91e577,

	/* m0_rm_right::ri_magix (fizzle fields) */
	M0_RM_CREDIT_MAGIC = 0x33f1221ef1e1d577,

	/* m0_rm_ur_tl::td_head_magic (idolise iliad) */
	M0_RM_USAGE_CREDIT_HEAD_MAGIC = 0x331d0115e111ad77,

	/* remotes_tl::td_head_magic (offal oldfool) */
	M0_RM_REMOTE_OWNER_HEAD_MAGIC = 0x330ffa101df00177,

	/* m0_rm_remote::rem_magix (hobo hillbill) */
	M0_RM_REMOTE_MAGIC = 0x3309047714771977,

	/* m0_reqh_rm_service::rms_magix (seidel afield) */
	M0_RM_SERVICE_MAGIC = 0x335e1de1af1e1d77,

	/* m0_owners_tl::ro_owner_linkage (eiffel doodle) */
	M0_RM_OWNER_LIST_MAGIC = 0x33E1FFE1D00D1E77,

	/* m0_owners_tl::td_head_magic (scalic seabed) */
	M0_RM_OWNER_LIST_HEAD_MAGIC = 0x335CA11C5EABED77,

/* RPC */
	/* m0_rpc_service_type::svt_magix (seedless seel) */
	M0_RPC_SERVICE_TYPE_MAGIC = 0x335eed1e555ee177,

	/* m0_rpc_service::svc_magix (selfless self) */
	M0_RPC_SERVICE_MAGIC = 0x335e1f1e555e1f77,

	/* m0_rpc_services_tl::td_head_magic (lillie lisboa) */
	M0_RPC_SERVICES_HEAD_MAGIC = 0x3311111e115b0a77,

	/* rpc_service_types_tl::td_head_magic (fosilised foe) */
	M0_RPC_SERVICE_TYPES_HEAD_MAGIC = 0x33f055115edf0e77,

	/* m0_rpc_bulk_buf::bb_link (lidded liliac) */
	M0_RPC_BULK_BUF_MAGIC = 0x3311dded1111ac77,

	/* m0_rpc_bulk::rb_magic (leafless idol) */
	M0_RPC_BULK_MAGIC = 0x331eaf1e551d0177,

	/* m0_rpc_frm::f_magic (adelice dobie) */
	M0_RPC_FRM_MAGIC = 0x33ade11ced0b1e77,

	/* itemq_tl::td_head_magic (dazzled cliff) */
	M0_RPC_ITEMQ_HEAD_MAGIC = 0x33da221edc11ff77,

	/* m0_rpc_item::ri_magic (boiled coolie) */
	M0_RPC_ITEM_MAGIC = 0x33b011edc0011e77,

	/* rpcitem_tl::td_head_magic (disabled disc) */
	M0_RPC_ITEM_HEAD_MAGIC = 0x33d15ab1edd15c77,

	/* ric_tl::td_head_magic (zizzled cache) */
	M0_RPC_ITEM_CACHE_HEAD_MAGIC = 0x3321221edcac4e77,

	/* pending_item_tl::td_head_magic (doss doze dose) */
	M0_RPC_ITEM_PENDING_CACHE_HEAD_MAGIC = 0x33D055D02ED05E77,

	/* m0_rpc_item_source::ri_magic (ACCESSIBLE AC) */
	M0_RPC_ITEM_SOURCE_MAGIC = 0x33ACCE551B1EAC77,

	/* item_source_tl::td_head_magic (AC ACCESSIBLE) */
	M0_RPC_ITEM_SOURCE_HEAD_MAGIC = 0x33ACACCE551B1E77,

	/* rpc_buffer::rb_magic (iodized isaac) */
	M0_RPC_BUF_MAGIC = 0x3310d12ed15aac77,

	/* m0_rpc_machine::rm_magix (deboise aloof) */
	M0_RPC_MACHINE_MAGIC = 0x33deb015ea100f77,

	/* m0_rpc_item_type::rit_magic (daffodil dace) */
	M0_RPC_ITEM_TYPE_MAGIC = 0x33daff0d11dace77,

	/* rit_tl::td_head_magic (caboodle cold) */
	M0_RPC_ITEM_TYPE_HEAD_MAGIC = 0x33cab00d1ec01d77,

	/* packet_item_tl::td_head_magic (falloff eagle) */
	M0_RPC_PACKET_HEAD_MAGIC = 0x33fa110ffea91e77,

	/* m0_rpc_conn::c_magic (classic alibi) */
	M0_RPC_CONN_MAGIC = 0x33c1a551ca11b177,

	/* rpc_conn_tl::td_head_magic (bloodless god) */
	M0_RPC_CONN_HEAD_MAGIC = 0x33b100d1e5590d77,

	/* m0_rpc_session::s_magic (azido ballade) */
	M0_RPC_SESSION_MAGIC = 0x33a21d0ba11ade77,

	/* session_tl::td_head_magic (sizeable bell) */
	M0_RPC_SESSION_HEAD_MAGIC = 0x33512eb1ebe1177,

	/* m0_rpc_slot::sl_magic (delible diode) */
	M0_RPC_SLOT_MAGIC = 0x33de11b1ed10de77,

	/* ready_slot_tl::td_head_magic (assoil azzola) */
	M0_RPC_SLOT_HEAD_MAGIC = 0x33a55011a2201a77,

	/* slot_item_tl::td_head_magic (efface eiffel) */
	M0_RPC_SLOT_REF_HEAD_MAGIC = 0x33effacee1ffe177,

	/* m0_rpc_chan::rc_magic (faceless idol) */
	M0_RPC_CHAN_MAGIC = 0x33face1e551d0177,

	/* rpc_chans_tl::td_head_magic (idesia fossil) */
	M0_RPC_CHAN_HEAD_MAGIC = 0x331de51af0551177,

	/* m0_rpc_chan_watch::mw_magic "accessboiled*/
	M0_RPC_MACHINE_WATCH_MAGIC = 0x33ACCE55B011ED77,

	/* rmach_watch_tl::td_head_magic "COCOAA CALLED" */
	M0_RPC_MACHINE_WATCH_HEAD_MAGIC = 0x33C0C0AACA11ED77,

/* FIS */
	/* m0_reqh_fi_service::fis_magix (saddled bleed) */
	M0_FI_SERVICE_MAGIC = 0x335add1edb1eed77,

	/* m0_rpc_conn_pool_item::cpi_magic "fiddle fiasco" */
	M0_RPC_CONN_POOL_ITEMS_MAGIC = 0x33F1DD1EF1A5C077,

	/* m0_rpc_conn_pool_items_list::td_head_magic "fossil boilie" */
	M0_RPC_CONN_POOL_ITEMS_HEAD_MAGIC = 0x33F05511B0111E77,

/* SNS repair */
	/** m0_sns_cm_file_ctx::sf_magic (labelled babe) */
	M0_SNS_CM_FILE_CTX_MAGIC = 0x331ABE11EDBABE77,

	/** m0_sns_cm::sc_magic (salesalesale) */
	M0_SNS_CM_MAGIC = 0x335A1E5A1E5A1E77,

/* stob */
	/* m0_stob::so_cache_magic (cache fill) */
	M0_STOB_CACHE_MAGIC         = 0x33cac4ef11177,

	/* stob/cache.c:stob_cache_tl::td_head_magic (cache billed) */
	M0_STOB_CACHE_HEAD_MAGIC    = 0x33cac4eb111ed77,

	/* m0_stob_type::st_magic (disc class) */
	M0_STOB_TYPES_MAGIC         = 0x33d15cc1a5577,

	/* stob/type.c:types_tl::td_head_magic (disc head) */
	M0_STOB_TYPES_HEAD_MAGIC    = 0x33d15c4ead77,

	/* m0_stob_domain::sd_magic (disc alle code) */
	M0_STOB_DOMAINS_MAGIC       = 0x33d15ca11ec0de77,

	/* stob/type.c:domains_tl::td_head_magic (disc loco code) */
	M0_STOB_DOMAINS_HEAD_MAGIC  = 0x33d15c10c0c0de77,

	/* ad_domain_map::adm_magic (ad sold class) */
	M0_AD_DOMAINS_MAGIC         = 0x33ad501dc1a5577,

	/* stob/ad.c:ad_domains_tl::td_head_magic (ad sold head) */
	M0_AD_DOMAINS_HEAD_MAGIC    = 0x33ad501d4ead77,

	/* stob_null::sn_magic (discoid solo) */
	M0_STOB_NULL_MAGIC          = 0x33d15c01d501077,

	/* stob/null.c:null_stobs_tl::td_head_magic (discoid boss) */
	M0_STOB_NULL_HEAD_MAGIC     = 0x33d15c01db05577,

	/* stob_null_domain::snd_magic (discoid class) */
	M0_STOB_DOM_NULL_MAGIC      = 0x33d15c01dc1a5577,

	/* stob/null.c:null_domains_tl::td_head_magic (discoid head) */
	M0_STOB_DOM_NULL_HEAD_MAGIC = 0x33d15c01d4ead77,

	/* stob_perf_domain::spd_magic (feed disc babe) */
	M0_STOB_DOM_PERF_MAGIC      = 0x33feedd15cbabe77,

	/* stob_perf::sp_magic (deed disc babe) */
	M0_STOB_PERF_MAGIC          = 0x33deedd15cbabe77,

	/* stob_perf_io::spi_magic (seed disc io io) */
	M0_STOB_PERF_IO_MAGIC       = 0x335eedd15c101077,

	/* stob/perf.c::stob_perf_ios_tl::td_head_magic (seed disc head) */
	M0_STOB_PERF_IO_HEAD_MAGIC  = 0x335eedd15c4ead77,

	/* stob/ad.h:m0_stob_ad_domain::sad_magix (bob ad disc bob) */
	M0_STOB_AD_DOMAIN_MAGIC     = 0x33b0badd15cb0b77,

/* Storage device */
	/* m0_storage_dev::isd_magic (defaced code) */
	M0_STORAGE_DEV_MAGIC = 0x33defacedc0de77,

	/* storage_dev_tl::td_head_magic (defaced head) */
	M0_STORAGE_DEV_HEAD_MAGIC = 0x335defaced4ead77,

/* Stats */
	/* m0_stats::s_magic (slab slab) */
	M0_STATS_MAGIC = 0x3351AB51AB77,

	/* stats_tl::.td_head_magic (slab sled) */
	M0_STATS_HEAD_MAGIC = 0x3351AB51ED77,

	/* stats_svc::ss_magic (slab sic)*/
	M0_STATS_SVC_MAGIC = 0x3351AB51C77,

	/* stats_query_fom::suf_magic (slab deed) */
	M0_STATS_QUERY_FOM_MAGIC = 0x3351ABDEED77,

	/* stats_update_fom::suf_magic (slab feed) */
	M0_STATS_UPDATE_FOM_MAGIC = 0x3351ABFEED77,

/* Trace */
	/* m0_trace_rec_header::trh_magic (foldable doll) */
	M0_TRACE_MAGIC = 0x33f01dab1ed01177,

	/* m0_trace_descr::td_magic (badass coders) */
	M0_TRACE_DESCR_MAGIC = 0x33bada55c0de2577,

	/* m0_trace_buf_header::tbh_magic (decoded sorce) */
	M0_TRACE_BUF_HEADER_MAGIC = 0x33dec0ded502ce77,

/* lib */
	/* kern_genarray::kga_magic (areexclusive) */
	M0_LIB_GENARRAY_MAGIC  = 0x33a2ee8c16517e77,
	/* hashlist::hl_magic (invincibilis) */
	M0_LIB_HASHLIST_MAGIC  = 0x3319519c1b111577,

	/* m0_clink::cl_magic (blessed call) */
	M0_LIB_CHAN_MAGIC      = 0x33b1e55edca1177,
	/* lib/chan.c:clink_tl::td_head_magic (blessed head) */
	M0_LIB_CHAN_HEAD_MAGIC = 0x33b1e55ed4ead77,

	/* tid_tl::td_head_magic (allodial dill) */
	M0_LIB_TIMER_TID_HEAD_MAGIC = 0x33a110d1a1d11177,
	/* m0_timer_tid::tt_magic (eila alia dill) */
	M0_LIB_TIMER_TID_MAGIC = 0x33e11aa11ad11177,

/* sss */
	/* ss_svc::sss_magic (coffeeleaf ad) */
	M0_SS_SVC_MAGIC = 0x33c0ffee1eafad77,
	/* ss_fom::ssf_magic (fossilizes lo) */
	M0_SS_FOM_MAGIC = 0x33f0551112e51077,

/* clovis */
	/* target_ioreq::ti_magic */
	M0_CLOVIS_TLIST_HEAD_MAGIC   = 0x3339816123512277,
	/* ioo_bobtype::bt_magix */
	M0_CLOVIS_IOFOP_MAGIC        = 0x3349816123512277,
	/* ioreq_fop::irf_magic */
	M0_CLOVIS_IOREQ_MAGIC        = 0x3359816123512277,
	/* pgiomap_bobtype::bt_magix */
	M0_CLOVIS_PGROUP_MAGIC       = 0x3369816123512277,
	/* dtbuf_bobtype::bt_magix */
	M0_CLOVIS_DTBUF_MAGIC        = 0x3379816123512277,
	/* nwxfer_bobtype::bt_magix */
	M0_CLOVIS_NWREQ_MAGIC        = 0x3389816123512277,
	/* tioreq_bobtype::bt_magix */
	M0_CLOVIS_TIOREQ_MAGIC       = 0x3399816123512277,
	/* oc_bobtype::bt_magix */
	M0_CLOVIS_OC_MAGIC           = 0x33a9816123512277,
	/* oo_bobtype::bt_magix */
	M0_CLOVIS_OO_MAGIC           = 0x33b9816123512277,
	/* ar_bobtype::bt_magix */
	M0_CLOVIS_AST_RC_MAGIC       = 0x33c9816123512277,
	M0_CLOVIS_ICR_MAGIC          = 0x33d9816123512277,
	/* op_bobtype::bt_magix */
	M0_CLOVIS_OP_MAGIC           = 0x33e9816123512277,
	M0_CLOVIS_M0C_MAGIC          = 0x33f9816123512277,
	/* oi_bobtype::bt_magix */
	M0_CLOVIS_OI_MAGIC           = 0x3309816123512277,
	/* os_bobtype::bt_magix */
	M0_CLOVIS_OS_MAGIC           = 0x3319816123512277,
	/* clovis_sync_fop_wrapper::sfw_tlink_magic */
	M0_CLOVIS_SYNC_TGT_TL_MAGIC  = 0x3320816123512277,
	/* clovis_sync_fop_wrapper::sfw_magic */
	M0_CLOVIS_INSTANCE_PTI_MAGIC = 0x3321816123512277,
	/* ol_bobtype::bt_magix */
	M0_CLOVIS_OL_MAGIC           = 0x3322816123512277,
	/* cr_bobtype::bt_magix */
	M0_CLOVIS_CR_MAGIC           = 0x3323816123512277,
	/* oci_bobtype::bt_magix */
	M0_CLOVIS_OCI_MAGIC          = 0x3324816123512277,
	/* m0_clovis_composite_layer::ccr_tlink_magic */
	M0_CLOVIS_CLAYER_TL_MAGIC    = 0x3325816123512277,
	/* m0_clovis_composite_extent:ce_tlink_magic */
	M0_CLOVIS_CEXT_TL_MAGIC      = 0x3326816123512277,
	/* composite_sub_io_ext:ce_tlink_magic */
	M0_CLOVIS_CIO_EXT_MAGIC      = 0x3327816123512277,
	/* m0_clovis_rm_lock_ctx::rmc_magic (ice ice ice) */
	M0_CLOVIS_RM_MAGIC           = 0x331CE1CE1C0E2277,
	/* rm_ctx_tl::td_head_magic (coca cola sea) */
	M0_CLOVIS_RM_HEAD_MAGIC      = 0x33C0CAC01A5EA277,

/* module/param */
	/* m0_param_source::ps_magic (boozed billie) */
	M0_PARAM_SOURCE_MAGIC = 0x33b002edb1111e77,
	/* m0_param_sources_tl::td_head_magic (ascidiozooid) */
	M0_PARAM_SOURCES_MAGIC = 0x33a5c1d102001d77,

/* MD */
	/* rdms_layout:l_magic (coffeeless ad)*/
	M0_LAYOUT_MD_MAGIC     = 0x33c0ffee1e55ad77,

/* format */
	/* m0_format_header::hd_magic (oilcaseslide) */
	M0_FORMAT_HEADER_MAGIC = 0x33011ca5e511de77,
	/* m0_format_footer::ft_magic (footerfooter) */
	M0_FORMAT_FOOTER_MAGIC = 0x33f007e7f007e777,

/* SPIL */
	/*                 (filecf coders)*/
	M0_SPIEL_FOP_MAGIC       = 0x33f11ecfc0de2577,
	/*                 (defile coders)*/
	M0_SPIEL_PROC_MAGIC      = 0x33def11ec0de2577,
	/*                 (diddle coders)*/
	M0_SPIEL_PROC_HEAD_MAGIC = 0x33d1dd1ec0de2577,

/* fid */
	/*                  (fid) */
	M0_FID_MAGIC           = 0x33f1d00000000077,
	/*                  (fidhead) */
	M0_FID_HEAD_MAGIC      = 0x33f1d4ead0000077,
/* DIX */
	/** m0_dix_cas_rop::crp_magix (basic oilseed) */
	M0_DIX_ROP_MAGIC       = 0x33ba51c0115eed77,
	/** cas_rop_tlist head magic (basic offload) */
	M0_DIX_ROP_HEAD_MAGIC  = 0x33ba51c0ff10ad77,
/* FDMI */
	/* m0_reqh_fdmi_service::rfdms_magic (abide dazzled) */
	M0_FDMS_REQH_SVC_MAGIC = 0x33ab1deda221ed77,

	/* m0_fol_fdmi_src_ctx::ffsc_magic (fol decade) */
	M0_FOL_FDMI_SRC_CTX_MAGIC = 0x33f01decade77,

	/* m0_fdmi_filter_reg::ffr_magic (scaffold feel) */
	M0_FDMI_FLTR_MAGIC = 0x335caff01dfee177,
	/* fdmi_filters head magic (localized lie) */
	M0_FDMI_FLTR_HEAD_MAGIC = 0x3310ca112ed11e77,
	/* fdmi_recs list magic (baseball isle) */
	M0_FDMI_RCRD_MAGIC = 0x33ba5eba11151e77,
	/* fdmi_recs list head magic (oldfield bozo) */
	M0_FDMI_RCRD_HEAD_MAGIC = 0x3301df1e1db02077,
	/* Currently not used (bla bla bla bla) */
	M0_FDMI_PCTX_MAGIC = 0x33b1ab1ab1ab1a77,
	/* Currently not used (blo blo blo blo) */
	M0_FDMI_PCTX_HEAD_MAGIC = 0x33b10b10b10b1077,
	/* Currently not used (lifesize icao) */
	M0_FDMI_PLUGIN_DOCK_FOM_TASK_MAGIC = 0x3311fe212e1ca077,
	/* Currently not used (acacia liable) */
	M0_FDMI_PLUGIN_DOCK_FOM_TASK_HEAD_MAGIC = 0x33acac1a11ab1e77,
	/* m0_fdmi_src_ctx::fsc_magic */
	M0_FDMI_SRC_DOCK_SRC_CTX_MAGIC = 0x3300fdfd01ea0377,
	/* fdmi_src_dock_src_list head magic */
	M0_FDMI_SRC_DOCK_SRC_CTX_HEAD_MAGIC = 0x3300fdfd7846fd77,
	/* fdmi_record list magic */
	M0_FDMI_SRC_DOCK_REC_MAGIC = 0x3321fdfd01650377,
	/* fdmi_record list head magic */
	M0_FDMI_SRC_DOCK_REC_HEAD_MAGIC = 0x3345fdfd0ac80377,
	/* fdmi_matched_filter_list magic */
	M0_FDMI_SRC_DOCK_MATCHED_FILTER_MAGIC = 0x33fefdfd01754377,
	/* fdmi_matched_filter_list head magic */
	M0_FDMI_SRC_DOCK_MATCHED_FILTER_HEAD_MAGIC = 0x330ffdfd97427677,
	/* pending_fops list magic (fleece office) */
	M0_FDMI_SRC_DOCK_PENDING_FOP_MAGIC = 0xf1eece0ff1ce,
	/* pending_fops list head magic (feosol obsess) */
	M0_FDMI_SRC_DOCK_PENDING_FOP_HEAD_MAGIC = 0xfe05010b5e55,
};

#endif /* __MERO_MERO_MAGIC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
