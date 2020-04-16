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
 * Original author:  Ajinkya Dhumal
 * Original creation date: 10-November-2019
 */

package com.seagates3.policy;

import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.amazonaws.auth.policy.Action;
import com.amazonaws.auth.policy.Condition;
import com.amazonaws.auth.policy.Principal;
import com.amazonaws.auth.policy.Resource;
import com.amazonaws.auth.policy.Statement;
import com.amazonaws.auth.policy.Statement.Effect;
import com.seagates3.dao.ldap.AccountImpl;
import com.seagates3.dao.ldap.UserImpl;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.PolicyResponseGenerator;

/**
 * Generic class to validate Policy. Sub classes of this class should be
 * instantiated in order to validate the policy.
 */
public
abstract class PolicyValidator {

 private
  final Logger LOGGER =
      LoggerFactory.getLogger(PolicyValidator.class.getName());

 protected
  PolicyResponseGenerator responseGenerator = null;

  abstract ServerResponse validatePolicy(String inputBucket, String jsonPolicy);

  /**
   * Validate if the Effect value is one of - Allow/Deny
   *
   * @param effectValue
   * @return {@link ServerResponse}
   */
 protected
  ServerResponse validateEffect(Effect effectValue) {
    ServerResponse response = null;
    if (effectValue != null) {
      if (Statement.Effect.valueOf(effectValue.name()) == null) {
        response = responseGenerator.malformedPolicy("Invalid effect : " +
                                                     effectValue.name());
        LOGGER.error("Effect value is invalid in bucket policy");
      }
    } else {
      response =
          responseGenerator.malformedPolicy("Missing required field Effect");
      LOGGER.error("Missing required field Effect");
    }
    return response;
  }

  /**
   * Validate the Conditions form Statement
   *
   * @param conditionList
 * @param actionList
   * @return {@link ServerResponse}
   */
 protected
  ServerResponse validateCondition(List<Condition> conditionList,
                                   List<Action> actionList) {
    ServerResponse response = null;
    ConditionUtil util = ConditionUtil.getInstance();
    if (conditionList != null) {
      for (Condition condition : conditionList) {
        String conditionType = condition.getType();
        String conditionKey = condition.getConditionKey();

        // Validate Condition Type
        if (!util.isConditionTypeValid(conditionType)) {
          response = responseGenerator.malformedPolicy(
              "Invalid Condition type : " + conditionType);
          LOGGER.error("Condition type - " + conditionType +
                       " is invalid in bucket policy");
          break;
        }

        // Validate Condition Key
        if (!util.isConditionKeyValid(conditionKey)) {
          response = responseGenerator.malformedPolicy(
              "Policy has an invalid condition key");
          LOGGER.error("Policy has an invalid condition key: " + conditionKey);
          break;
        }

        // Validate combination of Condition key and Action
        for (Action action : actionList) {
          if (!util.isConditionCombinationValid(conditionKey,
                                                action.getActionName())) {
            String errorMsg = "Conditions do not apply to combination of " +
                              "actions and resources in statement";
            response = responseGenerator.malformedPolicy(errorMsg);
            LOGGER.error(errorMsg + " for condition key: " + conditionKey);
            break;
          }
        }
      }
    }
    return response;
  }

  /**
   * Validate the Principal
   * @param principals
   * @return {@link ServerResponse}
   */
 protected
  ServerResponse validatePrincipal(List<Principal> principals) {
    ServerResponse response = null;
    if (principals != null && !principals.isEmpty()) {
      for (Principal principal : principals) {
        if (!principal.getProvider().equals("AWS") &&
            !principal.getProvider().equals("Service") &&
            !principal.getProvider().equals("Federated") &&
            !principal.getProvider().equals("*")) {

          response =
              responseGenerator.malformedPolicy("Invalid bucket policy syntax");
          LOGGER.error("Invalid bucket policy syntax");
          break;
        }
        if (!"*".equals(principal.getId()) && !isPrincipalIdValid(principal)) {
          response =
              responseGenerator.malformedPolicy("Invalid principal in policy");
          LOGGER.error("Invalid principal in policy");
          break;
        }
      }
    } else {
      response =
          responseGenerator.malformedPolicy("Missing required field Principal");
      LOGGER.error("Missing required field Principal");
    }
    return response;
  }

 private
  boolean isPrincipalIdValid(Principal principal) {
    boolean isValid = true;
    String provider = principal.getProvider();
    String id = principal.getId();
    AccountImpl accountImpl = new AccountImpl();
    UserImpl userImpl = new UserImpl();
    Account account = null;
    switch (provider) {
      case "AWS":
        account = null;
        User user = null;
        try {
          if (new PrincipalArnParser().isArnFormatValid(id)) {
            user = userImpl.findByArn(id);
            if (user == null || !user.exists()) {
              isValid = false;
            }
          } else {
            account = accountImpl.findByID(id);
            if (account == null || !account.exists()) {
              user = userImpl.findByUserId(id);
              if (user == null || !user.exists()) {
                isValid = false;
              }
            }
          }
        }
        catch (DataAccessException e) {
          LOGGER.error("Exception occurred while finding principal", e);
          isValid = false;
        }
        break;
      case "Service":
        account = null;
        try {
          account = accountImpl.findByCanonicalID(id);
        }
        catch (DataAccessException e) {
          LOGGER.error(
              "Exception occurred while finding account by canonical id", e);
        }
        if (account == null || !account.exists()) isValid = false;
        break;
      case "Federated":
        // Not supporting as of now
        isValid = false;
        break;
    }
    return isValid;
  }

  /**
   * Validate Action and Resource respectively first. Then validate each
   *Resource
   * against the Actions.
   *
   * @param actionList
   * @param resourceValues
   * @param inputBucket
   * @return {@link ServerResponse}
   */
  abstract ServerResponse
      validateActionAndResource(List<Action> actionList,
                                List<Resource> resourceValues,
                                String inputBucket);
}
