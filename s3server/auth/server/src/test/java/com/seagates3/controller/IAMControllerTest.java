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
 * Original author: Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 03-Feb-2017
 */

package com.seagates3.controller;

import static org.hamcrest.CoreMatchers.containsString;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.powermock.api.mockito.PowerMockito.doReturn;
import static org.powermock.api.mockito.PowerMockito.mock;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.spy;
import static org.powermock.api.mockito.PowerMockito.verifyPrivate;
import static org.powermock.api.mockito.PowerMockito.when;
import static org.powermock.api.mockito.PowerMockito.whenNew;

import java.util.Map;
import java.util.TreeMap;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;
import org.powermock.reflect.internal.WhiteboxImpl;

import com.seagates3.acl.ACLValidation;
import com.seagates3.authentication.ClientRequestParser;
import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.authentication.SignatureValidator;
import com.seagates3.authorization.Authorizer;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.authserver.IAMResourceMapper;
import com.seagates3.authserver.ResourceMap;
import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.GroupDAO;
import com.seagates3.dao.PolicyDAO;
import com.seagates3.dao.RoleDAO;
import com.seagates3.dao.UserDAO;
import com.seagates3.dao.ldap.AccessKeyImpl;
import com.seagates3.dao.ldap.AccountImpl;
import com.seagates3.dao.ldap.GroupImpl;
import com.seagates3.dao.ldap.PolicyImpl;
import com.seagates3.dao.ldap.RoleImpl;
import com.seagates3.dao.ldap.UserImpl;
import com.seagates3.exception.AuthResourceNotFoundException;
import com.seagates3.exception.InternalServerException;
import com.seagates3.exception.InvalidAccessKeyException;
import com.seagates3.exception.InvalidRequestorException;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AuthenticationResponseGenerator;
import com.seagates3.response.generator.AuthorizationResponseGenerator;
import com.seagates3.service.RequestorService;
import com.seagates3.util.BinaryUtil;

import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpResponseStatus;

