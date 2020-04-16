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
 * Original author:  Rajesh Nambiar   <rajesh.nambiarr@seagate.com>
 * Original creation date: 24-Jan-2017
 */

#include "s3_datetime.h"
#include "gtest/gtest.h"

class S3DateTimeTest : public testing::Test {
 protected:
  void SetUp() { s3dateobj_ptr = new S3DateTime(); }
  struct tm &get_point_in_time_test() {
    return s3dateobj_ptr->point_in_time;
  }
  std::string get_format_string_test(std::string format) {
    return s3dateobj_ptr->get_format_string(format);
  }
  void init_with_fmt_test(std::string &time_str, std::string format) {
    s3dateobj_ptr->init_with_fmt(time_str, format);
  }
  void TearDown() { delete s3dateobj_ptr; }

  S3DateTime *s3dateobj_ptr;
};

TEST_F(S3DateTimeTest, Constructor) {
  EXPECT_TRUE(s3dateobj_ptr->is_OK());
  EXPECT_EQ(0, get_point_in_time_test().tm_sec);
  EXPECT_EQ(0, get_point_in_time_test().tm_min);
  EXPECT_EQ(0, get_point_in_time_test().tm_hour);
  EXPECT_EQ(0, get_point_in_time_test().tm_mday);
  EXPECT_EQ(0, get_point_in_time_test().tm_mon);
  EXPECT_EQ(0, get_point_in_time_test().tm_year);
  EXPECT_EQ(0, get_point_in_time_test().tm_wday);
  EXPECT_EQ(0, get_point_in_time_test().tm_yday);
  EXPECT_EQ(0, get_point_in_time_test().tm_isdst);
}

TEST_F(S3DateTimeTest, InitTest) {
  time_t t = time(NULL);
  struct tm point_in_time;
  gmtime_r(&t, &point_in_time);
  s3dateobj_ptr->init_current_time();
  EXPECT_EQ(point_in_time.tm_min, get_point_in_time_test().tm_min);
  EXPECT_EQ(point_in_time.tm_hour, get_point_in_time_test().tm_hour);
  EXPECT_EQ(point_in_time.tm_mday, get_point_in_time_test().tm_mday);
  EXPECT_EQ(point_in_time.tm_mon, get_point_in_time_test().tm_mon);
  EXPECT_EQ(point_in_time.tm_year, get_point_in_time_test().tm_year);
  EXPECT_EQ(point_in_time.tm_wday, get_point_in_time_test().tm_wday);
  EXPECT_EQ(point_in_time.tm_yday, get_point_in_time_test().tm_yday);
  EXPECT_EQ(point_in_time.tm_isdst, get_point_in_time_test().tm_isdst);
}

TEST_F(S3DateTimeTest, InitFmtTest) {
  std::string time_str = "2017-01-25 16:31:01";
  init_with_fmt_test(time_str, "%Y-%m-%d %H:%M:%S");
  EXPECT_EQ(31, get_point_in_time_test().tm_min);
  EXPECT_EQ(16, get_point_in_time_test().tm_hour);
  EXPECT_EQ(25, get_point_in_time_test().tm_mday);
  EXPECT_EQ(0, get_point_in_time_test().tm_mon);
  // Year since 1900
  EXPECT_EQ(117, get_point_in_time_test().tm_year);
  // Days since Sunday (0-6)
  EXPECT_EQ(3, get_point_in_time_test().tm_wday);
  // Days since January 1
  EXPECT_EQ(24, get_point_in_time_test().tm_yday);
  EXPECT_EQ(0, get_point_in_time_test().tm_isdst);

  time_str = "Sunday, 29 January 2017 08:05:01 GMT";
  //"%a, %d %b %Y %H:%M:%S GMT"
  init_with_fmt_test(time_str, S3_GMT_DATETIME_FORMAT);
  EXPECT_EQ(5, get_point_in_time_test().tm_min);
  EXPECT_EQ(8, get_point_in_time_test().tm_hour);
  EXPECT_EQ(29, get_point_in_time_test().tm_mday);
  EXPECT_EQ(0, get_point_in_time_test().tm_mon);
  // Year since 1900
  EXPECT_EQ(117, get_point_in_time_test().tm_year);
  // Days since Sunday (0-6)
  EXPECT_EQ(0, get_point_in_time_test().tm_wday);
  // Days since January 1
  EXPECT_EQ(28, get_point_in_time_test().tm_yday);
  EXPECT_EQ(0, get_point_in_time_test().tm_isdst);

  time_str = "1994-11-05T13:15:30Z";
  init_with_fmt_test(time_str, S3_ISO_DATETIME_FORMAT);
  EXPECT_EQ(15, get_point_in_time_test().tm_min);
  EXPECT_EQ(13, get_point_in_time_test().tm_hour);
  EXPECT_EQ(5, get_point_in_time_test().tm_mday);
  EXPECT_EQ(10, get_point_in_time_test().tm_mon);
  EXPECT_EQ(94, get_point_in_time_test().tm_year);
  EXPECT_EQ(0, get_point_in_time_test().tm_wday);
  EXPECT_EQ(0, get_point_in_time_test().tm_yday);
  EXPECT_EQ(0, get_point_in_time_test().tm_isdst);
}

