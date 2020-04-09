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
 * Original author: Yuriy Umanets <yuriy_umanets@xyratex.com>
 *                  Huang Hua <hua_huang@xyratex.com>
 *                  Anatoliy Bilenko <anatoliy_bilenko@xyratex.com>
 * Original creation date: 05/04/2010
 */

#pragma once

#ifndef __MERO_M0T1FS_M0T1FS_H__
#define __MERO_M0T1FS_M0T1FS_H__

#include <linux/fs.h>
#include <linux/version.h>        /* LINUX_VERSION_CODE */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#include <linux/backing-dev.h>
#endif
#include <linux/pagemap.h>

#include "lib/tlist.h"
#include "lib/hash.h"
#include "lib/mutex.h"
#include "lib/semaphore.h"
#include "net/net.h"             /* m0_net_domain */
#include "rpc/rpc.h"
#include "reqh/reqh.h"
#include "pool/pool.h"           /* m0_pool, m0_pool_version, m0_pools_common */
#include "net/buffer_pool.h"
#include "fid/fid.h"
#include "cob/cob.h"             /* m0_cob_domain_id */
#include "layout/layout.h"
#include "ioservice/io_fops.h"   /* m0_fop_cob_create_fopt */
#include "mdservice/md_fops.h"   /* m0_fop_create_fopt */
#include "file/file.h"           /* m0_file */
#include "be/ut/helper.h"
#include "spiel/spiel.h"         /* m0_spiel */
#include "m0t1fs/linux_kernel/m0t1fs_addb2.h"
#include "mero/ha.h"             /* m0_mero_ha */
#include "conf/confc.h"          /* m0_conf_state */

/**
  @defgroup m0t1fs m0t1fs

  @section Overview

  m0t1fs is a mero client file-system for linux. It is implemented as a
  kernel module.

  @section m0t1fsfuncspec Function Specification

  m0t1fs has flat file-system structure i.e. no directories except root.
  m0t1fs does not support caching. All read-write requests are directly
  forwarded to servers.

  By default m0t1fs uses end-point address 0@lo:12345:45:6 as its local
  address. This address can be changed with local_addr module parameter.
  e.g. to make m0t1fs use 172.18.50.40@o2ib1:12345:34:1 as its end-point address
  load module with command:

  sudo insmod m0mero.ko local_addr="172.18.50.40@o2ib1:12345:34:1"

  m0t1fs can be mounted with mount command:

  mount -t m0t1fs -o <options_list> dontcare <dir_name>

  where <options_list> is a comma separated list of option=value elements.
  Currently supported list of options is:

  - confd [value type: end-point address, e.g. 192.168.50.40@tcp:12345:34:1]
      end-point address of confd service.

  - profile [value type: string]
      configuration profile used while fetching configuration data from confd.

  - fid_start [value type: uint32_t, default: 4]
      TEMPORARY option.
      Client allocates fid for a file during its creation. It assigns
      <0, $fid_start> for first file, <0, $fid_start+1> for second
      and so on.
      mdstore uses constant fid - M0_MDSERVICE_SLASH_FID <1, 65535>
      for namespace root exposed to user.

      Fids below 65535 are reserved for special objects such as: storage root,
      ".mero", ".mero/fid" and potentially many others that will live in ".mero"
      directory.

      Hence fid_start must be greater than 65535.

   'device' argument of mount command is ignored.

   m0t1fs supports following operations:
   - Creating a regular file
   - Remove a regular file
   - Listing files in root directory
   - file read/write of full-stripe width

   @section m0t1fslogspec Logical Specification

   <B>mount/unmount:</B>

   m0t1fs establishes rpc-connections and rpc-sessions with all the services
   obtained from configuration data. If multiple services have same end-point
   address, separate rpc-connection is established with each service i.e.
   if N services have same end-point address, there will be N rpc-connections
   leading to same target end-point.

   The rpc-connections and rpc-sessions will be terminated at unmount time.

   <B> Pools and Pool versions: </B>

   m0t1fs can work with multiple pools and pool versions.
   A pool version comprises of a set of services and devices attached to them.
   Layout and pool machine are associated with the pool version. Pool version
   creates a device to io service map during initialisation.

   m0t1fs finds a valid pool and pool version to start with which has no devices
   from the failure set during mount. Once pool version is obtained, if its
   corresponding pool already exist them the pool version is added to it else
   new pool is initialised.

   <B> Containers and component objects: </B>

   An io service provides access to storage objects, md service provides
   access to md objects and rm service provides access to resources.
   Containers are used to migrate and locate object. Each container is
   identified by container-id. Storage objects and md objects are identified
   by fid which is a pair <container-id, key>. All objects belonging to same
   container have same value for fid.container_id which is equal to id of that
   container.

   "Container location map" maps container-id to service.

   Even if containers are not yet implemented, notion of container id
   is required, to be able to locate a service serving some object
   identified by fid.

   Currently m0t1fs implements simple (and temporary) mechanism to
   build container location map. Number of containers is equal to
   P + 2, where P is pool width and additional 2 containers are used by
   meta-data and resource management.
   Pool width is a file-system parameter, obtained from configuration.

   Assume a user-visible file F. A gob representing F is assigned fid
   <0, K>, where K is taken from a monotonically increasing counter
   (m0t1fs_sb::csb_next_key). Container-id 0 is mapped to md-service,
   by container location map. Container-id `P+2' is mapped to rm-service.

   There are P number of component objects of file F, having fids
   { <i, K> | i = 1, 2..., P}. Here P is equal to pool_width mount option.
   Mapping from <gob_fid, cob_index> -> cob_fid is implemented using
   linear enumeration (B * x + A) with both A and B parameters set to 1.
   Container location map, maps container-ids from 1 to P, to io-services.

   Container location map is populated at mount time and is a part of
   a pool version.

   <B> Directory Operations: </B>

   To create a regular file, m0t1fs sends cob create requests to mds
   (for global object aka gob) and io-service (for component
   objects). Because, mds is not yet implemented, m0t1fs does not send
   cob create request to any mds.  Instead all directory entries are
   maintained in an in-memory list in root inode itself.

   If component object creation fails, m0t1fs does not attempt to
   cleanup component objects that were successfully created. This
   should be handled by dtm component, which is not yet implemented.

   <B> Read/Write: </B>

   m0t1fs currently supports only full stripe IO
   i.e. (iosize % (nr_data_units * stripe_unit_size) == 0)

   read-write operations on file are not synchronised.

   m0t1fs does not cache any data.

   For simplicity, m0t1fs does synchronous rpc with io-services, to
   read/write component objects.
 */

