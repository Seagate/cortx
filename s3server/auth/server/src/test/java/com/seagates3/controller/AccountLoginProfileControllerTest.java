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
 * Original author: Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 27-June-2019
 */
package com.seagates3.controller;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPEntry;
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.AccountLoginProfileDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.util.KeyGenUtil;
import io.netty.handler.codec.http.HttpResponseStatus;

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

@RunWith(PowerMockRunner.class)
    @PrepareForTest({DAODispatcher.class, KeyGenUtil.class,
                     AccountController.class})
    @PowerMockIgnore(
        {"javax.management.*"}) public class AccountLoginProfileControllerTest {

 private
  AccountLoginProfileController accountLoginProfileController;
 private
  final String ACCOUNT_NAME = "s3test";
 private
  final String ACCOUNT_ID = "12345";
 private
  final Account ACCOUNT;
 private
  AccountDAO mockAccountDao;
 private
  AccountLoginProfileDAO mockLoginProfileDao;
 private
  final String GET_RESOURCE_DAO = "getResourceDAO";
 private
  Map<String, String> requestBodyObj = null;
 private
  Requestor requestorObj = null;
 private
  final LDAPEntry accountEntry;
 private
  final String ACCOUNT_DN = "o=s3test,ou=accounts,dc=s3,dc=seagate,dc=com";

 public
  AccountLoginProfileControllerTest() {

    ACCOUNT = new Account();
    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    LDAPAttributeSet accountAttributeSet = new LDAPAttributeSet();
    accountAttributeSet.add(new LDAPAttribute("objectclass", "Account"));
    accountAttributeSet.add(new LDAPAttribute("o", "s3test"));
    accountAttributeSet.add(new LDAPAttribute("accountid", "98765test"));
    accountAttributeSet.add(new LDAPAttribute("canonicalId", "C12345"));
    accountAttributeSet.add(new LDAPAttribute("mail", "test@seagate.com"));
    accountEntry = new LDAPEntry(ACCOUNT_DN, accountAttributeSet);
  }

  @Before public void setUp() throws Exception {
    PowerMockito.mockStatic(DAODispatcher.class);
    mockAccountDao = Mockito.mock(AccountDAO.class);
    mockLoginProfileDao = Mockito.mock(AccountLoginProfileDAO.class);
    requestorObj = new Requestor();
    requestorObj.setAccount(ACCOUNT);
    requestBodyObj = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
    requestBodyObj.put("AccountName", "s3test");
  }

  @Test public void GetAccountLoginProfile_Sucessful_Api_Response()
      throws Exception {

    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPassword("password");
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    Mockito.when(mockAccountDao.find(ACCOUNT_NAME)).thenReturn(ACCOUNT);
    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));
    ServerResponse response = accountLoginProfileController.list();
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  @Test public void GetAccountLoginProfile_DataAccessException_Response()
      throws Exception {
    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    Mockito.doThrow(new DataAccessException("failed to search user.\n"))
        .when(mockAccountDao)
        .find(ACCOUNT_NAME);
    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));
    ServerResponse response = accountLoginProfileController.list();
    Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                        response.getResponseStatus());
  }

  @Test public void GetAccountLoginProfile_NoSuchEntity_Response()
      throws Exception {
    Account account = new Account();
    account.setId(ACCOUNT_ID);
    account.setName(ACCOUNT_NAME);

    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    Mockito.when(mockAccountDao.find(ACCOUNT_NAME)).thenReturn(account);
    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));
    ServerResponse response = accountLoginProfileController.list();
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
  }

  @Test public void CreateAccountLoginProfile_Sucessful_Api_Response()
      throws Exception {

    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    requestBodyObj.put("Password", "abcdefgh");
    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    PowerMockito.doReturn(mockLoginProfileDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO,
              DAOResource.ACCOUNT_LOGIN_PROFILE);
    Mockito.when(mockAccountDao.find(ACCOUNT_NAME)).thenReturn(ACCOUNT);
    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));
    ServerResponse response = accountLoginProfileController.create();

    String expectedString =
        "<PasswordResetRequired>false</PasswordResetRequired>";
    Assert.assertEquals(HttpResponseStatus.CREATED,
                        response.getResponseStatus());
    Assert.assertTrue(response.getResponseBody().contains(expectedString));
  }

  @Test public void CreateAccountLoginProfile_PasswordResetRequired()
      throws Exception {

    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    requestBodyObj.put("Password", "abcdefgh");
    requestBodyObj.put("PasswordResetRequired", "true");

    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    PowerMockito.doReturn(mockLoginProfileDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO,
              DAOResource.ACCOUNT_LOGIN_PROFILE);
    Mockito.when(mockAccountDao.find(ACCOUNT_NAME)).thenReturn(ACCOUNT);
    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));
    ServerResponse response = accountLoginProfileController.create();

    String expectedString =
        "<PasswordResetRequired>true</PasswordResetRequired>";
    Assert.assertEquals(HttpResponseStatus.CREATED,
                        response.getResponseStatus());
    Assert.assertTrue(response.getResponseBody().contains(expectedString));
  }

  @Test public void CreateAccountLoginProfile_NoPasswordResetRequired()
      throws Exception {

    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    requestBodyObj.put("Password", "abcdefg");
    requestBodyObj.put("PasswordResetRequired", "false");

    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    PowerMockito.doReturn(mockLoginProfileDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO,
              DAOResource.ACCOUNT_LOGIN_PROFILE);
    Mockito.when(mockAccountDao.find(ACCOUNT_NAME)).thenReturn(ACCOUNT);
    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));
    ServerResponse response = accountLoginProfileController.create();

    String expectedString =
        "<PasswordResetRequired>false</PasswordResetRequired>";
    Assert.assertEquals(HttpResponseStatus.CREATED,
                        response.getResponseStatus());
    Assert.assertTrue(response.getResponseBody().contains(expectedString));
  }

  @Test public void CreateAccountLoginProfile_AccountDoesNotExist()
      throws Exception {

    Account account = new Account();
    account.setName(ACCOUNT_NAME);
    requestBodyObj.put("Password", "abcd");
    requestBodyObj.put("PasswordResetRequired", "false");

    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    PowerMockito.doReturn(mockLoginProfileDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO,
              DAOResource.ACCOUNT_LOGIN_PROFILE);
    Mockito.when(mockAccountDao.find(ACCOUNT_NAME)).thenReturn(account);
    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));
    ServerResponse response = accountLoginProfileController.create();

    String expectedString = "<Error><Code>NoSuchEntity</Code>";
    Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                        response.getResponseStatus());
    Assert.assertTrue(response.getResponseBody().contains(expectedString));
  }

  @Test public void CreateAccountLoginProfile_DataAccessException()
      throws Exception {

    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    requestBodyObj.put("Password", "abcdefgh");

    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    PowerMockito.doReturn(mockLoginProfileDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO,
              DAOResource.ACCOUNT_LOGIN_PROFILE);
    Mockito.when(mockAccountDao.find(ACCOUNT_NAME)).thenReturn(ACCOUNT);
    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));
    Mockito.doThrow(new DataAccessException("")).when(mockLoginProfileDao).save(
        ACCOUNT);
    ServerResponse response = accountLoginProfileController.create();
    Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                        response.getResponseStatus());
  }

  @Test public void UpdateUserLoginProfile_Sucessful_Api_Response()
      throws Exception {

    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
    ACCOUNT.setPassword("password");
    ACCOUNT.setPwdResetRequired("false");
    ACCOUNT.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(mockAccountDao)
        .when(DAODispatcher.class, GET_RESOURCE_DAO, DAOResource.ACCOUNT);
    Mockito.when(mockAccountDao.find(ACCOUNT_NAME)).thenReturn(ACCOUNT);
    mockLoginProfileDao = Mockito.mock(AccountLoginProfileDAO.class);
    PowerMockito.doReturn(mockLoginProfileDao)
        .when(DAODispatcher.class, "getResourceDAO",
              DAOResource.ACCOUNT_LOGIN_PROFILE);
    Mockito.doNothing().when(mockLoginProfileDao).save(ACCOUNT);

    accountLoginProfileController = Mockito.spy(
        new AccountLoginProfileController(requestorObj, requestBodyObj));

    ServerResponse response = accountLoginProfileController.update();
    Assert.assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }
}
