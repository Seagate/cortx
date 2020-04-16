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
 * Original author: Dave Cohrs <Dave_Cohrs@xyratex.com>
 * Original creation date: 02/08/2012
 */

/**
   @page LNetDRVDLD LNet Transport Device DLD

   - @ref LNetDRVDLD-ovw
   - @ref LNetDRVDLD-def
   - @ref LNetDRVDLD-req
   - @ref LNetDRVDLD-depends
   - @ref LNetDRVDLD-highlights
   - @subpage LNetDRVDLD-fspec "Functional Specification" <!-- Note @subpage -->
   - @ref LNetDRVDLD-lspec
      - @ref LNetDRVDLD-lspec-comps
      - @ref LNetDRVDLD-lspec-dev
      - @ref LNetDRVDLD-lspec-ioctl
      - @ref LNetDRVDLD-lspec-mem
      - @ref LNetDRVDLD-lspec-dominit
      - @ref LNetDRVDLD-lspec-domfini
      - @ref LNetDRVDLD-lspec-reg
      - @ref LNetDRVDLD-lspec-bev
      - @ref LNetDRVDLD-lspec-tmstart
      - @ref LNetDRVDLD-lspec-tmstop
      - @ref LNetDRVDLD-lspec-buf
      - @ref LNetDRVDLD-lspec-event
      - @ref LNetDRVDLD-lspec-nids
      - @ref LNetDRVDLD-lspec-state
      - @ref LNetDRVDLD-lspec-thread
      - @ref LNetDRVDLD-lspec-numa
   - @ref LNetDRVDLD-conformance
   - @ref LNetDRVDLD-ut
   - @ref LNetDRVDLD-st
   - @ref LNetDRVDLD-O
   - @ref LNetDRVDLD-ref

   <hr>
   @section LNetDRVDLD-ovw Overview

   The Mero LNet Transport device provides user space access to the kernel
   @ref LNetDLD "Mero LNet Transport".  The
   @ref ULNetCoreDLD "User Space Core" implementation uses the device to
   communicate with the @ref KLNetCoreDLD "Kernel Core".  The device
   provides a conduit through which information flows between the user space and
   kernel core layers, initiated by the user space layer.  The specific
   operations that can be performed on the device are documented here.  Hooks
   for unit testing the device are also discussed.

   <hr>
   @section LNetDRVDLD-def Definitions

   Refer to <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV
779386NtFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>
.

   - @b reference A reference to an object is stored in terms of a memory
     page and offset, rather than as a simple address pointer.
   - @b pin Keep a page of user memory from being paged out of physical memory
     and cause it to be paged in if it was previously paged out.  Pinned user
     pages, like pages of kernel memory, are tracked by kernel @c page objects.
     Pinning a page does not assign it a kernel logical address; that requires
     subsequently mapping the page.  A pinned page remained pinned until it is
     explicitly unpinned.  Pinning may involve the use of shared, reference
     counted objects, but one should not depend on this for correctness.
   - @b map Assign a kernel logical address to a page of memory.  A mapped page
     remains mapped until explicitly unmapped.  Both kernel and pinned user
     pages can be mapped.  Mapping may involve the use of shared, reference
     counted objects and addresses, but one should not depend on this for
     correctness.  Each time a page is mapped, it may be assigned a different
     logical address.
   - @b unmap Remove the association of a kernel logical address from a page.
     After a page is unmapped, it has no logical address until it is explicitly
     remapped.
   - @b unpin Allow a previously pinned page to move freely, i.e. an unpinned
     page can be swapped out of physical memory.  Any struct @c page pointers
     to the previously pinned page are no longer valid after a page is unpinned.

   <hr>
   @section LNetDRVDLD-req Requirements

   - @b r.m0.net.xprt.lnet.user-space The implementation must
     accommodate the needs of the user space LNet transport.
   - @b r.m0.net.xprt.lnet.dev.pin-objects The implementation must pin
     shared objects in kernel memory to ensure they will not disappear while
     in use.
   - @b r.m0.net.xprt.lnet.dev.resource-tracking The implementation must
     track all shared resources and ensure they are released properly, even
     after a user space error.
   - @b r.m0.net.xprt.lnet.dev.safe-sharing The implementation must ensure
     that references to shared object are valid.
   - @b r.m0.net.xprt.lnet.dev.assert-free The implementation must ensure
     that the kernel module will not assert due to invalid shared state.
   - @b r.m0.net.xprt.lnet.dev.minimal-mapping The implementation must avoid
     mapping many kernel pages for long periods of time, avoiding excessive
     use of kernel high memory page map.

   <hr>
   @section LNetDRVDLD-depends Dependencies

   - @ref LNetCore "LNet Transport Core Interface" <!-- ../lnet_core.h --> <br>
     Several modifications are required on the Core interface itself:
     - A new @c nlx_core_buffer_event::cbe_kpvt pointer is required that can be
       set to refer to the new @c nlx_kcore_buffer_event object.
     - The @c nlx_core_tm_start() function is changed to remove the @c cepa and
       @c epp parameters.  The @c cepa parameter is always the same as the
       @c lctm->ctm_addr.  The Core API does not use a @c m0_net_end_point,
       so setting the @c epp at the core layer was inappropriate.  The LNet XO
       layer, which does use the end point, is modified to allocate this
       end point in the @c nlx_tm_ev_worker() logic itself.
     - The user space transport must ensure that shared objects do not
       cross page boundaries.  This applies only to shared core objects such as
       @c nlx_core_transfer_mc, not to buffer pages.  Since object allocation
       is actually done in the LNet XO layer (except for the
       @c nlx_core_buffer_event), this requires that an allocation wrapper
       function, @c nlx_core_mem_alloc(), be added to the Core Interface,
       implemented separately in the kernel and user transports, because the
       kernel transport has no such limitation and the @c m0_alloc_aligned()
       API, which could be used to satisfy this requirement, requires page
       aligned data or greater in kernel space.  A corresponding
       @c nlx_core_mem_free() is required to free the allocated memory.

   - @ref ULNetCore "LNet Transport Core User Private Interface" <!--
       ../ulnet_core.c --> <br>
     Besides the existence of this interface, the following dependencies
     exist:
     - The user space transport must call the ioctl requests specified in this
       DLD in the manner prescribed in the @ref LNetDRVDLD-lspec below.
     - The user space transport must implement the new @c nlx_core_mem_alloc()
       and @c nlx_core_mem_free() required by the Core Interface.

   - @ref KLNetCore "LNet Transport Core Kernel Private Interface" <!--
       ./lnet_core.h --> <br>
     Several modifications are required in this interface:
     - The kernel core objects with back pointers to the corresponding
       core objects must be changed to remove these pointers and replace them
       with use of @c nlx_core_kmem_loc objects.  More details of this
       dependency are discussed in @ref LNetDRVDLD-lspec-mem.
     - The @c bev_cqueue_pnext() and @c bev_cqueue_put() are modified such that
       they map and unmap the @c nlx_core_buffer_event object (atomic mapping
       must be used, because these functions are called from the LNet callback).
       This also requires use of @c nlx_core_kmem_loc references in the
       @c nlx_core_bev_link.
     - Many of the Core APIs implemented in the kernel must be refactored such
       that the portion that can be shared between the kernel-only transport and
       the kernel driver is moved into a new API.  The Kernel Core API
       implementation is changed to perform pre-checks, call the new shared API
       and complete post-shared operations.  The User Space Core tasks
       described in the
       @ref ULNetCoreDLD-lspec "User Space Core Logical Specification" can be
       used as a guide for how to refactor the Kernel Core implementation.  In
       addition, the operations on the @c nlx_kcore_ops structure guide the
       signatures of the refactored, shared operations.
     - The @c nlx_kcore_umd_init() function is changed to set the
       MD @c user_ptr to the @c nlx_kcore_buffer, not the @c nlx_core_buffer.
       @c kb_buffer_id and @c kb_qtype fields are added to the
       @c nlx_kcore_buffer and are set during @c nlx_kcore_buf_register() and
       @c nlx_kcore_umd_init() respectively.  This allows the lnet event
       callback to execute without using the @c nlx_core_buffer.
       All other uses of the MD @c user_ptr field must be changed accordingly.
     - Various blocks of @c M0_PRE() assertions used to validate shared objects
       before they are referenced should be refactored into new invariant-style
       functions so the driver can perform the checks and return an error
       without causing an kernel assertion.
     - The kernel core must implement the new @c nlx_core_mem_alloc()
       and @c nlx_core_mem_free() required by the Core Interface.

   <hr>
   @section LNetDRVDLD-highlights Design Highlights

   - The device provides ioctl-based access to the Kernel LNet Core Interface.
   - Ioctl requests correspond roughly to the
     @ref LNetCore "LNet Transport Core APIs".
   - Each user space @c m0_net_domain corresponds to opening a separate
     file descriptor.
   - The device driver tracks all resources associated with the file descriptor.
   - Well-defined patterns are used for sharing new resources between user
     and kernel space, referencing previously shared resources, and releasing
     shared resources.
   - The device driver can clean up a domain's resources in the case that the
     user program terminates prematurely.

   <hr>
   @section LNetDRVDLD-lspec Logical Specification

   - @ref LNetDRVDLD-lspec-comps
   - @ref LNetDRVDLD-lspec-dev
   - @ref LNetDRVDLD-lspec-ioctl
   - @ref LNetDRVDLD-lspec-mem
   - @ref LNetDRVDLD-lspec-dominit
   - @ref LNetDRVDLD-lspec-domfini
   - @ref LNetDRVDLD-lspec-reg
   - @ref LNetDRVDLD-lspec-bev
   - @ref LNetDRVDLD-lspec-tmstart
   - @ref LNetDRVDLD-lspec-tmstop
   - @ref LNetDRVDLD-lspec-buf
   - @ref LNetDRVDLD-lspec-event
   - @ref LNetDRVDLD-lspec-nids
   - @ref LNetDRVDLD-lspec-state
   - @ref LNetDRVDLD-lspec-thread
   - @ref LNetDRVDLD-lspec-numa

   @subsection LNetDRVDLD-lspec-comps Component Overview

   The LNet Device Driver is a layer between the user space transport
   core and the kernel space transport core.  The driver layer provides
   a mechanism for the user space to interact with the Lustre LNet kernel
   module.  It uses a subset of the kernel space transport core interface
   to implement this interaction.

   @see <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV7793
86NtFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>,
   specifically the Design Highlights component diagram.

   For reference, the relationship between the various components of the LNet
   transport and the networking layer is illustrated in the following UML
   diagram.
   @image html "../../net/lnet/lnet_xo.png" "LNet Transport Objects"

   The LNet Device Driver has no sub-components.  It has several internal
   functions that interact with the kernel space transport core layer.

   @subsection LNetDRVDLD-lspec-dev Device Setup and Shutdown

   The LNet device is registered with the kernel using the @c nlx_dev_init()
   function when the Mero Kernel module is loaded.  This function is
   called by the existing @c nlx_core_init() function.  The function performs
   the following tasks.
   - It registers the device with the kernel.  The device is registered as
     a miscellaneous device named "m0lnet".  As such, registration causes the
     device to appear as "/dev/m0lnet" in the device file system.
   - Sets a flag, @c ::nlx_dev_registered, denoting successful device
     registration.

   The LNet device is deregistered with the kernel using the @c nlx_dev_fini()
   function when the Mero Kernel module is unloaded.  This function is
   called by the existing @c nlx_core_fini() function.  The function performs
   the following task.
   - If device registration was performed successfully, deregisters the
     device and resets the @c ::nlx_dev_registered flag.

   @subsection LNetDRVDLD-lspec-ioctl Ioctl Request Behavior

   The @ref ULNetCore "user space implementation" of the
   @ref LNetCore "LNet Transport Core Interface" interacts with the
   @ref KLNetCore "LNet Transport Kernel Core" via ioctl requests.
   The file descriptor required to make the ioctl requests is obtained during
   the @ref LNetDRVDLD-lspec-dominit operation.

   All further interaction with the device, until the file descriptor is
   closed, is via ioctl requests.  Ioctl requests are served by the
   @c nlx_dev_ioctl() function, an implementation of the kernel @c
   file_operations::unlocked_ioctl() function.  This function performs the
   following steps.

   - It validates the request.
   - It copies in (from user space) the parameter object corresponding to the
     specific request for most _IOW requests.  Note that the requests that take
     pointers instead of parameter objects do not copy in, because the pointers
     are either references to kernel objects or shared objects to be pinned, not
     copied.
   - It calls a helper function to execute the command; specific helper
     functions are called out in the following sections.  The helper function
     will call a kernel core operation to execute the behavior shared between
     user space and kernel transports.  It does this indirectly from operations
     defined on the @c nlx_kcore_domain::kd_drv_ops operation object.
   - It copies out (to user space) the parameter object corresponding to the
     specific request for _IOR and _IOWR requests.
   - It returns the status, generally of the helper function.  This status
     follows the typical 0 for success, -errno for failure, except as
     specified for certain helper functions.

   The helper functions verify that the requested operation will not cause an
   assertion in the kernel core.  This is done by performing the same checks the
   Kernel Core APIs would do, but without asserting. Instead, they log an ADDB
   record and return an error status when verification fails.  The user space
   core can detect the error status and assert the user space process.  The
   error code @c -EBADR is used to report verification failure.  The error
   code @c -EFAULT is used to report invalid user addresses, such as for use
   in pinning pages or copying in user data.  Specific helper functions may
   return additional well-defined errors.

   @see @ref LNetDevInternal

   @subsection LNetDRVDLD-lspec-mem Shared Memory Management Strategy

   Some ioctl requests have the side effect of pinning user pages in memory.
   However, the mapping of pages (i.e. @c kmap() or @c kmap_atomic() functions)
   is performed only while the pages are to be used, and then unmapped as soon
   as possible.  The number of mappings available to @c kmap() is documented as
   being limited.  Except as noted, @c kmap_atomic() is used in blocks of code
   that will not sleep to map the page associated with an object.  Each time a
   shared object is mapped, its invariants are re-checked to ensure the page
   still contains the shared object.  Each shared core object is required to fit
   within a single page to simplify mapping and sharing.  The user space
   transport must ensure this requirement is met when it allocates core objects.
   Note that the pages of the @c m0_bufvec segments are not part of the shared
   @c nlx_core_buffer; they are referenced by the associated @c nlx_kcore_buffer
   object and are never mapped by the driver or kernel core layers.

   The @c nlx_core_kmem_loc structure stores the page and offset of an object.
   It also stores a checksum to detect inadvertent corruption of the address
   or offset.  This structure is used in place of pointers within structures
   used in kernel address space to reference shared (pinned) user space objects.
   Kernel core structures @c nlx_kcore_domain, @c nlx_kcore_transfer_mc,
   @c nlx_kcore_buffer and @c nlx_kcore_buffer_event refer to shared objects,
   and use fields such as nlx_kcore_domain::kd_cd_loc to store these references.
   Structures such as @c nlx_core_bev_link, @c nlx_core_bev_cqueue, while
   contained in shared objects also use @c nlx_core_kmem_loc, because these
   structures in turn need to reference yet other shared objects.  When the
   shared object is needed, it is mapped (e.g. @c kmap_atomic() returns a
   pointer to the mapped page, and the code adds the corresponding offset to
   obtain a pointer to the object itself), used, and unmapped.  The kernel
   pointer to the shared object is only used on the stack, never stored in a
   shared place.  This allow for unsynchronized, concurrent access to shared
   objects, just as if they were always mapped.

   The core data structures include kernel private pointers, such as
   @c nlx_core_transfer_mc::ctm_kpvt.  These opaque (to the user space) values
   are used as parameters to ioctl requests.  These pointers cannot be used
   directly, since it is possible they could be inadvertently corrupted.  To
   address that, when such pointers are passed to ioctl requests, they are first
   validated using @c virt_addr_valid() to ensure they can be dereferenced in
   the kernel and then further validated using the appropriate invariant,
   @c nlx_kcore_tm_invariant() in the case above.  If either validation fails,
   an error is returned, as discussed in @ref LNetDRVDLD-lspec-ioctl.

   @subsection LNetDRVDLD-lspec-dominit Domain Initialization

   The LNet Transport Device Driver is first accessed during domain
   initialization.  The @ref ULNetCoreDLD-lspec-dominit "user space core"
   opens the device and performs an initial @c #M0_LNET_DOM_INIT ioctl request.

   In the kernel, the @c open() and @c ioctl() system calls are handled by
   the @c nlx_dev_open() and @c nlx_dev_ioctl() subroutines, respectively.

   The @c nlx_dev_open() performs the following sequence.
   - It allocates a @c nlx_kcore_domain object, initializes it using
     @c nlx_kcore_kcore_dom_init() and assigns the object to the
     @c file->private_data field.

   The @c nlx_dev_ioctl() is described generally
   @ref LNetDRVDLD-lspec-ioctl "above".  It uses the helper function
   @c nlx_dev_ioctl_dom_init() to complete kernel domain initialization.
   The following tasks are performed.
   - The @c nlx_kcore_domain::kd_drv_mutex() is locked.
   - The @c nlx_kcore_domain is verified to ensure the core domain is not
     already initialized.
   - The @c nlx_core_domain object is pinned in kernel memory.
   - Information required to map the @c nlx_core_domain is saved in the
     @c nlx_kcore_domain::kd_cd_loc.
   - The @c nlx_core_domain is mapped and validated to ensure no assertions
     will occur.
   - The @c nlx_core_domain is initialized by @c nlx_kcore_ops::ko_dom_init().
   - The output parameters of the ioctl request, the three buffer size maximum
     values, are set in the provided @c m0_lnet_dev_dom_init_params object.
   - The @c nlx_core_domain is unmapped (it remains pinned).
   - The @c nlx_kcore_domain::kd_drv_mutex() is unlocked.

   @subsection LNetDRVDLD-lspec-domfini Domain Finalization

   During normal domain finalization, the @ref ULNetCoreDLD-lspec-domfini
   "user space core" closes its file descriptor after the upper layers have
   already cleaned up other resources (buffers and transfer machines).
   It is also possible that the user space process closes the file descriptor
   without first finalizing the associated domain resources, such as in
   the case that the user space process fails.

   In the kernel the @c close() system call is handled by the @c nlx_dev_close()
   subroutine.  Technically, @c nlx_dev_close() is called once by the kernel
   when the last reference to the file is closed (e.g. if the file descriptor
   had been duplicated).  The subroutine performs the following sequence.

   - It verifies that the domain is ready to be finalized.  That is, it
     checks that the resource lists @c nlx_kcore_domain::kd_drv_tms and
     @c nlx_kcore_domain::kd_drv_bufs are empty.
   - If the domain is not ready to be finalized, it releases the remaining
     domain resources itself.
     - Any running transfer machines must be stopped, their pending
       operations aborted.
     - Shared @c nlx_core_transfer_mc objects must be unpinned.
     - Shared @c nlx_core_buffer_event objects must be unpinned.
     - Each corresponding @c nlx_kcore_buffer_event object is freed.
     - Each @c nlx_kcore_transfer_mc object is freed.
     - All pinned buffer data pages must be unpinned.
     - All registered buffers must be deregistered.
     - Shared @c nlx_core_buffer objects must be unpinned.
     - Each corresponding @c nlx_kcore_buffer object is freed.
   - It calls @c nlx_kcore_ops::ko_dom_fini() to finalize the core domain.
   - It unpins the shared @c nlx_core_domain object, resetting the
       the @c nlx_kcore_domain::kd_cd_loc.
   - It resets (to NULL) the @c file->private_data.
   - It calls @c nlx_kcore_kcore_dom_fini() to finalize the @c nlx_kcore_domain
     object.
   - It frees the @c nlx_kcore_domain object.

   @subsection LNetDRVDLD-lspec-reg Buffer Registration and Deregistration

   While registering a buffer, the user space core performs a
   @c #M0_LNET_BUF_REGISTER ioctl request.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_buf_register() to complete kernel buffer registration.
   The following tasks are performed.
   - The parameters are validated to ensure no assertions will occur.
   - The @c m0_bufvec::ov_buf and @c m0_bufvec::ov_vec::v_count are copied in,
     temporarily (to avoid issues of either list crossing page boundaries that
     might occur by mapping the pages directly), and the corresponding fields
     of the @c m0_lnet_dev_buf_register_params::dbr_bvec is updated to refer
     to the copies.
   - The @c nlx_core_buffer, @c m0_lnet_dev_buf_register_params::dbr_lcbuf,
     is pinned in kernel memory.
   - Information required to map the @c nlx_core_buffer is saved in the
     @c nlx_kcore_buffer::kb_cb_loc.
   - The @c nlx_core_buffer is mapped and validated to ensure no assertions
     will occur.  It is also checked to ensure it is not already associated
     with a @c nlx_kcore_buffer object.
   - @c nlx_kcore_ops::ko_buf_register() is used to initialize
     the @c nlx_core_buffer and @c nlx_kcore_buffer objects.
   - @c nlx_kcore_buffer_uva_to_kiov() is used to pin the
     pages of the buffer segments and initialize the
     @c nlx_kcore_buffer::kb_kiov.
   - The @c nlx_core_buffer is unmapped (it remains pinned).
   - The @c nlx_kcore_buffer is added to the @c nlx_kcore_domain::kd_drv_bufs
     list.
   - Memory allocated for the temporary copies in the
     @c m0_lnet_dev_buf_register_params::dbr_bvec are freed.

   While deregistering a buffer, the user space core performs a
   @c #M0_LNET_BUF_DEREGISTER ioctl request.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_buf_deregister() to complete kernel buffer deregistration.
   The following tasks are performed.
   - The parameters are validated to ensure no assertions will occur.
   - The pages associated with the buffer, referenced by
     @c nlx_kcore_buffer::kb_kiov, are unpinned.
   - The buffer is removed from the @c nlx_kcore_domain::kd_drv_bufs list.
   - The @c nlx_core_buffer is mapped.
   - @c nlx_kcore_ops::ko_buf_deregister() is used to deregister
     the buffer.
   - The @c nlx_kcore_buffer object is freed.
   - The @c nlx_core_buffer is unmapped and unpinned.

   @subsection LNetDRVDLD-lspec-bev Managing the Buffer Event Queue

   The @c nlx_core_new_blessed_bev() helper allocates and blesses buffer event
   objects.  In user space, blessing the object requires interacting with the
   kernel by way of the @c #M0_LNET_BEV_BLESS ioctl request.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_bev_bless() to complete blessing the buffer event object.
   The following tasks are performed.
   - The parameters are validated to ensure no assertions will occur.
   - The @c nlx_core_buffer_event is pinned in kernel memory.
   - A @c nlx_kcore_buffer_event object is allocated and initialized.
   - Information required to map the pinned object is saved in the
     @c nlx_kcore_buffer_event object.
   - The @c nlx_core_buffer_event is mapped, validated to ensure no assertions
     will occur, and checked to ensure it is not already associated with a
     @c nlx_kcore_buffer_event object.
   - The @c bev_link_bless() function is called to bless the object.
   - The @c nlx_core_buffer_event is unmapped (it remains pinned).
   - The @c nlx_kcore_buffer_event object is added to the
     @c nlx_kcore_transfer_mc::ktm_drv_bevs list.

   Buffer event objects are never removed from the buffer event queue until
   the transfer machine is stopped.

   @see @ref LNetDRVDLD-lspec-tmstop

   @subsection LNetDRVDLD-lspec-tmstart Starting a Transfer Machine

   While starting a transfer machine, the user space core performs a
   @c #M0_LNET_TM_START ioctl request.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_tm_start() to complete starting the transfer machine.
   The following tasks are performed.
   - The parameters are validated to ensure no assertions will occur.
   - The @c nlx_core_transfer_mc object is pinned in kernel memory.
   - A @c nlx_kcore_transfer_mc is allocated.
   - Information required to map the pinned object is saved in the
     @c nlx_kcore_transfer_mc object.
   - The @c nlx_core_transfer_mc is mapped using @c kmap()
     because the core operation may sleep.
   - The @c nlx_core_transfer_mc is checked to ensure it is not already
     associated with a @c nlx_kcore_transfer_mc object and that it will
     not cause assertions.
   -  @c nlx_kcore_ops::ko_tm_start() is used to complete the kernel TM start.
   - The @c nlx_core_transfer_mc is unmapped (it remains pinned).
   - The @c nlx_kcore_transfer_mc is added to the
     @c nlx_kcore_domain::kd_drv_tms list.

   @subsection LNetDRVDLD-lspec-tmstop Stopping a Transfer Machine

   While stopping a transfer machine, the user space core performs a
   @c #M0_LNET_TM_STOP ioctl request.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_tm_stop() to complete stopping the transfer machine.
   The following tasks are performed.
   - The parameters are validated to ensure no assertions will occur.
   - The transfer machine is removed from the @c nlx_kcore_domain::kd_drv_tms
     list.
   - The buffer event objects on the @c nlx_kcore_transfer_mc::ktm_drv_bevs list
     are unpinned and their corresponding @c nlx_kcore_buffer_event objects
     freed.
   - The @c nlx_core_transfer_mc is mapped.
   - @c nlx_kcore_ops::ko_tm_stop() is used to stop the transfer machine.
   - The @c nlx_core_transfer_mc is unmapped and unpinned.

   @subsection LNetDRVDLD-lspec-buf Transfer Machine Buffer Queue Operations

   Several LNet core interfaces operate on buffers and transfer machine queues.
   In all user transport cases, the shared objects, @c nlx_core_buffer and
   @c nlx_core_transfer_mc, must have been previously shared with the kernel,
   through use of the @c #M0_LNET_BUF_REGISTER and @c #M0_LNET_TM_START ioctl
   requests, respectively.

   The ioctl requests available to the user space core for managing
   buffers and transfer machine buffer queues are as follows.
   - @c #M0_LNET_BUF_MSG_RECV
   - @c #M0_LNET_BUF_MSG_SEND
   - @c #M0_LNET_BUF_ACTIVE_RECV
   - @c #M0_LNET_BUF_ACTIVE_SEND
   - @c #M0_LNET_BUF_PASSIVE_RECV
   - @c #M0_LNET_BUF_PASSIVE_SEND
   - @c #M0_LNET_BUF_DEL

   The ioctl requests are handled by the following helper functions,
   respectively.
   - @c nlx_dev_ioctl_buf_msg_recv()
   - @c nlx_dev_ioctl_buf_msg_send()
   - @c nlx_dev_ioctl_buf_active_recv()
   - @c nlx_dev_ioctl_buf_active_send()
   - @c nlx_dev_ioctl_buf_passive_recv()
   - @c nlx_dev_ioctl_buf_passive_send()
   - @c nlx_dev_ioctl_buf_del()

   These helper functions each perform similar tasks.
   - The parameters are validated to ensure no assertions will occur.
   - The @c nlx_core_transfer_mc and @c nlx_core_buffer are mapped using
     @c kmap() because the core operations may sleep.
   - The corresponding kernel core operation is called.
     - @c nlx_kcore_ops::ko_buf_msg_recv()
     - @c nlx_kcore_ops::ko_buf_msg_send()
     - @c nlx_kcore_ops::ko_buf_active_recv()
     - @c nlx_kcore_ops::ko_buf_active_send()
     - @c nlx_kcore_ops::ko_buf_passive_recv()
     - @c nlx_kcore_ops::ko_buf_passive_send()
     - @c nlx_kcore_ops::ko_buf_del()
   - The @c nlx_core_transfer_mc and @c nlx_core_buffer are unmapped.

   @subsection LNetDRVDLD-lspec-event Waiting for Buffer Events

   To wait for buffer events, the user space core performs a
   @c #M0_LNET_BUF_EVENT_WAIT ioctl request.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_buf_event_wait() to perform the wait operation.
   The following tasks are performed.

   - The parameters are validated to ensure no assertions will occur.
   - The @c nlx_kcore_ops::ko_buf_event_wait() function is called.

   @subsection LNetDRVDLD-lspec-nids Node Identifier Support

   The user space core uses the @c #M0_LNET_NIDSTR_DECODE and
   @c #M0_LNET_NIDSTR_ENCODE requests to decode and encode NID strings,
   respectively.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_nidstr_decode() to decode the string.
   The following tasks are performed.

   - The parameter is validated to ensure no assertions will occur.
   - The @c libcfs_str2nid() function is called to convert the string to a NID.
   - In the case the result is LNET_NID_ANY, -EINVAL is returned,
     otherwise the @c dn_nid field is set.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_nidstr_encode() to decode the string.
   The following tasks are performed.

   - The parameter is validated to ensure no assertions will occur.
   - The @c libcfs_nid2str() function is called to convert the string to a NID.
   - The resulting string is copied to the @c dn_buf field.

   The user space core uses the @c #M0_LNET_NIDSTRS_GET to obtain the
   list of NID strings for the local LNet interfaces.

   The @c nlx_dev_ioctl() subroutine uses the helper function
   @c nlx_dev_ioctl_nidstrs_get() to decode the string.
   The following tasks are performed.

   - The parameters are validated to ensure no assertions will occur.
   - The @c nlx_core_nidstrs_get() API is called to get the list of NID strings.
   - The buffer size required to store the strings is computed (sum of the
     string lengths of the NID strings, plus trailing nuls, plus one).
   - A temporary buffer of the required size is allocated.
   - The NID strings are copied consecutively into the buffer.  Each NID string
     is nul terminated and an extra nul is written after the final NID string.
   - The contents of the buffer is copied to the user space buffer.
   - The @c nlx_core_nidstrs_put() API is called to release the list of NID
     strings.
   - The temporary buffer is freed.
   - The number of NID strings is returned on success; @c nlx_dev_ioctl()
     returns this positive number instead of the typical 0 for success.
   - The value -EFBIG is returned if the buffer is not big enough.

   @subsection LNetDRVDLD-lspec-state State Specification

   The LNet device driver does not introduce its own state model but operates
   within the frameworks defined by the Mero Networking Module and the Kernel
   device driver interface.  In general, resources are pinned and allocated when
   an object is first shared with the kernel by the user space process and
   are freed and unpinned when the user space requests.  To ensure there is no
   resource leakage, remaining resources are freed when the @c nlx_dev_close()
   API is called.

   The resources managed by the driver are tracked by the following lists:
   - @c nlx_kcore_domain::kd_cd_loc (a single item)
   - @c nlx_kcore_domain::kd_drv_tms
   - @c nlx_kcore_domain::kd_drv_bufs
   - @c nlx_kcore_transfer_mc::ktm_drv_bevs

   Each @c nlx_kcore_domain object has 2 valid states which can be determined
   by inspecting the @c nlx_kcore_domain::kd_cd_loc field:
   - @c nlx_core_kmem_loc_is_empty(&kd_cd_loc): The device is newly opened
   and the @c #M0_LNET_DOM_INIT ioctl request has not yet been performed.
   - @c nlx_core_kmem_loc_invariant(&kd_cd_loc): The @c #M0_LNET_DOM_INIT
   ioctl request has been performed, associating it with a @c nlx_core_domain
   object.  In this state, the @c nlx_kcore_domain is ready for use and remains
   in this state until finalized.

   @subsection LNetDRVDLD-lspec-thread Threading and Concurrency Model

   The LNet device driver has no threads of its own.  It operates within
   the context of a user space process and a kernel thread operating on behalf
   of that process.  All operations are invoked through the Linux device
   driver interface, specifically the operations defined on the
   @c ::nlx_dev_file_ops object.  The @c nlx_dev_open() and @c nlx_dev_close()
   are guaranteed to be called once each for each kernel @c file object, and
   calls to these operations are guaranteed to not overlap with calls to the
   @c nlx_dev_ioctl() operation. However, multiple calls to @c nlx_dev_ioctl()
   may occur simultaneously on different threads.

   Synchronization of device driver resources is controlled by a single mutex
   per domain, the @c nlx_kcore_domain::kd_drv_mutex.  This mutex must be
   held while manipulating the resource lists, @c nlx_kcore_domain::kd_drv_tms,
   @c nlx_kcore_domain::kd_drv_bufs and @c nlx_kcore_transfer_mc::ktm_drv_bevs.

   The mutex may also be used to serialize driver ioctl requests, such as in
   the case of @c #M0_LNET_DOM_INIT.

   The driver mutex must be obtained before any other Net or Kernel Core mutex.

   Mapping of @c nlx_core_kmem_loc object references can be performed without
   synchronization, because the @c nlx_core_kmem_loc never changes after an
   object is pinned, and the mapped pointer is specified to never be stored
   in a shared location, i.e. only on the stack.  The functions that unpin
   shared objects have invariants and pre-conditions to ensure that the objects
   are no longer in use and can be unpinned without causing a mapping failure.

   Cleanup of kernel resources for user domains synchronizes with the Kernel
   Core LNet EQ callback by use of the nlx_kcore_transfer_mc::ktm_bevq_lock
   and the nlx_kcore_transfer_mc::ktm_sem, as discussed in
   @ref KLNetCoreDLD-lspec-thread.

   @subsection LNetDRVDLD-lspec-numa NUMA optimizations

   The LNet device driver does not allocate threads.  The user space application
   can control thread processor affiliation by confining the threads it uses
   to access the device driver.

   <hr>
   @section LNetDRVDLD-conformance Conformance

   - @b i.m0.net.xprt.lnet.user-space The @ref LNetDRVDLD-lspec covers how
     each LNet Core operation in user space can be implemented using the
     driver ioctl requests.
   - @b i.m0.net.xprt.lnet.dev.pin-objects See @ref LNetDRVDLD-lspec-dominit,
     @ref LNetDRVDLD-lspec-reg, @ref LNetDRVDLD-lspec-bev,
     @ref LNetDRVDLD-lspec-tmstart.
   - @b i.m0.net.xprt.lnet.dev.resource-tracking
     See @ref LNetDRVDLD-lspec-domfini.
   - @b i.m0.net.xprt.lnet.dev.safe-sharing See @ref LNetDRVDLD-lspec-ioctl,
     specifically the paragraph covering the use of pinned pages.
   - @b i.m0.net.xprt.lnet.dev.assert-free See @ref LNetDRVDLD-lspec-ioctl.
   - @b i.m0.net.xprt.lnet.dev.minimal-mapping None of the work flows in the
     @ref LNetDRVDLD-lspec require that objects remain mapped across ioctl
     calls.  The @ref LNetDRVDLD-lspec-ioctl calls for use of @c kmap_atomic()
     when possible.
     @ref LNetDRVDLD-lspec-domfini, @ref LNetDRVDLD-lspec-ioctl.

   <hr>
   @section LNetDRVDLD-ut Unit Tests

   LNet Device driver unit tests focus on the covering the common code
   paths.  Code paths involving most Kernel LNet Core operations and
   the device wrappers will be handled as part of testing the user transport.
   Even so, some tests are most easily performed by coordinating user space
   code with kernel unit tests.  The following strategy will be used:
   - When the LNet unit test suite is initialized in the kernel, it creates
     a /proc/m0_lnet_ut file, registering read and write file operations.
   - The kernel UT waits (e.g. on a condition variable with a timeout) for
     the user space program to synchronize.  It may time out and fail the UT
     if the user space program does not synchronize quickly enough, e.g. after
     a few seconds.
   - A user space program is started concurrently with the kernel unit tests.
   - The user space program waits for the /proc/m0_lnet_ut to appear.
   - The user space program writes a message to the /proc/m0_lnet_ut to
     synchronize with the kernel unit test.
   - The write system call operation registered for /proc/m0_lnet_ut signals
     the condition variable that the kernel UT is waiting on.
   - The user space program loops.
     - The user space program reads the /proc/m0_lnet_ut for instructions.
     - Each instruction tells the user space program which test to perform;
       there is a special instruction to tell the user space program the
       unit test is complete.
     - The user space program writes the test result back.
   - When the LNet unit test suite is finalized in the kernel, the
     /proc/m0_lnet_ut file is removed.

   While ioctl requests on the /dec/m0lnet device could be used for such
   coordination, this would result in unit test code being mixed into the
   production code.  The use of a /proc file for coordinating unit tests
   ensures this is not the case.

   To enable unit testing of the device layer without requiring full kernel
   core behavior, the device layer accesses kernel core operations indirectly
   via the @c nlx_kcore_domain::kd_drv_ops operation structure.  During unit
   tests, these operations can be changed to call mock operations instead of
   the real kernel core operations.  This allows testing of things such as
   pinning and mapping pages without causing real core behavior to occur.

   @test Initializing the device causes it to be registered and visible
         in the file system.

   @test The device can be opened and closed.

   @test Reading or writing the device fails.

   @test Unsupported ioctl requests fail.

   @test A nlx_core_domain can be initialized and finalized, testing common
         code paths and the strategy of pinning and unpinning pages.

   @test A nlx_core_domain is initialized, and several nlx_core_transfer_mc
         objects can be started and then stopped, the domain finalized and the
         device is closed.  No cleanup is necessary.

   @test A nlx_core_domain is initialized, and the same nlx_core_transfer_mc
         object is started twice, the error is detected.  The remaining transfer
	 machine is stopped.  The device is closed.  No cleanup is necessary.

   @test A nlx_core_domain, and several nlx_core_transfer_mc objects can
         be registered, then the device is closed, and cleanup occurs.

   Buffer and buffer event management tests and more advanced domain and
   transfer machine test will be added as part of testing the user space
   transport.

   <hr>
   @section LNetDRVDLD-st System Tests
   System testing will be performed as part of the transport operation system
   test.

   <hr>
   @section LNetDRVDLD-O Analysis
   - The algorithmic complexity of ioctl requests is constant, except
     - the complexity of pinning a buffer varies with the number of pages in
       the buffer,
     - the complexity of stopping a transfer machine is proportional to the
       number of buffer events pinned.
   - The time to pin or @c kmap() a page is unpredictable, and depends, at the
     minimum, on current system load, memory consumption and other LNet users.
     For this reason, @c kmap_atomic() should be used when a shared page can
     be used without blocking.
   - The driver layer consumes a small amount of additional memory in the form
     of additional fields in the various kernel core objects.
   - The use of stack pointers instead of pointers within kernel core
     objects while mapping shared objects avoids the need to synchronize the
     use of pointers within the kernel core objects themselves.

   <hr>
   @section LNetDRVDLD-ref References
   - <a href="http://lwn.net/Kernel/LDD3/">Linux Device Drivers,
Third Edition By Jonathan Corbet, Alessandro Rubini,
Greg Kroah-Hartman, 2005</a>
   - <a href="http://lwn.net/Articles/119652/">The new way of ioctl(),
Jonathan Corbet, 2005</a>
   - <a href="https://docs.google.com/a/seagate.com/document/d/1oGQQpJsYV779386N
tFSlSlRddJHYE8Bo5Asr4ZO4DS8/edit?hl=en_US">HLD of Mero LNet Transport</a>
   - @ref LNetDLD "LNet Transport DLD"
   - @ref ULNetCoreDLD "LNet Transport User Space Core DLD"
   - @ref KLNetCoreDLD "LNet Transport Kernel Space Core DLD"
 */

