#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#ifndef WIN32
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#endif

#include <unistd.h>
#include <pthread.h>

#include <event2/event.h>
#include <event2/thread.h>

#include "evhtp-internal.h"
#include "evthr.h"

typedef struct evthr_cmd        evthr_cmd_t;
typedef struct evthr_pool_slist evthr_pool_slist_t;

struct evthr_cmd {
    uint8_t  stop;
    void   * args;
    evthr_cb cb;
} __attribute__((packed));

TAILQ_HEAD(evthr_pool_slist, evthr);

struct evthr_pool {
#ifdef EVTHR_SHARED_PIPE
    int rdr;
    int wdr;
#endif
    int                nthreads;
    evthr_pool_slist_t threads;
};

struct evthr {
    int             rdr;
    int             wdr;
    char            err;
    ev_t          * event;
    evbase_t      * evbase;
    pthread_mutex_t lock;
    pthread_t     * thr;
    evthr_init_cb   init_cb;
    void          * arg;
    void          * aux;

#ifdef EVTHR_SHARED_PIPE
    int            pool_rdr;
    struct event * shared_pool_ev;
#endif
    TAILQ_ENTRY(evthr) next;
};

#define _evthr_read(thr, cmd, sock) \
    (recv(sock, cmd, sizeof(evthr_cmd_t), 0) == sizeof(evthr_cmd_t)) ? 1 : 0

static void
_evthr_read_cmd(evutil_socket_t sock, short which, void * args) {
    evthr_t   * thread;
    evthr_cmd_t cmd;
    int         stopped;

    if (!(thread = (evthr_t *)args)) {
        return;
    }

    stopped = 0;

    if (evhtp_likely(_evthr_read(thread, &cmd, sock) == 1)) {
        stopped = cmd.stop;

        if (evhtp_likely(cmd.cb != NULL)) {
            (cmd.cb)(thread, cmd.args, thread->arg);
        }
    }

    if (evhtp_unlikely(stopped == 1)) {
        event_base_loopbreak(thread->evbase);
    }

    return;
} /* _evthr_read_cmd */

static void *
_evthr_loop(void * args) {
    evthr_t * thread;

    if (!(thread = (evthr_t *)args)) {
        return NULL;
    }

    if (thread == NULL || thread->thr == NULL) {
        pthread_exit(NULL);
    }

    thread->evbase = event_base_new();
    thread->event  = event_new(thread->evbase, thread->rdr,
                               EV_READ | EV_PERSIST, _evthr_read_cmd, args);

    event_add(thread->event, NULL);

#ifdef EVTHR_SHARED_PIPE
    if (thread->pool_rdr > 0) {
        thread->shared_pool_ev = event_new(thread->evbase, thread->pool_rdr,
                                           EV_READ | EV_PERSIST, _evthr_read_cmd, args);
        event_add(thread->shared_pool_ev, NULL);
    }
#endif

    pthread_mutex_lock(&thread->lock);

    if (thread->init_cb != NULL) {
        thread->init_cb(thread, thread->arg);
    }

    pthread_mutex_unlock(&thread->lock);

    event_base_loop(thread->evbase, 0);

    if (thread->err == 1) {
        fprintf(stderr, "FATAL ERROR!\n");
    }

    pthread_exit(NULL);
} /* _evthr_loop */

evthr_res
evthr_defer(evthr_t * thread, evthr_cb cb, void * arg) {
    evthr_cmd_t cmd = {
        .cb   = cb,
        .args = arg,
        .stop = 0
    };

    if (send(thread->wdr, &cmd, sizeof(cmd), 0) <= 0) {
        return EVTHR_RES_RETRY;
    }

    return EVTHR_RES_OK;
}

evthr_res
evthr_stop(evthr_t * thread) {
    evthr_cmd_t cmd = {
        .cb   = NULL,
        .args = NULL,
        .stop = 1
    };

    if (send(thread->wdr, &cmd, sizeof(evthr_cmd_t), 0) < 0) {
        return EVTHR_RES_RETRY;
    }

    pthread_join(*thread->thr, NULL);
    return EVTHR_RES_OK;
}

evbase_t *
evthr_get_base(evthr_t * thr) {
    return thr->evbase;
}

void
evthr_set_aux(evthr_t * thr, void * aux) {
    thr->aux = aux;
}

void *
evthr_get_aux(evthr_t * thr) {
    return thr->aux;
}

