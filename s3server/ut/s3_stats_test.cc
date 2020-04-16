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
 * Original author:  Vinayak Kale <vinayak.kale@seagate.com>
 * Original creation date: 31-October-2016
 */

#include "s3_stats.h"
#include <iostream>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_socket_interface.h"
#include "s3_option.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::_;
using ::testing::InSequence;
using testing::SetErrnoAndReturn;

extern S3Option* g_option_instance;
extern S3Stats* g_stats_instance;

class S3StatsTest : public testing::Test {
 protected:
  S3Stats* s3_stats_under_test;
  MockSocketInterface* mock_socket;

 public:
  virtual void SetUp() {
    g_option_instance = S3Option::get_instance();
    g_option_instance->set_stats_enable(true);

    mock_socket = new MockSocketInterface();
    if (g_stats_instance) {
      s3_stats_fini();
    }
    // socket call will be mocked to return 1, a success status.
    EXPECT_CALL(*mock_socket, socket(_, _, _))
        .WillOnce(Return(1))
        .RetiresOnSaturation();
    // fcntl will be mocked to return 0, a success status.
    EXPECT_CALL(*mock_socket, fcntl(_, _, _))
        .WillRepeatedly(Return(0))
        .RetiresOnSaturation();
    // inet_aton will be mocked to return 1, a success status.
    EXPECT_CALL(*mock_socket, inet_aton(_, _))
        .WillOnce(Return(1))
        .RetiresOnSaturation();

    // close is mocked to return 0.
    EXPECT_CALL(*mock_socket, close(_))
        .WillOnce(Return(0))
        .RetiresOnSaturation();

    s3_stats_under_test = g_stats_instance = S3Stats::get_instance(mock_socket);
    if (g_stats_instance == nullptr) {
      std::cout << "Error initializing g_stats_instance..\n";
    }
  }

  void TearDown() { s3_stats_fini(); }

  static void TearDownTestCase() {
    g_stats_instance = S3Stats::get_instance();
    g_option_instance->set_stats_enable(false);
  }
};

TEST_F(S3StatsTest, Init) {
  ASSERT_TRUE(g_stats_instance != NULL);
  EXPECT_EQ(g_stats_instance->host, g_option_instance->get_statsd_ip_addr());
  EXPECT_EQ(g_stats_instance->port, g_option_instance->get_statsd_port());
  EXPECT_NE(g_stats_instance->sock, -1);
  EXPECT_FALSE(g_stats_instance->metrics_whitelist.empty());

  // test private utility functions
  EXPECT_TRUE(g_stats_instance->is_fequal(1.0, 1.0));
  EXPECT_FALSE(g_stats_instance->is_fequal(1.0, 1.01));
  EXPECT_FALSE(g_stats_instance->is_fequal(1.0, 0.99));

  EXPECT_TRUE(g_stats_instance->is_keyname_valid("bucket1"));
  EXPECT_TRUE(g_stats_instance->is_keyname_valid("bucket.object"));
  EXPECT_FALSE(g_stats_instance->is_keyname_valid("bucket@object"));
  EXPECT_FALSE(g_stats_instance->is_keyname_valid("bucket:object"));
  EXPECT_FALSE(g_stats_instance->is_keyname_valid("bucket|object"));
  EXPECT_FALSE(g_stats_instance->is_keyname_valid("@bucket|object:"));
}

TEST_F(S3StatsTest, Whitelist) {
  EXPECT_TRUE(g_stats_instance->is_allowed_to_publish("uri_to_mero_oid"));
  EXPECT_TRUE(g_stats_instance->is_allowed_to_publish(
      "delete_object_from_clovis_failed"));
  EXPECT_TRUE(g_stats_instance->is_allowed_to_publish("total_request_time"));
  EXPECT_TRUE(g_stats_instance->is_allowed_to_publish(
      "get_bucket_location_request_count"));
  EXPECT_TRUE(
      g_stats_instance->is_allowed_to_publish("get_object_acl_request_count"));
  EXPECT_TRUE(
      g_stats_instance->is_allowed_to_publish("get_service_request_count"));
  EXPECT_FALSE(g_stats_instance->is_allowed_to_publish("xyz"));

  // test loading of non-existing whitelist yaml file
  g_option_instance->set_stats_whitelist_filename(
      "non-existing-whitelist.yaml");
  EXPECT_NE(g_stats_instance->load_whitelist(), 0);
  g_option_instance->set_stats_whitelist_filename(
      "s3stats-whitelist-test.yaml");
}