/**
  @section m0t1fs-metadata Metadata

  @section m0t1fs-metadata-rq Requirements

  The following requirements should be met:

  - @b R.DLD.MDSERVICE - all md operations implemented by m0t1fs
  should talk to mdservice in order to provide required functionality. In
  other words they should not generate hardcoded attributes locally on client
  and cache in inode/dentry cache. Required functionality here - is to provide
  mounts persistence.

  - @b R.DLD.POSIX - implemented operations should conform to POSIX
 */

/**
  @section Overview

  The main direction of this work (@ref m0t1fs-metadata-rq) is mostly to follow
  normal linux filesystem driver requirements as for the number and meaning of
  operations handled by the driver.

  This means number of things as follows bellow:

  - the fops presented below, correspond to the usual operations implemented
  by filesystem driver, such as link, unlink, create, etc;
  - mdservice on server side is connected by m0t1fs at mount time. Root inode
  attributes and (optionally) filesystem information, such as free blocks, etc.,
  that is usually needed by statfs, can be retrieved by m0t1fs from mount fop
  response;
  - inode attributes may also contain some information needed to perform I/O
  (layout) and this data may be sent to client as part of getattr fop.

  Fops defined by mdservice are described at @ref mdservice-fops-definition
  In this work we use only some of them in order to provide persistence
  between mounts.

  They are the following:
  - m0_fop_create and m0_fop_create_rep
  - m0_fop_unlink and m0_fop_unlink_rep
  - m0_fop_setattr and m0_fop_setattr_rep
  - m0_fop_getattr and m0_fop_getattr_rep

  @section m0t1fs-metadata-fs Detailed Functional Specification

  Some functions will see modifications in order to implement the
  client-server communication and not just create all metadata in
  memory.

  Only minimal set of operations is needed to support persitency
  between mounts.

  In order to implement mount persistency functionality, the following
  functionality should be implemented:

  - get mdservice volume information enough for statfs and get root
  fid at mount time
  - no hierarchy is needed, that is, no directories support will be
  added in this work
  - list of regular files in root directory should be supported
  - operations with regular files should be supported

  @section Initialization

  m0t1fs_init() - m0_mdservice_fop_init() is added to initialize md
  operations fops.

  m0t1fs_fini() - m0_mdservice_fop_fini() is added to finalize md
  operations fops.

  m0t1fs_fill_super() -> m0t1fs_connect_to_services() - will have also
  to connect the mdservice.

  It also may have some fields from space csb->csb_* initialized
  not from mount options but rather from connect reply information.

  m0t1fs_kill_sb() -> m0t1fs_disconnect_from_services() - will have
  also to disconnect from mdservice.

  @section Inode operations

  m0t1fs_mknod() - is to be added to m0t1fs_dir_inode_operations.
  This is one of variants of create fop.

  m0t1fs_create() - sending create fop is added. Layout speicified
  with mount options or obtained in mount time is to be packed into
  create fop and sent to the server. Errors should be handled on
  all stages.

  m0t1fs_lookup() - sending getattr/lookup fop is added. Errors are
  handled.

  m0t1fs_unlink() - sending unlink fop is added. Errors are handled
  (not needed for this work).

  m0t1fs_link() - is to be added to m0t1fs_dir_inode_operations.
  This is normal hard-link create function that sends link fop to
  mdservice (not needed for this work).

  m0t1fs_mkdir() - is to be added to m0t1fs_dir_inode_operations.
  This function sends create fop initialized in slightly different
  way than for creating regular file (not needed for this work).

  m0t1fs_rmdir() - is to be added to m0t1fs_dir_inode_operations.
  This function sends unlink fop (not needed for this work).

  m0t1fs_symlink() - is to be added to m0t1fs_dir_inode_operations.
  This function sends create fop with mode initialized for symlinks
  (not needed for this work).

  m0t1fs_rename() - is to be added to m0t1fs_dir_inode_operations.
  This function sends rename fop (not needed for this work).

  m0t1fs_setattr() - is to be added to m0t1fs_dir_inode_operations
  and m0t1fs_special_inode_operations. This function sends setattr
  fop.

  m0t1fs_getattr() - is to be added to m0t1fs_dir_inode_operations
  and m0t1fs_special_inode_operations. This function sends getattr
  fop and returns server side inode attributes.

  m0t1fs_permission() - is to be added to m0t1fs_dir_inode_operations
  and m0t1fs_special_inode_operations (not needed for this work).

  The following extended attributes operations to be added. We need
  them in order to run lustre on top of mero.

  m0t1fs_setxattr() - is to be added to m0t1fs_dir_inode_operations
  and m0t1fs_special_inode_operations (not needed for this work).

  m0t1fs_getxattr() - is to be added to m0t1fs_dir_inode_operations
  and m0t1fs_special_inode_operations (not needed for this work).

  m0t1fs_listxattr() - is to be added to m0t1fs_dir_inode_operations
  and m0t1fs_special_inode_operations (not needed for this work).

  m0t1fs_removexattr() - is to be added to m0t1fs_dir_inode_operations
  and m0t1fs_special_inode_operations (not needed for this work).

  Mdservice does not support xattrs fop operations yet. We will add them
  later as part of xattr support work.

  @section File operations

  m0t1fs_open() - is to be added to m0t1fs_dir_file_operations and
  m0t1fs_reg_file_operations.

  This function sends open fop and handles reply and errors in a
  standard way.

  m0t1fs_release() - is to be added to m0t1fs_dir_file_operations and
  m0t1fs_reg_file_operations.

  This function sends close fop on last release call.

  m0t1fs_readdir() - sending readdir and/or getpage fop is added.
  Errors handling and page cache management code should be added.

  @sections Misc changes

  fid_hash() - hash should be generated including ->f_container
  and f_key. This can be used for generating inode numbers in future.
  Still, changing inode numbers allocation is out of scope of this work.
 */