#include <linux/miscdevice.h>

M0_BASSERT(sizeof(struct nlx_xo_domain) < PAGE_SIZE);
M0_BASSERT(sizeof(struct nlx_xo_transfer_mc) < PAGE_SIZE);
M0_BASSERT(sizeof(struct nlx_xo_buffer) < PAGE_SIZE);
M0_BASSERT(sizeof(struct nlx_core_buffer_event) < PAGE_SIZE);

/* LNET_NIDSTR_SIZE is only defined in the kernel */
M0_BASSERT(M0_NET_LNET_NIDSTR_SIZE == LNET_NIDSTR_SIZE);

/**
   @defgroup LNetDevInternal LNet Transport Device Internals
   @ingroup LNetDev
   @brief Detailed functional specification of the internals of the
   LNet Transport Device

   @see @ref LNetDRVDLD "LNet Transport Device DLD" and @ref LNetDRVDLD-lspec

   @{
 */

/**
   Completes the kernel initialization of the kernel and shared core domain
   objects.  The user domain object is mapped into kernel space and its
   nlx_core_domain::cd_kpvt field is set.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.  The buffer size maximum fields are
   set on success.
 */
static int nlx_dev_ioctl_dom_init(struct nlx_kcore_domain *kd,
				  struct m0_lnet_dev_dom_init_params *p)

