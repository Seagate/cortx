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
* Original creation date: 17-Sep-2015
*/
package com.seagates3.controller;

import com.seagates3.authentication.ClientRequestParser;
import com.seagates3.authentication.ClientRequestToken;
import com.seagates3.authentication.SignatureValidator;
import com.seagates3.authorization.Authorizer;
import com.seagates3.authorization.IAMApiAuthorizer;
import com.seagates3.authorization.RootPermissionAuthorizer;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.authserver.IAMResourceMapper;
import com.seagates3.authserver.ResourceMap;
import com.seagates3.exception.AuthResourceNotFoundException;
import com.seagates3.exception.InternalServerException;
import com.seagates3.exception.InvalidAccessKeyException;
import com.seagates3.exception.InvalidArgumentException;

import com.seagates3.exception.InvalidRequestorException;
import com.seagates3.exception.InvalidUserException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.perf.S3Perf;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AuthenticationResponseGenerator;
import com.seagates3.service.RequestorService;
import com.seagates3.util.IEMUtil;
import io.netty.handler.codec.http.FullHttpRequest;
import io.netty.handler.codec.http.HttpResponseStatus;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public
class IAMController {

 private
  final Logger LOGGER = LoggerFactory.getLogger(IAMController.class.getName());
 private
  final S3Perf perf;
  AuthenticationResponseGenerator responseGenerator;

 public
  IAMController() {
    responseGenerator = new AuthenticationResponseGenerator();
    perf = new S3Perf();
  }

