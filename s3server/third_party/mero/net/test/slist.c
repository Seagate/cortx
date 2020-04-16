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
 * Original author: Maxim Medved <max_medved@xyratex.com>
 * Original creation date: 06/28/2012
 */

#include "lib/misc.h"		/* M0_SET0 */
#include "lib/memory.h"		/* M0_ALLOC_ARR */
#include "lib/errno.h"		/* ENOMEM */

#include "mero/magic.h"	/* M0_NET_TEST_SLIST_MAGIC */

#include "net/test/slist.h"

/**
   @defgroup NetTestSListInternals String List
   @ingroup NetTestInternals

   @{
 */

static bool slist_alloc(struct m0_net_test_slist *slist,
		        size_t string_nr,
		        size_t arr_len)
{
	M0_ALLOC_ARR(slist->ntsl_list, string_nr);
	if (slist->ntsl_list != NULL) {
		M0_ALLOC_ARR(slist->ntsl_str, arr_len);
		if (slist->ntsl_str == NULL)
			m0_free(slist->ntsl_list);
	}
	return slist->ntsl_list != NULL && slist->ntsl_str != NULL;
}

static void slist_free(struct m0_net_test_slist *slist)
{
	m0_free(slist->ntsl_list);
	m0_free(slist->ntsl_str);
}

int m0_net_test_slist_init(struct m0_net_test_slist *slist,
			   const char *str,
			   char delim)
{
	const char *str1;
	char	   *str2;
	size_t	    len = 0;
	size_t	    i = 0;
	bool	    allocated;

	M0_PRE(slist != NULL);
	M0_PRE(str   != NULL);
	M0_PRE(delim != '\0');

	M0_SET0(slist);

	len = strlen(str);
	if (len != 0) {
		for (str1 = str; str1 != NULL; str1 = strchr(str1, delim)) {
			++str1;
			++slist->ntsl_nr;
		}
		allocated = slist_alloc(slist, slist->ntsl_nr, len + 1);
		if (!allocated)
			return -ENOMEM;

		strncpy(slist->ntsl_str, str, len + 1);
		str2 = slist->ntsl_str;
		for ( ; str2 != NULL; str2 = strchr(str2, delim)) {
			if (str2 != slist->ntsl_str) {
				*str2 = '\0';
				++str2;
			}
			slist->ntsl_list[i++] = str2;
		}
	}
	M0_POST(m0_net_test_slist_invariant(slist));
	return 0;
}

bool m0_net_test_slist_invariant(const struct m0_net_test_slist *slist)
{
	size_t i;

	if (slist == NULL)
		return false;
	if (slist->ntsl_nr == 0)
		return true;
	if (slist->ntsl_list == NULL)
		return false;
	if (slist->ntsl_str == NULL)
		return false;

	/* check all pointers in ntsl_list */
	if (slist->ntsl_list[0] != slist->ntsl_str)
		return false;
	for (i = 1; i < slist->ntsl_nr; ++i)
		if (slist->ntsl_list[i - 1] >= slist->ntsl_list[i] ||
		    slist->ntsl_list[i] <= slist->ntsl_str)
			return false;
	return true;
}

void m0_net_test_slist_fini(struct m0_net_test_slist *slist)
{
	M0_PRE(m0_net_test_slist_invariant(slist));

	if (slist->ntsl_nr > 0)
		slist_free(slist);
	M0_SET0(slist);
}

bool m0_net_test_slist_unique(const struct m0_net_test_slist *slist)
{
	size_t i;
	size_t j;

	M0_PRE(m0_net_test_slist_invariant(slist));

	for (i = 0; i < slist->ntsl_nr; ++i)
		for (j = i + 1; j < slist->ntsl_nr; ++j)
			if (strcmp(slist->ntsl_list[i],
				   slist->ntsl_list[j]) == 0)
				return false;
	return true;
}

struct slist_params {
	uint64_t sp_magic;	/**< M0_NET_TEST_SLIST_MAGIC */
	size_t   sp_nr;		/**< number if strings in the list */
	size_t   sp_len;	/**< length of string array */
};

