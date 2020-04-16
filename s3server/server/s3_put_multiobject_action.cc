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
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original author:  Rajesh Nambiar        <rajesh.nambiar@seagate.com>
 * Original creation date: 22-Jan-2016
 */

#include "s3_put_multiobject_action.h"
#include "s3_error_codes.h"
#include "s3_log.h"
#include "s3_option.h"
#include "s3_perf_logger.h"
#include "s3_perf_metrics.h"

S3PutMultiObjectAction::S3PutMultiObjectAction(
    std::shared_ptr<S3RequestObject> req,
    std::shared_ptr<S3ObjectMultipartMetadataFactory> object_mp_meta_factory,
    std::shared_ptr<S3PartMetadataFactory> part_meta_factory,
    std::shared_ptr<S3ClovisWriterFactory> clovis_s3_writer_factory,
    std::shared_ptr<S3AuthClientFactory> auth_factory)
    : S3ObjectAction(std::move(req), nullptr, nullptr, true,
                     std::move(auth_factory)),
      total_data_to_stream(0),
      auth_failed(false),
      write_failed(false),
      clovis_write_in_progress(false),
      clovis_write_completed(false),
      auth_in_progress(false),
      auth_completed(false) {
  s3_log(S3_LOG_DEBUG, request_id, "Constructor\n");
  part_number = get_part_number();
  upload_id = request->get_query_string_value("uploadId");

  s3_log(S3_LOG_INFO, request_id,
         "S3 API: Upload Part. Bucket[%s] Object[%s]\
         Part[%d] for UploadId[%s]\n",
         request->get_bucket_name().c_str(), request->get_object_name().c_str(),
         part_number, upload_id.c_str());

  layout_id = -1;  // Loaded from multipart metadata

  if (S3Option::get_instance()->is_auth_disabled()) {
    auth_completed = true;
  }

  if (object_mp_meta_factory) {
    object_mp_metadata_factory = object_mp_meta_factory;
  } else {
    object_mp_metadata_factory =
        std::make_shared<S3ObjectMultipartMetadataFactory>();
  }

  if (part_meta_factory) {
    part_metadata_factory = part_meta_factory;
  } else {
    part_metadata_factory = std::make_shared<S3PartMetadataFactory>();
  }

  if (clovis_s3_writer_factory) {
    clovis_writer_factory = clovis_s3_writer_factory;
  } else {
    clovis_writer_factory = std::make_shared<S3ClovisWriterFactory>();
  }

  setup_steps();
}

void S3PutMultiObjectAction::setup_steps() {
  s3_log(S3_LOG_DEBUG, request_id, "Setting up the action\n");

  ACTION_TASK_ADD(S3PutMultiObjectAction::check_part_number, this);
  ACTION_TASK_ADD(S3PutMultiObjectAction::fetch_multipart_metadata, this);
  if (part_number == 1) {
    // Save first part size to multipart metadata in case of non
    // chunked mode.
    // In case of chunked multipart upload we wont have Content-Length
    // http header within part reqquest, so we cannot save it -- Size
    // is specified within streamed payload chunks(body)
    if (!request->is_chunked()) {
      ACTION_TASK_ADD(S3PutMultiObjectAction::save_multipart_metadata, this);
    }
  } else {
    // For chunked multipart upload only we need to access first part
    // info for part one size, in case of non chunked multipart upload
    // we get the same from multipart metadata.
    if (request->is_chunked()) {
      ACTION_TASK_ADD(S3PutMultiObjectAction::fetch_firstpart_info, this);
    }
  }
  ACTION_TASK_ADD(S3PutMultiObjectAction::compute_part_offset, this);
  ACTION_TASK_ADD(S3PutMultiObjectAction::initiate_data_streaming, this);
  ACTION_TASK_ADD(S3PutMultiObjectAction::save_metadata, this);
  ACTION_TASK_ADD(S3PutMultiObjectAction::send_response_to_s3_client, this);
  // ...
}

void S3PutMultiObjectAction::check_part_number() {
  // "Part numbers can be any number from 1 to 10,000, inclusive."
  // https://docs.aws.amazon.com/en_us/AmazonS3/latest/API/API_UploadPart.html
  if (part_number >= MINIMUM_PART_NUMBER &&
      part_number <= MAXIMUM_PART_NUMBER) {
    next();
  } else {
    set_s3_error("InvalidPart");
    send_response_to_s3_client();
  }
}

