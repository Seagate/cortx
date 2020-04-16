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

import com.seagates3.dao.AccessKeyDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.model.AccessKey;
import com.seagates3.model.User;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import static org.junit.Assert.*;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.verifyStatic;
import static org.powermock.api.mockito.PowerMockito.when;

@RunWith(PowerMockRunner.class)
@PrepareForTest({DAODispatcher.class})
@PowerMockIgnore("javax.management.*")
public class AccessKeyServiceTest {

    private User user;
    private AccessKeyDAO accessKeyDAO;
    private final long timeToExpire = 9000L;

    @Before
    public void setUp() throws Exception {
        user = mock(User.class);
        accessKeyDAO = mock(AccessKeyDAO.class);
        mockStatic(DAODispatcher.class);
        when(DAODispatcher.class, "getResourceDAO", DAOResource.ACCESS_KEY)
                .thenReturn(accessKeyDAO);
    }

    @Test
    public void createFedAccessKeyTest() throws Exception {
        when(user.getId()).thenReturn("MH12NS5144");

        AccessKey result = AccessKeyService.createFedAccessKey(user, timeToExpire);

        assertNotNull(result);
        assertNotNull(result.getSecretKey());
        assertNotNull(result.getId());
        assertNotNull(result.getExpiry());
        assertNotNull(result.getUserId());
        assertEquals("MH12NS5144", result.getUserId());
        assertTrue(result.isAccessKeyActive());

        verifyStatic();
        DAODispatcher.getResourceDAO(DAOResource.ACCESS_KEY);

        verify(accessKeyDAO).save(any(AccessKey.class));
        accessKeyDAO.save(any(AccessKey.class));
    }

    @Test(expected = NullPointerException.class)
    public void createFedAccessKeyTest_UserID_Null_ShouldThrowNullPointerException()
            throws Exception {
        AccessKeyService.createFedAccessKey(null, timeToExpire);
    }

    @Test(expected = NullPointerException.class)
    public void createFedAccessKeyTest_ShouldThrowNullPointerException() throws Exception {
        mockStatic(DAODispatcher.class);
        when(DAODispatcher.class, "getResourceDAO", DAOResource.ACCESS_KEY).thenReturn(null);

        AccessKeyService.createFedAccessKey(user, timeToExpire);
    }
}