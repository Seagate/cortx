/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar        <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 14-June-2016
 */

/*
   Usage examples:
   # Below example tries to upload 10 objects of size 1 GB each to s3 host
   # Uploads are triggered in parallel.
   ./s3perfclient -s3host '192.168.2.128' -upload_size_mb=1024 -uploadurl "/seagatebucket1/OneGBfile0" -s3port 80 -uploadcount 10

 */

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include <stdint.h>
#include <errno.h>

#include <fstream>
#include <string>
#include <vector>

// http://stackoverflow.com/a/8132440
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <evhtp.h>
#include <gflags/gflags.h>

#include "s3_timer.h"

// CLI args

DEFINE_string(s3host, "127.0.0.1",
                 "s3 server host ip");
DEFINE_int32(s3port, 80,
                "s3 server port numnber");
DEFINE_string(uploadfile, "/tmp/OneMBfile",
                "File to upload to s3 server");
DEFINE_string(uploadurl, "/seagatebucket/OneMBfile",
                "URL for upload");
DEFINE_int64(upload_size_mb, 1,
                "upload size in mb");
DEFINE_int64(uploadcount, 1,
                "Number of uploads");

class S3ClientRequest {
public:
  evhtp_connection_t *conn;
  evhtp_request_t    *request;
  struct evbuffer* body_buffer;
  S3Timer timer;
};

/*
static void
request_cb(evhtp_request_t * req, void * arg) {
    printf("hi %zu\n", evbuffer_get_length(req->buffer_in));
}

static int
output_header(evhtp_header_t * header, void * arg) {
    printf("print_response_headers() key = '%s', val = '%s'\n",
                        header->key, header->val);
    return 0;
}
*/

static evhtp_res
print_response_headers(evhtp_request_t * req, evhtp_headers_t * hdrs, void * arg ) {
  S3ClientRequest* client_request = (S3ClientRequest*)req->cbarg;
  client_request->timer.stop();

  printf("put_request_response_time_ms = %zu,", client_request->timer.elapsed_time_in_millisec());

  unsigned int resp_status = evhtp_request_status(req);
  printf("response status = %d\n", resp_status);

  // evhtp_headers_for_each(hdrs, output_header, NULL);
  return EVHTP_RES_OK;
}

void setup_request_headers(evhtp_request_t* request, struct evbuffer* buf) {
  evhtp_headers_add_header(request->headers_out,
                           evhtp_header_new("Accept-Encoding", "identity", 0, 0));
  evhtp_headers_add_header(request->headers_out,
                           evhtp_header_new("Authorization", "AWS4-HMAC-SHA256 Credential=AKIAJTYX36YCKQSAJT7Q/20160523/US/s3/aws4_request,SignedHeaders=content-length;content-type;host;x-amz-content-sha256;x-amz-date;x-amz-meta-s3cmd-attrs;x-amz-storage-class,Signature=329ba550642531735976b2c19b7d49d7e084877a199e78ffb98f08f4ce61a3ba", 0, 0));
  evhtp_headers_add_header(request->headers_out,
                           evhtp_header_new("Connection", "close", 0, 0));

  size_t out_len = evbuffer_get_length(buf);
  char sz_size[100] = {0};
  sprintf(sz_size, "%zu", out_len);

  printf("Sending Content-Length = %s bytes\n", sz_size);
  evhtp_headers_add_header(request->headers_out,
                          evhtp_header_new("Content-Length", sz_size, 1, 1));
  evhtp_headers_add_header(request->headers_out,
                          evhtp_header_new("Host", "s3.seagate.com", 0, 0));
  evhtp_headers_add_header(request->headers_out,
                          evhtp_header_new("Host", "s3.seagate.com", 0, 0));
  evhtp_headers_add_header(request->headers_out,
                          evhtp_header_new("content-type", "application/octet-stream", 0, 0));
  evhtp_headers_add_header(request->headers_out,
                          evhtp_header_new("x-amz-content-sha256", "30e14955ebf1352266dc2ff8067e68104607e750abb9d3b36582b8af909fcb58", 0, 0));
  evhtp_headers_add_header(request->headers_out,
                          evhtp_header_new("x-amz-date", "20160523T055836Z", 0, 0));
  evhtp_headers_add_header(request->headers_out,
                          evhtp_header_new("x-amz-meta-s3cmd-attrs", "uid:1000/gname:seagate/uname:seagate/gid:1000/mode:33188/mtime:1435603529/atime:1463983116/md5:b6d81b360a5672d80c27430f39153e2c/ctime:1435603529", 0, 0));
  evhtp_headers_add_header(request->headers_out,
                          evhtp_header_new("x-amz-storage-class", "STANDARD", 0, 0));
}

