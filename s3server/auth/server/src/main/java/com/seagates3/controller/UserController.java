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
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Requestor;
import com.seagates3.model.User;
import com.seagates3.response.ServerResponse;
import com.seagates3.response.generator.UserResponseGenerator;
import com.seagates3.util.KeyGenUtil;
import java.util.Map;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class UserController extends AbstractController {

    private final UserDAO userDAO;
    private final UserResponseGenerator userResponseGenerator;
    private final Logger LOGGER =
            LoggerFactory.getLogger(UserController.class.getName());
    private
     static final String USER_ID_PREFIX = "AIDA";

    public UserController(Requestor requestor,
            Map<String, String> requestBody) {
        super(requestor, requestBody);

        userDAO = (UserDAO) DAODispatcher.getResourceDAO(DAOResource.USER);
        userResponseGenerator = new UserResponseGenerator();
    }

    /**
     * Create a new IAM user.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse create() {
        User user;
        try {
            user = userDAO.find(requestor.getAccount().getName(),
                    requestBody.get("UserName"));
        } catch (DataAccessException ex) {
            return userResponseGenerator.internalServerError();
        }

        if (user.exists()) {
            LOGGER.error("User [" + user.getName() + "] already exists");
            return userResponseGenerator.entityAlreadyExists();
        }

        LOGGER.info("Creating user : " + user.getName());

        if (requestBody.containsKey("path")) {
            user.setPath(requestBody.get("path"));
        } else {
            user.setPath("/");
        }

        user.setUserType(User.UserType.IAM_USER);
        // UserId should starts with "AIDA".
        // for more info, please see EOS-2790
        user.setId(USER_ID_PREFIX + KeyGenUtil.createIamUserId());

        // create and set arn here
        String arn = "arn:aws:iam::" + requestor.getAccount().getId() +
                     ":user/" + user.getName();
        LOGGER.debug("Creating and setting ARN - " + arn);
        user.setArn(arn);
        try {
            userDAO.save(user);
        } catch (DataAccessException ex) {
            return userResponseGenerator.internalServerError();
        }

        return userResponseGenerator.generateCreateResponse(user);
    }

    /**
     * Delete an IAM user.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse delete() {
        Boolean userHasAccessKeys;
        User user;
        try {
            user = userDAO.find(requestor.getAccount().getName(),
                    requestBody.get("UserName"));
        } catch (DataAccessException ex) {
            return userResponseGenerator.internalServerError();
        }

        if (!user.exists()) {
            LOGGER.error("User [" + user.getName() + "] does not exists");
            return userResponseGenerator.noSuchEntity();
        }

        LOGGER.info("Deleting user : " + user.getName());

        AccessKeyDAO accessKeyDao
                = (AccessKeyDAO) DAODispatcher.getResourceDAO(DAOResource.ACCESS_KEY);

        try {
            userHasAccessKeys = accessKeyDao.hasAccessKeys(user.getId());
        } catch (DataAccessException ex) {
            return userResponseGenerator.internalServerError();
        }

        if (userHasAccessKeys) {
            return userResponseGenerator.deleteConflict();
        }

        try {
            userDAO.delete(user);
        } catch (DataAccessException ex) {
            return userResponseGenerator.internalServerError();
        }

        return userResponseGenerator.generateDeleteResponse();
    }

    /**
     * List all the IAM users having the given path prefix.
     *
     * @return ServerResponse.
     */
    @Override
    public ServerResponse list() {
        String pathPrefix;

        if (requestBody.containsKey("PathPrefix")) {
            pathPrefix = requestBody.get("PathPrefix");
        } else {
            pathPrefix = "/";
        }

        User[] userList;
        try {
            userList = userDAO.findAll(requestor.getAccount().getName(),
                    pathPrefix);
        } catch (DataAccessException ex) {
            return userResponseGenerator.internalServerError();
        }

        LOGGER.info("Listing users of account : "
                      + requestor.getAccount().getName());
        return userResponseGenerator.generateListResponse(userList);
    }

    /**
     * Update the name or path of an existing IAM user.
     *
     * @return ServerResponse
     */
    @Override
    public ServerResponse update() {
        String newUserName = null, newPath = null;

        if (!requestBody.containsKey("NewUserName")
                && !requestBody.containsKey("NewPath")) {
            return userResponseGenerator.missingParameter();
        }

        if ("root".equals(requestBody.get("UserName")) &&
                requestBody.containsKey("NewUserName")) {
            LOGGER.error("Cannot change user name of root user.");
            return userResponseGenerator.operationNotSupported(
                    "Cannot change user name of root user."
            );
        }

        User user;
        try {
            user = userDAO.find(requestor.getAccount().getName(),
                    requestBody.get("UserName"));
        } catch (DataAccessException ex) {
            return userResponseGenerator.internalServerError();
        }

        if (!user.exists()) {
            LOGGER.error("User [" + user.getName() + "] does not exists");
            return userResponseGenerator.noSuchEntity();
        }

        LOGGER.info("Updating user : " + user.getName());

        if (requestBody.containsKey("NewUserName")) {
            newUserName = requestBody.get("NewUserName");
            User newUser;

            try {
                newUser = userDAO.find(requestor.getAccount().getName(),
                        newUserName);
            } catch (DataAccessException ex) {
                return userResponseGenerator.internalServerError();
            }

            if (newUser.exists()) {
                return userResponseGenerator.entityAlreadyExists();
            }
        }

        if (requestBody.containsKey("NewPath")) {
            newPath = requestBody.get("NewPath");
        }

        try {
            userDAO.update(user, newUserName, newPath);
        } catch (DataAccessException ex) {
            return userResponseGenerator.internalServerError();
        }

        return userResponseGenerator.generateUpdateResponse();
    }
}
