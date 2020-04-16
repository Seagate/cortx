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
 * Original creation date: 24-July-2019
 */

package com.seagates3.acl;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.seagates3.dao.ldap.AccountImpl;
import com.seagates3.dao.ldap.GroupImpl;
import com.seagates3.model.Account;
import com.seagates3.model.Group;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AuthorizationResponseGenerator;

public
class ACLRequestValidator {

 protected
  final Logger LOGGER =
      LoggerFactory.getLogger(ACLRequestValidator.class.getName());
 protected
  AuthorizationResponseGenerator responseGenerator;
 protected
  static final Set<String> permissionHeadersSet = new HashSet<>();
 protected
  static final Set<String> cannedAclSet = new HashSet<>();

 public
  ACLRequestValidator() {
    responseGenerator = new AuthorizationResponseGenerator();
    initializePermissionHeadersSet();
    initializeCannedAclSet();
  }

  /**
   * Below method will initialize the set with valid permission headers
   */
 private
  void initializePermissionHeadersSet() {
    permissionHeadersSet.add("x-amz-grant-read");
    permissionHeadersSet.add("x-amz-grant-write");
    permissionHeadersSet.add("x-amz-grant-read-acp");
    permissionHeadersSet.add("x-amz-grant-write-acp");
    permissionHeadersSet.add("x-amz-grant-full-control");
  }

  /**
   * Below method will initialize the set with valid canned acls
   */
 private
  void initializeCannedAclSet() {
    cannedAclSet.add("private");
    cannedAclSet.add("public-read");
    cannedAclSet.add("public-read-write");
    cannedAclSet.add("authenticated-read");
    cannedAclSet.add("bucket-owner-read");
    cannedAclSet.add("bucket-owner-full-control");
    cannedAclSet.add("log-delivery-write");
    cannedAclSet.add("aws-exec-read");
  }

  /**
   * Below method will first check if only one of the below options is used in
   * request- 1. canned acl 2. permission header 3. acl in requestbody and then
   * method will perform requested account validation.
   *
   * @param requestBody
   * @return null- if request valid and errorResponse- if request not valid
   */
 public
  ServerResponse validateAclRequest(
      Map<String, String> requestBody,
      Map<String, List<Account>> accountPermissionMap,
      Map<String, List<Group>> groupPermissionMap) {
    int isCannedAclPresent = 0, isPermissionHeaderPresent = 0,
        isAclPresentInRequestBody = 0;
    ServerResponse response = new ServerResponse();
    // Check if canned ACL is present in request
    if (requestBody.get("x-amz-acl") != null) {
      isCannedAclPresent = 1;
    }
    // Check if permission header are present
    if (requestBody.get("x-amz-grant-read") != null ||
        requestBody.get("x-amz-grant-write") != null ||
        requestBody.get("x-amz-grant-read-acp") != null ||
        requestBody.get("x-amz-grant-write-acp") != null ||
        requestBody.get("x-amz-grant-full-control") != null) {
      isPermissionHeaderPresent = 1;
    }

    if (requestBody.get("ACL") != null) {
      isAclPresentInRequestBody = 1;
    }

    if ((isCannedAclPresent + isPermissionHeaderPresent +
         isAclPresentInRequestBody) > 1) {
      String errorMessage = null;
      if (isAclPresentInRequestBody == 1) {
        errorMessage = "This request does not support content";
        response = responseGenerator.unexpectedContent(errorMessage);
      } else {
        errorMessage =
            "Specifying both Canned ACLs and Header Grants is not allowed";
        response = responseGenerator.invalidRequest(errorMessage);
      }
      LOGGER.error(
          response.getResponseStatus() +
          " : Request failed: More than one options are used in request");
    }
    if (response.getResponseBody() == null ||
        response.getResponseStatus() == null) {
      if (isPermissionHeaderPresent == 1) {
        isValidPermissionHeader(requestBody, response, accountPermissionMap,
                                groupPermissionMap);
      } else if (isCannedAclPresent == 1 &&
                 !isValidCannedAcl(requestBody.get("x-amz-acl"))) {
        response = responseGenerator.badRequest();
      }
    }

    return response;
  }

 protected
  boolean isValidCannedAcl(String input) {
    boolean isValid = true;
    if (!cannedAclSet.contains(input)) {
      String errorMessage =
          "argument of type ' " + input + " ' is not iterable";
      LOGGER.error(errorMessage);
      isValid = false;
    }
    return isValid;
  }

  /**
   * Below method will validate the requested account
   *
   * @param requestBody
   * @return
   */
 protected
  boolean isValidPermissionHeader(
      Map<String, String> requestBody, ServerResponse response,
      Map<String, List<Account>> accountPermissionMap,
      Map<String, List<Group>> groupPermissionMap) {
    boolean isValid = true;
    for (String permissionHeader : permissionHeadersSet) {
      if (requestBody.get(permissionHeader) != null) {
        StringTokenizer tokenizer =
            new StringTokenizer(requestBody.get(permissionHeader), ",");
        List<String> valueList = new ArrayList<>();
        while (tokenizer.hasMoreTokens()) {
          valueList.add(tokenizer.nextToken());
        }
        for (String value : valueList) {
          if (!isGranteeValid(value, response, accountPermissionMap,
                              groupPermissionMap, permissionHeader)) {
            LOGGER.error(response.getResponseStatus() +
                         " : Grantee acocunt not valid");
            isValid = false;
            break;
          }
        }
      }
      // If any of the provided grantee is invalid then fail the request
      if (!isValid) {
        break;
      }
    }
    return isValid;
  }

