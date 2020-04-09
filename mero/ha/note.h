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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 *                  Maxim Medved <max.medved@seagate.com>
 * Original creation date: 10-May-2013
 */

#pragma once

#ifndef __MERO___HA_NOTE_H__
#define __MERO___HA_NOTE_H__

/**
 * @defgroup ha-note HA notification
 *
 * TODO update
 *
 * This module defines protocols and functions used to communicate HA-related
 * events between HA and Mero core.
 *
 * Any HA-related event is represented as a state change of a configuration
 * object. Configuration objects are stored in the Mero configuration data-base
 * hosted by confd services and accessible to Mero and HA instances through
 * confc module. A configuration object is identified by a unique 128-bit
 * identifier.
 *
 * HA-related state of a configuration object is represented by enum
 * m0_ha_obj_state. It is important to understand that this state is *not*
 * stored in confd. confd stores the "basic" information describing the
 * nomenclature of system elements (nodes, services, devices, pools, etc.) and
 * their relationships. HA maintains additional state on top of confd, which
 * describes the run-time behaviour of configuration elements.
 *
 * Among other things, confd stores, for certain types of objects, their
 * "delegation pointers". A delegation pointer of an object X is some object Y
 * that should be used when X fails. For example, to organise a fail-over pair,
 * 2 services should have delegation pointers set to each other. The delegation
 * pointer of a pool points to the pool to which writes should be re-directed in
 * case of an NBA event. When an object fails, the chain formed by delegation
 * pointers is followed until a usable object is found. If the chain is
 * exhausted before a usable object is found, a system error is declared. All
 * consecutive attempts to use the object would return the error until HA state
 * or confd state changes.
 *
 * <b>Use cases</b>
 *
 * 0. Mero initialisation
 *
 * On startup, Mero instance connects to confd and populates its local confc
 * with configuration objects. Then, Mero instance calls m0_ha_state_get() (one
 * or more times). This function accepts as an input parameter a vector
 * (m0_ha_nvec) that identifies objects for which state is queried
 * (m0_ha_note::no_state field is ignored). m0_ha_state_get() constructs a fop
 * of m0_ha_state_get_fopt type with nvec as data and sends it to the local HA
 * instance (via supplied session).
 *
 * HA replies with the same vector with m0_ha_note::no_state fields
 * set. m0_ha_state_get() stores received object states in confc and notifies
 * the caller about completion through the supplied channel.
 *
 * 1. Mero core notifies HA about failure.
 *
 * On detecting a failure, Mero core calls m0_ha_state_set(), which takes as an
 * input an nvec (with filled states), describing the failures, constructs
 * m0_ha_state_set_fopt fop and sends it to the local HA instance via supplied
 * session. HA replies with generic fop reply (m0_fop_generic_reply).
 *
 * @note that there is a separate mechanism, based on m0ctl, which is used to
 * notify HA about failures which cannot be reported through RPC.
 *
 * 2. HA notifies Mero about failure.
 *
 * When HA agrees about a failure, it sends to each Mero instance a
 * m0_ha_state_set_fopt fop. Mero replies with generic fop reply.
 *
 * m0_ha_state_accept() is called when a m0_ha_state_set_fopt fop is received.
 *
 * @{
 */

/* import */
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "lib/chan.h"
#include "lib/types.h"
#include "lib/buf.h"          /* m0_buf, m0_bufs */
#include "lib/buf_xc.h"       /* m0_buf_xc, m0_bufs_xc */
#include "lib/mutex.h"        /* m0_mutex */
#include "lib/tlist.h"        /* m0_tl */
#include "lib/atomic.h"       /* m0_atomic64 */
#include "xcode/xcode_attr.h"
#include "ha/dispatcher.h"    /* m0_ha_handler */

/* export */
struct m0_ha_note;
struct m0_ha_nvec;

/* foward declaration */
struct m0_conf_obj;

