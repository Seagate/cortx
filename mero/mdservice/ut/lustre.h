/* -*- C -*- */
/*
 * COPYRIGHT 2011 XYRATEX TECHNOLOGY LIMITED
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
 * Original creation date: 01/17/2011
 */

#pragma once

#ifndef __MERO_MDSERVICE_UT_LUSTRE_H__
#define __MERO_MDSERVICE_UT_LUSTRE_H__

/**
   These two structures used for testing mdstore functionality. To do
   so we use changelog dump created by dump.changelog program, parse
   it, convert to fops and feed to test program in fops form.
*/
struct m0_md_lustre_fid {
        uint64_t f_seq;
        uint32_t f_oid;
        uint32_t f_ver;
};

struct m0_md_lustre_logrec {
        uint16_t                 cr_namelen;
        uint16_t                 cr_flags;
        uint16_t                 cr_valid;
        uint32_t                 cr_mode;
        uint8_t                  cr_type;
        uint64_t                 cr_index;
        uint64_t                 cr_time;
        uint64_t                 cr_atime;
        uint64_t                 cr_ctime;
        uint64_t                 cr_mtime;
        uint32_t                 cr_nlink;
        uint32_t                 cr_rdev;
        uint64_t                 cr_version;
        uint64_t                 cr_size;
        uint64_t                 cr_blocks;
        uint64_t                 cr_blksize;
        uint32_t                 cr_uid;
        uint32_t                 cr_gid;
        uint32_t                 cr_sid;
        uint64_t                 cr_clnid;
        struct m0_md_lustre_fid  cr_tfid;
        struct m0_md_lustre_fid  cr_pfid;
        char                     cr_name[0];
} __attribute__((packed));

enum m0_md_lustre_logrec_type {
        RT_MARK     = 0,
        RT_CREATE   = 1,
        RT_MKDIR    = 2,
        RT_HARDLINK = 3,
        RT_SOFTLINK = 4,
        RT_MKNOD    = 5,
        RT_UNLINK   = 6,
        RT_RMDIR    = 7,
        RT_RENAME   = 8,
        RT_EXT      = 9,
        RT_OPEN     = 10,
        RT_CLOSE    = 11,
        RT_IOCTL    = 12,
        RT_TRUNC    = 13,
        RT_SETATTR  = 14,
        RT_XATTR    = 15,
        RT_HSM      = 16,
        RT_MTIME    = 17,
        RT_CTIME    = 18,
        RT_ATIME    = 19,
        RT_LAST
};

#endif
