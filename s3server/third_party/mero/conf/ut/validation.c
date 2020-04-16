/*
 * COPYRIGHT 2016 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Valery V. Vorotyntsev <valery.vorotyntsev@seagate.com>
 * Original creation date: 7-Jan-2016
 */

#include <glob.h>
#include "conf/validation.h"
#include "conf/ut/common.h"  /* m0_conf_ut_cache_from_file */
#include "lib/fs.h"          /* m0_file_read */
#include "lib/string.h"      /* m0_streq */
#include "lib/memory.h"      /* m0_free */
#include "ut/ut.h"

static char g_buf[192];

static void cache_load(struct m0_conf_cache *cache, const char *path,
		       char **sharp_out);

static void test_validation(void)
{
	glob_t g = {0};
	char **pathv;
	char  *err;
	char  *expected;
	int    rc;

	rc = glob(M0_SRC_PATH("conf/ut/t_*.xc"), 0, NULL, &g);
	M0_UT_ASSERT(rc == 0);
	for (pathv = g.gl_pathv; *pathv != NULL; ++pathv) {
#define _UT_ASSERT(cond)                         \
		M0_ASSERT_INFO(cond,             \
			       "path=%s\n"       \
			       "err={%s}\n"      \
			       "expected={%s}\n" \
			       "g_buf={%s}",     \
			       *pathv, (err ?: ""), (expected ?: ""), g_buf)

		cache_load(&m0_conf_ut_cache, *pathv, &expected);
		err = m0_conf_validation_error(&m0_conf_ut_cache,
					       g_buf, sizeof g_buf);
		_UT_ASSERT((err == NULL) == (expected == NULL));
		if (expected != NULL) {
			/* Strip "[<rule set>.<rule>] " prefix. */
			err = strstr(err, "] ");
			M0_UT_ASSERT(err != NULL);
			err += 2;
			_UT_ASSERT(m0_streq(err, expected));
		}
		free(expected);
#undef _UT_ASSERT
	}
	globfree(&g);
}

/**
 * If the first line of the input matches /^#+=/ regexp, sharp_comment()
 * returns the remainder of this line without the matched prefix.
 * The returned string is stripped of leading and trailing blanks.
 *
 * If there is no match, sharp_comment() returns NULL.
 *
 * @note  The returned pointer should be free()d. (But not m0_free()d.)
 */
static char *sharp_comment(const char *input)
{
	const char *start = input;
	const char *end;

	if (*start != '#')
		return NULL; /* not a comment */

	while (*start == '#')
		++start;
	if (*start != '=')
		return NULL; /* an ordinary, not a "sharp", comment */
	for (++start; isblank(*start); ++start) /* NB space_skip() */
		; /* lstrip */

	for (end = start; !M0_IN(*end, (0, '\n')); ++end)
		;
	while (isblank(*(end-1)) && end-1 > start)
		--end; /* rstrip */
	return strndup(start, end - start);
}

static void test_sharp_comment(void)
{
	static const struct {
		const char *input;
		const char *result;
	} samples[] = {
		{ "", NULL },
		{ "# ordinary comment", NULL },
		{ "not a comment", NULL },
		{ " #= not at start of the line", NULL },
		{ "#= special comment\nanything", "special comment" },
		{ "###=  \t text\t ", "text" },
		{ "#=no leading space", "no leading space" },
		{ "#= ", "" },
		{ "#==", "=" }
	};
	char  *s;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(samples); ++i) {
		if (samples[i].result == NULL) {
		    M0_UT_ASSERT(sharp_comment(samples[i].input) == NULL);
		} else {
			s = sharp_comment(samples[i].input);
			M0_UT_ASSERT(m0_streq(s, samples[i].result));
			free(s);
		}
	}
}

/**
 * @note Don't forget to free(*sharp_out).
 */
static void
cache_load(struct m0_conf_cache *cache, const char *path, char **sharp_out)
{
	char *confstr = NULL;
	int   rc;

	M0_PRE(path != NULL && *path != '\0');

	m0_conf_ut_cache_from_file(cache, path);
	rc = m0_file_read(path, &confstr);
	M0_UT_ASSERT(rc == 0);
	if (sharp_out != NULL)
		*sharp_out = sharp_comment(confstr);
	m0_free(confstr);
}

struct m0_ut_suite conf_validation_ut = {
	.ts_name  = "conf-validation-ut",
	.ts_init  = m0_conf_ut_cache_init,
	.ts_fini  = m0_conf_ut_cache_fini,
	.ts_tests = {
		{ "sharp-comment", test_sharp_comment },
		{ "validation",    test_validation },
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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
