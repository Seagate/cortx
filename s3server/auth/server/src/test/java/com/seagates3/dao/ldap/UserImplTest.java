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
 * Original creation date: 05-Jan-2016
 */
package com.seagates3.dao.ldap;

import static org.mockito.Mockito.times;

import java.util.ArrayList;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.internal.matchers.apachecommons.ReflectionEquals;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPModification;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.User;

@RunWith(PowerMockRunner.class)
    @PrepareForTest({LDAPUtils.class, UserImpl.class})
    @MockPolicy(Slf4jMockPolicy.class)
    @PowerMockIgnore({"javax.management.*"}) public class UserImplTest {

    private final String FIND_FILTER = "(cn=s3testuser)";
    private final String FIND_BYUSERID_FILTER = "(s3userid=s3UserId)";

    private
     final String[] FIND_ATTRS = {
         "s3userid",     "path",        "arn",
         "rolename",     "objectclass", "createtimestamp",
         "userPassword", "pwdReset",    "profileCreateDate"};
    private
     final String[] FIND_ALL_ATTRS = {"s3userid",        "cn", "path",
                                      "createtimestamp", "arn"};
    private
     final String[] FIND_BYUSERID_ATTRS = {
         "cn", "path", "arn", "rolename", "objectclass", "createtimestamp"};

    private final String LDAP_DATE;
    private final String EXPECTED_DATE;

    private final UserImpl userImpl;
    private final LDAPSearchResults ldapResults;
    private final LDAPEntry entry;

    private final LDAPAttribute userIdAttr;
    private final LDAPAttribute objectClassAttr;
    private final LDAPAttribute pathAttr;
    private final LDAPAttribute createTimeStampAttr;
    private final LDAPAttribute commonNameAttr;
    private final LDAPAttribute roleAttr;
    private
     final LDAPAttribute arn;

    @Rule
    public final ExpectedException exception = ExpectedException.none();

    public UserImplTest() {
        userImpl = new UserImpl();
        ldapResults = Mockito.mock(LDAPSearchResults.class);
        entry = Mockito.mock(LDAPEntry.class);
        userIdAttr = Mockito.mock(LDAPAttribute.class);
        objectClassAttr = Mockito.mock(LDAPAttribute.class);
        pathAttr = Mockito.mock(LDAPAttribute.class);
        createTimeStampAttr = Mockito.mock(LDAPAttribute.class);
        commonNameAttr = Mockito.mock(LDAPAttribute.class);
        roleAttr = Mockito.mock(LDAPAttribute.class);
        arn = Mockito.mock(LDAPAttribute.class);

        LDAP_DATE = "20160129160752Z";
        EXPECTED_DATE = "2016-01-29T16:07:52.000+0000";
    }

    private void setupUserAttr() throws Exception {
        PowerMockito.mockStatic(LDAPUtils.class);
        Mockito.when(ldapResults.next()).thenReturn(entry);

        Mockito.when(entry.getAttribute("s3userid")).thenReturn(userIdAttr);
        Mockito.when(userIdAttr.getStringValue()).thenReturn("123");

        Mockito.when(entry.getAttribute("objectclass"))
                .thenReturn(objectClassAttr);
        Mockito.when(objectClassAttr.getStringValue()).thenReturn("iamUser");

        Mockito.when(entry.getAttribute("path")).thenReturn(pathAttr);
        Mockito.when(pathAttr.getStringValue()).thenReturn("/");

        Mockito.when(entry.getAttribute("createtimestamp"))
                .thenReturn(createTimeStampAttr);
        Mockito.when(createTimeStampAttr.getStringValue())
            .thenReturn(LDAP_DATE);

        Mockito.when(entry.getAttribute("cn")).thenReturn(commonNameAttr);
        Mockito.when(commonNameAttr.getStringValue()).thenReturn("s3testuser");
        Mockito.when(entry.getAttribute("arn")).thenReturn(arn);
        Mockito.when(arn.getStringValue())
            .thenReturn("arn:aws:iam::accountid:user/s3test");
    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(LDAPUtils.class);
        Mockito.when(ldapResults.next()).thenReturn(entry);
    }

    @Test
    public void Find_LdapSearchFailed_ThrowException() throws Exception {
        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";
        PowerMockito.doThrow(new LDAPException()).when(
            LDAPUtils.class, "search", userBaseDN, 2, FIND_FILTER, FIND_ATTRS);
        exception.expect(DataAccessException.class);

        userImpl.find("s3test", "s3testuser");
    }

    @Test
    public void Find_UserDoesNotExist_ReturnUserObject() throws Exception {
        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");

        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";

        PowerMockito.doReturn(ldapResults).when(
            LDAPUtils.class, "search", userBaseDN, 2, FIND_FILTER, FIND_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        User user = userImpl.find("s3test", "s3testuser");
        Assert.assertThat(expectedUser, new ReflectionEquals(user));
    }

    @Test
    public void Find_ExceptionOccurredWhileIterating_ThrowException()
            throws Exception {
        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";

        PowerMockito.doReturn(ldapResults).when(
            LDAPUtils.class, "search", userBaseDN, 2, FIND_FILTER, FIND_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);
        Mockito.doThrow(new LDAPException()).when(ldapResults).next();

        exception.expect(DataAccessException.class);
        userImpl.find("s3test", "s3testuser");
    }

    @Test
    public void Find_IAMUserExists_ReturnUserObject() throws Exception {
        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("123");
        expectedUser.setUserType("iamUser");
        expectedUser.setPath("/");
        expectedUser.setCreateDate(EXPECTED_DATE);
        expectedUser.setArn("arn:aws:iam::accountid:user/s3test");

        setupUserAttr();

        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";
        PowerMockito.doReturn(ldapResults).when(
            LDAPUtils.class, "search", userBaseDN, 2, FIND_FILTER, FIND_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);

        User user = userImpl.find("s3test", "s3testuser");
        Assert.assertThat(expectedUser, new ReflectionEquals(user));
    }

    @Test
    public void Find_RoleUserExists_ReturnUserObject() throws Exception {
        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("123");
        expectedUser.setUserType("roleUser");
        expectedUser.setCreateDate(EXPECTED_DATE);
        expectedUser.setRoleName("roleUserName");
        expectedUser.setArn("arn:aws:iam::accountid:user/s3test");

        setupUserAttr();
        Mockito.when(objectClassAttr.getStringValue()).thenReturn("roleUser");
        Mockito.when(entry.getAttribute("rolename")).thenReturn(roleAttr);
        Mockito.when(roleAttr.getStringValue()).thenReturn("roleUserName");

        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";
        PowerMockito.doReturn(ldapResults).when(
            LDAPUtils.class, "search", userBaseDN, 2, FIND_FILTER, FIND_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);

        User user = userImpl.find("s3test", "s3testuser");
        Assert.assertThat(expectedUser, new ReflectionEquals(user));
    }

    @Test(expected = DataAccessException.class)
    public void FindById_LdapSearchFailed_ThrowException() throws Exception {
        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";
        PowerMockito.doThrow(new LDAPException())
            .when(LDAPUtils.class, "search", userBaseDN, 2,
                  FIND_BYUSERID_FILTER, FIND_BYUSERID_ATTRS);

        userImpl.findByUserId("s3test", "s3UserId");
    }

    @Test
    public void FindById_UserDoesNotExist_ReturnUserObject() throws Exception {
        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setId(null);

        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";

        PowerMockito.doReturn(ldapResults)
            .when(LDAPUtils.class, "search", userBaseDN, 2,
                  FIND_BYUSERID_FILTER, FIND_BYUSERID_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        User user = userImpl.findByUserId("s3test", "s3UserId");
        Assert.assertThat(expectedUser, new ReflectionEquals(user));
    }

    @Test(expected = DataAccessException.class)
    public void FindById_ExceptionOccurredWhileIterating_ThrowException()
            throws Exception {
        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";

        PowerMockito.doReturn(ldapResults)
            .when(LDAPUtils.class, "search", userBaseDN, 2,
                  FIND_BYUSERID_FILTER, FIND_BYUSERID_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);
        Mockito.doThrow(new LDAPException()).when(ldapResults).next();

        userImpl.findByUserId("s3test", "s3UserId");
    }

    @Test
    public void FindById_IAMUserExists_ReturnUserObject() throws Exception {
        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("s3UserId");
        expectedUser.setUserType("iamUser");
        expectedUser.setPath("/");
        expectedUser.setCreateDate(EXPECTED_DATE);
        expectedUser.setArn("arn:aws:iam::accountid:user/s3test");

        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";

        setupUserAttr();
        PowerMockito.doReturn(ldapResults)
            .when(LDAPUtils.class, "search", userBaseDN, 2,
                  FIND_BYUSERID_FILTER, FIND_BYUSERID_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);

        User user = userImpl.findByUserId("s3test", "s3UserId");
        Assert.assertThat(expectedUser, new ReflectionEquals(user));
    }

    @Test
    public void FindById_RoleUserExists_ReturnUserObject() throws Exception {
        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("s3UserId");
        expectedUser.setUserType("roleUser");
        expectedUser.setCreateDate(EXPECTED_DATE);
        expectedUser.setRoleName("roleUserName");
        expectedUser.setArn("arn:aws:iam::accountid:user/s3test");

        setupUserAttr();
        Mockito.when(objectClassAttr.getStringValue()).thenReturn("roleUser");
        Mockito.when(entry.getAttribute("rolename")).thenReturn(roleAttr);
        Mockito.when(roleAttr.getStringValue()).thenReturn("roleUserName");

        String userBaseDN = "ou=users,o=s3test,ou=accounts,"
                + "dc=s3,dc=seagate,dc=com";
        PowerMockito.doReturn(ldapResults)
            .when(LDAPUtils.class, "search", userBaseDN, 2,
                  FIND_BYUSERID_FILTER, FIND_BYUSERID_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);

        User user = userImpl.findByUserId("s3test", "s3UserId");
        Assert.assertThat(expectedUser, new ReflectionEquals(user));
    }

    @Test
    public void Delete_LdapDeleteFailed_ThrowException() throws Exception {
        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("123");

        String dn = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";
        PowerMockito.doThrow(new LDAPException())
            .when(LDAPUtils.class, "delete", dn);
        exception.expect(DataAccessException.class);

        userImpl.delete(user);
    }

    @Test
    public void Delete_Success() throws Exception {
        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("123");

        String dn = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";

        userImpl.delete(user);
        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.delete(dn);
    }

    @Test
    public void Save_SaveIAMUserFailed_ThrowException() throws Exception {
        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("123");
        user.setUserType(User.UserType.IAM_USER);
        user.setPath("/test");
        user.setArn("arn:aws:iam::accountid:user/s3test");

        PowerMockito.doThrow(new LDAPException())
            .when(LDAPUtils.class, "add", Mockito.any(LDAPEntry.class));
        exception.expect(DataAccessException.class);

        userImpl.save(user);
    }

    @Test
    public void Save_SaveIAMUserSuccess() throws Exception {
        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("123");
        user.setUserType(User.UserType.IAM_USER);
        user.setPath("/test");

        String saveUserDN = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";

        LDAPAttributeSet ldapAttributeSet =
            Mockito.mock(LDAPAttributeSet.class);
        LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
        LDAPEntry userEntry = Mockito.mock(LDAPEntry.class);

        PowerMockito.whenNew(LDAPAttributeSet.class)
            .withNoArguments()
            .thenReturn(ldapAttributeSet);

        PowerMockito.whenNew(LDAPAttribute.class).withAnyArguments().thenReturn(
            ldapAttribute);

        Mockito.doReturn(true).when(ldapAttributeSet).add(ldapAttribute);
        PowerMockito.whenNew(LDAPEntry.class)
                .withParameterTypes(String.class, LDAPAttributeSet.class)
                .withArguments(saveUserDN, ldapAttributeSet)
                .thenReturn(userEntry);

        userImpl.save(user);

        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("objectclass", "iamuser");
        PowerMockito.verifyNew(LDAPAttribute.class)
            .withArguments("cn", "s3testuser");
        PowerMockito.verifyNew(LDAPAttribute.class)
            .withArguments("s3userid", "123");
        PowerMockito.verifyNew(LDAPAttribute.class)
            .withArguments("path", "/test");

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(userEntry);
    }

    @Test
    public void Save_SaveRoleUserSuccess() throws Exception {
        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("123");
        user.setUserType(User.UserType.ROLE_USER);
        user.setRoleName("roleUserName");

        String saveUserDN = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";

        LDAPAttributeSet ldapAttributeSet =
            Mockito.mock(LDAPAttributeSet.class);
        LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
        LDAPEntry userEntry = Mockito.mock(LDAPEntry.class);

        PowerMockito.whenNew(LDAPAttributeSet.class)
            .withNoArguments()
            .thenReturn(ldapAttributeSet);

        PowerMockito.whenNew(LDAPAttribute.class).withAnyArguments().thenReturn(
            ldapAttribute);

        Mockito.doReturn(true).when(ldapAttributeSet).add(ldapAttribute);
        PowerMockito.whenNew(LDAPEntry.class)
                .withParameterTypes(String.class, LDAPAttributeSet.class)
                .withArguments(saveUserDN, ldapAttributeSet)
                .thenReturn(userEntry);

        userImpl.save(user);

        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("objectclass", "roleuser");
        PowerMockito.verifyNew(LDAPAttribute.class)
            .withArguments("cn", "s3testuser");
        PowerMockito.verifyNew(LDAPAttribute.class)
            .withArguments("s3userid", "123");
        PowerMockito.verifyNew(LDAPAttribute.class)
            .withArguments("rolename", "roleUserName");

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(userEntry);
    }

    @Test
    public void FindAll_LdapSearchFailed_ThrowException() throws Exception {
        String userBaseDN = "ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";
        String filter = "(&(path=/*)(objectclass=iamuser))";

        PowerMockito.doThrow(new LDAPException()).when(
            LDAPUtils.class, "search", userBaseDN, 2, filter, FIND_ALL_ATTRS);
        exception.expect(DataAccessException.class);

        userImpl.findAll("s3test", "/");
    }

    @Test public void FindAll_UserDoesNotExist_ReturnEmptyUserList()
        throws Exception {
        User[] expectedUserList = new User[0];

        String userBaseDN = "ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";
        String filter = "(&(path=/*)(objectclass=iamuser))";

        PowerMockito.doReturn(ldapResults).when(
            LDAPUtils.class, "search", userBaseDN, 2, filter, FIND_ALL_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        User[] userList = userImpl.findAll("s3test", "/");
        Assert.assertArrayEquals(expectedUserList, userList);
    }

    @Test
    public void FindAll_ExceptionOccuredWhileIterating_ThrowException()
            throws Exception {
        String userBaseDN = "ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";
        String filter = "(&(path=/*)(objectclass=iamuser))";
        PowerMockito.doReturn(ldapResults).when(
            LDAPUtils.class, "search", userBaseDN, 2, filter, FIND_ALL_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);
        Mockito.doThrow(new LDAPException()).when(ldapResults).next();

        exception.expect(DataAccessException.class);
        userImpl.findAll("s3test", "/");
    }

    @Test public void FindAll_IAMUsersFound_ReturnUserList() throws Exception {
        User expectedUser = new User();
        expectedUser.setAccountName("s3test");
        expectedUser.setName("s3testuser");
        expectedUser.setId("123");
        expectedUser.setUserType("iamUser");
        expectedUser.setPath("/");
        expectedUser.setCreateDate(EXPECTED_DATE);
        expectedUser.setArn("arn:aws:iam::accountid:user/s3test");

        setupUserAttr();

        String userBaseDN = "ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";
        String filter = "(&(path=/*)(objectclass=iamuser))";
        PowerMockito.doReturn(ldapResults).when(
            LDAPUtils.class, "search", userBaseDN, 2, filter, FIND_ALL_ATTRS);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE).thenReturn(
            Boolean.FALSE);

        User[] userList = userImpl.findAll("s3test", "/");
        Assert.assertEquals(1, userList.length);
        Assert.assertThat(expectedUser, new ReflectionEquals(userList[0]));
    }

    @Test
    public void Update_UserModifyFailed_ThrowException() throws Exception {
        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setId("123");

        String dn = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";
        PowerMockito.doThrow(new LDAPException())
            .when(LDAPUtils.class, "modify", dn, new ArrayList());

        exception.expect(DataAccessException.class);

        userImpl.update(user, null, null);
    }

    @Test
    public void Update_UserModifySuccess() throws Exception {
        User user = new User();
        user.setAccountName("s3test");
        user.setName("s3testuser");
        user.setArn("arn:aws:iam::accountid:user/s3test");
        user.setId("123");

        ArrayList modifyList = Mockito.mock(ArrayList.class);
        LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
        LDAPModification modification = Mockito.mock(LDAPModification.class);

        PowerMockito.whenNew(ArrayList.class).withNoArguments().thenReturn(
            modifyList);

        PowerMockito.whenNew(LDAPAttribute.class).withAnyArguments().thenReturn(
            ldapAttribute);

        PowerMockito.whenNew(LDAPModification.class)
                .withParameterTypes(int.class, LDAPAttribute.class)
                .withArguments(LDAPModification.REPLACE, ldapAttribute)
                .thenReturn(modification);

        String dn = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3,"
                + "dc=seagate,dc=com";

        userImpl.update(user, "s3newuser", "/test/update");

        PowerMockito.verifyNew(LDAPAttribute.class)
            .withArguments("cn", "s3newuser");
        PowerMockito.verifyNew(LDAPAttribute.class)
            .withArguments("path", "/test/update");
        PowerMockito.verifyNew(LDAPModification.class, times(3))
            .withArguments(LDAPModification.REPLACE, ldapAttribute);
        Mockito.verify(modifyList, Mockito.times(3)).add(modification);

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.modify(dn, modifyList);
    }

}

