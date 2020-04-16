/*
 * COPYRIGHT 2015 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Mikhail Antropov <mikhail.v.antropov@xyratex.com>
 * Original creation date: 19-May-2015
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CONF
#include "lib/trace.h"

#include "lib/assert.h"
#include "lib/misc.h"   /* M0_SET0 */
#include "lib/memory.h" /* m0_alloc_align */
#include "lib/errno.h"
#include "lib/string.h" /* m0_aspritf */
#include "conf/confd.h"
#include "conf/confd_stob.h"
#include "fol/fol.h"
#include "stob/domain.h"
#include "stob/io.h"
#include "stob/stob.h"
#ifndef __KERNEL__
  #include <sys/stat.h> /* fstat */
#endif

/**
   @addtogroup confd_stob
   @{
 */

enum {
	M0_CONFD_STOB_DOMAIN_KEY = 0x33C0FDD077,
};


int m0_confd_stob_init(struct m0_stob **stob,
		       const char      *location,
		       struct m0_fid   *confd_fid)
{
	int                    rc;
	struct m0_stob_domain *dom;
	struct m0_stob_id      stob_id;

	M0_PRE(stob != NULL);
	M0_PRE(location != NULL);
	M0_PRE(confd_fid != NULL);

	M0_ENTRY();

	*stob = NULL;

	/*
	 * TODO FIXME Current workflow assumes that m0_confd_stob_fini() is
	 * called even if m0_confd_stob_init() fails. Therefore, there is no
	 * cleanup code for error flow in this function.
	 * There are two problems:
	 * 1. If m0_stob_find() fails m0_confd_stob_fini() doesn't finalise
	 *    stob domain, because `stob` remains NULL;
	 * 2. There is case when m0_stob_domain_find_by_location() returns
	 *    existing stob domain, but m0_confd_stob_fini() finalises it.
	 *    This leads to possible double finalisation, because there is
	 *    other user that originally initialises the domain.
	 *
	 * Possible fix:
	 * 1. Cleanup on fail and call m0_confd_stob_fini() only after
	 *    successful initialisation;
	 * 2. Move stob domain init/fini to respective subsystem init/fini
	 *    functions
	 */

	dom = m0_stob_domain_find_by_location(location);
	rc = dom != NULL ? 0 :
	     m0_stob_domain_create_or_init(location, NULL,
					   M0_CONFD_STOB_DOMAIN_KEY,
					   NULL, &dom);
	if (rc != 0)
		return M0_ERR(rc);

	m0_stob_id_make(confd_fid->f_container, confd_fid->f_key,
			&dom->sd_id, &stob_id);
	rc = m0_stob_find(&stob_id, stob);
	if (rc == 0 && m0_stob_state_get(*stob) == CSS_UNKNOWN)
		rc = m0_stob_locate(*stob);
	if (rc == 0 && m0_stob_state_get(*stob) == CSS_NOENT)
		rc = m0_stob_create(*stob, NULL, NULL);

	return M0_RC(rc);
}

void m0_confd_stob_fini(struct m0_stob *stob)
{
	struct m0_stob_domain *dom;

	M0_ENTRY();

	if (stob != NULL) {
		dom = m0_stob_dom_get(stob);
		m0_stob_put(stob);
		m0_stob_domain_fini(dom);
	}

	M0_LEAVE();
}

/* TODO get length */
#ifndef __KERNEL__
static m0_bcount_t confd_stob_length(struct m0_stob *stob)
{
	struct stat statbuf;
	int         fd = m0_stob_fd(stob);

	return fstat(fd, &statbuf) == 0 ? statbuf.st_size : 0;
}
#else
static m0_bcount_t confd_stob_length(struct m0_stob *stob)
{
	return 0;
}
#endif

int m0_confd_stob_read(struct m0_stob *stob, char **str)
{
	int              rc;
	m0_bcount_t      length;
	struct m0_bufvec bv = M0_BUFVEC_INIT_BUF((void**)str, &length);

	M0_PRE(stob != NULL);
	M0_PRE(str != NULL);

	length = confd_stob_length(stob);

	*str = m0_alloc_aligned(length + 1, m0_stob_block_shift(stob));
	if (*str == NULL)
		return M0_ERR(-ENOMEM);

	rc = m0_stob_io_bufvec_launch(stob, &bv, SIO_READ, 0);
	if (rc != 0) {
		m0_free(*str);
		return M0_ERR(rc);
	}

	return M0_RC(0);
}

int m0_confd_stob_bufvec_write(struct m0_stob   *stob,
			       struct m0_bufvec *bufvec)
{
	M0_PRE(stob != NULL);
	M0_PRE(bufvec != NULL);

	return m0_stob_io_bufvec_launch(stob, bufvec, SIO_WRITE, 0);
}

int m0_confd_stob_write(struct m0_stob *stob, char *str)
{
	m0_bcount_t      length = strlen(str);
	m0_bcount_t      shift = m0_stob_block_shift(stob);
	struct m0_bufvec bv = M0_BUFVEC_INIT_BUF((void**)&str, &length);

	M0_PRE(stob != NULL);
	M0_PRE(str != NULL);
	M0_PRE(m0_addr_is_aligned(str, shift));

	return m0_stob_io_bufvec_launch(stob, &bv, SIO_WRITE, 0);
}

M0_INTERNAL int m0_conf_stob_location_generate(struct m0_fom  *fom,
					       char          **location)
{
	int         rc;
	char       *conf_path;
	char       *name;
	const char  stob_prefix[] = "linuxstob:";
	const char  stob_suffix[] = "confd";

	M0_PRE(fom != NULL);
	M0_PRE(location != NULL);

	M0_ENTRY();

	rc = m0_confd_service_to_filename(fom->fo_service, &conf_path);
	if (rc != 0) {
		*location = NULL;
		return M0_ERR(rc);
	}

	/*
	 * Generate location to store configuration databases loaded using
	 * load FOP. The idea is that this a same folder as folder of
	 * configuration file provided to confd at startup.
	 */
	name = strrchr(conf_path, '/');
	if (name != NULL)
		*name = '\0';
	m0_asprintf(location, "%s%s/%s",
		    stob_prefix,
		    name == NULL ? "." : conf_path,
		    stob_suffix);
	m0_free(conf_path);

	return *location == NULL ? M0_ERR(-ENOMEM) : M0_RC(rc);
}

/** @} end group confd_stob */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
