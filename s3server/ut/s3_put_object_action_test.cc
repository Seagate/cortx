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

#include <memory>
#include <string>
#include "mock_s3_clovis_wrapper.h"
#include "mock_s3_factory.h"
#include "mock_s3_probable_delete_record.h"
#include "s3_clovis_layout.h"
#include "s3_put_object_action.h"
#include "s3_test_utils.h"
#include "s3_ut_common.h"
#include "s3_m0_uint128_helper.h"

using ::testing::Eq;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Invoke;
using ::testing::_;
using ::testing::ReturnRef;
using ::testing::AtLeast;
using ::testing::DefaultValue;

#define OBJECT_KEY_LENGTH_MORE_THAN_1_KB                                       \
  "vaxsfhwmbuegarlllxjyppqbzewahzdgnykqcbmjnezicblmveddlnvuejvxtjkogpqmnexvpi" \
  "aqufqsxozqzsxxtmmlnukpfnpvtepdxvxqmnwnsceaujybilbqwwhhofxhlbvqeqbcbbagijtg" \
  "emhhfudggqdwpowidypjvxwwjayhghicnwupyritzpoobtwsbhihvzxnqlycpdwomlswvsvvvv" \
  "puhlfvhckzyazsvqvrubobhlrajnytsvhnboykzzdjtvzxsacdjawgztgqgesyxgyugmfwwoxi" \
  "aksrdtbiudlppssyoylbtazbsfvaxcysnetayhkpbtegvdxyowxfofnrkigayqtateseujcngr" \
  "rpfkqypqehvezuoxaqxonlxagmvbbaujjgvnhzvcgasuetslydhvxgttepjmxszczjcvsgrgjk" \
  "hedysupjtrcvtwhhgudpjgtmtrsmusucjtmzqejpfvmzsvjshkzzhtmdowgowvzwiqdhthsdbs" \
  "nxyhapevigrtvhbzpylibfxpfoxiwoyqfyzxskefjththojqgglhmhbzhluyoycxjuwbnkdhms" \
  "stycomxqzvvpvvkzoxhwvmpbwldqcrpsbpwrozymppbnnewmmmrxdxjqthetmfvjpeldndmomd" \
  "udinwjiixsidcxpbacrtlwmgaljzaglsjcbfnsfqyiawieycdvdhatwzcbypcyfsnpeuxmiugs" \
  "desnxhwywgtopqfbkxbpewohuecyneojfeksgukhsxalqxwzitszilqchkdokgaakogpswctds" \
  "uybydalwzznotdvmynxlkomxfeplorgzkvveuslhmmnyeufsjqkzoomzdfvaaaxzykmgcmqdqx" \
  "itjtmpkriwtihthlewlebaiekhzjctlnlwqrgwwhjulqkjfdsxhkxjyrahmmnqvyslxcbcuzob" \
  "mbwxopritmxzjtvnqbszdhfftmfedpxrkiktorpvibtcoatvkvpqvevyhsscoxshpbwjhzfwmv" \
  "ccvbjrnjfkchbrvgctwxhfaqoqhm"

#define CREATE_BUCKET_METADATA                                            \
  do {                                                                    \
    EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), load(_, _)) \
        .Times(AtLeast(1));                                               \
    action_under_test->fetch_bucket_info();                               \
  } while (0)

#define CREATE_OBJECT_METADATA                                              \
  do {                                                                      \
    CREATE_BUCKET_METADATA;                                                 \
    bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(   \
        object_list_indx_oid);                                              \
    bucket_meta_factory->mock_bucket_metadata                               \
        ->set_objects_version_list_index_oid(objects_version_list_idx_oid); \
    EXPECT_CALL(*(object_meta_factory->mock_object_metadata), load(_, _))   \
        .Times(AtLeast(1));                                                 \
    EXPECT_CALL(*(ptr_mock_request), http_verb())                           \
        .WillOnce(Return(S3HttpVerb::PUT));                                 \
    action_under_test->fetch_object_info();                                 \
  } while (0)

