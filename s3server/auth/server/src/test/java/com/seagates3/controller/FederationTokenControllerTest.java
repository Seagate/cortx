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
 * Original creation date: 21-May-2019
 */

package com.seagates3.controller;

import static org.junit.Assert.assertEquals;

import java.util.HashMap;
import java.util.Map;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.service.AccessKeyService;
import com.seagates3.service.UserService;

import io.netty.handler.codec.http.HttpResponseStatus;

@RunWith(PowerMockRunner.class) @PowerMockIgnore({"javax.management.*"})
    @PrepareForTest(
        {UserService.class,
         AccessKeyService.class}) public class FederationTokenControllerTest {
 private
  Account account;
 private
  static final String ACCOUNT_NAME = "s3test";
 private
  static final String ACCOUNT_ID = "12345";
 private
  Map<String, String> requestBody;
 private
  static final String NAME = "FederationUser";
 private
  User mockUser;
 private
  AccessKey mockAccessKey;
 private
  static final String ID = "FederationUserId";
 private
  static final String ACCESS_KEY_ID = "K_MRFEMqQoGHXwXTQK";
 private
  static final String ACCESS_KEY_EXPIRY = "";
 private
  static final String ACCESS_KEY_TOKEN = "";
 private
  static final String ACCESS_KEY_SECRET = "jcEaRvN7Dd3uSuB5QVS+1B74HQHvfsabcd";
 private
  FederationTokenController mockController;
 private
  Requestor requestor = null;
 private
  static final String DURATION = "12345";

  @Before public void setup() {
    account = new Account();
    account.setId(ACCOUNT_ID);
    account.setName(ACCOUNT_NAME);
    requestBody = new HashMap<String, String>();
    requestBody.put("Name", NAME);
    requestBody.put("DurationSeconds", DURATION);
    mockUser = Mockito.mock(User.class);
    mockAccessKey = Mockito.mock(AccessKey.class);
    requestor = new Requestor();
    requestor.setAccount(account);
    mockController =
        Mockito.spy(new FederationTokenController(requestor, requestBody));
  }

  /**
   * Below method will check if valid permanent user details and access key
   * provided then it should successfully create federation user
   * and send CREATED response back.
   * @throws DataAccessException
   */

  @Test public void federationTokenController_create()
      throws DataAccessException {
    Mockito.when(mockUser.getId()).thenReturn(ID);
    Mockito.when(mockUser.getName()).thenReturn(NAME);
    PowerMockito.mockStatic(UserService.class);
    PowerMockito.when(UserService.createFederationUser(requestor.getAccount(),
                                                       requestBody.get("Name")))
        .thenReturn(mockUser);
    Mockito.when(mockAccessKey.getId()).thenReturn(ACCESS_KEY_ID);
    Mockito.when(mockAccessKey.getExpiry()).thenReturn(ACCESS_KEY_EXPIRY);
    Mockito.when(mockAccessKey.getSecretKey()).thenReturn(ACCESS_KEY_SECRET);
    Mockito.when(mockAccessKey.getToken()).thenReturn(ACCESS_KEY_TOKEN);
    PowerMockito.mockStatic(AccessKeyService.class);
    int time = Integer.parseInt(requestBody.get("DurationSeconds"));
    PowerMockito.when(AccessKeyService.createFedAccessKey(mockUser, time))
        .thenReturn(mockAccessKey);
    ServerResponse response = mockController.create();
    assertEquals(HttpResponseStatus.CREATED, response.getResponseStatus());
  }

  /**
   *Below Test will check if AccessKey is NULL then method should throw
   *INTERNAL_SERVER_ERROR
   *@throws DataAccessException
   */

  @Test public void federationTokenController_create_accessKeyNull()
      throws DataAccessException {
    PowerMockito.mockStatic(UserService.class);
    PowerMockito.when(UserService.createFederationUser(requestor.getAccount(),
                                                       requestBody.get("Name")))
        .thenReturn(mockUser);
    PowerMockito.mockStatic(AccessKeyService.class);
    int time = Integer.parseInt(requestBody.get("DurationSeconds"));
    PowerMockito.when(AccessKeyService.createFedAccessKey(mockUser, time))
        .thenReturn(null);
    ServerResponse response = mockController.create();
    assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                 response.getResponseStatus());
  }

  /**
   * Below Test should assign default value to 'Duration' if not present in
   * request body
   * @throws DataAccessException
   */
  @Test public void
  federationTokenController_create_durationMissingRequestBody()
      throws DataAccessException {
    Map<String, String> requestBody2 = new HashMap<>();
    requestBody2.put("Name", NAME);
    Mockito.when(mockUser.getId()).thenReturn(ID);
    Mockito.when(mockUser.getName()).thenReturn(NAME);
    PowerMockito.mockStatic(UserService.class);
    PowerMockito.when(UserService.createFederationUser(requestor.getAccount(),
                                                       requestBody.get("Name")))
        .thenReturn(mockUser);
    Mockito.when(mockAccessKey.getId()).thenReturn(ACCESS_KEY_ID);
    Mockito.when(mockAccessKey.getExpiry()).thenReturn(ACCESS_KEY_EXPIRY);
    Mockito.when(mockAccessKey.getSecretKey()).thenReturn(ACCESS_KEY_SECRET);
    Mockito.when(mockAccessKey.getToken()).thenReturn(ACCESS_KEY_TOKEN);
    PowerMockito.mockStatic(AccessKeyService.class);
    long time = 43200;
    PowerMockito.when(AccessKeyService.createFedAccessKey(mockUser, time))
        .thenReturn(mockAccessKey);
    FederationTokenController mockController2 =
        Mockito.spy(new FederationTokenController(requestor, requestBody2));
    ServerResponse response = mockController2.create();
    assertEquals(HttpResponseStatus.CREATED, response.getResponseStatus());
  }
}
