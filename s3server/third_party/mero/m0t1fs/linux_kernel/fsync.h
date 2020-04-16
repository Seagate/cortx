/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED, A SEAGATE COMPANY
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
 * Original authors: James Morse   <james_morse@xyratex.com>,
 *                   Juan Gonzalez <juan_gonzalez@xyratex.com>,
 *                   Sining Wu     <sining_wu@xyratex.com>
 * Original creation date: 01-Apr-2014
 */


#include <linux/version.h>              /* LINUX_VERSION_CODE */

#include "mdservice/fsync_fops.h"       /* m0_fop_fsync_mds_fopt */
#include "fop/fop.h"                    /* m0_fop */

#pragma once

#ifndef __MERO_M0T1FS_FSYNC_H__
#define __MERO_M0T1FS_FSYNC_H__

/**
   @page DLD-fsync Fsync DLD

   - @ref DLD-fsync-overview
   - @ref DLD-fsync-funcspec
   - @ref DLD-fsync-ds
   - @ref DLD-fsync-logspec
   - @ref DLD-fsync-futurework
   - @ref DLD-fsync-errors
   - @ref DLD-fsync-tests
   - @ref fsync-metadata
   - @ref fsync-metadata-rq
   - @ref fsync-metadata-refs

   <hr/>

   @section DLD-fsync-overview Overview

   Fsync is a syscall provided by POSIX to 'write out' all changes to a file
   that are currently held in memory to persistant storage. This syscall is
   used by applications to ensure that data can be retrieved even if the
   system crashes or is rebooted. Fsync writes out both file data and
   metadata, and blocks the application until this information is written
   to persistent storage.

   Mero backend (be) is used for metadata and FOL records, and has a
   transaction based interface.

   Fsync is implemented by m0t1fs as a function to send an 'fsync' fop to each
   server that has pending transactions, the reply is sent by each server once
   the transaction ID specified in the request has become committed. Fsync
   has both an active and a passive mode. In passive mode the fsync fom merely
   waits for the transactions to become committed, in active mode it uses
   m0_be_tx_force(), to cause the transactions to make progress more quickly
   than they otherwise would.


   @section DLD-fsync-funcspec Functional Specification

   m0t1fs_fsync() sends a fop to a set of servers to monitor the progress of
   a range of transactions. The fop specifies an active/passive mode, a
   transaction ID and a locality ID.

   The server will send a reply when transactions up to and including the
   specified transaction ID have been committed. In passive mode the fsync
   state machine will wait until the specified transaction has been
   committed, at which point it will reply. In active mode the fsync machine
   will invoke m0_be_tx_force() to cause transaction-commit to complete in
   the shortest possible time and then it will wait. In both cases the reply
   fop specifies a committed transaction ID, which may be greater than the ID
   in the request.

   @section DLD-fsync-ds Data Structures
@code
//
// fsync request fop
//
struct m0_fop_fsync {
        // the transaction to monitor, reply when this is committed
        uint64_t ff_be_txid;

        // locality of the target tx
        size_t   ff_be_locality;

        // store one of enum m0_fsync_mode
        uint32_t ff_fsync_mode;
} M0_XCA_RECORD;

enum m0_fsync_mode {
        M0_FSYNC_MODE_PASSIVE,
        M0_FSYNC_MODE_ACTIVE,
};
@endcode

@code
//
// fsync reply fop
//
struct m0_fop_fsync_rep {
        // return code for the fsync operation. 0 for success
        uint32_t ffr_rc;

        // the committed transaction, may be later than requested
        uint64_t ffr_be_committed_txid;
} M0_XCA_RECORD;
@endcode

   @section DLD-fsync-logspec Logical Specification

   fsync adds a 'uint64_t <prefix>_be_txid' to each fop-reply type that causes
   FOL records to be generated. At the time of writing these are...

   From ioservice/io_fops.h:
      - write          m0_fop_cob_rw_reply
      - writev         m0_fop_cob_writev_rep
      - cob-create     m0_fop_cob_op_reply
      - cob-delete     m0_fop_cob_op_reply

   From mdservice/md_fops.h:
      - create         m0_fop_create_rep
      - link           m0_fop_link_rep
      - unlink         m0_fop_unlink_rep
      - open           m0_fop_open_rep
      - setattr        m0_fop_setattr_rep
      - setxattr       m0_fop_setxattr_rep
      - delxattr       m0_fop_delxattr_rep
      - rename         m0_fop_rename_rep

   @note <prefix>_be_txid is added to the fop-replies inside a new M0_XCA_RECORD
   m0_fop_mod_rep, which includes all those data that are common to several
   replies.

   @note write uses the same reply-type as read, the <prefix>_be_txid field will
         be present in both read and write replies, it must be initialised to
         0 for read replies.

   @note cob-create and cob-delete share a reply type.

   @note open can currently cause setattr to be called (when nlink changes),
         in the future it may also cause 'atime' to be updated.

   Code in ioservice and mdservice adds the be transaction ID m0_be_tx::t_id
   to the reply fop.

   Code in m0t1fs/file and m0t1fs/dir adds a mapping between senders and the
   largest-seen transaction ID for this open file (or directory). This is
   stored as a new member of struct m0t1fs_inode::ci_pending_tx
   for both files and directories.

   @note a sender should be identified by m0_reqh_service_ctx which
   represents a 1:1 relationship between mounted m0t1fs (super block) and a
   service instance (e.g., mdservice, ioservice).

   The mapping should be stored as an m0_tlist, while any file could be
   striped accross every server in the cluster, the records are almost always
   processed as a set. A new struct would be required that holds the mapping
   of senders to the largest-seen transaction m0_reqh_service_txid.
   These should be allocated when needed (when a reply with a transaction ID
   is received), and free'd when the file (or directory) is closed.

   When fsync() is called for an open file or directory, m0t1fs should first
   call simple_fsync() to ensure all dirty pages have been pushed through
   writeback and are pending on the server. simple_fsync() will block until
   all of its writes have completed, at which point we will have an up-to-date
   maximum transaction ID for each sender. The m0t1fs fsync code should then
   process each sender in the mapping and send an fsync fop specifying the
   maximum transaction ID seen for this sender, the ID for the locality where
   that transaction lives in and the active/passive flag. fsync() then waits
   for a reply for each fop before proceeding.

   On the ioserver/mdserver, the fsync-fop's fom uses m0_be_engine__tx_find()
   to find the transaction pointed-to from the fop request and performs
   different actions depending on the passive/active flag:

       - Active mode:   m0_be_tx_force() gets called to close the transaction's
                        group and trigger its immediate placing. The code then
                        waits until the transaction's state changes to
                        M0_BTS_PLACED.

       - Passive mode:  Instead of explictly calling m0_be_tx_force() to trigger
                        transaction logging as Active mode does, Passive mode
                        simply waits until the transaction's state changes to
                        M0_BTS_PLACED (tx_group fom will handle the
                        transaction logging in the background).

   In both cases, fsync-fom calls m0_fom_wait_on() to wait on tx->t_sm.sm_chan
   so it gets woken up when the transaction's state changes.

   When fsync-fop completes, a reply is generated, which includes the ID
   of the target transaction.

   m0_be_tx_force() checks the state of the transaction is M0_BTS_CLOSED
   and invokes m0_be_tx_group_close() on the transaction's group, using
   M0_TIME_IMMEDIATELY as the timeout. The group gets then closed and all
   its transactions get immediately logged and written in-place.

   Once a reply for a fsync-fop has been received, the corresponding entry
   in the mapping could be removed if its recorded maximum transaction ID is
   less than or equal to the transaction ID that was committed (for example
   the recorded maximum transaction ID may be 4, an fsync-fop is sent with
   value 4, and a reply is received with value 12 - due to transactions for
   other objects or clients that were committed in the intervening time).
   It is also possible that new transactions are started after fsync was
   called, and that the maximum transaction ID has increased since we started
   waiting.


   @section DLD-fsync-futurework Future Work

   -# VFS also provides a mechanism for synchronising the contents of a whole
      mount-point using the syncfs syscall which (eventually) calls sync_fs()
      as a super block operation. To allow this call to be implemented later,
      the fsync code should maintain a mount-point-wide map of sender to
      maximum transaction IDs in the super block. This should be updated
      whenever a new reply is received, and have its records pruned whenever
      a fsync-fop-reply is received.
   -# There is no scope for cancelling an fsync operation, and an application
      may become blocked until it completes. To allow higher-priority
      conflicting operations to occur (such as killing the application, or
      umount-ing m0t1fs), an fsync-cancel ioctl could be provided. It should
      specify a process id and file descriptor ID (both easily discovered
      through /proc), which causes the fsync operation to complete early
      (with a failure).
   -# A m0_be_engine::eng_last_tx_placed field could be used to keep track of
      the ID of the last transaction placed by the engine. This value can then
      be returned as part of every fsync fop reply, so clients know the current
      maximum transaction ID of a sender and can save some fsync fop requests.


   @section DLD-fsync-errors Error handling
   - no reply to fsync-fop
        POSIX requires that the application calling fsync() will be blocked
        until the action has been completed (fsync-reply has been received)
        [1]. The RPC layer will take care of lost requests and replies, but
        other events may cause the reply to be delayed indefinitely. This
        error is not handled, there is no timeout for fsync-fops.

   - transaction-to-sync hasn't arrived yet
        Clients should only receive transaction IDs from reply fops, so there
        is no case where an fsync-fop can specify a future transaction that
        has not yet been received (let alone replied to). This is considered
        a fatal error, and an fsync reply fop is sent immediately with
        'EINVAL' as the return code in 'ffr_rc'.

        This error could also occur if rpc_items are replayed in a different
	order, the m0_rpc_session::s_xid field suggests that any replay will
        occur in the original order. This scenario is not considered.

   - transaction ID overflow
        It is assumed that no server/target/device will survive the 18
        quintillion transactions needed to overflow a 64 bit counter, and
        thus transaction ID overflow does not need to be considered.

   - invalid mode received
        If the server fom receives a mode it does not recognise, it must send
        a reply with the value 'EINVAL' as the return code in 'ffr_rc'.

   - no rpc_conn is available for that sender
        Between receiving a reply, and fsync() being called, the server may fail
        or become unavailable. In this case an entry in the mapping will exist,
        but it is not possible for fsync() to send a fop to the failed server.

        In this case fsync() should interact with DTM to determine whether the
        transaction was replayed to a different node, and committed there. For
        now fsync() should just assume the server could have failed after
        receiving the message.

   - simple_fsync() failed partially or completely
        If any of the IO operations triggered by simple_fsync() fails, the
        fsync() syscall must abort and return 'EIO'.

   - error when updating the mapping between senders and max. transaction IDs
        Not having a valid and updated maximum transaction ID for a file means
        the file cannot be fsync'ed properly. It needs to be assessed how
        likely it is an error is detected at that stage.

        - to mitigate this risk, the 'm0_reqh_service_txid' is cached
          between sending an fsync-fop and receiving its reply. These records
          are only free'd when an inode is destroyed, so there is no scope
          for the record being not-found. The remaining case where this could
          happen is if the reply code is unable to obtain the required locks,
          indicating a deadlock, which the kernel will detect and 'handle'.

   - 'force functions' might fail
        Forcing a transaction seeks to shorten the time a transaction is kept
        waiting as part of an IO optimization mechanism. Even if this functions
        fail, the transaction is expected to be eventually logged and written
        and thus no special measures are required in this case.



   @section DLD-fsync-tests Tests
   - send fsync-fop expect fsync-reply-fop
   - check the service->txid mapping is cleaned up as appropriate

   - send fsync-fop receive fsync-reply-fop with a higher ID than expected
   - check service->txid mapping is cleaned up as appropriate

   - send fsync-fop for a transaction in the future, expect fsync-reply-fop with
     EINVAL ffr_rc

   - send fsync-fop, cause max_txid to increase before the reply is received
   - check service->txid mapping isn't cleaned up

   - Can't easily test a 'never completes' scenario - fault injection and
     test-timeout?


*/
/**
  @section fsync-metadata Metadata

  @section fsync-metadata-rq Requirements

  - @b R.DLD.POSIX - implemented operations should conform to POSIX.

  - @b R.DLD.UNIFORM - all fops that generate FOL records must be
    synchronised by fsync.

  - @b R.DLD.ACTIVEORPASSIVE - fsync must be either active or passive.

  - @b R.DLD.TXNOPTIONAL - Some operations (namely open) may not generate FOL
    records,it should be obvious from the reply that no new transaction
    occurred.

   @section fsync-metadata-refs References
      [1] POSIX fsync -
          http://pubs.opengroup.org/onlinepubs/009695399/functions/fsync.html

 */

