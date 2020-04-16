#ifndef __HTPARSE_H__
#define __HTPARSE_H__

#include "evhtp-config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct htparser;

enum htp_type {
    htp_type_request = 0,
    htp_type_response
};

enum htp_scheme {
    htp_scheme_none = 0,
    htp_scheme_ftp,
    htp_scheme_http,
    htp_scheme_https,
    htp_scheme_nfs,
    htp_scheme_unknown
};

enum htp_method {
    htp_method_GET = 0,
    htp_method_HEAD,
    htp_method_POST,
    htp_method_PUT,
    htp_method_DELETE,
    htp_method_MKCOL,
    htp_method_COPY,
    htp_method_MOVE,
    htp_method_OPTIONS,
    htp_method_PROPFIND,
    htp_method_PROPPATCH,
    htp_method_LOCK,
    htp_method_UNLOCK,
    htp_method_TRACE,
    htp_method_CONNECT, /* RFC 2616 */
    htp_method_PATCH,   /* RFC 5789 */
    htp_method_UNKNOWN,
};

enum htpparse_error {
    htparse_error_none = 0,
    htparse_error_too_big,
    htparse_error_inval_method,
    htparse_error_inval_reqline,
    htparse_error_inval_schema,
    htparse_error_inval_proto,
    htparse_error_inval_ver,
    htparse_error_inval_hdr,
    htparse_error_inval_chunk_sz,
    htparse_error_inval_chunk,
    htparse_error_inval_state,
    htparse_error_user,
    htparse_error_status,
    htparse_error_generic
};

typedef struct htparser      htparser;
typedef struct htparse_hooks htparse_hooks;

typedef enum htp_scheme      htp_scheme;
typedef enum htp_method      htp_method;
typedef enum htp_type        htp_type;
typedef enum htpparse_error  htpparse_error;

typedef int (* htparse_hook)(htparser *);
typedef int (* htparse_data_hook)(htparser *, const char *, size_t);


struct htparse_hooks {
    htparse_hook      on_msg_begin;
    htparse_data_hook method;
    htparse_data_hook scheme;              /* called if scheme is found */
    htparse_data_hook host;                /* called if a host was in the request scheme */
    htparse_data_hook port;                /* called if a port was in the request scheme */
    htparse_data_hook path;                /* only the path of the uri */
    htparse_data_hook args;                /* only the arguments of the uri */
    htparse_data_hook uri;                 /* the entire uri including path/args */
    htparse_hook      on_hdrs_begin;
    htparse_data_hook hdr_key;
    htparse_data_hook hdr_val;
    htparse_data_hook hostname;
    htparse_hook      on_hdrs_complete;
    htparse_hook      on_new_chunk;        /* called after parsed chunk octet */
    htparse_hook      on_chunk_complete;   /* called after single parsed chunk */
    htparse_hook      on_chunks_complete;  /* called after all parsed chunks processed */
    htparse_data_hook body;
    htparse_hook      on_msg_complete;
};


EVHTP_EXPORT size_t         htparser_run(htparser *, htparse_hooks *, const char *, size_t);
EVHTP_EXPORT int            htparser_should_keep_alive(htparser * p);
EVHTP_EXPORT htp_scheme     htparser_get_scheme(htparser *);
EVHTP_EXPORT htp_method     htparser_get_method(htparser *);
EVHTP_EXPORT const char   * htparser_get_methodstr(htparser *);
EVHTP_EXPORT const char   * htparser_get_methodstr_m(htp_method);
EVHTP_EXPORT void           htparser_set_major(htparser *, unsigned char);
EVHTP_EXPORT void           htparser_set_minor(htparser *, unsigned char);
EVHTP_EXPORT unsigned char  htparser_get_major(htparser *);
EVHTP_EXPORT unsigned char  htparser_get_minor(htparser *);
EVHTP_EXPORT unsigned char  htparser_get_multipart(htparser *);
EVHTP_EXPORT unsigned int   htparser_get_status(htparser *);
EVHTP_EXPORT uint64_t       htparser_get_content_length(htparser *);
EVHTP_EXPORT uint64_t       htparser_get_content_pending(htparser *);
EVHTP_EXPORT uint64_t       htparser_get_total_bytes_read(htparser *);
EVHTP_EXPORT htpparse_error htparser_get_error(htparser *);
EVHTP_EXPORT const char   * htparser_get_strerror(htparser *);
EVHTP_EXPORT void         * htparser_get_userdata(htparser *);
EVHTP_EXPORT void           htparser_set_userdata(htparser *, void *);
EVHTP_EXPORT void           htparser_init(htparser *, htp_type);
EVHTP_EXPORT htparser     * htparser_new(void);

#ifdef __cplusplus
}
#endif

#endif

