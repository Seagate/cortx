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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 06/28/2012
 */

#pragma once

#ifndef __MERO_NET_TEST_SLIST_H__
#define __MERO_NET_TEST_SLIST_H__

#include "lib/vec.h"		/* m0_bufvec */
#include "net/test/serialize.h"	/* m0_net_test_serialize_op */

/**
   @defgroup NetTestSListDFS String List
   @ingroup NetTestDFS

   @see
   @ref net-test

   @{
 */

/**
   String list.
 */
struct m0_net_test_slist {
	/**
	   Number of strings in the list. If it is 0, other fields are
	   not valid.
	 */
	size_t ntsl_nr;
	/**
	   Array of pointers to strings.
	 */
	char **ntsl_list;
	/**
	   Single array with NUL-separated strings (one after another).
	   ntsl_list contains the pointers to strings in this array.
	 */
	char  *ntsl_str;
};

/**
   Initialize a string list from a C string composed of individual sub-strings
   separated by a delimiter character.  The delimiter cannot be NUL and
   cannot be part of the sub-string.
   @pre slist != NULL
   @pre str != NULL
   @pre delim != NUL
   @post (result == 0) && m0_net_test_slist_invariant(slist)
 */
int m0_net_test_slist_init(struct m0_net_test_slist *slist,
			   const char *str,
			   char delim);
/**
   Finalize string list.
   @pre m0_net_test_slist_invariant(slist);
 */
void m0_net_test_slist_fini(struct m0_net_test_slist *slist);
bool m0_net_test_slist_invariant(const struct m0_net_test_slist *slist);

/**
   Is every string in list unique in this list.
   Time complexity - O(N*N), N - number of strings in the list.
   Two strings are equal if strcmp() returns 0.
   @return all strings in list are different.
   @pre m0_net_test_slist_invariant(slist);
 */
bool m0_net_test_slist_unique(const struct m0_net_test_slist *slist);

/**
   Serialize/deserialize string list to/from m0_bufvec.
   m0_net_test_slist_init() shall not be called for slist before
   m0_net_test_slist_serialize().
   m0_net_test_slist_fini() must be called for slist to free memory,
   allocated by m0_net_test_slist_serialize(M0_NET_TEST_DESERIALIZE, slist,...).
   @see m0_net_test_serialize().
 */
m0_bcount_t m0_net_test_slist_serialize(enum m0_net_test_serialize_op op,
					struct m0_net_test_slist *slist,
					struct m0_bufvec *bv,
					m0_bcount_t offset);

/**
   @} end of NetTestSListDFS group
 */

#endif /*  __MERO_NET_TEST_SLIST_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