/**
 * Enumeration of possible object states.
 */
enum m0_ha_obj_state {
	/** Object state is unknown. */
	M0_NC_UNKNOWN,
	/** Object can be used normally. */
	M0_NC_ONLINE,
	/**
	 * Object has experienced a permanent failure and cannot be
	 * recovered.
	 */
	M0_NC_FAILED,
	/**
	 * Object is experiencing a temporary failure. Halon will notify Mero
	 * when the object is available for use again.
	 */
	M0_NC_TRANSIENT,
	/**
	 * This state is only applicable to the pool objects. In this state,
	 * the pool is undergoing repair, i.e., the process of reconstructing
	 * data lost due to a failure and storing them in spare space.
	 */
	M0_NC_REPAIR,
	/**
	 * This state is only applicable to the pool objects. In this state,
	 * the pool device has completed sns repair. Its data is re-constructed
	 * on its corresponding spare space.
	 */
	M0_NC_REPAIRED,
	/**
	 * This state is only applicable to the pool objects. Rebalance process
	 * is complementary to repair: previously reconstructed data is being
	 * copied from spare space to the replacement storage.
	 */
	M0_NC_REBALANCE,

	M0_NC_NR
};

/**
 * Note describes (changed) object state.
 */
struct m0_ha_note {
	/** Object identifier. */
	struct m0_fid no_id;
	/** State, from enum m0_ha_obj_state. */
	uint32_t      no_state;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * "Note vector" describes changes in system state.
 */
struct m0_ha_nvec {
	/**
	 * @note Since this field is used for returning error code
	 *       to note interface users @see m0_conf_ha_state_update(),
	 *       changing this to int32_t.
	 *       Anyway nvec will not request status for 2^16 object
	 *       because of RPC mesg size constrains.
	 */
	int32_t            nv_nr;
	struct m0_ha_note *nv_note;
} M0_XCA_SEQUENCE M0_XCA_DOMAIN(rpc);

/**
 * Single "note vector" package.
 *
 * Rpc item makes use of it when notifying HA about remote side connectivity
 * issues (M0_NC_TRANSIENT <--> M0_NC_ONLINE)
 */
struct m0_ha_state_single {
	struct m0_ha_note hss_note;
	struct m0_ha_nvec hss_nvec;
};

enum m0_ha_state_update_defaults {
	M0_HA_STATE_UPDATE_LIMIT = 1024,
};

enum {
	M0_HA_NVEC_SET,
	M0_HA_NVEC_GET,
};

struct m0_ha_msg_nvec_array {
	struct m0_ha_note hmna_arr[M0_HA_STATE_UPDATE_LIMIT];
} M0_XCA_ARRAY M0_XCA_DOMAIN(rpc);

struct m0_ha_msg_nvec {
	/** M0_HA_NVEC_SET for note_set, M0_HA_NVEC_GET for note_get */
	uint64_t                    hmnv_type;
	uint64_t                    hmnv_id_of_get;
	/**
	 * Signal m0_conf_obj::co_ha_chan for changed HA states only?
	 *
	 * If .hmnv_ignore_same_state == 0, then m0_conf_obj::co_ha_chan
	 * of _every_ conf object in the .hmnv_arr will be signaled.
	 * Otherwise, only conf objects with changed HA state will be
	 * signaled.
	 *
	 * |-------------------|-------------------------|-----------------|
	 * | Received HA state | .hmnv_ignore_same_state | Signal object's |
	 * | of a conf object  |                         | .co_ha_chan?    |
	 * | is different from |                         |                 |
	 * | its stored state  |                         |                 |
	 * | (previously       |                         |                 |
	 * | received)?        |                         |                 |
	 * |-------------------|-------------------------|-----------------|
	 * |     the same      |            0            |       yes       |
	 * |     the same      |           != 0          |       no        |
	 * |     different     |            0            |       yes       |
	 * |     different     |           != 0          |       yes       |
	 * |-------------------|-------------------------|-----------------|
	 *
	 * NOTE: Hare monitors a subset (`S`) of Mero conf objects.
	 *       Whenever HA state of _any_ of those objects changes,
	 *       Hare creates an nvec with current HA states of _all_
	 *       objects in `S` and broadcasts this nvec to Mero processes.
	 *       Upon receiving such nvec, Mero process compares it with
	 *       previously stored one and signals only those conf objects,
	 *       HA states of which differ.
	 *
	 *       In order for this to work, Hare MUST set
	 *       .hmnv_ignore_same_state flag to 1.
	 *
	 * Valid values: 0 or 1.
	 */
	uint64_t                    hmnv_ignore_same_state;
	uint64_t                    hmnv_nr;
	struct m0_ha_msg_nvec_array hmnv_arr;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

#define M0_NVEC_PRINT(nvec_, label, level) ({			  \
	int i;                                                    \
	struct m0_ha_nvec *nvec = nvec_;                          \
	char *lbl = label;                                        \
	for (i = 0; i < nvec->nv_nr; i++) {                       \
		M0_LOG(level, "%s [%d] " FID_F ", (%d)", lbl, i,  \
		       FID_P(&nvec->nv_note[i].no_id),		  \
		       nvec->nv_note[i].no_state);		  \
	}                                                         \
})

