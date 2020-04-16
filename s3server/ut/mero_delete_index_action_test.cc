/*
 * COPYRIGHT 2020 SEAGATE LLC
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
 * Original creation date: 30-March-2020
 */

#include <memory>

#include "mock_s3_clovis_wrapper.h"
#include "mock_mero_request_object.h"
#include "mock_s3_factory.h"
#include "mero_delete_index_action.h"
#include "s3_m0_uint128_helper.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using ::testing::AtLeast;

class MeroDeleteIndexActionTest : public testing::Test {
 protected:
  MeroDeleteIndexActionTest() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    index_id = {0x1ffff, 0x1ffff};
    zero_index_id = {0ULL, 0ULL};
    call_count_one = 0;

    auto index_id_str_pair = S3M0Uint128Helper::to_string_pair(index_id);
    index_id_str_hi = index_id_str_pair.first;
    index_id_str_lo = index_id_str_pair.second;

    auto zero_index_id_str_pair =
        S3M0Uint128Helper::to_string_pair(zero_index_id);
    zero_index_id_str_hi = zero_index_id_str_pair.first;
    zero_index_id_str_lo = zero_index_id_str_pair.second;

    ptr_mock_request =
        std::make_shared<MockMeroRequestObject>(req, evhtp_obj_ptr);

    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();
    // Owned and deleted by shared_ptr in MeroDeleteIndexAction
    clovis_kvs_writer_factory = std::make_shared<MockS3ClovisKVSWriterFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);

    action_under_test.reset(
        new MeroDeleteIndexAction(ptr_mock_request, clovis_kvs_writer_factory));
  }

  std::shared_ptr<MockMeroRequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  std::shared_ptr<MeroDeleteIndexAction> action_under_test;

  struct m0_uint128 index_id;
  std::string index_id_str_lo;
  std::string index_id_str_hi;
  struct m0_uint128 zero_index_id;
  std::string zero_index_id_str_lo;
  std::string zero_index_id_str_hi;

  int call_count_one;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(MeroDeleteIndexActionTest, ConstructorTest) {
  EXPECT_NE(0, action_under_test->number_of_tasks());
}

TEST_F(MeroDeleteIndexActionTest, ValidIndexId) {
  EXPECT_CALL(*ptr_mock_request, get_index_id_hi()).Times(1).WillOnce(
      ReturnRef(index_id_str_hi));
  EXPECT_CALL(*ptr_mock_request, get_index_id_lo()).Times(1).WillOnce(
      ReturnRef(index_id_str_lo));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         MeroDeleteIndexActionTest::func_callback_one, this);
  action_under_test->validate_request();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(MeroDeleteIndexActionTest, InvalidIndexId) {
  EXPECT_CALL(*ptr_mock_request, get_index_id_hi()).Times(1).WillOnce(
      ReturnRef(zero_index_id_str_hi));
  EXPECT_CALL(*ptr_mock_request, get_index_id_lo()).Times(1).WillOnce(
      ReturnRef(zero_index_id_str_lo));
  // Delete index should not be called
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_index(_, _, _)).Times(0);
  // Report error Bad request
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/indexes/123-456"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(400, _)).Times(AtLeast(1));
  action_under_test->validate_request();
}

TEST_F(MeroDeleteIndexActionTest, EmptyIndexId) {
  std::string empty_index;
  EXPECT_CALL(*ptr_mock_request, get_index_id_hi()).Times(1).WillOnce(
      ReturnRef(empty_index));
  EXPECT_CALL(*ptr_mock_request, get_index_id_lo()).Times(1).WillOnce(
      ReturnRef(empty_index));
  // Delete index should not be called
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_index(_, _, _)).Times(0);
  // Report error Bad request
  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/indexes/123-456"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(400, _)).Times(AtLeast(1));
  action_under_test->validate_request();
}

TEST_F(MeroDeleteIndexActionTest, DeleteIndex) {
  action_under_test->index_id = index_id;

  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_index(_, _, _)).Times(1);
  action_under_test->delete_index();
  EXPECT_TRUE(action_under_test->clovis_kv_writer != nullptr);
}

TEST_F(MeroDeleteIndexActionTest, DeleteIndexSuccess) {

  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         MeroDeleteIndexActionTest::func_callback_one, this);
  action_under_test->delete_index_successful();
  EXPECT_EQ(1, call_count_one);
}

TEST_F(MeroDeleteIndexActionTest, DeleteIndexFailedIndexMissing) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::missing));

  EXPECT_CALL(*ptr_mock_request, send_response(204, _)).Times(AtLeast(1));

  action_under_test->delete_index_failed();
}

TEST_F(MeroDeleteIndexActionTest, DeleteIndexFailed) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed));

  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/indexes/123-456"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));

  action_under_test->delete_index_failed();
}

TEST_F(MeroDeleteIndexActionTest, DeleteIndexFailedToLaunch) {
  action_under_test->clovis_kv_writer =
      clovis_kvs_writer_factory->mock_clovis_kvs_writer;
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer), get_state())
      .WillRepeatedly(Return(S3ClovisKVSWriterOpState::failed_to_launch));

  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/indexes/123-456"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request,
              set_out_header_value(Eq("Retry-After"), Eq("1"))).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(AtLeast(1));

  action_under_test->delete_index_failed();
}

TEST_F(MeroDeleteIndexActionTest, SendSuccessResponse) {
  EXPECT_CALL(*ptr_mock_request, send_response(204, _)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(MeroDeleteIndexActionTest, SendBadRequestResponse) {
  action_under_test->set_s3_error("BadRequest");

  EXPECT_CALL(*ptr_mock_request, c_get_full_path())
      .WillOnce(Return("/indexes/123-456"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(400, _)).Times(AtLeast(1));

  action_under_test->send_response_to_s3_client();
}
