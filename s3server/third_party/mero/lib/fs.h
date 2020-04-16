/* -*- C -*- */
/*
 * COPYRIGHT 2014 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 2-Sep-2014
 */

#pragma once

#ifndef __MERO_LIB_FS_H__
#define __MERO_LIB_FS_H__

/**
 * Removes directory along with its files.
 * Subdirectories are not allowed.
 *
 * @note returns 0 in case when @dir does not exist.
 * @note works in user-space only for now.
 */
M0_INTERNAL int m0_cleandir(const char *dir);

#ifndef __KERNEL__
/**
 * Reads file contents into dynamically allocated string.
 *
 * @note If the call succeeds, the user is responsible for freeing
 *       allocated memory with m0_free(*out).
 */
M0_INTERNAL int m0_file_read(const char *path, char **out);
#endif

#endif /* __MERO_LIB_FS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
