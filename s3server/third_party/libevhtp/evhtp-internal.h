#ifndef __EVHTP_INTERNAL_H__
#define __EVHTP_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif


#if defined __GNUC__ || defined __llvm__
#       define evhtp_likely(x)         __builtin_expect(!!(x), 1)
#       define evhtp_unlikely(x)       __builtin_expect(!!(x), 0)
#else
#       define evhtp_likely(x)         (x)
#       define evhtp_unlikely(x)       (x)
#endif

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar)        \
    for ((var) = TAILQ_FIRST((head));                     \
         (var) && ((tvar) = TAILQ_NEXT((var), field), 1); \
         (var) = (tvar))
#endif

#define evhtp_safe_free(_var, _freefn) do { \
        _freefn((_var));                    \
        (_var) = NULL;                      \
}  while (0)


#define evhtp_assert(x)                                               \
    do {                                                              \
        if (evhtp_unlikely(!(x))) {                                   \
            fprintf(stderr, "Assertion failed: %s (%s:%s:%d)\n", # x, \
                    __func__, __FILE__, __LINE__);                    \
            fflush(stderr);                                           \
            abort();                                                  \
        }                                                             \
    } while (0)

#define evhtp_alloc_assert(x)                             \
    do {                                                  \
        if (evhtp_unlikely(!x)) {                         \
            fprintf(stderr, "Out of memory (%s:%s:%d)\n", \
                    __func__, __FILE__, __LINE__);        \
            fflush(stderr);                               \
            abort();                                      \
        }                                                 \
    } while (0)

#define evhtp_assert_fmt(x, fmt, ...)                                    \
    do {                                                                 \
        if (evhtp_unlikely(!(x))) {                                      \
            fprintf(stderr, "Assertion failed: %s (%s:%s:%d) " fmt "\n", \
                    # x, __func__, __FILE__, __LINE__, __VA_ARGS__);     \
            fflush(stderr);                                              \
            abort();                                                     \
        }                                                                \
    } while (0)

#define evhtp_errno_assert(x)                       \
    do {                                            \
        if (evhtp_unlikely(!(x))) {                 \
            fprintf(stderr, "%s [%d] (%s:%s:%d)\n", \
                    strerror(errno), errno,         \
                    __func__, __FILE__, __LINE__);  \
            fflush(stderr);                         \
            abort();                                \
        }                                           \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif

