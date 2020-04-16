/*
 * COPYRIGHT 2015 SEAGATE LLC
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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include <algorithm>
#include "s3_get_object_action.h"
#include "s3_clovis_layout.h"
#include "s3_error_codes.h"
#include "s3_log.h"
#include "s3_option.h"
#include "s3_common_utilities.h"
#include "s3_stats.h"
#include "s3_perf_metrics.h"

S3GetObjectAction::S3GetObjectAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3BucketMetadataFactory> bucket_meta_factory,
    std::shared_ptr<S3ObjectMetadataFactory> object_meta_factory,
    std::shared_ptr<S3ClovisReaderFactory> clovis_s3_factory)
    : S3ObjectAction(std::move(req), std::move(bucket_meta_factory),
                     std::move(object_meta_factory)),
      total_blocks_in_object(0),
      blocks_already_read(0),
      data_sent_to_client(0),
      content_length(0),
      first_byte_offset_to_read(0),
      last_byte_offset_to_read(0),
      total_blocks_to_read(0),
      read_object_reply_started(false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");

  s3_log(S3_LOG_INFO, request_id, "S3 API: Get Object. Bucket[%s] Object[%s]\n",
         request->get_bucket_name().c_str(),
         request->get_object_name().c_str());

  if (clovis_s3_factory) {
    clovis_reader_factory = std::move(clovis_s3_factory);
  } else {
    clovis_reader_factory = std::make_shared<S3ClovisReaderFactory>();
  }

  setup_steps();
}

void S3GetObjectAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");
  ACTION_TASK_ADD(S3GetObjectAction::validate_object_info, this);
  ACTION_TASK_ADD(S3GetObjectAction::check_full_or_range_object_read, this);
  ACTION_TASK_ADD(S3GetObjectAction::read_object, this);
  ACTION_TASK_ADD(S3GetObjectAction::send_response_to_s3_client, this);
  // ...
}

void S3GetObjectAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::missing) {
    set_s3_error("NoSuchBucket");
  } else if (bucket_metadata->get_state() ==
             S3BucketMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Bucket metadata load operation failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetObjectAction::fetch_object_info_failed() {
  if (bucket_metadata->get_state() == S3BucketMetadataState::present) {
    s3_log(S3_LOG_DEBUG, request_id, "Found bucket metadata\n");
    object_list_oid = bucket_metadata->get_object_list_index_oid();
    if (object_list_oid.u_hi == 0ULL && object_list_oid.u_lo == 0ULL) {
      s3_log(S3_LOG_ERROR, request_id, "Object not found\n");
      set_s3_error("NoSuchKey");
    } else {
      if (object_metadata->get_state() == S3ObjectMetadataState::missing) {
        s3_log(S3_LOG_DEBUG, request_id, "Object not found\n");
        set_s3_error("NoSuchKey");
      } else if (object_metadata->get_state() ==
                 S3ObjectMetadataState::failed_to_launch) {
        s3_log(S3_LOG_ERROR, request_id,
               "Object metadata load operation failed due to pre launch "
               "failure\n");
        set_s3_error("ServiceUnavailable");
      } else {
        s3_log(S3_LOG_DEBUG, request_id, "Object metadata fetch failed\n");
        set_s3_error("InternalError");
      }
    }
  }
  send_response_to_s3_client();
}

void S3GetObjectAction::validate_object_info() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  content_length = object_metadata->get_content_length();
  // as per RFC last_byte_offset_to_read is taken to be equal to one less than
  // the content length in bytes.
  last_byte_offset_to_read =
      (content_length == 0) ? content_length : (content_length - 1);
  s3_log(S3_LOG_DEBUG, request_id, "Found object of size %zu\n",
         content_length);
  if (object_metadata->check_object_tags_exists()) {
    request->set_out_header_value(
        "x-amz-tagging-count",
        std::to_string(object_metadata->object_tags_count()));
  }

  if (content_length == 0) {
    // AWS add explicit quotes "" to etag values.
    // https://docs.aws.amazon.com/AmazonS3/latest/API/API_GetObject.html
    std::string e_tag = "\"" + object_metadata->get_md5() + "\"";

    request->set_out_header_value("Last-Modified",
                                  object_metadata->get_last_modified_gmt());
    request->set_out_header_value("ETag", e_tag);
    request->set_out_header_value("Accept-Ranges", "bytes");
    request->set_out_header_value("Content-Length",
                                  object_metadata->get_content_length_str());
    for (auto it : object_metadata->get_user_attributes()) {
      request->set_out_header_value(it.first, it.second);
    }
    request->send_reply_start(S3HttpSuccess200);
    send_response_to_s3_client();
  } else {
    size_t clovis_unit_size =
        S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(
            object_metadata->get_layout_id());
    s3_log(S3_LOG_DEBUG, request_id,
           "clovis_unit_size = %zu for layout_id = %d\n", clovis_unit_size,
           object_metadata->get_layout_id());
    /* Count Data blocks from data size */
    total_blocks_in_object =
        (content_length + (clovis_unit_size - 1)) / clovis_unit_size;
    s3_log(S3_LOG_DEBUG, request_id, "total_blocks_in_object: (%zu)\n",
           total_blocks_in_object);
    next();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetObjectAction::set_total_blocks_to_read_from_object() {
  // to read complete object, total number blocks to read is equal to total
  // number of blocks
  if ((first_byte_offset_to_read == 0) &&
      (last_byte_offset_to_read == (content_length - 1))) {
    total_blocks_to_read = total_blocks_in_object;
  } else {
    // object read for valid range
    size_t clovis_unit_size =
        S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(
            object_metadata->get_layout_id());
    // get block of first_byte_offset_to_read
    size_t first_byte_offset_block =
        (first_byte_offset_to_read + clovis_unit_size) / clovis_unit_size;
    // get block of last_byte_offset_to_read
    size_t last_byte_offset_block =
        (last_byte_offset_to_read + clovis_unit_size) / clovis_unit_size;
    // get total number blocks to read for a given valid range
    total_blocks_to_read = last_byte_offset_block - first_byte_offset_block + 1;
  }
}