{
	struct page *pg;
	struct nlx_core_domain *cd;
	uint32_t off = NLX_PAGE_OFFSET((unsigned long) p->ddi_cd);
	int rc;

	m0_mutex_lock(&kd->kd_drv_mutex);
	if (!nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc)) {
		m0_mutex_unlock(&kd->kd_drv_mutex);
		return M0_ERR(-EBADR);
	}
	if (off + sizeof *cd > PAGE_SIZE)
		return M0_ERR(-EBADR);

	/* note: these calls can block */
	down_read(&current->mm->mmap_sem);
	rc = WRITABLE_USER_PAGE_GET(p->ddi_cd, pg);
	up_read(&current->mm->mmap_sem);

	if (rc >= 0) {
		M0_ASSERT(rc == 1);
		nlx_core_kmem_loc_set(&kd->kd_cd_loc, pg, off);
		cd = nlx_kcore_core_domain_map(kd);
		rc = kd->kd_drv_ops->ko_dom_init(kd, cd);
		if (rc == 0) {
			p->ddi_max_buffer_size =
			    nlx_core_get_max_buffer_size(cd);
			p->ddi_max_buffer_segment_size =
			    nlx_core_get_max_buffer_segment_size(cd);
			p->ddi_max_buffer_segments =
			    nlx_core_get_max_buffer_segments(cd);
			M0_ASSERT(nlx_kcore_domain_invariant(kd));
			M0_ASSERT(!nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc));
		}
		nlx_kcore_core_domain_unmap(kd);
	}
	m0_mutex_unlock(&kd->kd_drv_mutex);
	return M0_RC(rc);
}

