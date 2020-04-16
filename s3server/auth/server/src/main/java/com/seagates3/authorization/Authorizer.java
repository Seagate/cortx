/*
* COPYRIGHT 2016 SEAGATE LLC
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
* Original creation date: 27-May-2016
*/
package com.seagates3.authorization;

import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.xml.sax.SAXException;

import com.seagates3.acl.ACLAuthorizer;
import com.seagates3.acl.ACLCreator;
import com.seagates3.acl.ACLRequestValidator;
import com.seagates3.acl.ACLValidation;
import com.seagates3.acl.AccessControlList;
import com.seagates3.acl.AccessControlPolicy;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.BadRequestException;
import com.seagates3.exception.DataAccessException;
import com.seagates3.exception.GrantListFullException;
import com.seagates3.exception.InternalServerException;
import com.seagates3.model.Account;
import com.seagates3.model.Group;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.policy.BucketPolicyAuthorizer;
import com.seagates3.policy.BucketPolicyValidator;
import com.seagates3.policy.PolicyUtil;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AuthorizationResponseGenerator;
import com.seagates3.util.BinaryUtil;

import io.netty.handler.codec.http.HttpResponseStatus;

public
class Authorizer {

 private
  final Logger LOGGER = LoggerFactory.getLogger(Authorizer.class.getName());

 public
  Authorizer() {}

  /**
   * Request authorization
   *
   * @param requestor
   * @param requestBody
   * @return
   */
 public
  ServerResponse authorize(Requestor requestor,
                           Map<String, String> requestBody) {

    ServerResponse serverResponse = null;
    AuthorizationResponseGenerator responseGenerator =
        new AuthorizationResponseGenerator();
    try {
      String existingPolicy = requestBody.get("Policy");
      // Below will check put/get/delete policy for first time
      if (existingPolicy == null || existingPolicy.isEmpty()) {
        String clientQueryParams = requestBody.get("ClientQueryParams");
        if ("policy".equals(clientQueryParams)) {
          String bucketOwner = new AccessControlList().getOwner(requestBody);
          String requestorAccountCanonicalId = null;
          if (requestor != null) {
            requestorAccountCanonicalId =
                requestor.getAccount().getCanonicalId();
          }
          String requestorOwner = requestorAccountCanonicalId;
          if (bucketOwner.equals(requestorOwner)) {
            serverResponse = responseGenerator.ok();
          } else {
            serverResponse = responseGenerator.AccessDenied();
          }
        }
      } else {
        // serverResponse = either Authorized or UnAuthorized/AccessDenied or
        // NULL
        serverResponse = new BucketPolicyAuthorizer().authorizePolicy(
            requestor, requestBody);
      }
      if (serverResponse == null) {
        /// check for ACL Authorization if either policy is null or policy
        /// authorization
        /// is None
        LOGGER.debug("inside acl authorization....");
        serverResponse = checkAclAuthorization(requestor, requestBody, true);
      } else if (serverResponse.getResponseStatus() == HttpResponseStatus.OK) {
        LOGGER.debug("inside acl validation w.o. authorization....");
        serverResponse = checkAclAuthorization(requestor, requestBody, false);
      }
    }
    catch (Exception e) {
      LOGGER.error("exception in authorizaation.. ", e);
    }
    if (serverResponse != null) {
      LOGGER.debug("Authorization response is - " +
                   serverResponse.getResponseStatus());
    }
    return serverResponse;
  }

 private
  ServerResponse checkAclAuthorization(Requestor requestor,
                                       Map<String, String> requestBody,
                                       boolean isAclAuthorizationRequired) {
    AuthorizationResponseGenerator responseGenerator =
        new AuthorizationResponseGenerator();
    Map<String, List<Account>> accountPermissionMap = new HashMap<>();
    Map<String, List<Group>> groupPermissionMap = new HashMap<>();
    LOGGER.debug("request body : " + requestBody.toString());
    // Here we are assuming that - Authentication is successful hence
    // proceeding with authorization
    ServerResponse serverResponse = null;

    if (("PUT".equals(requestBody.get("Method")))) {
      serverResponse = new ACLRequestValidator().validateAclRequest(
          requestBody, accountPermissionMap, groupPermissionMap);
    }
    if (serverResponse != null && serverResponse.getResponseStatus() != null &&
        !serverResponse.getResponseStatus().equals(HttpResponseStatus.OK)) {
      LOGGER.error("ACL authorization request validation failed");
      return serverResponse;
    }
    LOGGER.info("ACL authorization request is validated successfully");

    if (isAclAuthorizationRequired) {
      serverResponse = isAclAuthorized(requestor, requestBody);
      if (serverResponse != null) {
        return serverResponse;
      }
    }

    // After ALLUser Requested Permission is Authorized.
    if (requestor == null && !("true".equals(requestBody.get("Request-ACL")))) {
      return responseGenerator.ok();
    }

    // Initialize a default AccessControlPolicy object and generate
    // authorization response if request header contains param value true
    // for- Request-ACL
    if ("true".equals(requestBody.get("Request-ACL"))) {
      try {
        String acl = null;
        ACLCreator aclCreator = new ACLCreator();

        if (!accountPermissionMap.isEmpty() || !groupPermissionMap.isEmpty()) {
          acl = aclCreator.createAclFromPermissionHeaders(
              requestor, accountPermissionMap, groupPermissionMap, requestBody);

        } else if (requestBody.get("x-amz-acl") != null) {
          acl = aclCreator.createACLFromCannedInput(requestor, requestBody);

        } else {  // neither permission headers nor canned acl requested so
                  // create default acl
          acl = aclCreator.createDefaultAcl(requestor);
        }
        LOGGER.info("Updated xml is - " + acl);
        return responseGenerator.generateAuthorizationResponse(requestor, acl);
      }
      catch (ParserConfigurationException | SAXException | IOException e) {
        LOGGER.error("Error while initializing default ACL");
        return responseGenerator.invalidACL();
      }
      catch (GrantListFullException e) {
        LOGGER.error("Error while initializing default ACL." +
                     " Grants more than " + AuthServerConfig.MAX_GRANT_SIZE +
                     " are not allowed.");
        return responseGenerator.grantListSizeViolation();
      }
      catch (TransformerException e) {
        LOGGER.error("Error while generating the Authorization Response");
        return responseGenerator.internalServerError();
      }
      catch (InternalServerException e) {
        LOGGER.error(e.getMessage());
        return e.getServerResponse();
      }
    }
    return responseGenerator.generateAuthorizationResponse(requestor, null);
  }