/**
  @section m0t1fs-metadata-ls Logical Specification

  - @ref m0t1fs-metadata-fs
  - @ref m0t1fs-metadata-rq

  All the fs callback functions, implemented in m0t1fs filesystem driver,
  are to be backed with corresponding fop request communicating mdservice
  on server side.

 */

/**
  @section m0t1fs-metadata-cf Conformance

  - @b R.DLD.MDSERVICE - Add support of meta-data operations (above) to m0t1fs.
  Implement a set of fops to be used for communicating meta-data operations
  between m0t1fs and mdservice.

  - @b R.DLD.POSIX - following standard linux fs driver callback functions
  and reqs guarantees basic POSIX conformance. More fine-graned conformance
  may be met with using "standard" test suites for file sytems, such as dbench,
  which is mentioned in Testing section (@ref m0t1fs-metadata-ts). This will
  not be applied in current work as only miimal set of operations will be
  implemented to support between mounts persistence.
 */

/**
  @section m0t1fs-metadata-ts Testing

  In principle full featured m0t1fs should pass standard fs tests such as
  dbench as part of integration testing. This work scope testing will cover
  the basic functionality that includes mount, create files, re-mount and check
  if files exist and have the same attributes as before re-mount.

  As for unit testing, that may run on "before-commit" basis,  small and fast
  set of basic fs operations (10-20 operations) can be added to standard UT
  framework.

 */

/**
  @section m0t1fs-metadata-dp Dependencies

  - layout. It would be good to have layout functionality in place but without
  it most of work also can be done.
  - recovery. Recovery principles better to be ready to finish this work. Some
  tests may load client node so much that connection problems can occur.

 */