TEST_F(S3DateTimeTest, InitFmtInvalidTest) {
  std::string time_str = "";
  // S3_GMT_DATETIME_FORMAT -- "%a, %d %b %Y %H:%M:%S GMT"
  init_with_fmt_test(time_str, S3_GMT_DATETIME_FORMAT);
  EXPECT_EQ(0, get_point_in_time_test().tm_min);
  EXPECT_EQ(0, get_point_in_time_test().tm_hour);
  EXPECT_EQ(0, get_point_in_time_test().tm_mday);
  EXPECT_EQ(0, get_point_in_time_test().tm_mon);
  // Year since 1900
  EXPECT_EQ(0, get_point_in_time_test().tm_year);
  // Days since Sunday (0-6)
  EXPECT_EQ(0, get_point_in_time_test().tm_wday);
  // Days since January 1
  EXPECT_EQ(0, get_point_in_time_test().tm_yday);
  EXPECT_EQ(0, get_point_in_time_test().tm_isdst);

  time_str = "";
  // S3_ISO_DATETIME_FORMAT -- "%Y-%m-%dT%T.000Z"
  init_with_fmt_test(time_str, S3_ISO_DATETIME_FORMAT);
  EXPECT_EQ(0, get_point_in_time_test().tm_min);
  EXPECT_EQ(0, get_point_in_time_test().tm_hour);
  EXPECT_EQ(0, get_point_in_time_test().tm_mday);
  EXPECT_EQ(0, get_point_in_time_test().tm_mon);
  // Year since 1900
  EXPECT_EQ(0, get_point_in_time_test().tm_year);
  // Days since Sunday (0-6)
  EXPECT_EQ(0, get_point_in_time_test().tm_wday);
  // Days since January 1
  EXPECT_EQ(0, get_point_in_time_test().tm_yday);
  EXPECT_EQ(0, get_point_in_time_test().tm_isdst);
}

TEST_F(S3DateTimeTest, InitGMTFmtTest) {
  std::string time_str = "Sunday, 29 January 2017 08:05:01 GMT";
  //"%a, %d %b %Y %H:%M:%S GMT"
  s3dateobj_ptr->init_with_gmt(time_str);
  EXPECT_EQ(5, get_point_in_time_test().tm_min);
  EXPECT_EQ(8, get_point_in_time_test().tm_hour);
  EXPECT_EQ(29, get_point_in_time_test().tm_mday);
  EXPECT_EQ(0, get_point_in_time_test().tm_mon);
  // Year since 1900
  EXPECT_EQ(117, get_point_in_time_test().tm_year);
  // Days since Sunday (0-6)
  EXPECT_EQ(0, get_point_in_time_test().tm_wday);
  // Days since January 1
  EXPECT_EQ(28, get_point_in_time_test().tm_yday);
  EXPECT_EQ(0, get_point_in_time_test().tm_isdst);
}

TEST_F(S3DateTimeTest, InitISOFmtTest) {
  std::string time_str = "2017-01-28T13:15:30Z";
  init_with_fmt_test(time_str, S3_ISO_DATETIME_FORMAT);
  EXPECT_EQ(15, get_point_in_time_test().tm_min);
  EXPECT_EQ(13, get_point_in_time_test().tm_hour);
  EXPECT_EQ(28, get_point_in_time_test().tm_mday);
  EXPECT_EQ(0, get_point_in_time_test().tm_mon);
  EXPECT_EQ(117, get_point_in_time_test().tm_year);
  EXPECT_EQ(0, get_point_in_time_test().tm_wday);
  EXPECT_EQ(0, get_point_in_time_test().tm_yday);
  EXPECT_EQ(0, get_point_in_time_test().tm_isdst);
}

TEST_F(S3DateTimeTest, GetISOFmtTest) {
  std::string iso_time = s3dateobj_ptr->get_isoformat_string();
  EXPECT_EQ('Z', iso_time.back());
}

TEST_F(S3DateTimeTest, GetGMTFmtTest) {
  std::string gmt_time = s3dateobj_ptr->get_gmtformat_string();
  EXPECT_EQ("GMT", gmt_time.substr(gmt_time.length() - 3));
}

TEST_F(S3DateTimeTest, GetFmtStringTest) {
  std::string fmt_time = get_format_string_test(S3_GMT_DATETIME_FORMAT);
  EXPECT_EQ("GMT", fmt_time.substr(fmt_time.length() - 3));

  fmt_time = get_format_string_test(S3_ISO_DATETIME_FORMAT);
  EXPECT_EQ('Z', fmt_time.back());
}
