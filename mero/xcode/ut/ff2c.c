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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 30-Dec-2011
 */

#include <stdio.h>                          /* printf, stdout */

#include "lib/misc.h"                       /* M0_SET0 */
#include "ut/ut.h"

#include "xcode/ff2c/lex.h"
#include "xcode/ff2c/parser.h"
#include "xcode/ff2c/sem.h"
#include "xcode/ff2c/gen.h"

static const char sample[] =
"/* comment. */					\n"
"require \"lib/vec.ff\";				\n"
"						\n"
"record {					\n"
"	u64 f_container;			\n"
"	u64 f_offset				\n"
"} fid;						\n"
"						\n"
"union {						\n"
"	u8  o_flag;				\n"
"	fid o_fid   :1;				\n"
"	u32 o_short :3				\n"
"} optfid;					\n"
"						\n"
"sequence {					\n"
"	u64    ofa_nr;				\n"
"	optfid ofa_data				\n"
"} optfidarray;					\n"
"						\n"
"sequence {					\n"
"	void   fa_none :NR;			\n"
"	optfid fa_data				\n"
"} fixarray;					\n"
"						\n"
"/*void nothing;					\n"
"u8   byte;					\n"
"u32  quad;					\n"
"u64  hyper;					\n"
"optfid t_alias;*/				\n"
"						\n"
"record {				\n"
"	fid      p_fid;				\n"
"	m0_vec   p_vec;				\n"
"	*m0_cred p_cred [m0_package_cred_get];	\n"
"	sequence {				\n"
"		u32 s_nr;			\n"
"		u8  s_data			\n"
"	} p_name				\n"
"} package					\n"
"						\n";

__attribute__((unused)) static void token_print(const struct ff2c_token *tok)
{
	if (tok->ft_type != 0)
		printf("[%s: %*.*s]", ff2c_token_type_name[tok->ft_type],
		       (int)tok->ft_len, (int)tok->ft_len, tok->ft_val);
	else
		printf("[no token]");
}

static void ff2c_lex_test(void)
{
	struct ff2c_context ctx;
	/* struct ff2c_token   tok; */

	M0_SET0(&ctx);
	ff2c_context_init(&ctx, sample, strlen(sample));

	/* while (ff2c_token_get(&ctx, &tok) > 0)
		token_print(&tok); */
	ff2c_context_fini(&ctx);
}

__attribute__((unused)) static void parser_print(struct ff2c_term *t, int depth)
{
	const char ruler[] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
	printf("%*.*s %11.11s: ", depth, depth, ruler,
	       ff2c_term_type_name[t->fn_type]);
	token_print(&t->fn_tok);
	printf("\n");
	for (t = t->fn_head; t != NULL; t = t->fn_next)
		parser_print(t, depth + 1);
}

static void ff2c_parser_test(void)
{
	struct ff2c_context ctx;
	struct ff2c_term   *t;
	int result;

	M0_SET0(&ctx);
	ff2c_context_init(&ctx, sample, strlen(sample));
	result = ff2c_parse(&ctx, &t);
	M0_UT_ASSERT(result == 0);
	/*
	printf("\n");
	parser_print(t, 0);
	*/
	ff2c_term_fini(t);
	ff2c_context_fini(&ctx);
}

static void ff2c_sem_test(void)
{
	int                  result;
	struct ff2c_context  ctx;
	struct ff2c_term    *term;
	struct ff2c_ff       ff;
/*	struct ff2c_require *r;
	struct ff2c_type    *t;
	struct ff2c_field   *f; */

	M0_SET0(&ctx);
	M0_SET0(&ff);

	ff2c_context_init(&ctx, sample, strlen(sample));
	result = ff2c_parse(&ctx, &term);
	M0_UT_ASSERT(result == 0);

	ff2c_sem_init(&ff, term);
/*
	for (r = ff.ff_require.l_head; r != NULL; r = r->r_next)
		printf("require %s\n", r->r_path);

	for (t = ff.ff_type.l_head; t != NULL; t = t->t_next) {
		printf("type %p name: %s xc: %s c: %s\n"
		       "\t%s %s %s %s %s %s %i\n", t,
		       t->t_name, t->t_xc_name, t->t_c_name,
		       t->t_compound ? "comp" : "",
		       t->t_atomic   ? "atom" : "",
		       t->t_opaque   ? "opaq" : "",
		       t->t_sequence ? "seq" : "",
		       t->t_union    ? "unio" : "",
		       t->t_record   ? "rec" : "",
		       t->t_nr);
		for (f = t->t_field.l_head; f != NULL; f = f->f_next) {
			printf("\tfield: %p name: %s c: %s tag: %s escape: %s\n"
			       "\t\tdecl: %s\n",
			       f->f_type, f->f_name, f->f_c_name,
			       f->f_tag ?: "", f->f_escape ?: "",
			       f->f_decl ?: "");
		}
		printf("\n");
	}
*/

	ff2c_sem_fini(&ff);

	ff2c_term_fini(term);
	ff2c_context_fini(&ctx);
}

static void ff2c_gen_test(void)
{
	struct ff2c_context ctx;
	struct ff2c_term   *t;
	struct ff2c_ff      ff;
	const struct ff2c_gen_opt opt ={
		.go_basename  = "basename",
		.go_guardname = "__GUARD__",
		.go_out       = fopen("/dev/null", "w") /* stdout */
	};

	int result;

	M0_SET0(&ctx);
	M0_SET0(&ff);

	ff2c_context_init(&ctx, sample, strlen(sample));
	result = ff2c_parse(&ctx, &t);
	M0_UT_ASSERT(result == 0);

	ff2c_sem_init(&ff, t);

	ff2c_h_gen(&ff, &opt);
	ff2c_c_gen(&ff, &opt);

	ff2c_sem_fini(&ff);
	ff2c_term_fini(t);
	ff2c_context_fini(&ctx);
}

struct m0_ut_suite xcode_ff2c_ut = {
        .ts_name = "ff2c-ut",
        .ts_init = NULL,
        .ts_fini = NULL,
        .ts_tests = {
                { "xcode-lex",    ff2c_lex_test },
                { "xcode-parser", ff2c_parser_test },
                { "xcode-sem",    ff2c_sem_test },
                { "xcode-gen",    ff2c_gen_test },
                { NULL, NULL }
        }
};

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */


