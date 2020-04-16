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
 * Original creation date: 13-December-2019
 */

package com.seagates3.policy;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import com.amazonaws.auth.policy.Action;
import com.amazonaws.auth.policy.Condition;
import com.amazonaws.auth.policy.Policy;
import com.amazonaws.auth.policy.Principal;
import com.amazonaws.auth.policy.Statement;
import com.amazonaws.auth.policy.Statement.Effect;
import com.amazonaws.util.json.JSONException;
import com.amazonaws.util.json.JSONObject;
import com.seagates3.acl.AccessControlList;
import com.seagates3.authorization.Authorizer;
import com.seagates3.dao.ldap.AccountImpl;
import com.seagates3.dao.ldap.UserImpl;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AuthorizationResponseGenerator;

import io.netty.handler.codec.http.HttpResponseStatus;

public
class BucketPolicyAuthorizer extends PolicyAuthorizer {

  @Override public ServerResponse authorizePolicy(
      Requestor requestor, Map<String, String> requestBody) {
    ServerResponse serverResponse = null;
    // authorizePolicy will return NULL if no relevant entry found in policy
    // authorized if match is found
    // AccessDenied if Deny found
    try {
      String requestedOperation =
          identifyOperationToAuthorize(requestBody).toLowerCase();
      LOGGER.debug("operation to authorize - " + requestedOperation);
      if (requestedOperation != null) {
        serverResponse =
            authorizeOperation(requestBody, requestedOperation, requestor);
      }
    }
    catch (Exception e) {
      LOGGER.error("Exception while authorizing", e);
    }
    LOGGER.debug("authorizePolicy response - " + serverResponse);
    return serverResponse;
  }

  /**
   * Below method will identify requested operation by user
   *
   * @param requestBody
   * @return
   */
 private
  String identifyOperationToAuthorize(Map<String, String> requestBody) {
    String s3Action = requestBody.get("S3Action");
    switch (s3Action) {
      case "HeadBucket":
        s3Action = "ListBucket";
        break;
      case "HeadObject":
        s3Action = "GetObject";
        break;
      case "DeleteBucketTagging":
        s3Action = "PutBucketTagging";
        break;
    }
    s3Action = "s3:" + s3Action;
    LOGGER.debug("identifyOperationToAuthorize has returned action as - " +
                 s3Action);
    return s3Action;
  }

  /**
   * Below will authorize requested operation based on Resource,Principal and
   * Action present inside existing policy
   *
   * @param existingPolicy
   * @param requestedOperation
   * @param requestor
   * @param requestedResource
   * @param resourceOwner
   * @return
   * @throws DataAccessException
   * @throws JSONException
   */
 private
  ServerResponse authorizeOperation(
      Map<String, String> requestBody, String requestedOperation,
      Requestor requestor) throws DataAccessException,
      JSONException {

    ServerResponse response = null;
    AuthorizationResponseGenerator responseGenerator =
        new AuthorizationResponseGenerator();

    JSONObject obj = new JSONObject(requestBody.get("Policy"));
    String policyString = obj.toString();
    policyString = policyString.replace(
        "CanonicalUser",
        "Service");  // TODO:temporary solution till we implement parser
    Policy existingPolicy = Policy.fromJson(policyString);
    String requestedResource =
        PolicyUtil.getResourceFromUri(requestBody.get("ClientAbsoluteUri"));
    String resourceOwner = new AccessControlList().getOwner(requestBody);

    List<Statement> statementList =
        new ArrayList<Statement>(existingPolicy.getStatements());
    if (requestor != null) {
    for (Statement stmt : statementList) {
      List<Principal> principalList = stmt.getPrincipals();
      List<Condition> conditions = stmt.getConditions();
      if (isPrincipalMatching(principalList, requestor)) {
        List<String> resourceList =
            PolicyUtil.convertCommaSeparatedStringToList(
                stmt.getResources().get(0).getId());
        if (isResourceMatching(resourceList, requestedResource)) {
          List<Action> actionsList = stmt.getActions();
          if (isActionMatching(actionsList, requestedOperation)) {
            if (isConditionMatching(conditions, requestBody)) {
              if (stmt.getEffect().equals(Effect.Allow)) {
                response = responseGenerator.ok();
              } else {
                response = responseGenerator.AccessDenied();
              }
            }
          }
        }
      }
    }
    }
    if (requestor == null) {
        return responseGenerator.AccessDenied();
      }

      // Below will handle Get/Put/Delete Bucket Policy
      if (PolicyUtil.isPolicyOperation(requestedOperation)) {
      if (response != null &&
          response.getResponseStatus() == HttpResponseStatus.OK) {
        Account ownerAccount =
            new AccountImpl().findByCanonicalID(resourceOwner);
        if (!ownerAccount.getName().equals(requestor.getAccount().getName())) {
          response = responseGenerator.methodNotAllowed(
              "The specified method is not allowed against this resource.");
        }
      } else {
        boolean isRootUser = Authorizer.isRootUser(
            new UserImpl().findByUserId(requestor.getId()));
        if (isRootUser &&
            requestor.getAccount().getCanonicalId().equals(resourceOwner)) {
          response = responseGenerator.ok();
        } else {
          response = responseGenerator.AccessDenied();
        }
      }
      } else {
        if (response != null &&
            response.getResponseStatus() == HttpResponseStatus.OK) {
          boolean isRootUser = Authorizer.isRootUser(
              new UserImpl().findByUserId(requestor.getId()));
          if (isRootUser ||
              requestor.getAccount().getCanonicalId().equals(resourceOwner)) {
            response = responseGenerator.ok();
          } else {
            response = responseGenerator.AccessDenied();
          }
        }
      }
    return response;
  }

