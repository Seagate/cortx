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
 * Original author: Nikita Danilov <nikita_danilov@xyratex.com>
 * Original creation date: 30-Dec-2011
 */

#pragma once

#ifndef __MERO_XCODE_FF2C_LEX_H__
#define __MERO_XCODE_FF2C_LEX_H__

/**
   @addtogroup xcode

   <b>ff2c. Lexical analysis.</b>

   Very simple hand-crafted lexer.

   Input is a contiguous memory buffer (buffer vector can be supported easily).

   ff2c_context data-structure tracks current input position. To make meaningful
   error reporting possible (line, column) coordinates are also tracked.

   Next input token is returned by ff2c_token_get(). Returned token points
   (ff2c_token::ft_val) directly into the input buffer. Specifically, token
   value is not NUL terminated.
 */
/** @{ */

#include <sys/types.h>                  /* size_t */
/* export */
struct ff2c_context;
struct ff2c_token;

/** Token types. For each value, a regular expression, matching the tokens of
    this type is provided. CONT means "[a-zA-Z_0-9]*" */
enum ff2c_token_type {
	/** "[a-zA-Z_]CONT" */
	FTT_IDENTIFIER = 1,
	/** "require" */
	FTT_REQUIRE,
	/** "\"[^\"]*\"" */
	FTT_STRING,
	/** "void" */
	FTT_VOID,
	/** "u8" */
	FTT_U8,
	/** "u32" */
	FTT_U32,
	/** "u64" */
	FTT_U64,
	/** "\*CONT" */
	FTT_OPAQUE,
	/** "record" */
	FTT_RECORD,
	/** "union" */
	FTT_UNION,
	/** "sequence" */
	FTT_SEQUENCE,
	/** "array" */
	FTT_ARRAY,
	/** "{" */
	FTT_OPEN,
	/** "}" */
	FTT_CLOSE,
	/** ";" */
	FTT_SEMICOLON,
	/** ":CONT" */
	FTT_TAG,
	/** "\[CONT\]" */
	FTT_ESCAPE
};

/** Human-readable names of values in ff2c_token_type */
extern const char *ff2c_token_type_name[];

/** Token */
struct ff2c_token {
	enum ff2c_token_type  ft_type;
	/** Pointer into input buffer, where token starts. */
	const char           *ft_val;
	/** Length of token's value in bytes. */
	size_t                ft_len;
};

/**
   Returns the next token.

   @retval +ve: success. "tok" is filled with the new token
   @retval   0: end of the input buffer reached
   @retval -ve: malformed input, error code is returned.
 */
int  ff2c_token_get(struct ff2c_context *ctx, struct ff2c_token *tok);
/**
   Put token back.

   The next call to ff2c_token_get() will return this token.

   When multiple tokens are put back, they are returned in LIFO order. Up to
   FF2C_CTX_STACK_MAX tokens can be returned.
 */
void ff2c_token_put(struct ff2c_context *ctx, struct ff2c_token *tok);

enum { FF2C_CTX_STACK_MAX = 32 };

/** Lexer context. */
struct ff2c_context {
	/** Input buffer. */
	const char        *fc_origin;
	/** Size of input buffer. */
	size_t             fc_size;
	/** Current position in the input buffer ("point"). */
	const char        *fc_pt;
	/** Number of characters remaining in the input buffer. */
	size_t             fc_remain;
	/** Stack of returned tokens. */
	struct ff2c_token  fc_stack[FF2C_CTX_STACK_MAX];
	/** Current stack depth. 0 means the stack is empty. */
	int                fc_depth;
	/** Current line, defined as the number of \n symbols before the
	    point. */
	int                fc_line;
	/** Current column, defined as the number of characters between the
	    point and previous \n or beginning of the input buffer, whichever is
	    less. */
	int                fc_col;
};

void ff2c_context_init(struct ff2c_context *ctx, const char *buf, size_t size);
void ff2c_context_fini(struct ff2c_context *ctx);
/**
   Print human-readable description of the current input position in "buf".
 */
int  ff2c_context_loc(struct ff2c_context *ctx, int nr, char *buf);

/** @} end of xcode group */

/* __MERO_XCODE_FF2C_LEX_H__ */
#endif

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