/**
   Registers a shared memory buffer with the kernel domain.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.
 */
static int nlx_dev_ioctl_buf_register(struct nlx_kcore_domain *kd,
				      struct m0_lnet_dev_buf_register_params *p)
{
	struct page *pg;
	struct nlx_core_buffer *cb;
	struct nlx_kcore_buffer *kb;
	uint32_t off = NLX_PAGE_OFFSET((unsigned long) p->dbr_lcbuf);
	m0_bcount_t sz;
	void **buf;
	m0_bcount_t *count;
	int n = p->dbr_bvec.ov_vec.v_nr;
	int rc;

	if (off + sizeof *cb > PAGE_SIZE)
		return M0_ERR(-EBADR);
	if (p->dbr_buffer_id == 0)
		return M0_ERR(-EBADR);
	NLX_ALLOC_ARR(buf, n);
	if (buf == NULL)
		return M0_ERR(-ENOMEM);
	NLX_ALLOC_ARR(count, n);
	if (count == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto fail_count;
	}

	sz = n * sizeof *buf;
	if (copy_from_user(buf, (void __user *) p->dbr_bvec.ov_buf, sz)) {
		rc = M0_ERR(-EFAULT);
		goto fail_copy;
	}
	sz = n * sizeof *count;
	if (copy_from_user(count,
			   (void __user *) p->dbr_bvec.ov_vec.v_count, sz)) {
		rc = M0_ERR(-EFAULT);
		goto fail_copy;
	}