class S3PutObjectActionTest : public testing::Test {
 protected:
  S3PutObjectActionTest() {
    S3Option::get_instance()->disable_auth();

    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();

    bucket_name = "seagatebucket";
    object_name = "objname";
    oid = {0x1ffff, 0x1ffff};
    old_object_oid = {0x1ffff, 0x1fff0};
    old_layout_id = 2;
    object_list_indx_oid = {0x11ffff, 0x1ffff};
    objects_version_list_idx_oid = {0x1ffff, 0x11fff};
    zero_oid_idx = {0ULL, 0ULL};

    layout_id =
        S3ClovisLayoutMap::get_instance()->get_best_layout_for_object_size();

    call_count_one = 0;

    ptr_mock_s3_clovis_api = std::make_shared<MockS3Clovis>();
    EXPECT_CALL(*ptr_mock_s3_clovis_api, m0_h_ufid_next(_))
        .WillRepeatedly(Invoke(dummy_helpers_ufid_next));

    async_buffer_factory =
        std::make_shared<MockS3AsyncBufferOptContainerFactory>(
            S3Option::get_instance()->get_libevent_pool_buffer_size());

    ptr_mock_request = std::make_shared<MockS3RequestObject>(
        req, evhtp_obj_ptr, async_buffer_factory);
    EXPECT_CALL(*ptr_mock_request, get_bucket_name())
        .WillRepeatedly(ReturnRef(bucket_name));
    EXPECT_CALL(*ptr_mock_request, get_object_name())
        .Times(AtLeast(1))
        .WillOnce(ReturnRef(object_name));
    EXPECT_CALL(*(ptr_mock_request), get_header_value(StrEq("x-amz-tagging")))
        .WillOnce(Return(""));

    // Owned and deleted by shared_ptr in S3PutObjectAction
    bucket_meta_factory =
        std::make_shared<MockS3BucketMetadataFactory>(ptr_mock_request);

    object_meta_factory = std::make_shared<MockS3ObjectMetadataFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);
    object_meta_factory->set_object_list_index_oid(object_list_indx_oid);

    clovis_writer_factory = std::make_shared<MockS3ClovisWriterFactory>(
        ptr_mock_request, oid, ptr_mock_s3_clovis_api);

    bucket_tag_body_factory_mock = std::make_shared<MockS3PutTagBodyFactory>(
        MockObjectTagsStr, MockRequestId);

    clovis_kvs_writer_factory = std::make_shared<MockS3ClovisKVSWriterFactory>(
        ptr_mock_request, ptr_mock_s3_clovis_api);
    std::map<std::string, std::string> input_headers;
    input_headers["Authorization"] = "1";
    EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
        ReturnRef(input_headers));
    // EXPECT_CALL(*ptr_mock_request, get_header_value(_));
    action_under_test.reset(new S3PutObjectAction(
        ptr_mock_request, ptr_mock_s3_clovis_api, bucket_meta_factory,
        object_meta_factory, clovis_writer_factory,
        bucket_tag_body_factory_mock, clovis_kvs_writer_factory));
  }

  std::shared_ptr<MockS3RequestObject> ptr_mock_request;
  std::shared_ptr<MockS3Clovis> ptr_mock_s3_clovis_api;
  std::shared_ptr<MockS3BucketMetadataFactory> bucket_meta_factory;
  std::shared_ptr<MockS3ObjectMetadataFactory> object_meta_factory;
  std::shared_ptr<MockS3ClovisWriterFactory> clovis_writer_factory;
  std::shared_ptr<MockS3ClovisKVSWriterFactory> clovis_kvs_writer_factory;
  std::shared_ptr<MockS3PutTagBodyFactory> bucket_tag_body_factory_mock;
  std::shared_ptr<MockS3AsyncBufferOptContainerFactory> async_buffer_factory;

  std::shared_ptr<S3PutObjectAction> action_under_test;

  struct m0_uint128 object_list_indx_oid;
  struct m0_uint128 objects_version_list_idx_oid;
  struct m0_uint128 oid, old_object_oid;
  struct m0_uint128 zero_oid_idx;
  int layout_id, old_layout_id;
  std::map<std::string, std::string> request_header_map;

  std::string MockObjectTagsStr;
  std::string MockRequestId;
  int call_count_one;
  std::string bucket_name, object_name;

 public:
  void func_callback_one() { call_count_one += 1; }
};