/* import */
struct m0t1fs_inode;
struct m0t1fs_sb;
struct m0_reqh_service_ctx;


/**
 * Wrapper for fsync messages, used to list/group pending replies
 * and pair fop/reply with the struct m0_reqh_service_txid
 * that needs updating.
 */
struct m0t1fs_fsync_fop_wrapper {
	/** The fop for fsync messages */
	struct m0_fop                ffw_fop;

	/**
	 * The service transaction that needs updating
	 * gain the m0t1fs_inode::ci_pending_txid_lock lock
	 * for inodes or the m0_reqh_service_ctx::sc_max_pending_tx_lock
	 * for the super block before dereferencing
	 */
	struct m0_reqh_service_txid *ffw_stx;

	struct m0_tlink              ffw_tlink;
	uint64_t                     ffw_tlink_magic;
};

/**
 * Ugly abstraction of m0t1fs_fsync interactions with wider mero code
 * - purely to facilitate unit testing.
 * - this is used in fsync.c and its unit tests.
 */
struct m0t1fs_fsync_interactions {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
	int (*kernel_fsync)(struct file *file, loff_t start, loff_t end,
	                    int datasync);
#else
	int (*kernel_fsync)(struct file *file, struct dentry *dentry,
	                    int datasync);
#endif
	int (*post_rpc)(struct m0_rpc_item *item);
	int (*wait_for_reply)(struct m0_rpc_item *item, m0_time_t timeout);
	void (*fop_fini)(struct m0_fop *fop);
	void (*fop_put)(struct m0_fop *fop);
};

