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
 * Original creation date: 06-Jan-2016
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPModification;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.authserver.AuthServerConfig;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.AccessKey;
import com.seagates3.model.User;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Matchers;
import org.mockito.Mockito;
import org.mockito.internal.matchers.apachecommons.ReflectionEquals;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PrepareForTest({LDAPUtils.class, AccessKeyImpl.class, AuthServerConfig.class})
@PowerMockIgnore( {"javax.management.*"})

public class AccessKeyImplTest {

    private final String ACCESSKEY_BASE_DN = "ou=accesskeys,dc=s3,dc=seagate,"
            + "dc=com";
    private final String[] FIND_ATTRS = {"s3userid", "sk", "exp", "token", "status",
        "createtimestamp", "objectclass"};

    private final String[] FIND_ALL_ATTRS = {"ak", "status", "createtimestamp"};

    private final String[] GETCOUNT_ATTRS = {"ak"};

    private final String LDAP_DATE;
    private final String EXPECTED_DATE;

    private final AccessKeyImpl accesskeyImpl;
    private final LDAPSearchResults ldapResults;
    private final LDAPEntry entry;
    private final LDAPAttribute accountIdAttr;
    private final LDAPAttribute accessKeyIdAttr;
    private final LDAPAttribute userIdAttr;
    private final LDAPAttribute secretKeyAttr;
    private final LDAPAttribute statusAttr;
    private final LDAPAttribute createTimeStampAttr;
    private final LDAPAttribute objectClassAttr;
    private final LDAPAttribute expiryAttr;
    private final LDAPAttribute tokenAttr;

    @Rule
    public final ExpectedException exception = ExpectedException.none();

    public AccessKeyImplTest() {
        accesskeyImpl = new AccessKeyImpl();
        ldapResults = Mockito.mock(LDAPSearchResults.class);
        entry = Mockito.mock(LDAPEntry.class);
        accountIdAttr = Mockito.mock(LDAPAttribute.class);
        accessKeyIdAttr = Mockito.mock(LDAPAttribute.class);
        userIdAttr = Mockito.mock(LDAPAttribute.class);
        secretKeyAttr = Mockito.mock(LDAPAttribute.class);
        statusAttr = Mockito.mock(LDAPAttribute.class);
        createTimeStampAttr = Mockito.mock(LDAPAttribute.class);
        objectClassAttr = Mockito.mock(LDAPAttribute.class);
        expiryAttr = Mockito.mock(LDAPAttribute.class);
        tokenAttr = Mockito.mock(LDAPAttribute.class);

        LDAP_DATE = "20160129160752Z";
        EXPECTED_DATE = "2016-01-29T16:07:52.000+0000";
    }

