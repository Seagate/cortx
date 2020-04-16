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
 * Original author: Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 22-May-2019
 */
package com.seagates3.controller;

import java.util.Date;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.seagates3.authorization.Authorizer;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.dao.UserLoginProfileDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.parameter.validator.S3ParameterValidatorUtil;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.UserLoginProfileResponseGenerator;
import com.seagates3.response.generator.UserResponseGenerator;
import com.seagates3.util.DateUtil;

public
class UserLoginProfileController extends AbstractController {

 private
  final UserDAO userDAO;
 private
  final UserLoginProfileDAO userLoginProfileDAO;

 private
  final UserLoginProfileResponseGenerator userLoginProfileResponseGenerator;
 private
  final UserResponseGenerator userResponseGenerator;
 private
  final Logger LOGGER =
      LoggerFactory.getLogger(UserLoginProfileController.class.getName());

 public
  UserLoginProfileController(Requestor requestor,
                             Map<String, String> requestBody) {
    super(requestor, requestBody);

    userDAO = (UserDAO)DAODispatcher.getResourceDAO(DAOResource.USER);
    userLoginProfileDAO = (UserLoginProfileDAO)DAODispatcher.getResourceDAO(
        DAOResource.USER_LOGIN_PROFILE);
    userLoginProfileResponseGenerator = new UserLoginProfileResponseGenerator();
    userResponseGenerator = new UserResponseGenerator();
  }

  @Override public ServerResponse create() {
    User user;
    try {
      user = userDAO.find(requestor.getAccount().getName(),
                          requestBody.get("UserName"));
    }
    catch (DataAccessException ex) {
      return userResponseGenerator.internalServerError();
    }

    if (!user.exists()) {
      LOGGER.error("User [" + user.getName() + "] does not exists");
      return userResponseGenerator.noSuchEntity();
    } else {
      if (Authorizer.isRootUser(user)) {
        LOGGER.error(
            "Cannot create account login profile with CreateUserLoginProfile");
        return userResponseGenerator.invalidUserType("Create");
      }
      if (user.getPassword() == null) {
        try {
          // Validate new password as per password policy
          if (!S3ParameterValidatorUtil.validatePasswordPolicy(
                   requestBody.get("Password"))) {
            LOGGER.error(
                "Password does not conform to the account password policy");
            return userResponseGenerator.passwordPolicyVoilation();
          }
          user.setPassword(requestBody.get("Password"));
          user.setProfileCreateDate(
              DateUtil.toLdapDate(new Date(DateUtil.getCurrentTime())));
          if (requestBody.get("PasswordResetRequired") == null) {
            user.setPwdResetRequired("FALSE");
          } else {
            user.setPwdResetRequired(
                requestBody.get("PasswordResetRequired").toUpperCase());
          }
          userLoginProfileDAO.save(user);
        }
        catch (DataAccessException ex) {
          LOGGER.error("Exception occurred while saving user - " +
                       user.getName());
          return userLoginProfileResponseGenerator.internalServerError();
        }
      } else {
        LOGGER.error("LoginProfile already exists for user" + user.getName());
        return userLoginProfileResponseGenerator.entityAlreadyExists();
      }
    }

    return userLoginProfileResponseGenerator.generateCreateResponse(user);
  }

  /**
   * Below method will return login profile of the user requested
   */
  @Override public ServerResponse list() {
    User user = null;
    ServerResponse response = null;
    try {
      user = userDAO.find(requestor.getAccount().getName(),
                          requestBody.get("UserName"));
      if (!user.exists()) {
        LOGGER.error("User [" + user.getName() + "] does not exists");
        return userLoginProfileResponseGenerator.noSuchEntity();
      } else {
        if (Authorizer.isRootUser(user)) {
          LOGGER.error(
              "Cannot get account login profile with GetUserLoginProfile");
          return userResponseGenerator.invalidUserType("Get");
        }
        if (user.getPassword() == null) {
          LOGGER.error("LoginProfile not created for user - " +
                       requestBody.get("UserName"));
          return userResponseGenerator.noSuchEntity();
        } else {
          LOGGER.info("Password reset flag - " + user.getPwdResetRequired());
          response =
              userLoginProfileResponseGenerator.generateGetResponse(user);
        }
      }
    }
    catch (DataAccessException ex) {
      response = userResponseGenerator.internalServerError();
    }

    LOGGER.debug("Returned response is  - " + response.getResponseBody());
    return response;
  }

