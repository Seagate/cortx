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
 * Original author: Subhash Arya <subhash_arya@xyratex.com>
 * Original creation date: 06/25/2011
 */

#include "lib/errno.h"
#include "lib/memory.h"
#include "lib/bitstring.h"
#include "lib/misc.h"
#include "lib/vec.h"
#include "ut/ut.h"
#include "mero/init.h"
#include "fop/fop.h"
#include "xcode/ut/xcode_fops_ff.h"

#include "rpc/rpc_opcodes.h"
#include "rpc/rpc.h"
#include "net/net.h"

/** Random test values. */
enum {
	ARR_COUNT_1     = 10,
	ARR_COUNT_2     = 11,
	TEST_OFFSET     = 0xABCDEF,
	TEST_COUNT      = 0x123456,
	TEST_INDEX      = 0xDEAD,
	TEST_VAL        = 0x1111,
	TEST_CNT_1      = 0x1234,
	TEST_FLAG       = 0x1,
	TEST_BUF_SIZE   = 33,
	NO_OF_BUFFERS   = 85,
	BUFVEC_SEG_SIZE = 256
};

static char *fop_test_buf = "test fop encode/decode";

struct m0_fop_type_ops test_ops = {
};

static struct m0_fop_type m0_fop_test_fopt;

static void fop_verify( struct m0_fop *fop)
{
	void		   *fdata;
	struct m0_fop_test *ftest;
	int		    i;
	int                 j;

	fdata = m0_fop_data(fop);
	ftest = (struct m0_fop_test *)fdata;
	M0_UT_ASSERT(ftest->ft_cnt == TEST_COUNT);
	M0_UT_ASSERT(ftest->ft_offset == TEST_OFFSET);
	M0_UT_ASSERT(ftest->ft_arr.fta_cnt == ARR_COUNT_1);
	M0_UT_ASSERT(ftest->ft_arr.fta_data->da_cnt == ARR_COUNT_2);
	for (i = 0; i < ftest->ft_arr.fta_cnt; ++i) {
		int      index    = TEST_INDEX;
		int      test_val = TEST_VAL;
		uint32_t test_cnt = TEST_CNT_1;

		for (j = 0; j < ftest->ft_arr.fta_data->da_cnt; ++j) {
			int       cnt;
			uint64_t  temp;
			char     *c;

			temp = ftest->ft_arr.fta_data[i].da_pair[j].p_offset;
			M0_UT_ASSERT(temp == test_val);
			test_val++;
			temp = ftest->ft_arr.fta_data[i].da_pair[j].p_cnt;
			M0_UT_ASSERT(temp == test_cnt);
			test_cnt++;
			temp =
			ftest->ft_arr.fta_data[i].da_pair[j].p_key.tk_index;
			M0_UT_ASSERT(temp == index);
			index++;
			temp =
			ftest->ft_arr.fta_data[i].da_pair[j].p_key.tk_val;
			M0_UT_ASSERT(temp == index);
			index++;
			temp =
			ftest->ft_arr.fta_data[i].da_pair[j].p_key.tk_flag;
			M0_UT_ASSERT(temp == TEST_FLAG);
			cnt = ftest->ft_arr.fta_data[i].da_pair[j].p_buf.tb_cnt;
			M0_UT_ASSERT(cnt == TEST_BUF_SIZE);
			c = (char *)
			     ftest->ft_arr.fta_data[i].da_pair[j].p_buf.tb_buf;
			temp = strcmp(c, fop_test_buf);
			M0_UT_ASSERT(temp == 0);
		}
	}
}

/** Clean up allocated fop structures. */
static void fop_free(struct m0_fop *fop)
{
	struct m0_fop_test *ccf1;
	unsigned int	    i;
	unsigned int	    j;

	ccf1 = m0_fop_data(fop);
	for (i = 0; i < ccf1->ft_arr.fta_cnt; ++i) {
		for (j = 0; j < ccf1->ft_arr.fta_data->da_cnt; ++j) {
			uint8_t *test_buf;

			test_buf =
			ccf1->ft_arr.fta_data[i].da_pair[j].p_buf.tb_buf;
			m0_free(test_buf);
		}
	}
        for (i = 0; i < ccf1->ft_arr.fta_cnt; ++i)
		m0_free(ccf1->ft_arr.fta_data[i].da_pair);

	m0_free(ccf1->ft_arr.fta_data);
	m0_free(m0_fop_data(fop));
	m0_free(fop);
}