TYPE_DESCR(slist_params) = {
	FIELD_DESCR(struct slist_params, sp_magic),
	FIELD_DESCR(struct slist_params, sp_nr),
	FIELD_DESCR(struct slist_params, sp_len),
};

static m0_bcount_t slist_encode(struct m0_net_test_slist *slist,
				struct m0_bufvec *bv,
				m0_bcount_t offset)
{
	struct slist_params sp;
	m0_bcount_t	    len;
	m0_bcount_t	    len_total;

	sp.sp_nr    = slist->ntsl_nr;
	sp.sp_len   = slist->ntsl_nr == 0 ? 0 :
		      slist->ntsl_list[slist->ntsl_nr - 1] -
		      slist->ntsl_list[0] +
		      strlen(slist->ntsl_list[slist->ntsl_nr - 1]) + 1;
	sp.sp_magic = M0_NET_TEST_SLIST_MAGIC;

	len = m0_net_test_serialize(M0_NET_TEST_SERIALIZE, &sp,
				    USE_TYPE_DESCR(slist_params), bv, offset);
	if (len == 0 || slist->ntsl_nr == 0)
		return len;
	len_total = net_test_len_accumulate(0, len);

	len = m0_net_test_serialize_data(M0_NET_TEST_SERIALIZE, slist->ntsl_str,
					 sp.sp_len, true,
					 bv, offset + len_total);
	len_total = net_test_len_accumulate(len_total, len);
	return len_total;
}

static m0_bcount_t slist_decode(struct m0_net_test_slist *slist,
				struct m0_bufvec *bv,
				m0_bcount_t offset)
{
	struct slist_params sp;
	m0_bcount_t	    len;
	m0_bcount_t	    len_total;
	size_t		    i;
	bool		    allocated;


	len = m0_net_test_serialize(M0_NET_TEST_DESERIALIZE, &sp,
				    USE_TYPE_DESCR(slist_params),
				    bv, offset);
	if (len == 0 || sp.sp_magic != M0_NET_TEST_SLIST_MAGIC)
		return 0;
	len_total = net_test_len_accumulate(0, len);

	M0_SET0(slist);
	slist->ntsl_nr = sp.sp_nr;
	/* zero-size string list */
	if (slist->ntsl_nr == 0)
		return len_total;

	allocated = slist_alloc(slist, sp.sp_nr, sp.sp_len + 1);
	if (!allocated)
		return 0;

	len = m0_net_test_serialize_data(M0_NET_TEST_DESERIALIZE,
					 slist->ntsl_str, sp.sp_len, true,
					 bv, offset + len_total);
	if (len == 0)
		goto failed;
	len_total = net_test_len_accumulate(len_total, len);

	slist->ntsl_list[0] = slist->ntsl_str;
	/* additional check if received string doesn't contains '\0' */
	slist->ntsl_str[sp.sp_len] = '\0';
	for (i = 1; i < slist->ntsl_nr; ++i) {
		slist->ntsl_list[i] = slist->ntsl_list[i - 1] +
				      strlen(slist->ntsl_list[i - 1]) + 1;
		if (slist->ntsl_list[i] - slist->ntsl_list[0] >= sp.sp_len)
			goto failed;
	}

	return len_total;
failed:
	slist_free(slist);
	return 0;
}

m0_bcount_t m0_net_test_slist_serialize(enum m0_net_test_serialize_op op,
					struct m0_net_test_slist *slist,
					struct m0_bufvec *bv,
					m0_bcount_t offset)
{
	M0_PRE(slist != NULL);
	M0_PRE(op == M0_NET_TEST_SERIALIZE || op == M0_NET_TEST_DESERIALIZE);

	return op == M0_NET_TEST_SERIALIZE ? slist_encode(slist, bv, offset) :
					     slist_decode(slist, bv, offset);
}

/**
   @} end of NetTestSListInternals group
 */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