/**
 * @addtogroup m0t1fs
 * @{
 *
 * @page mero-cmd-HLD-DLD Mero Clustered Meta-data HLD and DLD
 *
 * - @ref Mero-CMD-ovw
 * - @ref Mero-CMD-def
 * - @ref Mero-CMD-req
 * - @ref Mero-CMD-depends
 * - @ref Mero-CMD-highlights
 * - @ref Mero-CMD-fspec
 * - @ref Mero-CMD-lspec
 *   - @ref Mero-CMD-lspec-schema
 * - @ref Mero-CMD-conformance
 * - @ref Mero-CMD-ut
 * - @ref Mero-CMD-st
 * - @ref Mero-CMD-O
 * - @ref Mero-CMD-ref
 *
 * <HR>
 * @section Mero-CMD-ovw Overview
 * This document contains the high level and detailed level design for the Mero
 * Clustered Meta-data (a.k.a CMD).
 *
 * More than a single meta-data server will be supported in Mero CMD, scattering
 * meta-data and meta-data operations across all Mero server nodes. Multiple
 * mdservices will be running in a Mero system.
 *
 * These mdservices are share-nothing: each is completely independent from
 * others and manages its own isolated subset of global objects. Each mdservice
 * has a full set of md tables: namespace, object index, layout, omg, fab, etc.
 * The only resource shared by mdservices is fid name space.
 * The same as current implementation, Mero client will continue to have all
 * its objects created in a single root directory. In Active-Archive product
 * (AA), file names of objects in root directory are uniform: some kind
 * of oostore-generated identifiers is used as its file name.
 *
 * @section Mero-CMD-def Definitions
 *  - CMD: Clustered Meta-data. With CMD, meta-data of Mero is stored across
 *         multiple mdservices. Meta-data operations are handled by these
 *         mdservices. Meta-data operations go to proper mdservice on which
 *         the related meta-data is stored.
 *  - AA:  Active Archive. Mero client is used in some archive system. Objects
 *         from archive system are stored in Mero client as a file in root
 *         directory.
 *  - FID: File IDentification. FID is used to identify an object uniquely in
 *         Mero.
 *  - confc/confd: Mero configuration client and configuration daemon.
 *         Mero configuration is system information about how a Mero system
 *         is configured. This information is used to configure and start
 *         various Mero services, networks, transports, clients, etc.
 *
 * @section Mero-CMD-req Requirements
 * The specified requirements are as follows:
 * - R.CMD: Mero system may run mdservices on multiple server nodes. Mero
 *          meta-data are stored across all these mdservices. Mero meta-data
 *          services are handled by these mdservices.
 * - R.CMD.share-0: These mdservices are independent from each other. Nothing
 *                  is shared among them, except that FID namespace is
 *                  distributed among all mdservices.
 * - R.CMD.conf: The configuration of mdservices is managed by confd. A Mero
 *               client gets this configuration by confc.
 * - R.CMD.namespace: Objects (files) are all stored root directory. A Mero
 *                    client gets a full list of all objects in root directory
 *                    from all mdservices.
 * - R.CMD.code-reuse: The code to read root directory should be structured
 *                     so that it can be re-used to support striping in
 *                     non-root directory easily.
 * - R.CMD.oostore: A oostore mode for mero client is needed. In this mode,
 *                   some oostore creates files in root directory. File name
 *                   is its FID.
 * - R.CMD.FID: Treat FID as a resource. FID is allocated by RM service, and fid
 *              to mdservice mapping is also managed by RM service.
 *
 * @section Mero-CMD-depends Dependency
 * This feature has no dependency on others right now.
 *
 *
 * @section Mero-CMD-highlights Highlights
 * N/A
 *
 * @section Mero-CMD-fspec Functional Specification
 * To achieve the requirements in @ref Mero-CMD-req step by step, the CMD
 * implementation will be divided into several stages.
 *
 * - Stage 0: At this first stage, the number of mdservices and its
 *            configuration are fixed when system is initialised and cannot be
 *            changed afterwords. No new mdservices can be added. If a
 *            mdservice fails completely, the global objects it manages become
 *            unaccessible. The list of mdservices is given by confd;
 *            Mount option, fid_start, is still given to client. We still assume
 *            unique fids are generated by clients.
 *            An algorithm will be used to map a fid to mdservice. So Mero client
 *            can talk to the right mdservice when creating, reading, writing
 *            files, and other meta-data operations.
 * - Stage OOSTORE: In this stage, A special m0t1fs mode (specified via a mount
 *                   option) is added, specifically designed to work with
 *                   oostore. In this mode, file name is a fid. Only names of
 *                   the form recognised by m0_fid_sscanf() are allowed in the
 *                   root directory. When a file with the name "C:K" is created,
 *                   its fid is taken to be C:K. This makes "start_fid" option
 *                   unnecessary.
 * - Stage RM-CLIENT: In this stage, Mero clients start using RM for fid
 *                    allocation. There is a special single instance of RM
 *                    service, that manages the entire fid name-space
 *                    (0, 0)---(~0ULL, ~0ULL). Mero clients request ranges of
 *                    fids from the service and use them for allocation of fid.
 * - Stage RM-SERVICE: In this stage, we add support for adding more mdservices
 *                     at runtime. There is a special instance of RM service
 *                     (different from one in RM-CLIENT) which manages the
 *                     ranges of fids assigned to services. An mdservice gets
 *                     range of fids from RM. Clients get the entire
 *                     {fid_range -> mdservice} list from RM and use it to map
 *                     fids to services. In addition each mdservice keeps record
 *                     of fids actually used by existing files (this range can
 *                     be calculated from object index table). When a new
 *                     service is added, it asks RM for a new range. RM requests
 *                     existing mdservices to cancel their fid regions and
 *                     mdservices cancel some *unused* ranges, that is ranges
 *                     of fids for which there are no existing files. The new
 *                     service gets the union of these ranges.
 *
 * For AA alpha, we will only implement stage 0 and stage OOSTORE.
 *
 * @section Mero-CMD-lspec Logical Specification
 * For stage 0 and stage OOSTORE, a simple data structure and a simple
 * algorithm will be used to map an object and its operations to the right
 * mdservice. Mero client has a "struct m0_reqh_service_ctx" for every
 * service, including mdservice, ioservice, and other service. These service
 * contexts are linked in a list. We will also use an array to store all the
 * pointers to these mds contexts.
 * @code
 *	struct m0_reqh_service_ctx *mds_map[M0T1FS_MAX_MDS_NR];
 * #endofcode
 * A simple function is used to map a file to mdservice by hashing its file
 * name:
 * @code
 *        mdservice_idx = hash(filename) % mdservice_nr;
 * @endofcode
 * The mdservice_idx is the index in array mds_map. Thus, the operations on this
 * file will find the right mdservice to go for. Currently, rename operation
 * is not supported in Mero. Link operation will be disabled after this
 * feature is landned.
 *
 * The m0t1fs_readdir() will be modified to read directory entries from all
 * mdservices (or a sub-set of all, like a pool). READDIR requests will be sent
 * to these mdservices and EOF (end of file) will be set when all mdservices
 * have been iterated. This is like the root directory splits into all
 * mdservices. A mdservice index will be added into "struct m0t1fs_filedata" to
 * indicate the current mdservice that readdir() position is at.
 * @code
 *        struct m0t1fs_filedata {
 *                int                        fd_direof;
 *                struct m0_bitstring       *fd_dirpos;
 *                uint32_t                   fd_mds_index;
 *      };
 * @endofcode
 *
 * In normal mode, FID of new file is allocated by client. The mount option
 * fid_start helps client to generate unique FID. There will be a special and
 * temporary mode, OOSTORE mode of Mero client, and in this mode the FID of
 * new file is taken from the file name. (After stage RM-CLIENT, fid will be
 * managed by RM service.) File name of a new file is anything that
 * m0_fid_sscanf() can understand. FID of new file is taken from that.
 *
 * @todo Design for RM-CLIENT and RM-SERVER will be added later.
 *
 * @subsection Mero-CMD-lspec-schema Schema
 * Configuration of multiple mdservices is managed by confd, and provided to
 * Mero client via confc. The schema for mdservice is similar to that for
 * ioservice. Mero client startup code will parse this configuration and setup
 * mdservice and mapping array.
 *
 * @section Mero-CMD-conformance Conformance
 * - I.CMD: Multiple mdservices can be started in a Mero system, and clients
 *          can connect to these mdservices. Mero clients map objects to proper
 *          mdservice. Meta-data operations go to the right mdservice.
 * - I.CMD.share: Mero mdservice shares nothing, and is independent.
 * - I.CMD.conf: The configuration of mdservices is managed by confd. The
 *   		 mdservice schema is similar to that of ioservice. Mero clients
 *   		 get mdservice configuration from confd via confc.
 * - I.CMD.namespace: All objects are stored in root directory. Listing of root
 *                    directory from client will readdir() from all mdservices
 *                    and form a global name space.
 * - I.CMD.code-reuse: No particular hacking is used to gather the directory
 *                     entries for root dir. All code will be reusable for
 *                     non-root directory.
 * - I.CMD.oostore: A oostore mode for mero client is added. In this mode,
 *                   File name is a FID, the format of "C:K". FID of new file
 *                   is taken from its name.
 * - I.CMD.FID: @todo This will be implemented in RM-CLIENT and RM-SERVER stage.
 *
 * @section Mero-CMD-ut Unit Tests
 * Existing unit test are still valid and useful. The following unit tests are
 * added to verify new functionalities.
 *
 * Unit Test 1: Good configuration for mdservice. Mero client starts correctly.
 * Unit Test 2: Invalid configurations for mdservice. Mero client reports error
 *              and failed to start.
 * Unit Test 3: Different FID maps to correct mdservice index. mdservices are
 *              always in valid range.
 * Unit Test 4: readdir() can switch from one mds to another mds, and finally
 *              iterate all mdservice.
 * Unit Test 5: In OOSTORE mode, FID can be parsed from new file with valid
 *              file name.
 * Unit Test 6: In OOSTORE mode, FID cannot be parsed from new file with
 *              invalid file name, and error is reported.
 *
 * @section Mero-CMD-st System Tests
 * The following new system tests will be added.
 * System Test 1: Some files are created in root dir and readdir() will get a
 *                full list of these files without error.
 * System Test 2: Multiple mdservices are configured in confd, and Mero clients
 *                can start up correctly.
 *
 * @section Mero-CMD-O Performance
 * N/A
 *
 * @section Mero-CMD-ref References
 * N/A
 *
 * @}
 */