  /**
   * Below method will validate requested account against principal inside
   *policy
   *
   * @param principalList
   * @param requestor
   * @return
   * @throws DataAccessException
   */
 private
  boolean isPrincipalMatching(List<Principal> principalList,
                              Requestor requestor) throws DataAccessException {
    boolean isMatching = false;
    for (Principal principal : principalList) {
      String provider = principal.getProvider();
      String principalId = principal.getId();
      if ("*".equals(principalId)) {
        return true;
      }
      switch (provider) {
        case "AWS":
          if (new PrincipalArnParser().isArnFormatValid(principalId)) {
            User user = new UserImpl().findByArn(principalId);
            if (user != null && user.exists() &&
                user.getId().equals(requestor.getId())) {
              isMatching = true;
            }
          } else {
            if (principalId.equals(requestor.getAccount().getId())) {
              isMatching = true;
            } else if (principalId.equals(requestor.getId())) {
              isMatching = true;
            }
          }
          break;
        case "Service":
          if (principalId.equals(requestor.getAccount().getCanonicalId())) {
            isMatching = true;
          }
          break;
      }
      if (isMatching) {
        break;
      }
    }
    LOGGER.debug("isPrincipalMatching:: result - " +
                 String.valueOf(isMatching));
    return isMatching;
  }

  /**
   * Below will validate requested resource against resource inside policy
   *
   * @param resourceList
   * @param requestedResource
   * @return
   */
 private
  boolean isResourceMatching(List<String> resourceList,
                             String requestedResource) {
    boolean isMatching = false;
    for (String resourceArn : resourceList) {
      String resource = PolicyUtil.getResourceFromResourceArn(resourceArn);
      if (PolicyUtil.isPatternMatching(requestedResource, resource)) {
        isMatching = true;
        break;
      }
    }
    LOGGER.debug("isResourceMatching:: result - " + String.valueOf(isMatching));
    return isMatching;
  }

  /**
   * Below will validate requested operation against the actions inside policy
   * including wildcard-chars
   *
   * @param actionsList
   * @param requestedOperation
   * @return
   */
 private
  boolean isActionMatching(List<Action> actionsList,
                           String requestedOperation) {
    boolean isMatching = false;
    for (Action action : actionsList) {
      List<String> matchingActionsList =
          PolicyUtil.getAllMatchingActions(action.getActionName());
      for (String matchingAction : matchingActionsList) {
        if (matchingAction.equals(requestedOperation)) {
          isMatching = true;
          break;
        }
      }
      if (isMatching) {
        break;
      }
    }
    LOGGER.debug("isActionMatching:: result - " + String.valueOf(isMatching));
    return isMatching;
  }

  /**
   * Checks if the Conditions from policy are satisfied in the request.
   * Returns true if there are no conditions in policy.
   * @param conditions
   * @param requestBody
   * @return - true if policy conditions are satisfied
   */
 private
  boolean isConditionMatching(List<Condition> conditions,
                              Map<String, String> requestBody) {
    if (conditions.isEmpty()) return true;

    /**
     * Check if the headers from requestBody satisfy the policy conditions
     * Sample Condition -
     * "Condition": {
     *   "StringEquals": {
     *     "s3:x-amz-acl": ["bucket-owner-read", "bucket-owner-full-control"]
     *   }
     * }
     */
    boolean result = false;
    for (Condition condition : conditions) {
      PolicyCondition pc = ConditionFactory.getCondition(
          condition.getType(),
          ConditionUtil.removeKeyPrefix(condition.getConditionKey()),
          condition.getValues());
      if (pc != null)
        result = pc.isSatisfied(requestBody);
      else
        result = false;

      if (!result) break;
    }
    LOGGER.debug("isConditionMatching:: result - " + String.valueOf(result));
    return result;
  }
}


