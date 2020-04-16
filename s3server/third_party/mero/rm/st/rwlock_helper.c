#include <unistd.h>

#include "lib/trace.h"
#include "lib/user_space/getopts.h"
#include "mero/init.h"
#include "module/instance.h"
#include "rm/rm.h"
#include "rm/rm_rwlock.h"
#include "rm/rm_service.h"
#include "rpc/rpclib.h"        /* m0_rpc_client_connect */
#include "rm/st/wlock_helper.h"

int main(int argc, char **argv)
{
	static struct m0           instance;
	struct m0_net_domain       domain;
	struct m0_net_buffer_pool  buffer_pool;
	struct m0_reqh             reqh;
	struct m0_reqh_service    *rm_service;
	struct m0_fid              process_fid = M0_FID_TINIT('r', 1, 5);
	struct m0_fid              rms_fid = M0_FID_TINIT('s', 3, 10);
	struct m0_rpc_machine      rpc_mach;
	const char                *rm_ep;
	const char                *c_ep;
	int                        delay;
	int                        rc;

	rc = m0_init(&instance);
	if (rc != 0)
		return M0_ERR(rc);
        rc = M0_GETOPTS("m0rwlock", argc, argv,
                            M0_STRINGARG('s',
				         "server endpoint (RM)",
                                       LAMBDA(void, (const char *string) {
                                               rm_ep = string; })),
                            M0_STRINGARG('c',
				         "client endpoint",
                                       LAMBDA(void, (const char *string) {
                                               c_ep = string; })),
                            M0_FORMATARG('d',
				         "delay between write lock get and put",
					 "%i", &delay));
        if (rc != 0)
                return M0_ERR(rc);
	printf("s %s, c %s, d %d\n", rm_ep, c_ep, delay);
	rc = m0_net_domain_init(&domain, &m0_net_lnet_xprt);
	if (rc != 0)
		goto m0_fini;
	rc = m0_rpc_net_buffer_pool_setup(&domain, &buffer_pool, 2, 1);
	if (rc != 0)
		goto domain_fini;
	rc = M0_REQH_INIT(&reqh,
			  .rhia_dtm     = (void *)1,
			  .rhia_mdstore = (void *)1,
			  .rhia_fid     = &process_fid);
	if (rc != 0)
		goto buffer_cleanup;
	m0_reqh_start(&reqh);
	rc = m0_reqh_service_setup(&rm_service, &m0_rms_type, &reqh, NULL,
				   &rms_fid);
	if (rc != 0)
		goto reqh_fini;
	rc = m0_rpc_machine_init(&rpc_mach, &domain, c_ep, &reqh, &buffer_pool,
				 ~(uint32_t)0, 1 << 17, 2);
	if (rc != 0)
		goto services_terminate;
	rc = rm_write_lock_get(&rpc_mach, rm_ep);
	if (rc != 0)
		goto mach_fini;
	else
		m0_console_printf("Got write lock\n");
	sleep(delay);
	rm_write_lock_put();
	m0_console_printf("Released write lock\n");
mach_fini:
        m0_rpc_machine_fini(&rpc_mach);
services_terminate:
        m0_reqh_services_terminate(&reqh);
reqh_fini:
        m0_reqh_fini(&reqh);
buffer_cleanup:
        m0_rpc_net_buffer_pool_cleanup(&buffer_pool);
domain_fini:
        m0_net_domain_fini(&domain);
m0_fini:
        m0_fini();
	return M0_RC(rc);
}