struct m0_pdclust_layout;

M0_INTERNAL int m0t1fs_init(void);
M0_INTERNAL void m0t1fs_fini(void);

enum {
	M0T1FS_RPC_TIMEOUT              = 100,   /* seconds */
	M0T1FS_RPC_MAX_RETRIES          =  10,
	M0T1FS_RPC_RESEND_INTERVAL      = M0_MKTIME(M0T1FS_RPC_TIMEOUT, 0) /
					  M0T1FS_RPC_MAX_RETRIES,
	M0T1FS_MAX_NR_RPC_IN_FLIGHT     = 100,
	M0T1FS_DEFAULT_NR_DATA_UNITS    = 1,
	M0T1FS_DEFAULT_NR_PARITY_UNITS  = 1,
	M0T1FS_DEFAULT_STRIPE_UNIT_SIZE = PAGE_SIZE,
	M0T1FS_MAX_NR_CONTAINERS        = 4096,
	M0T1FS_COB_ID_STRLEN            = 34,
};

/** Represents type of IO request. */
enum io_req_type {
	IRT_READ,
	IRT_WRITE,
	IRT_TYPE_NR,
};

/**
   In memory m0t1fs super block. One instance per mounted file-system.
   super_block::s_fs_info points to instance of this type.
 */
struct m0t1fs_sb {
	struct m0_pools_common                  csb_pools_common;

	/** Total number of containers. */
	uint32_t                                csb_nr_containers;

	/** used by temporary implementation of m0t1fs_fid_alloc(). */
	uint64_t                                csb_next_key;

	/** mutex that serialises all file and directory operations */
	struct m0_mutex                         csb_mutex;

	/**
	 * Flag indicating if m0t1fs mount is active or not.
	 * Flag is set when m0t1fs is mounted and is reset by unmount thread.
	 */
	bool                                    csb_active;

	/**
	 * Instantaneous count of pending io requests.
	 * Every io request increments this value while initializing
	 * and decrements it while finalizing.
	 */
	struct m0_atomic64                      csb_pending_io_nr;

	/** Special thread which runs ASTs from io requests. */
	struct m0_thread                        csb_astthread;

	/**
	 * Channel on which unmount thread will wait. It will be signalled
	 * by AST thread while exiting.
	 */
	struct m0_chan                          csb_iowait;

	/** State machine group used for all IO requests. */
	struct m0_sm_group                      csb_iogroup;

	/** Root fid, retrieved from mdservice in mount time. */
	struct m0_fid                           csb_root_fid;

	/** Maximal allowed namelen (retrived from mdservice) */
	int                                     csb_namelen;

