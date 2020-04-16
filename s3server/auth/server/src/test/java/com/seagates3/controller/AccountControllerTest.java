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

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;

import java.util.HashMap;
import java.util.Map;

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
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.RoleDAO;
import com.seagates3.dao.UserDAO;
import com.seagates3.dao.ldap.LDAPUtils;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.model.Role;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.s3service.S3AccountNotifier;
import com.seagates3.util.KeyGenUtil;

import io.netty.handler.codec.http.HttpResponseStatus;

@RunWith(PowerMockRunner.class)
    @PrepareForTest({DAODispatcher.class, KeyGenUtil.class,
                     AccountController.class})
    @PowerMockIgnore(
        {"javax.management.*"}) public class AccountControllerTest {

    private final AccountController accountController;
    private final AccountDAO accountDAO;
    private final UserDAO userDAO;
    private final AccessKeyDAO accessKeyDAO;
    private final RoleDAO roleDAO;
    private final Requestor requestor;
    private final S3AccountNotifier s3;
    private Map<String, String> requestBody = new HashMap<>();

    public AccountControllerTest() throws Exception {
        PowerMockito.mockStatic(DAODispatcher.class);

        requestor = mock(Requestor.class);
        requestBody.put("AccountName", "s3test");
        requestBody.put("Email", "testuser@seagate.com");

        accountDAO = Mockito.mock(AccountDAO.class);
        userDAO = Mockito.mock(UserDAO.class);
        accessKeyDAO = Mockito.mock(AccessKeyDAO.class);
        roleDAO = Mockito.mock(RoleDAO.class);
        s3 = Mockito.mock(S3AccountNotifier.class);

        PowerMockito.whenNew(S3AccountNotifier.class)
            .withNoArguments()
            .thenReturn(s3);
        PowerMockito.doReturn(accountDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.ACCOUNT);

        PowerMockito.doReturn(userDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.USER);

        PowerMockito.doReturn(accessKeyDAO).when(
            DAODispatcher.class, "getResourceDAO", DAOResource.ACCESS_KEY);

        PowerMockito.doReturn(roleDAO)
            .when(DAODispatcher.class, "getResourceDAO", DAOResource.ROLE);

        accountController = new AccountController(requestor, requestBody);
    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(DAODispatcher.class);
        PowerMockito.mockStatic(KeyGenUtil.class);

        PowerMockito.doReturn("987654352188")
            .when(KeyGenUtil.class, "createAccountId");
        PowerMockito.doReturn("C1234").when(KeyGenUtil.class, "createId");
        PowerMockito.doReturn("can1234")
            .when(KeyGenUtil.class, "createCanonicalId");
    }

    @Test
    public void ListAccounts_AccountsSearchFailed_ReturnInternalServerError()
            throws Exception {
        Mockito.when(accountDAO.findAll()).thenThrow(
                new DataAccessException("Failed to fetch accounts.\n"));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = accountController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void ListAccounts_AccountsListEmpty_ReturnListAccountsResponse()
            throws Exception {
        Account[] expectedAccountList = new Account[0];

        Mockito.doReturn(expectedAccountList).when(accountDAO).findAll();

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<ListAccountsResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListAccountsResult>" + "<Accounts/>" +
            "<IsTruncated>false</IsTruncated>" + "</ListAccountsResult>" +
            "<ResponseMetadata>" + "<RequestId>0000</RequestId>" +
            "</ResponseMetadata>" + "</ListAccountsResponse>";

        ServerResponse response = accountController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }

    @Test
    public void ListAccounts_AccountsSearchSuccess_ReturnListAccountsResponse()
            throws Exception {
        Account expectedAccount = new Account();
        expectedAccount.setName("s3test");
        expectedAccount.setId("123456789012");
        expectedAccount.setCanonicalId("canonicalid");
        expectedAccount.setEmail("user.name@seagate.com");
        Account[] expectedAccountList = new Account[]{expectedAccount};

        Mockito.doReturn(expectedAccountList).when(accountDAO).findAll();

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<ListAccountsResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ListAccountsResult>" + "<Accounts>" + "<member>" +
            "<AccountName>s3test</AccountName>" +
            "<AccountId>123456789012</AccountId>" +
            "<CanonicalId>canonicalid</CanonicalId>" +
            "<Email>user.name@seagate.com</Email>" + "</member>" +
            "</Accounts>" + "<IsTruncated>false</IsTruncated>" +
            "</ListAccountsResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</ListAccountsResponse>";

        ServerResponse response = accountController.list();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());
    }

    @Test
    public void CreateAccount_AccountSearchFailed_ReturnInternalServerError()
            throws Exception {
        Mockito.when(accountDAO.find("s3test")).thenThrow(
                new DataAccessException("failed to search account.\n"));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = accountController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccount_AccountExists_ReturnEntityAlreadyExists()
            throws Exception {
        Account account = new Account();
        account.setId("123456789012");
        account.setName("s3test");

        Mockito.when(accountDAO.find("s3test")).thenReturn(account);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>EntityAlreadyExists</Code>" +
            "<Message>The request was rejected because it attempted to " +
            "create an account that already exists.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = accountController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CONFLICT,
                response.getResponseStatus());
    }

    @Test public void
    CreateAccount_UniqueCanonicalIdGenerationFailed_ReturnInternalServerError()
        throws Exception {
      Account account = new Account();
      account.setName("s3test");

      Mockito.when(accountDAO.find("s3test")).thenReturn(account);
      Mockito.doReturn(account).when(accountDAO).findByCanonicalID("can1234");

      final String expectedResponseBody =
          "<?xml version=\"1.0\" " + "encoding=\"UTF-8\" standalone=\"no\"?>" +
          "<ErrorResponse xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
          "<Error><Code>InternalFailure</Code>" +
          "<Message>The request processing has failed because of an " +
          "unknown error, exception or failure.</Message></Error>" +
          "<RequestId>0000</RequestId>" + "</ErrorResponse>";

      ServerResponse response = accountController.create();
      Assert.assertEquals(expectedResponseBody, response.getResponseBody());
      Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                          response.getResponseStatus());
    }

    @Test
    public void CreateAccount_AccountSaveFailed_ReturnInternalServerError()
            throws Exception {
        Account account = new Account();
        account.setName("s3test");

        Mockito.doReturn(account).when(accountDAO).find("s3test");
        Mockito.doThrow(new DataAccessException("failed to add new account.\n"))
            .when(accountDAO)
            .save(account);
        Mockito.doReturn(new Account()).when(accountDAO).findByCanonicalID(
            "can1234");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = accountController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccount_FailedToCreateRootUser_ReturnInternalServerError()
            throws Exception {
        Account account = new Account();
        account.setName("s3test");

        Mockito.doReturn(account).when(accountDAO).find("s3test");
        Mockito.doNothing().when(accountDAO).save(any(Account.class));
        Mockito.doThrow(new DataAccessException("failed to save new user.\n"))
                .when(userDAO).save(any(User.class));
        Mockito.doReturn(new Account()).when(accountDAO).findByCanonicalID(
            "can1234");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = accountController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void CreateAccount_FailedToCreateRootAccessKey_ReturnInternalServerError()
            throws Exception {
        Account account = new Account();
        account.setName("s3test");

        Mockito.doReturn(account).when(accountDAO).find("s3test");
        Mockito.doNothing().when(accountDAO).save(any(Account.class));
        Mockito.doNothing().when(userDAO).save(any(User.class));
        Mockito.doThrow(new DataAccessException(
                            "failed to save root access key.\n"))
            .when(accessKeyDAO)
            .save(any(AccessKey.class));
        Mockito.doReturn(new Account()).when(accountDAO).findByCanonicalID(
            "can1234");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = accountController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test public void CreateAccount_Success_ReturnCreateResponse()
        throws Exception {
        Account account = new Account();
        account.setName("s3test");
        ServerResponse resp = new ServerResponse();
        resp.setResponseStatus(HttpResponseStatus.OK);

        PowerMockito.doReturn("AKIASIAS")
            .when(KeyGenUtil.class, "createUserAccessKeyId");

        PowerMockito.doReturn("htuspscae/123")
            .when(KeyGenUtil.class, "generateSecretKey");

        Mockito.doReturn(account).when(accountDAO).find("s3test");
        Mockito.doNothing().when(accountDAO).save(any(Account.class));
        Mockito.doNothing().when(userDAO).save(any(User.class));
        Mockito.doNothing().when(accessKeyDAO).save(any(AccessKey.class));
        Mockito.doReturn(resp).when(s3).notifyNewAccount(
            any(String.class), any(String.class), any(String.class));
        Mockito.doReturn(new Account()).when(accountDAO).findByCanonicalID(
            "can1234");

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<CreateAccountResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<CreateAccountResult>" + "<Account>" +
            "<AccountId>987654352188</AccountId>" +
            "<CanonicalId>can1234</CanonicalId>" +
            "<AccountName>s3test</AccountName>" +
            "<RootUserName>root</RootUserName>" +
            "<AccessKeyId>AKIASIAS</AccessKeyId>" +
            "<RootSecretKeyId>htuspscae/123</RootSecretKeyId>" +
            "<Status>Active</Status>" + "</Account>" +
            "</CreateAccountResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</CreateAccountResponse>";

        ServerResponse response = accountController.create();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());
    }

    @Test
    public void ResetAccountAccessKey_AccountDoesNotExists_ReturnNoSuchEntity()
            throws Exception {
        Account account = mock(Account.class);
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(account.exists()).thenReturn(Boolean.FALSE);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"" +
            " standalone=\"no\"?><ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/" +
            "doc/2010-05-08/\"><Error><Code>NoSuchEntity</Code><Message>The " +
            "request" +
            " was rejected because it referenced an entity that does not " +
            "exist. " +
            "</Message></Error><RequestId>0000</RequestId></ErrorResponse>";

        ServerResponse response = accountController.resetAccountAccessKey();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                response.getResponseStatus());
    }

    @Test
    public void ResetAccountAccessKey_UserDAOException_InternalError()
            throws Exception {
        Account account = mock(Account.class);
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(account.exists()).thenReturn(Boolean.TRUE);
        Mockito.when(account.getName()).thenReturn("root");
        Mockito.doThrow(new DataAccessException("failed to get root user"))
            .when(userDAO)
            .find(any(String.class), any(String.class));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = accountController.resetAccountAccessKey();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void ResetAccountAccessKey_NoRootUser_ReturnNoSuchEntity()
            throws Exception {
        Account account = mock(Account.class);
        User root = mock(User.class);
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(account.exists()).thenReturn(Boolean.TRUE);
        Mockito.when(account.getName()).thenReturn("root");
        Mockito.when(userDAO.find(any(String.class), any(String.class)))
            .thenReturn(root);
        Mockito.when(root.exists()).thenReturn(Boolean.FALSE);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"" +
            " standalone=\"no\"?><ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/" +
            "doc/2010-05-08/\"><Error><Code>NoSuchEntity</Code><Message>The " +
            "request" +
            " was rejected because it referenced an entity that does not " +
            "exist. " +
            "</Message></Error><RequestId>0000</RequestId></ErrorResponse>";

        ServerResponse response = accountController.resetAccountAccessKey();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                response.getResponseStatus());
    }

    @Test public void ResetAccountAccessKey_Success_Return() throws Exception {
        Account account = new Account();
        account.setName("s3test");
        account.setId("987654352188");
        account.setCanonicalId("can1234");
        User root = new User();
        root.setName("root");
        root.setId("AKIASIAS");
        AccessKey[] accessKeys = new AccessKey[1];

        PowerMockito.doReturn("AKIASIAS")
            .when(KeyGenUtil.class, "createUserAccessKeyId");

        PowerMockito.doReturn("htuspscae/123")
            .when(KeyGenUtil.class, "generateSecretKey");

        accessKeys[0] = mock(AccessKey.class);
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(userDAO.find(any(String.class), any(String.class)))
            .thenReturn(root);

        Mockito.when(accessKeyDAO.findAll(root)).thenReturn(accessKeys);
        Mockito.doNothing().when(accessKeyDAO).delete(accessKeys[0]);

        Mockito.doNothing().when(accessKeyDAO).save(any(AccessKey.class));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" +
            "<ResetAccountAccessKeyResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<ResetAccountAccessKeyResult>" + "<Account>" +
            "<AccountId>987654352188</AccountId>" +
            "<CanonicalId>can1234</CanonicalId>" +
            "<AccountName>s3test</AccountName>" +
            "<RootUserName>root</RootUserName>" +
            "<AccessKeyId>AKIASIAS</AccessKeyId>" +
            "<RootSecretKeyId>htuspscae/123</RootSecretKeyId>" +
            "<Status>Active</Status>" + "</Account>" +
            "</ResetAccountAccessKeyResult>" + "<ResponseMetadata>" +
            "<RequestId>0000</RequestId>" + "</ResponseMetadata>" +
            "</ResetAccountAccessKeyResponse>";

        ServerResponse response = accountController.resetAccountAccessKey();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CREATED,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccount_AccountsSearchFailed_ReturnInternalServerError()
            throws DataAccessException {
        Mockito.when(accountDAO.find("s3test")).thenThrow(
                new DataAccessException("Failed to fetch accounts.\n"));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" " +
            "encoding=\"UTF-8\" standalone=\"no\"?>" + "<ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc/2010-05-08/\">" +
            "<Error><Code>InternalFailure</Code>" +
            "<Message>The request processing has failed because of an " +
            "unknown error, exception or failure.</Message></Error>" +
            "<RequestId>0000</RequestId>" + "</ErrorResponse>";

        ServerResponse response = accountController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.INTERNAL_SERVER_ERROR,
                response.getResponseStatus());
    }

    @Test
    public void DeleteAccount_AccountDoesNotExists_ReturnNoSuchEntity()
            throws Exception {
        Account account = mock(Account.class);
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(account.exists()).thenReturn(Boolean.FALSE);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"" +
            " standalone=\"no\"?><ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/" +
            "doc/2010-05-08/\"><Error><Code>NoSuchEntity</Code><Message>The " +
            "request" +
            " was rejected because it referenced an entity that does not " +
            "exist. " +
            "</Message></Error><RequestId>0000</RequestId></ErrorResponse>";

        ServerResponse response = accountController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                            response.getResponseStatus());
    }

    @Test
    public void DeleteAccount_AccountExists_ReturnUnauthorisedResponse()
            throws Exception {
        User[] users = new User[1];
        users[0] = new User();
        users[0].setName("root");
        users[0].setId("rootxyz");
        AccessKey[] accessKeys = new AccessKey[1];
        accessKeys[0] = mock(AccessKey.class);
        Account account = mock(Account.class);

        Mockito.when(account.getName()).thenReturn("s3test");
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(account.exists()).thenReturn(Boolean.TRUE);
        Mockito.when(userDAO.find("s3test", "root")).thenReturn(users[0]);
        Mockito.when(requestor.getId()).thenReturn("abcxyz");
        Mockito.when(userDAO.findAll("s3test", "/")).thenReturn(users);
        Mockito.when(accessKeyDAO.findAll(users[0])).thenReturn(accessKeys);

        final String expectedResponseBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
            "standalone=\"no\"?><ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc" +
            "/2010-05-08/\"><Error><Code>UnauthorizedOperation</" +
            "Code><Message>You " +
            "are not authorized to perform this operation. Check your IAM " +
            "policies" +
            ", and ensure that you are using the correct access keys. " +
            "</Message>" +
            "</Error><RequestId>0000</RequestId></ErrorResponse>";

        ServerResponse response = accountController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.UNAUTHORIZED,
                            response.getResponseStatus());
    }

    @Test
    public void DeleteAccount_AccountExists_ReturnDeleteResponse()
            throws Exception {
        User[] users = new User[1];
        users[0] = new User();
        users[0].setName("root");
        users[0].setId("rootxyz");
        AccessKey aKey = new AccessKey();

        aKey.setId("akey123");
        aKey.setSecretKey("skey1234");

        ServerResponse resp = new ServerResponse();
        resp.setResponseStatus(HttpResponseStatus.OK);

        AccessKey[] accessKeys = new AccessKey[1];
        accessKeys[0] = mock(AccessKey.class);
        Account account = mock(Account.class);

        Mockito.when(account.getName()).thenReturn("s3test");
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(account.exists()).thenReturn(Boolean.TRUE);
        Mockito.when(userDAO.find("s3test", "root")).thenReturn(users[0]);
        Mockito.when(requestor.getId()).thenReturn("rootxyz");
        Mockito.when(requestor.getAccesskey()).thenReturn(aKey);
        Mockito.when(userDAO.findAll("s3test", "/")).thenReturn(users);
        Mockito.when(accessKeyDAO.findAll(users[0])).thenReturn(accessKeys);
        Mockito.doReturn(resp).when(s3).notifyDeleteAccount(
            any(String.class), any(String.class), any(String.class),
            any(String.class));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
            "standalone=\"no\"?><DeleteAccountResponse " +
            "xmlns=\"https://iam.seagate" +
            ".com/doc/2010-05-08/\"><ResponseMetadata><RequestId>0000</" +
            "RequestId><" + "/ResponseMetadata></DeleteAccountResponse>";

        ServerResponse response = accountController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());

        Mockito.verify(accessKeyDAO).delete(accessKeys[0]);
        Mockito.verify(userDAO).delete(users[0]);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.USER_OU);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.ROLE_OU);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.GROUP_OU);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.POLICY_OU);
        Mockito.verify(accountDAO).delete(account);
    }

    @Test
    public void DeleteAccount_HasSubResources_ReturnDeleteConflict()
            throws Exception {
        User[] users = new User[2];
        users[0] = new User();
        users[0].setName("root");
        users[0].setId("rootxyz");
        users[1] = new User();
        users[1].setName("s3user");

        AccessKey aKey = new AccessKey();

        aKey.setId("akey123");
        aKey.setSecretKey("skey1234");

        ServerResponse resp = new ServerResponse();
        resp.setResponseStatus(HttpResponseStatus.OK);

        AccessKey[] accessKeys = new AccessKey[1];
        accessKeys[0] = mock(AccessKey.class);
        Account account = mock(Account.class);

        Mockito.when(account.getName()).thenReturn("s3test");
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(account.exists()).thenReturn(Boolean.TRUE);
        Mockito.when(userDAO.findAll("s3test", "/")).thenReturn(users);
        Mockito.when(userDAO.find("s3test", "root")).thenReturn(users[0]);
        Mockito.when(requestor.getId()).thenReturn("rootxyz");
        Mockito.when(requestor.getAccesskey()).thenReturn(aKey);
        Mockito.when(accessKeyDAO.findAll(users[0])).thenReturn(accessKeys);
        Mockito.doThrow(new DataAccessException(
                            "subordinate objects must be deleted first"))
            .when(accountDAO)
            .deleteOu(account, LDAPUtils.USER_OU);
        Mockito.doReturn(resp).when(s3).notifyDeleteAccount(
            any(String.class), any(String.class), any(String.class),
            any(String.class));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
            "standalone=\"no\"?><ErrorResponse " +
            "xmlns=\"https://iam.seagate.com/doc" +
            "/2010-05-08/\"><Error><Code>DeleteConflict</Code><Message>The " +
            "request" +
            " was rejected because it attempted to delete a resource that " +
            "has " + "attached " +
            "subordinate entities. The error message describes these " +
            "entities.</Message>" +
            "</Error><RequestId>0000</RequestId></ErrorResponse>";

        ServerResponse response = accountController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.CONFLICT,
                            response.getResponseStatus());

        Mockito.verify(accessKeyDAO, times(0)).delete(accessKeys[0]);
        Mockito.verify(userDAO, times(0)).delete(users[0]);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.USER_OU);
    }

    @Test
    public void DeleteAccount_ForceDelete_ReturnDeleteResponse()
            throws Exception {
        requestBody.put("force", "true");
        User[] users = new User[1];
        users[0] = new User();
        users[0].setName("root");
        users[0].setId("rootxyz");

        AccessKey aKey = new AccessKey();

        aKey.setId("akey123");
        aKey.setSecretKey("skey1234");

        AccessKey[] accessKeys = new AccessKey[1];
        accessKeys[0] = mock(AccessKey.class);
        Role[] roles = new Role[1];
        roles[0] = mock(Role.class);
        Account account = mock(Account.class);

        ServerResponse resp = new ServerResponse();
        resp.setResponseStatus(HttpResponseStatus.OK);

        Mockito.when(account.getName()).thenReturn("s3test");
        Mockito.when(accountDAO.find("s3test")).thenReturn(account);
        Mockito.when(account.exists()).thenReturn(Boolean.TRUE);
        Mockito.when(userDAO.find("s3test", "root")).thenReturn(users[0]);
        Mockito.when(requestor.getId()).thenReturn("rootxyz");
        Mockito.when(requestor.getAccesskey()).thenReturn(aKey);
        Mockito.when(userDAO.findAll("s3test", "/")).thenReturn(users);
        Mockito.when(accessKeyDAO.findAll(users[0])).thenReturn(accessKeys);
        Mockito.when(roleDAO.findAll(account, "/")).thenReturn(roles);
        Mockito.doReturn(resp).when(s3).notifyDeleteAccount(
            any(String.class), any(String.class), any(String.class),
            any(String.class));

        final String expectedResponseBody =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" " +
            "standalone=\"no\"?><DeleteAccountResponse " +
            "xmlns=\"https://iam.seagate" +
            ".com/doc/2010-05-08/\"><ResponseMetadata><RequestId>0000</" +
            "RequestId><" + "/ResponseMetadata></DeleteAccountResponse>";

        ServerResponse response = accountController.delete();
        Assert.assertEquals(expectedResponseBody, response.getResponseBody());
        Assert.assertEquals(HttpResponseStatus.OK,
                            response.getResponseStatus());

        Mockito.verify(accessKeyDAO).delete(accessKeys[0]);
        Mockito.verify(userDAO).delete(users[0]);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.USER_OU);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.ROLE_OU);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.GROUP_OU);
        Mockito.verify(accountDAO).deleteOu(account, LDAPUtils.POLICY_OU);
        Mockito.verify(accountDAO).delete(account);
        Mockito.verify(roleDAO).delete(roles[0]);
    }
}