/**
 * Queries HA about the current the failure state for a set of objects.
 *
 * Constructs a m0_ha_state_get_fopt from the "note" parameter and
 * sends it to an HA instance, returning immediately after the fop is sent.
 * When the reply (m0_ha_state_get_rep_fopt) is received, fills
 * m0_ha_note::no_state from the reply and signals the provided channel.
 *
 * On error (e.g., time-out), the function signals the channel, leaving
 * m0_ha_note::no_state intact, so that the caller can determine that
 * failure state wasn't fetched.
 *
 * Use cases:
 *
 *     this function is called by a Mero instance when it joins the cluster
 *     right after it received configuration information from the confd or
 *     afterwards, when the instance wants to access an object for the first
 *     time. The caller of m0_ha_state_get() is likely to call
 *     m0_ha_state_accept() when the reply is received.
 *
 * The caller must guarantee that on successful return from this function "note"
 * parameter is valid (i.e., not deallocated) until the channel is signalled.
 *
 * @pre m0_forall(i, note->nv_nr, note->nv_note[i].no_state == M0_NC_UNKNOWN &&
 *                                m0_conf_fid_is_valid(&note->nv_note[i].no_id))
 */
M0_INTERNAL int m0_ha_state_get(struct m0_ha_nvec *note, struct m0_chan *chan);
/**
 * Notifies HA about tentative change in the failure state for a set of
 * objects.
 *
 * Constructs a m0_ha_state_set_fopt from the "note" parameter, sends it
 * to an HA instance and returns immediately.
 * This function is used to report failures (and "unfailures") to HA.
 *
 * Use cases:
 *
 *     this function is called by a Mero instance when it detects a
 *     change in object behaviour. E.g., a timeout or increased
 *     latency of a particular service or device.
 *
 * Note that the failure state change is only tentative. It is up to HA to
 * accumulate and analyse the stream of failure notifications and to declare
 * failures. Specifically, a Mero instance should not assume that the object
 * failure state changed, unless explicitly told so by HA.
 *
 * Because failure state change is tentative, no error reporting is needed.
 *
 * @pre m0_forall(i, note->nv_nr, note->nv_note[i].no_state != M0_NC_UNKNOWN &&
 *                             m0_conf_fid_is_valid(&note->nv_note[i].no_id) &&
 *    ergo(M0_IN(note->nv_note[i].no_state, (M0_NC_REPAIR, M0_NC_REBALANCE)),
 *         m0_conf_fid_type(&note->nv_note[i].no_id) == &M0_CONF_POOL_TYPE))
 */