	NLX_ALLOC_PTR(kb);
	if (kb == NULL) {
		rc = M0_ERR(-ENOMEM);
		goto fail_copy;
	}
	kb->kb_magic = M0_NET_LNET_KCORE_BUF_MAGIC;

	down_read(&current->mm->mmap_sem);
	rc = WRITABLE_USER_PAGE_GET(p->dbr_lcbuf, pg);
	up_read(&current->mm->mmap_sem);
	if (rc < 0)
		goto fail_page;
	nlx_core_kmem_loc_set(&kb->kb_cb_loc, pg, off);
	cb = nlx_kcore_core_buffer_map(kb);
	if (cb->cb_magic != 0 || cb->cb_buffer_id != 0 || cb->cb_kpvt != NULL) {
		rc = -EBADR;
		goto fail_cb;
	}
	rc = kd->kd_drv_ops->ko_buf_register(kd, p->dbr_buffer_id, cb, kb);
	if (rc != 0)
		goto fail_cb;

	p->dbr_bvec.ov_buf = buf;
	p->dbr_bvec.ov_vec.v_count = count;
	rc = nlx_kcore_buffer_uva_to_kiov(kb, &p->dbr_bvec);
	p->dbr_bvec.ov_buf = NULL;
	p->dbr_bvec.ov_vec.v_count = NULL;
	if (rc != 0)
		goto fail_kiov;

