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
 * Original creation date: 10-Jan-2016
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.dao.AccountDAO;
import com.seagates3.dao.DAODispatcher;
import com.seagates3.dao.DAOResource;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.Account;
import com.seagates3.model.Requestor;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.internal.matchers.apachecommons.ReflectionEquals;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PrepareForTest({DAODispatcher.class, LDAPUtils.class})
@PowerMockIgnore({"javax.management.*"})
public class RequestorImplTest {

    private final String BASE_DN = "ou=accounts,dc=s3,dc=seagate,dc=com";
    private final String[] FIND_ATTRS = {"cn"};

    private final RequestorImpl requestorImpl;

    private final LDAPSearchResults ldapResults;
    private final LDAPEntry entry;
    private final LDAPAttribute commonNameAttr;

    @Rule
    public final ExpectedException exception = ExpectedException.none();

    public RequestorImplTest() {
        requestorImpl = new RequestorImpl();
        ldapResults = Mockito.mock(LDAPSearchResults.class);
        entry = Mockito.mock(LDAPEntry.class);
        commonNameAttr = Mockito.mock(LDAPAttribute.class);
    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(LDAPUtils.class);
    }

    @Test
    public void Find_UserDoesNotExist_ReturnEmptyRequestor() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("ak=AKIATEST");

        Requestor expectedRequestor = new Requestor();
        expectedRequestor.setAccessKey(accessKey);

        Requestor requestor = requestorImpl.find(accessKey);
        Assert.assertThat(expectedRequestor,
                new ReflectionEquals(requestor));
    }

    @Test
    public void Find_UserSearchFailed_ThrowException() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("ak=AKIATEST");
        accessKey.setUserId("123");

        String filter = "s3userid=123";

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "search",
                BASE_DN, 2, filter, FIND_ATTRS
        );

        exception.expect(DataAccessException.class);

        requestorImpl.find(accessKey);
    }

    @Test
    public void Find_UserSearchEmpty_ThrowException() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("ak=AKIATEST");
        accessKey.setUserId("123");

        String filter = "s3userid=123";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.doReturn(Boolean.FALSE).when(ldapResults).hasMore();

        exception.expect(DataAccessException.class);

        requestorImpl.find(accessKey);
    }

    @Test(expected = DataAccessException.class)
    public void Find_GetEntryShouldThrowException() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("ak=AKIATEST");
        accessKey.setUserId("123");
        String filter = "s3userid=123";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.doReturn(Boolean.TRUE).when(ldapResults).hasMore();
        Mockito.when(ldapResults.next()).thenThrow(new LDAPException());

        requestorImpl.find(accessKey);
    }

    @Test
    public void Find_UserFound_ReturnRequestor() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("ak=AKIATEST");
        accessKey.setUserId("123");

        Account account = new Account();
        account.setId("12345");
        account.setName("s3test");

        Requestor expectedRequestor = new Requestor();
        expectedRequestor.setAccessKey(accessKey);
        expectedRequestor.setId("123");
        expectedRequestor.setAccount(account);
        expectedRequestor.setName("s3testuser");

        String filter = "s3userid=123";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);

        Mockito.when(ldapResults.next()).thenReturn(entry);

        String dn = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";
        Mockito.when(entry.getDN())
                .thenReturn(dn);

        Mockito.when(entry.getAttribute("cn")).thenReturn(commonNameAttr);
        Mockito.when(commonNameAttr.getStringValue()).thenReturn("s3testuser");

        AccountDAO accountDAO = Mockito.mock(AccountDAO.class);
        PowerMockito.mockStatic(DAODispatcher.class);
        PowerMockito.doReturn(accountDAO).when(DAODispatcher.class,
                "getResourceDAO", DAOResource.ACCOUNT
        );
        Mockito.doReturn(account).when(accountDAO).find("s3test");

        Requestor requestor = requestorImpl.find(accessKey);
        Assert.assertThat(expectedRequestor, new ReflectionEquals(requestor));
    }
}