    private void setupAccessKeyAttr() throws Exception {
        PowerMockito.mockStatic(LDAPUtils.class);
        Mockito.when(ldapResults.next()).thenReturn(entry);

        Mockito.when(entry.getAttribute("accountid")).thenReturn(accountIdAttr);
        Mockito.when(accountIdAttr.getStringValue()).thenReturn("12345");

        Mockito.when(entry.getAttribute("s3userid")).thenReturn(userIdAttr);
        Mockito.when(userIdAttr.getStringValue()).thenReturn("123");

        Mockito.when(entry.getAttribute("ak")).thenReturn(accessKeyIdAttr);
        Mockito.when(accessKeyIdAttr.getStringValue()).thenReturn("AKIATEST");

        Mockito.when(entry.getAttribute("sk")).thenReturn(secretKeyAttr);
        Mockito.when(secretKeyAttr.getStringValue()).thenReturn("sk-123/test");

        Mockito.when(entry.getAttribute("status")).thenReturn(statusAttr);
        Mockito.when(statusAttr.getStringValue()).thenReturn("Active");

        Mockito.when(entry.getAttribute("createtimestamp"))
                .thenReturn(createTimeStampAttr);
        Mockito.when(createTimeStampAttr.getStringValue()).thenReturn(LDAP_DATE);

        Mockito.when(entry.getAttribute("objectclass"))
                .thenReturn(objectClassAttr);
        Mockito.when(objectClassAttr.getStringValue()).thenReturn("accesskey");
    }

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(LDAPUtils.class);
        PowerMockito.mockStatic(AuthServerConfig.class);
    }

    @Test
    public void Find_AccessKeySearchFailed_ThrowException() throws Exception {
        String filter = "ak=AKIATEST";

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ATTRS
        );

        exception.expect(DataAccessException.class);

        accesskeyImpl.find("AKIATEST");
    }

    @Test(expected = DataAccessException.class)
    public void Find_AccessKey_ThrowException() throws Exception {
        String filter = "ak=AKIATEST";
        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE);
        Mockito.when(ldapResults.next()).thenThrow(LDAPException.class);

        accesskeyImpl.find("AKIATEST");
    }

    @Test
    public void Find_AccessKeyNotFound_ReturnEmptyAccessKey() throws Exception {
        AccessKey expectedAccessKey = new AccessKey();
        expectedAccessKey.setId("AKIATEST");
        String filter = "ak=AKIATEST";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.doReturn(Boolean.FALSE).when(ldapResults).hasMore();

        AccessKey accessKey = accesskeyImpl.find("AKIATEST");
        Assert.assertThat(expectedAccessKey, new ReflectionEquals(accessKey));
    }

    @Test
    public void Find_AccessKeyFound_ReturnAccessKey() throws Exception {
        AccessKey expectedAccessKey = new AccessKey();
        expectedAccessKey.setId("AKIATEST");
        expectedAccessKey.setUserId("123");
        expectedAccessKey.setSecretKey("sk-123/test");
        expectedAccessKey.setCreateDate(EXPECTED_DATE);
        expectedAccessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        String filter = "ak=AKIATEST";

        setupAccessKeyAttr();

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);

        AccessKey accessKey = accesskeyImpl.find("AKIATEST");
        Assert.assertThat(expectedAccessKey, new ReflectionEquals(accessKey));
    }

    @Test
    public void Find_FedAccessKeyFound_ReturnAccessKey() throws Exception {
        AccessKey expectedAccessKey = new AccessKey();
        expectedAccessKey.setId("AKIATEST");
        expectedAccessKey.setUserId("123");
        expectedAccessKey.setSecretKey("sk-123/test");
        expectedAccessKey.setCreateDate(EXPECTED_DATE);
        expectedAccessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);
        expectedAccessKey.setExpiry("2016-01-29T16:07:52.000+0000");
        expectedAccessKey.setToken("XYZ");

        // Arrange
        String filter = "ak=AKIATEST";
        setupAccessKeyAttr();
        Mockito.when(objectClassAttr.getStringValue()).thenReturn("fedaccesskey");
        Mockito.when(entry.getAttribute("exp"))
                .thenReturn(expiryAttr);
        Mockito.when(expiryAttr.getStringValue()).thenReturn("20160129160752Z");
        Mockito.when(entry.getAttribute("token"))
                .thenReturn(tokenAttr);
        Mockito.when(tokenAttr.getStringValue()).thenReturn("XYZ");
        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);

        // Act
        AccessKey accessKey = accesskeyImpl.find("AKIATEST");

        // Verify
        Assert.assertThat(expectedAccessKey, new ReflectionEquals(accessKey));
    }

    @Test
    public void FindAll_AccessKeySearchFailed_ThrowException()
            throws Exception {
        User user = new User();
        user.setId("123");
        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ALL_ATTRS
        );

        exception.expect(DataAccessException.class);

        accesskeyImpl.findAll(user);
    }

    @Test
    public void FindAll_AccessKeyEmpty_ReturnEmptyList() throws Exception {
        AccessKey[] expectedAccessKeyList = new AccessKey[0];

        User user = new User();
        user.setId("123");

        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ALL_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        AccessKey[] accessKeyList = accesskeyImpl.findAll(user);
        Assert.assertThat(expectedAccessKeyList,
                new ReflectionEquals(accessKeyList));
    }

    @Test
    public void FindAll_AccessKeyIterationFailed_ThrowException()
            throws Exception {
        User user = new User();
        user.setId("123");

        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ALL_ATTRS
        );
        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ALL_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);
        Mockito.doThrow(new LDAPException()).when(ldapResults).next();

        exception.expect(DataAccessException.class);

        accesskeyImpl.findAll(user);
    }

    @Test
    public void FindAll_AccessKeyFound_ReturnList() throws Exception {
        setupAccessKeyAttr();

        AccessKey expectedAccessKey = new AccessKey();
        expectedAccessKey.setCreateDate(EXPECTED_DATE);
        expectedAccessKey.setId("AKIATEST");
        expectedAccessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        User user = new User();
        user.setId("123");

        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, FIND_ALL_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);

        AccessKey[] accessKeyList = accesskeyImpl.findAll(user);
        Assert.assertEquals(1, accessKeyList.length);
        Assert.assertThat(expectedAccessKey,
                new ReflectionEquals(accessKeyList[0]));
    }

    @Test
    public void GetCount_AccessKeySearchFailed_ThrowException()
            throws Exception {
        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, GETCOUNT_ATTRS
        );

        exception.expect(DataAccessException.class);

        accesskeyImpl.getCount("123");
    }

    @Test
    public void GetCount_ExceptionOccuredWhileIterating_ThrowException()
            throws Exception {
        String filter = "(&(s3userid=123)(objectclass=accesskey))";
        exception.expect(DataAccessException.class);

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, GETCOUNT_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);
        Mockito.when(ldapResults.next()).thenThrow(new LDAPException());

        accesskeyImpl.getCount("123");
    }

    @Test
    public void GetCount_NoAccessKey_ReturnZero() throws Exception {
        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, GETCOUNT_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        int count = accesskeyImpl.getCount("123");
        Assert.assertEquals(0, count);
    }

    @Test
    public void GetCount_HasTwoAccessKeys_ReturnTwo() throws Exception {
        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, GETCOUNT_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);
        Mockito.doReturn(entry).when(ldapResults).next();

        int count = accesskeyImpl.getCount("123");
        Assert.assertEquals(2, count);
    }

    @Test
    public void HasAccessKeys_NoAccessKey_ReturnFalse() throws Exception {
        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, GETCOUNT_ATTRS
        );
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        Boolean hasKeys = accesskeyImpl.hasAccessKeys("123");
        Assert.assertEquals(Boolean.FALSE, hasKeys);
    }

    @Test
    public void HasAccessKeys_HasAccessKeys_ReturnTrue() throws Exception {
        String filter = "(&(s3userid=123)(objectclass=accesskey))";

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, GETCOUNT_ATTRS
        );
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);
        Mockito.doReturn(entry).when(ldapResults).next();

        Boolean hasKeys = accesskeyImpl.hasAccessKeys("123");
        Assert.assertEquals(Boolean.TRUE, hasKeys);
    }

    @Test
    public void Delete_AccessKeyDeleteFailed_ThrowException() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("AKIATEST");

        String dn = "ak=AKIATEST,ou=accesskeys,dc=s3,dc=seagate,dc=com";

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "delete",
                dn
        );

        exception.expect(DataAccessException.class);

        accesskeyImpl.delete(accessKey);
    }

    @Test
    public void Delete_AccessKeyDeleted() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("AKIATEST");

        String dn = "ak=AKIATEST,ou=accesskeys,dc=s3,dc=seagate,dc=com";

        accesskeyImpl.delete(accessKey);
        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.delete(dn);
    }

    @Test
    public void Save_AccessKeySaveFailed_ThrowException() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setUserId("123");
        accessKey.setId("AKIATEST");
        accessKey.setSecretKey("s3-1232/test");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "add",
                Matchers.any(LDAPEntry.class)
        );

        exception.expect(DataAccessException.class);

        accesskeyImpl.save(accessKey);
    }

    @Test
    public void Save_AccessKeySaved() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setUserId("123");
        accessKey.setId("AKIATEST");
        accessKey.setSecretKey("s3-1232/test");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        String dn = "ak=AKIATEST,ou=accesskeys,dc=s3,dc=seagate,dc=com";

        LDAPAttributeSet ldapAttributeSet = Mockito.mock(LDAPAttributeSet.class);
        LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
        LDAPEntry accessKeyEntry = Mockito.mock(LDAPEntry.class);

        PowerMockito.whenNew(LDAPAttributeSet.class)
                .withNoArguments()
                .thenReturn(ldapAttributeSet);

        PowerMockito.whenNew(LDAPAttribute.class)
                .withAnyArguments()
                .thenReturn(ldapAttribute);

        Mockito.doReturn(true).when(ldapAttributeSet).add(ldapAttribute);
        PowerMockito.whenNew(LDAPEntry.class)
                .withParameterTypes(String.class, LDAPAttributeSet.class)
                .withArguments(dn, ldapAttributeSet)
                .thenReturn(accessKeyEntry);

        accesskeyImpl.save(accessKey);

        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("objectclass", "accesskey");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("s3userid", "123");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("ak", "AKIATEST");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("sk", "s3-1232/test");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("status", "Active");

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(accessKeyEntry);
    }

    @Test
    public void Save_FedAccessKeySaveFailed_ThrowException() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setUserId("123");
        accessKey.setId("AKIATEST");
        accessKey.setSecretKey("s3-1232/test");
        accessKey.setToken("s3token/812-12");
        accessKey.setExpiry(EXPECTED_DATE);
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "add",
                Matchers.any(LDAPEntry.class)
        );

        exception.expect(DataAccessException.class);

        accesskeyImpl.save(accessKey);
    }

    @Test
    public void Save_FedAccessKeySaved() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setUserId("123");
        accessKey.setId("AKIATEST");
        accessKey.setSecretKey("s3-1232/test");
        accessKey.setToken("s3token/812-12");
        accessKey.setExpiry("2016-01-09T10:55:36.000+0000");
        accessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);

        String dn = "ak=AKIATEST,ou=accesskeys,dc=s3,dc=seagate,dc=com";

        LDAPAttributeSet ldapAttributeSet = Mockito.mock(LDAPAttributeSet.class);
        LDAPAttribute ldapAttribute = Mockito.mock(LDAPAttribute.class);
        LDAPEntry accessKeyEntry = Mockito.mock(LDAPEntry.class);

        PowerMockito.whenNew(LDAPAttributeSet.class)
                .withNoArguments()
                .thenReturn(ldapAttributeSet);

        PowerMockito.whenNew(LDAPAttribute.class)
                .withAnyArguments()
                .thenReturn(ldapAttribute);

        Mockito.doReturn(true).when(ldapAttributeSet).add(ldapAttribute);
        PowerMockito.whenNew(LDAPEntry.class)
                .withParameterTypes(String.class, LDAPAttributeSet.class)
                .withArguments(dn, ldapAttributeSet)
                .thenReturn(accessKeyEntry);

        accesskeyImpl.save(accessKey);

        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("objectclass", "fedaccessKey");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("s3userid", "123");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("ak", "AKIATEST");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("sk", "s3-1232/test");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("token", "s3token/812-12");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("exp", "20160109105536Z");
        PowerMockito.verifyNew(LDAPAttribute.class)
                .withArguments("status", "Active");

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.add(accessKeyEntry);
    }

    @Test
    public void Update_AccessKeyModifyFailed_ThrowException() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("AKIATEST");

        LDAPAttribute updateAttr = Mockito.mock(LDAPAttribute.class);
        LDAPModification modification = Mockito.mock(LDAPModification.class);

        PowerMockito.whenNew(LDAPAttribute.class)
                .withParameterTypes(String.class, String.class)
                .withArguments("status", "Active")
                .thenReturn(updateAttr);

        PowerMockito.whenNew(LDAPModification.class)
                .withParameterTypes(int.class, LDAPAttribute.class)
                .withArguments(LDAPModification.REPLACE, updateAttr)
                .thenReturn(modification);

        String dn = "ak=AKIATEST,ou=accesskeys,dc=s3,dc=seagate,dc=com";
        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "modify",
                dn, modification
        );

        exception.expect(DataAccessException.class);

        accesskeyImpl.update(accessKey, "Active");
    }

    @Test
    public void UpdateUser_UserModifySuccess() throws Exception {
        AccessKey accessKey = new AccessKey();
        accessKey.setId("AKIATEST");

        LDAPAttribute updateAttr = Mockito.mock(LDAPAttribute.class);
        LDAPModification modification = Mockito.mock(LDAPModification.class);

        PowerMockito.whenNew(LDAPAttribute.class)
                .withParameterTypes(String.class, String.class)
                .withArguments("status", "Active")
                .thenReturn(updateAttr);

        PowerMockito.whenNew(LDAPModification.class)
                .withParameterTypes(int.class, LDAPAttribute.class)
                .withArguments(LDAPModification.REPLACE, updateAttr)
                .thenReturn(modification);

        String dn = "ak=AKIATEST,ou=accesskeys,dc=s3,dc=seagate,dc=com";
        PowerMockito.doNothing().when(LDAPUtils.class, "modify",
                dn, modification
        );

        accesskeyImpl.update(accessKey, "Active");

        PowerMockito.verifyStatic(Mockito.times(1));
        LDAPUtils.modify(dn, modification);
    }

    @Test
    public void FindFromToken_AccessKeyFound_ReturnAccessKey() throws Exception {
        AccessKey expectedAccessKey = new AccessKey();
        expectedAccessKey.setUserId("123");
        expectedAccessKey.setSecretKey("sk-123/test");
        expectedAccessKey.setCreateDate(EXPECTED_DATE);
        expectedAccessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);
        expectedAccessKey.setToken("AWS_SEC_TOKEN");

        String filter = "token=AWS_SEC_TOKEN";
        String[] attrs = {LDAPUtils.USER_ID, LDAPUtils.SECRET_KEY,
                LDAPUtils.EXPIRY, LDAPUtils.ACCESS_KEY_ID, LDAPUtils.STATUS,
                LDAPUtils.CREATE_TIMESTAMP, LDAPUtils.OBJECT_CLASS};

        setupAccessKeyAttr();

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, attrs);
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);

        AccessKey accessKey = accesskeyImpl.findFromToken("AWS_SEC_TOKEN");
        Assert.assertThat(expectedAccessKey, new ReflectionEquals(accessKey));
    }

    @Test
    public void FindFromToken_AccessKeyNotFound_ReturnEmptyAccessKey() throws Exception {
        AccessKey expectedAccessKey = new AccessKey();
        expectedAccessKey.setToken("AWS_SEC_TOKEN");

        String filter = "token=AWS_SEC_TOKEN";
        String[] attrs = {LDAPUtils.USER_ID, LDAPUtils.SECRET_KEY,
                LDAPUtils.EXPIRY, LDAPUtils.ACCESS_KEY_ID, LDAPUtils.STATUS,
                LDAPUtils.CREATE_TIMESTAMP, LDAPUtils.OBJECT_CLASS};

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, attrs);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.FALSE);

        AccessKey accessKey = accesskeyImpl.findFromToken("AWS_SEC_TOKEN");
        Assert.assertThat(expectedAccessKey, new ReflectionEquals(accessKey));
    }

    @Test(expected = DataAccessException.class)
    public void FindFromToken_SearchShouldTrowException() throws Exception {
        String filter = "token=AWS_SEC_TOKEN";
        String[] attrs = {LDAPUtils.USER_ID, LDAPUtils.SECRET_KEY,
                LDAPUtils.EXPIRY, LDAPUtils.ACCESS_KEY_ID, LDAPUtils.STATUS,
                LDAPUtils.CREATE_TIMESTAMP, LDAPUtils.OBJECT_CLASS};

        PowerMockito.doThrow(new LDAPException()).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, attrs);

        accesskeyImpl.findFromToken("AWS_SEC_TOKEN");
    }

    @Test(expected = DataAccessException.class)
    public void FindFromToken_GetEntryShouldThrowException() throws Exception {
        String filter = "token=AWS_SEC_TOKEN";
        String[] attrs = {LDAPUtils.USER_ID, LDAPUtils.SECRET_KEY,
                LDAPUtils.EXPIRY, LDAPUtils.ACCESS_KEY_ID, LDAPUtils.STATUS,
                LDAPUtils.CREATE_TIMESTAMP, LDAPUtils.OBJECT_CLASS};

        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, attrs);
        Mockito.when(ldapResults.hasMore()).thenReturn(Boolean.TRUE);
        Mockito.when(ldapResults.next()).thenThrow(new LDAPException());

        accesskeyImpl.findFromToken("AWS_SEC_TOKEN");
    }

    @Test
    public void FindFromToken_FedAccessKeyFound_ReturnAccessKey() throws Exception {
        AccessKey expectedAccessKey = new AccessKey();
        expectedAccessKey.setId("AKIATEST");
        expectedAccessKey.setUserId("123");
        expectedAccessKey.setSecretKey("sk-123/test");
        expectedAccessKey.setCreateDate(EXPECTED_DATE);
        expectedAccessKey.setStatus(AccessKey.AccessKeyStatus.ACTIVE);
        expectedAccessKey.setExpiry("2016-01-29T16:07:52.000+0000");
        expectedAccessKey.setToken("AWS_SEC_TOKEN");

        // Arrange
        String filter = "token=AWS_SEC_TOKEN";
        String[] attrs = {LDAPUtils.USER_ID, LDAPUtils.SECRET_KEY,
                LDAPUtils.EXPIRY, LDAPUtils.ACCESS_KEY_ID, LDAPUtils.STATUS,
                LDAPUtils.CREATE_TIMESTAMP, LDAPUtils.OBJECT_CLASS};

        setupAccessKeyAttr();
        Mockito.when(objectClassAttr.getStringValue()).thenReturn("fedaccesskey");
        Mockito.when(entry.getAttribute("exp"))
                .thenReturn(expiryAttr);
        Mockito.when(expiryAttr.getStringValue()).thenReturn("20160129160752Z");
        Mockito.when(entry.getAttribute("token"))
                .thenReturn(tokenAttr);
        Mockito.when(tokenAttr.getStringValue()).thenReturn("AWS_SEC_TOKEN");
        PowerMockito.doReturn(ldapResults).when(LDAPUtils.class, "search",
                ACCESSKEY_BASE_DN, 2, filter, attrs);
        Mockito.when(ldapResults.hasMore())
                .thenReturn(Boolean.TRUE)
                .thenReturn(Boolean.FALSE);

        // Act
        AccessKey accessKey = accesskeyImpl.findFromToken("AWS_SEC_TOKEN");

        // Verify
        Assert.assertThat(expectedAccessKey, new ReflectionEquals(accessKey));
    }
}
