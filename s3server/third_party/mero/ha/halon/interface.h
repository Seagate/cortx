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
 * Original author: Maxim Medved <max.medved@seagate.com>
 * Original creation date: 5-May-2016
 */

#pragma once

#ifndef __MERO_HA_HALON_INTERFACE_H__
#define __MERO_HA_HALON_INTERFACE_H__

/**
 * @defgroup ha
 *
 * - @ref halon-interface-highlights
 *   - @ref halon-interface-msg
 *   - @ref halon-interface-tag
 *   - @ref halon-interface-req-id
 *   - @ref halon-interface-lifetime
 * - @ref halon-interface-spec
 * - @ref halon-interface-sm
 *   - @ref halon-interface-sm-interface
 *   - @ref halon-interface-sm-link
 *   - @ref halon-interface-sm-msg-send
 *   - @ref halon-interface-sm-msg-recv
 * - @ref halon-interface-threads
 *
 * @section halon-interface-highlights Design Highlights
 *
 * - only one m0_halon_interface in a single process address space is supported
 *   at the moment;
 * - user of m0_halon_interface shouldn't care about rpc, reqh or other stuff
 *   that is not documented here. Threading constraints should be taken into
 *   account though.
 *
 * @section halon-interface-lspec Logical Specification
 *
 * @subsection halon-interface-tag Message Handling
 *
 * There are 2 kinds of messages: received and sent.
 * Each message is received or sent in context of m0_ha_link.
 *
 * When a message arrives the msg_received_cb() is executed. The user should
 * call m0_halon_interface_delivered() for each received message. The call means
 * that the message shouldn't be sent again if the current process restarts.
 *
 * To send a message user calls m0_halon_interface_send(). For each message sent
 * in this way either msg_is_delivered_cb() or msg_is_not_delivered_cb() is
 * called. Exactly one of this functions is called exactly once for each message
 * sent.
 *
 * msg_is_delivered_cb() means that the message has been successfully delivered
 * to the destination and it is not going to be resent if the destination
 * restarts. msg_is_not_delivered_cb() is called if the message can't be
 * delivered. It may be delivered already but there is no confirmation that it
 * has been delivered.
 *
 * @subsection halon-interface-tag Message Tag
 *
 * Each message has a tag. The tag has uint64_t type and it's assigned
 * internally when user tries to send a message (m0_halon_interface_send(),
 * for example).  Tag value is unique for all the messages sent or received
 * over a single m0_ha_link. No other assumption about tag value should be used.
 *
 * Tag is used for message identification when struct m0_ha_msg is not available.
 *
 * @subsection halon-interface-req-id Entrypoint Request ID
 *
 * Each entrypoint request has an ID. The ID is m0_uint128 and it's assigned
 * internally when a new entrypoint request is received.
 *
 * Request ID is used to make a mapping between entrypoint request and
 * m0_ha_link.
 *
 * @subsection halon-interface-lifetime Lifetime and Ownership
 *
 * - m0_halon_inteface shouldn't be used before m0_halon_interface_init() and
 *   shouldn't be used after m0_halon_interface_fini();
 * - m0_halon_interface_start()
 *   - local_rpc_endpoint is not used after the function returns;
 *   - callbacks can be executed at any point after the function is called but
 *     before m0_halon_interface_stop() returns;
 * - m0_halon_interface_entrypoint_reply()
 *   - all parameters (except hi) can be freed by the caller after the function
 *     returns;
 *   - can be called only after entrypoint_request_cb() is executed;
 *   - should be called exactly once for each entrypoint request;
 * - m0_halon_interface_send()
 *   - m0_ha_msg is not used after the function returns;
 * - entrypoint_request_cb()
 *   - remote_rpc_endpoint can be finalised at any moment after the callback
 *     returns;
 * - msg_received_cb()
 *   - msg is owned by the user only before m0_halon_interface_delivered() is
 *     called for the message;
 * - entrypoint request id
 *   - is used by the user in m0_halon_interface_entrypoint_reply() after
 *     entrypoint_request_cb() is executed;
 *   - is used by m0_halon_interface in either link_connected_cb() or
 *     link_reused_cb();
 *   - is not used after m0_halon_interface_entrypoint_reply() is called and
 *     link_connected_cb() or link_reused_cb() executed;
 * - m0_ha_link
 *   - m0_halon_interface controls m0_ha_link's lifetime;
 *   - m0_ha_link doesn't exist
 *     - before link_connected_cb() is called;
 *     - after link_disconnected_cb() is finished;
 *   - m0_halon_interface_send(), m0_halon_interface_delivered()
 *     - can be used only after link_connected_cb() is executed for the link;
 *     - can't be used after m0_halon_interface_disconnect() is called.
 *
 * @section halon-interface-sm State machines
 *
 * @subsection halon-interface-sm-interface m0_halon_interface
 *
 * @verbatim
 *
 *      UNINITIALISED
 *          |   ^
 *   init() |   | fini()
 *          v   |
 *       INITIALISED
 *          |    ^
 *  start() |    | stop()
 *          v    |
 *         WORKING
 *
 * @endverbatim
 *
 * @subsection halon-interface-sm-entrypoint Entrypoint requrest/reply
 *
 * @verbatim
 *
 *                                      UNINITIALISED
 *                                            |
 *                   entrypoint_request_cb()  |
 *                                            v
 *                                        REQUESTED
 *                                            |
 *     m0_halon_interface_entrypoint_reply()  |
 *                                            v
 *                                         REPLIED
 *                                            |
 *                       v--------------------v--------------------v
 *                       |                    |                    |
 *   link_connected_cb() |   link_reused_cb() |   link_absent_cb() |
 *                       |                    |                    |
 *                       >--------------------v--------------------<
 *                                            |
 *                                            v
 *                                          DONE
 *
 * @endverbatim
 *
 * @subsection halon-interface-sm-link m0_ha_link
 *
 * @verbatim
 *
 *                                  UNINITIALISED
 *                                    ^      |
 *                                    |      | link_connected_cb()
 *            link_disconnected_cb()  |      |
 *                                    |      |  +---+
 *                                    |      v  v   |
 *                           DISCONNECTED   ACTIVE  | link_reused_cb()
 *                                    |      |  |   |
 *                                    |      |  +---+
 *    m0_halon_interface_disconnect() |      |
 *                                    |      | link_is_disconnecting_cb()
 *                                    |      v
 *                                  DISCONNECTING
 *
 * @endverbatim
 *
 * @subsection halon-interface-sm-msg-send m0_ha_msg send
 *
 * @verbatim
 *
 *                     UNINITIALISED
 *                          |
 *                          | m0_halon_interface_send()
 *                          v
 *                       SENDING
 *                        |  |
 *  msg_is_delivered_cb() |  | msg_is_not_delivered_cb()
 *                        v  v
 *                        DONE
 *
 * @endverbatim
 *
 * @subsection halon-interface-sm-msg-recv m0_ha_msg recv
 *
 * @verbatim
 *
 *   UNINITIALISED
 *        |
 *        | msg_received_cb()
 *        v
 *     HANDLING
 *        |
 *        | m0_halon_interface_delivered()
 *        v
 *       DONE
 *
 * @endverbatim
 *
 * @section halon-interface-threads Threading and Concurrency Model
 *
 * - thread which calls m0_halon_interface_init() is considered as main thread
 *   for m0 instance inside m0_halon_interface;
 * - m0_halon_interface_start(), m0_halon_interface_stop(),
 *   m0_halon_interface_fini() should be called from the exactly the same thread
 *   m0_halon_interface_init() is called (main thread);
 * - if m0_halon_interface is in WORKING state then Mero locality threads are
 *   created;
 * - each callback from m0_halon_interface_start() is executed in the locality
 *   thread. The callback function:
 *   - can be called from any locality thread;
 *   - shouldn't wait for network or disk I/O;
 *   - shoudln't wait for another message to come or for a message to be
 *     delivered;
 *   - can call m0_halon_interface_entrypoint_reply(),
 *     m0_halon_interface_send(), m0_halon_interface_delivered(),
 *     m0_halon_interface_disconnect().
 * - entrypoint_request_cb() can be called from any locality thread at any time
 *   regardless of other entrypoint_request_cb() executing;
 * - msg_received_cb()
 *   - is called sequentially, one by one for the messages from the same link
 *     in the same order the sender has sent the messages;
 * - msg_is_delivered_cb(), msg_is_not_delivered_cb()
 *   - for the same m0_ha_link are called sequentially, in the order
 *     m0_halon_interface_send() was called for the messages;
 * - msg_received_cb(), msg_is_delivered_cb(), msg_is_not_delivered_cb()
 *   - may be called from different threads for the same m0_ha_link;
 *   - if there are links L1 and L2, then callbacks for the messages from the
 *     links are not synchronised in any way;
 * - link_connected_cb(), link_reused_cb(), link_is_disconnecting_cb(),
 *   link_disconnected_cb()
 *   - are called sequentially for the same link;
 *   - may be called from different threads for the same link;
 * - m0_halon_interface_init(), m0_halon_interface_fini(),
 *   m0_halon_interface_start(), m0_halon_interface_stop() are blocking calls.
 *   After the function returns m0_halon_interface is already moved to the
 *   appropriate state;
 * - m0_halon_interface_entrypoint_reply(), m0_halon_interface_send(),
 *   m0_halon_interface_delivered(), m0_halon_interface_disconnect() are
 *   non-blocking calls. They can be called from:
 *   - main thread;
 *   - locality thread;
 *   - any callback.
 *
 * @{
 */