	M0_ASSERT(kb->kb_kiov != NULL && kb->kb_kiov_len > 0);
	M0_POST(nlx_kcore_buffer_invariant(cb->cb_kpvt));
	nlx_kcore_core_buffer_unmap(kb);
	m0_mutex_lock(&kd->kd_drv_mutex);
	drv_bufs_tlist_add(&kd->kd_drv_bufs, kb);
	m0_mutex_unlock(&kd->kd_drv_mutex);
	m0_free(count);
	m0_free(buf);
	return 0;

fail_kiov:
	kd->kd_drv_ops->ko_buf_deregister(cb, kb);
fail_cb:
	nlx_kcore_core_buffer_unmap(kb);
	WRITABLE_USER_PAGE_PUT(kb->kb_cb_loc.kl_page);
fail_page:
	kb->kb_magic = 0;
	m0_free(kb);
fail_copy:
	m0_free(count);
fail_count:
	m0_free(buf);
	M0_ASSERT(rc < 0);
	return M0_RC(rc);
}

/**
   Unpins the pages referenced in a nlx_kcore_buffer::kb_kiov.
   @param kb The kernel buffer object.
 */
static void nlx_dev_buf_pages_unpin(const struct nlx_kcore_buffer *kb)
{
	size_t i;

	for (i = 0; i < kb->kb_kiov_len; ++i)
		WRITABLE_USER_PAGE_PUT(kb->kb_kiov[i].kiov_page);
}

/**
   Deregisters a shared memory buffer from the kernel domain.
   @param kd The kernel domain object.
   @param kb The kernel buffer object, freed upon return.
 */
static int nlx_dev_buf_deregister(struct nlx_kcore_domain *kd,
				  struct nlx_kcore_buffer *kb)
{
	struct nlx_core_buffer *cb;

	if (!nlx_kcore_buffer_invariant(kb))
		return M0_ERR(-EBADR);
	m0_mutex_lock(&kd->kd_drv_mutex);
	drv_bufs_tlist_del(kb);
	m0_mutex_unlock(&kd->kd_drv_mutex);
	nlx_dev_buf_pages_unpin(kb);
	cb = nlx_kcore_core_buffer_map(kb);
	kd->kd_drv_ops->ko_buf_deregister(cb, kb);
	nlx_kcore_core_buffer_unmap(kb);
	WRITABLE_USER_PAGE_PUT(kb->kb_cb_loc.kl_page);
	m0_free(kb);
	return 0;
}

/**
   Deregisters a shared memory buffer from the kernel domain.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.
 */
static int nlx_dev_ioctl_buf_deregister(struct nlx_kcore_domain *kd,
				   struct m0_lnet_dev_buf_deregister_params *p)
{
	struct nlx_kcore_buffer *kb = p->dbd_kb;

	/* protect against user space passing invalid ptr */
	if (!virt_addr_valid(kb))
		return M0_ERR(-EBADR);
	return nlx_dev_buf_deregister(kd, kb);
}

/**
   Enqueues a buffer on the appropriate transfer machine queue.
   @param p Ioctl request parameters.
   @param op Buffer queue operation to perform.
 */
static int nlx_dev_ioctl_buf_queue_op(
				  const struct m0_lnet_dev_buf_queue_params *p,
				  nlx_kcore_queue_op_t op)
{
	struct nlx_kcore_transfer_mc *ktm = p->dbq_ktm;
	struct nlx_kcore_buffer *kb = p->dbq_kb;
	struct nlx_core_buffer *cb;
	int rc;

	M0_PRE(op != NULL);
	if (!virt_addr_valid(ktm) || !virt_addr_valid(kb))
		return M0_ERR(-EBADR);
	if (!nlx_kcore_tm_invariant(ktm))
		return M0_ERR(-EBADR);
	if (!nlx_kcore_buffer_invariant(kb))
		return M0_ERR(-EBADR);
	cb = nlx_kcore_core_buffer_map(kb);
	if (!nlx_core_buffer_invariant(cb))
		rc = -EBADR;
	else
		rc = op(ktm, cb, kb);
	nlx_kcore_core_buffer_unmap(kb);

	return M0_RC(rc);
}

/**
   Cancels a buffer operation if possible.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.
 */
static int nlx_dev_ioctl_buf_del(const struct nlx_kcore_domain *kd,
				 const struct m0_lnet_dev_buf_queue_params *p)
{
	struct nlx_kcore_transfer_mc *ktm = p->dbq_ktm;
	struct nlx_kcore_buffer *kb = p->dbq_kb;

	if (!virt_addr_valid(ktm) || !virt_addr_valid(kb))
		return M0_ERR(-EBADR);
	if (!nlx_kcore_tm_invariant(ktm))
		return M0_ERR(-EBADR);
	if (!nlx_kcore_buffer_invariant(kb))
		return M0_ERR(-EBADR);
	return kd->kd_drv_ops->ko_buf_del(ktm, kb);
}

/**
   Waits for buffer events, or the timeout.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.
 */
static int nlx_dev_ioctl_buf_event_wait(const struct nlx_kcore_domain *kd,
			     const struct m0_lnet_dev_buf_event_wait_params *p)
{
	struct nlx_kcore_transfer_mc *ktm = p->dbw_ktm;
	struct nlx_core_transfer_mc *ctm;
	int rc;

	if (!virt_addr_valid(ktm))
		return M0_ERR(-EBADR);
	M0_ASSERT(nlx_kcore_tm_invariant(ktm));
	ctm = nlx_kcore_core_tm_map(ktm);
	if (!nlx_core_tm_invariant(ctm))
		rc = M0_ERR(-EBADR);
	else
		rc = kd->kd_drv_ops->ko_buf_event_wait(ctm, ktm,
						       p->dbw_timeout);
	nlx_kcore_core_tm_unmap(ktm);

	return M0_RC(rc);
}

/**
   Decodes a NID string into a NID.
   @param p Ioctl request parameters. The m0_lnet_dev_nid_encdec_params::dn_nid
   field is set on success.
 */
static int nlx_dev_ioctl_nidstr_decode(struct m0_lnet_dev_nid_encdec_params *p)
{
	return nlx_kcore_nidstr_decode(p->dn_buf, &p->dn_nid);
}

/**
   Encodes a NID into a NID string.
   @param p Ioctl request parameters. The m0_lnet_dev_nid_encdec_params::dn_buf
   field is set on success.
 */
static int nlx_dev_ioctl_nidstr_encode(struct m0_lnet_dev_nid_encdec_params *p)
{
	return nlx_kcore_nidstr_encode(p->dn_nid, p->dn_buf);
}

/**
   Gets the NID strings of all the local LNet interfaces.
   The NID strings are encoded consecutively in user space buffer denoted by
   the m0_lnet_dev_nidstrs_get_params::dng_buf field as a sequence nul
   terminated strings, with an final nul (string) terminating the list.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.
   @retval -EFBIG if the strings do not fit in the provided buffer.
 */
static int nlx_dev_ioctl_nidstrs_get(struct nlx_kcore_domain *kd,
				     struct m0_lnet_dev_nidstrs_get_params *p)
{
	char * const *nidstrs;
	int rc = nlx_kcore_nidstrs_get(&nidstrs);
	char *buf;
	m0_bcount_t sz;
	int i;

	if (rc != 0)
		return M0_RC(rc);
	for (i = 0, sz = 1; nidstrs[i] != NULL; ++i)
		sz += strlen(nidstrs[i]) + 1;
	if (sz > p->dng_size) {
		nlx_kcore_nidstrs_put(&nidstrs);
		return M0_ERR_INFO(-EFBIG, "sz=%"PRIu64" p->dng_size=%"PRIu64,
				   sz, p->dng_size);
	}
	NLX_ALLOC(buf, sz);
	if (buf == NULL) {
		nlx_kcore_nidstrs_put(&nidstrs);
		return M0_ERR(-ENOMEM);
	}
	for (i = 0, sz = 0; nidstrs[i] != NULL; ++i) {
		strcpy(&buf[sz], nidstrs[i]);
		sz += strlen(nidstrs[i]) + 1;
	}
	nlx_kcore_nidstrs_put(&nidstrs);
	if (copy_to_user((void __user *) p->dng_buf, buf, sz))
		rc = -EFAULT;
	else
		rc = i;
	m0_free(buf);

	return M0_RC(rc);
}

/**
   Completes the kernel portion of the TM start logic.
   The shared transfer machine object is pinned in kernel space and its
   nlx_core_transfer_mc::ctm_kpvt field is set.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.
 */
static int nlx_dev_ioctl_tm_start(struct nlx_kcore_domain *kd,
				  struct m0_lnet_dev_tm_start_params *p)
{
	struct page *pg;
	struct nlx_core_transfer_mc *ctm;
	struct nlx_kcore_transfer_mc *ktm;
	unsigned long utmp = (unsigned long) p->dts_ctm;
	uint32_t off = NLX_PAGE_OFFSET(utmp);
	int rc;

	if (off + sizeof *ctm > PAGE_SIZE)
		return M0_ERR(-EBADR);
	NLX_ALLOC_PTR(ktm);
	if (ktm == NULL)
		return M0_ERR(-ENOMEM);
	ktm->ktm_magic = M0_NET_LNET_KCORE_TM_MAGIC;

	down_read(&current->mm->mmap_sem);
	rc = WRITABLE_USER_PAGE_GET(utmp, pg);
	up_read(&current->mm->mmap_sem);
	if (rc < 0) {
		M0_LOG(M0_ERROR, "WRITABLE_USER_PAGE_GET() failed: rc=%d", rc);
		goto fail_page;
	}
	nlx_core_kmem_loc_set(&ktm->ktm_ctm_loc, pg, off);
	ctm = nlx_kcore_core_tm_map(ktm);
	if (ctm->ctm_magic != 0 || ctm->ctm_mb_counter != 0 ||
	    ctm->ctm_kpvt != NULL) {
		rc = M0_ERR(-EBADR);
		goto fail_ctm;
	}
	rc = kd->kd_drv_ops->ko_tm_start(kd, ctm, ktm);
	if (rc != 0)
		goto fail_ctm;
	nlx_kcore_core_tm_unmap(ktm);
	m0_mutex_lock(&kd->kd_drv_mutex);
	drv_tms_tlist_add(&kd->kd_drv_tms, ktm);
	m0_mutex_unlock(&kd->kd_drv_mutex);
	return M0_RC(0);

fail_ctm:
	nlx_kcore_core_tm_unmap(ktm);
	WRITABLE_USER_PAGE_PUT(ktm->ktm_ctm_loc.kl_page);
fail_page:
	ktm->ktm_magic = 0;
	m0_free(ktm);
	M0_ASSERT(rc != 0);
	return M0_ERR(rc);
}

/**
   Helper for nlx_dev_close() and nlx_dev_ioctl_tm_stop() to clean up kernel
   resources associated with an individual transfer machine.
   @param kd The kernel domain object.
   @param ktm The kernel transfer machine object, removed from the
   kd->kd_drv_tms and freed upon return.
 */
static int nlx_dev_tm_cleanup(struct nlx_kcore_domain *kd,
			      struct nlx_kcore_transfer_mc *ktm)
{
	struct nlx_kcore_buffer_event *kbev;
	struct nlx_core_transfer_mc *ctm;

	if (!nlx_kcore_tm_invariant(ktm))
		return M0_ERR(-EBADR);
	m0_tl_for(drv_bevs, &ktm->ktm_drv_bevs, kbev) {
		WRITABLE_USER_PAGE_PUT(kbev->kbe_bev_loc.kl_page);
		m0_mutex_lock(&kd->kd_drv_mutex);
		/**
		 * @todo XXX list removal is protected by the lock, but
		 * iteration over the list isn't? --nikita.
		 */
		drv_bevs_tlist_del(kbev);
		m0_mutex_unlock(&kd->kd_drv_mutex);
		drv_bevs_tlink_fini(kbev);
		m0_free(kbev);
	} m0_tl_endfor;
	m0_mutex_lock(&kd->kd_drv_mutex);
	drv_tms_tlist_del(ktm);
	m0_mutex_unlock(&kd->kd_drv_mutex);

	ctm = nlx_kcore_core_tm_map(ktm);
	kd->kd_drv_ops->ko_tm_stop(ctm, ktm);
	nlx_kcore_core_tm_unmap(ktm);
	WRITABLE_USER_PAGE_PUT(ktm->ktm_ctm_loc.kl_page);
	m0_free(ktm);
	return 0;
}

/**
   Complete the kernel portion of the TM stop logic.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.
 */
static int nlx_dev_ioctl_tm_stop(struct nlx_kcore_domain *kd,
				 struct m0_lnet_dev_tm_stop_params *p)
{
	struct nlx_kcore_transfer_mc *ktm = p->dts_ktm;

	/* protect against user space passing invalid ptr */
	if (!virt_addr_valid(ktm))
		return M0_ERR(-EBADR);
	return nlx_dev_tm_cleanup(kd, ktm);
}

/**
   Blesses a shared nlx_core_buffer_event object.
   The shared buffer event object is pinned in kernel space and its
   nlx_core_buffer_event::cbe_kpvt field is set.  The bev_link_bless()
   function is used to bless the nlx_core_buffer_event::cbe_tm_link.
   @param kd The kernel domain object.
   @param p Ioctl request parameters.
 */
static int nlx_dev_ioctl_bev_bless(struct nlx_kcore_domain *kd,
				   struct m0_lnet_dev_bev_bless_params *p)
{
	struct page *pg;
	struct nlx_kcore_transfer_mc *ktm = p->dbb_ktm;
	struct nlx_kcore_buffer_event *kbe;
	struct nlx_core_buffer_event *cbe;
	uint32_t off = NLX_PAGE_OFFSET((unsigned long) p->dbb_bev);
	int rc;

	if (!virt_addr_valid(ktm))
		return M0_ERR(-EBADR);
	if (!nlx_kcore_tm_invariant(ktm))
		return M0_ERR(-EBADR);
	if (off + sizeof *cbe > PAGE_SIZE)
		return M0_ERR(-EBADR);

	NLX_ALLOC_PTR(kbe);
	if (kbe == NULL)
		return M0_ERR(-ENOMEM);
	drv_bevs_tlink_init(kbe);

	down_read(&current->mm->mmap_sem);
	rc = WRITABLE_USER_PAGE_GET(p->dbb_bev, pg);
	up_read(&current->mm->mmap_sem);
	if (rc < 0)
		goto fail_page;
	nlx_core_kmem_loc_set(&kbe->kbe_bev_loc, pg, off);
	cbe = nlx_kcore_core_bev_map(kbe);
	if (cbe->cbe_kpvt != NULL ||
	    !nlx_core_kmem_loc_is_empty(&cbe->cbe_tm_link.cbl_p_self_loc)) {
		rc = -EBADR;
		goto fail_cbe;
	}
	bev_link_bless(&cbe->cbe_tm_link, pg);
	cbe->cbe_kpvt = kbe;
	nlx_kcore_core_bev_unmap(kbe);
	m0_mutex_lock(&kd->kd_drv_mutex);
	drv_bevs_tlist_add(&ktm->ktm_drv_bevs, kbe);
	m0_mutex_unlock(&kd->kd_drv_mutex);
	return 0;

fail_cbe:
	nlx_kcore_core_bev_unmap(kbe);
	WRITABLE_USER_PAGE_PUT(kbe->kbe_bev_loc.kl_page);
fail_page:
	drv_bevs_tlink_fini(kbe);
	kbe->kbe_magic = 0;
	m0_free(kbe);
	M0_ASSERT(rc != 0);
	return M0_RC(rc);
}

/**
   Performs an unlocked (BKL is not held) ioctl request on the m0lnet device.

   @pre nlx_kcore_domain_invariant(file->private_data)
   @param file File instance, corresponding to the nlx_kcore_domain.
   @param cmd The request or operation to perform.
   @param arg Argument to the operation, internally treated as a pointer
   whose type depends on the cmd.
 */
