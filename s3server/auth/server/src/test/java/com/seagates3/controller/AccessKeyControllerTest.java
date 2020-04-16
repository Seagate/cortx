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
 * Original creation date: 06-Jan-2015
 */
package com.seagates3.controller;

import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
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
@PrepareForTest({DAODispatcher.class, KeyGenUtil.class})
@PowerMockIgnore({"javax.management.*"})
public class AccessKeyControllerTest {

    private AccessKeyDAO accessKeyDAO;
    private UserDAO userDAO;
    private AccessKeyController accessKeyController;

    private final String ACCESS_KEY_ID = "AKIAKTEST";
    private final String ACCOUNT_NAME = "s3test";
    private final String ACCOUNT_ID = "12345";
    private final Account ACCOUNT;
    private final String REQUESTOR_NAME = "s3requestor";
    private final String SECRET_KEY = "123ASg/a-3";
    private final String USER_NAME = "s3testuser";

    public AccessKeyControllerTest() {
        ACCOUNT = new Account();
        ACCOUNT.setId(ACCOUNT_ID);
        ACCOUNT.setName(ACCOUNT_NAME);
    }

    /**
     * Create Access Key controller object and mock AccessKeyDAO and UserDAO for
     * create Access Key API.
     *
     * @param userName User name attribute.
     * @throws Exception
     */
    private void createAccessKeyController_CreateAPI(String userName)
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);
        requestor.setName(REQUESTOR_NAME);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        if (userName == null) {
            requestBody.put("UserName", REQUESTOR_NAME);
        } else {
            requestBody.put("UserName", USER_NAME);
        }

        userDAO = Mockito.mock(UserDAO.class);
        accessKeyDAO = Mockito.mock(AccessKeyDAO.class);

        PowerMockito.doReturn(userDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.USER
        );
        PowerMockito.doReturn(accessKeyDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.ACCESS_KEY
        );

        accessKeyController = new AccessKeyController(requestor, requestBody);
    }

    /**
     * Create Access Key controller object and mock AccessKeyDAO and UserDAO for
     * Delete Access Key API.
     *
     * @param userName User name attribute.
     * @throws Exception
     */
    private void createAccessKeyController_DeleteAPI(String userName)
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);
        requestor.setName(REQUESTOR_NAME);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        if (userName != null) {
            requestBody.put("UserName", USER_NAME);
        }
        requestBody.put("AccessKeyId", ACCESS_KEY_ID);

        userDAO = Mockito.mock(UserDAO.class);
        accessKeyDAO = Mockito.mock(AccessKeyDAO.class);

        PowerMockito.doReturn(userDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.USER
        );
        PowerMockito.doReturn(accessKeyDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.ACCESS_KEY
        );

        accessKeyController = new AccessKeyController(requestor, requestBody);
    }

    /**
     * Create Access Key controller object and mock AccessKeyDAO and UserDAO for
     * List Access Key API.
     *
     * @param userName User name attribute.
     * @throws Exception
     */
    private void createAccessKeyController_ListAPI(String userName)
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);
        requestor.setName(REQUESTOR_NAME);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        if (userName == null) {
            requestBody.put("UserName", REQUESTOR_NAME);
        } else {
            requestBody.put("UserName", USER_NAME);
        }

        userDAO = Mockito.mock(UserDAO.class);
        accessKeyDAO = Mockito.mock(AccessKeyDAO.class);

        PowerMockito.doReturn(userDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.USER
        );
        PowerMockito.doReturn(accessKeyDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.ACCESS_KEY
        );

        accessKeyController = new AccessKeyController(requestor, requestBody);
    }

    /**
     * Create Access Key controller object and mock AccessKeyDAO and UserDAO for
     * List Access Key API.
     *
     * @param userName User name attribute.
     * @throws Exception
     */
    private void createAccessKeyController_UpdateAPI(String userName)
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);
        requestor.setName(REQUESTOR_NAME);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);
        if (userName != null) {
            requestBody.put("UserName", USER_NAME);
        }
        requestBody.put("AccessKeyId", ACCESS_KEY_ID);
        requestBody.put("Status", "Inactive");

        userDAO = Mockito.mock(UserDAO.class);
        accessKeyDAO = Mockito.mock(AccessKeyDAO.class);

        PowerMockito.doReturn(userDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.USER
        );
        PowerMockito.doReturn(accessKeyDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.ACCESS_KEY
        );

        accessKeyController = new AccessKeyController(requestor, requestBody);
    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(DAODispatcher.class);
        PowerMockito.mockStatic(KeyGenUtil.class);

        PowerMockito.doReturn(ACCESS_KEY_ID).when(KeyGenUtil.class, "createUserAccessKeyId");
        PowerMockito.doReturn(SECRET_KEY).when(KeyGenUtil.class, "generateSecretKey");
    }

    @Test
    public void CreateAccessKey_UserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_CreateAPI(null);

        Mockito.when(userDAO.find(ACCOUNT_NAME, REQUESTOR_NAME)).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccessKey_UserDoesNotExist_ReturnNoSuchEntity()
            throws Exception {
        createAccessKeyController_CreateAPI(null);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);

        Mockito.doReturn(user).when(userDAO).find(ACCOUNT_NAME, REQUESTOR_NAME);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>NoSuchEntity</Code>"
                + "<Message>The request was rejected because it referenced a "
                + "user that does not exist.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.NOT_FOUND,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccessKey_AccessKeyGetCountFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_CreateAPI(null);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.doReturn(user).when(userDAO).find(ACCOUNT_NAME, REQUESTOR_NAME);
        Mockito.doThrow(new DataAccessException("failed to get access key count"))
                .when(accessKeyDAO).getCount("123");

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccessKey_AccessKeyQuotaExceeded_ReturnQuotaExceeded()
            throws Exception {
        createAccessKeyController_CreateAPI(null);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.doReturn(user).when(userDAO).find(ACCOUNT_NAME, REQUESTOR_NAME);
        Mockito.doReturn(2).when(accessKeyDAO).getCount("123");

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>AccessKeyQuotaExceeded</Code>"
                + "<Message>The request was rejected because the number of "
                + "access keys allowed for the user has exceeded quota.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccessKey_AccessKeySaveFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_CreateAPI(null);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.doReturn(user).when(userDAO).find(ACCOUNT_NAME, REQUESTOR_NAME);
        Mockito.doReturn(0).when(accessKeyDAO).getCount("123");
        Mockito.doThrow(new DataAccessException("Failed to save user access key"))
                .when(accessKeyDAO).save(Mockito.any(AccessKey.class));

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccessKey_AccessKeyCreated_ReturnSuccessResponse()
            throws Exception {
        createAccessKeyController_CreateAPI(null);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.doReturn(user).when(userDAO).find(ACCOUNT_NAME, REQUESTOR_NAME);
        Mockito.doReturn(0).when(accessKeyDAO).getCount("123");
        Mockito.doNothing().when(accessKeyDAO).save(Mockito.any(AccessKey.class));

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<CreateAccessKeyResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<CreateAccessKeyResult>"
                + "<AccessKey>"
                + "<UserName>s3requestor</UserName>"
                + "<AccessKeyId>AKIAKTEST</AccessKeyId>"
                + "<Status>Active</Status>"
                + "<SecretAccessKey>123ASg/a-3</SecretAccessKey>"
                + "</AccessKey>"
                + "</CreateAccessKeyResult>"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</CreateAccessKeyResponse>";

        ServerResponse response = accessKeyController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccessKey_AccessKeyCreatedWithRequestorName_ReturnSuccessResponse()
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);
        requestor.setName(REQUESTOR_NAME);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);

        userDAO = Mockito.mock(UserDAO.class);
        accessKeyDAO = Mockito.mock(AccessKeyDAO.class);

        PowerMockito.doReturn(userDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.USER
        );
        PowerMockito.doReturn(accessKeyDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.ACCESS_KEY
        );

        accessKeyController = new AccessKeyController(requestor, requestBody);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.doReturn(user).when(userDAO).find(ACCOUNT_NAME, REQUESTOR_NAME);
        Mockito.doReturn(0).when(accessKeyDAO).getCount("123");
        Mockito.doNothing().when(accessKeyDAO).save(Mockito.any(AccessKey.class));

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<CreateAccessKeyResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<CreateAccessKeyResult>"
                + "<AccessKey>"
                + "<UserName>s3requestor</UserName>"
                + "<AccessKeyId>AKIAKTEST</AccessKeyId>"
                + "<Status>Active</Status>"
                + "<SecretAccessKey>123ASg/a-3</SecretAccessKey>"
                + "</AccessKey>"
                + "</CreateAccessKeyResult>"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</CreateAccessKeyResponse>";

        ServerResponse response = accessKeyController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccessKey_AccessKeySearchFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_DeleteAPI(null);

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenThrow(
                new DataAccessException("failed to search acess key.\n")
        );

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccessKey_AccessKeyDoesNotExist_ReturnNoSuchEntity()
            throws Exception {
        createAccessKeyController_DeleteAPI(null);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>NoSuchEntity</Code>"
                + "<Message>The request was rejected because it referenced an "
                + "access key that does not exist.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.NOT_FOUND,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccessKey_UserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_DeleteAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME)).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccessKey_UserDoesNotExist_ReturnNoSuchEntity()
            throws Exception {
        createAccessKeyController_DeleteAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME)).thenReturn(user);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>NoSuchEntity</Code>"
                + "<Message>The request was rejected because it referenced a "
                + "user that does not exist.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.NOT_FOUND,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccessKey_AccessKeyDoesNotBelongToUser_ReturnBadRequest()
            throws Exception {
        createAccessKeyController_DeleteAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("456");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME)).thenReturn(user);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>BadRequest</Code>"
                + "<Message>Bad Request. Check request headers and body.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccessKey_AccessKeyDeleteFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_DeleteAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME)).thenReturn(user);
        Mockito.doThrow(new DataAccessException("Failed to delete access key"))
                .when(accessKeyDAO).delete(accessKey);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccessKey_AccessKeyDeleted_ReturnDeleteResponse()
            throws Exception {
        createAccessKeyController_DeleteAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME)).thenReturn(user);
        Mockito.doNothing().when(accessKeyDAO).delete(accessKey);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<DeleteAccessKeyResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</DeleteAccessKeyResponse>";

        ServerResponse response = accessKeyController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                response.getResponseStatus());
    }

    @Test
    public void ListAccessKey_UserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_ListAPI(null);

        Mockito.when(userDAO.find(ACCOUNT_NAME, REQUESTOR_NAME)).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void ListAccessKey_UserDoesNotExist_ReturnNoSuchEntity()
            throws Exception {
        createAccessKeyController_ListAPI(USER_NAME);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);

        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>NoSuchEntity</Code>"
                + "<Message>The request was rejected because it referenced a "
                + "user that does not exist.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.NOT_FOUND,
                response.getResponseStatus());
    }

    @Test
    public void ListAccessKey_AccessKeySearchFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_ListAPI(USER_NAME);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);
        Mockito.doThrow(new DataAccessException("Failed to fetch access keys"))
                .when(accessKeyDAO).findAll(user);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void ListAccessKey_AccessKeyEmpty_ReturnList()
            throws Exception {
        createAccessKeyController_ListAPI(USER_NAME);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        AccessKey[] accessKeyList = new AccessKey[0];

        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);
        Mockito.doReturn(accessKeyList).when(accessKeyDAO).findAll(user);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ListAccessKeysResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<ListAccessKeysResult>"
                + "<UserName>s3testuser</UserName>"
                + "<AccessKeyMetadata/>"
                + "<IsTruncated>false</IsTruncated>"
                + "</ListAccessKeysResult>"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</ListAccessKeysResponse>";

        ServerResponse response = accessKeyController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                response.getResponseStatus());
    }

    @Test
    public void ListAccessKey_AccessKeysFound_ReturnList()
            throws Exception {
        createAccessKeyController_ListAPI(USER_NAME);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);
        accessKey.setCreateDate("2016-01-06'T'10:15:11:000+530");

        AccessKey[] accessKeyList = new AccessKey[]{accessKey};

        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);
        Mockito.doReturn(accessKeyList).when(accessKeyDAO).findAll(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<ListAccessKeysResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListAccessKeysResult>" + "<UserName>s3testuser</UserName>" +
            "<AccessKeyMetadata>" + "<member>" +
            "<UserName>s3testuser</UserName>" +
            "<AccessKeyId>AKIAKTEST</AccessKeyId>" + "<Status>Active</Status>" +
            "<CreateDate>2016-01-06'T'10:15:11:000+530</CreateDate>" +
            "</member>" + "</AccessKeyMetadata>" +
            "<IsTruncated>false</IsTruncated>" + "</ListAccessKeysResult>" +
            "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
            "</ResponseMetadata>" + "</ListAccessKeysResponse>";

        ServerResponse response = accessKeyController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                response.getResponseStatus());
    }

    @Test
    public void ListAccessKey_UsingRequestorName_AccessKeysFound_ReturnList()
            throws Exception {
        Requestor requestor = new Requestor();
        requestor.setAccount(ACCOUNT);
        requestor.setName(REQUESTOR_NAME);

        Map<String, String> requestBody = new TreeMap<>(
                String.CASE_INSENSITIVE_ORDER);

        userDAO = Mockito.mock(UserDAO.class);
        accessKeyDAO = Mockito.mock(AccessKeyDAO.class);

        PowerMockito.doReturn(userDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.USER
        );
        PowerMockito.doReturn(accessKeyDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.ACCESS_KEY
        );

        accessKeyController = new AccessKeyController(requestor, requestBody);
        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);
        accessKey.setCreateDate("2016-01-06'T'10:15:11:000+530");

        AccessKey[] accessKeyList = new AccessKey[]{accessKey};

        Mockito.when(userDAO.find(ACCOUNT_NAME, REQUESTOR_NAME))
                .thenReturn(user);
        Mockito.doReturn(accessKeyList).when(accessKeyDAO).findAll(user);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<ListAccessKeysResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListAccessKeysResult>" + "<UserName>s3requestor</UserName>" +
            "<AccessKeyMetadata>" + "<member>" +
            "<UserName>s3requestor</UserName>" +
            "<AccessKeyId>AKIAKTEST</AccessKeyId>" + "<Status>Active</Status>" +
            "<CreateDate>2016-01-06'T'10:15:11:000+530</CreateDate>" +
            "</member>" + "</AccessKeyMetadata>" +
            "<IsTruncated>false</IsTruncated>" + "</ListAccessKeysResult>" +
            "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
            "</ResponseMetadata>" + "</ListAccessKeysResponse>";

        ServerResponse response = accessKeyController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_AccessKeySearchFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_UpdateAPI(null);

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenThrow(
                new DataAccessException("failed to search acess key.\n")
        );

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_AccessKeyDoesNotExist_ReturnNoSuchEntity()
            throws Exception {
        createAccessKeyController_UpdateAPI(null);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>NoSuchEntity</Code>"
                + "<Message>The request was rejected because it referenced an "
                + "access key that does not exist.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.NOT_FOUND,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_UserSearchFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_UpdateAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME)).thenThrow(
                new DataAccessException("failed to search user.\n"));

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_UserDoesNotExist_ReturnNoSuchEntity()
            throws Exception {
        createAccessKeyController_UpdateAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>NoSuchEntity</Code>"
                + "<Message>The request was rejected because it referenced a "
                + "user that does not exist.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.NOT_FOUND,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_AccessKeyDoesNotBelongToUser_ReturnNoSuchEntity()
            throws Exception {
        createAccessKeyController_UpdateAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("456");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);

        final String expectedResponseBody = "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
                "standalone=\"no\"?><ErrorResponse xmlns=\"https://iam.seagate.com/doc/" +
                "2010-05-08/\"><Error><Code>InvalidParameterValue</Code><Message>Access" +
                " key does not belong to provided user.</Message></Error><RequestId>0000" +
                "</RequestId></ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.BAD_REQUEST,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_SameStatus_ReturnUpdateResponse()
            throws Exception {
        createAccessKeyController_UpdateAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");
        accessKey.setStatus(AccessKey.AccessKeyStatus.INACTIVE);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<UpdateAccessKeyResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</UpdateAccessKeyResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_AccessKeyUpdateFailed_ReturnInternalServerError()
            throws Exception {
        createAccessKeyController_UpdateAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);
        Mockito.doThrow(new DataAccessException("Failed to upgrade access key"))
                .when(accessKeyDAO).update(accessKey, "Inactive");

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<Error><Code>InternalFailure</Code>"
                + "<Message>The request processing has failed because of an "
                + "unknown error, exception or failure.</Message></Error>"
                + "<RequestId>0000</RequestId>"
                + "</ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_AccessKeyUpdated_ReturnUpdateResponse()
            throws Exception {
        createAccessKeyController_UpdateAPI(USER_NAME);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName(USER_NAME);
        user.setId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);
        Mockito.doNothing().when(accessKeyDAO).update(accessKey, "Inactive");

        final String expectedResponseBody = "<?xml version=\"1.0\" "
                + "encoding=\"UTF-8\" standalone=\"no\"?>"
                + "<UpdateAccessKeyResponse "
                + "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">"
                + "<ResponseMetadata>"
                + "<RequestId>0000</RequestId>"
                + "</ResponseMetadata>"
                + "</UpdateAccessKeyResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_RootUserAccesskKey_UpdateShouldFail_WithUserName()
            throws Exception {
        createAccessKeyController_UpdateAPI("root");

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName("root");
        user.setId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.find(ACCOUNT_NAME, USER_NAME))
                .thenReturn(user);
        Mockito.doNothing().when(accessKeyDAO).update(accessKey, "Inactive");

        final String expectedResponseBody = "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
                "standalone=\"no\"?><ErrorResponse xmlns=\"https://iam.seagate.com/doc/" +
                "2010-05-08/\"><Error><Code>OperationNotSupported</Code><Message>Access " +
                "key status for root user can not be changed.</Message></Error><RequestId>" +
                "0000</RequestId></ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED, response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_RootUserAccesskKey_UpdateShouldFail()
            throws Exception {
        createAccessKeyController_UpdateAPI(null);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName("root");
        user.setId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.findByUserId(ACCOUNT_NAME, "123")).thenReturn(user);
        Mockito.doNothing().when(accessKeyDAO).update(accessKey, "Inactive");

        final String expectedResponseBody = "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
                "standalone=\"no\"?><ErrorResponse xmlns=\"https://iam.seagate.com/doc/" +
                "2010-05-08/\"><Error><Code>OperationNotSupported</Code><Message>Access " +
                "key status for root user can not be changed.</Message></Error><RequestId>" +
                "0000</RequestId></ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED, response.getResponseStatus());
    }

    @Test
    public void UpdateAccessKey_FindByUserId_ShouldThrowException()
            throws Exception {
        createAccessKeyController_UpdateAPI(null);

        AccessKey accessKey = new AccessKey();
        accessKey.setId(ACCESS_KEY_ID);
        accessKey.setUserId("123");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        User user = new User();
        user.setAccountName(ACCOUNT_NAME);
        user.setName("root");
        user.setId("123");

        Mockito.when(accessKeyDAO.find(ACCESS_KEY_ID)).thenReturn(accessKey);
        Mockito.when(userDAO.findByUserId(ACCOUNT_NAME, "123")).thenThrow(
                new DataAccessException("Failed to find user details.")
        );
        Mockito.doNothing().when(accessKeyDAO).update(accessKey, "Inactive");

        final String expectedResponseBody = "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
                "standalone=\"no\"?><ErrorResponse xmlns=\"https://iam.seagate.com/doc/" +
                "2010-05-08/\"><Error><Code>InternalFailure</Code><Message>The request " +
                "processing has failed because of an unknown error, exception or failure." +
                "</Message></Error><RequestId>0000</RequestId></ErrorResponse>";

        ServerResponse response = accessKeyController.update();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR, response.getResponseStatus());
    }
}