/**
 * m0t1fs fsync entry point
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
int m0t1fs_fsync(struct file *file, loff_t start, loff_t end, int datasync);
#else
int m0t1fs_fsync(struct file *file, struct dentry *dentry, int datasync);
#endif


/**
 * Updates fsync records in fop callbacks.
 * Service must be specified, one or both of csb/inode should be specified.
 * new_txid may be null.
 */
void m0t1fs_fsync_record_update(struct m0_reqh_service_ctx *service,
				struct m0t1fs_sb           *csb,
				struct m0t1fs_inode        *inode,
				struct m0_be_tx_remid      *btr);


/**
 * Create and send an fsync fop from the provided m0_reqh_service_txid.
 */
M0_INTERNAL int
m0t1fs_fsync_request_create(struct m0_reqh_service_txid      *stx,
			    struct m0t1fs_fsync_fop_wrapper **ffw,
			    enum m0_fsync_mode                mode);


/**
 * Wait for a reply to an fsync fop and process it.
 * Cleans-up the fop allocated in m0t1fs_fsync_request_create.
 *
 * inode may be NULL if the reply is only likely to touch the super block.
 * csb may be NULL, iff inode is specified.
 *
 */
M0_INTERNAL int
m0t1fs_fsync_reply_process(struct m0t1fs_sb                *csb,
			   struct m0t1fs_inode             *inode,
			   struct m0t1fs_fsync_fop_wrapper *ffw);


/**
 * m0t1fs sync_fs entry point
 */
int m0t1fs_sync_fs(struct super_block *sb, int wait);

/**
 * m0t1fs fsync core sends an fsync-fop to a list of services, then blocks,
 * waiting for replies. This is implemented as two loops.
 * The 'fop sending loop', generates and posts fops, adding them to a list
 * of pending fops. This is all done while holding the
 * m0t1fs_indode::ci_pending_txid_lock. The 'reply receiving loop'
 * works over the list of pending fops, waiting for a reply for each one.
 * It acquires the m0t1fs_indode::ci_pending_txid_lock only
 * when necessary.
 */
M0_INTERNAL int m0t1fs_fsync_core(struct m0t1fs_inode   *inode,
				  enum m0_fsync_mode     mode);

#endif /* __MERO_M0T1FS_FSYNC_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