/*
  Manually calculate the size of the fop based on the .ff file.
  For the current "test_fop" defined in xcode/ut/xcode_fops.ff, we have -

  struct m0_test_buf {
        uint32_t tb_cnt(33);              4
        uint8_t *tb_buf;                + 33 (tb_cnt * uint8_t = 33 * 1)
  };                                    = 37

  struct m0_test_key {
        uint32_t tk_index;                4
        uint64_t tk_val;                + 8
        uint8_t tk_flag;                + 1
  };                                    = 13

  struct m0_pair {
        uint64_t p_offset;                8
        uint32_t p_cnt;                 + 4
        struct m0_test_key p_key;       + 13
        struct m0_test_buf p_buf;       + 37
  };                                    = 62

  struct m0_desc_arr {
        uint32_t da_cnt(11);              4
        struct m0_pair *da_pair;        + 682 (da_cnt * da_pair =  11 * 62)
  };                                    = 686

  struct m0_fop_test_arr {
        uint32_t fta_cnt(10);             4
        struct m0_desc_arr *fta_data;   + 6860 (fta_cnt * fta_data = 10 * 686)
  };                                    = 6864

  struct m0_fop_test {
        uint32_t ft_cnt;                  4
        uint64_t ft_offset;             + 8
        struct m0_fop_test_arr ft_arr;  + 6864
  };                                    = 6876

 */

