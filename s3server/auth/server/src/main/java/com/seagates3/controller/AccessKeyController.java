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
 * Original creation date: 17-Sep-2014
 */
package com.seagates3.controller;

import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.AccessKey.AccessKeyStatus;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.AccessKeyResponseGenerator;
import com.seagates3.util.KeyGenUtil;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class AccessKeyController extends AbstractController {

    AccessKeyDAO accessKeyDAO;
    UserDAO userDAO;
    AccessKeyResponseGenerator accessKeyResponseGenerator;

    private final Logger LOGGER =
                  LoggerFactory.getLogger(AccessKeyController.class.getName());

    public AccessKeyController(Requestor requestor, Map<String, String> requestBody) {
        super(requestor, requestBody);

        accessKeyDAO = (AccessKeyDAO) DAODispatcher.getResourceDAO(DAOResource.ACCESS_KEY);
        userDAO = (UserDAO) DAODispatcher.getResourceDAO(DAOResource.USER);
        accessKeyResponseGenerator = new AccessKeyResponseGenerator();
    }

    /**
     * Create an access key for the user. Maximum access keys allowed per user
     * are 2.
     *
     * @return ServerRepsonse
     */
    @Override
    public ServerResponse create() {
        String userName;

        if (requestBody.containsKey("UserName")) {
            userName = requestBody.get("UserName");
        } else {
            userName = requestor.getName();
        }

        User user;
        try {
            user = userDAO.find(requestor.getAccount().getName(), userName);
        } catch (DataAccessException ex) {
            return accessKeyResponseGenerator.internalServerError();
        }

        if (!user.exists()) {
            LOGGER.error("User [" + user.getName() + "] does not exists");
            return accessKeyResponseGenerator.noSuchEntity("a user");
        }

        try {
            if (accessKeyDAO.getCount(user.getId()) == 2) {
                LOGGER.error("Access key quota exceeded for user: "
                                                  + user.getName());
                return accessKeyResponseGenerator.accessKeyQuotaExceeded();
            }
        } catch (DataAccessException ex) {
            return accessKeyResponseGenerator.internalServerError();
        }

        LOGGER.info("Creating access key for user: " + user.getName());

        AccessKey accessKey = new AccessKey();
        accessKey.setUserId(user.getId());
        accessKey.setId(KeyGenUtil.createUserAccessKeyId());
        accessKey.setSecretKey(KeyGenUtil.generateSecretKey());
        accessKey.setStatus(AccessKeyStatus.ACTIVE);

        try {
            accessKeyDAO.save(accessKey);
        } catch (DataAccessException ex) {
            return accessKeyResponseGenerator.internalServerError();
        }

        return accessKeyResponseGenerator.generateCreateResponse(userName,
                accessKey);
    }

    /**
     * Delete an access key.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse delete() {
        AccessKey accessKey;
        try {
            accessKey = accessKeyDAO.find(requestBody.get("AccessKeyId"));
        } catch (DataAccessException ex) {
            return accessKeyResponseGenerator.internalServerError();
        }

        if (!accessKey.exists()) {
            LOGGER.error("Access key does not exists");
            return accessKeyResponseGenerator.noSuchEntity("an access key");
        }

        /**
         * When both access key id and username are given, ensure that the
         * access key id belongs to the user.
         */
        if (requestBody.containsKey("UserName")) {
            User user;
            try {
                user = userDAO.find(requestor.getAccount().getName(),
                        requestBody.get("UserName"));
            } catch (DataAccessException ex) {
                return accessKeyResponseGenerator.internalServerError();
            }

            if (!user.exists()) {
                LOGGER.error("User [" + user.getName() + "] does not exists");
                return accessKeyResponseGenerator.noSuchEntity("a user");
            }

            if (accessKey.getUserId().compareTo(user.getId()) != 0) {
                return accessKeyResponseGenerator.badRequest();
            }
        }

        LOGGER.info("Deleting access key");
        try {
            accessKeyDAO.delete(accessKey);
        } catch (DataAccessException ex) {
            return accessKeyResponseGenerator.internalServerError();
        }

        return accessKeyResponseGenerator.generateDeleteResponse();
    }

    /**
     * List the access keys belonging to the user.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse list() {
        String userName;

        if (requestBody.containsKey("UserName")) {
            userName = requestBody.get("UserName");
        } else {
            userName = requestor.getName();
        }

        User user;
        try {
            user = userDAO.find(requestor.getAccount().getName(), userName);
        } catch (DataAccessException ex) {
            LOGGER.error("Failed find user: " + userName + " in account: "
                                        + requestor.getAccount().getName());
            return accessKeyResponseGenerator.internalServerError();
        }

        if (!user.exists()) {
            LOGGER.error("User [" + user.getName() + "] does not exists");
            return accessKeyResponseGenerator.noSuchEntity("a user");
        }

        AccessKey[] accessKeyList;
        try {
            accessKeyList = accessKeyDAO.findAll(user);
        } catch (DataAccessException ex) {
            LOGGER.error("Failed to fine access keys of user: "
                                                + user.getName());
            return accessKeyResponseGenerator.internalServerError();
        }

        return accessKeyResponseGenerator.generateListResponse(userName,
                accessKeyList);
    }

    /**
     * Update the access key status.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse update() {
        AccessKey accessKey;
        User user = null;

        try {
            accessKey = accessKeyDAO.find(requestBody.get("AccessKeyId"));
        } catch (DataAccessException ex) {
            return accessKeyResponseGenerator.internalServerError();
        }

        if (!accessKey.exists()) {
            return accessKeyResponseGenerator.noSuchEntity("an access key");
        }

        /**
         * When both access key id and username are given, ensure that the
         * access key id belongs to the user.
         */
        if (requestBody.containsKey("UserName")) {
            try {
                user = userDAO.find(requestor.getAccount().getName(),
                        requestBody.get("UserName"));
            } catch (DataAccessException ex) {
                return accessKeyResponseGenerator.internalServerError();
            }

            if (!user.exists()) {
                LOGGER.error("User [" + user.getName() + "] does not exists");
                return accessKeyResponseGenerator.noSuchEntity("a user");
            } else if (accessKey.getUserId().compareTo(user.getId()) != 0) {
                LOGGER.error("Access key does not belong to user:"
                                                    + user.getName());
                return accessKeyResponseGenerator.invalidParametervalue(
                        "Access key does not belong to provided user.");
            }
        } else {
            try {
                user = userDAO.findByUserId(requestor.getAccount().getName(),
                        accessKey.getUserId());
            } catch (DataAccessException e) {
                return accessKeyResponseGenerator.internalServerError();
            }
        }

        if (user.getName().equals("root")) {
            LOGGER.error("Access key status for root user can not be changed.");
            return accessKeyResponseGenerator.operationNotSupported(
                    "Access key status for root user can not be changed."
            );
        }

        String newStatus = requestBody.get("Status");
        if (accessKey.getStatus().compareTo(newStatus) != 0) {
            try {
                accessKeyDAO.update(accessKey, newStatus);
            } catch (DataAccessException ex) {
                return accessKeyResponseGenerator.internalServerError();
            }
        }

        return accessKeyResponseGenerator.generateUpdateResponse();
    }
}