  /**
   * Below method will handle acl authorization
   */
 private
  ServerResponse isAclAuthorized(Requestor requestor,
                                 Map<String, String> requestBody) {
    AuthorizationResponseGenerator responseGenerator =
        new AuthorizationResponseGenerator();
    try {
      if (requestBody.get("Auth-ACL") == null) {
        LOGGER.debug("ACL not present. Skipping ACL authorization.");
      } else if (!new ACLAuthorizer().isAuthorized(requestor, requestBody)) {
        LOGGER.info("Resource ACL denies access to the requestor for this " +
                    "operation.");
        return responseGenerator.AccessDenied();
      }
    }
    catch (ParserConfigurationException | SAXException | IOException |
           GrantListFullException e1) {
      LOGGER.error("Error while initializing authorization ACL");
      return responseGenerator.invalidACL();
    }
    catch (BadRequestException e2) {
      LOGGER.error("Request does not contain the mandatory - " +
                   "ACL, Method or ClientAbsoluteUri");
      return responseGenerator.badRequest();
    }
    catch (DataAccessException e3) {
      LOGGER.error("Exception while authorizing ", e3);
      responseGenerator.internalServerError();
    }
    return null;
  }

  /**
   * Below method will check if user is root user or no
   *
   * @param user
   * @return
   */

  /**
   * Check if the specified {@link User} is a Root user.
   *
   * @param user
   * @return
   */
 public
  static boolean isRootUser(User user) {
    boolean isRoot = false;
    if ("root".equals(user.getName())) {
      isRoot = true;
    }
    return isRoot;
  }

  /**
   * Validate the ACL if requested
   *
   * @param requestor
   * @param requestBody
   * @return true if ACL is valid
   */
 public
  ServerResponse validateACL(Map<String, String> requestBody) {

    Map<String, List<Account>> accountPermissionMap = new HashMap<>();
    Map<String, List<Group>> groupPermissionMap = new HashMap<>();
    LOGGER.debug("request body : " + requestBody.toString());
    ServerResponse serverResponse = null;

    serverResponse = new ACLRequestValidator().validateAclRequest(
        requestBody, accountPermissionMap, groupPermissionMap);

    if (serverResponse != null && serverResponse.getResponseStatus() != null &&
        !serverResponse.getResponseStatus().equals(HttpResponseStatus.OK)) {
      LOGGER.error("Invalid ACL Request");
      return serverResponse;
    }

    AuthorizationResponseGenerator responseGenerator =
        new AuthorizationResponseGenerator();
    ACLValidation aclValidation = null;
    try {

      aclValidation = new ACLValidation(requestBody.get("ACL"));
      }
      catch (ParserConfigurationException | SAXException | IOException e) {
        LOGGER.debug("Error while Parsing ACL XML");
        return responseGenerator.invalidACL();
      }
      catch (GrantListFullException e) {
        LOGGER.debug("Error while Parsing ACL XML. Number of grants exceeds " +
                     AuthServerConfig.MAX_GRANT_SIZE);
        return responseGenerator.invalidACL();
      }
      AccessControlPolicy existingAcp = null;
      try {
        if (requestBody.get("Auth-ACL") != null) {
          existingAcp = new AccessControlPolicy(
              BinaryUtil.base64DecodeString(requestBody.get("Auth-ACL")));
          LOGGER.debug(
              "Sending Auth-ACL for validating new owner against existing " +
              "owner");
        }
      }
      catch (ParserConfigurationException | SAXException | IOException |
             GrantListFullException e1) {
        LOGGER.error("Error while validating authorization ACL");
        return responseGenerator.invalidACL();
      }
      return aclValidation.validate(existingAcp);
    }

    /**
     * Validate the policy
     *
     * @param requestBody
     * @return
     */
   public
    ServerResponse validatePolicy(Map<String, String> requestBody) {
      LOGGER.debug("request body : " + requestBody.toString());
      ServerResponse serverResponse = null;
      serverResponse = new BucketPolicyValidator().validatePolicy(
          PolicyUtil.getBucketFromUri(requestBody.get("ClientAbsoluteUri")),
          requestBody.get("Policy"));
      if (serverResponse != null &&
          serverResponse.getResponseStatus() != null &&
          !serverResponse.getResponseStatus().equals(HttpResponseStatus.OK)) {
        LOGGER.error("Invalid Bucket Policy");
        return serverResponse;
      }
      return new AuthorizationResponseGenerator().ok();
    }
}