TEST_F(S3PutObjectActionTest, ConstructorTest) {
  EXPECT_EQ(0, action_under_test->tried_count);
  EXPECT_EQ("uri_salt_", action_under_test->salt);
  EXPECT_NE(0, action_under_test->number_of_tasks());
}

TEST_F(S3PutObjectActionTest, FetchBucketInfo) {
  CREATE_BUCKET_METADATA;
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
}

TEST_F(S3PutObjectActionTest, ValidateObjectKeyLengthPositiveCase) {
  EXPECT_CALL(*ptr_mock_request, get_object_name())
      .WillOnce(ReturnRef(object_name));

  action_under_test->validate_object_key_len();

  EXPECT_STRNE("KeyTooLongError",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, ValidateObjectKeyLengthNegativeCase) {
  EXPECT_CALL(*ptr_mock_request, get_object_name())
      .WillOnce(ReturnRef(OBJECT_KEY_LENGTH_MORE_THAN_1_KB));

  action_under_test->validate_object_key_len();

  EXPECT_STREQ("KeyTooLongError",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, ValidateRequestTags) {
  call_count_one = 0;
  request_header_map.clear();
  request_header_map["x-amz-tagging"] = "key1=value1&key2=value2";
  EXPECT_CALL(*ptr_mock_request, get_header_value(_))
      .WillOnce(Return("key1=value1&key2=value2"));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutObjectActionTest::func_callback_one, this);

  action_under_test->validate_x_amz_tagging_if_present();

  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutObjectActionTest, VaidateEmptyTags) {
  request_header_map.clear();
  request_header_map["x-amz-tagging"] = "";
  EXPECT_CALL(*ptr_mock_request, get_header_value(_)).WillOnce(Return(""));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->clear_tasks();

  action_under_test->validate_x_amz_tagging_if_present();
  EXPECT_STREQ("InvalidTagError",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, VaidateInvalidTagsCase1) {
  request_header_map.clear();
  request_header_map["x-amz-tagging"] = "key1=";
  EXPECT_CALL(*ptr_mock_request, get_header_value(_)).WillOnce(Return("key1="));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->clear_tasks();

  action_under_test->validate_x_amz_tagging_if_present();
  EXPECT_STREQ("InvalidTagError",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, VaidateInvalidTagsCase2) {
  request_header_map.clear();
  request_header_map["x-amz-tagging"] = "key1=value1&=value2";
  EXPECT_CALL(*ptr_mock_request, get_header_value(_))
      .WillOnce(Return("key1=value1&=value2"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->clear_tasks();

  action_under_test->validate_x_amz_tagging_if_present();
  EXPECT_STREQ("InvalidTagError",
               action_under_test->get_s3_error_code().c_str());
}

// Count of tags exceding limit.

TEST_F(S3PutObjectActionTest, VaidateInvalidTagsCase3) {
  request_header_map.clear();
  request_header_map["x-amz-tagging"] =
      "key1=value1&key2=value2&key3=value3&key4=value4&key5=value5&key6=value6&"
      "key7=value7&key8=value8&key9=value9&key10=value10&key11=value11";
  EXPECT_CALL(*ptr_mock_request, get_header_value(_)).WillOnce(Return(
      "key1=value1&key2=value2&key3=value3&key4=value4&key5=value5&key6=value6&"
      "key7=value7&key8=value8&key9=value9&key10=value10&key11=value11"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);
  action_under_test->clear_tasks();

  action_under_test->validate_x_amz_tagging_if_present();
  EXPECT_STREQ("InvalidTagError",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, VaidateInvalidTagsCase4) {
  request_header_map.clear();
  request_header_map["x-amz-tagging"] = "Key=seag`ate&Value=marketing";
  EXPECT_CALL(*ptr_mock_request, get_header_value(_))
      .WillOnce(Return("Key=seag`ate&Value=marketing"));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->clear_tasks();
  action_under_test->validate_x_amz_tagging_if_present();

  EXPECT_STREQ("InvalidTagError",
               action_under_test->get_s3_error_code().c_str());
}

// Case 1 : URL encoded
TEST_F(S3PutObjectActionTest, VaidateSpecialCharTagsCase1) {
  call_count_one = 0;
  request_header_map.clear();
  const char *x_amz_tagging = "ke%2by=valu%2be&ke%2dy=valu%2de";  // '+' & '-'
  request_header_map["x-amz-tagging"] = x_amz_tagging;
  EXPECT_CALL(*ptr_mock_request, get_header_value(_))
      .WillOnce(Return(x_amz_tagging));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutObjectActionTest::func_callback_one, this);

  action_under_test->validate_x_amz_tagging_if_present();

  EXPECT_EQ(1, call_count_one);
}

// Case 2 : Non URL encoded
TEST_F(S3PutObjectActionTest, VaidateSpecialCharTagsCase2) {
  call_count_one = 0;
  request_header_map.clear();
  request_header_map["x-amz-tagging"] = "ke+y=valu+e&ke-y=valu-e";
  EXPECT_CALL(*ptr_mock_request, get_header_value(_))
      .WillOnce(Return("ke+y=valu+e&ke-y=valu-e"));
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutObjectActionTest::func_callback_one, this);

  action_under_test->validate_x_amz_tagging_if_present();

  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutObjectActionTest, FetchObjectInfoWhenBucketNotPresent) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::missing));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("NoSuchBucket", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata == NULL);
}

TEST_F(S3PutObjectActionTest, FetchObjectInfoWhenBucketFailed) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata == NULL);
}