void S3PutMultiObjectAction::chunk_auth_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  auth_in_progress = false;
  auth_completed = true;
  if (check_shutdown_and_rollback(true)) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (clovis_write_completed) {
    if (write_failed) {
      // No need of setting error as its set when we set write_failed
      send_response_to_s3_client();
    } else {
      next();
    }
  } else {
    // wait for write to complete. do nothing here.
  }
}

void S3PutMultiObjectAction::chunk_auth_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  auth_failed = true;
  auth_completed = true;
  set_s3_error("SignatureDoesNotMatch");
  if (check_shutdown_and_rollback(true)) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (clovis_write_in_progress) {
    // Do nothing, handle after write returns
  } else {
    send_response_to_s3_client();
  }
}

void S3PutMultiObjectAction::fetch_bucket_info_success() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (bucket_metadata->get_state() == S3BucketMetadataState::present) {
    next();
  } else {
    set_s3_error("NoSuchBucket");
    send_response_to_s3_client();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
void S3PutMultiObjectAction::fetch_object_info_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
void S3PutMultiObjectAction::fetch_bucket_info_failed() {
  s3_log(S3_LOG_ERROR, request_id, "Bucket does not exists\n");
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
}

void S3PutMultiObjectAction::fetch_multipart_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  object_multipart_metadata =
      object_mp_metadata_factory->create_object_mp_metadata_obj(
          request, bucket_metadata->get_multipart_index_oid(), upload_id);

  object_multipart_metadata->load(
      std::bind(&S3PutMultiObjectAction::next, this),
      std::bind(&S3PutMultiObjectAction::fetch_multipart_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::fetch_multipart_failed() {
  // Log error
  s3_log(S3_LOG_ERROR, request_id,
         "Failed to retrieve multipart upload metadata\n");
  if (object_multipart_metadata->get_state() ==
      S3ObjectMetadataState::missing) {
    set_s3_error("NoSuchUpload");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
}

void S3PutMultiObjectAction::save_multipart_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // This function to be called for part 1 upload
  // so that other parts can see the size of part 1
  // to proceed.Also this is only in case of
  // non-chunked part upload.
  assert(part_number == 1);
  size_t part_one_size_in_multipart_metadata =
      object_multipart_metadata->get_part_one_size();
  size_t current_part_one_size = request->get_data_length();

  if (part_one_size_in_multipart_metadata != 0) {
    s3_log(S3_LOG_WARN, request_id,
           "Part one size in multipart metadata "
           "(%zu) differs from current part one "
           "size (%zu)",
           part_one_size_in_multipart_metadata, current_part_one_size);
    if (current_part_one_size != part_one_size_in_multipart_metadata) {
      s3_log(S3_LOG_ERROR, request_id,
             "Part one size in multipart metadata "
             "(%zu) differs from current part one "
             "size (%zu)",
             part_one_size_in_multipart_metadata, current_part_one_size);
      set_s3_error("InvalidObjectState");
      send_response_to_s3_client();
      s3_log(S3_LOG_DEBUG, "", "Exiting\n");
      return;
    }
  }
  // to rest Date and Last-Modfied time object metadata
  object_multipart_metadata->reset_date_time_to_current();
  object_multipart_metadata->set_part_one_size(current_part_one_size);
  object_multipart_metadata->save(
      std::bind(&S3PutMultiObjectAction::next, this),
      std::bind(&S3PutMultiObjectAction::save_multipart_metadata_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::save_multipart_metadata_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  s3_log(S3_LOG_ERROR, request_id,
         "Failed to update multipart metadata with part one size\n");
  if (object_multipart_metadata->get_state() ==
      S3ObjectMetadataState::failed_to_launch) {
    s3_log(S3_LOG_WARN, request_id,
           "Multipart metadata save operation failed\n");
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::fetch_firstpart_info() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  part_metadata = part_metadata_factory->create_part_metadata_obj(
      request, object_multipart_metadata->get_part_index_oid(), upload_id, 1);
  part_metadata->load(
      std::bind(&S3PutMultiObjectAction::next, this),
      std::bind(&S3PutMultiObjectAction::fetch_firstpart_info_failed, this), 1);
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::fetch_firstpart_info_failed() {
  s3_log(S3_LOG_WARN, request_id,
         "Part 1 metadata doesn't exist, cannot determine \"consistent\" part "
         "size\n");
  if (part_metadata->get_state() == S3PartMetadataState::missing ||
      part_metadata->get_state() == S3PartMetadataState::failed_to_launch) {
    // May happen if part 2/3... comes before part 1, in that case those part
    // upload need to be retried(by that time part 1 meta data will get in)
    // or its a pre launch failure
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
}

void S3PutMultiObjectAction::compute_part_offset() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  size_t offset = 0;
  if (part_number != 1) {
    size_t part_one_size = 0;
    if (request->is_chunked()) {
      part_one_size = part_metadata->get_content_length();
    } else {
      // In case of no chunked multipart upload we put in
      // part one size to the multipart metadata, if its not there
      // it means part one request didn't come till now
      part_one_size = object_multipart_metadata->get_part_one_size();
      if (part_one_size == 0) {
        // May happen if part 2/3... comes before part 1, in that case those
        // part pload need to be retried(by that time part 1 metadata will
        // get in)
        s3_log(S3_LOG_WARN, request_id,
               "Part 1 size is not there in multipart "
               "metadata, hence rejecting this "
               "request of part %d for time being, "
               "Try later...\n",
               part_number);
        set_s3_error("ServiceUnavailable");
        send_response_to_s3_client();
        return;
      }
    }
    s3_log(S3_LOG_DEBUG, request_id, "Part size = %zu for part_number = %d\n",
           request->get_content_length(), part_number);
    // Calculate offset
    offset = (part_number - 1) * part_one_size;
    s3_log(S3_LOG_DEBUG, request_id, "Offset for clovis write = %zu\n", offset);
  }
  // Create writer to write from given offset as per the partnumber
  clovis_writer = clovis_writer_factory->create_clovis_writer(
      request, object_multipart_metadata->get_oid(), offset);
  layout_id = object_multipart_metadata->get_layout_id();
  clovis_writer->set_layout_id(layout_id);

  // FIXME multipart uploads are corrupted when partsize is not aligned with
  // clovis unit size for given layout_id. We block such uploads temporarily
  // and it will be fixed as a bug.
  if (part_number == 1) {
    // Reject during first part itself
    size_t unit_size =
        S3ClovisLayoutMap::get_instance()->get_unit_size_for_layout(layout_id);
    size_t part_size = request->get_data_length();
    s3_log(S3_LOG_DEBUG, request_id,
           "Check part size (%zu) and unit_size (%zu) compatibility\n",
           part_size, unit_size);
    if ((part_size % unit_size) != 0) {
      s3_log(S3_LOG_DEBUG, request_id,
             "Rejecting request as part size is not aligned w.r.t unit_size\n");
      // part size is not multiple of unit size, block request
      set_s3_error("InvalidPartPerS3Mero");
      s3_log(S3_LOG_DEBUG, "", "Exiting\n");
      send_response_to_s3_client();
      return;
    }
  }
  next();

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::initiate_data_streaming() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  total_data_to_stream = request->get_data_length();
  // request->resume();

  if (request->is_chunked() && !S3Option::get_instance()->is_auth_disabled()) {
    get_auth_client()->init_chunk_auth_cycle(
        std::bind(&S3PutMultiObjectAction::chunk_auth_successful, this),
        std::bind(&S3PutMultiObjectAction::chunk_auth_failed, this));
  }

  if (total_data_to_stream == 0) {
    next();  // Zero size object.
  } else {
    if (request->has_all_body_content()) {
      write_object(request->get_buffered_input());
    } else {
      s3_log(S3_LOG_DEBUG, request_id,
             "We do not have all the data, so start listening....\n");
      // Start streaming, logically pausing action till we get data.
      request->listen_for_incoming_data(
          std::bind(&S3PutMultiObjectAction::consume_incoming_content, this),
          S3Option::get_instance()->get_clovis_write_payload_size(layout_id));
    }
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::consume_incoming_content() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  // for shutdown testcases, check FI and set shutdown signal
  S3_CHECK_FI_AND_SET_SHUTDOWN_SIGNAL(
      "put_multiobject_action_consume_incoming_content_shutdown_fail");
  if (request->is_s3_client_read_error()) {
    if (!clovis_write_in_progress) {
      client_read_error();
    }
    return;
  }

  log_timed_counter(put_timed_counter, "incoming_object_data_blocks");
  s3_perf_count_incoming_bytes(
      request->get_buffered_input()->get_content_length());
  if (!clovis_write_in_progress) {
    if (request->get_buffered_input()->is_freezed() ||
        request->get_buffered_input()->get_content_length() >=
            S3Option::get_instance()->get_clovis_write_payload_size(
                layout_id)) {
      write_object(request->get_buffered_input());
      if (!clovis_write_in_progress && clovis_write_completed) {
        s3_log(S3_LOG_DEBUG, "", "Exiting\n");
        return;
      }
    }
  }
  if (!request->get_buffered_input()->is_freezed() &&
      request->get_buffered_input()->get_content_length() >=
          (S3Option::get_instance()->get_clovis_write_payload_size(layout_id) *
           S3Option::get_instance()->get_read_ahead_multiple())) {
    s3_log(S3_LOG_DEBUG, request_id, "Pausing with Buffered length = %zu\n",
           request->get_buffered_input()->get_content_length());
    request->pause();
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::send_chunk_details_if_any() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  while (request->is_chunk_detail_ready()) {
    S3ChunkDetail detail = request->pop_chunk_detail();
    s3_log(S3_LOG_DEBUG, request_id, "Using chunk details for auth:\n");
    detail.debug_dump();
    if (!S3Option::get_instance()->is_auth_disabled()) {
      if (detail.get_size() == 0) {
        // Last chunk is size 0
        get_auth_client()->add_last_checksum_for_chunk(
            detail.get_signature(), detail.get_payload_hash());
      } else {
        get_auth_client()->add_checksum_for_chunk(detail.get_signature(),
                                                  detail.get_payload_hash());
      }
      auth_in_progress = true;  // this triggers auth
    }
  }
}

void S3PutMultiObjectAction::write_object(
    std::shared_ptr<S3AsyncBufferOptContainer> buffer) {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (request->is_chunked()) {
    // Also send any ready chunk data for auth
    send_chunk_details_if_any();
  }
  clovis_write_in_progress = true;

  clovis_writer->write_content(
      std::bind(&S3PutMultiObjectAction::write_object_successful, this),
      std::bind(&S3PutMultiObjectAction::write_object_failed, this), buffer);

  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::write_object_successful() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  clovis_write_in_progress = false;
  if (check_shutdown_and_rollback()) {
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }
  if (request->is_s3_client_read_error()) {
    client_read_error();
    return;
  }
  s3_log(S3_LOG_DEBUG, request_id, "Write successful\n");
  if (request->is_chunked()) {
    if (auth_failed) {
      set_s3_error("SignatureDoesNotMatch");
      send_response_to_s3_client();
      return;
    }
  }
  if (/* buffered data len is at least equal max we can write to clovis in one
         write */
      request->get_buffered_input()->get_content_length() >=
          S3Option::get_instance()->get_clovis_write_payload_size(layout_id) ||
      /* we have all the data buffered and ready to write */
      (request->get_buffered_input()->is_freezed() &&
       request->get_buffered_input()->get_content_length() > 0)) {
    write_object(request->get_buffered_input());
  } else if (request->get_buffered_input()->is_freezed() &&
             request->get_buffered_input()->get_content_length() == 0) {
    clovis_write_completed = true;
    if (request->is_chunked()) {
      if (auth_completed) {
        next();
      } else {
        // else wait for auth to complete
        send_chunk_details_if_any();
      }
    } else {
      next();
    }
  } else if (!request->get_buffered_input()->is_freezed()) {
    // else we wait for more incoming data
    request->resume();
  }
}

void S3PutMultiObjectAction::write_object_failed() {
  s3_log(S3_LOG_ERROR, request_id, "Write to clovis failed\n");

  clovis_write_in_progress = false;
  clovis_write_completed = true;

  if (request->is_s3_client_read_error()) {
    client_read_error();
    return;
  }
  if (clovis_writer->get_state() == S3ClovisWriterOpState::failed_to_launch) {
    set_s3_error("ServiceUnavailable");
  } else {
    set_s3_error("InternalError");
  }
  if (request->is_chunked()) {
    write_failed = true;
    request->pause();  // pause any further reading from client.
    get_auth_client()->abort_chunk_auth_op();
    if (!auth_in_progress) {
      send_response_to_s3_client();
    }
  } else {
    send_response_to_s3_client();
  }
}

void S3PutMultiObjectAction::save_metadata() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  part_metadata = part_metadata_factory->create_part_metadata_obj(
      request, object_multipart_metadata->get_part_index_oid(), upload_id,
      part_number);

  // to rest Date and Last-Modfied time object metadata
  part_metadata->reset_date_time_to_current();
  part_metadata->set_content_length(request->get_data_length_str());
  part_metadata->set_md5(clovis_writer->get_content_md5());
  for (auto it : request->get_in_headers_copy()) {
    if (it.first.find("x-amz-meta-") != std::string::npos) {
      part_metadata->add_user_defined_attribute(it.first, it.second);
    }
  }
  // bypass shutdown signal check for next task
  check_shutdown_signal_for_next_task(false);

  if (s3_fi_is_enabled("fail_save_part_mdata")) {
    s3_fi_enable_once("clovis_kv_put_fail");
  }

  part_metadata->save(
      std::bind(&S3PutMultiObjectAction::next, this),
      std::bind(&S3PutMultiObjectAction::save_metadata_failed, this));
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::save_metadata_failed() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");
  if (part_metadata->get_state() == S3PartMetadataState::failed_to_launch) {
    s3_log(S3_LOG_ERROR, request_id,
           "Save of Part metadata failed due to pre launch failure\n");
    set_s3_error("ServiceUnavailable");
  } else {
    s3_log(S3_LOG_ERROR, request_id, "Save of Part metadata failed\n");
    set_s3_error("InternalError");
  }
  send_response_to_s3_client();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

void S3PutMultiObjectAction::send_response_to_s3_client() {
  s3_log(S3_LOG_INFO, request_id, "Entering\n");

  if ((auth_in_progress) &&
      (get_auth_client()->get_state() == S3AuthClientOpState::started)) {
    get_auth_client()->abort_chunk_auth_op();
    s3_log(S3_LOG_DEBUG, "", "Exiting\n");
    return;
  }

  if (reject_if_shutting_down() ||
      (is_error_state() && !get_s3_error_code().empty())) {
    // Send response with 'Service Unavailable' code.
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();

    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));

    if (get_s3_error_code() == "ServiceUnavailable" ||
        get_s3_error_code() == "InternalError") {
      request->set_out_header_value("Connection", "close");
    }

    if (get_s3_error_code() == "ServiceUnavailable") {
      request->set_out_header_value("Retry-After", "1");
    }

    request->send_response(error.get_http_status_code(), response_xml);
  } else if (clovis_writer != NULL) {
    // AWS adds explicit quotes "" to etag values.
    std::string e_tag = "\"" + clovis_writer->get_content_md5() + "\"";

    request->set_out_header_value("ETag", e_tag);

    request->send_response(S3HttpSuccess200);
  } else {
    set_s3_error("InternalError");
    S3Error error(get_s3_error_code(), request->get_request_id(),
                  request->get_object_uri());
    std::string& response_xml = error.to_xml();
    request->set_out_header_value("Content-Type", "application/xml");
    request->set_out_header_value("Content-Length",
                                  std::to_string(response_xml.length()));
    request->set_out_header_value("Connection", "close");
    request->send_response(error.get_http_status_code(), response_xml);
  }

  S3_RESET_SHUTDOWN_SIGNAL;  // for shutdown testcases
  request->resume(false);

  done();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}
void S3PutMultiObjectAction::set_authorization_meta() {
  s3_log(S3_LOG_DEBUG, request_id, "Entering\n");
  auth_client->set_acl_and_policy(bucket_metadata->get_encoded_bucket_acl(),
                                  bucket_metadata->get_policy_as_json());
  next();
  s3_log(S3_LOG_DEBUG, "", "Exiting\n");
}

