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
 * Original creation date: 27-June-2019
 */

package com.seagates3.controller;

import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.novell.ldap.LDAPException;
import com.seagates3.authorization.Authorizer;
import com.seagates3.authserver.AuthServerConstants;
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.dao.ldap.LDAPUtils;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.TempAuthCredentialsResponseGenerator;
import com.seagates3.service.AccessKeyService;

public
class TempAuthCredentialsController extends AbstractController {

 private
  TempAuthCredentialsResponseGenerator authCredentialsResponseGenerator;
 private
  final UserDAO userDAO;
 private
  final Logger LOGGER =
      LoggerFactory.getLogger(TempAuthCredentialsController.class.getName());

 public
  TempAuthCredentialsController(Requestor requestor,
                                Map<String, String> requestBody) {
    super(requestor, requestBody);
    authCredentialsResponseGenerator =
        new TempAuthCredentialsResponseGenerator();
    userDAO = (UserDAO)DAODispatcher.getResourceDAO(DAOResource.USER);
  }

  /**
   * Below method will return temporary credentials for requested
   * user/account
   */

  @Override public ServerResponse create() throws DataAccessException {
    ServerResponse response = null;
    User user = null;
    Account account = null;
    try {
      account = getAccount();
      user = (account != null) ? getUser() : null;
      if (user == null) {
        response = authCredentialsResponseGenerator.noSuchEntity();
      } else {
        response = checkForPasswordResetFlag(account, user);
        if (response == null) {
          int durationArr[] = new int[1];
          response = getDuration(user, durationArr);
          if (response == null) {
            authenticateWithLdap(account, user);
            LOGGER.debug("LDAP Authentication successfull");
            AccessKey accessKey =
                AccessKeyService.createFedAccessKey(user, durationArr[0]);
            if (accessKey == null) {
              LOGGER.debug("AccessKey is null");
              response = authCredentialsResponseGenerator.internalServerError();
            } else {
              response =
                  authCredentialsResponseGenerator.generateCreateResponse(
                      user.getName(), accessKey);
              LOGGER.debug("Successfully generated Create response");
            }
          }
        }
      }
    }
    catch (NumberFormatException e) {
      LOGGER.error("NumberFormatException occurred - " + e);
      response = authCredentialsResponseGenerator.invalidParametervalue(
          "Invalid value for parameter - duration");
    }
    catch (LDAPException e) {
      LOGGER.error("LDAPException occurred - " + e);
      LOGGER.error("LDAPException result code - " + e.getResultCode());
      if (e.getResultCode() == LDAPException.INVALID_CREDENTIALS) {
        response = authCredentialsResponseGenerator.invalidCredentials();
      } else {
        response = authCredentialsResponseGenerator.internalServerError();
      }
    }
    catch (DataAccessException e) {

      response = authCredentialsResponseGenerator.internalServerError();
    }
    return response;
  }

  /**
   * Below method will perform duration limit checks considering if user is
   * root user or IAM user Numbers/limits are as per AWS standards
   *
   * @param user
   * @param returnResponse
   * @return
   */
 private
  ServerResponse getDuration(User user, int[] durationArray) {

    String durationInRequest = requestBody.get("Duration");
    ServerResponse returnResponse = null;
    boolean isMaxDurationIntervalExceeded = false;

    if (Authorizer.isRootUser(user)) {
      if (durationInRequest == null) {
        durationInRequest = String.valueOf(
            AuthServerConstants.MAX_AND_DEFAULT_ROOT_USER_DURATION);
      } else if (Integer.parseInt(durationInRequest) >
                 AuthServerConstants.MAX_AND_DEFAULT_ROOT_USER_DURATION) {
        isMaxDurationIntervalExceeded = true;
      }
    } else {
      if (durationInRequest == null) {
        durationInRequest = AuthServerConstants.DEFAULT_IAM_USER_DURATION;
      } else if (Integer.parseInt(durationInRequest) >
                 AuthServerConstants.MAX_IAM_USER_DURATION) {
        isMaxDurationIntervalExceeded = true;
      }
    }
    int duration = Integer.parseInt(durationInRequest);
    if (duration < AuthServerConstants.MIN_ROOT_IAM_USER_DURATION) {
      returnResponse =
          authCredentialsResponseGenerator.minDurationIntervalViolated();
    } else if (isMaxDurationIntervalExceeded) {
      returnResponse =
          authCredentialsResponseGenerator.maxDurationIntervalExceeded();
    } else {
      durationArray[0] = duration;
    }
    return returnResponse;
  }

  /**
   * Below method will create DN based on root user or IAM user and do a bind
   * call for authentication
   *
   * @param account
   * @param user
   * @throws LDAPException
   */

 private
  void authenticateWithLdap(Account account, User user) throws LDAPException {
    String password = requestBody.get("Password");
    String dn = null;

    if (Authorizer.isRootUser(user)) {

      dn = String.format("%s=%s,%s=accounts,%s", LDAPUtils.ORGANIZATIONAL_NAME,
                         account.getName(), LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                         LDAPUtils.BASE_DN);
    } else {
      dn = String.format("%s=%s,%s=%s,%s=%s,%s=%s,%s", LDAPUtils.USER_ID,
                         user.getId(), LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                         LDAPUtils.USER_OU, LDAPUtils.ORGANIZATIONAL_NAME,
                         account.getName(), LDAPUtils.ORGANIZATIONAL_UNIT_NAME,
                         LDAPUtils.ACCOUNT_OU, LDAPUtils.BASE_DN);
    }

    LOGGER.info("dn is ---- " + dn);
    LDAPUtils.bind(dn, password);
  }

  /**
   * Below method will check if account exist in LDAP or no
   *
   * @return
   * @throws DataAccessException
   */
 private
  Account getAccount() throws DataAccessException {
    String accountName = requestBody.get("AccountName");
    AccountDAO accountDao =
        (AccountDAO)DAODispatcher.getResourceDAO(DAOResource.ACCOUNT);
    Account account = accountDao.find(accountName);
    if (!account.exists()) {
      LOGGER.error("Account [" + accountName + "] doesnot exist");
      account = null;
    }
    return account;
  }

  /**
   * Below method will check if user exist in LDAP or no
   *
   * @return
   * @throws DataAccessException
   */
 private
  User getUser() throws DataAccessException {
    String userName = requestBody.get("UserName");
    String accountName = requestBody.get("AccountName");
    User user = null;
    if (userName == null) {
      user = userDAO.find(accountName, "root");
    } else {
      user = userDAO.find(accountName, userName);
    }
    if (!user.exists()) {
      LOGGER.error("User - " + userName + " doesn't exist under account - " +
                   accountName);
      user = null;
    }
    return user;
  }

  /**
   * Below method will check if password reset required flag is true for
   * account/user depend upon the request. If request is for IAM user then
   * check the flag for that particulr user and if request is for
   * root/account user then check flag for account
   *
   * @param account
   * @param user
   * @return
   */
 private
  ServerResponse checkForPasswordResetFlag(Account account, User user) {
    ServerResponse response = null;
    if (Authorizer.isRootUser(user)) {
      if ("True".equalsIgnoreCase(account.getPwdResetRequired())) {
        response = authCredentialsResponseGenerator.passwordResetRequired();
        LOGGER.debug("PwdResetFlag is true for account");
      }
    } else {
      if ("True".equalsIgnoreCase(user.getPwdResetRequired())) {
        response = authCredentialsResponseGenerator.passwordResetRequired();
        LOGGER.debug("PwdResetFlag is true for user");
      }
    }
    return response;
  }
}
