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

/**
   @addtogroup xcode

   @{
 */

#include <string.h>                   /* strchr, strlen */
#include <stdbool.h>                  /* bool */
#include <stdio.h>                    /* snprintf */
#include <errno.h>
#include <assert.h>
#include <err.h>

#include "xcode/ff2c/lex.h"

static const struct keyword {
	const char           *k_name;
	enum ff2c_token_type  k_type;
} keywords[] = {
	{ "void",     FTT_VOID },
	{ "require",  FTT_REQUIRE },
	{ "u8",       FTT_U8 },
	{ "u32",      FTT_U32 },
	{ "u64",      FTT_U64 },
	{ "record",   FTT_RECORD },
	{ "union",    FTT_UNION },
	{ "sequence", FTT_SEQUENCE },
	{ "array",    FTT_ARRAY },
	{ "{",        FTT_OPEN },
	{ "}",        FTT_CLOSE },
	{ ";",        FTT_SEMICOLON },
	{ NULL,       0 }
};

static void ctx_move(struct ff2c_context *ctx, size_t nob)
{
	assert(nob <= ctx->fc_remain);

	for (; nob > 0; --nob, --ctx->fc_remain, ++ctx->fc_pt) {
		if (*ctx->fc_pt == '\n') {
			ctx->fc_line++;
			ctx->fc_col = 0;
		} else
			ctx->fc_col++;
	}
}

static void ctx_step(struct ff2c_context *ctx)
{
	ctx_move(ctx, 1);
}

#define SAFE(ctx, cond) ((ctx)->fc_remain > 0 && (cond))

static bool at(struct ff2c_context *ctx, char c)
{
	return SAFE(ctx, *ctx->fc_pt == c);
}

static bool at_string(struct ff2c_context *ctx, const char *s, size_t n)
{
	return ctx->fc_remain >= n && memcmp(ctx->fc_pt, s, n) == 0;
}

static bool is_start(char c)
{
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool is_middle(char c)
{
	return ('0' <= c && c <= '9') || is_start(c);
}

static void tok_end(struct ff2c_context *ctx, struct ff2c_token *tok)
{
	tok->ft_len = ctx->fc_pt - tok->ft_val;
}

static void skip_space(struct ff2c_context *ctx)
{
	static const char space[] = " \t\v\n\r";
	const char       *start;

	do {
		start = ctx->fc_pt;
		while (SAFE(ctx, strchr(space, *ctx->fc_pt)))
			ctx_step(ctx);
		if (at_string(ctx, "/*", 2)) {
			ctx_step(ctx);
			do {
				ctx_step(ctx);
				if (ctx->fc_remain < 2)
					errx(2, "Unterminated comment");
			} while (!at_string(ctx, "*/", 2));
			ctx_move(ctx, 2);
		}
	} while (start != ctx->fc_pt);
}

static bool get_literal(struct ff2c_context *ctx, struct ff2c_token *tok)
{
	ctx_step(ctx);
	skip_space(ctx);
	tok->ft_val = ctx->fc_pt;
	while (SAFE(ctx, is_middle(*ctx->fc_pt)))
		ctx_step(ctx);
	tok_end(ctx, tok);
	return tok->ft_len > 0;
}

int ff2c_token_get(struct ff2c_context *ctx, struct ff2c_token *tok)
{
	const struct keyword *kw;

	/*
	 * Majority of token types are constant keywords, detected by iterating
	 * over keywords[] array.
	 *
	 * Others are (not entirely coincidentally) identifiable by their first
	 * character, which makes analyzing very simple.
	 */

	if (ctx->fc_depth > 0) {
		*tok = ctx->fc_stack[--ctx->fc_depth];
		return +1;
	}

	skip_space(ctx);
	if (ctx->fc_remain == 0)
		return 0;
	for (kw = &keywords[0]; kw->k_name != NULL; ++kw) {
		size_t      len = strlen(kw->k_name);
		const char *pt = ctx->fc_pt;

		if (ctx->fc_remain >= len && !memcmp(pt, kw->k_name, len)) {
			tok->ft_type = kw->k_type;
			tok->ft_val  = pt;
			tok->ft_len  = len;
			ctx_move(ctx, len);
			return +1;
		}
	}

	if (at(ctx, ':')) {
		if (get_literal(ctx, tok)) {
			tok->ft_type = FTT_TAG;
			return +1;
		} else {
			warnx("\":\" must be followed by a tag");
			return -EINVAL;
		}
	} else if (at(ctx, '[')) {
		if (get_literal(ctx, tok)) {
			skip_space(ctx);
			if (at(ctx, ']')) {
				ctx_step(ctx);
				tok->ft_type = FTT_ESCAPE;
				return +1;
			}
		}
		warnx("\"[\" must be followed by an escape and \"]\"");
		return -EINVAL;
	} else if (at(ctx, '*')) {
		if (get_literal(ctx, tok)) {
			tok->ft_type = FTT_OPAQUE;
			return +1;
		} else {
			warnx("\"*\" must be followed by a type name");
			return -EINVAL;
		}
	} else if (at(ctx, '"')) {
		tok->ft_val  = ctx->fc_pt;
		tok->ft_type = FTT_STRING;
		do
			ctx_step(ctx);
		while (SAFE(ctx, *ctx->fc_pt != '"'));
		if (at(ctx, '"')) {
			ctx_step(ctx);
			tok_end(ctx, tok);
			return +1;
		}
	} else if (SAFE(ctx, is_start(*ctx->fc_pt))) {
		tok->ft_val  = ctx->fc_pt;
		tok->ft_type = FTT_IDENTIFIER;
		do
			ctx_step(ctx);
		while (SAFE(ctx, is_middle(*ctx->fc_pt)));
		tok_end(ctx, tok);
		return +1;
	}
	return -ENOENT;
}

void ff2c_token_put(struct ff2c_context *ctx, struct ff2c_token *tok)
{
	assert(ctx->fc_depth < FF2C_CTX_STACK_MAX);

	ctx->fc_stack[ctx->fc_depth++] = *tok;
}

void ff2c_context_init(struct ff2c_context *ctx, const char *buf, size_t size)
{
	ctx->fc_remain = ctx->fc_size = size;
	ctx->fc_origin = ctx->fc_pt   = buf;
}

void ff2c_context_fini(struct ff2c_context *ctx)
{
}

int ff2c_context_loc(struct ff2c_context *ctx, int nr, char *buf)
{
	return snprintf(buf, nr, "[%i:%i] %zu/%zu: '%c'",
			ctx->fc_line, ctx->fc_col, ctx->fc_pt - ctx->fc_origin,
			ctx->fc_size, *ctx->fc_pt);
}

const char *ff2c_token_type_name[] = {
	[FTT_IDENTIFIER] = "IDENTIFIER",
	[FTT_REQUIRE]    = "REQUIRE",
	[FTT_STRING]     = "STRING",
	[FTT_VOID]       = "VOID",
	[FTT_U8]         = "U8",
	[FTT_U32]        = "U32",
	[FTT_U64]        = "U64",
	[FTT_OPAQUE]     = "OPAQUE",
	[FTT_RECORD]     = "RECORD",
	[FTT_UNION]      = "UNION",
	[FTT_SEQUENCE]   = "SEQUENCE",
	[FTT_ARRAY]      = "ARRAY",
	[FTT_OPEN]       = "OPEN",
	[FTT_CLOSE]      = "CLOSE",
	[FTT_SEMICOLON]  = "SEMICOLON",
	[FTT_TAG]        = "TAG",
	[FTT_ESCAPE]     = "ESCAPE"
};

/** @} end of xcode group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