#include "lib/types.h"          /* bool */

struct m0_rpc_machine;
struct m0_reqh;
struct m0_halon_interface_internal;
struct m0_ha_link;
struct m0_ha_msg;
struct m0_fid;
struct m0_spiel;
struct m0_thread;

struct m0_halon_interface {
	struct m0_halon_interface_internal *hif_internal;
};

/**
 * Mero is ready to work after the call.
 *
 * This function also compares given version against current library version.
 * It detects if the given version is compatible with the current version.
 *
 * @param hi                   this structure should be zeroed.
 * @param build_git_rev_id     @see m0_build_info::bi_git_rev_id
 * @param build_configure_opts @see m0_build_info::bi_configure_opts
 * @param debug_options        options that affect debugging. See below.
 * @param node_uuid            node UUID string. @see lib/uuid.h.
 *
 * Debug options (double quotes here are only for clarification):
 * - "disable-compatibility-check"  Don't verify compatibility of Mero and
 *                                  Halon versions.
 * - "log-entrypoint"               Log steps of entrypoint request/reply
 *                                  processing (M0_WARN logging level).
 * - "log-link"                     Log life cycle of m0_ha_link
 *                                  (M0_WARN level).
 * - "log-msg"                      Log info about sent/received messages,
 *                                  including delivery status (M0_WARN level).
 *
 * The options can appear anywhere in the string, the code checks if the option
 * is present with strstr(). Example: "log-link, log-msg".
 *
 */
int m0_halon_interface_init(struct m0_halon_interface **hi_out,
                            const char                 *build_git_rev_id,
                            const char                 *build_configure_opts,
                            const char                 *debug_options,
                            const char                 *node_uuid);

/**
 * Finalises everything has been initialised during the init() call.
 * Mero functions shouldn't be used after this call.
 *
 * @note This function should be called from the exactly the same thread init()
 * has been called.
 *
 * @see m0_halon_interface_init()
 */
void m0_halon_interface_fini(struct m0_halon_interface *hi);

/**
 * Starts everything needed to handle entrypoint requests and
 * m0_ha_msg send/recv.
 *
 * @param local_rpc_endpoint    the function creates local rpc machine with this
 *                              endpoint in the current process. All network
 *                              communications using this m0_halon_interface
 *                              uses this rpc machine.
 * @param process_fid           process fid of the current process
 * @param ha_service_fid        HA service fid inside the current process
 * @param rm_service_fid        RM service fid inside the current process
 * @param entrypoint_request_cb this callback is executed when
 *                              entrypoint request arrives.
 * @param msg_received_cb       this callback is executed when
 *                              a new message arrives.
 *
 * - entrypoint_request_cb()
 *   - for each callback the m0_halon_interface_entrypoint_reply() should
 *     be called with the same req_id value;
 *   - req_id is assigned by m0_halon_interface;
 *   - remote_rpc_endpoint parameter contains rpc endpoint from which the
 *     entrypoint request has been received;
 * - msg_received_cb()
 *   - it's called when the message is received by the local rpc machine;
 *   - m0_halon_interface_delivered() should be called for each message
 *     received.
 * - msg_is_delivered_cb()
 *   - it's called when the message is delivered to the destination;
 * - msg_is_not_delivered_cb()
 *   - it's called when the message is not guaranteed to be delivered to the
 *     destination.
 * - link_connected_cb()
 *   - it's called when a new link is established;
 * - link_reused_cb()
 *   - it's called when an existing link is reused for the entrypoint request
 *     made with req_id;
 * - link_is_disconnecting_cb()
 *   - it's called when link is starting to disconnect from the opposite
 *     endpoint. After m0_halon_interface_disconnect() is called for the link
 *     and msg_is_delivered_cb() or msg_is_not_delivered_cb() is called for the
 *     each message sent over the link link_is_disconnected_cb() is executed;
 * - link_is_disconnected_cb()
 *   - it's called at the end of m0_ha_link lifetime. m0_halon_interface_send()
 *     shouldn't be called for the link after this callback is executed.
 *     msg_received_cb(), msg_is_delivered_cb() and msg_is_not_delivered_cb()
 *     are not going to be called for the link after the callback is executed.
 */