TEST_F(S3PutObjectActionTest, FetchObjectInfoWhenBucketFailedTolaunch) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .WillRepeatedly(Return(S3BucketMetadataState::failed_to_launch));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->fetch_bucket_info_failed();

  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
  EXPECT_TRUE(action_under_test->bucket_metadata != NULL);
  EXPECT_TRUE(action_under_test->object_metadata == NULL);
}

/*   TODO metadata fetch moved to s3_object_action class,
//     so these test will be moved there
TEST_F(S3PutObjectActionTest,
       FetchObjectInfoWhenBucketPresentAndObjIndexAbsent) {
  CREATE_BUCKET_METADATA;

  EXPECT_CALL(*(bucket_meta_factory->mock_bucket_metadata), get_state())
      .Times(AtLeast(1))
      .WillOnce(Return(S3BucketMetadataState::present));
  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      zero_oid_idx);

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), load(_, _))
      .Times(0);

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
ACTION_TASK_ADD_OBJPTR(action_under_test,
S3PutObjectActionTest::func_callback_one, this);

  action_under_test->fetch_bucket_info_successful();

  EXPECT_EQ(1, call_count_one);
  EXPECT_TRUE(action_under_test->bucket_metadata != nullptr);
  EXPECT_TRUE(action_under_test->object_metadata == nullptr);
}
*/
TEST_F(S3PutObjectActionTest, FetchObjectInfoReturnedFoundShouldHaveNewOID) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillOnce(Return(S3ObjectMetadataState::present));

  struct m0_uint128 old_oid = {0x1ffff, 0x1ffff};
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_oid())
      .Times(1)
      .WillOnce(Return(old_oid));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_layout_id())
      .Times(1)
      .WillOnce(Return(layout_id));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutObjectActionTest::func_callback_one, this);

  // Remember default generated OID
  struct m0_uint128 oid_before_regen = action_under_test->new_object_oid;
  action_under_test->fetch_object_info_success();

  EXPECT_EQ(1, call_count_one);
  EXPECT_OID_NE(zero_oid_idx, action_under_test->old_object_oid);
  EXPECT_OID_NE(oid_before_regen, action_under_test->new_object_oid);
}