@RunWith(PowerMockRunner.class) @MockPolicy(Slf4jMockPolicy.class)
    @PowerMockIgnore({"javax.management.*"}) @PrepareForTest(
        {IAMResourceMapper.class, IAMController.class,
         RequestorService.class,  ClientRequestParser.class,
         Authorizer.class,        AuthenticationResponseGenerator.class,
         AuthServerConfig.class,  DAODispatcher.class,
         ACLValidation.class}) public class IAMControllerTest {

 private
  IAMController controller;
 private
  FullHttpRequest httpRequest;
 private
  Map<String, String> requestBody;
 private
  ServerResponse serverResponse;
 private
  ResourceMap resourceMap;
 private
  Requestor requestor;
 private
  ClientRequestToken clientRequestToken;
 private
  SignatureValidator signatureValidator;
 private
  String acl;
  @Before public void setUp() throws Exception {
    mockStatic(IAMResourceMapper.class);
    mockStatic(RequestorService.class);
    mockStatic(ClientRequestParser.class);
    mockStatic(AuthServerConfig.class);
    mockStatic(ClientRequestToken.class);
    mockStatic(ACLValidation.class);

    resourceMap = mock(ResourceMap.class);
    httpRequest = mock(FullHttpRequest.class);
    serverResponse = mock(ServerResponse.class);
    requestor = mock(Requestor.class);
    clientRequestToken = mock(ClientRequestToken.class);
    signatureValidator = mock(SignatureValidator.class);

    controller = new IAMController();
    requestBody = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
  }

  @BeforeClass public static void setUpBeforeClass() throws Exception {
    AuthServerConfig.authResourceDir = "../resources";
  }

  @Test public void serveTest_Action_CreateAccount() throws Exception {
    requestBody.put("Action", "CreateAccount");
    IAMController controllerSpy = spy(controller);
    when(IAMResourceMapper.getResourceMap("CreateAccount"))
        .thenReturn(resourceMap);
    when(AuthServerConfig.getLdapLoginCN()).thenReturn("admin");
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(clientRequestToken, requestor))
        .thenReturn(serverResponse);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(clientRequestToken.getAccessKeyId()).thenReturn("admin");
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    whenNew(Requestor.class).withNoArguments().thenReturn(requestor);
    doReturn(Boolean.TRUE)
        .when(controllerSpy, "validateRequest", resourceMap, requestBody);
    doReturn(serverResponse).when(controllerSpy, "performAction", resourceMap,
                                  requestBody, requestor);

    ServerResponse response = controllerSpy.serve(httpRequest, requestBody);

    assertEquals(serverResponse, response);
    verifyPrivate(controllerSpy)
        .invoke("validateRequest", resourceMap, requestBody);
    verifyPrivate(controllerSpy)
        .invoke("performAction", resourceMap, requestBody, requestor);
  }

  @Test public void serveTest_InvalidAccessKeyException() throws Exception {
    requestBody.put("Action", "CreateUser");
    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    InvalidAccessKeyException exception = mock(InvalidAccessKeyException.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(exception.getServerResponse()).thenReturn(serverResponse);
    when(RequestorService.getRequestor(clientRequestToken))
        .thenThrow(exception);

    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(serverResponse, response);
  }

  @Test public void serveTest_InternalServerException() throws Exception {
    requestBody.put("Action", "CreateUser");
    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    InternalServerException exception = mock(InternalServerException.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(exception.getServerResponse()).thenReturn(serverResponse);
    when(RequestorService.getRequestor(clientRequestToken))
        .thenThrow(exception);

    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(serverResponse, response);
  }

  @Test public void serveTest_InvalidRequestorException() throws Exception {
    requestBody.put("Action", "CreateUser");
    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    InvalidRequestorException exception = mock(InvalidRequestorException.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(exception.getServerResponse()).thenReturn(serverResponse);
    when(RequestorService.getRequestor(clientRequestToken))
        .thenThrow(exception);

    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(serverResponse, response);
  }

  @Test public void serveTest_AccessDenied_No_Authheader() throws Exception {
    requestBody.put("Action", "AuthenticateUser");
    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(new AuthorizationResponseGenerator().AccessDenied(), response);
  }

  @Test public void serveTest_AccessDenied_Empty_Authheader() throws Exception {
    requestBody.put("Action", "AuthenticateUser");
    requestBody.put("authorization", "");
    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(new AuthorizationResponseGenerator().AccessDenied(), response);
  }

  @Test public void serveTest_AuthorizeUser() throws Exception {
    String expectedResponseBody =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=" +
        "\"no\"?><AuthorizeUserResponse " +
        "xmlns=\"https://iam.seagate.com/doc/2010-05-08/" +
        "\"><AuthorizeUserResult><UserId>MH12</UserId><UserName>tylerdurden</" +
        "UserName>" + "<AccountId>NS5144</AccountId><AccountName>jack</" +
        "AccountName><CanonicalId>MH12</CanonicalId></" + "AuthorizeUser" +
        "Result><ResponseMetadata><RequestId>0000</RequestId></" +
        "ResponseMetadata>" + "</AuthorizeUserResponse>";

    acl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
          "<AccessControlPolicy " +
          "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" +
          " <Owner>\n" + "  <ID>MH12</ID>\n" +
          "  <DisplayName>Owner_Name</DisplayName>\n" + " </Owner>\n" +
          " <AccessControlList>\n" + "  <Grant>\n" + "   <Grantee " +
          "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n" +
          "      xsi:type=\"CanonicalUser\">\n" + "    <ID>MH12</ID>\n" +
          "    <DisplayName>Grantee_Name</DisplayName>\n" + "   </Grantee>\n" +
          "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
          " </AccessControlList>\n" + "</AccessControlPolicy>\n";

    requestBody.put("Action", "AuthorizeUser");
    requestBody.put("Authorization", "abc");
    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    Account account = mock(Account.class);
    whenNew(Requestor.class).withNoArguments().thenReturn(requestor);
    when(AuthServerConfig.getReqId()).thenReturn("0000");
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(requestor.getId()).thenReturn("MH12");
    when(requestor.getName()).thenReturn("tylerdurden");
    when(requestor.getAccount()).thenReturn(account);

    when(account.getId()).thenReturn("NS5144");
    when(account.getName()).thenReturn("jack");
    when(account.getCanonicalId()).thenReturn("MH12");
    when(RequestorService.getRequestor(clientRequestToken))
        .thenReturn(requestor);
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", "/seagatebucket-aj01/dir-1/abc1");
    requestBody.put("ACL", BinaryUtil.encodeToBase64String(acl));

    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
    assertEquals(expectedResponseBody, response.getResponseBody());
  }

  @Test public void serveTest_IncorrectSignature() throws Exception {
    requestBody.put("Action", "CreateUser");
    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    SignatureValidator signatureValidator = mock(SignatureValidator.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(RequestorService.getRequestor(clientRequestToken))
        .thenReturn(requestor);
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(clientRequestToken, requestor))
        .thenReturn(serverResponse);
    when(serverResponse.getResponseStatus())
        .thenReturn(HttpResponseStatus.UNAUTHORIZED);

    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(serverResponse, response);
    assertEquals(HttpResponseStatus.UNAUTHORIZED, response.getResponseStatus());
  }

  @Test public void serveTest_AuthenticateUser() throws Exception {
    requestBody.put("Action", "AuthenticateUser");
    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    SignatureValidator signatureValidator = mock(SignatureValidator.class);

    Account account = mock(Account.class);
    when(requestor.getName()).thenReturn("tylerdurden");
    when(requestor.getAccount()).thenReturn(account);
    when(account.getId()).thenReturn("NS5144");
    when(account.getName()).thenReturn("jack");

    AuthenticationResponseGenerator responseGenerator =
        mock(AuthenticationResponseGenerator.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(RequestorService.getRequestor(clientRequestToken))
        .thenReturn(requestor);
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(clientRequestToken, requestor))
        .thenReturn(serverResponse);
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    whenNew(AuthenticationResponseGenerator.class).withNoArguments().thenReturn(
        responseGenerator);
    when(responseGenerator.generateAuthenticatedResponse(
             requestor, clientRequestToken)).thenReturn(serverResponse);
    when(serverResponse.getResponseBody()).thenReturn("<AuthenticateUser>");

    IAMController controller = new IAMController();
    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(serverResponse, response);
    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  @Test public void serveTest_UnAuthenticatedUser() throws Exception {
    requestBody.put("Action", "AuthenticateUser");
    requestBody.put("authorization", "AWS AKIAIOSFODN&EXAMPL*#" +
                                         "frJIUN8DYpKDtOLCwo//yllqDzg=");
    when(AuthServerConfig.getReqId()).thenReturn("0000");

    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenCallRealMethod();

    ServerResponse response = controller.serve(httpRequest, requestBody);
    assertThat(response.getResponseBody(),
               containsString(
                   "The AWS access key Id you provided does not exist in our " +
                   "records."));
    assertEquals(HttpResponseStatus.UNAUTHORIZED, response.getResponseStatus());
  }

  @Test public void serveTest_AccessKeyWithSpace() throws Exception {
    requestBody.put("Action", "AuthenticateUser");
    requestBody.put("authorization", "AWS AKIA IOSFODN&EXAMPL$#:" +
                                         "frJIUN8DYpKDtOLCwo//yllqDzg=");
    when(AuthServerConfig.getReqId()).thenReturn("0000");

    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenCallRealMethod();

    ServerResponse response = controller.serve(httpRequest, requestBody);
    assertThat(response.getResponseBody(), containsString("Invalid Argument"));
    assertEquals(HttpResponseStatus.BAD_REQUEST, response.getResponseStatus());
  }

  @Test public void serveTest_ValidateACL() throws Exception {
    String expectedResponseBody = "Action successful";

    acl = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
          "<AccessControlPolicy " +
          "xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n" +
          " <Owner>\n" + "  <ID>MH12</ID>\n" +
          "  <DisplayName>tylerdurden</DisplayName>\n" + " </Owner>\n" +
          " <AccessControlList>\n" + "  <Grant>\n" + "   <Grantee " +
          "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n" +
          "      xsi:type=\"CanonicalUser\">\n" + "    <ID>MH12</ID>\n" +
          "    <DisplayName>tylerdurden</DisplayName>\n" + "   </Grantee>\n" +
          "   <Permission>READ</Permission>\n" + "  </Grant>\n" +
          " </AccessControlList>\n" + "</AccessControlPolicy>\n";

    requestBody.put("Action", "ValidateACL");
    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    Account account = mock(Account.class);
    when(AuthServerConfig.getReqId()).thenReturn("0000");
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(requestor.getId()).thenReturn("MH12");
    when(requestor.getName()).thenReturn("tylerdurden");
    when(requestor.getAccount()).thenReturn(account);

    when(account.getId()).thenReturn("MH12");
    when(account.getName()).thenReturn("tylerdurden");
    when(account.getCanonicalId()).thenReturn("MH12");
    when(RequestorService.getRequestor(clientRequestToken))
        .thenReturn(requestor);
    PowerMockito.when(ACLValidation.class, "checkIdExists", "MH12",
                      "tylerdurden").thenReturn(true);
    requestBody.put("Method", "GET");
    requestBody.put("ClientAbsoluteUri", "/seagatebucket-aj01/dir-1/abc1");
    requestBody.put("ACL", acl);

    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
    assertEquals(expectedResponseBody, response.getResponseBody());
  }

  @Test public void serveTest_ValidV2Request() throws Exception {
    requestBody.put("Action", "AuthenticateUser");
    requestBody.put("host", "s3.seagate.com");
    requestBody.put("authorization", "AWS RqkWRyVIQrq5Aq9eMUt1HQ:" +
                                         "frJIUN8DYpKDtOLCwo//yllqDzg=");

    SignatureValidator signatureValidator = mock(SignatureValidator.class);
    Account account = mock(Account.class);
    when(requestor.getName()).thenReturn("tyle/rdurden");
    when(requestor.getAccount()).thenReturn(account);
    when(account.getId()).thenReturn("NS5144");
    when(account.getName()).thenReturn("jack");

    AuthenticationResponseGenerator responseGenerator =
        mock(AuthenticationResponseGenerator.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenCallRealMethod();
    PowerMockito.when(ClientRequestParser.class, "getAWSRequestParser",
                      any(ClientRequestToken.AWSSigningVersion.class))
        .thenCallRealMethod();
    PowerMockito.when(ClientRequestParser.class, "toRequestParserClass",
                      anyString()).thenCallRealMethod();
    String[] endpoints = {"s3-us.seagate.com", "s3-europe.seagate.com",
                          "s3-asia.seagate.com"};
    PowerMockito.mockStatic(AuthServerConfig.class);
    PowerMockito.doReturn(endpoints)
        .when(AuthServerConfig.class, "getEndpoints");
    when(RequestorService.getRequestor(any(ClientRequestToken.class)))
        .thenReturn(requestor);
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(any(ClientRequestToken.class),
                                     eq(requestor))).thenReturn(serverResponse);
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    whenNew(AuthenticationResponseGenerator.class).withNoArguments().thenReturn(
        responseGenerator);
    when(responseGenerator.generateAuthenticatedResponse(
             eq(requestor), any(ClientRequestToken.class)))
        .thenReturn(serverResponse);
    when(serverResponse.getResponseBody()).thenReturn("<AuthenticateUser>");

    IAMController controller = new IAMController();
    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  @Test public void serveTest_ValidV4Request() throws Exception {
    requestBody.put("Action", "AuthenticateUser");
    requestBody.put("host", "s3.seagate.com");
    requestBody.put("authorization",
                    "AWS4-HMAC-SHA256w Credential=AKIAJTYX36YCKQ" +
                        "SAJT7Q/20190223/US/s3/" +
                        "aws4_request,SignedHeaders=host;x-amz-content" +
                        "-sha256;x-amz-date,Signature=" +
                        "63c12bb559dafad96dad0ed9630d76986a6c21f" +
                        "7d0702a767da78ed0342dda28");

    SignatureValidator signatureValidator = mock(SignatureValidator.class);
    Account account = mock(Account.class);
    when(requestor.getName()).thenReturn("tyle/rdurden");
    when(requestor.getAccount()).thenReturn(account);
    when(account.getId()).thenReturn("NS5144");
    when(account.getName()).thenReturn("jack");

    AuthenticationResponseGenerator responseGenerator =
        mock(AuthenticationResponseGenerator.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenCallRealMethod();
    PowerMockito.when(ClientRequestParser.class, "getAWSRequestParser",
                      any(ClientRequestToken.AWSSigningVersion.class))
        .thenCallRealMethod();
    PowerMockito.when(ClientRequestParser.class, "toRequestParserClass",
                      anyString()).thenCallRealMethod();
    String[] endpoints = {"s3-us.seagate.com", "s3-europe.seagate.com",
                          "s3-asia.seagate.com"};
    PowerMockito.mockStatic(AuthServerConfig.class);
    PowerMockito.doReturn(endpoints)
        .when(AuthServerConfig.class, "getEndpoints");
    when(RequestorService.getRequestor(any(ClientRequestToken.class)))
        .thenReturn(requestor);
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(any(ClientRequestToken.class),
                                     eq(requestor))).thenReturn(serverResponse);
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    whenNew(AuthenticationResponseGenerator.class).withNoArguments().thenReturn(
        responseGenerator);
    when(responseGenerator.generateAuthenticatedResponse(
             eq(requestor), any(ClientRequestToken.class)))
        .thenReturn(serverResponse);
    when(serverResponse.getResponseBody()).thenReturn("<AuthenticateUser>");

    IAMController controller = new IAMController();
    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  @Test public void serveTest_InvalidParams() throws Exception {
    requestBody.put("Action", "ListAccounts");
    IAMController controllerSpy = spy(controller);
    when(IAMResourceMapper.getResourceMap("ListAccounts"))
        .thenReturn(resourceMap);
    when(AuthServerConfig.getLdapLoginCN()).thenReturn("admin");
    when(AuthServerConfig.getReqId()).thenReturn("0000");
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(clientRequestToken, requestor))
        .thenReturn(serverResponse);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(clientRequestToken.getAccessKeyId()).thenReturn("admin");
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    whenNew(Requestor.class).withNoArguments().thenReturn(requestor);
    doReturn(Boolean.FALSE)
        .when(controllerSpy, "validateRequest", resourceMap, requestBody);

    ServerResponse response = controllerSpy.serve(httpRequest, requestBody);

    assertThat(response.getResponseBody(),
               containsString(
                   "An invalid or out-of-range value was supplied for the " +
                   "input parameter."));
    assertEquals(HttpResponseStatus.BAD_REQUEST, response.getResponseStatus());
    verifyPrivate(controllerSpy)
        .invoke("validateRequest", resourceMap, requestBody);
  }

  @Test public void serveTest_AuthResourceNotFoundException() throws Exception {
    requestBody.put("Action", "ListAccounts");
    IAMController controllerSpy = spy(controller);
    when(IAMResourceMapper.getResourceMap("ListAccounts"))
        .thenThrow(AuthResourceNotFoundException.class);
    when(AuthServerConfig.getLdapLoginCN()).thenReturn("admin");
    when(AuthServerConfig.getReqId()).thenReturn("0000");
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(clientRequestToken, requestor))
        .thenReturn(serverResponse);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(clientRequestToken.getAccessKeyId()).thenReturn("admin");
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    whenNew(Requestor.class).withNoArguments().thenReturn(requestor);
    doReturn(Boolean.FALSE)
        .when(controllerSpy, "validateRequest", resourceMap, requestBody);

    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertThat(response.getResponseBody(),
               containsString("The requested operation is not supported."));
    assertEquals(HttpResponseStatus.UNAUTHORIZED, response.getResponseStatus());
  }

  @Test public void serveTest_InvalidLdapUser() throws Exception {
    requestBody.put("Action", "CreateAccount");
    when(AuthServerConfig.getLdapLoginCN()).thenReturn("admin");
    when(AuthServerConfig.getReqId()).thenReturn("0000");
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(clientRequestToken.getAccessKeyId()).thenReturn("user");

    ServerResponse response = controller.serve(httpRequest, requestBody);

    assertThat(response.getResponseBody(),
               containsString("The Ldap user id you provided does not exist."));
    assertEquals(HttpResponseStatus.UNAUTHORIZED, response.getResponseStatus());
  }

  @Test public void validateRequestTest() throws Exception {
    when(resourceMap.getParamValidatorClass()).thenReturn(
        "com.seagates3.parameter.validator.AccountParameterValidator");
    when(resourceMap.getParamValidatorMethod())
        .thenReturn("isValidCreateParams");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertFalse(result);
  }

  /**
   * Test Scenario for :
   * {@link IAMController#validateRequest(ResourceMap, Map)}
   * When valid request is passed
   * @throws Exception
   */
  @Test public void validateRequestTest_Success() throws Exception {
    requestBody.put("Action", "CreateAccount");
    requestBody.put("Email", "xyz@email.com");
    requestBody.put("AccountName", "valid-name");
    resourceMap = new ResourceMap("Account", "create");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertTrue(result);
  }

  /**
   * Test Scenario for :
   * {@link IAMController#validateRequest(ResourceMap, Map)}
   * When invalid request is passed in the form of email
   * @throws Exception
   */
  @Test public void validateRequestTest_InvalidEmail() throws Exception {
    requestBody.put("Action", "CreateAccount");
    requestBody.put("Email", "xyz-email.com");
    requestBody.put("AccountName", "valid-name");
    resourceMap = new ResourceMap("Account", "create");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertFalse(result);
  }

  /**
   * Test Scenario for :
   * {@link IAMController#validateRequest(ResourceMap, Map)}
   * When invalid request is passed in the form of invalid account name
   * @throws Exception
   */
  @Test public void validateRequestTest_InvalidAccountName() throws Exception {
    requestBody.put("Action", "CreateAccount");
    requestBody.put("Email", "xyz@email.com");
    requestBody.put("AccountName", "invalid!name");
    resourceMap = new ResourceMap("Account", "create");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertFalse(result);
  }

  /**
   * Test Scenario for :
   * {@link IAMController#validateRequest(ResourceMap, Map)}
   * When invalid request is passed in the form of null account name
   * @throws Exception
   */
  @Test public void validateRequestTest_NullAccountName() throws Exception {
    requestBody.put("Action", "CreateAccount");
    requestBody.put("Email", "xyz@email.com");
    requestBody.put("AccountName", null);
    resourceMap = new ResourceMap("Account", "create");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertFalse(result);
  }

  /**
   * Test Scenario for :
   * {@link IAMController#validateRequest(ResourceMap, Map)}
   * When invalid request is passed in the form of empty account name
   * @throws Exception
   */
  @Test public void validateRequestTest_EmptyAccountName() throws Exception {
    requestBody.put("Action", "CreateAccount");
    requestBody.put("Email", "xyz@email.com");
    requestBody.put("AccountName", "");
    resourceMap = new ResourceMap("Account", "create");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertFalse(result);
  }

  @Test public void validateRequestTest_ClassNotFoundException()
      throws Exception {
    when(resourceMap.getParamValidatorClass()).thenReturn(
        "com.seagates3.parameter.validator.UnknownParameterValidator");
    when(resourceMap.getParamValidatorMethod())
        .thenReturn("isValidCreateParams");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertFalse(result);
  }

  @Test public void validateRequestTest_NoSuchMethodException()
      throws Exception {
    when(resourceMap.getParamValidatorClass()).thenReturn(
        "com.seagates3.parameter.validator.AccountParameterValidator");
    when(resourceMap.getParamValidatorMethod())
        .thenReturn("isValidUnknownParams");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertFalse(result);
  }

  @Test public void validateRequestTest_N() throws Exception {
    when(resourceMap.getParamValidatorClass()).thenReturn(
        "com.seagates3.parameter.validator.AccountParameterValidator");
    when(resourceMap.getParamValidatorMethod())
        .thenReturn("isValidCreateParams");

    Boolean result = WhiteboxImpl.invokeMethod(controller, "validateRequest",
                                               resourceMap, requestBody);

    assertFalse(result);
  }

  /*
   * Test for the scenario:
   *
   * When root user's access key and secret key are given
   * for creating access key for any non root user.
   */
  @Test public void serveTest_ValidUserRoot() throws Exception {
    requestBody.put("UserName", "usr1");

    IAMController controllerSpy = spy(controller);
    requestBody.put("Action", "CreateAccessKey");

    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    SignatureValidator signatureValidator = mock(SignatureValidator.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(RequestorService.getRequestor(clientRequestToken))
        .thenReturn(requestor);
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(clientRequestToken, requestor))
        .thenReturn(serverResponse);
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    when(IAMResourceMapper.getResourceMap("CreateAccessKey"))
        .thenReturn(resourceMap);
    doReturn(Boolean.TRUE)
        .when(controllerSpy, "validateRequest", resourceMap, requestBody);

    when(requestor.getName()).thenReturn("root");
    doReturn(serverResponse).when(controllerSpy, "performAction", resourceMap,
                                  requestBody, requestor);
    ServerResponse response = controllerSpy.serve(httpRequest, requestBody);

    assertEquals(serverResponse, response);
    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  /*
   * Test for the scenario:
   *
   * When non root user (i.e. usr1)'s access key and secret key are given
   * for creating access key for other non root user(i.e. usr2).
   */
  @Test public void serveTest_InvalidUserException() throws Exception {
    IAMController controllerSpy = spy(controller);
    requestBody.put("Action", "CreateAccessKey");
    requestBody.put("UserName", "usr2");

    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    SignatureValidator signatureValidator = mock(SignatureValidator.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(RequestorService.getRequestor(clientRequestToken))
        .thenReturn(requestor);
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(clientRequestToken, requestor))
        .thenReturn(serverResponse);
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    when(AuthServerConfig.getReqId()).thenReturn("0000");

    when(IAMResourceMapper.getResourceMap("CreateAccessKey"))
        .thenReturn(resourceMap);
    doReturn(Boolean.TRUE)
        .when(controllerSpy, "validateRequest", resourceMap, requestBody);

    when(requestor.getName()).thenReturn("usr1");
    doReturn(serverResponse).when(controllerSpy, "performAction", resourceMap,
                                  requestBody, requestor);
    ServerResponse response = controllerSpy.serve(httpRequest, requestBody);

    assertThat(
        response.getResponseBody(),
        containsString("User is not authorized to perform invoked action"));
    assertEquals(HttpResponseStatus.UNAUTHORIZED, response.getResponseStatus());
  }

  /*
   * Test for the scenario:
   *
   * When non root user (i.e. usr1)'s access key and secret key are given
   * for creating access key for self.
   */
  @Test public void serveTest_ValidUserSelf() throws Exception {
    IAMController controllerSpy = spy(controller);
    requestBody.put("Action", "CreateAccessKey");
    requestBody.put("UserName", "usr1");

    ClientRequestToken clientRequestToken = mock(ClientRequestToken.class);
    SignatureValidator signatureValidator = mock(SignatureValidator.class);
    when(ClientRequestParser.parse(httpRequest, requestBody))
        .thenReturn(clientRequestToken);
    when(RequestorService.getRequestor(clientRequestToken))
        .thenReturn(requestor);
    whenNew(SignatureValidator.class).withNoArguments().thenReturn(
        signatureValidator);
    when(signatureValidator.validate(clientRequestToken, requestor))
        .thenReturn(serverResponse);
    when(serverResponse.getResponseStatus()).thenReturn(HttpResponseStatus.OK);
    when(IAMResourceMapper.getResourceMap("CreateAccessKey"))
        .thenReturn(resourceMap);
    doReturn(Boolean.TRUE)
        .when(controllerSpy, "validateRequest", resourceMap, requestBody);

    when(requestor.getName()).thenReturn("usr1");
    doReturn(serverResponse).when(controllerSpy, "performAction", resourceMap,
                                  requestBody, requestor);
    ServerResponse response = controllerSpy.serve(httpRequest, requestBody);

    assertEquals(serverResponse, response);
    assertEquals(HttpResponseStatus.OK, response.getResponseStatus());
  }

  /**
   * Test  Scenario for :
   *
   * {@link IAMController#performAction(ResourceMap, Map, Requestor)}
   * When user performs ListAccounts action
   *
   * @throws Exception
   */
  @Test public void performActionTest() throws Exception {
    requestBody.put("Action", "ListAccounts");
    resourceMap = new ResourceMap("Account", "list");
    final String getResourceDAO = "getResourceDAO";
    AccountDAO accDao = mock(AccountImpl.class);
    AccessKeyDAO accKeyDao = new AccessKeyImpl();
    UserDAO userDao = new UserImpl();
    RoleDAO roleDao = new RoleImpl();
    GroupDAO gDao = new GroupImpl();
    PolicyDAO pDao = new PolicyImpl();
    Account[] accounts = new Account[0];

    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(accDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.ACCOUNT);
    PowerMockito.doReturn(accKeyDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.ACCESS_KEY);
    PowerMockito.doReturn(userDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.USER);
    PowerMockito.doReturn(roleDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.ROLE);
    PowerMockito.doReturn(gDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.GROUP);
    PowerMockito.doReturn(pDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.POLICY);

    when(accDao.findAll()).thenReturn(accounts);

    when(AuthServerConfig.getReqId()).thenReturn("0000");
    ServerResponse result = WhiteboxImpl.invokeMethod(
        controller, "performAction", resourceMap, requestBody, requestor);

    assertNotNull(result);
  }

  @Test public void performActionTest_ClassNotFoundException()
      throws Exception {
    when(resourceMap.getControllerClass())
        .thenReturn("com.seagates3.controller.UnknownController");

    ServerResponse result = WhiteboxImpl.invokeMethod(
        controller, "performAction", resourceMap, requestBody, requestor);

    assertNull(result);
  }

  /**
   * Test  Scenario for :
   *
   * {@link IAMController#performAction(ResourceMap, Map, Requestor)}
   * Handles NoSuchMethodException - When user performs an undefined action
   * on IAM controller
   *
   * @throws Exception
   */
  @Test public void performActionTest_NoSuchMethodException() throws Exception {
    requestBody.put("Action", "ListAccounts");
    resourceMap = new ResourceMap("Account", "noSuchAction");
    final String getResourceDAO = "getResourceDAO";
    AccountDAO accDao = new AccountImpl();
    AccessKeyDAO accKeyDao = new AccessKeyImpl();
    UserDAO userDao = new UserImpl();
    RoleDAO roleDao = new RoleImpl();
    GroupDAO gDao = new GroupImpl();
    PolicyDAO pDao = new PolicyImpl();

    PowerMockito.mockStatic(DAODispatcher.class);
    PowerMockito.doReturn(accDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.ACCOUNT);
    PowerMockito.doReturn(accKeyDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.ACCESS_KEY);
    PowerMockito.doReturn(userDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.USER);
    PowerMockito.doReturn(roleDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.ROLE);
    PowerMockito.doReturn(gDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.GROUP);
    PowerMockito.doReturn(pDao)
        .when(DAODispatcher.class, getResourceDAO, DAOResource.POLICY);

    ServerResponse result = WhiteboxImpl.invokeMethod(
        controller, "performAction", resourceMap, requestBody, requestor);

    assertNull(result);
  }

  /**
   * Test  Scenario for :
   *
   * {@link IAMController#performAction(ResourceMap, Map, Requestor)}
   * Handles InvocationTargetException - on Reflection APIs.
   *
   * @throws Exception
   */
  @Test public void performActionTest_InvocationTargetException()
      throws Exception {
    resourceMap = new ResourceMap("Account", "list");
    ServerResponse result = WhiteboxImpl.invokeMethod(
        controller, "performAction", resourceMap, requestBody, requestor);

    assertNull(result);
  }
}

