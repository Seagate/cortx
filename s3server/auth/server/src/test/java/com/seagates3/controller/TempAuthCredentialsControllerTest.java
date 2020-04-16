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
 * Original author:  Shalaka Dharap
 * Original creation date: 05-July-2019
 */

package com.seagates3.controller;

import java.util.Date;
import java.util.Map;
import java.util.TreeMap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import com.novell.ldap.LDAPException;
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.dao.ldap.LDAPUtils;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.AccessKey.AccessKeyStatus;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.service.AccessKeyService;
import com.seagates3.util.KeyGenUtil;

import io.netty.handler.codec.http.HttpResponseStatus;

@PowerMockIgnore({"javax.management.*"}) @RunWith(PowerMockRunner.class)
    @PrepareForTest({AccessKeyService.class, LDAPUtils.class,
                     DAODispatcher.class,    KeyGenUtil.class}) public class
    TempAuthCredentialsControllerTest {

 private
  UserDAO userDAO;
 private
  AccountDAO accountDAO;
 private
  final String ACCOUNT_NAME = "s3test";
 private
  final String ACCOUNT_ID = "12345";
 private
  final Account account;
 private
  final Account nonExistingAccount;
 private
  AccessKey accessKey;

 private
  TempAuthCredentialsController controller;

 public
  TempAuthCredentialsControllerTest() {
    account = new Account();
    nonExistingAccount = new Account();
    account.setId(ACCOUNT_ID);
    account.setName(ACCOUNT_NAME);
  }

  @Before public void setUp() throws Exception {
    Requestor requestor = new Requestor();
    requestor.setAccount(account);

    Map<String, String> requestBody =
        new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
    requestBody.put("AccountName", "s3test");
    requestBody.put("UserName", "s3testuser");
    requestBody.put("Password", "abc");
    requestBody.put("PasswordResetRequired", "false");
    PowerMockito.mockStatic(DAODispatcher.class);
    userDAO = Mockito.mock(UserDAO.class);
    accountDAO = Mockito.mock(AccountDAO.class);

    PowerMockito.doReturn(userDAO)
        .when(DAODispatcher.class, "getResourceDAO", DAOResource.USER);

    PowerMockito.doReturn(accountDAO)
        .when(DAODispatcher.class, "getResourceDAO", DAOResource.ACCOUNT);

    controller = new TempAuthCredentialsController(requestor, requestBody);
  }
  /**
   * Below will test the success scenario
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_success() throws Exception {

    User user = new User();
    user.setAccountName("s3test");
    user.setName("s3testuser");
    user.setId("123");
    Mockito.when(accountDAO.find("s3test")).thenReturn(account);
    Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
    PowerMockito.mockStatic(LDAPUtils.class);
    PowerMockito.doNothing().when(LDAPUtils.class, "bind", Mockito.anyString(),
                                  Mockito.anyString());
    accessKey = new AccessKey();
    accessKey.setId("1");
    accessKey.setSecretKey("seghfgfghfsdf2576TTTgg/djjkk");
    accessKey.setExpiry(new Date(System.currentTimeMillis()).toString());
    accessKey.setStatus(AccessKeyStatus.ACTIVE);
    accessKey.setToken("djdfsjhgs65tgrjjjfTTTHXXhssjqQQW");
    PowerMockito.mockStatic(AccessKeyService.class);
    PowerMockito.when(AccessKeyService.createFedAccessKey(user, 43200))
        .thenReturn(accessKey);
    ServerResponse response = controller.create();
    Assert.assertTrue(
        response.getResponseBody().contains("<UserName>s3testuser</UserName>"));
    Assert.assertEquals(HttpResponseStatus.CREATED,
                        response.getResponseStatus());
  }

  /**
   * Below will test if account doesn't exist then method should return
   * UnAuthorized response
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_fail_accountNotExist()
      throws Exception {
    Mockito.when(accountDAO.find("s3test")).thenReturn(nonExistingAccount);
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }
  /**
   * Below will test if user doesn't exist then method should return
   * UnAuthorized response
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_fail_userNotExist()
      throws Exception {
    Mockito.when(accountDAO.find("s3test")).thenReturn(account);
    User nonExistingUser = new User();
    Mockito.when(userDAO.find("s3test", "s3testuser"))
        .thenReturn(nonExistingUser);
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  /**
   * Below will test if password reset is true and fail the operation
   * accordingly
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_fail_passwordResetTrue()
      throws Exception {
    User user = new User();
    user.setAccountName("s3test");
    user.setName("s3testuser");
    user.setId("123");
    user.setPwdResetRequired("true");
    Mockito.when(accountDAO.find("s3test")).thenReturn(account);
    Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  /**
   * Below will test if duration provided is more than limit for IAM user
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_fail_maxDurationExceed()
      throws Exception {
    User user = new User();
    user.setAccountName("s3test");
    user.setName("s3testuser");
    user.setId("123");
    Mockito.when(accountDAO.find("s3test")).thenReturn(account);
    Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
    controller.requestBody.put("Duration", "129601");
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.NOT_ACCEPTABLE,
                        response.getResponseStatus());
  }

  /**
   * Below will test accessKey null scenario for root user
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_success_rootuser_accessKeyNull()
      throws Exception {

    controller.requestBody.put("UserName", null);
    User user = new User();
    user.setAccountName("s3test");
    user.setName("root");
    user.setId("123");
    Mockito.when(accountDAO.find("s3test")).thenReturn(account);
    Mockito.when(userDAO.find("s3test", "root")).thenReturn(user);
    PowerMockito.mockStatic(LDAPUtils.class);
    PowerMockito.doNothing().when(LDAPUtils.class, "bind", Mockito.anyString(),
                                  Mockito.anyString());
    accessKey = new AccessKey();
    accessKey.setId("1");
    accessKey.setSecretKey("seghfgfghfsdf2576TTTgg/djjkk");
    accessKey.setExpiry(new Date(System.currentTimeMillis()).toString());
    accessKey.setStatus(AccessKeyStatus.ACTIVE);
    accessKey.setToken("djdfsjhgs65tgrjjjfTTTHXXhssjqQQW");
    PowerMockito.mockStatic(AccessKeyService.class);
    PowerMockito.when(AccessKeyService.createFedAccessKey(user, 43200))
        .thenReturn(accessKey);
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                        response.getResponseStatus());
  }

  /**
   * Below method will test NumberFormatException
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_NumberFormatException()
      throws Exception {
    controller.requestBody.put("Duration", "1234a");
    User user = new User();
    user.setAccountName("s3test");
    user.setName("s3testuser");
    user.setId("123");
    Mockito.when(accountDAO.find("s3test")).thenReturn(account);
    Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                        response.getResponseStatus());
  }

  /**
   * Below method will test DataAccessException while fetching record from LDAP
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_DataAccessException()
      throws Exception {

    Mockito.when(accountDAO.find("s3test"))
        .thenThrow(new DataAccessException("failed to search account"));
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                        response.getResponseStatus());
  }

  /**
   * Below will test LDAP exception if occurs during bind with LDAP
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_LDAPException() throws Exception {

    User user = new User();
    user.setAccountName("s3test");
    user.setName("s3testuser");
    user.setId("123");
    Mockito.when(accountDAO.find("s3test")).thenReturn(account);
    Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
    PowerMockito.mockStatic(LDAPUtils.class);
    PowerMockito.doThrow(new LDAPException()).when(
        LDAPUtils.class, "bind", Mockito.anyString(), Mockito.anyString());
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                        response.getResponseStatus());
  }

  /**
   * Below will test LDAP exception if occurs during bind with LDAP because of
   * invalid credentials
   * @throws Exception
   */
  @Test public void getTempAuthCred_create_invalidCredentials_LDAPException()
      throws Exception {

    User user = new User();
    user.setAccountName("s3test");
    user.setName("s3testuser");
    user.setId("123");
    Mockito.when(accountDAO.find("s3test")).thenReturn(account);
    Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
    PowerMockito.mockStatic(LDAPUtils.class);
    LDAPException mockException = Mockito.mock(LDAPException.class);
    Mockito.when(mockException.getResultCode())
        .thenReturn(LDAPException.INVALID_CREDENTIALS);
    PowerMockito.doThrow(mockException).when(
        LDAPUtils.class, "bind", Mockito.anyString(), Mockito.anyString());
    ServerResponse response = controller.create();
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }
}