TEST_F(S3PutObjectActionTest, FetchObjectInfoReturnedNotFoundShouldUseURL2OID) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::missing));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutObjectActionTest::func_callback_one, this);

  // Remember default generated OID
  struct m0_uint128 oid_before_regen = action_under_test->new_object_oid;
  action_under_test->fetch_object_info_success();

  EXPECT_EQ(1, call_count_one);
  EXPECT_OID_EQ(zero_oid_idx, action_under_test->old_object_oid);
  EXPECT_OID_EQ(oid_before_regen, action_under_test->new_object_oid);
}

TEST_F(S3PutObjectActionTest, FetchObjectInfoReturnedInvalidStateReportsError) {
  CREATE_OBJECT_METADATA;

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), get_state())
      .WillRepeatedly(Return(S3ObjectMetadataState::failed));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutObjectActionTest::func_callback_one, this);

  // Remember default generated OID
  struct m0_uint128 oid_before_regen = action_under_test->new_object_oid;

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->fetch_object_info_success();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_EQ(0, call_count_one);
  EXPECT_OID_EQ(zero_oid_idx, action_under_test->old_object_oid);
  EXPECT_OID_EQ(oid_before_regen, action_under_test->new_object_oid);
}

TEST_F(S3PutObjectActionTest, CreateObjectFirstAttempt) {
  EXPECT_CALL(*ptr_mock_request, get_content_length()).Times(1).WillOnce(
      Return(1024));
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              create_object(_, _, _)).Times(1);
  action_under_test->create_object();
  EXPECT_TRUE(action_under_test->clovis_writer != nullptr);
}

TEST_F(S3PutObjectActionTest, CreateObjectSecondAttempt) {
  EXPECT_CALL(*ptr_mock_request, get_content_length()).Times(2).WillRepeatedly(
      Return(1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              create_object(_, _, _)).Times(2);
  action_under_test->create_object();
  action_under_test->tried_count = 1;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), set_oid(_))
      .Times(1);
  action_under_test->create_object();
  EXPECT_TRUE(action_under_test->clovis_writer != nullptr);
}

TEST_F(S3PutObjectActionTest, CreateObjectFailedTestWhileShutdown) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->create_object_failed();
  EXPECT_TRUE(action_under_test->clovis_writer == NULL);
  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3PutObjectActionTest, CreateObjectFailedWithCollisionExceededRetry) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_state())
      .Times(1)
      .WillOnce(Return(S3ClovisWriterOpState::exists));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->tried_count = MAX_COLLISION_RETRY_COUNT + 1;
  action_under_test->create_object_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, CreateObjectFailedWithCollisionRetry) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_state())
      .Times(1)
      .WillOnce(Return(S3ClovisWriterOpState::exists));
  EXPECT_CALL(*ptr_mock_request, get_content_length()).Times(1).WillOnce(
      Return(1024));

  action_under_test->tried_count = MAX_COLLISION_RETRY_COUNT - 1;
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), set_oid(_))
      .Times(1);
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              create_object(_, _, _)).Times(1);

  action_under_test->create_object_failed();
}

TEST_F(S3PutObjectActionTest, CreateObjectFailedTest) {
  EXPECT_CALL(*ptr_mock_request, get_content_length()).Times(1).WillOnce(
      Return(1024));
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              create_object(_, _, _)).Times(1);
  action_under_test->create_object();

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisWriterOpState::failed));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(_, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->create_object_failed();
  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, CreateObjectFailedToLaunchTest) {
  EXPECT_CALL(*ptr_mock_request, get_content_length()).Times(1).WillOnce(
      Return(1024));
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              create_object(_, _, _)).Times(1);
  action_under_test->create_object();

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_state())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(S3ClovisWriterOpState::failed_to_launch));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->create_object_failed();
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, CreateNewOidTest) {
  struct m0_uint128 old_oid = action_under_test->new_object_oid;

  action_under_test->create_new_oid(old_oid);

  EXPECT_OID_NE(old_oid, action_under_test->new_object_oid);
}

