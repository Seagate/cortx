/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original creation date: 23-Mar-2017
 */

#pragma once

#ifndef __S3_UT_MOCK_S3_FACTORY_H__
#define __S3_UT_MOCK_S3_FACTORY_H__

#include <memory>

#include "mock_s3_async_buffer_opt_container.h"
#include "mock_s3_auth_client.h"
#include "mock_s3_bucket_metadata.h"
#include "mock_s3_clovis_kvs_reader.h"
#include "mock_s3_clovis_kvs_writer.h"
#include "mock_s3_clovis_reader.h"
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_clovis_writer.h"
#include "mock_s3_object_metadata.h"
#include "mock_s3_object_multipart_metadata.h"
#include "mock_s3_part_metadata.h"
#include "mock_s3_put_bucket_body.h"
#include "mock_s3_request_object.h"
#include "mock_s3_put_tag_body.h"
#include "mock_s3_global_bucket_index_metadata.h"
#include "s3_factory.h"

class MockS3BucketMetadataFactory : public S3BucketMetadataFactory {
 public:
  MockS3BucketMetadataFactory(std::shared_ptr<S3RequestObject> req,
                              std::shared_ptr<MockS3Clovis> s3_clovis_mock_ptr =
                                  nullptr)
      : S3BucketMetadataFactory() {
    //  We create object here since we want to set some expectations
    // Before create_bucket_metadata_obj() is called
    mock_bucket_metadata =
        std::make_shared<MockS3BucketMetadata>(req, s3_clovis_mock_ptr);
  }

  std::shared_ptr<S3BucketMetadata> create_bucket_metadata_obj(
      std::shared_ptr<S3RequestObject> req) override {
    return mock_bucket_metadata;
  }

  // Use this to setup your expectations.
  std::shared_ptr<MockS3BucketMetadata> mock_bucket_metadata;
};

class MockS3ObjectMetadataFactory : public S3ObjectMetadataFactory {
 public:
  MockS3ObjectMetadataFactory(std::shared_ptr<S3RequestObject> req,
                              std::shared_ptr<MockS3Clovis> s3_clovis_mock_ptr =
                                  nullptr)
      : S3ObjectMetadataFactory() {
    mock_object_metadata =
        std::make_shared<MockS3ObjectMetadata>(req, s3_clovis_mock_ptr);
  }

  void set_object_list_index_oid(struct m0_uint128 id) {
    mock_object_metadata->set_object_list_index_oid(id);
  }

  std::shared_ptr<S3ObjectMetadata> create_object_metadata_obj(
      std::shared_ptr<S3RequestObject> req,
      struct m0_uint128 indx_oid = {0ULL, 0ULL}) override {
    mock_object_metadata->set_object_list_index_oid(indx_oid);
    return mock_object_metadata;
  }

  std::shared_ptr<MockS3ObjectMetadata> mock_object_metadata;
};

class MockS3PartMetadataFactory : public S3PartMetadataFactory {
 public:
  MockS3PartMetadataFactory(std::shared_ptr<S3RequestObject> req,
                            m0_uint128 indx_oid, std::string upload_id,
                            int part_num)
      : S3PartMetadataFactory() {
    mock_part_metadata = std::make_shared<MockS3PartMetadata>(
        req, indx_oid, upload_id, part_num);
  }

  std::shared_ptr<S3PartMetadata> create_part_metadata_obj(
      std::shared_ptr<S3RequestObject> req, struct m0_uint128 indx_oid,
      std::string upload_id, int part_num) override {
    return mock_part_metadata;
  }

  std::shared_ptr<S3PartMetadata> create_part_metadata_obj(
      std::shared_ptr<S3RequestObject> req, std::string upload_id,
      int part_num) override {
    return mock_part_metadata;
  }

  std::shared_ptr<MockS3PartMetadata> mock_part_metadata;
};

