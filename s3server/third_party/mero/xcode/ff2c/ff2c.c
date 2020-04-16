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
 * Original creation date: 02-Jan-2012
 */

/**
   @addtogroup xcode

   <b>ff2c</b>

   ff2c is a simple translator, taking as an input a set of descriptions of
   desired serialized representations and producing a set of C declarations of
   types having given serialized representations. In addition, definitions of
   xcode data (m0_xcode_type and m0_xcode_field) for generated types is
   produced.

   Serialized representation descriptions are given in a language with the
   following grammar:

   @verbatim
ff             ::= statement-list
statement-list ::= statement | statement ';' statement-list
statement      ::= require | declaration
require        ::= 'require' '"' pathname '"'
declaration    ::= type identifier
type           ::= atomic | compound | opaque | identifier
atomic         ::= 'void' | 'u8' | 'u32' | 'u64'
compound       ::= kind '{' field-list '}'
opaque         ::= '*' identifier
kind           ::= 'record' | 'union' | 'sequence'
field-list     ::= field | field ';' field-list
field          ::= declaration tag escape
tag            ::= empty | ':' expression
escape         ::= empty | '[' identifier ']'
   @endverbatim

The language is case-sensitive. Tokens are separated by whitespace and C-style
comments.

The meaning of language constructs is explained in the following example:
@verbinclude "./sample.ff"

  The translator is structured as a sequence of 4 passes:

      - lexical analysis (lex.c): takes a buffer as an input and produces a
        stream of tokens;

      - parser (parser.h): builds a parse tree out of stream of tokens. Every
        node in the tree have the same structure: it contains a token and a list
        of child nodes;

      - semantical pass (sem.h): traverses parse tree and builds a tree of
        semantical nodes, corresponding to types and fields. Semantical nodes
        are "higher level" than syntax nodes are more closely aligned with the
        requirements of the following generation pass;

      - generation (gen.h): take semantic tree and produce corresponding C
        declarations and definitions.
   @{
 */

#include <err.h>
#include <sysexits.h>
#include <unistd.h>                           /* getopt, close, open */
#include <sys/mman.h>                         /* mmap, munmap */
#include <sys/types.h>
#include <sys/stat.h>                         /* stat */
#include <fcntl.h>                            /* O_RDONLY */
#include <libgen.h>                           /* dirname */
#include <string.h>                           /* basename, strdup, strlen */
#include <stdlib.h>                           /* malloc, free */
#include <ctype.h>                            /* toupper */
#include <stdio.h>                            /* fopen, fclose */

#include "xcode/ff2c/lex.h"
#include "xcode/ff2c/parser.h"
#include "xcode/ff2c/sem.h"
#include "xcode/ff2c/gen.h"

int main(int argc, char **argv)
{
	int          fd;
	int          optch;
	int          result;
	const char  *path;
	void        *addr;
	struct stat  buf;
	char        *scratch;
	char        *ch;
	char        *bname;
	char        *gname;
	char        *out_h;
	char        *out_c;
	size_t       len;
	FILE        *c;
	FILE        *h;

	struct ff2c_context   ctx;
	struct ff2c_term     *t;
	struct ff2c_ff        ff;
	struct ff2c_gen_opt   opt;

	while ((optch = getopt(argc, argv, "")) != -1) {
	}

	path = argv[optind];
	fd = open(path, O_RDONLY);
	if (fd == -1)
		err(EX_NOINPUT, "cannot open \"%s\"", path);
	result = fstat(fd, &buf);
	if (result == -1)
		err(EX_UNAVAILABLE, "cannot fstat \"%s\"", path);
	addr = mmap(NULL, buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED)
		err(EX_OSERR, "cannot mmap \"%s\"", path);

	scratch = fmt("%s", path);
	len = strlen(scratch);
	if (len > 3 && strcmp(scratch + len - 3, ".ff") == 0)
		*(scratch + len - 3) = 0;

	out_h = fmt("%s_ff.h", scratch);
	out_c = fmt("%s_ff.c", scratch);

	bname = basename(scratch);
	gname = fmt("__MERO_%s_%s_FF_H__", basename(dirname(scratch)), bname);

	for (ch = gname; *ch != 0; ch++) {
		*ch = toupper(*ch);
		if (strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_", *ch) == NULL)
 			*ch = '_';
	}

	opt.go_basename  = bname;
	opt.go_guardname = gname;

	c = fopen(out_c, "w");
	if (c == NULL)
		err(EX_CANTCREAT, "cannot open \"%s\" for writing", out_c);

	h = fopen(out_h, "w");
	if (h == NULL)
		err(EX_CANTCREAT, "cannot open \"%s\" for writing", out_h);

	memset(&ctx, 0, sizeof ctx);
	memset(&ff, 0, sizeof ff);

	ff2c_context_init(&ctx, addr, buf.st_size);
	result = ff2c_parse(&ctx, &t);
	if (result != 0)
		err(EX_DATAERR, "cannot parse");

	ff2c_sem_init(&ff, t);

	opt.go_out = h;
	ff2c_h_gen(&ff, &opt);
	opt.go_out = c;
	ff2c_c_gen(&ff, &opt);

	ff2c_sem_fini(&ff);
	ff2c_term_fini(t);
	ff2c_context_fini(&ctx);

	fclose(h);
	fclose(c);

	free(out_c);
	free(out_h);
	free(gname);
	free(scratch);
	result = munmap(addr, buf.st_size);
	if (result == -1)
		warn("cannot munmap");
	result = close(fd);
	if (result == -1)
		warn("cannot close");
	return EXIT_SUCCESS;
}

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