static long nlx_dev_ioctl(struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	struct nlx_kcore_domain *kd = file->private_data;
	union {
		struct m0_lnet_dev_dom_init_params       dip;
		struct m0_lnet_dev_tm_start_params       tsp;
		struct m0_lnet_dev_tm_stop_params        tpp;
		struct m0_lnet_dev_buf_register_params   brp;
		struct m0_lnet_dev_buf_deregister_params bdp;
		struct m0_lnet_dev_buf_queue_params      bqp;
		struct m0_lnet_dev_buf_event_wait_params bep;
		struct m0_lnet_dev_nid_encdec_params     nep;
		struct m0_lnet_dev_nidstrs_get_params    ngp;
		struct m0_lnet_dev_bev_bless_params      bbp;
	} p;
	unsigned sz = _IOC_SIZE(cmd);
	int rc;

	M0_THREAD_ENTER;
	M0_PRE(nlx_kcore_domain_invariant(kd));

	if (_IOC_TYPE(cmd) != M0_LNET_IOC_MAGIC ||
	    _IOC_NR(cmd) < M0_LNET_IOC_MIN_NR  ||
	    _IOC_NR(cmd) > M0_LNET_IOC_MAX_NR || sz > sizeof p) {
		rc = M0_ERR(-ENOTTY);
		goto done;
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (copy_from_user(&p, (void __user *) arg, sz)) {
			rc = M0_ERR(-EFAULT);
			goto done;
		}
	}

	switch (cmd) {
	case M0_LNET_DOM_INIT:
		rc = nlx_dev_ioctl_dom_init(kd, &p.dip);
		break;
	case M0_LNET_TM_START:
		rc = nlx_dev_ioctl_tm_start(kd, &p.tsp);
		break;
	case M0_LNET_TM_STOP:
		rc = nlx_dev_ioctl_tm_stop(kd, &p.tpp);
		break;
	case M0_LNET_BUF_REGISTER:
		rc = nlx_dev_ioctl_buf_register(kd, &p.brp);
		break;
	case M0_LNET_BUF_DEREGISTER:
		rc = nlx_dev_ioctl_buf_deregister(kd, &p.bdp);
		break;
	case M0_LNET_BUF_MSG_RECV:
		rc = nlx_dev_ioctl_buf_queue_op(&p.bqp,
					      kd->kd_drv_ops->ko_buf_msg_recv);
		break;
	case M0_LNET_BUF_MSG_SEND:
		rc = nlx_dev_ioctl_buf_queue_op(&p.bqp,
					      kd->kd_drv_ops->ko_buf_msg_send);
		break;
	case M0_LNET_BUF_ACTIVE_RECV:
		rc = nlx_dev_ioctl_buf_queue_op(&p.bqp,
					   kd->kd_drv_ops->ko_buf_active_recv);
		break;
	case M0_LNET_BUF_ACTIVE_SEND:
		rc = nlx_dev_ioctl_buf_queue_op(&p.bqp,
					   kd->kd_drv_ops->ko_buf_active_send);
		break;
	case M0_LNET_BUF_PASSIVE_RECV:
		rc = nlx_dev_ioctl_buf_queue_op(&p.bqp,
					  kd->kd_drv_ops->ko_buf_passive_recv);
		break;
	case M0_LNET_BUF_PASSIVE_SEND:
		rc = nlx_dev_ioctl_buf_queue_op(&p.bqp,
					  kd->kd_drv_ops->ko_buf_passive_send);
		break;
	case M0_LNET_BUF_DEL:
		rc = nlx_dev_ioctl_buf_del(kd, &p.bqp);
		break;
	case M0_LNET_BUF_EVENT_WAIT:
		rc = nlx_dev_ioctl_buf_event_wait(kd, &p.bep);
		break;
	case M0_LNET_BEV_BLESS:
		rc = nlx_dev_ioctl_bev_bless(kd, &p.bbp);
		break;
	case M0_LNET_NIDSTR_DECODE:
		rc = nlx_dev_ioctl_nidstr_decode(&p.nep);
		break;
	case M0_LNET_NIDSTR_ENCODE:
		rc = nlx_dev_ioctl_nidstr_encode(&p.nep);
		break;
	case M0_LNET_NIDSTRS_GET:
		rc = nlx_dev_ioctl_nidstrs_get(kd, &p.ngp);
		break;
	default:
		rc = M0_ERR(-ENOTTY);
		break;
	}

	if (rc >= 0 && (_IOC_DIR(cmd) & _IOC_READ)) {
		if (copy_to_user((void __user *) arg, &p, sz))
			rc = M0_ERR(-EFAULT);
	}

done:
	if (rc < 0 && ergo(cmd == M0_LNET_BUF_EVENT_WAIT,
	                   !M0_IN(rc, (-ETIMEDOUT, -ERESTARTSYS))))
		M0_LOG(M0_ERROR, "cmd=0x%x rc=%d", cmd, rc);
	if (rc == -ERESTARTSYS)
		rc = -EINTR;
	return M0_RC(rc);
}

/**
   Open the /dev/m0lnet device.

   There is a 1:1 correspondence between struct file objects and
   nlx_kcore_domain objects.  Thus, user processes will open the m0lnet
   device once for each m0_net_domain.
   @param inode Inode object for the device.
   @param file File object for this instance.
 */
static int nlx_dev_open(struct inode *inode, struct file *file)
{
	struct nlx_kcore_domain *kd;
	int rc;
	M0_THREAD_ENTER;

	if (!(file->f_flags & O_RDWR))
		return M0_ERR(-EACCES);

	NLX_ALLOC_PTR(kd);
	if (kd == NULL)
		return M0_ERR(-ENOMEM);
	rc = nlx_kcore_kcore_dom_init(kd);
	if (rc != 0) {
		m0_free(kd);
	} else {
		file->private_data = kd;
	}
	return M0_RC(rc);
}

/**
   Releases all resources for the given struct file.

   This operation is called once when the file is being released.  There is a
   1:1 correspondence between struct file objects and nlx_kcore_domain objects,
   so this operation will release all kernel resources for the domain.  That can
   be expensive if the user process failed to release all transfer machines
   and buffers before closing the file.

   @param inode Device inode object.
   @param file File object being released.
 */
M0_INTERNAL int nlx_dev_close(struct inode *inode, struct file *file)
{
	struct nlx_kcore_domain *kd =
	    (struct nlx_kcore_domain *) file->private_data;
	struct nlx_core_domain *cd;
	struct nlx_kcore_transfer_mc *ktm;
	struct nlx_kcore_buffer *kb;
	bool cleanup = false;
	int rc;
	M0_THREAD_ENTER;

	M0_PRE(nlx_kcore_domain_invariant(kd));
	file->private_data = NULL;

	/*
	 * user program may not unmap all areas, eg if it was killed.
	 * 1. Cancel all outstanding buffer operations.
	 * 2. Clean up (stop, et al) all running TMs, this can take a while.
	 * 3. De-register all buffers.
	 */
	m0_tl_for(drv_bufs, &kd->kd_drv_bufs, kb) {
		ktm = kb->kb_ktm;
		if (ktm != NULL) {
			/*
			 * Only LNetMDUnlink() causes nlx_kcore_LNetMDUnlink()
			 * failure.  That can happen if the operation completes
			 * concurrently with the execution of this loop.
			 * Such failures are OK in this context.
			 */
			nlx_kcore_LNetMDUnlink(ktm, kb);
		}
	} m0_tl_endfor;
	m0_tl_for(drv_tms, &kd->kd_drv_tms, ktm) {
		/*
		 * Wait until no more buffers are associated with this TM and
		 * the event callback is no longer using the ktm.  Must be in
		 * the ktm-based loop because it needs to synchronize use of the
		 * kb_ktm with the LNet event callback using the spinlock. Only
		 * holds spinlock for extremely short periods to avoid seriously
		 * blocking event callback.  Depends on:
		 * 1. There can be no other threads down-ing ktm_sem or adding
		 *    new buffers to queues while in nlx_dev_close().
		 * 2. kb_ktm is set to NULL in the spinlock and before ktm_sem
		 *    is up'd.
		 * 3. The final reference to the ktm in nlx_kcore_eq_cb() is to
		 *    unlock the spinlock, after up-ing ktm_sem.
		 * 4. LNet events involving unlink are not dropped.
		 */
		m0_tlist_for(&drv_bufs_tl, &kd->kd_drv_bufs, kb) {
			spin_lock(&ktm->ktm_bevq_lock);
			while (kb->kb_ktm == ktm) {
				spin_unlock(&ktm->ktm_bevq_lock);
				wait_event(ktm->ktm_wq, kb->kb_ktm == NULL);
				spin_lock(&ktm->ktm_bevq_lock);
			}
			spin_unlock(&ktm->ktm_bevq_lock);
		} m0_tlist_endfor;

		rc = nlx_dev_tm_cleanup(kd, ktm);
		M0_ASSERT(rc == 0);
		cleanup = true;
	} m0_tl_endfor;
	m0_tl_for(drv_bufs, &kd->kd_drv_bufs, kb) {
		rc = nlx_dev_buf_deregister(kd, kb);
		M0_ASSERT(rc == 0);
		cleanup = true;
	} m0_tl_endfor;

	if (cleanup)
		M0_LOG(M0_NOTICE, "close cleanup");

	/* user program may not successfully perform M0_NET_DOM_INIT ioctl */
	if (!nlx_core_kmem_loc_is_empty(&kd->kd_cd_loc)) {
		cd = nlx_kcore_core_domain_map(kd);
		kd->kd_drv_ops->ko_dom_fini(kd, cd);
		nlx_kcore_core_domain_unmap(kd);
		WRITABLE_USER_PAGE_PUT(kd->kd_cd_loc.kl_page);
		nlx_core_kmem_loc_set(&kd->kd_cd_loc, NULL, 0);
	}

	nlx_kcore_kcore_dom_fini(kd);
	m0_free(kd);
	return M0_RC(0);
}

/** File operations for the m0lnet device, UT can override. */
static struct file_operations nlx_dev_file_ops = {
	.owner          = THIS_MODULE,
        .unlocked_ioctl = nlx_dev_ioctl,
        .open           = nlx_dev_open,
        .release        = nlx_dev_close
};

/**
   Device description.
   The device is named /dev/m0lnet when nlx_dev_init() completes.
   The major number is 10 (misc), and the minor number is assigned dynamically
   when misc_register() is called.
 */
static struct miscdevice nlx_dev = {
        .minor   = MISC_DYNAMIC_MINOR,
        .name    = M0_LNET_DEV,
        .fops    = &nlx_dev_file_ops
};
static bool nlx_dev_registered = false;

/** @} */ /* LNetDevInternal */

/**
   @addtogroup LNetDev
   @{
 */

M0_INTERNAL int nlx_dev_init(void)
{
	int rc;

	rc = misc_register(&nlx_dev);
	if (rc != 0)
		return M0_RC(rc);
	nlx_dev_registered = true;
	printk("Mero %s registered with minor %d\n",
	       nlx_dev.name, nlx_dev.minor);
	return M0_RC(rc);
}

M0_INTERNAL void nlx_dev_fini(void)
{
	if (nlx_dev_registered) {
		misc_deregister(&nlx_dev);
		nlx_dev_registered = false;
		printk("Mero %s deregistered\n", nlx_dev.name);
	}
}

/** @} */ /* LNetDev */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