  /**
   * Authenticate the requestor first. If the requestor is authenticated, then
   * perform the requested action.
   *
   * @param httpRequest
   * @param requestBody
   * @return
   */
 public
  ServerResponse serve(FullHttpRequest httpRequest,
                       Map<String, String> requestBody) {
    String requestAction = requestBody.get("Action");
    LOGGER.info("Requested action is  - " + requestAction);
    ClientRequestToken clientRequestToken = null;
    Requestor requestor = null;
    ServerResponse serverResponse;
    // CreateAccount needs to be authenticated with Ldap credentials
    if (requestAction.equals("CreateAccount") ||
        requestAction.equals("ListAccounts") ||
        requestAction.equals("ResetAccountAccessKey")) {
      LOGGER.debug("Parsing Client Request");
      try {
        clientRequestToken =
            ClientRequestParser.parse(httpRequest, requestBody);
      }
      catch (InvalidAccessKeyException ex) {
        LOGGER.debug(ex.getServerResponse().getResponseBody());
        return ex.getServerResponse();
      }
      catch (InvalidArgumentException ex) {
        LOGGER.debug(ex.getServerResponse().getResponseBody());
        return ex.getServerResponse();
      }

      if (clientRequestToken == null) {
        return responseGenerator.AccessDenied();
      }
      AccessKey akey = new AccessKey();
      String ldapUser = AuthServerConfig.getLdapLoginCN();
      if (!ldapUser.equals(clientRequestToken.getAccessKeyId())) {
        LOGGER.error("Invalid ldap user, authentication failed");
        AuthenticationResponseGenerator responseGenerator =
            new AuthenticationResponseGenerator();
        serverResponse = responseGenerator.invalidLdapUserId();
        return serverResponse;
      }
      akey.setId(ldapUser);
      akey.setSecretKey(AuthServerConfig.getLdapLoginPassword());
      requestor = new Requestor();
      requestor.setAccessKey(akey);
      LOGGER.debug("Calling signature validator.");

      perf.startClock();
      serverResponse =
          new SignatureValidator().validate(clientRequestToken, requestor);
      perf.endClock();
      perf.printTime("Request validation");

      if (!serverResponse.getResponseStatus().equals(HttpResponseStatus.OK)) {
        LOGGER.error("Incorrect signature. Request not authenticated");
        return serverResponse;
      }
    } else if ((requestBody.get("Authorization") == null) &&
               requestAction.equals("AuthorizeUser")) {

      serverResponse = new Authorizer().authorize(null, requestBody);
      return serverResponse;

    } else if (!requestAction.equals("AssumeRoleWithSAML") &&
               !requestAction.equals("GetTempAuthCredentials")) {
      LOGGER.debug("Parsing Client Request");
      try {
        if (!"AuthorizeUser".equals(requestAction)) {
          clientRequestToken =
              ClientRequestParser.parse(httpRequest, requestBody);
          /*
           * Client Request Token will be null if the request is incorrect.
         */
          if (clientRequestToken == null) {
            return responseGenerator.AccessDenied();
          }
        }
      }
      catch (InvalidAccessKeyException ex) {
        LOGGER.debug(ex.getServerResponse().getResponseBody());
        return ex.getServerResponse();
      }
      catch (InvalidArgumentException ex) {
        LOGGER.debug(ex.getServerResponse().getResponseBody());
        return ex.getServerResponse();
      }

      try {
        if (!requestAction.equals("ValidateACL") &&
            !requestAction.equals("ValidatePolicy") &&
            !"AuthorizeUser".equals(requestAction)) {
          requestor = RequestorService.getRequestor(clientRequestToken);
      }
      }
      catch (InvalidAccessKeyException ex) {
        LOGGER.debug(ex.getServerResponse().getResponseBody());
        return ex.getServerResponse();
      }
      catch (InternalServerException ex) {
        LOGGER.debug(ex.getServerResponse().getResponseBody());
        return ex.getServerResponse();
      }
      catch (InvalidRequestorException ex) {
        LOGGER.debug(ex.getServerResponse().getResponseBody());
        return ex.getServerResponse();
      }

      LOGGER.debug("Requestor is valid." + requestor);
      if (requestAction.equals("AuthorizeUser")) {

        Account account = new Account();
        account.setId(requestBody.get("RequestorAccountId"));
        account.setName(requestBody.get("RequestorAccountName"));
        account.setCanonicalId(requestBody.get("RequestorCanonicalId"));
        account.setEmail(requestBody.get("RequestorEmail"));
        requestor = new Requestor();
        requestor.setId(requestBody.get("RequestorUserId"));
        requestor.setName(requestBody.get("RequestorUserName"));
        requestor.setAccount(account);

        LOGGER.info("Authorizing user: " + requestor.getName() + " account: " +
                    requestor.getAccount().getName());
        serverResponse = new Authorizer().authorize(requestor, requestBody);
        return serverResponse;
      } else if (requestAction.equals("ValidateACL")) {
        LOGGER.debug("Validating Acl:");
        serverResponse = new Authorizer().validateACL(requestBody);
        return serverResponse;
      } else if (requestAction.equals("ValidatePolicy")) {
        LOGGER.debug("Validating Policy:");
        serverResponse = new Authorizer().validatePolicy(requestBody);
        return serverResponse;
      }

      LOGGER.debug("Calling signature validator.");

      perf.startClock();
      serverResponse =
          new SignatureValidator().validate(clientRequestToken, requestor);
      perf.endClock();
      perf.printTime("Request validation");

      if (!serverResponse.getResponseStatus().equals(HttpResponseStatus.OK)) {
        LOGGER.error("Incorrect signature. Request not authenticated");
        return serverResponse;
      }

      if (requestAction.equals("AuthenticateUser")) {
        serverResponse = responseGenerator.generateAuthenticatedResponse(
            requestor, clientRequestToken);
        LOGGER.info("Request is authenticated for user: " +
                    requestor.getName() + " account: " +
                    requestor.getAccount().getName());

        return serverResponse;
      }
    } else {
      requestor = new Requestor();
    }

    ResourceMap resourceMap;
    try {
      resourceMap = IAMResourceMapper.getResourceMap(requestAction);
    }
    catch (AuthResourceNotFoundException ex) {
      return responseGenerator.operationNotSupported();
    }

    LOGGER.debug("Controller class -" + resourceMap.getControllerClass());
    LOGGER.debug("Action name - " + resourceMap.getControllerAction());

    if (!validateRequest(resourceMap, requestBody)) {
      LOGGER.debug("Input parameters are not valid.");
      return responseGenerator.invalidParametervalue();
    }

    /*
     * Check if User is authorized to perform invoked action.
     * Only ldap credentials are allowed for createaccount and listaccounts.
     * Hence, createaccount and listaccounts don't require this check.
     * Skipping GetTempAuthCredentials here because Authorization is performed-
     * with username and password entered by user
     */
    if (!(requestAction.equals("CreateAccount") ||
          requestAction.equals("ListAccounts") ||
          requestAction.equals("ResetAccountAccessKey") ||
          requestAction.equals("ChangePassword") ||
          requestAction.equals("GetTempAuthCredentials"))) {
      try {
        if (RootPermissionAuthorizer.getInstance().containsAction(
                requestAction)) {
          new IAMApiAuthorizer().authorizeRootUser(requestor, requestBody);
        } else {
          new IAMApiAuthorizer().authorize(requestor, requestBody);
        }
        LOGGER.info("Request is authorized for user: " + requestor.getName() +
                    " account: " + requestor.getAccount());
      }
      catch (InvalidUserException e) {
        LOGGER.error(e.getServerResponse().getResponseBody());
        return e.getServerResponse();
      }
    }

    return performAction(resourceMap, requestBody, requestor);
  }