TEST_F(S3PutObjectActionTest, InitiateDataStreamingForZeroSizeObject) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*ptr_mock_request, get_content_length()).Times(1).WillOnce(
      Return(0));

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutObjectActionTest::func_callback_one, this);

  action_under_test->initiate_data_streaming();
  EXPECT_FALSE(action_under_test->write_in_progress);
  EXPECT_EQ(1, call_count_one);
}

TEST_F(S3PutObjectActionTest, InitiateDataStreamingExpectingMoreData) {
  EXPECT_CALL(*ptr_mock_request, get_content_length()).Times(1).WillOnce(
      Return(1024));
  EXPECT_CALL(*ptr_mock_request, has_all_body_content()).Times(1).WillOnce(
      Return(false));
  EXPECT_CALL(*ptr_mock_request, listen_for_incoming_data(_, _)).Times(1);

  action_under_test->initiate_data_streaming();

  EXPECT_FALSE(action_under_test->write_in_progress);
}

TEST_F(S3PutObjectActionTest, InitiateDataStreamingWeHaveAllData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*ptr_mock_request, get_content_length())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(1024));
  EXPECT_CALL(*ptr_mock_request, has_all_body_content()).Times(1).WillOnce(
      Return(true));
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  action_under_test->initiate_data_streaming();

  EXPECT_TRUE(action_under_test->write_in_progress);
}

// Write not in progress and we have all the data
TEST_F(S3PutObjectActionTest, ConsumeIncomingShouldWriteIfWeAllData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  action_under_test->consume_incoming_content();

  EXPECT_TRUE(action_under_test->write_in_progress);
}

// Write not in progress, expecting more, we have exact what we can write
TEST_F(S3PutObjectActionTest, ConsumeIncomingShouldWriteIfWeExactData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id)));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);

  action_under_test->consume_incoming_content();

  EXPECT_TRUE(action_under_test->write_in_progress);
}

// Write not in progress, expecting more, we have more than we can write
TEST_F(S3PutObjectActionTest, ConsumeIncomingShouldWriteIfWeHaveMoreData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id) +
           1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);

  action_under_test->consume_incoming_content();

  EXPECT_TRUE(action_under_test->write_in_progress);
}

// we are expecting more data
TEST_F(S3PutObjectActionTest, ConsumeIncomingShouldPauseWhenWeHaveTooMuch) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id) *
           S3Option::get_instance()->get_read_ahead_multiple() * 2));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  action_under_test->consume_incoming_content();

  EXPECT_TRUE(action_under_test->write_in_progress);
}

TEST_F(S3PutObjectActionTest,
       ConsumeIncomingShouldNotWriteWhenWriteInprogress) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->write_in_progress = true;
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .Times(1)
      .WillOnce(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id) *
           2));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(0);

  action_under_test->consume_incoming_content();
}

TEST_F(S3PutObjectActionTest, WriteObjectShouldWriteContentAndMarkProgress) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(1024));

  action_under_test->write_object(async_buffer_factory->get_mock_buffer());

  EXPECT_TRUE(action_under_test->write_in_progress);
}

TEST_F(S3PutObjectActionTest, WriteObjectFailedShouldUndoMarkProgress) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->write_in_progress = true;
  action_under_test->new_oid_str = S3M0Uint128Helper::to_string(oid);
  MockS3ProbableDeleteRecord *prob_rec = new MockS3ProbableDeleteRecord(
      action_under_test->new_oid_str, {0ULL, 0ULL}, "abc_obj", oid, layout_id,
      object_list_indx_oid, objects_version_list_idx_oid,
      "" /* Version does not exists yet */, false /* force_delete */,
      false /* is_multipart */, {0ULL, 0ULL});
  action_under_test->new_probable_del_rec.reset(prob_rec);
  // expectations for mark_new_oid_for_deletion()
  EXPECT_CALL(*prob_rec, set_force_delete(true)).Times(1);
  EXPECT_CALL(*prob_rec, to_json()).Times(1);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _)).Times(1);

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_state())
      .Times(1)
      .WillOnce(Return(S3ClovisWriterOpState::failed));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->write_object_failed();

  EXPECT_STREQ("InternalError", action_under_test->get_s3_error_code().c_str());
  EXPECT_FALSE(action_under_test->write_in_progress);
}