// Tests that make use of mock_socket interface to check behaviour of s3stats.
TEST_F(S3StatsTest, S3StatsGetInstanceMustFailForInvalidSocket) {
  S3Stats::delete_instance();

  MockSocketInterface* another_mock_socket = new MockSocketInterface();

  // socket call will be mocked to return -1.
  EXPECT_CALL(*another_mock_socket, socket(_, _, _)).WillOnce(Return(-1));

  // Construct a S3Stats instance using invalid socket.
  S3Stats* s3_stats_test = S3Stats::get_instance(another_mock_socket);

  // socket is invalid (-1) so get_instance will return NULL.
  EXPECT_TRUE(s3_stats_test == nullptr);
}

/* Unit test that verifies success return of S3Stats::send using mocked socket
 * calls.*/
TEST_F(S3StatsTest, S3StatsSendMustSucceedIfSocketSendToSucceeds) {
  // sendto will be mocked to return 0, a success status.
  EXPECT_CALL(*mock_socket, sendto(_, _, _, _, _, _)).WillOnce(Return(0));

  // socket is valid so get_instance will not return NULL.
  ASSERT_TRUE(s3_stats_under_test != nullptr);

  // Ensure send return success status.
  EXPECT_TRUE(s3_stats_under_test->send("TestMsg", 1) != -1);
}

/* Unit test that verifies return of S3Stats::get_instance using mocked fcntl
 * call.*/
TEST_F(S3StatsTest, S3StatsGetInstanceMustFailIfSocketFcntlFails) {
  MockSocketInterface* another_mock_socket = new MockSocketInterface();

  // Clean previously created S3Stats.
  S3Stats::delete_instance();

  // socket call will be mocked to return 1, a success status.
  EXPECT_CALL(*another_mock_socket, socket(_, _, _)).WillOnce(Return(1));

  // fcntl will be mocked to return -1, a failure status.
  EXPECT_CALL(*another_mock_socket, fcntl(_, _, _)).WillRepeatedly(Return(-1));

  // Construct a S3Stats instance using invalid socket.
  S3Stats* s3_stats_under_test = S3Stats::get_instance(another_mock_socket);

  // socket is valid but fcntl failed so get_instance will return NULL.
  ASSERT_TRUE(s3_stats_under_test == nullptr);
}

/* Unit test that verifies success return of S3Stats::get_instance using mocked
 * socket
 * calls.*/
TEST_F(S3StatsTest, S3StatsGetInstanceMustFailIfSocketItonFails) {
  MockSocketInterface* another_mock_socket = new MockSocketInterface();

  // Clean previously created S3Stats
  S3Stats::delete_instance();

  // socket call will be mocked to return 1, a success status.
  EXPECT_CALL(*another_mock_socket, socket(_, _, _)).WillOnce(Return(1));

  // fcntl will be mocked to return 0, a success status.
  EXPECT_CALL(*another_mock_socket, fcntl(_, _, _)).WillRepeatedly(Return(0));

  // inet_aton will be mocked to return 0, a failure status.
  EXPECT_CALL(*another_mock_socket, inet_aton(_, _)).WillOnce(Return(0));

  // Construct a S3Stats instance using invalid socket.
  S3Stats* s3_stats_under_test = S3Stats::get_instance(another_mock_socket);

  // socket is valid but inet_aton failed so get_instance will return NULL.
  ASSERT_TRUE(s3_stats_under_test == nullptr);
}

/* Unit test that verifies failure of S3Stats::send using mocked socket
 * calls.*/