/** Test function to check generic fop encode decode */
static void test_fop_encdec(void)
{
	int                      rc;
	struct m0_bufvec_cursor  cur;
	void                    *cur_addr;
	int                      i;
	int                      j;
	struct m0_fop           *f1;
	struct m0_fop           *fd1;
	struct m0_net_buffer    *nb;
	struct m0_fop_test      *ccf1;
	struct m0_xcode_ctx      xctx;
	struct m0_xcode_ctx      xctx1;
	size_t                   fop_size;
	size_t                   act_fop_size = 6876;
	struct m0_rpc_machine    machine;

	m0_test_buf_xc->xct_flags     = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_test_key_xc->xct_flags     = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_pair_xc->xct_flags         = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_desc_arr_xc->xct_flags     = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_fop_test_arr_xc->xct_flags = M0_XCODE_TYPE_FLAG_DOM_RPC;
	m0_fop_test_xc->xct_flags     = M0_XCODE_TYPE_FLAG_DOM_RPC;

	M0_FOP_TYPE_INIT(&m0_fop_test_fopt,
			 .name      = "xcode fop test",
			 .opcode    = M0_XCODE_UT_OPCODE,
			 .xt        = m0_fop_test_xc,
			 .rpc_flags = 0,
			 .fop_ops   = &test_ops);

	/* Allocate a fop and populate its fields with test values. */
	f1 = m0_fop_alloc(&m0_fop_test_fopt, NULL, &machine);
	M0_UT_ASSERT(f1 != NULL);

	ccf1 = m0_fop_data(f1);
	M0_ASSERT(ccf1 != NULL);
	ccf1->ft_arr.fta_cnt = ARR_COUNT_1;
	ccf1->ft_cnt    = TEST_COUNT;
	ccf1->ft_offset = TEST_OFFSET;
	M0_ALLOC_ARR(ccf1->ft_arr.fta_data, ccf1->ft_arr.fta_cnt);
	M0_UT_ASSERT(ccf1->ft_arr.fta_data != NULL);

        for (i = 0; i < ccf1->ft_arr.fta_cnt; ++i) {
		ccf1->ft_arr.fta_data[i].da_cnt=ARR_COUNT_2;
		M0_ALLOC_ARR(ccf1->ft_arr.fta_data[i].da_pair,
		     ccf1->ft_arr.fta_data[i].da_cnt);
		M0_UT_ASSERT(ccf1->ft_arr.fta_data[i].da_pair != NULL);
	}

	for (i = 0; i < ccf1->ft_arr.fta_cnt; ++i) {
		uint64_t ival  = TEST_VAL;
		int      index = TEST_INDEX;
		char     flag  = TEST_FLAG;
		uint32_t cnt   = TEST_CNT_1;

		for (j = 0; j < ccf1->ft_arr.fta_data->da_cnt; ++j) {
			uint8_t *test_buf;

			ccf1->ft_arr.fta_data[i].da_pair[j].p_offset = ival++;
			ccf1->ft_arr.fta_data[i].da_pair[j].p_cnt = cnt++;
			ccf1->ft_arr.fta_data[i].da_pair[j].p_key.tk_index =
				index++;
			ccf1->ft_arr.fta_data[i].da_pair[j].p_key.tk_val   =
				index++;
			ccf1->ft_arr.fta_data[i].da_pair[j].p_key.tk_flag  =
				flag;
			M0_ALLOC_ARR(test_buf, TEST_BUF_SIZE);
			M0_UT_ASSERT(test_buf != NULL);
			ccf1->ft_arr.fta_data[i].da_pair[j].p_buf.tb_buf =
				test_buf;
			ccf1->ft_arr.fta_data[i].da_pair[j].p_buf.tb_cnt =
				TEST_BUF_SIZE;
			memcpy(ccf1->ft_arr.fta_data[i].da_pair[j].p_buf.tb_buf,
			       fop_test_buf, strlen(fop_test_buf));
		}
	}

	/* Check the size of the fop using the interfaces. */
	m0_xcode_ctx_init(&xctx, &(struct m0_xcode_obj) {
			  f1->f_type->ft_xt, ccf1 });
	fop_size = m0_xcode_length(&xctx);
	M0_UT_ASSERT(fop_size == act_fop_size);

	/* Allocate a netbuf and a bufvec, check alignments. */
	M0_ALLOC_PTR(nb);
        m0_bufvec_alloc(&nb->nb_buffer, NO_OF_BUFFERS, BUFVEC_SEG_SIZE);
        m0_bufvec_cursor_init(&cur, &nb->nb_buffer);
        cur_addr = m0_bufvec_cursor_addr(&cur);
	M0_UT_ASSERT(M0_IS_8ALIGNED(cur_addr));

	m0_xcode_ctx_init(&xctx, &(struct m0_xcode_obj) {
			  f1->f_type->ft_xt, ccf1 });
	m0_bufvec_cursor_init(&xctx.xcx_buf, &nb->nb_buffer);

	/* Encode the fop into the bufvec. */
	rc = m0_xcode_encode(&xctx);
	M0_UT_ASSERT(rc == 0);
	cur_addr = m0_bufvec_cursor_addr(&cur);
	M0_UT_ASSERT(M0_IS_8ALIGNED(cur_addr));
	/*
	   Allocate a fop for decode. The payload from the bufvec will be
	   decoded into this fop.
	   Since this is a decode fop we do not allocate fop->f_data.fd_data
	   since this allocation is done by xcode.
	   For more, see comments in m0_fop_item_type_default_decode()
	 */
	M0_ALLOC_PTR(fd1);
	M0_UT_ASSERT(fd1 != NULL);
	m0_fop_init(fd1, &m0_fop_test_fopt, NULL, m0_fop_release);
	m0_bufvec_cursor_init(&cur, &nb->nb_buffer);
	cur_addr = m0_bufvec_cursor_addr(&cur);
	M0_UT_ASSERT(M0_IS_8ALIGNED(cur_addr));

	/* Decode the payload from bufvec into the fop. */
	m0_xcode_ctx_init(&xctx1, &(struct m0_xcode_obj) {
			  fd1->f_type->ft_xt, NULL });
	xctx1.xcx_alloc = m0_xcode_alloc;
	xctx1.xcx_buf   = cur;
	rc = m0_xcode_decode(&xctx1);
	M0_UT_ASSERT(rc == 0);
	fd1->f_data.fd_data = m0_xcode_ctx_top(&xctx1);

	cur_addr = m0_bufvec_cursor_addr(&cur);
	M0_UT_ASSERT(M0_IS_8ALIGNED(cur_addr));

	/* Verify the fop data. */
	fop_verify(fd1);

	/* Clean up and free all the allocated memory. */
	m0_bufvec_free(&nb->nb_buffer);
	m0_free(nb);
	fop_free(f1);
	fop_free(fd1);
	m0_fop_type_fini(&m0_fop_test_fopt);
}

static int xcode_bufvec_fop_init(void)
{
	m0_xc_xcode_fops_init();
	return 0;
}

static int xcode_bufvec_fop_fini(void)
{
	m0_xc_xcode_fops_fini();
	return 0;
}

struct m0_ut_suite xcode_bufvec_fop_ut = {
	.ts_name  = "xcode_bufvec_fop-ut",
	.ts_init  = xcode_bufvec_fop_init,
	.ts_fini  = xcode_bufvec_fop_fini,
	.ts_tests = {
		{ "xcode_bufvec_fop", test_fop_encdec },
		{ NULL, NULL }
	}
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