TEST_F(S3PutObjectActionTest, WriteObjectFailedDuetoEntityOpenFailure) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->write_in_progress = true;
  action_under_test->new_oid_str = S3M0Uint128Helper::to_string(oid);
  MockS3ProbableDeleteRecord *prob_rec = new MockS3ProbableDeleteRecord(
      action_under_test->new_oid_str, {0ULL, 0ULL}, "abc_obj", oid, layout_id,
      object_list_indx_oid, objects_version_list_idx_oid,
      "" /* Version does not exists yet */, false /* force_delete */,
      false /* is_multipart */, {0ULL, 0ULL});
  action_under_test->new_probable_del_rec.reset(prob_rec);
  // expectations for mark_new_oid_for_deletion()
  EXPECT_CALL(*prob_rec, set_force_delete(true)).Times(1);
  EXPECT_CALL(*prob_rec, to_json()).Times(1);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              put_keyval(_, _, _, _, _)).Times(1);

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_state())
      .Times(1)
      .WillOnce(Return(S3ClovisWriterOpState::failed_to_launch));
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->write_object_failed();

  EXPECT_FALSE(action_under_test->write_in_progress);
  EXPECT_STREQ("ServiceUnavailable",
               action_under_test->get_s3_error_code().c_str());
}

TEST_F(S3PutObjectActionTest, WriteObjectSuccessfulWhileShuttingDown) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);
  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->write_in_progress = true;

  action_under_test->write_object_successful();

  S3Option::get_instance()->set_is_s3_shutting_down(false);

  EXPECT_FALSE(action_under_test->write_in_progress);
}

// We have all the data: Freezed
TEST_F(S3PutObjectActionTest, WriteObjectSuccessfulShouldWriteStateAllData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->write_in_progress = true;

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(1024));
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);

  action_under_test->write_object_successful();

  EXPECT_TRUE(action_under_test->write_in_progress);
}

// We have some data but not all and exact to write
TEST_F(S3PutObjectActionTest,
       WriteObjectSuccessfulShouldWriteWhenExactWritableSize) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->write_in_progress = true;

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id)));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->write_object_successful();

  EXPECT_TRUE(action_under_test->write_in_progress);
}

// We have some data but not all and but have more to write
TEST_F(S3PutObjectActionTest,
       WriteObjectSuccessfulShouldWriteSomeDataWhenMoreData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->write_in_progress = true;

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id) +
           1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(1);
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->write_object_successful();

  EXPECT_TRUE(action_under_test->write_in_progress);
}

// We have some data but not all and but have more to write
TEST_F(S3PutObjectActionTest, WriteObjectSuccessfulDoNextStepWhenAllIsWritten) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->write_in_progress = true;

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(true));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(0);

  // Mock out the next calls on action.
  action_under_test->clear_tasks();
  ACTION_TASK_ADD_OBJPTR(action_under_test,
                         S3PutObjectActionTest::func_callback_one, this);

  action_under_test->write_object_successful();

  EXPECT_EQ(1, call_count_one);
  EXPECT_FALSE(action_under_test->write_in_progress);
}

// We expecting more and not enough to write
TEST_F(S3PutObjectActionTest, WriteObjectSuccessfulShouldRestartReadingData) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;
  action_under_test->_set_layout_id(layout_id);

  // mock mark progress
  action_under_test->write_in_progress = true;

  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), is_freezed())
      .WillRepeatedly(Return(false));
  // S3Option::get_instance()->get_clovis_write_payload_size() = 1048576 * 1
  // S3_READ_AHEAD_MULTIPLE: 1
  EXPECT_CALL(*async_buffer_factory->get_mock_buffer(), get_content_length())
      .WillRepeatedly(Return(
           S3Option::get_instance()->get_clovis_write_payload_size(layout_id) -
           1024));

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer),
              write_content(_, _, _)).Times(0);

  action_under_test->write_object_successful();

  EXPECT_FALSE(action_under_test->write_in_progress);
}