class MockS3ObjectMultipartMetadataFactory
    : public S3ObjectMultipartMetadataFactory {
 public:
  MockS3ObjectMultipartMetadataFactory(
      std::shared_ptr<S3RequestObject> req,
      std::shared_ptr<MockS3Clovis> s3_clovis_mock_ptr, std::string upload_id)
      : S3ObjectMultipartMetadataFactory() {
    //  We create object here since we want to set some expectations
    // Before create_bucket_metadata_obj() is called
    mock_object_mp_metadata = std::make_shared<MockS3ObjectMultipartMetadata>(
        req, s3_clovis_mock_ptr, upload_id);
  }

  void set_object_list_index_oid(struct m0_uint128 id) {
    mock_object_mp_metadata->set_object_list_index_oid(id);
  }

  std::shared_ptr<S3ObjectMetadata> create_object_mp_metadata_obj(
      std::shared_ptr<S3RequestObject> req, struct m0_uint128 mp_indx_oid,
      std::string upload_id) override {
    mock_object_mp_metadata->set_object_list_index_oid(mp_indx_oid);
    return mock_object_mp_metadata;
  }

  // Use this to setup your expectations.
  std::shared_ptr<MockS3ObjectMultipartMetadata> mock_object_mp_metadata;
};

class MockS3ClovisWriterFactory : public S3ClovisWriterFactory {
 public:
  MockS3ClovisWriterFactory(
      std::shared_ptr<RequestObject> req, m0_uint128 oid,
      std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api = nullptr)
      : S3ClovisWriterFactory() {
    mock_clovis_writer =
        std::make_shared<MockS3ClovisWriter>(req, oid, ptr_mock_s3_clovis_api);
  }

  MockS3ClovisWriterFactory(
      std::shared_ptr<RequestObject> req,
      std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api)
      : S3ClovisWriterFactory() {
    mock_clovis_writer =
        std::make_shared<MockS3ClovisWriter>(req, ptr_mock_s3_clovis_api);
  }

  std::shared_ptr<S3ClovisWriter> create_clovis_writer(
      std::shared_ptr<RequestObject> req, struct m0_uint128 oid) override {
    return mock_clovis_writer;
  }

  std::shared_ptr<S3ClovisWriter> create_clovis_writer(
      std::shared_ptr<RequestObject> req) override {
    return mock_clovis_writer;
  }

  std::shared_ptr<S3ClovisWriter> create_clovis_writer(
      std::shared_ptr<RequestObject> req, struct m0_uint128 oid,
      uint64_t offset) override {
    return mock_clovis_writer;
  }

  std::shared_ptr<MockS3ClovisWriter> mock_clovis_writer;
};

class MockS3ClovisReaderFactory : public S3ClovisReaderFactory {
 public:
  MockS3ClovisReaderFactory(std::shared_ptr<RequestObject> req, m0_uint128 oid,
                            int layout_id,
                            std::shared_ptr<MockS3Clovis> s3_clovis_mock_apis =
                                nullptr)
      : S3ClovisReaderFactory() {
    mock_clovis_reader = std::make_shared<MockS3ClovisReader>(
        req, oid, layout_id, s3_clovis_mock_apis);
  }

  std::shared_ptr<S3ClovisReader> create_clovis_reader(
      std::shared_ptr<RequestObject> req, struct m0_uint128 oid, int layout_id,
      std::shared_ptr<ClovisAPI> clovis_api = nullptr) override {
    return mock_clovis_reader;
  }

  std::shared_ptr<MockS3ClovisReader> mock_clovis_reader;
};

class MockS3ClovisKVSReaderFactory : public S3ClovisKVSReaderFactory {
 public:
  MockS3ClovisKVSReaderFactory(std::shared_ptr<RequestObject> req,
                               std::shared_ptr<MockS3Clovis> s3_clovis_mock_api)
      : S3ClovisKVSReaderFactory() {
    mock_clovis_kvs_reader =
        std::make_shared<MockS3ClovisKVSReader>(req, s3_clovis_mock_api);
  }

  std::shared_ptr<S3ClovisKVSReader> create_clovis_kvs_reader(
      std::shared_ptr<RequestObject> req,
      std::shared_ptr<ClovisAPI> s3_clovis_api = nullptr) override {
    return mock_clovis_kvs_reader;
  }

  std::shared_ptr<MockS3ClovisKVSReader> mock_clovis_kvs_reader;
};