M0_INTERNAL void m0_ha_state_set(const struct m0_ha_nvec *note);
/**
 * Notify local HA about state of configuration objects.
 */
M0_INTERNAL void m0_ha_local_state_set(const struct m0_ha_nvec *nvec);

/**
 * Asynchronous version of m0_ha_state_set() intended for posting single state.
 */
M0_INTERNAL void m0_ha_state_single_post(struct m0_ha_nvec     *nvec);
/**
 * Incorporates received failure state changes in the cache of every confc
 * instance registered with the global HA context (see m0_ha_client_add()).
 *
 * Failure states of configuration objects are received from HA (not confd),
 * but are stored in the same data-structure (conf client cache  (m0_confc)
 * consisting of configuration objects (m0_conf_obj)).
 *
 * This function updates failures states of configuration objects according to
 * "nvec".
 *
 * Use cases:
 *
 *     this function is called when a Mero instance receives a failure state
 *     update (m0_ha_state_set_fopt) from HA. This is a "push" notification
 *     mechanism (HA sends updates) as opposed to m0_ha_state_get(), where Mero
 *     "pulls" updates.
 *
 * @note: m0_conf_obj should be modified to hold HA-related state.
 * Valery Vorotyntsev (valery.vorotyntsev@seagate.com) is the configuration
 * sub-system maintainer.
 *
 * @pre m0_forall(i, note->nv_nr, note->nv_note[i].no_state != M0_NC_UNKNOWN &&
 *                             m0_conf_fid_is_valid(&note->nv_note[i].no_id) &&
 *    ergo(M0_IN(note->nv_note[i].no_state, (M0_NC_REPAIR, M0_NC_REBALANCE)),
 *         m0_conf_fid_type(&note->nv_note[i].no_id) == &M0_CONF_POOL_TYPE))
 *
 * Actual cache update is done by ha_state_accept() called on per-client basis
 * in the course of iterating global HA context client list.
 *
 * See m0_ha_msg_nvec::hmnv_ignore_same_state for ignore_same_state flag
 * description.
 */
M0_INTERNAL void m0_ha_state_accept(const struct m0_ha_nvec *note,
				    bool                     ignore_same_state);

M0_INTERNAL void m0_conf_ha_callback(struct m0_conf_obj *obj);

struct m0_ha_link;

M0_INTERNAL void m0_ha_msg_accept(const struct m0_ha_msg *msg,
                                  struct m0_ha_link      *hl);
M0_INTERNAL uint64_t
m0_ha_msg_nvec_send(const struct m0_ha_nvec *nvec,
		    uint64_t                 id_of_get,
		    bool                     ignore_same_state,
		    int                      direction,
		    struct m0_ha_link       *hl);

struct m0_ha_note_handler {
	struct m0_tl             hnh_gets;
	struct m0_mutex          hnh_lock;
	struct m0_ha_handler     hnh_handler;
	struct m0_ha_dispatcher *hnh_dispatcher;
	uint64_t                 hnh_id_of_get;
};

M0_INTERNAL int m0_ha_note_handler_init(struct m0_ha_note_handler *hnh,
                                        struct m0_ha_dispatcher   *hd);
M0_INTERNAL void m0_ha_note_handler_fini(struct m0_ha_note_handler *hnh);

M0_INTERNAL uint64_t m0_ha_note_handler_add(struct m0_ha_note_handler *hnh,
                                            struct m0_ha_nvec         *nvec_req,
                                            struct m0_chan            *chan);
M0_INTERNAL void m0_ha_note_handler_signal(struct m0_ha_note_handler *hnh,
                                           struct m0_ha_nvec         *nvec_rep,
                                           uint64_t                   id);

M0_INTERNAL const char *m0_ha_state2str(enum m0_ha_obj_state state);

/** @} end of ha-note group */
#endif /* __MERO___HA_NOTE_H__ */

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