evthr_t *
evthr_new(evthr_init_cb init_cb, void * args) {
    evthr_t * thread;
    int       fds[2];

    if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1) {
        return NULL;
    }

    evutil_make_socket_nonblocking(fds[0]);
    evutil_make_socket_nonblocking(fds[1]);

    if (!(thread = calloc(sizeof(evthr_t), 1))) {
        return NULL;
    }

    thread->thr     = malloc(sizeof(pthread_t));
    thread->init_cb = init_cb;
    thread->arg     = args;
    thread->rdr     = fds[0];
    thread->wdr     = fds[1];

    if (pthread_mutex_init(&thread->lock, NULL)) {
        evthr_free(thread);
        return NULL;
    }

    return thread;
} /* evthr_new */

int
evthr_start(evthr_t * thread) {
    if (thread == NULL || thread->thr == NULL) {
        return -1;
    }

    if (pthread_create(thread->thr, NULL, _evthr_loop, (void *)thread)) {
        return -1;
    }

    return 0;
}

void
evthr_free(evthr_t * thread) {
    if (thread == NULL) {
        return;
    }

    if (thread->rdr > 0) {
        close(thread->rdr);
    }

    if (thread->wdr > 0) {
        close(thread->wdr);
    }

    if (thread->thr) {
        free(thread->thr);
    }

    if (thread->event) {
        event_free(thread->event);
    }

    if (thread->evbase) {
        event_base_free(thread->evbase);
    }

    free(thread);
} /* evthr_free */

void
evthr_pool_free(evthr_pool_t * pool) {
    evthr_t * thread;
    evthr_t * save;

    if (pool == NULL) {
        return;
    }

    TAILQ_FOREACH_SAFE(thread, &pool->threads, next, save) {
        TAILQ_REMOVE(&pool->threads, thread, next);

        evthr_free(thread);
    }

    free(pool);
}

evthr_res
evthr_pool_stop(evthr_pool_t * pool) {
    evthr_t * thr;
    evthr_t * save;

    if (pool == NULL) {
        return EVTHR_RES_FATAL;
    }

    TAILQ_FOREACH_SAFE(thr, &pool->threads, next, save) {
        evthr_stop(thr);
    }

    return EVTHR_RES_OK;
}

evthr_res
evthr_pool_defer(evthr_pool_t * pool, evthr_cb cb, void * arg) {
#ifdef EVTHR_SHARED_PIPE
    evthr_cmd_t cmd = {
        .cb   = cb,
        .args = arg,
        .stop = 0
    };

    if (evhtp_unlikely(send(pool->wdr, &cmd, sizeof(cmd), 0) == -1)) {
        return EVTHR_RES_RETRY;
    }

    return EVTHR_RES_OK;
#else
    evthr_t * thr = NULL;

    if (pool == NULL) {
        return EVTHR_RES_FATAL;
    }

    if (cb == NULL) {
        return EVTHR_RES_NOCB;
    }

    thr = TAILQ_FIRST(&pool->threads);

    TAILQ_REMOVE(&pool->threads, thr, next);
    TAILQ_INSERT_TAIL(&pool->threads, thr, next);


    return evthr_defer(thr, cb, arg);
#endif
} /* evthr_pool_defer */

evthr_pool_t *
evthr_pool_new(int nthreads, evthr_init_cb init_cb, void * shared) {
    evthr_pool_t * pool;
    int            i;

#ifdef EVTHR_SHARED_PIPE
    int fds[2];
#endif

    if (nthreads == 0) {
        return NULL;
    }

    if (!(pool = calloc(sizeof(evthr_pool_t), 1))) {
        return NULL;
    }

    pool->nthreads = nthreads;
    TAILQ_INIT(&pool->threads);

#ifdef EVTHR_SHARED_PIPE
    if (evutil_socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) == -1) {
        return NULL;
    }

    evutil_make_socket_nonblocking(fds[0]);
    evutil_make_socket_nonblocking(fds[1]);

    pool->rdr = fds[0];
    pool->wdr = fds[1];
#endif

    for (i = 0; i < nthreads; i++) {
        evthr_t * thread;

        if (!(thread = evthr_new(init_cb, shared))) {
            evthr_pool_free(pool);
            return NULL;
        }

#ifdef EVTHR_SHARED_PIPE
        thread->pool_rdr = fds[0];
#endif

        TAILQ_INSERT_TAIL(&pool->threads, thread, next);
    }

    return pool;
} /* evthr_pool_new */

int
evthr_pool_start(evthr_pool_t * pool) {
    evthr_t * evthr = NULL;

    if (pool == NULL) {
        return -1;
    }

    TAILQ_FOREACH(evthr, &pool->threads, next) {
        if (evthr_start(evthr) < 0) {
            return -1;
        }

        usleep(5000);
    }

    return 0;
}