	struct m0_net_xprt                     *csb_xprt;
	/** local endpoint address module parameter */
	char                                   *csb_laddr;
	struct m0_net_domain                    csb_ndom;
	struct m0_rpc_machine                   csb_rpc_machine;
	struct m0_reqh                          csb_reqh;
	struct m0_net_buffer_pool               csb_buffer_pool;

	struct m0_be_ut_backend                 csb_ut_be;
	struct m0_be_ut_seg                     csb_ut_seg;

	/** lnet tmid for client ep */
	size_t                                  csb_tmid;

	/** /.mero virtual dir dentry */
	struct dentry                          *csb_mero_dentry;

	/** /.mero/fid virtual dir dentry */
	struct dentry                          *csb_fid_dentry;

	/** Superblock from VFS in which csb is present in sb_fs_info. */
	struct super_block                     *csb_sb;
	/** virtual dirs cached attributes */
	struct m0_fop_cob                       csb_virt_body;

	/** oostore mode */
	bool                                    csb_oostore;
	/** verify mode: verify parity on read */
	bool                                    csb_verify;

	/** HA service context. */
	struct m0_reqh_service_ctx             *csb_ha_rsctx;

	struct m0_mero_ha                       csb_mero_ha;

	/** List of m0t1fs inodes linked through m0t1fs_inode::ci_sb_linkage. */
	struct m0_tl                            csb_inodes;
	struct m0_mutex                         csb_inodes_lock;

	/**
	 * List of pending transactions, by service,
	 * protected by csb_service_pending_txid_map_lock
	 */
	struct m0_htable                        csb_service_pending_txid_map;
	struct m0_mutex                         csb_service_pending_txid_map_lock;

	/**
	 * This clink gets subscribed to "conf has expired" event, broadcasted
	 * at m0_reqh::rh_conf_cache_exp cahnnel.
	 */
	struct m0_clink                         csb_conf_exp;
	/**
	 * This clink gets subscribed to "new conf is ready" event, broadcasted
	 * at m0_reqh::rh_conf_cache_ready cahnnel.
	 */
	struct m0_clink                         csb_conf_ready;
	/**
	 * This clink gets subscribed to "new conf is ready" event, broadcasted
	 * at m0_reqh::rh_conf_cache_ready_async channel.
	 */
	struct m0_clink                         csb_conf_ready_async;

	/**
	 * io and md requests wait on this channel till the revoked conf
	 * is restored.
	 */
	struct m0_chan                          csb_conf_ready_chan;

	/** Indicates the state of confc update with respect to csb.  */
	struct m0_confc_update_state            csb_confc_state;

	/** Number of active requests under csb. */
	uint64_t                                csb_reqs_nr;

	struct m0_fid                           csb_process_fid;
	struct m0_fid                           csb_profile_fid;

	struct m0_reqh_service                 *csb_rm_service;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
	struct backing_dev_info                 csb_backing_dev_info;
#endif
};

struct m0t1fs_filedata {
	int                        fd_direof;
	struct m0_bitstring       *fd_dirpos;
	uint32_t                   fd_mds_index;
};

/**
   Metadata operation helper structure.
 */
struct m0t1fs_mdop {
	struct m0_cob_attr    mo_attr;
	enum m0_cob_type      mo_cob_type;
	struct m0_layout     *mo_layout;
	bool                  mo_use_hint; /**< if true, mo_hash_hint is valid*/
	uint32_t              mo_hash_hint;/**< hash hint for mdservice map   */
};

M0_TL_DESCR_DECLARE(ispti, extern);
M0_TL_DECLARE(ispti, extern, struct m0_reqh_service_txid);

/**
   Inode representing global file.
 */
struct m0t1fs_inode {
	/** vfs inode */
	struct inode               ci_inode;
	struct m0_fid              ci_fid;
	/** layout and related information for the file's data */
	struct m0_layout_instance *ci_layout_instance;
	/** Protects the layout instance. */
	struct m0_mutex            ci_layout_lock;
	/**
	 * Locking mechanism provided by resource manager
	 * ci_flock::fi_fid contains fid of gob
	 */
	struct m0_file             ci_flock;
	/** An owner for maintaining file locks */
	struct m0_rm_owner         ci_fowner;
	/** Remote portal for requesting resource from creditor */
	struct m0_rm_remote        ci_creditor;
	/** Pool version fid */
	struct m0_fid              ci_pver;
	/** File layout ID */
	uint64_t                   ci_layout_id;
	/** list of pending transactions */
	struct m0_tl               ci_pending_tx;
	struct m0_mutex            ci_pending_tx_lock;

	uint64_t                   ci_magic;
	/** A link into list (csb_inodes) anchored in m0t1fs superblock. */
	struct m0_tlink            ci_sb_linkage;
	/* Has layout been changed via setfattr? */
	bool                       ci_layout_changed;
};

M0_TL_DESCR_DECLARE(csb_inodes, M0_EXTERN);
M0_TL_DECLARE(csb_inodes, M0_EXTERN, struct m0t1fs_inode);

/** CPUs semaphore - to control CPUs usage by parity calcs. */
extern struct m0_semaphore m0t1fs_cpus_sem;

