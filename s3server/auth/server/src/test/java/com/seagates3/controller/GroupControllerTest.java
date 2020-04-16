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
 * Original author: Sachitanand Shelake <sachitanand.shelake@seagate.com>
 * Original creation date: 23-May-2019
 */
package com.seagates3.controller;

import java.util.Map;
import java.util.TreeMap;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.GroupDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Group;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.util.ARNUtil;
import com.seagates3.util.KeyGenUtil;

import io.netty.handler.codec.http.HttpResponseStatus;

@RunWith(PowerMockRunner.class) @MockPolicy(Slf4jMockPolicy.class)
    @PowerMockIgnore({"javax.management.*"})
    @PrepareForTest({DAODispatcher.class, KeyGenUtil.class,
                     ARNUtil.class}) public class GroupControllerTest {

 private
  GroupController groupController;
 private
  GroupDAO groupDAO;
 private
  final String ACCOUNT_NAME = "s3test";
 private
  final String ACCOUNT_ID = "12345";
 private
  final Account ACCOUNT;
 private
  final String GROUP_NAME = "s3testgroup";
 private
  final String GROUP_ID = "12345";
 private
  final String GROUP_ARN = "arn:seagate:iam::12345";
 private
  final String GROUP_CREATE_DATE = "2019-05-23T12:22:52.557+0000";
 private
  final String GROUP_PATH = "/";

 public
  GroupControllerTest() {
    ACCOUNT = new Account();
    ACCOUNT.setId(ACCOUNT_ID);
    ACCOUNT.setName(ACCOUNT_NAME);
  }

  /**
   * Create group controller object and mock GroupDAO.
   * Take default path = "/".
   *
   * @throws Exception
   */
 private
  void createGroupController_CreateAPI() throws Exception {
    GroupControllerTest.this.createGroupController_CreateAPI("/");
  }

  /**
   * Create group controller object and mock GroupDAO for Create Group API.
   * Take input group path.
   *
   * @param path Group path attribute.
   * @throws Exception
   */
 private
  void createGroupController_CreateAPI(String path) throws Exception {
    Requestor requestor = new Requestor();
    requestor.setAccount(ACCOUNT);

    Map<String, String> requestBody =
        new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
    requestBody.put("GroupName", GROUP_NAME);
    requestBody.put("Path", path);

    groupController = new GroupController(requestor, requestBody);
  }

  @Before public void setUp() throws Exception {
    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.mockStatic(KeyGenUtil.class);
    PowerMockito.mockStatic(ARNUtil.class);

    groupDAO = Mockito.mock(GroupDAO.class);

    PowerMockito.doReturn(groupDAO)
        .when(DAODispatcher.class, "getResourceDAO", DAOResource.GROUP);

    PowerMockito.doReturn(GROUP_ID).when(KeyGenUtil.class, "createId");

    PowerMockito.doReturn(GROUP_ARN)
        .when(ARNUtil.class, "createARN", GROUP_ID, "group", GROUP_ID);
  }

  /**
   * Test if group search failed, it should throw Internal Server Error
   *
   * @throws Exception
   */
  @Test public void CreateGroup_GroupSearchFailed_ReturnInternalServerError()
      throws Exception {
    createGroupController_CreateAPI();

    Mockito.when(groupDAO.find(ACCOUNT, GROUP_NAME))
        .thenThrow(new DataAccessException("failed to search group.\n"));

    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/" +
        "2010-05-08/\">" + "<Error><Code>InternalFailure</Code>" +
        "<Message>The request processing has failed because of an " +
        "unknown error, exception or failure.</Message>" + "</Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = groupController.create();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                        response.getResponseStatus());
  }

  /**
   * Test if group already exists, it should throw EntityAlreadyExists
   *
   * @throws Exception
   */
  @Test public void CreateGroup_GroupExists_ReturnEntityAlreadyExists()
      throws Exception {
    createGroupController_CreateAPI();

    Group group = new Group();
    group.setGroupId(GROUP_ID);
    group.setName(GROUP_NAME);
    group.setAccount(ACCOUNT);

    Mockito.when(groupDAO.find(ACCOUNT, GROUP_NAME)).thenReturn(group);

    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/" +
        "doc/2010-05-08/\">" + "<Error><Code>EntityAlreadyExists</Code>" +
        "<Message>The request was rejected because it attempted " +
        "to create or update a resource that already exists." + "</Message>" +
        "</Error>" + "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = groupController.create();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.CONFLICT,
                        response.getResponseStatus());
  }

  /**
   * Test if new group save action fails then
   * it should throw Internal Server Error
   *
   * @throws Exception
   */
  @Test public void CreateGroup_GroupSaveFailed_ReturnInternalServerError()
      throws Exception {
    createGroupController_CreateAPI();

    Group group = new Group();
    group.setName(GROUP_NAME);
    group.setAccount(ACCOUNT);

    Mockito.when(groupDAO.find(ACCOUNT, GROUP_NAME)).thenReturn(group);
    Mockito.doThrow(new DataAccessException("failed to save new group.\n"))
        .when(groupDAO)
        .save(group);

    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/" +
        "2010-05-08/\">" + "<Error><Code>InternalFailure</Code>" +
        "<Message>The request processing has failed because of an " +
        "unknown error, exception or failure.</Message>" + "</Error>" +
        "<RequestId>0000</RequestId>" + "</ErrorResponse>";

    ServerResponse response = groupController.create();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                        response.getResponseStatus());
  }

  /**
   * Test group creation by validating response body
   *
   * @throws Exception
   */
  @Test public void CreateGroup_NewGroupCreated_ReturnCreateResponse()
      throws Exception {
    createGroupController_CreateAPI();

    Group group = Mockito.mock(Group.class);

    Mockito.when(group.getCreateDate()).thenReturn(GROUP_CREATE_DATE);
    Mockito.when(group.getGroupId()).thenReturn(GROUP_ID);
    Mockito.when(group.getARN()).thenReturn(GROUP_ARN);
    Mockito.when(group.getPath()).thenReturn(GROUP_PATH);
    Mockito.when(group.getAccount()).thenReturn(ACCOUNT);
    Mockito.when(group.getName()).thenReturn(GROUP_NAME);
    Mockito.when(groupDAO.find(ACCOUNT, GROUP_NAME)).thenReturn(group);
    Mockito.doNothing().when(groupDAO).save(group);

    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<CreateGroupResponse xmlns=\"https://iam.seagate.com/doc/" +
        "2010-05-08/\">" + "<CreateGroupResult>" + "<Group>" +
        "<Path>/</Path>" + "<Arn>arn:seagate:iam::12345</Arn>" +
        "<GroupId>12345</GroupId>" + "<GroupName>s3testgroup</GroupName>" +
        "<CreateDate>2019-05-23T12:22:52.557+0000</CreateDate>" + "</Group>" +
        "</CreateGroupResult>" + "<ResponseMetadata>" +
        "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
        "</CreateGroupResponse>";

    ServerResponse response = groupController.create();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.CREATED,
                        response.getResponseStatus());
  }

  /**
   * Test group creation with default path "/"
   *
   * @throws Exception
   */
  @Test public void
  CreateGroup_NewGroupCreatedWithDefaultPath_ReturnCreateResponse()
      throws Exception {

    // build groupController with default path
    Requestor requestor = new Requestor();
    requestor.setAccount(ACCOUNT);

    Map<String, String> requestBody =
        new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
    requestBody.put("GroupName", GROUP_NAME);

    groupController = new GroupController(requestor, requestBody);

    Group group = Mockito.mock(Group.class);

    Mockito.when(group.getCreateDate()).thenReturn(GROUP_CREATE_DATE);
    Mockito.when(group.getGroupId()).thenReturn(GROUP_ID);
    Mockito.when(group.getARN()).thenReturn(GROUP_ARN);
    Mockito.when(group.getPath()).thenReturn(GROUP_PATH);
    Mockito.when(group.getAccount()).thenReturn(ACCOUNT);
    Mockito.when(group.getName()).thenReturn(GROUP_NAME);
    Mockito.when(groupDAO.find(ACCOUNT, GROUP_NAME)).thenReturn(group);
    Mockito.doNothing().when(groupDAO).save(group);

    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<CreateGroupResponse xmlns=\"https://iam.seagate.com/doc/" +
        "2010-05-08/\">" + "<CreateGroupResult>" + "<Group>" +
        "<Path>/</Path>" + "<Arn>arn:seagate:iam::12345</Arn>" +
        "<GroupId>12345</GroupId>" + "<GroupName>s3testgroup</GroupName>" +
        "<CreateDate>2019-05-23T12:22:52.557+0000</CreateDate>" + "</Group>" +
        "</CreateGroupResult>" + "<ResponseMetadata>" +
        "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
        "</CreateGroupResponse>";

    ServerResponse response = groupController.create();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.CREATED,
                        response.getResponseStatus());
  }

  /**
   * Test group creation with custom path "/test"
   *
   * @throws Exception
   */
  @Test public void CreateGroup_NewGroupCreatedWithPath_ReturnCreateResponse()
      throws Exception {
    String path = "/test";
    GroupControllerTest.this.createGroupController_CreateAPI(path);

    Group group = Mockito.mock(Group.class);

    Mockito.when(group.getCreateDate()).thenReturn(GROUP_CREATE_DATE);
    Mockito.when(group.getGroupId()).thenReturn(GROUP_ID);
    Mockito.when(group.getARN()).thenReturn(GROUP_ARN);
    Mockito.when(group.getPath()).thenReturn(path);
    Mockito.when(group.getAccount()).thenReturn(ACCOUNT);
    Mockito.when(group.getName()).thenReturn(GROUP_NAME);
    Mockito.when(groupDAO.find(ACCOUNT, GROUP_NAME)).thenReturn(group);
    Mockito.doNothing().when(groupDAO).save(group);

    final String expectedResponseBody =
        "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
        "<CreateGroupResponse xmlns=\"https://iam.seagate.com/" +
        "doc/2010-05-08/\">" + "<CreateGroupResult>" + "<Group>" +
        "<Path>/test</Path>" + "<Arn>arn:seagate:iam::12345</Arn>" +
        "<GroupId>12345</GroupId>" + "<GroupName>s3testgroup</GroupName>" +
        "<CreateDate>2019-05-23T12:22:52.557+0000</CreateDate>" + "</Group>" +
        "</CreateGroupResult>" + "<ResponseMetadata>" +
        "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
        "</CreateGroupResponse>";

    ServerResponse response = groupController.create();
    Assert.assertEquals(expectedResponseBody, response.getResponseBody());
    Assert.assertEquals(HttpResponseStatus.CREATED,
                        response.getResponseStatus());
  }
}
