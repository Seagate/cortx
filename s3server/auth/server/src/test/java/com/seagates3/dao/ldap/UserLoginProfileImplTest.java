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
 * Original author: Abhilekh Mustapure <abhilekh.mustapure@seagate.com>
 * Original creation date: 22-May-2019
 */
package com.seagates3.dao.ldap;

import com.novell.ldap.LDAPAttribute;
import com.novell.ldap.LDAPAttributeSet;
import com.novell.ldap.LDAPEntry;
import com.novell.ldap.LDAPException;
import com.novell.ldap.LDAPModification;
import com.novell.ldap.LDAPSearchResults;
import com.seagates3.exception.DataAccessException;
import com.seagates3.model.User;
import com.seagates3.util.KeyGenUtil;

import java.util.ArrayList;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.times;
import org.mockito.internal.matchers.apachecommons.ReflectionEquals;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.api.mockito.mockpolicies.Slf4jMockPolicy;
import org.powermock.core.classloader.annotations.MockPolicy;
import org.powermock.core.classloader.annotations.PowerMockIgnore;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
    @PrepareForTest({LDAPUtils.class, UserLoginProfileImpl.class})
    @MockPolicy(Slf4jMockPolicy.class) @PowerMockIgnore(
        {"javax.management.*"}) public class UserLoginProfileImplTest {

 private
  final UserLoginProfileImpl userLoginProfileImpl;
 private
  final LDAPSearchResults ldapResults;
 private
  final LDAPEntry entry;

 private
  final LDAPAttribute userIdAttr;
 private
  final LDAPAttribute objectClassAttr;
 private
  final LDAPAttribute pathAttr;
 private
  final LDAPAttribute commonNameAttr;
 private
  final LDAPAttribute roleAttr;

  @Rule public final ExpectedException exception = ExpectedException.none();

 public
  UserLoginProfileImplTest() {
    userLoginProfileImpl = new UserLoginProfileImpl();
    ldapResults = Mockito.mock(LDAPSearchResults.class);
    entry = Mockito.mock(LDAPEntry.class);
    userIdAttr = Mockito.mock(LDAPAttribute.class);
    objectClassAttr = Mockito.mock(LDAPAttribute.class);
    pathAttr = Mockito.mock(LDAPAttribute.class);
    commonNameAttr = Mockito.mock(LDAPAttribute.class);
    roleAttr = Mockito.mock(LDAPAttribute.class);
  }

  @Before public void setUp() throws Exception {
    PowerMockito.mockStatic(LDAPUtils.class);
    Mockito.when(ldapResults.next()).thenReturn(entry);
  }

  @Test public void Create_UserLoginProfileFailed_ThrowException()
      throws Exception {

    PowerMockito.mockStatic(UserLoginProfileImpl.class);
    User user = new User();
    user.setAccountName("s3test");
    user.setName("s3testuser");
    user.setId("123");
    user.setPassword("abcdef");
    user.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    user.setPwdResetRequired("true");

    String dn = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3," +
                "dc=seagate,dc=com";

    ArrayList mockList = Mockito.mock(ArrayList.class);
    PowerMockito.whenNew(ArrayList.class).withNoArguments().thenReturn(
        mockList);

    PowerMockito.doThrow(new LDAPException())
        .when(LDAPUtils.class, "modify", dn, mockList);

    exception.expect(DataAccessException.class);

    userLoginProfileImpl.save(user);
  }

  @Test public void Update_UserModifySuccess() throws Exception {

    User user = new User();
    user.setAccountName("s3test");
    user.setName("s3testuser");
    user.setId("123");
    user.setPassword("abcd");
    user.setProfileCreateDate("2019-06-16 15:38:53+00:00");
    user.setPwdResetRequired("true");

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

    String dn = "s3userid=123,ou=users,o=s3test,ou=accounts,dc=s3," +
                "dc=seagate,dc=com";

    userLoginProfileImpl.save(user);

    PowerMockito.verifyNew(LDAPAttribute.class)
        .withArguments("userPassword", "abcd");
    PowerMockito.verifyNew(LDAPModification.class, times(3))
        .withArguments(LDAPModification.REPLACE, ldapAttribute);
    Mockito.verify(modifyList, Mockito.times(3)).add(modification);

    PowerMockito.verifyStatic(Mockito.times(1));
    LDAPUtils.modify(dn, modifyList);
  }
}