int m0_halon_interface_start(struct m0_halon_interface *hi,
                             const char                *local_rpc_endpoint,
                             const struct m0_fid       *process_fid,
                             const struct m0_fid       *ha_service_fid,
                             const struct m0_fid       *rm_service_fid,
                             void                     (*entrypoint_request_cb)
				(struct m0_halon_interface         *hi,
				 const struct m0_uint128           *req_id,
				 const char             *remote_rpc_endpoint,
				 const struct m0_fid    *process_fid,
				 const char             *git_rev_id,
				 uint64_t                pid,
				 bool                    first_request),
			     void                     (*msg_received_cb)
				(struct m0_halon_interface *hi,
				 struct m0_ha_link         *hl,
				 const struct m0_ha_msg    *msg,
				 uint64_t                   tag),
			     void                     (*msg_is_delivered_cb)
				(struct m0_halon_interface *hi,
				 struct m0_ha_link         *hl,
				 uint64_t                   tag),
			     void                     (*msg_is_not_delivered_cb)
				(struct m0_halon_interface *hi,
				 struct m0_ha_link         *hl,
				 uint64_t                   tag),
			     void                    (*link_connected_cb)
			        (struct m0_halon_interface *hi,
				 const struct m0_uint128   *req_id,
			         struct m0_ha_link         *link),
			     void                    (*link_reused_cb)
			        (struct m0_halon_interface *hi,
				 const struct m0_uint128   *req_id,
			         struct m0_ha_link         *link),
			     void                    (*link_absent_cb)
			        (struct m0_halon_interface *hi,
				 const struct m0_uint128   *req_id),
			     void                    (*link_is_disconnecting_cb)
			        (struct m0_halon_interface *hi,
			         struct m0_ha_link         *link),
			     void                     (*link_disconnected_cb)
			        (struct m0_halon_interface *hi,
			         struct m0_ha_link         *link));