TEST_F(S3PutObjectActionTest, SaveMetadata) {
  CREATE_BUCKET_METADATA;
  bucket_meta_factory->mock_bucket_metadata->set_object_list_index_oid(
      object_list_indx_oid);

  action_under_test->new_object_metadata =
      object_meta_factory->mock_object_metadata;
  action_under_test->new_oid_str = S3M0Uint128Helper::to_string(oid);

  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;

  EXPECT_CALL(*ptr_mock_request, get_data_length_str()).Times(1).WillOnce(
      Return("1024"));
  EXPECT_CALL(*ptr_mock_request, get_header_value("content-md5"))
      .Times(1)
      .WillOnce(Return(""));

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              reset_date_time_to_current()).Times(AtLeast(1));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              set_content_length(Eq("1024"))).Times(AtLeast(1));
  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_content_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              set_md5(Eq("abcd1234abcd"))).Times(AtLeast(1));
  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), set_tags(_))
      .Times(AtLeast(1));

  std::map<std::string, std::string> input_headers;
  input_headers["x-amz-meta-item-1"] = "1024";
  input_headers["x-amz-meta-item-2"] = "s3.seagate.com";

  EXPECT_CALL(*ptr_mock_request, get_in_headers_copy()).Times(1).WillOnce(
      ReturnRef(input_headers));

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata),
              add_user_defined_attribute(Eq("x-amz-meta-item-1"), Eq("1024")))
      .Times(AtLeast(1));
  EXPECT_CALL(
      *(object_meta_factory->mock_object_metadata),
      add_user_defined_attribute(Eq("x-amz-meta-item-2"), Eq("s3.seagate.com")))
      .Times(AtLeast(1));

  EXPECT_CALL(*(object_meta_factory->mock_object_metadata), save(_, _))
      .Times(AtLeast(1));

  action_under_test->save_metadata();
}

TEST_F(S3PutObjectActionTest, SendResponseWhenShuttingDown) {
  S3Option::get_instance()->set_is_s3_shutting_down(true);

  EXPECT_CALL(*ptr_mock_request, pause()).Times(1);
  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request,
              set_out_header_value(Eq("Retry-After"), Eq("1"))).Times(1);
  EXPECT_CALL(*ptr_mock_request, send_response(503, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  // send_response_to_s3_client is called in check_shutdown_and_rollback
  action_under_test->check_shutdown_and_rollback();

  S3Option::get_instance()->set_is_s3_shutting_down(false);
}

TEST_F(S3PutObjectActionTest, SendErrorResponse) {
  action_under_test->set_s3_error("InternalError");

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(false)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3PutObjectActionTest, SendSuccessResponse) {
  action_under_test->clovis_writer = clovis_writer_factory->mock_clovis_writer;

  // Simulate success
  action_under_test->s3_put_action_state =
      S3PutObjectActionState::metadataSaved;

  // expectations for remove_new_oid_probable_record()
  action_under_test->new_oid_str = S3M0Uint128Helper::to_string(oid);
  EXPECT_CALL(*(clovis_kvs_writer_factory->mock_clovis_kvs_writer),
              delete_keyval(_, _, _, _)).Times(1);

  EXPECT_CALL(*(clovis_writer_factory->mock_clovis_writer), get_content_md5())
      .Times(AtLeast(1))
      .WillOnce(Return("abcd1234abcd"));

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(200, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->send_response_to_s3_client();
}

TEST_F(S3PutObjectActionTest, SendFailedResponse) {
  action_under_test->set_s3_error("InternalError");
  action_under_test->s3_put_action_state =
      S3PutObjectActionState::validationFailed;

  EXPECT_CALL(*ptr_mock_request, set_out_header_value(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, send_response(500, _)).Times(AtLeast(1));
  EXPECT_CALL(*ptr_mock_request, resume(_)).Times(1);

  action_under_test->send_response_to_s3_client();
}

