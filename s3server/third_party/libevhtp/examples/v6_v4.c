#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "evhtp-internal.h"
#include "evhtp.h"

static void
_req_cb(evhtp_request_t * req, void * arg) {
    const char * ver = (const char *)arg;

    evbuffer_add(req->buffer_out, ver, strlen(ver));
    evhtp_send_reply(req, EVHTP_RES_OK);
}

int
main(int argc, char ** argv) {
    struct event_base * evbase;
    evhtp_t           * htp_v6;
    evhtp_t           * htp_v4;
    int                 rc;

    evbase = event_base_new();
    evhtp_alloc_assert(evbase);

    htp_v6 = evhtp_new(evbase, NULL);
    evhtp_alloc_assert(htp_v6);

    htp_v4 = evhtp_new(evbase, NULL);
    evhtp_alloc_assert(htp_v4);

    evhtp_set_gencb(htp_v6, _req_cb, (void *)"ipv6");
    evhtp_set_gencb(htp_v4, _req_cb, (void *)"ipv4");

    rc = evhtp_bind_socket(htp_v6, "ipv6:::/128", 9090, 1024);
    evhtp_errno_assert(rc != -1);

    rc = evhtp_bind_socket(htp_v4, "ipv4:0.0.0.0", 9090, 1024);
    evhtp_errno_assert(rc != -1);

    event_base_loop(evbase, 0);

    return 0;
} /* main */