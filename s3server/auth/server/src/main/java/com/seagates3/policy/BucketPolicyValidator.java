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
 * Original creation date: 23-October-2019
 */

package com.seagates3.policy;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.amazonaws.auth.policy.Action;
import com.amazonaws.auth.policy.Policy;
import com.amazonaws.auth.policy.Resource;
import com.amazonaws.auth.policy.Statement;
import com.amazonaws.auth.policy.internal.JsonDocumentFields;
import com.amazonaws.util.json.JSONArray;
import com.amazonaws.util.json.JSONException;
import com.amazonaws.util.json.JSONObject;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.BucketPolicyResponseGenerator;
import com.seagates3.util.BinaryUtil;

public
class BucketPolicyValidator extends PolicyValidator {

 private
  final Logger LOGGER =
      LoggerFactory.getLogger(BucketPolicyValidator.class.getName());
  static Set<String> policyElements = new HashSet<>();
  static Set<String> nestedPolicyElements = new HashSet<>();

 public
  BucketPolicyValidator() {
    responseGenerator = new BucketPolicyResponseGenerator();
    initializePolicyElements();
    initializeNestedPolicyElements();
  }

 private
  void initializePolicyElements() {
    policyElements.add("Version");
    policyElements.add("Id");
    policyElements.add("Statement");
    policyElements.add("Effect");
    policyElements.add("Sid");
    policyElements.add("Principal");
    policyElements.add("Action");
    policyElements.add("Resource");
    policyElements.add("Condition");
  }

 private
  static void initializeNestedPolicyElements() {

    nestedPolicyElements.add("Effect");
    nestedPolicyElements.add("Sid");
    nestedPolicyElements.add("Principal");
    nestedPolicyElements.add("Action");
    nestedPolicyElements.add("Resource");
    nestedPolicyElements.add("Condition");
  }

  /**
   * Below method will validate bucket policy json passed in
   *
   * @param jsonPolicy
   * @return null if policy validated successfully. not null if policy
   *         invalidated.
   */
 public
  ServerResponse validatePolicy(String inputBucket, String jsonPolicy) {
    ServerResponse response = null;
    Policy policy = null;
    if (jsonPolicy.length() > 20480) {
      LOGGER.error(
          "policy exceeding max allowed size 20480 bytes.Current policy size " +
          "- " + jsonPolicy.length());
      return responseGenerator.malformedPolicy(
          "Normalized policy document exceeds the maximum allowed size of " +
          "20480 bytes");
    }
    try {
      JSONObject obj =
          new JSONObject(BinaryUtil.base64DecodeString(jsonPolicy));
      response = validatePolicyElements(obj);
      if (response != null) {
        return response;
      }
      String policyString = obj.toString();
      policyString = policyString.replace(
          "CanonicalUser",
          "Service");  // TODO:temporary solution till we implement parser
      policy = Policy.fromJson(policyString);
    }
    catch (JSONException e) {
      response = responseGenerator.malformedPolicy(
          "This policy contains invalid Json");
      LOGGER.error("This policy contains invalid Json");
      return response;
    }
    catch (Exception e) {
      response = responseGenerator.malformedPolicy(e.getMessage());
      LOGGER.error("Exception in ValidatePolicy -  ", e);
      return response;
    }
    // Statement validation
    if (policy.getStatements() != null) {
      List<Statement> statementList =
          new ArrayList<Statement>(policy.getStatements());
      for (Statement stmt : statementList) {
        // Action validation
        response = validateActionAndResource(stmt.getActions(),
                                             stmt.getResources(), inputBucket);
        if (response != null) break;

        // Effect validation
        response = validateEffect(stmt.getEffect());
        if (response != null) break;

        // condition validation
        response = validateCondition(stmt.getConditions(), stmt.getActions());
        if (response != null) break;

        // Principal Validation
        response = validatePrincipal(stmt.getPrincipals());
        if (response != null) break;
      }
    } else {  // statement null
      response =
          responseGenerator.malformedPolicy("Missing required field Statement");
      LOGGER.error("Missing required field Statement");
    }
    return response;
  }

