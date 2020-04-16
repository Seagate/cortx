package com.seagates3.policy;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import org.junit.BeforeClass;
import org.junit.Test;

public
class S3ArnParserTest {

 private
  static ArnParser arnParser;

  @BeforeClass public static void setUpBeforeClass() throws Exception {
    arnParser = new S3ArnParser();
  }

  @Test public void test_isArnFormatValid_success_s3Bucket() {
    String arn = "arn:aws:s3:::seagatebucket/abc";
    assertTrue(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_success_s3_folder() {
    String arn = "arn:aws:s3:::seagatebucket/dir1/*";
    assertTrue(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_success_s3_bucketOnly() {
    String arn = "arn:aws:s3:::seagatebucket";
    assertTrue(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_fail_s3_region() {
    String arn = "arn:aws:s3:us-east::seagatebucket";
    assertFalse(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_fail_s3_accountid() {
    String arn = "arn:aws:s3::123456789012:seagatebucket";
    assertFalse(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_fail_invalid_fullarn() {
    String arn = "arn:aws:s3:us:123456789012:seagatebucket";
    assertFalse(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_fail_iam_arn() {
    String arn = "arn:aws:iam:::seagatebucket";
    assertFalse(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_fail_randomString() {
    String arn = "qwerty";
    assertFalse(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_fail_emptyString() {
    String arn = "";
    assertFalse(arnParser.isArnFormatValid(arn));
  }

  @Test public void test_isArnFormatValid_fail_nullString() {
    String arn = null;
    assertFalse(arnParser.isArnFormatValid(arn));
  }
}