  /**
   * Validate the request parameters.
   *
   * @param resourceMap
   * @param requestBody
   * @return
   */
 private
  Boolean validateRequest(ResourceMap resourceMap,
                          Map<String, String> requestBody) {
    Boolean isValidrequest = false;
    Class < ? > validator;
    Method method;
    Object obj;
    String validatorClass = resourceMap.getParamValidatorClass();

    try {
      LOGGER.debug("Calling " + resourceMap.getControllerAction() +
                   " validator.");
      validator = Class.forName(validatorClass);
      obj = validator.newInstance();
      method =
          validator.getMethod(resourceMap.getParamValidatorMethod(), Map.class);
      isValidrequest = (Boolean)method.invoke(obj, requestBody);
    }
    catch (ClassNotFoundException ex) {
      IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.CLASS_NOT_FOUND_EX,
                  "Failed to get required class",
                  String.format("\"cause\": \"%s\"", ex.getCause()));
    }
    catch (NoSuchMethodException ex) {
      IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.NO_SUCH_METHOD_EX,
                  "Failed to invoke method",
                  String.format("\"cause\": \"%s\"", ex.getCause()));
    }
    catch (SecurityException | IllegalAccessException |
           IllegalArgumentException | InvocationTargetException |
           InstantiationException ex) {
      LOGGER.error("Exception: ", ex);
    }

    return isValidrequest;
  }

  /**
   * Perform action requested by user.
   */
 private
  ServerResponse performAction(ResourceMap resourceMap,
                               Map<String, String> requestBody,
                               Requestor requestor) {

    String controllerClassName = resourceMap.getControllerClass();
    Class < ? > controller;
    Constructor < ? > controllerConstructor;
    Method method;
    Object obj;

    try {
      LOGGER.debug("Calling " + resourceMap.getControllerAction() +
                   " controller.");
      controller = Class.forName(controllerClassName);
      controllerConstructor =
          controller.getConstructor(Requestor.class, Map.class);
      obj = controllerConstructor.newInstance(requestor, requestBody);

      method = controller.getMethod(resourceMap.getControllerAction());
      return (ServerResponse)method.invoke(obj);
    }
    catch (ClassNotFoundException ex) {
      IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.CLASS_NOT_FOUND_EX,
                  "Failed to get required class",
                  String.format("\"cause\": \"%s\"", ex.getCause()));
    }
    catch (NoSuchMethodException ex) {
      IEMUtil.log(IEMUtil.Level.ERROR, IEMUtil.NO_SUCH_METHOD_EX,
                  "Failed to invoke method",
                  String.format("\"cause\": \"%s\"", ex.getCause()));
    }
    catch (SecurityException | IllegalAccessException |
           IllegalArgumentException | InvocationTargetException |
           InstantiationException ex) {
      LOGGER.error("Exception: ", ex);
    }

    return null;
  }
}