 private
  ServerResponse validatePolicyElements(JSONObject jsonObject)
      throws JSONException {
    ServerResponse response = null;
    boolean isEffectFieldPresent = false;
    Iterator<String> keys = jsonObject.keys();
    while (keys.hasNext()) {
      String key = keys.next();
      if (!policyElements.contains(key)) {  // some unknown field found
        response = responseGenerator.malformedPolicy("Unknown field " + key);
        LOGGER.error("Unknown field - " + key);
        break;
      }
      if (jsonObject.get(key) instanceof JSONArray) {
        if (!key.equals(JsonDocumentFields.STATEMENT)) {
          response = responseGenerator.malformedPolicy(
              "Invalid bucket policy syntax.");
          LOGGER.error(
              "Invalid bucket policy syntax. Misplaced policy elements");
          break;
        }
        JSONArray arr = (JSONArray)jsonObject.get(key);
        for (int count = 0; count < arr.length(); count++) {
          isEffectFieldPresent = false;
          JSONObject obj = (JSONObject)arr.get(count);
          Iterator<String> objKeys = obj.keys();
          while (objKeys.hasNext()) {
            String objKey = objKeys.next();
            if (!policyElements.contains(objKey)) {
              response =
                  responseGenerator.malformedPolicy("Unknown field " + objKey);
              LOGGER.error("Unknown field - " + objKey);
              return response;
            }
            if (!nestedPolicyElements.contains(objKey)) {
              response = responseGenerator.malformedPolicy(
                  "Invalid bucket policy syntax.");
              LOGGER.error(
                  "Invalid bucket policy syntax. Misplaced policy elements");
              return response;
            } else if ("Effect".equals(objKey)) {  // Adding effect check here
                                                   // as json parser setting the
              // default value when not present
              String effectValue = obj.get(objKey).toString();
              if (effectValue != null) {
                if (effectValue.isEmpty()) {

                  response = responseGenerator.malformedPolicy(
                      "Missing required field Effect cannot be empty!");
                  LOGGER.error("Required field Effect is empty");

                } else if (!effectValue.equals(
                                Statement.Effect.Allow.toString()) &&
                           !effectValue.equals(
                                Statement.Effect.Deny.toString())) {
                  response = responseGenerator.malformedPolicy(
                      "Invalid effect : " + effectValue);
                  LOGGER.error("Effect value is invalid in bucket policy - " +
                               effectValue);
                }
                isEffectFieldPresent = true;
              }
            } else if ("Principal".equals(objKey)) {
              if (obj.get(objKey) != null &&
                  obj.get(objKey) instanceof String) {
                if (obj.get(objKey).toString().isEmpty()) {
                  response = responseGenerator.malformedPolicy(
                      "Missing required field Principal cannot be empty!");
                  LOGGER.error("Principal value is empty..");
                } else if (!obj.get(objKey).toString().equals("*")) {
                  response = responseGenerator.malformedPolicy(
                      "Invalid policy syntax.");
                  LOGGER.error("Principal value is not following syntax..");
                }
              }
            }
          }
          if (!isEffectFieldPresent) {
            response = responseGenerator.malformedPolicy(
                "Missing required field Effect");
            LOGGER.error("Missing required field Effect");
            return response;
          }
        }
      } else {
        if (!JsonDocumentFields.POLICY_ID.equals(key) &&
            !JsonDocumentFields.VERSION.equals(key)) {
          response = responseGenerator.malformedPolicy(
              "Invalid bucket policy syntax.");
          LOGGER.error(
              "Invalid bucket policy syntax. Misplaced policy elements");
          break;
        }
      }
    }
    return response;
  }

 private
  ServerResponse validateResource(List<Resource> resourceValues,
                                  String inputBucket,
                                  List<String> actionsList) {
    ServerResponse response = null;
    boolean isMatchFound = false;
    if (resourceValues != null && !resourceValues.isEmpty()) {
      List<String> resources = PolicyUtil.convertCommaSeparatedStringToList(
          resourceValues.get(0).getId());
      for (String resourceArn : resources) {
        if (new S3ArnParser().isArnFormatValid(resourceArn)) {
          String resourceName =
              PolicyUtil.getResourceFromResourceArn(resourceArn);
          int slashPosition = resourceName.indexOf('/');
          if (slashPosition != -1) {
            resourceName = resourceName.substring(0, slashPosition);
          }
          // Just check for bucket name match. Input bucket and bucket inside
          // policy json must be same
          if (!resourceName.equals(inputBucket)) {
            // input bucket not valid
            response = responseGenerator.malformedPolicy(
                "Policy has invalid resource");
            LOGGER.error("Bucket name entered not valid");
            break;
          } else {
            // check for resource and action combination validity
            if (PolicyUtil.isActionValidForResource(actionsList,
                                                    slashPosition)) {
              isMatchFound = true;
              break;
            }
          }
        } else {  // arn format not valid
          response =
              responseGenerator.malformedPolicy("Policy has invalid resource");
          LOGGER.error("Resource ARN format not valid");
          break;
        }
      }
    } else {
      response =
          responseGenerator.malformedPolicy("Missing required field Resource");
      LOGGER.error("Missing required field Resource");
    }
    if (response == null && !isMatchFound) {
      response = responseGenerator.malformedPolicy(
          "Action does not apply to any resource(s) in statement");
      LOGGER.error("Action and resource combination invalid");
    }
    return response;
  }

  @Override ServerResponse
  validateActionAndResource(List<Action> actionList,
                            List<Resource> resourceValues, String inputBucket) {
    ServerResponse response = null;
    List<String> matchingActionsList = null;
    if (actionList != null && !actionList.isEmpty()) {
      for (Action action : actionList) {
        if (action != null && action.getActionName() != null &&
            !action.getActionName().isEmpty()) {
          matchingActionsList =
              PolicyUtil.getAllMatchingActions(action.getActionName());
          if (matchingActionsList == null || matchingActionsList.isEmpty()) {
            response =
                responseGenerator.malformedPolicy("Policy has invalid action");
            LOGGER.error("Policy has invalid action");
            break;
          } else {
            response = validateResource(resourceValues, inputBucket,
                                        matchingActionsList);
            if (response != null) {
              break;
            }
          }
        } else {
          response = responseGenerator.malformedPolicy(
              "Missing required field Action cannot be empty!");
          LOGGER.error("Required field Action is empty");
        }
      }
    } else {
      response =
          responseGenerator.malformedPolicy("Missing required field Action");
      LOGGER.error("Missing required field Action");
    }
    return response;
  }
}