class MockS3ClovisKVSWriterFactory : public S3ClovisKVSWriterFactory {
 public:
  MockS3ClovisKVSWriterFactory(std::shared_ptr<RequestObject> req,
                               std::shared_ptr<MockS3Clovis> s3_clovis_api =
                                   nullptr)
      : S3ClovisKVSWriterFactory() {
    mock_clovis_kvs_writer =
        std::make_shared<MockS3ClovisKVSWriter>(req, s3_clovis_api);
  }

  std::shared_ptr<S3ClovisKVSWriter> create_clovis_kvs_writer(
      std::shared_ptr<RequestObject> req,
      std::shared_ptr<ClovisAPI> s3_clovis_api = nullptr) override {
    return mock_clovis_kvs_writer;
  }

  std::shared_ptr<MockS3ClovisKVSWriter> mock_clovis_kvs_writer;
};

class MockS3AsyncBufferOptContainerFactory
    : public S3AsyncBufferOptContainerFactory {
 public:
  MockS3AsyncBufferOptContainerFactory(size_t size_of_each_buf)
      : S3AsyncBufferOptContainerFactory() {
    mock_async_buffer =
        std::make_shared<MockS3AsyncBufferOptContainer>(size_of_each_buf);
  }

  std::shared_ptr<S3AsyncBufferOptContainer> create_async_buffer(
      size_t size_of_each_buf) override {
    return mock_async_buffer;
  }

  std::shared_ptr<MockS3AsyncBufferOptContainer> get_mock_buffer() {
    return mock_async_buffer;
  }

  std::shared_ptr<MockS3AsyncBufferOptContainer> mock_async_buffer;
};

class MockS3PutBucketBodyFactory : public S3PutBucketBodyFactory {
 public:
  MockS3PutBucketBodyFactory(std::string& xml) : S3PutBucketBodyFactory() {
    mock_put_bucket_body = std::make_shared<MockS3PutBucketBody>(xml);
  }

  std::shared_ptr<S3PutBucketBody> create_put_bucket_body(std::string& xml)
      override {
    return mock_put_bucket_body;
  }

  // Use this to setup your expectations.
  std::shared_ptr<MockS3PutBucketBody> mock_put_bucket_body;
};

class MockS3PutTagBodyFactory : public S3PutTagsBodyFactory {
 public:
  MockS3PutTagBodyFactory(std::string& xml, std::string& request_id)
      : S3PutTagsBodyFactory() {
    mock_put_bucket_tag_body =
        std::make_shared<MockS3PutTagBody>(xml, request_id);
  }

  std::shared_ptr<S3PutTagBody> create_put_resource_tags_body(
      std::string& xml, std::string& request_id) override {
    return mock_put_bucket_tag_body;
  }

  // Use this to setup your expectations.
  std::shared_ptr<MockS3PutTagBody> mock_put_bucket_tag_body;
};

class MockS3AuthClientFactory : public S3AuthClientFactory {
 public:
  MockS3AuthClientFactory(std::shared_ptr<S3RequestObject> req)
      : S3AuthClientFactory() {
    mock_auth_client = std::make_shared<MockS3AuthClient>(req);
  }

  std::shared_ptr<S3AuthClient> create_auth_client(
      std::shared_ptr<RequestObject> req,
      bool skip_authorization = false) override {
    return mock_auth_client;
  }

  std::shared_ptr<MockS3AuthClient> mock_auth_client;
};

class MockS3GlobalBucketIndexMetadataFactory
    : public S3GlobalBucketIndexMetadataFactory {
 public:
  MockS3GlobalBucketIndexMetadataFactory(std::shared_ptr<S3RequestObject> req)
      : S3GlobalBucketIndexMetadataFactory() {
    mock_global_bucket_index_metadata =
        std::make_shared<MockS3GlobalBucketIndexMetadata>(req);
  }

  std::shared_ptr<S3GlobalBucketIndexMetadata>
  create_s3_global_bucket_index_metadata(std::shared_ptr<S3RequestObject> req)
      override {
    return mock_global_bucket_index_metadata;
  }

  std::shared_ptr<MockS3GlobalBucketIndexMetadata>
      mock_global_bucket_index_metadata;
};

#endif

