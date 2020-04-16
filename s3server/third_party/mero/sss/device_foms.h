/* -*- C -*- */
/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 21-Apr-2015
 */

#pragma once
#ifndef __MERO_SSS_DEVICE_H__
#define __MERO_SSS_DEVICE_H__

#include "fop/fom_generic.h"

/**
  @addtogroup DLDGRP_sss_device Device command

  @section DLD_sss_device-fom Device command FOM

  - @ref DLD-sss_device-fspec-ds
  - @ref DLD-sss_device-fspec-usecases-attach
  - @ref DLD-sss_device-fspec-usecases-detach
  - @ref DLD-sss_device-fspec-usecases-format

  @section DLD-sss_device-fspec Functional Specification
  Device commands provides control of individual devices like attach, detach and
  format using directly creation/deletion stob and stob domain and pool events.

   Generic TX fom phases start TX transaction. Its interfere to create and
   finalize AD stob domain. Work with AD stob domain made a separate stage.
   For work with AD stob domain need to get additional data. Read them from
   confc subsystem move to first stage too.
   Both stages can to diagnose error and can go to Failure phase.
   Fom phase SSS_DFOM_SWITCH switch to next dependence Device command ID and
   current stage.

   First stage - before TX generic fom phases:
   - initiate fom - check device by disk fid, read additional data: cid,
   storage device fid from confc
   - work with AD stob domain and stob

   Second stage - after TX generic fom phases:
   - work with Pool machine and Pool events

  @subsection DLD-sss_device-fspec-ds Data Structures
  - m0_sss_device_fom
    Represents fom with additional fields describing device such as index in
    pool machine table, cid, storage device fid and current stage of command.

  @subsection DLD-sss_device-fspec-usecases-attach Device attach command
    Create AD stob domain and main stob for device and start pool event with
    change device status on online.

   State transition diagram for Device attach command

   @verbatim

              Standard fom generic phases
     inits, authenticate, resources, authorization
			 |
			 v
     +<---------SSS_DFOM_DISK_OPENING---+
     |				wait for opening m0_conf_drive
     |			 v--------------+
     +<-----SSS_DFOM_DISK_HA_STATE_GET--+
     |                          wait for HA state nvec reply
     |			 v--------------+
     |        SSS_DFOM_DISK_OPENED------+
     |                          accept HA state nvec,
     |			        read m0_conf_drive,
     |			        wait for opening m0_conf_root
     |                   v--------------+
     |         SSS_DFOM_FS_OPENED,
     |			read m0_conf_root, start iterator m0_conf_sdev
     |                   v--------------+
     |         SSS_DFOM_SDEV_ITER,
     |			search m0_conf_sdev,
     |			check "sdev belong IO service of this node"
     |                   v--------------+
     +<--------SSS_DFOM_SWITCH
     |                   |
     |                   v
     +<-------- SSS_DFOM_SDEV_OPENING,
     |                   |
     |                   v
     +<--------SSS_DFOM_ATTACH_STOB
     |			 |
     |			 v
     +<--Standard fom generic phases TX context
     |			 |
     |			 v
     +<--------SSS_DFOM_SWITCH
     |			 |
     |			 v
     +<----SSS_DFOM_ATTACH_POOL_MACHINE
     |			 |
     |			 v
 FOPH_FAILED        FOPH_SUCCESS
     +------------------>|
			 v
            Standard fom generic phases
               send reply and finish

   @endverbatim

  @subsection DLD-sss_device-fspec-usecases-detach Device detach command
    Finalization AD stob domain and main stob for device and start pool event
    with change device status on offline.
    Note! Finalization of stob domain is not equal to its destruction.
          Finalization does not delete stob's meta-data in BE.

   State transition diagram for Device detach command

   @verbatim

              Standard fom generic phases
     inits, authenticate, resources, authorization
			 |
			 v
     +<---------SSS_DFOM_DISK_OPENING---+
     |				wait for opening m0_conf_drive
     |			 v--------------+
     |      SSS_DFOM_DISK_HA_STATE_GET--+
     |                          just go next doing no query
     |			 v--------------+
     |        SSS_DFOM_DISK_OPENED------+
     |			read m0_conf_drive, wait for opening m0_conf_root
     |                   v--------------+
     |         SSS_DFOM_FS_OPENED,
     |			read m0_conf_root, start iterator m0_conf_sdev
     |                   v--------------+
     |         SSS_DFOM_SDEV_ITER,
     |			search m0_conf_sdev,
     |			check "sdev belong IO service of this node"
     |                   v--------------+
     +<--------SSS_DFOM_SWITCH
     |			 |
     |			 v
     +<--------SSS_DFOM_DETACH_STOB
     |			 |
     |			 v
     +<--Standard fom generic phases TX context
     |			 |
     |			 v
     +<--------SSS_DFOM_SWITCH
     |			 |
     |			 v
     +<----SSS_DFOM_DETACH_POOL_MACHINE
     |			 |
     |			 v
 FOPH_FAILED        FOPH_SUCCESS
     +------------------>|
			 v
            Standard fom generic phases
               send reply and finish

   @endverbatim

  @subsection DLD-sss_device-fspec-usecases-format Device format command
    Format HW device

   State transition diagram for Device format command

     @verbatim

              Standard fom generic phases
     inits, authenticate, resources, authorization
			 |
			 v
     +<---------SSS_DFOM_DISK_OPENING---+
     |				wait for opening m0_conf_drive
     |			 v--------------+
     |      SSS_DFOM_DISK_HA_STATE_GET--+
     |                          just go next doing no query
     |			 v--------------+
     |        SSS_DFOM_DISK_OPENED------+
     |			read m0_conf_drive, wait for opening m0_conf_root
     |                   v--------------+
     |         SSS_DFOM_FS_OPENED,
     |			read m0_conf_root, start iterator m0_conf_sdev
     |                   v--------------+
     |         SSS_DFOM_SDEV_ITER,
     |			search m0_conf_sdev,
     |			check "sdev belong IO service of this node"
     |                   v--------------+
     +<--------SSS_DFOM_SWITCH
     |			 |
     |			 v
     +<--Standard fom generic phases TX context
     |			 |
     |			 v
     +<-----------SSS_DFOM_SWITCH
     |			 |
     |			 v
     +<-----------SSS_DFOM_FORMAT
     |			 |
     |			 v
 FOPH_FAILED        FOPH_SUCCESS
     +------------------>|
			 v
            Standard fom generic phases
               send reply and finish

   @endverbatim
*/

/**
 * Device command fom phases enumeration
 */
enum sss_device_fom_phases {
	/**
	 * Select next step depending on Device command and current stage.
	 */
	SSS_DFOM_SWITCH = M0_FOPH_NR + 1,
	/**
	* Start read additional data from confc
	*/
	SSS_DFOM_DISK_OPENING,
	/**
	 * Initiate getting device HA state. Conf object for the device being
	 * re-attached must retain previously set HA state.
	 */
	SSS_DFOM_DISK_HA_STATE_GET,
	/**
	 * FOM waits until disk configuration object is retrieved.
	 * Also internal FOM state is populated after configuration is
	 * retrieved.
	 */
	SSS_DFOM_DISK_OPENED,
	/**
	 * m0_conf_root opened
	 */
	SSS_DFOM_FS_OPENED,
	/**
	 * SDEV iterator - search sdev with it parent
	 */
	SSS_DFOM_SDEV_ITER,
	/**
	 * Open storage device configuration object.
	 * Start Conf iterator for search storage device parent - IO service
	 */
	SSS_DFOM_SDEV_OPENING,
	/**
	 * Create AD stob domain and main stob for device.
	 */
	SSS_DFOM_ATTACH_STOB,
	/**
	 * Change device status on online in Pool machine. Create and run Pool
	 * event.
	 */
	SSS_DFOM_ATTACH_POOL_MACHINE,
	/**
	 * Finalization AD stob domain and main stob for device.
	 */
	SSS_DFOM_DETACH_STOB,
	/**
	 * Wait until stob detach operation is completed.
	 */
	SSS_DFOM_DETACH_STOB_WAIT,
	/**
	 * Change device status on offline in Pool machine. Create and run Pool
	 * event.
	 */
	SSS_DFOM_DETACH_POOL_MACHINE,
	/**
	 * Format device
	 */
	SSS_DFOM_FORMAT,
};

/** @} end group DLD-sss_device */

#endif /* __MERO_SSS_DEVICE_H__ */
/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