void setup_test_by_file(evhtp_request_t* request, struct evbuffer* buf, std::string filename) {
  // read content from file.
  std::ifstream ifs(filename.c_str());
  std::string request_body( (std::istreambuf_iterator<char>(ifs) ),
                       (std::istreambuf_iterator<char>()    ) );
  evbuffer_add(buf, request_body.c_str(), request_body.length());
  setup_request_headers(request, buf);
}

void my_evbuffer_cleanup(const void *data,
    size_t datalen, void *extra) {
  printf("my_evbuffer_cleanup called...\n");
  free((void*)data);
}

void setup_data_to_upload(struct evbuffer* buf, void* data, size_t s_bytes) {
  // printf("setup_data_to_upload called...\n");
  evbuffer_add_reference(buf, data, s_bytes, NULL, NULL);
}

// void setup_test_by_size(evhtp_request_t* request, struct evbuffer* buf, size_t s_bytes) {
//   void *data = malloc(s_bytes);
//   evbuffer_add_reference(buf, data, s_bytes, my_evbuffer_cleanup, NULL);
//   setup_request_headers(request, buf);
// }

void send_request(evhtp_connection_t * conn, evhtp_request_t* request, std::string uri, struct evbuffer* req_body_buffer) {
  // printf("send_request called...\n");
  S3ClientRequest* client_request = (S3ClientRequest*)request->cbarg;
  client_request->timer.start();
  evhtp_make_request(conn, request, htp_method_PUT, uri.c_str());
  evhtp_send_reply_body(request, req_body_buffer);
}

void tead_down_test() {
  return;
}

int
main(int argc, char ** argv) {
    evbase_t           * evbase;

    gflags::ParseCommandLineFlags(&argc, &argv, true);

    evbase  = event_base_new();
    size_t s_bytes = FLAGS_upload_size_mb * 1024 * 1024;
    void *data = malloc(s_bytes);

    std::vector<S3ClientRequest> reqs(FLAGS_uploadcount);
    for (int i = 0; i < FLAGS_uploadcount; ++i) {
      reqs[i].conn    = evhtp_connection_new(evbase, FLAGS_s3host.c_str(), FLAGS_s3port);
      reqs[i].request = evhtp_request_new(NULL, evbase);
      reqs[i].body_buffer = evbuffer_new();
      setup_data_to_upload(reqs[i].body_buffer, data, s_bytes);

      reqs[i].request->cbarg = (void*)&reqs[i];

      evhtp_set_hook(&reqs[i].conn->hooks, evhtp_hook_on_headers, (evhtp_hook)print_response_headers, NULL);
      setup_request_headers(reqs[i].request, reqs[i].body_buffer);
    }

    for (int i = 0; i < FLAGS_uploadcount; ++i) {
      char cnt[100] = {0};
      sprintf(cnt, "%d", i);
      send_request(reqs[i].conn, reqs[i].request, FLAGS_uploadurl + cnt, reqs[i].body_buffer);
    }

    event_base_loop(evbase, 0);

    for (int i = 0; i < FLAGS_uploadcount; ++i) {
      evbuffer_free(reqs[i].body_buffer);
    }
    free(data);
    event_base_free(evbase);

    return 0;
}