  /**
   * Below method will update login profile of requested user.
   */
  @Override public ServerResponse update() throws DataAccessException {
    User user = null;
    ServerResponse response = null;
    try {
      user = userDAO.find(requestor.getAccount().getName(),
                          requestBody.get("UserName"));

      if (!user.exists()) {
        LOGGER.error("User [" + requestBody.get("UserName") +
                     "] does not exists");
        response = userResponseGenerator.noSuchEntity();
      } else {
        if (Authorizer.isRootUser(user)) {
          LOGGER.error("Cannot update account login profile with " +
                       "UpdateUserLoginProfile");
          return userResponseGenerator.invalidUserType("Update");
        }
        if (user.getPassword() == null &&
            (user.getProfileCreateDate() == null ||
             user.getProfileCreateDate().isEmpty())) {
          LOGGER.error("LoginProfile not created for user - " +
                       requestBody.get("UserName"));
          String errorMessage =
              "Login Profile for User " + user.getName() + " cannot be found.";
          response = userResponseGenerator.noSuchEntity(errorMessage);
        } else {

          if (requestBody.get("Password") != null) {
            // Validate new password as per password policy
            if (!S3ParameterValidatorUtil.validatePasswordPolicy(
                     requestBody.get("Password"))) {
              LOGGER.error(
                  "Password does not conform to the account password policy");
              return userResponseGenerator.passwordPolicyVoilation();
            }

            user.setPassword(requestBody.get("Password"));
            LOGGER.info("Updating old password with new password");
          }

          if (requestBody.get("PasswordResetRequired") != null) {
            user.setPwdResetRequired(
                requestBody.get("PasswordResetRequired").toUpperCase());
            LOGGER.info("Updating password reset required flag");
          }
          userLoginProfileDAO.save(user);
          response = userLoginProfileResponseGenerator.generateUpdateResponse();
        }
      }
    }
    catch (DataAccessException ex) {
      LOGGER.error("Exception occurred while doing ldap operation for user - " +
                   requestBody.get("UserName"));
      response = userLoginProfileResponseGenerator.internalServerError();
    }
    return response;
  }

  /**
  * Below method will change password for current IAM user.
  * @throws DataAccessException
  */
  @Override public ServerResponse changepassword() throws DataAccessException {
    User user = null;
    ServerResponse response = null;
    try {
      user =
          userDAO.find(requestor.getAccount().getName(), requestor.getName());
      if (!user.exists()) {
        LOGGER.error("User [" + requestor.getName() + "] does not exists");
        response = userResponseGenerator.noSuchEntity();
      } else {
        if (Authorizer.isRootUser(user)) {
          LOGGER.error("Only IAM Users can change their own password");
          return userResponseGenerator.invalidUserType();
        }
        if (user.getPassword() == null) {
          LOGGER.error("LoginProfile not created for user - " +
                       requestor.getName());
          return userResponseGenerator.noSuchEntity();
        } else {
          String oldPassword = requestBody.get("OldPassword");
          String newPassword = requestBody.get("NewPassword");
          if (oldPassword != null && newPassword != null) {
            if (user.getPassword().equals(oldPassword) &&
                !oldPassword.equals(newPassword)) {
              // Validate new password as per password policy
              if (!S3ParameterValidatorUtil.validatePasswordPolicy(
                       newPassword)) {
                LOGGER.error(
                    "Password does not conform to the account password policy");
                return userResponseGenerator.passwordPolicyVoilation();
              }

              user.setPassword(newPassword);
              user.setPwdResetRequired("FALSE");
              LOGGER.info("changing old password with new password");
              userLoginProfileDAO.save(user);
              response = userLoginProfileResponseGenerator
                             .generateChangePasswordResponse();
            } else {
              response = userResponseGenerator.invalidPassword();
            }
          } else {
            response = userResponseGenerator.missingParameter();
          }
        }
      }
    }
    catch (DataAccessException ex) {
      LOGGER.error("Exception occurred while doing ldap operation for user - " +
                   requestor.getName());
      response = userLoginProfileResponseGenerator.internalServerError();
    }
    return response;
  }
}