  /**
   * Below method will validate the requested account
   * @param grantee
   * @return true/false
   */
 protected
  boolean isGranteeValid(String grantee, ServerResponse response,
                         Map<String, List<Account>> accountPermissionMap,
                         Map<String, List<Group>> groupPermissionMap,
                         String permission) {

    boolean isValid = true;
    AccountImpl accountImpl = new AccountImpl();
    GroupImpl groupImpl = new GroupImpl();
    int index = grantee.indexOf('=');
    if (index == -1) {
      response.setResponseBody(
          responseGenerator.invalidArgument().getResponseBody());
      response.setResponseStatus(
          responseGenerator.invalidArgument().getResponseStatus());
      return false;
        }
        String granteeDetails = grantee.substring(index + 1);
        String actualGrantee = grantee.substring(0, index);
        Account account = null;
        Group group = null;
        try {
          if (granteeDetails != null && !granteeDetails.trim().isEmpty()) {
            if (actualGrantee.equalsIgnoreCase("emailaddress")) {
              account = accountImpl.findByEmailAddress(granteeDetails);
            } else if (actualGrantee.equalsIgnoreCase("id")) {
              account = accountImpl.findByCanonicalID(granteeDetails);
            } else if (actualGrantee.equalsIgnoreCase("URI")) {
              group = groupImpl.getGroup(granteeDetails);
            }
            if (account != null && account.exists()) {
              updateAccountPermissionMap(accountPermissionMap, permission,
                                         account);
            } else if (group != null && group.exists()) {
              updateGroupPermissionMap(groupPermissionMap, permission, group);
            } else {
              setInvalidResponse(actualGrantee, response);
              isValid = false;
            }

          } else {
            setInvalidResponse(actualGrantee, response);
            isValid = false;
          }
        }
        catch (Exception e) {
          isValid = false;
          LOGGER.error("Exception occurred while validating grantee - ", e.getMessage());
        }
        return isValid;
  }

  /**
   * Updates the map if group exists
   *
   * @param groupPermissionMap
   * @param permission
   * @param account
   */
 private
  void updateGroupPermissionMap(Map<String, List<Group>> groupPermissionMap,
                                String permission, Group group) {
    if (groupPermissionMap.get(permission) == null) {
      List<Group> groupList = new ArrayList<>();
      groupList.add(group);
      groupPermissionMap.put(permission, groupList);
    } else {
      if (!isGroupAlreadyExists(groupPermissionMap.get(permission), group)) {
        groupPermissionMap.get(permission).add(group);
      }
    }
  }

 private
  void setInvalidResponse(String grantee, ServerResponse response) {
    ServerResponse actualResponse = null;
    if (grantee.equals("emailaddress")) {
      actualResponse = responseGenerator.unresolvableGrantByEmailAddress();
    } else if (grantee.equals("id")) {
      actualResponse = responseGenerator.invalidArgument("Invalid id");
    } else {
      actualResponse = responseGenerator.invalidArgument();
    }
    response.setResponseBody(actualResponse.getResponseBody());
    response.setResponseStatus(actualResponse.getResponseStatus());
  }

  /**
   * Updates the map if account exists
   *
   * @param accountPermissionMap
   * @param permission
   * @param account
   */
 private
  void updateAccountPermissionMap(
      Map<String, List<Account>> accountPermissionMap, String permission,
      Account account) {
    if (accountPermissionMap.get(permission) == null) {
      List<Account> accountList = new ArrayList<>();
      accountList.add(account);
      accountPermissionMap.put(permission, accountList);
    } else {
      if (!isAccountAlreadyExists(accountPermissionMap.get(permission),
                                  account)) {
        accountPermissionMap.get(permission).add(account);
      }
    }
  }

  /**
   * Below method will check if account is already present inside list or not
   *
   * @param accountList
   * @param account
   * @return true/false
   */
 private
  boolean isAccountAlreadyExists(List<Account> accountList, Account account) {
    boolean isExists = false;
    for (Account acc : accountList) {
      if (acc.getCanonicalId().equals(account.getCanonicalId())) {
        isExists = true;
        break;
      }
    }
    return isExists;
  }

  /**
   * Below method will check if group is already present inside list or not
   *
   * @param groupList
   * @param group
   * @return true/false
   */
 private
  boolean isGroupAlreadyExists(List<Group> groupList, Group group) {
    boolean isExists = false;
    for (Group grp : groupList) {
      if (grp.getPath().equals(group.getPath())) {
        isExists = true;
        break;
      }
    }
    return isExists;
  }
}
