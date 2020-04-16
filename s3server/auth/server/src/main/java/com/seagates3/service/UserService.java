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
 * Original creation date: 22-Jan-2016
 */
package com.seagates3.service;

import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.Account;
import com.seagates3.model.User;
import com.seagates3.util.KeyGenUtil;

public class UserService {

    /**
     * Search for the role user. If the user doesn't exist, create a new user
     *
     * @param account User Account.
     * @param roleName Role name.
     * @param roleSessionName Role session name.
     * @return User
     * @throws com.seagates3.exception.DataAccessException
     */
    public static User createRoleUser(Account account, String roleName,
            String roleSessionName) throws DataAccessException {
        String userRoleName = String.format("%s/%s", roleName, roleSessionName);

        UserDAO userDAO = (UserDAO) DAODispatcher.getResourceDAO(DAOResource.USER);
        User user = userDAO.find(account.getName(), userRoleName);

        if (!user.exists()) {
            user.setId(KeyGenUtil.createUserId());
            user.setRoleName(userRoleName);
            user.setUserType(User.UserType.ROLE_USER);
            userDAO.save(user);
        }

        return user;
    }

    /**
     * Search for the federation user. If the user doesn't exist, create a new
     * user
     *
     * TODO - Create a new class for federation user. Federation user should
     * contain a reference to the details of the permanent user whose
     * credentials were used at the time of creation. Federation user will
     * derive the policy settings of the permanent user.
     *
     * @param account User Name.
     * @param userName User name
     * @return User.
     * @throws com.seagates3.exception.DataAccessException
     */
    public static User createFederationUser(Account account, String userName)
            throws DataAccessException {
        UserDAO userDAO = (UserDAO) DAODispatcher.getResourceDAO(
                DAOResource.USER);
        User user = userDAO.find(account.getName(), userName);

        if (!user.exists()) {
            user.setId(KeyGenUtil.createUserId());
            user.setName(userName);
            user.setUserType(User.UserType.IAM_FED_USER);
            userDAO.save(user);

        }
        return user;
    }
}