static inline struct m0t1fs_sb *M0T1FS_SB(const struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct m0t1fs_inode *M0T1FS_I(const struct inode *inode)
{
	return container_of(inode, struct m0t1fs_inode, ci_inode);
}

extern const struct file_operations m0t1fs_dir_file_operations;
extern const struct file_operations m0t1fs_fid_dir_file_operations;
extern const struct file_operations m0t1fs_reg_file_operations;

extern const struct inode_operations m0t1fs_dir_inode_operations;
extern const struct inode_operations m0t1fs_fid_dir_inode_operations;
extern const struct inode_operations m0t1fs_reg_inode_operations;

extern const struct address_space_operations m0t1fs_aops;

/* super.c */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
M0_INTERNAL struct dentry *m0t1fs_mount(struct file_system_type *fstype,
					int flags, const char *devname,
					void *data);
#else
M0_INTERNAL int m0t1fs_get_sb(struct file_system_type *fstype,
			      int flags,
			      const char *devname,
			      void *data, struct vfsmount *mnt);
#endif

M0_INTERNAL void m0t1fs_sb_init(struct m0t1fs_sb *csb);
M0_INTERNAL void m0t1fs_sb_fini(struct m0t1fs_sb *csb);

M0_INTERNAL void m0t1fs_kill_sb(struct super_block *sb);

M0_INTERNAL void m0t1fs_fs_lock(struct m0t1fs_sb *csb);
M0_INTERNAL void m0t1fs_fs_unlock(struct m0t1fs_sb *csb);
M0_INTERNAL bool m0t1fs_fs_is_locked(const struct m0t1fs_sb *csb);

M0_INTERNAL int m0t1fs_fs_conf_lock(struct m0t1fs_sb *csb);
M0_INTERNAL void m0t1fs_fs_conf_unlock(struct m0t1fs_sb *csb);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
M0_INTERNAL int m0t1fs_getattr(const struct path *path, struct kstat *stat,
			       uint32_t request_mask, uint32_t query_flags);
M0_INTERNAL int m0t1fs_fid_getattr(const struct path *path, struct kstat *stat,
				   uint32_t request_mask, uint32_t query_flags);
#else
M0_INTERNAL int m0t1fs_getattr(struct vfsmount *mnt, struct dentry *de,
			       struct kstat *stat);
M0_INTERNAL int m0t1fs_fid_getattr(struct vfsmount *mnt, struct dentry *de,
			           struct kstat *stat);
#endif
M0_INTERNAL int m0t1fs_setattr(struct dentry *dentry, struct iattr *attr);
M0_INTERNAL int m0t1fs_fid_setattr(struct dentry *dentry, struct iattr *attr);
M0_INTERNAL void m0t1fs_inode_update(struct inode *inode,
				     struct m0_fop_cob *body);
/**
 * Takes a superblock reference. If configuration is being updated, the
 * function blocks till new conf is ready or configuration reading impossible
 * due to rconfc fail.
 *
 * @return 0 if lock is taken,
 * @return -ESTALE if rconfc fails and reading configuration is disallowed,
 */
M0_INTERNAL int m0t1fs_ref_get_lock(struct m0t1fs_sb *csb);
M0_INTERNAL void m0t1fs_ref_put_lock(struct m0t1fs_sb *csb);

M0_INTERNAL struct m0_rpc_session *
m0t1fs_container_id_to_session(const struct m0_pool_version *pver,
			       uint64_t container_id);

M0_INTERNAL struct m0_rpc_session *
m0t1fs_filename_to_mds_session(const struct m0t1fs_sb *csb,
			       const unsigned char *filename,
			       unsigned int nlen,
			       bool use_hint,
			       uint32_t hash_hint);

/* inode.c */

M0_INTERNAL int m0t1fs_inode_cache_init(void);
M0_INTERNAL void m0t1fs_inode_cache_fini(void);

M0_INTERNAL bool m0t1fs_inode_is_root(const struct inode *inode);
M0_INTERNAL bool m0t1fs_inode_is_dot_mero(const struct inode *inode);
M0_INTERNAL bool m0t1fs_inode_is_dot_mero_fid(const struct inode *inode);

M0_INTERNAL struct inode *m0t1fs_root_iget(struct super_block *sb,
					   const struct m0_fid *root_fid);
M0_INTERNAL struct inode *m0t1fs_iget(struct super_block *sb,
				      const struct m0_fid *fid,
				      struct m0_fop_cob *body);

M0_INTERNAL struct inode *m0t1fs_alloc_inode(struct super_block *sb);
M0_INTERNAL void m0t1fs_destroy_inode(struct inode *inode);

M0_INTERNAL int m0t1fs_inode_layout_init(struct m0t1fs_inode *ci);
M0_INTERNAL int m0t1fs_inode_layout_rebuild(struct m0t1fs_inode *ci,
					    struct m0_fop_cob *body);
M0_INTERNAL int m0t1fs_inode_test(struct inode *inode, void *opaque);

/* dir.c */

M0_INTERNAL struct m0_rm_domain *m0t1fs_rm_domain_get(struct m0t1fs_sb *sb);

M0_INTERNAL void m0t1fs_file_lock_init(struct m0t1fs_inode    *ci,
				       struct m0t1fs_sb *csb);

M0_INTERNAL void m0t1fs_file_lock_fini(struct m0t1fs_inode *ci);

/**
 * I/O mem stats.
 * Prefix "a_" stands for "allocate".
 * Prefix "d_" stands for "de-allocate".
 */
struct io_mem_stats {
	uint64_t a_ioreq_nr;
	uint64_t d_ioreq_nr;
	uint64_t a_pargrp_iomap_nr;
	uint64_t d_pargrp_iomap_nr;
	uint64_t a_target_ioreq_nr;
	uint64_t d_target_ioreq_nr;
	uint64_t a_io_req_fop_nr;
	uint64_t d_io_req_fop_nr;
	uint64_t a_data_buf_nr;
	uint64_t d_data_buf_nr;
	uint64_t a_page_nr;
	uint64_t d_page_nr;
};

M0_INTERNAL int m0t1fs_mds_cob_create(struct m0t1fs_sb          *csb,
				      const struct m0t1fs_mdop  *mo,
				      struct m0_fop            **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_unlink(struct m0t1fs_sb          *csb,
				      const struct m0t1fs_mdop  *mo,
				      struct m0_fop            **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_link(struct m0t1fs_sb          *csb,
				    const struct m0t1fs_mdop  *mo,
				    struct m0_fop            **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_lookup(struct m0t1fs_sb          *csb,
				      const struct m0t1fs_mdop  *mo,
				      struct m0_fop            **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_getattr(struct m0t1fs_sb           *csb,
				       const struct m0t1fs_mdop   *mo,
				       struct m0_fop             **rep_fop);

M0_INTERNAL int m0t1fs_mds_statfs(struct m0t1fs_sb                *csb,
				  struct m0_fop                  **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_setattr(struct m0t1fs_sb           *csb,
				       const struct m0t1fs_mdop   *mo,
				       struct m0_fop             **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_readdir(struct m0t1fs_sb           *csb,
				       const struct m0t1fs_mdop   *mo,
				       struct m0_fop             **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_setxattr(struct m0t1fs_sb            *csb,
					const struct m0t1fs_mdop    *mo,
					struct m0_fop              **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_getxattr(struct m0t1fs_sb            *csb,
					const struct m0t1fs_mdop    *mo,
					struct m0_fop              **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_listxattr(struct m0t1fs_sb             *csb,
					 const struct m0t1fs_mdop     *mo,
					 struct m0_fop               **rep_fop);

M0_INTERNAL int m0t1fs_mds_cob_delxattr(struct m0t1fs_sb            *csb,
					const struct m0t1fs_mdop    *mo,
					struct m0_fop              **rep_fop);

M0_INTERNAL int m0t1fs_size_update(struct dentry *dentry,
				   uint64_t newsize);

M0_INTERNAL int m0t1fs_inode_set_layout_id(struct m0t1fs_inode *ci,
					   struct m0t1fs_mdop *mo,
			    		   int layout_id);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
extern const struct xattr_handler *m0t1fs_xattr_handlers[];
M0_INTERNAL int m0t1fs_setxattr(const struct xattr_handler *handler,
				struct dentry *dentry, struct inode *inode,
				const char *name, const void *value,
				size_t size, int flags);
M0_INTERNAL int m0t1fs_fid_setxattr(const struct xattr_handler *handler,
				    struct dentry *dentry, struct inode *inode,
				    const char *name, const void *value,
				    size_t size, int flags);
M0_INTERNAL int m0t1fs_getxattr(const struct xattr_handler *handler,
				struct dentry *dentry, struct inode *inode,
				const char *name, void *buffer,
				size_t size);
M0_INTERNAL int m0t1fs_fid_getxattr(const struct xattr_handler *handler,
				    struct dentry *dentry, struct inode *inode,
				    const char *name, void *buffer,
				    size_t size);
#else
M0_INTERNAL int m0t1fs_setxattr(struct dentry *dentry, const char *name,
				const void *value, size_t size, int flags);

M0_INTERNAL int m0t1fs_fid_setxattr(struct dentry *dentry, const char *name,
				    const void *value, size_t size, int flags);

M0_INTERNAL ssize_t m0t1fs_getxattr(struct dentry *dentry, const char *name,
				    void *buffer, size_t size);

M0_INTERNAL ssize_t m0t1fs_fid_getxattr(struct dentry *dentry, const char *name,
					void *buffer, size_t size);
#endif
M0_INTERNAL int m0t1fs_removexattr(struct dentry *dentry, const char *name);
M0_INTERNAL int m0t1fs_fid_removexattr(struct dentry *dentry, const char *name);

M0_INTERNAL ssize_t m0t1fs_listxattr(struct dentry *dentry, char *buffer,
				     size_t size);
M0_INTERNAL ssize_t m0t1fs_fid_listxattr(struct dentry *dentry, char *buffer,
				     size_t size);

M0_INTERNAL const struct m0_fid *
		m0t1fs_inode_fid(const struct m0t1fs_inode *ci);

void m0t1fs_fid_alloc(struct m0t1fs_sb *csb, struct m0_fid *out);

/**
 * Given a fid of an existing file, update "fid allocator" so that this fid is
 * not given out to another file.
 */
void m0t1fs_fid_accept(struct m0t1fs_sb *csb, const struct m0_fid *fid);

unsigned long fid_hash(const struct m0_fid *fid);
M0_INTERNAL struct m0t1fs_sb *m0_fop_to_sb(struct m0_fop *fop);
M0_INTERNAL struct m0_file *m0_fop_to_file(struct m0_fop *fop);

M0_INTERNAL int m0t1fs_cob_getattr(struct inode *inode);
M0_INTERNAL int m0t1fs_cob_setattr(struct inode *inode, struct m0t1fs_mdop *mo);
M0_INTERNAL int m0t1fs_fill_cob_attr(struct m0_fop_cob *cob);
M0_INTERNAL struct m0_confc *m0_csb2confc(struct m0t1fs_sb *csb);
M0_INTERNAL struct m0_rconfc *m0_csb2rconfc(struct m0t1fs_sb *csb);
#endif /* __MERO_M0T1FS_M0T1FS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