/**
 * Stops sending/receiving messages and entrypoint requests.
 */
void m0_halon_interface_stop(struct m0_halon_interface *hi);

/**
 * Sends entrypoint reply.
 *
 * @param req_id         request id received in the entrypoint_request_cb()
 * @param rc             return code for the entrypoint.
 *                       It's delivered to the user
 * @param confd_nr       number of confds
 * @param confd_fid_data array of confd fids
 * @param confd_eps_data array of confd endpoints
 * @param confd_quorum   confd quorum for rconfc. @see m0_rconfc::rc_quorum
 * @param rm_fid         Active RM fid
 * @param rp_eps         Active RM endpoint
 *
 * @note This function can be called from entrypoint_request_cb().
 */
void m0_halon_interface_entrypoint_reply(
                struct m0_halon_interface  *hi,
                const struct m0_uint128    *req_id,
                int                         rc,
                uint32_t                    confd_nr,
                const struct m0_fid        *confd_fid_data,
                const char                **confd_eps_data,
                uint32_t                    confd_quorum,
                const struct m0_fid        *rm_fid,
                const char                 *rm_eps);

/**
 * Send m0_ha_msg using m0_ha_link.
 *
 * @param hl  m0_ha_link to send
 * @param msg msg to send
 * @param tag message tag is returned here.
 */
void m0_halon_interface_send(struct m0_halon_interface *hi,
                             struct m0_ha_link         *hl,
                             const struct m0_ha_msg    *msg,
                             uint64_t                  *tag);

/**
 * Notifies remote side that the message is delivered. The remote side will not
 * resend the message if Halon crashes and then m0d reconnects again after this
 * call.
 *
 * - this function should be called for all messages received in
 *   msg_received_cb();
 * - this function can be called from the msg_received_cb() callback.
 */
void m0_halon_interface_delivered(struct m0_halon_interface *hi,
                                  struct m0_ha_link         *hl,
                                  const struct m0_ha_msg    *msg);


/**
 * Notifies m0_halon_interface that no m0_halon_interface_send() will be called
 * for this link.
 *
 * The function should be called after link_is_disconnecting_cb() is called.
 */
void m0_halon_interface_disconnect(struct m0_halon_interface *hi,
                                   struct m0_ha_link         *hl);

/**
 * Returns rpc machine created during m0_halon_interface_start().
 * Returns NULL if m0_halon_interface is not in WORKING state.
 *
 * The rpc machine should not be used after m0_halon_interface_stop() is called.
 *
 * @note This function may be removed in the future. It exists only to make
 * m0_rpc_machine available for Spiel.
 */
struct m0_rpc_machine *
m0_halon_interface_rpc_machine(struct m0_halon_interface *hi);

/**
 * Returns request handler created during m0_halon_interface_start().
 * Returns NULL if m0_halon_interface is not in WORKING state.
 *
 * The reqh should not be used after m0_halon_interface_stop() is called.
 *
 * @note This function may be removed in the future. It exists only to make
 * m0_reqh available for Spiel.
 * @see m0_halon_interface_rpc_machine()
 */
struct m0_reqh *m0_halon_interface_reqh(struct m0_halon_interface *hi);

/**
 * Returns spiel instance initialised during m0_halon_interface_start().
 * Returns NULL if m0_halon_interface is not in WORKING state.
 *
 * The spiel instance should not be used after m0_halon_interface_stop() is
 * called.
 */
struct m0_spiel *m0_halon_interface_spiel(struct m0_halon_interface *hi);

M0_INTERNAL int m0_halon_interface_thread_adopt(struct m0_halon_interface *hi,
						struct m0_thread *thread);
M0_INTERNAL void m0_halon_interface_thread_shun(void);

/** @} end of ha group */
#endif /* __MERO_HA_HALON_INTERFACE_H__ */

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
