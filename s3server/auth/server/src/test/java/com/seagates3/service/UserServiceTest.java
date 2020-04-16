/*
 * COPYRIGHT 2017 SEAGATE LLC
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
 * Original author: Sushant Mane <sushant.mane@seagate.com>
 * Original creation date: 12-Jan-2017
 */
package com.seagates3.service;

import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.dao.UserDAO;
import com.seagates3.model.Account;
import com.seagates3.model.User;
import com.seagates3.util.KeyGenUtil;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import static org.junit.Assert.*;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.powermock.api.mockito.PowerMockito.mockStatic;

@RunWith(PowerMockRunner.class)
@PrepareForTest({RequestorService.class, DAODispatcher.class, KeyGenUtil.class})
@PowerMockIgnore("javax.management.*")
@MockPolicy(Slf4jMockPolicy.class)
public class UserServiceTest {

    private UserDAO userDAO;
    private Account account;
    private User user;

    private final String accountName = "sampleUser";
    private final String userName = "sampleUserName";
    private final String roleName = "sampleUserRoleName";
    private final String roleSessionName = "sampleRoleSessionName";
    private final String userID = "SampleUserId";

    @Before
    public void setUp() throws Exception {
        user = new User();

        userDAO = mock(UserDAO.class);
        account = mock(Account.class);
        mockStatic(DAODispatcher.class);
        mockStatic(KeyGenUtil.class);

        when(DAODispatcher.getResourceDAO(DAOResource.USER)).thenReturn(userDAO);
        when(account.getName()).thenReturn(accountName);
        when(KeyGenUtil.createUserId()).thenReturn(userID);
    }

    @Test
    public void createRoleUserTest() throws Exception {
        String userRoleName = String.format("%s/%s", roleName, roleSessionName);
        when(userDAO.find(accountName, userRoleName)).thenReturn(user);

        User result = UserService.createRoleUser(account, roleName, roleSessionName);

        assertEquals(userID, result.getId());
        assertEquals(userRoleName, result.getRoleName());
        assertEquals(User.UserType.ROLE_USER, result.getUserType());
    }

    @Test
    public void createFederationUserTest() throws Exception {
        when(userDAO.find(accountName, userName)).thenReturn(user);

        User result = UserService.createFederationUser(account, userName);

        assertEquals(userID, result.getId());
        assertEquals(userName, result.getName());
        assertEquals(User.UserType.IAM_FED_USER, result.getUserType());
    }
}