bool S3GetObjectAction::validate_range_header_and_set_read_options(
    const std::string& range_value) {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // The header can consist of 'blank' character(s) only
  if (std::find_if_not(range_value.begin(), range_value.end(), &::isspace) ==
      range_value.end()) {
    s3_log(S3_LOG_DEBUG, request_id,
           "\"Range:\" header consists of blank symbol(s) only");
    return true;
  }
  // reference: http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.35.
  // parse the Range header value
  // eg: bytes=0-1024 value
  std::size_t pos = range_value.find('=');
  // return false when '=' not found
  if (pos == std::string::npos) {
    s3_log(S3_LOG_INFO, request_id, "Invalid range(%s)\n", range_value.c_str());
    return false;
  }

  std::string bytes_unit = S3CommonUtilities::trim(range_value.substr(0, pos));
  std::string byte_range_set = range_value.substr(pos + 1);

  // check bytes_unit has bytes string or not
  if (bytes_unit != "bytes") {
    s3_log(S3_LOG_INFO, request_id, "Invalid range(%s)\n", range_value.c_str());
    return false;
  }

  if (byte_range_set.empty()) {
    // found range as bytes=
    s3_log(S3_LOG_INFO, request_id, "Invalid range(%s)\n", range_value.c_str());
    return false;
  }
  // byte_range_set has multi range
  pos = byte_range_set.find(',');
  if (pos != std::string::npos) {
    // found ,
    // in this case, AWS returns full object and hence we do too
    s3_log(S3_LOG_INFO, request_id, "unsupported multirange(%s)\n",
           byte_range_set.c_str());
    // initialize the first and last offset values with actual object offsets
    // to read complte object
    first_byte_offset_to_read = 0;
    last_byte_offset_to_read = content_length - 1;
    return true;
  }
  pos = byte_range_set.find('-');
  if (pos == std::string::npos) {
    // not found -
    s3_log(S3_LOG_INFO, request_id, "Invalid range(%s)\n", range_value.c_str());
    return false;
  }

  std::string first_byte = byte_range_set.substr(0, pos);
  std::string last_byte = byte_range_set.substr(pos + 1);

  // trip leading and trailing space
  first_byte = S3CommonUtilities::trim(first_byte);
  last_byte = S3CommonUtilities::trim(last_byte);

  // invalid pre-condition checks
  // 1. first and last byte offsets are empty
  // 2. first/last byte is not empty and it has invalid data like char
  if ((first_byte.empty() && last_byte.empty()) ||
      (!first_byte.empty() &&
       !S3CommonUtilities::string_has_only_digits(first_byte)) ||
      (!last_byte.empty() &&
       !S3CommonUtilities::string_has_only_digits(last_byte))) {
    s3_log(S3_LOG_INFO, request_id, "Invalid range(%s)\n", range_value.c_str());
    return false;
  }
  // -nnn
  // Return last 'nnn' bytes from object.
  if (first_byte.empty()) {
    first_byte_offset_to_read = content_length - atol(last_byte.c_str());
    last_byte_offset_to_read = content_length - 1;
  } else if (last_byte.empty()) {
    // nnn-
    // Return from 'nnn' bytes to content_length-1 from object.
    first_byte_offset_to_read = atol(first_byte.c_str());
    last_byte_offset_to_read = content_length - 1;
  } else {
    // both are not empty
    first_byte_offset_to_read = atol(first_byte.c_str());
    last_byte_offset_to_read = atol(last_byte.c_str());
  }
  // last_byte_offset_to_read is greater than or equal to the current length of
  // the entity-body, last_byte_offset_to_read is taken to be equal to
  // one less than the current length of the entity- body in bytes.
  if (last_byte_offset_to_read > content_length - 1) {
    last_byte_offset_to_read = content_length - 1;
  }
  // Range validation
  // If a syntactically valid byte-range-set includes at least one byte-
  // range-spec whose first-byte-pos is less than the current length of the
  // entity-body, or at least one suffix-byte-range-spec with a non- zero
  // suffix-length, then the byte-range-set is satisfiable.
  if ((first_byte_offset_to_read >= content_length) ||
      (first_byte_offset_to_read > last_byte_offset_to_read)) {
    s3_log(S3_LOG_INFO, request_id, "Invalid range(%s)\n", range_value.c_str());
    return false;
  }
  // valid range
  s3_log(S3_LOG_DEBUG, request_id, "valid range(%zu-%zu) found\n",
         first_byte_offset_to_read, last_byte_offset_to_read);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
  return true;
}

