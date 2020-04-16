/*
 * COPYRIGHT 2015 SEAGATE LLC
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
 * Original author:  Arjun Hariharan <arjun.hariharan@seagate.com>
 * Original creation date: 29-Dec-2015
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
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.util.KeyGenUtil;

import io.netty.handler.codec.http.HttpResponseStatus;

@PowerMockIgnore({"javax.management.*"}) @RunWith(PowerMockRunner.class)
    @PrepareForTest({DAODispatcher.class,
                     KeyGenUtil.class}) public class UserControllerTest {

    private UserController userController;
    private UserDAO userDAO;
    private AccessKeyDAO accessKeyDAO;
    private final String ACCOUNT_NAME = "s3test";
    private
     final String ACCOUNT_ID = "AIDA5KZQJXPTROAIAKCKO";
    private final Account ACCOUNT;

    public UserControllerTest() {
        ACCOUNT = new Account();
        ACCOUNT.setId(ACCOUNT_ID);
        ACCOUNT.setName(ACCOUNT_NAME);
    }

    /**
     * Create user controller object and mock UserDAO. Take default path = "/".
     *
     * @throws Exception
     */
    private void createUserController_CreateAPI() throws Exception {
        UserControllerTest.this.createUserController_CreateAPI("/");
    }

    /**
     * Create user controller object and mock UserDAO for Create User API.
     *
     * @param path User path attribute.
     * @throws Exception
     */
    private void createUserController_CreateAPI(String path) throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        requestBody.put("UserName", "s3testuser");
        requestBody.put("Path", path);

        userDAO = Mockito.mock(UserDAO.class);

        PowerMockito.doReturn(userDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.USER);

        userController = new UserController(requestor, requestBody);
    }

    /**
     * Create user controller object and mock UserDAO for Delete User API.
     *
     * @param path
     * @throws Exception
     */
    private void createUserController_DeleteAPI() throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);

        Map<String, String> requestBody =
            new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("UserName", "s3testuser");

        userDAO = Mockito.mock(UserDAO.class);
        accessKeyDAO = Mockito.mock(AccessKeyDAO.class);

        PowerMockito.doReturn(userDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.USER);
        PowerMockito.doReturn(accessKeyDAO).when(
            DAODispatcher.class, "getResourceDAO", DAOResource.ACCESS_KEY);

        userController = new UserController(requestor, requestBody);
    }

    /**
     * Create user controller object and mock UserDAO for List API. Take default
     * path = "/".
     *
     * @throws Exception
     */
    private void createUserController_ListAPI() throws Exception {
        createUserController_ListAPI("/");
    }

    /**
     * Create user controller object and mock UserDAO for List User API.
     *
     * @param path User path attribute.
     * @throws Exception
     */
    private void createUserController_ListAPI(String path) throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);

        Map<String, String> requestBody =
            new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("Path", path);

        userDAO = Mockito.mock(UserDAO.class);

        PowerMockito.doReturn(userDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.USER);

        userController = new UserController(requestor, requestBody);
    }

    /**
     * Create user controller object and mock UserDAO for Update User API.
     *
     * @param path User path attribute.
     * @throws Exception
     */
   private
    void createUserController_UpdateAPI(String newUserName,
                                        String newPath) throws Exception {
        createUserController_UpdateAPI("s3testuser", newUserName, newPath);
    }

   private
    void createUserController_UpdateAPI(String userName, String newUserName,
                                        String newPath) throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);

        Map<String, String> requestBody =
            new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("UserName", userName);
        if (newPath != null) {
            requestBody.put("NewPath", newPath);
        }

        if (newUserName != null) {
            requestBody.put("NewUserName", newUserName);
        }

        userDAO = Mockito.mock(UserDAO.class);

        PowerMockito.doReturn(userDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.USER);

        userController = new UserController(requestor, requestBody);
    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(DAODispatcher.class);
        PowerMockito.mockStatic(KeyGenUtil.class);

        PowerMockito.doReturn("5KZQJXPTROAIAKCKO")
            .when(KeyGenUtil.class, "createIamUserId");
    }

    @Test
    public void CreateUser_UserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_CreateAPI();

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void CreateUser_UserExists_ReturnEntityAlreadyExists()
            throws Exception {
        createUserController_CreateAPI();

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>EntityAlreadyExists</Code>" +
            "<Message>The request was rejected because it attempted " +
            "to create or update a resource that already " +
            "exists.</Message></Error>" + "<RequestId>0000</RequestId>" +
            "</ErrorResponse>";

        ServerResponse response = userController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CONFLICT,
                response.getResponseStatus());
    }

    @Test
    public void CreateUser_UserSaveFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_CreateAPI();

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
        Mockito.doThrow(new DataAccessException("failed to save new user.\n"))
                .when(userDAO).save(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void CreateUser_NewUserCreated_ReturnCreateResponse()
            throws Exception {
        createUserController_CreateAPI();

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setArn("arn:aws:iam::1:user/s3testuser");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
        Mockito.doNothing().when(userDAO).save(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<CreateUserResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<CreateUserResult>" + "<User>" + "<Path>/</Path>" +
            "<UserName>s3testuser</UserName>" +
            "<UserId>AIDA5KZQJXPTROAIAKCKO</UserId>" +
            "<Arn>arn:aws:iam::AIDA5KZQJXPTROAIAKCKO:user/s3testuser</Arn>" +
            "</User>" + "</CreateUserResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</CreateUserResponse>";

        ServerResponse response = userController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());
    }

    @Test
    public void CreateUser_NewUserCreatedWithDefaultPath_ReturnCreateResponse()
            throws Exception {
      final String expectedResponseBody =
          "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
          "<CreateUserResponse " +
          "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
          "<CreateUserResult>" + "<User>" + "<Path>/</Path>" +
          "<UserName>s3testuser</UserName>" +
          "<UserId>AIDA5KZQJXPTROAIAKCKO</UserId>" +
          "<Arn>arn:aws:iam::AIDA5KZQJXPTROAIAKCKO:user/s3testuser</Arn>" +
          "</User>" + "</CreateUserResult>" + "<ResponseMetadata>" +
          "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
          "</CreateUserResponse>";

        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);
        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        requestBody.put("UserName", "s3testuser");
        userDAO = Mockito.mock(UserDAO.class);
        PowerMockito.doReturn(userDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.USER);
        userController = new UserController(requestor, requestBody);
        User user = new User();
        user.setAccountName("s3test");
        user.setArn("arn:aws:iam::1:user/s3testuser");
        user.setName("s3testuser");
        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
        Mockito.doNothing().when(userDAO).save(user);

        ServerResponse response = userController.create();

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());
    }

    @Test
    public void CreateUser_NewUserCreatedWithPath_ReturnCreateResponse()
            throws Exception {
        UserControllerTest.this.createUserController_CreateAPI("/test");

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setArn("arn:aws:iam::1:user/s3testuser");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
        Mockito.doNothing().when(userDAO).save(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<CreateUserResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<CreateUserResult>" + "<User>" + "<Path>/test</Path>" +
            "<UserName>s3testuser</UserName>" +
            "<UserId>AIDA5KZQJXPTROAIAKCKO</UserId>" +
            "<Arn>arn:aws:iam::AIDA5KZQJXPTROAIAKCKO:user/s3testuser</Arn>" +
            "</User>" + "</CreateUserResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</CreateUserResponse>";

        ServerResponse response = userController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());
    }

    @Test
    public void DeleteUser_UserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_DeleteAPI();

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void DeleteUser_UserDoesNotExist_ReturnNoSuchEntity()
            throws Exception {
        createUserController_CreateAPI();

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>NoSuchEntity</Code>" +
            "<Message>The request was rejected because it referenced an " +
            "entity that does not exist. </Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                response.getResponseStatus());
    }

    @Test
    public void DeleteUser_AccessKeySearchFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_DeleteAPI();

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
        Mockito.doThrow(new DataAccessException("Failed to search access keys"))
            .when(accessKeyDAO)
            .hasAccessKeys("AIDA5KZQJXPTROAIAKCKO");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void DeleteUser_UserHasAccesskeys_ReturnDeleteConflict()
            throws Exception {
        createUserController_DeleteAPI();

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
        Mockito.doReturn(Boolean.TRUE).when(accessKeyDAO).hasAccessKeys(
            "AIDA5KZQJXPTROAIAKCKO");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>DeleteConflict</Code>" +
            "<Message>The request was rejected because it attempted to " +
            "delete a resource that has attached subordinate entities. " +
            "The error message describes these entities.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CONFLICT,
                response.getResponseStatus());
    }

    @Test
    public void DeleteUser_UserDeleteFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_DeleteAPI();

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
        Mockito.doReturn(Boolean.FALSE).when(accessKeyDAO).hasAccessKeys(
            "AIDA5KZQJXPTROAIAKCKO");
        Mockito.doThrow(new DataAccessException("Failed to delete user"))
                .when(userDAO).delete(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void DeleteUser_UserDeleteSuccess_ReturnDeleteResponse()
            throws Exception {
        createUserController_DeleteAPI();

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenReturn(user);
        Mockito.doReturn(Boolean.FALSE).when(accessKeyDAO).hasAccessKeys(
            "AIDA5KZQJXPTROAIAKCKO");
        Mockito.doNothing().when(userDAO).delete(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<DeleteUserResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
            "</ResponseMetadata>" + "</DeleteUserResponse>";

        ServerResponse response = userController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                response.getResponseStatus());
    }

    @Test
    public void ListUser_UserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_ListAPI();

        Mockito.when(userDAO.findAll("s3test", "/")).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void ListUser_UserListEmpty_ReturnListUserResponse()
            throws Exception {
        createUserController_ListAPI();

        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("AIDA5KZQJXPTROAIAKCKO");
        expectedUser.setUserType("iamUser");
        expectedUser.setPath("/");
        expectedUser.setCreateDate("2016-01-06T10:15:11.000+0530");

        User[] expectedUserList = new User[0];

        Mockito.doReturn(expectedUserList).when(userDAO).findAll("s3test", "/");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ListUsersResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListUsersResult>" + "<Users/>" +
            "<IsTruncated>false</IsTruncated>" + "</ListUsersResult>" +
            "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
            "</ResponseMetadata>" + "</ListUsersResponse>";

        ServerResponse response = userController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }

    @Test
    public void ListUser_UserListEmptyWithPathPrefix_ReturnListUserResponse()
            throws Exception {
      final String expectedResponseBody =
          "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
          "<ListUsersResponse " +
          "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
          "<ListUsersResult>" + "<Users/>" +
          "<IsTruncated>false</IsTruncated>" + "</ListUsersResult>" +
          "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
          "</ResponseMetadata>" + "</ListUsersResponse>";

        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);
        Map<String, String> requestBody =
            new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
        requestBody.put("PathPrefix", "/");

        userDAO = Mockito.mock(UserDAO.class);
        PowerMockito.doReturn(userDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.USER);
        userController = new UserController(requestor, requestBody);
        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("AIDA5KZQJXPTROAIAKCKO");
        expectedUser.setUserType("iamUser");
        expectedUser.setPath("/");
        expectedUser.setCreateDate("2016-01-06T10:15:11.000+0530");
        User[] expectedUserList = new User[0];
        Mockito.doReturn(expectedUserList).when(userDAO).findAll("s3test", "/");

        ServerResponse response = userController.list();

        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }

    @Test
    public void ListUser_UserSearchSuccess_ReturnListUserResponse()
            throws Exception {
        createUserController_ListAPI();

        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("AIDA5KZQJXPTROAIAKCKO");
        expectedUser.setUserType("iamUser");
        expectedUser.setPath("/");
        expectedUser.setCreateDate("2016-01-06T10:15:11.000+0530");
        expectedUser.setArn("arn:aws:iam::1:user/s3testuser");

        User[] expectedUserList = new User[]{expectedUser};

        Mockito.doReturn(expectedUserList).when(userDAO).findAll("s3test", "/");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ListUsersResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListUsersResult>" + "<Users>" + "<member>" +
            "<UserId>AIDA5KZQJXPTROAIAKCKO</UserId>" + "<Path>/</Path>" +
            "<UserName>s3testuser</UserName>" +
            "<Arn>arn:aws:iam::1:user/s3testuser</Arn>" +
            "<CreateDate>2016-01-06T10:15:11.000+0530</CreateDate>" +
            "</member>" + "</Users>" + "<IsTruncated>false</IsTruncated>" +
            "</ListUsersResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</ListUsersResponse>";

        ServerResponse response = userController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }

    @Test
    public void ListUser_UserSearchWithPathPrefixSuccess_ReturnListUserResponse()
            throws Exception {
        createUserController_ListAPI("/test");

        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("AIDA5KZQJXPTROAIAKCKO");
        expectedUser.setUserType("iamUser");
        expectedUser.setPath("/test");
        expectedUser.setCreateDate("2016-01-06'T'10:15:11.000+530");
        expectedUser.setArn("arn:aws:iam::1:user/s3testuser");

        User[] expectedUserList = new User[]{expectedUser};

        Mockito.doReturn(expectedUserList).when(userDAO).findAll("s3test", "/");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ListUsersResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListUsersResult>" + "<Users>" + "<member>" +
            "<UserId>AIDA5KZQJXPTROAIAKCKO</UserId>" + "<Path>/test</Path>" +
            "<UserName>s3testuser</UserName>" +
            "<Arn>arn:aws:iam::1:user/s3testuser</Arn>" +
            "<CreateDate>2016-01-06'T'10:15:11.000+530</CreateDate>" +
            "</member>" + "</Users>" + "<IsTruncated>false</IsTruncated>" +
            "</ListUsersResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</ListUsersResponse>";

        ServerResponse response = userController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }

    @Test
    public void UpdateUser_NoUpdateField_ReturnMissingParameter()
            throws Exception {
        createUserController_UpdateAPI(null, null);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>MissingParameter</Code>" +
            "<Message>A required parameter for the specified action is " +
            "not supplied.</Message></Error>" + "<RequestId>0000</RequestId>" +
            "</ErrorResponse>";

        ServerResponse response = userController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                response.getResponseStatus());
    }

    @Test public void UpdateUser_UpdateRootUser_ShouldFail() throws Exception {
        createUserController_UpdateAPI("root", "rootNewName", null);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
            "standalone=\"no\"?><ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc" +
            "/2010-05-08/\"><Error><Code>OperationNotSupported</" +
            "Code><Message>Cannot" + " change user name of root " +
            "user.</Message></Error><RequestId>0000<" +
            "/RequestId></ErrorResponse>";

        ServerResponse response = userController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                response.getResponseStatus());
    }

    @Test
    public void UpdateUser_UserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_UpdateAPI("s3newuser", "/test/update");

        Mockito.when(userDAO.find("s3test", "s3testuser")).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void UpdateUser_UserDoesNotExist_ReturnNoSuchEntity()
            throws Exception {
        createUserController_UpdateAPI("s3newuser", "/test/update");

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");

        Mockito.doReturn(user).when(userDAO).find("s3test", "s3testuser");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>NoSuchEntity</Code>" +
            "<Message>The request was rejected because it referenced an " +
            "entity that does not exist. </Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                response.getResponseStatus());
    }

    @Test
    public void UpdateUser_NewUserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_UpdateAPI("s3newuser", "/test/update");

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        Mockito.doReturn(user).when(userDAO).find("s3test", "s3testuser");
        Mockito.when(userDAO.find("s3test", "s3newuser")).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void UpdateUser_NewUserExists_ReturnEntityAlreadyExists()
            throws Exception {
        createUserController_UpdateAPI("s3newuser", "/test/update");

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        User newUser = new User();
        newUser.setAccountName("s3test");
        newUser.setName("s3newuser");
        newUser.setId("AIDA6MQQJXOTCOAIAKCYW");

        Mockito.doReturn(user).when(userDAO).find("s3test", "s3testuser");
        Mockito.doReturn(newUser).when(userDAO).find("s3test", "s3newuser");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>EntityAlreadyExists</Code>" +
            "<Message>The request was rejected because it attempted to " +
            "create or update a resource that already " +
            "exists.</Message></Error>" + "<RequestId>0000</RequestId>" +
            "</ErrorResponse>";

        ServerResponse response = userController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CONFLICT,
                response.getResponseStatus());
    }

    @Test
    public void UpdateUser_UpdatedFailed_ReturnInternalServerError()
            throws Exception {
        createUserController_UpdateAPI("s3newuser", "/test/update");

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        User newUser = new User();
        newUser.setAccountName("s3test");
        newUser.setName("s3newuser");

        Mockito.doReturn(user).when(userDAO).find("s3test", "s3testuser");
        Mockito.doReturn(newUser).when(userDAO).find("s3test", "s3newuser");
        Mockito.doThrow(new DataAccessException("Failed to update user.\n"))
                .when(userDAO).update(user, "s3newuser", "/test/update");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = userController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void UpdateUser_UpdatedSuccess_ReturnUpdateResponse()
            throws Exception {
        createUserController_UpdateAPI("s3newuser", "/test/update");

        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("AIDA5KZQJXPTROAIAKCKO");

        User newUser = new User();
        newUser.setAccountName("s3test");
        newUser.setName("s3newuser");

        Mockito.doReturn(user).when(userDAO).find("s3test", "s3testuser");
        Mockito.doReturn(newUser).when(userDAO).find("s3test", "s3newuser");
        Mockito.doNothing().when(userDAO).update(user, "s3newuser",
                                                 "/test/update");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<UpdateUserResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
            "</ResponseMetadata>" + "</UpdateUserResponse>";

        ServerResponse response = userController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }
}

