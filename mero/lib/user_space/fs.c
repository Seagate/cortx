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

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_LIB
#include "lib/trace.h"

#include <unistd.h>		/* close(2) */
#include <dirent.h>		/* opendir(3) */
#include <fcntl.h>		/* open(2) */
#include <errno.h>
#include "lib/memory.h"         /* M0_ALLOC_ARR */

M0_INTERNAL int m0_cleandir(const char *dir)
{
	struct dirent *de;
	DIR           *d;
	int            rc;
	int            fd;

	fd = open(dir, O_RDONLY|O_DIRECTORY);
	if (fd == -1) {
		if (errno == ENOENT)
			return M0_RC(0);
		rc = -errno;
		M0_LOG(M0_NOTICE, "open(%s) failed: rc=%d", dir, rc);
		return M0_ERR(rc);
	}
	d = opendir(dir);
	if (d != NULL) {
		while ((de = readdir(d)) != NULL)
			unlinkat(fd, de->d_name, 0);
		closedir(d);
	}
	close(fd);

	rc = rmdir(dir) == 0 ? 0 : -errno;
	if (rc != 0)
		M0_LOG(M0_ERROR, "rmdir(%s) failed: rc=%d", dir, rc);
	return M0_RC(rc);
}

M0_INTERNAL int m0_file_read(const char *path, char **out)
{
	FILE  *f;
	long   size;
	size_t n;
	int    rc = 0;

	M0_ENTRY("path=`%s'", path);

	f = fopen(path, "rb");
	if (f == NULL)
		return M0_ERR_INFO(-errno, "path=`%s'", path);

	rc = fseek(f, 0, SEEK_END);
	if (rc == 0) {
		size = ftell(f);
		rc = fseek(f, 0, SEEK_SET);
	}
	if (rc != 0) {
		fclose(f);
		return M0_ERR_INFO(-errno, "fseek() failed: path=`%s'", path);
	}
	/* it should be freed by the caller */
	M0_ALLOC_ARR(*out, size + 1);
	if (*out != NULL) {
		n = fread(*out, 1, size + 1, f);
		M0_ASSERT_INFO(n == size, "n=%zu size=%ld", n, size);
		if (ferror(f))
			rc = -errno;
		else if (!feof(f))
			rc = M0_ERR(-EFBIG);
		else
			(*out)[n] = '\0';
	} else {
		rc = M0_ERR(-ENOMEM);
	}

	fclose(f);
	return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
