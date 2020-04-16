/*
 * COPYRIGHT 2019 SEAGATE LLC
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
 * Original author:  Prashanth Vanaparthy   <prashanth.vanaparthy@seagate.com>
 * Original creation date: 09-July-2019
 */

#include "mero_uri.h"
#include "mock_mero_request_object.h"

using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;
using ::testing::ReturnRef;

class MeroURITEST : public testing::Test {
 protected:
  MeroURITEST() {
    evhtp_request_t *req = NULL;
    EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
    ptr_mock_request =
        std::make_shared<MockMeroRequestObject>(req, evhtp_obj_ptr);
  }

  std::shared_ptr<MockMeroRequestObject> ptr_mock_request;
};

class MeroPathStyleURITEST : public MeroURITEST {};

TEST_F(MeroURITEST, Constructor) {

  MeroURI merouri_test_obj(ptr_mock_request);
  EXPECT_STREQ("", merouri_test_obj.get_key_name().c_str());
  EXPECT_STREQ("", merouri_test_obj.get_object_oid_lo().c_str());
  EXPECT_STREQ("", merouri_test_obj.get_object_oid_hi().c_str());
  EXPECT_STREQ("", merouri_test_obj.get_index_id_lo().c_str());
  EXPECT_STREQ("", merouri_test_obj.get_index_id_hi().c_str());
  EXPECT_EQ(MeroApiType::unsupported, merouri_test_obj.get_mero_api_type());
  EXPECT_EQ(MeroOperationCode::none, merouri_test_obj.get_operation_code());
}

TEST_F(MeroPathStyleURITEST, KeyNameTest) {
  evhtp_request_t *req = NULL;
  EvhtpInterface *evhtp_obj_ptr = new EvhtpWrapper();
  std::shared_ptr<MockMeroRequestObject> ptr_mock_request =
      std::make_shared<MockMeroRequestObject>(req, evhtp_obj_ptr);

  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillOnce(Return("/indexes/123-456/Object-key1"))
      .WillOnce(Return("/indexes/454-999/Object-key2"));

  MeroPathStyleURI meropathstyleone(ptr_mock_request);
  EXPECT_EQ(MeroApiType::keyval, meropathstyleone.get_mero_api_type());
  EXPECT_STREQ("Object-key1", meropathstyleone.get_key_name().c_str());
  EXPECT_STREQ("456", meropathstyleone.get_index_id_lo().c_str());
  EXPECT_STREQ("123", meropathstyleone.get_index_id_hi().c_str());

  MeroPathStyleURI meropathstyletwo(ptr_mock_request);
  EXPECT_EQ(MeroApiType::keyval, meropathstyletwo.get_mero_api_type());
  EXPECT_STREQ("Object-key2", meropathstyletwo.get_key_name().c_str());
  EXPECT_STREQ("999", meropathstyletwo.get_index_id_lo().c_str());
  EXPECT_STREQ("454", meropathstyletwo.get_index_id_hi().c_str());
}

TEST_F(MeroPathStyleURITEST, ObjectOidTest) {

  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillOnce(Return("/objects/123-456"))
      .WillOnce(Return("/objects/454-999/"));

  MeroPathStyleURI meropathstyleone(ptr_mock_request);
  EXPECT_EQ(MeroApiType::object, meropathstyleone.get_mero_api_type());
  EXPECT_STREQ("456", meropathstyleone.get_object_oid_lo().c_str());
  EXPECT_STREQ("123", meropathstyleone.get_object_oid_hi().c_str());

  MeroPathStyleURI meropathstyletwo(ptr_mock_request);
  EXPECT_EQ(MeroApiType::object, meropathstyletwo.get_mero_api_type());
  EXPECT_STREQ("999", meropathstyletwo.get_object_oid_lo().c_str());
  EXPECT_STREQ("454", meropathstyletwo.get_object_oid_hi().c_str());
}

TEST_F(MeroPathStyleURITEST, IndexListTest) {

  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillOnce(Return("/indexes/123-456"))
      .WillOnce(Return("/indexes/454-999/"));

  MeroPathStyleURI meropathstyleone(ptr_mock_request);
  EXPECT_EQ(MeroApiType::index, meropathstyleone.get_mero_api_type());
  EXPECT_STREQ("456", meropathstyleone.get_index_id_lo().c_str());
  EXPECT_STREQ("123", meropathstyleone.get_index_id_hi().c_str());

  MeroPathStyleURI meropathstyletwo(ptr_mock_request);
  EXPECT_EQ(MeroApiType::index, meropathstyletwo.get_mero_api_type());
  EXPECT_STREQ("999", meropathstyletwo.get_index_id_lo().c_str());
  EXPECT_STREQ("454", meropathstyletwo.get_index_id_hi().c_str());
}

TEST_F(MeroPathStyleURITEST, UnsupportedURITest) {

  EXPECT_CALL(*ptr_mock_request, c_get_full_encoded_path())
      .WillOnce(Return("/indexes123/123-456"))
      .WillOnce(Return("/objects123/"));

  MeroPathStyleURI meropathstyleone(ptr_mock_request);
  EXPECT_EQ(MeroApiType::unsupported, meropathstyleone.get_mero_api_type());

  MeroPathStyleURI meropathstyletwo(ptr_mock_request);
  EXPECT_EQ(MeroApiType::unsupported, meropathstyletwo.get_mero_api_type());
}