void S3GetObjectAction::check_full_or_range_object_read() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  std::string range_header_value = request->get_header_value("Range");
  if (range_header_value.empty()) {
    // Range is not specified, read complte object
    s3_log(S3_LOG_DEBUG, request_id, "Range is not specified\n");
    next();
  } else {
    // parse the Range header value
    // eg: bytes=0-1024 value
    s3_log(S3_LOG_DEBUG, request_id, "Range found(%s)\n",
           range_header_value.c_str());
    if (validate_range_header_and_set_read_options(range_header_value)) {
      next();
    } else {
      set_s3_error("InvalidRange");
      send_response_to_s3_client();
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetObjectAction::read_object() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // get total number of blocks to read from an object
  set_total_blocks_to_read_from_object();
  clovis_reader = clovis_reader_factory->create_clovis_reader(
      request, object_metadata->get_oid(), object_metadata->get_layout_id());
  // get the block,in which first_byte_offset_to_read is present
  // and initilaize the last index with starting offset the block
  size_t block_start_offset =
      first_byte_offset_to_read -
      (first_byte_offset_to_read %
       S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(
           object_metadata->get_layout_id()));
  clovis_reader->set_last_index(block_start_offset);
  read_object_data();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetObjectAction::read_object_data() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }

  size_t max_blocks_in_one_read_op =
      S3Option::get_instance()->get_clovis_units_per_request();
  size_t blocks_to_read = 0;

  s3_log(S3_LOG_DEBUG, request_id, "max_blocks_in_one_read_op: (%zu)\n",
         max_blocks_in_one_read_op);
  s3_log(S3_LOG_DEBUG, request_id, "blocks_already_read: (%zu)\n",
         blocks_already_read);
  s3_log(S3_LOG_DEBUG, request_id, "total_blocks_to_read: (%zu)\n",
         total_blocks_to_read);
  if (blocks_already_read != total_blocks_to_read) {
    if ((total_blocks_to_read - blocks_already_read) >
        max_blocks_in_one_read_op) {
      blocks_to_read = max_blocks_in_one_read_op;
    } else {
      blocks_to_read = total_blocks_to_read - blocks_already_read;
    }
    s3_log(S3_LOG_DEBUG, request_id, "blocks_to_read: (%zu)\n", blocks_to_read);

    if (blocks_to_read > 0) {
      bool op_launched = clovis_reader->read_object_data(
          blocks_to_read,
          std::bind(&S3GetObjectAction::send_data_to_client, this),
          std::bind(&S3GetObjectAction::read_object_data_failed, this));
      if (!op_launched) {
        if (clovis_reader->get_state() ==
            S3ClovisReaderOpState::failed_to_launch) {
          set_s3_error("ServiceUnavailable");
          s3_log(S3_LOG_ERROR, request_id,
                 "read_object_data called due to clovis_entity_open failure\n");
        } else {
          set_s3_error("InternalError");
        }
        send_response_to_s3_client();
      }
    } else {
      send_response_to_s3_client();
    }
  } else {
    // We are done Reading
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetObjectAction::send_data_to_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_stats_inc("read_object_data_success_count");
  log_timed_counter(get_timed_counter, "outgoing_object_data_blocks");

  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (!read_object_reply_started) {
    s3_timer.start();

    // AWS add explicit quotes "" to etag values.
    // https://docs.aws.amazon.com/AmazonS3/latest/API/API_GetObject.html
    std::string e_tag = "\"" + object_metadata->get_md5() + "\"";

    request->set_out_header_value("Last-Modified",
                                  object_metadata->get_last_modified_gmt());
    request->set_out_header_value("ETag", e_tag);
    s3_log(S3_LOG_INFO, request_id, "e_tag= %s", e_tag.c_str());
    request->set_out_header_value("Accept-Ranges", "bytes");
    request->set_out_header_value(
        "Content-Length", std::to_string(get_requested_content_length()));
    for (auto it : object_metadata->get_user_attributes()) {
      request->set_out_header_value(it.first, it.second);
    }
    if (!request->get_header_value("Range").empty()) {
      std::ostringstream content_range_stream;
      content_range_stream << "bytes " << first_byte_offset_to_read << "-"
                           << last_byte_offset_to_read << "/" << content_length;
      request->set_out_header_value("Content-Range",
                                    content_range_stream.str());
      // Partial Content
      request->send_reply_start(S3HttpSuccess206);
    } else {
      request->send_reply_start(S3HttpSuccess200);
    }
    read_object_reply_started = true;
  } else {
    s3_timer.resume();
  }
  s3_log(S3_LOG_DEBUG, request_id, "Earlier data_sent_to_client = %zu bytes.\n",
         data_sent_to_client);

  char* data = NULL;
  size_t length = 0;
  size_t requested_content_length = get_requested_content_length();
  s3_log(S3_LOG_DEBUG, request_id,
         "object requested content length size(%zu).\n",
         requested_content_length);
  length = clovis_reader->get_first_block(&data);

  while (length > 0) {
    size_t read_data_start_offset = 0;
    blocks_already_read++;
    if (data_sent_to_client == 0) {
      // get starting offset from the block,
      // condition true for only statring block read object.
      // this is to set get first offset byte from initial read block
      // eg: read_data_start_offset will be set to 1000 on initial read block
      // for a given range 1000-1500 to read from 2mb object
      read_data_start_offset =
          first_byte_offset_to_read %
          S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(
              object_metadata->get_layout_id());
      length -= read_data_start_offset;
    }
    // to read number of bytes from final read block of read object
    // that is requested content length is lesser than the sum of data has been
    // sent to client and current read block size
    if ((data_sent_to_client + length) >= requested_content_length) {
      // length will have the size of remaining byte to sent
      length = requested_content_length - data_sent_to_client;
    }
    data_sent_to_client += length;
    s3_log(S3_LOG_DEBUG, request_id, "Sending %zu bytes to client.\n", length);
    request->send_reply_body(data + read_data_start_offset, length);
    s3_perf_count_outcoming_bytes(length);
    length = clovis_reader->get_next_block(&data);
  }
  s3_timer.stop();

  if (data_sent_to_client != requested_content_length) {
    read_object_data();
  } else {
    const auto mss = s3_timer.elapsed_time_in_millisec();
    LOG_PERF("get_object_send_data_ms", request_id.c_str(), mss);
    s3_stats_timing("get_object_send_data", mss);

    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3GetObjectAction::read_object_data_failed() {
  s3_log(S3_LOG_DEBUG, request_id, "Failed to read object data from clovis\n");
  // set error only when reply is not started
  if (!read_object_reply_started) {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
}

void S3GetObjectAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if (reject_if_shutting_down()) {
    if (read_object_reply_started) {
      request->send_reply_end();
    } else {
      // Send response with 'Service Unavailable' code.
      s3_log(S3_LOG_DEBUG, request_id,
             "sending 'Service Unavailable' response...\n");
      S3Error error("ServiceUnavailable", request->get_request_id(),
                    request->get_object_uri());
      std::string& response_xml = error.to_xml();
      request->set_out_header_value("Content-Type", "application/xml");
      request->set_out_header_value("Content-Length",
                                    std::to_string(response_xml.length()));
      request->set_out_header_value("Retry-After", "1");

      request->send_response(error.get_http_status_code(), response_xml);
    }
  } else if (is_error_state() && !get_s3_error_code().empty()) {
    // Invalid Bucket Name
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    if (get_s3_error_code() == "ServiceUnavailable") {
      request->set_out_header_value("Retry-After", "1");
    }
    request->send_response(error.get_http_status_code(), response_xml);
  } else if (object_metadata &&
             (object_metadata->get_content_length() == 0 ||
              (clovis_reader &&
               clovis_reader->get_state() == S3ClovisReaderOpState::success))) {
    request->send_reply_end();
  } else {
    if (read_object_reply_started) {
      request->send_reply_end();
    } else {
      S3Error error("InternalError", request->get_request_id(),
                    request->get_object_uri());
      std::string& response_xml = error.to_xml();
      request->set_out_header_value("Content-Type", "application/xml");
      request->set_out_header_value("Content-Length",
                                    std::to_string(response_xml.length()));
      request->set_out_header_value("Retry-After", "1");

      request->send_response(error.get_http_status_code(), response_xml);
    }
  }
  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
