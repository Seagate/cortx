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

import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.GroupDAO;
import com.seagates3.dao.PolicyDAO;
import com.seagates3.dao.RoleDAO;
import com.seagates3.dao.UserDAO;
import com.seagates3.dao.ldap.LDAPUtils;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.AccessKey.AccessKeyStatus;
import com.seagates3.model.Account;
import com.seagates3.model.Group;
import com.seagates3.model.Policy;
import com.seagates3.model.Requestor;
import com.seagates3.model.Role;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AccountResponseGenerator;
import com.seagates3.s3service.S3AccountNotifier;
import com.seagates3.util.KeyGenUtil;

import io.netty.handler.codec.http.HttpResponseStatus;

import java.util.Map;


import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class AccountController extends AbstractController {

    private final Logger LOGGER = LoggerFactory.getLogger(AccountController.class.getName());
    private final AccountDAO accountDao;
    private final UserDAO userDAO;
    private final AccessKeyDAO accessKeyDAO;
    private final RoleDAO roleDAO;
    private final GroupDAO groupDAO;
    private final PolicyDAO policyDAO;
    private final AccountResponseGenerator accountResponseGenerator;
    private final S3AccountNotifier s3;
    private boolean internalRequest = false;
    public AccountController(Requestor requestor,
            Map<String, String> requestBody) {
        super(requestor, requestBody);

        accountResponseGenerator = new AccountResponseGenerator();
        accountDao = (AccountDAO) DAODispatcher.getResourceDAO(DAOResource.ACCOUNT);
        accessKeyDAO = (AccessKeyDAO) DAODispatcher.getResourceDAO(DAOResource.ACCESS_KEY);
        userDAO = (UserDAO) DAODispatcher.getResourceDAO(DAOResource.USER);
        roleDAO = (RoleDAO) DAODispatcher.getResourceDAO(DAOResource.ROLE);
        groupDAO = (GroupDAO) DAODispatcher.getResourceDAO(DAOResource.GROUP);
        policyDAO = (PolicyDAO) DAODispatcher.getResourceDAO(DAOResource.POLICY);
        s3 = new S3AccountNotifier();
    }

    /*
     * Fetch all accounts from database
     */
    public ServerResponse list() {
        Account[] accounts;

        try {
            accounts = accountDao.findAll();
        } catch (DataAccessException ex) {
            return accountResponseGenerator.internalServerError();
        }

        return accountResponseGenerator.generateListResponse(accounts);
    }

    @Override
    public ServerResponse create() {
        String name = requestBody.get("AccountName");
        String email = requestBody.get("Email");
        Account account;

        LOGGER.info("Creating account: " + name);

        try {
            account = accountDao.find(name);
        } catch (DataAccessException ex) {
            return accountResponseGenerator.internalServerError();
        }

        if (account.exists()) {
            return accountResponseGenerator.entityAlreadyExists();
        }

        account.setId(KeyGenUtil.createAccountId());

        try {
          // Generate unique canonical id for account
          String canonicalId = generateUniqueCanonicalId();
          if (canonicalId != null) {
            account.setCanonicalId(canonicalId);
          } else {
            // Failed to generate unique canonical id
            return accountResponseGenerator.internalServerError();
          }
        }
        catch (DataAccessException ex) {
          return accountResponseGenerator.internalServerError();
        }

        account.setEmail(email);

        try {
            accountDao.save(account);
        } catch (DataAccessException ex) {
          // Check for constraint violation exception for email address
          if (ex.getMessage().contains("some attributes not unique")) {
            try {
              account = accountDao.findByEmailAddress(email);
            }
            catch (DataAccessException e) {
              return accountResponseGenerator.internalServerError();
            }

            if (account.exists()) {
              return accountResponseGenerator.emailAlreadyExists();
            }
          }
            return accountResponseGenerator.internalServerError();
        }

        User root;
        try {
          root = createRootUser(name, account.getId());
        } catch (DataAccessException ex) {
            return accountResponseGenerator.internalServerError();
        }

        AccessKey rootAccessKey;
        try {
            rootAccessKey = createRootAccessKey(root);
        } catch (DataAccessException ex) {
            return accountResponseGenerator.internalServerError();
        }

        try {
          // Added delay so that newly created keys are replicated in ldap
          Thread.sleep(500);
        }
        catch (InterruptedException e) {
          LOGGER.error("Create account delay failing - ", e);
          Thread.currentThread().interrupt();
        }

        return accountResponseGenerator.generateCreateResponse(account, root,
                rootAccessKey);
    }

    /**
     * Generate canonical id and check if its unique in ldap
     * @throws DataAccessException
     */
   private
    String generateUniqueCanonicalId() throws DataAccessException {
      Account account;
      String canonicalId;
      for (int i = 0; i < 5; i++) {
        canonicalId = KeyGenUtil.createCanonicalId();
        account = accountDao.findByCanonicalID(canonicalId);

        if (!account.exists()) {
          return canonicalId;
        }
      }
      return null;
    }

    public ServerResponse resetAccountAccessKey() {
        String name = requestBody.get("AccountName");
        LOGGER.info("Resetting access key of account: " + name);

        Account account;
        try {
            account = accountDao.find(name);
        } catch (DataAccessException ex) {
            return accountResponseGenerator.internalServerError();
        }

        if (!account.exists()) {
            LOGGER.error("Account [" + name +"] doesnot exist");
            return accountResponseGenerator.noSuchEntity();
        }

        User root;
        try {
            root = userDAO.find(account.getName(), "root");
        } catch (DataAccessException e) {
            return accountResponseGenerator.internalServerError();
        }

        if (!root.exists()) {
                LOGGER.error("Root user of account [" + name +"] doesnot exist");
                return accountResponseGenerator.noSuchEntity();
        }

        // Delete Existing Root Access Keys
        LOGGER.info("Deleting existing access key of account: " + name);
        try {
            deleteAccessKeys(root);
        } catch (DataAccessException e) {
            return accountResponseGenerator.internalServerError();
        }

        LOGGER.debug("Creating new access key for account: " + name);
        AccessKey rootAccessKey;
        try {
            rootAccessKey = createRootAccessKey(root);
        } catch (DataAccessException ex) {
            return accountResponseGenerator.internalServerError();
        }
        try {
          // Added delay so that newly created keys are replicated in ldap
          Thread.sleep(500);
        }
        catch (InterruptedException e) {
          LOGGER.error("Reset key  delay failing - ", e);
          Thread.currentThread().interrupt();
        }
        return accountResponseGenerator.generateResetAccountAccessKeyResponse(
                                          account, root, rootAccessKey);
    }

    /*
     * Create a root user for the account.
     */
   private
    User createRootUser(String accountName,
                        String accountId) throws DataAccessException {
        User user = new User();
        user.setAccountName(accountName);
        user.setName("root");
        user.setPath("/");
        user.setUserType(User.UserType.IAM_USER);
        user.setId(KeyGenUtil.createUserId());
        user.setArn("arn:aws:iam::" + accountId + ":root");
        LOGGER.info("Creating root user for account: " + accountName);

        userDAO.save(user);
        return user;
    }

    /*
     * Create access keys for the root user.
     */
    private AccessKey createRootAccessKey(User root) throws DataAccessException {
        AccessKey accessKey = new AccessKey();
        accessKey.setUserId(root.getId());
        accessKey.setId(KeyGenUtil.createUserAccessKeyId());
        accessKey.setSecretKey(KeyGenUtil.generateSecretKey());
        accessKey.setStatus(AccessKeyStatus.ACTIVE);

        accessKeyDAO.save(accessKey);

        return accessKey;
    }

    @Override
    public ServerResponse delete() {
        String name = requestBody.get("AccountName");
        Account account;

        LOGGER.info("Deleting account: " + name);

        try {
            account = accountDao.find(name);
        } catch (DataAccessException ex) {
            return accountResponseGenerator.internalServerError();
        }

        if (!account.exists()) {
            return accountResponseGenerator.noSuchEntity();
        }

        User root;
        try {
            root = userDAO.find(account.getName(), "root");
        } catch (DataAccessException e) {
            return accountResponseGenerator.internalServerError();
        }

        if (!requestor.getId().equals(root.getId())) {
            return accountResponseGenerator.unauthorizedOperation();
        }

        boolean force = false;
        if (requestBody.containsKey("force")) {
            force = Boolean.parseBoolean(requestBody.get("force"));
        }

        //Notify S3 Server of account deletion

        if (!internalRequest) {
            LOGGER.debug("Sending delete account [" + account.getName() +
                                       "] notification to S3 Server");
            ServerResponse resp = s3.notifyDeleteAccount(
                account.getId(), requestor.getAccesskey().getId(),
                requestor.getAccesskey().getSecretKey(),
                requestor.getAccesskey().getToken());
            if(!resp.getResponseStatus().equals(HttpResponseStatus.OK)) {
                LOGGER.error("Account [" + account.getName() + "] delete "
                    + "notification failed.");
                return resp;
            }
        }

        try {
            if (force) {
                deleteUsers(account, "/");
                deleteRoles(account, "/");
                deleteGroups(account, "/");
                deletePolicies(account, "/");
            } else {
                User[] users = userDAO.findAll(account.getName(), "/");
                if (users.length == 1 && "root".equals(users[0].getName())) {
                    deleteUser(users[0]);
                }
            }

            accountDao.deleteOu(account, LDAPUtils.USER_OU);
            accountDao.deleteOu(account, LDAPUtils.ROLE_OU);
            accountDao.deleteOu(account, LDAPUtils.GROUP_OU);
            accountDao.deleteOu(account, LDAPUtils.POLICY_OU);
            accountDao.delete(account);
        } catch (DataAccessException e) {
            if (e.getLocalizedMessage().contains("subordinate objects must be deleted first")) {
                return accountResponseGenerator.deleteConflict();
            }

            return accountResponseGenerator.internalServerError();
        }

        return accountResponseGenerator.generateDeleteResponse();
    }

    private void deleteAccessKeys(User user) throws DataAccessException {

        LOGGER.info("Deleting all access keys of user: " + user.getName());

        AccessKey[] accessKeys = accessKeyDAO.findAll(user);
        for (AccessKey accessKey : accessKeys) {
            accessKeyDAO.delete(accessKey);
        }
    }

    private void deleteUser(User user) throws DataAccessException {

        LOGGER.info("Deleting user: " + user.getName());

        deleteAccessKeys(user);
        userDAO.delete(user);
    }

    private void deleteUsers(Account account, String path) throws DataAccessException {

        LOGGER.info("Deleting all users of account: " + account.getName());

        User[] users = userDAO.findAll(account.getName(), path);
        for (User user : users) {
            deleteUser(user);
        }
    }

    private void deleteRoles(Account account, String path) throws DataAccessException {

        LOGGER.info("Deleting all associated roles of account: "
                                            + account.getName());

        Role[] roles = roleDAO.findAll(account, path);
        for (Role role : roles) {
            roleDAO.delete(role);
        }
    }

    private void deleteGroups(Account account, String path) {
        // TODO
    }

    private void deletePolicies(Account account, String path) {
        // TODO
    }
}

