package com.seagates3.policy;

import java.util.ArrayList;
import java.util.Map;
import java.util.TreeMap;
import org.junit.Before;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.modules.junit4.PowerMockRunner;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import com.amazonaws.auth.policy.Action;
import com.amazonaws.auth.policy.Condition;
import com.amazonaws.auth.policy.actions.S3Actions;
import com.seagates3.authorization.Authorizer;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.BucketPolicyResponseGenerator;
import com.seagates3.util.BinaryUtil;
import com.seagates3.dao.ldap.AccountImpl;
import com.seagates3.model.Account;

import io.netty.handler.codec.http.HttpResponseStatus;

@RunWith(PowerMockRunner.class) @PrepareForTest({BucketPolicyValidator.class})
    @PowerMockIgnore(
        {"javax.management.*"}) public class BucketPolicyValidatorTest {

  String positiveJsonInput = null;
  String negativeJsonInput = null;
  Map<String, String> requestBody = new TreeMap<>();
  PolicyValidator validator = new BucketPolicyValidator();
  AccountImpl mockaccountImpl = null;
  Account account;

  @Before public void setup() {
    mockaccountImpl = Mockito.mock(AccountImpl.class);
    account = new Account();
    account.setId("1234");
  }

  /**
   * Test to validate json- Positive test with correct json
   */
  @Test public void validateBucketPolicy_Positive() {
    String positiveJsonInput =
        "{\r\n" + "  \"Id\": \"Policy1571741920713\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1571741573370\",\r\n" +
        "      \"Resource\": \"arn:aws:s3:::MyBucket/a.txt\",\r\n" +
        "      \"Action\": [\r\n" + "              \"s3:GetObject\"\r\n" +
        "      ],\r\n" + "      \"Effect\": \"Allow\",\r\n" +
        "            \"Principal\": {\r\n" + "        \"AWS\": [\r\n" +
        "          \"*\"\r\n" + "        ]\r\n" + "      }\r\n" + "    }\r\n" +
        "  ]\r\n" + "}";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy",
                    BinaryUtil.encodeToBase64String(positiveJsonInput));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  /**
   * Below will validate json with resource as NULL
   */
  @Test public void validateBucketPolicy_Resource_Null() {
    String negativeJsonInput =
        "{\r\n" + "  \"Id\": \"Policy1572590142744\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1572590138556\",\r\n" +
        "      \"Action\": [\r\n" + "        \"s3:GetBucketAcl\",\r\n" +
        "        \"s3:DeleteBucket\"\r\n" + "      ],\r\n" +
        "      \"Effect\": \"Allow\",\r\n" + "      \r\n" +
        "      \"Principal\": \"*\"\r\n" + "    }\r\n" + "  ]\r\n" +
        "}   \r\n" + "";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy",
                    BinaryUtil.encodeToBase64String(negativeJsonInput));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(new BucketPolicyResponseGenerator()
                            .malformedPolicy("Missing required field Resource")
                            .getResponseBody(),
                        response.getResponseBody());
  }

  /**
   * Below will validate json with NULL Action value
   */
  @Test public void validateBucketPolicy_Action_Null() {
    String inputJson = "{\r\n" + "  \"Id\": \"Policy1572590142744\",\r\n" +
                       "  \"Version\": \"2012-10-17\",\r\n" +
                       "  \"Statement\": [\r\n" + "    {\r\n" +
                       "      \"Sid\": \"Stmt1572590138556\",\r\n" +
                       "      \r\n" + "      \"Effect\": \"Allow\",\r\n" +
                       "      \"Resource\": \"arn:aws:s3:::MyBucket\",\r\n" +
                       "      \"Principal\": \"*\"\r\n" + "    }\r\n" +
                       "  ]\r\n" + "}   \r\n" + "";

    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy", BinaryUtil.encodeToBase64String(inputJson));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(new BucketPolicyResponseGenerator()
                            .malformedPolicy("Missing required field Action")
                            .getResponseBody(),
                        response.getResponseBody());
  }

  /**
   * Below will validate Bucket operation against resource with object
   */
  @Test public void
  validateBucketPolicy_Invalid_Action_Resource_Combination_Check1() {
    String inputJson =
        "{\r\n" + "  \"Id\": \"Policy1572590142744\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1572590138556\",\r\n" +
        "      \"Action\": [\r\n" + "        \"s3:GetBucketAcl\",\r\n" +
        "        \"s3:DeleteBucket\"\r\n" + "      ],\r\n" +
        "      \"Effect\": \"Allow\",\r\n" +
        "      \"Resource\": \"arn:aws:s3:::MyBucket/sample.txt\",\r\n" +
        "      \"Principal\": \"*\"\r\n" + "    }\r\n" + "  ]\r\n" +
        "}   \r\n" + "";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy", BinaryUtil.encodeToBase64String(inputJson));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(
        new BucketPolicyResponseGenerator()
            .malformedPolicy(
                 "Action does not apply to any resource(s) in statement")
            .getResponseBody(),
        response.getResponseBody());
  }

  /**
   * Below will validate Object actions against bucket resource
   */
  @Test public void
  validateBucketPolicy_Invalid_Action_Resource_Combination_Check2() {
    String inputJson =
        "{\r\n" + "  \"Id\": \"Policy1572590142744\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1572590138556\",\r\n" +
        "      \"Action\": [\r\n" + "        \"s3:GetObjectAcl\",\r\n" +
        "        \"s3:GetObject\"\r\n" + "      ],\r\n" +
        "      \"Effect\": \"Allow\",\r\n" +
        "      \"Resource\": \"arn:aws:s3:::MyBucket\",\r\n" +
        "      \"Principal\": \"*\"\r\n" + "    }\r\n" + "  ]\r\n" +
        "}   \r\n" + "";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy", BinaryUtil.encodeToBase64String(inputJson));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(
        new BucketPolicyResponseGenerator()
            .malformedPolicy(
                 "Action does not apply to any resource(s) in statement")
            .getResponseBody(),
        response.getResponseBody());
  }

  /**
   * Below will validate with wild card characters in actions
   */
  @Test public void
  validateBucketPolicy_ActionWithWildCardChars_Positive_Check() {
    String inputJson =
        "{\r\n" + "  \"Id\": \"Policy1571741920713\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1571741573370\",\r\n" +
        "      \"Resource\": \"arn:aws:s3:::MyBucket/a.txt\",\r\n" +
        "      \"Action\": [\r\n" + "              \"s3:GetOb*\"\r\n" +
        "      ],\r\n" + "      \"Effect\": \"Allow\",\r\n" +
        "            \"Principal\": {\r\n" + "        \"AWS\": [\r\n" +
        "          \"*\"\r\n" + "        ]\r\n" + "      }\r\n" + "    }\r\n" +
        "  ]\r\n" + "}";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy", BinaryUtil.encodeToBase64String(inputJson));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  /**
   * Below will validate Object actions against bucket resource along with wild
   * card characters in Actions
   */
  @Test public void
  validateBucketPolicy_ActionWithWildCardChars_Negative_Check() {
    String inputJson =
        "{\r\n" + "  \"Id\": \"Policy1572590142744\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1572590138556\",\r\n" +
        "      \"Action\": [\r\n" + "        \"s3:GetOb*\",\r\n" +
        "        \"s3:GetObject?cl\"\r\n" + "      ],\r\n" +
        "      \"Effect\": \"Allow\",\r\n" +
        "      \"Resource\": \"arn:aws:s3:::MyBucket\",\r\n" +
        "      \"Principal\": \"*\"\r\n" + "    }\r\n" + "  ]\r\n" +
        "}   \r\n" + "";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy", BinaryUtil.encodeToBase64String(inputJson));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(
        new BucketPolicyResponseGenerator()
            .malformedPolicy(
                 "Action does not apply to any resource(s) in statement")
            .getResponseBody(),
        response.getResponseBody());
  }

  /**
   * Below will validate json with missing Effect field
   */
  @Test public void validateBucketPolicy_Effect_Missing() {
    String inputJson =
        "{\r\n" + "  \"Id\": \"Policy1571741920713\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1571741573370\",\r\n" +
        "      \"Resource\": \"arn:aws:s3:::MyBucket/a.txt\",\r\n" +
        "	  \"Action\": [\r\n" +
        "	          \"s3:GetObjec?\"\r\n" + "      ],\r\n" +
        "      \"Principal\": {\r\n" + "        \"AWS\": [\r\n" +
        "          \"*\"\r\n" + "        ]\r\n" + "      }\r\n" + "    }\r\n" +
        "  ]\r\n" + "}";

    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy", BinaryUtil.encodeToBase64String(inputJson));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(new BucketPolicyResponseGenerator()
                            .malformedPolicy("Missing required field Effect")
                            .getResponseBody(),
                        response.getResponseBody());
  }

  /**
   * Below positive test will validate multiple resources in json
   */
  @Test public void validateBucketPolicy_multiple_resources_positive_test() {
    String inputJson =
        "{\r\n" + "  \"Id\": \"Policy1571741920713\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1571741573370\",\r\n" +
        "      \"Resource\": " +
        "\"arn:aws:s3:::MyBucket,arn:aws:s3:::MyBucket/a.txt\",\r\n" +
        "	  \"Action\": [\r\n" +
        "	          \"s3:GetObjec?\"\r\n" + "      ],\r\n" +
        "      \"Effect\": \"Allow\",\r\n" + "	  \"Principal\": {\r\n" +
        "        \"AWS\": [\r\n" + "          \"*\"\r\n" + "        ]\r\n" +
        "      }\r\n" + "    }\r\n" + "  ]\r\n" + "}";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy", BinaryUtil.encodeToBase64String(inputJson));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  /**
   * Below negative test will validate multiple invalid resources in json
   */
  @Test public void validateBucketPolicy_multiple_resources_negative_test() {
    String inputJson =
        "{\r\n" + "  \"Id\": \"Policy1571741920713\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1571741573370\",\r\n" +
        "      \"Resource\": " +
        "\"arn:aws:s3:::MyBucket/b.txt,arn:aws:s3:::MyBucket/a.txt\",\r\n" +
        "	  \"Action\": [\r\n" +
        "	          \"s3:GetBucketAcl\"\r\n" + "      ],\r\n" +
        "      \"Effect\": \"Allow\",\r\n" + "	  \"Principal\": {\r\n" +
        "        \"AWS\": [\r\n" + "          \"*\"\r\n" + "        ]\r\n" +
        "      }\r\n" + "    }\r\n" + "  ]\r\n" + "}";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy", BinaryUtil.encodeToBase64String(inputJson));
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(
        new BucketPolicyResponseGenerator()
            .malformedPolicy(
                 "Action does not apply to any resource(s) in statement")
            .getResponseBody(),
        response.getResponseBody());
  }

  /**
   * Validate StringEquals valid condition
   */
  @Test public void test_validateCondition_StringEquals_success() {

    Condition condition = new Condition()
                              .withType("StringEquals")
                              .withConditionKey("s3:x-amz-acl")
                              .withValues("bucket-owner-read");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.PutObject;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate StringEqualsIfExists valid condition
   */
  @Test public void test_validateCondition_StringEqualsIfExists_success() {

    Condition condition = new Condition()
                              .withType("StringEqualsIfExists")
                              .withConditionKey("s3:x-amz-acl")
                              .withValues("bucket-owner-read");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.PutObject;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate Bool valid condition
   */
  @Test public void test_validateCondition_Bool_success() {

    Condition condition = new Condition()
                              .withType("Bool")
                              .withConditionKey("aws:SecureTransport")
                              .withValues("true");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.PutObject;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate Bool valid condition but with invalid key value pairs
   */
  @Test public void test_validateCondition_Bool_invalidKeyValue_success() {

    Condition condition =
        new Condition().withType("Bool").withConditionKey("aws:").withValues(
            "abc");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.PutObject;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate NumericLessThanEquals valid condition
   */
  @Test public void test_validateCondition_NumericLessThanEquals_success() {

    Condition condition = new Condition()
                              .withType("NumericLessThanEquals")
                              .withConditionKey("s3:max-keys")
                              .withValues("10");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.ListObjects;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate DateLessThan valid condition
   */
  @Test public void test_validateCondition_DateLessThan_success() {

    Condition condition = new Condition()
                              .withType("DateLessThan")
                              .withConditionKey("aws:CurrentTime")
                              .withValues("2013-06-30T00:00:00Z");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.ListObjects;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate StringLike valid condition
   */
  @Test public void test_validateCondition_StringLike_success() {

    Condition condition = new Condition()
                              .withType("StringLike")
                              .withConditionKey("s3:x-amz-acl")
                              .withValues("bucket-owner*");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.PutObject;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate StringLike invalid key - should fail
   */
  @Test public void test_validateCondition_StringLike_invalidKey_fail() {

    Condition condition = new Condition()
                              .withType("StringLike")
                              .withConditionKey("s3:x-")
                              .withValues("bucket-owner*");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.PutObject;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNotNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate StringLike valid condition key - aws:*
   */
  @Test public void test_validateCondition_StringLike_validKeyAWS_success() {

    Condition condition = new Condition()
                              .withType("StringLike")
                              .withConditionKey("aws:garbage")
                              .withValues("qwerty");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.PutObject;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    Assert.assertNull(validator.validateCondition(list, actionList));
  }

  /**
   * Validate StringEquals valid condition
   */
  @Test public void test_validateCondition_invalidCombination_fail() {

    Condition condition = new Condition()
                              .withType("StringEquals")
                              .withConditionKey("s3:max-keys")
                              .withValues("bucket-owner-read");
    ArrayList<Condition> list = new ArrayList<>();
    list.add(condition);
    Action acton = S3Actions.PutObject;
    ArrayList<Action> actionList = new ArrayList<>();
    actionList.add(acton);
    ServerResponse response = validator.validateCondition(list, actionList);
    Assert.assertNotNull(response);
  }

  /**
   *    * validateBucketPolicy_with_CanonicalUser
   *       * @throws Exception
   *          */

  @Test public void validateBucketPolicy_CanonicalUser_Positive()
      throws Exception {
    String positiveJsonInput =
        "{\r\n" + "  \"Id\": \"Policy1572590142744\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1572590138556\",\r\n" +
        "      \"Action\": [\r\n" + "        \r\n" +
        "\"s3:GetBucketPolicy\",\"s3:PutBucketPolicy\",\"s3:" +
        "PutBucketAcl\"\r\n" + "      ],\r\n" +
        "      \"Effect\": \"Allow\",\r\n" +
        "      \"Resource\": \"arn:aws:s3:::MyBucket\",\r\n" +
        "      \"Principal\": " + "{\"CanonicalUser\":" +
        "\"c73bfd36bc80701b1fbf1205498dc34625830069a8bff\"}" + "\r\n" +
        "    }\r\n" + "  ]\r\n" + "}";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy",
                    BinaryUtil.encodeToBase64String(positiveJsonInput));
    PowerMockito.whenNew(AccountImpl.class).withNoArguments().thenReturn(
        mockaccountImpl);
    Mockito.when(mockaccountImpl.findByCanonicalID(
                     "c73bfd36bc80701b1fbf1205498dc34625830069a8bff"))
        .thenReturn(account);
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  /**
 *    * validatePolicy with canonicalUser word in Action
 *       * @throws Exception
 *          */
  @Test public void validateBucketPolicy_CanonicalUser_atWrongPlace()
      throws Exception {
    String positiveJsonInput =
        "{\r\n" + "  \"Id\": \"Policy1572590142744\",\r\n" +
        "  \"Version\": \"2012-10-17\",\r\n" + "  \"Statement\": [\r\n" +
        "    {\r\n" + "      \"Sid\": \"Stmt1572590138556\",\r\n" +
        "      \"Action\": [\r\n" + "        \r\n" +
        "\"s3:CanonicalUserGetBucketPolicy\",\"s3:PutBucketPolicy\",\"s3:" +
        "PutBucketAcl\"\r\n" + "      ],\r\n" +
        "      \"Effect\": \"Allow\",\r\n" +
        "      \"Resource\": \"arn:aws:s3:::MyBucket\",\r\n" +
        "      \"Principal\": " + "{\"CanonicalUser\":" +
        "\"c73bfd36bc80701b1fbf1205498dc34625830069a8bff\"}" + "\r\n" +
        "    }\r\n" + "  ]\r\n" + "}";
    requestBody.put("ClientAbsoluteUri", "/MyBucket");
    requestBody.put("Policy",
                    BinaryUtil.encodeToBase64String(positiveJsonInput));
    PowerMockito.whenNew(AccountImpl.class).withNoArguments().thenReturn(
        mockaccountImpl);
    Mockito.when(mockaccountImpl.findByCanonicalID(
                     "c73bfd36bc80701b1fbf1205498dc34625830069a8bff"))
        .thenReturn(account);
    Authorizer authorizer = new Authorizer();
    ServerResponse response = authorizer.validatePolicy(requestBody);
    Assert.assertEquals(new BucketPolicyResponseGenerator()
                            .malformedPolicy("Policy has invalid action")
                            .getResponseBody(),
                        response.getResponseBody());
  }
}