TEST_F(S3StatsTest, S3StatsSendMustRetryAndFailIfRetriesFail) {
  // sendto will be mocked to return -1, the failure status.
  EXPECT_CALL(*mock_socket, sendto(_, _, _, _, _, _))
      .Times(3)  // 1 initial attempt + 2 retries.
      .WillRepeatedly(SetErrnoAndReturn(EAGAIN, -1));

  // Construct a S3Stats instance using invalid socket.
  // S3Stats* s3_stats_under_test = S3Stats::get_instance(mock_socket);

  // socket is valid so get_instance will return NULL.
  ASSERT_TRUE(s3_stats_under_test != nullptr);
  EXPECT_TRUE(s3_stats_under_test->send("TestMsg", 2) == -1);
}

/* Unit test that verifies success return of S3Stats::count using mocked socket
 * calls.*/
TEST_F(S3StatsTest, S3StatsCountCallMustNotFailForValidSocketCalls) {
  // sendto will be mocked to return 1, the success status.
  EXPECT_CALL(*mock_socket, sendto(_, _, _, _, _, _)).WillOnce(Return(1));

  // Check return code of count, it internally calls form_and_send_msg, which
  // again calls send.
  EXPECT_NE(s3_stats_under_test->count("internal_error_count", 1), -1);
}

/* Unit test that verifies success return of S3Stats::timing using mocked socket
 * calls.*/
TEST_F(S3StatsTest, S3StatsTimingCallMustNotFailForValidSocketCalls) {
  // sendto will be mocked to return 1, the success status.
  EXPECT_CALL(*mock_socket, sendto(_, _, _, _, _, _)).WillOnce(Return(1));

  // socket is valid so get_instance will return NULL.
  ASSERT_TRUE(s3_stats_under_test != nullptr);

  // Check return code of count, it internally calls form_and_send_msg, which
  // again calls send.
  EXPECT_NE(s3_stats_under_test->timing("internal_error_count", 1), -1);
}

/* Unit test that verifies success return of S3Stats::set_gauge using mocked
 * socket
 * calls.*/
TEST_F(S3StatsTest, S3StatsSetGaugeCallMustNotFailForValidSocketCalls) {
  // sendto will be mocked to return 1, the success status.
  EXPECT_CALL(*mock_socket, sendto(_, _, _, _, _, _)).WillOnce(Return(1));

  // socket is valid so get_instance will return NULL.
  ASSERT_TRUE(s3_stats_under_test != nullptr);

  // Check return code of count, it internally calls form_and_send_msg, which
  // again calls send.
  EXPECT_NE(s3_stats_under_test->set_gauge("internal_error_count", 1), -1);
}

/* Unit test that verifies success return of S3Stats::update_gauge using mocked
 * socket
 * calls.*/
TEST_F(S3StatsTest, S3StatsUpdateGaugeCallMustNotFailForValidSocketCalls) {
  // sendto will be mocked to return 1, the success status.
  EXPECT_CALL(*mock_socket, sendto(_, _, _, _, _, _)).WillOnce(Return(1));

  // socket is valid so get_instance will return NULL.
  ASSERT_TRUE(s3_stats_under_test != nullptr);

  // Check return code of count, it internally calls form_and_send_msg, which
  // again calls send.
  EXPECT_NE(s3_stats_under_test->update_gauge("internal_error_count", 1), -1);
}

/* Unit test that verifies success return of S3Stats::count_unique using mocked
 * socket
 * calls.*/
TEST_F(S3StatsTest, S3StatsCountUniqueCallMustNotFailForValidSocketCalls) {
  // sendto will be mocked to return 1, the success status.
  EXPECT_CALL(*mock_socket, sendto(_, _, _, _, _, _)).WillOnce(Return(1));

  // socket is valid so get_instance will return NULL.
  ASSERT_TRUE(s3_stats_under_test != nullptr);

  // Check return code of count, it internally calls form_and_send_msg, which
  // again calls send.
  EXPECT_NE(s3_stats_under_test->count_unique("internal_error_count", "1"), -1);
}
